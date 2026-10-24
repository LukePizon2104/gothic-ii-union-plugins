// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	using zdcore::Part;

	int PartNode(zCModel* model, Part part) {
		static const char* exact[] = { "BIP01 HEAD", "BIP01 L FOREARM", "BIP01 R FOREARM", "BIP01 L UPPERARM", "BIP01 R UPPERARM",
			"BIP01 L CALF", "BIP01 R CALF", "BIP01 L THIGH", "BIP01 R THIGH" };
		static const char* contains[] = { nullptr, "L FOREARM", "R FOREARM", "L UPPERARM", "R UPPERARM", "L CALF", "R CALF", "L THIGH", "R THIGH" };
		if (!model || part == Part::NoPart)
			return -1;
		return FindNode(model, exact[(int)part], contains[(int)part]);
	}

	// Pamiec kikutow w aivarze AivarIndex (przezywa zapis gry). Wylaczona przy AivarIndex=-1 albo gdy PersistReapplyWorld
	// uzna, ze aivar uzywa mod (nazwa AIV_* w skryptach, wartosci nie-maski, "bez glowy" zywi) - wtedy maska tylko na sesje,
	// z wezlow scisnietych w modelu (SquashHasNode), bez zapisu do NPC.
	static zdcore::PersistMode persistMode = zdcore::PersistMode::On;

	static bool PersistUsable() {
		return zcfg.aivarIndex >= 0 && zcfg.aivarIndex < zdcore::AIVAR_COUNT && persistMode == zdcore::PersistMode::On;
	}

	// Maski odciec zastosowane w tej sesji (CutPart, ReapplyCuts, cofniecie czasu) - odczyt dla zTimeRewind w kazdym ticku
	// nagrywania (RewindLink.cpp): bez GetModel i szukania wezlow. Postac trzymana przez AddRef (adres nie wroci jako inna).
	static std::unordered_map<oCNpc*, uint32_t> cutMasks;

	uint32_t CutMaskOf(oCNpc* npc) {
		if (!npc || cutMasks.empty())
			return 0u;
		auto it = cutMasks.find(npc);
		return it != cutMasks.end() ? it->second : 0u;
	}

	void CutMaskRemember(oCNpc* npc, uint32_t mask) {
		if (!npc)
			return;
		mask &= zdcore::CUT_MASK_BITS;
		auto it = cutMasks.find(npc);
		if (it == cutMasks.end()) {
			if (!mask)
				return;
			npc->AddRef();
			cutMasks[npc] = mask;
			return;
		}
		if (mask) {
			it->second = mask;
			return;
		}
		cutMasks.erase(it);
		npc->Release();
	}

	void CutMasksClear() {
		for (auto& kv : cutMasks)
			kv.first->Release();
		cutMasks.clear();
	}

	void CutMasksCollect() {
		for (auto it = cutMasks.begin(); it != cutMasks.end();) {
			if (it->first->refCtr <= 1) {   // tylko nasza referencja - postac usunieta ze swiata
				oCNpc* npc = it->first;
				it = cutMasks.erase(it);
				npc->Release();
			}
			else
				++it;
		}
	}

	// Maska z wezlow scisnietych w tej sesji (bez aivara).
	static uint32_t SessionMask(oCNpc* npc) {
		zCModel* model = npc && npc->homeWorld ? npc->GetModel() : nullptr;
		uint32_t mask = 0;
		for (int i = 0; model && i < (int)Part::NoPart; i++) {
			int node = PartNode(model, (Part)i);
			if (node >= 0 && SquashHasNode(model, node))
				mask = zdcore::MaskSet(mask, (Part)i);
		}
		return mask;
	}

	uint32_t PersistGet(oCNpc* npc) {
		if (!npc)
			return 0u;
		if (!PersistUsable())
			return SessionMask(npc);
		int v = npc->aiscriptvars[zcfg.aivarIndex];
		return zdcore::AivarMaskValid(v) ? (uint32_t)v : 0u;
	}

	void PersistSet(oCNpc* npc, uint32_t mask) {
		if (!npc || !PersistUsable())
			return;
		int cur = npc->aiscriptvars[zcfg.aivarIndex];
		if (!zdcore::AivarMaskValid(cur)) {
			static bool warned = false;
			if (!warned)
				ZLog("Persist: aivar[%d] uzywa skrypt (wartosc %d u %s) - zmien AivarIndex w [ZDISMEMBER]",
					zcfg.aivarIndex, cur, npc->GetInstanceName().ToChar());
			warned = true;
			return;
		}
		npc->aiscriptvars[zcfg.aivarIndex] = (int)mask;
	}

	uint32_t PartsAvailable(oCNpc* npc) {
		zCModel* model = npc ? npc->GetModel() : nullptr;
		if (!model)
			return 0;
		uint32_t mask = PersistGet(npc), avail = 0;
		if (zdcore::MaskHas(mask, Part::Head))
			mask = 0x1FF;   // bez glowy reszty juz nie tniemy
		for (int i = 0; i < (int)Part::NoPart; i++) {
			Part p = (Part)i;
			if (zdcore::MaskHas(mask, p) || PartNode(model, p) < 0)
				continue;
			if ((p == Part::ArmL && zdcore::MaskHas(mask, Part::UpperArmL)) || (p == Part::ArmR && zdcore::MaskHas(mask, Part::UpperArmR)) ||
				(p == Part::LegL && zdcore::MaskHas(mask, Part::ThighL)) || (p == Part::LegR && zdcore::MaskHas(mask, Part::ThighR)))
				continue;
			avail = zdcore::MaskSet(avail, p);
		}
		return avail;
	}

	bool CutPart(oCNpc* npc, Part part, const zVEC3& velocity, bool spawnGib) {
		ZPROF_SCOPE("Cut");   // klon czesci, sciskanie, krew
		zCModel* model = npc ? npc->GetModel() : nullptr;
		int node = PartNode(model, part);
		if (node < 0 || !zdcore::MaskHas(PartsAvailable(npc), part))
			return false;
		zVEC3 pos = npc->GetTrafoModelNodeToWorld(model->nodeList[node]).GetTranslation();
		if (spawnGib)
			SpawnGib(npc, node, velocity);
		SquashNode(model, node);
		BloodBurst(pos, 3.0f, &velocity);   // krew w strone lotu czesci (od ciosu), nie w kamere
		BloodStump(npc, node, 3.0f);
		BloodPool(npc);
		PersistSet(npc, zdcore::MaskSet(PersistGet(npc), part));
		CutMaskRemember(npc, zdcore::MaskSet(CutMaskOf(npc), part));
		ZDebug("Cut %s %s (wezel %d)", npc->GetInstanceName().ToChar(), zdcore::PartName(part), node);
		return true;
	}

	void ReapplyCuts(oCNpc* npc) {
		zCModel* model = npc ? npc->GetModel() : nullptr;
		uint32_t mask = PersistGet(npc);
		if (!model || !mask)
			return;
		uint32_t applied = 0;
		for (int i = 0; i < (int)Part::NoPart; i++)
			if (zdcore::MaskHas(mask, (Part)i)) {
				int node = PartNode(model, (Part)i);
				if (node >= 0) {
					SquashNode(model, node);
					applied = zdcore::MaskSet(applied, (Part)i);
				}
			}
		CutMaskRemember(npc, CutMaskOf(npc) | applied);
	}

	// Aivary nazwane stalymi skryptu (AIV_*, AIVRX_* ...): tych nie bierzemy. Parser sie nie zmienia - raz na sesje.
	static const bool* ScriptNamedAivars() {
		static bool named[zdcore::AIVAR_COUNT] = {};
		static bool done = false;
		if (done || !parser)
			return named;
		done = true;
		int n = parser->symtab.GetNumInList(), consts = 0;
		for (int i = 0; i < n; i++) {
			zCPar_Symbol* s = parser->GetSymbol(i);
			if (!s || s->type != zPAR_TYPE_INT || s->ele != 1 || !(s->flags & zPAR_FLAG_CONST) || !zdcore::IsAivarConstName(s->name.ToChar()))
				continue;
			int v = 0;
			s->GetValue(v, 0);
			zdcore::MarkAivarName(s->name.ToChar(), v, named);
			consts++;
		}
		ZLog("Persist: %d stalych AIV* w skryptach; aivary bez nazwy: %s", consts, zdcore::FreeAivarList(named, nullptr).c_str());
		return named;
	}

	void PersistReapplyWorld() {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return;
		const bool* named = ScriptNamedAivars();
		int idx = zcfg.aivarIndex;
		bool inRange = idx >= 0 && idx < zdcore::AIVAR_COUNT;
		// Spis: u ilu NPC kazdy aivar jest niezerowy (mod moze uzywac aivarow takze z DLL, np. New Balance 81-87 bez nazw
		// AIV_* - wiec wolny = bez nazwy i zero u wszystkich) oraz co siedzi w AivarIndex.
		int used[zdcore::AIVAR_COUNT] = {};
		zdcore::PersistCensus census;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc)
				continue;
			for (int i = 0; i < zdcore::AIVAR_COUNT; i++)
				if (npc->aiscriptvars[i])
					used[i]++;
			if (inRange)
				zdcore::PersistCount(census, npc->aiscriptvars[idx], npc->attribute[NPC_ATR_HITPOINTS] > 0);
		}
		persistMode = zdcore::DecidePersist(idx, inRange && named[idx], census);
		ZLog("Persist: aivary bez nazwy i zerowe u wszystkich NPC: %s (mod moze ich uzywac z DLL - sprawdz przed AivarIndex)",
			zdcore::FreeAivarList(named, used).c_str());
		if (inRange)
			ZLog("Persist: aivar[%d] u %d NPC: maska %d (u zywych %d, z glowa %d), inne wartosci %d - pamiec kikutow %s",
				idx, census.npcs, census.masks, census.aliveMasked, census.aliveHeadless, census.foreign,
				zdcore::PersistModeName(persistMode));
		else
			ZLog("Persist: pamiec kikutow %s - odciecia tylko do konca sesji", zdcore::PersistModeName(persistMode));
		if (persistMode != zdcore::PersistMode::On)
			return;
		int count = 0, skipped = 0;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			uint32_t mask = npc ? PersistGet(npc) : 0u;
			if (!mask)
				continue;
			if (zdcore::MaskHas(mask, Part::Head) && npc->attribute[NPC_ATR_HITPOINTS] > 0) {
				skipped++;   // nasze odciecie glowy zabija - zywy z ta maska to nie nasz zapis
				continue;
			}
			ReapplyCuts(npc);
			count++;
		}
		ZLog("Persist: odtworzono kikuty u %d NPC%s", count, skipped ? " (pominieto zywych z maska glowy)" : "");
		if (skipped)
			ZDebug("Persist: pominieto %d zywych NPC z maska glowy w aivar[%d]", skipped, idx);
	}
}
