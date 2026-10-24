// Logika bez silnika: uniki i przewroty (moves_w3.ini) - definicje, wybor kierunku, okno nietykalnosci.
#pragma once
#include <cmath>
#include <string>
#include <vector>

namespace zdcore {
	enum MoveDir { MOVE_BACK, MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT };

	struct MoveDef {
		std::string id;                  // RODZAJ_KIERUNEK, np. DODGE_L, ROLL_B
		std::string overlay;             // nakladka z animacja ruchu (podmienia T_*PARADEJUMPB)
		std::string overlayW3;           // wariant z koncem w gardzie stylu W3 (gdy bohater stoi w nim); pusty = brak
		float trajFps = 25.0f;
		std::vector<float> traj;         // x,z (cm) w ukladzie voba na starcie: x w prawo, z do przodu
		float iframeFrom = 0.0f, iframeTo = 0.0f;   // nietykalnosc (s od startu)
		float cancel = 1.0f;             // po tylu sekundach sterowanie wraca (reszta animacji to powrot do postawy)
		float hitTime = -1.0f;           // kontra: chwila ciecia (s) - plugin zadaje wtedy obrazenia; -1 = bez ciosu
		float reach = -1.0f;             // kontra: od voba do punktu kontaktu (ostrze / bark / stopa) w chwili ciosu, cm; -1 = brak
		float exitTime = -1.0f;          // unik / przewrot: od tej chwili (s) atak / blok / kolejny ruch z bufora przerywa ruch; -1 = brak (Core/Evade)
	};

	// Sekcje [RODZAJ_K] z overlay=, traj=, opcjonalnie overlayW3=, trajFps=, iframes=od,do, cancel=, hit=, reach=, exit=.
	// Bledne wpisy pomijane.
	std::vector<MoveDef> ParseMoves(const std::string& text, std::vector<std::string>& errors);

	// Ruch danego rodzaju (DODGE/ROLL) w kierunku; nullptr gdy brak w pliku.
	const MoveDef* FindMove(const std::vector<MoveDef>& defs, const std::string& kind, MoveDir dir);

	// Kierunek z wcisnietych klawiszy: bok ma pierwszenstwo, potem tyl, przod; nic (albo lewo i prawo) = tyl.
	MoveDir MoveDirFromKeys(bool left, bool right, bool forward, bool back);
	// Przewrot: gdy jego klawisz jest tez skokiem gry (Alt), tylko z bokiem albo tylem - sam / z przodem = -1 (skok).
	int RollDirFromKeys(bool sharesJump, bool left, bool right, bool forward, bool back);

	// Dwuklik kierunku (unik / przewrot bez klawisza ruchu): drugie zbocze tego samego kierunku najwyzej 'window' s po
	// pierwszym, bez zbocza innego kierunku pomiedzy. down[] w kolejnosci MoveDir. Nieuzbrojony (bez broni / Shiftu)
	// tylko sledzi klawisze - trzymany w chwili uzbrojenia nie jest zboczem. Dwa kierunki w jednej klatce kasuja okno.
	struct DoubleTap {
		bool prev[4] = { false, false, false, false };
		int lastDir = -1;                // kierunek pierwszego wcisniecia, -1 = brak okna
		float age = 0.0f;                // s od pierwszego wcisniecia
	};
	int DoubleTapStep(DoubleTap& s, const bool down[4], float dt, float window, bool armed);

	// Bohater trzyma akcje (sterowanie Gothica 1: postawa S_xHATTACK, stan BS_HIT), ale nie wyprowadza ciosu - zadna
	// animacja T_*ATTACK* poza przejsciami T_*_2_*. Wtedy unik / przewrot moze wystartowac jak z postawy.
	bool AttackHoldIdle(const std::vector<std::string>& activeAnis);

	inline bool MoveInvulnerable(const MoveDef& d, float t) {
		return t >= d.iframeFrom && t <= d.iframeTo;
	}

