// Logika bez silnika: przeskoki i wspinanie w biegu (parkour W3, traversal_w3.ini) - definicje ruchow, wykrywanie
// przeszkody z wynikow promieni, wybor klipu, dopasowanie toru 3D do prawdziwej przeszkody.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	enum TravKind { TRAV_NONE = 0, TRAV_CLIMB = 1, TRAV_VAULT = 2 };

	// Sekcja traversal_w3.ini (make_traversal.py). Czasy w s od startu, odleglosci w cm.
	struct TravDef {
		std::string id;                  // CLIMB_100_LF, VAULT_150_RF
		TravKind kind = TRAV_NONE;
		std::string overlay;             // nakladka z animacjami (T_CANNOTTAKE / T_xPARADEJUMPB)
		int bucket = 0;                  // kubelek wysokosci z klipu W3 (30/100/150/200/250)
		float height = 0.0f;             // wysokosc gory w klipie (uklad Gothica)
		bool swingLeft = true;           // foot=L: lewa noga w powietrzu na starcie (prawa podparta)
		bool idle = false;               // start=IDLE: z miejsca (bohater stoi / idzie; klipy climb_idle / vault_idle)
		float trajFps = 25.0f;
		std::vector<float> traj;         // x,y,z: x w prawo, y wysokosc voba nad ziemia startu, z do przodu
		float edge = 0.0f;               // sciana przeszkody od voba na starcie (z)
		float takeoff = 0.0f, contact = 0.0f;   // okno dopasowania wysokosci (odbicie .. podparcie na gorze)
		float release = -1.0f, land = -1.0f;    // przeskok: okno dopasowania ziemi za przeszkoda
		float depth = 0.0f;              // wspiecie: najmniejsza glebokosc gory za sciana
		float thick = 0.0f;              // przeskok: najwieksza grubosc gory
		float exit = 0.0f;               // od tej chwili z W prosto w bieg
		float cancel = 1.0f;             // koniec ruchu (cala animacja z przejsciem do postawy)
	};

	// "x,y,z x,y,z ..." -> plaska lista; false przy zlej trojce.
	bool ParseTraj3(const std::string& value, std::vector<float>& out);
	// Punkt toru w czasie t (liniowo miedzy probkami, poza zakresem skrajna probka; pusty tor = 0).
	void SampleTraj3(const std::vector<float>& xyz, float fps, float t, float& x, float& y, float& z);
	// Sekcje [ID] z overlay=, kind=CLIMB|VAULT, bucket=, height=, foot=L|R, trajFps=, traj=, edge=, takeoff=, contact=,
	// release=, land=, depth=, thick=, exit=, cancel=. Bledne pomijane (opis w errors).
	std::vector<TravDef> ParseTraversal(const std::string& text, std::vector<std::string>& errors);

	inline float TravSmooth01(float u) {
		u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
		return u * u * (3.0f - 2.0f * u);
	}

	// ---------- Wykrywanie ----------
	// Wynik promienia w ukladzie bohatera: ziemia pod nim y = 0, przod +z, prawo +x. Normalna (nx, ny, nz) - sciana
	// zwrocona do bohatera ma nz < 0; bez normalnej (vob bez poly) normal = false.
	struct TravRay {
		bool hit = false;
		float dist = 0.0f;               // od poczatku promienia
		float nx = 0.0f, ny = 0.0f, nz = 0.0f;
		bool normal = false;
	};
	// Etap wykrywania, do ktorego naleza kolejne promienie (debug: rysowanie wg etapu).
	enum TravStage {
		TRAVST_NONE = 0, TRAVST_MARCH_FWD, TRAVST_MARCH_DOWN, TRAVST_SPIKE, TRAVST_BASE, TRAVST_PROFILE, TRAVST_TOP,
		TRAVST_SIDES, TRAVST_DEPTH, TRAVST_CLEAR, TRAVST_VAULT, TRAVST_HEADROOM, TRAVST_WIDTH, TRAVST_END
	};
	// Promienie swiata (plugin: TraceRayNearestHit bez postaci; testy: pudelka, teren, zerdzie).
	struct TravWorld {
		virtual ~TravWorld() {}
		// TravDetect oznacza etap przed kolejnymi promieniami (zapis do debugu - TravRecorder); domyslnie nic
		virtual void Stage(int) {}
		// z punktu (x, y, z) w kierunku (dx, dy, dz) - uzywane: +z (poziomo do przodu), -y, +y - na dlugosc len
		virtual TravRay Cast(float x, float y, float z, float dx, float dy, float dz, float len) = 0;
		TravRay Forward(float x, float y, float z, float len) { return Cast(x, y, z, 0.0f, 0.0f, 1.0f, len); }
		// w dol z (x, yFrom, z) do yTo; trafienie na wysokosci yFrom - dist
		TravRay Down(float x, float z, float yFrom, float yTo) { return Cast(x, yFrom, z, 0.0f, -1.0f, 0.0f, yFrom - yTo); }
		// w gore z (x, yFrom, z) do yTo; true = cos nad glowa
		bool Up(float x, float z, float yFrom, float yTo) { return Cast(x, yFrom, z, 0.0f, 1.0f, 0.0f, yTo - yFrom).hit; }
	};

	struct TravLimits {
		float minHeight = 22.0f;         // nizej - stopien (bieg i tak wejdzie)
		float maxHeight = 355.0f;        // wyzej - zwykly skok gry (225..355 = kubelek 250, ponad 250 rozciagniety w pionie)
		float minFace = 30.0f;           // sciana blizej - juz w niej (zwykly skok)
		float maxFace = 420.0f;          // zasieg szukania sciany (plugin: wg predkosci, TravLookahead)
		float faceFrom = 10.0f, faceTo = 110.0f, faceStep = 10.0f;   // promienie sciany nad ziemia lokalna (dwa = sciana;
		                                 // co 10 cm - cienkie zerdzie plotu 10 cm)
		float wallMaxNy = 0.75f;         // sciana: |ny| najwyzej tyle (zerdz plotu trafiona nad / pod osia)
		float wallPairNy = 0.9f;         // drugi promien pary sciany moze byc pochyly do tyle (wierzch zerdzi; stok do ~25 st. nie)
		float wallMinFacing = 0.82f;     // sciana: cos kata poziomej normalnej do kierunku biegu (ok. 35 st.)
		float marchStep = 50.0f;         // szukanie sciany: krok po terenie (promien w dol co tyle)
		float marchUp = 90.0f;           // promien ziemi z tyle nad poprzednia ziemia (stok do ~60 st. na krok)
		float marchDown = 150.0f;        // ...do tyle pod nia (glebszy uskok = koniec szukania)
		float marchSpike = 35.0f;        // ziemia wyzej o tyle na krok, a 25 cm dalej znow nizej = wierzch zerdzi / slupka
		float marchFrom = 0.0f;          // szukanie od tej odleglosci (bufor: tylko okolica znanej sciany; teren od
		float marchFromY = 0.0f;         // ziemi marchFromY) - teren przed nia nieznany
		float scanStep = 20.0f;          // profil nad podstawa sciany co tyle (od 120 cm; nizej co 10 cm od 10 cm)
		float scanMax = 400.0f;          // profil do tej wysokosci (log podaje prawdziwa gore takze ponad maxHeight)
		float profileBack = 50.0f;       // promienie profilu startuja tyle przed sciana (nie z bohatera - teren)
		float topProbe = 35.0f;          // na wysokosci h przeszkoda = trafienie blizej niz sciana + tyle
		float topInsets[7] = { 2.0f, 5.0f, 12.0f, 25.0f, 40.0f, 60.0f, 80.0f };   // promienie gory za sciana: najblizsza plaska
		                                 // wygrywa, przed nia warga (zaokraglona / pochyla krawedz nasypu)
		float topMinNy = 0.75f;          // gora plaska (do ok. 41 st.)
		float lipMinNy = 0.64f;          // warga przed plaska gora do ok. 50 st. (bardziej stroma tylko tuz pod gora)
		float lipDrop = 50.0f;           // warga najwyzej tyle ponizej plaskiej gory
		float topTol = 22.0f;            // gora rowna: odchylka wysokosci
		float side = 18.0f;              // promienie boczne gory (pol szerokosci barkow)
		float headroom = 185.0f;         // wspiecie: miejsce nad gora (stojaca postac)
		float climbMinClear = 40.0f;     // wspiecie: wolne za sciana na wysokosci ciala co najmniej (klip: depth)
		float climbMinDepth = 50.0f;     // gora plytsza = nie platforma (przeskok albo nic)
		float climbOneSideDepth = 60.0f; // gora co najmniej tak gleboka - wystarczy jeden bok (drugi zasloniety)
		float vaultHeadroom = 110.0f;    // przeskok: miejsce nad gora (cialo przechodzi nad nia)
		float vaultLandZ = 115.0f;       // przeskok: punkt ladowania za sciana
		float vaultLandMin = -120.0f;    // ziemia za przeszkoda wzgledem podstawy sciany: najnizej
		float vaultMinDrop = 25.0f;      // ...najwyzej gora - tyle (parapet tarasu: ladowanie wyzej niz podstawa)
		float vaultMaxThick = 120.0f;    // gora grubsza - nie przeskok
		float widthSide = 32.0f;         // szerokosc sciany: promienie na wysokosci sciany tyle w bok (pien, slup = waska)
		float widthTol = 45.0f;          // ...trafiaja te sama sciane: do tyle od niej (sciana pod katem do 35 st. - 22 cm)
		float widthMinHeight = 60.0f;    // ...sprawdzane od tej wysokosci przeszkody (nizsze: krawezniki, zerdzie)
		float roundSag = 5.0f;           // boki dalej niz plaszczyzna z normalnej o tyle (oba) = obiekt okragly - bez obrotu
		float endTol = 35.0f;            // koniec ruchu: ziemia pod punktem konca najwyzej tyle od poziomu konca
		float walkMinNy = 0.70f;         // powierzchnia do chodzenia za krawedzia (dach, stok do ok. 45 st.; silnik:
		                                 // zMV_MAX_GROUND_ANGLE_WALK ok. 50 st.) - gora pochyla, nie zastawia "wolnego"
		float walkTan = 1.0f;            // ...przyrost wysokosci na cm w glab (tan 45 st.)
	};
	// Glebokosc gory: profil za sciana (wiecej = "gleboka" platforma, TRAV_DEEP).
	static const float TRAV_DEPTH_STEPS[] = { 20.0f, 35.0f, 50.0f, 65.0f, 80.0f, 100.0f, 120.0f, 140.0f };
	static const float TRAV_DEEP = 150.0f;

	struct TravResult {
		TravKind kind = TRAV_NONE;
		const char* reason = "";         // dlaczego nie (log, konsola)
		float wallDist = 0.0f;           // sciana wzdluz kierunku biegu (0 = nie znaleziona)
		float face = 0.0f;               // sciana: odleglosc prostopadla od bohatera
		float wallYaw = 0.0f;            // kierunek w sciane wzgledem przodu (rad, + = w prawo)
		float base = 0.0f;               // ziemia u podstawy sciany wzgledem ziemi bohatera (stok przed sciana)
		float height = 0.0f;             // gora nad podstawa (kubelek klipu)
		float top = 0.0f;                // gora nad ziemia bohatera (base + height)
		float scanTop = -1.0f;           // "za wysoko": najwyzszy zablokowany poziom profilu nad podstawa
		float depth = 0.0f;              // wspiecie: glebokosc gory (TRAV_DEEP = co najmniej tyle)
		float clear = TRAV_DEEP;         // wspiecie: wolne za sciana na wysokosci ciala (TRAV_DEEP = bez ograniczen)
		float thick = 0.0f;              // przeskok: grubosc gory (gorne oszacowanie)
		float landY = 0.0f;              // przeskok: ziemia za przeszkoda wzgledem ziemi bohatera
		bool vaultOk = false;            // przeskok tez mozliwy (gdy wspiecie nie ma klipu)
		const char* vaultWhy = "";       // dlaczego nie przeskok (log)
		int bucket = 0;
		std::vector<float> groundZ, groundY;   // teren po drodze do sciany (z, y wzgledem ziemi bohatera)
		std::vector<float> topZ, topY;   // gora za sciana (z od bohatera, y) - wysokosc tam, gdzie bohater stanie
		float wallRayY = 0.0f;           // wysokosc promienia, ktory trafil sciane (wzgledem ziemi bohatera)
		float sideL = -1.0f, sideR = -1.0f;   // szerokosc: sciana na promieniach bocznych (z od bohatera, -1 = brak)
		bool round = false;              // obiekt okragly (snopek, beczka) - bez obrotu do normalnej
		bool slope = false;              // gora pochyla (dach, stok do walkMinNy) - koniec wspiecia na jej wysokosci
		float endBlockY = -1e9f;         // TravPickChecked: w punkcie konca cos wyzej niz koniec (stos skrzyn) - wysokosc
	};
	// log: opcjonalny opis do logu diagnostycznego (teren, sciana, profil, gora, decyzja).
	TravResult TravDetect(TravWorld& w, const TravLimits& lim, std::string* log = nullptr);

	// Kubelek wysokosci klipu: 30 (do 65), 100 (do 125), 150 (do 175), 200 (do 225), 250 (do maxHeight); 0 = poza.
	int TravBucket(float height, const TravLimits& lim);
	// Noga w powietrzu na starcie wg wysokosci stop (nizsza = podparta): true = lewa w powietrzu (klip _LF).
	inline bool TravSwingLeft(float leftFootY, float rightFootY) {
		return rightFootY <= leftFootY;
	}
	// Klip: ten sam rodzaj i kubelek, geometria pasuje (wspiecie: gora i miejsce za sciana co najmniej depth - 20;
	// przeskok: gora najwyzej thick + 10), najpierw noga z biegu, potem druga. Wspiecie bez klipu, a przeskok mozliwy
	// (vaultOk) - r.kind zmienia sie na przeskok. nullptr = brak.
	// idle: klipy z miejsca (start=IDLE) - najblizsza wysokosc (r.bucket = kubelek klipu), bez nogi.
	const TravDef* TravPick(const std::vector<TravDef>& defs, TravResult& r, bool swingLeft, bool idle = false);
	// Punkt konca ruchu w ukladzie bohatera (tor obrocony o wallYaw jak w pluginie) i poziom ziemi tam (TravEndLevel).
	void TravEndPoint(const TravDef& d, const TravResult& r, float& x, float& y, float& z);
	// Podparcie w punkcie konca: promien w dol trafia ziemie najwyzej lim.endTol od poziomu konca (hitY: trafienie).
	// Miejsce na stojaca postac na koncu ruchu (stopy konca i 25 cm dalej, do lim.headroom) - nawis / sciana = false.
	bool TravEndHeadroom(TravWorld& w, const TravDef& d, const TravResult& r, const TravLimits& lim);
	bool TravEndSupported(TravWorld& w, const TravDef& d, const TravResult& r, const TravLimits& lim, float* hitY = nullptr);
	// TravPick + podparcie konca (snopek: koniec obok obiektu w powietrzu): wspiecie bez podparcia - przeskok (gdy
	// mozliwy i podparty), inaczej brak (r.kind = TRAV_NONE, r.reason "brak podparcia na koncu ruchu").
	const TravDef* TravPickChecked(TravWorld& w, const std::vector<TravDef>& defs, TravResult& r, bool swingLeft,
		const TravLimits& lim, std::string* log = nullptr, bool idle = false);
	// Roslinnosc po nazwie wizualu / tekstury / materialu (drzewa, krzaki, bluszcz, liscie) - nie sciana ani gora.
	bool TravFoliageName(const std::string& name);

	// ---------- Start, bufor wcisniecia i dopasowanie toru ----------
	enum TravStart { TRAVSTART_NOW, TRAVSTART_WAIT, TRAVSTART_TOO_FAR, TRAVSTART_TOO_CLOSE };
	struct TravTiming {
		float kMin = 0.55f;              // sciana blizej niz kMin * edge - za blisko na rozbieg klipu
		float kMax = 1.2f;               // dalej niz kMax * edge - czekac (bieg dalej)
		float kGo = 1.08f;               // w czekaniu start, gdy sciana blizej niz kGo * edge
		float maxWait = 1.0f;            // najdluzsze czekanie na odleglosc przy wcisnieciu (s)
		float minSpeed = 150.0f;         // predkosc do szacowania czekania (cm/s)
		float bufferMax = 1.2f;          // bufor wcisniecia najwyzej tyle (s), potem skok gry
		float maxTurn = 0.61f;           // skret od kierunku przy wcisnieciu (rad, ok. 35 st.) = skok gry
		float lookTime = 1.0f;           // zasieg szukania sciany: predkosc * tyle s + sciana klipu
		float lookMin = 330.0f, lookMax = 800.0f;
		float idleMaxFace = 130.0f;      // z miejsca: sciana najwyzej tyle (prostopadle) - dalej skok gry; tor dochodzi do niej
		float idleLook = 220.0f;         // ...szukanie sciany do tyle wzdluz patrzenia (sciana pod katem)
	};
	// Decyzja dla sciany 'face' (cm) i klipu o scianie 'edge'; waiting = juz czeka (prog kGo).
	TravStart TravPlanStart(float face, float edge, float speed, bool waiting, const TravTiming& tm);
	// Podpowiedz przeskoku (TraversalHint, 21.10): napis z klawiszem skoku, gdy bohater biegnie na przeszkode, ktora Alt
	// teraz by przejal (canStart, klip znaleziony, start teraz albo po dobiegu) - najwyzej left razy, co najmniej gap s
	// od poprzedniej.
	struct TravHint {
		int left = 0;                    // ile podpowiedzi zostalo (TraversalHint - TraversalHintShown)
		float since = 1e9f;              // s od ostatniej
	};
	bool TravHintDue(const TravHint& h, bool canStart, bool picked, TravStart st, float gap);
	// Z miejsca: start od razu, gdy sciana najwyzej idleMaxFace (tor dochodzi do niej - dz jak przy biegu), inaczej za daleko.
	TravStart TravIdleStart(float face, const TravTiming& tm);
	// Zasieg szukania sciany przy predkosci speed (cm/s) i najdalszej scianie klipow maxEdge.
	float TravLookahead(float speed, float maxEdge, const TravTiming& tm);
	// Bufor wcisniecia (sciana w zasiegu, bieg do niej): klatka -> czekac / start / przerwac (skok gry teraz).
	enum TravBufferStep { TRAVBUF_KEEP, TRAVBUF_START, TRAVBUF_ABORT };
	struct TravBufferInput {
		float time = 0.0f;               // s od wcisniecia
		float turn = 0.0f;               // skret od kierunku przy wcisnieciu (rad)
		bool forwardKey = true;          // W trzymane (bez S)
		bool running = true;             // stan biegu i predkosc
		bool obstacleOk = true;          // przeszkoda dalej pasuje do klipu
		TravStart plan = TRAVSTART_WAIT; // TravPlanStart z czasem do konca bufora
	};
	TravBufferStep TravBufferDecide(const TravBufferInput& in, const TravTiming& tm, const char*& why);

	// Skok gry po odmowie (deasemblacja G2A 2.6). W biegu (S_RUNL) oCAniCtrl_Human::PC_JumpForward (0x6B1E00) rzuca
	// promien ze srodka postaci (centerPos) 400 cm do przodu i 400 * sin(zMV_MAX_GROUND_ANGLE_WALK) w gore: trafienie =
	// skok anulowany (do_jump = 0, nic sie nie dzieje). Inaczej T_RUNL_2_JUMP - stala animacja z ruchem korzenia, bez
	// wzgledu na predkosc biegu (G2: ok. 290 cm w 0,8 s, szczyt +82 cm; HUMANS_ACROBATIC: ok. 470 cm, +38 cm), potem
	// S_JUMP z fizyka (CheckJump 0x6B4FE0: predkosc z ruchu voba, bez szukania krawedzi). Skok w biegu nigdy nie chwyta
	// krawedzi - uderzenie w sciane w locie = odbicie. Zawis / wspiecie gry tylko ze skoku z miejsca (JumpForward
	// 0x6B21E0: DetectClimbUpLedge -> JUMPUPLOW / JUMPUPMID / JUMPUP i zawis), krawedz najwyzej
	// 0,95 * GetJumpUpHeight + wzrost nad stopami. AlignModelToLedge silnik wola tylko przy wychodzeniu z wody.
	struct TravGameJumpParams {
		float reach = 290.0f;            // zasieg ruchu korzenia T_RUNL_2_JUMP (plugin: z animacji modelu)
		float rise = 82.0f;              // szczyt lotu nad ziemia startu
		float apexAt = 0.62f;            // szczyt w tej czesci zasiegu
		float landExtra = 90.0f;         // + opadanie S_JUMP i promien ciala
		float clearMargin = 10.0f;       // stopy co najmniej tyle nad krawedzia - skok przejdzie
		float stepMax = 30.0f;           // przeszkoda najwyzej tyle - kolizja postaci na nia wejdzie (bez odbicia; log 111838:
		                                 // 43 cm 34 cm przed bohaterem - skok w nia wlecial i odbil sie 2,5 m)
		float rayY = 90.0f;              // promien silnika: start nad ziemia bohatera (centerPos)
		float rayUp = 0.766f;            // ...wznosi sie o tyle na cm do przodu (sin 50 st.)
		float rayLen = 400.0f;           // ...do przodu
		float ledgeMax = 0.0f;           // skok z miejsca: krawedz najwyzej tyle nad ziemia bohatera (0 = nieznane)
		float ledgeFace = 100.0f;        // skok z miejsca od razu przy scianie najwyzej tak daleko (prostopadle)
		float ledgeRun = 600.0f;         // ...dalej (do tyle): dobieg do sciany i skok z miejsca przy niej (plugin)
		// runda 12: chwyt w locie (TraversalAirGrab) dokonczy skok - krawedz w zasiegu dloni z toru skoku
		bool airGrab = false;
		float grabTopMin = 110.0f;       // krawedz co najmniej tyle nad ziemia startu (nie stopien)
		float grabTopMax = 307.0f;       // ...najwyzej tyle (wzrost + szczyt lotu + 45 cm nad glowe)
		float chasmDrop = 60.0f;         // podstawa sciany tyle ponizej bohatera = przepasc przed nia (dobieg wpadlby)
	};
	enum TravGameJump {
		TRAVJUMP_PASS = 0,               // skok gry (brak sciany, ladowanie przed nia, przejdzie nad przeszkoda)
		TRAVJUMP_SUPPRESS = 1,           // bez skoku (silnik i tak go anuluje albo odbicie od sciany w locie)
		TRAVJUMP_LEDGE = 2               // skok gry z miejsca (zawis / wspiecie gry) - plugin sprawdza CanJumpLedge
	};
	struct TravGameJumpInfo {
		TravGameJump verdict = TRAVJUMP_PASS;
		const char* why = "";
		float obstacle = -1.0f;          // gora przeszkody nad ziemia bohatera (-1 = nieznana)
		bool tall = false;               // gora nieznana - co najmniej obstacle
		float reach = 0.0f;              // zasieg skoku z opadaniem
		float feetAtWall = 0.0f;         // stopy w locie nad ziemia bohatera przy scianie
		float rayAtWall = 0.0f;          // promien silnika nad ziemia bohatera przy scianie
		bool engineCancels = false;      // promien silnika trafi (sciana albo teren) - skok w biegu anulowany
	};
	// Stopy w locie skoku w biegu nad ziemia startu w odleglosci z (luk do szczytu i w dol do zasiegu, dalej 0).
	float TravGameJumpFeet(const TravGameJumpParams& p, float z);
	TravGameJumpInfo TravGameJumpVerdict(const TravResult& r, const TravGameJumpParams& p);
	// Napis do debugu i logu: "skok gry: ...", "bez skoku: ...", "skok z miejsca: ..." z liczbami.
	std::string TravGameJumpLabel(const TravGameJumpInfo& g);

	// Start skoku gry w biegu (T_RUNL/R_2_JUMP): silnik potrafi w jednej klatce cofnac bohatera do pozycji sprzed ok.
	// 0,27 s (log 16.10: -157 cm przy 585 cm/s, -206 cm przy 764 cm/s). Krok w klatce (wzdluz biegu, w bok) w oknie
	// startu skoku = cofniecie do poprawienia (plugin przywraca pozycje sprzed klatki + poprzedni krok).
	struct TravJumpGuardParams {
		float window = 0.5f;             // s od startu animacji skoku
		float maxBack = 40.0f;           // krok w tyl wiekszy niz tyle w jednej klatce = cofniecie (kolizja daje kilka cm)
	};
	bool TravJumpTeleport(float along, float t, const TravJumpGuardParams& p);

	// Runda 9 - skok korzenia animacji w klatce: przesuniecie z ruchu korzenia (zCModel::vobTrans 0x150, MoveLocal
	// w zCVob::DoFrameActivity 0x602D8A, liczone w AdvanceAnis 0x57CA90 z thisPos - lastPos animacji) ponad to, co
	// animacja moze dac w klatce = blad (klatki korzenia petli biegu niespojne po zmianie animacji / nakladki), nie ruch.
	// Lokalnie: x w prawo, y w gore, z do przodu.
	struct TravRootGlitchParams {
		float base = 60.0f;              // cm w klatce zawsze dozwolone
		float speed = 1000.0f;           // cm/s ruchu korzenia (sprint ok. 850) - dozwolone dt * tyle
	};
	bool TravRootGlitch(float tx, float ty, float tz, float dt, const TravRootGlitchParams& p);
	// Krok w locie wzdluz kierunku lotu najwyzej tyle - ped i poprawki nie przekraczaja limitu pedu.
	struct TravFlightClampParams {
		float slack = 1.1f;              // x limit pedu (drgania czasu klatki)
		float extra = 3.0f;              // cm
	};
	float TravFlightMaxStep(float maxSpeed, float dt, const TravFlightClampParams& p);
	// Straznik lotu: cofniecie w klatce na starcie (TravJumpTeleport) - pozycja z poprzedniej klatki plus zwykly krok
	// (lastAlong, najwyzej maxStep); krok do przodu ponad maxStep (powrot silnika po cofnieciu, glitch) - przyciety
	// zawsze. Zwraca krok do ustawienia (wzdluz lotu); fixed: 1 cofniecie, 2 przyciecie, 0 bez zmian.
	float TravFlightGuardStep(float along, float lastAlong, float airT, float maxStep, const TravJumpGuardParams& g,
		int& fixed);

	// Chwyt krawedzi w locie (skok gry, spadanie): krawedz znaleziona przez silnik (DetectClimbUpLedge) na wysokosci dloni.
	struct TravAirGrabParams {
		float minAirTime = 0.12f;        // s w powietrzu przed pierwszym sprawdzeniem (nie przy odbiciu)
		float minAboveFeet = 110.0f;     // krawedz co najmniej tyle nad stopami (nie stopien)
		float headBelow = 45.0f;         // krawedz od tyle pod glowa...
		float headAbove = 45.0f;         // ...do tyle nad nia (wyzej - nie siegnie, dosuniecie dloni do krawedzi za duze)
		float cooldown = 0.7f;           // s po puszczeniu krawedzi / chwycie bez ponownego chwytu
	};
	// true = chwyt (wysokosci swiata: krawedz, glowa, stopy).
	bool TravAirGrabOk(float ledgeY, float headY, float feetY, const TravAirGrabParams& p);
	// Kiedy chwycic (mniejszy przeskok do pozycji zawisu): dosuniecie dloni do krawedzi poziomo (shiftH) najwyzej
	// maxShift albo bohater juz sie do niej nie zbliza (speedToward) albo krawedz zaraz wyjdzie ponad zasieg dloni
	// (ledgeVsHead = krawedz - glowa).
	struct TravAirGrabTiming {
		float maxShift = 70.0f;          // cm - dalej czekac, bohater leci do krawedzi
		float approachSpeed = 120.0f;    // cm/s - wolniej = nie zbliza sie, chwyt od razu
		float smoothTime = 0.22f;        // s - dosuniecie do pozycji zawisu (ease-out) po AlignModelToLedge
	};
	bool TravAirGrabNow(float shiftH, float ledgeVsHead, float speedToward, const TravAirGrabParams& p,
		const TravAirGrabTiming& t);
	// Ease-out (1 - (1 - u)^3) dla dosuniecia do zawisu.
	float TravEaseOut(float u);
	// Chwyt w locie (runda 8): dlonie na krawedzi od chwytu. Poza przechodzi z lotu w S_HANG (blend), wiec dlon wzgledem
	// voba sie zmienia - co klatke cel = vob, przy ktorym dlon biezacej pozy jest na krawedzi (vob + krawedz - dlon),
	// dochodzenie z pozycji sprzed chwytu (e = ease-out 0..1); jedna os.
	inline float TravGrabLockAxis(float before, float vob, float hand, float ledge, float e) {
		float lock = vob + (ledge - hand);
		return before + (lock - before) * e;
	}
	// Trzymanie zawisu w czasie dosuwania (silnik: S_HANG bez S = od razu podciaganie T_HANG_2_STAND; S + skok = zawis):
	// do smoothTime + extra, najwyzej maxTime; S gracza albo koniec zawisu - puszczenie (silnik: spadanie / wspiecie).
	struct TravGrabHold {
		float extra = 0.08f;             // s po dosunieciu - poza zawisu w pelni (blend S_HANG = smoothTime)
		float maxTime = 0.6f;            // s - zabezpieczenie
	};
	inline bool TravGrabHolding(float st, float smoothTime, bool hangActive, bool downHeld, const TravGrabHold& h) {
		if (downHeld || !hangActive || st >= h.maxTime)
			return false;
		return st < smoothTime + h.extra;
	}

	// Ped skoku gry w biegu / sprincie (JumpKeepMomentum): animacja T_RUNL_2_JUMP niesie ok. 370 cm/s (268 cm w 0,72 s),
	// sprint 700-850 cm/s - pozioma predkosc w skoku utrzymana jak przy odbiciu (do maxSpeed), po ladowaniu wygasa.
	struct TravMomentumParams {
		float maxSpeed = 850.0f;         // cm/s najwyzej
		float minGain = 40.0f;           // predkosc odbicia co najmniej tyle ponad animacje - inaczej bez dodatku
		float landFade = 0.2f;           // s wygasania po ladowaniu
	};
	// Pozioma predkosc do utrzymania (cm/s; 0 = bez dodatku). landedT: s od ladowania (< 0 = w powietrzu).
	float TravMomentumTarget(float v0, float animSpeed, float landedT, const TravMomentumParams& p);
	// Dosuniecie w tej klatce (cm): brakujaca czesc target * dt ponad krok silnika wzdluz biegu (bez dublowania).
	float TravMomentumExtra(float target, float engineStep, float dt);
	// Skok gry z pedem (luk na debugu, werdykt): zasieg i opadanie rozciagniete o v0 / predkosc animacji.
	TravGameJumpParams TravGameJumpWithSpeed(const TravGameJumpParams& p, float v0, float animSpeed,
		const TravMomentumParams& mp);

	// Dopasowanie toru klipu do przeszkody: rozbieg przesuniety o dz (sciana face zamiast edge) plynnie do chwili
	// podparcia, rozbieg po terenie (probki ground*, do odbicia), wysokosc o dH (gora height zamiast d.height) i ziemia
	// u podstawy (base) w oknie takeoff..contact, przeskok: ziemia za przeszkoda landY w oknie release..land.
	struct TravFit {
		float dz = 0.0f, dH = 0.0f, base = 0.0f, landY = 0.0f;
		float endAdj = 0.0f;             // wspiecie: gora tam, gdzie bohater stanie, minus gora przy krawedzi
		std::vector<float> groundZ, groundY;   // teren rozbiegu (z, y wzgledem ziemi startu)
	};
	TravFit TravFitFor(const TravDef& d, const TravResult& r);
	// Teren rozbiegu w odleglosci z (liniowo miedzy probkami, za ostatnia - base).
	float TravGround(const TravFit& f, float z);
	// Punkt toru (x w prawo, y nad ziemia startu, z do przodu) w chwili t.
	void TravSample(const TravDef& d, const TravFit& f, float t, float& x, float& y, float& z);
	// Wysokosc voba nad ziemia startu na koncu ruchu (wspiecie - gora, przeskok - ziemia za przeszkoda).
	float TravEndLevel(const TravDef& d, const TravFit& f);
	// Runda 12: wyjscie w bieg (W od chwili exit) dopiero, gdy tor jest na wysokosci konca (stopy na gorze / na ziemi za
	// przeszkoda, najwyzej tol cm od niej). Dawniej tylko exit + ziemia pod vobem do 45 cm - przy stopach w skrzyniach
	// (koniec klipu jeszcze nizej) promien ziemi startowal w przeszkodzie, kolizja po wlaczeniu wypychala bohatera
	// 1,8-4,2 m do tylu (log 138339 / 138345 / 138357).
	bool TravRunExitReady(const TravDef& d, const TravFit& f, float t, float tol = 12.0f);

	// Runda 13 - falszywe spadanie: silnik zaczyna spadanie, gdy "nad podloga" (stopy - podloga kolizji z jednego promienia
	// ze srodka, zCAIPlayer::CalcStateVars 0x50E440 -> CheckPhysics 0x50DC40) przekroczy zMV_STEP_HEIGHT - szczelina
	// miedzy skrzyniami / dziura przy kamyku pod srodkiem wystarczy. Wybaczanie: probki podlogi na obwodzie ciala
	// (8 punktow) i przed bohaterem - gdy cialo ma podparcie najwyzej maxDrop nizej, bez spadania.
	struct FallForgiveParams {
		float maxDrop = 40.0f;           // podloga probki najwyzej tyle pod stopami = podparcie (FallForgiveDrop)
		float maxRise = 60.0f;           // ...i najwyzej tyle nad nimi (wyzej - sciana / wnetrze przeszkody, nie podloga)
		int minRing = 2;                 // w ruchu: podparcie z przodu (probka przed bohaterem) i tyle na obwodzie
		int minRingStill = 5;            // bez ruchu: tyle probek obwodu z podparciem
		float minSpeed = 50.0f;          // cm/s - ponizej "bez ruchu"
		float maxTime = 0.4f;            // s wybaczania pod rzad najwyzej (dluzej - spada jak w grze)
	};
	enum FallForgive { FALL_NONE = 0, FALL_REAL = 1, FALL_FORGIVE = 2 };
	// centerDrop: stopy - podloga silnika (nad podloga); ring: 8 probek obwodu (spadek, > 1e8 = brak podlogi), ahead:
	// probka przed bohaterem w kierunku ruchu; forgivenTime: jak dlugo juz wybaczane pod rzad.
	FallForgive FallForgiveDecide(float centerDrop, float stepHeight, const float ring[8], float ahead, float speed,
		float forgivenTime, const FallForgiveParams& p);
	// Opis wyniku do logu / konsoli.
	std::string TravDescribe(const TravResult& r);

	// 23.10 (W3): przeskok / wspiecie z dobyta bronia - bron chowana od razu (wizual do slotu / na plecy, bez animacji
	// chowania), ruch idzie torem i animacjami bez broni, po koncu bron wraca do reki od razu (TraversalSheathe /
	// TraversalRedraw). Tryby broni silnika: 0 brak, 1 piesci, 2 sztylet, 3 1H, 4 2H (luk / kusza / magia - przeskoku nie ma).
	struct TravSheatheIn {
		bool enabled = true;           // TraversalSheathe
		int weaponMode = 0;            // tryb broni bohatera teraz
		bool leftHandBusy = false;     // cos w ZS_LEFTHAND / ZS_LEFTARM (pochodnia, tarcza, lewa bron pary NB) - chowania
		                               // tego silnik nie obsluguje, NB robi to swoimi hakami w animacjach - zostaje jak dotad
		bool weaponMsg = false;        // dobycie / chowanie w kolejce bohatera
	};
	enum TravSheatheAct { TRAVSH_NONE = 0, TRAVSH_SHEATHE = 1, TRAVSH_KEEP = 2 };
	// NONE - bez broni (nic do chowania); SHEATHE - schowaj teraz; KEEP - ruch z bronia w reku jak dotad (why = powod).
	TravSheatheAct TravSheatheDecide(const TravSheatheIn& in, const char*& why);
	// Powrot broni po ruchu: tryb sprzed chowania, jesli bohater nadal bez broni, zywy, bron (dla 1H / 2H) nadal
	// zalozona i ruch nie skonczyl sie zapisem / wczytaniem. 0 = bez powrotu (why = powod).
	int TravRedrawMode(bool enabled, int savedMode, int weaponModeNow, bool alive, bool saveOrLoad, bool weaponEquipped,
		const char*& why);
}
