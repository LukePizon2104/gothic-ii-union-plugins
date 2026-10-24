// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Gildia i sciezki New Balance (opis w PathMenu.h, logika w Core\PathMenu.*). Napisy ekranowe w cp1250 (escape).
	static const float PM_WATCH_EVERY = 0.25f;        // s czasu rzeczywistego miedzy przegladami znacznikow i umiejetnosci
	static const float PM_ENEMY_RECHECK = 0.25f;      // okno wlasne: przeglad walczacych wrogow
	static const float PM_ENEMY_RADIUS = 1500.0f;     // jak medytacja
	static const DWORD PM_HOOK_STALE_MS = 250;        // hak klawisza bez petli gry dluzej (menu gry, pauza) - klawisz dla gry
	static const DWORD PM_TELL_GAP_MS = 800;
	static const double PM_MSG_MS = 4000.0;           // komunikat w naglowku okna
	static const DWORD PM_FORCED_LOG_GAP_MS = 10000;  // ten sam znacznik zerowany czesciej - zliczane, nie logowane
	static const float PM_CONSOLE_WAIT = 15.0f;
	static const char* const PM_FIST_OVERLAY = "HUMANS_FISTBLOOD.MDS";

	enum PmRowId {
		PMR_GUILD = 100, PMR_GUILD_BASE = 101, PMR_GUILD_VIA = 102, PMR_GUILD_HOOK = 103,
		PMR_PATH0 = 200, PMR_VAMPTYPE = 290, PMR_NOPATHS = 299,
		PMR_BACK = 999, PMR_SKILL0 = 1000, PMR_GROUP0 = 3000, PMR_NOSKILLS = 3999
	};

	// ---------- zmienne skryptu przez parser ----------
	static std::unordered_map<std::string, zCPar_Symbol*> pmSyms;

	struct PmParserVars : zdcore::PmVars {
		zCPar_Symbol* Sym(const std::string& n) const {
			auto it = pmSyms.find(n);
			if (it != pmSyms.end())
				return it->second;
			zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(n.c_str())) : nullptr;
			if (s && s->type != zPAR_TYPE_INT)
				s = nullptr;
			pmSyms[n] = s;
			return s;
		}
		int Count(const std::string& n) const override {
			zCPar_Symbol* s = Sym(n);
			return s ? (int)s->ele : 0;
		}
		int Get(const std::string& n, int i = 0) const override {
			zCPar_Symbol* s = Sym(n);
			if (!s || i < 0 || i >= (int)s->ele)
				return 0;
			int v = 0;
			s->GetValue(v, i);
			return v;
		}
		void Set(const std::string& n, int v, int i = 0) override {
			zCPar_Symbol* s = Sym(n);
			if (s && i >= 0 && i < (int)s->ele)
				s->SetValue(v, i);
		}
	};
	static PmParserVars pmVars;

	// ---------- stan ----------
	static int pmAvail = -1;                      // -1 nieznane, 0 brak (Steam / Kroniki), 1 jest
	static std::string pmMissing;
	static bool pmAvailLogged = false;
	static zdcore::PmWatchMem pmMem;
	static float pmWatchAge = 1e9f;
	static DWORD pmForcedLog[zdcore::PM_PATHS] = {};
	static int pmForcedQuiet[zdcore::PM_PATHS] = {};
	static bool pmDlgRestored = false;
	static std::string pmDlgNpc;
	static bool pmFistPending = false;
	static std::string pmFistWhy;
	// strony
	static int pmSkillPath = -1;                  // podstrona umiejetnosci sciezki albo -1
	static int pmGuildSel = -1;                   // wybrana pozycja wiersza gildii (przed Enter)
	static std::vector<zdcore::PmGuildChoice> pmGuilds;
	static std::vector<int> pmVampTypes;
	static int pmLastRow[3] = { PMR_GUILD, PMR_PATH0, PMR_BACK };
	static std::string pmMsg;
	static double pmMsgUntil = 0.0;
	static DWORD pmLastTell = 0;
	// okno wlasne
	static bool pmOpen = false;
	static zdcore::MenuList pmMenu;
	static int pmTab = 0;
	static bool pmPending = false;
	static bool pmMovLock = false;
	static int pmHpPrev = -1;
	static float pmEnemyAge = 0.0f;
	static DWORD pmLoopTick = 0;
	static double pmClockPrev = 0.0;
	static float pmConsoleWait = 0.0f;

	// ---------- pomocnicze ----------

	bool PathMenuAvailable() {
		if (!zcfg.enabled || !zcfg.pathMenu || !parser)
			return false;
		if (pmAvail < 0) {
			pmAvail = zdcore::PmAvailable(pmVars, &pmMissing) ? 1 : 0;
			if (!pmAvailLogged) {
				pmAvailLogged = true;
				if (pmAvail)
					ZLog("Sciezki: menu gildii i sciezek dostepne (New Balance) - klawisz %d (mods %d), ochrona rozmow %d", zcfg.pathMenuKey,
						zcfg.pathMenuKeyMods, zcfg.pathDialogGuard);
				else
					ZLog("Sciezki: menu niedostepne - brak symboli %s (Steam / Kroniki albo paczka bez zDismember_NB_Paths.d)",
						pmMissing.c_str());
			}
		}
		return pmAvail == 1;
	}

	static int PmGuild() {
		return player ? player->guild : 0;
	}

	static std::string PmGuildLabel(int g) {
		char buf[96];
		sprintf_s(buf, "%s (%d)", zdcore::PmGuildName(g), g);
		return buf;
	}

	static void PmTell(const char* text) {
		if (!text)
			return;
		pmMsg = text;
		pmMsgUntil = ZClockMs() + PM_MSG_MS;
		DWORD now = GetTickCount();
		if (now - pmLastTell < PM_TELL_GAP_MS || !ogame || !ogame->GetTextView())
			return;
		pmLastTell = now;
		ogame->GetTextView()->PrintTimedCXY(zSTRING(text), 2500.0f, nullptr);
	}

	// Skrypt int bez parametrow (RX_ISCUTSCENEACTIVE) - tylko przy akcji, nie co klatke. Brak = 0.
	static int PmCallInt(const char* func) {
		int idx = parser ? parser->GetIndex(zSTRING(func)) : -1;
		if (idx < 0)
			return 0;
		void* r = parser->CallFunc(idx);
		return r ? *(int*)r : 0;
	}

	// Blokady z silnika i skryptu. cutscene: przy akcji takze RX_ISCUTSCENEACTIVE().
	static zdcore::PmWorld PmReadWorld(bool cutscene) {
		zdcore::PmWorld w;
		w.guild = PmGuild();
		w.trans = pmVars.Get("HEROTRANS") != 0 || pmVars.Get("HEROTRANSRUNE") != 0 || pmVars.Get("HEROTRANSOPEN") != 0 || !HeroIsHuman();
		w.dialog = !oCInformationManager::GetInformationManager().HasFinished() || (cutscene && PmCallInt("RX_ISCUTSCENEACTIVE") != 0);
		w.joint = pmVars.Get("JOINTBONUSTYPE") != 0;
		w.holyLight = pmVars.Get("PALLIGHTISON") != 0;
		w.bloodGift = pmVars.Get("RX_VAMP_BLOODDAR_CURRENTBLOODTYPE") != 0;
		int island = pmVars.Count("ANCIENT_ISLAND_ZEN") ? pmVars.Get("ANCIENT_ISLAND_ZEN") : 47;
		w.island = pmVars.Count("CURRENTLEVEL") && pmVars.Get("CURRENTLEVEL") == island;
		w.familiar = pmVars.Get("RX_VAMP_SKILLFAMILIAR_ACTIVE") != 0;
		return w;
	}

	static std::string PmBlockersText(const zdcore::PmWorld& w) {
		std::string s;
		auto add = [&](bool on, const char* t) {
			if (on)
				s += (s.empty() ? "" : ", ") + std::string(t);
		};
		add(w.trans, "przemiana");
		add(w.joint, "joint");
		add(w.holyLight, "\x8Cwi\xEAte \x8Cwiat\xB3o");
		add(w.bloodGift, "dar krwi Beliara");
		add(w.island, "Wyspa Staro\xBFytnych");
		return s;
	}

	// Indeks nakladki na liscie bohatera (bez wielkosci liter - NB zaklada "HUMANS_FISTBLOOD.mds") albo -1.
	static int PmFindOverlay(const char* mds) {
		std::string want = zdcore::Upper(mds);
		for (int i = 0; i < player->activeOverlays.GetNum(); i++)
			if (zdcore::Upper(player->activeOverlays[i].ToChar()) == want)
				return i;
		return -1;
	}

	// Nakladka piesciarza NB po zmianie typu / sciezki / gildii / stylu - jak RX_APPLYANIM_WEAPONS. Zalozenie / zdjecie
	// nakladki zatrzymuje animacje postaci, wiec w medytacji i przy otwartym oknie czeka do konca.
	static void PmFistSync() {
		if (!pmFistPending || !player || MeditateActive() || pmOpen || player->IsDead())
			return;
		pmFistPending = false;
		bool want = zdcore::PmFistOverlayWanted(pmVars, PmGuild());
		int found = PmFindOverlay(PM_FIST_OVERLAY);
		if (want == (found >= 0))
			return;
		if (want)
			player->ApplyOverlay(zSTRING(PM_FIST_OVERLAY));
		else {
			zSTRING exact = player->activeOverlays[found];   // ta sama pisownia, ktora zalozylo NB
			player->RemoveOverlay(exact);
		}
		ZLog("Sciezki: nakladka %s %s (%s; warunek NB: wampir + gildia 16 + typ 2 + Styl walki Potepionych)", PM_FIST_OVERLAY,
			want ? "zalozona" : "zdjeta", pmFistWhy.c_str());
	}

	static void PmFistLater(const char* why) {
		if (!pmVars.Count("RX_VAMP_SKILLFIST_NEWOVERLAY"))
			return;
		pmFistPending = true;
		pmFistWhy = why;
	}

	// ---------- ochrona rozmow ----------

	static void PmDlgBegin(oCNpc* npc, const char* src) {
		if (pmDlgRestored || !zcfg.pathDialogGuard || !PathMenuAvailable())
			return;
		pmDlgNpc = npc ? zdcore::Upper(npc->GetInstanceName().ToChar()) : "";
		std::vector<zdcore::PmEvent> ev;
		int n = zdcore::PmDialogRestore(pmVars, pmDlgNpc, ev);
		pmDlgRestored = true;
		if (n > 0 || !ev.empty()) {
			std::string list;
			for (auto& e : ev)
				list += (list.empty() ? "" : ", ") + e.var + " " + std::to_string(e.from) + "->" + std::to_string(e.to);
			ZLog("Sciezki: ochrona rozmow - start (%s, %s): wylaczone umiejetnosci przywrocone na czas rozmowy: %s", src,
				pmDlgNpc.empty() ? "-" : pmDlgNpc.c_str(), list.c_str());
		}
		else
			ZDebug("Sciezki: ochrona rozmow - start (%s, %s), nic do przywrocenia", src, pmDlgNpc.empty() ? "-" : pmDlgNpc.c_str());
	}

	static void PmDlgEnd(const char* why) {
		if (!pmDlgRestored)
			return;
		pmDlgRestored = false;
		std::vector<zdcore::PmEvent> ev;
		int n = zdcore::PmDialogReapply(pmVars, ev);
		for (auto& e : ev)
			if (e.kind == zdcore::PME_SKILL_BOUGHT)
				ZLog("Sciezki: w rozmowie z %s nauczona wylaczona umiejetnosc %s %d -> %d - zapamietane, znow wylaczona", pmDlgNpc.c_str(),
					e.var.c_str(), e.from, e.to);
		if (n > 0)
			ZLog("Sciezki: ochrona rozmow - koniec (%s, %s): %d umiejetnosci znow wylaczonych", why, pmDlgNpc.c_str(), n);
	}

	// oCInformationManager::SetNpc 0x6609F0 (start rozmowy; gdy IsDone == 0 - rozmowa juz trwa, oryginal nic nie robi):
	// przed oryginalem, bo warunki C_INFO liczy pozniej CollectInfos (Update w kolejnych klatkach). Petla gry (PathMenuUpdate)
	// lapie start rozmowy bez haka (inny hak w lancuchu) i koniec rozmowy.
	// UWAGA: SetNpc to w silniku prawdziwy __fastcall (SDK: "void __fastcall SetNpc(oCNpc*)") - this w ECX, npc w EDX,
	// bez argumentow na stosie. Wersja z (self, edx, npc) czytala npc ze stosu i wracala 'ret 4' - psula stos wolajacego
	// i rozmowa zawisala bez okna (21.10, zgloszenie z testow; Alt+F1 wychodzil).
	static void __fastcall PmInfoSetNpc(oCInformationManager* self, oCNpc* npc);
	static CInvoke<void(__fastcall*)(oCInformationManager*, oCNpc*)> Ivk_oCInformationManager_SetNpc(
		ENGINE == Engine_G2A ? 0x006609F0 : 0, &PmInfoSetNpc, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall PmInfoSetNpc(oCInformationManager* self, oCNpc* npc) {
		ZHookHit(ZH_INFO_SETNPC);
		if (self && npc && self->IsDone && zcfg.enabled && zcfg.pathMenu)
			PmDlgBegin(npc, "SetNpc");
		Ivk_oCInformationManager_SetNpc(self, npc);
	}

	// ---------- pilnowanie ----------

	static void PmLogEvents(const std::vector<zdcore::PmEvent>& ev) {
		DWORD now = GetTickCount();
		for (const zdcore::PmEvent& e : ev) {
			switch (e.kind) {
			case zdcore::PME_PATH_FORCED_OFF:
				if (e.id >= 0 && e.id < zdcore::PM_PATHS && pmForcedLog[e.id] && now - pmForcedLog[e.id] < PM_FORCED_LOG_GAP_MS) {
					pmForcedQuiet[e.id]++;
					break;
				}
				ZLog("Sciezki: %s wylaczona w menu, a %s = %d (zbroja / szata fake guilds albo rozmowa) - znow 0, zapamietane%s",
					zdcore::PmPath(e.id).flag, e.var.c_str(), e.from, pmForcedQuiet[e.id] ? (" (+" + std::to_string(pmForcedQuiet[e.id]) +
					" razy wczesniej bez wpisu)").c_str() : "");
				pmForcedLog[e.id] = now;
				pmForcedQuiet[e.id] = 0;
				break;
			case zdcore::PME_PATH_GAME_OFF:
				ZLog("Sciezki: %s %d -> 0 bez menu (przepaska Bractwa, przyjecie DH / DK, petla szamana) - wylaczona przez gre, "
					"wlaczyc mozna w menu", e.var.c_str(), e.from);
				break;
			case zdcore::PME_PATH_GAME_ON:
				ZLog("Sciezki: %s 0 -> %d bez menu (zbroja / szata / rozmowa)", e.var.c_str(), e.to);
				if (e.id == zdcore::PM_VAMP)
					PmFistLater("znacznik wampira z gry");
				break;
			case zdcore::PME_SKILL_REZERO:
				ZLog("Sciezki: wylaczona umiejetnosc %s = %d poza rozmowa (nauka / ksiega / skrypt) - znow 0, poziom zapamietany",
					e.var.c_str(), e.from);
				break;
			default:
				break;
			}
		}
	}

	static void PmWatchStep(float realDt) {
		pmWatchAge += realDt;
		if (pmWatchAge < PM_WATCH_EVERY || !player)
			return;
		pmWatchAge = 0.0f;
		std::vector<zdcore::PmEvent> ev;
		zdcore::PmWatch(pmVars, pmMem, pmDlgRestored, ev);
		PmLogEvents(ev);
	}

	// ---------- strony okna ----------

	static void PmRemember(const zdcore::MenuList& m, int page) {
		if (m.cursor >= 0 && m.cursor < (int)m.rows.size() && page >= 0 && page < 2)
			pmLastRow[pmSkillPath >= 0 && page == 1 ? 2 : page] = m.rows[m.cursor].id;
	}

	void PathMenuEnterPage(int page) {
		pmSkillPath = -1;
		pmGuildSel = -1;
	}

	static void PmBuildGuild(zdcore::MenuList& m) {
		int cur = PmGuild();
		pmGuilds = zdcore::PmGuildChoices(pmVars, cur);
		zdcore::MenuRow r;
		r.id = PMR_GUILD;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Gildia";
		int curIdx = -1;
		for (size_t i = 0; i < pmGuilds.size(); i++) {
			r.values.push_back(PmGuildLabel(pmGuilds[i].guild));
			if (pmGuilds[i].guild == cur)
				curIdx = (int)i;
		}
		if (curIdx < 0) {
			// obecna spoza listy (przemiana, bandyci) - pokazana, nie do wybrania z powrotem
			zdcore::PmGuildChoice c;
			c.guild = cur;
			c.via = "obecna";
			pmGuilds.insert(pmGuilds.begin(), c);
			r.values.insert(r.values.begin(), PmGuildLabel(cur));
			curIdx = 0;
		}
		int sel = pmGuildSel >= 0 && pmGuildSel < (int)pmGuilds.size() ? pmGuildSel : curIdx;
		r.value = sel;
		r.activates = true;
		r.wrap = true;
		int target = pmGuilds[sel].guild;
		r.hint = (target == cur ? std::string("Obecna. ") : std::string("Enter - ustaw do nast\xEApnej zmiany zbroi. ")) +
			zdcore::PmGuildPathsHint(pmVars, target);
		m.rows.push_back(r);

		zdcore::MenuRow t;
		t.id = PMR_GUILD_VIA;
		t.kind = zdcore::MROW_TEXT;
		t.label = "Zdobyta: " + pmGuilds[sel].via;
		m.rows.push_back(t);
		if (pmVars.Count("VALUE_HERO_GUILD")) {
			t = zdcore::MenuRow();
			t.id = PMR_GUILD_BASE;
			t.kind = zdcore::MROW_TEXT;
			t.label = "Po zdj\xEA" "ciu zbroi [G]: " + PmGuildLabel(pmVars.Get("VALUE_HERO_GUILD"));
			m.rows.push_back(t);
		}
		if (pmVars.Get("VALUE_GUILD_HOOK") == zdcore::PM_GUILD_HOOK_MAGIC) {
			t = zdcore::MenuRow();
			t.id = PMR_GUILD_HOOK;
			t.kind = zdcore::MROW_TEXT;
			t.label = "Hak bandyt\xF3w aktywny (zostanie zdj\xEAty)";
			m.rows.push_back(t);
		}
		m.footer = { "Lewo / prawo - wyb\xF3r, Enter - ustaw, Q / E - zak\xB3" "adki, Esc - wr\xF3\xE6" };
	}

	// Lata patch_guildskills (build_union_abi_fix.py, 22.10) w UNION_ABI.DLL NB: wzorzec szybkiego leczenia paladyna
	// po lacie ('cmp [ecx+230h],1' + 6x NOP + 'mov ecx,[...]') szukany raz w sekcjach kodu modulu. Brak modulu albo
	// wzorca = menu pokazuje stan bez laty (umiejetnosci z lata - "wymaga gildii").
	static int pmGuildPatch = -1;
	static bool pmGuildPatchNoMod = false;
	static bool PmGuildPatch() {
		if (pmGuildPatch >= 0)
			return pmGuildPatch == 1;
		HMODULE mod = GetModuleHandleA("UNION_ABI.DLL");
		if (!mod) {
			if (!pmGuildPatchNoMod)
				ZLog("Sciezki: brak modulu UNION_ABI.DLL - stan klas gildii bez laty patch_guildskills");
			pmGuildPatchNoMod = true;
			return false;
		}
		static const BYTE pat[] = { 0x83, 0xB9, 0x30, 0x02, 0x00, 0x00, 0x01, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x8B, 0x0D };
		const BYTE* b = (const BYTE*)mod;
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(b + ((const IMAGE_DOS_HEADER*)b)->e_lfanew);
		const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
		pmGuildPatch = 0;
		for (WORD i = 0; i < nt->FileHeader.NumberOfSections && !pmGuildPatch; i++, sec++) {
			if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) || sec->Misc.VirtualSize < sizeof(pat))
				continue;
			const BYTE* p = b + sec->VirtualAddress;
			for (size_t k = 0, n = sec->Misc.VirtualSize - sizeof(pat); k <= n; k++)
				if (p[k] == pat[0] && !memcmp(p + k, pat, sizeof(pat))) {
					pmGuildPatch = 1;
					break;
				}
		}
		ZLog("Sciezki: lata patch_guildskills w UNION_ABI.DLL (0x%p): %s", (void*)mod, pmGuildPatch ? "jest" : "brak");
		return pmGuildPatch == 1;
	}

	// Stan grupy w menu: sciezka - jak PmPathStateText; klasa gildii - aktywna (wlasna gildia) / aktywna (lata) /
	// czesciowo / inna gildia / nie zdobyta.
	static std::string PmGroupStateText(int id, int guild) {
		if (!zdcore::PmIsGuildClass(id))
			return zdcore::PmPathStateText(zdcore::PmReadPath(pmVars, id).state);
		if (!zdcore::PmGuildClassEarned(pmVars, id, guild))
			return zdcore::PmPathStateText(zdcore::PMS_NONE);
		if (zdcore::PmGuildClassOwn(id, guild))
			return "aktywna";
		bool patch = PmGuildPatch();
		int reach = zdcore::PmGuildClassReach(id, guild, patch);
		if (reach == 2)
			return "aktywna (\xB3" "atka)";
		if (reach == 1)
			return patch ? "cz\xEA\x9C" "ciowo (\xB3" "atka)" : "cz\xEA\x9C" "ciowo";
		return "inna gildia";
	}

	// Dopisek do opisu umiejetnosci klasy gildii: czy dziala w obecnej gildii i dlaczego (mapa E1 / E4).
	static std::string PmGateNote(const zdcore::PmSkillDef& d, int guild) {
		if (!zdcore::PmIsGuildClass(d.path) || d.kind == zdcore::PSK_LOCKED || zdcore::PmGuildClassOwn(d.path, guild))
			return "";
		std::string cls = zdcore::PmGuildClass(d.path).name;
		bool patch = PmGuildPatch();
		switch (zdcore::PmSkillGuildGate(d.idx)) {
		case zdcore::PGG_ANY:
			return "Dzia\xB3" "a w ka\xBF" "dej gildii (DLL nie sprawdza gildii).";
		case zdcore::PGG_TPL:
			return "Dzia\xB3" "a w ka\xBF" "dej gildii (\xB3" "atka patch_tplskills).";
		case zdcore::PGG_PATCH:
			if (d.idx == 243 && patch)          // Spokoj Guru: skryptowa czesc z guildskills_isway.d (22.10)
				return "Dzia\xB3" "a w ka\xBF" "dej gildii (\xB3" "atka patch_guildskills i guildskills_isway.d).";
			return patch ? "Dzia\xB3" "a w ka\xBF" "dej gildii (\xB3" "atka patch_guildskills)." :
				"Teraz nie dzia\xB3" "a: wymaga gildii " + cls + " (brak \xB3" "atki patch_guildskills).";
		case zdcore::PGG_PART:
			return patch ? "Poza gildi\xB9 " + cls + " dzia\xB3" "a cz\xEA\x9C\xE6 z DLL (\xB3" "atka); cz\xEA\x9C\xE6 skryptowa NB tylko przy gildii." :
				"Teraz nie dzia\xB3" "a: wymaga gildii " + cls + ".";
		case zdcore::PGG_SCRIPT:
			return "Teraz nie dzia\xB3" "a: skrypt NB wymaga gildii " + cls + " (bez \xB3" "atki - funkcja walki / co chwil\xEA).";
		}
		return "";
	}

	static std::string PmGateJoin(const std::string& note, const std::string& info) {
		return note.empty() ? info : note + " " + info;
	}

	static void PmBuildPaths(zdcore::MenuList& m) {
		int earned = 0, open = 0;
		for (int p = 0; p < zdcore::PM_PATHS; p++) {
			zdcore::PmPathView v = zdcore::PmReadPath(pmVars, p);
			if (v.state == zdcore::PMS_NONE)
				continue;
			earned++;
			const zdcore::PmPathDef& d = zdcore::PmPath(p);
			zdcore::MenuRow r;
			r.id = PMR_PATH0 + p;
			r.kind = zdcore::MROW_CHOICE;
			r.label = d.name;
			r.values = { zdcore::PmPathStateText(zdcore::PMS_ON),
				zdcore::PmPathStateText(v.state == zdcore::PMS_OFF_GAME ? zdcore::PMS_OFF_GAME : zdcore::PMS_OFF_MENU) };
			r.value = v.state == zdcore::PMS_ON ? 0 : 1;
			r.wrap = true;
			r.activates = true;
			r.hint = std::string(d.hint) + (v.state == zdcore::PMS_OFF_GAME ?
				". Wy\xB3\xB9" "czona przez gr\xEA (przepaska / przyj\xEA" "cie DH lub DK / p\xEAtla szamana)" : "") + ". Enter - umiej\xEAtno\x9C" "ci";
			m.rows.push_back(r);
			if (p == zdcore::PM_VAMP) {
				pmVampTypes = zdcore::PmVampTypes(pmVars);
				int t = pmVars.Get("RX_VAMPIREWAYTYPE");
				zdcore::MenuRow vt;
				vt.id = PMR_VAMPTYPE;
				vt.kind = zdcore::MROW_CHOICE;
				vt.label = "   Typ wampira";
				if (t < 1 || t > 3) {
					vt.values = { "brak (wyb\xF3r u Pana Zamku)" };
					vt.enabled = false;
				}
				else {
					for (size_t i = 0; i < pmVampTypes.size(); i++) {
						vt.values.push_back(zdcore::PmVampTypeName(pmVampTypes[i]));
						if (pmVampTypes[i] == t)
							vt.value = (int)i;
					}
					vt.wrap = true;
					vt.enabled = pmVampTypes.size() > 1;
				}
				vt.hint = "Zestaw umiej\xEAtno\x9C" "ci wampira (zdobyte typy). Zablokowane w trakcie daru krwi Beliara";
				m.rows.push_back(vt);
			}
		}
		// klasy gildii: umiejetnosci gildii glownych (dzialaja przy swojej gildii); Enter = lista z przelacznikami
		int curGuild = PmGuild();
		for (int c = zdcore::PM_PATHS; c < zdcore::PM_GROUPS; c++) {
			const zdcore::PmGuildClassDef& gc = zdcore::PmGuildClass(c);
			bool got = zdcore::PmGuildClassEarned(pmVars, c, curGuild);
			if (c == zdcore::PM_PATHS) {
				zdcore::MenuRow g;
				g.id = PMR_NOPATHS - 2;
				g.kind = zdcore::MROW_TEXT;
				g.label = "- Klasy gildii -";
				m.rows.push_back(g);
			}
			zdcore::MenuRow r;
			r.id = PMR_PATH0 + c;
			r.kind = zdcore::MROW_CHOICE;
			r.label = gc.name;
			r.values = { PmGroupStateText(c, curGuild) };
			r.dim = !got || zdcore::PmGuildClassReach(c, curGuild, PmGuildPatch()) == 0;
			r.activates = true;
			r.hint = std::string(gc.hint) + ". " + gc.howTo + ". Enter - umiej\xEAtno\x9C" "ci";
			m.rows.push_back(r);
		}
		// niezdobyte: szare, bez przelacznika - jak / u kogo zdobyc, blokady nauki; Enter = podglad umiejetnosci
		for (int p = 0; p < zdcore::PM_PATHS; p++) {
			const zdcore::PmPathDef& d = zdcore::PmPath(p);
			if (!zdcore::PmHas(pmVars, d.flag) || zdcore::PmReadPath(pmVars, p).state != zdcore::PMS_NONE)
				continue;
			if (open++ == 0) {
				zdcore::MenuRow g;
				g.id = PMR_NOPATHS - 1;
				g.kind = zdcore::MROW_TEXT;
				g.label = "- Do zdobycia -";
				m.rows.push_back(g);
			}
			zdcore::MenuRow r;
			r.id = PMR_PATH0 + p;
			r.kind = zdcore::MROW_CHOICE;
			r.label = d.name;
			r.values = { zdcore::PmPathStateText(zdcore::PMS_NONE) };
			r.dim = true;
			r.activates = true;
			r.hint = std::string(d.howTo ? d.howTo : "") + ". Enter - podgl\xB9" "d umiej\xEAtno\x9C" "ci";
			m.rows.push_back(r);
		}
		if (!earned && !open) {
			zdcore::MenuRow t;
			t.id = PMR_NOPATHS;
			t.kind = zdcore::MROW_TEXT;
			t.label = "Brak \x9C" "cie\xBF" "ek w skryptach";
			m.rows.push_back(t);
		}
		m.hintLines = 3;
		m.footer = { "Lewo / prawo - w\xB3\xB9" "cz / wy\xB3\xB9" "cz, Enter - umiej\xEAtno\x9C" "ci (szare: do zdobycia - podgl\xB9" "d), Q / E - zak\xB3" "adki, Esc - wr\xF3\xE6" };
	}

	// Koszt w grze: te same funkcje kosztu NB co rozmowa nauki (tryb gry: Arena Koszmaru x0,7, SNC, FullSNC 0).
	// Brak funkcji / stalej w parserze = "" (wtedy koszt bazowy z tabeli). zCParser::CallFunc z dwoma intami.
	static bool PmCallInt2(const char* func, int a, int b, int& out) {
		int idx = parser && func ? parser->GetIndex(zSTRING(func)) : -1;
		if (idx < 0)
			return false;
		void* r = parser->CallFunc(idx, a, b);
		if (!r)
			return false;
		out = *(int*)r;
		return true;
	}

	static std::string PmLiveCost(const zdcore::PmSkillInfo* inf, int from) {
		if (!inf || !inf->lpFunc || !inf->lpConst || !pmVars.Count(inf->lpConst) || from > inf->levels)
			return "";
		bool gold = inf->goldFunc && inf->goldConst && pmVars.Count(inf->goldConst);
		int lc = pmVars.Get(inf->lpConst), gc = gold ? pmVars.Get(inf->goldConst) : 0;
		std::vector<int> lp, gv;
		for (int lvl = std::max(1, from); lvl <= inf->levels; lvl++) {
			int v = 0;
			if (!PmCallInt2(inf->lpFunc, lc, inf->lpLevelArg < 0 ? lvl : inf->lpLevelArg, v))
				return "";
			lp.push_back(v);
			if (gold) {
				int g = 0;
				if (!PmCallInt2(inf->goldFunc, gc, inf->goldLevelArg < 0 ? lvl : inf->goldLevelArg, g))
					return "";
				gv.push_back(g);
			}
		}
		std::string s = zdcore::PmCostText(lp, gv);
		return s.empty() ? "bez kosztu (tryb gry)" : s;
	}

	// Opis umiejetnosci pod lista: efekt, koszt (od poziomu 'from'), wymagania, nauczyciel.
	static std::string PmSkillInfoText(const zdcore::PmSkillInfo* inf, int from, bool withCost) {
		if (!inf)
			return "";
		std::string s = inf->effect && inf->effect[0] ? std::string(inf->effect) + " " : "";
		if (withCost) {
			std::string live = PmLiveCost(inf, from);
			std::string cost = !live.empty() ? live : inf->cost ? inf->cost : "";
			if (!cost.empty())
				s += (from > 1 ? "Koszt od poz. " + std::to_string(from) + ": " : std::string("Koszt: ")) + cost + ". ";
			if (inf->req && inf->req[0])
				s += std::string("Wymaga: ") + inf->req + ". ";
		}
		if (inf->teacher && inf->teacher[0])
			s += std::string("Nauczyciel: ") + inf->teacher + ".";
		return s;
	}

	static void PmBuildSkills(zdcore::MenuList& m, int path) {
		zdcore::MenuRow b;
		b.id = PMR_BACK;
		b.kind = zdcore::MROW_ACTION;
		b.label = "< \x8C" "cie\xBFki";
		b.hint = "Powr\xF3t do listy \x9C" "cie\xBF" "ek (te\xBF Esc / PPM)";
		m.rows.push_back(b);
		const auto& all = zdcore::PmSkills();
		int shown = 0, groups = 0, learnedN = 0;
		int guild = PmGuild();
		for (size_t i = 0; i < all.size(); i++) {
			const zdcore::PmSkillDef& d = all[i];
			if (d.path != path)
				continue;
			if (d.kind == zdcore::PSK_GROUP) {
				bool has = false;
				for (size_t j = i + 1; j < all.size() && all[j].kind != zdcore::PSK_GROUP && all[j].path == path; j++)
					has = true;
				if (!has)
					continue;
				zdcore::MenuRow g;
				g.id = PMR_GROUP0 + groups++;
				g.kind = zdcore::MROW_TEXT;
				g.label = std::string("- ") + d.name + " -";
				m.rows.push_back(g);
				continue;
			}
			zdcore::PmSkillView s = zdcore::PmReadSkill(pmVars, d);
			const zdcore::PmSkillInfo* inf = zdcore::PmSkillInfoFor(d.idx);
			int maxL = inf ? std::max(1, inf->levels) : 1;
			int level = std::max(s.value, s.keep);
			shown++;
			zdcore::MenuRow r;
			r.id = PMR_SKILL0 + d.idx;
			r.kind = zdcore::MROW_CHOICE;
			std::string lv = maxL > 1 ? " (" + std::to_string(std::min(level, maxL)) + "/" + std::to_string(maxL) + ")" :
				level > 1 ? " (" + std::to_string(level) + ")" : "";
			r.label = std::string("   ") + d.name + lv;
			if (!s.learned) {
				// podglad: niewyuczona - szara, wybieralna (opis pod lista), bez przelacznika
				r.values = { "nie wyuczona" };
				r.dim = true;
				r.hint = PmGateJoin(PmGateNote(d, guild), PmSkillInfoText(inf, 1, true));
			}
			else if (d.kind == zdcore::PSK_LOCKED) {
				learnedN++;
				r.values = { "trwa\xB3" "a" };
				r.dim = true;
				r.hint = PmSkillInfoText(inf, level + 1, level < maxL) + " Trwa\xB3" "a premia naliczona przy nauce - bez prze\xB3\xB9" "czania.";
			}
			else {
				learnedN++;
				r.values = { "W\xB3.", "Wy\xB3." };
				r.value = s.off ? 1 : 0;
				r.wrap = true;
				std::string extra;
				if (d.guard == zdcore::PG_DK_OFF)
					extra = " Tylko przy wy\xB3\xB9" "czonej \x9C" "cie\xBF" "ce Mrocznego rycerza.";
				else if (d.guard == zdcore::PG_FAMILIAR)
					extra = " Tylko przy odwo\xB3" "anym chowa\xF1" "cu.";
				if (d.flags & zdcore::PSF_DIALOG_TEACHER)
					extra += " Dzia\xB3" "a w rozmowach - przywracana tylko u nauczyciela.";
				r.hint = PmGateJoin(PmGateNote(d, guild), PmSkillInfoText(inf, level + 1, level < maxL)) + " Wy\xB3\xB9" "czenie: " +
					d.var + " = 0, poziom zapami\xEAtany; w rozmowach wraca." + extra;
			}
			m.rows.push_back(r);
		}
		if (!shown) {
			zdcore::MenuRow t;
			t.id = PMR_NOSKILLS;
			t.kind = zdcore::MROW_TEXT;
			t.label = "Brak umiej\xEAtno\x9C" "ci tej \x9C" "cie\xBFki w tabeli";
			m.rows.push_back(t);
		}
		m.hintLines = zdcore::PmIsGuildClass(path) ? 5 : 4;
		m.footer = { "Lewo / prawo / LPM - w\xB3\xB9" "cz / wy\xB3\xB9" "cz; szare: niewyuczone (opis pod list\xB9) i trwa\xB3" "e; Esc / PPM - wr\xF3\xE6" };
		ZDebug("Sciezki: strona umiejetnosci %s - %d pozycji, wyuczonych %d", zdcore::PmGroupLogName(path), shown, learnedN);
	}

	void PathMenuBuild(zdcore::MenuList& m, int page) {
		int keepId = -1;
		if (m.cursor >= 0 && m.cursor < (int)m.rows.size())
			keepId = m.rows[m.cursor].id;
		m.rows.clear();
		m.first = 0;
		m.hintLines = 2;
		if (!PathMenuAvailable()) {
			zdcore::MenuRow t;
			t.kind = zdcore::MROW_TEXT;
			t.label = "Brak skrypt\xF3w New Balance";
			m.rows.push_back(t);
			m.cursor = -1;
			return;
		}
		if (page == 0)
			PmBuildGuild(m);
		else if (pmSkillPath >= 0)
			PmBuildSkills(m, pmSkillPath);
		else
			PmBuildPaths(m);
		if (zcfg.menuMouse)
			m.footer.push_back("Mysz: LPM - wybierz / Enter, k\xF3\xB3ko - zmie\xF1, PPM - wr\xF3\xE6");
		int want = pmLastRow[page == 0 ? 0 : pmSkillPath >= 0 ? 2 : 1];
		if (keepId >= 0 && zdcore::MenuFindRow(m, keepId) >= 0)
			want = keepId;
		if (!zdcore::MenuSelectRow(m, want)) {
			m.cursor = 0;
			zdcore::MenuFixCursor(m);
		}
		PathMenuHeader(m, page);
	}

	void PathMenuHeader(zdcore::MenuList& m, int page) {
		m.header.clear();
		if (!PathMenuAvailable())
			return;
		zdcore::PmWorld w = PmReadWorld(false);
		std::string g = "Gildia: " + PmGuildLabel(w.guild);
		if (page == 1 && pmSkillPath >= 0)
			m.header.push_back(std::string(zdcore::PmGroupName(pmSkillPath)) + ": " + PmGroupStateText(pmSkillPath, w.guild));
		m.header.push_back(g);
		if (!pmMsg.empty() && ZClockMs() < pmMsgUntil)
			m.header.push_back(pmMsg);
		else {
			std::string b = PmBlockersText(w);
			if (!b.empty())
				m.header.push_back("Blokada: " + b);
		}
	}

	static void PmRefuse(const char* what, zdcore::PmDeny d) {
		if (d == zdcore::PMD_SAME)
			return;
		ZLog("Sciezki: odmowa - %s: %s", what, zdcore::PmDenyText(d));
		PmTell(zdcore::PmDenyScreen(d));
	}

	static void PmApplyGuild(int sel) {
		if (sel < 0 || sel >= (int)pmGuilds.size())
			return;
		int target = pmGuilds[sel].guild;
		zdcore::PmWorld w = PmReadWorld(true);
		bool earned = pmGuilds[sel].via != "obecna" || target == w.guild;
		char what[96];
		sprintf_s(what, "gildia %d -> %d", w.guild, target);
		zdcore::PmDeny d = zdcore::PmGuildDeny(w, pmVars, target, earned);
		if (d != zdcore::PMD_OK) {
			PmRefuse(what, d);
			return;
		}
		zdcore::PmGuildPlan p = zdcore::PmGuildApply(pmVars, target);
		int old = player->guild;
		player->guild = target;
		ZLog("Sciezki: gildia %d -> %d (do nastepnej zmiany zbroi [G]; VALUE_HERO_GUILD %d bez zmian%s); slad: %s; %s", old, player->guild,
			pmVars.Get("VALUE_HERO_GUILD"), p.hookUndone ? (", hak bandytow zdjety, VALUE_TEMP_HERO_GUILD " +
			std::to_string(p.hookTempGuild)).c_str() : "", pmGuilds[sel].via.c_str(), zdcore::PmSummary(pmVars, target).c_str());
		std::string msg = "Gildia: " + PmGuildLabel(target);
		PmTell(msg.c_str());
		PmFistLater("zmiana gildii");
	}

	static void PmApplyPath(int p, bool on) {
		zdcore::PmWorld w = PmReadWorld(true);
		zdcore::PmPathView before = zdcore::PmReadPath(pmVars, p);
		zdcore::PmResult r = zdcore::PmSetPath(pmVars, w, p, on);
		char what[96];
		sprintf_s(what, "%s %s", zdcore::PmPath(p).flag, on ? "wlaczenie" : "wylaczenie");
		if (r.deny != zdcore::PMD_OK) {
			PmRefuse(what, r.deny);
			return;
		}
		std::string notes;
		for (auto& n : r.notes)
			notes += "; " + n;
		ZLog("Sciezki: %s - %s -> %s (gildia %d; %s)%s", zdcore::PmPath(p).flag, zdcore::PmPathStateLog(before.state),
			zdcore::PmPathStateLog(zdcore::PmReadPath(pmVars, p).state), w.guild, zdcore::PmChangesText(r).c_str(), notes.c_str());
		std::string msg = std::string(zdcore::PmPath(p).name) + ": " + (on ? "w\xB3\xB9" "czona" : "wy\xB3\xB9" "czona");
		PmTell(msg.c_str());
		if (p == zdcore::PM_VAMP)
			PmFistLater("sciezka wampira");
	}

	static void PmApplyVampType(int idx) {
		if (idx < 0 || idx >= (int)pmVampTypes.size())
			return;
		int t = pmVampTypes[idx];
		zdcore::PmWorld w = PmReadWorld(true);
		int old = pmVars.Get("RX_VAMPIREWAYTYPE");
		zdcore::PmResult r = zdcore::PmSetVampType(pmVars, w, t);
		char what[64];
		sprintf_s(what, "typ wampira %d -> %d", old, t);
		if (r.deny != zdcore::PMD_OK) {
			PmRefuse(what, r.deny);
			return;
		}
		ZLog("Sciezki: typ wampira %d -> %d (%s)", old, t, zdcore::PmChangesText(r).c_str());
		std::string msg = std::string("Typ wampira: ") + zdcore::PmVampTypeName(t);
		PmTell(msg.c_str());
		PmFistLater("typ wampira");
	}

	static void PmApplySkill(int idx, bool on) {
		const zdcore::PmSkillDef* d = zdcore::PmSkillByIdx(idx);
		if (!d)
			return;
		zdcore::PmWorld w = PmReadWorld(true);
		zdcore::PmResult r = zdcore::PmSetSkill(pmVars, w, *d, on);
		char what[128];
		sprintf_s(what, "umiejetnosc %s %s", d->var, on ? "wlaczenie" : "wylaczenie");
		if (r.deny != zdcore::PMD_OK) {
			PmRefuse(what, r.deny);
			return;
		}
		ZLog("Sciezki: umiejetnosc %s (%s) %s (%s)", d->var, zdcore::PmGroupLogName(d->path), on ? "wlaczona" : "wylaczona",
			zdcore::PmChangesText(r).c_str());
		std::string msg = std::string(d->name) + ": " + (on ? "w\xB3\xB9" "czona" : "wy\xB3\xB9" "czona");
		PmTell(msg.c_str());
		if (d->flags & zdcore::PSF_FISTANIM)
			PmFistLater("Styl walki Potepionych");
	}

	bool PathMenuEvent(zdcore::MenuList& m, int page, const zdcore::MenuEvent& e) {
		if (e.type == zdcore::MEV_CLOSE) {
			if (page == 1 && pmSkillPath >= 0) {
				int back = PMR_PATH0 + pmSkillPath;
				pmSkillPath = -1;
				pmLastRow[1] = back;
				PathMenuBuild(m, page);
				return false;
			}
			return true;
		}
		if (e.type == zdcore::MEV_MOVED) {
			PmRemember(m, page);
			return false;
		}
		bool rebuild = false;
		if (page == 0) {
			if (e.rowId == PMR_GUILD && e.type == zdcore::MEV_CHANGED) {
				pmGuildSel = e.value;
				rebuild = true;
			}
			else if (e.rowId == PMR_GUILD && e.type == zdcore::MEV_ACTIVATE) {
				PmApplyGuild(e.value);
				pmGuildSel = -1;
				rebuild = true;
			}
		}
		else if (pmSkillPath < 0) {
			if (e.rowId >= PMR_PATH0 && e.rowId < PMR_PATH0 + zdcore::PM_GROUPS) {
				int p = e.rowId - PMR_PATH0;
				if (e.type == zdcore::MEV_CHANGED && p < zdcore::PM_PATHS)
					PmApplyPath(p, e.value == 0);
				else if (e.type == zdcore::MEV_ACTIVATE) {
					pmLastRow[1] = e.rowId;
					pmSkillPath = p;
					pmLastRow[2] = PMR_BACK;
				}
				rebuild = true;
			}
			else if (e.rowId == PMR_VAMPTYPE && e.type == zdcore::MEV_CHANGED) {
				PmApplyVampType(e.value);
				rebuild = true;
			}
		}
		else {
			if (e.rowId == PMR_BACK && e.type == zdcore::MEV_ACTIVATE) {
				pmLastRow[1] = PMR_PATH0 + pmSkillPath;
				pmSkillPath = -1;
				rebuild = true;
			}
			else if (e.rowId >= PMR_SKILL0 && e.rowId < PMR_SKILL0 + zdcore::PM_SKILL_SLOTS && e.type == zdcore::MEV_CHANGED) {
				PmApplySkill(e.rowId - PMR_SKILL0, e.value == 0);
				rebuild = true;
			}
		}
		if (rebuild) {
			PathMenuBuild(m, page);
			PmRemember(m, page);
		}
		return false;
	}

	// ---------- okno wlasne ----------

	static bool PmModsHeld(int mods) {
		if (!zinput)
			return false;
		auto down = [](int a, int b) { return zinput->KeyPressed(a) || zinput->KeyPressed(b); };
		if ((mods & 1) && !down(KEY_LCONTROL, KEY_RCONTROL))
			return false;
		if ((mods & 2) && !down(KEY_LSHIFT, KEY_RSHIFT))
			return false;
		if ((mods & 4) && !down(KEY_LMENU, KEY_RMENU))
			return false;
		return true;
	}

	static ZMenuLayout PmLayout() {
		ZMenuLayout l;
		l.x = 4500;
		l.y = 1000;
		l.w = 3400;
		l.h = 5600;
		return l;
	}

	static void PmMenuBuild() {
		zdcore::MenuList m;
		m.title = "Gildia i \x9C" "cie\xBFki";
		m.tabs = { "Gildia", "\x8C" "cie\xBFki" };
		m.tab = std::clamp(pmTab, 0, 1);
		pmMenu = m;
		PathMenuBuild(pmMenu, pmMenu.tab);
	}

	static const char* PmOpenDeny() {
		if (!player || !ogame || !ogame->GetGameWorld())
			return "brak bohatera";
		if (player->IsDead() || player->IsUnconscious() || !player->GetModel())
			return "bohater martwy / nieprzytomny";
		if (MeditateActive())
			return "medytacja";
		if (player->inventory2.IsOpen())
			return "ekwipunek";
		if (!oCInformationManager::GetInformationManager().HasFinished())
			return "rozmowa";
		oCDocumentManager& dm = oCDocumentManager::GetDocumentManager();
		if (dm.ListDocuments && dm.ListDocuments->next)
			return "dokument / mapa";
		if (MovesActive() || TraversalActive() || DirectorActive() || LootActive())
			return "ruch pluginu (unik, przeskok, finisher, podnoszenie)";
		if (player->IsMovLock())
			return "blokada sterowania (skrypt / inna wtyczka)";
		float d = 0.0f;
		if (FindFightingFoe(PM_ENEMY_RADIUS, nullptr, d))
			return "walka";
		return nullptr;
	}

	static void PmClose(const char* why) {
		if (!pmOpen)
			return;
		pmOpen = false;
		if (pmMovLock && player)
			player->SetMovLock(FALSE);
		pmMovLock = false;
		ZMenuCapture(false, "sciezki");
		ZLog("Sciezki: okno zamkniete (%s)", why);
	}

	static void PmOpenNow(const char* src) {
		const char* deny = PmOpenDeny();
		if (deny) {
			ZLog("Sciezki: okno - odmowa (%s): %s", src, deny);
			if (!strcmp(deny, "walka"))
				PmTell("Wrogowie w pobli\xBFu!");
			return;
		}
		pmOpen = true;
		PathMenuEnterPage(pmTab);
		PmMenuBuild();
		ZMenuCapture(true, "sciezki");
		if (!player->IsMovLock()) {
			player->SetMovLock(TRUE);
			pmMovLock = true;
		}
		pmHpPrev = player->attribute[NPC_ATR_HITPOINTS];
		pmEnemyAge = 0.0f;
		ZLog("Sciezki: okno otwarte (%s) - %s", src, zdcore::PmSummary(pmVars, PmGuild()).c_str());
	}

	bool PathMenuKeyEvent(unsigned short dik) {
		if (!zcfg.enabled || !zcfg.pathMenu || zcfg.pathMenuKey <= 0 || dik != zcfg.pathMenuKey || !PmModsHeld(zcfg.pathMenuKeyMods))
			return false;
		bool inGame = pmLoopTick && GetTickCount() - pmLoopTick <= PM_HOOK_STALE_MS && player && ogame && ogame->GetGameWorld();
		if (!inGame || (zcon && zcon->IsVisible()) || !PathMenuAvailable())
			return false;   // Steam / Kroniki / menu gry - klawisz dla gry
		pmPending = true;
		return true;
	}

	bool PathMenuOpen() {
		return pmOpen;
	}

	static void PmWindowUpdate(float realDt) {
		const char* why = nullptr;
		if (!player || !ogame || !ogame->GetGameWorld())
			why = "brak bohatera / swiata";
		else if (player->IsDead() || player->IsUnconscious())
			why = "bohater martwy / nieprzytomny";
		else if (!oCInformationManager::GetInformationManager().HasFinished())
			why = "rozmowa";
		else if (pmHpPrev >= 0 && player->attribute[NPC_ATR_HITPOINTS] < pmHpPrev)
			why = "trafienie";
		else {
			pmEnemyAge += realDt;
			if (pmEnemyAge >= PM_ENEMY_RECHECK) {
				pmEnemyAge = 0.0f;
				float d = 0.0f;
				if (FindFightingFoe(PM_ENEMY_RADIUS, nullptr, d))
					why = "walczacy wrog w poblizu";
			}
		}
		if (why) {
			if (!strcmp(why, "walczacy wrog w poblizu") || !strcmp(why, "trafienie"))
				PmTell("Wrogowie w pobli\xBFu!");
			PmClose(why);
			return;
		}
		pmHpPrev = player->attribute[NPC_ATR_HITPOINTS];
		// zdarzenie okna (klawisz albo mysz); true = okno zamkniete
		auto handle = [](zdcore::MenuEvent e, const char* closeWhy) -> bool {
			if (e.type == zdcore::MEV_TAB) {
				pmTab = e.value;
				PathMenuEnterPage(pmTab);
				PathMenuBuild(pmMenu, pmTab);
				return false;
			}
			if (PathMenuEvent(pmMenu, pmMenu.tab, e)) {
				PmClose(closeWhy);
				ZMenuClearKeys();
				return true;
			}
			return false;
		};
		zdcore::MenuKey k;
		int dik = 0;
		while (ZMenuNextKey(k, dik))
			if (handle(zdcore::MenuHandleKey(pmMenu, k), "Esc"))
				return;
		zdcore::MenuMouseKind mk;
		int wx = 0, wy = 0;
		while (ZMenuNextMouse(mk, wx, wy)) {
			zdcore::MenuMouseResult r = zdcore::MenuMouse(pmMenu, ZMenuHitMap(), mk, wx, wy);
			if (r.back) {
				zdcore::MenuEvent esc;
				esc.type = zdcore::MEV_CLOSE;   // podlista umiejetnosci - powrot, inaczej zamkniecie okna
				if (handle(esc, "PPM"))
					return;
			}
			else if (r.ev.type != zdcore::MEV_NONE && handle(r.ev, "mysz"))
				return;
		}
		ZMenuMouse(true, false);   // kursor; PPM bez obrotu kamery (bohater stoi, kamera gry)
		PathMenuHeader(pmMenu, pmMenu.tab);
	}

	void PathMenuUpdate() {
		pmLoopTick = GetTickCount();
		double clock = ZClockMs();
		float realDt = pmClockPrev > 0.0 ? (float)std::clamp((clock - pmClockPrev) / 1000.0, 0.0, 0.1) : 0.0f;
		pmClockPrev = clock;
		if (!zcfg.enabled || !zcfg.pathMenu || !player || !ogame || !ogame->GetGameWorld()) {
			PathMenuFinishNow(!zcfg.enabled || !zcfg.pathMenu ? "wylaczone" : "brak bohatera / swiata");
			return;
		}
		if (!PathMenuAvailable())
			return;
		// ochrona rozmow: start bez haka (inny hak w lancuchu) i koniec rozmowy
		bool dlg = !oCInformationManager::GetInformationManager().HasFinished();
		if (dlg && !pmDlgRestored && zcfg.pathDialogGuard)
			PmDlgBegin(oCInformationManager::GetInformationManager().Npc, "petla");
		else if (!dlg && pmDlgRestored)
			PmDlgEnd("koniec rozmowy");
		PmWatchStep(realDt);
		PmFistSync();
		if (pmConsoleWait > 0.0f) {
			pmConsoleWait -= realDt;
			if (!(zcon && zcon->IsVisible())) {
				pmConsoleWait = 0.0f;
				if (!pmOpen)
					PmOpenNow("konsola");
			}
		}
		if (pmPending) {
			pmPending = false;
			if (!pmOpen)
				PmOpenNow("klawisz");
		}
		if (pmOpen)
			PmWindowUpdate(realDt);
	}

	void PathMenuDraw() {
		if (pmOpen && player && ogame)
			ZMenuDraw(pmMenu, PmLayout());
	}

	void PathMenuFinishNow(const char* why) {
		pmPending = false;
		pmConsoleWait = 0.0f;
		PmClose(why);
		PmDlgEnd(why);
	}

	void PathMenuLoadBegin() {
		PathMenuFinishNow("wczytanie / zmiana swiata");
		pmMem = zdcore::PmWatchMem();
		pmFistPending = false;
	}

	void PathMenuLoadEnd() {
		pmSyms.clear();
		pmAvail = -1;
		pmMem = zdcore::PmWatchMem();
		pmWatchAge = 1e9f;
		if (PathMenuAvailable())
			ZLog("Sciezki: po wczytaniu - %s", zdcore::PmSummary(pmVars, PmGuild()).c_str());
	}

	std::string PathMenuConsole(const std::string& arg) {
		std::string a = zdcore::Upper(arg);
		if (!player || !ogame || !ogame->GetGameWorld())
			return "brak bohatera";
		if (!PathMenuAvailable())
			return "menu sciezek niedostepne: " + (zcfg.pathMenu ? "brak " + pmMissing : std::string("PathMenu=0"));
		if (a == "SKILLS") {
			std::string s = "umiejetnosci (wyuczone):";
			for (const zdcore::PmSkillDef& d : zdcore::PmSkills()) {
				if (d.kind == zdcore::PSK_GROUP)
					continue;
				zdcore::PmSkillView v = zdcore::PmReadSkill(pmVars, d);
				if (!v.learned)
					continue;
				char buf[128];
				sprintf_s(buf, " %s=%d%s%s;", d.var, v.value, v.off ? (" WYL(keep " + std::to_string(v.keep) + ")").c_str() : "",
					d.kind == zdcore::PSK_LOCKED ? " trwala" : "");
				s += buf;
			}
			ZLog("Sciezki (konsola): %s", s.c_str());
			return s;
		}
		if (a == "OPEN") {
			pmConsoleWait = PM_CONSOLE_WAIT;
			return "okno otworzy sie po zamknieciu konsoli";
		}
		zdcore::PmWorld w = PmReadWorld(false);
		std::string blk = PmBlockersText(w);
		std::string s = zdcore::PmSummary(pmVars, w.guild) + "; okno " + (pmOpen ? "otwarte" : "zamkniete") + ", klawisz " +
			std::to_string(zcfg.pathMenuKey) + " (mods " + std::to_string(zcfg.pathMenuKeyMods) + "), ochrona rozmow " +
			(zcfg.pathDialogGuard ? (pmDlgRestored ? "AKTYWNA (" + pmDlgNpc + ")" : std::string("wl.")) : std::string("wyl.")) +
			", VALUE_HERO_GUILD " + std::to_string(pmVars.Get("VALUE_HERO_GUILD")) + (blk.empty() ? "" : ", blokady: " + blk);
		ZLog("Sciezki (konsola): %s", s.c_str());
		return s;
	}
}
