// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct ZCommand {
		std::string name, help;
		ZCommandFn fn;
	};

	static std::vector<ZCommand>& ZCommands() {
		static std::vector<ZCommand> list;
		return list;
	}

	void ZConsoleRegister(const char* name, const char* help, ZCommandFn fn) {
		ZCommands().push_back({ zdcore::Upper(name), help, fn });
		if (zcon)
			zcon->Register(zSTRING("ZDISM ") + zSTRING(name), help);
	}

	oCNpc* ZConsoleTarget() {
		if (!player)
			return nullptr;
		if (auto focus = zDYNAMIC_CAST<oCNpc>(player->focus_vob))
			return focus;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return nullptr;
		zVEC3 p = player->GetPositionWorld();
		oCNpc* best = nullptr;
		float bestDist = 2000.0f * 2000.0f;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 q = npc->GetPositionWorld();
			float dx = q.n[0] - p.n[0], dy = q.n[1] - p.n[1], dz = q.n[2] - p.n[2];
			float d = dx * dx + dy * dy + dz * dz;
			if (d < bestDist) {
				bestDist = d;
				best = npc;
			}
		}
		return best;
	}

	static int __cdecl ZConsoleEval(const zSTRING& input, zSTRING& msg) {
		auto args = zdcore::SplitArgs(input.ToChar());
		if (args.empty() || args[0] != "ZDISM")
			return FALSE;
		std::vector<std::string> sub(args.begin() + 1, args.end());
		if (sub.empty() || sub[0] == "HELP") {
			std::string all = "zdism:";
			for (auto& c : ZCommands())
				all += " " + c.name;
			msg = all.c_str();
			return TRUE;
		}
		for (auto& c : ZCommands())
			if (c.name == sub[0]) {
				msg = c.fn(sub);
				return TRUE;
			}
		msg = zSTRING("nieznana komenda zdism ") + sub[0].c_str();
		return TRUE;
	}

	// Na czas wykonania komendy ZDISM nasza funkcja trafia do evalfunc[0] i zaraz wraca oryginal.
	HOOK Ivk_zCConsole_Evaluate AS(&zCConsole::Evaluate, &zCConsole::Evaluate_Union);
	int zCConsole::Evaluate_Union(zSTRING const& input) {
		ZHookHit(ZH_CONSOLE_EVALUATE);
		if (this != zcon || evalcount <= 0 || _strnicmp(input.ToChar(), "ZDISM", 5) != 0)
			return THISCALL(Ivk_zCConsole_Evaluate)(input);
		auto saved = evalfunc[0];
		evalfunc[0] = ZConsoleEval;
		int result = THISCALL(Ivk_zCConsole_Evaluate)(input);
		evalfunc[0] = saved;
		return result;
	}
}
