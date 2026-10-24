// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Medytacja W3 (spec [dokument projektowy - niepublikowany]; logika bez silnika: Core\Meditate.*, okno: ZMenu.* + Core\MenuList.*).
	// MeditateKey (Enter) albo MeditateKeyAlt + MeditateKeyAltMods (Ctrl + przecinek) poza walka: plugin chowa bron,
	// zaklada nakladke HUMANS_ZDF_W3_MEDITATE, bohater kleka (T_STAND_2_PRAY -> petla S_PRAY) i otwiera sie
	// okno: Czekaj N godz. / Czekaj do pory dnia / Tempo / Regeneracja / Dar aktywny (New Balance) / Wstan. Czekanie:
	// ztimer->factorMotion = tempo (co klatke) + dolozenie reszty tempa do zegara gry (silnik tnie klatke do 100 ms),
	// petla kleczenia w normalnym tempie (timeScale modelu). Koniec czekania (cel, Esc / Enter) = z powrotem okno;
	// walczacy wrog = wstanie; trafienie, dialog, smierc = koniec od razu. Wstanie: T_SLEEPGROUND_2_STAND, nakladka zdjeta.
	// Nazwy nakladki i animacji mozna nadpisac w Gothic.ini (klucze nie sa dopisywane): MeditateOverlay, MeditateAniStart,
	// MeditateAniLoop, MeditateAniStop - silnik przyjmuje w nakladce tylko animacje, ktore sa w modelu bazowym HUMANS.
	// Kamera orbitalna (MeditateCam, MeditateCamSens): od klekania do wstania mysz obraca kamere wokol bohatera - haki
	// zCAICamera::CheckKeys 0x4A45C0 (wlasny bestRotY / bestRotX po oryginale) i AI_Normal 0x4A4370 (frameTime kamery
	// w czekaniu); po medytacji azymut wraca plynnie za bohatera. Opis silnika i logika: Core\Meditate.h (MedCam*).

	// Nowe wcisniecie z bufora klawiszy (hak zCInput_Win32::ProcessInputEvents w ZMenu.cpp, poza oknem pluginu): true =
	// polknij (klawisz medytacji, gdy ja zaczyna; kombinacja z modyfikatorem, np. Ctrl + przecinek - zawsze).
	bool MeditateKeyEvent(unsigned short dik, int state);
	// Trwa medytacja (od decyzji do wstania): gra nie widzi klawiszy logicznych (haki GetState / GetToggled), sterowanie
	// zablokowane, hak oCAniCtrl_Human::_Stand pominiety.
	bool MeditateActive();
	// Kombinacja drugiego klawisza wcisnieta (Styles.cpp: Ctrl + przecinek nie przelacza zestawu ruchu LocoKey).
	bool MeditateComboHeld(int key);
	void MeditateUpdate();
	// Game_PostLoop: okno albo pasek czekania.
	void MeditateDraw();
	// Zapis / wczytanie / zmiana swiata / zdism reload / wylaczenie: koniec od razu (tempo, zegar, nakladka, blokada).
	void MeditateFinishNow(const char* why);
	// zdism meditate [info|stop]
	std::string MeditateConsole(const std::string& arg);
}
