// Logika bez silnika: menu kolowe szybkiego wyboru (Wheel.cpp w pluginie, spec [dokument projektowy - niepublikowany]).
// Automat klawisza (stukniecie = ekwipunek, przytrzymanie = kolo), uklad stron (8 pol, kusza przypieta na dole), wybor
// pola z wektora myszy, tempo czasu z przenoszeniem reszty ulamkow miedzy klatkami, reguly odmowy otwarcia.
// Silnik (G2A): zCTimer::ResetTimer 0x5F96B0 liczy frameTime = ftol(dt_ms x factorMotion), przycina do 100 ms i gubi
// ulamek w kazdej klatce (lastTimer = biezacy czas) - przy 6,7 ms / klatke i tempie 0,10 swiat stoi (0 ms). WheelTimeStep
// liczy czas gry z ulamkiem (float) i int z reszta przenoszona do nastepnej klatki.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	// ---------- klawisz (Tab) ----------
	enum WheelKeyState { WK_IDLE = 0, WK_ARMED, WK_OPEN, WK_WAIT_RELEASE };
	enum WheelKeyAct {
		WKA_NONE = 0,
		WKA_TAP,        // puszczony przed progiem - klawisz dla gry (ekwipunek), wstrzykniety do bufora
		WKA_OPEN,       // trzymany >= prog - kolo
		WKA_CONFIRM,    // puszczony przy otwartym kole - zatwierdz
		WKA_LOST,       // utrata fokusu okna gry (uzbrojony: bez ekwipunku; otwarte kolo: anuluj)
	};
	struct WheelKey {
		int state = WK_IDLE;
		double pressMs = 0.0;
	};
	// Nowe wcisniecie (z bufora klawiszy). true = przejete (uzbrojone) - tylko z WK_IDLE.
	bool WheelKeyPress(WheelKey& k, double nowMs);
	// Krok co klatke: down = klawisz fizycznie trzymany, focus = okno gry ma fokus.
	WheelKeyAct WheelKeyStep(WheelKey& k, double nowMs, bool down, bool focus, int holdMs);
	// Kolo zamkniete inaczej niz puszczeniem (LPM, Esc, PPM, przerwanie): przy trzymanym klawiszu czekamy na puszczenie
	// (bez ponownego otwarcia), inaczej IDLE.
	void WheelKeyClosed(WheelKey& k, bool down);
	const char* WheelKeyStateName(int s);

	// ---------- uklad ----------
	const int WHEEL_SLOTS = 8;      // pola na okregu: 0 = gora (godz. 12), dalej zgodnie z zegarem co 45 st.
	const int WHEEL_XBOW_SLOT = 4;  // dol (godz. 6) - kusza przypieta na kazdej stronie
	enum WheelEntryKind { WE_EMPTY = 0, WE_SPELL, WE_XBOW };
	struct WheelEntry {
		int kind = WE_EMPTY;
		int spell = -1;             // indeks czaru w ksiedze (WE_SPELL)
	};
	// Kolejnosc czarow jak Shift + kolko zQuickSign (qs::WheelOrderKey): slot keyNo rosnaco, przy rownych indeks w
	// ksiedze; czary bez klawisza (keyNo <= 0) na koncu. -> indeksy czarow.
	std::vector<int> WheelSpellOrder(const std::vector<int>& keyNo);
	// Pola na czary na stronie: 7 z kusza (dol zajety), 8 bez.
	int WheelSpellsPerPage(bool xbow);
	int WheelPageCount(int nSpells, bool xbow);   // co najmniej 1
	// Pole slot strony page: czar z kolejnosci order, kusza (dol, gdy xbow) albo puste.
	WheelEntry WheelSlotEntry(const std::vector<int>& order, bool xbow, int page, int slot);
	// Strona i pole aktywnej pozycji (wybrana kusza albo czar activeSpell). false = brak (strona 0, pole -1).
	bool WheelFindActive(const std::vector<int>& order, bool xbow, bool xbowSelected, int activeSpell, int& page, int& slot);
	// Srodek pola slot na okregu o promieniu r (os y w dol, jak ekran).
	void WheelSlotCenter(int slot, float r, float& x, float& y);

	// ---------- mysz ----------
	// Pole z wektora myszy (x w prawo, y w dol): -1 w martwej strefie; inaczej kierunek od srodka -> najblizsze pole.
	int WheelSlotFromVec(float x, float y, float deadZone);
	struct WheelMouse {
		float x = 0.0f, y = 0.0f;   // suma delt (piksele umowne)
		int slot = -1;              // ostatnio wskazane pole (zostaje po powrocie do srodka)
	};
	// Delta myszy: wektor sumowany i przyciety do promienia clampR; wybor pola poza martwa strefa. true = zmiana pola.
	bool WheelMouseMove(WheelMouse& m, float dx, float dy, float clampR, float deadZone);

	// ---------- czas ----------
	struct WheelTime {
		double carry = 0.0;         // reszta ms (int frameTime) przenoszona miedzy klatkami
	};
	struct WheelTimeOut {
		float frameMs = 0.0f;       // frameTimeFloat (z ulamkiem)
		unsigned frameInt = 0;      // frameTime (int) - suma w czasie = suma frameMs
	};
	// Klatka gry: realMs (roznica lastTimer silnika) x factor, przycieta do capMs (silnik: 100 ms).
	WheelTimeOut WheelTimeStep(WheelTime& t, unsigned realMs, float factor, float capMs);
	// Plynne przejscie tempa (0..1): krok realMs / rampMs w strone celu.
	float WheelTempoApproach(float cur, float target, double realMs, double rampMs);
	// Cel tempa: kolo otwarte -> WheelSlowmo %, celowanie z kuszy -> XbowAimSlowmo %; mniejsze wygrywa; 1 = bez.
	float WheelTempoTarget(bool wheelOpen, int wheelPct, bool xbowAim, int xbowAimPct);

	// ---------- odmowa otwarcia ----------
	struct WheelSense {
		bool enabled = true;        // Wheel=1 i plugin wlaczony
		bool inGame = true;         // petla gry biegnie, jest bohater i swiat
		bool human = true;          // bohater czlowiekiem (bez przemiany)
		bool down = false;          // smierc / nieprzytomnosc
		const char* window = nullptr;   // otwarte okno gry (konsola, ekwipunek, dialog, dokument, pauza, menu)
		bool pluginWindow = false;  // okno zDismember (medytacja, sciezki, inne ZMenu)
		bool foreignWindow = false; // nowy callback wejscia przed ogame (okno innego pluginu, np. pasek czarow NB)
		bool cutscene = false;
		bool mob = false;           // interakcja z mobem
		bool pluginMove = false;    // finisher, przeskok, unik, podnoszenie
		bool quickCast = false;     // zQuickSign: rzut / celowanie / strumien / gest (ZQS_Busy)
		bool xbowBusy = false;      // strzal z kuszy w toku
		bool rewind = false;        // zTimeRewind: cofanie czasu (ZTR_RewindActive) albo czar trzymany w trybie magii
		bool empty = false;         // brak czarow w ksiedze i kuszy
	};
	enum WheelDeny {
		WDENY_OK = 0, WDENY_OFF, WDENY_NOT_IN_GAME, WDENY_NOT_HUMAN, WDENY_DOWN, WDENY_WINDOW, WDENY_PLUGIN_WINDOW,
		WDENY_FOREIGN_WINDOW, WDENY_CUTSCENE, WDENY_MOB, WDENY_PLUGIN_MOVE, WDENY_QUICKCAST, WDENY_XBOW, WDENY_REWIND,
		WDENY_EMPTY, WDENY_COUNT
	};
	WheelDeny WheelDenyCheck(const WheelSense& s);
	const char* WheelDenyText(int d);   // do logu (ASCII)

	// ---------- opis ----------
	enum WheelSpellKind { WSK_RUNE = 0, WSK_SCROLL, WSK_SIGN };
	// Rodzaj pozycji na srodku kola (cp1250): "Runa", "Zwoj x3", "Znak Wiedzmina".
	std::string WheelKindText(int kind, int amount);
	// Tekst odnowienia: "Odnowienie: 1,4 s" (pusty przy 0).
	std::string WheelCooldownText(int ms);
	// Wiersz opisu przedmiotu jak w ekwipunku: "tekst" albo "tekst  N" (count != 0); pusty tekst -> "".
	std::string WheelItemLine(const std::string& text, int count);
	// Napis strony: "Strona 2/3" (pusty przy jednej stronie).
	std::string WheelPageText(int page, int pages);
	// Nazwa przycieta do maxChars znakow (z "..."), dla podpisu pola.
	std::string WheelShortName(const std::string& name, int maxChars);

	// ---------- klawisz kola: przypisanie ----------
	// Kody WheelKey: klawiatura = DIK 1..0xFF, mysz = kody bufora silnika 0x20C..0x213 (MOUSE_BUTTONLEFT .. MOUSE_XBUTTON5).
	enum { WHEEL_MOUSE_LEFT = 0x20C, WHEEL_MOUSE_RIGHT = 0x20D, WHEEL_MOUSE_MID = 0x20E, WHEEL_MOUSE_X1 = 0x20F,
		WHEEL_MOUSE_X2 = 0x210, WHEEL_MOUSE_LAST = 0x213, WHEEL_KEY_MAX = 0x213 };
	bool WheelIsMouseCode(int code);
	int WheelMouseVk(int code);              // przycisk myszy -> VK (LPM 1, PPM 2, srodkowy 4, boczne 5 / 6); inne 0
	int WheelMouseCodeFromVk(int vk);        // odwrotnie (0 = nie przycisk)
	// Czy kod mozna przypisac kolu: Esc (anulowanie), LPM / PPM (zatwierdzenie / anulowanie w kole), kolko myszy, brak - nie.
	// why = powod (cp1250) przy odmowie.
	bool WheelKeyBindable(int code, const char** why);
	std::string WheelMouseName(int code);    // "Mysz 4" / "Mysz (srodek)"... (cp1250)
}