	// Kontra: chwila ciecia minela miedzy poprzednia a ta klatka (raz na ruch).
	inline bool MoveHitDue(const MoveDef& d, float tPrev, float t) {
		return d.hitTime >= 0.0f && tPrev < d.hitTime && t >= d.hitTime;
	}

	// Przeciwnik (ex,ez) blisko bohatera (hx,hz) i przed nim: kierunek (dirX,dirZ) znormalizowany, minDot = cos kata.
	inline bool CounterInReach(float hx, float hz, float dirX, float dirZ, float ex, float ez, float maxDist, float minDot) {
		float dx = ex - hx, dz = ez - hz, l2 = dx * dx + dz * dz;
		if (l2 > maxDist * maxDist)
			return false;
		if (l2 < 1.0f)
			return true;
		return (dx * dirX + dz * dirZ) / std::sqrt(l2) > minDot;
	}

	// Klawisz skoku (Alt) w walce: Alt+W czeka na drugie wcisniecie Alt (podwojne = przewrot w przod) albo na atak
	// (atak z wyskoku), sam Alt - na atak (gdy atak z wyskoku jest dostepny). Bez nich po oknie skok gry. Alt z bokiem /
	// tylem (przewrot od razu) idzie poza oknem.
	enum RollTapResult { ROLLTAP_NONE, ROLLTAP_WAIT, ROLLTAP_ROLL, ROLLTAP_JUMP, ROLLTAP_ATTACK };
	struct RollTap {
		bool waiting = false;
		float left = 0.0f;               // s do konca okna
		bool forward = false;            // okno otwarte z przodem (drugie Alt = przewrot w przod)
		float age = 0.0f;                // s od pierwszego wcisniecia
		float held = -1.0f;              // pierwsze wcisniecie puszczone po tylu s (-1 = trzymane) - diagnostyka
	};
	// Okna klawisza skoku. Z przodem (Alt+W: drugie Alt albo atak) dluzsze - podwojne stukniecie bywa wolniejsze niz
	// 0,22 s (log 15.10: drugie Alt 0,33 s po pierwszym = skok gry); sam Alt bez kierunku czeka tylko na atak (Alt
	// i atak prawie naraz) - krotko, zeby skok gry nie czekal dluzej niz dotad.
	struct JumpTapWindows {
		float doubleTap = 0.30f;
		float attack = 0.22f;
	};
	// Okna z ustawienia JumpAttackWindow (21.10): sam Alt czeka na atak attackWindow s, z przodem (Alt+W: drugie Alt
	// albo atak) co najmniej doubleTapMin.
	inline JumpTapWindows JumpTapWindowsFor(float attackWindow, float doubleTapMin = 0.40f) {
		JumpTapWindows w;
		w.attack = attackWindow;
		w.doubleTap = attackWindow > doubleTapMin ? attackWindow : doubleTapMin;
		return w;
	}
	// Bok z klawiszy: strafe (Q/E) zawsze, obrot (A/D, GAME_LEFT/RIGHT - tez mysz) tylko bez biegu do przodu -
	// w biegu z W obrot to skret, nie zamiar ruchu w bok (inaczej Alt+W, Alt w zakrecie dawalo przewrot w bok).
	inline void SideKeys(bool strafeL, bool turnL, bool strafeR, bool turnR, bool forward, bool& left, bool& right) {
		left = strafeL || (turnL && !forward);
		right = strafeR || (turnR && !forward);
	}
	// Bok dla uniku (osobny klawisz uniku): strafe i obrot zawsze, tez z W - unik nie dzieli klawisza z przewrotem w przod.
	inline void DodgeSideKeys(bool strafeL, bool turnL, bool strafeR, bool turnR, bool& left, bool& right) {
		left = strafeL || turnL;
		right = strafeR || turnR;
	}

