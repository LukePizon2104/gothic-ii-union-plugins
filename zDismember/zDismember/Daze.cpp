// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static const char* DAZE_OVERLAY = "HUMANS_ZDF_DAZE.MDS";

	struct DazeEntry {
		oCNpc* npc;    // AddRef
		float age;
	};

	static std::vector<DazeEntry> dazed;

	bool IsHumanGuild(oCNpc* npc) {
		static int separator = -1;
		if (separator < 0) {
			separator = 16;
			if (parser)
				if (zCPar_Symbol* sym = parser->GetSymbol("GIL_SEPERATOR_HUM"))
					sym->GetValue(separator, 0);
		}
		return npc && npc->guild < separator;
	}

	static DazeEntry* Find(oCNpc* npc) {
		for (auto& e : dazed)
			if (e.npc == npc)
				return &e;
		return nullptr;
	}

	void DazeBeforeDrop(oCNpc* victim, oCNpc* attacker) {
		if (!zcfg.dazeEnabled || !victim || attacker != player || victim == player || Find(victim))
			return;
		zCModel* model = victim->GetModel();
		if (!model || !IsHumanGuild(victim) || _stricmp(model->GetVisualName().ToChar(), "HUMANS") != 0 && !strstr(model->GetVisualName().ToChar(), "HUMANS"))
			return;
		if (!model->ApplyModelProtoOverlay(DAZE_OVERLAY)) {
			static bool warned = false;
			if (!warned)
				ZLog("Daze: brak nakladki %s - oszolomienie wylaczone", DAZE_OVERLAY);
			warned = true;
			return;
		}
		victim->AddRef();
		dazed.push_back({ victim, 0.0f });
		ZDebug("Daze: %s", victim->GetInstanceName().ToChar());
	}

	bool IsDazed(oCNpc* npc) {
		return Find(npc) != nullptr;
	}

	bool DazeCanExecute(oCNpc* npc) {
		DazeEntry* e = Find(npc);
		return e && e->age >= zcfg.dazeGrace && !npc->IsDead() && npc->IsUnconscious();
	}

	static std::vector<DazeEntry>::iterator Drop(std::vector<DazeEntry>::iterator it) {
		it->npc->Release();
		return dazed.erase(it);
	}

	void DazeRelease(oCNpc* npc) {
		for (auto it = dazed.begin(); it != dazed.end(); ++it)
			if (it->npc == npc) {
				Drop(it);
				return;
			}
	}

	// Koniec okna: zdejmujemy nakladke i NPC przewraca sie animacja z gry - dalej zwykla nieprzytomnosc.
	static void FallDown(oCNpc* npc) {
		zCModel* model = npc->GetModel();
		if (!model)
			return;
		bool back = model->IsAnimationActive("S_WOUNDEDB") || model->IsAnimationActive("T_STAND_2_WOUNDEDB");
		model->RemoveModelProtoOverlay(DAZE_OVERLAY);
		model->StartAni(back ? "T_STAND_2_WOUNDEDB" : "T_STAND_2_WOUNDED", 0);
	}

	void DazeUpdate(float dt) {
		for (auto it = dazed.begin(); it != dazed.end();) {
			DazeEntry& e = *it;
			e.age += dt;
			oCNpc* npc = e.npc;
			if (npc->IsDead() || !npc->homeWorld) {
				it = Drop(it);   // nakladka zostaje - smierc z kleczek gra z niej
				continue;
			}
			if (DirectorIsVictim(npc)) {
				++it;
				continue;
			}
			if (!npc->IsUnconscious()) {
				if (zCModel* model = npc->GetModel())
					model->RemoveModelProtoOverlay(DAZE_OVERLAY);
				it = Drop(it);
				continue;
			}
			if (e.age >= zcfg.dazeSeconds) {
				FallDown(npc);
				it = Drop(it);
				continue;
			}
			++it;
		}
	}

	void DazeBeforeSave() {
		while (!dazed.empty()) {
			oCNpc* npc = dazed.front().npc;
			if (!npc->IsDead() && npc->homeWorld)
				FallDown(npc);
			Drop(dazed.begin());
		}
	}

	void DazeClearAll() {
		for (auto& e : dazed)
			e.npc->Release();
		dazed.clear();
	}
}
