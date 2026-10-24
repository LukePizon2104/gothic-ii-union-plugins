// Logika bez silnika: korekty pozycji bohatera (straznik skoku gry, skoku korzenia, teleportu kolizji) a przestawienia
// bohatera przez kogos innego (zTimeRewind w trakcie cofania, teleport skryptu / konsoli, ResetPos, inne wtyczki).
// Silnik: zDismember\Traversal.cpp (haki SetPositionWorld / SetTrafoObjToWorld / Move / EndMovement). Testy:
// tests\test_core.cpp. Log NB 18.10: zTimeRewind (DIFF_02) co klatke stawial bohatera w pozycji z nagrania, a nasz
// straznik lotu (S_FALLDN) w tej samej klatce cofal go z powrotem - ping-pong ~150 cm co klatke.
#pragma once
#include <cmath>
#include <string>

namespace zdcore {
	// Modul ramki stosu wywolania (od bezposredniego wywolujacego).
	enum HeroModKind { HG_MOD_EXE = 0, HG_MOD_OURS, HG_MOD_FOREIGN, HG_MOD_SYSTEM };
	enum HeroSetKind { HG_SET_ENGINE = 0, HG_SET_OURS, HG_SET_EXTERNAL };
	const int HG_OURS_SCAN = 4;   // ramki exe / systemowe przeszukiwane w glab za naszym DLL (nasze wywolanie silnika)

	// Kto ustawil pozycje bohatera. frames[0] = bezposredni wywolujacy. exeTeleport: bezposredni wywolujacy w exe to
	// funkcja teleportu (ResetPos, EmergencyResetPos, BeamTo, Enable, konsola, oCMobInter::SetIdealPosition) - zawsze
	// przestawienie. Obcy DLL jako bezposredni wywolujacy = przestawienie (zTimeRewind, NB). Exe: nasz DLL w kilku
	// nastepnych ramkach = nasze wywolanie funkcji silnika (np. AlignModelToLedge), inaczej ruch silnika (obcy DLL
	// glebiej, np. hak NB na DoAI nad ruchem silnika, nie zmienia tego).
	inline int ClassifyHeroSet(const int* frames, int n, bool exeTeleport) {
		if (!frames || n <= 0)
			return HG_SET_ENGINE;
		if (frames[0] == HG_MOD_OURS)
			return HG_SET_OURS;
		if (frames[0] == HG_MOD_FOREIGN)
			return HG_SET_EXTERNAL;
		if (frames[0] == HG_MOD_EXE && exeTeleport)
			return HG_SET_EXTERNAL;
		for (int i = 1; i < n && i <= HG_OURS_SCAN; i++) {
			if (frames[i] == HG_MOD_OURS)
				return HG_SET_OURS;
			if (frames[i] == HG_MOD_FOREIGN)
				break;
		}
		return HG_SET_ENGINE;
	}

	// Ostatnie przestawienie bohatera z zewnatrz (czas petli pluginu w s).
	struct HeroExtTracker {
		float t = -1e9f;
		std::string who;
		int count = 0;
	};
	inline void HeroExtNote(HeroExtTracker& tr, float now, const std::string& who) {
		tr.t = now;
		tr.who = who;
		tr.count++;
	}
	inline bool HeroExtRecent(const HeroExtTracker& tr, float now, float window) {
		return now - tr.t <= window + 1e-4f;
	}

	// Model bohatera zatrzymany (timeScale 0 - zTimeRewind zamraza marionetki, takze bohatera, na czas cofania i
	// ustawia klatki animacji sam: ruch korzenia z przewijanych klatek ma wtedy metry w klatce).
	inline bool HeroModelFrozen(float timeScale) {
		return timeScale < 0.001f;
	}

	// Powod wstrzymania naszych korekt pozycji (log) albo nullptr.
	inline const char* HeroGuardSuspendWhy(bool frozen, bool externalRecent) {
		if (frozen)
			return "model bohatera zamrozony (cofanie czasu / inna wtyczka)";
		if (externalRecent)
			return "zewnetrzne przestawienie bohatera";
		return nullptr;
	}

