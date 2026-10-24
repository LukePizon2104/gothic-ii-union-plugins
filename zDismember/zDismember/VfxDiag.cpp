// Supported with union (c) 2020 Union team
// Union SOURCE file

// Diagnostyka i bezpiecznik cyklu zycia efektow wizualnych (oCVisualFX), tylko Gothic II NoTR.
//
// Awaria NB z 23.10 (03:03 i 19:07, ten sam stos): Gothic2.exe 0x48B290 = oCVisualFX::DisposeHierachy+0x1E0 - niszczony
// efekt (nie dziecko, parent != this) szuka siebie w parent->fxList, a parent wskazuje na zwolniona i ponownie zajeta
// pamiec (w rejestrach liczby float zamiast tablicy i licznika). Ostatnia referencje efektu oddaje oCNpc::ClearVobList
// (Release wpisu listy vobow bohatera, ret 0x75D821) wolane z NB (UNION_ABI.DLL: co 500 ms ClearVobList + CreateVobList
// 750/1500 cm wokol bohatera - wykrywanie walki w poblizu).
//
// Mechanizm (statycznie, Gothic2.exe):
// - parent != this ustawia tylko oCVisualFX::CreateHierachy (0x48ADF0): efekty z emFXCreate_S trafiaja do fxList korzenia
//   (root = parent = korzen) albo, gdy isChild, do childList rodzica.
// - oCVisualFX::DisposeHierachy korzenia (root == this) zeruje root/parent kazdego efektu z fxList - w porzadku.
// - oCVisualFX::Kill (0x493F70) zabija i zwalnia efekty z childList/fxList i czysci obie tablice, ale NIE zeruje ich
//   root/parent. Efekt z listy, ktory przezyje Kill (ma referencje z zewnatrz - np. z listy vobow NPC zebranej
//   CreateVobList przed chwila), zostaje z wiszacym wskaznikiem na korzen. Korzen zwalnia sie chwile pozniej, a gdy
//   ClearVobList odda ostatnia referencje dziecka, DisposeHierachy dziecka czyta parent->fxList ze zwolnionej pamieci.
// - Kill wolaja: ~oCSpell / oCSpell::Kill, oCItem::RemoveEffect / ThisVobRemovedFromWorld / ~oCItem (efekt przedmiotu),
//   oCNpc::RemoveEffect / EV_StopFX (AI_StopFX w dialogach) / Disable / Shrink / CleanUp / ~oCNpc (efekty postaci).
//
// Co robi plik ([ZDISMEMBER] VfxDiag=1 log, VfxKillFix=1 bezpiecznik; oba domyslnie 1 na czas diagnozy):
// - hak Kill: przed oryginalem zeruje root/parent efektow z fxList/childList, ktore wskazuja na zabijany efekt (dokladnie
//   to, co robi DisposeHierachy korzenia) - wiszacy wskaznik nie powstaje. Log, gdy jakis efekt z listy ma referencje
//   z zewnatrz (przezyje Kill): nazwa, licznik, czy w swiecie, kto wola Kill (stos).
// - hak DisposeHierachy (wolany tylko z ~oCVisualFX): sprawdza rodzica - zwolniony (zniszczony po ostatnim
//   CreateHierachy) albo nieczytelny / bez znanej tablicy wirtualnej = wiszacy. Wtedy wpis w logu z pierscieniem 64
//   ostatnich zdarzen efektow i (VfxKillFix=1) zerowanie parent/root przed oryginalem - bez odpinania od nieistniejacego
//   rodzica, zamiast awarii. Efekty zwalniane z listy vobow NPC (ClearVobList na stosie) z rodzicem - linia w logu.
// - hak CreateHierachy: rodzic i korzen zywe (znana tablica wirtualna, zdjete z listy zniszczonych).
// - haki CreateAndPlay (obie wersje): tworzenie efektu do pierscienia; tworzone przez inne moduly niz Gothic2.exe
//   i UNION_ABI.DLL (nasze pluginy) - tez linia w logu (nazwa, wskaznik, wolajacy).
// Kazda linia diagnostyki od razu na dysk (ZLogFlush), zeby ostatnie wpisy przed ewentualna awaria byly w pliku.

