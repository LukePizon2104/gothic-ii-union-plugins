// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Nakladki z naszej paczki zakladane bohaterowi przez inne pluginy: gesty Znakow zQuickSign (make_signs.py) -
	// zakladane i zdejmowane przy kazdym rzucie. Medytacja W3 (Meditate.cpp) - na czas medytacji.
	static const char* PRELOAD_EXTRA[] = { "HUMANS_ZDF_SIGNS", "HUMANS_ZDF_SIGNS_STAND", "HUMANS_ZDF_W3_MEDITATE" };

	struct HeldProto {
		std::string key;              // zdcore::OverlayKey (bez .MDS)
		zCModelPrototype* proto;
	};
	static std::vector<HeldProto> preHeld;
	static zCModelPrototype* preBase = nullptr;   // baza (HUMANS.MDS bohatera) - nasza referencja
	static const double SLOW_APPLY_MS = 2.0;
	static int slowApplyLeft = 40;                // wpisow "zalozenie trwalo" na sesje

	double ZClockMs() {
		static double msPerTick = 0.0;
		if (msPerTick == 0.0) {
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			msPerTick = 1000.0 / (double)f.QuadPart;
		}
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		return (double)c.QuadPart * msPerTick;
	}

	bool PreloadHolds(const std::string& overlay) {
		std::string k = zdcore::OverlayKey(overlay);
		for (auto& h : preHeld)
			if (h.key == k)
				return true;
		return false;
	}

	void PreloadReleaseAll(const char* why) {
		if (preHeld.empty() && !preBase)
			return;
		int n = (int)preHeld.size();
		// Release 0x588EE0: refCtr-- i usuniecie przy 0 - nakladki zalozone teraz na modelach maja wlasne referencje
		for (auto& h : preHeld)
			if (h.proto)
				h.proto->Release();
		preHeld.clear();
		if (preBase)
			preBase->Release();
		preBase = nullptr;
		ZLog("Nakladki zawczasu: zwolniono %d (%s)", n, why);
	}

	void PreloadOverlays(const char* when) {
		ZPROF_SCOPE("Preload");   // po zdism reload liczy sie do klatki; w LoadEnd po nim jest zprof::Break
		if (!zcfg.preloadOverlays) {
			PreloadReleaseAll("PreloadOverlays=0");
			return;
		}
		zCModel* m = player ? player->GetModel() : nullptr;
		zCModelPrototype* base = m && HeroIsHuman() && m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		if (!base) {
			ZLog("Nakladki zawczasu (%s): pominiete - bohater bez modelu czlowieka (przemiana?), proba przy nastepnym wczytaniu", when);
			return;
		}
		if (preBase && preBase != base) {
			// nie powinno sie zdarzyc (trzymamy referencje bazy, wiec silnik znajduje ten sam prototyp) - stare nie pasuja
			ZLog("Nakladki zawczasu (%s): inna baza bohatera (%s) niz przy wczytaniu - zwalniam i wczytuje od nowa", when,
				base->modelProtoName.ToChar());
			PreloadReleaseAll("inna baza");
		}
		if (!preBase) {
			preBase = base;
			preBase->refCtr++;   // jak znaleziony prototyp w zCModelPrototype::Load (inc [esi+8] pod 0x589347)
		}
		std::vector<std::string> extra;
		for (const char* e : PRELOAD_EXTRA)
			extra.push_back(e);
		for (auto& e : AniSwapOverlayNames())   // nakladki talentu podmieniane przy zamianie animacji 1H <-> 2H
			extra.push_back(e);
		std::vector<std::string> wanted = zdcore::HeroOverlaysToPreload(StylesOverlayNames(), extra, MovesDefs(), TraversalDefs(),
			FinisherDefs());
		std::vector<std::string> held;
		for (auto& h : preHeld)
			held.push_back(h.key);
		std::vector<std::string> todo = zdcore::PreloadMissing(wanted, held);
		if (todo.empty()) {
			ZLog("Nakladki zawczasu (%s): wszystkie %d w pamieci", when, (int)preHeld.size());
			return;
		}
		double t0 = ZClockMs();
		int fromDisk = 0, inMemory = 0, absent = 0, rejected = 0;
		std::vector<std::pair<double, std::string>> slow;
		for (auto& name : todo) {
			if (!ZOverlayExists(name, "")) {   // brak MSB (np. bez prywatnej paczki zDismember_W3.vdf) - silnik tylko by ostrzegl
				absent++;
				continue;
			}
			double a = ZClockMs();
			// Load: znaleziony = refCtr+1, nowy = MSB + MAN-y z dysku, PrepareAsModelProtoOverlay(baza), refCtr 1
			zCModelPrototype* p = zCModelPrototype::Load(zSTRING((name + ".MDS").c_str()), base);
			double ms = ZClockMs() - a;
			if (!p) {
				absent++;
				ZLog("Nakladki zawczasu: %s - silnik nie wczytal (zSpy), pomijam", name.c_str());
				continue;
			}
			if (p->baseModelProto != base) {
				// wczytana wczesniej pod inna baze albo odrzucona w PrepareAsModelProtoOverlay - zalozenie bohaterowi i tak
				// by sie nie udalo ("overlay- and model base-.mds do not match", 0x5788B5)
				rejected++;
				ZLog("Nakladki zawczasu: %s - inna baza niz model bohatera, nie trzymam", name.c_str());
				p->Release();
				continue;
			}
			(p->refCtr == 1 ? fromDisk : inMemory)++;
			preHeld.push_back({ name, p });
			slow.push_back({ ms, name });
		}
		double total = ZClockMs() - t0;
		std::sort(slow.begin(), slow.end(), [](const std::pair<double, std::string>& a, const std::pair<double, std::string>& b) {
			return a.first > b.first;
		});
		std::string top;
		for (size_t i = 0; i < slow.size() && i < 3; i++) {
			char buf[96];
			sprintf_s(buf, "%s%s %.1f ms", i ? ", " : "", slow[i].second.c_str(), slow[i].first);
			top += buf;
		}
		ZLog("Nakladki zawczasu (%s): +%d (z dysku %d, juz w pamieci %d), brak pliku %d, inna baza %d; razem trzymanych %d; "
			"czas %.0f ms%s%s", when, fromDisk + inMemory, fromDisk, inMemory, absent, rejected, (int)preHeld.size(), total,
			top.empty() ? "" : "; najdluzej: ", top.c_str());
	}

	int ZApplyOverlayTimed(zCModel* m, const std::string& mds, const char* who) {
		if (!m)
			return 0;
		ZPROF_SCOPE("Ovl.Apply");   // silnik: Load (z dysku, gdy nikt nie trzyma prototypu) + stop wszystkich animacji
		double a = ZClockMs();
		int ok = m->ApplyModelProtoOverlay(mds.c_str());
		double ms = ZClockMs() - a;
		if (ms >= SLOW_APPLY_MS && slowApplyLeft > 0) {
			slowApplyLeft--;
			std::string k = zdcore::OverlayKey(mds);
			ZLog("Nakladki: zalozenie %s (%s) trwalo %.1f ms - %s", k.c_str(), who, ms, PreloadHolds(k) ?
				"wczytana zawczasu (koszt silnika poza dyskiem)" : zcfg.preloadOverlays ? "NIE wczytana zawczasu (MSB/MAN z dysku)" :
				"PreloadOverlays=0 (MSB/MAN z dysku)");
		}
		return ok;
	}

	void ZRemoveOverlayTimed(zCModel* m, const std::string& mds) {
		if (!m)
			return;
		ZPROF_SCOPE("Ovl.Remove");   // silnik: stop wszystkich animacji + Release (usuniecie prototypu przy refCtr 0)
		m->RemoveModelProtoOverlay(mds.c_str());
	}
}
