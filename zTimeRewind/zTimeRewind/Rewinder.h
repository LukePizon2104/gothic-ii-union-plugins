// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Prosba z czaru: przewijanie startuje w najblizszej klatce Game_Loop.
	void RewindRequest();
	// Auto-cofniecie przy smierci: start w najblizszej klatce, samo o 'seconds', z mana.
	void RewindRequestAuto(float seconds);
	// Przewijanie trwa albo czeka na start (smierc w tej klatce ma byc przechwycona).
	bool RewindPending();
	bool RewindActive();
	// Blokady runy (dialog, cutscenka, mob, woda, przemiana, za krotki bufor, za malo many). why = tekst dla gracza.
	bool RewindCanStart(std::string& why);
	// Game_Loop: start na prosbe, krok kursora, marionetki, Commit po zatrzymaniu.
	void RewindUpdate(float realDt);
	// Menu / zapis: Commit w biezacym punkcie kursora.
	void RewindFinishNow();
	// Wczytanie / zmiana swiata: koniec bez Commit (swiat i tak znika).
	void RewindAbort();
	// "tr rewind <s>": przewija samo o s sekund, bez many i bez trzymania akcji.
	void RewindConsole(float seconds);
}