	struct JumpTapInput {
		bool press = false;              // zbocze klawisza skoku
		bool forwardOnly = false;        // sam przod (bez boku i tylu)
		bool noDir = false;              // bez kierunku (lewo + prawo naraz tez)
		bool attack = false;             // zbocze klawisza ataku gry
		bool rollArmed = false;          // przewrot w przod dostepny
		bool attackArmed = false;        // atak z wyskoku dostepny (bron biala 1H/2H, ruch w ini)
		bool down = false;               // klawisz skoku trzymany (diagnostyka: kiedy puszczony)
	};
	// Klatka. Bez czekania: press z przodem (przewrot albo atak dostepny; okno doubleTap) albo bez kierunku (atak
	// dostepny; okno attack) -> WAIT (polknac skok), a z atakiem w tej samej klatce -> ATTACK; inaczej NONE.
	// W czekaniu: atak (gdy dostepny) -> ATTACK, press ->
	// ROLL (przod przy pierwszym albo drugim wcisnieciu i przewrot dostepny) albo JUMP; W wcisniete w oknie bez przodu
	// (przewrot dostepny) wydluza je do doubleTap od pierwszego wcisniecia; koniec okna -> JUMP (oddac skok grze),
	// poza tym WAIT.
	RollTapResult JumpTapStep(RollTap& s, const JumpTapInput& in, float dt, const JumpTapWindows& w);
	// Oba okna rowne 'window'.
	RollTapResult JumpTapStep(RollTap& s, const JumpTapInput& in, float dt, float window);
	// Sam przewrot w przod (bez ataku z wyskoku): press = zbocze klawisza przewrotu, gdy przejecie skoku jest mozliwe.
	RollTapResult RollTapStep(RollTap& s, bool press, bool forwardOnly, float dt, float window);
	// Klawisz skoku wcisniety w walce (poza oknem): gra ma go nie widziec - bok / tyl (przewrot), sam przod przy
	// przewrocie albo ataku z wyskoku (czekanie), bez kierunku przy ataku z wyskoku (czekanie).
	bool JumpKeyTakenOver(bool left, bool right, bool forward, bool back, bool rollArmed, bool attackArmed);