	// Teleport kolizji (zCVob::EndMovement z zCVob::DoFrameActivity): przy zwyklym ruchu w klatce (zadane najwyzej
	// maxRequest) rozwiazanie kolizji postaci przenosi bohatera o metry - do pozycji sprzed 0,3-0,6 s (log NB/Steam:
	// start skoku w biegu przy scianie "zadane 55 cm, kolizja zmienila o 272 cm", start spadania, wlaczenie kolizji po
	// przeskoku). Wynik daleko i od zadanego, i od startu klatki = blad, nie zderzenie (zderzenie zostawia przy starcie).
	struct CollTeleportParams {
		float maxRequest = 150.0f;   // cm zadanego ruchu w klatce (sprint 850 cm/s przy 0,1 s + zapas)
		float minJump = 100.0f;      // cm - wynik kolizji co najmniej tyle od zadanego i od startu klatki
		int maxStreak = 20;          // klatek pod rzad - dluzej silnik sie upiera (np. ruchoma przeszkoda), wynik przyjety
	};
	inline float HgDist(const float a[3], const float b[3]) {
		float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
		return std::sqrt(x * x + y * y + z * z);
	}
	inline float HgFlatDist(const float a[3], const float b[3]) {
		float x = a[0] - b[0], z = a[2] - b[2];
		return std::sqrt(x * x + z * z);
	}
	inline bool CollTeleportGlitch(const float from[3], const float req[3], const float to[3], const CollTeleportParams& p) {
		return HgDist(req, from) <= p.maxRequest && HgDist(to, req) >= p.minJump && HgDist(to, from) >= p.minJump;
	}
	// true = cofnac wynik do startu klatki; streak liczy klatki pod rzad (zerowany klatka bez bledu).
	inline bool CollTeleportDecide(bool glitch, int& streak, const CollTeleportParams& p) {
		if (!glitch) {
			streak = 0;
			return false;
		}
		if (streak >= p.maxStreak)
			return false;
		streak++;
		return true;
	}

	// Nasza poprawka pozycji (from -> to) cofnieta w nastepnej klatce: bohater znow przy from (silnik / kolizja upiera
	// sie przy swoim) - dalsze poprawki tylko walcza z silnikiem (ping-pong; render i tak pokazuje pozycje silnika).
	inline bool TravFixUndone(const float from[3], const float to[3], const float cur[3]) {
		float fix = HgFlatDist(to, from);
		if (fix < 30.0f)
			return false;
		float tol = 0.25f * fix > 30.0f ? 0.25f * fix : 30.0f;
		return HgFlatDist(cur, from) <= tol;
	}

	// Poprawki straznika lotu (cofniecie w klatce, przyciecie kroku, dryf, ped) tylko w skoku z biegu obslugiwanym
	// przez plugin (start T_*RUN*_2_JUMP), bez wstrzymania i bez cofnietej poprawki w tym skoku.
	inline bool TravFlightFixAllowed(bool runJumpStart, bool suspended, bool gaveUp) {
		return runJumpStart && !suspended && !gaveUp;
	}

	// Koniec przeskoku / wspiecia (log NB 58087 / 58119, piramida beczek NW_HARBOUR_BARRELGROUP_01 pod murem): w pozycji
	// konca toru kolizja silnika (wlaczona przy wyjsciu w bieg) wypycha bohatera o 2,25 m do tylu na wysokosci gory -
	// spada. Sonda: bohater w pozycji kandydata, zerowy ruch z kolizja, przesuniecie wyniku. Kandydaci (przesuniecia
	// wzgledem pozycji, x/z w kierunku ruchu dx/dz): sama pozycja, w gore 15/30/45 cm (stopy nad gorna powierzchnia
	// kolizji vobu - promien trafia w zaokraglona beczke nizej), z withForward: 25 cm dalej na 0/15/30 cm. Pierwszy z
	// przesunieciem najwyzej tol = przyjety; zaden - koniec ruchu niedostepny.
	const int TRAV_SETTLE_MAX = 8;
	inline int TravSettleOffsets(float dx, float dz, bool withForward, float out[TRAV_SETTLE_MAX][3]) {
		int n = 0;
		for (float up : { 0.0f, 15.0f, 30.0f, 45.0f }) {
			out[n][0] = 0.0f;
			out[n][1] = up;
			out[n][2] = 0.0f;
			n++;
		}
		if (withForward)
			for (float up : { 0.0f, 15.0f, 30.0f }) {
				out[n][0] = dx * 25.0f;
				out[n][1] = up;
				out[n][2] = dz * 25.0f;
				n++;
			}
		return n;
	}
	inline int TravSettlePick(const float* shifts, int n, float tol) {
		for (int i = 0; shifts && i < n; i++)
			if (shifts[i] <= tol)
				return i;
		return -1;
	}

