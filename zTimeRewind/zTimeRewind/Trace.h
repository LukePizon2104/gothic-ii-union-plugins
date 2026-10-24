// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Diagnostyka do zTimeRewind.log (Debug=1): opisy stanu postaci i modelu, zeby test w grze dalo sie
	// przeanalizowac z logu bez nagran.

	// Aktywne animacje: NAZWA[warstwa klatka/ostatnia waga-w-korzeniu, F = wygasa], ...
	std::string TrDescribeAnis(zCModel* m);
	// Postac: instancja, pozycja, kierunek, HP, zycie, stan ciala, tryb broni, timeScale, animacje.
	std::string TrDescribeNpc(oCNpc* npc);
	// Nazwa animacji modelu po ID (albo "-").
	std::string TrAniName(zCModel* m, int aniId);
	// Odleglosc kamery od bohatera (cm), -1 gdy brak.
	float TrCameraDistance();
	// true co 'period' sekund (licznik w acc) - ogranicza czestotliwosc wpisow.
	bool TrTraceDue(float& acc, float dt, float period);
}