	// ---------- Klawisz skoku w walce: skok gry OD RAZU (runda 7, dawny tryb z czekaniem wyzej) ----------
	// Pierwsze Alt z samym przodem (przewrot albo atak z wyskoku dostepny) albo bez kierunku (atak dostepny) idzie do gry
	// (skok od razu) i otwiera okno: drugie Alt z przodem (przewrot dostepny) = przerwac skok i przewrot w przod, atak =
	// przerwac skok i atak z wyskoku; Alt i atak w tej samej klatce = atak z wyskoku od razu. Bok / tyl - przewrot jak
	// dotad (poza oknem). Plugin ocenia, czy skok da sie jeszcze przerwac (JumpCancelDecide).
	enum JumpNowResult {
		JUMPNOW_NONE,                    // nic
		JUMPNOW_JUMP,                    // pierwsze Alt - skok gry od razu, okno otwarte
		JUMPNOW_WAIT,                    // okno trwa
		JUMPNOW_ROLL,                    // drugie Alt z przodem - przewrot w przod (przerwac skok)
		JUMPNOW_ATTACK,                  // atak w oknie albo akord - atak z wyskoku (przerwac skok)
		JUMPNOW_CLOSED                   // okno minelo (skok gry trwa dalej)
	};
	JumpNowResult JumpTapNowStep(RollTap& s, const JumpTapInput& in, float dt, const JumpTapWindows& w);
	// Hak wejscia: w oknie polykane tylko nowe wcisniecie skoku (pierwsze puszczone) - pierwsze, trzymane, idzie do gry.
	inline bool JumpTapSwallow(const RollTap& s) {
		return s.waiting && s.held >= 0.0f;
	}
	// Runda 10: stan z chwili wcisniecia skoku. Silnik czyta klawisz w AI (hak GetState) i w tej samej klatce startuje
	// skok gry (stan ciala 8), a MovesUpdate (Game_Loop, po AI) widzial juz skok i nie otwieral okna przewrotu / ataku.
	// Hak zapamietuje przy pierwszej obserwacji nowego wcisniecia, czy ruch mogl wtedy ruszyc; MovesUpdate zuzywa to
	// przy zboczu klawisza (raz), bez haka - stan biezacy.
	struct JumpPressLatch {
		bool seen = false;
		bool canWait = false;
	};
	inline void JumpPressObserve(JumpPressLatch& l, bool canWaitNow) {
		if (!l.seen) {
			l.seen = true;
			l.canWait = canWaitNow;
		}
	}
	inline bool JumpPressCanWait(JumpPressLatch& l, bool canWaitNow, bool edge) {
		bool latched = l.seen && l.canWait && edge;
		l = JumpPressLatch();
		return canWaitNow || latched;
	}
	// Animacje skoku gry: odbicie (T_RUNL_2_JUMP, T_1HRUNR_2_JUMP, T_STAND_2_JUMP, T_JUMPB; bez JUMPUP i naszych
	// T_xPARADEJUMPB) i lot (S_JUMP, S_1HJUMP, S_FALL, S_FALLDN, T_JUMP_2_FALLDN).
	bool IsJumpTakeoffAni(const std::string& name);
	bool IsJumpFlightAni(const std::string& name);
	enum JumpCancel {
		JUMPCANCEL_BEFORE,               // skok jeszcze nie ruszyl (zadanie czeka na krok) - skasowac, ruch z ziemi
		JUMPCANCEL_TAKEOFF,              // odbicie, nisko - przerwac (fizyka wylaczona, vob schodzi na ziemie w ruchu)
		JUMPCANCEL_LATE                  // lot / fizyka / wysoko - za pozno
	};
	struct JumpCancelInput {
		bool started = false;            // skok gry ruszyl (animacja skoku, stan skoku albo lot)
		bool takeoff = false;            // animacja odbicia aktywna
		bool flight = false;             // animacja lotu aktywna
		bool physics = false;            // fizyka lotu wlaczona
		float height = 0.0f;             // stopy nad ziemia (cm)
	};
	struct JumpCancelLimits {
		float maxHeight = 50.0f;         // poza odbiciem: najwyzej tyle nad ziemia
		float takeoffMaxHeight = 90.0f;  // w odbiciu (okno 0,30 s - ok. 70 cm): najwyzej tyle
	};
	JumpCancel JumpCancelDecide(const JumpCancelInput& in, const JumpCancelLimits& lim);
	// Zejscie voba na ziemie w ruchu po przerwanym skoku: wysokosc ponad postawe pozostala po t s (smoothstep w dropTime).
	float MoveDropLeft(float drop, float t, float dropTime);

	// Kamera Gothica (zCMovementTracker) wygladza pozycje gracza i kamery srednia wykladnicza o stalej ok. N * 25 ms
	// (N = liczba probek: numPreSamplesPlayerPos 10, numSamplesCam = inertiaTrans * 99 + 1, domyslnie 10-11). Ruch
	// szybko w strone kamery (unik / przewrot w tyl, 5-8 m/s) wyprzedza ja o 1,5-2 m - bohater wjezdza na dolna krawedz
	// ekranu. true, gdy ruch (koniec toru trajX w prawo, trajZ do przodu voba patrzacego w dirX,dirZ) idzie w strone
	// kamery (toCam = bohater -> kamera w poziomie, dowolna dlugosc): kat ponizej 60 st. i droga co najmniej minDist.
	bool MoveTowardCamera(float dirX, float dirZ, float trajX, float trajZ, float toCamX, float toCamZ, float minDist);
	// Liczba probek wygladzania na czas takiego ruchu: najwyzej cap (krocej = kamera blizej za bohaterem), co najmniej 1.
	inline int CameraAssistSamples(int current, int cap) {
		int n = current < cap ? current : cap;
		return n < 1 ? 1 : n;
	}

