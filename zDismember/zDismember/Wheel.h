// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Menu kolowe szybkiego wyboru w stylu W3 (spec [dokument projektowy - niepublikowany], logika Core\Wheel.*).
	// Przytrzymanie WheelKey (Caps Lock; od 23.10 wieczor, wczesniej Tab) ponad WheelHoldMs otwiera kolo: czas zwalnia do WheelSlowmo %, kamera i bohater stoja,
	// mysz wybiera pole (kierunek od srodka), rolka zmienia strone, puszczenie Tab zatwierdza (LPM tez, PPM / Esc anuluje).
	// Krotkie stukniecie = Tab wstrzykniety do bufora klawiszy (ekwipunek otwiera sie zwykla droga gry, z hakami NB / KM /
	// Hedina). Zawartosc: czary ksiegi bohatera (runy, zwoje, Znaki W3) + kusza zDismember przypieta na dole kazdej strony.
	// Wybor tylko ustawia aktywna pozycje (zQuickSign ZQS_Select) - F rzuca / strzela jak dotad.
	// Modul czasu (hak zCWorld::AdvanceClock 0x6260E0) obsluguje tez XbowAimSlowmo (spowolnienie w celowaniu z kuszy).
	void WheelUpdate();                       // Game_Loop: otwarcie / zatwierdzenie, przerwania, linia bazowa okien
	void WheelDraw();                         // Game_PostLoop: kolo nad swiatem
	bool WheelIsOpen();
	bool WheelBlocksGameKeys();               // haki GetState / GetToggled: gra nie widzi klawiszy logicznych
	bool WheelWalkOn();                       // hak GetState(GAME_SLOW): chod przelaczany stuknieciem Caps Lock (kolo pod Caps Lock)
	bool WheelFilterPress(int code);          // ZmFilterPress (bez okna ZMenu): nowe wcisniecie Tab -> uzbrojenie (true = polkniete)
	void WheelInputStep(zCInput_Win32* in);   // ZmProcessInput po filtrze: prog / stukniecie (wstrzykniecie), mysz kola
	void WheelFinishNow(const char* why);     // od razu: kolo zamkniete, tempo czasu przywrocone (zapis, wczytanie, pauza...)
	void WheelLoadBegin();
	bool WheelBindPress(int code);           // ZmFilterPress (pierwszy): nasluch klawisza kola (zdism wheel bind, menu)
	void WheelMenuLoop();                     // Game_MenuLoop: wiersz klawisza na stronie "Kolo wyboru"
	std::string WheelConsole(const std::string& arg);   // zdism wheel [info|open|close]
}
