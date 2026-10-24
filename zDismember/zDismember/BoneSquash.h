// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Sciskanie wezlow modelu do ~0 zaraz po zCModel::AdvanceAnis (dziala takze pod GD3D11).
	void SquashNode(zCModel* model, int nodeIndex);
	bool SquashHasNode(zCModel* model, int nodeIndex);
	// Zdejmuje sciskanie wezla (cofniecie czasu): skala 1 od razu, wpis modelu znika, gdy pusty. False, gdy nie byl scisniety.
	bool SquashUnnode(zCModel* model, int nodeIndex);
	void SquashClear(zCModel* model);
	void SquashClearAll();
	// Zwalnia wpisy modeli, ktore trzymamy juz tylko my (NPC usuniety ze swiata). Wolac co kilka sekund.
	void SquashCollect();

	// Klon odcietej czesci: zamrozona poza zrodla, przodkowie galezi scisnieci do punktu w poczatku ukladu,
	// korzen galezi = macierz jednostkowa (czesc lezy dokladnie w ukladzie voba).
	void SquashSetBranch(zCModel* clone, int keepNode, const std::vector<zMAT4>& pose);

	// Indeks wezla: dokladna nazwa, a gdy brak - pierwszy zawierajacy 'contains'.
	int FindNode(zCModel* model, const char* exact, const char* contains = nullptr);
	int NodeIndex(zCModel* model, zCModelNodeInst* node);
	bool NodeInSubtree(zCModelNodeInst* node, zCModelNodeInst* root);

	// Czy prototyp (baza / nakladka) ma te animacje (wskaznik) - po aniID, bez zCModelPrototype::SearchAni, ktore
	// wywraca sie na pustych wpisach protoAnis nakladek (crash w New Balance). Przy Debug=1 raz na prototyp ZDebug.
	bool ProtoHasAni(zCModelPrototype* proto, zCModelAni* ani);
	// Najwyzsza nakladka modelu (albo baza) z ta animacja; "" gdy zadna.
	std::string ProtoOfAniName(zCModel* model, zCModelAni* ani);
}
