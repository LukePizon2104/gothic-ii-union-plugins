// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Pierwszy istniejacy plik z listy (VDF albo dysk) do 'out'.
	bool ZReadGameFile(const char* const* names, int count, std::string& out);
	// Skompilowana nakladka (.MSB) i jej animacja (OVERLAY-ANI.MAN; pusta nazwa = bez sprawdzania animacji).
	bool ZOverlayExists(const std::string& overlay, const std::string& ani);
	// Czyta System\zDismember\finishers.ini (VDF albo dysk), odrzuca wpisy z bledami i bez nakladek.
	void FinishersLoad();
	const std::vector<zdcore::FinisherDef>& FinisherDefs();
	const zdcore::FinisherDef* FinisherById(const std::string& id);
	// GROUP_HUMAN / ORC / ZOMBIE / MONSTER (szkielet z BIP01 HEAD); 0 gdy nie nadaje sie na ofiare.
	uint32_t VictimGroup(oCNpc* npc);
}
