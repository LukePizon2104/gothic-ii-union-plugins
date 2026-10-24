// Logika bez silnika: szybki strzal z kuszy w stylu W3 (spec [dokument projektowy - niepublikowany]). Odmowy,
// przebieg strzalu (wejscie -> celowanie -> strzal -> wyjscie) z chwilami z crossbow_w3.ini, przerwania przed / po
// belcie, wybor celu automatycznego, odnowienie wedlug talentu i plan pozyczania wpisow protoAnis (nosniki Znakow).
// Silnik (pocisk, udawanie trybu kuszy, wizual, animacje): zDismember\Crossbow.cpp.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	// ---------- odmowy (kolejnosc sprawdzania jak w XbowGateCheck) ----------
	enum XbowRefuse {
		XR_OK = 0,
		XR_OFF,          // Crossbow=0 / plugin wylaczony
		XR_NOPACK,       // brak HUMANS_ZDF_XBOW (prywatna paczka zDismember_W3.vdf)
		XR_MODEL,        // bohater nie w modelu czlowieka (przemiana)
		XR_DEAD,         // nieprzytomny / martwy
		XR_DIALOG,       // dialog, przerywnik, ekwipunek, blokada ruchu (finisher, medytacja)
		XR_INVENTORY,
		XR_BUSY,         // unik / przewrot / przeskok / finisher / podnoszenie / gest Znaku w toku
		XR_WATER,        // plywanie / nurkowanie / brodzenie (actionMode >= 4)
		XR_AIR,          // skok / upadek / slizg
		XR_MOB,          // przy mobie / drabinie
		XR_MODE,         // tryb broni 5 / 6 / 7 (strzelaj zwykle / rzucaj czar)
		XR_NOXBOW,       // brak zalozonej kuszy
		XR_BOW,          // zalozony luk (v1 tylko kusza)
		XR_TORCH,        // pochodnia w lewej rece (XbowWithTorch=0)
		XR_SHIELD,       // tarcza NB (XbowWithShield=0)
		XR_DUAL,         // bron parowa NB (XbowWithDual=0)
		XR_NOBOLTS,      // brak beltow
		XR_COOLDOWN,     // kusza sie przeladowuje
		XR_ACTIVE,       // strzal juz trwa
		XR_COUNT
	};
	const char* XbowRefuseLabel(int r);     // do logu
	const char* XbowRefuseMessage(int r);   // komunikat dla gracza (cp1250), "" = bez komunikatu

	struct XbowGate {
		bool enabled = true, pack = true, human = true, alive = true;
		bool dialog = false, inventory = false, busy = false;
		bool water = false, airborne = false, mob = false;
		int fmode = 0;                       // fmode bohatera (NB: 8 bron parowa, 9 tarcza)
		bool hasCrossbow = false, isBow = false;
		bool torch = false, shield = false, dual = false;
		bool allowTorch = false, allowShield = true, allowDual = true;
		bool hasBolts = false, createAmmo = false;
		int cooldownLeftMs = 0;
		bool active = false;
	};
	int XbowGateCheck(const XbowGate& g);

	// ---------- chwile klipow (s przy tempie 100 %, crossbow_w3.ini z make_xbow.py) ----------
	struct XbowTimes {
		float inLen = 0.52f;       // XBOW_IN (aim_start_rp): siegniecie, chwyt, uniesienie do celowania
		float loopLen = 1.04f;     // XBOW_LOOP (aim_idle_rp, petla)
		float shootLen = 1.04f;    // XBOW_SHOOT (odrzut + aim_end_rp)
		float grab = 0.20f;        // GrabMs: kusza z plecow do lewej dloni (od startu XBOW_IN)
		float fire = 0.0f;         // FireMs: belt (od startu XBOW_SHOOT)
		float release = 0.56f;     // ReleaseMs: kusza z dloni na plecy (od startu XBOW_SHOOT)
		float turn = 0.21f;        // RotateToTarget W3: obrot do celu na postoju (od startu XBOW_IN)
		float outStart = 0.32f;    // ShootOutMs: poczatek odlozenia w XBOW_SHOOT (po odrzucie) - wyjscie po anulowaniu PPM
		float gripCm = -35.0f;     // GripCm: chwyt kuszy (os X wizualu, cm od poczatku) - skala w dloni wokol niego
	};
	// crossbow_w3.ini: InMs= LoopMs= ShootMs= GrabMs= FireMs= ReleaseMs= TurnMs= ShootOutMs= (ms przy 100 %), GripCm=
	// (cm). Brakujace klucze zostaja domyslne; false = brak ktoregokolwiek klucza dlugosci albo zle wartosci (err).
	bool ParseXbowIni(const std::string& text, XbowTimes& t, std::string& err);
	// Tempo XbowSpeed (%) -> mnoznik czasu (100 % = 1, 200 % = 0,5); poza 25..400 przyciete.
	float XbowTimeScale(int speedPct);

	// ---------- przebieg strzalu ----------
	enum XbowPhase { XP_IDLE = 0, XP_IN, XP_LOOP, XP_SHOOT, XP_COUNT };
	enum XbowIntent { XI_UNDECIDED = 0, XI_TAP, XI_HOLD, XI_RELEASED, XI_CANCEL };
	// Wejscie z zQuickSign (ZDISM_XbowInput): 1 wcisniecie, 2 prog przytrzymania, 3 puszczenie, 4 anulowanie (PPM).
	enum XbowInputPhase { XIN_PRESS = 1, XIN_HOLD = 2, XIN_RELEASE = 3, XIN_CANCEL = 4 };
	const char* XbowPhaseName(int p);

	struct XbowRun {
		int phase = XP_IDLE;
		int intent = XI_UNDECIDED;
		float t = 0.0f;            // s od startu biezacej fazy (czas gry, juz w tempie XbowSpeed)
		float total = 0.0f;        // s od wcisniecia
		bool grabbed = false;      // kusza w dloni
		bool fired = false;        // belt wystrzelony
		bool released = false;     // kusza z powrotem na plecach
		bool aimed = false;        // byla petla celowania
		bool Active() const { return phase != XP_IDLE; }
	};
	// Zdarzenia jednego kroku (Step) - plugin wykonuje je w tej kolejnosci.
	struct XbowEvents {
		bool startLoop = false, startShoot = false;
		bool grab = false, fire = false, release = false;
		bool end = false;
	};
	void XbowStart(XbowRun& r);
	// Wejscie gracza; false = zignorowane (zla faza). Puszczenie przed progiem (XI_UNDECIDED) = stukniecie.
	bool XbowApplyInput(XbowRun& r, int inputPhase);
	// Krok o dt (s czasu gry); scale = XbowTimeScale. W fazie IN po inLen: przytrzymanie -> LOOP, inaczej SHOOT
	// (stukniecie, puszczenie albo brak decyzji). LOOP trwa do puszczenia (XI_RELEASED) - wtedy SHOOT od razu.
	XbowEvents XbowStep(XbowRun& r, const XbowTimes& tm, float scale, float dt);
	// Anulowanie PPM z kusza w dloni (W3 ThrowStop = aim_end_rp): przebieg skacze do odlozenia w XBOW_SHOOT (outStart)
	// bez beltu - fired = true bez zdarzenia fire (bez zuzycia beltu i przeladowania), release i end jak po strzale.
	// false = kuszy jeszcze nie ma w dloni (przed chwytem) albo belt juz poszedl - wtedy zwykle przerwanie.
	bool XbowCancelOut(XbowRun& r, const XbowTimes& tm, float scale);
	// Czas do bełtu od wcisniecia przy stuknieciu (s, z tempem) - do logu i testow.
	float XbowTapFireAt(const XbowTimes& tm, float scale);

	// Przerwania: przyczyna -> co zrobic. Przed beltem (unik, trafienie, ekwipunek, zmiana broni, dialog, PPM) = bez
	// strzalu, bez zuzycia beltu i bez odnowienia, kusza od razu na plecy, animacja wygaszona. Po belcie: unik / przewrot /
	// finisher / ekwipunek / dialog / smierc = animacja przerwana i kusza od razu na plecy; trafienie = animacja dogrywa sie.
	enum XbowCut { XC_HIT = 0, XC_MOVE, XC_INVENTORY, XC_WEAPON, XC_DIALOG, XC_CANCEL, XC_DEATH, XC_RESET, XC_COUNT };
	enum XbowCutAction { XCA_IGNORE = 0, XCA_ABORT_FREE, XCA_ABORT_AFTER };
	int XbowCutVerdict(const XbowRun& r, int cut);
	const char* XbowCutLabel(int cut);

	// ---------- cel automatyczny ----------
	struct XbowCand {
		float dx = 0.0f, dz = 0.0f;   // od bohatera (cm, poziomo)
		float dy = 0.0f;
		bool alive = true, hostile = false, visible = true, focus = false;
	};
	struct XbowAimRules {
		float range = 3000.0f;      // XbowAutoRange (cm)
		float coneDeg = 40.0f;      // XbowAutoCone (pol kata)
		float moveConeDeg = 100.0f; // XbowMoveCone: w ruchu cel tylko tak blisko kierunku biegu
		bool neutral = false;       // XbowAutoNeutral: tez nie-wrodzy
		float maxRise = 600.0f;     // |dy| wieksze = poza (balkon nad / pod bohaterem)
	};
	// Kat (st., 0..180) miedzy kierunkiem (fx, fz) a (dx, dz); 180 przy zerowym wektorze.
	float XbowAngleDeg(float fx, float fz, float dx, float dz);
	// Ocena (mniej = lepiej): kat / stozek + odleglosc / zasieg; < 0 = poza (martwy, nie-wrog, zakryty, za daleko, kat).
	float XbowScore(const XbowCand& c, float camX, float camZ, const XbowAimRules& rules);
	// Indeks celu: fokus (gdy spelnia warunki - takze poza stozkiem kamery, byle przed bohaterem: kat od kierunku
	// bohatera <= 90 st.), inaczej najlepsza ocena w stozku kamery. moving = bohater w ruchu: dodatkowo kat od kierunku
	// biegu (heroX, heroZ) <= moveConeDeg. -1 = brak (strzal w punkt kamery / przed siebie).
	int XbowPickTarget(const std::vector<XbowCand>& c, float camX, float camZ, float heroX, float heroZ, bool moving,
		const XbowAimRules& rules);

	// ---------- odnowienie ----------
	// Poziom talentu kuszy 0..2 (GetTalentSkill / nakladki HUMANS_CBOWT1 / T2), ms z XbowCooldownMs (3 wartosci).
	int XbowCooldownMs(const int ms[3], int talent);

	// ---------- pozyczanie wpisow protoAnis ----------
	// Lista prototypow modelu od bazy (0) do gory: czy nasz (HUMANS_ZDF_*), czy ma wpis pod danym aniID.
	struct XbowProtoInfo {
		bool ours = false;
		bool hasEntry = false;
	};
	enum XbowBorrowWhy { XB_OK = 0, XB_FOREIGN_ABOVE, XB_NO_OURS, XB_EMPTY };
	// Prototyp, w ktorym kladziemy klip: ten, ktory dzis rozwiazuje ID (najwyzszy z wpisem), gdy jest nasz; gdy ID
	// rozwiazuje baza (albo nikt) - najwyzszy nasz. Obcy prototyp z wpisem nad naszymi = -1 (XB_FOREIGN_ABOVE).
	int XbowBorrowSlot(const std::vector<XbowProtoInfo>& protos, int* why);
	const char* XbowBorrowWhyLabel(int why);

	// Nosniki klipow kuszy (0 nowych nazw): ruch (tulow i rece) / postoj (cale cialo) x wejscie / petla / strzal. Te same
	// tablice sa w make_xbow.py (XBOW_ANIS) - test tests_pipeline.py.
	enum XbowClip { XCLIP_IN = 0, XCLIP_LOOP, XCLIP_SHOOT, XCLIP_COUNT };
	const char* XbowClipAni(int clip, bool standing);
	const char* XbowClipName(int clip);

	// Zmienna skryptu ZDF_XBOW_SLOT (zapis gry): bit 0 = kusza wybrana pod F.
	inline bool XbowSlotSelected(int v) { return (v & 1) != 0; }
	inline int XbowSlotValue(int v, bool selected) { return selected ? (v | 1) : (v & ~1); }
}
