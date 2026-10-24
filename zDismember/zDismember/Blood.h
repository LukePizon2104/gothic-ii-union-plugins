// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	bool BloodEnabled();
	// Wytrysk PFX_BLOOD w punkcie swiata (vob usuwany po kilku sekundach). dir: preferowany poziomy kierunek wytrysku
	// (np. ciosu albo lotu czesci); wytrysk nigdy nie leci w strone kamery.
	zCVob* BloodBurst(const zVEC3& pos, float lifetime = 3.0f, const zVEC3* dir = nullptr);
	// Kaluza krwi pod NPC - ta sama funkcja, ktorej uzywa oryginalne dobicie (zCAIPlayer::AddBlood).
	void BloodPool(oCNpc* npc);
	// Krew z kikuta: wytrysk co 0,35 s w pozycji wezla przez 'seconds'.
	void BloodStump(oCNpc* npc, int nodeIndex, float seconds);
	void BloodUpdate(float dt);
	// Zatrzymuje krew z kikutow postaci (wezel, -1 = wszystkie) - cofniecie czasu. Zwraca liczbe.
	int BloodStumpsRemove(oCNpc* npc, int nodeIndex);
	void BloodClearAll();
	// Gasi efekty palenia przypiete do NPC (VOB_BURN, VOB_MAGICBURN, dzieci, dym) - przy jego smierci.
	int BurnEffectsStop(oCNpc* npc);
}
