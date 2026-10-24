// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Ragdolle zwlok: po smierci (poza finisherami i egzekucjami - te maja wlasna animacje) cialo ludzkiego szkieletu
	// pada fizycznie (Core\Ragdoll), a kosci modelu sa co klatke ustawiane wzdluz symulowanych odcinkow. Po spoczynku
	// poza zostaje zamrozona (vob przesuniety pod miednice); po wczytaniu gry zwloki leza zwykla poza smierci.
	void CorpsesOnDeath(oCNpc* npc, oCNpc* killer);
	void CorpsesUpdate(float dt);
	// Wolane z hooka AdvanceAnis po animacji, przed sciskaniem odcietych czesci.
	void CorpseApply(zCModel* model);
	void CorpsesClearAll();
	// Porzuca ragdoll zwlok postaci (cofniecie czasu do chwili, gdy zyla): dalej poza z animacji. False, gdy nie mial.
	bool CorpseForget(oCNpc* npc);
	int CorpsesCount();
}
