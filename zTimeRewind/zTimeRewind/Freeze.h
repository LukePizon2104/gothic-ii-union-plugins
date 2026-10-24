// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Swiat zamrozony na czas przewijania: AI skryptow, przetwarzanie NPC, kolejki rozkazow i sterowanie
	// postaci (takze bohatera) nic nie robia. Kamera i render dzialaja dalej.
	extern bool trFrozen;
	// Ile razy marionetka musiala przelaczyc animacje (diagnostyka do logu).
	extern int trPuppetAniSwitches;

	// Glowna animacja modelu: aktywna, niewygasajaca, z najnizszej warstwy. -1 = brak.
	int TrMainAni(zCModel* m, float& frame);

	// Ustawia marionetke: pozycja, kierunek i (mode 1/2) klatka glownej animacji. mode 0 = tylko pozycja.
	void TrSetPuppetPose(oCNpc* npc, float x, float y, float z, float yaw, int aniId, float frame, int mode);
}