	// Obrazenia ciosu z czesci obrazen bronia (repel: bark / kopniak): (obrazenia broni + sila) * share, co najmniej 1
	// (ochrone i minimalne obrazenia liczy dalej gra).
	float StrikeDamage(int weaponDamage, int strength, float share);

	// Animacja chodu / biegu / postawy (S_*RUN*, S_*WALK*, T_*RUN*_2_*, T_*WALK*...) - nie skok, trafienie ani atak.
	bool IsLocomotionAni(const std::string& name);
	// Animacja ruchu znikla w trakcie: wznowic ja, gdy nasza nakladka dalej jest na modelu, bohater nie stracil HP,
	// a warstwe 1 zajmuje tylko postawa / bieg (oCAniCtrl_Human::_Stand przy movlocku) albo nic (cudza nakladka
	// zatrzymala wszystkie animacje) - najwyzej maxResumes razy na ruch.
	bool MoveResumable(const std::vector<std::string>& layerOne, bool hpDropped, bool overlayOn, int resumes, int maxResumes);

	// Warianty ruchu: sekcje base, base2, base3... (np. JUMPATK_F, JUMPATK_F2) - id rowne base albo base z numerem.
	bool IsMoveVariant(const std::string& id, const std::string& base);
	// Warianty dla broni w kolejnosci z pliku: 2H z base2H, a bez nich (i dla 1H) z base1H.
	std::vector<const MoveDef*> WeaponMoveVariants(const std::vector<MoveDef>& defs, const std::string& base1H,
		const std::string& base2H, bool twoHanded);
	// Skala krokow wypadu do ciosu, przy ktorej cel jest w zasiegu (need, pos, neg jak w LungeScale), bez przycinania:
	// < 0 = cel blizej, niz zostawia odrzut; tor bez wypadu = 1.
	float LungeNeedScale(float need, float pos, float neg);
	// Wypad pasuje do celu: przy tej skali nogi z animacji nie slizgaja sie wyraznie (dreptanie w miejscu / ciagniecie).
	inline bool LungeFitsWell(float scale) {
		return scale >= 0.6f && scale <= 1.5f;
	}
	// Wybor wariantu ataku. scales: skala wypadu kazdego wariantu do celu (LungeNeedScale) albo puste (bez celu - pasuja
	// wszystkie). Losowo (rnd) sposrod pasujacych, przy kilku bez powtorzenia poprzedniego (lastId); gdy nie pasuje
	// zaden - wariant o skali proporcjonalnie najblizszej 1. nullptr gdy brak wariantow.
	const MoveDef* PickMoveVariant(const std::vector<const MoveDef*>& vars, const std::vector<float>& scales, unsigned rnd,
		const std::string& lastId);

	// Atak z wyskoku: JUMPATK_F, _F2... (1H), JUMPATK2H_F, _F2... (2H; bez nich 1H). Find = pierwszy wariant, nullptr
	// gdy brak w pliku.
	inline bool IsJumpAttack(const MoveDef& d) {
		return d.id.rfind("JUMPATK", 0) == 0;
	}
	std::vector<const MoveDef*> JumpAttackVariants(const std::vector<MoveDef>& defs, bool twoHanded);
	const MoveDef* FindJumpAttack(const std::vector<MoveDef>& defs, bool twoHanded);
	// Atak w biegu (zamiast ataku w biegu gry w stylu W3): RUNATK_F, _F2... (1H), RUNATK2H_F, _F2... (2H; bez nich 1H).
	inline bool IsRunAttack(const MoveDef& d) {
		return d.id.rfind("RUNATK", 0) == 0;
	}
	std::vector<const MoveDef*> RunAttackVariants(const std::vector<MoveDef>& defs, bool twoHanded);
	const MoveDef* FindRunAttack(const std::vector<MoveDef>& defs, bool twoHanded);