namespace GOTHIC_ENGINE {
#if ENGINE == Engine_G2A
	// ---------- pola (G2A, oVisFx.h / zObject.h / zVob.h) ----------
	static const int VD_OFS_REFCTR   = 0x004;  // zCObject::refCtr
	static const int VD_OFS_OBJNAME  = 0x010;  // zCObject::objectName (zSTRING)
	static const int VD_OFS_HOMEWLD  = 0x0B8;  // zCVob::homeWorld
	static const int VD_OFS_STATE    = 0x3B8;  // oCVisualFX::fxState
	static const int VD_OFS_ROOT     = 0x3BC;  // oCVisualFX::root
	static const int VD_OFS_PARENT   = 0x3C0;  // oCVisualFX::parent
	static const int VD_OFS_FXLIST   = 0x3D0;  // zCArray<oCVisualFX*> fxList (array, numAlloc, numInArray)
	static const int VD_OFS_CHILDREN = 0x3DC;  // zCArray<oCVisualFX*> childList
	static const int VD_OFS_ORIGIN   = 0x4A8;  // oCVisualFX::origin
	static const int VD_OFS_FXNAME   = 0x4C4;  // oCVisualFX::fxName (zSTRING, instancja skryptu)
	static const int VD_OFS_FLAGS    = 0x55C;  // pola bitowe: 4 = isChild, 8 = isDeleted, 0x20 = shouldDelete
	static const DWORD VD_FLAG_ISCHILD = 4;
	static const DWORD VD_RET_CLEARVOBLIST = 0x0075D821;   // oCNpc::ClearVobList - po Release wpisu listy vobow
	static const DWORD VD_VT_VISUALFX = 0x0083029C;        // tablica wirtualna oCVisualFX

	// ---------- ustawienia ----------
	static bool vdLoaded = false;
	static bool vdDiag = true;   // VfxDiag
	static bool vdFix = true;    // VfxKillFix

	static void VdLoad() {
		if (vdLoaded || !zoptions)
			return;
		vdLoaded = true;
		vdDiag = ReadIntDef("VfxDiag", 1, 0, 1) != 0;
		vdFix = ReadIntDef("VfxKillFix", 1, 0, 1) != 0;
		ZLog("VFX: diagnostyka efektow %s, bezpiecznik Kill/DisposeHierachy %s (VfxDiag=%d, VfxKillFix=%d)",
			vdDiag ? "wlaczona" : "wylaczona", vdFix ? "wlaczony" : "wylaczony", vdDiag ? 1 : 0, vdFix ? 1 : 0);
	}

	// ---------- odczyt pamieci bez wywrotki (SEH, bez obiektow C++ w funkcjach z __try) ----------
	static bool VdRd(const void* p, DWORD& v) {
		__try {
			v = *(const DWORD*)p;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			v = 0;
			return false;
		}
	}

	static DWORD VdGet(const BYTE* obj, int ofs) {
		DWORD v = 0;
		VdRd(obj + ofs, v);
		return v;
	}