	// Wyjscie z przeskoku / wspiecia w bieg (W trzymane) - log NB 19.10: koniec w postawie 0 z 14 wypchniec kolizji,
	// wyjscie w bieg 8 z 27 (pierwsza klatka biegu: "zadane 0 cm, kolizja zmienila o 130-259 cm" - bohater 1,3-2,6 m do
	// tylu na wysokosci gory i spadek). Wyjscie w bieg tylko, gdy droga pierwszych krokow jest wolna (promienie na
	// wysokosci kapsuly, need cm) i krok sondy z kolizja (stepCm do przodu) konczy sie przy zadanym miejscu; inaczej
	// koniec w postawie (tor do konca klipu, kolizja na koncu - bez wypchniec w logu).
	struct TravExitPathParams {
		float need = 130.0f;         // cm wolnej drogi przed bohaterem (pierwsze kroki biegu + promien ciala)
		float stepCm = 20.0f;        // krok sondy z kolizja
		float stepTol = 30.0f;       // wynik kroku najwyzej tyle od zadanego miejsca
	};
	// hits: odleglosci trafien promieni do przodu (< 0 = brak). Zwraca najblizsza przeszkode w zasiegu need albo -1.
	inline float TravRunExitBlockDist(const float* hits, int n, const TravExitPathParams& p) {
		float best = -1.0f;
		for (int i = 0; hits && i < n; i++)
			if (hits[i] >= 0.0f && hits[i] <= p.need && (best < 0.0f || hits[i] < best))
				best = hits[i];
		return best;
	}
	// moved: o ile przesunal sie bohater przy kroku sondy (zadane stepCm).
	inline bool TravRunExitStepOk(float stepCm, float moved, const TravExitPathParams& p) {
		return std::fabs(moved - stepCm) <= p.stepTol;
	}
	// Nastepny ruch (chwyt w locie, dobieg, przeskok) tuz po koncu poprzedniego tylko z nowym Alt (albo Alt wcisniety
	// w oknie wyjscia - swiadomy lancuch); sinceEnd < 0 = poprzedniego ruchu nie bylo.
	inline bool TravChainBlocked(float sinceEnd, bool freshAlt, float minRun) {
		return sinceEnd >= 0.0f && sinceEnd < minRun && !freshAlt;
	}

	// Klawisze ruchu gry (W/S/A/D, boki, skok, chod) w trakcie animacji przeskoku / wspiecia: gra ich nie widzi (blokada
	// ruchu silnika plus haki NB na _Forward / PC_JumpForward / PC_Turnings). Wyjatki: wyjscie w bieg (silnik prowadzi)
	// i nasz odczyt surowy (okno wyjscia czyta W przez MvRawState).
	inline bool TravMaskMoveKey(bool isMoveKey, bool travActive, bool runExit, bool rawRead) {
		return isMoveKey && travActive && !runExit && !rawRead;
	}
	// ---------- 24.10 (log NB 49176-49388, schodki swiatyni w Jarkendarze): straznik oscylacji ----------
	// Ping-pong ok. 3 m przy wyjsciu w bieg i przy pedzie skoku: silnik (kolizja) przestawia bohatera do tylu, nasza
	// korekta wraca go, silnik znow do tylu... (19 korekt "skok pozycji po naszym ruchu" w 0,12 s, w kazdej klatce render
	// pokazywal raz jedna, raz druga pozycje). Zasada: najwyzej jedna korekta pozycji na klatke (zdFrame - odstep miedzy
	// renderami) i ta sama korekta (z tego samego miejsca w to samo albo z powrotem) w kolejnych klatkach maxRepeats
	// razy = oscylacja: korekty wstrzymane na holdFrames klatek, zostaje pozycja silnika (ostatnia stabilna dla kolizji),
	// kazdy przypadek w logu.
	struct HeroOscParams {
		float minFix = 100.0f;       // cm - mniejsze korekty nie licza sie do oscylacji (zwykle dosuwanie)
		float sameTol = 40.0f;       // cm - "to samo miejsce" (cel silnika dryfuje z biegiem o kilka cm na klatke)
		int maxRepeats = 2;          // powtorzen pod rzad - trzecia taka sama korekta to juz oscylacja
		unsigned gapFrames = 2;      // klatek miedzy korektami najwyzej tyle = "pod rzad"
		unsigned holdFrames = 30;    // klatek bez korekt po wykryciu (ok. 0,5 s)
	};
	enum HeroOscVerdict { HOSC_APPLY = 0, HOSC_SAME_FRAME, HOSC_OSCILLATION, HOSC_HOLD };
	struct HeroOscState {
		bool haveFix = false;
		unsigned fixFrame = 0;
		float from[3] = { 0.0f, 0.0f, 0.0f }, to[3] = { 0.0f, 0.0f, 0.0f };
		int repeats = 0;
		bool holding = false;
		unsigned holdUntil = 0;
		int trips = 0;               // wykryte oscylacje (log)
	};
	// Czy wykonac korekte from -> to w klatce frame. HOSC_APPLY zapisuje ja jako ostatnia.
	inline HeroOscVerdict HeroOscDecide(HeroOscState& s, unsigned frame, const float from[3], const float to[3],
		const HeroOscParams& p) {
		if (s.holding && frame < s.holdUntil)
			return HOSC_HOLD;
		s.holding = false;
		if (s.haveFix && s.fixFrame == frame)
			return HOSC_SAME_FRAME;
		float fix = HgDist(from, to);
		bool recent = s.haveFix && frame > s.fixFrame && frame - s.fixFrame <= p.gapFrames;
		float tolFrom = 0.5f * fix > p.sameTol ? 0.5f * fix : p.sameTol;
		bool again = recent && fix >= p.minFix && HgDist(to, s.to) <= p.sameTol && HgDist(from, s.from) <= tolFrom;
		bool back = recent && fix >= p.minFix && HgDist(to, s.from) <= p.sameTol && HgDist(from, s.to) <= tolFrom;
		s.repeats = again || back ? s.repeats + 1 : 0;
		if (s.repeats >= p.maxRepeats) {
			s.holding = true;
			s.holdUntil = frame + p.holdFrames;
			s.repeats = 0;
			s.haveFix = false;
			s.trips++;
			return HOSC_OSCILLATION;
		}
		s.haveFix = true;
		s.fixFrame = frame;
		for (int i = 0; i < 3; i++) {
			s.from[i] = from[i];
			s.to[i] = to[i];
		}
		return HOSC_APPLY;
	}
	inline const char* HeroOscVerdictName(HeroOscVerdict v) {
		return v == HOSC_APPLY ? "wykonana" : v == HOSC_SAME_FRAME ? "druga w tej klatce - pominieta" :
			v == HOSC_OSCILLATION ? "oscylacja - zatrzymane" : "wstrzymane po oscylacji";
	}