	// Kroki ruchu po terenie: zbocza jak chod Gothica (do ok. 50 st.), stopnie; sciany i urwiska zatrzymuja.
	enum StepVerdict { STEP_OK, STEP_WALL, STEP_DROP };
	struct TerrainLimits {
		float maxSlope = 1.19f;          // tan najwiekszego kata podejscia (50 st. - zMV_MAX_GROUND_ANGLE_WALK)
		float minNormalY = 0.64f;        // cos tego kata: bardziej strome podloze pod gore = sciana
		float stepUp = 30.0f;            // stopien / kamien w jednym kroku (cm)
		float maxDrop = 100.0f;          // glebszy uskok w jednym kroku = urwisko (bez zeskakiwania)
	};
	// rise = ziemia celu - ziemia obecna, horiz = krok w poziomie (cm), normalY = skladowa pionowa normalnej podloza celu.
	StepVerdict ClassifyStep(float rise, float horiz, float normalY, const TerrainLimits& lim);
	// Laczny wznios od startu (po drodze pathLen cm) - dluzszy podjazd bardziej stromy niz pozwala kat = sciana.
	inline bool ClimbWithinSlope(float climb, float pathLen, const TerrainLimits& lim) {
		return climb <= lim.maxSlope * pathLen + lim.stepUp;
	}

	// Utkniecie ruchu: planned = krok z toru (cm), done = wykonany po kolizji, timeLeft = s do konca ruchu.
	// true = przerwac (plynnie do postawy) - ruch w scianie zamiast przebierac nogami w miejscu. Pod koniec (przejscie
	// do postawy) nie przerywa, chwilowa przeszkoda i slizg wzdluz sciany tez nie.
	struct MoveBlock {
		float lost = 0.0f;               // niewykonana droga (cm), wygasa przy postepie
	};
	bool MoveBlockStep(MoveBlock& b, float planned, float done, float timeLeft);

	// Wypad kontry. Droga do przodu (z) toru miedzy t0 i t1 (s): przyrosty dodatnie (wypad) i ujemne (odrzut odbicia).
	void TrajForward(const std::vector<float>& xz, float fps, float t0, float t1, float& pos, float& neg);
	// Mnoznik krokow wypadu do chwili ciosu, by przejsc 'need' cm (odrzut neg zostaje bez zmian); [0, maxScale].
	float LungeScale(float need, float pos, float neg, float maxScale);
	// Przeciwnik osiagalny wypadem (need = odleglosc - zasieg ciosu), z zapasem tol (cm).
	inline bool LungeReachable(float need, float pos, float neg, float maxScale, float tol) {
		return need <= neg + pos * maxScale + tol;
	}
	// Obrot (rad) o najwyzej maxStep w strone target, krotsza droga.
	float TurnToward(float yaw, float target, float maxStep);
	// Odepchniecie (repel): przesuniecie przeciwnika po t s z dist cm w dur s - szybko na poczatku, wygaszone.
	inline float PushOffset(float t, float dist, float dur) {
		if (t <= 0.0f || dur <= 0.0f)
			return t > 0.0f ? dist : 0.0f;
		float u = t >= dur ? 0.0f : 1.0f - t / dur;
		return dist * (1.0f - u * u * u);
	}

	// Finisher zamiast powalenia czlowieka: tryb 0 = nigdy, 1 = tylko trwale wrogi (arena, bojki i questy bez zmian),
	// 2 = kazdy poza przyjacielem. Postawa jak w silniku: 0 wrogi, 1 zly, 2 neutralny, 3 przyjazny.
	inline bool KnockoutFinisherAllowed(int mode, int permAttitude) {
		return mode == 1 ? permAttitude == 0 : mode == 2 ? permAttitude != 3 : false;
	}

