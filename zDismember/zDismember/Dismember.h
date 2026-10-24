// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Wezel czesci w szkielecie (BIP01 HEAD, BIP01 L FOREARM...), -1 gdy brak.
	int PartNode(zCModel* model, zdcore::Part part);
	// Maska czesci, ktore istnieja w szkielecie i nie sa odciete (ani odciete wyzej: ramie zabiera przedramie).
	uint32_t PartsAvailable(oCNpc* npc);
	// Odcina czesc: odlatujacy klon, sciskanie wezla, krew, zapis maski w aivar. False gdy niedostepna.
	bool CutPart(oCNpc* npc, zdcore::Part part, const zVEC3& velocity, bool spawnGib = true);
	// Ponownie sciska wezly z maski (po wczytaniu gry / zmianie swiata).
	void ReapplyCuts(oCNpc* npc);

	uint32_t PersistGet(oCNpc* npc);
	void PersistSet(oCNpc* npc, uint32_t mask);
	void PersistReapplyWorld();

	// Maska odciec zastosowanych w tej sesji (bez GetModel - szybki odczyt dla zTimeRewind, RewindLink.cpp).
	uint32_t CutMaskOf(oCNpc* npc);
	// Zapis maski sesji (0 = wpis usuniety). Nie rusza modelu ani aivara.
	void CutMaskRemember(oCNpc* npc, uint32_t mask);
	void CutMasksClear();     // wczytanie / zmiana swiata
	void CutMasksCollect();   // co kilka sekund: postacie usuniete ze swiata
}
