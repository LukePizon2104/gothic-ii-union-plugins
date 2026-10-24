// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Odcieta czesc: klon modelu NPC z jedna galezia szkieletu, w miejscu wezla, z predkoscia 'velocity' (cm/s).
	zCVob* SpawnGib(oCNpc* npc, int keepNode, const zVEC3& velocity);
	void GibsUpdate(float dt);
	int GibsCount();
	// Usuwa odlamki odciete z postaci (node = wezel ciecia, -1 = wszystkie jej) - cofniecie czasu. Zwraca liczbe.
	int GibsRemoveFor(oCNpc* npc, int node);
	// Czesci nie trafiaja do zapisu: przed zapisem wyjmowane ze swiata, po zapisie wkladane z powrotem.
	void GibsBeforeSave();
	void GibsAfterSave();
	void GibsClearAll();
}