	// Style walki: indeks -1 = AUTO, cykl AUTO -> 0 -> ... -> count-1 -> AUTO (step +1 / -1).
	int CycleStyle(int current, int count, int step);
	// Styl W3 sam (AUTO) od progu talentu w procentach.
	inline bool AutoW3Style(int talentPercent, int minTalent, bool enabled, bool available) {
		return enabled && available && talentPercent >= minTalent;
	}
	// Nazwa do wyswietlenia: HUMANS_ZDF_W3_1H/2H, HUMANS_ZDF_NAT_* (Gothic), reszta = zEBA.
	std::string StyleLabel(const std::string& overlay);

	// Wartosc z Gothic.ini [KEYS] (pary bajtow little-endian, np. "38009d00") -> kody klawiszy bez zer.
	std::vector<int> ParseKeyBinding(const std::string& hex);

	// ---------- Unik / przewrot wzgledem celu (W3), sterowanie, przenikanie postaci ----------
	// Kierunek z id ruchu (sufiks _F/_B/_L/_R); -1 = brak (atak, kontra z numerem wariantu itp.).
	int MoveDirOfId(const std::string& id);
	// Unik albo przewrot (DODGE_* / ROLL_*) - tylko one sledza cel, daja sie sterowac i przenikaja postacie.
	inline bool IsEvadeMove(const MoveDef& d) {
		return d.id.rfind("DODGE_", 0) == 0 || d.id.rfind("ROLL_", 0) == 0;
	}
	// Ruch w bok / w tyl zawsze wzgledem celu (krazenie, odskok); w przod tylko na zyczenie - przewrot w przod obok
	// wroga to flankowanie, sledzenie wciagaloby bohatera w srodek wroga.
	inline bool MoveTracksTarget(MoveDir d, bool trackForward) {
		return d != MOVE_FORWARD || trackForward;
	}
	// Kat (rad) jak w Dodge.cpp: yaw = atan2(x, z) - 0 = +Z (przod), +pi/2 = +X (prawo).
	inline float YawTo(float hx, float hz, float ex, float ez) {
		return std::atan2(ex - hx, ez - hz);
	}
	// b - a sprowadzone do [-pi, pi] (krotsza droga).
	float AngleDiff(float a, float b);

	struct TrackParams {
		float turnRate = 8.0f;           // rad/s obrotu do celu
		float minDist = 90.0f;           // blizej (przenikanie, mijanie) - bez obrotu
		float startAngle = 1.75f;        // cel dalej od osi niz tyle na starcie (ok. 100 st.) - bez sledzenia (bez piruetu)
		float releaseAngle = 1.92f;      // cel dalej niz tyle w trakcie (ok. 110 st., minal go) - koniec sledzenia
		float maxDist = 1200.0f;         // cel dalej - bez sledzenia
	};
	// Sledzenie na start ruchu: cel w zasiegu i nie za plecami (yaw = kierunek voba).
	bool TrackStartAllowed(float yaw, float hx, float hz, float ex, float ez, const TrackParams& p);
	// Klatka sledzenia: nowy yaw voba - obrot (najwyzej turnRate * dt) przodem do celu + offset (korekta mysza).
	// Blizej niz minDist bez obrotu; cel za plecami (releaseAngle) = released (trwale, bez obrotu do konca ruchu).
	float TrackYawStep(float yaw, float hx, float hz, float ex, float ez, float offset, float dt, const TrackParams& p,
		bool& released);

