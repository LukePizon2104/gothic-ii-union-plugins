// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Menu kolowe szybkiego wyboru - opis w Wheel.h, logika w Core\Wheel.*.
	static const double WH_RAMP_MS = 120.0;        // przejscie tempa czasu (otwarcie / zamkniecie)
	static const DWORD WH_LOOP_STALE_MS = 500;     // hak wejscia wolany tez w menu gry - bez Game_Loop dluzej = poza gra
	static const int WH_BASE_CALM_FRAMES = 30;     // linia bazowa callbackow wejscia po tylu spokojnych klatkach
	static const float WH_CONSOLE_WAIT_S = 10.0f;

	struct WhItem {
		int spell = -1;
		void* spellPtr = nullptr;
		int spellId = -1;
		int keyNo = 0;
		std::string name;
		int kind = zdcore::WSK_RUNE;
		int amount = 1;
		int circle = 0;
		int sign = -1;                 // zdcore::SIGN_* albo -1
		std::vector<std::string> lines;
	};

	static zdcore::WheelKey whKey;
	static bool whOpen = false;
	static bool whOpenReq = false, whConfirmReq = false, whCancelReq = false, whClickReq = false;
	static const char* whCancelWhy = "";
	static double whArmMs = 0.0, whOpenMs = 0.0;
	static DWORD whLoopTick = 0;
	static float whConsoleWait = 0.0f;
	static bool whFromConsole = false;
	static double whLoopClock = 0.0;

	// zawartosc z chwili otwarcia
	static std::vector<WhItem> whItems;    // indeks = indeks czaru w ksiedze
	static std::vector<int> whOrder;
	static int whActiveSpell = -1;
	static bool whXbow = false, whXbowSel = false;
	static int whXbowBolts = 0, whXbowCd = 0;
	static std::string whXbowName;
	static int whPage = 0, whPages = 1, whActivePage = 0, whActiveSlot = -1;
	static int whPageDelta = 0;
	static zdcore::WheelMouse whMouse;
	static bool whLmbPrev = false, whRmbPrev = false;

	// czas
	static float whTempo = 1.0f;           // biezace tempo kola / celowania (0..1)
	static bool whManaging = false;        // factorMotion ustawiany przez nas
	static float whExternal = 1.0f;        // factorMotion innych (gra, NB, konsola, finisher)
	static float whWritten = 1.0f;         // nasz ostatni zapis
	static zdcore::WheelTime whTime;
	static double whClockPrev = 0.0;
	static int whExternalChanges = 0;
	struct WhTimeStats {
		int frames = 0, engineZero = 0;
		double realMs = 0.0, gameMs = 0.0;
		float minMs = 1e9f, maxMs = 0.0f;
	};
	static WhTimeStats whStats;
	static bool WhPhysDown(int code);   // klawiatura albo przycisk myszy (definicja przy przypisaniu klawisza)

	// okna innych pluginow: callbacki wejscia przed ogame
	static std::vector<void*> whCbBase;
	static bool whCbBaseSet = false;
	static int whCbCalm = 0;

	// ---------------------------------------------------------------- eksporty innych pluginow

	struct WhExport {
		const char* fn;
		FARPROC p;
		DWORD next;
	};
	static WhExport whZqsBusy = { "ZQS_Busy", nullptr, 0 };
	static WhExport whZqsSelect = { "ZQS_Select", nullptr, 0 };
	static WhExport whZqsCooldown = { "ZQS_CooldownLeftMs", nullptr, 0 };
	static WhExport whZtrActive = { "ZTR_RewindActive", nullptr, 0 };
	static const char* const WH_ZQS_MODS[] = { "zQuickSign.dll", "NB_UNION_PLUGIN_ALLOWED_01.DLL" };
	static const char* const WH_ZTR_MODS[] = { "zTimeRewind.dll", "NB_UNION_PLUGIN_DIFF_02.DLL" };

	// Po nazwie modulu, a bez niego po wszystkich modulach procesu (DLL z VDF); ponowna proba co 5 s.
	static FARPROC WhExportGet(WhExport& e, const char* const* names, int n) {
		if (e.p)
			return e.p;
		DWORD now = GetTickCount();
		if (e.next && (int)(now - e.next) < 0)
			return nullptr;
		e.next = now + 5000;
		HMODULE found = nullptr;
		for (int i = 0; i < n && !e.p; i++) {
			HMODULE m = GetModuleHandleA(names[i]);
			if (m && (e.p = GetProcAddress(m, e.fn)) != nullptr)
				found = m;
		}
		if (!e.p) {
			HMODULE mods[512];
			DWORD need = 0;
			if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need))
				for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 512 && !e.p; i++)
					if ((e.p = GetProcAddress(mods[i], e.fn)) != nullptr)
						found = mods[i];
		}
		if (e.p) {
			char path[MAX_PATH] = "?";
			GetModuleFileNameA(found, path, MAX_PATH);
			const char* b = strrchr(path, '\\');
			ZLog("Kolo: eksport %s w %s", e.fn, b ? b + 1 : path);
		}
		return e.p;
	}

	static bool WhQsBusy() {
		typedef int(__cdecl* F)();
		F f = (F)WhExportGet(whZqsBusy, WH_ZQS_MODS, 2);
		return f && f() != 0;
	}

	static int WhQsCooldown() {
		typedef int(__cdecl* F)();
		F f = (F)WhExportGet(whZqsCooldown, WH_ZQS_MODS, 2);
		return f ? f() : 0;
	}

	static bool WhRewindActive() {
		typedef int(__cdecl* F)();
		F f = (F)WhExportGet(whZtrActive, WH_ZTR_MODS, 2);
		return f && f() != 0;
	}

	// ---------------------------------------------------------------- stan gry

	static bool WhFocus() {
		HWND want = *(HWND*)0x008D1D8C;   // okno wejscia (zCInput_Win32::GetKey porownuje z GetFocus)
		return !want || GetFocus() == want;
	}

	static bool WhInGame() {
		return player && ogame && ogame->GetGameWorld() && whLoopTick && GetTickCount() - whLoopTick <= WH_LOOP_STALE_MS;
	}

	struct WhCbNode {
		void* data;
		WhCbNode* next;
	};

	// Callbacki wejscia przed ogame (zCInputCallback::inputList 0xAB6460, pierwszy wezel pod +4; GetInput 0x7A55C0 wola
	// je po kolei do pierwszego, ktory przyjmie klawisz). false = ogame nie na liscie.
	static bool WhCallbacksBefore(std::vector<void*>& out) {
		out.clear();
		if (!ogame)
			return false;
		int guard = 0;
		for (WhCbNode* n = *(WhCbNode**)0x00AB6464; n && guard < 256; n = n->next, guard++) {
			if (n->data == (void*)ogame)
				return true;
			out.push_back(n->data);
		}
		return false;
	}

	static void* WhForeignCallback() {
		if (!whCbBaseSet)
			return nullptr;
		std::vector<void*> now;
		if (!WhCallbacksBefore(now))
			return nullptr;
		for (void* c : now)
			if (std::find(whCbBase.begin(), whCbBase.end(), c) == whCbBase.end())
				return c;
		return nullptr;
	}

	static std::string WhCbDesc(void* cb) {
		if (!cb)
			return "-";
		DWORD vt = 0;
		if (!HdRead((DWORD)(uintptr_t)cb, &vt, 4))
			return "?";
		return HdModule(vt);
	}

	static void WhBaselineUpdate() {
		if (whCbBaseSet || !player || !ogame || !ogame->GetGameWorld())
			return;
		bool calm = !MdWindow(false) && !ZMenuCapturing() && !MeditateActive() && !PathMenuOpen();
		whCbCalm = calm ? whCbCalm + 1 : 0;
		if (whCbCalm < WH_BASE_CALM_FRAMES)
			return;
		bool inList = WhCallbacksBefore(whCbBase);
		whCbBaseSet = true;
		std::string mods;
		for (void* c : whCbBase)
			mods += (mods.empty() ? "" : ", ") + WhCbDesc(c);
		ZLog("Kolo: linia bazowa okien - %d callbackow wejscia przed gra%s%s%s", (int)whCbBase.size(), inList ? "" : " (ogame poza lista!)",
			mods.empty() ? "" : ": ", mods.c_str());
	}

	// Eksporty kuszy (Crossbow.cpp, tylko G2A).
	static int WhXbowState(int* avail, int* sel, int* bolts, int* cd, char* label, int size) {
#if ENGINE == Engine_G2A
		return ZDISM_XbowState(avail, sel, bolts, cd, label, size);
#else
		if (avail)
			*avail = 0;
		return 0;
#endif
	}

	static int WhXbowSelect(int on) {
#if ENGINE == Engine_G2A
		return ZDISM_XbowSelect(on);
#else
		return 0;
#endif
	}

	static bool WhXbowAimingNow() {
		return xbRun.phase == zdcore::XP_LOOP;   // Crossbow.cpp (ta sama jednostka kompilacji): petla celowania
	}

	// Stan do odmowy. press = przy wcisnieciu (pelne warunki); inaczej przy otwartym kole (tylko przerwania).
	static zdcore::WheelSense WhSense(bool press, std::string& extra) {
		zdcore::WheelSense s;
		s.enabled = zcfg.enabled && zcfg.wheel;
		s.inGame = WhInGame();
		if (!s.enabled || !s.inGame)
			return s;
		s.down = player->IsDead() || player->IsUnconscious();
		if (s.down)
			return s;
		s.human = HeroIsHuman();
		s.window = MdWindow(false);
		if (s.window)
			extra = s.window;
		s.pluginWindow = MeditateActive() || PathMenuOpen() || (ZMenuCapturing() && !whOpen);
		void* foreign = WhForeignCallback();
		s.foreignWindow = foreign != nullptr;
		if (foreign)
			extra = "callback " + WhCbDesc(foreign);
		zCEventManager* em = player->GetEM(FALSE);
		s.cutscene = em && em->GetCutsceneMode();
		s.mob = player->GetInteractMob() != nullptr;
		s.pluginMove = DirectorActive() || TraversalActive() || MovesActive() || LootActive();
		if (!press)
			return s;
		s.quickCast = WhQsBusy();
		s.xbowBusy = XbowBusy();
		bool rewind = WhRewindActive();
		bool casting = false;
		if (player->GetWeaponMode() == NPC_WEAPON_MAG) {
			mvRawRead = true;           // odczyt bez podmian unikow / podnoszenia (Dodge.cpp, Loot.cpp)
			casting = LootRawDown();
			mvRawRead = false;
		}
		s.rewind = rewind || casting;
		if (casting && !rewind)
			extra = "tryb magii, akcja trzymana";
		oCMag_Book* book = player->GetSpellBook();
		int n = book ? book->spells.GetNum() : 0;
		int avail = 0;
		WhXbowState(&avail, nullptr, nullptr, nullptr, nullptr, 0);
		s.empty = n <= 0 && !avail;
		return s;
	}

	// ---------------------------------------------------------------- zawartosc

	static void WhCollect() {
		whItems.clear();
		whOrder.clear();
		whActiveSpell = -1;
		oCMag_Book* book = player ? player->GetSpellBook() : nullptr;
		int n = book ? std::min(book->spells.GetNum(), 64) : 0;
		std::vector<int> keyNo;
		for (int i = 0; i < n; i++) {
			WhItem it;
			oCSpell* sp = book->spells[i];
			it.spell = i;
			it.spellPtr = sp;
			it.spellId = sp ? sp->GetSpellID() : -1;
			it.keyNo = sp ? sp->keyNo : 0;
			oCItem* item = sp ? book->GetSpellItem(sp) : nullptr;
			if (item) {
				it.name = item->description.ToChar();
				if (it.name.empty())
					it.name = item->name.ToChar();
				bool scroll = (item->flags & ITM_FLAG_MULTI) != 0;
				it.kind = scroll ? zdcore::WSK_SCROLL : zdcore::WSK_RUNE;
				it.amount = item->amount;
				it.circle = item->mag_circle;
				for (int t = 0; t < ITM_TEXT_MAX && it.lines.size() < 5; t++) {
					std::string l = zdcore::WheelItemLine(item->text[t].ToChar(), item->count[t]);
					if (!l.empty())
						it.lines.push_back(l);
				}
			}
			if (it.name.empty())
				it.name = "?";
			for (int k = 0; k < zdcore::SIGN_COUNT; k++)
				if (it.spellId >= 0 && zcfg.signSpellIds[k] == it.spellId) {
					it.sign = k;
					it.kind = zdcore::WSK_SIGN;
				}
			keyNo.push_back(it.keyNo);
			whItems.push_back(it);
		}
		whOrder = zdcore::WheelSpellOrder(keyNo);
		whActiveSpell = book && book->spellnr >= 0 && book->spellnr < n ? book->spellnr : -1;
		int avail = 0, sel = 0, bolts = 0, cd = 0;
		char label[64] = "";
		WhXbowState(&avail, &sel, &bolts, &cd, label, sizeof(label));
		whXbow = avail != 0;
		whXbowSel = whXbow && sel != 0;
		whXbowBolts = bolts;
		whXbowCd = cd;
		whXbowName = label[0] ? label : "Kusza";
		whPages = zdcore::WheelPageCount(n, whXbow);
		zdcore::WheelFindActive(whOrder, whXbow, whXbowSel, whActiveSpell, whActivePage, whActiveSlot);
		whPage = whActivePage;
	}

	static std::string WhEntryName(const zdcore::WheelEntry& e) {
		if (e.kind == zdcore::WE_XBOW)
			return "kusza " + whXbowName;
		if (e.kind == zdcore::WE_SPELL && e.spell >= 0 && e.spell < (int)whItems.size()) {
			const WhItem& it = whItems[e.spell];
			char b[160];
			sprintf_s(b, "czar %d (id %d, slot %d) \"%s\"", e.spell, it.spellId, it.keyNo, it.name.c_str());
			return b;
		}
		return "puste pole";
	}

	static int WhHoverSlot() {
		return whMouse.slot >= 0 ? whMouse.slot : (whPage == whActivePage ? whActiveSlot : -1);
	}

	// ---------------------------------------------------------------- otwarcie / zamkniecie

	static void WhLogStats(const char* how) {
		const WhTimeStats& s = whStats;
		ZLog("Kolo: %s po %.2f s; czas: %d klatek, rzeczywisty %.0f ms, gra %.1f ms (x%.3f), klatka gry %.2f..%.2f ms, silnik dalby 0 ms "
			"w %d klatkach (u nas %s), factorMotion zewn. %.2f (zmiany z zewnatrz: %d)", how, (ZClockMs() - whOpenMs) / 1000.0, s.frames,
			s.realMs, s.gameMs, s.realMs > 0 ? s.gameMs / s.realMs : 0.0, s.frames ? s.minMs : 0.0f, s.maxMs, s.engineZero,
			s.frames && s.minMs > 0.0f ? "zawsze > 0" : s.frames ? "BYLO 0" : "-", whExternal, whExternalChanges);
	}

	static void WhClose(const char* how) {
		if (!whOpen)
			return;
		whOpen = false;
		ZMenuCapture(false, "kolo");
		if (whKey.state == zdcore::WK_OPEN)
			zdcore::WheelKeyClosed(whKey, WhPhysDown(zcfg.wheelKey));
		whConfirmReq = whCancelReq = whClickReq = false;
		whPageDelta = 0;
		WhLogStats(how);
	}

	static void WhRestoreTime(const char* why) {
		if (whManaging && ztimer) {
			ztimer->factorMotion = whExternal;
			ZLog("Kolo: tempo czasu przywrocone od razu (%s) - factorMotion %.2f", why, whExternal);
		}
		whManaging = false;
		whTempo = 1.0f;
		whTime = zdcore::WheelTime();
	}

	static void WhOpen(const char* src) {
		std::string extra;
		zdcore::WheelSense s = WhSense(true, extra);
		zdcore::WheelDeny d = zdcore::WheelDenyCheck(s);
		if (d != zdcore::WDENY_OK) {
			ZLog("Kolo: odmowa otwarcia (%s) - %s%s%s", src, zdcore::WheelDenyText(d), extra.empty() ? "" : ": ", extra.c_str());
			if (whKey.state == zdcore::WK_OPEN)
				zdcore::WheelKeyClosed(whKey, WhPhysDown(zcfg.wheelKey));
			return;
		}
		WhCollect();
		whMouse = zdcore::WheelMouse();
		whLmbPrev = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
		whRmbPrev = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		whPageDelta = 0;
		whConfirmReq = whCancelReq = whClickReq = false;
		whStats = WhTimeStats();
		whExternalChanges = 0;
		whOpenMs = ZClockMs();
		whOpen = true;
		whFromConsole = whKey.state != zdcore::WK_OPEN;
		ZMenuCapture(true, "kolo");
		zdcore::WheelEntry act = zdcore::WheelSlotEntry(whOrder, whXbow, whActivePage, whActiveSlot);
		ZLog("Kolo: otwarte (%s, klawisz trzymany %.0f ms) - czarow %d, stron %d, kusza %s%s, aktywne: %s (strona %d, pole %d), "
			"tempo %d%%, factorMotion %.2f", src, whOpenMs - whArmMs, (int)whItems.size(), whPages, whXbow ? "tak" : "nie",
			whXbowSel ? " (wybrana)" : "", WhEntryName(act).c_str(), whActivePage + 1, whActiveSlot, zcfg.wheelSlowmo,
			ztimer ? ztimer->factorMotion : 0.0f);
	}

	static int WhQsSelect(int index) {
		typedef int(__cdecl* F)(int);
		F f = (F)WhExportGet(whZqsSelect, WH_ZQS_MODS, 2);
		return f ? f(index) : -1;
	}

	static void WhConfirm(const char* src) {
		int slot = whMouse.slot;
		if (slot < 0) {
			WhClose((std::string("zamkniete (") + src + ") - bez wyboru myszy, bez zmiany").c_str());
			return;
		}
		zdcore::WheelEntry e = zdcore::WheelSlotEntry(whOrder, whXbow, whPage, slot);
		std::string name = WhEntryName(e);
		char how[400];
		if (e.kind == zdcore::WE_EMPTY) {
			sprintf_s(how, "zamkniete (%s) - puste pole %d strony %d, bez zmiany", src, slot, whPage + 1);
			WhClose(how);
			return;
		}
		bool already = (e.kind == zdcore::WE_XBOW && whXbowSel) || (e.kind == zdcore::WE_SPELL && !whXbowSel && e.spell == whActiveSpell);
		if (already) {
			sprintf_s(how, "zatwierdzone (%s) %s - juz wybrane", src, name.c_str());
			WhClose(how);
			return;
		}
		int r = 0;
		const char* path = "";
		if (e.kind == zdcore::WE_XBOW) {
			r = WhQsSelect(-2);
			path = "ZQS_Select(-2)";
			if (r < 0) {
				r = WhXbowSelect(1);
				path = "ZDISM_XbowSelect(1) - bez zQuickSign";
			}
		}
		else {
			oCMag_Book* book = player ? player->GetSpellBook() : nullptr;
			bool same = book && e.spell < book->spells.GetNum() && (void*)book->spells[e.spell] == whItems[e.spell].spellPtr;
			if (!same) {
				sprintf_s(how, "odrzucone (%s) %s - ksiega zmienila sie od otwarcia", src, name.c_str());
				WhClose(how);
				return;
			}
			r = WhQsSelect(e.spell);
			path = "ZQS_Select";
			if (r < 0) {
				path = "SetFrontSpell - bez zQuickSign";
				if (player->GetWeaponMode() == NPC_WEAPON_MAG || book->open)
					r = 0;
				else {
					book->SetFrontSpell(e.spell);
					WhXbowSelect(0);
					r = 1;
				}
			}
		}
		if (r > 0 && player) {
			// zQuickSign ZQS_Select zwraca 1 takze, gdy bramka gry odmowila zamiany runy w rece - sprawdzamy skutek
			int avail = 0, sel = 0;
			WhXbowState(&avail, &sel, nullptr, nullptr, nullptr, 0);
			oCMag_Book* book = player->GetSpellBook();
			bool magic = player->GetWeaponMode() == NPC_WEAPON_MAG;   // zamiana runy w rece idzie animacja - bez sprawdzania
			bool done = e.kind == zdcore::WE_XBOW ? sel != 0 : (magic || (book && book->spellnr == e.spell && !sel));
			if (!done) {
				sprintf_s(how, "odrzucone (%s) %s - wybor sie nie zmienil (%s: bramka gry / runa w rece - komunikat zQuickSign)", src,
					name.c_str(), path);
				WhClose(how);
				return;
			}
		}
		if (r > 0)
			sprintf_s(how, "zatwierdzone (%s) %s - strona %d pole %d (%s)", src, name.c_str(), whPage + 1, slot, path);
		else
			sprintf_s(how, "odrzucone (%s) %s - %s odmowil (rzut / strzal w toku, runa w rece poza bramka gry)", src, name.c_str(), path);
		WhClose(how);
	}

	// ---------------------------------------------------------------- wejscie (hak ProcessInputEvents)

	// ---------------------------------------------------------------- Caps Lock (WheelKey=58, 23.10 wieczor)
	// Silnik (G2A): oCAIHuman::PC_SlowMove 0x69A350 (z _WalkCycle: PC_SlowMove(GetState(GAME_SLOW))) czyta przelacznik Caps Lock
	// Windows przez GetKeyboardState (bajt VK_CAPITAL & 1) - wlaczony = chod, jak trzymany GAME_SLOW; przy wlaczonym Caps
	// Lock i GAME_SLOW wylacza go SetCapsLock 0x694FE0 (keybd_event VK_CAPITAL). oCAIHuman::PC_Sneak 0x69A790 przy skradaniu
	// i wlaczonym Caps Lock tez go wylacza (keybd_event). Gdy kolo jest pod Caps Lock: oba skoki w pamieci na "zawsze jak
	// wylaczony" (WheelCapsPatch) - chod tylko z GAME_SLOW, Caps Lock tylko dla kola; stukniecie Caps Lock przelacza chod
	// pluginu (WheelTapWalk: GetState(GAME_SLOW) = 1), a kazde fizyczne wcisniecie Caps Lock jest po puszczeniu cofane w
	// Windows (WheelCapsRestore: keybd_event, syntetyczne wcisniecie polykane), zeby dioda i wielkie litery nie zostawaly.
	static const int WH_DIK_CAPS = 0x3A;
	struct WhPatchSite {
		DWORD addr;
		BYTE orig, patched;
		const char* what;
	};
	static const WhPatchSite WH_CAPS_SITES[] = {
		{ 0x0069A39B, 0x74, 0xEB, "PC_SlowMove: Caps Lock = chod" },          // je 0x69A3C0 -> jmp (przelacznik wylaczony)
		{ 0x0069A7DA, 0x74, 0xEB, "PC_Sneak: wylaczanie Caps Lock" },          // je 0x69A7F8 -> jmp (bez keybd_event)
	};
	static bool whCapsPatched = false, whCapsPatchFailed = false;
	static bool whWalkOn = false;              // chod pluginu (stukniecie Caps Lock)
	static bool whPhysPrev = false, whPhysSession = false, whSessionRestore = false, whSessionArmed = false;
	static int whSessionToggle0 = 0;
	static int whSynthPending = 0;             // syntetyczne wcisniecia Caps Lock do polkniecia
	static double whSynthUntil = 0.0;
	static unsigned whBufPressStamp = ~0u;      // zdFrame ostatniego wcisniecia z bufora
	static int whArmBuf = 0, whArmPoll = 0;    // zrodla uzbrojenia (log / zdism wheel)

	static const char* WhKeyName() {
		return zcfg.wheelKey == 0x3A ? "Caps Lock" : zcfg.wheelKey == 0x0F ? "Tab" : "klawisz";
	}

	static bool WhCapsKey() {
		return zcfg.wheelKey == WH_DIK_CAPS;
	}

	static bool WhCapsWanted() {
		return ENGINE == Engine_G2A && zcfg.enabled && zcfg.wheel && WhCapsKey() && zcfg.wheelCapsPatch;
	}

	static void WhCapsPatchUpdate() {
		bool want = WhCapsWanted();
		if (want == whCapsPatched || (want && whCapsPatchFailed))
			return;
		if (want) {
			for (auto& s : WH_CAPS_SITES)
				if (*(BYTE*)s.addr != s.orig) {
					whCapsPatchFailed = true;
					ZLog("Kolo: Caps Lock - bajt 0x%08X = %02X zamiast %02X (%s) - inny plugin zmienil kod, chod z Caps Lock zostaje",
						(unsigned)s.addr, *(BYTE*)s.addr, s.orig, s.what);
					return;
				}
		}
		for (auto& s : WH_CAPS_SITES) {
			DWORD old = 0;
			if (VirtualProtect((void*)s.addr, 1, PAGE_EXECUTE_READWRITE, &old)) {
				*(BYTE*)s.addr = want ? s.patched : s.orig;
				VirtualProtect((void*)s.addr, 1, old, &old);
				FlushInstructionCache(GetCurrentProcess(), (void*)s.addr, 1);
			}
		}
		whCapsPatched = want;
		ZLog("Kolo: Caps Lock %s - %s, %s", want ? "odczepiony od gry" : "znow dla gry",
			want ? "przelacznik Windows nie wlacza chodu (PC_SlowMove 0x69A39B) i skradanie go nie wylacza (PC_Sneak 0x69A7DA)" :
			"chod z przelacznika Caps Lock jak w grze", want && zcfg.wheelTapWalk ? "stukniecie Caps Lock = chod (plugin)" :
			want ? "stukniecie Caps Lock = nic (WheelTapWalk=0)" : "");
	}

	bool WheelWalkOn() {
		return whWalkOn && whCapsPatched && zcfg.wheelTapWalk;
	}

	static int WhCapsToggle() {
		return (GetKeyState(VK_CAPITAL) & 1) != 0 ? 1 : 0;
	}

	// Cofniecie przelacznika Caps Lock w Windows po fizycznym wcisnieciu (jak SetCapsLock silnika: wcisniecie + puszczenie).
	static void WhCapsRestore(int before) {
		int now = WhCapsToggle();
		whSynthPending = 1;
		whSynthUntil = ZClockMs() + 400.0;
		keybd_event(VK_CAPITAL, (BYTE)WH_DIK_CAPS, 0, 0);
		keybd_event(VK_CAPITAL, (BYTE)WH_DIK_CAPS, KEYEVENTF_KEYUP, 0);
		ZDebug("Kolo: Caps Lock w Windows przywrocony (przelacznik przed wcisnieciem %d, po %d -> syntetyczne wcisniecie)", before, now);
	}

	static bool WhSynthActive() {
		if (whSynthPending > 0 && ZClockMs() > whSynthUntil)
			whSynthPending = 0;
		return whSynthPending > 0;
	}

	// Uzbrojenie (wspolne dla bufora i odczytu fizycznego). true = uzbrojony.
	static bool WhTryArm(const char* src) {
		if (whKey.state == zdcore::WK_WAIT_RELEASE)
			whKey.state = zdcore::WK_IDLE;   // puszczenie przegapione - nowe wcisniecie
		if (whKey.state != zdcore::WK_IDLE)
			return true;
		if (!WhInGame() || (zcon && zcon->IsVisible()))
			return false;
		if ((zcfg.wheelKey == zcfg.dodgeKey || zcfg.wheelKey == zcfg.rollKey) && MvFightKeysOurs()) {
			ZDebug("Kolo: klawisz %d to tez unik / przewrot - w walce dla ruchow, kolo tylko poza walka", zcfg.wheelKey);
			return false;
		}
		std::string extra;
		zdcore::WheelSense s = WhSense(true, extra);
		zdcore::WheelDeny d = zdcore::WheelDenyCheck(s);
		if (d != zdcore::WDENY_OK) {
			if (d == zdcore::WDENY_WINDOW || d == zdcore::WDENY_PLUGIN_WINDOW || d == zdcore::WDENY_OFF || d == zdcore::WDENY_NOT_IN_GAME)
				ZDebug("Kolo: klawisz %d dla gry - %s%s%s", zcfg.wheelKey, zdcore::WheelDenyText(d), extra.empty() ? "" : ": ", extra.c_str());
			else
				ZLog("Kolo: odmowa (klawisz %d, %s) - %s%s%s", zcfg.wheelKey, src, zdcore::WheelDenyText(d), extra.empty() ? "" : ": ",
					extra.c_str());
			return false;
		}
		whArmMs = ZClockMs();
		zdcore::WheelKeyPress(whKey, whArmMs);
		whSessionArmed = true;
		(strcmp(src, "bufor") == 0 ? whArmBuf : whArmPoll)++;
		ZDebug("Kolo: klawisz %d uzbrojony (%s, prog %d ms)", zcfg.wheelKey, src, zcfg.wheelHoldMs);
		return true;
	}


	// ---------------------------------------------------------------- klawisz kola: przypisanie z menu, przyciski myszy
	// Fizyczny stan klawisza / przycisku: klawiatura przez skan -> VK (ZmPhysicallyDown), mysz przez VK_*BUTTON.
	static bool WhPhysDown(int code) {
		if (zdcore::WheelIsMouseCode(code)) {
			int vk = zdcore::WheelMouseVk(code);
			return vk && (GetAsyncKeyState(vk) & 0x8000) != 0;
		}
		return ZmPhysicallyDown(code);
	}

	// Nazwa klawisza do menu i logu (cp1250): mysz z Core, klawiatura z GetKeyNameText (nazwa Windows, np. "CAPS LOCK", "Q").
	static std::string WhKeyLabel(int code) {
		if (code == 0x3A)
			return "Caps Lock";
		if (code == 0x0F)
			return "Tab";
		if (zdcore::WheelIsMouseCode(code))
			return zdcore::WheelMouseName(code);
		if (code > 0 && code < 0x100) {
			char name[64] = "";
			LONG lp = ((LONG)(code & 0x7F) << 16) | ((code & 0x80) ? (1L << 24) : 0);
			if (GetKeyNameTextA(lp, name, sizeof(name)) > 0)
				return name;
		}
		char b[32];
		sprintf_s(b, "kod %d", code);
		return b;
	}

	// Kolizje klawisza: [KEYS] Gothic.ini, klawisze zDismember, F zQuickSign, srodkowy przycisk (Hedin Quick Items).
	static std::string WhKeyConflicts(int code) {
		std::string out;
		auto add = [&](const std::string& w) { out += (out.empty() ? "" : ", ") + w; };
		static const char* keys[] = { "keyEnd", "keyHeal", "keyPotion", "keyLockTarget", "keyParade", "keyActionRight",
			"keyActionLeft", "keyUp", "keyDown", "keyLeft", "keyRight", "keyStrafeLeft", "keyStrafeRight", "keyAction",
			"keySlow", "keySMove", "keyWeapon", "keySneak", "keyLook", "keyLookFP", "keyInventory", "keyShowStatus",
			"keyShowLog", "keyShowMap", "keyTorch", "keyIntSprint" };
		if (zoptions)
			for (const char* k : keys) {
				if (!zoptions->EntryExists("KEYS", k))
					continue;
				for (int c : zdcore::ParseKeyBinding(zoptions->ReadString("KEYS", k, "").ToChar()))
					if (c == code)
						add(k);
			}
		struct Own { int key; const char* what; };
		const Own own[] = { { zcfg.dodgeKey, "unik zDismember" }, { zcfg.rollKey, "przewrot zDismember" },
			{ zcfg.styleKey, "styl zDismember" }, { zcfg.locoKey, "ruch zDismember" }, { zcfg.swapKey, "zamiana 1H/2H zDismember" },
			{ zcfg.meditateKey, "medytacja" }, { zcfg.meditateKeyAlt, "medytacja (drugi)" }, { zcfg.pathMenuKey, "okno sciezek" },
			{ zcfg.secondSwordKey, "drugi miecz" } };
		for (auto& o : own)
			if (o.key > 0 && o.key == code)
				add(o.what);
		char ini[MAX_PATH];
		if (GetModuleFileNameA(nullptr, ini, MAX_PATH)) {
			char* sl = strrchr(ini, '\\');
			if (sl) {
				strcpy_s(sl + 1, MAX_PATH - (sl + 1 - ini), "zQuickSign.ini");
				int f = (int)GetPrivateProfileIntA("ZQUICKSIGN", "KeyDik", 0x21, ini);
				if (f == code)
					add("F zQuickSign (KeyDik)");
			}
		}
		if (code == zdcore::WHEEL_MOUSE_MID)
			add("srodkowy przycisk - Hedin Quick Items");
		return out;
	}

	static bool whBinding = false;            // nasluch klawisza (zdism wheel bind / menu)
	static DWORD whBindTick = 0;
	static bool whBindMousePrev[3] = {};
	static bool whBindCapsPending = false;    // przypisany Caps Lock - po puszczeniu przelacznik Windows cofniety
	static std::string whMenuText;            // ostatni tekst wiersza menu ZDISM_WHEEL_KEY_VALUE
	static void* whMenuItem = nullptr;
	static const DWORD WH_BIND_TIMEOUT_MS = 10000;

	static void WhBindStart(const char* src) {
		whBinding = true;
		whBindTick = GetTickCount();
		static const int vks[3] = { 0x04, 0x05, 0x06 };
		for (int i = 0; i < 3; i++)
			whBindMousePrev[i] = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
		ZLog("Kolo: przypisanie klawisza (%s) - czekam na klawisz albo przycisk myszy (Esc anuluje, %u s)", src,
			(unsigned)(WH_BIND_TIMEOUT_MS / 1000));
	}

	static void WhBindAccept(int code, const char* src) {
		const char* why = nullptr;
		if (!zdcore::WheelKeyBindable(code, &why)) {
			ZLog("Kolo: przypisanie - %s odrzucony (%s), czekam dalej", WhKeyLabel(code).c_str(), why ? why : "?");
			return;
		}
		int old = zcfg.wheelKey;
		whBinding = false;
		WheelFinishNow("zmiana klawisza kola");
		zcfg.wheelKey = code;
		if (zoptions)
			zoptions->WriteInt("ZDISMEMBER", "WheelKey", code, FALSE);
		whBindCapsPending = code == 0x3A && zcfg.wheelCapsRestore;
		std::string col = WhKeyConflicts(code);
		ZLog("Kolo: klawisz kola przypisany (%s): %s (kod %d, bylo %s)%s%s", src, WhKeyLabel(code).c_str(), code,
			WhKeyLabel(old).c_str(), col.empty() ? " - bez kolizji" : " - UWAGA, ten sam klawisz: ", col.c_str());
	}

	// ZmFilterPress (pierwszy): w nasluchu kazde wcisniecie z bufora (klawiatura, mysz) jest nasze.
	bool WheelBindPress(int code) {
		if (!whBinding)
			return false;
		if (GetTickCount() - whBindTick > WH_BIND_TIMEOUT_MS) {
			whBinding = false;
			ZLog("Kolo: przypisanie - nic nie wcisnieto w %u s, klawisz bez zmian", (unsigned)(WH_BIND_TIMEOUT_MS / 1000));
			return false;
		}
		if (code == 0x01) {
			whBinding = false;
			ZLog("Kolo: przypisanie anulowane (Esc) - klawisz bez zmian: %s", WhKeyLabel(zcfg.wheelKey).c_str());
			return true;
		}
		if (code == 0x20A || code == 0x20B)
			return true;   // kolko myszy - nie klawisz
		WhBindAccept(code, "bufor gry");
		return true;
	}

	// Co klatke (hak wejscia, tez w menu): przyciski myszy, ktorych silnik nie zglasza buforem (boczne) - GetAsyncKeyState.
	static void WhBindPoll() {
		if (whBindCapsPending && !ZmPhysicallyDown(0x3A)) {
			whBindCapsPending = false;
			WhCapsRestore(-1);
		}
		if (!whBinding)
			return;
		if (GetTickCount() - whBindTick > WH_BIND_TIMEOUT_MS) {
			whBinding = false;
			ZLog("Kolo: przypisanie - nic nie wcisnieto w %u s, klawisz bez zmian", (unsigned)(WH_BIND_TIMEOUT_MS / 1000));
			return;
		}
		static const int vks[3] = { 0x04, 0x05, 0x06 };
		for (int i = 0; i < 3 && whBinding; i++) {
			bool d = (GetAsyncKeyState(vks[i]) & 0x8000) != 0;
			if (d && !whBindMousePrev[i] && WhFocus())
				WhBindAccept(zdcore::WheelMouseCodeFromVk(vks[i]), "GetAsyncKeyState");
			whBindMousePrev[i] = d;
		}
	}

	// Game_MenuLoop: wiersz ZDISM_WHEEL_KEY_VALUE strony "Kolo wyboru" pokazuje biezacy klawisz (albo prosbe o klawisz).
	void WheelMenuLoop() {
		zCMenuItem* it = zCMenuItem::GetByName(zSTRING("ZDISM_WHEEL_KEY_VALUE"));
		if (!it)
			return;
		std::string t;
		if (whBinding)
			t = "Wci\x9Cnij klawisz... (Esc anuluje)";
		else {
			t = WhKeyLabel(zcfg.wheelKey);
			static int colKey = -1;
			static DWORD colTick = 0;
			static std::string col;
			if (colKey != zcfg.wheelKey || GetTickCount() - colTick > 2000) {   // ini czytane co 2 s, nie co klatke
				colKey = zcfg.wheelKey;
				colTick = GetTickCount();
				col = WhKeyConflicts(zcfg.wheelKey);
			}
			if (!col.empty())
				t += "  (te\xBF: " + col + ")";
		}
		if (t == whMenuText && (void*)it == whMenuItem)
			return;
		whMenuText = t;
		whMenuItem = it;
		it->SetText(zSTRING(t.c_str()), 0, TRUE);
	}

	bool WheelFilterPress(int code) {
		if (!zcfg.enabled || !zcfg.wheel || code != zcfg.wheelKey || ENGINE != Engine_G2A)
			return false;
		if (WhCapsKey() && WhSynthActive()) {
			whSynthPending--;
			ZDebug("Kolo: syntetyczne wcisniecie Caps Lock (przywrocenie przelacznika) polkniete");
			return true;
		}
		whBufPressStamp = zdFrame;
		if ((zcfg.wheelKey == zcfg.dodgeKey || zcfg.wheelKey == zcfg.rollKey) && MvFightKeysOurs())
			return false;   // wspolny z unikiem / przewrotem (Kroniki: DodgeKey=58) - w walce wcisniecie dla ruchow
		bool armed = WhTryArm("bufor");
		if (WhCapsKey() && whCapsPatched)
			return true;    // Caps Lock tylko dla kola - gra go nie widzi (tez przy odmowie; i tak nic w grze nie robi)
		return armed;
	}

	// Stukniecie: kod klawisza na koniec bufora 0x8D1D50 (bez keyevent / keytoggle - NB bCloseInvTabForce czyta toggle przy
	// otwartym ekwipunku). W nastepnej klatce GetInput zdejmie go zwykla droga (oCGame::HandleEvent -> OpenInventory).
	static void WhInject(int code, double heldMs) {
		ZmKeyBuffer* buf = (ZmKeyBuffer*)0x008D1D50;
		if (buf->num >= buf->numAlloc) {
			typedef void(__fastcall* AllocDelta_t)(void*, void*, int);
			((AllocDelta_t)0x004184E0)(buf, nullptr, 16);   // zCArray<int>::AllocDelta
		}
		if (!buf->data || buf->num >= buf->numAlloc || buf->num < 0) {
			ZLog("Kolo: Tab stukniecie (%.0f ms) - bufor klawiszy niedostepny, ekwipunek nie otwarty", heldMs);
			return;
		}
		buf->data[buf->num++] = code;
		ZLog("Kolo: Tab stukniecie (%.0f ms) -> ekwipunek (Tab wstrzykniety do bufora gry, w kolejce %d)", heldMs, buf->num);
	}

	static void WhMouseStep(zCInput_Win32* in) {
		float dx = 0.0f, dy = 0.0f, dz = 0.0f;
		in->GetMousePos(dx, dy, dz);
		float sens = (float)zcfg.wheelMouseSens / 100.0f;
		float dead = (float)zcfg.wheelDeadZone;
		int before = whMouse.slot;
		if (zdcore::WheelMouseMove(whMouse, dx * sens, dy * sens, dead * 3.0f, dead))
			ZDebug("Kolo: mysz -> pole %d (bylo %d)", whMouse.slot, before);
		*(int*)0x008D165C = 0;   // kamera i obrot bohatera bez ruchu myszy
		*(int*)0x008D1660 = 0;
		if (dz != 0.0f) {
			whPageDelta += dz > 0.0f ? -1 : 1;
			*(int*)0x008D1664 = 0;
		}
		bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0, rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		if (lmb && !whLmbPrev && zcfg.wheelClickConfirm)
			whClickReq = true;
		if (rmb && !whRmbPrev) {
			whCancelReq = true;
			whCancelWhy = "PPM";
		}
		whLmbPrev = lmb;
		whRmbPrev = rmb;
	}

	void WheelInputStep(zCInput_Win32* in) {
		if (ENGINE != Engine_G2A || !in)
			return;
		bool on = zcfg.enabled && zcfg.wheel;
		bool focus = WhFocus();
		// fizyczny stan klawisza (GetAsyncKeyState) - zapas, gdy wcisniecie nie przyszlo buforem DirectInput / Raw Input
		WhBindPoll();
		if (zCMenu::GetActive())
			WheelMenuLoop();   // menu gry otwarte - wiersz klawisza kola (zapas, gdy Game_MenuLoop nie biegnie)
		bool phys = on && !whBinding && WhPhysDown(zcfg.wheelKey);
		bool synth = WhCapsKey() && WhSynthActive();
		if (on && phys && !whPhysPrev && !synth && focus) {
			whPhysSession = true;
			whSessionArmed = whKey.state != zdcore::WK_IDLE;
			whSessionRestore = WhCapsKey() && whCapsPatched && zcfg.wheelCapsRestore && !(zcon && zcon->IsVisible());
			whSessionToggle0 = WhCapsToggle();
			if (whKey.state == zdcore::WK_IDLE && whBufPressStamp != zdFrame) {
				bool shared = (zcfg.wheelKey == zcfg.dodgeKey || zcfg.wheelKey == zcfg.rollKey) && MvFightKeysOurs();
				if (!shared && WhTryArm("GetAsyncKeyState"))
					ZLog("Kolo: wcisniecie klawisza %d bez zdarzenia w buforze gry - uzbrojone z GetAsyncKeyState", zcfg.wheelKey);
			}
		}
		if (whPhysPrev && !phys && whPhysSession) {
			whPhysSession = false;
			if (whSessionRestore)
				WhCapsRestore(whSessionToggle0);
		}
		whPhysPrev = phys && !synth;
		if (whKey.state != zdcore::WK_IDLE) {
			double now = ZClockMs();
			bool down = phys || (zcfg.wheelKey > 0 && zcfg.wheelKey < 0x240 && ((unsigned char*)0x008D1678)[zcfg.wheelKey] != 0);
			zdcore::WheelKeyAct a = zdcore::WheelKeyStep(whKey, now, down, focus, zcfg.wheelHoldMs);
			switch (a) {
			case zdcore::WKA_TAP:
				if (WhCapsKey()) {
					if (whCapsPatched && zcfg.wheelTapWalk && WhInGame()) {
						whWalkOn = !whWalkOn;
						ZLog("Kolo: Caps Lock stukniecie (%.0f ms) -> chod %s (plugin, GetState(GAME_SLOW))", now - whArmMs,
							whWalkOn ? "wlaczony" : "wylaczony");
					}
					else
						ZLog("Kolo: Caps Lock stukniecie (%.0f ms) - nic (WheelTapWalk=0)", now - whArmMs);
				}
				else if (zdcore::WheelIsMouseCode(zcfg.wheelKey))
					ZDebug("Kolo: stukniecie przycisku %s (%.0f ms) - nic", WhKeyLabel(zcfg.wheelKey).c_str(), now - whArmMs);
				else if (WhInGame())
					WhInject(zcfg.wheelKey, now - whArmMs);
				else
					ZLog("Kolo: stukniecie poza petla gry (menu / wczytywanie) - bez wstrzykniecia");
				break;
			case zdcore::WKA_OPEN:
				whOpenReq = true;
				break;
			case zdcore::WKA_CONFIRM:
				whConfirmReq = true;
				break;
			case zdcore::WKA_LOST:
				if (whOpen) {
					whCancelReq = true;
					whCancelWhy = "utrata fokusu okna gry";
				}
				else
					ZLog("Kolo: klawisz uzbrojony, a okno gry stracilo fokus - bez kola");
				break;
			default:
				break;
			}
		}
		if (whOpen && WhInGame())
			WhMouseStep(in);
	}

	// ---------------------------------------------------------------- petla gry

	bool WheelIsOpen() {
		return whOpen;
	}

	bool WheelBlocksGameKeys() {
		return whOpen;
	}

	void WheelUpdate() {
		whLoopTick = GetTickCount();
		WhCapsPatchUpdate();   // Caps Lock odczepiony od chodu gry, gdy kolo pod Caps Lock (i z powrotem)
		static int keyLogged = -1;
		if (keyLogged != zcfg.wheelKey && zcfg.wheel) {
			keyLogged = zcfg.wheelKey;
			std::string col = WhKeyConflicts(zcfg.wheelKey);
			ZLog("Kolo: klawisz %s (kod %d)%s%s", WhKeyLabel(zcfg.wheelKey).c_str(), zcfg.wheelKey, col.empty() ? " - bez kolizji" :
				" - UWAGA, ten sam klawisz: ", col.c_str());
		}
		double clock = ZClockMs();
		float realDt = whLoopClock > 0.0 ? (float)std::clamp((clock - whLoopClock) / 1000.0, 0.0, 0.2) : 0.0f;
		whLoopClock = clock;
		WhBaselineUpdate();
		if (whConsoleWait > 0.0f) {
			whConsoleWait -= realDt;
			if (!(zcon && zcon->IsVisible())) {
				whConsoleWait = 0.0f;
				if (!whOpen)
					WhOpen("konsola");
			}
			else if (whConsoleWait <= 0.0f)
				ZLog("Kolo: konsola otwarta dluzej niz %.0f s - otwarcie odwolane", WH_CONSOLE_WAIT_S);
		}
		if (!zcfg.enabled || !zcfg.wheel) {
			if (whOpen)
				WhClose("zamkniete - wylaczone");
			whOpenReq = false;
			whKey = zdcore::WheelKey();
			return;
		}
		if (whOpenReq) {
			whOpenReq = false;
			if (!whOpen && whKey.state == zdcore::WK_OPEN)
				WhOpen(WhKeyName());
			else if (!whOpen)
				ZLog("Kolo: prog przytrzymania minal poza petla gry, Tab juz puszczony - bez kola");
		}
		if (!whOpen) {
			whConfirmReq = whCancelReq = whClickReq = false;
			return;
		}
		// klawisze w kolejce okna (wszystkie wcisniecia polkniete): Esc = anuluj
		zdcore::MenuKey k;
		int dik = 0;
		while (ZMenuNextKey(k, dik))
			if (k == zdcore::MKEY_ESC && !whCancelReq) {
				whCancelReq = true;
				whCancelWhy = "Esc";
			}
		if (whPageDelta != 0 && whPages > 1) {
			int old = whPage;
			whPage = ((whPage + whPageDelta) % whPages + whPages) % whPages;
			whMouse.slot = zdcore::WheelSlotFromVec(whMouse.x, whMouse.y, (float)zcfg.wheelDeadZone);   // ten sam kierunek na nowej stronie
			ZDebug("Kolo: strona %d -> %d z %d", old + 1, whPage + 1, whPages);
		}
		whPageDelta = 0;
		std::string extra;
		zdcore::WheelSense s = WhSense(false, extra);
		zdcore::WheelDeny d = zdcore::WheelDenyCheck(s);
		if (d != zdcore::WDENY_OK && d != zdcore::WDENY_FOREIGN_WINDOW && d != zdcore::WDENY_NOT_HUMAN) {
			WhRestoreTime("przerwanie");
			WhClose((std::string("przerwane - ") + zdcore::WheelDenyText(d) + (extra.empty() ? "" : ": " + extra)).c_str());
			return;
		}
		if (whCancelReq) {
			WhClose((std::string("anulowane (") + whCancelWhy + ")").c_str());
			return;
		}
		if (whClickReq) {
			WhConfirm("LPM");
			return;
		}
		if (whConfirmReq) {
			WhConfirm("puszczenie klawisza");
			return;
		}
	}

	void WheelFinishNow(const char* why) {
		whOpenReq = whConfirmReq = whCancelReq = whClickReq = false;
		whConsoleWait = 0.0f;
		if (whOpen) {
			whOpen = false;
			ZMenuCapture(false, "kolo");
			WhLogStats((std::string("przerwane z zewnatrz (") + why + ")").c_str());
		}
		whKey = zdcore::WheelKey();
		WhRestoreTime(why);
		WhCapsPatchUpdate();   // wylaczone kolo / plugin - chod z Caps Lock znow dla gry
	}

	void WheelLoadBegin() {
		WheelFinishNow("wczytanie / zmiana swiata");
		whCbBaseSet = false;
		whCbBase.clear();
		whCbCalm = 0;
	}

	// ---------------------------------------------------------------- czas (hak zCWorld::AdvanceClock)
	// oCGame::Render 0x6C86E9: world->AdvanceClock(timeStep) - timeStep < 0 (zwykle -1) = ztimer.ResetTimer(), inaczej
	// SetFrameTime(staly krok). Przy tempie < 1: factorMotion = zewnetrzny x tempo (miejsca dzielace przez factorMotion -
	// kamera, obroty, napisy - dostaja czas rzeczywisty), po ResetTimer czas klatki z ulamkiem (frameTimeFloat*) i int z
	// reszta przenoszona dalej (frameTime, totalTime*), bez utraty ulamka.

	static void __fastcall WhAdvanceClock(zCWorld* self, void* edx, float step);
	static CInvoke<void(__fastcall*)(zCWorld*, void*, float)> Ivk_zCWorld_AdvanceClock(
		ENGINE == Engine_G2A ? 0x006260E0 : 0, &WhAdvanceClock, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall WhAdvanceClock(zCWorld* self, void* edx, float step) {
		ZHookHit(ZH_WORLD_ADVANCECLOCK);
		if (ENGINE != Engine_G2A || !ztimer) {
			Ivk_zCWorld_AdvanceClock(self, edx, step);
			return;
		}
		double now = ZClockMs();
		double realDt = whClockPrev > 0.0 ? std::clamp(now - whClockPrev, 0.0, 200.0) : 0.0;
		whClockPrev = now;
		bool aim = zcfg.enabled && zcfg.crossbow && zcfg.xbowAimSlowmo < 100 && WhXbowAimingNow();
		float target = zcfg.enabled ? zdcore::WheelTempoTarget(whOpen && zcfg.wheel, zcfg.wheelSlowmo, aim, zcfg.xbowAimSlowmo) : 1.0f;
		whTempo = zdcore::WheelTempoApproach(whTempo, target, realDt, WH_RAMP_MS);
		if (!whManaging && whTempo >= 0.9999f) {
			Ivk_zCWorld_AdvanceClock(self, edx, step);
			return;
		}
		if (!whManaging) {
			whManaging = true;
			whExternal = ztimer->factorMotion;
			whWritten = whExternal;
			whTime = zdcore::WheelTime();
			ZDebug("Kolo: tempo czasu - start (%s, zewn. factorMotion %.2f -> cel %.2f)", whOpen ? "kolo" : "celowanie z kuszy", whExternal, target);
		}
		else if (fabsf(ztimer->factorMotion - whWritten) > 1e-4f) {
			whExternalChanges++;
			ZLog("Kolo: tempo zewnetrzne %.2f (bylo %.2f) -> kolo %.2f", ztimer->factorMotion, whExternal, ztimer->factorMotion * whTempo);
			whExternal = ztimer->factorMotion;
		}
		if (whTempo >= 0.9999f) {
			ztimer->factorMotion = whExternal;
			whManaging = false;
			ZDebug("Kolo: tempo czasu - koniec (factorMotion %.2f)", whExternal);
			Ivk_zCWorld_AdvanceClock(self, edx, step);
			return;
		}
		float f = whExternal * whTempo;
		ztimer->factorMotion = f;
		whWritten = f;
		unsigned long lastOld = ztimer->lastTimer, totI = ztimer->totalTime;
		float totF = ztimer->totalTimeFloat;
		Ivk_zCWorld_AdvanceClock(self, edx, step);
		if (step >= 0.0f)
			return;   // staly krok (timeStep) - SetFrameTime bez factorMotion, nic do poprawy
		unsigned real = (unsigned)(ztimer->lastTimer - lastOld);
		unsigned engineInt = ztimer->frameTime;
		zdcore::WheelTimeOut o = zdcore::WheelTimeStep(whTime, real, f, 100.0f);
		ztimer->frameTimeFloat = o.frameMs;
		ztimer->frameTimeFloatSecs = o.frameMs / 1000.0f;
		ztimer->frameTime = o.frameInt;
		ztimer->totalTime = totI + o.frameInt;
		ztimer->totalTimeFloat = totF + o.frameMs;
		ztimer->totalTimeFloatSecs = ztimer->totalTimeFloat / 1000.0f;
		if (whOpen) {
			whStats.frames++;
			whStats.realMs += real;
			whStats.gameMs += o.frameMs;
			whStats.engineZero += engineInt == 0 ? 1 : 0;
			whStats.minMs = std::min(whStats.minMs, o.frameMs);
			whStats.maxMs = std::max(whStats.maxMs, o.frameMs);
		}
	}

	// ---------------------------------------------------------------- rysowanie

	static zCView* whBoxView = nullptr;
	static zCView* whBoxHiView = nullptr;
	static zCView* whPanelView = nullptr;
	static zCView* whTextView = nullptr;
	static std::string whPanelBack;

	static const zCOLOR WH_SIGN_COLORS[zdcore::SIGN_COUNT] = {
		zCOLOR(120, 170, 255, 255),   // Aard
		zCOLOR(255, 130, 70, 255),    // Igni
		zCOLOR(120, 230, 120, 255),   // Axii
		zCOLOR(200, 130, 255, 255),   // Yrden
		zCOLOR(255, 210, 80, 255),    // Quen
	};
	static const zCOLOR WH_RUNE(215, 215, 215, 255), WH_SCROLL(230, 200, 150, 255), WH_XBOW(205, 165, 115, 255),
		WH_EMPTY(80, 80, 80, 255), WH_TITLE(255, 220, 90, 255), WH_TEXT(230, 230, 230, 255), WH_HINT(165, 165, 165, 255);

	static zCOLOR WhEntryColor(const zdcore::WheelEntry& e) {
		if (e.kind == zdcore::WE_XBOW)
			return WH_XBOW;
		if (e.kind != zdcore::WE_SPELL || e.spell < 0 || e.spell >= (int)whItems.size())
			return WH_EMPTY;
		const WhItem& it = whItems[e.spell];
		if (it.sign >= 0)
			return WH_SIGN_COLORS[it.sign];
		return it.kind == zdcore::WSK_SCROLL ? WH_SCROLL : WH_RUNE;
	}

	static zCOLOR WhDim(const zCOLOR& c, int pct) {
		return zCOLOR((unsigned char)(c.r * pct / 100), (unsigned char)(c.g * pct / 100), (unsigned char)(c.b * pct / 100), 255);
	}

	static zCView* WhNewView(const char* back) {
		zCView* v = new zCView(0, 0, 8192, 8192);
		v->SetAlphaBlendFunc(zTRnd_AlphaBlendFunc::zRND_ALPHA_FUNC_BLEND);
		if (back)
			v->InsertBack(zSTRING(back));
		return v;
	}

	// Widok w prostokacie w pikselach ekranu (sw x sh).
	static void WhBlitRect(zCView* v, float x, float y, float w, float h, const zCOLOR& c, int transparency) {
		screen->InsertItem(v);
		v->SetPos(screen->anx((int)x), screen->any((int)y));
		v->SetSize(screen->anx(std::max(1, (int)w)), screen->any(std::max(1, (int)h)));
		v->SetColor(c);
		v->SetTransparency(transparency);
		v->Blit();
		screen->RemoveItem(v);
		int vx, vy, vsx, vsy;
		screen->GetViewport(vx, vy, vsx, vsy);
		zrenderer->SetViewport(vx, vy, vsx, vsy);
	}

	static void WhPrintCenter(zCView* v, float xPx, float yPx, const std::string& s, const zCOLOR& c) {
		if (s.empty())
			return;
		v->SetFontColor(c);
		int w = ZmWidth(v, s);
		v->Print(std::max(0, screen->anx((int)xPx) - w / 2), screen->any((int)yPx), zSTRING(s.c_str()));
	}

	void WheelDraw() {
		if (!whOpen || !screen || !zrenderer || !player)
			return;
		ZPROF_SCOPE("Wheel.Draw");
		float sw = (float)std::max(1, screen->nax(8192)), sh = (float)std::max(1, screen->nay(8192));
		float k = sh / 1080.0f;
		float cx = sw * 0.5f, cy = sh * 0.47f;
		float r = (float)zcfg.wheelRadius * k, box = 118.0f * k;
		if (!whBoxView) {
			whBoxView = WhNewView("INV_SLOT.TGA");
			whBoxHiView = WhNewView("INV_SLOT_HIGHLIGHTED.TGA");
			whTextView = WhNewView(nullptr);
		}
		std::string back = GetModuleHandleA("UNION_ABI.DLL") ? "AS_BACK_CRAFT.TGA" : "DLG_CHOICE.TGA";
		if (!whPanelView || back != whPanelBack) {
			if (!whPanelView)
				whPanelView = WhNewView(nullptr);
			whPanelView->InsertBack(zSTRING(back.c_str()));
			whPanelBack = back;
		}
		int hover = WhHoverSlot();
		// panel srodkowy
		float pw = r * 1.32f, ph = r * 0.98f;
		WhBlitRect(whPanelView, cx - pw / 2, cy - ph / 2, pw, ph, zCOLOR(255, 255, 255, 255), 215);
		// pola
		for (int s = 0; s < zdcore::WHEEL_SLOTS; s++) {
			zdcore::WheelEntry e = zdcore::WheelSlotEntry(whOrder, whXbow, whPage, s);
			float ox = 0, oy = 0;
			zdcore::WheelSlotCenter(s, r, ox, oy);
			bool hi = s == hover;
			float b = hi ? box * 1.15f : box;
			zCOLOR c = WhEntryColor(e);
			WhBlitRect(hi ? whBoxHiView : whBoxView, cx + ox - b / 2, cy + oy - b / 2, b, b, hi ? c : WhDim(c, 70), hi ? 255 : 220);
		}
		// teksty
		zCView* t = whTextView;
		screen->InsertItem(t);
		t->SetPos(0, 0);
		t->SetSize(8192, 8192);
		t->ClrPrintwin();
		t->SetFont(zSTRING("FONT_OLD_10_WHITE_HI.TGA"));
		float fy = (float)screen->nay(std::max(1, t->FontY()));
		for (int s = 0; s < zdcore::WHEEL_SLOTS; s++) {
			zdcore::WheelEntry e = zdcore::WheelSlotEntry(whOrder, whXbow, whPage, s);
			if (e.kind == zdcore::WE_EMPTY)
				continue;
			float ox = 0, oy = 0;
			zdcore::WheelSlotCenter(s, r, ox, oy);
			bool active = (e.kind == zdcore::WE_XBOW && whXbowSel) || (e.kind == zdcore::WE_SPELL && !whXbowSel && e.spell == whActiveSpell);
			zCOLOR c = s == hover ? WH_TITLE : active ? zCOLOR(255, 240, 170, 255) : WH_TEXT;
			std::string l1, l2;
			if (e.kind == zdcore::WE_XBOW) {
				l1 = "Kusza";
				l2 = whXbowBolts >= 999 ? "be\xB3ty: bez limitu" : "be\xB3ty: " + std::to_string(whXbowBolts);
			}
			else {
				const WhItem& it = whItems[e.spell];
				std::vector<std::string> w = ZmWrap(t, it.name, screen->anx((int)(box * 1.6f)));
				l1 = w.size() > 0 ? zdcore::WheelShortName(w[0], 18) : "";
				l2 = w.size() > 1 ? zdcore::WheelShortName(w[1], 18) : "";
				if (it.kind == zdcore::WSK_SCROLL)
					l2 = (l2.empty() ? "" : l2 + " ") + "x" + std::to_string(it.amount);
			}
			float ty = cy + oy - (l2.empty() ? fy / 2 : fy);
			WhPrintCenter(t, cx + ox, ty, l1, c);
			WhPrintCenter(t, cx + ox, ty + fy, l2, c);
			if (active)
				WhPrintCenter(t, cx + ox, cy + oy - box / 2 - fy * 1.1f, "[F]", zCOLOR(255, 240, 170, 255));
		}
		// opis na srodku
		std::vector<std::pair<std::string, zCOLOR>> lines;
		zdcore::WheelEntry he = zdcore::WheelSlotEntry(whOrder, whXbow, whPage, hover);
		if (he.kind == zdcore::WE_XBOW) {
			lines.push_back({ whXbowName, WH_TITLE });
			lines.push_back({ whXbowBolts >= 999 ? "Be\xB3ty: bez limitu" : "Be\xB3ty: " + std::to_string(whXbowBolts), WH_TEXT });
			std::string cd = zdcore::WheelCooldownText(whXbowCd);
			if (!cd.empty())
				lines.push_back({ cd, WH_TEXT });
			lines.push_back({ "F: stukni\xEA" "cie / przytrzymanie", WH_HINT });
		}
		else if (he.kind == zdcore::WE_SPELL) {
			const WhItem& it = whItems[he.spell];
			lines.push_back({ it.name, WH_TITLE });
			lines.push_back({ zdcore::WheelKindText(it.kind, it.amount), it.sign >= 0 ? WH_SIGN_COLORS[it.sign] : WH_TEXT });
			for (auto& l : it.lines)
				lines.push_back({ l, WH_TEXT });
			if (it.circle > 0)
				lines.push_back({ "Kr\xB9g " + std::to_string(it.circle), WH_TEXT });
			std::string cd = zdcore::WheelCooldownText(WhQsCooldown());
			if (!cd.empty())
				lines.push_back({ cd, WH_TEXT });
		}
		else
			lines.push_back({ whItems.empty() && !whXbow ? "Brak czar\xF3w" : "Wybierz myszk\xB9", WH_HINT });
		std::string pg = zdcore::WheelPageText(whPage, whPages);
		if (!pg.empty())
			lines.push_back({ "< " + pg + " >  (rolka)", WH_HINT });
		float ly = cy - (float)lines.size() * fy * 1.15f / 2.0f;
		for (auto& l : lines) {
			WhPrintCenter(t, cx, ly, l.first, l.second);
			ly += fy * 1.15f;
		}
		WhPrintCenter(t, cx, cy + r + box * 0.62f + fy * 0.4f,
			std::string("Pu\x9C\xE6 ") + WhKeyName() + (zcfg.wheelClickConfirm ? " / LPM - wybierz    PPM / Esc - anuluj" :
				" - wybierz    PPM / Esc - anuluj"), WH_HINT);
		t->Blit();
		screen->RemoveItem(t);
		int vx, vy, vsx, vsy;
		screen->GetViewport(vx, vy, vsx, vsy);
		zrenderer->SetViewport(vx, vy, vsx, vsy);
	}

	// ---------------------------------------------------------------- konsola, eksport

	std::string WheelConsole(const std::string& arg) {
		std::string a = zdcore::Upper(arg);
		if (a == "CLOSE") {
			bool was = whOpen;
			WheelFinishNow("zdism wheel close");
			return was ? "kolo zamkniete" : "kolo nie bylo otwarte";
		}
		if (a == "BIND" || a == "KEY") {
			WhBindStart("konsola / menu");
			return "wcisnij klawisz albo przycisk myszy dla kola (Esc anuluje)";
		}
		if (a == "OPEN") {
			if (!zcfg.wheel)
				return "kolo wylaczone (Wheel=0)";
			whConsoleWait = WH_CONSOLE_WAIT_S;
			return "kolo otworzy sie po zamknieciu konsoli (zamkniecie: LPM / PPM / Esc)";
		}
		std::vector<void*> cbs;
		bool inList = WhCallbacksBefore(cbs);
		void* foreign = WhForeignCallback();
		char buf[1200];
		sprintf_s(buf, "kolo %s, klawisz %d (stan %s), prog %d ms, tempo %d%% (celowanie z kuszy %d%%), martwa strefa %d, promien %d, "
			"czulosc %d%%, LPM %d; otwarte %d (strona %d/%d, czarow %d, kusza %d); tempo teraz %.2f, factorMotion %.2f (zewn. %.2f, "
			"nasze %d); eksporty: ZQS_Busy %s, ZQS_Select %s, ZTR_RewindActive %s; callbacki przed gra %d%s (baza %s %d), obcy %s; Caps Lock: "
			"odczepiony %d%s, chod pluginu %d, uzbrojenia z bufora %d / z GetAsyncKeyState %d, przelacznik Windows %d",
			zcfg.wheel ? "wl." : "wyl.", zcfg.wheelKey, zdcore::WheelKeyStateName(whKey.state), zcfg.wheelHoldMs, zcfg.wheelSlowmo,
			zcfg.xbowAimSlowmo, zcfg.wheelDeadZone, zcfg.wheelRadius, zcfg.wheelMouseSens, zcfg.wheelClickConfirm, whOpen, whPage + 1,
			whPages, (int)whItems.size(), whXbow, whTempo, ztimer ? ztimer->factorMotion : 0.0f, whExternal, whManaging,
			WhExportGet(whZqsBusy, WH_ZQS_MODS, 2) ? "tak" : "nie", WhExportGet(whZqsSelect, WH_ZQS_MODS, 2) ? "tak" : "nie",
			WhExportGet(whZtrActive, WH_ZTR_MODS, 2) ? "tak" : "nie", (int)cbs.size(), inList ? "" : " (ogame poza lista)",
			whCbBaseSet ? "gotowa" : "jeszcze nie", (int)whCbBase.size(), foreign ? WhCbDesc(foreign).c_str() : "brak", whCapsPatched,
			whCapsPatchFailed ? " (BLAD laty)" : "", whWalkOn, whArmBuf, whArmPoll, WhCapsToggle());
		ZLog("Kolo (konsola): %s", buf);
		return buf;
	}

#if ENGINE == Engine_G2A
	// Dla innych pluginow (zQuickSign): 1 = kolo otwarte (klawisze polykane, czas zwolniony).
	extern "C" __declspec(dllexport) int __cdecl ZDISM_WheelOpen() {
		return whOpen ? 1 : 0;
	}
#endif
}