	static bool VdWr(BYTE* obj, int ofs, DWORD v) {
		__try {
			*(DWORD*)(obj + ofs) = v;
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	// zSTRING (vptr, allocater, vector, length, reserved): tekst spod +8, dlugosc spod +0xC.
	static void VdStr(const BYTE* str, char* out, int n) {
		out[0] = 0;
		__try {
			const char* s = *(const char* const*)(str + 8);
			int len = *(const int*)(str + 0xC);
			if (!s || len <= 0)
				return;
			if (len > n - 1)
				len = n - 1;
			for (int i = 0; i < len; i++) {
				char c = s[i];
				out[i] = c >= 32 && c < 127 ? c : '?';
			}
			out[len] = 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			lstrcpynA(out, "<nieczytelna>", n);
		}
	}

	// Nazwa efektu: instancja skryptu (fxName), a bez niej nazwa voba.
	static void VdName(const BYTE* fx, char* out, int n) {
		VdStr(fx + VD_OFS_FXNAME, out, n);
		if (!out[0])
			VdStr(fx + VD_OFS_OBJNAME, out, n);
		if (!out[0])
			lstrcpynA(out, "?", n);
	}

	// ---------- moduly i stos ----------
	struct VdMod {
		DWORD base, end;
		char name[32];
	};
	static VdMod vdMods[128];
	static int vdModCount = 0;
	static DWORD vdModsTick = 0;

	static void VdModsRefresh() {
		vdModsTick = GetTickCount();
		vdModCount = 0;
		HMODULE mods[128];
		DWORD need = 0;
		if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need))
			return;
		int n = (int)(need / sizeof(HMODULE));
		if (n > 128)
			n = 128;
		for (int i = 0; i < n; i++) {
			MODULEINFO mi;
			if (!K32GetModuleInformation(GetCurrentProcess(), mods[i], &mi, sizeof(mi)))
				continue;
			VdMod& m = vdMods[vdModCount++];
			m.base = (DWORD)mi.lpBaseOfDll;
			m.end = m.base + mi.SizeOfImage;
			if (!K32GetModuleBaseNameA(GetCurrentProcess(), mods[i], m.name, sizeof(m.name)))
				lstrcpynA(m.name, "?", sizeof(m.name));
		}
	}

	static const VdMod* VdModOf(DWORD a) {
		if (!vdModCount || GetTickCount() - vdModsTick > 60000)
			VdModsRefresh();
		for (int i = 0; i < vdModCount; i++)
			if (a >= vdMods[i].base && a < vdMods[i].end)
				return &vdMods[i];
		return nullptr;
	}

