// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Zamiana animacji 1H <-> 2H bohatera (AniSwap.h, Core/AniSwap). Silnik G2A:
	// - oCAniCtrl_Human::InitAnimations 0x6A4010 -> InitAllAnis 0x6A5BF0: tablice id animacji per tryb broni (int[10],
	//   s_run..t_jump_2_runl 0x2D8-0xA08, s_sneak..t_hitfrun 0xA20-0xFC0, s_weapon 0x11B0); napisy "1H" / "2H" wpisane
	//   w kod (bez GetWeaponString), w petli po trybach 0..7 tez InitFightAnis(tryb) 0x6AA0B0 (tablice walki 0xD18-0xFC0).
	//   Po oryginale tablice bohatera przestawiane wg zdcore::SwapTable (arr[tryb] = arr[map[tryb]]) - bez s_weapon
	//   (poza S_1H w dobyciu broni, zdcore::SwapModeArrayRanges).
	// - Dobycie broni na postoju (17.10, zaciecie z reka na rekojesci): EV_DrawWeapon1 0x74D2E0 -> DrawWeapon1 0x6B2610
	//   startuje natywne T_RUN_2_1H (nastepna S_1H) i czeka, az zagra s_weapon[tryb]; DrawWeapon2 0x6B2CF0 startuje
	//   T_1H_2_1HRUN z nastepna S_1HRUN (po nazwie), a EV_DrawWeapon2 0x74D580 konczy dopiero, gdy gra _s_walk (+0x1008,
	//   po zamianie S_2HRUN). Stad: s_weapon natywne, a w trakcie komunikatu broni nastepna / grana postawa natywna
	//   przestawiana na postawe po zamianie (SwDrawGuard); straznik po 2 s przywraca natywne tablice do konca komunikatu.
	// - InitFightAnis(tryb) wolane osobno (SetTalentSkill 0x731616, Reset 0x6A5091): oryginal dla trybu zrodlowego,
	//   wynik przeniesiony do trybu broni, stare wartosci trybu zrodlowego wracaja.
	// - Nazwy skladane w biegu przez GetWeaponString 0x6AEC60 / GetWeaponHitString 0x6AEEF0 (static, cdecl, zwrot zSTRING
	//   przez ukryty wskaznik): oCNpc::EV_Parade 0x7522D0 (T_1HPARADE_0...), oCAIHuman::FightMelee 0x696EC0
	//   (T_1HSPECIALATTACK, T_1HSFINISH), CanToggleWalkModeTo 0x6ABFB0 / CanToggleWalkMode 0x6ACC10 - przedrostek
	//   zamieniony tylko w tych funkcjach i tylko dla bohatera (swCtx). Dobycie / schowanie / zmiana broni (EV_DrawWeapon
	//   0x74CC10 T_MOVE_2_1HMOVE, DrawWeapon1/2, RemoveWeapon1/2, EV_RemoveWeapon, EV_ChooseWeapon) poza tymi funkcjami -
	//   natywne. oCNpc::GetWeaponMode 0x738C40, SetWeaponMode2 i SetFightAnis 0x6AAA40 (tablice AI walki) bez zmian.
	// - Nakladki talentu (oCNpc::SetTalentSkill 0x730F60: HUMANS_1HST1/2, HUMANS_2HST1/2 przez oCNpc::ApplyOverlay -
	//   lista oCNpc::activeOverlays +0x928 idzie do zapisu). Zamiana zmienia je tylko na modelu (zCModel::Apply /
	//   RemoveModelProtoOverlay), activeOverlays zostaje natywna = wzorzec przywrocenia (tez po wczytaniu i zmianie swiata).
	static oCAniCtrl_Human* swAc = nullptr;                     // sterownik, ktorego tablice przestawilismy
	static zdcore::SwapTable swTable = zdcore::SwapIdentity();  // tablica zastosowana w tablicach swAc
	static int swCtx = 0;       // > 0: funkcja silnika sklada nazwy animacji bohatera (GetWeaponString z zamiana)
	static int swInAll = 0;     // InitAllAnis w toku - tablice przestawiane po oryginale, napisy bez zamiany

	struct SwCurrent {
		int wm = 0;                 // tryb walki (prawdziwy)
		int type = zdcore::SWAP_TYPE_NONE;   // bron biala w rece albo zalozona
		std::string instance, name; // instancja (ITMW_...) i nazwa z gry (cp1250)
		oCItem* item = nullptr;
		int instanz = -1;           // indeks instancji (z wskaznikiem - klucz pamieci nazwy)
		int dual = zdcore::DUAL_NONE;   // bron parowa New Balance (zdcore::DualState) - zawsze natywne animacje NB
	};
	static SwCurrent swCur;
	static unsigned swDeferSince = 0;     // klatka pierwszego odlozenia biezacej zmiany
	static std::string swDeferLogged;     // powod ostatnio zalogowanego odlozenia
	static std::string swOvlKey;          // podpis wykonanego planu nakladek (chciane|natywne)
	static bool swOvlTouched = false;     // nakladki talentu na modelu inne niz natywne (nasze)
	static std::string swOvlState = "natywne";
	static std::vector<std::string> swOvlFailed;   // nie dalo sie zalozyc - bez ponawiania
	static bool swPrevKey = false;
	static bool swReloadOnce = false;     // po wczytaniu / zmianie swiata: stan tablic nieznany - jedna przebudowa
	static bool swBuilt = false;          // InitAllAnis bohatera od ostatniego wpisu w logu
	static std::string swLogged;          // ostatnio zalogowany stan przebudowy
	static std::string swMissLogged;      // ostatnio zalogowana diagnostyka brakujacych nazw
	static int swNatStance[zdcore::SWAP_WM_COUNT][3];   // natywne S_xRUN / S_xWALK / S_xSNEAK per tryb (przed zamiana)
	static bool swSuspend = false;        // straznik dobycia: natywne tablice do konca komunikatu broni
	static zdcore::SwapWeaponWatch swWatch;
	static std::string swWatchSig;        // ostatnio zalogowany stan komunikatu broni (Debug)
	static int swWatchLines = 0;          // wpisow stanu na komunikat (limit)
	static bool swLayoutOk = true;        // offsety tablic z Core zgodne z naglowkiem silnika
	static std::string SwActiveAnis(zCModel* m);

	static bool SwModelHuman(zCModel* m) {
		return m && _strnicmp(m->GetVisualName().ToChar(), "HUMANS", 6) == 0;
	}

	static bool SwHeroAc(oCAniCtrl_Human* ac) {
		return ac && player && ac->npc == player && SwModelHuman(ac->model);
	}

	// ---------------------------------------------------------------- bronie parowe New Balance (23.10)
	// Para *_RIGHT / *_LEFT: zmienne skryptu RH_READY_2X2 / LH_READY_2X2 (sa w zapisie), bez nich nakladki
	// HUMANS_1X2ST3 / HUMANS_2X2ST3 w oCNpc::activeOverlays (zdcore::DualState). Steam G2 / Kroniki: brak zmiennych
	// i broni *_RIGHT - zawsze DUAL_NONE.
	static zCPar_Symbol* SwIntSym(const char* name, zCPar_Symbol*& cache, zCParser*& owner, unsigned& retry) {
		if (owner != parser) {
			owner = parser;
			cache = nullptr;
			retry = 0;
		}
		if (!cache && parser && zdFrame >= retry) {
			retry = zdFrame + 300;   // brak symbolu (inna gra) - ponowna proba za ok. 5 s, nie co klatke
			zCPar_Symbol* s = parser->GetSymbol(zSTRING(name));
			cache = s && s->type == zPAR_TYPE_INT ? s : nullptr;
		}
		return cache;
	}

	static int SwDualOf(oCItem* weapon) {
		if (!player)
			return zdcore::DUAL_NONE;
		static zCPar_Symbol* rh = nullptr, * lh = nullptr;
		static zCParser* rhOwner = nullptr, * lhOwner = nullptr;
		static unsigned rhRetry = 0, lhRetry = 0;
		zdcore::DualInput in;
		if (zCPar_Symbol* a = SwIntSym("RH_READY_2X2", rh, rhOwner, rhRetry)) {
			int v = 0;
			a->GetValue(v, 0);
			in.rhVar = v;
		}
		if (zCPar_Symbol* b = SwIntSym("LH_READY_2X2", lh, lhOwner, lhRetry)) {
			int v = 0;
			b->GetValue(v, 0);
			in.lhVar = v;
		}
		if (in.rhVar < 0)
			for (int i = 0; i < player->activeOverlays.GetNum(); i++)
				in.overlays.push_back(player->activeOverlays[i].ToChar());
		if (weapon) {
			in.weapon = weapon->GetInstanceName().ToChar();
			in.weaponType = zdcore::SwapTypeOfItemFlags(weapon->flags);
		}
		return zdcore::DualState(in);
	}

	static oCItem* SwHeroMelee() {
		int wm = player->GetWeaponMode();
		oCItem* w = wm >= NPC_WEAPON_DAG && wm <= NPC_WEAPON_2HS ? player->GetWeapon() : nullptr;
		return w ? w : player->GetEquippedMeleeWeapon();
	}

	int HeroDualState() {
		static unsigned frame = 0;
		static int state = zdcore::DUAL_NONE, logged = zdcore::DUAL_NONE;
		if (frame && frame == zdFrame)
			return state;
		frame = zdFrame ? zdFrame : 1;
		state = player && player->homeWorld && HeroIsHuman() ? SwDualOf(SwHeroMelee()) : zdcore::DUAL_NONE;
		if (state != logged) {
			logged = state;
			oCItem* w = state && player ? SwHeroMelee() : nullptr;
			ZLog("Bron parowa NB: %s%s%s - %s", state ? zdcore::DualLabel(state) : "brak", w ? ", bron " : "", w ? w->GetInstanceName().ToChar() : "",
				state ? "natywne animacje New Balance (bez zamiany 1H <-> 2H, stylu W3 i talentu nad HUMANS_1X2ST3 / 2X2ST3, dobycie "
				"i polozenie broni natywne); klawisz stylu przy parze tylko informuje" : "koniec pary - wybor stylu 1H / 2H jak przed para");
		}
		return state;
	}

	// Bron biala bohatera: w rece (dobyta 1H / 2H / sztylet), inaczej zalozona.
	static SwCurrent SwReadCurrent() {
		SwCurrent c;
		if (!player)
			return c;
		c.wm = player->GetWeaponMode();
		oCItem* w = c.wm >= NPC_WEAPON_DAG && c.wm <= NPC_WEAPON_2HS ? player->GetWeapon() : nullptr;
		if (!w)
			w = player->GetEquippedMeleeWeapon();
		if (w) {
			c.item = w;
			c.instanz = w->instanz;
			c.type = zdcore::SwapTypeOfItemFlags(w->flags);
			if (w == swCur.item && w->instanz == swCur.instanz && !swCur.instance.empty()) {   // nazwa raz na przedmiot
				c.instance = swCur.instance;
				c.name = swCur.name;
			}
			else {
				c.instance = w->GetInstanceName().ToChar();
				c.name = w->name.ToChar();
			}
		}
		int t = zdcore::SwapTypeOfWeaponMode(c.wm);
		if (t != zdcore::SWAP_TYPE_NONE)
			c.type = t;   // tryb dobytej broni rozstrzyga
		// bez pamieci z klatki: wolane tez w haku InitAllAnis w trakcie skryptu zalozenia (RH_READY_2X2 juz ustawione)
		c.dual = w ? SwDualOf(w) : zdcore::DUAL_NONE;
		return c;
	}

	static zdcore::SwapTable SwWantedTable(const SwCurrent& c) {
		if (!zcfg.enabled || swSuspend || !swLayoutOk || c.dual != zdcore::DUAL_NONE)
			return zdcore::SwapIdentity();   // bron parowa NB: natywne tablice (animacje 1x2 / 2x2 sa w trybie 2H)
		return zdcore::SwapBuildTable(zcfg.swap, c.type, c.instance);
	}

	// Offsety z Core (zdcore::SwapModeArrayRanges) a pola naglowka silnika - raz; niezgodne = zamiana wylaczona.
	static bool SwCheckLayout(oCAniCtrl_Human* ac) {
		static int checked = 0;
		if (checked)
			return checked > 0;
		auto ofs = [ac](const void* p) { return (int)((const char*)p - (const char*)ac); };
		const auto& r = zdcore::SwapModeArrayRanges();
		bool ok = r.size() == 2 && ofs(ac->s_run) == r[0].begin && ofs(&ac->t_stand_2_jumpuplow) == r[0].end &&
			ofs(ac->s_sneak) == r[1].begin && ofs(&ac->t_stumble) == r[1].end && ofs(ac->s_weapon) == zdcore::SWAP_OFS_S_WEAPON &&
			zdcore::SWAP_ARRAY_STRIDE == (int)sizeof(ac->s_run);
		checked = ok ? 1 : -1;
		if (!ok)
			ZLog("Zamiana animacji: UWAGA uklad oCAniCtrl_Human inny niz w Core (s_run %d, s_sneak %d, s_weapon %d) - zamiana wylaczona",
				ofs(ac->s_run), ofs(ac->s_sneak), ofs(ac->s_weapon));
		return ok;
	}

	// Kazda przestawiana tablica id animacji per tryb broni (int[ANI_NUM], tryby 0..7) - bez s_weapon (dobycie broni).
	template <class F> static void SwForEachModeArray(oCAniCtrl_Human* ac, F f) {
		for (auto& r : zdcore::SwapModeArrayRanges())
			for (int o = r.begin; o < r.end; o += zdcore::SWAP_ARRAY_STRIDE)
				f((int*)((char*)ac + o));
	}

	// ---------------------------------------------------------------- haki silnika

	// oCAniCtrl_Human::InitAllAnis - wszystkie tablice per tryb broni (z InitAnimations). Po oryginale dla bohatera:
	// przestawienie wg tablicy chcianej teraz (bron w rece / zalozona, ustawienia). Inny sterownik - bez zmian.
	static void __fastcall SwInitAllAnis(oCAniCtrl_Human* self, void* edx);
	static CInvoke<void(__fastcall*)(oCAniCtrl_Human*, void*)> Ivk_oCAniCtrl_Human_InitAllAnis(
		ENGINE == Engine_G2A ? 0x006A5BF0 : 0, &SwInitAllAnis, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwInitAllAnis(oCAniCtrl_Human* self, void* edx) {
		ZHookHit(ZH_INITALLANIS);
		swInAll++;
		Ivk_oCAniCtrl_Human_InitAllAnis(self, edx);
		swInAll--;
		if (!SwHeroAc(self)) {
			if (self == swAc) {   // sterownik bez naszej zamiany (np. bohater w innym modelu)
				swAc = nullptr;
				swTable = zdcore::SwapIdentity();
			}
			return;
		}
		ZPROF_SCOPE("AniSwap.Init");   // przestawienie tablic animacji bohatera
		swLayoutOk = SwCheckLayout(self);
		for (int wm = 0; wm < zdcore::SWAP_WM_COUNT; wm++) {   // postawy natywne - koniec dobycia broni (SwDrawGuard)
			swNatStance[wm][0] = self->s_run[wm];
			swNatStance[wm][1] = self->s_walk[wm];
			swNatStance[wm][2] = self->s_sneak[wm];
		}
		swCur = SwReadCurrent();
		zdcore::SwapTable t = SwWantedTable(swCur);
		if (!zdcore::SwapIsIdentity(t))
			SwForEachModeArray(self, [&](int* a) { zdcore::SwapRemapModeArray(a, t); });
		swAc = self;
		swTable = t;
		swBuilt = true;
	}

	// oCAniCtrl_Human::InitFightAnis(tryb) - tablice walki jednego trybu. W InitAllAnis bez zmian (przestawia je hak
	// wyzej); wolane osobno dla bohatera z zamiana: oryginal dla trybu zrodlowego i przeniesienie wyniku.
	static void __fastcall SwInitFightAnis(oCAniCtrl_Human* self, void* edx, int wm);
	static CInvoke<void(__fastcall*)(oCAniCtrl_Human*, void*, int)> Ivk_oCAniCtrl_Human_InitFightAnis(
		ENGINE == Engine_G2A ? 0x006AA0B0 : 0, &SwInitFightAnis, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwInitFightAnis(oCAniCtrl_Human* self, void* edx, int wm) {
		ZHookHit(ZH_INITFIGHTANIS);
		int src = zdcore::SwapMapWeaponMode(swTable, wm);
		if (swInAll > 0 || self != swAc || src == wm || wm < 0 || wm >= zdcore::SWAP_WM_COUNT || !SwHeroAc(self)) {
			Ivk_oCAniCtrl_Human_InitFightAnis(self, edx, wm);
			return;
		}
		std::vector<int> before;
		SwForEachModeArray(self, [&](int* a) { before.push_back(a[src]); });
		Ivk_oCAniCtrl_Human_InitFightAnis(self, edx, src);
		size_t i = 0;
		SwForEachModeArray(self, [&](int* a) {
			int v = a[src];
			a[src] = before[i++];
			a[wm] = v;
		});
	}

	// Zakres funkcji silnika skladajacej nazwy animacji bohatera w biegu - GetWeaponString z zamiana.
	struct SwCtxScope {
		bool on;
		explicit SwCtxScope(oCAniCtrl_Human* ac) : on(ac && ac == swAc && !zdcore::SwapIsIdentity(swTable) && SwHeroAc(ac)) {
			if (on)
				swCtx++;
		}
		~SwCtxScope() {
			if (on)
				swCtx--;
		}
	};

	// static zSTRING oCAniCtrl_Human::GetWeaponString(int) - "FIST" / "1H" / "2H" / "BOW"... (cdecl, ukryty wskaznik zwrotu).
	static zSTRING* __cdecl SwGetWeaponString(zSTRING* ret, int wm);
	static CInvoke<zSTRING*(__cdecl*)(zSTRING*, int)> Ivk_oCAniCtrl_Human_GetWeaponString(
		ENGINE == Engine_G2A ? 0x006AEC60 : 0, &SwGetWeaponString, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static zSTRING* __cdecl SwGetWeaponString(zSTRING* ret, int wm) {
		if (swCtx > 0 && swInAll == 0)
			wm = zdcore::SwapMapWeaponMode(swTable, wm);
		return Ivk_oCAniCtrl_Human_GetWeaponString(ret, wm);
	}

	// static zSTRING oCAniCtrl_Human::GetWeaponHitString(int) - "1HS" / "2HS" (T_1HSFINISH w FightMelee).
	static zSTRING* __cdecl SwGetWeaponHitString(zSTRING* ret, int wm);
	static CInvoke<zSTRING*(__cdecl*)(zSTRING*, int)> Ivk_oCAniCtrl_Human_GetWeaponHitString(
		ENGINE == Engine_G2A ? 0x006AEEF0 : 0, &SwGetWeaponHitString, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static zSTRING* __cdecl SwGetWeaponHitString(zSTRING* ret, int wm) {
		if (swCtx > 0 && swInAll == 0)
			wm = zdcore::SwapMapWeaponMode(swTable, wm);
		return Ivk_oCAniCtrl_Human_GetWeaponHitString(ret, wm);
	}

	// oCNpc::EV_Parade - parada: T_1HPARADE_0 (+ _A2 / _A3) z GetWeaponString.
	static int __fastcall SwEV_Parade(oCNpc* self, void* edx, oCMsgAttack* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgAttack*)> Ivk_oCNpc_EV_Parade(
		ENGINE == Engine_G2A ? 0x007522D0 : 0, &SwEV_Parade, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SwEV_Parade(oCNpc* self, void* edx, oCMsgAttack* msg) {
		ZHookHit(ZH_EV_PARADE);
		SwCtxScope ctx(self && self == player && swAc ? self->GetAnictrl() : nullptr);
		return Ivk_oCNpc_EV_Parade(self, edx, msg);
	}

	// oCAIHuman::FightMelee - walka wrecz sterowana klawiszami: T_1HSPECIALATTACK (GetWeaponString), T_1HSFINISH
	// (dobicie, GetWeaponHitString).
	static int __fastcall SwFightMelee(oCAIHuman* self, void* edx);
	static CInvoke<int(__fastcall*)(oCAIHuman*, void*)> Ivk_oCAIHuman_FightMelee(
		ENGINE == Engine_G2A ? 0x00696EC0 : 0, &SwFightMelee, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SwFightMelee(oCAIHuman* self, void* edx) {
		ZHookHit(ZH_FIGHTMELEE);
		SwCtxScope ctx(self);
		return Ivk_oCAIHuman_FightMelee(self, edx);
	}

	// oCAniCtrl_Human::CanToggleWalkModeTo(int) / CanToggleWalkMode() - czy sa przejscia T_1HRUN_2_1HWALK... dla broni.
	static int __fastcall SwCanToggleWalkModeTo(oCAniCtrl_Human* self, void* edx, int mode);
	static CInvoke<int(__fastcall*)(oCAniCtrl_Human*, void*, int)> Ivk_oCAniCtrl_Human_CanToggleWalkModeTo(
		ENGINE == Engine_G2A ? 0x006ABFB0 : 0, &SwCanToggleWalkModeTo, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SwCanToggleWalkModeTo(oCAniCtrl_Human* self, void* edx, int mode) {
		SwCtxScope ctx(self);
		return Ivk_oCAniCtrl_Human_CanToggleWalkModeTo(self, edx, mode);
	}

	static int __fastcall SwCanToggleWalkMode(oCAniCtrl_Human* self, void* edx);
	static CInvoke<int(__fastcall*)(oCAniCtrl_Human*, void*)> Ivk_oCAniCtrl_Human_CanToggleWalkMode(
		ENGINE == Engine_G2A ? 0x006ACC10 : 0, &SwCanToggleWalkMode, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SwCanToggleWalkMode(oCAniCtrl_Human* self, void* edx) {
		SwCtxScope ctx(self);
		return Ivk_oCAniCtrl_Human_CanToggleWalkMode(self, edx);
	}

	// oCAniCtrl_Human::CorrectAniStates - co klatke (oCAIHuman::DoAI 0x69BF04) wygasza grane postawy innych trybow broni
	// (petla po trybach 0..7 poza biezacym i 2: FadeOutAni(s_walk / s_run / s_sneak[i])). Po zamianie tryby z tym samym
	// zrodlem maja te same id (zdcore::SwapCollidingModes) - na czas wywolania ich postawy = -1, potem wracaja.
	static void __fastcall SwCorrectAniStates(oCAniCtrl_Human* self, void* edx);
	static CInvoke<void(__fastcall*)(oCAniCtrl_Human*, void*)> Ivk_oCAniCtrl_Human_CorrectAniStates(
		ENGINE == Engine_G2A ? 0x006AAFA0 : 0, &SwCorrectAniStates, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwCorrectAniStates(oCAniCtrl_Human* self, void* edx) {
		if (self != swAc || zdcore::SwapIsIdentity(swTable) || !SwHeroAc(self)) {
			Ivk_oCAniCtrl_Human_CorrectAniStates(self, edx);
			return;
		}
		std::vector<int> modes = zdcore::SwapCollidingModes(swTable, self->npc->GetWeaponMode());
		struct Saved { int mode, run, walk, sneak; };
		Saved saved[zdcore::SWAP_WM_COUNT];
		int n = 0;
		for (int i : modes) {
			saved[n++] = { i, self->s_run[i], self->s_walk[i], self->s_sneak[i] };
			self->s_run[i] = self->s_walk[i] = self->s_sneak[i] = -1;
		}
		Ivk_oCAniCtrl_Human_CorrectAniStates(self, edx);
		for (int k = 0; k < n; k++) {
			self->s_run[saved[k].mode] = saved[k].run;
			self->s_walk[saved[k].mode] = saved[k].walk;
			self->s_sneak[saved[k].mode] = saved[k].sneak;
		}
	}

	// ---------------------------------------------------------------- przebudowa w locie

	// Biezace animacje walki z tablic (pierwsza czesc oCAniCtrl_Human::SetFightAnis 0x6AAA40 - bez tablic AI walki i bez
	// haka zEBA na SetFightAnis, ktory przy tej samej broni zakladalby styl od nowa).
	static void SwRefreshFightAnis(oCAniCtrl_Human* ac, int wm) {
		if (wm < 0 || wm >= zdcore::SWAP_WM_COUNT)
			return;
		ac->_t_walk_2_aim = ac->t_walk_2_aim[wm];
		ac->_t_walkl_2_aim = ac->t_walkl_2_aim[wm];
		ac->_t_walkr_2_aim = ac->t_walkr_2_aim[wm];
		ac->_s_aim = ac->s_aim[wm];
		ac->_t_aim_2_walk = ac->t_aim_2_walk[wm];
		if (wm <= NPC_WEAPON_2HS) {
			ac->_t_hitl = ac->t_hitl[wm];
			ac->_t_hitr = ac->t_hitr[wm];
			ac->_t_hitf = ac->t_hitf[wm];
			ac->_t_hitfrun = ac->t_hitfrun[wm];
			ac->_t_paradeL = ac->t_paradeL[wm];
		}
		else if (wm == NPC_WEAPON_BOW || wm == NPC_WEAPON_CBOW) {
			ac->_t_hitback = ac->t_hitback[wm];
			ac->_t_hitf = ac->t_hitf[wm];
			ac->_s_hitf = ac->s_hitf[wm];
		}
	}

	// InitAnimations (z hakiem InitAllAnis - tablice wg biezacej zamiany), biezace animacje walki i postawa: gdy grala
	// stara postawa (_s_walk), od razu nowa (z przejsciem silnika) - bez tego IsStateAniActive nie widzi postoju.
	// Po przebudowie: kazda grana animacja, ktora byla w tablicy trybu broni (postawa, petla biegu, przejscie...), przechodzi
	// w odpowiednik z nowej tablicy z ta sama faza (S_1HRUNL 0.4 -> S_2HRUNL 0.4) - przelaczenie w biegu bez stopu
	// i bez czekania na kolejny stan; StartAni przy granej starej robi zwykle przejscie silnika.
	static void SwReload(oCAniCtrl_Human* ac, zCModel* m) {
		ZPROF_SCOPE("AniSwap.Reload");   // przebudowa animacji bohatera w locie
		int wm = player->GetWeaponMode();
		bool armed = wm > 0 && wm < zdcore::SWAP_WM_COUNT;
		std::vector<int> before, after;
		if (armed)
			SwForEachModeArray(ac, [&](int* a) { before.push_back(a[wm]); });
		ac->InitAnimations();
		SwRefreshFightAnis(ac, wm);
		if (armed) {
			SwForEachModeArray(ac, [&](int* a) { after.push_back(a[wm]); });
			struct Move { zCModelAni* to; float progress; std::string from; };
			std::vector<Move> moves;
			for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
				zCModelAniActive* a = m->aniChannels[i];
				if (!a || !a->protoAni || a->isFadingOut)
					continue;
				for (size_t k = 0; k < before.size() && k < after.size(); k++)
					if (before[k] == a->protoAni->aniID && after[k] >= 0 && after[k] != before[k]) {
						if (zCModelAni* to = m->GetAniFromAniID(after[k]))
							moves.push_back({ to, a->GetProgressPercent(), a->protoAni->aniName.ToChar() });
						break;
					}
			}
			for (auto& mv : moves) {
				if (m->GetActiveAni(mv.to))
					continue;
				m->StartAni(mv.to, 0);
				if (zCModelAniActive* n = m->GetActiveAni(mv.to))
					n->SetProgressPercent(mv.progress);
				ZDebug("Zamiana animacji: przebudowa - %s -> %s (faza %.2f)", mv.from.c_str(), mv.to->aniName.ToChar(), mv.progress);
			}
		}
		if (ac != swAc) {   // InitAllAnis nie uznal sterownika za bohatera (nie powinno sie zdarzyc) - bez petli
			swAc = ac;
			swTable = zdcore::SwapIdentity();
		}
	}

	// ---------------------------------------------------------------- nakladki talentu

	static std::vector<std::string> SwNativeOverlays() {
		std::vector<std::string> out;
		for (int i = 0; i < player->activeOverlays.GetNum(); i++)
			out.push_back(player->activeOverlays[i].ToChar());
		return out;
	}

	static std::vector<std::string> SwWantedOverlays(const std::vector<std::string>& native) {
		bool active = zcfg.enabled && swCur.dual == zdcore::DUAL_NONE && zdcore::SwapActiveFor(zcfg.swap, swCur.type, swCur.instance);
		std::vector<std::string> w = zdcore::SwapWantedOverlays(native, active, zcfg.swap.level, swCur.type,
			player->GetTalentSkill(oCNpcTalent::NPC_TAL_1H), player->GetTalentSkill(oCNpcTalent::NPC_TAL_2H));
		w.erase(std::remove_if(w.begin(), w.end(), [](const std::string& n) {
			return std::find(swOvlFailed.begin(), swOvlFailed.end(), n) != swOvlFailed.end();
		}), w.end());
		return w;
	}

	static std::string SwJoin(const std::vector<std::string>& v) {
		std::string s;
		for (auto& x : v)
			s += (s.empty() ? "" : " ") + x;
		return s.empty() ? "-" : s;
	}

	// Plan na modelu: zdejmij, zaloz (silnik dopisuje na wierzch), kolejnosc calej listy; petle ruchu z faza jak
	// w Styles (zdjecie / zalozenie zatrzymuje wszystkie animacje).
	// Operacja na nakladkach bez zatrzymania animacji: zCModel::ApplyModelProtoOverlay 0x578840 / RemoveModelProtoOverlay
	// zatrzymuja WSZYSTKIE grane animacje (petla StopAni 0x57ACB0 pod 0x578955). Migawka kanalow (Director:
	// TakeAniSnapshot / RestoreAniSnapshot - te same animacje z faza, wagami przejsc i nastepnymi), animacje zdejmowanej
	// nakladki wracaja w wersji z wierzchu z faza, nastepne po id na wersje z wierzchu, stany ruchu przelozone na wersje
	// z wierzchu (SyncLocoAnis - tego wymaga IsStateAniActive 0x6ADE10 / IsRunning). Zwraca: czy cos przywrocono.
	template <class F> static bool SwKeepAnis(zCModel* m, zCModelPrototype* removing, F op) {
		AniSnapshot s;
		TakeAniSnapshot(m, removing, s);
		std::vector<LocoSnap> loco = SnapLocoAnis(m);
		std::vector<std::pair<int, float>> removed;   // animacje zdejmowanej nakladki: id i faza (wskaznik zniknie)
		for (auto& c : s.chans)
			if (c.fromRemoved && !c.isFadingOut)
				removed.push_back({ c.ani->aniID, c.old->GetProgressPercent() });
		op();
		if (m->numActiveAnis != 0 || s.chans.empty())
			return false;   // silnik nic nie zatrzymal (juz zalozona / brak pliku)
		RestoreAniSnapshot(m, s);
		for (auto& r : removed) {
			zCModelAni* top = m->GetAniFromAniID(r.first);
			if (!top || m->GetActiveAni(top))
				continue;
			m->StartAni(top, 0);
			if (zCModelAniActive* a = m->GetActiveAni(top))
				a->SetProgressPercent(r.second);
		}
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (a && a->nextAni && a->nextAni->aniID >= 0)
				if (zCModelAni* top = m->GetAniFromAniID(a->nextAni->aniID))
					a->nextAni = top;
		}
		SyncLocoAnis(m, loco);
		return true;
	}

	static void SwApplyOverlayPlan(zCModel* m, const zdcore::SwapOverlayPlan& p) {
		ZPROF_SCOPE("AniSwap.Overlays");   // podmiana nakladek talentu (bez stopu animacji)
		for (auto& r : p.remove)
			SwKeepAnis(m, FindOverlayProto(m, r), [&] { ZRemoveOverlayTimed(m, r + ".MDS"); });
		for (auto& a : p.add) {
			int ok = 0;
			SwKeepAnis(m, nullptr, [&] { ok = ZApplyOverlayTimed(m, a + ".MDS", "zamiana animacji"); });
			if (!ok) {
				ZLog("Zamiana animacji: nie podpieto %s - pomijam (bez ponawiania)", a.c_str());
				swOvlFailed.push_back(a);
			}
		}
		std::vector<LocoSnap> snap = SnapLocoAnis(m);
		ReorderOverlays(m, p.order);
		SyncLocoAnis(m, snap);
	}

	// oCNpc::ApplyOverlay 0x72D2C0 / RemoveOverlay 0x72D5C0 dla bohatera (zEBA zaklada styl w haku SetFightAnis - w polowie
	// dobycia, skrypty Mdl_ApplyOverlayMds, SetTalentSkill): bez zatrzymania animacji (SwKeepAnis). Przedtem zalozenie stylu
	// zEBA przy dobyciu zatrzymywalo T_1H_2_1HRUN i bohater stal chwile w pozie (test 17.10).
	static int swNpcOvlDepth = 0;
	static int swKeepLogs = 0;

	static zCModel* SwKeepModel(oCNpc* self) {
		if (!zcfg.enabled || !self || self != player || !self->homeWorld || swNpcOvlDepth > 0 || !HeroIsHuman())
			return nullptr;
		return self->GetModel();
	}

	static int __fastcall SwNpcApplyOverlay(oCNpc* self, void* edx, const zSTRING& name);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, const zSTRING&)> Ivk_oCNpc_ApplyOverlay(
		ENGINE == Engine_G2A ? 0x0072D2C0 : 0, &SwNpcApplyOverlay, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SwNpcApplyOverlay(oCNpc* self, void* edx, const zSTRING& name) {
		ZHookHit(ZH_APPLYOVERLAY);
		zCModel* m = SwKeepModel(self);
		if (!m)
			return Ivk_oCNpc_ApplyOverlay(self, edx, name);
		void* ret = _ReturnAddress();
		int r = 0;
		swNpcOvlDepth++;
		bool kept = SwKeepAnis(m, nullptr, [&] { r = Ivk_oCNpc_ApplyOverlay(self, edx, name); });
		swNpcOvlDepth--;
		if (kept && zcfg.debug && swKeepLogs < 60) {
			swKeepLogs++;
			ZDebug("Nakladki: %s zalozona przez oCNpc::ApplyOverlay (z %s) - animacje bohatera zachowane: %s", name.ToChar(),
				TravAddrDesc(ret).c_str(), SwActiveAnis(m).c_str());
		}
		return r;
	}

	static void __fastcall SwNpcRemoveOverlay(oCNpc* self, void* edx, const zSTRING& name);
	static CInvoke<void(__fastcall*)(oCNpc*, void*, const zSTRING&)> Ivk_oCNpc_RemoveOverlay(
		ENGINE == Engine_G2A ? 0x0072D5C0 : 0, &SwNpcRemoveOverlay, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwNpcRemoveOverlay(oCNpc* self, void* edx, const zSTRING& name) {
		ZHookHit(ZH_REMOVEOVERLAY);
		zCModel* m = SwKeepModel(self);
		if (!m) {
			Ivk_oCNpc_RemoveOverlay(self, edx, name);
			return;
		}
		void* ret = _ReturnAddress();
		swNpcOvlDepth++;
		bool kept = SwKeepAnis(m, FindOverlayProto(m, name.ToChar()), [&] { Ivk_oCNpc_RemoveOverlay(self, edx, name); });
		swNpcOvlDepth--;
		if (kept && zcfg.debug && swKeepLogs < 60) {
			swKeepLogs++;
			ZDebug("Nakladki: %s zdjeta przez oCNpc::RemoveOverlay (z %s) - animacje bohatera zachowane: %s", name.ToChar(),
				TravAddrDesc(ret).c_str(), SwActiveAnis(m).c_str());
		}
	}

	static std::string SwOverlayStateLabel(const std::vector<std::string>& native, const std::vector<std::string>& wanted) {
		std::string n = zdcore::SwapTalentSignature(native), w = zdcore::SwapTalentSignature(wanted);
		if (n == w)
			return "natywne (" + (n.empty() ? std::string("brak") : n) + ")";
		return (w.empty() ? std::string("brak") : w) + " zamiast " + (n.empty() ? std::string("brak") : n);
	}

	// ---------------------------------------------------------------- diagnostyka

	// Nazwy z przedrostkiem fromType grane w modelu (bez dobycia broni), ktorych odpowiednika toType model nie ma.
	static int SwMissing(zCModel* m, int fromType, int toType, int& total, std::string& examples) {
		int missing = 0, shown = 0;
		zCModelPrototype* base = m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		int n = base ? base->protoAnis.GetNum() : 0;
		for (int id = 0; id < n; id++) {
			zCModelAni* a = m->GetAniFromAniID(id);
			if (!a)
				continue;
			std::string name = a->aniName.ToChar();
			if (zdcore::SwapIsDrawAni(name))
				continue;
			std::string to = zdcore::SwapAniName(name, fromType, toType);
			if (to.empty())
				continue;
			total++;
			int id2 = m->GetAniIDFromAniName(zSTRING(to.c_str()));
			if (id2 >= 0 && m->GetAniFromAniID(id2))
				continue;
			missing++;
			if (shown++ < 6)
				examples += (examples.empty() ? "" : ", ") + to + " (" + OverlayOfAni(m, a) + ")";
		}
		return missing;
	}

	static void SwLogBuild(zCModel* m, const char* why) {
		if (swLogged.empty() && zcfg.swap.mode == zdcore::SWAP_OFF && zdcore::SwapIsIdentity(swTable) && !swOvlTouched)
			return;   // zamiana nigdy nie uzyta w tej sesji - bez wpisow przy kazdej przebudowie silnika
		const char* tn = swCur.type == zdcore::SWAP_TYPE_1H ? "1H" : swCur.type == zdcore::SWAP_TYPE_2H ? "2H" : "-";
		char buf[768];
		sprintf_s(buf, "Zamiana animacji [kl. %u]: %s - bron %s (%s, tryb walki %d)%s%s%s, wariant %s%s, tablica %s, poziom %s, "
			"nakladki talentu: %s", zdFrame, why, swCur.instance.empty() ? "brak" : swCur.instance.c_str(), tn, swCur.wm,
			swCur.dual ? ", bron parowa NB " : "", swCur.dual ? zdcore::DualLabel(swCur.dual) : "",
			swCur.dual ? " - natywnie, bez zamiany" : "", zdcore::SwapModeLabel(zcfg.swap.mode),
			zcfg.swap.perWeapon ? " (pamiec per bron)" : "", zdcore::SwapTableLabel(swTable).c_str(),
			zcfg.swap.level ? "talent drugiego typu" : "talent broni w rece", swOvlState.c_str());
		std::string line = buf;
		if (line.substr(line.find(']')) == swLogged)   // bez numeru klatki; silnik przebudowuje tez przy sprincie, talencie
			return;
		swLogged = line.substr(line.find(']'));
		ZLog("%s", line.c_str());
		if (!zcfg.debug)
			return;
		std::string diag;
		for (int from : { zdcore::SWAP_TYPE_1H, zdcore::SWAP_TYPE_2H }) {
			int fromWm = from == zdcore::SWAP_TYPE_1H ? zdcore::SWAP_WM_1H : zdcore::SWAP_WM_2H;
			if (zdcore::SwapMapWeaponMode(swTable, fromWm) == fromWm)
				continue;
			int to = zdcore::SwapOtherType(from), total = 0;
			std::string ex;
			int miss = SwMissing(m, from, to, total, ex);
			char d[1024];
			sprintf_s(d, "%s%s->%s: brak w modelu %d z %d nazw%s%s", diag.empty() ? "" : " | ", from == 1 ? "1H" : "2H",
				to == 1 ? "1H" : "2H", miss, total, miss ? ", np. " : "", ex.c_str());
			diag += d;
		}
		if (!diag.empty() && diag != swMissLogged) {
			swMissLogged = diag;
			ZLog("Zamiana animacji: %s (brakujace = nakladka zEBA / stylu bez odpowiednika drugiego typu - gra wersja "
				"natywna albo nic)", diag.c_str());
		}
	}

	// ---------------------------------------------------------------- stan bohatera, klawisz

	static zdcore::SwapBusy SwBusyNow(oCAniCtrl_Human* ac, zCModel* m) {
		zdcore::SwapBusy b;
		b.director = DirectorActive();
		b.moves = MovesActive();
		b.traversal = TraversalActive();
		b.attacking = HeroAttacking();
		b.parade = ac->IsParadeRunning() != nullptr;
		b.weaponMsg = HeroWeaponMessagePending();
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		b.body = !(bs == BS_STAND || bs == BS_WALK || bs == BS_SNEAK || bs == BS_RUN || bs == BS_SPRINT) || player->IsDead() ||
			player->IsUnconscious();
		b.soft = LootActive() || !BlockingAni(m).empty();
		return b;
	}

	static void SwSaveSettings() {
		zoptions->WriteInt("ZDISMEMBER", "WeaponSwap", zcfg.swap.mode, FALSE);
		zoptions->WriteInt("ZDISMEMBER", "WeaponSwapLast", zcfg.swap.last, FALSE);
		zoptions->WriteString("ZDISMEMBER", "WeaponSwapWeapons", zdcore::SwapJoinWeapons(zcfg.swap.weapons).c_str(), FALSE);
	}

	static void SwTell(const std::string& text) {
		if (ogame && ogame->GetTextView())
			ogame->GetTextView()->PrintTimedCXY(zSTRING(text.c_str()), 2500.0f, nullptr);
	}

	// Napis ekranowy (cp1250): l = \xB3, a = \xB9, n = \xF1, e = \xEA.
	static const char* SwModeText(int mode) {
		switch (mode) {
		case zdcore::SWAP_1H_AS_2H: return "1H jak 2H";
		case zdcore::SWAP_2H_AS_1H: return "2H jak 1H";
		case zdcore::SWAP_BOTH: return "1H jak 2H i 2H jak 1H";
		default: return "wy\xB3.";
		}
	}

	static std::string SwToggle() {
		swCur = SwReadCurrent();
		zdcore::SwapToggleResult r = zdcore::SwapToggle(zcfg.swap, swCur.type, swCur.instance);
		zcfg.swap = r.settings;
		SwSaveSettings();
		std::string text;
		if (r.perWeaponChange)
			text = "Zamiana animacji (" + swCur.name + "): " + (r.active ? std::string("w\xB3. - ") + SwModeText(zcfg.swap.mode) :
				std::string("wy\xB3."));
		else {
			text = std::string("Zamiana animacji: ") + SwModeText(zcfg.swap.mode);
			if (zcfg.swap.mode != zdcore::SWAP_OFF && swCur.type != zdcore::SWAP_TYPE_NONE && !r.active)
				text += zcfg.swap.perWeapon ? " (ta bro\xF1 nie na li\x9C" "cie)" : " (bro\xF1 w r\xEA" "ce bez zmian)";
		}
		if (swCur.dual != zdcore::DUAL_NONE)
			text += " - bro\xF1 parowa NB zawsze natywnie";
		SwTell(text);
		std::string log = std::string("Zamiana animacji: klawisz - wariant ") + zdcore::SwapModeLabel(zcfg.swap.mode) +
			(zcfg.swap.perWeapon ? ", pamiec per bron, lista: " + (zcfg.swap.weapons.empty() ? std::string("pusta") :
			zdcore::SwapJoinWeapons(zcfg.swap.weapons)) : std::string()) + ", bron " +
			(swCur.instance.empty() ? std::string("brak") : swCur.instance) + (r.active ? " - zamiana" : " - natywnie");
		ZLog("%s", log.c_str());
		return log;
	}

	static bool SwModsHeld(int mods) {
		auto down = [](int a, int b) { return zinput->KeyPressed(a) || zinput->KeyPressed(b); };
		if ((mods & zdcore::SWAP_MOD_CTRL) && !down(KEY_LCONTROL, KEY_RCONTROL))
			return false;
		if ((mods & zdcore::SWAP_MOD_SHIFT) && !down(KEY_LSHIFT, KEY_RSHIFT))
			return false;
		if ((mods & zdcore::SWAP_MOD_ALT) && !down(KEY_LMENU, KEY_RMENU))
			return false;
		return true;
	}

	bool AniSwapComboHeld(int key) {
		return zinput && zcfg.swapKey > 0 && key == zcfg.swapKey && zcfg.swapMods != 0 && SwModsHeld(zcfg.swapMods);
	}

	static void SwKeyUpdate() {
		bool down = zinput && zcfg.swapKey > 0 && zinput->KeyPressed(zcfg.swapKey) && SwModsHeld(zcfg.swapMods);
		bool pressed = down && !swPrevKey;
		swPrevKey = down;
		if (!pressed || !player || (zcon && zcon->IsVisible()) || player->inventory2.IsOpen() ||
			!oCInformationManager::GetInformationManager().HasFinished())
			return;
		SwToggle();
	}

	// ---------------------------------------------------------------- straznik dobycia / schowania broni

	static const unsigned SW_WATCH_LIMIT_MS = 2000;   // komunikat broni dluzej = zaciecie
	static const int SW_WATCH_LINES = 12;             // wpisow stanu (Debug) na komunikat
	static unsigned swClockMs = 0;                    // czas gry z petli (bez pauzy i menu)

	static oCMsgWeapon* SwWeaponMsg() {
		zCEventManager* em = player->GetEM(FALSE);
		if (!em)
			return nullptr;
		for (int i = 0; i < em->GetNumMessages(); i++)
			if (oCMsgWeapon* w = zDYNAMIC_CAST<oCMsgWeapon>(em->GetEventMessage(i)))
				if (w->subType <= oCMsgWeapon::EV_FORCEREMOVEWEAPON)
					return w;
		return nullptr;
	}

	static const char* SwWeaponMsgName(int sub) {
		static const char* names[] = { "DRAWWEAPON", "DRAWWEAPON1", "DRAWWEAPON2", "REMOVEWEAPON", "REMOVEWEAPON1",
			"REMOVEWEAPON2", "CHOOSEWEAPON", "FORCEREMOVEWEAPON" };
		return sub >= 0 && sub < 8 ? names[sub] : "?";
	}

	static std::string SwAniName(zCModel* m, int id) {
		zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		return id < 0 ? std::string("-") : a ? std::string(a->aniName.ToChar()) : "brak(" + std::to_string(id) + ")";
	}

	// Aktywne animacje bohatera (warstwa, faza, nastepna) - podpis do logu.
	static std::string SwActiveAnis(zCModel* m) {
		std::string s;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* ani = a ? a->protoAni : nullptr;
			if (!ani)
				continue;
			char buf[256];
			sprintf_s(buf, "%s%s[w%d %.2f%s -> %s]", s.empty() ? "" : " ", ani->aniName.ToChar(), ani->layer,
				a->GetProgressPercent(), a->isFadingOut ? " wygasa" : "", a->nextAni ? a->nextAni->aniName.ToChar() : "-");
			s += buf;
		}
		return s.empty() ? "brak" : s;
	}

	// Pelny stan dobycia: komunikat, tryby, kolejka EM, animacje grane i oczekiwane przez silnik.
	static std::string SwWeaponState(oCAniCtrl_Human* ac, zCModel* m, oCMsgWeapon* msg) {
		int sel = ac->wmode_selected >= 0 && ac->wmode_selected < zdcore::SWAP_WM_COUNT ? ac->wmode_selected : 0;
		std::string em;
		if (zCEventManager* e = player->GetEM(FALSE))
			for (int i = 0; i < e->GetNumMessages() && i < 8; i++) {
				zCEventMessage* x = e->GetEventMessage(i);
				if (!x)
					continue;
				em += (em.empty() ? "" : ", ") + std::string(x->_GetClassDef()->className.ToChar()) + ":" + std::to_string(x->subType);
			}
		char buf[1536];
		sprintf_s(buf, "%s (cel %d), tryb walki %d, wmode %d, wybrany %d, tablica %s%s | kolejka: %s | gra: %s | silnik czeka na: "
			"s_weapon[%d] %s, _s_walk %s (natywna postawa %s)", msg ? SwWeaponMsgName(msg->subType) : "-", msg ? msg->targetMode : -1,
			player->GetWeaponMode(), ac->wmode, sel, zdcore::SwapTableLabel(swTable).c_str(), swSuspend ? " (wstrzymana)" : "",
			em.empty() ? "pusta" : em.c_str(), SwActiveAnis(m).c_str(), sel, SwAniName(m, ac->s_weapon[sel]).c_str(),
			SwAniName(m, ac->_s_walk).c_str(), SwAniName(m, swNatStance[sel][0]).c_str());
		return buf;
	}

	// Koniec natywnego lancucha dobycia (S_1HRUN / WALK / SNEAK z nazwy) -> postawa po zamianie (_s_walk), na ktora czeka
	// EV_DrawWeapon2: nastepna animacja przejscia przestawiona, a gdy natywna postawa juz gra - start postawy po zamianie.
	static void SwRedirectStance(oCAniCtrl_Human* ac, zCModel* m) {
		int modes[2] = { ac->wmode_selected, player->GetWeaponMode() };
		for (int k = 0; k < 2; k++) {
			int wm = modes[k];
			if (wm <= 0 || wm >= zdcore::SWAP_WM_COUNT || zdcore::SwapMapWeaponMode(swTable, wm) == wm || (k == 1 && wm == modes[0]))
				continue;
			int cur[3] = { ac->s_run[wm], ac->s_walk[wm], ac->s_sneak[wm] };
			for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
				zCModelAniActive* a = m->aniChannels[i];
				if (!a || !a->protoAni || a->isFadingOut)
					continue;
				int id = a->nextAni ? zdcore::SwapStanceRedirect(swNatStance[wm], cur, a->nextAni->aniID) : -1;
				zCModelAni* next = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
				if (next) {
					a->nextAni = next;
					ZDebug("Zamiana animacji: dobycie - po %s postawa %s zamiast natywnej", a->protoAni->aniName.ToChar(),
						next->aniName.ToChar());
				}
				id = a->nextAniOverride ? zdcore::SwapStanceRedirect(swNatStance[wm], cur, a->nextAniOverride->aniID) : -1;
				if (zCModelAni* ov = id >= 0 ? m->GetAniFromAniID(id) : nullptr)
					a->nextAniOverride = ov;
				id = zdcore::SwapStanceRedirect(swNatStance[wm], cur, a->protoAni->aniID);
				zCModelAni* to = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
				if (to && !m->GetActiveAni(to)) {
					ZDebug("Zamiana animacji: dobycie - gra natywna %s, start %s (na nia czeka EV_DrawWeapon2)",
						a->protoAni->aniName.ToChar(), to->aniName.ToChar());
					m->StartAni(to, 0);
					return;   // lista kanalow sie zmienila
				}
			}
		}
	}

	static void SwSync(bool now, const char* why);

	// Co klatke: komunikat broni bohatera (dobycie / schowanie / zmiana). Przy zamianie: postawa konca dobycia, wpisy
	// stanu (Debug) i straznik - ten sam komunikat dluzej niz 2 s = pelny stan do logu i natywne tablice do jego konca.
	static void SwDrawGuard(oCAniCtrl_Human* ac, zCModel* m) {
		ZPROF_SCOPE("AniSwap.Guard");   // komunikat broni bohatera przy zamianie animacji
		swClockMs += ztimer ? (unsigned)ztimer->frameTimeFloat : 16u;
		oCMsgWeapon* msg = SwWeaponMsg();
		bool ours = ac == swAc && (!zdcore::SwapIsIdentity(swTable) || swSuspend);
		int step = zdcore::SwapWatchStep(swWatch, msg, swClockMs, SW_WATCH_LIMIT_MS);
		if (step == zdcore::SWAP_WATCH_END) {
			if (swSuspend) {
				swSuspend = false;
				ZLog("Zamiana animacji: komunikat broni zakonczony - zamiana wraca (przebudowa w wolnej klatce)");
			}
			else if (ours)
				ZDebug("Zamiana animacji: komunikat broni zakonczony");
			swWatchSig.clear();
			return;
		}
		if (!msg || !ours)
			return;
		if (step == zdcore::SWAP_WATCH_START)
			swWatchLines = 0;
		if (!swSuspend)
			SwRedirectStance(ac, m);
		if (zcfg.debug && swWatchLines < SW_WATCH_LINES) {
			std::string sig = SwActiveAnis(m) + "|" + std::to_string(msg->subType) + "|" + std::to_string(ac->wmode);
			if (sig != swWatchSig) {
				swWatchSig = sig;
				swWatchLines++;
				ZDebug("Zamiana animacji [kl. %u]: %s %s", zdFrame, step == zdcore::SWAP_WATCH_START ? "komunikat broni:" : "stan:",
					SwWeaponState(ac, m, msg).c_str());
			}
		}
		if (step == zdcore::SWAP_WATCH_STUCK && !swSuspend) {
			ZLog("Zamiana animacji: STRAZNIK - komunikat broni trwa %.1f s: %s - natywne tablice do konca komunikatu",
				(swClockMs - swWatch.startMs) / 1000.0f, SwWeaponState(ac, m, msg).c_str());
			swSuspend = true;
			SwSync(true, "straznik dobycia - natywne tablice");
		}
	}

	// ---------------------------------------------------------------- diagnostyka drgania postawy (Debug=1)
	// Kto startuje / wygasza animacje bohatera: zCModel::StartAni(zCModelAni*, int) 0x57B0C0 (StartAni(int) 0x57B070 skacze
	// tu jmp - adres powrotu = prawdziwy wywolujacy), FadeOutAni(zCModelAniActive*) 0x57F020 (FadeOutAni(int) skacze tu),
	// StopAni(zCModelAniActive*) 0x57ACB0. Ta sama postawa S_* (warstwa 1) startowana od nowa >= 4 razy w 60 klatkach =
	// wpis RESTART z wywolujacym start i ostatnie wygaszenie / stop, plus sledzenie 40 klatek. zdism swap watch [s] =
	// wszystkie starty / wygaszenia bohatera przez s sekund.
	static zCModel* swDiagModel = nullptr;   // model bohatera (z petli) - porownanie wskaznika w hakach
	static zdcore::SwapRestartWatch swRestart;
	static std::string swLastFade;
	static unsigned swTraceUntil = 0;
	static int swTraceLines = 0, swRestartLogs = 0;

	// Stos bez ramek (zEBA / zModelProtoExtender hakuja StartAni(int) i FadeOutAni(int) - RtlCaptureStackBackTrace konczy
	// sie na ich opakowaniu): przeglad slow stosu - adresy w kodzie Gothic2.exe tuz za instrukcja call (E8 rel32 albo
	// FF /2), do 6 kandydatow od najblizszego.
	static bool SwReadDword(const DWORD* p, DWORD& v) {
		__try {
			v = *p;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	static std::string SwScanStack() {
		volatile DWORD probe = 0;
		const DWORD* sp = (const DWORD*)&probe;
		std::string s;
		int found = 0;
		for (int i = 0; i < 160 && found < 6; i++) {
			DWORD v = 0;
			if (!SwReadDword(sp + i, v))
				break;
			if (v < 0x00401005 || v >= 0x0082E000)
				continue;
			const BYTE* c = (const BYTE*)v;
			bool call = c[-5] == 0xE8 || (c[-2] == 0xFF && (c[-1] & 0x38) == 0x10) || (c[-3] == 0xFF && (c[-2] & 0x38) == 0x10) ||
				(c[-6] == 0xFF && (c[-5] & 0x38) == 0x10);
			if (!call)
				continue;
			char buf[24];
			sprintf_s(buf, "%s0x%08X", found ? " < " : "", (unsigned)v);
			s += buf;
			found++;
		}
		return s.empty() ? "-" : s;
	}

	static bool SwDiagOn(zCModel* self) {
		return self && self == swDiagModel && (zcfg.debug || swTraceUntil > zdFrame);
	}

	static void SwTrace(const char* what, zCModelAni* ani, void* ret) {
		if (swTraceUntil <= zdFrame || swTraceLines >= 400 || !ani)
			return;
		swTraceLines++;
		ZLog("Animacje bohatera [kl. %u]: %s %s (w%d, %s) z %s; exe na stosie: %s", zdFrame, what, ani->aniName.ToChar(), ani->layer,
			OverlayOfAni(swDiagModel, ani).c_str(), TravAddrDesc(ret).c_str(), SwScanStack().c_str());
	}

	static void __fastcall SwModelStartAni(zCModel* self, void* edx, zCModelAni* ani, int mode);
	static CInvoke<void(__fastcall*)(zCModel*, void*, zCModelAni*, int)> Ivk_zCModel_StartAni(
		ENGINE == Engine_G2A ? 0x0057B0C0 : 0, &SwModelStartAni, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwModelStartAni(zCModel* self, void* edx, zCModelAni* ani, int mode) {
		if (!SwDiagOn(self) || !ani) {
			Ivk_zCModel_StartAni(self, edx, ani, mode);
			return;
		}
		void* ret = _ReturnAddress();
		zCModelAniActive* was = self->GetActiveAni(ani);
		bool restart = !was || was->isFadingOut;   // StartAni przy granej (nie wygasajacej) nic nie robi
		Ivk_zCModel_StartAni(self, edx, ani, mode);
		if (!restart)
			return;
		SwTrace("start", ani, ret);
		const char* n = ani->aniName.ToChar();
		if (ani->layer == 1 && n[0] == 'S' && n[1] == '_' && zdcore::SwapRestartStep(swRestart, ani->aniID, zdFrame, 4, 60) &&
			swRestartLogs < 20) {
			swRestartLogs++;
			ZLog("Animacje bohatera [kl. %u]: RESTART postawy %s (%s) 4x w 60 kl. - zamiana %s, start z %s; stos: %s; exe na stosie: "
				"%s; ostatnie wygaszenie / stop: %s; gra: %s", zdFrame, n, OverlayOfAni(self, ani).c_str(),
				zdcore::SwapTableLabel(swTable).c_str(), TravAddrDesc(ret).c_str(), TravStackDesc().c_str(), SwScanStack().c_str(),
				swLastFade.empty() ? "-" : swLastFade.c_str(), SwActiveAnis(self).c_str());
			if (swTraceUntil <= zdFrame) {   // dalszy przebieg w logu
				swTraceUntil = zdFrame + 40;
				swTraceLines = 0;
			}
		}
	}

	static void SwNoteFade(const char* what, zCModel* self, zCModelAniActive* a, void* ret) {
		if (!a || !a->protoAni)
			return;
		SwTrace(what, a->protoAni, ret);
		if (a->protoAni->layer == 1) {
			char buf[512];
			sprintf_s(buf, "%s %s [kl. %u] z %s (exe na stosie: %s)", what, a->protoAni->aniName.ToChar(), zdFrame,
				TravAddrDesc(ret).c_str(), SwScanStack().c_str());
			swLastFade = buf;
		}
	}

	static void __fastcall SwModelFadeOutAni(zCModel* self, void* edx, zCModelAniActive* a);
	static CInvoke<void(__fastcall*)(zCModel*, void*, zCModelAniActive*)> Ivk_zCModel_FadeOutAni(
		ENGINE == Engine_G2A ? 0x0057F020 : 0, &SwModelFadeOutAni, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwModelFadeOutAni(zCModel* self, void* edx, zCModelAniActive* a) {
		if (SwDiagOn(self) && a && !a->isFadingOut)
			SwNoteFade("wygaszenie", self, a, _ReturnAddress());
		Ivk_zCModel_FadeOutAni(self, edx, a);
	}

	static void __fastcall SwModelStopAni(zCModel* self, void* edx, zCModelAniActive* a);
	static CInvoke<void(__fastcall*)(zCModel*, void*, zCModelAniActive*)> Ivk_zCModel_StopAni(
		ENGINE == Engine_G2A ? 0x0057ACB0 : 0, &SwModelStopAni, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SwModelStopAni(zCModel* self, void* edx, zCModelAniActive* a) {
		if (SwDiagOn(self) && a)
			SwNoteFade("stop", self, a, _ReturnAddress());
		Ivk_zCModel_StopAni(self, edx, a);
	}

	// ---------------------------------------------------------------- petla

	int AniSwapAniWeaponMode(int wm) {
		if (!player || !swAc || zdcore::SwapIsIdentity(swTable) || player->GetAnictrl() != swAc)
			return wm;
		return zdcore::SwapMapWeaponMode(swTable, wm);
	}

	int AniSwapSlotDriver(int slotType) {
		bool applied = player && swAc && player->GetAnictrl() == swAc;
		return zdcore::SwapSlotDriver(applied ? swTable : zdcore::SwapIdentity(), swCur.type, slotType);
	}

	int AniSwapSlotTalent(int talent) {
		int slot = talent == oCNpcTalent::NPC_TAL_1H ? zdcore::SWAP_TYPE_1H : talent == oCNpcTalent::NPC_TAL_2H ?
			zdcore::SWAP_TYPE_2H : zdcore::SWAP_TYPE_NONE;
		if (slot == zdcore::SWAP_TYPE_NONE)
			return talent;
		return AniSwapSlotDriver(slot) == zdcore::SWAP_TYPE_1H ? oCNpcTalent::NPC_TAL_1H : oCNpcTalent::NPC_TAL_2H;
	}

	std::vector<std::string> AniSwapOverlayNames() {
		std::vector<std::string> out;
		for (int type : { zdcore::SWAP_TYPE_1H, zdcore::SWAP_TYPE_2H })
			for (int level = 1; level <= 2; level++)
				out.push_back(zdcore::SwapTalentOverlay(type, level));
		return out;
	}

	// Zmiany chciane teraz: tablica animacji i nakladki talentu. Z 'now' = bez sprawdzania zajetosci (zapis, wylaczenie).
	static void SwSync(bool now, const char* why) {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		if (!ac || !m)
			return;
		swCur = SwReadCurrent();
		zdcore::SwapTable want = SwWantedTable(swCur);
		zdcore::SwapTable have = ac == swAc ? swTable : zdcore::SwapIdentity();
		bool tableChange = !zdcore::SwapSame(want, have) || swReloadOnce;
		std::vector<std::string> native = SwNativeOverlays();
		std::vector<std::string> wanted = SwWantedOverlays(native);
		std::string sigN = zdcore::SwapTalentSignature(native), sigW = zdcore::SwapTalentSignature(wanted);
		std::string key = sigW + "|" + sigN;
		bool ovlChange = key != swOvlKey && (swOvlTouched || sigW != sigN);
		if (key != swOvlKey && !ovlChange)
			swOvlKey = key;   // nic naszego na modelu i nic do zmiany
		if (!tableChange && !ovlChange) {
			swDeferSince = 0;
			swDeferLogged.clear();
			if (swBuilt) {   // przebudowa przez silnik (talent, wczytanie, sprint) - wpis tylko przy zmianie stanu
				swBuilt = false;
				SwLogBuild(m, "animacje przebudowane");
			}
			return;
		}
		if (!now) {
			const char* reason = zdcore::SwapDeferReason(SwBusyNow(ac, m), swDeferSince ? zdFrame - swDeferSince : 0);
			if (reason) {
				if (!swDeferSince)
					swDeferSince = zdFrame ? zdFrame : 1;
				if (swDeferLogged != reason) {
					swDeferLogged = reason;
					ZDebug("Zamiana animacji: zmiana odlozona - trwa %s", reason);
				}
				return;
			}
		}
		swDeferSince = 0;
		swDeferLogged.clear();
		if (ovlChange) {
			zdcore::SwapOverlayPlan p = zdcore::SwapPlanOverlays(ModelOverlays(m), native, wanted);
			if (!p.remove.empty() || !p.add.empty())
				SwApplyOverlayPlan(m, p);
			ZDebug("Zamiana animacji: nakladki talentu - zdejmij %s | zaloz %s | lista %s", SwJoin(p.remove).c_str(),
				SwJoin(p.add).c_str(), SwJoin(p.order).c_str());
			swOvlKey = key;
			swOvlTouched = sigW != sigN;
			swOvlState = SwOverlayStateLabel(native, wanted);
		}
		if (tableChange)
			SwReload(ac, m);
		swReloadOnce = false;
		swBuilt = false;
		SwLogBuild(m, why);
	}

	void AniSwapUpdate() {
		SwKeyUpdate();
		swDiagModel = nullptr;
		if (!player || !player->homeWorld || !HeroIsHuman())
			return;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		swDiagModel = m;
		if (ac && m)
			SwDrawGuard(ac, m);
		SwSync(false, "przebudowa w locie");
	}

	void AniSwapRestoreIfDisabled() {
		if (!player || !player->homeWorld || (!swOvlTouched && (!swAc || zdcore::SwapIsIdentity(swTable))) || !HeroIsHuman())
			return;
		SwSync(true, "plugin wylaczony - natywne");
	}

	void AniSwapBeforeSave() {
		if (!swOvlTouched || !player || !player->homeWorld || !HeroIsHuman())
			return;
		zCModel* m = player->GetModel();
		std::vector<std::string> native = SwNativeOverlays();
		zdcore::SwapOverlayPlan p = zdcore::SwapPlanOverlays(ModelOverlays(m), native, native);
		if (!p.remove.empty() || !p.add.empty())
			SwApplyOverlayPlan(m, p);
		swOvlKey.clear();       // po zapisie petla zalozy je znow
		swOvlTouched = false;
		swOvlState = "natywne (zapis)";
	}

	void AniSwapForget() {
		// Zmiana swiata: bohater (i jego model) przechodzi dalej - nakladki talentu wracaja do natywnych teraz, poki
		// stary swiat istnieje. Wczytanie zapisu: stary bohater zaraz zniknie - przywrocenie bez znaczenia, ale nieszkodliwe.
		AniSwapBeforeSave();
		// Sterownik mogl przejsc ze starymi tablicami (zmiana swiata) albo zniknac (wskaznik do porownan, nie do uzycia):
		// gdy zamiana byla albo jest w ustawieniach - jedna przebudowa po wczytaniu ustala stan.
		swReloadOnce = !zdcore::SwapIsIdentity(swTable) || zcfg.swap.mode != zdcore::SWAP_OFF;
		swAc = nullptr;
		swTable = zdcore::SwapIdentity();
		swCtx = 0;
		swOvlKey.clear();
		swOvlTouched = false;
		swOvlState = "natywne";
		swDeferSince = 0;
		swDeferLogged.clear();
		swSuspend = false;
		swWatch = zdcore::SwapWeaponWatch();
		swWatchSig.clear();
		swDiagModel = nullptr;
		swRestart = zdcore::SwapRestartWatch();
		swLastFade.clear();
		swCur = SwCurrent();
		swLogged.clear();
		swMissLogged.clear();
	}

	std::string AniSwapConsole(const std::string& arg) {
		std::string a = zdcore::Upper(arg);
		if (a == "TOGGLE")
			return SwToggle();
		if (a.size() == 1 && a[0] >= '0' && a[0] <= '3') {
			zcfg.swap.mode = a[0] - '0';
			if (zcfg.swap.mode != zdcore::SWAP_OFF)
				zcfg.swap.last = zcfg.swap.mode;
			SwSaveSettings();
			return std::string("zamiana animacji: ") + zdcore::SwapModeLabel(zcfg.swap.mode) + " (przebudowa w nastepnych klatkach)";
		}
		if (a == "WATCH") {
			swTraceUntil = zdFrame + 240;
			swTraceLines = 0;
			return "starty / wygaszenia / stopy animacji bohatera do zDismember.log przez ok. 4 s";
		}
		if (a == "RELOAD") {
			swAc = nullptr;   // petla uzna tablice za natywne i przebuduje
			swTable = zdcore::SwapIdentity();
			swLogged.clear();
			swMissLogged.clear();
			return "przebudowa animacji bohatera w nastepnej wolnej klatce";
		}
		SwCurrent c = SwReadCurrent();
		char buf[768];
		sprintf_s(buf, "zamiana: wariant %s, pamiec per bron %s (lista: %s), poziom %s, klawisz %s | bron %s (%s) - %s | "
			"tablica %s | nakladki talentu: %s%s", zdcore::SwapModeLabel(zcfg.swap.mode), zcfg.swap.perWeapon ? "wl." : "wyl.",
			zcfg.swap.weapons.empty() ? "pusta" : zdcore::SwapJoinWeapons(zcfg.swap.weapons).c_str(),
			zcfg.swap.level ? "talent drugiego typu" : "talent broni w rece", zdcore::SwapKeyLabel(zcfg.swapKey, zcfg.swapMods).c_str(),
			c.instance.empty() ? "brak" : c.instance.c_str(), c.type == 1 ? "1H" : c.type == 2 ? "2H" : "-",
			c.dual ? (std::string("bron parowa NB ") + zdcore::DualLabel(c.dual) + " - natywnie").c_str() :
			zdcore::SwapActiveFor(zcfg.swap, c.type, c.instance) ? "zamiana" : "natywnie", zdcore::SwapTableLabel(
			player && player->GetAnictrl() == swAc ? swTable : zdcore::SwapIdentity()).c_str(), swOvlState.c_str(),
			swDeferSince ? (std::string(" | odlozone: ") + swDeferLogged).c_str() : "");
		ZLog("%s", buf);
		return buf;
	}
}