	// ---------- 24.10: ped skoku gry tylko przez predkosc fizyki ----------
	// Dawniej w fazie animacji (T_*RUN*_2_JUMP, bez fizyki) ped dosuwal pozycje SetPositionWorld z kolizja: przy sciance
	// / stopniu tuz przed bohaterem kolizja przenosila go 2,5-3,6 m do tylu ("ped - kolizja przeniosla bohatera o 351 cm
	// przy dosunieciu 5 cm"), my cofalismy, a koniec pedu zerowal lot i ped startowal od nowa w nastepnej klatce (9x
	// "Skok gry: ped 850 cm/s" w 0,07 s - ping-pong). Teraz: w fazie animacji nic (ruch korzenia gry), w fazie fizyki
	// predkosc ciala sztywnego wzdluz lotu = animacja + (cel - animacja) * fade, fade z wolnej drogi przed bohaterem
	// (lookTime s lotu z celem; przy minClear cm i blizej - sama predkosc gry): przy przeszkodzie dodatek wygasa,
	// zamiast wpychac w nia.
	struct TravMomentumFadeParams {
		float lookTime = 0.25f;      // s lotu z predkoscia celu - tyle drogi ma byc wolne dla pelnego pedu
		float minClear = 25.0f;      // cm (od promienia ciala) - blizej przeszkody bez dodatku
	};
	inline float TravMomentumFade(float clear, float target, const TravMomentumFadeParams& p) {
		float look = target * p.lookTime;
		if (look <= p.minClear + 1.0f)
			return clear > p.minClear ? 1.0f : 0.0f;
		float u = (clear - p.minClear) / (look - p.minClear);
		return u < 0.0f ? 0.0f : u > 1.0f ? 1.0f : u;
	}
	// Nowa predkosc wzdluz lotu (cm/s) przy obecnej va: podniesiona do want, a przy wygaszaniu (fade < 1) nasz dodatek
	// ponad predkosc gry zdjety; predkosci gry (do anim) nie ruszamy. target <= 0 - bez zmian.
	inline float TravMomentumPhysicsVel(float target, float anim, float va, float fade) {
		if (target <= 0.0f)
			return va;
		float base = anim < target ? anim : target;
		float want = base + (target - base) * fade;
		if (va < want)
			return fade >= 1.0f || va >= base ? want : va;   // przy przeszkodzie wolniejszego od gry (zderzenie) nie pchamy

		if (fade < 1.0f && va > want && va > base)
			return want > base ? want : base;
		return va;
	}

