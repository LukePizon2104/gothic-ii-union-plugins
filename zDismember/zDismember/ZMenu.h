// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Okno listy pluginu - modul wielokrotnego uzytku (medytacja; potem menu gildii / umiejetnosci). Model wierszy i
	// nawigacja bez silnika: Core\MenuList.*. Tu: przechwytywanie klawiatury i rysowanie zCView (wzor: menu J zTransmog).
	//
	// Klawisze: hak zCInput_Win32::ProcessInputEvents 0x4D5700 (ZMenu.cpp) - po odczycie bufora DirectInput kazde nowe
	// wcisniecie z tej klatki idzie najpierw do pluginu. Gdy przechwytywanie jest wlaczone, kazde WCISNIECIE zostaje
	// u nas - znika z bufora klawiszy i z tablic stanu silnika (gra, skrypty i inne pluginy go nie widza: Esc nie
	// otwiera menu gry, strzalki / WSAD nie ruszaja bohatera, skroty nie dzialaja). Puszczen nie ruszamy - klawisz
	// trzymany przed otwarciem okna puszcza sie w grze normalnie. Poza oknem: klawisze otwierajace okna pluginu
	// (MeditateKeyEvent). UWAGA: zCInput_Win32::SetState 0x4D4E00 to nie zdarzenia klawiatury, tylko stany klawiszy
	// logicznych wstrzykiwane z kodu (ProcessInputEvents zeruje je co klatke).
	void ZMenuCapture(bool on, const char* who);
	bool ZMenuCapturing();
	// Wcisniecie (kod DIK albo przycisk myszy z bufora silnika) przy wlaczonym przechwytywaniu: do kolejki okna,
	// true = polknij. state 0 - nic (puszczenia nie przechodza przez bufor).
	bool ZMenuKeyEvent(unsigned short dik, int state);
	// Nastepne wcisniecie z kolejki (z autopowtorzeniem strzalek). false = kolejka pusta.
	bool ZMenuNextKey(zdcore::MenuKey& key, int& dik);
	void ZMenuClearKeys();
	// Autopowtorzenie trzymanej strzalki - raz na klatke, czas rzeczywisty (puszczenie z GetAsyncKeyState, bo
	// polkniety klawisz nie ma juz stanu w silniku).
	void ZMenuInputUpdate(float realDt);

	// Mysz okna (MenuMouse=1): wlasciciel wlacza kursor, gdy okno z lista jest na ekranie (medytacja w menu, okno
	// sciezek). Wtedy w haku ProcessInputEvents ruch myszy idzie do kursora pluginu (rysowany LO.TGA), a delty
	// silnika (0x8D165C / 60 / 64) sa zerowane - kamera i bohater nie dostaja ruchu. Przyciski z fizycznego stanu
	// (GetAsyncKeyState), wcisniecia myszy w buforze silnika i tak sa polykane (przechwytywanie klawiatury).
	// orbitRmb: przytrzymany PPM z ruchem = ruch dla gry (kamera orbitalna medytacji), kursor stoi; krotki klik PPM
	// (bez ruchu) = MMOUSE_RIGHT (wstecz).
	void ZMenuMouse(bool cursor, bool orbitRmb);
	bool ZMenuMouseOrbiting();   // PPM przeciagany - kamera dostaje ruch myszy
	// Nastepne zdarzenie myszy: wspolrzedne okna z ostatniego ZMenuDraw (0..8192; poza oknem < 0 / > 8192).
	bool ZMenuNextMouse(zdcore::MenuMouseKind& kind, int& wx, int& wy);
	// Mapa trafien ostatnio narysowanego okna (wiersze, wartosci, zakladki).
	const zdcore::MenuHitMap& ZMenuHitMap();

	// Polozenie okna na ekranie (wirtualne 0..8192) i wyglad.
	struct ZMenuLayout {
		int x = 4600, y = 1300, w = 3200, h = 4700;
		std::string back;                  // tekstura tla; puste = DLG_CHOICE.TGA (New Balance: AS_BACK_CRAFT.TGA)
		int transparency = 230;
		const char* titleFont = "FONT_OLD_20_WHITE_HI.TGA";
		const char* font = "FONT_OLD_10_WHITE_HI.TGA";
	};
	// Rysowanie okna listy (Game_PostLoop - po swiecie, przed kolejna klatka): tytul, naglowek, wiersze z wartosciami
	// (zaznaczony wyrozniony, przewijanie dlugich list), opis zaznaczonego wiersza, podpowiedzi.
	void ZMenuDraw(zdcore::MenuList& m, const ZMenuLayout& lay);
	// Male okno z liniami tekstu (np. stan czekania) - pierwsza linia wyrozniona.
	void ZMenuDrawLines(const std::vector<std::string>& lines, const ZMenuLayout& lay);
}
