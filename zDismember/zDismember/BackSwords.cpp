// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Miecze na plecach (BackSwords.h). Silnik G2A (deasemblacja w [notatki lokalne]):
	// - oCNpc::EquipItem 0x7323C0 wybiera slot po fladze przedmiotu (DAG / SWD / AXE -> ZS_SWORD, 2HD_* -> ZS_LONGSWORD,
	//   BOW -> ZS_BOW, CROSSBOW -> ZS_CROSSBOW) i PutInSlot ustawia wizual przedmiotu w wezle modelu. Plugin NIE zmienia
	//   slotow (zapis, skrypty, NB, zEBA widza wszystko natywnie) - przestawia tylko lokalna macierz wezla ZS_SWORD
	//   (1H na plecach) / ZS_LONGSWORD (2H) / ZS_BOW (drugi miecz) po animacji (hak AdvanceAnis), wzgledem BIP01 SPINE2.
	//   Animacje gry kluczuja wezly ZS_* (S_RUN wszystkie 34), wiec macierz jest nadpisywana w kazdej klatce.
	// - Dobycie: zdarzenie modelu DEF_FIGHTMODE "1H" -> oCNpc::DoDoAniEvents 0x742A20 -> SetWeaponMode2 (bron ze slotu do
	//   ZS_RIGHTHAND); chowanie DEF_FIGHTMODE "" (z reki do slotu). Nazwy: DrawWeapon1 0x6B2610 (T_RUN_2_1H -> S_1H),
	//   DrawWeapon2 0x6B2CF0 (T_1H_2_1HRUN), RemoveWeapon1 0x6B3040 ("T_" + bron + "RUN_2_" + bron), RemoveWeapon2 0x6B33B0
	//   ("T_" + bron + "_2_RUN"), w biegu EV_DrawWeapon 0x74CC10 (warstwa 2 T_MOVE_2_1HMOVE). Nakladki HUMANS_ZDF_DRAW_1H /
	//   _2H (wariant A) zaklada plan Styles; wariant B / BELT / natywny = podmiana wpisow protoAnis (jak ciecia boczne 2H w
	//   AtkTempo.cpp) - bez zakladania nakladek (zalozenie zatrzymuje wszystkie animacje postaci).
	// - Drugi miecz: wizual przedmiotu z ekwipunku w wezle ZS_BOW (zCModel::SetNodeVisual 0x5792A0 - AddRef wizualu); luk
	//   w slocie ZS_BOW jest wtedy ukryty (SecondWithRanged=1), kusza - wizual wezla ZS_CROSSBOW wyzerowany.
	// - Zapis: tylko zmienne int skryptu sa w zapisie (zCParser::SaveGlobalVars 0x796C40: typ int, bez flag) -
	//   data\System\Autorun\zDismember_Back.d: ZDF_BACK_SECOND = skrot nazwy instancji, ZDF_BACK_FLAGS bit 1 = bron w B.
	static const char* BS_OV[2] = { "HUMANS_ZDF_DRAW_1H", "HUMANS_ZDF_DRAW_2H" };
	static const char* BS_LOADED[3] = { "HUMANS_ZDF_DRAW_1H_B", "HUMANS_ZDF_DRAW_2H_B", "HUMANS_ZDF_DRAW_1H_BELT" };
	static const int BS_LOADED_BELT = 2;
	static const int BS_ANI_N = 7;
	static const char* BS_ANIS[2][BS_ANI_N] = {
		{ "T_RUN_2_1H", "S_1H", "T_1H_2_1HRUN", "T_1HRUN_2_1H", "T_1H_2_RUN", "T_MOVE_2_1HMOVE", "T_1HMOVE_2_MOVE" },
		{ "T_RUN_2_2H", "S_2H", "T_2H_2_2HRUN", "T_2HRUN_2_2H", "T_2H_2_RUN", "T_MOVE_2_2HMOVE", "T_2HMOVE_2_MOVE" } };
	static const bool BS_ANI_SHEATHE[BS_ANI_N] = { false, false, false, true, true, false, true };
	static const char* BS_NODE_NAMES[4] = { "", "ZS_SWORD", "ZS_LONGSWORD", "ZS_BOW" };

	// ---------------------------------------------------------------- dane z paczki
	static zdcore::BackMat bsPos[3];        // [SPOT_A], [SPOT_B] wzgledem BIP01 SPINE2 (z make_draw.py)
	static bool bsPosOk = false;
	static bool bsAvail = false;            // nakladki A + pozycje - funkcja mozliwa
	static std::vector<std::string> bsFailed;

	// ---------------------------------------------------------------- wpisy nakladek (wariant dobycia)
	static zCModelPrototype* bsLoaded[3] = {};
	static zCModelPrototype* bsLoadedBase[3] = {};
	static bool bsLoadTried[3] = {};
	struct BsSlot {
		zCModelPrototype* proto = nullptr;  // zalozona nakladka HUMANS_ZDF_DRAW_xH (wpisy z pliku = wariant A)
		bool held = false;                  // nasza referencja na nim (wpisy rozne od pliku)
		zdcore::SideEntry e[BS_ANI_N];
		int variant = -1;                   // wpisany wariant (log)
	};
	static BsSlot bsSlot[2];
	static zCModelPrototype* bsIdsBase = nullptr;
	static int bsIds[2][BS_ANI_N];
	static std::map<std::string, float> bsBaseFps;    // fpsRate z pliku per "NAKLADKA|ANIMACJA"
	static std::string bsScaleLogged;
	static bool bsScaleNow = true;         // przeskalowac w tej klatce (nowy wpis / prototyp)

	// ---------------------------------------------------------------- stan bohatera
	static uint32_t bsSecondHash = 0;
	static oCItem* bsSecond = nullptr;      // sprawdzony w ekwipunku w tej klatce
	static std::string bsSecondInst, bsSecondName;
	static zCVisual* bsSecondVis = nullptr; // nasza referencja (AddRef) - wizual w wezle ZS_BOW
	static bool bsPrimaryAtB = false;
	static oCItem* bsLastPrimary = nullptr;
	static std::string bsLastPrimaryInst;
	static bool bsVarsDirty = false;
	static unsigned bsResolveFrame = 0;
	static bool bsSecondMissingLogged = false;

	// ---------------------------------------------------------------- model, wezly, wizualy
	struct BsNodes {
		zCModel* model = nullptr;
		int spine2 = -1, crossbow = -1;
		int node[4] = { -1, -1, -1, -1 };   // zdcore::BNODE_*
		std::vector<int> chain[4];          // wezly od dziecka rodzica wezla do SPINE2 (iloczyn lokalnych macierzy)
		bool chainOk[4] = {};
	};
	static BsNodes bsN;
	struct BsOverride {
		int node = -1, kind = 0;
		zdcore::BackMat pos;
	};
	static BsOverride bsOv[2];
	static int bsOvN = 0;
	static zCModel* bsOvModel = nullptr;
	static int bsOvNodesSet[4] = { -1, -1, -1, -1 };   // wezly, ktore ostatnio przestawialismy (powrot do spoczynku)
	static bool bsBowShown = false;          // nasz wizual w wezle ZS_BOW
	static bool bsCbowHidden = false;
	static zdcore::BackPlan bsPlan;
	static std::string bsPlanLogged;

	// ---------------------------------------------------------------- zamiana
	static zdcore::BackSwapRun bsSwap;
	static oCItem* bsSwapNew = nullptr;
	static int bsSwapNewInstanz = -1;        // stos jednakowych mieczy: zalozenie odszczepia nowy przedmiot (SplitItem)
	static std::string bsSwapNewName;
	static bool bsSwapPending = false;       // z konsoli - po jej zamknieciu
	// straznik chwytu: nasza animacja dobycia przerwana w polowie (slot dloni przelozony)
	static const void* bsGripAni = nullptr;
	static float bsGripLeft = 0.0f;          // s do konca naszej animacji w ostatniej klatce, w ktorej grala
	static bool bsRestored = true;

	// ---------------------------------------------------------------- pomocnicze

	static void BsTell(const std::string& text) {
		if (ogame && ogame->GetTextView())
			ogame->GetTextView()->PrintTimedCXY(zSTRING(text.c_str()), 2500.0f, nullptr);
	}

	static zdcore::BackSettings BsSettings() {
		zdcore::BackSettings s;
		s.on = zcfg.backSwords;
		s.back1H = zcfg.back1H;
		s.beltSpeed = zcfg.beltDrawSpeed;
		s.second = zcfg.secondSword;
		s.withRanged = zcfg.secondWithRanged;
		return s;
	}

	static int BsMeleeType(oCItem* it) {
		return it ? zdcore::SwapTypeOfItemFlags(it->flags) : zdcore::BACK_TYPE_NONE;
	}

	static bool BsInInventory(oCItem* it) {
		if (!it || !player)
			return false;
		for (auto node = player->inventory2.inventory.next; node; node = node->next)
			if (node->GetData() == it)
				return true;
		return false;
	}

	// Zalozona bron biala: w rece (dobyta) albo w slocie.
	static oCItem* BsPrimary() {
		if (!player)
			return nullptr;
		int wm = player->GetWeaponMode();
		oCItem* w = wm >= NPC_WEAPON_DAG && wm <= NPC_WEAPON_2HS ? player->GetWeapon() : nullptr;
		return w ? w : player->GetEquippedMeleeWeapon();
	}

	static std::string BsInst(oCItem* it) {
		return it ? std::string(it->GetInstanceName().ToChar()) : std::string();
	}

	static void BsSetSecondVisual(zCVisual* v) {
		if (v == bsSecondVis)
			return;
		if (v)
			v->AddRef();
		if (bsSecondVis)
			bsSecondVis->Release();
		bsSecondVis = v;
	}

	static zCVisual* BsItemVisual(oCItem* it) {
		if (!it)
			return nullptr;
		zCVisual* v = it->GetVisual();
		if (!v) {
			it->CreateVisual();
			v = it->GetVisual();
		}
		return v;
	}

	// ---------------------------------------------------------------- zmienne skryptu (zapis)

	static zCPar_Symbol* BsSym(const char* name) {
		zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(name)) : nullptr;
		return s && s->type == zPAR_TYPE_INT ? s : nullptr;
	}

	static void BsWriteVars(const char* why) {
		zCPar_Symbol* a = BsSym("ZDF_BACK_SECOND");
		zCPar_Symbol* b = BsSym("ZDF_BACK_FLAGS");
		if (!a || !b) {
			static bool logged = false;
			if (!logged) {
				logged = true;
				ZLog("Miecze na plecach: brak zmiennych ZDF_BACK_SECOND / ZDF_BACK_FLAGS (zDismember_Back.d) - drugi miecz nie "
					"przetrwa zapisu");
			}
			return;
		}
		a->SetValue((int)bsSecondHash, 0);
		b->SetValue(bsPrimaryAtB ? zdcore::BACK_FLAG_PRIMARY_B : 0, 0);
		bsVarsDirty = false;
		ZDebug("Miecze na plecach: zmienne zapisu ZDF_BACK_SECOND=%u ZDF_BACK_FLAGS=%d (%s)", bsSecondHash, bsPrimaryAtB ? 1 : 0, why);
	}

	static void BsReadVars() {
		zCPar_Symbol* a = BsSym("ZDF_BACK_SECOND");
		zCPar_Symbol* b = BsSym("ZDF_BACK_FLAGS");
		int h = 0, f = 0;
		if (a)
			a->GetValue(h, 0);
		if (b)
			b->GetValue(f, 0);
		bsSecondHash = (uint32_t)h & 0x7FFFFFFFu;
		bsPrimaryAtB = (f & zdcore::BACK_FLAG_PRIMARY_B) != 0;
		bsVarsDirty = false;
	}

	// ---------------------------------------------------------------- drugi miecz

	static void BsSetSecond(oCItem* it, const char* why) {
		bsSecond = it;
		bsSecondInst = BsInst(it);
		bsSecondName = it ? std::string(it->name.ToChar()) : std::string();
		bsSecondHash = it ? zdcore::BackNameHash(bsSecondInst) : 0;
		BsSetSecondVisual(BsItemVisual(it));
		bsVarsDirty = true;
		bsSecondMissingLogged = false;
		ZLog("Miecze na plecach: drugi miecz %s%s%s (%s)", it ? bsSecondInst.c_str() : "brak", it ? " - " : "",
			it ? bsSecondName.c_str() : "", why);
	}

	// Drugi miecz w ekwipunku: wskaznik sprawdzany co klatke (lista ekwipunku), po skrocie szukany co 30 klatek.
	static void BsResolveSecond() {
		if (bsSecond && !BsInInventory(bsSecond)) {
			ZLog("Miecze na plecach: drugi miecz %s zniknal z ekwipunku (sprzedany / upuszczony) - czekam, az wroci", bsSecondInst.c_str());
			bsSecond = nullptr;
			BsSetSecondVisual(nullptr);
		}
		if (bsSecond || !bsSecondHash || zdFrame - bsResolveFrame < 30)
			return;
		bsResolveFrame = zdFrame;
		for (auto node = player->inventory2.inventory.next; node; node = node->next) {
			oCItem* it = node->GetData();
			if (!it || (it->flags & ITM_FLAG_ACTIVE) || BsMeleeType(it) == zdcore::BACK_TYPE_NONE)
				continue;
			std::string inst = BsInst(it);
			if (zdcore::BackNameHash(inst) == bsSecondHash) {
				bsSecond = it;
				bsSecondInst = inst;
				bsSecondName = it->name.ToChar();
				BsSetSecondVisual(BsItemVisual(it));
				ZLog("Miecze na plecach: drugi miecz z zapisu - %s (%s)", inst.c_str(), bsSecondName.c_str());
				return;
			}
		}
		if (!bsSecondMissingLogged) {
			bsSecondMissingLogged = true;
			ZLog("Miecze na plecach: drugiego miecza z zapisu (skrot %u) nie ma w ekwipunku - na plecach nic", bsSecondHash);
		}
	}

	// Zalozona bron zmienila sie: gdy zalozono drugi miecz (zamiana albo ekwipunek), dawna bron staje sie drugim mieczem
	// i miecze zamieniaja sie miejscami na plecach (zdcore::PrimaryAtBAfterSwap).
	static void BsTrackPrimary() {
		oCItem* prim = BsPrimary();
		if (prim == bsLastPrimary)
			return;
		oCItem* old = bsLastPrimary;
		std::string oldInst = bsLastPrimaryInst;
		bsLastPrimary = prim;
		bsLastPrimaryInst = BsInst(prim);
		// zalozony drugi miecz albo przedmiot odszczepiony z jego stosu (EquipItem -> SplitItem przy kilku jednakowych) -
		// stos tylko przy prawdziwej zmianie broni (po wczytaniu zalozony jednakowy miecz to nie zamiana)
		if (!prim || !bsSecond || (prim != bsSecond && (prim->instanz != bsSecond->instanz || !old)))
			return;
		bool oldOk = old && BsInInventory(old) && !(old->flags & ITM_FLAG_ACTIVE) && BsMeleeType(old) != zdcore::BACK_TYPE_NONE;
		bool atB = zdcore::PrimaryAtBAfterSwap(bsPlan);
		ZLog("Miecze na plecach: zalozony drugi miecz %s - %s, bron w %s", bsSecondInst.c_str(),
			oldOk ? ("dawna bron " + oldInst + " na plecy").c_str() : "dawnej broni brak w ekwipunku", atB ? "B" : "A");
		bsPrimaryAtB = atB;
		bsVarsDirty = true;
		BsSetSecond(oldOk ? old : nullptr, "zamiana");
	}

	// ---------------------------------------------------------------- wybor w ekwipunku (zwykly, Hedin Simple Inventory)
	// Hedin Simple Inventory (NB, HEDIN_SIMPLE_INVENTORY.DLL, deasemblacja w [notatki lokalne]): wlasna lista
	// (kategorie, mysz), ale podswietlony wiersz HSI wpisuje do zaznaczenia silnika - oCItemContainer::selectedItem (+24h)
	// gracza = indeks na liscie contents (GothicTwoInv::SetSelected, RVA 0x19EE0) - i sam czyta GetSelectedItem (podglad
	// przedmiotu, uzycie, ilosc). Wiec GetSelectedItem = wiersz pod kursorem / podswietlony tez w HSI.
	// Klawiatura: przy otwartym ekwipunku HSI w Game_PreLoop (przed naszym Game_Loop) w trybie wpisywania ilosci i w
	// wyszukiwarce (RShift+S / klik w pasek) co klatke zbiera klawisze i wola zinput->ClearKeyBuffer - keyevent silnika
	// wyzerowany, zinput->KeyPressed nie widzi klawisza. Stad zapas: fizyczny stan klawisza (GetAsyncKeyState) przy oknie
	// gry na pierwszym planie, pominiety, gdy wyszukiwarka HSI ma klawiature (litery ida do pola wyszukiwania).
	static const char* BS_HSI_DLL = "HEDIN_SIMPLE_INVENTORY.DLL";
	struct BsHsi {
		bool checked = false;
		HMODULE mod = nullptr;
		const volatile unsigned char* searchMode = nullptr;    // bFilterSearchMode z ini (RVA 0x90448)
		const volatile unsigned char* searchActive = nullptr;  // wyszukiwarka ma klawiature (RVA 0x9040B)
	};
	static BsHsi bsHsi;

	// Flagi wyszukiwarki HSI: Game_PreLoop HSI, RVA 0x1D903 (wersja z 4.08.2025, ts PE 1750389960):
	//   80 3D <bFilterSearchMode> 00 | 0F 84 rel32 | 80 3D <searchActive> 00 | 0F 84 rel32
	// Operandy (po relokacji) musza wskazywac w obraz modulu pod RVA 0x90448 / 0x9040B - inna wersja HSI = bez wykrywania.
	static void BsHsiResolve() {
		static unsigned lastFrame = 0;
		if (bsHsi.checked && zdFrame - lastFrame < 300)
			return;   // modul sprawdzany co 300 klatek (GetModuleHandle pod blokada loadera)
		lastFrame = zdFrame;
		HMODULE m = GetModuleHandleA(BS_HSI_DLL);
		if (m == bsHsi.mod && bsHsi.checked)
			return;
		bsHsi = BsHsi();
		bsHsi.checked = true;
		bsHsi.mod = m;
		if (!m)
			return;
		const unsigned char* base = (const unsigned char*)m;
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
		const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
		uint32_t size = nt->OptionalHeader.SizeOfImage;
		const uint32_t SIG = 0x1D903, MODE = 0x90448, ACTIVE = 0x9040B;
		bool ok = size > MODE + 1 && size > SIG + 32;
		const unsigned char* p = base + SIG;
		uint32_t opMode = 0, opActive = 0;
		if (ok) {
			ok = p[0] == 0x80 && p[1] == 0x3D && p[6] == 0x00 && p[7] == 0x0F && p[8] == 0x84 && p[13] == 0x80 && p[14] == 0x3D &&
				p[19] == 0x00 && p[20] == 0x0F && p[21] == 0x84;
			memcpy(&opMode, p + 2, 4);
			memcpy(&opActive, p + 15, 4);
			ok = ok && opMode == (uint32_t)(uintptr_t)(base + MODE) && opActive == (uint32_t)(uintptr_t)(base + ACTIVE);
		}
		if (ok) {
			bsHsi.searchMode = base + MODE;
			bsHsi.searchActive = base + ACTIVE;
		}
		ZLog("Drugi miecz: Hedin Simple Inventory (%s) - zaznaczenie z GetSelectedItem (HSI wpisuje podswietlony wiersz), "
			"wyszukiwarka HSI %s", BS_HSI_DLL, ok ? "wykrywana (wpisywanie liter nie wybiera miecza)" :
			"NIEROZPOZNANA wersja - G w polu wyszukiwania moze wybrac miecz");
	}

	static bool BsHsiTyping() {
		BsHsiResolve();
		return bsHsi.searchActive && bsHsi.searchMode && *bsHsi.searchMode && *bsHsi.searchActive;
	}

	// Okno gry na pierwszym planie (GetAsyncKeyState widzi klawisze calego systemu).
	static bool BsGameFocus() {
		HWND fg = GetForegroundWindow();
		DWORD pid = 0;
		if (fg)
			GetWindowThreadProcessId(fg, &pid);
		return fg && pid == GetCurrentProcessId();
	}

	// Klawisz fizycznie trzymany (DIK -> skan -> VK; rozszerzone 0x80+ = prefiks E0, jak ZMenu.cpp ZmPhysicallyDown).
	static bool BsPhysDown(int dik) {
		if (dik <= 0 || dik >= 0x100)
			return false;
		UINT sc = (dik & 0x80) ? (0xE000u | (UINT)(dik & 0x7F)) : (UINT)dik;
		UINT vk = MapVirtualKeyA(sc, MAPVK_VSC_TO_VK_EX);
		return vk != 0 && (GetAsyncKeyState((int)vk) & 0x8000) != 0;
	}

	static bool BsPhysModsHeld(int mods) {
		auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
		if ((mods & zdcore::SWAP_MOD_CTRL) && !down(VK_CONTROL))
			return false;
		if ((mods & zdcore::SWAP_MOD_SHIFT) && !down(VK_SHIFT))
			return false;
		if ((mods & zdcore::SWAP_MOD_ALT) && !down(VK_MENU))
			return false;
		return true;
	}

	static std::string BsItemDesc(oCItem* it) {
		if (!it)
			return "brak";
		char buf[256];
		int t = BsMeleeType(it);
		sprintf_s(buf, "%s '%s' (flagi 0x%X, %s, ilosc %d%s)", BsInst(it).c_str(), it->name.ToChar(), (unsigned)it->flags,
			t == zdcore::BACK_TYPE_2H ? "bron 2H" : t == zdcore::BACK_TYPE_NONE ? "nie bron biala" : "bron 1H", it->amount,
			(it->flags & ITM_FLAG_ACTIVE) ? ", zalozony" : "");
		return buf;
	}

	// Zaznaczenie silnika: indeks / liczba na liscie contents, przewiniecie.
	static std::string BsSelDesc(oCItem* it) {
		oCNpcInventory& inv = player->inventory2;
		int n = 0;
		for (auto node = inv.contents ? inv.contents->next : nullptr; node; node = node->next)
			n++;
		char buf[96];
		sprintf_s(buf, "indeks %d z %d (przewiniecie %d): ", inv.selectedItem, n, inv.offset);
		return buf + BsItemDesc(it);
	}

	static void BsPickTell(const std::string& text) {
		ZDebug("Drugi miecz: napis \"%s\"", text.c_str());
		BsTell(text);
	}

	static void BsPickFromInventory(int source) {
		oCItem* it = player->inventory2.GetSelectedItem();
		const char* inv = bsHsi.mod ? "Hedin Simple Inventory" : "ekwipunek gry";
		static bool detailLogged = false;
		std::string sel = BsSelDesc(it);
		if (!detailLogged || zcfg.debug) {
			detailLogged = true;
			ZLog("Drugi miecz [klawisz w ekwipunku, %s, zrodlo %s]: zaznaczony %s", inv, zdcore::SecondKeySourceLabel(source),
				sel.c_str());
		}
		if (!it || BsMeleeType(it) == zdcore::BACK_TYPE_NONE) {
			ZLog("Drugi miecz: odmowa - zaznaczony %s (nie bron biala)", it ? BsInst(it).c_str() : "brak");
			BsPickTell("Drugi miecz: zaznacz bro\xF1 bia\xB3\xB9");
			return;
		}
		if (it->flags & ITM_FLAG_ACTIVE) {
			ZLog("Drugi miecz: odmowa - %s jest zalozony", BsInst(it).c_str());
			BsPickTell("Drugi miecz: to bro\xF1 za\xB3o\xBFona - zaznacz inn\xB9");
			return;
		}
		if (it == bsSecond) {
			BsSetSecond(nullptr, "ekwipunek - odznaczony");
			BsPickTell("Drugi miecz: brak");
			return;
		}
		BsSetSecond(it, "ekwipunek");
		BsPickTell("Drugi miecz: " + bsSecondName);
	}

	// ---------------------------------------------------------------- wpisy nakladek

	static void BsIds(zCModel* m) {
		zCModelPrototype* base = m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		if (base == bsIdsBase)
			return;
		bsIdsBase = base;
		for (int t = 0; t < 2; t++)
			for (int k = 0; k < BS_ANI_N; k++)
				bsIds[t][k] = AniId(m, BS_ANIS[t][k]);
	}

	// Prototyp wariantu B / BELT: wczytany pod baza HUMANS bohatera, trzymany przez nas (jak szybkie ciecia 2H).
	static zCModelPrototype* BsLoadedProto(zCModel* m, int k) {
		zCModelPrototype* base = m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		if (!base)
			return nullptr;
		if (bsLoaded[k])
			return bsLoadedBase[k] == base ? bsLoaded[k] : nullptr;
		if (bsLoadTried[k])
			return nullptr;
		bsLoadTried[k] = true;
		if (!ZOverlayExists(BS_LOADED[k], BS_ANIS[k == 1 ? 1 : 0][0])) {
			ZLog("Miecze na plecach: brak %s w paczce - wariant zastapiony (B -> A, BELT -> natywne)", BS_LOADED[k]);
			return nullptr;
		}
		zCModelPrototype* p = zCModelPrototype::Load(zSTRING((std::string(BS_LOADED[k]) + ".MDS").c_str()), base);
		if (!p) {
			ZLog("Miecze na plecach: silnik nie wczytal %s", BS_LOADED[k]);
			return nullptr;
		}
		if (p->baseModelProto != base) {
			ZLog("Miecze na plecach: %s pod inna baza niz model bohatera - pomijam", BS_LOADED[k]);
			p->Release();
			return nullptr;
		}
		bsLoaded[k] = p;
		bsLoadedBase[k] = base;
		bsScaleNow = true;
		ZLog("Miecze na plecach: wczytano %s", BS_LOADED[k]);
		return p;
	}

	// Wpis k -> want. Referencje jak w AtkTempo (AtkSideWrite): obca animacja +1 (oddawana przy zdjeciu), oryginal z pliku po
	// wyjeciu trzymamy my (jego referencja z tablicy) i oddajemy tablicy przy powrocie.
	static void BsWrite(zCModelPrototype* p, int id, zdcore::SideEntry& e, zCModelAni* want) {
		zCModelAni* cur = p->protoAnis.GetArray()[id];
		if (cur && cur != e.orig)
			cur->Release();
		if (want && want != e.orig)
			want->AddRef();
		p->protoAnis.GetArray()[id] = want;
		zdcore::SideEntryWrote(e, want);
	}

	static void BsHold(BsSlot& s) {
		bool changed = false;
		for (auto& e : s.e)
			changed = changed || e.changed;
		if (changed && !s.held && s.proto) {
			s.proto->refCtr++;
			s.held = true;
		}
		else if (!changed && s.held && s.proto) {
			s.held = false;
			s.proto->Release();
		}
	}

	static void BsRestoreSlot(zCModel* m, int t, const char* why) {
		BsSlot& s = bsSlot[t];
		zCModelPrototype* p = s.proto;
		bool any = false;
		for (int k = 0; p && k < BS_ANI_N; k++) {
			zdcore::SideEntry& e = s.e[k];
			if (!e.changed || !e.known)
				continue;
			int id = e.orig ? ((zCModelAni*)e.orig)->aniID : m ? AniId(m, BS_ANIS[t][k]) : -1;
			if (id >= 0 && id < p->protoAnis.GetNum() && p->protoAnis.GetArray()[id] == e.placed) {
				BsWrite(p, id, e, (zCModelAni*)e.orig);
				any = true;
			}
		}
		if (any)
			ZLog("Miecze na plecach: %s - wpisy dobycia z pliku (%s)", BS_OV[t], why);
		BsHold(s);
		s = BsSlot();
	}

	static void BsEntriesUpdate(zCModel* m, int t, int variant) {
		zCModelPrototype* p = AtkModelProto(m, BS_OV[t]);
		if (!p)
			return;   // nakladka nie na modelu (plan Styles jeszcze jej nie zalozyl / zdjeta przed zapisem)
		BsSlot& s = bsSlot[t];
		if (s.proto && s.proto != p)
			BsRestoreSlot(m, t, "inny prototyp nakladki na modelu");
		s.proto = p;
		zCModelPrototype* src = nullptr;
		int eff = variant;
		if (variant == zdcore::DRAWV_B) {
			src = BsLoadedProto(m, t);
			if (!src)
				eff = zdcore::DRAWV_A;
		}
		else if (variant == zdcore::DRAWV_BELT) {
			src = t == 0 ? BsLoadedProto(m, BS_LOADED_BELT) : nullptr;
			if (!src)
				eff = zdcore::DRAWV_NATIVE;
		}
		bool pending = false;
		for (int k = 0; k < BS_ANI_N; k++) {
			int id = bsIds[t][k];
			if (id < 0 || id >= p->protoAnis.GetNum())
				continue;
			zCModelAni* cur = p->protoAnis.GetArray()[id];
			zdcore::SideEntryObserve(s.e[k], cur);
			zCModelAni* want = eff == zdcore::DRAWV_A ? (zCModelAni*)s.e[k].orig : eff == zdcore::DRAWV_NATIVE ? nullptr :
				ProtoAni(src, id);
			if (want == cur)
				continue;
			if (AtkAniPlaying(m, cur) || AtkAniPlaying(m, want)) {
				pending = true;   // gra stara wersja (aktywna animacja trzyma wskaznik) - po jej koncu
				continue;
			}
			BsWrite(p, id, s.e[k], want);
			bsScaleNow = true;
		}
		BsHold(s);
		if (!pending && eff != s.variant) {
			s.variant = eff;
			ZLog("Miecze na plecach: dobycie %s = %s%s", t == 0 ? "1H" : "2H", zdcore::DrawVariantLabel(eff),
				eff != variant ? " (wariantu z planu brak w paczce)" : "");
		}
	}

	// Tempo: fpsRate naszych animacji = plik x % (BackDrawSpeed / BackSheatheSpeed; BELT - BeltDrawSpeed).
	static void BsScaleAni(const char* proto, zCModelAni* a, int percent) {
		if (!a)
			return;
		std::string key = std::string(proto) + "|" + a->aniName.ToChar();
		auto it = bsBaseFps.find(key);
		if (it == bsBaseFps.end())
			it = bsBaseFps.emplace(key, a->fpsRate > 0.0f ? a->fpsRate : 25.0f).first;
		float target = it->second * (float)percent / 100.0f;
		if (fabsf(a->fpsRate - target) > 1e-3f)
			a->fpsRate = target;
	}

	static void BsScaleAll(zCModel* m, bool toFile) {
		int draw = toFile ? 100 : zcfg.backDrawSpeed, sheathe = toFile ? 100 : zcfg.backSheatheSpeed;
		int belt = toFile ? 100 : zcfg.beltDrawSpeed;
		for (int t = 0; t < 2; t++)
			for (int k = 0; k < BS_ANI_N; k++) {
				int pct = BS_ANI_SHEATHE[k] ? sheathe : draw;
				if (bsSlot[t].e[k].known)
					BsScaleAni(BS_OV[t], (zCModelAni*)bsSlot[t].e[k].orig, pct);
				int id = bsIds[t][k];
				if (bsLoaded[t])
					BsScaleAni(BS_LOADED[t], ProtoAni(bsLoaded[t], id), pct);
				if (t == 0 && bsLoaded[BS_LOADED_BELT])
					BsScaleAni(BS_LOADED[BS_LOADED_BELT], ProtoAni(bsLoaded[BS_LOADED_BELT], id), belt);
			}
		char buf[96];
		sprintf_s(buf, "%d/%d/%d", draw, sheathe, belt);
		if (!toFile && bsScaleLogged != buf) {
			bsScaleLogged = buf;
			ZLog("Miecze na plecach: tempo dobycia %d%%, chowania %d%% (zza plecow), przy pasie %d%%", draw, sheathe, belt);
		}
	}

	// ---------------------------------------------------------------- wezly i wizualy

	static zdcore::BackMat BsFromZ(const zMAT4& z) {
		zdcore::BackMat b;
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 4; c++)
				b.m[r][c] = z.v[r].n[c];
		return b;
	}

	static void BsToZ(const zdcore::BackMat& b, zMAT4& z) {
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 4; c++)
				z.v[r].n[c] = b.m[r][c];
		z.v[3].n[0] = z.v[3].n[1] = z.v[3].n[2] = 0.0f;
		z.v[3].n[3] = 1.0f;
	}

	static int BsNodeIndex(zCModel* m, zCModelNodeInst* n) {
		for (int i = 0; i < m->nodeList.GetNum(); i++)
			if (m->nodeList[i] == n)
				return i;
		return -1;
	}

	static void BsNodesFor(zCModel* m) {
		if (bsN.model == m)
			return;
		bsN = BsNodes();
		bsN.model = m;
		bsBowShown = false;      // stary model (zniknal albo przebudowany) - nasze wizualy razem z nim
		bsCbowHidden = false;
		for (int& n : bsOvNodesSet)
			n = -1;
		bsN.spine2 = FindNode(m, "BIP01 SPINE2");
		bsN.crossbow = FindNode(m, "ZS_CROSSBOW");
		std::string info;
		for (int k = zdcore::BNODE_SWORD; k <= zdcore::BNODE_BOW; k++) {
			bsN.node[k] = FindNode(m, BS_NODE_NAMES[k]);
			int n = bsN.node[k];
			if (n < 0 || bsN.spine2 < 0)
				continue;
			zCModelNodeInst* parent = m->nodeList[n]->parentNode;
			std::vector<int> up;
			zCModelNodeInst* cur = m->nodeList[bsN.spine2];
			while (cur && cur != parent) {
				up.push_back(BsNodeIndex(m, cur));
				cur = cur->parentNode;
			}
			if (cur == parent && std::find(up.begin(), up.end(), -1) == up.end()) {
				bsN.chain[k].assign(up.rbegin(), up.rend());
				bsN.chainOk[k] = true;
			}
			info += std::string(info.empty() ? "" : ", ") + BS_NODE_NAMES[k] + (bsN.chainOk[k] ? " (" + std::to_string(up.size()) +
				" wezly do SPINE2)" : " (rodzic poza kregoslupem - bez przestawiania)");
		}
		ZDebug("Miecze na plecach: model bohatera %s - %s", m->GetVisualName().ToChar(), info.c_str());
	}

	static zCVisual* BsSlotVisual(const char* slot) {
		oCItem* it = player->GetSlotItem(zSTRING(slot));
		return it ? it->GetVisual() : nullptr;
	}

	static void BsVisuals(zCModel* m, const zdcore::BackPlan& p) {
		int bow = bsN.node[zdcore::BNODE_BOW];
		if (bow >= 0) {
			zCModelNodeInst* n = m->nodeList[bow];
			if (p.second && bsSecondVis) {
				if (n->nodeVisual != bsSecondVis)
					m->SetNodeVisual(n, bsSecondVis, FALSE);
				bsBowShown = true;
			}
			else if (bsBowShown) {
				zCVisual* v = BsSlotVisual("ZS_BOW");   // luk w slocie wraca na plecy, inaczej pusto
				if (n->nodeVisual != v)
					m->SetNodeVisual(n, v, FALSE);
				bsBowShown = false;
			}
		}
		if (bsN.crossbow >= 0 && !XbowInHand()) {   // szybki strzal: kusza w lewej dloni - wezel prowadzi Crossbow.cpp
			zCModelNodeInst* n = m->nodeList[bsN.crossbow];
			if (p.hideCrossbow) {
				if (n->nodeVisual)
					m->SetNodeVisual(n, nullptr, FALSE);
				bsCbowHidden = true;
			}
			else if (bsCbowHidden) {
				zCVisual* v = BsSlotVisual("ZS_CROSSBOW");
				if (n->nodeVisual != v)
					m->SetNodeVisual(n, v, FALSE);
				bsCbowHidden = false;
			}
		}
	}

	static zdcore::BackMat BsPosFor(int spot) {
		float* adj = zcfg.backAdjust[spot == zdcore::SPOT_B ? 1 : 0];
		return zdcore::BackAdjust(bsPos[spot == zdcore::SPOT_B ? zdcore::SPOT_B : zdcore::SPOT_A], adj);
	}

	static void BsOverrides(zCModel* m, const zdcore::BackPlan& p) {
		BsOverride next[2];
		int n = 0;
		if (p.primaryNode != zdcore::BNODE_NONE && p.primarySpot != zdcore::SPOT_NATIVE && bsN.node[p.primaryNode] >= 0 &&
			bsN.chainOk[p.primaryNode])
			next[n++] = { bsN.node[p.primaryNode], p.primaryNode, BsPosFor(p.primarySpot) };
		if (p.second && bsN.node[zdcore::BNODE_BOW] >= 0 && bsN.chainOk[zdcore::BNODE_BOW])
			next[n++] = { bsN.node[zdcore::BNODE_BOW], zdcore::BNODE_BOW, BsPosFor(p.secondSpot) };
		// wezly, ktorych juz nie przestawiamy: od razu spoczynek (animacja nadpisze w nastepnej klatce)
		for (int k = 1; k < 4; k++) {
			bool still = false;
			for (int i = 0; i < n; i++)
				still = still || next[i].kind == k;
			if (!still && bsOvNodesSet[k] >= 0 && bsOvNodesSet[k] < m->nodeList.GetNum()) {
				zCModelNodeInst* node = m->nodeList[bsOvNodesSet[k]];
				if (node->protoNode)
					node->trafo = node->protoNode->trafo;
				bsOvNodesSet[k] = -1;
			}
		}
		for (int i = 0; i < n; i++)
			bsOvNodesSet[next[i].kind] = next[i].node;
		bsOv[0] = next[0];
		bsOv[1] = next[1];
		bsOvN = n;
		bsOvModel = m;
	}

	// ---------------------------------------------------------------- straznik przestawienia (23.10)
	// Polozenie wezla w ukladzie modelu: iloczyn lokalnych macierzy po rodzicach (parentNode, jak silnik przy renderze).
	static zdcore::BackMat BsObj(zCModelNodeInst* n, const zdcore::BackMat* localOverride = nullptr) {
		zdcore::BackMat acc = localOverride ? *localOverride : BsFromZ(n->trafo);
		int guard = 0;
		for (zCModelNodeInst* p = n->parentNode; p && guard < 64; p = p->parentNode, guard++)
			acc = zdcore::BackMul(BsFromZ(p->trafo), acc);
		return acc;
	}

	static std::string BsLayerAnis(zCModel* m) {
		std::string s;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (a && a->protoAni && !a->isFadingOut)
				s += std::string(s.empty() ? "" : " ") + a->protoAni->aniName.ToChar() + "(w" + std::to_string(a->protoAni->layer) + ")";
		}
		return s.empty() ? "-" : s;
	}

	static int bsGuardLogs = 0;
	static unsigned bsGuardLast[4] = {};
	static int bsGuardRejected = 0;          // odrzucone przestawienia od ostatniego wpisu

	// Przestawienie odrzucone (lancuch wezlow do SPINE2 nieaktualny - inny szkielet / przebudowany model, NaN): wezel
	// natywnie w tej klatce (animacja kluczuje - zostaje jej wartosc, inaczej spoczynek), wpis raz na serie.
	static void BsGuardReject(zCModel* m, const BsOverride& o, float err) {
		zCModelNodeInst* node = m->nodeList[o.node];
		if (node->numNodeAnis <= 0 && node->protoNode)
			node->trafo = node->protoNode->trafo;
		bsGuardRejected++;
		if (bsGuardLogs >= 20 || (bsGuardLast[o.kind] && zdFrame - bsGuardLast[o.kind] < 300))
			return;
		bsGuardLogs++;
		bsGuardLast[o.kind] = zdFrame ? zdFrame : 1;
		ZLog("Miecze na plecach: STRAZNIK [kl. %u] - %s: polozenie wzgledem BIP01 SPINE2 po przestawieniu %s (blad %.1f cm, "
			"prog %.0f) - wezel natywnie w tej klatce, lancuch wezlow odswiezony; model %s, wezlow %d, animacje %s, stan ciala %d, "
			"tryb broni %d (odrzuconych od ostatniego wpisu: %d)", zdFrame, BS_NODE_NAMES[o.kind], err < 0.0f ? "niepoprawne (NaN)" :
			"inne niz z planu", err, zdcore::BACK_GUARD_TOL, m->GetVisualName().ToChar(), m->nodeList.GetNum(), BsLayerAnis(m).c_str(),
			player ? player->GetBodyState() & BS_ONLY_STATE : -1, player ? player->GetWeaponMode() : -1, bsGuardRejected);
		bsGuardRejected = 0;
		bsN.model = nullptr;   // lancuch od nowa w nastepnej klatce petli (BsNodesFor)
	}

	void BackSwordsApply(zCModel* m) {
		if (!bsOvN || m != bsOvModel)
			return;
		int nn = m->nodeList.GetNum();
		if (bsN.model != m || bsN.spine2 < 0 || bsN.spine2 >= nn)
			return;
		zdcore::BackMat spineObj = BsObj(m->nodeList[bsN.spine2]);
		for (int i = 0; i < bsOvN; i++) {
			const BsOverride& o = bsOv[i];
			if (o.node < 0 || o.node >= nn)
				continue;
			zdcore::BackMat L = o.pos;
			const std::vector<int>& ch = bsN.chain[o.kind];
			bool chainOk = true;
			for (int c : ch)
				chainOk = chainOk && c >= 0 && c < nn;
			if (!chainOk) {
				BsGuardReject(m, o, -1.0f);
				continue;
			}
			if (!ch.empty()) {
				zdcore::BackMat acc = BsFromZ(m->nodeList[ch[0]]->trafo);
				for (size_t j = 1; j < ch.size(); j++)
					acc = zdcore::BackMul(acc, BsFromZ(m->nodeList[ch[j]]->trafo));
				L = zdcore::BackMul(acc, o.pos);
			}
			// straznik: wezel z nowa lokalna macierza w ukladzie modelu (po prawdziwych rodzicach) wzgledem SPINE2 = pozycja
			// z planu; inaczej (lancuch z BsNodesFor nie pasuje do hierarchii modelu, NaN) - natywny slot w tej klatce
			float err = zdcore::BackOffsetError(spineObj, BsObj(m->nodeList[o.node], &L), o.pos);
			if (err < 0.0f || err > zdcore::BACK_GUARD_TOL) {
				BsGuardReject(m, o, err);
				continue;
			}
			BsToZ(L, m->nodeList[o.node]->trafo);
		}
	}

	// ---------------------------------------------------------------- diagnostyka (23.10, zrzut z mieczem "w powietrzu")
	// Co 10 klatek: (1) miecz z planu w macierzach z ostatniego renderu (trafoObjToCam) - odleglosc od kregoslupa a |pozycja
	// z planu|, z para kontrolna BIP01 HEAD - SPINE2 (renderer nie odswiezyl macierzy = bez wpisu); (2) przedmiot w lewej
	// dloni (ZS_LEFTHAND: lewa bron pary NB, pochodnia) przy wezle bez kluczy animacji - wisi w ostatniej / spoczynkowej
	// pozie wezla, a nie tam, gdzie kladzie go animacja NB (1x2 / 2x2 kluczuja ZS_LEFTHAND 34 cm od spoczynku). Wpis raz
	// na serie (powrot do normy przez 60 klatek konczy serie), najwyzej 20 na sesje.
	static int bsDiagHead = -2, bsDiagLeft = -2;
	static zCModel* bsDiagModel = nullptr;
	static int bsDiagLogs = 0;
	static bool bsDiagSword = false;
	static unsigned bsDiagSwordOk = 0;
	static std::string bsDiagLeftSig;

	static float BsCamDist(zCModel* m, int a, int b) {
		const zMAT4& ma = m->nodeList[a]->trafoObjToCam;
		const zMAT4& mb = m->nodeList[b]->trafoObjToCam;
		float dx = ma.v[0].n[3] - mb.v[0].n[3], dy = ma.v[1].n[3] - mb.v[1].n[3], dz = ma.v[2].n[3] - mb.v[2].n[3];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	static void BsDiag(zCModel* m) {
		if (zdFrame % 10 != 0 || bsDiagLogs >= 20 || bsN.model != m || bsN.spine2 < 0)
			return;
		if (bsDiagModel != m) {
			bsDiagModel = m;
			bsDiagHead = FindNode(m, "BIP01 HEAD");
			bsDiagLeft = FindNode(m, "ZS_LEFTHAND");
			bsDiagSword = false;
			bsDiagLeftSig.clear();
		}
		int nn = m->nodeList.GetNum();
		// (1) miecz z planu w renderze
		bool drifted = false;
		for (int i = 0; i < bsOvN && bsDiagHead >= 0 && bsDiagHead < nn; i++) {
			const BsOverride& o = bsOv[i];
			if (o.node < 0 || o.node >= nn || !m->nodeList[o.node]->nodeVisual)
				continue;
			float expected = sqrtf(o.pos.m[0][3] * o.pos.m[0][3] + o.pos.m[1][3] * o.pos.m[1][3] + o.pos.m[2][3] * o.pos.m[2][3]);
			zdcore::BackMat h = BsObj(m->nodeList[bsDiagHead]), sp = BsObj(m->nodeList[bsN.spine2]);
			float refRest = zdcore::BackMatDistance(h, sp);
			float drift = 0.0f;
			if (!zdcore::BackRenderDrift(BsCamDist(m, o.node, bsN.spine2), expected, BsCamDist(m, bsDiagHead, bsN.spine2), refRest, drift))
				continue;
			drifted = true;
			if (bsDiagSword)
				break;
			bsDiagSword = true;
			bsDiagLogs++;
			ZLog("Miecze na plecach: UWAGA [kl. %u] - miecz w %s w renderze %.0f cm od BIP01 SPINE2 (z planu %.0f, roznica %.0f cm) - "
				"przestawienie po animacji nie doszlo do renderu; animacje %s, stan ciala %d, tryb broni %d, mob/interakcja %d, "
				"nakladek %d, para NB %s", zdFrame, BS_NODE_NAMES[o.kind], BsCamDist(m, o.node, bsN.spine2), expected, drift,
				BsLayerAnis(m).c_str(), player->GetBodyState() & BS_ONLY_STATE, player->GetWeaponMode(),
				player->GetInteractMob() ? 1 : 0, m->modelProtoList.GetNum() - 1, zdcore::DualLabel(HeroDualState()));
			break;
		}
		if (!drifted && bsDiagSword) {
			if (!bsDiagSwordOk)
				bsDiagSwordOk = zdFrame;
			else if (zdFrame - bsDiagSwordOk >= 60) {
				bsDiagSword = false;
				bsDiagSwordOk = 0;
				ZLog("Miecze na plecach: miecz znow na plecach w renderze [kl. %u]", zdFrame);
			}
		}
		else
			bsDiagSwordOk = 0;
		// (2) lewa dlon
		oCItem* lh = player->GetWeaponMode() != NPC_WEAPON_NONE ? player->GetSlotItem(zSTRING("ZS_LEFTHAND")) : nullptr;
		if (lh && !(lh->mainflag & ITM_CAT_NF) && !(lh->flags & ITM_FLAG_SHIELD))
			lh = nullptr;   // pochodnia itp. - spoczynkowa poza wezla jest dla nich wlasciwa
		if (!lh || bsDiagLeft < 0 || bsDiagLeft >= nn) {
			bsDiagLeftSig.clear();
			return;
		}
		zCModelNodeInst* ln = m->nodeList[bsDiagLeft];
		bool keyed = ln->numNodeAnis > 0;
		std::string top;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL && top.empty(); i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (a && a->protoAni && !a->isFadingOut && a->protoAni->layer == 1)
				top = std::string(a->protoAni->aniName.ToChar()) + " z " + OverlayOfAni(m, a->protoAni);
		}
		std::string sig = std::string(lh->GetInstanceName().ToChar()) + (keyed ? "|k|" : "|n|") + top;
		if (keyed || sig == bsDiagLeftSig)
			return;
		bsDiagLeftSig = sig;
		bsDiagLogs++;
		float off = 0.0f;
		if (ln->protoNode) {
			zdcore::BackMat a = BsFromZ(ln->trafo), b = BsFromZ(ln->protoNode->trafo);
			off = zdcore::BackMatDistance(a, b);
		}
		ZLog("Miecze na plecach: lewa dlon [kl. %u] - %s w ZS_LEFTHAND, wezel bez kluczy animacji (gra %s; tryb broni %d, para NB %s) "
			"- przedmiot w ostatniej / spoczynkowej pozie wezla (%.0f cm od spoczynku), moze wisiec obok dloni; animacje NB 1x2 / "
			"2x2 kluczuja ZS_LEFTHAND", zdFrame, lh->GetInstanceName().ToChar(), top.empty() ? "-" : top.c_str(), player->GetWeaponMode(),
			zdcore::DualLabel(HeroDualState()), off);
	}

	// ---------------------------------------------------------------- straznik chwytu

	// Nasza animacja dobycia / chowania przerwana przed koncem (trafienie, Interrupt) zostawia przelozony slot dloni
	// ZS_RIGHTHAND - animacje zEBA nim nie steruja, wiec "przykleja sie" (jak po finisherze) - powrot do spoczynku.
	static void BsGripGuard(zCModel* m, float dt) {
		const void* found = nullptr;
		float left = 0.0f;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni)
				continue;
			for (int t = 0; t < 2 && !found; t++)
				for (int k = 0; k < BS_ANI_N && !found; k++) {
					bool ours = (bsSlot[t].e[k].known && a->protoAni == bsSlot[t].e[k].orig) ||
						(bsLoaded[t] && a->protoAni == ProtoAni(bsLoaded[t], bsIds[t][k]));
					if (ours) {
						found = a->protoAni;
						float fps = a->protoAni->fpsRate > 0.01f ? a->protoAni->fpsRate : 25.0f;
						left = ((float)a->protoAni->numFrames - 1.0f - a->actFrame) / fps;
					}
				}
			if (found)
				break;
		}
		// koniec naturalny (nastepna animacja z silnika) - zostalo najwyzej ok. 2 klatki gry; inaczej przerwanie
		if (bsGripAni && !found && bsGripLeft > (2.5f * dt > 0.06f ? 2.5f * dt : 0.06f)) {
			ResetWeaponGrip(player);
			ZDebug("Miecze na plecach: animacja dobycia / chowania przerwana (%.2f s przed koncem) - chwyt dloni z powrotem", bsGripLeft);
		}
		bsGripAni = found;
		bsGripLeft = left;
	}

	// ---------------------------------------------------------------- zamiana

	static bool BsBusy(std::string& what) {
		if (DirectorActive())
			what = "finisher";
		else if (MovesActive())
			what = "unik / przeskok";
		else if (MeditateActive())
			what = "medytacja";
		else if (PathMenuOpen())
			what = "okno sciezek";
		else if (LootActive() || IaActive())
			what = "interakcja z bronia";
		else
			return false;
		return true;
	}

	static zdcore::BackSwapGate BsGate() {
		zdcore::BackSwapGate g;
		g.secondValid = bsSecond && BsInInventory(bsSecond) && !(bsSecond->flags & ITM_FLAG_ACTIVE);
		g.inventoryOpen = player->inventory2.IsOpen() != 0;
		g.dialog = !oCInformationManager::GetInformationManager().HasFinished();
		g.console = zcon && zcon->IsVisible();
		g.dead = player->IsDead() || player->IsUnconscious();
		g.swapping = bsSwap.phase != zdcore::SWP_IDLE;
		g.weaponMsg = HeroWeaponMessagePending();
		g.attacking = HeroAttacking();
		g.mounted = HeroMounted();
		g.dualPair = HeroDualState() != zdcore::DUAL_NONE;
		g.busy = BsBusy(g.busyWhat);
		return g;
	}

	static bool BsStanding() {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		return ac && ac->IsStanding();
	}

	static void BsSendWeapon(bool draw, int mode) {
		zCEventManager* em = player->GetEM(FALSE);
		if (!em)
			return;
		// jak PC_WeaponMove przy dobyciu (LootInteract): na stojaco EV_DRAWWEAPON1, w ruchu EV_DRAWWEAPON; chowanie jak
		// medytacja - EV_REMOVEWEAPON (silnik sam wybiera chowanie na stojaco / w biegu)
		bool standing = BsStanding();
		oCMsgWeapon::TWeaponSubType sub = draw ? (standing ? oCMsgWeapon::EV_DRAWWEAPON1 : oCMsgWeapon::EV_DRAWWEAPON) :
			oCMsgWeapon::EV_REMOVEWEAPON;
		em->OnMessage(new oCMsgWeapon(sub, mode, 0), player);
		ZDebug("Miecze na plecach: zamiana - %s (%s, tryb %d)", draw ? "dobycie" : "chowanie", standing ? "na stojaco" : "w ruchu", mode);
	}

	static bool BsIsSwapNew(oCItem* prim) {
		return prim && bsSwapNew && (prim == bsSwapNew || prim->instanz == bsSwapNewInstanz);
	}

	static int BsDrawMode(oCItem* it) {
		return BsMeleeType(it) == zdcore::BACK_TYPE_2H ? NPC_WEAPON_2HS : NPC_WEAPON_1HS;
	}

	static void BsSwapRequest(const char* who) {
		zdcore::BackSwapGate g = BsGate();
		std::string why = zdcore::BackSwapRefusal(g);
		if (!why.empty()) {
			ZLog("Miecze na plecach: zamiana (%s) odrzucona - %s", who, why.c_str());
			if (!g.secondValid)
				BsTell("Drugi miecz: brak - w ekwipunku zaznacz bro\xF1 i naci\x9Cnij klawisz zamiany");
			return;
		}
		oCItem* prim = BsPrimary();
		int wm = player->GetWeaponMode();
		bool melee = wm >= NPC_WEAPON_DAG && wm <= NPC_WEAPON_2HS;
		bsSwapNew = bsSecond;
		bsSwapNewInstanz = bsSecond->instanz;
		bsSwapNewName = bsSecondName;
		if (!melee) {
			// bez dobytej broni bialej (schowana, piesci, luk, czar w rece): sama zmiana wyposazenia
			player->Equip(bsSwapNew);
			bool ok = BsIsSwapNew(BsPrimary());
			ZLog("Miecze na plecach: zamiana (%s, bron schowana / inna w rece, tryb %d) - %s %s", who, wm, bsSecondInst.c_str(),
				ok ? "zalozony" : "NIE zalozony (wymagania / skrypt)");
			if (ok) {
				BsTrackPrimary();
				BsTell("Bro\xF1: " + bsSwapNewName);
			}
			bsSwapNew = nullptr;
			return;
		}
		zdcore::BackSwapStart(bsSwap);
		ZLog("Miecze na plecach: zamiana (%s) - %s -> %s: chowanie, zalozenie, dobycie", who, BsInst(prim).c_str(),
			bsSecondInst.c_str());
	}

	static void BsSwapStep(float dt) {
		if (bsSwap.phase == zdcore::SWP_IDLE)
			return;
		zdcore::BackSwapTick t;
		t.dt = dt;
		t.weaponMode = player->GetWeaponMode();
		t.weaponMsg = HeroWeaponMessagePending();
		t.newEquipped = BsIsSwapNew(BsPrimary());
		std::string busy;
		t.interrupted = player->IsDead() || player->IsUnconscious() || !oCInformationManager::GetInformationManager().HasFinished() ||
			HeroMounted() || BsBusy(busy);
		int phase = bsSwap.phase;
		switch (zdcore::BackSwapStep(bsSwap, t)) {
		case zdcore::SWA_REMOVE:
			BsSendWeapon(false, 0);
			break;
		case zdcore::SWA_EQUIP:
			if (bsSwapNew && BsInInventory(bsSwapNew) && !(bsSwapNew->flags & ITM_FLAG_ACTIVE)) {
				player->Equip(bsSwapNew);
				BsTrackPrimary();
			}
			ZLog("Miecze na plecach: zamiana - bron schowana (%.2f s), zalozenie %s: %s", bsSwap.total, bsSwapNewName.c_str(),
				BsIsSwapNew(BsPrimary()) ? "tak" : "NIE");
			break;
		case zdcore::SWA_DRAW:
			BsSendWeapon(true, BsDrawMode(BsPrimary()));
			break;
		case zdcore::SWA_DONE:
			ZLog("Miecze na plecach: zamiana - %s (%.2f s, tryb %d)", bsSwap.why.c_str(), bsSwap.total, t.weaponMode);
			BsTell("Bro\xF1: " + bsSwapNewName);
			bsSwapNew = nullptr;
			break;
		case zdcore::SWA_FAIL:
			ZLog("Miecze na plecach: zamiana przerwana w fazie %s - %s%s%s", zdcore::BackSwapPhaseLabel(phase), bsSwap.why.c_str(),
				busy.empty() ? "" : ", trwa ", busy.c_str());
			bsSwapNew = nullptr;
			break;
		case zdcore::SWA_FAIL_REDRAW:
			ZLog("Miecze na plecach: zamiana - %s - dobycie dawnej broni", bsSwap.why.c_str());
			BsSendWeapon(true, BsDrawMode(BsPrimary()));
			bsSwapNew = nullptr;
			break;
		default:
			break;
		}
	}

	static zdcore::SecondKeyState bsKeyState;
	static bool bsInvWasOpen = false;

	static void BsKey() {
		int key = zcfg.secondSwordKey;
		bool console = zcon && zcon->IsVisible();
		bool invOpen = player->inventory2.IsOpen() != 0;
		BsHsiResolve();
		if (invOpen != bsInvWasOpen) {
			bsInvWasOpen = invOpen;
			if (invOpen && zcfg.debug)
				ZDebug("Drugi miecz: ekwipunek otwarty (%s) - petla dziala, zaznaczony %s", bsHsi.mod ? "Hedin Simple Inventory" :
					"ekwipunek gry", BsSelDesc(player->inventory2.GetSelectedItem()).c_str());
		}
		zdcore::SecondKeyInput in;
		if (key > 0) {
			in.engine = zinput && zinput->KeyPressed(key) && SwModsHeld(zcfg.secondSwordKeyMods);
			in.physical = BsPhysDown(key) && BsPhysModsHeld(zcfg.secondSwordKeyMods);
			in.focus = in.physical && BsGameFocus();
			in.typing = in.physical && (console || (invOpen && BsHsiTyping()));   // pisanie w konsoli / wyszukiwarce HSI
		}
		zdcore::SecondKeyEvent ev = zdcore::SecondKeyStep(bsKeyState, in);
		if (ev.releasedUnseen) {
			// dowod: klawisz trzymany ev.heldFrames klatek, a keyevent silnika (zinput->KeyPressed) caly czas 0
			static int unseenLogged = 0;
			if (unseenLogged++ < 3 || zcfg.debug)
				ZLog("Drugi miecz: klawisz %s trzymany %d klatek, silnik (zinput->KeyPressed) go nie widzial - %s; uzyty stan "
					"fizyczny (GetAsyncKeyState)", zdcore::SwapKeyLabel(key, zcfg.secondSwordKeyMods).c_str(), ev.heldFrames,
					invOpen ? (bsHsi.mod ? "przy otwartym Hedin Simple Inventory" : "przy otwartym ekwipunku gry")
					: "poza ekwipunkiem");
		}
		if (in.physical && in.typing && !console && !in.engine && !bsKeyState.held) {
			static unsigned typingFrame = 0;
			if (zdFrame - typingFrame > 30)
				ZDebug("Drugi miecz: klawisz %s pominiety - wyszukiwarka Hedin Simple Inventory ma klawiature",
					zdcore::SwapKeyLabel(key, zcfg.secondSwordKeyMods).c_str());
			typingFrame = zdFrame;
		}
		if (bsSwapPending && !console) {
			bsSwapPending = false;
			BsSwapRequest("konsola");
		}
		if (!ev.pressed)
			return;
		static bool firstLogged = false;
		if (!firstLogged || zcfg.debug) {
			firstLogged = true;
			ZLog("Drugi miecz: klawisz %s - zrodlo %s, zinput %d, fizycznie %d, fokus %d, ekwipunek %s (IsOpen %d), konsola %d, "
				"SecondSword %d", zdcore::SwapKeyLabel(key, zcfg.secondSwordKeyMods).c_str(), zdcore::SecondKeySourceLabel(ev.pressed),
				in.engine ? 1 : 0, in.physical ? 1 : 0, in.focus ? 1 : 0, bsHsi.mod ? "Hedin Simple Inventory" : "gry",
				invOpen ? 1 : 0, console ? 1 : 0, zcfg.secondSword ? 1 : 0);
		}
		if (!zcfg.secondSword || console)
			return;
		if (invOpen) {
			BsPickFromInventory(ev.pressed);
			return;
		}
		BsSwapRequest("klawisz");
	}

	// ---------------------------------------------------------------- stan i plan

	static zdcore::BackState BsReadState(zCModel* m) {
		zdcore::BackState st;
		oCItem* prim = BsPrimary();
		st.primaryType = BsMeleeType(prim);
		st.primaryBlade = prim && (prim->flags & ITM_FLAG_SWD) && ItemWeaponCut(prim, 0, false) != zdcore::WeaponCut::NoCut;
		st.primaryAtB = bsPrimaryAtB;
		st.secondValid = bsSecond && bsSecondVis && !(bsSecond->flags & ITM_FLAG_ACTIVE);
		int wm = player->GetWeaponMode();
		oCItem* ranged = wm == NPC_WEAPON_BOW || wm == NPC_WEAPON_CBOW ? player->GetWeapon() : nullptr;
		if (!ranged)
			ranged = player->GetEquippedRangedWeapon();
		st.bowEquipped = ranged && (ranged->flags & ITM_FLAG_BOW);
		st.cbowEquipped = ranged && (ranged->flags & ITM_FLAG_CROSSBOW);
		st.overlays = bsAvail && AtkModelProto(m, BS_OV[0]) && AtkModelProto(m, BS_OV[1]);
		st.dualPair = HeroDualState() != zdcore::DUAL_NONE;   // para NB: polozenie i dobycie natywne
		return st;
	}

	static void BsLogPlan(const zdcore::BackPlan& p, const zdcore::BackState& st) {
		std::string line = zdcore::BackPlanText(p);
		char buf[256];
		sprintf_s(buf, " | zalozona %s%s, drugi %s%s", bsLastPrimaryInst.empty() ? "brak" : bsLastPrimaryInst.c_str(),
			st.primaryBlade ? " (miecz)" : "", bsSecondInst.empty() ? "brak" : bsSecondInst.c_str(),
			bsSecondHash && !bsSecond ? " (nie w ekwipunku)" : "");
		line += buf;
		if (!st.overlays)
			line += bsAvail ? " | nakladki dobycia jeszcze nie na modelu" : " | brak paczki W3 (HUMANS_ZDF_DRAW_*) - natywnie";
		if (st.dualPair)
			line += " | bron parowa NB - polozenie i dobycie natywne (animacje 1x2 / 2x2)";
		if (line != bsPlanLogged) {
			bsPlanLogged = line;
			ZLog("Miecze na plecach: %s", line.c_str());
		}
	}

	void BackSwordsUpdate(float dt) {
		zCModel* m = player && player->homeWorld && HeroIsHuman() ? player->GetModel() : nullptr;
		if (!m) {
			bsOvN = 0;
			return;
		}
		if (!zcfg.backSwords) {
			BackSwordsRestore("BackSwords=0");
			return;
		}
		bsRestored = false;
		BsNodesFor(m);
		BsIds(m);
		BsResolveSecond();
		BsTrackPrimary();
		BsKey();
		BsSwapStep(dt);
		zdcore::BackState st = BsReadState(m);
		zdcore::BackPlan plan = zdcore::PlanBack(BsSettings(), st);
		bsPlan = plan;
		BsLogPlan(plan, st);
		BsEntriesUpdate(m, 0, plan.draw1H);
		BsEntriesUpdate(m, 1, plan.draw2H);
		if (bsScaleNow || zdFrame % 15 == 0) {
			bsScaleNow = false;
			BsScaleAll(m, false);
		}
		BsVisuals(m, plan);
		BsOverrides(m, plan);
		BsGripGuard(m, dt);
		BsDiag(m);
		if (bsVarsDirty)
			BsWriteVars("zmiana");
	}

	void BackSwordsRestore(const char* why) {
		if (bsRestored)
			return;
		bsRestored = true;
		zCModel* m = player && player->homeWorld && HeroIsHuman() ? player->GetModel() : nullptr;
		for (int t = 0; t < 2; t++)
			BsRestoreSlot(m, t, why);
		if (m)
			BsScaleAll(m, true);
		bsScaleLogged.clear();
		if (m && m == bsN.model) {
			zdcore::BackPlan none;
			BsVisuals(m, none);
			BsOverrides(m, none);
		}
		bsOvN = 0;
		bsSwap = zdcore::BackSwapRun();
		bsSwapNew = nullptr;
		bsKeyState = zdcore::SecondKeyState();
		bsPlanLogged.clear();
		ZLog("Miecze na plecach: wylaczone (%s) - dobycie, tempo i wezly broni jak w grze", why);
	}

	// ---------------------------------------------------------------- nakladki dla planu Styles

	std::vector<std::string> BackSwordsOverlays() {
		std::vector<std::string> out;
		if (!zcfg.backSwords || !bsAvail)
			return out;
		for (const char* o : BS_OV)
			if (std::find(bsFailed.begin(), bsFailed.end(), o) == bsFailed.end())
				out.push_back(o);
		return out;
	}

	std::vector<std::string> BackSwordsOverlayNames() {
		std::vector<std::string> out;
		if (bsAvail)
			for (const char* o : BS_OV)
				out.push_back(o);
		return out;
	}

	bool BackSwordsIsOurOverlay(const std::string& name) {
		return name.rfind("HUMANS_ZDF_DRAW_", 0) == 0;
	}

	void BackSwordsOverlayFailed(const std::string& name) {
		if (!BackSwordsIsOurOverlay(name) || std::find(bsFailed.begin(), bsFailed.end(), name) != bsFailed.end())
			return;
		bsFailed.push_back(name);
		ZLog("Miecze na plecach: nie podpieto %s - dobycie zza plecow wylaczone do zdism reload", name.c_str());
		bsAvail = false;
	}

	// ---------------------------------------------------------------- wczytanie

	// Kolizje klawisza drugiego miecza z [KEYS] i innymi klawiszami pluginu (log).
	static void BsWarnKey() {
		int key = zcfg.secondSwordKey;
		if (key <= 0)
			return;
		static const char* keys[] = { "keyEnd", "keyHeal", "keyPotion", "keyLockTarget", "keyParade", "keyActionRight",
			"keyActionLeft", "keyUp", "keyDown", "keyLeft", "keyRight", "keyStrafeLeft", "keyStrafeRight", "keyAction",
			"keySlow", "keySMove", "keyWeapon", "keySneak", "keyLook", "keyLookFP", "keyInventory", "keyShowStatus",
			"keyShowLog", "keyShowMap", "keyTorch", "keyIntSprint" };
		for (const char* k : keys)
			for (int code : IniKeys(k))
				if (code == key)
					ZLog("Miecze na plecach: UWAGA klawisz %s (SecondSwordKey=%d) jest tez w [KEYS] %s - zmien SecondSwordKey",
						zdcore::KeyLabel(key).c_str(), key, k);
		struct { const char* name; int code; int mods; } ours[] = { { "DodgeKey", zcfg.dodgeKey, 0 }, { "RollKey", zcfg.rollKey, 0 },
			{ "StyleKey", zcfg.styleKey, 0 }, { "LocoKey", zcfg.locoKey, 0 }, { "MeditateKey", zcfg.meditateKey, 0 },
			{ "PathMenuKey", zcfg.pathMenuKey, zcfg.pathMenuKeyMods }, { "WeaponSwapKey", zcfg.swapKey, zcfg.swapMods } };
		for (auto& o : ours)
			if (o.code == key && o.mods == zcfg.secondSwordKeyMods)
				ZLog("Miecze na plecach: UWAGA SecondSwordKey=%d to tez %s", key, o.name);
	}

	void BackSwordsLoad() {
		bsFailed.clear();
		bsPosOk = false;
		std::string text;
		static const char* names[] = { "\\SYSTEM\\ZDISMEMBER\\BACKSWORDS_W3.INI", "SYSTEM\\ZDISMEMBER\\BACKSWORDS_W3.INI" };
		std::string err = "brak pliku System\\zDismember\\backswords_w3.ini";
		if (ZReadGameFile(names, 2, text))
			bsPosOk = zdcore::ParseBackIni(text, bsPos[zdcore::SPOT_A], bsPos[zdcore::SPOT_B], err);
		bool ovl = ZOverlayExists(BS_OV[0], BS_ANIS[0][0]) && ZOverlayExists(BS_OV[1], BS_ANIS[1][0]);
		bsAvail = bsPosOk && ovl;
		if (!bsAvail)
			ZLog("Miecze na plecach: niedostepne - %s%s (prywatna paczka zDismember_W3.vdf z make_draw.py); dobycie i bron jak w grze",
				bsPosOk ? "" : err.c_str(), ovl ? "" : (bsPosOk ? "brak nakladek HUMANS_ZDF_DRAW_1H/2H" : ", brak nakladek HUMANS_ZDF_DRAW_1H/2H"));
		else
			ZLog("Miecze na plecach: BackSwords=%d Back1H=%d (%s), tempo dobycia %d%% / chowania %d%% / przy pasie %d%%, drugi miecz %d "
				"(klawisz %s%s, przy luku %s), pozycje A/B %.1f cm od siebie, przesuniecie A %g %g %g / %g %g %g, B %g %g %g / %g %g %g",
				(int)zcfg.backSwords, zcfg.back1H, zcfg.back1H == 0 ? "1H przy pasie" : zcfg.back1H == 1 ? "miecze na plecach" :
				"kazda bron 1H na plecach", zcfg.backDrawSpeed, zcfg.backSheatheSpeed, zcfg.beltDrawSpeed, (int)zcfg.secondSword,
				zdcore::KeyLabel(zcfg.secondSwordKey).c_str(), zcfg.secondSwordKeyMods ? " z modyfikatorem" : "",
				zcfg.secondWithRanged ? "zamiast luku" : "ukryty", zdcore::BackMatDistance(bsPos[1], bsPos[2]),
				zcfg.backAdjust[0][0], zcfg.backAdjust[0][1], zcfg.backAdjust[0][2], zcfg.backAdjust[0][3], zcfg.backAdjust[0][4],
				zcfg.backAdjust[0][5], zcfg.backAdjust[1][0], zcfg.backAdjust[1][1], zcfg.backAdjust[1][2], zcfg.backAdjust[1][3],
				zcfg.backAdjust[1][4], zcfg.backAdjust[1][5]);
		BsWarnKey();
		bsScaleLogged.clear();
		bsPlanLogged.clear();
		bsScaleNow = true;
	}

	void BackSwordsBeforeSave() {
		BsWriteVars("zapis");
	}

	void BackSwordsLoadBegin() {
		bsDiagModel = nullptr;
		bsSwap = zdcore::BackSwapRun();
		bsSwapNew = nullptr;
		bsSwapPending = false;
		bsSecond = nullptr;
		BsSetSecondVisual(nullptr);
		bsLastPrimary = nullptr;
		bsLastPrimaryInst.clear();
		bsN = BsNodes();
		bsOvN = 0;
		bsOvModel = nullptr;
		for (int& n : bsOvNodesSet)
			n = -1;
		bsBowShown = bsCbowHidden = false;
		bsGripAni = nullptr;
		bsPlanLogged.clear();
	}

	void BackSwordsLoadEnd() {
		BsReadVars();
		bsResolveFrame = 0;
		bsSecondMissingLogged = false;
		ZLog("Miecze na plecach: stan z zapisu - drugi miecz %s, bron w %s", bsSecondHash ? ("skrot " + std::to_string(bsSecondHash)).c_str() :
			"brak", bsPrimaryAtB ? "B" : "A");
		zCModel* m = player && HeroIsHuman() ? player->GetModel() : nullptr;
		if (m && bsAvail && zcfg.backSwords)
			for (int k = 0; k < 3; k++)
				BsLoadedProto(m, k);   // wariant B / BELT zawczasu (bez wczytywania z dysku przy pierwszej zamianie)
	}

	bool BackSwordsSwapping() {
		return bsSwap.phase != zdcore::SWP_IDLE;
	}

	// ---------------------------------------------------------------- konsola

	static oCItem* BsFindInInventory(const std::string& inst) {
		for (auto node = player->inventory2.inventory.next; node; node = node->next) {
			oCItem* it = node->GetData();
			if (it && !(it->flags & ITM_FLAG_ACTIVE) && BsMeleeType(it) != zdcore::BACK_TYPE_NONE &&
				zdcore::Upper(BsInst(it)) == zdcore::Upper(inst))
				return it;
		}
		return nullptr;
	}

	static oCItem* BsBestOther() {
		oCItem* best = nullptr;
		for (auto node = player->inventory2.inventory.next; node; node = node->next) {
			oCItem* it = node->GetData();
			if (it && !(it->flags & ITM_FLAG_ACTIVE) && BsMeleeType(it) != zdcore::BACK_TYPE_NONE &&
				(!best || it->damageTotal > best->damageTotal))
				best = it;
		}
		return best;
	}

	static std::string BsInfo() {
		zCModel* m = player ? player->GetModel() : nullptr;
		std::string s = bsAvail ? "" : "NIEDOSTEPNE (brak paczki W3 / pozycji) | ";
		s += zdcore::BackPlanText(bsPlan);
		char buf[512];
		sprintf_s(buf, " | zalozona %s, drugi %s (%s), w B: %s | zamiana: %s | tempo %d/%d/%d%% | wpisy 1H %s, 2H %s | "
			"nakladki na modelu: %s", bsLastPrimaryInst.empty() ? "brak" : bsLastPrimaryInst.c_str(),
			bsSecondInst.empty() ? "brak" : bsSecondInst.c_str(), bsSecond ? "w ekwipunku" : bsSecondHash ? "nie ma w ekwipunku" : "-",
			bsPrimaryAtB ? "zalozona" : "drugi", zdcore::BackSwapPhaseLabel(bsSwap.phase), zcfg.backDrawSpeed, zcfg.backSheatheSpeed,
			zcfg.beltDrawSpeed, bsSlot[0].variant >= 0 ? zdcore::DrawVariantLabel(bsSlot[0].variant) : "-",
			bsSlot[1].variant >= 0 ? zdcore::DrawVariantLabel(bsSlot[1].variant) : "-",
			m && AtkModelProto(m, BS_OV[0]) && AtkModelProto(m, BS_OV[1]) ? "tak" : "nie");
		s += buf;
		return s;
	}

	std::string BackSwordsConsole(const std::string& args) {
		std::vector<std::string> a = zdcore::SplitArgs(args);
		std::string cmd = a.empty() ? "INFO" : a[0];
		if (!player)
			return "brak bohatera";
		std::string out;
		if (cmd == "INFO")
			out = BsInfo();
		else if (cmd == "SECOND") {
			std::string arg = a.size() > 1 ? a[1] : "";
			if (arg.empty())
				out = "drugi miecz: " + (bsSecondInst.empty() ? std::string("brak") : bsSecondInst + " (" + bsSecondName + ")") +
				" - zdism back second <INSTANCJA|clear|auto>";
			else if (arg == "CLEAR") {
				BsSetSecond(nullptr, "konsola");
				out = "drugi miecz: brak";
			}
			else {
				oCItem* it = arg == "AUTO" ? BsBestOther() : BsFindInInventory(arg);
				if (!it)
					out = arg == "AUTO" ? "brak innej broni bialej w ekwipunku" : "brak " + arg + " (niezalozonej broni bialej) w ekwipunku";
				else {
					BsSetSecond(it, arg == "AUTO" ? "konsola auto" : "konsola");
					out = "drugi miecz: " + bsSecondInst + " (" + bsSecondName + ")";
				}
			}
		}
		else if (cmd == "SWAP") {
			bsSwapPending = true;
			out = "zamiana po zamknieciu konsoli";
		}
		else if (cmd == "ADJUST" && a.size() >= 2 && (a[1] == "A" || a[1] == "B")) {
			int i = a[1] == "B" ? 1 : 0;
			if (a.size() == 2)
				out = "BackAdjust" + a[1] + " = " + std::to_string(zcfg.backAdjust[i][0]) + " " + std::to_string(zcfg.backAdjust[i][1]) +
				" " + std::to_string(zcfg.backAdjust[i][2]) + " " + std::to_string(zcfg.backAdjust[i][3]) + " " +
				std::to_string(zcfg.backAdjust[i][4]) + " " + std::to_string(zcfg.backAdjust[i][5]);
			else {
				std::string rest;
				for (size_t k = 2; k < a.size(); k++)
					rest += (rest.empty() ? "" : " ") + a[k];
				float v[6] = { 0, 0, 0, 0, 0, 0 };
				std::vector<std::string> parts = zdcore::SplitArgs(rest);
				bool ok = parts.size() >= 3 && parts.size() <= 6;
				for (size_t k = 0; ok && k < parts.size(); k++) {
					char* end = nullptr;
					v[k] = strtof(parts[k].c_str(), &end);
					ok = end && *end == 0;
				}
				if (!ok)
					out = "zdism back adjust a|b x y z [rx ry rz] (cm, st. w ukladzie miecza)";
				else {
					for (int k = 0; k < 6; k++)
						zcfg.backAdjust[i][k] = v[k];
					char buf[160];
					sprintf_s(buf, "%g %g %g %g %g %g", v[0], v[1], v[2], v[3], v[4], v[5]);
					zoptions->WriteString("ZDISMEMBER", i ? "BackAdjustB" : "BackAdjustA", buf, FALSE);
					out = std::string("BackAdjust") + (i ? "B" : "A") + " = " + buf + " (zapisane w Gothic.ini; przelozenie w dobyciu "
						"rozjedzie sie o tyle - na stale: make_draw.py --depth)";
				}
			}
		}
		else
			out = "zdism back [info|second [INSTANCJA|clear|auto]|swap|adjust a|b x y z [rx ry rz]]";
		ZLog("zdism back: %s", out.c_str());
		return out;
	}
}
