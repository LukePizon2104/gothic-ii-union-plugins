// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Szybki strzal z kuszy (Crossbow.h). Silnik G2A 2.6 (deasemblacja w [notatki lokalne]):
	// - oCNpc::DoInsertMunition 0x744190: belt z ekwipunku (inventory2 Remove 1 szt.; zalozony - Equip) albo, przy
	//   ITM_FLAG_CREATEAMMO, oCWorld::CreateVob(zVOB_TYPE_ITEM, munition); effectVob->vt+0xA0(1, 1); kusza->CopyDamage(belt).
	// - oCNpc::DoShootArrow 0x7446B0: SetSleeping(1), AddVob, new oCAIArrow, SetAI, nazwa "Pfeil von NSC" + imie,
	//   oCAIArrow::SetupAIVob(belt, strzelec, cel), SetSleeping(0), Release(ai). Strzela tylko przy fmode 5 / 6.
	// - SetupAIVob 0x6A10E0 (bez sprawdzania fmode): belt w ZS_RIGHTHAND strzelca, predkosc 2000 w strone celu.
	// - oCAIArrow::ReportCollisionToAI 0x6A1530: talent (GetWeaponMode() != 5) + 3, szansa, oCMsgDamage do ofiary.
	// - oCNpc::OnDamage_Hit 0x666610: przy GetWeaponMode() <= 4 drugi rzut jak dla broni bialej (porazka = 10 %).
	// Dlatego na czas SetupAIVob i trafienia naszego beltu (kopia vtable obiektu oCAIArrow z wlasnym
	// ReportCollisionToAI; zapasowo hak OnDamage po desc.pItemWeapon) fmode bohatera = 6, a w TNpcSlot ZS_RIGHTHAND
	// lezy kusza (podmiana wskaznika, bez PutInSlot) - silnik, skrypty (B_AssessDamage, Npc_GetReadiedWeapon) i haki
	// modow (NB, Ikarus w Kronikach, GFA) licza strzal z kuszy, a miecz zostaje w dloni.
	static const char* XB_PROTO = "HUMANS_ZDF_XBOW";
	static const int XB_VT_RCAI = 6;           // zCObject 0..3, zCAIBase: DoAI 4, CanThisCollideWith 5, ReportCollisionToAI 6
	static const DWORD XB_ENGINE_RCAI = 0x006A1530;
	static const DWORD XB_ENGINE_DOAI = 0x006A1460;

	static zdcore::XbowTimes xbTimes;
	static bool xbTimesFromIni = false;
	static bool xbPack = false;
	static zCModelPrototype* xbSrc = nullptr;      // HUMANS_ZDF_XBOW wczytany (nie zakladany) - nasza referencja
	static zCModelPrototype* xbSrcBase = nullptr;
	static bool xbSrcTried = false;
	static std::map<std::string, float> xbBaseFps;
	static unsigned xbLastTell = 0;

	// ---------------------------------------------------------------- pomocnicze

	static void XbTell(const std::string& text, bool force = false) {
		DWORD now = GetTickCount();
		if (text.empty() || (!force && now - xbLastTell < 900) || !ogame || !ogame->GetTextView())
			return;
		xbLastTell = now;
		ogame->GetTextView()->PrintTimedCXY(zSTRING(text.c_str()), 2000.0f, nullptr);
	}

	static const char* XbName(zCVob* v) {
		static char buf[8][64];
		static int next = 0;
		if (!v)
			return "-";
		char* b = buf[next];
		next = (next + 1) & 7;
		if (oCNpc* n = zDYNAMIC_CAST<oCNpc>(v))
			strncpy_s(b, 64, n->GetInstanceName().ToChar(), _TRUNCATE);
		else if (oCItem* it = zDYNAMIC_CAST<oCItem>(v))
			strncpy_s(b, 64, it->GetInstanceName().ToChar(), _TRUNCATE);
		else
			strncpy_s(b, 64, v->GetObjectName().ToChar(), _TRUNCATE);
		return b;
	}

	static zCModelAni* XbClipOf(zCModelPrototype* src, zCModel* m, int clip, bool standing) {
		int id = m ? AniId(m, zdcore::XbowClipAni(clip, standing)) : -1;
		zCModelAni* a = ProtoAni(src, id);
		return a && a->layer >= 2 ? a : nullptr;   // pusty / bazowy wpis = paczka bez kuszy
	}

	static oCItem* XbCrossbow() {
		oCItem* w = player ? player->GetEquippedRangedWeapon() : nullptr;
		return w && w->HasFlag(ITM_FLAG_CROSSBOW) ? w : nullptr;
	}

	static int XbBoltCount(oCItem* xbow) {
		if (!xbow || xbow->munition <= 0)
			return 0;
		oCItem* b = player->IsInInv(xbow->munition, 1);
		return b ? b->amount : 0;
	}

	static bool XbStanding() {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		return ac && ac->walkmode <= 2 && ac->IsStanding();
	}

	static int XbTalent() {
		int t = player ? player->GetTalentSkill(oCNpcTalent::NPC_TAL_CROSSBOW) : 0;
		zCModel* m = player ? player->GetModel() : nullptr;
		for (int i = 1; m && i < m->modelProtoList.GetNum(); i++) {
			std::string k = m->modelProtoList[i] ? ProtoKey(m->modelProtoList[i]) : "";
			if (k == "HUMANS_CBOWT2")
				t = t > 2 ? t : 2;
			else if (k == "HUMANS_CBOWT1")
				t = t > 1 ? t : 1;
		}
		return t;
	}

	// Punkt celowania na NPC: BIP01 SPINE1 modelu, inaczej srodek bbox.
	static zVEC3 XbAimPointOn(oCNpc* npc) {
		zCModel* m = npc->GetModel();
		zCModelNodeInst* n = m ? m->SearchNode(zSTRING("BIP01 SPINE1")) : nullptr;
		if (n)
			return npc->GetTrafoModelNodeToWorld(n).GetTranslation();
		const zTBBox3D& bb = npc->bbox3D;
		return (bb.mins + bb.maxs) * 0.5f;
	}

	// ---------------------------------------------------------------- zQuickSign (wzajemna blokada)

	typedef int(__cdecl* XbZqsBusy_t)();
	static bool XbQsBusy() {
		static XbZqsBusy_t fn = nullptr;
		static DWORD next = 0;
		if (!fn) {
			DWORD now = GetTickCount();
			if ((int)(now - next) < 0)
				return false;
			next = now + 5000;
			HMODULE m = GetModuleHandleA("zQuickSign.dll");
			if (!m)
				m = GetModuleHandleA("NB_UNION_PLUGIN_ALLOWED_01.DLL");
			if (m)
				fn = (XbZqsBusy_t)GetProcAddress(m, "ZQS_Busy");
			if (!fn)
				return false;
		}
		return fn() != 0;
	}

	// ---------------------------------------------------------------- prototyp zrodlowy i pozyczanie wpisow

	static zCModelPrototype* XbSource(zCModel* m) {
		zCModelPrototype* base = m && m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		if (!base)
			return nullptr;
		if (xbSrc)
			return xbSrcBase == base ? xbSrc : nullptr;
		if (xbSrcTried || !xbPack)
			return nullptr;
		xbSrcTried = true;
		zCModelPrototype* p = zCModelPrototype::Load(zSTRING((std::string(XB_PROTO) + ".MDS").c_str()), base);
		if (!p) {
			ZLog("Kusza: silnik nie wczytal %s - szybki strzal bez animacji", XB_PROTO);
			return nullptr;
		}
		if (p->baseModelProto != base) {
			ZLog("Kusza: %s pod inna baza niz model bohatera - pomijam", XB_PROTO);
			p->Release();
			return nullptr;
		}
		xbSrc = p;
		xbSrcBase = base;
		std::string have;
		for (int c = 0; c < zdcore::XCLIP_COUNT; c++)
			for (int s = 0; s < 2; s++) {
				zCModelAni* a = XbClipOf(p, m, c, s != 0);
				have += std::string(have.empty() ? "" : ", ") + zdcore::XbowClipName(c) + (s ? "/postoj " : "/ruch ") +
					(a ? (std::to_string(a->numFrames) + " kl. w" + std::to_string(a->layer)) : std::string("BRAK"));
				if (a)
					xbBaseFps[std::string(zdcore::XbowClipAni(c, s != 0))] = a->fpsRate > 0.0f ? a->fpsRate : 25.0f;
			}
		ZLog("Kusza: wczytano %s (%s)", XB_PROTO, have.c_str());
		return p;
	}

	static void XbScaleClips(zCModel* m) {
		if (!xbSrc || !m)
			return;
		for (int c = 0; c < zdcore::XCLIP_COUNT; c++)
			for (int s = 0; s < 2; s++) {
				zCModelAni* a = XbClipOf(xbSrc, m, c, s != 0);
				auto it = xbBaseFps.find(zdcore::XbowClipAni(c, s != 0));
				if (!a || it == xbBaseFps.end())
					continue;
				float want = it->second / zdcore::XbowTimeScale(zcfg.xbowSpeed);   // XbowSpeed 200 % = fps x2
				if (fabsf(a->fpsRate - want) > 1e-3f)
					a->fpsRate = want;
			}
	}

	struct XbBorrow {
		zCModelPrototype* proto = nullptr;   // refCtr++ na czas pozyczenia
		int id = -1;
		zCModelAni* orig = nullptr;          // wpis z pliku (jego referencje trzyma dalej tablica - nie ruszamy)
		zCModelAni* placed = nullptr;        // nasz klip (AddRef)
		std::string ani, protoName;
	};
	static std::vector<XbBorrow> xbBorrowed;
	static zCModel* xbBorrowModel = nullptr;

	static bool XbIsOurs(zCModelPrototype* p) {
		return p && ProtoKey(p).rfind("HUMANS_ZDF_", 0) == 0;
	}

	// Klip pod jego wlasnym aniID w prototypie, ktory dzis rozwiazuje to ID (zdcore::XbowBorrowSlot). why = powod odmowy.
	static bool XbBorrowOne(zCModel* m, zCModelAni* clip, const char* ani, std::string& why) {
		int id = clip->aniID;
		if (id != AniId(m, ani)) {
			why = "aniID klipu inne niz nazwy w modelu";
			return false;
		}
		if (m->GetAniFromAniID(id) == clip)
			return true;   // juz (np. nakladka HUMANS_ZDF_XBOW zalozona)
		for (auto& b : xbBorrowed)
			if (b.id == id && b.placed == clip)
				return true;
		std::vector<zdcore::XbowProtoInfo> info;
		int n = m->modelProtoList.GetNum();
		for (int i = 0; i < n; i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			info.push_back({ i > 0 && XbIsOurs(p), ProtoAni(p, id) != nullptr });
		}
		int w = 0, k = zdcore::XbowBorrowSlot(info, &w);
		if (k < 0) {
			why = zdcore::XbowBorrowWhyLabel(w);
			for (int i = n - 1; i > 0 && w == zdcore::XB_FOREIGN_ABOVE; i--)
				if (info[i].hasEntry && !info[i].ours) {
					why += " (" + ProtoKey(m->modelProtoList[i]) + ")";
					break;
				}
			return false;
		}
		zCModelPrototype* p = m->modelProtoList[k];
		if (id >= p->protoAnis.GetNum()) {
			why = "tablica nakladki krotsza niz aniID";
			return false;
		}
		zCModelAni* cur = p->protoAnis.GetArray()[id];
		if (AtkAniPlaying(m, cur)) {
			why = std::string("nosnik ") + ani + " wlasnie gra";
			return false;
		}
		clip->AddRef();
		p->protoAnis.GetArray()[id] = clip;
		p->refCtr++;
		XbBorrow b;
		b.proto = p;
		b.id = id;
		b.orig = cur;
		b.placed = clip;
		b.ani = ani;
		b.protoName = ProtoKey(p);
		xbBorrowed.push_back(b);
		xbBorrowModel = m;
		if (m->GetAniFromAniID(id) != clip) {
			why = "po zapisie GetAniFromAniID nie daje klipu";
			return false;
		}
		ZDebug("Kusza: wpis %s w %s pozyczony (klip kuszy, bylo %s)", ani, b.protoName.c_str(), cur ? "gest / animacja z pliku" : "pusto");
		return true;
	}

	// Wpisy z powrotem jak w pliku. force = takze gdy klip jeszcze gra (wygasza sie) - klip zyje w prototypie zrodlowym.
	static int XbRestoreEntries(const char* why, bool force) {
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		int done = 0, left = 0;
		for (size_t i = xbBorrowed.size(); i-- > 0;) {
			XbBorrow& b = xbBorrowed[i];
			if (!force && m && m == xbBorrowModel && AtkAniPlaying(m, b.placed)) {
				left++;
				continue;
			}
			zCModelPrototype* p = b.proto;
			if (b.id >= 0 && b.id < p->protoAnis.GetNum() && p->protoAnis.GetArray()[b.id] == b.placed) {
				p->protoAnis.GetArray()[b.id] = b.orig;
				b.placed->Release();
			}
			else
				ZLog("Kusza: wpis %s w %s zmieniony z zewnatrz - zostawiam (bez oddania klipu)", b.ani.c_str(), b.protoName.c_str());
			p->Release();
			xbBorrowed.erase(xbBorrowed.begin() + i);
			done++;
		}
		if (done)
			ZLog("Kusza: wpisy oddane (%d, %s)%s", done, why, left ? " - reszta po wygaszeniu klipu" : "");
		if (xbBorrowed.empty())
			xbBorrowModel = nullptr;
		return left;
	}

	// ---------------------------------------------------------------- udawanie trybu kuszy (RAII)

	static int xbSpoofDepth = 0;
	static bool xbViaSetup = true;         // [ZDISMEMBER] XbowSetupAIVob (Gothic.ini, czytane tu - bez zmian w Config.*)
	static bool xbNbRecord = false;        // ostatni belt przeszedl przez SetupAIVob (rekord NB)
	static oCNpc* xbHitNpc = nullptr;      // NPC przy ostatniej kolizji naszego beltu (tylko do porownan)
	static DWORD xbHitTick = 0;
	static DWORD xbShotTick = 0;           // ostatni szybki strzal - diagnostyka OnDamage bohatera przez 3 s
	static bool xbInRcai = false;          // trwa nasz ReportCollisionToAI (trafienie w oknie udawania)
	struct XbowRangedSpoof {
		oCNpc* npc = nullptr;
		int savedMode = 0;
		TNpcSlot* rh = nullptr;
		oCVob* savedVob = nullptr;
		bool on = false;
		XbowRangedSpoof(oCNpc* n, oCItem* xbow) {
			if (!n || xbSpoofDepth > 0)
				return;
			npc = n;
			savedMode = n->fmode;
			rh = n->GetInvSlot(zSTRING("ZS_RIGHTHAND"));
			savedVob = rh ? rh->vob : nullptr;
			n->fmode = NPC_WEAPON_CBOW;
			if (rh && xbow)
				rh->vob = xbow;
			on = true;
			xbSpoofDepth++;
		}
		~XbowRangedSpoof() {
			if (!on)
				return;
			xbSpoofDepth--;
			if (npc->fmode == NPC_WEAPON_CBOW)
				npc->fmode = savedMode;
			else
				ZLog("Kusza: tryb broni zmieniony w oknie udawania (%d) - zostaje", npc->fmode);
			if (rh)
				rh->vob = savedVob;
		}
	};

	// ---------------------------------------------------------------- belty w locie

	struct XbBolt {
		oCItem* bolt = nullptr;        // tylko do porownan (bez referencji)
		oCAIArrow* ai = nullptr;
		DWORD tick = 0;
		int dmg = 0;
		oCNpc* target = nullptr;       // tylko do logu (bez referencji)
		bool hitLogged = false;
	};
	static std::vector<XbBolt> xbBolts;

	static XbBolt* XbBoltOf(oCItem* it) {
		for (auto& b : xbBolts)
			if (it && b.bolt == it)
				return &b;
		return nullptr;
	}

	static XbBolt* XbBoltOfAi(oCAIArrow* ai) {
		for (auto& b : xbBolts)
			if (ai && b.ai == ai)
				return &b;
		return nullptr;
	}

	// Raport po trafieniu: ofiara po 2 s (HP, smierc, stan AI, postawa) i PD bohatera.
	struct XbReport {
		oCNpc* npc = nullptr;          // AddRef
		int hp0 = 0;
		unsigned long xp0 = 0;
		DWORD at = 0;
		int att0 = -1;
	};
	static std::vector<XbReport> xbReports;

	static void XbReportStart(oCNpc* npc, int hp0) {
		for (auto& r : xbReports)
			if (r.npc == npc)
				return;
		if (xbReports.size() >= 8 || !npc)
			return;
		npc->AddRef();
		XbReport r;
		r.npc = npc;
		r.hp0 = hp0;
		r.xp0 = player ? player->experience_points : 0;
		r.at = GetTickCount() + 2000;
		r.att0 = npc->GetAttitude(player);
		xbReports.push_back(r);
	}

	static void XbReportsUpdate(bool flushAll) {
		DWORD now = GetTickCount();
		for (size_t i = xbReports.size(); i-- > 0;) {
			XbReport& r = xbReports[i];
			if (!flushAll && (int)(now - r.at) < 0)
				continue;
			oCNpc* n = r.npc;
			bool world = n->homeWorld != nullptr;
			ZLog("Kusza: po 2 s - %s HP %d -> %d%s%s, stan AI %s, postawa do bohatera %d -> %d, PD bohatera %+ld", XbName(n), r.hp0,
				n->attribute[NPC_ATR_HITPOINTS], n->IsDead() ? " (martwy)" : "", n->IsUnconscious() ? " (nieprzytomny)" : "",
				world ? n->state.curState.name.ToChar() : "-", r.att0, world && player ? n->GetAttitude(player) : -1,
				player ? (long)player->experience_points - (long)r.xp0 : 0L);
			n->Release();
			xbReports.erase(xbReports.begin() + i);
		}
	}

	// Kopia vtable oCAIArrow tylko dla naszych obiektow: wpis ReportCollisionToAI -> XbRcai (wola oryginalny wpis - ten
	// z vtable klasy, czyli silnik albo hak NB).
	typedef void(__fastcall* XbRcai_t)(oCAIArrow*, void*, const void*);
	static DWORD xbVt[17];             // [0] = RTTI, potem 16 wpisow
	static DWORD xbVtFrom = 0;         // oryginalna vtable, z ktorej zrobiono kopie
	static XbRcai_t xbRcaiOrig = nullptr;
	static bool xbVtBad = false;

	static void __fastcall XbRcai(oCAIArrow* self, void* edx, const void* report) {
		XbBolt* b = XbBoltOfAi(self);
		oCItem* xbow = XbCrossbow();
		int hasHit0 = self->hasHit;
		if (!b || !player || self->owner != player || !zcfg.crossbow) {
			xbRcaiOrig(self, edx, report);
			return;
		}
		int mode0 = player->fmode;
		// NPC przy belcie w chwili kolizji (najblizszy do 250 cm) - HP przed / po i raport po 2 s (dowod obrazen)
		oCNpc* nearNpc = nullptr;
		float best = 250.0f * 250.0f;
		oCWorld* w = ogame ? ogame->GetGameWorld() : nullptr;
		zVEC3 bp = self->vob ? self->vob->GetPositionWorld() : zVEC3(0, 0, 0);
		for (auto it = w && w->voblist_npcs ? w->voblist_npcs->next : nullptr; it && self->vob; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld)
				continue;
			zVEC3 c = (npc->bbox3D.mins + npc->bbox3D.maxs) * 0.5f;
			float d = (c - bp).Length_Sqr();
			if (d < best) {
				best = d;
				nearNpc = npc;
			}
		}
		int nearHp0 = nearNpc ? nearNpc->attribute[NPC_ATR_HITPOINTS] : 0;
		if (nearNpc) {
			xbHitNpc = nearNpc;
			xbHitTick = GetTickCount();
		}
		{
			XbowRangedSpoof sp(player, xbow);
			xbInRcai = true;
			xbRcaiOrig(self, edx, report);
			xbInRcai = false;
		}
		if (nearNpc) {
			ZLog("Kusza: kolizja przy %s (%.0f cm od srodka) - HP w oknie %d -> %d, rekord NB %s", XbName(nearNpc), sqrtf(best), nearHp0,
				nearNpc->attribute[NPC_ATR_HITPOINTS], xbNbRecord ? "tak (SetupAIVob)" : "nie");
			if (!nearNpc->IsDead())
				XbReportStart(nearNpc, nearHp0);
		}
		ZLog("Kusza: kolizja beltu po %u ms (tryb broni %d -> 6 w oknie -> %d, kusza w ZS_RIGHTHAND %s), trafienie %d -> %d%s",
			GetTickCount() - b->tick, mode0, player->fmode, xbow ? "tak" : "brak kuszy", hasHit0, self->hasHit,
			b->hitLogged ? "" : " - bez obrazen w oknie (sciana, chybienie albo wiadomosc obrazen w kolejce)");
	}

	static void XbMarkArrow(oCAIArrow* ai) {
		DWORD* vt = *(DWORD**)ai;
		if (xbVtBad)
			return;
		if (!xbVtFrom) {
			xbVtFrom = (DWORD)vt;
			memcpy(xbVt, vt - 1, sizeof(xbVt));
			xbRcaiOrig = (XbRcai_t)vt[XB_VT_RCAI];
			bool okLayout = vt[XB_VT_RCAI] == XB_ENGINE_RCAI || vt[4] == XB_ENGINE_DOAI;
			ZLog("Kusza: vtable oCAIArrow %08X - DoAI %08X%s, ReportCollisionToAI %08X%s%s", (DWORD)vt, vt[4],
				vt[4] == XB_ENGINE_DOAI ? " (silnik)" : "", vt[XB_VT_RCAI], vt[XB_VT_RCAI] == XB_ENGINE_RCAI ? " (silnik)" :
				" (hak innego pluginu / moda)", okLayout ? "" : " - NIEZNANY UKLAD, trafienie bez udawania (tylko hak OnDamage)");
			if (!okLayout) {
				xbVtBad = true;
				return;
			}
			xbVt[1 + XB_VT_RCAI] = (DWORD)&XbRcai;
		}
		if ((DWORD)vt != xbVtFrom) {
			ZLog("Kusza: obiekt oCAIArrow z inna vtable (%08X zamiast %08X) - bez podmiany", (DWORD)vt, xbVtFrom);
			return;
		}
		*(DWORD**)ai = xbVt + 1;
	}

	// ---------------------------------------------------------------- pocisk

	struct XbAimSpec {
		oCNpc* npc = nullptr;     // cel (bez referencji - wazny w tej klatce)
		bool point = false;       // punkt celownika / kamery
		zVEC3 at;
		const char* how = "przed siebie";
	};

	static zVEC3 XbMuzzle(const zVEC3& dirHint, bool inHand) {
		zCModel* m = player->GetModel();
		zCModelNodeInst* lh = m ? m->SearchNode(zSTRING("ZS_LEFTHAND")) : nullptr;
		if (lh && inHand) {
			zMAT4 t = player->GetTrafoModelNodeToWorld(lh);
			zVEC3 fwd = zVEC3(-t.v[0].n[0], -t.v[1].n[0], -t.v[2].n[0]);   // kusza: os -X wezla (jak C_CBOW_5 w ZS_RIGHTHAND)
			float s = fwd.Length();   // XbowHandScale skaluje baze wezla
			if (s < 1e-3f)
				return t.GetTranslation();
			fwd = fwd * (1.0f / s);
			return t.GetTranslation() + fwd * (-xbTimes.gripCm * s + zcfg.xbowMuzzleCm);   // poczatek kuszy -> chwyt -> wylot
		}
		// bez kuszy w dloni (zdism xbow fire): wysokosc piersi, 40 cm przed bohaterem
		zVEC3 p = player->GetPositionWorld();
		zCModelNodeInst* sp = m ? m->SearchNode(zSTRING("BIP01 SPINE2")) : nullptr;
		if (sp)
			p = player->GetTrafoModelNodeToWorld(sp).GetTranslation();
		return p + dirHint * 40.0f;
	}

	// Lot beltu (log): po 0,15 s pozycja, przebyta droga i predkosc; koniec lotu (wbity / zabrany / usuniety) - gdzie.
	// Belt trzymany AddRef najwyzej 5 s.
	struct XbFlight {
		oCItem* bolt = nullptr;
		void* ai = nullptr;          // tylko do porownania z callback_ai
		zVEC3 start;
		DWORD tick = 0;
		bool early = false;
		int frames = 0;
	};
	static std::vector<XbFlight> xbFlights;

	// Trafo pocisku jak SetupAIVob (0x6A1398 SetHeadingAtWorld + 0x6A13CE PostRotateY(90)): os "at" w strone celu, potem
	// obrot o 90 st. wokol Y (grot wzdluz osi modelu beltu).
	static zMAT4 XbArrowMatrix(const zVEC3& pos, const zVEC3& dir) {
		zVEC3 at = dir, up(0.0f, 1.0f, 0.0f);
		if (fabsf(at.n[1]) > 0.99f)
			up = zVEC3(1.0f, 0.0f, 0.0f);
		zVEC3 right(up.n[1] * at.n[2] - up.n[2] * at.n[1], up.n[2] * at.n[0] - up.n[0] * at.n[2], up.n[0] * at.n[1] - up.n[1] * at.n[0]);
		right = right * (1.0f / right.Length());
		zVEC3 up2(at.n[1] * right.n[2] - at.n[2] * right.n[1], at.n[2] * right.n[0] - at.n[0] * right.n[2],
			at.n[0] * right.n[1] - at.n[1] * right.n[0]);
		zMAT4 m;
		for (int r = 0; r < 3; r++) {
			m.v[r].n[0] = right.n[r];
			m.v[r].n[1] = up2.n[r];
			m.v[r].n[2] = at.n[r];
			m.v[r].n[3] = pos.n[r];
		}
		m.v[3].n[0] = m.v[3].n[1] = m.v[3].n[2] = 0.0f;
		m.v[3].n[3] = 1.0f;
		m.PostRotateY(90.0f);
		return m;
	}

	static void XbFlightStart(oCItem* bolt, const zVEC3& start, void* ai) {
		if (xbFlights.size() >= 8)
			return;
		bolt->AddRef();
		XbFlight f;
		f.bolt = bolt;
		f.ai = ai;
		f.start = start;
		f.tick = GetTickCount();
		xbFlights.push_back(f);
	}

	static void XbFlightsUpdate(bool flushAll) {
		DWORD now = GetTickCount();
		for (size_t i = xbFlights.size(); i-- > 0;) {
			XbFlight& f = xbFlights[i];
			DWORD el = now - f.tick;
			oCItem* b = f.bolt;
			zCRigidBody* rb = b->homeWorld ? b->GetRigidBody() : nullptr;
			float spd = rb ? rb->v.Length() : 0.0f;
			float dist = (b->GetPositionWorld() - f.start).Length();
			if (el <= 500 && !flushAll && f.frames < 40) {   // co klatke przez 0,5 s
				f.frames++;
				zVEC3 p = b->GetPositionWorld();
				ZLog("Kusza: lot [%3u ms] %.0f %.0f %.0f, %.0f cm od wylotu, v %.0f, swiat %d, AI %s, fizyka %d, sen %d, ruch %d, "
					"kolizja %d/%d, trafienie %d", el, p.n[0], p.n[1], p.n[2], dist, spd, b->homeWorld ? 1 : 0,
					b->callback_ai == f.ai ? "nasze" : b->callback_ai ? "INNE" : "brak", b->physicsEnabled ? 1 : 0, (int)b->sleepingMode,
					(int)b->isInMovementMode, b->collDetectionStatic ? 1 : 0, b->collDetectionDynamic ? 1 : 0,
					b->callback_ai == f.ai && f.ai ? ((oCAIArrow*)f.ai)->hasHit : -1);
			}
			if (!f.early && el >= 150 && !flushAll) {
				f.early = true;
				ZLog("Kusza: lot beltu po %u ms - %.0f cm od wylotu, predkosc %.0f cm/s, %s", el, dist, spd,
					b->homeWorld ? "w swiecie" : "poza swiatem");
			}
			bool gone = !b->homeWorld || b->refCtr <= 1;
			if (gone || el >= 5000 || flushAll) {
				ZLog("Kusza: lot beltu - koniec po %u ms: %.0f cm od wylotu, predkosc %.0f, %s", el, dist, spd,
					!b->homeWorld ? "usuniety ze swiata / zabrany" : b->refCtr <= 1 ? "zostal tylko u nas" : "nadal w swiecie (5 s)");
				b->Release();
				xbFlights.erase(xbFlights.begin() + i);
			}
		}
	}

	// -> belt wystrzelony (true) albo powod odmowy w why.
	static bool XbFireBolt(const XbAimSpec& aim, bool inHand, const char* who, std::string& why) {
		oCItem* xbow = XbCrossbow();
		oCWorld* world = player && player->homeWorld ? (oCWorld*)player->homeWorld : nullptr;
		if (!xbow || !world) {
			why = "brak zalozonej kuszy";
			return false;
		}
		int mun = xbow->munition;
		bool create = xbow->HasFlag(ITM_FLAG_CREATEAMMO) != 0;
		int before = XbBoltCount(xbow);
		if (mun <= 0 || (!create && before <= 0)) {
			why = "brak beltow";
			return false;
		}
		oCItem* bolt = nullptr;
		if (create)
			bolt = zDYNAMIC_CAST<oCItem>(world->CreateVob(zVOB_TYPE_ITEM, mun));
		else {
			bolt = player->RemoveFromInv(mun, 1);
			if (bolt && bolt->HasFlag(ITM_FLAG_ACTIVE))
				player->Equip(bolt);   // jak silnik 0x744371 - zalozony belt zdjety
		}
		if (!bolt) {
			why = create ? "CreateVob beltu nie dal przedmiotu" : "RemoveFromInv nie dal beltu";
			return false;
		}
		if (bolt->effectVob) {
			void* fx = bolt->effectVob;
			((void(__fastcall*)(void*, void*, int, int))(*(DWORD**)fx)[0xA0 / 4])(fx, nullptr, 1, 1);
		}
		xbow->CopyDamage(bolt);
		int after = XbBoltCount(xbow);
		zVEC3 aimAt = aim.npc ? XbAimPointOn(aim.npc) : aim.at;
		zVEC3 heroDir = player->GetAtVectorWorld();
		zVEC3 muzzle = XbMuzzle(heroDir, inHand);
		zVEC3 dir = aimAt - muzzle;
		float len = dir.Length();
		if (len < 1.0f) {
			dir = heroDir;
			len = 1.0f;
			aimAt = muzzle + dir * 1000.0f;
		}
		else
			dir = dir * (1.0f / len);
		// Belt z ekwipunku ma stare polozenie (gdzie lezal stos) - ustawiamy go w wylocie, zanim wejdzie do swiata i zanim
		// dostanie kolizje: w swiecie z kolizja SetTrafoObjToWorld to ruch z kolizja od starego punktu (sciany po drodze
		// zatrzymywaly belt w starym miejscu - test 23.10: 19481 cm od wylotu, bez lotu i trafienia).
		zVEC3 oldPos = bolt->GetPositionWorld();
		bool wasInWorld = bolt->homeWorld != nullptr;
		if (wasInWorld)
			bolt->homeWorld->RemoveVob(bolt);
		bolt->SetCollDetStat(FALSE);
		bolt->SetCollDetDyn(FALSE);
		bolt->SetTrafoObjToWorld(XbArrowMatrix(muzzle, dir));
		bolt->SetSleeping(TRUE);
		world->AddVob(bolt);
		oCAIArrow* ai = new oCAIArrow();
		bolt->SetAI(ai);
		bolt->SetVobName(zSTRING("Pfeil von NSC") + player->GetName(0));
		int fmode0 = player->fmode;
		// Tor ustawiamy sami - jak oCAIArrow::SetupAIVob 0x6A10E0, ale od razu z wylotu kuszy w punkt celu (bez wolania
		// SetupAIVob): New Balance podmienia SetupAIVob bohatera na wlasny free-aim (0x10235200: start z kamery, slad
		// CreateTrail w tamtym miejscu), a przestawienie gotowego beltu zostawialo smuge sladu od starego punktu do wylotu
		// (cienka linia na kilka sekund) i belt bez kolizji (brak ReportCollisionToAI i obrazen - log 23.10).
		float speed = zcfg.xbowBoltSpeed;
		// New Balance: rekord pocisku (tryb broni [rec+0x420] i naladowanie [rec+0x41C] z chwili strzalu) zaklada hak NB na
		// oCAIArrow::SetupAIVob (UNION_ABI 0x10235B5B) - funkcja obrazen NB (0x10324684) bierze tryb z rekordu, a bez niego
		// z player->fmode: szybki strzal z mieczem w rece liczyl sie jak cios mieczem (test 23.10: ~1060 zamiast ~600).
		// Dlatego SetupAIVob wolamy (w oknie udawania: fmode 6, kusza w ZS_RIGHTHAND) - tylko po rekord i pola oCAIArrow;
		// tor (polozenie, kierunek, predkosc, slad) ustawiamy potem sami od nowa, przy wylaczonej kolizji.
		bool viaSetup = xbViaSetup;
		if (viaSetup) {
			XbowRangedSpoof sp(player, xbow);
			ai->SetupAIVob(bolt, player, aim.npc);
		}
		if (ai->arrow != bolt) {   // bez SetupAIVob (XbowSetupAIVob=0) albo hak go nie ustawil - pola jak silnik 0x6A1106..0x6A11C6
			ai->ClearIgnoreCDVob();
			ai->targetNPC = aim.npc;
			if (aim.npc)
				aim.npc->AddRef();
			ai->collisionOccured = 0;
			ai->vob = bolt;
			bolt->AddRef();
			ai->owner = player;
			player->AddRef();
			ai->arrow = bolt;
			bolt->AddRef();
			if (bolt->effectVob) {   // jak 0x6A1244: efekt beltu magicznego (2, 1)
				void* fx = bolt->effectVob;
				((void(__fastcall*)(void*, void*, int, int))(*(DWORD**)fx)[0xA0 / 4])(fx, nullptr, 2, 1);
			}
			ai->AddIgnoreCDVob(player);
		}
		zVEC3 posSetup = bolt->GetPositionWorld();
		bolt->SetCollDetStat(FALSE);
		bolt->SetCollDetDyn(FALSE);
		bolt->SetTrafoObjToWorld(XbArrowMatrix(muzzle, dir));   // bez kolizji - zwykle przestawienie, nie ruch
		bolt->SetSleeping(FALSE);
		bolt->SetPhysicsEnabled(TRUE);
		zCRigidBody* rb = bolt->GetRigidBody();
		if (rb) {
			rb->SetVelocity(dir * speed);
			rb->gravityOn = FALSE;   // jak 0x6A12E6
		}
		bolt->SetCollDetStat(TRUE);
		bolt->SetCollDetDyn(TRUE);
		bolt->SetCollisionClass((zCCollisionObjectDef*)0x008D8368);   // klasa kolizji pocisku jak 0x6A1418
		bolt->SetFlag(ITM_FLAG_NFOCUS);                                 // 0x800000 jak 0x6A1427
		if (ai->trailStrip) {   // slad zalozony przez SetupAIVob w jego punkcie startu - od nowa od wylotu
			ai->trailStrip->EmptyEffect();
			if (ai->trailVob)
				ai->trailVob->SetPositionWorld(muzzle);
		}
		else
			ai->CreateTrail(bolt);                                      // slad od wylotu (po ustawieniu toru)
		xbNbRecord = viaSetup;
		ZLog("Kusza: SetupAIVob %s - jego start %.0f cm od wylotu, tor ustawiony od nowa z wylotu, slad %s",
			viaSetup ? "wolany w oknie (rekord pocisku NB z trybem 6)" : "pominiety (XbowSetupAIVob=0)", (posSetup - muzzle).Length(),
			ai->trailStrip ? "wyczyszczony / od wylotu" : "brak");
		XbMarkArrow(ai);
		xbShotTick = GetTickCount();
		XbBolt rec;
		rec.bolt = bolt;
		rec.ai = ai;
		rec.tick = GetTickCount();
		rec.dmg = bolt->GetFullDamage();
		rec.target = aim.npc;
		if (xbBolts.size() > 24)
			xbBolts.erase(xbBolts.begin());
		xbBolts.push_back(rec);
		ai->Release();
		std::string dmg;
		for (int i = 0; i < oEDamageIndex_MAX; i++)
			if (bolt->damage[i])
				dmg += " " + std::to_string(i) + ":" + std::to_string(bolt->damage[i]);
		ZLog("Kusza: strzal (%s) - %s, belt %s%s (%d -> %d szt.), obrazenia po CopyDamage %d [%s ], cel %s (%s, %.0f cm), "
			"tryb broni %d (okno 6), talent kuszy %d, szansa %d%%", who, XbName(xbow), XbName(bolt), create ? " (tworzony)" : "",
			before, after, rec.dmg, dmg.c_str(), aim.npc ? XbName(aim.npc) : "punkt", aim.how, (aimAt - muzzle).Length(), fmode0,
			XbTalent(), player->GetHitChance(NPC_HITCHANCE_CROSSBOW));
		ZLog("Kusza: tor wlasny (bez SetupAIVob) z %s %.0f %.0f %.0f -> %.0f %.0f %.0f, predkosc %.0f cm/s, slad %s, kolizja %d/%d",
			inHand ? "wylotu kuszy" : "piersi bohatera", muzzle.n[0], muzzle.n[1], muzzle.n[2], aimAt.n[0], aimAt.n[1], aimAt.n[2],
			speed, ai->trailStrip ? "od wylotu" : "brak", bolt->collDetectionStatic ? 1 : 0, bolt->collDetectionDynamic ? 1 : 0);
		ZLog("Kusza: belt przed strzalem %.0f cm od wylotu (%s), po wstawieniu %.1f cm od wylotu, AI %s, wizual %s, fizyka %d, "
			"sen %d", (oldPos - muzzle).Length(), wasInWorld ? "byl w swiecie - zdjety" : "z ekwipunku, poza swiatem",
			(bolt->GetPositionWorld() - muzzle).Length(), bolt->callback_ai == ai ? "nasz oCAIArrow" : "INNY",
			bolt->visual ? "jest" : "BRAK", bolt->physicsEnabled ? 1 : 0, (int)bolt->sleepingMode);
		{
			const zMAT4& m = bolt->trafoObjToWorld;
			float lx = sqrtf(m.v[0].n[0] * m.v[0].n[0] + m.v[1].n[0] * m.v[1].n[0] + m.v[2].n[0] * m.v[2].n[0]);
			float ly = sqrtf(m.v[0].n[1] * m.v[0].n[1] + m.v[1].n[1] * m.v[1].n[1] + m.v[2].n[1] * m.v[2].n[1]);
			float lz = sqrtf(m.v[0].n[2] * m.v[0].n[2] + m.v[1].n[2] * m.v[1].n[2] + m.v[2].n[2] * m.v[2].n[2]);
			zVEC3 tp = ai->trailVob ? ai->trailVob->GetPositionWorld() : zVEC3(0, 0, 0);
			ZLog("Kusza: macierz beltu [%.3f %.3f %.3f %.0f | %.3f %.3f %.3f %.0f | %.3f %.3f %.3f %.0f], dlugosci osi %.3f %.3f %.3f%s; "
				"slad: vob %s %.0f %.0f %.0f (%.1f cm od wylotu), pasek %s", m.v[0].n[0], m.v[0].n[1], m.v[0].n[2], m.v[0].n[3],
				m.v[1].n[0], m.v[1].n[1], m.v[1].n[2], m.v[1].n[3], m.v[2].n[0], m.v[2].n[1], m.v[2].n[2], m.v[2].n[3], lx, ly, lz,
				fabsf(lx - 1.0f) > 0.01f || fabsf(ly - 1.0f) > 0.01f || fabsf(lz - 1.0f) > 0.01f ? " - NIE ORTONORMALNA" : "",
				ai->trailVob ? "jest" : "brak", tp.n[0], tp.n[1], tp.n[2], ai->trailVob ? (tp - muzzle).Length() : -1.0f,
				ai->trailStrip ? "jest" : "brak");
			if ((bolt->GetPositionWorld() - muzzle).Length() > 5.0f)
				ZLog("Kusza: UWAGA - belt po wstawieniu %.0f cm od wylotu (oczekiwane 0)", (bolt->GetPositionWorld() - muzzle).Length());
		}
		XbFlightStart(bolt, muzzle, ai);
		return true;
	}

	// ---------------------------------------------------------------- wybor celu

	static bool XbHostile(oCNpc* npc) {
		return npc->IsHostile(player) || npc->enemy == player || npc->GetAttitude(player) == NPC_ATT_HOSTILE;
	}

	struct XbPick {
		oCNpc* npc = nullptr;      // AddRef
		zVEC3 point;
		bool point_ = false;
		const char* how = "przed siebie";
	};

	static void XbPickRelease(XbPick& p) {
		if (p.npc)
			p.npc->Release();
		p = XbPick();
	}

	// Cel automatyczny (stukniecie): fokus / najlepszy wrog w stozku kamery (Core), inaczej punkt promienia kamery.
	static void XbAutoTarget(XbPick& out, bool moving) {
		XbPickRelease(out);
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zCVob* cam = ogame ? ogame->camVob : nullptr;
		if (!world || !player)
			return;
		zVEC3 hp = player->GetPositionWorld(), hd = player->GetAtVectorWorld();
		zVEC3 cd = cam ? cam->GetAtVectorWorld() : hd;
		zdcore::XbowAimRules rules;
		rules.range = zcfg.xbowAutoRange;
		rules.coneDeg = zcfg.xbowAutoCone;
		rules.moveConeDeg = zcfg.xbowMoveCone;
		rules.neutral = zcfg.xbowAutoNeutral;
		std::vector<zdcore::XbowCand> cands;
		std::vector<oCNpc*> npcs;
		oCNpc* focus = player->GetFocusNpc();
		for (auto it = world->voblist_npcs ? world->voblist_npcs->next : nullptr; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!SgAlive(npc))
				continue;
			zVEC3 q = npc->GetPositionWorld();
			zdcore::XbowCand c;
			c.dx = q.n[0] - hp.n[0];
			c.dy = q.n[1] - hp.n[1];
			c.dz = q.n[2] - hp.n[2];
			if (c.dx * c.dx + c.dz * c.dz > rules.range * rules.range)
				continue;
			c.hostile = XbHostile(npc);
			c.focus = npc == focus;
			c.visible = SgVisible(npc, nullptr);
			cands.push_back(c);
			npcs.push_back(npc);
		}
		int i = zdcore::XbowPickTarget(cands, cd.n[0], cd.n[2], hd.n[0], hd.n[2], moving, rules);
		if (i >= 0) {
			out.npc = npcs[i];
			out.npc->AddRef();
			out.how = cands[i].focus ? "fokus" : "stozek kamery";
			return;
		}
		SgAim a;
		if (!moving && SgCameraAim(a, "kamera")) {
			out.point = a.point;
			out.point_ = true;
			out.how = "punkt kamery (bez celu)";
			SgAimRelease(a);
			return;
		}
		SgAimRelease(a);
		zCModelNodeInst* sp = player->GetModel() ? player->GetModel()->SearchNode(zSTRING("BIP01 SPINE2")) : nullptr;
		zVEC3 chest = sp ? player->GetTrafoModelNodeToWorld(sp).GetTranslation() : hp;
		out.point = chest + hd * rules.range;
		out.point_ = true;
		out.how = "przed siebie (bez celu)";
	}

	// ---------------------------------------------------------------- stan strzalu

	static zdcore::XbowRun xbRun;
	static XbPick xbPick;               // cel z chwili wcisniecia / puszczenia
	static oCItem* xbWeapon = nullptr;  // kusza strzalu (AddRef)
	static zCModel* xbModel = nullptr;  // model bohatera strzalu
	static DWORD xbCooldownUntil = 0;
	static bool xbSelected = false;     // kusza wybrana pod F (ZDF_XBOW_SLOT)
	static bool xbHeroHit = false;      // bohater trafiony (OnDamage) - przerwanie przed beltem
	static int xbCurClip = -1;          // XCLIP_* grajacego klipu
	static bool xbCurStanding = false;
	static zCModelAni* xbCurAni = nullptr;
	static int xbRestarts = 0;
	static bool xbFading = false;       // po koncu / przerwaniu - wpisy oddawane po wygaszeniu klipu
	static float xbTurnLeft = 0.0f;
	static int xbConsoleCmd = 0;        // 1 fire, 2 shoot, 3 aim, 4 anis - po zamknieciu konsoli
	static float xbConsoleAimT = -1.0f;
	static bool xbFromConsole = false;
	// wizual kuszy
	static bool xbInHand = false;
	static zCModel* xbHandModel = nullptr;
	static int xbNodeLeft = -1, xbNodeCbow = -1;
	static zCVisual* xbCbowPrev = nullptr;     // co bylo w ZS_CROSSBOW przy chwycie (AddRef)
	static bool xbCbowWasEmpty = false;
	static zCVisual* xbLeftPrev = nullptr;     // druga bron pary NB w ZS_LEFTHAND (AddRef) - ukryta na czas strzalu
	static zMAT4 xbLeftWritten;
	static bool xbLeftWrittenOk = false;
	// test zdism xbow anis
	static struct {
		bool on = false;
		zCModelAni* clip = nullptr;
		int id = -1;
		float t = 0.0f;
		bool started = false;
		std::string from;
	} xbAnisTest;

	bool XbowInHand() {
		return xbInHand;
	}

	// Strzal w toku albo wpisy animacji jeszcze pozyczone (klip wygasa) - zQuickSign nie zagra wtedy gestu Znaku.
	bool XbowBusy() {
		return xbRun.Active() || !xbBorrowed.empty();
	}

	static void XbVisualToHand() {
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m || !xbWeapon || xbInHand)
			return;
		xbNodeLeft = FindNode(m, "ZS_LEFTHAND");
		xbNodeCbow = FindNode(m, "ZS_CROSSBOW");
		zCVisual* vis = xbWeapon->GetVisual();
		if (xbNodeLeft < 0 || !vis) {
			ZLog("Kusza: brak wezla ZS_LEFTHAND albo wizualu kuszy - strzal bez kuszy w dloni");
			return;
		}
		zCModelNodeInst* lh = m->nodeList[xbNodeLeft];
		if (lh->nodeVisual && lh->nodeVisual != vis) {
			xbLeftPrev = lh->nodeVisual;
			xbLeftPrev->AddRef();
		}
		m->SetNodeVisual(lh, vis, FALSE);
		if (xbNodeCbow >= 0) {
			zCModelNodeInst* cb = m->nodeList[xbNodeCbow];
			xbCbowWasEmpty = cb->nodeVisual == nullptr;
			if (cb->nodeVisual) {
				xbCbowPrev = cb->nodeVisual;
				xbCbowPrev->AddRef();
				m->SetNodeVisual(cb, nullptr, FALSE);
			}
		}
		xbInHand = true;
		xbHandModel = m;
		xbLeftWrittenOk = false;
		ZLog("Kusza: z plecow do lewej dloni%s%s", xbLeftPrev ? " (druga bron pary ukryta)" : "",
			xbCbowWasEmpty ? " (na plecach jej nie bylo - SecondWithRanged=1)" : "");
	}

	static void XbVisualBack(const char* why) {
		if (!xbInHand)
			return;
		xbInHand = false;
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		if (m && m == xbHandModel) {
			if (xbNodeLeft >= 0 && xbNodeLeft < m->nodeList.GetNum()) {
				oCItem* it = player->GetSlotItem(zSTRING("ZS_LEFTHAND"));
				zCVisual* v = it ? it->GetVisual() : nullptr;   // druga bron pary (albo nic) - ze slotu, jak gra
				m->SetNodeVisual(m->nodeList[xbNodeLeft], v, FALSE);
			}
			if (xbNodeCbow >= 0 && xbNodeCbow < m->nodeList.GetNum() && !xbCbowWasEmpty) {
				oCItem* it = player->GetSlotItem(zSTRING("ZS_CROSSBOW"));
				zCVisual* v = it ? it->GetVisual() : xbCbowPrev;
				m->SetNodeVisual(m->nodeList[xbNodeCbow], v, FALSE);
			}
		}
		if (xbCbowPrev)
			xbCbowPrev->Release();
		if (xbLeftPrev)
			xbLeftPrev->Release();
		xbCbowPrev = xbLeftPrev = nullptr;
		xbHandModel = nullptr;
		ZLog("Kusza: z dloni na plecy (%s)", why);
	}

	void CrossbowApplyNodes(zCModel* m) {
		if (!xbInHand || m != xbHandModel || xbNodeLeft < 0 || xbNodeLeft >= m->nodeList.GetNum())
			return;
		bool adj = zcfg.xbowHandScale != 100;
		for (int i = 0; i < 6 && !adj; i++)
			adj = zcfg.xbowHandAdjust[i] != 0.0f;
		if (!adj)
			return;
		zCModelNodeInst* n = m->nodeList[xbNodeLeft];
		if (xbLeftWrittenOk && memcmp(&n->trafo, &xbLeftWritten, sizeof(zMAT4)) == 0)
			return;   // AdvanceAnis nie przeliczyl wezla od nowa - juz poprawiony
		// przesuniecie i obrot wokol chwytu (punkt (GripCm, 0, 0) kuszy - tam jest dlon), skala tez wokol chwytu:
		// L' = L * T(g) * Adjust * S * T(-g)
		float g = xbTimes.gripCm;
		zdcore::BackMat L = BsFromZ(n->trafo);
		zdcore::BackMat Tg = zdcore::BackIdentity(), Tng = zdcore::BackIdentity();
		Tg.m[0][3] = g;
		Tng.m[0][3] = -g;
		zdcore::BackMat A = zdcore::BackAdjust(zdcore::BackIdentity(), zcfg.xbowHandAdjust);
		float s = zcfg.xbowHandScale / 100.0f;
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				A.m[r][c] *= s;
		L = zdcore::BackMul(zdcore::BackMul(zdcore::BackMul(L, Tg), A), Tng);
		BsToZ(L, n->trafo);
		xbLeftWritten = n->trafo;
		xbLeftWrittenOk = true;
	}

	static zCModelAniActive* XbActiveClip(zCModel* m, zCModelAni* a) {
		for (int i = 0; a && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++)
			if (m->aniChannels[i] && m->aniChannels[i]->protoAni == a && !m->aniChannels[i]->isFadingOut)
				return m->aniChannels[i];
		return nullptr;
	}

	// Klip kuszy: pozyczony wpis pod jego aniID, start po wskazniku (przy odmowie pozyczenia - tez, z wpisem w logu).
	static bool XbStartClip(int clip, bool standing, float frame, bool quiet = false) {
		zCModel* m = player->GetModel();
		zCModelPrototype* src = XbSource(m);
		zCModelAni* a = XbClipOf(src, m, clip, standing);
		if (!a) {
			ZLog("Kusza: brak klipu %s (%s) - strzal bez animacji", zdcore::XbowClipName(clip), standing ? "postoj" : "ruch");
			xbCurAni = nullptr;
			xbCurClip = clip;
			return false;
		}
		std::string why;
		const char* ani = zdcore::XbowClipAni(clip, standing);
		if (!XbBorrowOne(m, a, ani, why))
			ZLog("Kusza: wpis %s nie pozyczony (%s) - klip startowany po wskazniku", ani, why.c_str());
		if (zCModelAniActive* old = XbActiveClip(m, a))
			m->StopAni(old);
		if (xbCurAni && xbCurAni != a)
			if (zCModelAniActive* prev = XbActiveClip(m, xbCurAni))
				m->FadeOutAni(prev);
		m->StartAni(a, 0);
		zCModelAniActive* act = XbActiveClip(m, a);
		if (act && frame > 0.0f)
			act->SetActFrame(frame);
		xbCurAni = a;
		xbCurClip = clip;
		xbCurStanding = standing;
		xbRestarts = 0;
		if (!quiet || !act)
			ZLog("Kusza: klip %s (%s, %s) %s%s", zdcore::XbowClipName(clip), ani, standing ? "postoj - cale cialo" : "ruch - tulow i rece",
				act ? "gra" : "NIE WYSTARTOWAL", frame > 0.0f ? (" od kl. " + std::to_string((int)frame)).c_str() : "");
		return act != nullptr;
	}

	// Co klatke: przejscie z wariantu postoju na ruch od tej samej klatki; klip zatrzymany przez silnik - wznowienie (3x);
	// petla celowania - od nowa po koncu.
	static void XbWatchClip() {
		zCModel* m = player->GetModel();
		if (!xbCurAni || !m)
			return;
		zCModelAniActive* act = XbActiveClip(m, xbCurAni);
		if (act && xbCurStanding && !XbStanding()) {
			float f = act->actFrame;
			ZDebug("Kusza: bohater ruszyl - klip %s dalej tylko tulow i rece od kl. %d", zdcore::XbowClipName(xbCurClip), (int)f);
			XbStartClip(xbCurClip, false, f);
			return;
		}
		if (act)
			return;
		if (xbCurClip == zdcore::XCLIP_LOOP) {
			XbStartClip(zdcore::XCLIP_LOOP, xbCurStanding && XbStanding(), 0.0f, true);   // petla celowania od nowa
			return;
		}
		// klip skonczyl sie sam przed czasem z ini albo zatrzymal go silnik (Interrupt, rzut, nakladka)
		float len = xbCurClip == zdcore::XCLIP_IN ? xbTimes.inLen : xbTimes.shootLen;
		float t = xbRun.t, scale = zdcore::XbowTimeScale(zcfg.xbowSpeed);
		if (t < len * scale - 0.08f && xbRestarts < 3) {
			xbRestarts++;
			float frame = t / scale * 25.0f;
			ZDebug("Kusza: klip %s zatrzymany po %.2f s - wznawiam od kl. %d", zdcore::XbowClipName(xbCurClip), t, (int)frame);
			int r = xbRestarts;
			XbStartClip(xbCurClip, xbCurStanding, frame);
			xbRestarts = r;
		}
	}

	static void XbFadeClip() {
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		if (m && xbCurAni)
			if (zCModelAniActive* a = XbActiveClip(m, xbCurAni))
				m->FadeOutAni(a);
		xbCurAni = nullptr;
		xbCurClip = -1;
	}

	static void XbEndShot(const char* why, bool fade) {
		XbVisualBack(why);
		if (fade)
			XbFadeClip();
		xbCurAni = nullptr;
		xbCurClip = -1;
		xbRun = zdcore::XbowRun();
		XbPickRelease(xbPick);
		if (xbWeapon)
			xbWeapon->Release();
		xbWeapon = nullptr;
		xbHeroHit = false;
		xbFading = !xbBorrowed.empty();
		xbFromConsole = false;
		xbConsoleAimT = -1.0f;
	}

	// Natychmiast i bez kosztu (przed beltem) albo przerwanie po belcie.
	static void XbCut(int cut) {
		int v = zdcore::XbowCutVerdict(xbRun, cut);
		if (v == zdcore::XCA_IGNORE)
			return;
		ZLog("Kusza: przerwanie - %s %s", zdcore::XbowCutLabel(cut), v == zdcore::XCA_ABORT_FREE ?
			"przed beltem: bez strzalu, beltu i przeladowania" : "po belcie: animacja przerwana, kusza na plecy");
		XbEndShot(zdcore::XbowCutLabel(cut), true);
	}

	static zdcore::XbowGate XbGate(bool needPack) {
		zdcore::XbowGate g;
		g.enabled = zcfg.enabled && zcfg.crossbow;
		g.human = HeroIsHuman();
		g.pack = !needPack || (xbPack && g.human && XbSource(player->GetModel()) != nullptr);
		g.alive = !player->IsDead() && !player->IsUnconscious();
		g.dialog = !oCInformationManager::GetInformationManager().HasFinished() || player->IsMovLock() || MeditateActive() ||
			PathMenuOpen() || (zcon && zcon->IsVisible());
		g.inventory = player->inventory2.IsOpen() != 0;
		g.busy = MovesActive() || TraversalActive() || DirectorActive() || LootActive() || IaActive() || XbQsBusy();
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		g.water = bs == BS_SWIM || bs == BS_DIVE || (ac && ac->actionMode >= 5);
		g.airborne = bs == BS_JUMP || bs == BS_FALL || bs == BS_STUMBLE || bs == BS_LIE;
		g.mob = bs == BS_MOBINTERACT || bs == BS_MOBINTERACT_INTERRUPT || bs == BS_CLIMB || bs == BS_SIT ||
			player->GetInteractMob() != nullptr;
		g.fmode = player->fmode;
		oCItem* ranged = player->GetEquippedRangedWeapon();
		g.hasCrossbow = ranged && ranged->HasFlag(ITM_FLAG_CROSSBOW);
		g.isBow = ranged && ranged->HasFlag(ITM_FLAG_BOW);
		oCItem* left = player->GetSlotItem(zSTRING("ZS_LEFTHAND"));
		g.torch = left && (left->HasFlag(ITM_FLAG_TORCH) || strstr(zdcore::Upper(left->GetInstanceName().ToChar()).c_str(), "TORCH"));
		g.dual = (left && !g.torch) || g.fmode == 8;
		g.shield = g.fmode == 9 || player->GetSlotItem(zSTRING("ZS_LEFTARM")) != nullptr;
		g.allowTorch = zcfg.xbowWithTorch;
		g.allowShield = zcfg.xbowWithShield;
		g.allowDual = zcfg.xbowWithDual;
		g.createAmmo = g.hasCrossbow && ranged->HasFlag(ITM_FLAG_CREATEAMMO);
		g.hasBolts = g.hasCrossbow && XbBoltCount(ranged) > 0;
		int left_ = (int)(xbCooldownUntil - GetTickCount());
		g.cooldownLeftMs = xbCooldownUntil && left_ > 0 ? left_ : 0;
		g.active = xbRun.Active();
		return g;
	}

	static void XbRefuse(int r, const char* who) {
		static int lastR = -1;
		static DWORD lastT = 0;
		DWORD now = GetTickCount();
		if (r != lastR || now - lastT > 3000)
			ZLog("Kusza: odmowa (%s) - %s", who, zdcore::XbowRefuseLabel(r));
		lastR = r;
		lastT = now;
		if (r == zdcore::XR_COOLDOWN) {
			int ms = (int)(xbCooldownUntil - now);
			char buf[64];
			sprintf_s(buf, "Kusza si\xEA prze\xB3" "adowuje: %d,%d s", ms / 1000, (ms % 1000) / 100);
			XbTell(buf);
		}
		else
			XbTell(zdcore::XbowRefuseMessage(r));
	}

	// Start strzalu (wcisniecie F albo konsola). -> kod odmowy (0 = przyjete).
	static int XbBegin(const char* who) {
		zdcore::XbowGate g = XbGate(true);
		int r = zdcore::XbowGateCheck(g);
		if (r != zdcore::XR_OK) {
			XbRefuse(r, who);
			return r;
		}
		if (!xbBorrowed.empty())
			XbRestoreEntries("nowy strzal", true);
		XbScaleClips(player->GetModel());
		xbWeapon = XbCrossbow();
		xbWeapon->AddRef();
		xbModel = player->GetModel();
		zdcore::XbowStart(xbRun);
		xbHeroHit = false;
		xbFading = false;
		bool standing = XbStanding();
		XbAutoTarget(xbPick, !standing);
		xbTurnLeft = standing && xbPick.npc ? xbTimes.turn * zdcore::XbowTimeScale(zcfg.xbowSpeed) : 0.0f;
		XbStartClip(zdcore::XCLIP_IN, standing, 0.0f);
		ZLog("Kusza: start (%s) - %s, tryb broni %d, %s, cel %s (%s), belty %d, tempo %d%%, belt za %.2f s przy stuknieciu",
			who, XbName(xbWeapon), player->fmode, standing ? "postoj" : "ruch", xbPick.npc ? XbName(xbPick.npc) : "punkt",
			xbPick.how, XbBoltCount(xbWeapon), zcfg.xbowSpeed, zdcore::XbowTapFireAt(xbTimes, zdcore::XbowTimeScale(zcfg.xbowSpeed)));
		return 0;
	}

	static void XbTurnStep(float dt) {
		if (xbTurnLeft <= 0.0f || !xbPick.npc || !XbStanding())
			return;
		zVEC3 hp = player->GetPositionWorld(), hd = player->GetAtVectorWorld(), tp = xbPick.npc->GetPositionWorld();
		float dx = tp.n[0] - hp.n[0], dz = tp.n[2] - hp.n[2], dl = sqrtf(dx * dx + dz * dz), fl = sqrtf(hd.n[0] * hd.n[0] + hd.n[2] * hd.n[2]);
		if (dl < 1.0f || fl < 1e-3f) {
			xbTurnLeft = 0.0f;
			return;
		}
		float fx = hd.n[0] / fl, fz = hd.n[2] / fl;
		dx /= dl;
		dz /= dl;
		float ang = atan2f(fx * dz - fz * dx, fx * dx + fz * dz);
		float step = fabsf(ang) * (dt >= xbTurnLeft ? 1.0f : dt / xbTurnLeft);
		float maxStep = 15.7f * dt;   // do 900 st./s
		if (step > maxStep)
			step = maxStep;
		float a = ang > 0.0f ? step : -step;
		float c = cosf(a), s = sinf(a);
		player->SetHeadingYWorld(zVEC3(hp.n[0] + (fx * c - fz * s) * 100.0f, hp.n[1], hp.n[2] + (fx * s + fz * c) * 100.0f));
		xbTurnLeft -= dt;
	}

	static void XbDoFire() {
		XbAimSpec aim;
		if (xbPick.npc && !SgAlive(xbPick.npc))
			XbPickRelease(xbPick);
		if (!xbPick.npc && !xbPick.point_ && !xbRun.aimed)
			XbAutoTarget(xbPick, !XbStanding());
		aim.npc = xbPick.npc;
		aim.point = xbPick.point_;
		aim.at = xbPick.point;
		aim.how = xbPick.how;
		std::string why;
		if (XbFireBolt(aim, xbInHand, xbRun.aimed ? "celowanie" : "stukniecie", why)) {
			int cd = zdcore::XbowCooldownMs(zcfg.xbowCooldownMs, XbTalent());
			xbCooldownUntil = GetTickCount() + cd;
			if (!xbCooldownUntil)
				xbCooldownUntil = 1;
			ZDebug("Kusza: przeladowanie %d ms (talent %d)", cd, XbTalent());
		}
		else
			ZLog("Kusza: belt nie wystrzelony - %s", why.c_str());
	}

	// ---------------------------------------------------------------- wejscie z zQuickSign (eksporty)

	static int XbInput(int phase, const float* point, void* hitVob, const char* who) {
		if (!player || !zcfg.enabled || !zcfg.crossbow)
			return zdcore::XR_OFF;
		if (phase == zdcore::XIN_PRESS)
			return XbBegin(who);
		if (!xbRun.Active())
			return zdcore::XR_ACTIVE;
		if (phase == zdcore::XIN_RELEASE && xbRun.intent == zdcore::XI_HOLD && point) {
			XbPickRelease(xbPick);
			oCNpc* npc = hitVob ? zDYNAMIC_CAST<oCNpc>((zCVob*)hitVob) : nullptr;
			if (npc && SgAlive(npc)) {
				xbPick.npc = npc;
				npc->AddRef();
				xbPick.how = "NPC pod celownikiem";
			}
			else {
				xbPick.point = zVEC3(point[0], point[1], point[2]);
				xbPick.point_ = true;
				xbPick.how = "punkt celownika";
			}
		}
		if (phase == zdcore::XIN_CANCEL) {
			// PPM z kusza w dloni: odlozenie jak W3 ThrowStop (aim_end_rp z XBOW_SHOOT od ShootOutMs), bez beltu i przeladowania
			float scale = zdcore::XbowTimeScale(zcfg.xbowSpeed);
			if (zdcore::XbowCancelOut(xbRun, xbTimes, scale)) {
				XbPickRelease(xbPick);
				bool st = XbStanding();
				XbStartClip(zdcore::XCLIP_SHOOT, st, xbTimes.outStart * 25.0f);
				ZLog("Kusza: anulowanie (%s) - odlozenie kuszy: XBOW_SHOOT od kl. %d (%s), na plecy za %.2f s, bez beltu i przeladowania",
					who, (int)(xbTimes.outStart * 25.0f), st ? "postoj" : "ruch", (xbTimes.release - xbTimes.outStart) * scale);
				return 0;
			}
		}
		bool ok = zdcore::XbowApplyInput(xbRun, phase);
		ZLog("Kusza: wejscie %d (%s) w fazie %s -> %s", phase, who, zdcore::XbowPhaseName(xbRun.phase), ok ? "przyjete" : "pominiete");
		if (phase == zdcore::XIN_CANCEL && ok)
			XbCut(zdcore::XC_CANCEL);
		return 0;
	}

