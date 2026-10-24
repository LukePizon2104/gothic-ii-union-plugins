// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Tempo ciosow stylu Wiedzmina (menu "Tempo ciosow Wiedzmina", [ZDISMEMBER] W3AtkSpeed1HLeft..2HCombo) i wariant ciec
	// bocznych 2H (W3Side2H). Tempo = fpsRate animacji T_xHATTACKL/R i S_xHATTACK TYLKO w prototypach naszych nakladek
	// HUMANS_ZDF_W3_1H / _2H (i szybkich ciec HUMANS_ZDF_W3_2HS) = fps z pliku (zapamietany przy pierwszym widzeniu) x %.
	// Silnik liczy trafienie, okno kombinacji i koniec ciosu w klatkach, wiec skaluja sie razem z animacja; nasze
	// systemy (Reach "cios za", doskok, PerfectDodge, wyjscia ruchow) biora biezacy fpsRate. 0 % = "jak natywne": skala
	// = czas do kolejnego ciosu W3 / animacji, ktora gralaby bez nakladki W3 (talent / zEBA / HUMANS), w 0,5..3.
	// Ciecia boczne 2H: W3 = z pliku, W3 szybkie = animacje z prototypu HUMANS_ZDF_W3_2HS (wczytany, NIE zakladany)
	// wpisane w protoAnis HUMANS_ZDF_W3_2H, natywne = pusty wpis (silnik bierze animacje z nakladki ponizej). Wpis
	// zmieniany tylko, gdy zadna z tych animacji nie gra na bohaterze; prototyp z podmienionym wpisem trzymamy wlasna
	// referencja (nie zniknie z podmiana), referencje animacji zbilansowane.
	void AtkTempoUpdate();                  // co klatke po stylach
	void AtkTempoRestore(const char* why);  // tempo z pliku + ciecia boczne 2H z pliku (plugin wylaczony)
	void AtkTempoForget();                  // nowy swiat: odnowienie wpisow logu
	std::string AtkTempoConsole(const std::string& arg);   // zdism atk [info]
}
