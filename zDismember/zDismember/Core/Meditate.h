// Logika bez silnika: medytacja W3 (Meditate.cpp w pluginie, spec [dokument projektowy - niepublikowany]) - stany, cel czekania,
// dokladanie czasu do zegara gry, regeneracja, przerwania, dar aktywny New Balance.
// Silnik (G2A, deasemblacja 21.10):
// - oCWorldTimer (worldTime = ms gry od polnocy, day) - oCWorldTimer::Timer 0x780D80 co klatke (oCGame::Render+0x100)
//   dodaje ztimer->frameTimeFloat; po przekroczeniu doby (0xAB371C) day + 1 i oCGame::RefreshNpcs. Godzina = 250000
//   (0x83E168), minuta = 250000 / 60.
// - zCTimer::ResetTimer 0x5F96B0: frameTime = czas rzeczywisty x factorMotion, PRZYCIETY do 100 ms na klatke - tempo
//   20 przy 60 fps to w silniku tylko ok. x6. Reszte tempa plugin doklada do worldTime sam (MedClockTopUp).
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	const double MED_TICKS_PER_HOUR = 250000.0;
	const double MED_TICKS_PER_DAY = 24.0 * MED_TICKS_PER_HOUR;
	const double MED_MIN_PER_DAY = 1440.0;

	// ---------- czas gry ----------
	// Minuty gry od dnia 0 (double - duze numery dni bez utraty dokladnosci).
	double MedGameMinutes(int day, double worldTicks);
	void MedSplitMinutes(double minutes, int& day, int& hour, int& minute);
	std::string MedClockText(double minutes);   // "14:05"

	// ---------- cel czekania ----------
	enum MedUntil { MED_UNTIL_MORNING = 0, MED_UNTIL_NOON, MED_UNTIL_EVENING, MED_UNTIL_MIDNIGHT, MED_UNTIL_COUNT };
	int MedUntilHour(int until);                // 6, 12, 18, 0
	const char* MedUntilLabel(int until);       // napis w menu (cp1250)
	double MedTargetAfterHours(double now, int hours);
	// Najblizsza godzina HH:00 pozniej niz teraz - co najmniej minuta (o 6:00:30 "rano" = jutro 6:00).
	double MedTargetUntilHour(double now, int hour);

	// ---------- tempo ----------
	// 21.10: do x100 (prosba autora) - silnik i tak tnie klatke do 100 ms, reszte tempa plugin doklada do zegara,
	// wiec symulacja swiata nie przyspiesza ponad ok. x6 przy 60 fps niezaleznie od tempa
	const int MED_SPEED_MIN = 5, MED_SPEED_MAX = 100;
	int MedClampSpeed(int speed);
	const std::vector<int>& MedSpeedValues();   // 5, 10, ..., 30, 40, 50, 60, 75, 100
	int MedSpeedIndex(int speed);               // najblizsza pozycja w MedSpeedValues
	// Ile ms gry dolozyc w tej klatce do worldTime: tempo x czas rzeczywisty minus to, co dodal silnik (frameTimeFloat
	// po przycieciu do 100 ms). Bez przejscia przez polnoc (przejscie doby robi silnik w nastepnej klatce - dzien + 1,
	// RefreshNpcs) i najwyzej do celu. Zawsze >= 0.
	double MedClockTopUp(double tempo, double realMs, double engineMs, double worldTicks, double ticksPerDay, double ticksToTarget);
	// Skala czasu modelu bohatera w czekaniu (petla kleczenia w normalnym tempie mimo factorMotion): czas rzeczywisty /
	// czas silnika w klatce, 0,02..1.
	float MedHeroTimeScale(double realMs, double engineMs);

	// ---------- regeneracja ----------
	enum MedRegen { MED_REGEN_NONE = 0, MED_REGEN_GRADUAL, MED_REGEN_SLEEP, MED_REGEN_COUNT };
	const char* MedRegenLabel(int mode);        // cp1250
	struct MedRegenAcc {
		double hp = 0.0, mana = 0.0;            // ulamki punktow z poprzednich klatek
	};
	// Stopniowa: HP i mana rosna proporcjonalnie do czasu gry, pelne po regenHours godzin; dHours = przyrost w tej
	// klatce. add* = punkty do dodania teraz (najwyzej do max), ulamki zostaja w acc. Inne tryby - 0.
	void MedRegenStep(int mode, double dHours, double regenHours, int hp, int hpMax, int mana, int manaMax,
		MedRegenAcc& acc, int& addHp, int& addMana);
	// Jak sen: pelne HP i mana, gdy przeczekano co najmniej sleepHours godzin (krocej - nic).
	bool MedSleepRestores(int mode, double waitedHours, double sleepHours);

	// ---------- start ----------
	enum MedDeny {
		MDENY_OK = 0,
		MDENY_OFF,          // Meditate=0 albo plugin wylaczony
		MDENY_NO_ANIS,      // brak nakladki / animacji medytacji
		MDENY_NOT_HUMAN,    // inny model (przemiana)
		MDENY_DOWN,         // martwy, nieprzytomny
		MDENY_WINDOW,       // ekwipunek, dialog, konsola, okno na wierzchu wejscia gry
		MDENY_MOUNTED,      // na wierzchowcu (NB)
		MDENY_PLUGIN_MOVE,  // unik, przewrot, przeskok, finisher, podnoszenie pluginu
		MDENY_MOVLOCK,      // blokada sterowania (skrypt, inna wtyczka)
		MDENY_BUSY,         // kolejka komunikatow bohatera niepusta (poza chowaniem broni)
		MDENY_ATTACK,       // trwa cios
		MDENY_BODY,         // nie stoi na ziemi (skok, upadek, wspinanie, plywanie, siedzenie...)
		MDENY_WATER,        // w wodzie
		MDENY_ENEMY,        // walczacy wrog w poblizu
		MDENY_COUNT
	};
	struct MedHeroState {
		bool enabled = true, anis = true, human = true, down = false, window = false, mounted = false, pluginMove = false;
		bool movLock = false, busy = false, attacking = false, bodyOk = true, water = false, enemy = false;
	};
	MedDeny MedStartDeny(const MedHeroState& s);
	const char* MedDenyText(MedDeny d);         // do logu
	// Napis dla gracza (cp1250) albo nullptr - odmowy bez znaczenia dla gracza (wylaczone, okno, przemiana, blokada
	// sterowania - okna innych wtyczek).
	const char* MedDenyScreen(MedDeny d);

	// ---------- maszyna stanow ----------
	enum MedState { MED_IDLE = 0, MED_SHEATHE, MED_KNEEL, MED_MENU, MED_WAIT, MED_RISE };
	const char* MedStateName(int s);
	enum MedStop {
		MSTOP_NONE = 0,
		MSTOP_TARGET,       // osiagnieta godzina docelowa
		MSTOP_KEY,          // Esc / Enter
		MSTOP_HIT,          // spadek HP
		MSTOP_ENEMY,        // walczacy wrog w poblizu
		MSTOP_DIALOG,       // dialog / okno informacji
		MSTOP_DOWN,         // smierc, nieprzytomnosc, inny model
		MSTOP_LOST,         // animacja kleczenia przerwana przez gre / nakladka zdjeta z zewnatrz
		MSTOP_SHEATHE,      // bron sie nie schowala
		MSTOP_MENU,         // "Wstan" / Esc w menu
		MSTOP_EXTERNAL,     // zapis, wczytanie, wylaczenie pluginu, konsola
		MSTOP_COUNT
	};
	const char* MedStopText(int s);

	// Stan swiata w klatce (z silnika).
	struct MedSense {
		double now = 0.0;           // czas gry (minuty od dnia 0)
		bool weaponOut = false;     // bron w reku albo komunikat broni w kolejce
		bool startPlaying = false;  // animacja klekania gra
		bool loopPlaying = false;   // petla kleczenia gra
		bool stopPlaying = false;   // animacja wstawania gra
		bool cancelKey = false;     // Esc / Enter wcisniete w tej klatce (klekanie, czekanie)
		bool hit = false;           // HP spadlo od poprzedniej klatki
		bool enemy = false;         // walczacy wrog w poblizu
		bool dialog = false;        // dialog / okno informacji
		bool down = false;          // martwy, nieprzytomny, bez modelu, inny model
	};
	enum MedAct {
		MACT_NONE = 0,
		MACT_KNEEL,         // zalozyc nakladke, start klekania, blokada sterowania
		MACT_OPEN_MENU,     // kleczy - menu
		MACT_WAIT_END,      // koniec czekania (czas i tempo przywrocone), z powrotem menu
		MACT_RISE,          // start wstawania (menu zamkniete)
		MACT_FINISH,        // wstal - sprzatanie (nakladka, blokada)
		MACT_ABORT          // koniec od razu, bez animacji wstawania (trafienie, dialog, smierc, zapis)
	};
	struct MedStep {
		MedAct act = MACT_NONE;
		MedStop why = MSTOP_NONE;
		MedState from = MED_IDLE;   // stan przed krokiem (np. MED_WAIT - przywrocic tempo i zegar)
	};
	struct MedMachine {
		MedState state = MED_IDLE;
		float t = 0.0f;             // czas rzeczywisty w stanie (s)
		double waitFrom = 0.0, waitTo = 0.0;
		MedStop lastStop = MSTOP_NONE;
	};
	const float MED_SHEATHE_MAX = 3.0f;   // chowanie broni najwyzej tyle s
	const float MED_KNEEL_MAX = 9.0f;     // bezpiecznik klekania (wariant 2 trwa 7,2 s)
	const float MED_RISE_MAX = 8.0f;      // bezpiecznik wstawania
	const float MED_ANI_GRACE = 0.3f;     // tyle s po starcie animacja moze jeszcze nie grac (kolejka silnika)

	// Start: bron w reku = najpierw chowanie (MED_SHEATHE, wynik MACT_NONE), inaczej od razu klekanie (MACT_KNEEL).
	MedStep MedBegin(MedMachine& m, bool weaponOut);
	MedStep MedUpdate(MedMachine& m, const MedSense& s, float realDt);
	// Z menu: czekanie do celu (tylko w MED_MENU i gdy cel pozniej niz teraz).
	bool MedStartWait(MedMachine& m, double now, double target);
	// "Wstan" / Esc w menu, Esc w klekaniu -> MED_RISE (MACT_RISE); w innych stanach nic.
	MedStep MedRise(MedMachine& m, MedStop why);
	// Koniec z zewnatrz (zapis, wczytanie, wylaczenie): MACT_ABORT, gdy trwa medytacja.
	MedStep MedAbort(MedMachine& m, MedStop why);
	bool MedActive(const MedMachine& m);
	// Postep czekania 0..1.
	double MedWaitProgress(const MedMachine& m, double now);

	// ---------- dar aktywny (New Balance z polipatronatem) ----------
	// karma_skill_learned[96] Innos, [97] Beliar, [98] Adanos >= 1 = patronat; slot [90] = wybor wersji daru aktywnego
	// (lata patch_activegod w UNION_ABI.DLL): 1 Innos, 2 Adanos, 3 Beliar, 0 auto (lancuch Innos > Adanos > Beliar).
	enum MedGod { MGOD_AUTO = 0, MGOD_INNOS = 1, MGOD_ADANOS = 2, MGOD_BELIAR = 3 };
	const int MED_GOD_SLOT = 90, MED_GOD_INNOS_SLOT = 96, MED_GOD_BELIAR_SLOT = 97, MED_GOD_ADANOS_SLOT = 98;
	// Wybory wiersza "Dar aktywny": auto + bogowie z patronatem (Innos, Adanos, Beliar); puste, gdy bogow mniej niz 2.
	std::vector<int> MedGodChoices(int innos, int beliar, int adanos);
	// Pozycja wartosci slotu 90 na liscie (nieznana / bog bez patronatu = 0 auto).
	int MedGodChoiceIndex(const std::vector<int>& choices, int slot90);
	const char* MedGodLabel(int god);           // cp1250

	// ---------- kamera orbitalna (Meditate.cpp: haki zCAICamera::CheckKeys / AI_Normal) ----------
	// Silnik (G2A, deasemblacja 21.10): zCAICamera::DoAI 0x4A4FB0 woła CheckKeys 0x4A45C0 dwa razy; miedzy nimi liczy
	// punkt idealny camSysFirstPos (+0x288) = tracker->playerPos obrocony o bestRotY (+0x44, azymut) i bestRotX (+0x38,
	// elewacja) w ukladzie bohatera, w odleglosci bestRange x 100. Potem AI_Normal 0x4A4370 (AdjustCenterSphere = kolizja,
	// InterpolateTo = doganianie) ciagnie kamere do tego punktu. CheckKeys: mysz Y -> bestRotX zawsze (dlatego pochylanie
	// dziala), mysz X -> bestRotY tylko w trybach LOOK / INVENTORY / CLIMB / DEATH / MOBLADDER / *CAMMODMOB*; w NORMAL,
	// RUN, MELEE, MAGIC... mysz X obraca bohatera (oCAIHuman::PC_Turnings, w medytacji zablokowane), a bestRotY jest
	// przycinany do -80..90. Wbudowany tryb rozgladania (CAMMODLOOK, AI_LookingCam 0x4A3690) obraca tylko klawiszami
	// i w zakresie +-90, INVENTORY zmienia kadr (2,2 m, elewacja 20, bez rotOffsetX) - pelnej orbity w kadrze gry nie daje
	// zaden. Plugin w medytacji podstawia po kazdym CheckKeys wlasny azymut / elewacje z myszy - reszta (punkt celu,
	// odleglosc, kolizja, doganianie) zostaje silnikowi.
	enum MedCamPhase { MCAM_OFF = 0, MCAM_ACTIVE, MCAM_RETURN };
	struct MedCam {
		MedCamPhase phase = MCAM_OFF;
		// azymut bez zawijania (krotsza droga liczona raz - przy celu i biezacym w tym samym obrocie); na zewnatrz MedCamAzi
		float azi = 0.0f, aziGoal = 0.0f;
		float elev = 0.0f, elevGoal = 0.0f;   // stopnie (bestRotX)
		float elevMin = 0.0f, elevMax = 0.0f;
		float baseAzi = 0.0f;                 // bestRotY trybu kamery - cel powrotu po medytacji
		float t = 0.0f;                       // s w fazie
		float turned = 0.0f;                  // laczny obrot azymutu z myszy (stopnie, do logu)
	};
	const float MED_CAM_ELEV_MIN = -30.0f;      // silnik w CheckKeys: -60..85
	const float MED_CAM_ELEV_MAX = 80.0f;
	// Stopnie na jednostke zCInput::GetMousePos przy MeditateCamSens = 1. GetMousePos = zliczenia DirectInput x czulosc
	// z opcji gry (mouseSensitivity x 0,5 + 0,3 - 0x427527) z odwroceniem osi z opcji - czyli juz ustawienia gry.
	const float MED_CAM_DEG_PER_UNIT = 0.15f;
	const float MED_CAM_TAU = 0.08f;            // wygladzanie myszy (s)
	const float MED_CAM_RETURN_TAU = 0.25f;     // powrot za bohatera po medytacji (s)
	const float MED_CAM_RETURN_MIN_SPEED = 40.0f;   // stopnie / s - koncowka powrotu bez dlugiego ogona
	const float MED_CAM_RETURN_DONE = 0.5f;     // stopnie od celu = koniec powrotu
	const float MED_CAM_RETURN_MAX = 3.0f;      // bezpiecznik powrotu (s)
	const float MED_CAM_GAP = 0.5f;             // dluzsza przerwa miedzy klatkami kamery (dialog, menu gry) w powrocie = od razu
	const float MED_CAM_DT_MAX = 0.1f;          // krok wygladzania najwyzej tyle s

	float MedCamWrap(float deg);                    // (-180, 180]
	float MedCamDelta(float from, float to);        // najkrotsza roznica katow, (-180, 180]
	// Krok wykladniczy w strone celu (ulamek 1 - exp(-dt / tau)), co najmniej minSpeed x dt, bez przeskoku celu.
	float MedCamApproach(float cur, float goal, float dt, float tau, float minSpeed);
	float MedCamAzi(const MedCam& c);               // azymut do bestRotY, (-180, 180]
	// Start w medytacji z biezacych bestRotY / bestRotX (bez skoku). base = cel powrotu (bestRotY trybu; przy nowej
	// medytacji w trakcie powrotu - stary cel). Zakres elewacji rozszerzony do wartosci startowej.
	void MedCamBegin(MedCam& c, float azi, float elev, float base);
	// Ruch myszy (jednostki GetMousePos), tylko MCAM_ACTIVE: w prawo = azymut maleje (jak CheckKeys w trybach z obrotem,
	// widok w prawo), elevSign = zCAICamera::s_iLookAroundSgn (jak silnik: bestRotX += y).
	void MedCamMouse(MedCam& c, float dx, float dy, float degPerUnit, int elevSign);
	// Klatka (czas rzeczywisty od poprzedniej klatki kamery). true = zapisac teraz bestRotY (i w ACTIVE bestRotX); w ostatniej
	// klatce powrotu azymut = cel dokladnie, potem MCAM_OFF.
	bool MedCamStep(MedCam& c, float dt);
	// Koniec medytacji: ACTIVE -> RETURN (krotsza droga do baseAzi). Inne fazy - nic.
	void MedCamRelease(MedCam& c);
	// Tryb kamery zmieniony (SetMode wczytal bestRotY / bestRotX z CAMERA.DAT): w ACTIVE nowy cel powrotu, w RETURN koniec
	// (silnik przejal kamere z nowym trybem).
	void MedCamModeChanged(MedCam& c, float newBase);
	// Tryby, w ktorych DoAI uzywa AI_Normal (orbita ma sens): bez CAMMODFIRSTPERSON / LOOK / DIALOG i bez pierwszej osoby.
	bool MedCamModeOrbits(const std::string& modeUpper, bool firstPerson);
	// frameTime trackera (frameTimeFloatSecs / factorMotion, 0x4B742A): silnik tnie klatke do 100 ms, wiec w czekaniu
	// x100 kamera dostaje ok. 1 ms zamiast 16 ms i wlecze sie (InterpolateTo: t = veloTrans x frameTime). Wynik: czas
	// rzeczywisty (najwyzej MED_CAM_DT_MAX), gdy wyraznie (ponad MED_CAM_DT_FIX razy) wiekszy od silnikowego - inaczej
	// silnikowy (poza czekaniem oba sa rowne z dokladnoscia do pomiaru, wtedy bez zmian).
	const float MED_CAM_DT_FIX = 1.5f;
	float MedCamTrackerDt(float engineDt, float realDt);
}