	// Obrot gry (rad, + = w prawo) jak oCAIHuman::PC_Turnings (0x69A940) + oCAniCtrl_Human::Turn (0x6AE540): st. =
	// 0,1 (turnSpeed gracza) * frameMs * arg; klawisze GAME_LEFT/RIGHT arg = -/+1 (w walce 2) i maja pierwszenstwo;
	// mysz arg = clamp(mouseX * zMouseRotationScale, +-100) * 0,1.
	float GameTurnRad(int keyDir, bool fight, float mouseX, float rotScale, float frameMs);
	// Klawisze obrotu trzymane przy starcie ruchu (A/D jako wybor kierunku uniku) nie obracaja, dopoki nie zostana
	// puszczone - potem kazde wcisniecie obraca.
	struct SteerKeyGate {
		bool heldL = false, heldR = false;
	};
	void SteerGateStart(SteerKeyGate& g, bool left, bool right);
	// -1 lewo, 1 prawo, 0 nic (albo oba).
	int SteerGateDir(SteerKeyGate& g, bool left, bool right);
	// Z celem: korekta mysza jako przesuniecie kierunku sledzenia - delta * scale, najwyzej +-maxOffset.
	inline float SteerOffsetStep(float offset, float delta, float scale, float maxOffset) {
		float o = offset + delta * scale;
		return o > maxOffset ? maxOffset : (o < -maxOffset ? -maxOffset : o);
	}

	// Krok (dx,dz) z p obok postaci (kolo c, promien r): wejscie w kolo zamienione na slizg po jego brzegu (rzut na
	// brzeg + reszta drogi lukiem w strone ruchu - bez hamowania); czolowo strona przeciwna do srodka kola, przy
	// dokladnie czolowym - strona (sideX, sideZ). Z wnetrza kola (wrog wszedl w bohatera) wyjscie wolne, w glab nie.
	void DeflectAroundCircle(float px, float pz, float& dx, float& dz, float cx, float cz, float r, float sideX, float sideZ);
	// Koniec ruchu w postaci: przesuniecie (ox,oz) z p na brzeg kola (c, r); w samym srodku w kierunku (fbX, fbZ).
	// false = p poza kolem.
	bool CircleSeparation(float px, float pz, float cx, float cz, float r, float fbX, float fbZ, float& ox, float& oz);

	// ---------- Anulowanie ruchu blokiem ----------
	// Chwila (s), w ktorej tor przebyl share calej drogi (0 dla toru w miejscu).
	float TrajTimeAtShare(const std::vector<float>& xz, float fps, float share);
	// Od kiedy blok przerywa ruch parada: po ciosie (kontra, ataki z wypadem: hitTime + afterHit), a unik / przewrot -
	// po pathShare drogi, nie przed koncem nietykalnosci; najpozniej cancel.
	float ParadeCancelFrom(const MoveDef& d, float afterHit, float pathShare);
	// Wcisniecie bloku w trakcie ruchu pamietane do okna: trzymane - do skutku, puszczone - jeszcze 'buffer' s.
	// Blok trzymany od startu (kontra odpalona blokiem) bez nowego wcisniecia nic nie robi.
	struct ParadeBuffer {
		float left = 0.0f;
	};
	// true = przerwac teraz (raz na wcisniecie).
	bool ParadeCancelStep(ParadeBuffer& b, bool press, bool held, bool windowOpen, float dt, float buffer);

	// ---------- Atak w biegu: wyjscie prosto w bieg, rozped z wejscia ----------
	// Kiedy (s) atak w biegu przechodzi w bieg (W trzymane): afterHit po ciosie, najpozniej cancel - blend.
	float RunExitTime(const MoveDef& d, float afterHit, float blend);
	// Srednia predkosc toru (cm/s) miedzy t i t + span.
	float TrajSpeedAt(const std::vector<float>& xz, float fps, float t, float span);
	// Rozped: predkosc bohatera przed atakiem / predkosc toru na starcie, w [1, maxRatio] (bez zwalniania).
	float EntrySpeedRatio(float entrySpeed, float clipSpeed, float maxRatio);
	// Mnoznik krokow do przodu: ratio na starcie, liniowo do 1 w chwili ciosu.
	inline float CarryScale(float ratio, float t, float hitTime) {
		if (hitTime <= 0.0f || t >= hitTime)
			return 1.0f;
		return 1.0f + (ratio - 1.0f) * (1.0f - t / hitTime);
	}
}
