// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Gildia i sciezki klas New Balance (logika bez silnika: Core\PathMenu.*, mapa: [dokument projektowy - niepublikowany]).
	// Tylko gdy parser ma symbole NB i paczki NB pluginu (data_nb\System\Autorun\zDismember_NB_Paths.d) - Steam i Kroniki
	// bez zakladek i bez klawisza (wykrywanie po symbolach: zdcore::PmAvailable).
	// - Zakladki "Gildia" i "Sciezki" w oknie medytacji (Meditate.cpp) oraz to samo okno bez klekania pod PathMenuKey
	//   (domyslnie 26 = '[') poza walka.
	// - Gildia: wybor jednej ze zdobytych (slady VALUE_G* fake guilds) - hero.guild do nastepnej zmiany zbroi [G]
	//   (VALUE_HERO_GUILD bez zmian, hak bandytow zdejmowany).
	// - Sciezki: wlacz / wylacz znacznik (zapamietany w ZDF_PATH_KEEP, bit w ZDF_PATHS_OFF), typ wampira; podstrona
	//   umiejetnosci sciezki (wyuczone: przelacznik, trwale premie - zablokowane).
	// - Pilnowanie co 250 ms: zbroja / szata wlaczajaca wylaczona sciezke - znacznik znow 0; wylaczona umiejetnosc > 0
	//   poza rozmowa - znow 0. Ochrona rozmow (PathDialogGuard): hak oCInformationManager::SetNpc 0x6609F0 + petla -
	//   na czas rozmowy wylaczone umiejetnosci maja zapamietane wartosci (nauczyciel nie sprzeda drugi raz), po rozmowie
	//   znow 0 (wyzszy poziom kupiony w rozmowie - zapamietany).
	// Menu nie nadpisuje zadnych funkcji Daedalusa (regula 21.10 - ungate-nb). Log "Sciezki: ...", konsola zdism paths.
	bool PathMenuAvailable();

	// Strony zakladek (page 0 gildia, 1 sciezki) - dla okna medytacji i okna wlasnego. Build ustawia wiersze i stopke,
	// zostawia tytul i zakladki wlasciciela; Header co klatke; Event - zdarzenie z MenuHandleKey (poza MEV_TAB):
	// true = Esc na gorze strony (wlasciciel zamyka / wraca do swojej zakladki).
	void PathMenuEnterPage(int page);
	void PathMenuBuild(zdcore::MenuList& m, int page);
	void PathMenuHeader(zdcore::MenuList& m, int page);
	bool PathMenuEvent(zdcore::MenuList& m, int page, const zdcore::MenuEvent& e);

	// Okno bez klekania. KeyEvent: z haka ProcessInputEvents (ZMenu.cpp), true = polknij.
	bool PathMenuKeyEvent(unsigned short dik);
	bool PathMenuOpen();
	void PathMenuUpdate();   // Game_Loop: pilnowanie, ochrona rozmow, okno
	void PathMenuDraw();     // Game_PostLoop
	void PathMenuFinishNow(const char* why);   // zapis / wczytanie / reload / wylaczenie: okno zamkniete, rozmowa odtworzona
	void PathMenuLoadBegin();
	void PathMenuLoadEnd();
	std::string PathMenuConsole(const std::string& arg);   // zdism paths [info|skills|open]
}
