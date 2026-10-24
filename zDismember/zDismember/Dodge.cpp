// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static std::vector<zdcore::MoveDef> moveDefs;

	// Trwajacy ruch: animacja w miejscu z nakladki ruchu, vob prowadzony krokami po torze (przyrosty kolejnych probek
	// toru w biezacym kierunku voba) z kolizja, slizgiem wzdluz scian i sledzeniem ziemi.
	struct MoveState {
		const zdcore::MoveDef* def = nullptr;
		std::string overlay;          // z .MDS
		std::string aniName;
		int aniId = -1;
		float t = 0.0f;
		zVEC3 start, dir;             // start; przod voba (poziomy) - kontra obraca go do przeciwnika
		float yOff = 0.0f;            // wysokosc voba nad ziemia
		float startGround = 0.0f;     // ziemia na starcie ruchu
		float ground = 0.0f;          // ziemia pod vobem po ostatnim kroku
		float pathLen = 0.0f;         // przebyta droga w poziomie
		float trajX = 0.0f, trajZ = 0.0f;   // wykonana probka toru (uklad ruchu: x w prawo, z do przodu)
		zdcore::MoveBlock block;      // utkniecie uniku / przewrotu
		zdcore::TerrainLimits terrain;
		float ending = -1.0f;         // >= 0: plynne konczenie (silnik miesza z postawa) - tyle s do zdjecia nakladki
		int weaponMode = 0;
		oCNpc* target = nullptr;      // kontra / atak z wyskoku: przeciwnik (AddRef do konca ruchu)
		float desired = 0.0f;         // kontra: odleglosc do przeciwnika w chwili ciosu (zasieg ciosu + jego promien)
		zCModelAni* ani = nullptr;    // wystartowana animacja ruchu (wersja z naszej nakladki)
		float progress = 0.0f;        // jej postep z ostatniej klatki (wznowienie)
		int resumes = 0;              // wznowienia po zgaszeniu animacji przez silnik / inna wtyczke
		int heroHp = 0;               // HP bohatera z ostatniej klatki (trafienie = przerwanie bez wznawiania)
		bool heroHit = false;
		bool obstacleLogged = false;  // "przeszkoda" w logu raz na ruch
		int obstacleFrames = 0;
		// unik / przewrot wzgledem celu (W3) i sterowanie mysza / klawiszami obrotu
		oCNpc* track = nullptr;       // wrog w celowniku (AddRef) - bohater obraca sie do niego, tor idzie w ukladzie voba
		bool trackReleased = false;   // minal cel (przewrot obok / przez niego) - dalej prosto
		float trackOffset = 0.0f;     // korekta mysza kierunku sledzenia (rad)
		zdcore::SteerKeyGate steerGate;
		// przenikanie postaci: NPC na liscie ignorowanych kolizji AI bohatera (oCAIHuman::AddIgnoreCD, AddRef)
		std::vector<oCNpc*> passNpcs;
		oCAIHuman* passAi = nullptr;
		bool passChars = false;       // promienie kroku bez postaci, omijanie malym kolem, koniec wypychany obok
		float lastStepX = 0.0f, lastStepZ = 0.0f;   // kierunek ostatniego kroku (wypychanie z dokladnego srodka)
		bool separateLogged = false;
		// anulowanie blokiem (parada gry)
		float paradeFrom = 1e9f;      // od tej chwili (s) wcisniety blok przerywa ruch parada
		bool paradeKeyPrev = false, downKeyPrev = false;
		zdcore::ParadeBuffer paradeBuf;
		int endKind = 0;              // MVEND_*: jak konczy sie faza 'ending'
		// atak w biegu
		float runExitAt = -1.0f;      // >= 0: od tej chwili z W prosto w bieg / sprint (bez gardy)
		float carry = 1.0f;           // rozped z wejscia: mnoznik krokow do przodu do chwili ciosu
		float carryTime = -1.0f;      // ruch bez ciosu (przewrot z przerwanego skoku): rozped wygasa do tej chwili (s)
		float yDrop = 0.0f;           // przerwany skok gry: vob ponad postawa na starcie - schodzi na ziemie (MOVE_DROP_TIME)
		// unik / przewrot (17.10, Core/Evade): okno wyjscia z buforem wcisniec, obrot do wroga na koncu
		float exitFrom = 1e9f;        // od tej chwili (s) atak / blok / kolejny ruch z bufora przerywa ruch
		zdcore::ExitQueue exitQ;
		zdcore::AttackKeys atkPrev;   // klawisze ciosu z poprzedniej klatki (zbocza; Gothic 1: akcja + przod / bok)
		float faceFrom = 1e9f;        // od tej chwili obrot przodem do wroga, w exitFrom patrzy na niego
		oCNpc* face = nullptr;        // wrog do obrotu na koncu (AddRef)
		bool faceLogged = false;
		float faceStartYaw = 0.0f;
		bool facing = false;          // obrot na koncu trwa - reszta toru w kierunku pathDir (sprzed obrotu)
		zVEC3 pathDir;
		bool perfect = false;         // unik w ostatniej chwili
	};
	// MVEND_ATTACK: atak gry z okna wyjscia, MVEND_CHAIN: kolejny unik / przewrot z bufora po krotkim przejsciu
	enum { MVEND_NORMAL = 0, MVEND_PARADE = 1, MVEND_RUN = 2, MVEND_ATTACK = 3, MVEND_CHAIN = 4 };
	static MoveState ms;
	static float moveCooldown = 0.0f;
	static bool prevDodgeKey = false, prevRollKey = false;
	// Klawisz uniku wg zCInput_Win32::SetState - takze wcisniecie polkniete w walce, ktorego gra (i New Balance: C =
	// receptury) nie widzi. Polkniete wcisniecie ma tez polkniete puszczenie.
	static bool dodgeKeyRaw = false, dodgeKeySwallowed = false;
	static zdcore::DoubleTap dblTap;
	static int pendingDodgeDir = -1;     // kierunek uniku z dwukliku (-1 = z klawiszy w chwili startu)
	// Wcisniecie zapamietane na chwile (w trakcie ruchu, odnowienia, ataku) - kolejny unik bez idealnego wyczucia czasu.
	static int pendingKind = 0;          // 0 brak, 1 unik, 2 przewrot, 3 atak z wyskoku, 4 atak w biegu
	static int pendingRollDir = 0;       // kierunek przewrotu z chwili wcisniecia
	static bool rollSharesJump = false;  // klawisz przewrotu = skok gry (Alt): przewrot z bokiem / tylem, w przod podwojnie
	static float pendingTime = 0.0f;
	static const float MOVE_BUFFER = 0.25f;
	static bool prevParade = false;
	// Podwojny Alt (przewrot w przod pod klawiszem skoku): od rundy 7 pierwsze Alt idzie do gry od razu (skok bez
	// czekania), okno po nim (Core JumpTapNowStep, JumpTapWindows): Alt+W na drugie Alt / atak, sam Alt na atak -
	// przerwanie skoku (MvJumpCancel). Wstrzykiwanie skoku (jumpInject*) zostaje dla przerwanego bufora przeskoku
	// (Traversal.cpp). Bez kluczy ini (stale ponizej).
	// runda 8: okna szersze (skok i tak startuje od razu - dluzsze okno nie opoznia skoku): drugie Alt do 0,40 s
	// (log 15.10: bywalo 0,33 s po pierwszym), atak do 0,30 s; od 21.10 okno ataku z ini (JumpAttackWindow, menu)
	static zdcore::JumpTapWindows JumpTapWindowsNow() {
		return zdcore::JumpTapWindowsFor(zcfg.jumpAttackWindow);
	}
	static zdcore::RollTap rollTap;
	// runda 10: czy ruch mogl ruszyc w chwili wcisniecia skoku (hak klawisza w AI, przed startem skoku gry w tej klatce)
	static zdcore::JumpPressLatch jumpPressLatch;
	static int jumpInjectFrames = 0;
	static bool jumpInjectToggle = false;
	// Skok gry od razu (runda 7): Alt+W / sam Alt w walce idzie do gry, drugie Alt / atak w oknie przerywa skok
	// (Core JumpTapNowStep, JumpCancelDecide) - przewrot w przod / atak z wyskoku z biezacej pozy, vob schodzi na ziemie
	// w ruchu (MOVE_DROP_TIME), ped skoku -> rozped przewrotu.
	// runda 8: w odbiciu do 110 cm (okno 0,40 s - szczyt T_RUNL_2_JUMP ok. 80 cm), poza nim do 50 cm
	static const zdcore::JumpCancelLimits JUMP_CANCEL = { 50.0f, 110.0f };
	static const float MOVE_DROP_TIME = 0.12f;       // s zejscia voba na ziemie po przerwanym skoku
	static const float JUMP_ROLL_CARRY_TIME = 0.35f; // s - rozped przerwanego skoku wygasa w przewrocie do tej chwili
	static float altOutAge = 10.0f;      // s od ostatniego Alt poza walka (diagnostyka W + 2x Alt bez broni)
	static bool mvJumpDrop = false;      // nastepny StartMove: z przerwanego skoku (vob ponad postawa)
	static float mvJumpSpeed = 0.0f;     // predkosc pozioma w chwili przerwania (rozped przewrotu)
	static float mvStandYOff = -1.0f;    // vob nad ziemia w postawie (probka na ziemi z bronia; -1 = brak)
	// Diagnostyka okna w logu (Debug=1): spoznione drugie Alt i atak tuz przed Alt.
	static const float TAP_LATE_LOG = 0.6f;          // drugie Alt do tylu s po pierwszym (okno juz minelo) - wpis w logu
	static const float TAP_ATTACK_LEAD_LOG = 0.25f;  // atak do tylu s przed Alt - wpis w logu (liczy sie atak po Alt)
	static float tapLateClock = -1.0f;   // s od pierwszego Alt okna zakonczonego skokiem gry (-1 = nie liczy)
	static float tapLateWindow = 0.0f;   // dlugosc tamtego okna
	static float attackEdgeAge = 10.0f;  // s od ostatniego wcisniecia ataku gry
	static float tapAttackLead = -1.0f;  // atak tyle s przed pierwszym Alt biezacego okna (-1 = nie bylo)
	// Atak z wyskoku (JUMPATK_F / JUMPATK2H_F): klawisz skoku (Alt), a w oknie atak gry (akcja / LPM).
	// Tez sam Alt czeka wtedy na atak (skok gry po oknie, jak przy Alt+W). Klucz ini (inny agent): JumpAttack=1.
	static const bool JUMP_ATTACK_ENABLED = true;
	static const float JUMP_TARGET_RANGE = 500.0f;   // cel ataku z wyskoku: wrog przed bohaterem blizej niz tyle
	// Atak w biegu (RUNATK_*): w stylu W3 atak w biegu gry (oCNpc::EV_AttackRun) przejety - ruch W3 calym cialem. Klucz
	// ini (inny agent): RunAttack=1.
	static const bool RUN_ATTACK_ENABLED = true;
	static const float RUN_TARGET_RANGE = 650.0f;    // cel ataku w biegu: wrog przed bohaterem blizej niz tyle
	static std::string lastLungeVariant[2];          // ostatni wariant ataku z wyskoku [0] / w biegu [1] - bez powtorzen
	static int jumpKey = 0;              // fizyczny klawisz skoku (keySMove; przy RollKey = Alt to ten sam)
	static bool prevJumpKey = false, prevAttackKey = false;
	static bool mvRawRead = false;       // odczyt stanu klawisza gry przez plugin - bez naszych podmian
	// Ochrona animacji ruchu: tyle wznowien na ruch po zgaszeniu jej przez silnik albo inna wtyczke.
	static const int MOVE_MAX_RESUMES = 3;
	// Obrazenia repel (obuchowe): czesc ciosu bronia (obrazenia broni + sila). Klucze ini: RepelDamageBash=50,
	// RepelDamageKick=35 (%).
	static const float REPEL_DAMAGE_BASH = 0.5f, REPEL_DAMAGE_KICK = 0.35f;

	// Kontra: wybor przeciwnika i celowanie wypadu.
	static const float COUNTER_REACH = 300.0f;      // przeciwnik w zamachu blizej niz tyle...
	static const float COUNTER_MIN_DOT = 0.5f;      // ...i przed bohaterem (do 60 st. od osi - reszte dokreca wypad)
	static const float COUNTER_HIT_TOL = 45.0f;     // w chwili ciosu przeciwnik najwyzej tyle dalej niz zasieg - inaczej pudlo
	static const float COUNTER_DEFAULT_REACH = 100.0f;   // moves_w3.ini bez reach=
	static const float COUNTER_TURN_RATE = 6.0f;    // rad/s obrotu do przeciwnika do chwili ciosu
	static const float LUNGE_MAX = 1.7f;            // wypad najwyzej tyle razy dluzszy niz w animacji (dalej slizg stop)
	static const float LUNGE_TOL = 30.0f;           // zapas osiagalnosci przeciwnika (cm)
	static const float KICK_CRAMPED = 40.0f;        // kopniak, gdy przeciwnik blizej niz zasieg stopy o tyle - za ciasno
	static const float FOE_RADIUS_MIN = 25.0f, FOE_RADIUS_MAX = 90.0f;
	// Repel: zatoczenie z animacji silnika + odepchniecie (cm w REPEL_PUSH_TIME s, z kolizja).
	static const float REPEL_PUSH_BASH = 40.0f, REPEL_PUSH_KICK = 70.0f, REPEL_PUSH_TIME = 0.3f;
	// Koniec ruchu: przejscie do postawy miesza silnik (animacja ruchu wygasa, postawa stylu gracza wchodzi).
	static const float MOVE_END_BLEND = 0.24f;      // zwykly koniec (= doklejone przejscie w animacji, REC_FRAMES)
	static const float MOVE_ABORT_BLEND = 0.2f;     // utkniecie w scianie

	// Kolizja ruchu: swiat i postacie (bez bohatera), bez przezroczystych poly i pociskow; z normalna (zbocza, slizg).
	static const int MOVE_TRACE = zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
		zTRACERAY_VOB_IGNORE_PROJECTILES | zTRACERAY_POLY_NORMAL;
	static const int MOVE_GROUND_TRACE = MOVE_TRACE | zTRACERAY_VOB_IGNORE_CHARACTER;
	static const float MOVE_RADIUS = 35.0f;      // odstep od scian
	static const float MOVE_SIDE = 25.0f;        // promienie boczne (szerokosc barkow)

	// Unik / przewrot wzgledem wroga w celowniku (fokus, potem enemy bohatera) jak w W3: bohater obraca sie przodem do
	// niego (TRACK_PARAMS.turnRate), a tor idzie w ukladzie voba - bok = krazenie wokol wroga, tyl = odskok od niego,
	// tor nadaza za ruszajacym sie wrogiem. Przod domyslnie bez sledzenia (przewrot obok wroga = flankowanie).
	// Klucze ini (inny agent): MoveTrack=1, MoveTrackForward=0, MoveTrackTurnRate=8 (rad/s).
	static const bool MOVE_TRACK = true;
	static const bool MOVE_TRACK_FORWARD = false;
	static const zdcore::TrackParams TRACK_PARAMS;
	// Sterowanie w trakcie uniku / przewrotu: mysz i klawisze obrotu (GAME_LEFT/RIGHT, jak gra: 0,1 * ms * arg st.)
	// obracaja bohatera i dalsza czesc toru. Z celem - tylko lekka korekta kierunku sledzenia (czesc obrotu, najwyzej
	// MOVE_STEER_TARGET_MAX). Klucze ini (inny agent): MoveSteer=1, MoveSteerScale=100 (%), MoveSteerTargetScale=30 (%),
	// MoveSteerTargetMax=30 (st.).
	static const bool MOVE_STEER = true;
	static const float MOVE_STEER_SCALE = 1.0f;
	static const float MOVE_STEER_TARGET_SCALE = 0.3f;
	static const float MOVE_STEER_TARGET_MAX = 0.52f;
	// Przenikanie postaci w trakcie uniku / przewrotu: NPC w poblizu nie blokuja ruchu (silnik ich nie zderza z bohaterem,
	// promienie kroku ich nie widza), tor omija ich srodek kolem MOVE_NPC_RADIUS (przenikanie barkami, bez wchodzenia
	// w tulow), a koniec ruchu jest wypychany na MOVE_NPC_END_RADIUS od srodka postaci (krokami z kolizja ze scianami).
	// Klucze ini (inny agent): MovePassNpcs=1, MoveNpcRadius=30, MoveNpcEndRadius=60 (cm).
	static const bool MOVE_PASS_NPCS = true;
	static const float MOVE_NPC_RADIUS = 30.0f;
	static const float MOVE_NPC_END_RADIUS = 60.0f;
	static const float MOVE_NPC_PUSH_SPEED = 300.0f;   // cm/s wypychania konca ruchu z postaci
	static const float MOVE_NPC_SEPARATE_LEAD = 0.3f;  // s przed koncem ruchu - zaczyna wypychanie
	static const float MOVE_PASS_RANGE = 900.0f;       // NPC blizej niz tyle na starcie ruchu
	static const float MOVE_NPC_LEVEL = 180.0f;        // roznica wysokosci - inne pietro, bez omijania
	// Blok w koncowce ruchu (kontra, unik, przewrot, ataki z wypadem; po ciosie / po glownej drodze) przerywa ruch
	// i od razu daje parade gry (komunikat EV_PARADE jak oCAIHuman::FightMelee - animacja T_xHPARADE_* ze stylu),
	// z krotkim przejsciem. Klucze ini (inny agent): ParadeCancel=1, ParadeCancelAfterHit=0.12 (s).
	static const bool PARADE_CANCEL = true;
	static const float PARADE_AFTER_HIT = 0.12f;       // kontra / atak: okno od tylu s po ciosie
	static const float PARADE_PATH_SHARE = 0.75f;      // unik / przewrot: okno po tej czesci drogi (i po nietykalnosci)
	static const float PARADE_BUFFER = 0.35f;          // klikniecie bloku przed oknem pamietane tyle s
	static const float PARADE_BLEND = 0.12f;           // przejscie ruch -> parada
	// Atak w biegu: wejscie z biegu / sprintu mieszane dluzej (petla biegu wygasa pod ruchem zamiast stanac), rozped
	// z wejscia (sprint = szybszy wypad, wygasa do ciosu), a z W trzymanym po ciosie prosto z powrotem w bieg / sprint
	// (bez gardy). Klucze ini (inny agent): RunAttackChain=1, RunAttackCarry=1.
	static const bool RUN_CHAIN = true;
	static const bool RUN_CARRY = true;
	static const float RUN_ENTRY_BLEND = 0.2f;         // wejscie ataku w biegu z petli biegu / sprintu
	static const float MOVE_ENTRY_BLEND_LOOP = 0.15f;  // wejscie innych ruchow z biegu
	static const float RUN_EXIT_AFTER_HIT = 0.24f;     // wyjscie w bieg tyle s po ciosie
	static const float RUN_EXIT_BLEND = 0.3f;          // przejscie ruch -> petla biegu
	static const float RUN_CARRY_MAX = 1.6f;           // rozped najwyzej tyle razy szybszy niz tor klipu
	// Dzwiek trafienia repelem (instancje C_SFX z SFX.DAT; warianty _A1/_A2 losuje silnik). Klucze ini (inny agent):
	// RepelSoundBash=CS_MAM_FI_FL, RepelSoundKick=CS_IAM_WO_FL.
	static const char* const REPEL_SOUND_BASH = "CS_MAM_FI_FL";   // piesc w cialo (CS_MAM_FI_FL_01..03.wav)
	static const char* const REPEL_SOUND_KICK = "CS_IAM_WO_FL";   // tepe uderzenie w cialo
	// oCAniCtrl_Human::_Stand w trakcie naszego ruchu pomijany (G2A) - petla biegu moze wygasac pod ruchem.
	static const bool MV_STAND_HOOK = ENGINE == Engine_G2A;
	static unsigned int mvUpdateFrame = 0;   // klatka ostatniego MovesUpdate (hak _Stand dziala tylko, gdy plugin zyje)
	static zVEC3 mvLastPos;
	static bool mvSpeedValid = false;
	static float heroSpeed = 0.0f;           // predkosc bohatera w poziomie (cm/s, wygladzona) - rozped ataku w biegu

	// Odepchniecie po repelu (przeciwnik przesuwany krokami z kolizja, niezaleznie od konca ruchu bohatera).
	struct RepelPushState {
		oCNpc* npc = nullptr;
		zVEC3 dir;
		float t = 0.0f, dist = 0.0f, done = 0.0f, yOff = 0.0f;
	};
	static RepelPushState repelPush;

	// Okno wyjscia z uniku / przewrotu (17.10): przejscie ruch -> atak gry / kolejny ruch.
	static const float EXIT_ATTACK_BLEND = 0.12f;   // ruch wygasa pod atakiem gry (EV_ATTACKFORWARD jak oCAIHuman::FightMelee)
	static const float EXIT_CHAIN_BLEND = 0.1f;     // ruch -> postawa, potem od razu kolejny ruch z bufora
	static const float EXIT_CHAIN_MIN_PRESS = 0.3f; // unik / przewrot wcisniety wczesniej w ruchu nie idzie do bufora
	static const float FACE_MAX_DIST = 1500.0f;     // obrot na koncu do wroga najwyzej tak daleko
	static const float FACE_MIN_DIST = 40.0f;       // blizej - bez obrotu (kierunek nieokreslony)

	static bool MoveCanStart(int& wm);
	static void MvSetBlend(zCModel* m, zCModelAniActive* in, zCModelAniActive* out, float sec);
	static void MvEvadeBegin();
	static void MvEvadeExitBegin();
	static bool MvFaceStep(float dt);
	static void PerfectDodgeCheck();
	static void PerfectDodgeUpdate(float dt);
	static void PerfectDodgeReset();
	static bool PerfectDodgeSwallow(oCNpc* npc, oCMsgAttack* msg, const char* what);
	static void MvParadeBegin();
	static void MvPassEnd();

	static float MvFlatLen(const zVEC3& v) {
		return sqrtf(v.n[0] * v.n[0] + v.n[2] * v.n[2]);
	}

	static float MvFlatDist(const zVEC3& a, const zVEC3& b) {
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2];
		return sqrtf(dx * dx + dz * dz);
	}

	// Stan klawisza gry bez naszych podmian (hak GetState przepuszcza odczyt).
	static bool MvRawState(unsigned short logicalKey) {
		if (!zinput)
			return false;
		mvRawRead = true;
		bool down = zinput->GetState(logicalKey) > 0.0f;
		mvRawRead = false;
		return down;
	}

	// Kierunek z klawiszy gracza (bez podmian dla silnika - w trakcie ruchu obrot A/D jest dla gry wyzerowany).
	static void ReadDirKeys(bool& left, bool& right, bool& forward, bool& back) {
		forward = MvRawState(GAME_UP);
		back = MvRawState(GAME_DOWN);
		zdcore::SideKeys(MvRawState(GAME_STRAFELEFT), MvRawState(GAME_LEFT), MvRawState(GAME_STRAFERIGHT),
			MvRawState(GAME_RIGHT), forward, left, right);
	}

	// Sam przod (bez boku i tylu) - jak RollDirFromKeys: bok, gdy dokladnie jeden z lewo/prawo, potem tyl.
	static bool MvForwardOnly(bool l, bool r, bool f, bool b) {
		return f && l == r && !b;
	}

	// W walce (bron 1H/2H/piesci, ludzki model, ruchy wczytane) plugin moze przejac klawisz skoku.
	static bool MvFightKeysOurs() {
		if (!zcfg.enabled || !zcfg.movesEnabled || moveDefs.empty() || !player || !zinput || !HeroIsHuman() || HeroMounted())
			return false;
		int wm = player->GetWeaponMode();
		return wm == NPC_WEAPON_1HS || wm == NPC_WEAPON_2HS || wm == NPC_WEAPON_FIST;
	}

	// Klawisz uniku wcisniety: stan gry albo nasz (wcisniecie polkniete w walce w haku SetState).
	static bool MvDodgeKeyDown() {
		if (zcfg.dodgeKey <= 0 || !zinput)
			return false;
		return (dodgeKeySwallowed && dodgeKeyRaw) || zinput->KeyPressed(zcfg.dodgeKey) != 0;
	}

	// Podwojny Alt mozliwy: klawisz przewrotu to skok gry i jest przewrot w przod.
	static bool MvRollTapArmed() {
		return rollSharesJump && zcfg.rollKey > 0 && MvFightKeysOurs() &&
			zdcore::FindMove(moveDefs, "ROLL", zdcore::MOVE_FORWARD) != nullptr;
	}

	// Atak z wyskoku mozliwy: bron biala 1H/2H w reku i ruch w moves_w3.ini.
	static bool MvJumpAttackArmed() {
		if (!JUMP_ATTACK_ENABLED || jumpKey <= 0 || !MvFightKeysOurs())
			return false;
		int wm = player->GetWeaponMode();
		return (wm == NPC_WEAPON_1HS || wm == NPC_WEAPON_2HS) &&
			zdcore::FindJumpAttack(moveDefs, AniSwapAniWeaponMode(wm) == NPC_WEAPON_2HS) != nullptr;
	}

	static bool MvAttackDown() {
		return MvRawState(GAME_ACTION) || MvRawState(GAME_ACTIONLEFT) || MvRawState(GAME_ACTIONRIGHT);
	}

	// Okno klawisza skoku ma sens tylko, gdy ruch moze zaraz wystartowac albo trwa nasz (bufor wcisniecia). W powietrzu,
	// w upadku, w ataku gry, w reakcji na trafienie itp. skok idzie od razu do gry - bez opoznienia i bez drugiego skoku
	// po wyladowaniu, gdy spoznione drugie Alt otwieralo nowe okno w trakcie skoku.
	static bool MvTapCanWait() {
		int wm = 0;
		return ms.def != nullptr || MoveCanStart(wm);
	}

	// Klawisz skoku wcisniety w tej klatce (poprzedni stan z MovesUpdate) - Alt i atak naraz, zanim otworzy sie okno.
	static bool MvJumpPressFresh() {
		if (jumpKey <= 0 || !zinput || !zinput->KeyPressed(jumpKey))
			return false;
		return !(rollSharesJump ? prevRollKey : prevJumpKey);
	}

	// Klawisze gry w walce: -1 = bez zmian, 0 = polkniety, 1 = wcisniety (skok wstrzykniety po oknie Alt).
	// Skok (GAME_SMOVE): Alt z bokiem / tylem = przewrot, Alt+W = czekanie na drugie Alt albo atak, sam Alt = czekanie
	// na atak (atak z wyskoku), bez nich skok gry. Atak gry (akcja) w oknie polkniety - zamiast ciosu atak z wyskoku;
	// takze w klatce wcisniecia Alt (akord Alt + atak), gdy okno jeszcze sie nie otworzylo.
	// Drugie przypisanie skoku (prawy Ctrl) dziala zawsze.
	// Klawisze gry, ktore w trakcie finishera (ustawienie i animacja do oddania sterowania) gra ma nie widziec: skok gry
	// z Alt i cios kombinacji przerywaly animacje bohatera (log NB 21.10: W3_D01_2H po 0,28 s, FIN_2H_SLASH po 0,18 s),
	// a ofiara grala dalej swoja polowe pary.
	static bool FinisherHidesKey(unsigned short k) {
		return k == GAME_UP || k == GAME_DOWN || k == GAME_LEFT || k == GAME_RIGHT || k == GAME_STRAFELEFT || k == GAME_STRAFERIGHT ||
			k == GAME_ACTION || k == GAME_ACTIONLEFT || k == GAME_ACTIONRIGHT || k == GAME_ACTION2 || k == GAME_SMOVE || k == GAME_SMOVE2 ||
			k == GAME_PARADE || k == GAME_WEAPON || k == GAME_SNEAK || k == GAME_SLOW;
	}

	int MovesGameKeyOverride(unsigned short logicalKey, bool toggled) {
		if (mvRawRead)
			return -1;
		if (DirectorPlaying() && FinisherHidesKey(logicalKey))
			return 0;
		// W trakcie naszego ruchu gra nie widzi obrotu A/D: przy trzymanym obrocie startowala animacje obrotu, ktora
		// przykrywala animacje uniku / przewrotu (16.10: przewrot A + Alt "bez animacji", Q + Alt dobrze). Nasze
		// sterowanie czyta klawisze przez MvRawState.
		if (ms.def && (logicalKey == GAME_LEFT || logicalKey == GAME_RIGHT))
			return 0;
		// chwyt w locie: zawis trzymany do dosuniecia dloni (Traversal.cpp)
		int th = TraversalGameKeyOverride(logicalKey, toggled);
		if (th >= 0)
			return th;
		// unik z trzymana akcja (sterowanie Gothica 1: akcja + kierunek = cios): przy trzymanym klawiszu uniku gra nie
		// widzi kierunku, wiec nie zaczyna ciosu w bok / w przod - kierunek uniku czytamy przez MvRawState
		if (zcfg.dodgeWithAction && (logicalKey == GAME_LEFT || logicalKey == GAME_RIGHT || logicalKey == GAME_UP ||
			logicalKey == GAME_DOWN || logicalKey == GAME_STRAFELEFT || logicalKey == GAME_STRAFERIGHT) &&
			MvDodgeKeyDown() && MvFightKeysOurs() && MvAttackDown())
			return 0;
		if (logicalKey == GAME_ACTION || logicalKey == GAME_ACTIONLEFT || logicalKey == GAME_ACTIONRIGHT) {
			if (!MvJumpAttackArmed())
				return -1;
			if (rollTap.waiting)
				return 0;
			if (!MvJumpPressFresh())
				return -1;
			bool l, r, f, b;
			ReadDirKeys(l, r, f, b);
			bool fwdOrPlain = MvForwardOnly(l, r, f, b) || (!f && !b && l == r);
			return fwdOrPlain && MvTapCanWait() ? 0 : -1;
		}
		if (logicalKey != GAME_SMOVE && logicalKey != GAME_SMOVE2)
			return -1;
		if (toggled && jumpInjectToggle) {
			jumpInjectToggle = false;
			return 1;
		}
		if (jumpInjectFrames > 0)
			return toggled ? 0 : 1;
		// przeskok / wspiecie w biegu przed pasujaca przeszkoda (Traversal.cpp) - pierwszenstwo przed przewrotem, atakiem
		// z wyskoku i skokiem gry, takze poza walka; bez przeszkody wcisniecie idzie dalej jak dotad
		if (TraversalTakesJump())
			return 0;
		if (!MvFightKeysOurs())
			return -1;
		// runda 10 (log 134418): AI czyta klawisz i w tej samej klatce startuje skok (stan 8) - MovesUpdate po niej widzial
		// juz skok i nie otwieral okna; stan z pierwszej obserwacji nowego wcisniecia (przed skokiem)
		if (MvJumpPressFresh())
			zdcore::JumpPressObserve(jumpPressLatch, MvTapCanWait());
		if (MvDodgeKeyDown())
			return 0;
		// okno po skoku od razu (runda 7): pierwsze Alt (trzymane) dalej dla gry, nowe wcisniecie w oknie polykane -
		// przewrot / atak z wyskoku zamiast drugiego skoku
		if (rollTap.waiting)
			return zdcore::JumpTapSwallow(rollTap) ? 0 : -1;
		if (!rollSharesJump && zcfg.rollKey > 0 && zinput->KeyPressed(zcfg.rollKey))
			return 0;
		if (jumpKey <= 0 || !zinput->KeyPressed(jumpKey))
			return -1;
		bool l, r, f, b;
		ReadDirKeys(l, r, f, b);
		// Alt z bokiem / tylem = przewrot od razu (gra go nie widzi); sam Alt i Alt+W - skok gry od razu (okno w MovesUpdate)
		if (rollSharesJump)
			return zdcore::RollDirFromKeys(true, l, r, f, b) >= 0 ? 0 : -1;
		return -1;
	}

	// Sekcja Input.Keys: tylko decyzja pluginu (wolane wiele razy na klatke - wywolan/klatke w podsumowaniu); przy
	// wcisnieciu skoku w biegu tu tez leci wykrywanie przeskoku (Traversal.Decide).
	HOOK Ivk_zCInput_Win32_GetState AS(&zCInput_Win32::GetState, &zCInput_Win32::GetState_Union);
	float zCInput_Win32::GetState_Union(unsigned short key) {
		ZHookHit(ZH_INPUT_GETSTATE);
		if (MeditateActive() || PathMenuOpen() || WheelBlocksGameKeys())
			return 0.0f;   // medytacja (Meditate.cpp), okno sciezek, menu kolowe: gra nie widzi klawiszy logicznych - tez trzymanych przed startem
		if (key == GAME_SLOW && WheelWalkOn())
			return 1.0f;   // chod przelaczany stuknieciem Caps Lock (Wheel.cpp) zamiast przelacznika Caps Lock silnika (PC_SlowMove)
		int o;
		{
			ZPROF_SCOPE("Input.Keys");
			o = MovesGameKeyOverride(key, false);
			if (o < 0)
				o = LootGameKeyOverride(key, false);   // atak z bronia bez wrogow = podniesienie (Loot.cpp)
		}
		if (o >= 0)
			return (float)o;
		return THISCALL(Ivk_zCInput_Win32_GetState)(key);
	}

	HOOK Ivk_zCInput_Win32_GetToggled AS(&zCInput_Win32::GetToggled, &zCInput_Win32::GetToggled_Union);
	int zCInput_Win32::GetToggled_Union(unsigned short key) {
		ZHookHit(ZH_INPUT_GETTOGGLED);
		int toggled = THISCALL(Ivk_zCInput_Win32_GetToggled)(key);   // oryginal zuzywa wcisniecie (bez skoku po puszczeniu)
		if (MeditateActive() || PathMenuOpen() || WheelBlocksGameKeys())
			return 0;
		ZPROF_SCOPE("Input.Keys");
		int o = MovesGameKeyOverride(key, true);
		if (o < 0)
			o = LootGameKeyOverride(key, true);
		return o >= 0 ? o : toggled;
	}

	// zCInput_Win32::SetState 0x4D4E00 - zdarzenie klawisza z ProcessInputEvents, zanim trafi do tablic stanu silnika
	// (keyevent / keytoggle / bufor), z ktorych czyta gra i New Balance (KEY_CRAFTREC = C otwiera receptury takze w
	// walce). W walce wcisniecie klawisza uniku zostaje tylko u nas (MvDodgeKeyDown); puszczenie polknietego tez.
	// Nasz hak jest nad hakiem NB (Detours idzie za skokiem na wejsciu), wiec NB tego wcisniecia nie widzi.
	HOOK Ivk_zCInput_Win32_SetState AS(&zCInput_Win32::SetState, &zCInput_Win32::SetState_Union);
	int zCInput_Win32::SetState_Union(unsigned short key, int state) {
		if (zcfg.dodgeKey > 0 && key == zcfg.dodgeKey && key != jumpKey) {
			dodgeKeyRaw = state != 0;
			bool swallow = state ? zcfg.dodgeKeyCombatOnly && MvFightKeysOurs() && !(zcon && zcon->IsVisible())
				: dodgeKeySwallowed;
			if (state && swallow != dodgeKeySwallowed)
				ZDebug("Ruchy: klawisz uniku %d %s", (int)key, swallow ? "w walce - tylko unik (gra go nie widzi)" :
					"poza walka - dla gry");
			if (state)
				dodgeKeySwallowed = swallow;
			if (swallow) {
				if (!state)
					dodgeKeySwallowed = false;
				return 0;
			}
		}
		return THISCALL(Ivk_zCInput_Win32_SetState)(key, state);
	}

	// Klawisz uniku/przewrotu przypisany tez do akcji gry (Gothic.ini [KEYS]) = gra reaguje na to samo wcisniecie.
	static void WarnKeyClashes() {
		rollSharesJump = false;
		jumpKey = 0;
		static const char* keys[] = { "keyEnd", "keyHeal", "keyPotion", "keyLockTarget", "keyParade", "keyActionRight",
			"keyActionLeft", "keyUp", "keyDown", "keyLeft", "keyRight", "keyStrafeLeft", "keyStrafeRight", "keyAction",
			"keySlow", "keySMove", "keyWeapon", "keySneak", "keyLook", "keyLookFP", "keyInventory", "keyShowStatus",
			"keyShowLog", "keyShowMap", "keyTorch", "keyIntSprint" };
		for (const char* k : keys) {
			if (!zoptions->EntryExists("KEYS", k))
				continue;
			std::vector<int> codes = zdcore::ParseKeyBinding(zoptions->ReadString("KEYS", k, "").ToChar());
			if (_stricmp(k, "keySMove") == 0 && !codes.empty() && codes[0] < 256)
				jumpKey = codes[0];   // pierwszy klawisz skoku (klawiatura) - atak z wyskoku
			for (int code : codes)
				for (int mine : { zcfg.dodgeKey, zcfg.rollKey })
					if (mine > 0 && code == mine) {
						if (_stricmp(k, "keySMove") == 0) {
							if (mine == zcfg.rollKey)
								rollSharesJump = true;
							ZLog("Ruchy: klawisz %d (%s) to tez skok - w walce przejety przez plugin, poza walka skacze", mine,
								mine == zcfg.dodgeKey ? "unik" : "przewrot");
						}
						else
							ZLog("Ruchy: UWAGA klawisz %d (%s) jest tez przypisany w grze do %s - zmien DodgeKey/RollKey w [ZDISMEMBER]",
								mine, mine == zcfg.dodgeKey ? "unik" : "przewrot", k);
					}
		}
		if (rollSharesJump) {
			jumpKey = zcfg.rollKey;
			ZLog("Ruchy: w walce (bron / piesci) przewrot = klawisz skoku z bokiem / tylem od razu; W + klawisz = skok gry od razu, "
				"drugie wcisniecie w %.2f s (nisko, w odbiciu do %.0f cm) przerywa skok przewrotem w przod; poza walka - skok gry",
				JumpTapWindowsNow().doubleTap, JUMP_CANCEL.takeoffMaxHeight);
		}
		else if (!jumpKey && zcfg.rollKey != 56)
			jumpKey = 56;   // brak keySMove w ini - skok gry domyslnie pod lewym Altem
		bool atk1 = zdcore::FindJumpAttack(moveDefs, false) != nullptr, atk2 = zdcore::FindMove(moveDefs, "JUMPATK2H", zdcore::MOVE_FORWARD) != nullptr;
		if (JUMP_ATTACK_ENABLED && atk1 && jumpKey > 0)
			ZLog("Ruchy: atak z wyskoku = klawisz skoku (%d, skok gry od razu) i atak w %.2f s (z W %.2f s; bron 1H%s; warianty 1H %d, "
				"2H %d) - przerywa skok",
				jumpKey, JumpTapWindowsNow().attack, JumpTapWindowsNow().doubleTap, atk2 ? " i 2H" : ", 2H z animacja 1H",
				(int)zdcore::JumpAttackVariants(moveDefs, false).size(), (int)zdcore::JumpAttackVariants(moveDefs, true).size());
		if (RUN_ATTACK_ENABLED && zdcore::FindRunAttack(moveDefs, false))
			ZLog("Ruchy: atak w biegu W3 w stylu W3 (bieg do przodu + atak gry) - warianty 1H %d, 2H %d",
				(int)zdcore::RunAttackVariants(moveDefs, false).size(), (int)zdcore::RunAttackVariants(moveDefs, true).size());
	}

	void MovesLoad() {
		moveDefs.clear();
		static const char* names[] = { "\\SYSTEM\\ZDISMEMBER\\MOVES_W3.INI", "SYSTEM\\ZDISMEMBER\\MOVES_W3.INI" };
		std::string text;
		if (!ZReadGameFile(names, 2, text)) {
			ZLog("Ruchy: brak moves_w3.ini - uniki i przewroty wylaczone");
			return;
		}
		std::vector<std::string> errors;
		auto defs = zdcore::ParseMoves(text, errors);
		for (auto& e : errors)
			ZLog("Ruchy (moves_w3.ini): %s", e.c_str());
		for (auto& d : defs) {
			if (!ZOverlayExists(d.overlay, "T_1HPARADEJUMPB")) {
				ZLog("Ruchy: %s - brak nakladki %s, pominiety", d.id.c_str(), d.overlay.c_str());
				continue;
			}
			if (!d.overlayW3.empty() && !ZOverlayExists(d.overlayW3, "T_1HPARADEJUMPB")) {
				ZLog("Ruchy: %s - brak nakladki %s, koniec zawsze w postawie Gothica", d.id.c_str(), d.overlayW3.c_str());
				d.overlayW3.clear();
			}
			moveDefs.push_back(d);
		}
		ZLog("Ruchy: wczytano %d (unik: klawisz %d, przewrot: klawisz %d)", (int)moveDefs.size(), zcfg.dodgeKey, zcfg.rollKey);
		WarnKeyClashes();
	}

	void MovesReloadKeys() {
		WarnKeyClashes();
	}

	int MovesCount() {
		return (int)moveDefs.size();
	}

	const std::vector<zdcore::MoveDef>& MovesDefs() {
		return moveDefs;
	}

	std::string MovesList() {
		std::string s = "ruchy:";
		for (auto& d : moveDefs)
			s += " " + d.id;
		return s;
	}

	bool MovesActive() {
		return ms.def != nullptr || TraversalActive();
	}

	bool MovesAttackHandoff() {
		return ms.def != nullptr && ms.endKind == MVEND_ATTACK;
	}

	bool MovesHeroInvulnerable() {
		return ms.def && ms.ending < 0.0f && zdcore::MoveInvulnerable(*ms.def, ms.t);
	}

	// Bohater w ludzkim modelu (po przemianie w zwierze nakladki HUMANS_* sie nie nadaja).
	void ResetWeaponGrip(oCNpc* npc) {
		zCModel* m = npc ? npc->GetModel() : nullptr;
		int i = m ? FindNode(m, "ZS_RIGHTHAND") : -1;
		if (i >= 0 && m->nodeList[i]->protoNode)
			m->nodeList[i]->trafo = m->nodeList[i]->protoNode->trafo;
	}

	bool HeroIsHuman() {
		zCModel* m = player ? player->GetModel() : nullptr;
		return m && _strnicmp(m->GetVisualName().ToChar(), "HUMANS", 6) == 0;
	}

	// Trwa dobywanie / chowanie broni (komunikat broni w kolejce) - ruch przerwalby je w polowie.
	static bool HeroWeaponMessagePending() {
		zCEventManager* em = player ? player->GetEM(FALSE) : nullptr;
		if (!em)
			return false;
		for (int i = 0; i < em->GetNumMessages(); i++)
			if (zDYNAMIC_CAST<oCMsgWeapon>(em->GetEventMessage(i)))
				return true;
		return false;
	}

	static bool MoveGroundAt(const zVEC3& p, float& y) {
		oCWorld* world = ogame->GetGameWorld();
		zVEC3 from(p.n[0], p.n[1] + 150.0f, p.n[2]);
		if (!world->TraceRayNearestHit(from, zVEC3(0.0f, -400.0f, 0.0f), (zCVob*)nullptr, MOVE_GROUND_TRACE) || !world->traceRayReport.foundHit)
			return false;
		y = world->traceRayReport.foundIntersection.n[1];
		return true;
	}

	// Promien z pominieciem dwoch vobow (bohater i przeciwnik kontry / odpychany NPC i bohater).
	static bool MvTrace(const zVEC3& from, const zVEC3& ray, zCVob* self, zCVob* also, int flags) {
		oCWorld* world = ogame->GetGameWorld();
		static zCArray<zCVob*> ignore;
		ignore.EmptyList();
		ignore.Insert(self);
		if (also)
			ignore.Insert(also);
		return world->TraceRayNearestHit(from, ray, &ignore, flags) && world->traceRayReport.foundHit;
	}

	enum MvStepResult { MVSTEP_OK, MVSTEP_WALL, MVSTEP_GROUND };

	// Krok o d (poziomo) z 'cur' (ziemia curGround): ziemia celu po zboczu do kata chodu Gothica (ClassifyStep), stopnie,
	// bez urwisk; potem promienie rownolegle do zbocza na wysokosci kolan, piersi i barkow (sciany, postacie).
	// MVSTEP_WALL: wallN = normalna przeszkody (do slizgu).
	static MvStepResult MvTryStep(zCVob* self, zCVob* also, const zVEC3& cur, float curGround, const zVEC3& d,
		const zdcore::TerrainLimits& lim, float& groundOut, zVEC3& wallN, int rayFlags = MOVE_TRACE) {
		oCWorld* world = ogame->GetGameWorld();
		float l = MvFlatLen(d);
		if (l < 0.01f) {
			groundOut = curGround;
			return MVSTEP_OK;
		}
		float ux = d.n[0] / l, uz = d.n[2] / l;
		// ziemia celu: od najwyzszego dozwolonego wzniosu (z zapasem) w dol do uskoku - nawisy nad glowa nie przeszkadzaja
		float top = curGround + lim.stepUp + lim.maxSlope * l + 40.0f;
		float bottom = curGround - lim.maxDrop - 30.0f;
		if (!MvTrace(zVEC3(cur.n[0] + d.n[0], top, cur.n[2] + d.n[2]), zVEC3(0.0f, bottom - top, 0.0f), self, also, MOVE_GROUND_TRACE))
			return MVSTEP_GROUND;
		float g = world->traceRayReport.foundIntersection.n[1];
		float ny = world->traceRayReport.foundPolyNormal.n[1];
		if (ny <= 0.0f || ny > 1.001f)
			ny = 1.0f;   // bez normalnej (np. vob) - ocena tylko po wzniosie
		if (zdcore::ClassifyStep(g - curGround, l, ny, lim) != zdcore::STEP_OK)
			return MVSTEP_GROUND;
		float slope = (g - curGround) / l;
		if (slope > lim.maxSlope)
			slope = lim.maxSlope;
		else if (slope < -lim.maxSlope)
			slope = -lim.maxSlope;
		float reach = l + MOVE_RADIUS;
		zVEC3 ray(ux * reach, slope * reach, uz * reach);
		float sx = -uz * MOVE_SIDE, sz = ux * MOVE_SIDE;
		const float rays[4][2] = { { 50.0f, 0.0f }, { 120.0f, 0.0f }, { 90.0f, 1.0f }, { 90.0f, -1.0f } };
		for (auto& r : rays) {
			zVEC3 a(cur.n[0] + sx * r[1], curGround + r[0], cur.n[2] + sz * r[1]);
			if (MvTrace(a, ray, self, also, rayFlags)) {
				wallN = world->traceRayReport.foundPolyNormal;
				return MVSTEP_WALL;
			}
		}
		groundOut = g;
		return MVSTEP_OK;
	}

	// Slizg wzdluz sciany: bez skladowej kroku w strone sciany (normalna w poziomie); za maly = brak slizgu.
	static bool MvSlide(const zVEC3& d, const zVEC3& n, zVEC3& out) {
		float nx = n.n[0], nz = n.n[2], nl = sqrtf(nx * nx + nz * nz);
		if (nl < 0.3f || nl > 1.01f)
			return false;   // podloga, sufit albo bez normalnej
		nx /= nl;
		nz /= nl;
		float into = d.n[0] * nx + d.n[2] * nz;
		if (into >= 0.0f)
			return false;
		out = zVEC3(d.n[0] - nx * into, 0.0f, d.n[2] - nz * into);
		return MvFlatLen(out) >= 0.3f * MvFlatLen(d);
	}

	// Krok bohatera o d z kolizja (slizg wzdluz sciany, laczny wznios wg kata) - zwraca wykonana droge (cm).
	static float MvStepHero(const zVEC3& d) {
		if (MvFlatLen(d) < 0.01f)
			return 0.0f;
		zVEC3 cur = player->GetPositionWorld();
		zVEC3 step = d, n;
		float g = ms.ground;
		// unik / przewrot z przenikaniem: postacie nie blokuja promieni (omijanie i wypychanie liczy plugin)
		int rays = ms.passChars ? MOVE_TRACE | zTRACERAY_VOB_IGNORE_CHARACTER : MOVE_TRACE;
		MvStepResult res = MvTryStep(player, ms.target, cur, ms.ground, step, ms.terrain, g, n, rays);
		if (res == MVSTEP_WALL) {
			zVEC3 slid;
			if (!MvSlide(d, n, slid))
				return 0.0f;
			step = slid;
			res = MvTryStep(player, ms.target, cur, ms.ground, step, ms.terrain, g, n, rays);
		}
		if (res != MVSTEP_OK)
			return 0.0f;
		float sl = MvFlatLen(step);
		if (!zdcore::ClimbWithinSlope(g - ms.startGround, ms.pathLen + sl, ms.terrain))
			return 0.0f;   // dlugi podjazd bardziej stromy niz chod gry
		float drop = zdcore::MoveDropLeft(ms.yDrop, ms.t, MOVE_DROP_TIME);   // przerwany skok: zejscie na ziemie
		player->SetPositionWorld(zVEC3(cur.n[0] + step.n[0], g + ms.yOff + drop, cur.n[2] + step.n[2]));
		ms.ground = g;
		ms.pathLen += sl;
		return sl;
	}

	// Kat chodu z ustawien AI bohatera (zMV_MAX_GROUND_ANGLE_WALK, radiany), gdy rozsadny - zbocza jak przy chodzeniu.
	static zdcore::TerrainLimits MvHeroTerrain() {
		zdcore::TerrainLimits lim;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		float a = ac ? ac->config.zMV_MAX_GROUND_ANGLE_WALK : 0.0f;
		if (a > 0.35f && a < 1.3f) {
			lim.maxSlope = tanf(a);
			lim.minNormalY = cosf(a);
		}
		return lim;
	}

	// Nazwy animacji ruchu bohatera dla trybu broni - przy zamianie animacji 1H <-> 2H wg animacji drugiego typu.
	static const char* MoveAniName(int wm) {
		wm = AniSwapAniWeaponMode(wm);
		return wm == NPC_WEAPON_2HS ? "T_2HPARADEJUMPB" : wm == NPC_WEAPON_FIST ? "T_FISTPARADEJUMPB" : "T_1HPARADEJUMPB";
	}

	static const char* MoveStanceName(int wm) {
		wm = AniSwapAniWeaponMode(wm);
		return wm == NPC_WEAPON_2HS ? "S_2HRUN" : wm == NPC_WEAPON_FIST ? "S_FISTRUN" : "S_1HRUN";
	}

	static bool MoveCanStart(int& wm) {
		if (!player || !ogame || !ogame->GetGameWorld() || player->IsDead() || player->IsUnconscious() || !player->GetModel())
			return false;
		if (DirectorActive() || TraversalActive() || player->IsMovLock() || (zcon && zcon->IsVisible()) || player->inventory2.IsOpen())
			return false;
		if (!oCInformationManager::GetInformationManager().HasFinished())
			return false;
		wm = player->GetWeaponMode();
		if (wm != NPC_WEAPON_1HS && wm != NPC_WEAPON_2HS && wm != NPC_WEAPON_FIST)
			return false;
		if (!HeroIsHuman() || HeroWeaponMessagePending() || HeroMounted())
			return false;
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		return bs == BS_STAND || bs == BS_WALK || bs == BS_RUN || bs == BS_SNEAK || bs == BS_SPRINT || bs == BS_PARADE;
	}

	// Unik / przewrot z trzymana akcja (DodgeWithAction): sterowanie Gothica 1 trzyma bohatera w postawie ataku
	// (S_xHATTACK, stan BS_HIT), wiec MoveCanStart odmawia. Start dozwolony, gdy poza stanem wszystko jak w
	// MoveCanStart i nie trwa cios (zdcore::AttackHoldIdle po aktywnych animacjach modelu).
	static bool MvAttackHoldCanStart(int& wm) {
		if (!zcfg.dodgeWithAction || !player || player->IsDead() || player->IsUnconscious())
			return false;
		zCModel* m = player->GetModel();
		if (!m || (player->GetBodyState() & BS_ONLY_STATE) != BS_HIT)
			return false;
		if (DirectorActive() || TraversalActive() || player->IsMovLock() || (zcon && zcon->IsVisible()) || player->inventory2.IsOpen())
			return false;
		if (!oCInformationManager::GetInformationManager().HasFinished())
			return false;
		wm = player->GetWeaponMode();
		if (wm != NPC_WEAPON_1HS && wm != NPC_WEAPON_2HS && wm != NPC_WEAPON_FIST)
			return false;
		if (!HeroIsHuman() || HeroWeaponMessagePending() || HeroMounted())
			return false;
		std::vector<std::string> names;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (a && a->protoAni && !a->isFadingOut)
				names.push_back(a->protoAni->aniName.ToChar());
		}
		bool ok = zdcore::AttackHoldIdle(names);
		if (zcfg.debug) {
			std::string all;
			for (const std::string& n : names)
				all += (all.empty() ? "" : ", ") + n;
			ZDebug("Ruchy: unik z trzymana akcja - %s (%s)", ok ? "start z postawy ataku" : "cios w toku, czeka w buforze",
				all.c_str());
		}
		return ok;
	}

	// Postawa walki bohatera pochodzi z nakladki stylu W3 (garda Geralta) - wtedy ruch z koncem w tej gardzie (overlayW3).
	static bool MvStanceIsW3(zCModel* m, int wm) {
		int id = m->GetAniIDFromAniName(MoveStanceName(wm));
		zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		if (!a)
			return false;
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && id < p->protoAnis.GetNum() && p->protoAnis[id] == a &&
				_strnicmp(p->modelProtoName.ToChar(), "HUMANS_ZDF_W3_", 14) == 0)
				return true;
		}
		return false;
	}

	// Atak w biegu W3 mozliwy: ruch moze wystartowac (bron biala 1H/2H, ludzki model, bez blokady), sa sekcje RUNATK_*,
	// a postawa pochodzi ze stylu W3 (w stylach Gothica / zEBA zostaje ich atak w biegu).
	static bool MvRunAttackArmed() {
		int wm = 0;
		if (!RUN_ATTACK_ENABLED || ms.def || moveCooldown > 0.0f || !MvFightKeysOurs() || !MoveCanStart(wm))
			return false;
		if (wm != NPC_WEAPON_1HS && wm != NPC_WEAPON_2HS)
			return false;
		return !zdcore::RunAttackVariants(moveDefs, AniSwapAniWeaponMode(wm) == NPC_WEAPON_2HS).empty() &&
			MvStanceIsW3(player->GetModel(), wm);
	}

	// oCNpc::EV_AttackRun - atak w biegu gry: oCAIHuman::FightMelee przy biegu do przodu i ataku wysyla EV_ATTACKRUN,
	// a handler (co klatke, TRUE = komunikat zakonczony) gra T_xHATTACKMOVE na warstwie 2 nad biegiem. Przejety: TRUE od
	// razu (bez animacji gry), ruch W3 startuje w MovesUpdate - poza obsluga komunikatow, bo StartMove czysci kolejke.
	// W trakcie naszego ruchu atak w biegu gry tez polykany (nie gra gornej polowy nad ruchem). Hak po adresie (w UserAPI
	// oCNpc brak deklaracji _Union), adres dla silnika tego zrodla.
	static int __fastcall MvEV_AttackRun(oCNpc* self, void* edx, oCMsgAttack* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgAttack*)> Ivk_oCNpc_EV_AttackRun(
		ENGINE == Engine_G2A ? 0x007517D0 : ENGINE == Engine_G2 ? 0x006F29F0 : ENGINE == Engine_G1A ? 0x006DED30 : 0x006ABEF0,
		&MvEV_AttackRun, IvkEnabled(ENGINE));
	static int __fastcall MvEV_AttackRun(oCNpc* self, void* edx, oCMsgAttack* msg) {
		ZHookHit(ZH_EV_ATTACKRUN);
		if (self && self == player && zcfg.enabled) {
			if (TraversalActive())
				return TRUE;   // przeskok / wspiecie - bez gornej polowy ataku nad animacja
			if (ms.def) {
				// wyjscie ataku w biegu w bieg (blokada juz zdjeta): kolejny atak w biegu po zakonczeniu przejscia
				if (ms.endKind == MVEND_RUN && pendingKind != 4) {
					pendingKind = 4;
					pendingTime = MOVE_BUFFER;
					ZDebug("Atak w biegu gry w trakcie wyjscia w bieg - nastepny atak W3 po przejsciu");
				}
				return TRUE;
			}
			if (MvRunAttackArmed()) {
				if (pendingKind != 4)
					ZDebug("Atak w biegu gry przejety - ruch W3");
				pendingKind = 4;
				pendingTime = MOVE_BUFFER;
				return TRUE;
			}
		}
		if (self && self != player && PerfectDodgeSwallow(self, msg, "w biegu"))
			return TRUE;   // wrog po uniku w ostatniej chwili nie zaczyna ataku
		return Ivk_oCNpc_EV_AttackRun(self, edx, msg);
	}

	// oCNpc::EV_AttackForward / Left / Right (G2A 0x74F400 / 0x750160 / 0x750D00) - obsluga komunikatu ataku wrecz (co klatke
	// do konca ciosu). Wrog, ktory "stracil namiar" po uniku w ostatniej chwili, przez PerfectDodgeLostTime nie zaczyna
	// NOWEGO ataku: komunikat spoza migawki z chwili uniku konczony od razu (TRUE, bez animacji) - AI walki wysle nastepny
	// po oknie. Trwajacy cios (ten unikniety) gra dalej. Tylko G2A (adresy).
	static int __fastcall MvEV_AttackForward(oCNpc* self, void* edx, oCMsgAttack* msg);
	static int __fastcall MvEV_AttackLeft(oCNpc* self, void* edx, oCMsgAttack* msg);
	static int __fastcall MvEV_AttackRight(oCNpc* self, void* edx, oCMsgAttack* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgAttack*)> Ivk_oCNpc_EV_AttackForward(
		ENGINE == Engine_G2A ? 0x0074F400 : 0, &MvEV_AttackForward, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgAttack*)> Ivk_oCNpc_EV_AttackLeft(
		ENGINE == Engine_G2A ? 0x00750160 : 0, &MvEV_AttackLeft, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgAttack*)> Ivk_oCNpc_EV_AttackRight(
		ENGINE == Engine_G2A ? 0x00750D00 : 0, &MvEV_AttackRight, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall MvEV_AttackForward(oCNpc* self, void* edx, oCMsgAttack* msg) {
		ZHookHit(ZH_EV_ATTACKFORWARD);
		if (self && self != player && PerfectDodgeSwallow(self, msg, "w przod"))
			return TRUE;
		return Ivk_oCNpc_EV_AttackForward(self, edx, msg);
	}
	static int __fastcall MvEV_AttackLeft(oCNpc* self, void* edx, oCMsgAttack* msg) {
		ZHookHit(ZH_EV_ATTACKLEFT);
		if (self && self != player && PerfectDodgeSwallow(self, msg, "w lewo"))
			return TRUE;
		return Ivk_oCNpc_EV_AttackLeft(self, edx, msg);
	}
	static int __fastcall MvEV_AttackRight(oCNpc* self, void* edx, oCMsgAttack* msg) {
		ZHookHit(ZH_EV_ATTACKRIGHT);
		if (self && self != player && PerfectDodgeSwallow(self, msg, "w prawo"))
			return TRUE;
		return Ivk_oCNpc_EV_AttackRight(self, edx, msg);
	}

	// oCAniCtrl_Human::_Stand (0x6B7490) - wolany co klatke przy movlocku z oCAIHuman::Moving - widzi na modelu petle
	// biegu (_s_walkl), start biegu (_t_walk_2_walkl), bieg w tyl (_s_walkbl) albo celowanie (_s_aim), takze wygasajace
	// po starcie naszej animacji (IsAniActive po kanalach, bez patrzenia na wygaszanie), i startuje postawe _s_walk
	// (zCModel::StartAni 0x6B7688) na warstwie 1 - to gasilo ruch w pierwszej klatce, gdy bohater biegl (Alt+W =
	// bieg w oknie podwojnego Alt). Te animacje zatrzymujemy od razu (bez mieszania z biegiem, ale ruch zostaje).
	static int MvStopStandTriggers(zCModel* m) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!m || !ac)
			return 0;
		const int ids[] = { ac->_s_walkl, ac->_t_walk_2_walkl, ac->_s_walkbl, ac->_s_aim };
		int n = 0;
		for (int id : ids) {
			zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			if (a && a != ms.ani && m->IsAniActive(a)) {
				m->StopAni(a);
				n++;
			}
		}
		return n;
	}

	// Te same animacje ruchu gry (bieg / sprint z najwyzszej nakladki, start biegu, bieg w tyl, celowanie) aktywne na
	// modelu - wejscie ruchu z biegu / sprintu.
	static std::vector<zCModelAni*> MvMotionLoops(zCModel* m) {
		std::vector<zCModelAni*> out;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!m || !ac)
			return out;
		const int ids[] = { ac->_s_walkl, ac->_t_walk_2_walkl, ac->_s_walkbl, ac->_s_aim };
		for (int id : ids) {
			zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			if (a && m->GetActiveAni(a))
				out.push_back(a);
		}
		return out;
	}

	// oCAniCtrl_Human::_Stand (0x6B7490, G2A) - przy movlocku co klatke z oCAIHuman::Moving. W trakcie naszego ruchu
	// pomijany dla bohatera: animacjami steruje plugin (ruch, postawa w BeginEnding, parada). Dzieki temu petla biegu /
	// sprintu moze plynnie wygasac pod animacja ruchu (wczesniej trzeba ja bylo zatrzymac od razu - MvStopStandTriggers -
	// bo _Stand widzac ja startowal postawe i gasil ruch: przeskok przy ataku w biegu / uniku z biegu), a parada
	// z anulowania nie jest nadpisywana postawa. Tylko gdy plugin zyje (MovesUpdate w ostatnich klatkach).
	static void __fastcall MvStand(oCAniCtrl_Human* self, void* edx);
	static CInvoke<void(__fastcall*)(oCAniCtrl_Human*, void*)> Ivk_oCAniCtrl_Human_Stand(0x006B7490, &MvStand,
		MV_STAND_HOOK ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall MvStand(oCAniCtrl_Human* self, void* edx) {
		ZHookHit(ZH_STAND);
		if ((ms.def || TraversalActive() || MeditateActive()) && player && zdFrame - mvUpdateFrame <= 2 && player->IsMovLock() &&
			(self == player->GetAnictrl() || self == (oCAniCtrl_Human*)player->human_ai))
			return;
		Ivk_oCAniCtrl_Human_Stand(self, edx);
	}

	// Nakladka (nazwa jak w ms.overlay, z .MDS albo bez) lezy na modelu.
	static bool MvOverlayOn(zCModel* m, const std::string& mds) {
		std::string want = zdcore::Upper(mds);
		size_t dot = want.find('.');
		if (dot != std::string::npos)
			want = want.substr(0, dot);
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p)
				continue;
			std::string n = zdcore::Upper(p->modelProtoName.ToChar());
			size_t d = n.find('.');
			if ((d == std::string::npos ? n : n.substr(0, d)) == want)
				return true;
		}
		return false;
	}

	// Nakladka, z ktorej pochodzi animacja (najwyzsza z tym wskaznikiem), "model" = bazowy.
	static std::string MvOverlayOfAni(zCModel* m, zCModelAni* ani) {
		int id = ani ? ani->aniID : -1;
		for (int i = m->modelProtoList.GetNum() - 1; i >= 0 && id >= 0; i--) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && id < p->protoAnis.GetNum() && p->protoAnis[id] == ani)
				return i == 0 ? std::string("model") : std::string(p->modelProtoName.ToChar());
		}
		return "?";
	}

	// Aktywne animacje warstwy 1 poza nasza (nazwy) i opis do logu.
	static std::vector<std::string> MvLayerOneAnis(zCModel* m, std::string& desc) {
		std::vector<std::string> out;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p == ms.ani || p->layer != 1)
				continue;
			out.push_back(p->aniName.ToChar());
			desc += std::string(" ") + p->aniName.ToChar() + "(" + MvOverlayOfAni(m, p) + (a->isFadingOut ? ", wygasa" : "") + ")";
		}
		if (desc.empty())
			desc = " brak";
		return out;
	}

	// Kamera przy ruchu w strone kamery (unik / przewrot w tyl). Kamera Gothica (zCMovementTracker) wygladza pozycje
	// gracza i swoja srednia wykladnicza o stalej ok. N * 25 ms (UpdateSampledPlayerPos: numPreSamplesPlayerPos = 10,
	// Update / InterpolateTo: numSamplesCam = inertiaTrans * 99 + 1 = ok. 11 - deasemblacja G2A 0x4BB0F0, 0x4B73F0,
	// 0x4B8110). Przy 5-8 m/s w strone kamery bohater wyprzedza ja o 1,5-2 m i wjezdza na dolna krawedz ekranu. Na czas
	// takiego ruchu liczba probek najwyzej CAM_ASSIST_SAMPLES (ok. 0,1 s), potem przywrocona - tylko gdy nikt jej
	// w miedzyczasie nie zmienil (zmiana trybu kamery przelicza numSamplesCam). N to tylko dzielnik szybkosci doganiania
	// (bez buforow probek), zmiana nie przesuwa kamery skokiem. Tylko G2A (offsety sprawdzone). Klucz ini (inny agent):
	// MoveCameraAssist=1.
	static const bool CAM_ASSIST = true;
	static const int CAM_ASSIST_SAMPLES = 4;
	static const float CAM_ASSIST_MIN_DIST = 150.0f;   // krotszy ruch w strone kamery nie przeszkadza
	struct CamAssistState {
		zCAICamera* cam = nullptr;
		zCMovementTracker* tracker = nullptr;
		int savedPre = 0, savedCam = 0, setPre = 0, setCam = 0;
	};
	static CamAssistState camAssist;

	static void CamAssistEnd() {
		if (!camAssist.tracker)
			return;
		zCAICamera* cam = zCAICamera::GetCurrent();
		if (cam && cam == camAssist.cam && cam->moveTracker == camAssist.tracker) {
			zCMovementTracker* t = camAssist.tracker;
			if (t->numPreSamplesPlayerPos == camAssist.setPre)
				t->numPreSamplesPlayerPos = camAssist.savedPre;
			if (t->numSamplesCam == camAssist.setCam)
				t->numSamplesCam = camAssist.savedCam;
		}
		camAssist = CamAssistState();
	}

	// Po starcie ruchu (ms.dir ustawiony): ruch po torze def idzie w strone kamery - szybsze doganianie.
	static void CamAssistBegin(const zdcore::MoveDef* def) {
		CamAssistEnd();
		if (!CAM_ASSIST || ENGINE != Engine_G2A || !def || def->traj.size() < 4 || !player)
			return;
		zCAICamera* cam = zCAICamera::GetCurrent();
		zCMovementTracker* t = cam ? cam->moveTracker : nullptr;
		if (!t || !cam->camVob)
			return;
		zVEC3 hp = player->GetPositionWorld(), cp = cam->camVob->GetPositionWorld();
		float tx = def->traj[def->traj.size() - 2], tz = def->traj.back();
		if (!zdcore::MoveTowardCamera(ms.dir.n[0], ms.dir.n[2], tx, tz, cp.n[0] - hp.n[0], cp.n[2] - hp.n[2], CAM_ASSIST_MIN_DIST))
			return;
		camAssist.cam = cam;
		camAssist.tracker = t;
		camAssist.savedPre = t->numPreSamplesPlayerPos;
		camAssist.savedCam = t->numSamplesCam;
		camAssist.setPre = zdcore::CameraAssistSamples(camAssist.savedPre, CAM_ASSIST_SAMPLES);
		camAssist.setCam = zdcore::CameraAssistSamples(camAssist.savedCam, CAM_ASSIST_SAMPLES);
		t->numPreSamplesPlayerPos = camAssist.setPre;
		t->numSamplesCam = camAssist.setCam;
		ZDebug("Ruch %s w strone kamery: wygladzanie kamery %d/%d -> %d/%d probek na czas ruchu", def->id.c_str(),
			camAssist.savedPre, camAssist.savedCam, camAssist.setPre, camAssist.setCam);
	}

	static bool StartMove(const zdcore::MoveDef* def, int wm) {
		ZPROF_SCOPE("Moves.Start");   // nakladka ruchu (silnik zatrzymuje i wznawia animacje)
		zCModel* hm = player->GetModel();
		bool w3End = !def->overlayW3.empty() && MvStanceIsW3(hm, wm);
		std::string overlay = (w3End ? def->overlayW3 : def->overlay) + ".MDS";
		// zalozenie nakladki w silniku zatrzymuje wszystkie animacje - bez tego ruch wchodzil twardo, bez blendu
		// z biezacej pozy (parada, bieg); z migawka ruch miesza sie z nia przez blendIn z MSB
		if (!ZApplyOverlayKeepAnis(hm, overlay)) {
			ZLog("Ruchy: nie podpieto nakladki %s", overlay.c_str());
			return false;
		}
		const char* ani = MoveAniName(wm);
		int id = hm->GetAniIDFromAniName(ani);
		if (id < 0) {
			ZRemoveOverlayKeepAnis(hm, overlay);
			ZLog("Ruchy: brak animacji %s w %s", ani, overlay.c_str());
			return false;
		}
		if (zCEventManager* em = player->GetEM(FALSE))
			em->Clear();
		std::vector<zCModelAni*> loops = MvMotionLoops(hm);   // bieg / sprint przed ruchem
		if (oCAniCtrl_Human* tac = player->GetAnictrl())
			tac->StopTurnAnis();   // obrot A/D trzymany przy starcie (przewrot A + Alt) - inaczej przykrywa ruch
		hm->StartAni(id, 0);
		player->SetMovLock(TRUE);
		ms = MoveState();
		ms.def = def;
		ms.overlay = overlay;
		ms.aniName = ani;
		ms.aniId = id;
		ms.ani = hm->GetAniFromAniID(id);
		ms.weaponMode = wm;
		ms.heroHp = player->attribute[NPC_ATR_HITPOINTS];
		if (MV_STAND_HOOK) {
			// petla biegu wygasa pod ruchem (hak _Stand nie zgasi ruchu) - wejscie z biegu / sprintu dluzej mieszane
			if (!loops.empty()) {
				float blend = zdcore::IsRunAttack(*def) ? RUN_ENTRY_BLEND : MOVE_ENTRY_BLEND_LOOP;
				MvSetBlend(hm, ms.ani ? hm->GetActiveAni(ms.ani) : nullptr, nullptr, blend);
				for (zCModelAni* a : loops)
					if (zCModelAniActive* la = hm->GetActiveAni(a))
						MvSetBlend(hm, nullptr, la, blend);
				ZDebug("Ruch %s: wejscie z biegu (%d animacji ruchu wygasa w %.2f s)", def->id.c_str(), (int)loops.size(), blend);
			}
		}
		else if (int stopped = MvStopStandTriggers(hm))
			ZDebug("Ruch %s: zatrzymano %d animacji biegu (postawa silnika zgasilaby ruch)", def->id.c_str(), stopped);
		ms.start = player->GetPositionWorld();
		zVEC3 at = player->GetAtVectorWorld();
		float l = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
		ms.dir = l > 0.01f ? zVEC3(at.n[0] / l, 0.0f, at.n[2] / l) : zVEC3(0.0f, 0.0f, 1.0f);
		float g;
		bool onGround = MoveGroundAt(ms.start, g);
		ms.yOff = onGround ? ms.start.n[1] - g : 0.0f;
		if (mvJumpDrop && onGround) {
			// z przerwanego skoku gry: postawa na wysokosci z ziemi, nadmiar schodzi w MOVE_DROP_TIME (bez skoku pozycji)
			oCAniCtrl_Human* ac = player->GetAnictrl();
			float stand = mvStandYOff > 0.0f ? mvStandYOff : ms.yOff - (ac && ac->aboveFloor > 0.0f ? ac->aboveFloor : 0.0f);
			if (stand > 0.0f && ms.yOff > stand) {
				ms.yDrop = ms.yOff - stand;
				ms.yOff = stand;
			}
			ZDebug("Ruch %s z przerwanego skoku: vob %.0f cm ponad postawa - schodzi w %.2f s", def->id.c_str(), ms.yDrop,
				MOVE_DROP_TIME);
		}
		mvJumpDrop = false;
		ms.startGround = ms.ground = onGround ? g : ms.start.n[1] - ms.yOff;
		ms.terrain = MvHeroTerrain();
		zdcore::SampleTraj(def->traj, def->trajFps, 0.0f, ms.trajX, ms.trajZ);
		ZDebug("Ruch %s (%s%s)", def->id.c_str(), ani, w3End ? ", koniec w gardzie W3" : "");
		if (zdcore::IsEvadeMove(*def))
			MvEvadeBegin();
		MvParadeBegin();
		if (zdcore::IsEvadeMove(*def))
			MvEvadeExitBegin();
		if (RUN_CHAIN && zdcore::IsRunAttack(*def))
			ms.runExitAt = zdcore::RunExitTime(*def, RUN_EXIT_AFTER_HIT, RUN_EXIT_BLEND);
		CamAssistBegin(def);
		return true;
	}

	static zCModelAniActive* MvActiveAni(zCModel* m, const char* name) {
		int id = m->GetAniIDFromAniName(name);
		zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		return a ? m->GetActiveAni(a) : nullptr;
	}

	// Czas przejscia silnika (wygasanie 'out', wejscie 'in') na wezlach - jak SetTransitionSeconds w Director.cpp
	// (CalcWeights: waga += predkosc * czas; MSB daje 0,1 s).
	static void MvSetBlend(zCModel* m, zCModelAniActive* in, zCModelAniActive* out, float sec) {
		float speed = 1.0f / sec;
		if (in)
			in->blendInOverride = speed;
		if (out)
			out->blendOutOverride = -speed;
		for (int i = 0; i < m->nodeList.GetNum(); i++) {
			zCModelNodeInst* node = m->nodeList[i];
			if (!node)
				continue;
			for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++) {
				auto& e = node->nodeAniList[k];
				if (in && e.modelAni == in && e.blendState == zCModelNodeInst::zMDL_BLEND_STATE_FADEIN && e.weightSpeed > 0.0f)
					e.weightSpeed = speed;
				else if (out && e.modelAni == out && e.blendState == zCModelNodeInst::zMDL_BLEND_STATE_FADEOUT && e.weightSpeed < 0.0f)
					e.weightSpeed = -speed;
			}
		}
	}

	// Postawa po ruchu: silnik wygasza animacje ruchu i wprowadza postawe z najwyzszej nakladki (styl gracza: garda W3 /
	// Gothic / zEBA) przez 'blend' s - bez przeskoku z pozy konca ruchu (doklejone przejscie celuje w postawe Gothica albo,
	// w wariancie overlayW3, w garde W3; styl zEBA ma jeszcze inna). Nakladka ruchu schodzi po wymieszaniu (FinishMove,
	// z migawka animacji).
	// Nasza animacja ruchu, dopoki nakladka ruchu jest na modelu (po jej zdjeciu wskaznik juz nie zyje).
	static zCModelAni* MvAni(zCModel* m) {
		return m && ms.ani && MvOverlayOn(m, ms.overlay) ? ms.ani : nullptr;
	}

	static void BeginEnding(float blend) {
		zCModel* hm = player->GetModel();
		zCModelAni* ani = MvAni(hm);
		zCModelAniActive* out = ani ? hm->GetActiveAni(ani) : nullptr;
		if (out)
			out->blendOutOverride = -1.0f / blend;   // StartAni -> FadeOutAni bierze ja na wezly
		if (player->GetWeaponMode() == ms.weaponMode) {
			const char* stance = MoveStanceName(ms.weaponMode);
			hm->StartAni(stance, 0);
			MvSetBlend(hm, MvActiveAni(hm, stance), out ? hm->GetActiveAni(ani) : nullptr, blend);
		}
		ms.ending = blend;
	}

	// interrupted: animacje przerwala gra (trafienie, smierc) - wtedy bez wymuszania postawy. Nakladka schodzi z migawka
	// animacji (ZRemoveOverlayKeepAnis): postawa po BeginEnding albo reakcja silnika na trafienie graja dalej.
	// Koniec przez parade (MVEND_PARADE) albo bieg (MVEND_RUN): postawa tylko, gdy na warstwie 1 nic nie gra (parada /
	// petla biegu nie weszla).
	static zdcore::AttackKeys MvAttackKeys();
	static std::string MvAttackKeysDesc(const zdcore::AttackKeys& k);

	static void FinishMove(bool interrupted) {
		int kind = ms.endKind;
		zCModel* hm = player ? player->GetModel() : nullptr;
		// Debug: unik / przewrot do konca z trzymana akcja - gra (FightMelee, sterowanie Gothica 1) po zdjeciu blokady da
		// cios tylko z nowym wcisnieciem kierunku; akcja + bok trzymane bez niego = postawa bez ciosu (zamrozenie)
		if (zcfg.debug && player && ms.def && zdcore::IsEvadeMove(*ms.def) && kind == MVEND_NORMAL && !interrupted) {
			zdcore::AttackKeys k = MvAttackKeys();
			if (k.action)
				ZDebug("Ruch %s: koniec po %.2f s bez wyjscia atakiem, akcja trzymana (%s)", ms.def->id.c_str(), ms.t,
					MvAttackKeysDesc(k).c_str());
		}
		if (hm) {
			const char* stance = MoveStanceName(ms.weaponMode);
			zCModelAni* ani = MvAni(hm);
			if (ani && hm->IsAniActive(ani))
				hm->StopAni(ani);
			if (ms.obstacleFrames > 1)
				ZDebug("Ruch %s: przeszkoda w %d klatkach", ms.def ? ms.def->id.c_str() : "?", ms.obstacleFrames);
			ZRemoveOverlayKeepAnis(hm, ms.overlay);
			bool wantStance = !interrupted;
			if (kind == MVEND_PARADE || kind == MVEND_RUN || kind == MVEND_ATTACK) {
				// parada / bieg / atak gry (albo cokolwiek, co gra wystartowala w przejsciu: atak, parada w biegu) gra dalej
				wantStance = true;
				for (int i = 0; i < hm->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
					zCModelAniActive* a = hm->aniChannels[i];
					if (a && a->protoAni && a->protoAni->layer == 1 && !a->isFadingOut)
						wantStance = false;
				}
			}
			if (wantStance && player->GetWeaponMode() == ms.weaponMode && !MvActiveAni(hm, stance))
				hm->StartAni(stance, 0);
		}
		if (player) {
			player->SetMovLock(FALSE);
			ResetWeaponGrip(player);
		}
		if (ms.target)
			ms.target->Release();
		MvPassEnd();
		if (ms.track)
			ms.track->Release();
		if (ms.face)
			ms.face->Release();
		ms = MoveState();
		// kolejny ruch z bufora okna wyjscia (MVEND_CHAIN) rusza w nastepnej klatce
		moveCooldown = kind == MVEND_PARADE || kind == MVEND_CHAIN ? 0.0f : 0.15f;
		if (kind == MVEND_PARADE)
			prevParade = false;   // parada w trakcie zamachu wroga = kontra (zbocze IsParadeRunning w nastepnej klatce)
		CamAssistEnd();
	}

	static bool FoeReady(oCNpc* npc) {
		return npc && npc != player && !npc->IsDead() && !npc->IsUnconscious() && npc->homeWorld && npc->GetModel();   // homeWorld najpierw: GetModel na zdespawnowanym = UnShrink (10-20 ms)
	}

	// ---------- Unik / przewrot wzgledem celu, sterowanie, przenikanie postaci ----------

	// Wrog do sledzenia: atakuje bohatera, jest jego wrogiem albo jest do niego wrogo / zle nastawiony.
	static bool MvTrackCandidate(oCNpc* npc) {
		if (!FoeReady(npc) || npc->IsAPlayer())
			return false;
		return npc->enemy == player || player->enemy == npc || npc->GetAttitude(player) <= NPC_ATT_ANGRY;
	}

	// Cel uniku / przewrotu: przeciwnik w celowniku (fokus), a bez niego enemy bohatera w walce.
	static oCNpc* MvTrackTarget() {
		oCNpc* f = player->GetFocusNpc();
		if (f && MvTrackCandidate(f))
			return f;
		if (player->enemy && MvTrackCandidate(player->enemy))
			return player->enemy;
		return nullptr;
	}

	// Postacie w poblizu na liscie ignorowanych kolizji AI bohatera (oCAIHuman::AddIgnoreCD) - silnik sprawdza
	// CanThisCollideWith AI obu vobow (zCVob::CollectCollisionContext_Vobs 0x61E870), wiec bohater i NPC przenikaja sie
	// w obie strony. Wpisy gry (juz ignorowane) zostaja nietkniete; nasze zdejmuje MvPassEnd.
	static void MvPassBegin() {
		if (!MOVE_PASS_NPCS)
			return;
		oCAIHuman* ai = player->human_ai;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!ai || !world || !world->voblist_npcs)
			return;
		zVEC3 hp = player->GetPositionWorld();
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || MvFlatDist(hp, npc->GetPositionWorld()) > MOVE_PASS_RANGE)
				continue;
			if (!ai->CanThisCollideWith(npc))
				continue;
			ai->AddIgnoreCD(npc);
			npc->AddRef();
			ms.passNpcs.push_back(npc);
		}
		ms.passAi = ai;
		ms.passChars = true;
	}

	static void MvPassEnd() {
		bool same = ms.passAi && player && player->human_ai == ms.passAi;
		for (oCNpc* npc : ms.passNpcs) {
			if (same)
				ms.passAi->SubIgnoreCD(npc);
			npc->Release();
		}
		ms.passNpcs.clear();
		ms.passAi = nullptr;
		ms.passChars = false;
	}

	// Start uniku / przewrotu: cel (bok / tyl, przod wg MOVE_TRACK_FORWARD; nie za plecami), blokada klawiszy obrotu
	// trzymanych przy starcie (wybor kierunku), przenikanie postaci.
	static void MvEvadeBegin() {
		zdcore::SteerGateStart(ms.steerGate, MvRawState(GAME_LEFT), MvRawState(GAME_RIGHT));
		int dir = zdcore::MoveDirOfId(ms.def->id);
		if (MOVE_TRACK && dir >= 0 && zdcore::MoveTracksTarget((zdcore::MoveDir)dir, MOVE_TRACK_FORWARD)) {
			if (oCNpc* t = MvTrackTarget()) {
				zVEC3 hp = player->GetPositionWorld(), ep = t->GetPositionWorld();
				float yaw = atan2f(ms.dir.n[0], ms.dir.n[2]);
				if (zdcore::TrackStartAllowed(yaw, hp.n[0], hp.n[2], ep.n[0], ep.n[2], TRACK_PARAMS)) {
					t->AddRef();
					ms.track = t;
					ZDebug("Ruch %s wzgledem %s (odleglosc %.0f, kat %.0f st.)", ms.def->id.c_str(), t->GetName(0).ToChar(),
						MvFlatDist(hp, ep), zdcore::AngleDiff(yaw, zdcore::YawTo(hp.n[0], hp.n[2], ep.n[0], ep.n[2])) * 57.3f);
				}
				else
					ZDebug("Ruch %s: cel %s za plecami / za daleko - bez sledzenia", ms.def->id.c_str(), t->GetName(0).ToChar());
			}
		}
		MvPassBegin();
	}

	// zMouseRotationScale gry ([ENGINE], statyczna w PC_Turnings - 0x8B0E40 w G2A), inaczej domyslne 2.
	static float MvMouseRotScale() {
		if (ENGINE == Engine_G2A) {
			float s = *(float*)0x008B0E40;
			if (s > 0.01f && s < 50.0f)
				return s;
		}
		return 2.0f;
	}

	// Klatka uniku / przewrotu: obrot do celu (tor w ukladzie voba - krazenie / odskok) albo sterowanie mysza / klawiszami
	// obrotu. Przy movlocku gra nie obraca bohatera (oCAIHuman::Moving -> _Stand, PC_Turnings nie wolany), wiec obrot
	// liczymy jak gra i ustawiamy SetHeadingAtWorld (jak kontra).
	static void EvadeSteer(float dt) {
		if (MvFaceStep(dt))
			return;   // koncowka: obrot przodem do wroga (okno wyjscia)
		float yaw = atan2f(ms.dir.n[0], ms.dir.n[2]), ny = yaw;
		float turn = 0.0f;
		if (MOVE_STEER && zinput) {
			int kd = zdcore::SteerGateDir(ms.steerGate, MvRawState(GAME_LEFT), MvRawState(GAME_RIGHT));
			float mx = 0.0f, my = 0.0f, mz = 0.0f;
			zinput->GetMousePos(mx, my, mz);
			turn = zdcore::GameTurnRad(kd, true, mx, MvMouseRotScale(), dt * 1000.0f) * MOVE_STEER_SCALE;
		}
		oCNpc* t = ms.track;
		if (t && !ms.trackReleased && FoeReady(t)) {
			ms.trackOffset = zdcore::SteerOffsetStep(ms.trackOffset, turn, MOVE_STEER_TARGET_SCALE, MOVE_STEER_TARGET_MAX);
			zVEC3 hp = player->GetPositionWorld(), ep = t->GetPositionWorld();
			bool released = false;
			ny = zdcore::TrackYawStep(yaw, hp.n[0], hp.n[2], ep.n[0], ep.n[2], ms.trackOffset, dt, TRACK_PARAMS, released);
			if (released) {
				ms.trackReleased = true;
				ZDebug("Ruch %s: minal %s po %.2f s - dalej prosto (sterowanie mysza)", ms.def->id.c_str(), t->GetName(0).ToChar(), ms.t);
			}
		}
		else
			ny = yaw + turn;
		if (fabsf(ny - yaw) < 1e-5f)
			return;
		ms.dir = zVEC3(sinf(ny), 0.0f, cosf(ny));
		player->SetHeadingAtWorld(ms.dir);
	}

	// ---------- Anulowanie ruchu blokiem ----------

	static void MvParadeBegin() {
		ms.paradeFrom = zdcore::ParadeCancelFrom(*ms.def, PARADE_AFTER_HIT, PARADE_PATH_SHARE);
		ms.paradeKeyPrev = MvRawState(GAME_PARADE);
		ms.downKeyPrev = MvRawState(GAME_DOWN);
	}

	// Parada gry z koncowki ruchu: nasza animacja wygasa w PARADE_BLEND, a komunikat EV_PARADE (jak FightMelee: cel =
	// fokus) startuje T_xHPARADE_* z nakladki stylu (oCNpc::EV_Parade) - silnik miesza ja z wygasajacym ruchem. Wcisniecie
	// zuzyte (GetToggled), zeby gra po zdjeciu blokady nie powtorzyla parady. Nakladka ruchu schodzi po przejsciu
	// (FinishMove), wtedy parada w trakcie zamachu wroga moze dac kontre (zbocze IsParadeRunning).
	static void BeginParadeCancel(bool combo) {
		zCModel* hm = player->GetModel();
		zCModelAni* ani = MvAni(hm);
		zCModelAniActive* act = ani ? hm->GetActiveAni(ani) : nullptr;
		if (act)
			act->blendOutOverride = -1.0f / PARADE_BLEND;
		if (zinput) {
			zinput->GetToggled(GAME_PARADE);
			if (combo)
				zinput->GetToggled(GAME_DOWN);
		}
		zCEventManager* em = player->GetEM(FALSE);
		if (em) {
			oCMsgAttack* msg = new oCMsgAttack(oCMsgAttack::EV_PARADE, (zCVob*)player->GetFocusNpc(), 0.0f);
			em->OnMessage(msg, player);
		}
		ZDebug("Ruch %s: blok po %.2f s (okno od %.2f s) - parada%s", ms.def->id.c_str(), ms.t, ms.paradeFrom,
			em ? "" : " (brak menedzera komunikatow)");
		ms.endKind = MVEND_PARADE;
		ms.ending = PARADE_BLEND + 0.04f;
	}

	// Blok w trakcie ruchu: klawisz parady (GAME_PARADE) albo kombinacja gry atak + tyl (wcisniecie tylu przy
	// trzymanym ataku). Okno od ms.paradeFrom albo w fazie konczenia.
	static void MvParadeCheck(float dt) {
		if (!PARADE_CANCEL || ms.endKind != MVEND_NORMAL || !zinput || player->GetWeaponMode() != ms.weaponMode)
			return;
		bool parade = MvRawState(GAME_PARADE), down = MvRawState(GAME_DOWN), action = MvRawState(GAME_ACTION);
		bool press = (parade && !ms.paradeKeyPrev) || (action && down && !ms.downKeyPrev);
		bool held = parade || (action && down);
		ms.paradeKeyPrev = parade;
		ms.downKeyPrev = down;
		bool open = ms.t >= ms.paradeFrom || ms.ending >= 0.0f;
		// unik / przewrot z oknem wyjscia: blok wcisniety w trakcie pamietany do okna (EvadeExitBuffer)
		float buffer = ms.exitFrom < 1e8f && zcfg.evadeExitBuffer > PARADE_BUFFER ? zcfg.evadeExitBuffer : PARADE_BUFFER;
		if (zdcore::ParadeCancelStep(ms.paradeBuf, press, held, open, dt, buffer))
			BeginParadeCancel(!parade);
	}

	// ---------- Unik / przewrot: okno wyjscia, obrot do wroga na koncu (17.10) ----------

	// useGothic1Controls (G2A 0x8B21C0, jak w oCAIHuman::FightMelee): akcja + przod = cios; inaczej z Gothic.ini.
	static bool MvGothic1Controls() {
		if (ENGINE == Engine_G2A) {
			int v = *(int*)0x008B21C0;
			if (v == 0 || v == 1)
				return v != 0;
		}
		return zoptions && zoptions->ReadBool("GAME", "useGothic1Controls", TRUE) != 0;
	}

	// Klawisze ciosu bez naszych podmian (A/D sa dla gry wyzerowane w trakcie ruchu). Bok jak w FightMelee: obrot albo
	// strafe (A / Q, D / E).
	static zdcore::AttackKeys MvAttackKeys() {
		zdcore::AttackKeys k;
		k.action = MvRawState(GAME_ACTION);
		k.up = MvRawState(GAME_UP);
		k.left = MvRawState(GAME_LEFT) || MvRawState(GAME_STRAFELEFT);
		k.right = MvRawState(GAME_RIGHT) || MvRawState(GAME_STRAFERIGHT);
		k.actionLeft = MvRawState(GAME_ACTIONLEFT);
		k.actionRight = MvRawState(GAME_ACTIONRIGHT);
		return k;
	}

	static const char* MvAttackDirName(int dir) {
		return dir == zdcore::EXIT_ATK_LEFT ? "w lewo" : dir == zdcore::EXIT_ATK_RIGHT ? "w prawo" : "w przod";
	}

	static std::string MvAttackKeysDesc(const zdcore::AttackKeys& k) {
		char buf[160];
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "akcja %d, W %d, A %d Q %d, D %d E %d, akcja L/P %d/%d", k.action, k.up,
			MvRawState(GAME_LEFT), MvRawState(GAME_STRAFELEFT), MvRawState(GAME_RIGHT), MvRawState(GAME_STRAFERIGHT),
			k.actionLeft, k.actionRight);
		return buf;
	}

	// Diagnostyka wyjscia atakiem (Debug=1): warstwa 1 modelu przez EXIT_TRACE_TIME s albo do startu ciosu bez ruchu -
	// wpis przy kazdej zmianie (martwa klatka / poza miedzy ruchem a ciosem widac jako przerwe).
	static const float EXIT_TRACE_TIME = 0.5f;
	struct ExitTrace {
		float t = -1.0f;              // s od wyjscia; < 0 = nie sledzi
		int ani = -1;                 // animacja ciosu z komunikatu
		std::string id, last;
	};
	static ExitTrace exitTrace;

	// Start uniku / przewrotu: okno wyjscia (exit= z moves_w3.ini), blok najpozniej od niego, obrot do wroga od
	// EvadeFaceLead s przed nim, unik w ostatniej chwili.
	static void MvEvadeExitBegin() {
		float last = ms.def->cancel - MOVE_END_BLEND;
		if (zcfg.evadeEarlyExit) {
			ms.exitFrom = zdcore::MoveExitFrom(*ms.def, MOVE_END_BLEND);
			if (ms.exitFrom < ms.paradeFrom)
				ms.paradeFrom = ms.exitFrom;
		}
		float arrive = ms.exitFrom < last ? ms.exitFrom : last;
		ms.faceFrom = arrive - zcfg.evadeFaceLead > 0.0f ? arrive - zcfg.evadeFaceLead : 0.0f;
		ms.atkPrev = MvAttackKeys();   // A trzymane z Alt+A: samo zbocze akcji pozniej = cios w lewo
		if (zcfg.evadeEarlyExit)
			ZDebug("Ruch %s: okno wyjscia od %.2f s (atak / blok / kolejny ruch z bufora; koniec %.2f s), obrot do wroga od %.2f s",
				ms.def->id.c_str(), ms.exitFrom, ms.def->cancel, ms.faceFrom);
		PerfectDodgeCheck();
	}

	// Wrog do obrotu na koncu: cel sledzenia (takze po minieciu go), inaczej wrog w celowniku / enemy bohatera.
	static oCNpc* MvFaceTarget() {
		if (!ms.face) {
			oCNpc* t = ms.track && FoeReady(ms.track) ? ms.track : MvTrackTarget();
			if (!t)
				return nullptr;
			t->AddRef();
			ms.face = t;
		}
		return FoeReady(ms.face) ? ms.face : nullptr;
	}

	// Klatka obrotu na koncu uniku / przewrotu (od faceFrom, takze w fazie konczenia): w oknie wyjscia bohater patrzy na
	// wroga - pierwszy cios idzie w niego (bok / plecy po przewrocie obok). Reszta toru zostaje w kierunku sprzed obrotu
	// (pathDir) - przewrot obok wroga nie zawraca do niego. true = obrot przejal sterowanie (bez sledzenia i myszy).
	static bool MvFaceStep(float dt) {
		if (!zcfg.evadeFaceEnd || !ms.def || !zdcore::IsEvadeMove(*ms.def) || ms.t < ms.faceFrom)
			return false;
		oCNpc* t = MvFaceTarget();
		if (!t)
			return false;
		zVEC3 hp = player->GetPositionWorld(), ep = t->GetPositionWorld();
		float dist = MvFlatDist(hp, ep);
		if (dist < FACE_MIN_DIST || dist > FACE_MAX_DIST || fabsf(ep.n[1] - hp.n[1]) > MOVE_NPC_LEVEL)
			return false;
		float yaw = atan2f(ms.dir.n[0], ms.dir.n[2]);
		float to = zdcore::YawTo(hp.n[0], hp.n[2], ep.n[0], ep.n[2]);
		float last = ms.def->cancel - MOVE_END_BLEND;
		float arrive = ms.exitFrom < last ? ms.exitFrom : last;
		if (!ms.faceLogged) {
			ms.faceLogged = true;
			ms.faceStartYaw = yaw;
			ms.pathDir = ms.dir;
			ms.facing = true;
			ZDebug("Ruch %s: od %.2f s obrot do %s (%.0f st., odleglosc %.0f) - przodem do niego w %.2f s", ms.def->id.c_str(),
				ms.t, t->GetName(0).ToChar(), zdcore::AngleDiff(yaw, to) * 57.3f, dist, arrive);
		}
		float ny = zdcore::EndFaceYawStep(yaw, to, ms.t, ms.faceFrom, arrive, dt, zcfg.evadeFaceRate * (3.14159265f / 180.0f));
		if (fabsf(ny - yaw) > 1e-5f) {
			ms.dir = zVEC3(sinf(ny), 0.0f, cosf(ny));
			player->SetHeadingAtWorld(ms.dir);
		}
		return true;
	}

	// Atak gry z okna wyjscia: ruch wygasa w EXIT_ATTACK_BLEND pod ciosem z najwyzszej nakladki (styl gracza: kombo W3 /
	// Gothic / zEBA) - komunikat EV_ATTACKFORWARD / LEFT / RIGHT jak oCAIHuman::FightMelee (0x6973D8 / 0x697643 /
	// 0x6975CD: aniID = _t_hitf / _t_hitl / _t_hitr, combo 1), wiec trafienie, kombo, doskok (Reach) i krew ida zwykla
	// sciezka gry. Kierunek z chwili wcisniecia (bufor). Wcisniecia zuzyte - tez boki (Alt+A zostawia A niezuzyte, a gra
	// po zdjeciu blokady dolozylaby z niego cios w lewo).
	// 17.10 (log: zawsze S_1HATTACK, takze po przewrocie w bok): wczesniej tylko przod - cios w bok (akcja + A/D/Q/E)
	// nie wychodzil z okna, ruch dochodzil do konca i dopiero gra (FightMelee po zdjeciu blokady) zaczynala cios albo -
	// przy akcji i boku trzymanych bez nowego wcisniecia - stala w gardzie (0x698275: TRUE bez komunikatu).
	static bool BeginAttackExit(int dir) {
		zCModel* hm = player->GetModel();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCEventManager* em = player->GetEM(FALSE);
		int want = dir;
		int hitAni = ac ? zdcore::ExitAttackAni(dir, ac->_t_hitf, ac->_t_hitl, ac->_t_hitr) : -1;
		if (!hm || !ac || !em || hitAni < 0) {
			ZDebug("Ruch %s: atak z okna wyjscia niemozliwy (brak animacji ciosu / kolejki)", ms.def->id.c_str());
			return false;
		}
		zCModelAni* ani = MvAni(hm);
		if (zCModelAniActive* act = ani ? hm->GetActiveAni(ani) : nullptr)
			act->blendOutOverride = -1.0f / EXIT_ATTACK_BLEND;
		std::string keys = zcfg.debug ? MvAttackKeysDesc(MvAttackKeys()) : std::string();
		if (zinput) {
			static const unsigned short used[] = { GAME_UP, GAME_ACTION, GAME_LEFT, GAME_RIGHT, GAME_STRAFELEFT,
				GAME_STRAFERIGHT, GAME_ACTIONLEFT, GAME_ACTIONRIGHT };
			for (unsigned short k : used)
				zinput->GetToggled(k);
		}
		oCMsgAttack::TAttackSubType sub = dir == zdcore::EXIT_ATK_LEFT ? oCMsgAttack::EV_ATTACKLEFT :
			dir == zdcore::EXIT_ATK_RIGHT ? oCMsgAttack::EV_ATTACKRIGHT : oCMsgAttack::EV_ATTACKFORWARD;
		oCMsgAttack* msg = new oCMsgAttack(sub, hitAni, 1);
		em->OnMessage(msg, player);
		ms.endKind = MVEND_ATTACK;
		ms.ending = EXIT_ATTACK_BLEND + 0.04f;
		zCModelAni* hit = hm->GetAniFromAniID(hitAni);
		ZDebug("Ruch %s: atak %s po %.2f s - %s (okno od %.2f s, koniec ruchu bylby %.2f s; %s)%s%s", ms.def->id.c_str(),
			MvAttackDirName(dir), ms.t, hit ? hit->aniName.ToChar() : "?", ms.exitFrom, ms.def->cancel, keys.c_str(),
			want != dir ? " - brak animacji boku, przod" : "", ms.perfect ? " - po uniku w ostatniej chwili" : "");
		if (zcfg.debug) {
			exitTrace = ExitTrace();
			exitTrace.t = 0.0f;
			exitTrace.ani = hitAni;
			exitTrace.id = ms.def->id;
		}
		return true;
	}

	// Klatka diagnostyki wyjscia atakiem: warstwa 1, stan ciala i blokada przy zmianie; koniec, gdy cios gra bez ruchu.
	static void MvExitTraceStep(float dt) {
		if (exitTrace.t < 0.0f)
			return;
		zCModel* hm = player && player->homeWorld ? player->GetModel() : nullptr;
		if (!hm) {
			exitTrace.t = -1.0f;
			return;
		}
		exitTrace.t += dt;
		std::string desc;
		for (int i = 0; i < hm->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = hm->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1)
				continue;
			char buf[96];
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, " %s(%s)", p->aniName.ToChar(), a->isFadingOut ? "wygasa" :
				p->aniID == exitTrace.ani ? "cios" : "gra");
			desc += buf;
		}
		if (desc.empty())
			desc = " brak";
		zCModelAni* hit = hm->GetAniFromAniID(exitTrace.ani);
		zCModelAniActive* ha = hit ? hm->GetActiveAni(hit) : nullptr;
		bool swing = ha && !ha->isFadingOut;
		char state[96];
		_snprintf_s(state, sizeof(state), _TRUNCATE, "; ruch %s, blokada %d, stan %d", ms.def ? "wygasa" : "zdjety",
			player->IsMovLock(), player->GetBodyState() & BS_ONLY_STATE);
		std::string line = desc + state;
		if (line != exitTrace.last) {
			ZDebug("Wyjscie atakiem %s +%.2f s: warstwa 1:%s%s", exitTrace.id.c_str(), exitTrace.t, line.c_str(),
				swing ? "" : " - cios jeszcze nie gra");
			exitTrace.last = line;
		}
		if ((swing && !ms.def) || exitTrace.t >= EXIT_TRACE_TIME)
			exitTrace.t = -1.0f;
	}

	// Kolejny unik / przewrot z bufora: krotkie przejscie do postawy (EXIT_CHAIN_BLEND), potem od razu nowy ruch
	// (pendingKind, bez odnowienia).
	static void BeginChainExit(int action, int dir) {
		pendingKind = action == zdcore::EXIT_ROLL ? 2 : 1;
		pendingTime = MOVE_BUFFER;
		if (action == zdcore::EXIT_ROLL && dir >= 0)
			pendingRollDir = dir;
		ms.endKind = MVEND_CHAIN;
		ZDebug("Ruch %s: po %.2f s kolejny %s z bufora (okno od %.2f s)", ms.def->id.c_str(), ms.t,
			action == zdcore::EXIT_ROLL ? "przewrot" : "unik", ms.exitFrom);
		BeginEnding(EXIT_CHAIN_BLEND);
	}

	// Klatka uniku / przewrotu (tez w zwyklym konczeniu): wcisniecia ataku / uniku / przewrotu do bufora, w oknie wyjscia -
	// wyjscie.
	// pressKind: 1 unik, 2 przewrot (zbocze klawisza w tej klatce), rollDir - kierunek przewrotu.
	static bool MvExitCheck(float dt, int pressKind, int rollDir) {
		// takze w zwyklym konczeniu (przejscie do postawy) - atak / kolejny ruch nie czeka na zdjecie blokady
		if (!zcfg.evadeEarlyExit || ms.exitFrom > 1e8f || ms.endKind != MVEND_NORMAL || player->GetWeaponMode() != ms.weaponMode)
			return false;
		zdcore::AttackKeys keys = MvAttackKeys();
		int attack = zdcore::AttackPressDir(MvGothic1Controls(), keys, ms.atkPrev);
		ms.atkPrev = keys;
		bool open = ms.t >= ms.exitFrom;
		if (attack != zdcore::EXIT_ATK_NONE) {
			zdcore::ExitQueuePush(ms.exitQ, zdcore::EXIT_ATTACK, attack);   // kierunek z chwili wcisniecia
			if (!open)
				ZDebug("Ruch %s: atak %s po %.2f s - czeka na okno wyjscia (%.2f s)", ms.def->id.c_str(),
					MvAttackDirName(attack), ms.t, ms.exitFrom);
		}
		if ((pressKind == 1 || pressKind == 2) && !zdcore::ChainPressAccepted(ms.t, EXIT_CHAIN_MIN_PRESS))
			ZDebug("Ruch %s: %s po %.2f s - za wczesnie na kolejny ruch (od %.2f s), pominiety", ms.def->id.c_str(),
				pressKind == 2 ? "przewrot" : "unik", ms.t, EXIT_CHAIN_MIN_PRESS);
		else if (pressKind == 1 || pressKind == 2) {
			zdcore::ExitQueuePush(ms.exitQ, pressKind == 2 ? zdcore::EXIT_ROLL : zdcore::EXIT_DODGE, pressKind == 2 ? rollDir : -1);
			if (!open)
				ZDebug("Ruch %s: %s po %.2f s - czeka na okno wyjscia (%.2f s)", ms.def->id.c_str(),
					pressKind == 2 ? "przewrot" : "unik", ms.t, ms.exitFrom);
		}
		int dir = ms.exitQ.dir;
		int a = zdcore::ExitQueueStep(ms.exitQ, open, dt, zcfg.evadeExitBuffer);
		if (a == zdcore::EXIT_ATTACK)
			return BeginAttackExit(dir);
		if (a == zdcore::EXIT_DODGE || a == zdcore::EXIT_ROLL) {
			BeginChainExit(a, dir);
			return true;
		}
		return false;
	}

	// ---------- Atak w biegu: wyjscie prosto w bieg / sprint ----------

	// Z W (bez S) trzymanym po ciosie bohater biegnie dalej.
	static bool MvRunExitWanted() {
		return MvRawState(GAME_UP) && !MvRawState(GAME_DOWN) && player->GetWeaponMode() == ms.weaponMode;
	}

	// Petla biegu gry (oCAniCtrl_Human::_s_walkl - z najwyzszej nakladki: trucht W3 / sprint) startuje pod wygasajaca
	// animacja ataku, blokada ruchu zdjeta od razu: oCAIHuman::_WalkCycle z W widzi bieg i biegnie dalej (bez
	// T_xHRUN_2_xHRUNL z gardy), a zEBA przy Shift wraca w sprint. Nakladka ruchu schodzi po przejsciu.
	static bool BeginRunExit() {
		zCModel* hm = player->GetModel();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		if (!hm || !ac || ac->_s_walkl < 0)
			return false;
		zCModelAni* ani = MvAni(hm);
		zCModelAniActive* out = ani ? hm->GetActiveAni(ani) : nullptr;
		if (out)
			out->blendOutOverride = -1.0f / RUN_EXIT_BLEND;
		hm->StartAni(ac->_s_walkl, 0);
		zCModelAni* loop = hm->GetAniFromAniID(ac->_s_walkl);
		zCModelAniActive* in = loop ? hm->GetActiveAni(loop) : nullptr;
		if (!in) {
			ZDebug("Ruch %s: petla biegu nie wystartowala - koniec w gardzie", ms.def->id.c_str());
			return false;
		}
		MvSetBlend(hm, in, ani ? hm->GetActiveAni(ani) : nullptr, RUN_EXIT_BLEND);
		player->SetMovLock(FALSE);
		ms.endKind = MVEND_RUN;
		ms.ending = RUN_EXIT_BLEND;
		ZDebug("Ruch %s: po %.2f s z W prosto w bieg (%s)", ms.def->id.c_str(), ms.t, loop->aniName.ToChar());
		return true;
	}

	static float MvAniFps(zCModel* m, zCModelAni* a);

	// Wrog w zasiegu kontry, ale z blokiem za wczesnie (CounterWindow) - do logu.
	struct CounterEarly {
		oCNpc* npc = nullptr;
		float tth = -1.0f;
		std::string ani;
	};

	// Czas do trafienia (s) z biezacej animacji ciosu wroga (jak przy uniku w ostatniej chwili); -1 = brak.
	static float FoeTimeToHit(oCNpc* npc, std::string* aniName = nullptr) {
		oCAniCtrl_Human* ac = npc->GetAnictrl();
		zCModel* m = npc->GetModel();
		if (!ac || !m || ac->hitAniID < 0 || ac->comboNr < 0 || ac->comboNr >= ANI_MAX_HIT)
			return -1.0f;
		zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
		if (!a || !a->protoAni || a->isFadingOut)
			return -1.0f;
		if (aniName)
			*aniName = a->protoAni->aniName.ToChar();
		return zdcore::EnemyTimeToHit((float)ac->comboInfo[ac->comboNr].hitOptFrame, a->actFrame, MvAniFps(m, a->protoAni));
	}

	// Najblizszy przeciwnik przed bohaterem, ktory go atakuje i jest w zamachu (needPreHit) - albo dowolny wrogi (konsola).
	// window > 0 (CounterWindow): tylko wrog, ktorego cios trafi najwyzej za tyle s; blizszy za wczesnie trafia do early.
	static oCNpc* CounterTarget(bool needPreHit, float window = 0.0f, CounterEarly* early = nullptr) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return nullptr;
		zVEC3 hp = player->GetPositionWorld(), at = player->GetAtVectorWorld();
		float l = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
		if (l < 0.01f)
			return nullptr;
		oCNpc* best = nullptr;
		float bestD2 = 0.0f;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!FoeReady(npc) || npc->enemy != player)
				continue;
			oCAniCtrl_Human* ac = npc->GetAnictrl();
			if (needPreHit && !(ac && ac->IsInPreHit()))
				continue;
			zVEC3 p = npc->GetPositionWorld();
			if (!zdcore::CounterInReach(hp.n[0], hp.n[2], at.n[0] / l, at.n[2] / l, p.n[0], p.n[2], COUNTER_REACH, COUNTER_MIN_DOT))
				continue;
			float d2 = (p.n[0] - hp.n[0]) * (p.n[0] - hp.n[0]) + (p.n[2] - hp.n[2]) * (p.n[2] - hp.n[2]);
			if (needPreHit && window > 0.0f) {
				std::string ani;
				float tth = FoeTimeToHit(npc, &ani);
				if (!zdcore::CounterTiming(tth, window)) {
					if (early && !early->npc) {
						early->npc = npc;
						early->tth = tth;
						early->ani = ani;
					}
					continue;
				}
			}
			if (!best || d2 < bestD2) {
				best = npc;
				bestD2 = d2;
			}
		}
		return best;
	}

	// Animacja parady bohatera (IsParadeRunning) i nakladka (.MDS), z ktorej gra - jak PryParadeAni w Parry.cpp.
	static bool MvParadeAni(oCAniCtrl_Human* ac, std::string& ani, std::string& overlay, bool& fading) {
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		zCModelAniActive* a = ac && m ? ac->IsParadeRunning() : nullptr;
		if (!a || !a->protoAni)
			return false;
		ani = a->protoAni->aniName.ToChar();
		fading = a->isFadingOut != 0;
		overlay = ProtoOfAniName(m, a->protoAni);   // nie SearchAni - crash NB na pustych wpisach nakladki
		return true;
	}

	// Promien przeciwnika z jego bboxa (czlowiek ok. 25 cm, troll wiecej).
	static float FoeRadius(oCNpc* foe) {
		const zTBBox3D& bb = foe->bbox3D;
		float w = bb.maxs.n[0] - bb.mins.n[0], dp = bb.maxs.n[2] - bb.mins.n[2];
		float r = 0.35f * (w < dp ? w : dp);
		return r < FOE_RADIUS_MIN ? FOE_RADIUS_MIN : (r > FOE_RADIUS_MAX ? FOE_RADIUS_MAX : r);
	}

	// Odleglosc srodkow w chwili ciosu: zasieg ciosu z animacji (reach=, ostrze / bark / stopa) + promien przeciwnika.
	static float CounterDesired(const zdcore::MoveDef* d, oCNpc* foe) {
		return (d->reach >= 0.0f ? d->reach : COUNTER_DEFAULT_REACH) + FoeRadius(foe);
	}

	static bool IsRepel(const zdcore::MoveDef* d) {
		return d && (d->id.rfind("BASH", 0) == 0 || d->id.rfind("KICK", 0) == 0);
	}

	static bool IsKick(const zdcore::MoveDef* d) {
		return d && d->id.rfind("KICK", 0) == 0;
	}

	// Kontra siegnie przeciwnika: wypad z animacji (wydluzony najwyzej LUNGE_MAX) wystarcza do zasiegu ciosu;
	// kopniak nie z bliska (noga w przeciwniku).
	static bool CounterFits(const zdcore::MoveDef* d, oCNpc* foe) {
		if (!d || d->hitTime <= 0.0f)
			return false;
		float pos, neg;
		zdcore::TrajForward(d->traj, d->trajFps, 0.0f, d->hitTime, pos, neg);
		float need = MvFlatDist(player->GetPositionWorld(), foe->GetPositionWorld()) - CounterDesired(d, foe);
		if (IsKick(d) && need < -KICK_CRAMPED)
			return false;
		return zdcore::LungeReachable(need, pos, neg, LUNGE_MAX, LUNGE_TOL);
	}

	// Kontra: odbicie z riposta (COUNTER_L/R, strona z kierunku) albo - z szansa CounterRepelChance - repel W3:
	// pchniecie barkiem (2 na 3) albo kopniecie. Pierwsza, ktora siegnie przeciwnika; zadna = zwykla parada.
	static const zdcore::MoveDef* PickCounter(oCNpc* foe, bool l, bool r) {
		const zdcore::MoveDef* bash = zdcore::FindMove(moveDefs, "BASH", zdcore::MOVE_FORWARD);
		const zdcore::MoveDef* kick = zdcore::FindMove(moveDefs, "KICK", zdcore::MOVE_FORWARD);
		zdcore::MoveDir side = l != r ? (l ? zdcore::MOVE_LEFT : zdcore::MOVE_RIGHT) : (rand() & 1) ? zdcore::MOVE_LEFT : zdcore::MOVE_RIGHT;
		const zdcore::MoveDef* riposte = zdcore::FindMove(moveDefs, "COUNTER", side);
		const zdcore::MoveDef* order[3] = { riposte, nullptr, nullptr };
		if ((int)(ZRand32() % 100) < zcfg.counterRepelChance) {
			bool bashFirst = bash && (!kick || ZRand32() % 3 < 2);
			order[0] = bashFirst ? bash : kick;
			order[1] = bashFirst ? kick : bash;
			order[2] = riposte;
		}
		for (const zdcore::MoveDef* d : order)
			if (CounterFits(d, foe))
				return d;
		ZDebug("Kontra: %s za daleko (%.0f cm) - zwykla parada", foe->GetName(0).ToChar(),
			MvFlatDist(player->GetPositionWorld(), foe->GetPositionWorld()));
		return nullptr;
	}

	static bool StartCounter(const zdcore::MoveDef* def, oCNpc* foe, int wm) {
		if (!StartMove(def, wm))
			return false;
		foe->AddRef();
		ms.target = foe;
		ms.desired = CounterDesired(def, foe);
		ZDebug("Kontra %s na %s: odleglosc %.0f, w chwili ciosu %.0f", def->id.c_str(), foe->GetName(0).ToChar(),
			MvFlatDist(player->GetPositionWorld(), foe->GetPositionWorld()), ms.desired);
		return true;
	}

	// Do chwili ciosu bohater obraca sie do przeciwnika (najwyzej COUNTER_TURN_RATE) - wypad i cios ida tam, gdzie on stoi.
	static void CounterSteer(float dt) {
		oCNpc* foe = ms.target;
		if (!FoeReady(foe) || ms.def->hitTime < 0.0f || ms.t > ms.def->hitTime + 0.05f)
			return;
		zVEC3 a = player->GetPositionWorld(), b = foe->GetPositionWorld();
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2];
		if (dx * dx + dz * dz < 1.0f)
			return;
		float yaw = atan2f(ms.dir.n[0], ms.dir.n[2]);
		float ny = zdcore::TurnToward(yaw, atan2f(dx, dz), COUNTER_TURN_RATE * dt);
		ms.dir = zVEC3(sinf(ny), 0.0f, cosf(ny));
		player->SetHeadingAtWorld(ms.dir);
	}

	static void StopRepelPush() {
		if (repelPush.npc)
			repelPush.npc->Release();
		repelPush = RepelPushState();
	}

	static void StartRepelPush(oCNpc* foe, float dist) {
		StopRepelPush();
		zVEC3 a = player->GetPositionWorld(), b = foe->GetPositionWorld();
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2], l = sqrtf(dx * dx + dz * dz);
		float g;
		if (!MoveGroundAt(b, g))
			return;
		foe->AddRef();
		repelPush.npc = foe;
		repelPush.dir = l > 1.0f ? zVEC3(dx / l, 0.0f, dz / l) : ms.dir;
		repelPush.dist = dist;
		repelPush.yOff = b.n[1] - g;
	}

	// Odepchniecie: przeciwnik przesuwany (oprocz ruchu z animacji zatoczenia) z kolizja jak bohater; sciana = koniec.
	static void RepelPushUpdate(float dt) {
		if (!repelPush.npc)
			return;
		if (!FoeReady(repelPush.npc) || repelPush.t >= REPEL_PUSH_TIME || !ogame || !ogame->GetGameWorld()) {
			StopRepelPush();
			return;
		}
		repelPush.t += dt;
		float want = zdcore::PushOffset(repelPush.t, repelPush.dist, REPEL_PUSH_TIME) - repelPush.done;
		if (want < 0.05f)
			return;
		zVEC3 cur = repelPush.npc->GetPositionWorld(), n;
		zVEC3 d(repelPush.dir.n[0] * want, 0.0f, repelPush.dir.n[2] * want);
		float g;
		static const zdcore::TerrainLimits lim = zdcore::TerrainLimits();
		if (MvTryStep(repelPush.npc, player, cur, cur.n[1] - repelPush.yOff, d, lim, g, n) != MVSTEP_OK) {
			StopRepelPush();
			return;
		}
		repelPush.npc->SetPositionWorld(zVEC3(cur.n[0] + d.n[0], g + repelPush.yOff, cur.n[2] + d.n[2]));
		repelPush.done += want;
	}

	// Zatoczenie jak reakcja silnika na mocny cios (oCNpc::OnDamage_Anim, "Stumble"): kolejka AI wyczyszczona, akcja
	// przerwana, stan BS_STUMBLE i animacja jako komunikat EV_PLAYANI_NOOVERLAY - AI czeka do konca zatoczenia, zamiast
	// nadpisac je w nastepnej klatce kolejnym atakiem (sam StartAni bywal bez efektu). Cios z przodu = T_STUMBLEB
	// (do tylu), od tylu T_STUMBLE - jak w silniku (azymut > 90 st. = "B").
	// 1 = zatacza sie, 0 = stan bez przerywania (skrypt, interakcja), -1 = model bez tych animacji.
	static int RepelStagger(oCNpc* foe) {
		zCModel* m = foe->GetModel();
		zVEC3 fp = foe->GetPositionWorld(), hp = player->GetPositionWorld(), at = foe->GetAtVectorWorld();
		bool front = (hp.n[0] - fp.n[0]) * at.n[0] + (hp.n[2] - fp.n[2]) * at.n[2] >= 0.0f;
		const char* ani = front ? "T_STUMBLEB" : "T_STUMBLE";
		if (m->GetAniIDFromAniName(ani) < 0)
			ani = front ? "T_STUMBLE" : "T_STUMBLEB";
		if (m->GetAniIDFromAniName(ani) < 0)
			return -1;
		int bs = foe->GetBodyState() & BS_ONLY_STATE;
		bool fighting = bs == BS_STAND || bs == BS_WALK || bs == BS_SNEAK || bs == BS_RUN || bs == BS_SPRINT ||
			bs == BS_AIMNEAR || bs == BS_AIMFAR || bs == BS_HIT || bs == BS_PARADE || bs == BS_STUMBLE;
		if (foe->bodyStateInterruptableOverride || !(fighting || foe->IsBodyStateInterruptable()))
			return 0;
		zCEventManager* em = foe->GetEM(FALSE);
		if (!em)
			return 0;
		foe->ClearEM();
		foe->Interrupt(FALSE, FALSE);
		foe->SetBodyState(BS_STUMBLE);
		oCMsgConversation* msg = new oCMsgConversation(oCMsgConversation::EV_PLAYANI_NOOVERLAY, zSTRING(ani));
		msg->SetHighPriority(TRUE);
		em->OnMessage(msg, foe);
		return 1;
	}

	// Obrazenia ciosu bohatera bronia jak w grze (obrazenia broni + sila) - podstawa obrazen zapasowych i repel.
	static int HeroWeaponDamage() {
		oCItem* w = player->GetWeapon();
		return w ? w->GetFullDamage() : player->GetFullDamage();
	}

	// NPC_FLAG_IMMORTAL (np. Greg, Onar, Kati) - silnik nie odejmuje mu HP, cios nic nie widac w logu.
	static bool NpcImmortal(oCNpc* npc) {
		return npc && npc->HasFlag(NPC_FLAG_IMMORTAL) != 0;
	}

	// Obrazenia przez oCNpc::OnDamage(zCVob*, zCVob*, float, int, zVEC3) 0x67B860: deskryptor z napastnikiem (bohater),
	// bronia z reki, fDamageTotal i typem (enuModeDamage), potem wirtualny OnDamage(oSDamageDescriptor&) 0x6660E0 - ta
	// sama sciezka co cios silnika (ochrona, NPC_MINIMAL_DAMAGE, niesmiertelnosc, smierc / powalenie, reakcja, krew,
	// dzwiek, percepcje; nasze haki odciecia). CreateHit (0x6B0830) zadaje zawsze obrazenia broni (typ z broni) - do
	// ciosu obuchowego bark / kopniak sie nie nadaje.
	static void HeroDamage(oCNpc* foe, float damage, int type) {
		zVEC3 b = foe->GetPositionWorld();
		zVEC3 at(b.n[0], b.n[1] + 40.0f, b.n[2]);
		foe->OnDamage(player, player, damage, type, at);
	}

	// Cios bronia: CreateHit (obrazenia broni, krytyk wg talentu, krew, dzwiek), a gdyby nic nie zadal - obrazenia broni
	// przez OnDamage. Log: HP przed / po i niesmiertelnosc celu.
	static void HeroWeaponStrike(oCNpc* foe, const char* label, float dist) {
		int hp = foe->attribute[NPC_ATR_HITPOINTS];
		bool immortal = NpcImmortal(foe);
		if (oCAniCtrl_Human* ac = player->GetAnictrl())
			ac->CreateHit(foe);
		bool viaEngine = foe->attribute[NPC_ATR_HITPOINTS] != hp || foe->IsDead() || foe->IsUnconscious();
		bool fallback = !viaEngine && !immortal && FoeReady(foe);
		if (fallback) {
			int type = NpcHasEdgeWeapon(player) ? oEDamageType_Edge : oEDamageType_Blunt;
			HeroDamage(foe, (float)(HeroWeaponDamage() + player->attribute[NPC_ATR_STRENGTH]), type);
		}
		ZDebug("%s: %s HP %d -> %d (%s), niesmiertelny (NPC_FLAG_IMMORTAL): %s, odleglosc %.0f", label, foe->GetName(0).ToChar(),
			hp, foe->attribute[NPC_ATR_HITPOINTS], viaEngine ? "CreateHit" : fallback ? "OnDamage" : "brak obrazen",
			immortal ? "TAK" : "nie", dist);
	}

	// Repel W3: cios obuchowy (bark / kopniak - czesc ciosu bronia), potem przeciwnik traci atak, zatacza sie i jest
	// odpychany (obrazenia najpierw: reakcja silnika na trafienie ustepuje zatoczeniu). Zabity / powalony juz sie nie
	// zatacza. Stwor bez animacji zatoczenia dostaje sam cios.
	// Dzwiek trafienia barkiem / kopniakiem na trafionym (3D, bez slotu - nie gasi jego glosu bolu z OnDamage).
	static void MvImpactSound(zCVob* at, bool kick) {
		if (!zsound || !at)
			return;
		zsound->PlaySound3D(zSTRING(kick ? REPEL_SOUND_KICK : REPEL_SOUND_BASH), at, zSND_SLOT_NONE, nullptr);
	}

	static void RepelStrike(oCNpc* foe) {
		bool kick = IsKick(ms.def);
		int hp = foe->attribute[NPC_ATR_HITPOINTS];
		bool immortal = NpcImmortal(foe);
		float dmg = zdcore::StrikeDamage(HeroWeaponDamage(), player->attribute[NPC_ATR_STRENGTH],
			kick ? REPEL_DAMAGE_KICK : REPEL_DAMAGE_BASH);
		MvImpactSound(foe, kick);   // tylko tu: CounterStrike wola RepelStrike dopiero, gdy wrog jest w zasiegu ciosu
		HeroDamage(foe, dmg, oEDamageType_Blunt);
		ZDebug("Kontra repel %s: %s obrazenia %.0f obuchowe, HP %d -> %d, niesmiertelny (NPC_FLAG_IMMORTAL): %s",
			kick ? "kopniak" : "bark", foe->GetName(0).ToChar(), dmg, hp, foe->attribute[NPC_ATR_HITPOINTS], immortal ? "TAK" : "nie");
		if (!FoeReady(foe) || DirectorIsVictim(foe))
			return;   // zabity, powalony albo w finisherze
		int r = RepelStagger(foe);
		if (r > 0) {
			StartRepelPush(foe, kick ? REPEL_PUSH_KICK : REPEL_PUSH_BASH);
			ZDebug("Kontra repel: %s zatacza sie (stan %d)", foe->GetName(0).ToChar(), foe->GetBodyState() & BS_ONLY_STATE);
			return;
		}
		ZDebug("Kontra repel: %s %s - bez zatoczenia", foe->GetName(0).ToChar(),
			r < 0 ? "bez T_STUMBLE" : "w stanie bez przerywania");
	}

	// Riposta: cios silnika (obrazenia broni, ochrona, krew, dzwiek trafienia), gdy przeciwnik w zasiegu ciosu.
	static void CounterStrike() {
		oCNpc* foe = ms.target;
		if (!FoeReady(foe))
			return;
		zVEC3 a = player->GetPositionWorld(), b = foe->GetPositionWorld();
		float dist = MvFlatDist(a, b);
		if (!zdcore::CounterInReach(a.n[0], a.n[2], ms.dir.n[0], ms.dir.n[2], b.n[0], b.n[2], ms.desired + COUNTER_HIT_TOL, 0.3f)) {
			ZDebug("Kontra %s: pudlo (odleglosc %.0f, zasieg %.0f)", ms.def->id.c_str(), dist, ms.desired);
			return;
		}
		if (IsRepel(ms.def)) {
			RepelStrike(foe);
			return;
		}
		HeroWeaponStrike(foe, "Kontra", dist);
	}

	static const char* LungeLabel(const zdcore::MoveDef* d) {
		return d && zdcore::IsRunAttack(*d) ? "Atak w biegu" : "Atak z wyskoku";
	}

	// Cel ataku z wypadem (z wyskoku / w biegu): przeciwnik w celowniku (fokus), a bez niego najblizszy wrog bohatera
	// przed nim blizej niz range - o ile wypad ktoregos wariantu (wydluzony najwyzej LUNGE_MAX) siegnie go w chwili
	// ciosu. Bez celu atak leci prosto przed siebie.
	static oCNpc* LungeTarget(const std::vector<const zdcore::MoveDef*>& vars, float range) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zVEC3 hp = player->GetPositionWorld(), at = player->GetAtVectorWorld();
		float l = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
		if (!world || l < 0.01f)
			return nullptr;
		auto ok = [&](oCNpc* npc) {
			if (!FoeReady(npc) || npc->IsAPlayer())
				return false;
			zVEC3 p = npc->GetPositionWorld();
			if (!zdcore::CounterInReach(hp.n[0], hp.n[2], at.n[0] / l, at.n[2] / l, p.n[0], p.n[2], range, COUNTER_MIN_DOT))
				return false;
			for (const zdcore::MoveDef* d : vars)
				if (CounterFits(d, npc))
					return true;
			return false;
		};
		oCNpc* focus = player->GetFocusNpc();
		if (focus && ok(focus))
			return focus;
		oCNpc* best = nullptr;
		float bestD2 = 0.0f;
		for (auto it = world->voblist_npcs ? world->voblist_npcs->next : nullptr; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc->enemy != player || !ok(npc))
				continue;
			zVEC3 p = npc->GetPositionWorld();
			float d2 = (p.n[0] - hp.n[0]) * (p.n[0] - hp.n[0]) + (p.n[2] - hp.n[2]) * (p.n[2] - hp.n[2]);
			if (!best || d2 < bestD2) {
				best = npc;
				bestD2 = d2;
			}
		}
		return best;
	}

	// Skala krokow wypadu wariantu do chwili ciosu, przy ktorej cel bedzie w zasiegu (bez przycinania) - wybor wariantu
	// pasujacego do odleglosci (dlugi wypad na bliski cel = dreptanie w miejscu).
	static float LungeNeed(const zdcore::MoveDef* d, oCNpc* foe) {
		float pos, neg;
		zdcore::TrajForward(d->traj, d->trajFps, 0.0f, d->hitTime, pos, neg);
		return zdcore::LungeNeedScale(MvFlatDist(player->GetPositionWorld(), foe->GetPositionWorld()) - CounterDesired(d, foe),
			pos, neg);
	}

	static bool StartLungeMove(const zdcore::MoveDef* def, int wm, oCNpc* foe) {
		if (!StartMove(def, wm))
			return false;
		if (foe) {
			foe->AddRef();
			ms.target = foe;
			ms.desired = CounterDesired(def, foe);
		}
		else if (RUN_CARRY && zdcore::IsRunAttack(*def)) {
			// bez celu: wypad nie zwalnia ze sprintu (z celem wypad i tak skaluje LungeScale do odleglosci)
			float clip = zdcore::TrajSpeedAt(def->traj, def->trajFps, 0.0f, 0.12f);
			ms.carry = zdcore::EntrySpeedRatio(heroSpeed, clip, RUN_CARRY_MAX);
			if (ms.carry > 1.01f)
				ZDebug("%s %s: rozped z wejscia %.0f cm/s (tor %.0f) - wypad x%.2f do ciosu", LungeLabel(def), def->id.c_str(),
					heroSpeed, clip, ms.carry);
		}
		ZDebug("%s %s: cel %s, odleglosc %.0f, w chwili ciosu %.0f", LungeLabel(def), def->id.c_str(),
			foe ? foe->GetName(0).ToChar() : "brak (prosto)", foe ? MvFlatDist(player->GetPositionWorld(), foe->GetPositionWorld()) : 0.0f,
			foe ? ms.desired : 0.0f);
		return true;
	}

	// Atak z wypadem (run = w biegu, inaczej z wyskoku): wariant dla broni losowany przy kazdym uzyciu, bez powtorzenia
	// poprzedniego; z celem tylko sposrod tych, ktorych wypad pasuje do odleglosci (PickMoveVariant).
	static bool StartLungeAttack(bool run, int wm) {
		bool two = AniSwapAniWeaponMode(wm) == NPC_WEAPON_2HS;   // warianty wg animacji (zamiana 1H <-> 2H)
		std::vector<const zdcore::MoveDef*> vars = run ? zdcore::RunAttackVariants(moveDefs, two) : zdcore::JumpAttackVariants(moveDefs, two);
		if (vars.empty())
			return false;
		oCNpc* foe = LungeTarget(vars, run ? RUN_TARGET_RANGE : JUMP_TARGET_RANGE);
		std::vector<float> scales;
		std::string desc;
		if (foe)
			for (const zdcore::MoveDef* d : vars) {
				scales.push_back(LungeNeed(d, foe));
				char buf[64];
				sprintf_s(buf, " %s=%.2f", d->id.c_str(), scales.back());
				desc += buf;
			}
		std::string& last = lastLungeVariant[run ? 1 : 0];
		const zdcore::MoveDef* def = zdcore::PickMoveVariant(vars, scales, ZRand32(), last);
		if (foe)
			ZDebug("%s: skala wypadu wariantow do celu:%s -> %s", LungeLabel(def), desc.c_str(), def ? def->id.c_str() : "-");
		if (!def || !StartLungeMove(def, wm, foe))
			return false;
		last = def->id;
		return true;
	}

	// Ciecie ataku z wypadem: cios bronia (CreateHit - obrazenia broni, krew; odciecie przy smierci przez haki DoDie)
	// w cel i w innych wrogow bohatera w zasiegu ostrza przed nim (Rend i ataki z rozbiegu w W3 tez tna wszystkich przed
	// Geraltem).
	static void LungeStrike() {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return;
		zVEC3 a = player->GetPositionWorld();
		float reach = ms.def->reach >= 0.0f ? ms.def->reach : COUNTER_DEFAULT_REACH;
		std::vector<oCNpc*> hit;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!FoeReady(npc) || npc->IsAPlayer() || (npc != ms.target && npc->enemy != player))
				continue;
			zVEC3 b = npc->GetPositionWorld();
			if (zdcore::CounterInReach(a.n[0], a.n[2], ms.dir.n[0], ms.dir.n[2], b.n[0], b.n[2],
				reach + FoeRadius(npc) + COUNTER_HIT_TOL, 0.3f))
				hit.push_back(npc);
		}
		for (oCNpc* npc : hit)
			if (FoeReady(npc))
				HeroWeaponStrike(npc, LungeLabel(ms.def), MvFlatDist(a, npc->GetPositionWorld()));
		if (hit.empty())
			ZDebug("%s %s: pudlo (zasieg %.0f)", LungeLabel(ms.def), ms.def->id.c_str(), reach);
	}

	// Krok voba po torze w tej klatce (przyrost toru w biezacym kierunku). Kontra: wypad do chwili ciosu skalowany tak,
	// by przeciwnik byl wtedy w zasiegu (LungeScale), po ciosie bez wchodzenia w niego. true = unik / przewrot utknal.
	// NPC stoi (omijanie / wypychanie konca ruchu) na poziomie bohatera.
	static bool MvNpcBlocksPass(oCNpc* npc, const zVEC3& hp) {
		if (!npc || !npc->homeWorld || npc->IsDead() || npc->IsUnconscious())
			return false;
		return fabsf(npc->GetPositionWorld().n[1] - hp.n[1]) < MOVE_NPC_LEVEL;
	}

	// Przenikanie: krok d omija srodki postaci kolem MOVE_NPC_RADIUS (slizg po brzegu bez hamowania).
	static void MvDeflectNpcs(zVEC3& d) {
		zVEC3 hp = player->GetPositionWorld();
		float sx = ms.dir.n[2], sz = -ms.dir.n[0];   // prawo voba - strona przy dokladnie czolowym
		for (oCNpc* npc : ms.passNpcs) {
			if (!MvNpcBlocksPass(npc, hp))
				continue;
			zVEC3 np = npc->GetPositionWorld();
			zdcore::DeflectAroundCircle(hp.n[0], hp.n[2], d.n[0], d.n[2], np.n[0], np.n[2], MOVE_NPC_RADIUS, sx, sz);
		}
	}

	// Koniec ruchu nie w postaci: bohater wypychany na MOVE_NPC_END_RADIUS od srodkow postaci (MOVE_NPC_PUSH_SPEED, kroki
	// z kolizja ze scianami; zablokowany - w bok o 60 / 120 st.).
	static void MvSeparate(float dt) {
		if (ms.passNpcs.empty())
			return;
		zVEC3 hp = player->GetPositionWorld();
		float px = 0.0f, pz = 0.0f;
		bool any = false;
		for (oCNpc* npc : ms.passNpcs) {
			if (!MvNpcBlocksPass(npc, hp))
				continue;
			zVEC3 np = npc->GetPositionWorld();
			float ox, oz;
			if (zdcore::CircleSeparation(hp.n[0], hp.n[2], np.n[0], np.n[2], MOVE_NPC_END_RADIUS, ms.lastStepX, ms.lastStepZ, ox, oz)) {
				px += ox;
				pz += oz;
				any = true;
			}
		}
		float l = sqrtf(px * px + pz * pz);
		if (!any || l < 0.5f)
			return;
		float maxL = MOVE_NPC_PUSH_SPEED * dt;
		if (l > maxL) {
			px *= maxL / l;
			pz *= maxL / l;
			l = maxL;
		}
		static const float turns[] = { 0.0f, 1.05f, -1.05f, 2.1f, -2.1f };
		for (float a : turns) {
			float c = cosf(a), s = sinf(a);
			zVEC3 d(px * c + pz * s, 0.0f, -px * s + pz * c);
			if (MvStepHero(d) > 0.3f * l) {
				if (!ms.separateLogged)
					ZDebug("Ruch %s: koniec w postaci - wypychanie obok (%.0f st.)", ms.def->id.c_str(), a * 57.3f);
				ms.separateLogged = true;
				return;
			}
		}
	}

	// scale: mnoznik kroku (wyjscie ataku w biegu - tor wygasa, bieg gry przejmuje).
	static bool MoveAlong(float tPrev, float scale = 1.0f) {
		ZPROF_SCOPE("Moves.Step");   // vob po torze z kolizja (promienie) w trakcie ruchu
		float x, z;
		zdcore::SampleTraj(ms.def->traj, ms.def->trajFps, ms.t, x, z);
		float dx = (x - ms.trajX) * scale, dz = (z - ms.trajZ) * scale;
		ms.trajX = x;
		ms.trajZ = z;
		if (ms.carry > 1.0f && dz > 0.0f && !ms.target)
			dz *= zdcore::CarryScale(ms.carry, ms.t, ms.def->hitTime > 0.0f ? ms.def->hitTime : ms.carryTime);
		if (ms.target && FoeReady(ms.target)) {
			float dist = MvFlatDist(player->GetPositionWorld(), ms.target->GetPositionWorld());
			if (ms.def->hitTime > 0.0f && tPrev < ms.def->hitTime) {
				float pos, neg;
				zdcore::TrajForward(ms.def->traj, ms.def->trajFps, tPrev, ms.def->hitTime, pos, neg);
				if (dz > 0.0f)
					dz *= zdcore::LungeScale(dist - ms.desired, pos, neg, LUNGE_MAX);
			}
			else if (dz > 0.0f && dist - dz < ms.desired - 20.0f)
				dz = dist > ms.desired - 20.0f ? dist - (ms.desired - 20.0f) : 0.0f;
		}
		// obrot do wroga na koncu uniku / przewrotu (MvFaceStep): reszta toru w kierunku sprzed obrotu
		const zVEC3& pd = ms.facing ? ms.pathDir : ms.dir;
		zVEC3 right(pd.n[2], 0.0f, -pd.n[0]);
		zVEC3 d(right.n[0] * dx + pd.n[0] * dz, 0.0f, right.n[2] * dx + pd.n[2] * dz);
		if (ms.passChars)
			MvDeflectNpcs(d);
		float planned = MvFlatLen(d);
		if (planned > 0.5f) {
			ms.lastStepX = d.n[0] / planned;
			ms.lastStepZ = d.n[2] / planned;
		}
		float done = MvStepHero(d);
		if (ms.target)
			return false;   // kontra / atak z wypadem na cel: przeszkoda = stoi, cios i tak pada
		if (done < planned * 0.5f && planned > 0.5f) {
			// raz na ruch (dalej tylko licznik, podsumowanie przy koncu ruchu) - co klatke zasypywalo log
			if (!ms.obstacleLogged)
				ZDebug("Ruch %s: przeszkoda po %.2f s (krok %.0f z %.0f cm)", ms.def->id.c_str(), ms.t, done, planned);
			ms.obstacleLogged = true;
			ms.obstacleFrames++;
		}
		return zdcore::MoveBlockStep(ms.block, planned, done, ms.def->cancel - ms.t);
	}

	const char* MovesForce(const std::string& id) {
		const zdcore::MoveDef* def = nullptr;
		for (auto& d : moveDefs)
			if (d.id == zdcore::Upper(id))
				def = &d;
		if (!def)
			return "nieznany ruch (zdism moves)";
		if (ms.def || DirectorActive() || !player || player->IsDead() || !HeroIsHuman())
			return "teraz nie mozna";
		int wm = player->GetWeaponMode();
		if (wm != NPC_WEAPON_1HS && wm != NPC_WEAPON_2HS && wm != NPC_WEAPON_FIST)
			return "dobadz broni albo piesci";
		if (zdcore::IsJumpAttack(*def) || zdcore::IsRunAttack(*def)) {
			std::vector<const zdcore::MoveDef*> one = { def };
			oCNpc* foe = LungeTarget(one, zdcore::IsRunAttack(*def) ? RUN_TARGET_RANGE : JUMP_TARGET_RANGE);
			return StartLungeMove(def, wm, foe) ? "start (atak z wypadem)" : "nie wystartowal (log)";
		}
		if (def->hitTime >= 0.0f) {
			oCNpc* foe = CounterTarget(false);
			if (foe)
				return StartCounter(def, foe, wm) ? "start (kontra)" : "nie wystartowal (log)";
		}
		return StartMove(def, wm) ? "start" : "nie wystartowal (log)";
	}

	// ---------- Unik w ostatniej chwili (W3 "perfect dodge", 17.10) ----------
	// Unik / przewrot zaczety, gdy wrog w poblizu (walczy z bohaterem albo jest celem sledzenia) jest w zamachu
	// (IsInPreHit) najwyzej PerfectDodgeWindow s przed trafieniem (klatka ciosu hitOptFrame biezacego ciecia, tempo
	// animacji x tempo modelu): ten wrog na PerfectDodgeLostTime s "traci namiar" - jego obrot (po AI, co klatke)
	// najwyzej PerfectDodgeTurnRate st./s i nie zaczyna nowego ataku (haki EV_Attack*; trwajacy cios gra dalej), a pierwszy
	// cios bohatera w PerfectDodgeHeroWindow s po oknie wyjscia gra w tempie x PerfectDodgeHeroSpeed (tempo modelu, tylko
	// gdy 1 i przywracane, gdy nikt go nie zmienil). Sygnal: blysk z iskrami przy wrogu i dzwiek PerfectDodgeSound.
	struct LostTrack {
		oCNpc* npc = nullptr;            // AddRef
		float left = 0.0f;
		float yaw = 0.0f;                // kierunek po ograniczeniu z poprzedniej klatki
		std::vector<oCMsgAttack*> allowed;   // komunikaty ataku z chwili uniku (trwajacy cios) - nie polykane
		int swallowed = 0;
		float clamped = 0.0f;            // laczny obrot zatrzymany ograniczeniem (rad, diagnostyka)
	};
	static std::vector<LostTrack> lostTrack;
	static zdcore::HeroBoost heroBoost;
	static int heroBoostCombo = -1;      // segment kombo, w ktorym przyspieszenie ruszylo (kolejny = koniec)
	static int perfectCount = 0;

	static float MvAniFps(zCModel* m, zCModelAni* a) {
		float fps = a && a->fpsRate > 0.0f ? a->fpsRate : 25.0f;
		return m && m->timeScale > 0.01f ? fps * m->timeScale : fps;
	}

	static void PerfectDodgeCheck() {
		if (!zcfg.perfectDodge || !ms.def)
			return;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return;
		zVEC3 hp = player->GetPositionWorld();
		bool any = false;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!FoeReady(npc) || npc->IsAPlayer() || (npc->enemy != player && npc != ms.track))
				continue;
			zVEC3 p = npc->GetPositionWorld();
			float dist = MvFlatDist(hp, p);
			if (dist > zcfg.perfectDodgeRange || fabsf(p.n[1] - hp.n[1]) > MOVE_NPC_LEVEL)
				continue;
			oCAniCtrl_Human* ac = npc->GetAnictrl();
			zCModel* m = npc->GetModel();
			if (!ac || !m || ac->hitAniID < 0 || ac->comboNr < 0 || ac->comboNr >= ANI_MAX_HIT)
				continue;
			zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
			if (!a || !a->protoAni || a->isFadingOut || !ac->IsInPreHit())
				continue;
			float tth = zdcore::EnemyTimeToHit((float)ac->comboInfo[ac->comboNr].hitOptFrame, a->actFrame, MvAniFps(m, a->protoAni));
			if (!zdcore::PerfectDodgeTiming(tth, zcfg.perfectDodgeWindow)) {
				ZDebug("Ruch %s: %s w zamachu (%s), cios za %.2f s - za wczesnie na unik w ostatniej chwili (okno %.2f s)",
					ms.def->id.c_str(), npc->GetName(0).ToChar(), a->protoAni->aniName.ToChar(), tth, zcfg.perfectDodgeWindow);
				continue;
			}
			LostTrack e;
			for (auto& o : lostTrack)
				if (o.npc == npc)
					e.npc = npc;   // juz traci namiar - tylko przedluzenie
			if (e.npc) {
				for (auto& o : lostTrack)
					if (o.npc == npc && o.left < zcfg.perfectDodgeLostTime)
						o.left = zcfg.perfectDodgeLostTime;
			}
			else {
				e.npc = npc;
				npc->AddRef();
				e.left = zcfg.perfectDodgeLostTime;
				zVEC3 at = npc->GetAtVectorWorld();
				e.yaw = atan2f(at.n[0], at.n[2]);
				if (zCEventManager* em = npc->GetEM(FALSE))
					for (int i = 0; i < em->GetNumMessages(); i++)
						if (oCMsgAttack* am = zDYNAMIC_CAST<oCMsgAttack>(em->GetEventMessage(i)))
							e.allowed.push_back(am);
				lostTrack.push_back(e);
			}
			any = true;
			perfectCount++;
			ZDebug("Unik w ostatniej chwili #%d: %s %s, cios %s za %.2f s (okno %.2f), odleglosc %.0f - traci namiar na %.2f s "
				"(obrot do %.0f st./s, bez nowego ataku)", perfectCount, ms.def->id.c_str(), npc->GetName(0).ToChar(),
				a->protoAni->aniName.ToChar(), tth, zcfg.perfectDodgeWindow, dist, zcfg.perfectDodgeLostTime, zcfg.perfectDodgeTurnRate);
			if (zcfg.perfectDodgeFx) {
				// blysk miedzy wrogiem a bohaterem na wysokosci broni
				float dx = hp.n[0] - p.n[0], dz = hp.n[2] - p.n[2], l = sqrtf(dx * dx + dz * dz);
				float k = l > 1.0f ? (l < 60.0f ? l * 0.5f : 40.0f) / l : 0.0f;
				ParryFxFlash(zVEC3(p.n[0] + dx * k, p.n[1] + 40.0f, p.n[2] + dz * k));
			}
		}
		if (!any)
			return;
		ms.perfect = true;
		float exitAt = ms.exitFrom < 1e8f ? ms.exitFrom : ms.def->cancel;
		zdcore::HeroBoostArm(heroBoost, exitAt - ms.t + zcfg.perfectDodgeHeroWindow);
		heroBoostCombo = -1;
		if (zsound && !zcfg.perfectDodgeSound.empty())
			zsound->PlaySound3D(zSTRING(zcfg.perfectDodgeSound.c_str()), player, zSND_SLOT_NONE, nullptr);
	}

	static bool PerfectDodgeSwallow(oCNpc* npc, oCMsgAttack* msg, const char* what) {
		if (lostTrack.empty() || !msg || !zcfg.enabled)
			return false;
		for (auto& e : lostTrack) {
			if (e.npc != npc)
				continue;
			if (msg->IsInUse() || std::find(e.allowed.begin(), e.allowed.end(), msg) != e.allowed.end())
				return false;   // cios rozpoczety przed unikiem
			if (!e.swallowed)
				ZDebug("Unik w ostatniej chwili: %s chcial zaatakowac (%s) %.2f s przed odzyskaniem namiaru - wstrzymany",
					npc->GetName(0).ToChar(), what, e.left);
			e.swallowed++;
			return true;
		}
		return false;
	}

	static void PerfectDodgeUpdate(float dt) {
		float rate = zcfg.perfectDodgeTurnRate * (3.14159265f / 180.0f);
		for (size_t i = 0; i < lostTrack.size();) {
			LostTrack& e = lostTrack[i];
			e.left -= dt;
			if (e.left <= 0.0f || !FoeReady(e.npc) || !zcfg.enabled) {
				ZDebug("Unik w ostatniej chwili: %s odzyskal namiar (wstrzymane ataki: %d, zatrzymany obrot %.0f st.)",
					e.npc->GetName(0).ToChar(), e.swallowed, e.clamped * 57.3f);
				e.npc->Release();
				lostTrack.erase(lostTrack.begin() + i);
				continue;
			}
			zVEC3 at = e.npc->GetAtVectorWorld();
			if (at.n[0] * at.n[0] + at.n[2] * at.n[2] > 1e-4f) {
				float yaw = atan2f(at.n[0], at.n[2]);
				float ny = zdcore::LostTrackYaw(e.yaw, yaw, rate, dt);
				float cut = fabsf(zdcore::AngleDiff(ny, yaw));
				if (cut > 1e-4f) {
					e.npc->SetHeadingAtWorld(zVEC3(sinf(ny), 0.0f, cosf(ny)));
					e.clamped += cut;
				}
				e.yaw = ny;
			}
			i++;
		}
		// pierwszy cios bohatera po uniku w ostatniej chwili szybszy (tempo modelu na czas tego ciecia)
		if (!heroBoost.armed && !heroBoost.active)
			return;
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!m || !ac) {
			heroBoost = zdcore::HeroBoost();
			return;
		}
		// model zamrozony przez zTimeRewind (timeScale 0 na czas cofania): stan czeka. Koniec ciosu przy 0 nie zdejmowal
		// naszego x boost (tempo != nasze), a zTimeRewind oddawal potem tempo ze startu = przyspieszone na stale.
		// Po cofnieciu tempo wraca do naszego - zdejmujemy je sami w pierwszej klatce bez ciosu.
		if (zdcore::HeroModelFrozen(m->timeScale))
			return;
		zCModelAniActive* a = ac->hitAniID >= 0 ? m->GetActiveAni(ac->hitAniID) : nullptr;
		bool attacking = a && !a->isFadingOut && (heroBoostCombo < 0 || ac->comboNr == heroBoostCombo);
		bool was = heroBoost.active;
		float s = zdcore::HeroBoostStep(heroBoost, attacking, dt, m->timeScale, zcfg.perfectDodgeHeroSpeed);
		if (fabsf(s - m->timeScale) > 1e-4f)
			m->timeScale = s;
		if (!was && heroBoost.active) {
			heroBoostCombo = ac->comboNr;
			ZDebug("Unik w ostatniej chwili: pierwszy cios %s w tempie x%.2f", a && a->protoAni ? a->protoAni->aniName.ToChar() : "?", s);
		}
		else if (was && !heroBoost.active) {
			ZDebug("Unik w ostatniej chwili: koniec szybszego ciosu (tempo modelu %.2f)", m->timeScale);
			heroBoostCombo = -1;
		}
	}

	static void PerfectDodgeReset() {
		for (auto& e : lostTrack)
			if (e.npc)
				e.npc->Release();
		lostTrack.clear();
		if (heroBoost.active && player && player->homeWorld)
			if (zCModel* m = player->GetModel())
				if (fabsf(m->timeScale - heroBoost.applied) < 1e-3f)
					m->timeScale = heroBoost.base;
		heroBoost = zdcore::HeroBoost();
		heroBoostCombo = -1;
	}

	void MovesFinishNow() {
		PerfectDodgeReset();
		if (ms.def)
			FinishMove(true);
		CamAssistEnd();
		StopRepelPush();
		rollTap = zdcore::RollTap();
		jumpInjectFrames = 0;
		jumpInjectToggle = false;
		pendingKind = 0;
		tapLateClock = -1.0f;
	}

	// Bohater w stanie, w ktorym gra nie przerwala ruchu reakcja (trafienie, upadek, woda, interakcja).
	static bool MvBodyStateKeepsMove() {
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		return bs == BS_STAND || bs == BS_WALK || bs == BS_SNEAK || bs == BS_RUN || bs == BS_SPRINT || bs == BS_PARADE ||
			bs == BS_HIT;   // BS_HIT: CreateHit naszego ciosu
	}

	// Ochrona animacji ruchu. Zgaszona przed koncem przez postawe silnika (oCAniCtrl_Human::_Stand przy movlocku) albo
	// zatrzymanie wszystkich animacji (zalozenie / zdjecie cudzej nakladki, np. koniec sprintu zEBA) - wznowienie
	// w tym samym miejscu (najwyzej MOVE_MAX_RESUMES razy). Trafienie, zatoczenie, upadek, smierc - koniec ruchu jak
	// dotad (reakcja gry wygrywa). false = zakonczyc ruch (przerwany).
	static bool MoveGuard(zCModel* hm) {
		int hpNow = player->attribute[NPC_ATR_HITPOINTS];
		if (hpNow < ms.heroHp)
			ms.heroHit = true;
		ms.heroHp = hpNow;
		zCModelAni* ani = MvAni(hm);
		zCModelAniActive* act = ani ? hm->GetActiveAni(ani) : nullptr;
		if (act && !act->isFadingOut) {
			ms.progress = act->GetProgressPercent();
			return true;
		}
		if (ani && !act && ms.t <= 0.1f)
			return true;   // pierwsze klatki - animacja mogla jeszcze nie wejsc
		if (!act && ms.progress >= 0.85f) {
			// dobiegla konca przed BeginEnding (skok czasu klatki) - silnik juz wchodzi w postawe
			ZDebug("Ruch %s: animacja dobiegla konca po %.2f s (postep %.0f%%) - koniec ruchu", ms.def->id.c_str(), ms.t,
				ms.progress * 100.0f);
			return false;
		}
		std::string desc;
		std::vector<std::string> layerOne = MvLayerOneAnis(hm, desc);
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		if (!MvBodyStateKeepsMove() || !zdcore::MoveResumable(layerOne, ms.heroHit, ani != nullptr, ms.resumes, MOVE_MAX_RESUMES)) {
			ZDebug("Ruch %s: animacje przerwala gra po %.2f s (warstwa 1:%s; stan %d%s%s, wznowien %d) - koniec ruchu",
				ms.def->id.c_str(), ms.t, desc.c_str(), bs, ms.heroHit ? ", trafienie" : "", ani ? "" : ", brak nakladki ruchu",
				ms.resumes);
			return false;
		}
		ms.resumes++;
		if (act)
			hm->StopAni(ani);   // wygasa - start od nowa w tym samym miejscu
		hm->StartAni(ani, 0);
		if (zCModelAniActive* a = hm->GetActiveAni(ani))
			a->SetProgressPercent(ms.progress);
		int stopped = MvStopStandTriggers(hm);
		ZDebug("Ruch %s: animacje zgasila postawa silnika / inna wtyczka po %.2f s (warstwa 1:%s; stan %d) - wznowiona w %.0f%% (%d/%d, bieg zatrzymany: %d)",
			ms.def->id.c_str(), ms.t, desc.c_str(), bs, ms.progress * 100.0f, ms.resumes, MOVE_MAX_RESUMES, stopped);
		return true;
	}

	// Przerwanie skoku gry (drugi Alt / atak w oknie po skoku od razu). Silnik: oCAIHuman::PC_SpecialMove (0x699DF0)
	// ustawia zadanie skoku (oCAniCtrl_Human::do_jump +0x170), skok w biegu rusza dopiero w fazie kroku
	// (PC_JumpForward 0x6B1E00 / GetFootPos 0x6B1BC0) - T_RUNL_2_JUMP: ruch korzenia, fizyka wylaczona, stan ciala BS_JUMP;
	// po nim S_JUMP z fizyka (CheckJump 0x6B4FE0: StartPhysicsWithVel, ladowanie LandAndStartAni). Przed startem:
	// do_jump = 0 (ruch z ziemi). W odbiciu: do_jump = 0, fizyka wylaczona (predkosc zero), stan ciala BS_RUN, animacja
	// skoku wygasa pod ruchem (StartMove: blend z biezacej pozy), vob schodzi na ziemie w ruchu (MOVE_DROP_TIME).
	// Lot S_JUMP / fizyka / wysoko - za pozno (CheckJump zakonczylby nasz ruch ladowaniem silnika).
	static zdcore::JumpCancel MvJumpCancel(float& height, std::string& anis) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* hm = player ? player->GetModel() : nullptr;
		height = ac ? ac->aboveFloor : 0.0f;
		if (!ac || !hm)
			return zdcore::JUMPCANCEL_LATE;
		zdcore::JumpCancelInput in;
		for (int i = 0; i < hm->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = hm->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1 || a->isFadingOut)
				continue;
			std::string n = p->aniName.ToChar();
			anis += (anis.empty() ? "" : "+") + n;
			in.takeoff = in.takeoff || zdcore::IsJumpTakeoffAni(n);
			in.flight = in.flight || zdcore::IsJumpFlightAni(n);
		}
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		in.physics = player->physicsEnabled != 0;
		in.started = in.takeoff || in.flight || in.physics || bs == BS_JUMP || ac->state == zCAIPlayer::zMV_STATE_FLY;
		in.height = height > 0.0f ? height : 0.0f;
		zdcore::JumpCancel c = zdcore::JumpCancelDecide(in, JUMP_CANCEL);
		if (c == zdcore::JUMPCANCEL_LATE)
			return c;
		ac->do_jump = 0;
		mvJumpSpeed = heroSpeed;
		if (c == zdcore::JUMPCANCEL_TAKEOFF) {
			if (in.physics) {
				if (zCRigidBody* rb = player->GetRigidBody()) {
					rb->v = zVEC3(0.0f, 0.0f, 0.0f);
					rb->PLinMom = zVEC3(0.0f, 0.0f, 0.0f);
				}
				player->SetPhysicsEnabled(FALSE);
			}
			player->SetBodyState(BS_RUN);
			mvJumpDrop = true;
		}
		return c;
	}

	static bool KeyEdge(int dik, bool& prev) {
		bool down = dik > 0 && zinput && zinput->KeyPressed(dik) != 0;
		bool edge = down && !prev;
		prev = down;
		return edge;
	}

	void MovesUpdate(float dt) {
		mvUpdateFrame = zdFrame;
		PerfectDodgeUpdate(dt);   // wrogowie po uniku w ostatniej chwili (obrot po AI), szybszy pierwszy cios bohatera
		MvExitTraceStep(dt);      // Debug: animacje po wyjsciu atakiem z uniku / przewrotu
		// predkosc bohatera (rozped ataku w biegu ze sprintu); skoki pozycji (teleport, wczytanie) pomijane
		if (player) {
			zVEC3 p = player->GetPositionWorld();
			if (mvSpeedValid && dt > 0.0f) {
				float v = MvFlatDist(p, mvLastPos) / dt;
				if (v < 2500.0f)
					heroSpeed = heroSpeed * 0.5f + v * 0.5f;
			}
			mvLastPos = p;
			mvSpeedValid = true;
			// wysokosc voba w postawie z bronia (przerwany skok: do niej schodzi przewrot / atak z wyskoku) - co 8 klatek
			oCAniCtrl_Human* sac = player->GetAnictrl();
			int sbs = player->GetBodyState() & BS_ONLY_STATE;
			if ((zdFrame & 7) == 0 && !ms.def && sac && sac->state == zCAIPlayer::zMV_STATE_STAND && !player->physicsEnabled &&
				MvFightKeysOurs() && (sbs == BS_STAND || sbs == BS_WALK || sbs == BS_RUN || sbs == BS_SPRINT)) {
				float g;
				if (MoveGroundAt(p, g) && p.n[1] - g > 40.0f && p.n[1] - g < 200.0f)
					mvStandYOff = p.n[1] - g;
			}
		}
		else
			mvSpeedValid = false;
		if (jumpInjectFrames > 0 && --jumpInjectFrames == 0)
			jumpInjectToggle = false;   // gra nie zapytala o GetToggled - bez skoku pozniej
		// klawisz skoku przejety przez przeskok / wspiecie w biegu (Traversal.cpp; rozstrzygniecie raz na wcisniecie,
		// tu albo w haku wejscia) - bez okna przewrotu / ataku z wyskoku i bez wstrzyknietego skoku gry
		bool travJump = TraversalTakesJump();
		if (travJump && (rollTap.waiting || pendingKind == 2 || pendingKind == 3)) {
			// wstrzykniety skok gry zostaje - moze pochodzic z przerwanego bufora przeskoku (Traversal.cpp)
			ZDebug("Ruchy: klawisz skoku przejal przeskok / wspiecie - okno Alt zamkniete");
			rollTap = zdcore::RollTap();
			pendingKind = 0;
		}
		bool dodgeDown = MvDodgeKeyDown();
		bool dodge = dodgeDown && !prevDodgeKey;
		prevDodgeKey = dodgeDown;
		if (dodge)
			pendingDodgeDir = -1;
		bool rollEdge = KeyEdge(zcfg.rollKey, prevRollKey);
		bool rollEdgeRaw = rollEdge;   // diagnostyka sciezki Alt
		if (travJump && rollSharesJump)
			rollEdge = false;
		bool roll = rollEdge;
		if (moveCooldown > 0.0f)
			moveCooldown -= dt;
		bool l = false, r = false, f = false, b = false;
		if (zinput)
			ReadDirKeys(l, r, f, b);
		if (roll) {
			int d = zdcore::RollDirFromKeys(rollSharesJump, l, r, f, b);
			if (d < 0)
				roll = false;   // sam Alt (skok gry) albo Alt+W (czeka na drugie Alt)
			else
				pendingRollDir = d;
		}
		// dwuklik kierunku (DoubleTapDodge: 1 z dobyta bronia, 2 z bronia i Shiftem) - unik albo przewrot w te strone;
		// bok to strafe albo obrot (Q/A, E/D), jak przy uniku z klawisza
		if (zinput) {
			bool dl = false, dr = false;
			zdcore::DodgeSideKeys(MvRawState(GAME_STRAFELEFT), MvRawState(GAME_LEFT), MvRawState(GAME_STRAFERIGHT),
				MvRawState(GAME_RIGHT), dl, dr);
			bool dirDown[4] = { MvRawState(GAME_DOWN), MvRawState(GAME_UP), dl, dr };   // kolejnosc zdcore::MoveDir
			bool shift = zinput->KeyPressed(KEY_LSHIFT) || zinput->KeyPressed(KEY_RSHIFT);
			bool armed = zcfg.doubleTap > 0 && MvFightKeysOurs() && (zcfg.doubleTap == 1 || shift);
			int td = zdcore::DoubleTapStep(dblTap, dirDown, dt, zcfg.doubleTapWindow, armed);
			if (td >= 0) {
				static const char* const dirName[] = { "tyl", "przod", "lewo", "prawo" };
				ZDebug("Ruchy: dwuklik %s -> %s", dirName[td], zcfg.doubleTapMove ? "przewrot" : "unik");
				if (zcfg.doubleTapMove) {
					roll = true;
					pendingRollDir = td;
				}
				else {
					dodge = true;
					pendingDodgeDir = td;
				}
			}
		}
		// klawisz skoku w walce: Alt+W, Alt = przewrot w przod; Alt (sam albo z W) i atak w oknie = atak z wyskoku;
		// bez nich w oknie = skok gry
		bool jumpEdge = rollSharesJump ? rollEdge : KeyEdge(jumpKey, prevJumpKey);
		bool jumpEdgeRaw = rollSharesJump ? rollEdgeRaw : jumpEdge;
		if (travJump)
			jumpEdge = false;
		bool attackDown = MvAttackDown();
		bool attackEdge = attackDown && !prevAttackKey;
		prevAttackKey = attackDown;
		float attackAgeBefore = attackEdgeAge;
		attackEdgeAge = attackEdge ? 0.0f : (attackEdgeAge < 10.0f ? attackEdgeAge + dt : 10.0f);
		// okno tylko, gdy ruch moze zaraz wystartowac (albo trwa nasz); w oknie skok gry juz leci (stan skoku) - dalej
		// uzbrojone, decyzja o przerwaniu w MvJumpCancel. Swieze wcisniecie: stan z haka klawisza (przed startem skoku
		// gry w tej klatce - runda 10)
		bool canWaitNow = MvTapCanWait();
		bool canWait = zdcore::JumpPressCanWait(jumpPressLatch, canWaitNow, jumpEdge) || rollTap.waiting;
		if (canWait && !canWaitNow && jumpEdge && !rollTap.waiting && zcfg.debug)
			ZDebug("Ruchy: Alt - okno wg stanu z chwili wcisniecia (teraz stan %d - skok gry juz ruszyl)",
				player ? player->GetBodyState() & BS_ONLY_STATE : -1);
		zdcore::JumpTapInput tap;
		tap.rollArmed = MvRollTapArmed() && canWait;
		tap.attackArmed = MvJumpAttackArmed() && canWait;
		tap.press = jumpEdge && (tap.rollArmed || tap.attackArmed);
		tap.forwardOnly = MvForwardOnly(l, r, f, b);
		tap.noDir = !f && !b && l == r;
		tap.attack = attackEdge;
		tap.down = rollSharesJump ? prevRollKey : prevJumpKey;
		// diagnostyka: drugie Alt po oknie, ktore juz oddalo skok grze (za wolne podwojne stukniecie)
		if (tapLateClock >= 0.0f) {
			tapLateClock += dt;
			if (jumpEdge) {
				ZDebug("Ruchy: drugie Alt %.2f s po pierwszym - po oknie %.2f s (skok gry juz poszedl)", tapLateClock, tapLateWindow);
				tapLateClock = -1.0f;
			}
			else if (tapLateClock > TAP_LATE_LOG)
				tapLateClock = -1.0f;
		}
		if (!tap.rollArmed && !tap.attackArmed && rollTap.waiting) {
			// wyjscie z walki, trafienie, upadek w trakcie okna - skok przepada
			ZDebug("Ruchy: okno skoku przerwane po %.2f s (stan %d) - bez skoku", rollTap.age,
				player ? player->GetBodyState() & BS_ONLY_STATE : -1);
			rollTap = zdcore::RollTap();
		}
		zdcore::RollTap before = rollTap;
		zdcore::JumpNowResult tapRes = zdcore::JumpTapNowStep(rollTap, tap, dt, JumpTapWindowsNow());
		if (tapRes == zdcore::JUMPNOW_JUMP)
			tapAttackLead = attackAgeBefore + dt < TAP_ATTACK_LEAD_LOG ? attackAgeBefore + dt : -1.0f;
		float tapAge = before.waiting ? before.age + dt : 0.0f;   // czas od pierwszego Alt do tej klatki
		bool jumpAttack = false;
		switch (tapRes) {
		case zdcore::JUMPNOW_JUMP:
			// skok gry od razu (klawisz nie polkniety) - okno na drugie Alt / atak
			ZDebug("Ruchy: skok od razu (%s, okno %s %.2f s)", rollTap.forward ? "Alt+W" : "Alt",
				rollTap.forward ? "przewrotu / ataku" : "ataku", rollTap.left);
			break;
		case zdcore::JUMPNOW_ROLL:
		case zdcore::JUMPNOW_ATTACK: {
			bool isRoll = tapRes == zdcore::JUMPNOW_ROLL;
			const char* what = isRoll ? "drugi Alt" : (before.waiting ? "atak" : "Alt + atak naraz");
			float h = 0.0f;
			std::string anis;
			zdcore::JumpCancel c = MvJumpCancel(h, anis);
			if (c == zdcore::JUMPCANCEL_LATE) {
				ZDebug("Ruchy: %s po %.2f s, wysokosc %.0f cm (%s) - za pozno, skok gry trwa", what, tapAge, h, anis.c_str());
				break;
			}
			if (isRoll) {
				roll = true;
				pendingRollDir = zdcore::MOVE_FORWARD;
			}
			else
				jumpAttack = true;
			ZDebug("Ruchy: %s po %.2f s, wysokosc %.0f cm (%s) -> %s (%s)", what, tapAge, h, anis.c_str(),
				isRoll ? "przewrot w przod" : "atak z wyskoku", c == zdcore::JUMPCANCEL_BEFORE ? "skok jeszcze nie ruszyl" :
				"skok przerwany");
			break;
		}
		case zdcore::JUMPNOW_CLOSED: {
			char held[48], lead[64];
			if (before.held < 0.0f)
				sprintf_s(held, "trzymany");
			else
				sprintf_s(held, "puszczony po %.2f s", before.held);
			lead[0] = '\0';
			if (tapAttackLead >= 0.0f)
				sprintf_s(lead, ", atak %.2f s PRZED Alt (liczy sie atak po Alt)", tapAttackLead);
			ZDebug("Ruchy: bez drugiego Alt / ataku w %.2f s - skok gry trwa (okno %s, Alt %s%s)", tapAge,
				before.forward ? "z W" : "bez W", held, lead);
			tapLateClock = tapAge;
			tapLateWindow = tapAge;
			break;
		}
		case zdcore::JUMPNOW_NONE:
			if (before.waiting && tap.press)
				ZDebug("Ruchy: drugi Alt po %.2f s bez W - pominiety (skok gry trwa)", tapAge);
			break;
		default:
			break;
		}
		// diagnostyka (Debug=1): kazde Alt w walce - ktora sciezka wygrala (gdy nie ma wpisu wyzej); poza walka W + 2x Alt
		bool altEdge = rollSharesJump ? rollEdgeRaw : jumpEdgeRaw;
		altOutAge = altOutAge < 10.0f ? altOutAge + dt : 10.0f;
		if (altEdge && player && zcfg.debug) {
			int wmNow = player->GetWeaponMode();
			int bsNow = player->GetBodyState() & BS_ONLY_STATE;
			if (!MvFightKeysOurs()) {
				if (f && altOutAge < JumpTapWindowsNow().doubleTap)
					ZDebug("Ruchy: W + 2x Alt poza walka (bron %d) - przewrot w przod tylko w walce (bron biala / piesci); tu skok gry"
						" / przeskok", wmNow);
				altOutAge = 0.0f;
			}
			else if (travJump)
				ZDebug("Ruchy: Alt w walce (bron %d, stan %d) - przejal przeskok / wspiecie", wmNow, bsNow);
			else if (roll && !before.waiting && tapRes == zdcore::JUMPNOW_NONE)
				ZDebug("Ruchy: Alt w walce (bron %d) - przewrot %d od razu (bok / tyl)", wmNow, pendingRollDir);
			else if (!tap.press && tapRes == zdcore::JUMPNOW_NONE && !before.waiting) {
				int wmc = 0;
				const char* why = !MvRollTapArmed() && !MvJumpAttackArmed() ? "przewrot w przod / atak z wyskoku niedostepny" :
					!MvTapCanWait() ? (TraversalActive() ? "trwa przeskok / wspiecie" : player->IsMovLock() ? "blokada ruchu" :
					ms.def ? "trwa ruch" : !MoveCanStart(wmc) ? "ruch nie moze ruszyc (stan ciala - skok / upadek / atak)" : "?") :
					"?";
				ZDebug("Ruchy: Alt w walce (bron %d, stan %d, W %d) - skok gry bez okna (%s)", wmNow, bsNow, f ? 1 : 0, why);
			}
		}
		if (dodge || roll || jumpAttack) {
			pendingKind = jumpAttack ? 3 : roll ? 2 : 1;
			pendingTime = MOVE_BUFFER;
		}
		else if (pendingTime > 0.0f && (pendingTime -= dt) <= 0.0f)
			pendingKind = 0;
		RepelPushUpdate(dt);
		oCAniCtrl_Human* hac = player ? player->GetAnictrl() : nullptr;
		bool paradeNow = hac && hac->IsParadeRunning() != nullptr;
		bool paradeEdge = paradeNow && !prevParade;
		prevParade = paradeNow;

		// Kontra jak w W3: blok w trakcie zamachu przeciwnika (z broni 1H/2H) - odbicie i riposta zamiast samej parady.
		// Odskok w tyl (T_xPARADEJUMPB) tylko z CounterOnBackstep; z CounterWindow blok najwyzej tyle s przed trafieniem.
		if (!ms.def && paradeEdge && zcfg.counters && zcfg.movesEnabled && moveCooldown <= 0.0f) {
			int wm = 0;
			CounterEarly early;
			oCNpc* foe = MoveCanStart(wm) && wm != NPC_WEAPON_FIST ? CounterTarget(true, zcfg.counterWindow, &early) : nullptr;
			std::string pAni, pOvl;
			bool pFading = false;
			if ((foe || early.npc) && !zcfg.counterOnBackstep && MvParadeAni(hac, pAni, pOvl, pFading) &&
				zdcore::IsBackstepParade(pAni, pOvl, pFading)) {
				ZDebug("Kontra: odskok (%s, %s) - bez kontry (CounterOnBackstep=0)", pAni.c_str(), pOvl.c_str());
				foe = nullptr;
			}
			else if (!foe && early.npc) {
				if (early.tth >= 0.0f)
					ZDebug("Kontra: za wczesnie na kontre: %s (%s) - cios za %.2f s, okno %.2f s - zwykla parada",
						early.npc->GetName(0).ToChar(), early.ani.c_str(), early.tth, zcfg.counterWindow);
				else
					ZDebug("Kontra: %s w zamachu bez klatki ciosu (%s) - okno %.2f s nie do sprawdzenia, zwykla parada",
						early.npc->GetName(0).ToChar(), early.ani.empty() ? "brak animacji ciosu" : early.ani.c_str(), zcfg.counterWindow);
			}
			if (foe)
				if (const zdcore::MoveDef* def = PickCounter(foe, l, r))
					if (StartCounter(def, foe, wm)) {
						pendingKind = 0;
						return;
					}
		}

		if (!ms.def) {
			if (!zcfg.movesEnabled || moveDefs.empty() || moveCooldown > 0.0f || !pendingKind)
				return;
			int wm = 0;
			bool fromHold = false;
			if (!MoveCanStart(wm)) {
				if (!((pendingKind == 1 || pendingKind == 2) && MvAttackHoldCanStart(wm)))
					return;   // zostaje w buforze do konca MOVE_BUFFER
				fromHold = true;
			}
			if (fromHold)
				if (zCModel* hm = player->GetModel())
					for (int i = hm->numActiveAnis - 1; i >= 0; i--) {   // postawa ataku (S_xHATTACK) nie miesza sie z ruchem
						zCModelAniActive* a = i < zMDL_MAX_ANIS_PARALLEL ? hm->aniChannels[i] : nullptr;
						if (a && a->protoAni && zdcore::AttackHoldIdle({ a->protoAni->aniName.ToChar() }))
							hm->FadeOutAni(a);
					}
			int kind = pendingKind;
			pendingKind = 0;
			pendingTime = 0.0f;
			if (kind == 3 || kind == 4) {   // atak z wyskoku / w biegu (EV_ATTACKRUN przejety w MvEV_AttackRun)
				if ((wm == NPC_WEAPON_1HS || wm == NPC_WEAPON_2HS) && !StartLungeAttack(kind == 4, wm))
					ZDebug("Ruchy: %s nie wystartowal", kind == 4 ? "atak w biegu" : "atak z wyskoku");
				return;
			}
			if (kind == 1 && zinput)   // unik: obrot A/D tez jest bokiem (takze z W), jak strafe Q/E
				zdcore::DodgeSideKeys(MvRawState(GAME_STRAFELEFT), MvRawState(GAME_LEFT), MvRawState(GAME_STRAFERIGHT),
					MvRawState(GAME_RIGHT), l, r);
			zdcore::MoveDir d = kind == 2 ? (zdcore::MoveDir)pendingRollDir :
				pendingDodgeDir >= 0 ? (zdcore::MoveDir)pendingDodgeDir : zdcore::MoveDirFromKeys(l, r, f, b);
			pendingDodgeDir = -1;
			const zdcore::MoveDef* def = zdcore::FindMove(moveDefs, kind == 2 ? "ROLL" : "DODGE", d);
			float jumpSpeed = mvJumpSpeed;
			mvJumpSpeed = 0.0f;
			if (def && StartMove(def, wm) && kind == 2 && jumpSpeed > 0.0f) {
				// przewrot z przerwanego skoku: ped skoku -> rozped przewrotu (wygasa do JUMP_ROLL_CARRY_TIME)
				float clip = zdcore::TrajSpeedAt(def->traj, def->trajFps, 0.0f, 0.12f);
				ms.carry = zdcore::EntrySpeedRatio(jumpSpeed, clip, RUN_CARRY_MAX);
				ms.carryTime = JUMP_ROLL_CARRY_TIME;
				ZDebug("Ruch %s z przerwanego skoku: predkosc %.0f cm/s (tor %.0f) - rozped x%.2f do %.2f s", def->id.c_str(),
					jumpSpeed, clip, ms.carry, JUMP_ROLL_CARRY_TIME);
			}
			else if (!def && kind == 2)
				ZDebug("Ruchy: brak ruchu ROLL kierunek %d w moves_w3.ini", (int)d);
			return;
		}

		if (!player || player->IsDead() || !player->GetModel() || !ogame || !ogame->GetGameWorld()) {
			MovesFinishNow();
			return;
		}
		float tPrev = ms.t;
		ms.t += dt;
		MvParadeCheck(dt);   // blok w koncowce: parada zamiast reszty ruchu (ustawia faze konczenia)
		// unik / przewrot: atak / kolejny ruch wcisniety w trakcie - w oknie wyjscia przerywa ruch (ustawia faze konczenia)
		if (zdcore::IsEvadeMove(*ms.def))
			MvExitCheck(dt, roll ? 2 : dodge ? 1 : 0, pendingRollDir);
		if (ms.ending >= 0.0f) {   // postawa / parada / bieg wchodzi (blend silnika) - potem zdjecie nakladki i sterowanie
			if (zdcore::IsEvadeMove(*ms.def) && ms.endKind != MVEND_RUN)
				MvFaceStep(dt);   // obrot do wroga dokonczony w przejsciu
			if (ms.endKind == MVEND_ATTACK) {
				// atak gry z okna wyjscia: reszta toru wygasa w przejsciu (bez stopu w miejscu), ruch ciosu doklada silnik
				float g;
				if (MoveGroundAt(player->GetPositionWorld(), g))
					ms.ground = g;
				MoveAlong(tPrev, ms.ending / (EXIT_ATTACK_BLEND + 0.04f));
			}
			else if (ms.endKind == MVEND_RUN) {
				// bieg gry przejmuje (blokada zdjeta - kierunek z voba, mysz obraca jak w biegu), tor ataku wygasa
				zVEC3 at = player->GetAtVectorWorld();
				float l = MvFlatLen(at);
				if (l > 0.01f)
					ms.dir = zVEC3(at.n[0] / l, 0.0f, at.n[2] / l);
				float g;
				if (MoveGroundAt(player->GetPositionWorld(), g))
					ms.ground = g;   // silnik tez przesuwa bohatera (bieg) - ziemia od biezacej pozycji
				MoveAlong(tPrev, ms.ending / RUN_EXIT_BLEND);
			}
			else if (ms.passChars)
				MvSeparate(dt);
			ms.ending -= dt;
			if (ms.ending <= 0.0f)
				FinishMove(false);
			return;
		}
		if (ms.target)
			CounterSteer(dt);
		else if (zdcore::IsEvadeMove(*ms.def))
			EvadeSteer(dt);
		if (zdcore::MoveHitDue(*ms.def, tPrev, ms.t)) {
			if (zdcore::IsJumpAttack(*ms.def) || zdcore::IsRunAttack(*ms.def))
				LungeStrike();
			else
				CounterStrike();
		}
		zCModel* hm = player->GetModel();
		if (!MoveGuard(hm)) {
			FinishMove(true);
			return;
		}
		if (MoveAlong(tPrev)) {
			ZDebug("Ruch %s: utknal po %.2f s - do postawy", ms.def->id.c_str(), ms.t);
			BeginEnding(MOVE_ABORT_BLEND);
			return;
		}
		if (ms.passChars && ms.t >= ms.def->cancel - MOVE_END_BLEND - MOVE_NPC_SEPARATE_LEAD)
			MvSeparate(dt);
		if (ms.runExitAt >= 0.0f && ms.t >= ms.runExitAt && MvRunExitWanted()) {
			if (BeginRunExit())
				return;
			ms.runExitAt = -1.0f;   // petla biegu nie weszla - zwykly koniec w gardzie
		}
		if (ms.t >= ms.def->cancel - MOVE_END_BLEND)
			BeginEnding(MOVE_END_BLEND);
	}
}