	// ---------- 24.10: ramka animacji petli biegu (zCModelAni::aniBBox3DObjSpace) ----------
	// Petle ruchu W3 (HUMANS_ZDF_W3_LOCO / _SPRINT, make_loco.py) mialy ramke liczona wzgledem korzenia z 1. klatki - z cala
	// droga cyklu: S_RUNL truchtu W3 do +310 cm, sprintu W3 do +466 cm przed postacia (HUMANS-S_RUNL gry +47,
	// HUMANS_SPRINT +78). Kolizja postaci liczona od nowa (wlaczenie kolizji po przeskoku, nasze przestawienie z
	// kolizja) wypychala bohatera do tylu o ok. (przod ramki - wolna droga): 326 cm w sprincie, 108 w truchcie, 0 w biegu
	// gry. Nowe paczki maja ramke bez drogi korzenia (do ok. 90 cm); straznik dla starych / obcych petli: wyjscie w bieg
	// z petla, ktorej ramka siega dalej niz maxFront przed korzen - zablokowane (koniec w postawie, bieg przejmie gra).
	struct TravLoopBoxParams {
		float maxFront = 150.0f;     // cm przed korzeniem (gra: 47-97, nowe W3: 72-91)
	};
	inline bool TravLoopBoxTooLong(float bboxMaxZ, const TravLoopBoxParams& p) {
		return bboxMaxZ > p.maxFront;
	}
	// Szacowane wypchniecie do tylu (cm) przy ramce siegajacej front cm przed korzen i wolnej drodze clear cm.
	inline float TravLoopBoxPush(float front, float clear) {
		return front > clear ? front - clear : 0.0f;
	}
	// ---------- 24.10: najwyzsza wspinaczka (log: stopien 350, gora 361 cm - "za wysoko" z biegu, a wspiecie gry z miejsca
	// siega 389-401 cm: GetJumpUpHeight 234 + wzrost 168-178) ----------
	// Wspiecie naszym klipem (z biegu i z miejsca) do wiekszej z wartosci: base (TravLimits::maxHeight) i krawedzi skoku
	// z miejsca gry x margin; najwyzej cap. native <= 0 (nieznane) - base.
	struct TravClimbMaxParams {
		float margin = 1.12f;
		float cap = 460.0f;
	};
	inline float TravClimbMaxHeight(float base, float native, const TravClimbMaxParams& p) {
		float v = native > 0.0f ? native * p.margin : 0.0f;
		v = v > p.cap ? p.cap : v;
		return v > base ? v : base;
	}
	// ---------- 24.10: pewny grunt pod obrysem postaci (szpary miedzy skrzyniami stosu 2x2 - bohater utykal w szczelinie) ----------
	// drops[0] = srodek, drops[1..n-1] = punkty obwodu (cm: stopy - ziemia pod punktem, ujemne = ziemia wyzej, >= 1e8 = brak
	// ziemi). Niepewnie: srodek w szczelinie (gleboko przy plaskim obwodzie), ziemia wyraznie nad stopami (w przeszkodzie)
	// albo za malo podpartych punktow obwodu.
	struct TravFootingParams {
		float centerDeep = 12.0f;    // srodek najwyzej tyle pod stopami
		float ringDeep = 15.0f;      // punkt obwodu podparty: najwyzej tyle pod stopami
		float above = 25.0f;         // ziemia najwyzej tyle nad stopami (wyzej = stopy w przeszkodzie)
		int minRing = 2;             // podpartych punktow obwodu co najmniej (z 4; beczka - okragla gora)
	};
	inline bool TravFootingOk(const float* drops, int n, const TravFootingParams& p, int* ringOk = nullptr) {
		if (!drops || n <= 0)
			return false;
		int ok = 0;
		for (int i = 1; i < n; i++)
			if (drops[i] <= p.ringDeep && drops[i] >= -p.above)
				ok++;
		if (ringOk)
			*ringOk = ok;
		bool center = drops[0] <= p.centerDeep && drops[0] >= -p.above;
		return center && ok >= (n - 1 < p.minRing ? n - 1 : p.minRing);
	}
	// Zaklinowanie po naszym ruchu: W trzymane, bohater stoi (przesuniecie w oknie najwyzej maxMove) od window s, grunt
	// pod nim niepewny - wypchniecie na najblizszy pewny grunt.
	struct TravStuckParams {
		float window = 0.5f;         // s bez ruchu
		float maxMove = 5.0f;        // cm w oknie
		float after = 3.0f;          // s po koncu naszego ruchu - tylko wtedy
	};
	inline bool TravStuck(float sinceEnd, float stillT, bool wHeld, bool footingOk, const TravStuckParams& p) {
		return sinceEnd >= 0.0f && sinceEnd <= p.after && wHeld && !footingOk && stillT >= p.window;
	}
}
