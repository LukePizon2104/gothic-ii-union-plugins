// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Diagnostyka hakow - opis w HookDiag.h. Adresy G2A (Gothic2.exe 2.6, jak w hakach modulow).
	unsigned zdHookCalls[ZH_COUNT] = {};

	struct HookDiagEntry {
		const char* name;
		DWORD addr;
	};
	static const HookDiagEntry HOOK_DIAG[ZH_COUNT] = {
		{ "OnDamage", 0x006660E0 },
		{ "OnDamage_Hit", 0x00666610 },
		{ "DoDie", 0x00736760 },
		{ "DropUnconscious", 0x00735EB0 },
		{ "EV_AttackFinish", 0x00751AF0 },
		{ "AdvanceAnis", 0x0057CA90 },
		{ "DoAIState", 0x0076D1A0 },
		{ "InitAllAnis", 0x006A5BF0 },
		{ "InitFightAnis", 0x006AA0B0 },
		{ "HitCombo", 0x006B0260 },
		{ "StartParadeEffects", 0x006B16F0 },
		{ "EV_Parade", 0x007522D0 },
		{ "FightMelee", 0x00696EC0 },
		{ "Npc.ApplyOverlay", 0x0072D2C0 },
		{ "Npc.RemoveOverlay", 0x0072D5C0 },
		{ "EV_AttackRun", 0x007517D0 },
		{ "EV_AttackForward", 0x0074F400 },
		{ "EV_AttackLeft", 0x00750160 },
		{ "EV_AttackRight", 0x00750D00 },
		{ "EV_DamagePerFrame", 0x006649A0 },
		{ "Spell.InvestedMana", 0x00485D30 },
		{ "Spell.SetReleaseStatus", 0x00486670 },
		{ "Spell.Cast", 0x00485360 },
		{ "CheckPhysics", 0x0050DC40 },
		{ "EndMovement", 0x0061E0D0 },
		{ "AniCtrl._Stand", 0x006B7490 },
		{ "IsInFightRange", 0x0067CB60 },
		{ "Input.GetState", 0x004D4D30 },
		{ "Input.GetToggled", 0x004D5020 },
		{ "Console.Evaluate", 0x00784860 },
		{ "Input.ProcessInputEvents", 0x004D5700 },   // ZMenu.cpp: klawiatura okien pluginu (medytacja)
		{ "Cam.CheckKeys", 0x004A45C0 },              // Meditate.cpp: kamera orbitalna (azymut / elewacja z myszy)
		{ "Cam.AI_Normal", 0x004A4370 },              // Meditate.cpp: frameTime kamery w czekaniu
		{ "Info.SetNpc", 0x006609F0 },                // PathMenu.cpp: start rozmowy (ochrona rozmow - sciezki NB)
		{ "World.AdvanceClock", 0x006260E0 },         // Wheel.cpp: tempo czasu kola / celowania z kuszy (ulamki klatek)
	};
	// Funkcje haczone przez NB, ktorych my nie haczymy - tylko do logu wejscia (kto jest w lancuchu).
	static const HookDiagEntry HOOK_DIAG_OTHER[] = {
		{ "oCSpell::Invest", 0x004850D0 },
		{ "oCVisualFX::SetByScript", 0x0048D4B0 },
	};
	static const DWORD HOOK_DIAG_CALL_MAGICMODE = 0x0069A0CA;   // Signs.cpp: call oCAIHuman::MagicMode (ruch w podtrzymaniu)

	static const float HOOK_REPORT_MARKS[] = { 15.0f, 60.0f, 180.0f, 600.0f };
	static const int HOOK_REPORT_COUNT = (int)(sizeof(HOOK_REPORT_MARKS) / sizeof(HOOK_REPORT_MARKS[0]));
	static ULONGLONG hookDiagStart = 0;
	static int hookReportsDone = 0;

	static std::string HdModule(DWORD a) {
		HMODULE h = nullptr;
		char path[MAX_PATH] = "?";
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &h) || !h) {
			char buf[48];
			sprintf_s(buf, "0x%08X (poza modulem - trampolina/stub)", (unsigned)a);
			return buf;
		}
		GetModuleFileNameA(h, path, MAX_PATH);
		const char* base = strrchr(path, '\\');
		base = base ? base + 1 : path;
		char buf[MAX_PATH + 32];
		if (h == GetModuleHandleA(nullptr))
			sprintf_s(buf, "%s 0x%08X", base, (unsigned)a);
		else
			sprintf_s(buf, "%s+0x%X", base, (unsigned)(a - (DWORD)(uintptr_t)h));
		return buf;
	}

	static bool HdRead(DWORD a, void* out, size_t n) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
			return false;
		if ((uintptr_t)a + n > (uintptr_t)mbi.BaseAddress + mbi.RegionSize)
			return false;
		memcpy(out, (const void*)a, n);
		return true;
	}

	// Wejscie funkcji: kolejne skoki (do 4) z modulem kazdego celu. Pierwszy cel = ostatni (najwyzszy) hak w lancuchu.
	static std::string HdEntryChain(DWORD addr) {
		std::string s;
		DWORD a = addr;
		for (int hop = 0; hop < 4; hop++) {
			uint8_t b[8];
			if (!HdRead(a, b, sizeof(b)))
				return s + (s.empty() ? "" : " -> ") + "(brak odczytu)";
			uint32_t target = 0, slot = 0;
			int k = zdcore::DecodeEntryJump(b, (uint32_t)a, target, slot);
			if (k == zdcore::HOOKJMP_INDIRECT && !HdRead((DWORD)slot, &target, 4))
				return s + " -> [FF25 nieczytelne]";
			if (k == zdcore::HOOKJMP_NONE)
				return hop == 0 ? "bez skoku na wejsciu (nikt nie haczy albo hak poza wejsciem)" : s;
			s += (s.empty() ? "" : " -> ") + HdModule((DWORD)target);
			a = (DWORD)target;
		}
		return s;
	}

	// ---------- HookGuard: haki Ikarusa/LeGo zalozone przez skrypty moda PO naszych hakach Union ----------
	// Kroniki Myrtany (INIT_FEATURES_GAMESTART): HookEngineF na wejsciu oCNpc::OnDamage i oCNpc::EV_AttackFinish, ktore
	// haczymy. HookEngine kopiuje 7 bajtow wejscia surowo, czyli nasz skok E9 rel32 - w stubie skacze on w przypadkowe
	// miejsce (zdcore::PlanIkarusStubFix). Cel skoku na wejsciu z Game_Init (zanim ruszyly skrypty) = wzorzec; zmiana
	// w czasie gry -> szukamy stubu i przeliczamy skopiowany skok. Sprawdzenie przy wczytaniu swiata i co 1 s.
	static uint32_t hdEntryInit[ZH_COUNT];
	static uint32_t hdEntrySeen[ZH_COUNT];
	static ULONGLONG hdGuardNext = 0;

	static bool HdEntryTarget(DWORD addr, uint32_t& target) {
		uint8_t b[8];
		uint32_t slot = 0;
		if (!HdRead(addr, b, sizeof(b)))
			return false;
		int k = zdcore::DecodeEntryJump(b, (uint32_t)addr, target, slot);
		if (k == zdcore::HOOKJMP_INDIRECT)
			return HdRead((DWORD)slot, &target, 4);
		return k == zdcore::HOOKJMP_REL;
	}

	// Odczyt do konca regionu pamieci (stub Ikarusa lezy na stercie, moze konczyc sie przy granicy regionu).
	static uint32_t HdReadUpTo(DWORD a, uint8_t* out, uint32_t n) {
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
			return 0;
		uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
		uint32_t got = (uintptr_t)a + n > end ? (uint32_t)(end - (uintptr_t)a) : n;
		memcpy(out, (const void*)a, got);
		return got;
	}

	static bool HdPatchRel(DWORD at, int32_t rel) {
		DWORD old = 0;
		if (!VirtualProtect((LPVOID)at, 4, PAGE_EXECUTE_READWRITE, &old))
			return false;
		memcpy((void*)at, &rel, 4);
		VirtualProtect((LPVOID)at, 4, old, &old);
		FlushInstructionCache(GetCurrentProcess(), (LPCVOID)at, 4);
		return true;
	}

	void HookGuardInit() {
		for (int i = 0; i < ZH_COUNT; i++) {
			uint32_t t = 0;
			hdEntryInit[i] = HdEntryTarget(HOOK_DIAG[i].addr, t) ? t : 0;
			hdEntrySeen[i] = hdEntryInit[i];
		}
		hdGuardNext = 0;
	}

	void HookGuardCheck(const char* when) {
		for (int i = 0; i < ZH_COUNT; i++) {
			uint32_t expected = hdEntryInit[i];
			uint32_t now = 0;
			if (!expected || !HdEntryTarget(HOOK_DIAG[i].addr, now) || now == hdEntrySeen[i])
				continue;
			hdEntrySeen[i] = now;
			if (now == expected)
				continue;
			uint8_t stub[256];
			uint32_t n = HdReadUpTo((DWORD)now, stub, sizeof(stub));
			zdcore::IkarusStubPlan p = zdcore::PlanIkarusStubFix(stub, n, now, (uint32_t)HOOK_DIAG[i].addr, expected);
			bool ok = p.kind != zdcore::IKSTUB_PATCH || HdPatchRel((DWORD)(now + p.copyOffset + 1), p.newRel);
			ZLog("Haki [%s]: wejscie %s 0x%08X przepisane (teraz -> %s, przy starcie -> %s): %s%s", when, HOOK_DIAG[i].name,
				(unsigned)HOOK_DIAG[i].addr, HdModule((DWORD)now).c_str(), HdModule((DWORD)expected).c_str(),
				zdcore::IkarusStubKindName(p.kind), ok ? "" : " - BLAD zapisu (VirtualProtect), grozi crash");
			if (p.kind == zdcore::IKSTUB_PATCH && ok)
				ZLog("Haki:   %s: stub 0x%08X, %u bajtow wejscia od +%u, skok 0x%08X -> 0x%08X", HOOK_DIAG[i].name, (unsigned)now,
					p.oldInstr, p.copyOffset, p.jumpsTo, expected);
			ZLogFlush();
		}
	}

	void HookGuardUpdate() {
		ULONGLONG t = GetTickCount64();
		if (t < hdGuardNext)
			return;
		hdGuardNext = t + 1000;
		HookGuardCheck("gra");
	}

	void HookDiagInit() {
		hookDiagStart = GetTickCount64();
		hookReportsDone = 0;
		if (!zcfg.debug)
			return;
		ZLog("Haki: wejscia funkcji (pierwszy cel skoku = najwyzszy hak lancucha; nasz DLL nizej wola oryginal przez lancuch):");
		for (int i = 0; i < ZH_COUNT; i++)
			ZLog("Haki:   %-22s 0x%08X: %s", HOOK_DIAG[i].name, (unsigned)HOOK_DIAG[i].addr, HdEntryChain(HOOK_DIAG[i].addr).c_str());
		for (auto& e : HOOK_DIAG_OTHER)
			ZLog("Haki:   %-22s 0x%08X: %s (nie nasz)", e.name, (unsigned)e.addr, HdEntryChain(e.addr).c_str());
		uint8_t b[5];
		uint32_t t = 0;
		if (HdRead(HOOK_DIAG_CALL_MAGICMODE, b, 5) && zdcore::DecodeCallSite(b, HOOK_DIAG_CALL_MAGICMODE, t))
			ZLog("Haki:   call MagicMode 0x%08X -> %s", (unsigned)HOOK_DIAG_CALL_MAGICMODE, HdModule((DWORD)t).c_str());
		else
			ZLog("Haki:   call MagicMode 0x%08X: brak E8 (ktos przepisal miejsce wywolania)", (unsigned)HOOK_DIAG_CALL_MAGICMODE);
	}

	static std::string HdCountsLine() {
		const char* names[ZH_COUNT];
		for (int i = 0; i < ZH_COUNT; i++)
			names[i] = HOOK_DIAG[i].name;
		return zdcore::HookCountsLine(names, zdHookCalls, ZH_COUNT);
	}

	void HookDiagUpdate() {
		if (!zcfg.debug || !hookDiagStart)
			return;
		float elapsed = (float)(GetTickCount64() - hookDiagStart) / 1000.0f;
		if (!zdcore::HookReportDue(elapsed, hookReportsDone, HOOK_REPORT_MARKS, HOOK_REPORT_COUNT))
			return;
		ZLog("Haki [%.0f s od startu]: wywolania naszych hakow: %s (0 przy funkcji, ktora juz musiala sie wykonac - np. "
			"OnDamage po walce, InitAllAnis po wczytaniu - znaczy, ze inny hak w lancuchu nie wola oryginalu)", elapsed,
			HdCountsLine().c_str());
	}

	std::string HookDiagConsole(const std::string& arg) {
		if (arg == "RESET") {
			for (auto& c : zdHookCalls)
				c = 0;
			ZLog("Haki: liczniki od nowa");
			return "haki: liczniki od nowa";
		}
		if (arg == "CHAIN") {
			bool dbg = zcfg.debug;
			zcfg.debug = true;
			ULONGLONG start = hookDiagStart;
			int done = hookReportsDone;
			HookDiagInit();
			hookDiagStart = start;
			hookReportsDone = done;
			zcfg.debug = dbg;
			return "haki: wejscia funkcji w zDismember.log";
		}
		std::string line = HdCountsLine();
		ZLog("Haki (konsola): %s", line.c_str());
		return line.c_str();
	}
}