#if ENGINE == Engine_G2A
	// zQuickSign: stan slotu kuszy (wybor, etykieta). avail = zDismember z paczka W3 i zalozona kusza. 1 = odpowiedz.
	extern "C" __declspec(dllexport) int __cdecl ZDISM_XbowState(int* avail, int* selected, int* bolts, int* cooldownMs, char* label,
		int size) {
		oCItem* xbow = player && zcfg.enabled && zcfg.crossbow ? XbCrossbow() : nullptr;
		bool a = xbow && xbPack && HeroIsHuman();
		if (avail)
			*avail = a ? 1 : 0;
		if (selected)
			*selected = a && xbSelected ? 1 : 0;
		if (bolts)
			*bolts = xbow ? (xbow->HasFlag(ITM_FLAG_CREATEAMMO) ? 999 : XbBoltCount(xbow)) : 0;
		int left = (int)(xbCooldownUntil - GetTickCount());
		if (cooldownMs)
			*cooldownMs = xbCooldownUntil && left > 0 ? left : 0;
		if (label && size > 0) {
			std::string name = xbow ? xbow->description.ToChar() : "";
			if (name.empty() && xbow)
				name = xbow->name.ToChar();
			strncpy_s(label, size, name.c_str(), _TRUNCATE);
		}
		return 1;
	}

	extern "C" __declspec(dllexport) int __cdecl ZDISM_XbowSelect(int on) {
		if (xbSelected != (on != 0))
			ZLog("Kusza: slot pod F %s", on ? "wybrany" : "odznaczony");
		xbSelected = on != 0;
		SgParserSetInt("ZDF_XBOW_SLOT", zdcore::XbowSlotValue(SgParserInt("ZDF_XBOW_SLOT", 0), xbSelected));
		return 1;
	}

	// phase: 1 wcisniecie, 2 prog przytrzymania, 3 puszczenie (point = punkt celownika albo 0 = cel automatyczny),
	// 4 anulowanie. -> 0 przyjete, inaczej kod odmowy (zdcore::XbowRefuse).
	extern "C" __declspec(dllexport) int __cdecl ZDISM_XbowInput(int phase, const float* point, void* hitVob) {
		return XbInput(phase, point, hitVob, "F (zQuickSign)");
	}

	extern "C" __declspec(dllexport) int __cdecl ZDISM_XbowBusy() {
		return XbowBusy() ? 1 : 0;
	}
