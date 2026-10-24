// Logika bez silnika: okno z lista wierszy - modul wielokrotnego uzytku (medytacja, potem menu gildii i umiejetnosci).
// Wiersz: akcja (Enter), wybor wartosci (lewo / prawo; Enter moze go tez wykonac, np. "Czekaj: 3 h") albo tekst.
// Nawigacja gora / dol omija wiersze ukryte, wylaczone i tekstowe (w kolko). Rysowanie (zCView) i klawisze (hak
// zCInput_Win32::SetState - silnik nie widzi wcisniec, gdy okno jest otwarte) sa w ZMenu.* pluginu.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	enum MenuRowKind { MROW_ACTION = 0, MROW_CHOICE, MROW_TEXT };

	struct MenuRow {
		int id = 0;
		MenuRowKind kind = MROW_ACTION;
		std::string label;
		std::vector<std::string> values;   // MROW_CHOICE: teksty wartosci
		int value = 0;                     // MROW_CHOICE: indeks w values
		bool wrap = false;                 // MROW_CHOICE: lewo na pierwszej = ostatnia (i odwrotnie)
		bool visible = true;
		bool enabled = true;               // wylaczony = widoczny szary, bez wyboru
		bool dim = false;                  // szary, ale wybieralny (podglad: opis w podpowiedzi, bez zmiany wartosci)
		bool activates = false;            // MROW_CHOICE: Enter wykonuje wiersz (MEV_ACTIVATE z wartoscia)
		std::string hint;                  // opis zaznaczonego wiersza (pod lista)
	};

	struct MenuList {
		std::string title;
		std::vector<std::string> tabs;     // zakladki pod tytulem (puste = bez zakladek); Q / E / Tab przelaczaja
		int tab = 0;                       // biezaca zakladka (indeks w tabs)
		std::vector<std::string> header;   // linie stanu pod tytulem
		std::vector<std::string> footer;   // podpowiedzi klawiszy na dole
		int hintLines = 2;                 // linie opisu zaznaczonego wiersza (pod lista)
		std::vector<MenuRow> rows;
		int cursor = 0;                    // indeks w rows (zaznaczony wiersz)
		int first = 0;                     // pierwszy rysowany wiersz (przewijanie dlugich list)
	};

	enum MenuKey { MKEY_NONE = 0, MKEY_UP, MKEY_DOWN, MKEY_LEFT, MKEY_RIGHT, MKEY_ENTER, MKEY_ESC, MKEY_HOME, MKEY_END,
		MKEY_PGUP, MKEY_PGDN, MKEY_TAB_PREV, MKEY_TAB_NEXT };
	// MEV_TAB: zmiana zakladki (value = nowy indeks w tabs) - wiersze buduje od nowa wlasciciel okna.
	enum MenuEventType { MEV_NONE = 0, MEV_MOVED, MEV_CHANGED, MEV_ACTIVATE, MEV_CLOSE, MEV_TAB };
	struct MenuEvent {
		MenuEventType type = MEV_NONE;
		int rowId = -1;
		int value = 0;
	};

	// Kod DIK -> klawisz okna: strzalki, W/S/A/D, klawiatura numeryczna 8/2/4/6, Enter (oba), Esc, Backspace (= Esc),
	// Home / End, PgUp / PgDn, Q (zakladka w lewo), E i Tab (zakladka w prawo). Inne -> MKEY_NONE.
	MenuKey MenuKeyFromDik(int dik);
	const char* MenuKeyName(MenuKey k);
	bool MenuRowSelectable(const MenuRow& r);
	int MenuFindRow(const MenuList& m, int id);          // indeks w rows albo -1
	MenuRow* MenuRowById(MenuList& m, int id);
	// Kursor na wybieralny wiersz: zostaje, gdy juz jest; inaczej najblizszy w dol, potem w gore; brak = -1.
	void MenuFixCursor(MenuList& m);
	// Kursor na wiersz o tym id (gdy wybieralny).
	bool MenuSelectRow(MenuList& m, int id);
	MenuEvent MenuHandleKey(MenuList& m, MenuKey k);
	// Tekst wartosci wiersza wyboru: "< x20 >" (strzalki tylko tam, gdzie mozna zmienic), wiersz bez wartosci = "".
	std::string MenuValueText(const MenuRow& r);
	// Wiersze do narysowania (indeksy w rows: widoczne, od m.first), najwyzej maxRows; m.first przesuwane tak, zeby
	// kursor byl na liscie.
	std::vector<int> MenuVisibleRows(MenuList& m, int maxRows);

	// ---------- mysz (ZMenu.cpp: kursor, przyciski, kolko) ----------
	// Mapa trafien z ostatniego rysowania okna, we wspolrzednych okna 0..8192 (jak zCView): wiersze (y, strefa
	// wartosci z polami strzalek "< " / " >"), zakladki. Rysowanie wypelnia, mysz czyta w nastepnej klatce.
	struct MenuHitRow {
		int row = -1;                      // indeks w MenuList::rows
		int y0 = 0, y1 = 0;
		int valX0 = 0, valX1 = 0;          // tekst wartosci (0, 0 = brak)
		int arrowW = 0;                    // szerokosc pola strzalki na brzegach wartosci
	};
	struct MenuHitTab {
		int tab = -1;
		int x0 = 0, x1 = 0, y0 = 0, y1 = 0;
	};
	struct MenuHitMap {
		bool valid = false;
		int x0 = 0, x1 = 8192;             // poziomy zakres wierszy
		std::vector<MenuHitRow> rows;
		std::vector<MenuHitTab> tabs;
	};
	enum MenuMouseKind { MMOUSE_MOVE = 0, MMOUSE_LEFT, MMOUSE_RIGHT, MMOUSE_WHEEL_UP, MMOUSE_WHEEL_DOWN };
	struct MenuMouseResult {
		MenuEvent ev;                      // jak z klawisza (MEV_NONE = nic)
		bool back = false;                 // PPM: wstecz (wlasciciel decyduje: podlista / zakladka / zamkniecie)
		bool inside = false;               // kursor nad oknem
	};
	int MenuHitRowAt(const MenuHitMap& h, int x, int y);   // indeks w h.rows albo -1
	int MenuHitTabAt(const MenuHitMap& h, int x, int y);   // numer zakladki albo -1
	// Zdarzenie myszy w punkcie (x, y) okna: ruch = najechanie zaznacza wiersz; LPM = zakladka, strzalka "<" / ">"
	// wiersza wyboru, Enter (akcja / wiersz z activates), inaczej nastepna wartosc; kolko nad wartoscia = zmiana
	// wartosci (w gore = w prawo), gdzie indziej = gora / dol; PPM = back.
	MenuMouseResult MenuMouse(MenuList& m, const MenuHitMap& h, MenuMouseKind k, int x, int y);

	// Autopowtorzenie strzalek (DirectInput daje tylko wcisniecie i puszczenie): po delay s co rate s.
	struct MenuRepeat {
		MenuKey key = MKEY_NONE;
		float held = 0.0f;
		float next = 0.0f;
	};
	bool MenuKeyRepeats(MenuKey k);   // gora / dol / lewo / prawo / PgUp / PgDn
	void MenuRepeatPress(MenuRepeat& r, MenuKey k);
	void MenuRepeatRelease(MenuRepeat& r, MenuKey k);
	// Liczba powtorzen trzymanego klawisza w tej klatce (0 = brak).
	int MenuRepeatStep(MenuRepeat& r, float dt, float delay = 0.40f, float rate = 0.08f);
}
