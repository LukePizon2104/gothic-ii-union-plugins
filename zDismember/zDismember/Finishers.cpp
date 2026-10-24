// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static std::vector<zdcore::FinisherDef> finisherDefs;

	bool ZReadGameFile(const char* const* names, int count, std::string& out) {
		for (int i = 0; i < count; i++) {
			VirtualFile file;
			if (!file.Open(names[i]))
				continue;
			long size = file.Size();
			if (size <= 0)
				continue;
			out.resize(size);
			file.Read(&out[0], size);
			return true;
		}
		return false;
	}

	// Gra szuka skompilowanych modeli po samej nazwie pliku - tak samo tutaj.
	static bool FileExists(const std::string& name) {
		char found[1024] = { 0 };
		return vdf_searchfile((text)zdcore::Upper(name).c_str(), found) != 0;
	}

	// Nakladka i animacja w niej (OVERLAY-ANI.MAN); bez MAN gra puscilaby animacje bazowa z tym czasem ciecia.
	bool ZOverlayExists(const std::string& overlay, const std::string& ani) {
		return FileExists(overlay + ".MSB") && (ani.empty() || FileExists(overlay + "-" + ani + ".MAN"));
	}

	static void LoadCatalogFile(const char* const* names, int count, const char* label, bool required) {
		std::string text;
		if (!ZReadGameFile(names, count, text)) {
			if (required)
				ZLog("Finishers: brak %s - finishery z tego pliku wylaczone", label);
			return;
		}
		std::vector<std::string> errors;
		auto defs = zdcore::ParseCatalog(text, errors);
		for (auto& e : errors)
			ZLog("Finishers (%s): %s", label, e.c_str());
		for (auto& d : defs) {
			if (!ZOverlayExists(d.overlay, d.heroAni)) {
				ZLog("Finishers: %s - brak nakladki %s albo animacji %s, pominiety", d.id.c_str(), d.overlay.c_str(), d.heroAni.c_str());
				continue;
			}
			if (!d.inplace.empty() && !ZOverlayExists(d.inplace, d.heroAni)) {
				ZLog("Finishers: %s - brak nakladki w miejscu %s, bez wariantu w miejscu", d.id.c_str(), d.inplace.c_str());
				d.inplace.clear();
			}
			if (!d.victimOverlay.empty() && !ZOverlayExists(d.victimOverlay, d.victimAni)) {
				ZLog("Finishers: %s - brak nakladki ofiary %s, pominiety", d.id.c_str(), d.victimOverlay.c_str());
				continue;
			}
			finisherDefs.push_back(d);
		}
	}

	void FinishersLoad() {
		finisherDefs.clear();
		static const char* mainNames[] = { "\\SYSTEM\\ZDISMEMBER\\FINISHERS.INI", "SYSTEM\\ZDISMEMBER\\FINISHERS.INI" };
		// prywatne finishery parowane z Wiedzmina 3 (osobna paczka zDismember_W3.vdf, nie do wydania)
		static const char* w3[] = { "\\SYSTEM\\ZDISMEMBER\\FINISHERS_W3.INI", "SYSTEM\\ZDISMEMBER\\FINISHERS_W3.INI" };
		LoadCatalogFile(mainNames, 2, "finishers.ini", true);
		LoadCatalogFile(w3, 2, "finishers_w3.ini", false);
		int paired = 0;
		for (auto& d : finisherDefs)
			paired += d.paired() ? 1 : 0;
		ZLog("Finishers: wczytano %d (parowanych %d)", (int)finisherDefs.size(), paired);
	}

	const std::vector<zdcore::FinisherDef>& FinisherDefs() {
		return finisherDefs;
	}

	const zdcore::FinisherDef* FinisherById(const std::string& id) {
		for (auto& d : finisherDefs)
			if (zdcore::Upper(d.id) == zdcore::Upper(id))
				return &d;
		return nullptr;
	}

	uint32_t VictimGroup(oCNpc* npc) {
		zCModel* model = npc ? npc->GetModel() : nullptr;
		if (!model)
			return 0;
		std::string name = zdcore::Upper(model->GetVisualName().ToChar());
		if (name.find("HUMANS") == 0)
			return IsHumanGuild(npc) ? zdcore::GROUP_HUMAN : (FindNode(model, "BIP01 HEAD") >= 0 ? zdcore::GROUP_MONSTER : 0u);
		if (name.find("ORC") == 0)
			return zdcore::GROUP_ORC;
		if (name.find("ZOMBIE") == 0)
			return zdcore::GROUP_ZOMBIE;
		return FindNode(model, "BIP01 HEAD") >= 0 ? zdcore::GROUP_MONSTER : 0u;
	}
}