#endif

	// ---------------------------------------------------------------- trafienia (CombatHooks)

	void XbowOnDamage(oCNpc* victim, oCNpc::oSDamageDescriptor& desc, int hpBefore, bool swallowed) {
		if (!player || !victim)
			return;
		if (victim == player && desc.pNpcAttacker && desc.pNpcAttacker != player && xbRun.Active() && !xbRun.fired &&
			(victim->attribute[NPC_ATR_HITPOINTS] < hpBefore) && zcfg.xbowHitCancel)
			xbHeroHit = true;
		if (desc.pNpcAttacker == player && victim != player && xbShotTick && GetTickCount() - xbShotTick < 3000) {
			oCItem* iw = desc.pItemWeapon;
			ZLog("Kusza: OnDamage bohatera po strzale - %s HP %d -> %d%s, pola 0x%X, bron %s%s, tryb broni desc %d, typ 0x%X, "
				"suma %.0f, tryb napastnika teraz %d, okno udawania %d", XbName(victim), hpBefore, victim->attribute[NPC_ATR_HITPOINTS],
				swallowed ? " (polkniete)" : "", desc.dwFieldsValid, iw ? XbName(iw) : "-", XbBoltOf(iw) ? " (nasz belt)" : "",
				desc.enuModeWeapon, desc.enuModeDamage, desc.fDamageTotal, player->fmode, xbSpoofDepth);
		}
		oCItem* w = desc.pItemWeapon;
		XbBolt* b = XbBoltOf(w);
		if (!b && desc.pNpcAttacker == player && w && desc.enuModeWeapon == 4 && zcfg.debug) {
			// porownanie: zwykly strzal bohatera (tryb luku / kuszy) - te same punkty pomiaru co szybki strzal
			ZDebug("Kusza (zwykly strzal): trafienie %s - %s, HP %d -> %d (obrazenia pocisku %d), tryb napastnika %d, desc: bron %d, "
				"typ 0x%X", XbName(victim), swallowed ? "POLKNIETE" : "trafiony", hpBefore, victim->attribute[NPC_ATR_HITPOINTS],
				w->GetFullDamage(), player->GetWeaponMode(), desc.enuModeWeapon, desc.enuModeDamage);
			return;
		}
		if (!b || desc.pNpcAttacker != player)
			return;
		b->hitLogged = true;
		oCItem* inHand = player->GetWeapon();
		ZLog("Kusza: trafienie %s - %s, HP %d -> %d (obrazenia beltu %d), tryb napastnika %d, Npc_GetReadiedWeapon %s, "
			"desc: bron %d, typ 0x%X, w oknie udawania %s", XbName(victim), swallowed ? "POLKNIETE (unik / finisher / Quen)" : "trafiony",
			hpBefore, victim->attribute[NPC_ATR_HITPOINTS], b->dmg, player->GetWeaponMode(), XbName(inHand), desc.enuModeWeapon,
			desc.enuModeDamage, xbSpoofDepth > 0 ? (xbInRcai ? "tak (ReportCollisionToAI)" : "tak") : "NIE - wiadomosc z kolejki");
		if (!swallowed)
			XbReportStart(victim, hpBefore);
	}

	// Zapas na wiadomosc obrazen z kolejki (poza oknem ReportCollisionToAI): udawanie na czas OnDamage naszym beltem.
	XbowDamageGuard::XbowDamageGuard(oCNpc* victim, oCNpc::oSDamageDescriptor& desc) {
		oCItem* w = desc.pItemWeapon;
		bool ours = XbBoltOf(w) != nullptr || (victim == xbHitNpc && GetTickCount() - xbHitTick < 2000 && !desc.pFXHit &&
			!desc.nSpellID);
		if (!player || desc.pNpcAttacker != player || xbSpoofDepth > 0 || !ours || victim == player)
			return;
		p = new XbowRangedSpoof(player, XbCrossbow());
		ZLog("Kusza: OnDamage %s poza oknem trafienia - udawanie trybu kuszy w haku OnDamage", XbName(victim));
	}

	XbowDamageGuard::~XbowDamageGuard() {
		delete (XbowRangedSpoof*)p;
	}

	// ---------------------------------------------------------------- test pozyczania (zdism xbow anis)

	static void XbAnisTestEnd(const char* why) {
		if (!xbAnisTest.on)
			return;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (m && xbAnisTest.clip)
			if (zCModelAniActive* a = XbActiveClip(m, xbAnisTest.clip))
				m->StopAni(a);
		XbRestoreEntries(why, true);
		int id = xbAnisTest.id;
		zCModelAni* now = m && id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		ZLog("Kusza: test pozyczania - koniec (%s): po oddaniu GetAniFromAniID(%s) = %s (warstwa %d)%s", why,
			zdcore::XbowClipAni(zdcore::XCLIP_IN, true), now ? now->aniName.ToChar() : "-", now ? now->layer : -1,
			now && now->layer == 21 ? " - gest Znaku wrocil" : "");
		xbAnisTest.on = false;
		xbAnisTest.clip = nullptr;
	}

	static std::string XbAnisTestStart() {
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m)
			return "brak modelu";
		if (xbRun.Active())
			return "strzal w toku";
		XbAnisTestEnd("nowy test");
		const char* ani = zdcore::XbowClipAni(zdcore::XCLIP_IN, true);
		int id = AniId(m, ani);
		zCModelAni* before = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		zCModelAni* clip = XbClipOf(XbSource(m), m, zdcore::XCLIP_IN, true);
		std::string from = "HUMANS_ZDF_XBOW (klip kuszy)";
		if (!clip) {
			clip = id >= 0 && m->modelProtoList.GetNum() ? ProtoAni(m->modelProtoList[0], id) : nullptr;   // bazowa z HUMANS
			from = "HUMANS (bazowa animacja tej nazwy)";
		}
		if (!clip)
			return std::string("brak klipu testowego dla ") + ani;
		std::string why;
		bool ok = XbBorrowOne(m, clip, ani, why);
		zCModelAni* after = m->GetAniFromAniID(id);
		std::string protoList;
		for (int i = 0; i < m->modelProtoList.GetNum(); i++)
			protoList += std::string(i ? " " : "") + (m->modelProtoList[i] ? ProtoKey(m->modelProtoList[i]) : "?") +
				(ProtoAni(m->modelProtoList[i], id) ? "*" : "");
		ZLog("Kusza: test pozyczania %s: klip z %s, przed: %s (warstwa %d), pozyczenie %s%s, po: GetAniFromAniID %s klip; "
			"prototypy (* = ma wpis): %s", ani, from.c_str(), before ? before->aniName.ToChar() : "-", before ? before->layer : -1,
			ok ? "OK" : "NIE - ", ok ? "" : why.c_str(), after == clip ? "=" : "!=", protoList.c_str());
		xbAnisTest.on = true;
		xbAnisTest.clip = clip;
		xbAnisTest.id = id;
		xbAnisTest.t = 0.0f;
		xbAnisTest.started = false;
		xbAnisTest.from = from;
		return ok ? "pozyczony - klip gra po zamknieciu konsoli (log)" : "odmowa: " + why;
	}

	static void XbAnisTestUpdate(float dt) {
		if (!xbAnisTest.on)
			return;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m) {
			XbAnisTestEnd("brak modelu");
			return;
		}
		if (!xbAnisTest.started) {
			m->StartAni(xbAnisTest.id, 0);   // po ID - jak gra silnik (sprawdza pozyczony wpis)
			zCModelAniActive* a = XbActiveClip(m, xbAnisTest.clip);
			ZLog("Kusza: test pozyczania - StartAni(%d) po ID: %s", xbAnisTest.id, a ? "gra klip testowy" :
				"NIE gra klipu testowego (inna animacja albo nie wystartowal)");
			xbAnisTest.started = true;
		}
		xbAnisTest.t += dt;
		if (xbAnisTest.t > 3.0f || !XbActiveClip(m, xbAnisTest.clip))
			XbAnisTestEnd(xbAnisTest.t > 3.0f ? "3 s" : "klip sie skonczyl");
	}

	// ---------------------------------------------------------------- petla

	static void XbConsoleRun() {
		int cmd = xbConsoleCmd;
		xbConsoleCmd = 0;
		if (cmd == 1) {
			zdcore::XbowGate g = XbGate(false);
			int r = zdcore::XbowGateCheck(g);
			if (r != zdcore::XR_OK && r != zdcore::XR_COOLDOWN) {
				ZLog("Kusza: zdism xbow fire - odmowa: %s", zdcore::XbowRefuseLabel(r));
				XbTell(zdcore::XbowRefuseMessage(r), true);
				return;
			}
			XbPick p;
			XbAutoTarget(p, !XbStanding());
			XbAimSpec aim;
			aim.npc = p.npc;
			aim.point = p.point_;
			aim.at = p.point;
			aim.how = p.how;
			std::string why;
			if (!XbFireBolt(aim, false, "zdism xbow fire - bez animacji", why))
				ZLog("Kusza: zdism xbow fire - %s", why.c_str());
			XbPickRelease(p);
		}
		else if (cmd == 2 || cmd == 3) {
			if (XbBegin(cmd == 2 ? "konsola: stukniecie" : "konsola: celowanie 1,5 s") == 0) {
				xbFromConsole = true;
				if (cmd == 2)
					zdcore::XbowApplyInput(xbRun, zdcore::XIN_RELEASE);
				else {
					zdcore::XbowApplyInput(xbRun, zdcore::XIN_HOLD);
					xbConsoleAimT = 1.5f;
				}
			}
		}
		else if (cmd == 4)
			XbAnisTestUpdate(0.0f);
	}

	void CrossbowUpdate(float dt) {
		ZPROF_SCOPE("Crossbow");
		XbReportsUpdate(false);
		XbFlightsUpdate(false);
		if (!player || !player->homeWorld)
			return;
		for (size_t i = xbBolts.size(); i-- > 0;)
			if (GetTickCount() - xbBolts[i].tick > 15000)
				xbBolts.erase(xbBolts.begin() + i);
		if (xbConsoleCmd && !(zcon && zcon->IsVisible()))
			XbConsoleRun();
		XbAnisTestUpdate(dt);
		if (!xbRun.Active()) {
			if (xbFading && XbRestoreEntries("koniec strzalu", false) == 0)
				xbFading = false;
			return;
		}
		zCModel* m = player->GetModel();
		// przerwania
		if (!zcfg.enabled || !zcfg.crossbow) {
			XbCut(zdcore::XC_RESET);
			return;
		}
		if (m != xbModel || !HeroIsHuman()) {
			XbCut(zdcore::XC_RESET);
			return;
		}
		if (player->IsDead() || player->IsUnconscious()) {
			XbCut(zdcore::XC_DEATH);
			return;
		}
		if (MovesActive() || TraversalActive() || DirectorActive()) {
			XbCut(zdcore::XC_MOVE);
			return;
		}
		if (player->inventory2.IsOpen()) {
			XbCut(zdcore::XC_INVENTORY);
			return;
		}
		int fm = player->fmode;
		if (XbCrossbow() != xbWeapon || fm == 5 || fm == 6 || fm == 7) {
			XbCut(zdcore::XC_WEAPON);
			return;
		}
		if (!oCInformationManager::GetInformationManager().HasFinished() || player->IsMovLock() || MeditateActive()) {
			XbCut(zdcore::XC_DIALOG);
			return;
		}
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		if (bs == BS_SWIM || bs == BS_DIVE || bs == BS_FALL || bs == BS_LIE || bs == BS_CLIMB) {
			XbCut(zdcore::XC_MOVE);
			return;
		}
		if (xbHeroHit) {
			xbHeroHit = false;
			XbCut(zdcore::XC_HIT);
			if (!xbRun.Active())
				return;
		}
		if (xbConsoleAimT > 0.0f) {
			xbConsoleAimT -= dt;
			if (xbConsoleAimT <= 0.0f) {
				SgAim a;
				if (SgCameraAim(a, "konsola")) {
					float pt[3] = { a.point.n[0], a.point.n[1], a.point.n[2] };
					XbInput(zdcore::XIN_RELEASE, pt, a.npc, "konsola: puszczenie");
				}
				else
					XbInput(zdcore::XIN_RELEASE, nullptr, nullptr, "konsola: puszczenie");
				SgAimRelease(a);
			}
		}
		XbTurnStep(dt);
		float scale = zdcore::XbowTimeScale(zcfg.xbowSpeed);
		zdcore::XbowEvents e = zdcore::XbowStep(xbRun, xbTimes, scale, dt);
		if (e.grab)
			XbVisualToHand();
		if (e.startLoop)
			XbStartClip(zdcore::XCLIP_LOOP, XbStanding(), 0.0f);
		if (e.startShoot)
			XbStartClip(zdcore::XCLIP_SHOOT, XbStanding(), 0.0f);
		if (e.fire)
			XbDoFire();
		if (e.release)
			XbVisualBack("koniec strzalu");
		if (e.end) {
			ZLog("Kusza: koniec strzalu po %.2f s", xbRun.total);
			XbEndShot("koniec strzalu", false);
			return;
		}
		XbWatchClip();
	}

	void CrossbowReset(const char* why) {
		if (xbRun.Active())
			ZLog("Kusza: strzal przerwany (%s)", why);
		XbAnisTestEnd(why);
		if (xbRun.Active())
			XbEndShot(why, true);
		XbVisualBack(why);
		XbRestoreEntries(why, true);
		xbFading = false;
		xbConsoleCmd = 0;
	}

	void CrossbowLoad() {
		xbPack = ZOverlayExists(XB_PROTO, zdcore::XbowClipAni(zdcore::XCLIP_IN, true));
		// XbowSetupAIVob=1: SetupAIVob wolany w oknie udawania (rekord pocisku New Balance - obrazenia jak zwykly strzal)
		if (!zoptions->EntryExists("ZDISMEMBER", "XbowSetupAIVob"))
			zoptions->WriteInt("ZDISMEMBER", "XbowSetupAIVob", 1, FALSE);
		xbViaSetup = zoptions->ReadInt("ZDISMEMBER", "XbowSetupAIVob", 1) != 0;
		static const char* names[] = { "\\SYSTEM\\ZDISMEMBER\\CROSSBOW_W3.INI", "SYSTEM\\ZDISMEMBER\\CROSSBOW_W3.INI" };
		std::string text, err;
		xbTimes = zdcore::XbowTimes();
		xbTimesFromIni = ZReadGameFile(names, 2, text) && zdcore::ParseXbowIni(text, xbTimes, err);
		ZLog("Kusza: %s, paczka %s, chwile %s (wejscie %.2f, petla %.2f, strzal %.2f, chwyt %.2f, belt %.2f, na plecy %.2f s)%s",
			zcfg.crossbow ? "wlaczona" : "wylaczona (Crossbow=0)", xbPack ? "HUMANS_ZDF_XBOW jest" : "BEZ HUMANS_ZDF_XBOW (bez animacji)",
			xbTimesFromIni ? "z crossbow_w3.ini" : "domyslne", xbTimes.inLen, xbTimes.loopLen, xbTimes.shootLen, xbTimes.grab,
			xbTimes.fire, xbTimes.release, err.empty() ? "" : (" - crossbow_w3.ini: " + err).c_str());
	}

	void CrossbowLoadBegin() {
		CrossbowReset("wczytanie / zmiana swiata");
		XbReportsUpdate(true);
		XbFlightsUpdate(true);
		xbBolts.clear();
		xbCooldownUntil = 0;
	}

	void CrossbowLoadEnd() {
		xbSelected = zdcore::XbowSlotSelected(SgParserInt("ZDF_XBOW_SLOT", 0));
		if (!parser || !parser->GetSymbol(zSTRING("ZDF_XBOW_SLOT")))
			ZLog("Kusza: brak zmiennej ZDF_XBOW_SLOT (zDismember_Xbow.d) - wybor slotu tylko do konca sesji");
	}

	void CrossbowBeforeSave() {
		CrossbowReset("zapis");
		SgParserSetInt("ZDF_XBOW_SLOT", zdcore::XbowSlotValue(SgParserInt("ZDF_XBOW_SLOT", 0), xbSelected));
	}

	std::string CrossbowConsole(const std::string& args) {
		auto a = zdcore::SplitArgs(args);
		std::string sub = a.empty() ? "INFO" : a[0];
		char buf[600];
		if (sub == "FIRE" || sub == "SHOOT" || sub == "AIM") {
			xbConsoleCmd = sub == "FIRE" ? 1 : sub == "SHOOT" ? 2 : 3;
			return sub == "FIRE" ? "belt bez animacji po zamknieciu konsoli (log: Kusza)" :
				sub == "SHOOT" ? "szybki strzal po zamknieciu konsoli" : "celowanie 1,5 s i strzal w punkt kamery po zamknieciu konsoli";
		}
		if (sub == "ANIS") {
			if (a.size() > 1 && a[1] == "INFO") {
				std::string s = "pozyczone wpisy:";
				for (auto& b : xbBorrowed)
					s += " " + b.ani + "@" + b.protoName;
				return xbBorrowed.empty() ? "pozyczone wpisy: brak" : s;
			}
			std::string r = XbAnisTestStart();
			xbConsoleCmd = 4;
			return r;
		}
		if (sub == "RESET") {
			CrossbowReset("zdism xbow reset");
			xbCooldownUntil = 0;
			return "kusza: stan od nowa";
		}
		oCItem* xbow = player ? XbCrossbow() : nullptr;
		zdcore::XbowGate g = player ? XbGate(true) : zdcore::XbowGate();
		int r = player ? zdcore::XbowGateCheck(g) : zdcore::XR_OFF;
		int cd = (int)(xbCooldownUntil - GetTickCount());
		sprintf_s(buf, "kusza %s (belty %d, talent %d, szansa %d%%), slot pod F %s, faza %s, gotowosc: %s, przeladowanie %d ms, "
			"pozyczone wpisy %d, paczka %s, chwile %s, tempo %d%%, w dloni %d", xbow ? XbName(xbow) : "brak", XbBoltCount(xbow), player ? XbTalent() : 0,
			player ? player->GetHitChance(NPC_HITCHANCE_CROSSBOW) : 0, xbSelected ? "wybrany" : "nie", zdcore::XbowPhaseName(xbRun.phase),
			zdcore::XbowRefuseLabel(r), xbCooldownUntil && cd > 0 ? cd : 0, (int)xbBorrowed.size(), xbPack ? "tak" : "nie",
			xbTimesFromIni ? "z ini" : "domyslne", zcfg.xbowSpeed, xbInHand ? 1 : 0);
		ZLog("zdism xbow: %s", buf);
		return buf;
	}
}