	// Adres powrotu: tuz za call rel32 (E8), call [rej+disp32] / call [mem] (FF 9x / FF 15), call [rej+disp8] (FF 5x),
	// call rej / call [rej] (FF Dx / FF 1x).
	static bool VdIsRet(DWORD v) {
		__try {
			const BYTE* c = (const BYTE*)v;
			if (c[-5] == 0xE8)
				return true;
			if (c[-6] == 0xFF && ((c[-5] & 0xF8) == 0x90 || c[-5] == 0x15))
				return true;
			if (c[-3] == 0xFF && (c[-2] & 0xF8) == 0x50)
				return true;
			if (c[-2] == 0xFF && ((c[-1] & 0xF8) == 0xD0 || (c[-1] & 0xF8) == 0x10))
				return true;
			return false;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	// Do max adresow powrotu od slotu powrotu haka (pierwszy = bezposredni wolajacy), przeglad words slow stosu.
	static int VdStack(const DWORD* from, DWORD* out, int max, int words) {
		int n = 0;
		for (int i = 0; i < words && n < max; i++) {
			DWORD v = 0;
			if (!VdRd(from + i, v))
				break;
			if (v < 0x00401000 || !VdModOf(v) || !VdIsRet(v))
				continue;
			out[n++] = v;
		}
		return n;
	}

	static bool VdStackHas(const DWORD* from, DWORD what, int words) {
		for (int i = 0; i < words; i++) {
			DWORD v = 0;
			if (!VdRd(from + i, v))
				return false;
			if (v == what)
				return true;
		}
		return false;
	}

	static void VdAddr(DWORD a, char* out, int n) {
		const VdMod* m = a ? VdModOf(a) : nullptr;
		if (m)
			_snprintf_s(out, n, _TRUNCATE, "%s+0x%X", m->name, a - m->base);
		else
			_snprintf_s(out, n, _TRUNCATE, "%08X", a);
	}

	static void VdStackText(const DWORD* ret, int n, char* out, int size) {
		out[0] = 0;
		for (int i = 0; i < n; i++) {
			char a[64];
			VdAddr(ret[i], a, sizeof(a));
			size_t len = strlen(out);
			_snprintf_s(out + len, size - len, _TRUNCATE, "%s%s", i ? " < " : "", a);
		}
		if (!out[0])
			lstrcpynA(out, "-", size);
	}

	// ---------- znane tablice wirtualne (zywe efekty) i lista zniszczonych ----------
	static DWORD vdVt[32] = { VD_VT_VISUALFX };
	static int vdVtCount = 1;

	static void VdLearnVt(const BYTE* fx) {
		DWORD vt = 0;
		if (!fx || !VdRd(fx, vt) || !vt)
			return;
		for (int i = 0; i < vdVtCount; i++)
			if (vdVt[i] == vt)
				return;
		if (vdVtCount < 32)
			vdVt[vdVtCount++] = vt;
	}

	static bool VdKnownVt(const BYTE* fx) {
		DWORD vt = 0;
		if (!VdRd(fx, vt))
			return false;
		for (int i = 0; i < vdVtCount; i++)
			if (vdVt[i] == vt)
				return true;
		return false;
	}

	struct VdDead {
		const BYTE* fx;
		DWORD tick;
		DWORD caller;
		char name[28];
	};
	static VdDead vdDead[1024];
	static unsigned vdDeadPos = 0;

	static const VdDead* VdFindDead(const BYTE* fx) {
		if (!fx)
			return nullptr;
		for (int i = 0; i < 1024; i++)
			if (vdDead[i].fx == fx)
				return &vdDead[i];
		return nullptr;
	}

	static void VdForgetDead(const BYTE* fx) {
		if (!fx)
			return;
		for (int i = 0; i < 1024; i++)
			if (vdDead[i].fx == fx)
				vdDead[i].fx = nullptr;
	}

	static void VdMarkDead(const BYTE* fx, DWORD caller) {
		VdForgetDead(fx);
		VdDead& d = vdDead[vdDeadPos++ % 1024];
		d.fx = fx;
		d.tick = GetTickCount();
		d.caller = caller;
		VdName(fx, d.name, sizeof(d.name));
	}

	// ---------- pierscien ostatnich zdarzen ----------
	struct VdEv {
		DWORD tick;
		char kind;      // C utworzony, H CreateHierachy, K Kill, D niszczony (DisposeHierachy)
		const BYTE* fx;
		DWORD parent, root;
		int ref, members, survivors;
		DWORD ret[3];
		char name[32];
	};
	static VdEv vdRing[64];
	static unsigned vdRingPos = 0;

	static void VdRingAdd(char kind, const BYTE* fx, const DWORD* ret, int nret, int members = 0, int survivors = 0) {
		VdEv& e = vdRing[vdRingPos++ % 64];
		e.tick = GetTickCount();
		e.kind = kind;
		e.fx = fx;
		e.parent = VdGet(fx, VD_OFS_PARENT);
		e.root = VdGet(fx, VD_OFS_ROOT);
		e.ref = (int)VdGet(fx, VD_OFS_REFCTR);
		e.members = members;
		e.survivors = survivors;
		for (int i = 0; i < 3; i++)
			e.ret[i] = i < nret ? ret[i] : 0;
		VdName(fx, e.name, sizeof(e.name));
	}

	static void VdRingDump(const char* why) {
		ZLog("VFX: ostatnie zdarzenia efektow (%s), od najstarszego:", why);
		DWORD now = GetTickCount();
		for (unsigned i = 0; i < 64; i++) {
			const VdEv& e = vdRing[(vdRingPos + i) % 64];
			if (!e.kind)
				continue;
			char st[200];
			VdStackText(e.ret, e.ret[2] ? 3 : e.ret[1] ? 2 : e.ret[0] ? 1 : 0, st, sizeof(st));
			ZLog("VFX:   -%5.2f s %c %p '%s' ref %d rodzic %08X korzen %08X%s | %s", (now - e.tick) / 1000.0f, e.kind, e.fx,
				e.name, e.ref, e.parent, e.root,
				e.kind == 'K' ? (e.survivors ? " | lista: przezyja Kill" : "") : "", st);
		}
		ZLogFlush();
	}

	// Efekty listy (fxList / childList) efektu fx: wskazniki pod list (array) i liczba (numInArray).
	static int VdList(const BYTE* fx, int ofs, const BYTE** out, int max) {
		DWORD arr = VdGet(fx, ofs), num = VdGet(fx, ofs + 8);
		if (!arr || (int)num <= 0)
			return 0;
		int n = (int)num < max ? (int)num : max;
		for (int i = 0; i < n; i++) {
			DWORD p = 0;
			VdRd((const BYTE*)arr + 4 * i, p);
			out[i] = (const BYTE*)p;
		}
		return n;
	}

	// ---------- Kill ----------
	// Przed oryginalem: efekty z fxList / childList z root / parent == fx dostaja 0 (jak w DisposeHierachy korzenia).
	// Kill zwalnia je i czysci tablice - bez tego efekt z referencja z zewnatrz zostaje z wiszacym rodzicem.
	static void VdBeforeKill(BYTE* fx, const DWORD* retSlot) {
		VdLearnVt(fx);
		VdForgetDead(fx);
		const BYTE* m[64];
		int nfx = VdList(fx, VD_OFS_FXLIST, m, 32);
		int nch = VdList(fx, VD_OFS_CHILDREN, m + nfx, 32);
		int total = nfx + nch, survivors = 0, cleared = 0;
		char surv[600];
		surv[0] = 0;
		for (int i = 0; i < total; i++) {
			BYTE* c = (BYTE*)m[i];
			if (!c || c == fx)
				continue;
			int ref = (int)VdGet(c, VD_OFS_REFCTR);
			bool inWorld = VdGet(c, VD_OFS_HOMEWLD) != 0;
			int expect = 1 + (inWorld ? 1 : 0);   // tablica listy + swiat
			if (ref > expect) {
				survivors++;
				char nm[40];
				VdName(c, nm, sizeof(nm));
				size_t len = strlen(surv);
				_snprintf_s(surv + len, sizeof(surv) - len, _TRUNCATE, "%s%p '%s' ref %d%s", len ? ", " : "", c, nm, ref,
					inWorld ? " w swiecie" : " poza swiatem");
			}
			if (vdFix) {
				if (VdGet(c, VD_OFS_ROOT) == (DWORD)fx && VdWr(c, VD_OFS_ROOT, 0))
					cleared++;
				if (VdGet(c, VD_OFS_PARENT) == (DWORD)fx && VdWr(c, VD_OFS_PARENT, 0))
					cleared++;
			}
		}
		DWORD ret[6];
		int nret = VdStack(retSlot, ret, survivors && vdDiag ? 6 : 3, 96);
		if (vdDiag)
			VdRingAdd('K', fx, ret, nret, total, survivors);
		if (survivors && vdDiag) {
			char nm[48], st[400];
			VdName(fx, nm, sizeof(nm));
			VdStackText(ret, nret, st, sizeof(st));
			ZLog("VFX: Kill %p '%s' (ref %d, origin %08X): %d efekt(ow) listy, %d z referencja z zewnatrz: %s; %s; wola %s",
				fx, nm, (int)VdGet(fx, VD_OFS_REFCTR), VdGet(fx, VD_OFS_ORIGIN), total, survivors, surv,
				vdFix ? "rodzic/korzen wyzerowane (bezpiecznik)" : "BEZ bezpiecznika - zostana z wiszacym rodzicem", st);
			ZLogFlush();
		}
	}

	static void __fastcall Hk_oCVisualFX_Kill(oCVisualFX* self, void* edx);
	static CInvoke<void(__fastcall*)(oCVisualFX*, void*)> Ivk_oCVisualFX_Kill(0x00493F70, &Hk_oCVisualFX_Kill,
		IvkEnabled(ENGINE));
	static void __fastcall Hk_oCVisualFX_Kill(oCVisualFX* self, void* edx) {
		VdLoad();
		if (self && (vdDiag || vdFix))
			VdBeforeKill((BYTE*)self, (const DWORD*)_AddressOfReturnAddress());
		Ivk_oCVisualFX_Kill(self, edx);
	}

	// ---------- DisposeHierachy (tylko z ~oCVisualFX) ----------
	static int vdBadLogs = 0, vdVobListLogs = 0;

	static void VdBeforeDispose(BYTE* fx, const DWORD* retSlot) {
		DWORD parent = VdGet(fx, VD_OFS_PARENT), root = VdGet(fx, VD_OFS_ROOT), flags = VdGet(fx, VD_OFS_FLAGS);
		const BYTE* p = (const BYTE*)parent;
		bool hasParent = p && p != fx;
		const VdDead* dead = hasParent ? VdFindDead(p) : nullptr;
		bool bad = hasParent && (dead || !VdKnownVt(p));
		bool fromVobList = hasParent && VdStackHas(retSlot, VD_RET_CLEARVOBLIST, 160);
		// dzieci z wiszacym wskaznikiem (DisposeHierachy pisze do kazdego) - tylko log
		const BYTE* kids[32];
		int nk = VdList(fx, VD_OFS_CHILDREN, kids, 32), badKids = 0;
		for (int i = 0; i < nk; i++)
			if (kids[i] && kids[i] != fx && (VdFindDead(kids[i]) || !VdKnownVt(kids[i])))
				badKids++;
		DWORD ret[6];
		int nret = VdStack(retSlot, ret, bad || badKids || fromVobList ? 6 : 3, 96);
		if (vdDiag)
			VdRingAdd('D', fx, ret, nret);
		if ((bad || badKids) && vdBadLogs < 40) {
			vdBadLogs++;
			char nm[48], st[400], dn[64];
			VdName(fx, nm, sizeof(nm));
			VdStackText(ret, nret, st, sizeof(st));
			if (dead) {
				char dc[64];
				VdAddr(dead->caller, dc, sizeof(dc));
				_snprintf_s(dn, sizeof(dn), _TRUNCATE, "'%s' zniszczony %.2f s temu (%s)", dead->name,
					(GetTickCount() - dead->tick) / 1000.0f, dc);
			}
			else
				lstrcpynA(dn, "nieczytelny / obca tablica wirtualna", sizeof(dn));
			ZLog("VFX: NIESZCZONY EFEKT Z WISZACYM %s: %p '%s' ref %d, isChild %d, stan %d, rodzic %08X %s, korzen %08X,"
				" zle dzieci %d/%d%s; wola %s", bad ? "RODZICEM" : "DZIECKIEM", fx, nm, (int)VdGet(fx, VD_OFS_REFCTR),
				(flags & VD_FLAG_ISCHILD) ? 1 : 0, (int)VdGet(fx, VD_OFS_STATE), parent, bad ? dn : "(rodzic OK)", root,
				badKids, nk, fromVobList ? ", z listy vobow NPC (ClearVobList)" : "", st);
			VdRingDump("wiszacy wskaznik");
		}
		else if (fromVobList && vdDiag && vdVobListLogs < 200) {
			vdVobListLogs++;
			char nm[48], st[400];
			VdName(fx, nm, sizeof(nm));
			VdStackText(ret, nret, st, sizeof(st));
			ZLog("VFX: efekt potomny zwolniony z listy vobow NPC: %p '%s' rodzic %08X (zywy), korzen %08X; wola %s", fx, nm,
				parent, root, st);
			ZLogFlush();
		}
		if (bad && vdFix) {
			VdWr(fx, VD_OFS_PARENT, 0);
			if (root == parent)
				VdWr(fx, VD_OFS_ROOT, 0);
			ZLog("VFX: bezpiecznik - %p bez odpinania od nieistniejacego rodzica %08X (zamiast awarii w DisposeHierachy)", fx,
				parent);
			ZLogFlush();
		}
		VdMarkDead(fx, nret ? ret[0] : 0);
	}

	static void __fastcall Hk_oCVisualFX_DisposeHierachy(oCVisualFX* self, void* edx);
	static CInvoke<void(__fastcall*)(oCVisualFX*, void*)> Ivk_oCVisualFX_DisposeHierachy(0x0048B0B0,
		&Hk_oCVisualFX_DisposeHierachy, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCVisualFX_DisposeHierachy(oCVisualFX* self, void* edx) {
		VdLoad();
		if (self && (vdDiag || vdFix))
			VdBeforeDispose((BYTE*)self, (const DWORD*)_AddressOfReturnAddress());
		Ivk_oCVisualFX_DisposeHierachy(self, edx);
	}

	// ---------- CreateHierachy: tu powstaja powiazania rodzic - dziecko (tworca i jego korzen zyja) ----------
	static void __fastcall Hk_oCVisualFX_CreateHierachy(oCVisualFX* self, void* edx);
	static CInvoke<void(__fastcall*)(oCVisualFX*, void*)> Ivk_oCVisualFX_CreateHierachy(0x0048ADF0,
		&Hk_oCVisualFX_CreateHierachy, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCVisualFX_CreateHierachy(oCVisualFX* self, void* edx) {
		if (self) {
			const BYTE* fx = (const BYTE*)self;
			const BYTE* root = (const BYTE*)VdGet(fx, VD_OFS_ROOT);
			const BYTE* parent = (const BYTE*)VdGet(fx, VD_OFS_PARENT);
			VdLearnVt(fx);
			VdForgetDead(fx);
			if (root && root != fx) {
				VdLearnVt(root);
				VdForgetDead(root);
			}
			if (parent && parent != fx && parent != root) {
				VdLearnVt(parent);
				VdForgetDead(parent);
			}
		}
		Ivk_oCVisualFX_CreateHierachy(self, edx);
	}

	// ---------- CreateAndPlay: kto tworzy efekty ----------
	static void VdCreated(const BYTE* fx, const DWORD* retSlot) {
		VdLearnVt(fx);
		VdForgetDead(fx);
		VdLoad();
		if (!vdDiag)
			return;
		DWORD ret[4];
		int nret = VdStack(retSlot, ret, 3, 64);
		VdRingAdd('C', fx, ret, nret);
		const VdMod* m = nret ? VdModOf(ret[0]) : nullptr;
		if (!m || !_stricmp(m->name, "Gothic2.exe") || !_stricmp(m->name, "UNION_ABI.DLL"))
			return;
		char nm[48], st[300];
		VdName(fx, nm, sizeof(nm));
		VdStackText(ret, nret, st, sizeof(st));
		ZLog("VFX: utworzony %p '%s' (ref %d) przez %s", fx, nm, (int)VdGet(fx, VD_OFS_REFCTR), st);
		ZLogFlush();
	}

	static oCVisualFX* __cdecl Hk_oCVisualFX_CreateAndPlayVob(const zSTRING& name, const zCVob* origin, const zCVob* target,
		int level, float damage, int damageType, int isProjectile);
	static CInvoke<oCVisualFX*(__cdecl*)(const zSTRING&, const zCVob*, const zCVob*, int, float, int, int)>
		Ivk_oCVisualFX_CreateAndPlayVob(0x0048E760, &Hk_oCVisualFX_CreateAndPlayVob, IvkEnabled(ENGINE));
	static oCVisualFX* __cdecl Hk_oCVisualFX_CreateAndPlayVob(const zSTRING& name, const zCVob* origin, const zCVob* target,
		int level, float damage, int damageType, int isProjectile) {
		oCVisualFX* fx = Ivk_oCVisualFX_CreateAndPlayVob(name, origin, target, level, damage, damageType, isProjectile);
		if (fx)
			VdCreated((const BYTE*)fx, (const DWORD*)_AddressOfReturnAddress());
		return fx;
	}

	static oCVisualFX* __cdecl Hk_oCVisualFX_CreateAndPlayPos(const zSTRING& name, const zVEC3& pos, const zCVob* target,
		int level, float damage, int damageType, int isProjectile);
	static CInvoke<oCVisualFX*(__cdecl*)(const zSTRING&, const zVEC3&, const zCVob*, int, float, int, int)>
		Ivk_oCVisualFX_CreateAndPlayPos(0x0048EA80, &Hk_oCVisualFX_CreateAndPlayPos, IvkEnabled(ENGINE));
	static oCVisualFX* __cdecl Hk_oCVisualFX_CreateAndPlayPos(const zSTRING& name, const zVEC3& pos, const zCVob* target,
		int level, float damage, int damageType, int isProjectile) {
		oCVisualFX* fx = Ivk_oCVisualFX_CreateAndPlayPos(name, pos, target, level, damage, damageType, isProjectile);
		if (fx)
			VdCreated((const BYTE*)fx, (const DWORD*)_AddressOfReturnAddress());
		return fx;
	}
#endif
}
