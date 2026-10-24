// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Interakcje z dobyta bronia - opis zachowania w LootInteract.h, fakty silnika i przebieg w Core\Interact.h.
	// Wcisniecie: Loot.cpp (hak klawisza z Dodge.cpp, po MovesGameKeyOverride - blokada klawiszy w finisherze zostaje)
	// rozstrzyga przedmiot albo cel interakcji (mniejszy wynik), tu start w petli i przebieg (zdcore::IaStep).
	static const int IA_MAX_CHECKS = 4;          // celow na klatke z danymi z silnika (zamek, pozycje) i promieniem
	// StandActions 0x698EA0: call oCNpc::OpenDeadNpc pod 0x699051 (E8 rel32). Gdy inny modul podmienil to wywolanie
	// (Union PATCH), wolamy ten sam cel co gra; hak na wejsciu (Detours) dziala i tak.
	static const DWORD IA_OPENDEAD_CALLSITE = 0x00699051;
	static const DWORD IA_OPENDEAD_ENGINE = 0x00762970;
	// StandActions przed oCMobMsg(EV_STARTINTERACTION): licznik AssessUseMob (statyczny float 0xAAD6A0) = okres 3000 ms
	// (0x83BE1C) - pierwsze AssessUseMob_S w CheckMobInteraction od razu, jak przy zwyklym otwarciu (kradziez).
	static float* const IA_USEMOB_TIMER = (float*)0x00AAD6A0;
	static const float* const IA_USEMOB_PERIOD = (const float*)0x0083BE1C;
	static const int IA_TALENT_PICKLOCK = 5;     // jak oCMobLockable::CanOpen: HasTalent(5, 1)

	struct IaCand {
		zCVob* vob = nullptr;                    // AddRef
		zdcore::IaKind kind = zdcore::IA_NONE;
		zdcore::IaPlan plan = zdcore::IAP_NONE;
		float score = 0.0f;
		float dist = 0.0f;
		std::string name;                        // nazwa z gry (fokus)
		std::string desc;                        // opis do logu: identyfikator, zamek, skrypty, lup
	};
	static IaCand iaCand;      // najlepszy cel ostatniego skanu (plan przejmuje klawisz)
	static IaCand iaSkip;      // najlepszy pomijany cel (log przy wcisnieciu, konsola)
	static IaCand iaPending;   // decyzja z haka klawisza - start w IaUpdate
	static IaCand iaCur;       // cel przebiegu
	static zdcore::IaRun iaRun;
	static std::string iaPendingSrc;
	static bool iaOpenDeadLogged = false;

	static void IaHold(zCVob*& slot, zCVob* v) {
		if (slot == v)
			return;
		if (v)
			v->AddRef();
		if (slot)
			slot->Release();
		slot = v;
	}

	static void IaCopy(IaCand& dst, const IaCand& src) {
		IaHold(dst.vob, src.vob);
		dst.kind = src.kind;
		dst.plan = src.plan;
		dst.score = src.score;
		dst.dist = src.dist;
		dst.name = src.name;
		dst.desc = src.desc;
	}

	static void IaReset(IaCand& c) {
		IaHold(c.vob, nullptr);
		c = IaCand();
	}

	static zdcore::IaSettings IaSettingsNow() {
		zdcore::IaSettings s;
		s.doors = zdcore::IaClampDoors(zcfg.lootDoors);
		s.chests = zcfg.lootChests;
		s.corpses = zcfg.lootCorpses;
		s.redraw = zcfg.lootRedraw;
		return s;
	}

	static bool IaInWorld(zCVob* v) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		return v && world && v->homeWorld == world;
	}

	static zdcore::IaKind IaKindOf(zCVob* v) {
		if (zDYNAMIC_CAST<oCMobDoor>(v))
			return zdcore::IA_DOOR;
		if (zDYNAMIC_CAST<oCMobContainer>(v))
			return zdcore::IA_CHEST;
		if (zDYNAMIC_CAST<oCNpc>(v))
			return zdcore::IA_CORPSE;
		return zdcore::IA_NONE;
	}

	// Nazwa z gry (jak fokus): MOB - GetName (symbol focusName), NPC - GetName(0).
	static std::string IaShownName(zCVob* v, zdcore::IaKind k) {
		if (k == zdcore::IA_CORPSE) {
			oCNpc* n = zDYNAMIC_CAST<oCNpc>(v);
			return n ? n->GetName(0).ToChar() : "";
		}
		oCMOB* m = zDYNAMIC_CAST<oCMOB>(v);
		if (!m)
			return "";
		zSTRING n = m->GetName();
		return n.ToChar();
	}

	// Identyfikator do logu: MOB - nazwa voba / schemat, NPC - instancja.
	static std::string IaVobId(zCVob* v, zdcore::IaKind k) {
		char buf[200];
		if (k == zdcore::IA_CORPSE) {
			oCNpc* n = zDYNAMIC_CAST<oCNpc>(v);
			_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s", n ? n->GetInstanceName().ToChar() : "?");
			return buf;
		}
		oCMobInter* m = zDYNAMIC_CAST<oCMobInter>(v);
		if (!m)
			return "?";
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s / %s", m->GetObjectName().ToChar(), m->GetScemeName().ToChar());
		return buf;
	}

	static zdcore::IaMobInfo IaMobInfoOf(oCMobInter* mob) {
		zdcore::IaMobInfo m;
		m.door = zDYNAMIC_CAST<oCMobDoor>(mob) != nullptr;
		m.chest = !m.door && zDYNAMIC_CAST<oCMobContainer>(mob) != nullptr;
		m.destroyed = mob->isDestroyed != 0;
		m.occupied = mob->IsOccupied() != 0;
		m.condition = mob->conditionFunc.Length() > 0;
		m.stateFunc = mob->onStateFuncName.Length() > 0;
		m.useWithItem = mob->useWithItem.Length() > 0;
		m.open = mob->state_num > 0 && mob->state == mob->state_num;
		if (oCMobLockable* lk = zDYNAMIC_CAST<oCMobLockable>(mob)) {
			m.locked = lk->locked != 0;
			m.keyNeeded = lk->keyInstance.Length() > 0;
			m.pickLock = lk->pickLockStr.Length() > 0;
			if (m.locked && m.keyNeeded)
				m.hasKey = player->IsInInv(lk->keyInstance, 1) != nullptr;
			if (m.locked && m.pickLock) {
				m.hasLockpick = player->IsInInv(zSTRING("ITKE_LOCKPICK"), 1) != nullptr;
				m.pickTalent = player->HasTalent(IA_TALENT_PICKLOCK, 1) != 0;
			}
		}
		return m;
	}

	static zdcore::IaCorpseInfo IaCorpseInfoOf(oCNpc* n) {
		zdcore::IaCorpseInfo c;
		c.dead = n->IsDead() != 0;
		c.unconscious = n->IsUnconscious() != 0;
		// jak lista okna (oCNpcContainer::CreateList): bez zalozonych rzeczy (ITEM_ACTIVE) i zbroi
		c.hasLoot = (c.dead || c.unconscious) && !n->inventory2.IsEmpty(TRUE, TRUE);
		return c;
	}

	static zdcore::IaBox IaRelBox(const zTBBox3D& b, const zVEC3& hp) {
		zdcore::IaBox r;
		r.minX = b.mins.n[0] - hp.n[0];
		r.minY = b.mins.n[1] - hp.n[1];
		r.minZ = b.mins.n[2] - hp.n[2];
		r.maxX = b.maxs.n[0] - hp.n[0];
		r.maxY = b.maxs.n[1] - hp.n[1];
		r.maxZ = b.maxs.n[2] - hp.n[2];
		return r;
	}

	// Pozycje interakcji (TMobOptPos) po ScanIdealPositions - bez SearchFreePosition, ktory dla gracza wysyla
	// skrypty PLAYER_MOB_TOO_FAR_AWAY / WRONG_SIDE.
	static void IaMobPositions(oCMobInter* mob, const zVEC3& hp, std::vector<zdcore::IaPos>& out) {
		out.clear();
		mob->ScanIdealPositions();
		for (zCList<TMobOptPos>* l = mob->optimalPosList.next; l; l = l->next) {
			TMobOptPos* p = l->data;
			if (!p)
				continue;
			zVEC3 t = p->trafo.GetTranslation();
			zdcore::IaPos q;
			q.dx = t.n[0] - hp.n[0];
			q.dy = t.n[1] - hp.n[1];
			q.dz = t.n[2] - hp.n[2];
			q.free = p->npc == nullptr || p->npc == player;
			out.push_back(q);
		}
	}

	static float IaClampf(float v, float lo, float hi) {
		return v < lo ? lo : v > hi ? hi : v;
	}

	// Widocznosc: promien od piersi do najblizszego punktu obrysu (MOB) / srodka obrysu (zwloki).
	static bool IaVisible(const zVEC3& hp, zCVob* v, zdcore::IaKind k) {
		const zTBBox3D& b = v->bbox3D;
		zVEC3 to;
		if (k == zdcore::IA_CORPSE)
			to = zVEC3((b.mins.n[0] + b.maxs.n[0]) * 0.5f, (b.mins.n[1] + b.maxs.n[1]) * 0.5f + 5.0f, (b.mins.n[2] + b.maxs.n[2]) * 0.5f);
		else
			to = zVEC3(IaClampf(hp.n[0], b.mins.n[0], b.maxs.n[0]), IaClampf(hp.n[1] + 40.0f, b.mins.n[1], b.maxs.n[1]),
				IaClampf(hp.n[2], b.mins.n[2], b.maxs.n[2]));
		return LootRayClear(hp, to);
	}

	struct IaGeo {
		zCVob* vob;
		zdcore::IaKind kind;
		float score;
		float dist;
	};

	// Plan celu z danymi z silnika (zamek, lup, zasieg, widocznosc); false = pominiety (powod w desc).
	static bool IaEvaluate(const IaGeo& g, const zVEC3& hp, const zdcore::IaSettings& st, IaCand& out) {
		out.kind = g.kind;
		out.score = g.score;
		out.dist = g.dist;
		out.name = IaShownName(g.vob, g.kind);
		std::string id = IaVobId(g.vob, g.kind);
		if (g.kind == zdcore::IA_CORPSE) {
			oCNpc* n = zDYNAMIC_CAST<oCNpc>(g.vob);
			zdcore::IaCorpseInfo ci = IaCorpseInfoOf(n);
			out.plan = zdcore::IaPlanCorpse(ci, st);
			out.desc = id + (ci.dead ? ", martwy" : ", nieprzytomny") + (ci.hasLoot ? ", jest lup" : ", bez lupu");
		}
		else {
			oCMobInter* mob = zDYNAMIC_CAST<oCMobInter>(g.vob);
			zdcore::IaMobInfo mi = IaMobInfoOf(mob);
			out.plan = zdcore::IaPlanMob(mi, st);
			char st2[40];
			_snprintf_s(st2, sizeof(st2), _TRUNCATE, ", stan %d/%d", mob->state, mob->state_num);
			out.desc = id + ": " + zdcore::IaMobDesc(mi) + st2;
			if (zdcore::IaPlanActs(out.plan)) {
				std::vector<zdcore::IaPos> pos;
				IaMobPositions(mob, hp, pos);
				if (!zdcore::IaMobReachable(pos, zdcore::IA_MOB_REACH)) {
					char buf[80];
					_snprintf_s(buf, sizeof(buf), _TRUNCATE, "; za daleko od miejsca interakcji (%d pozycji, > %.0f cm)", (int)pos.size(), zdcore::IA_MOB_REACH);
					out.desc += buf;
					return false;
				}
			}
		}
		if (!zdcore::IaPlanActs(out.plan))
			return false;
		if (!IaVisible(hp, g.vob, g.kind)) {
			out.desc += "; zasloniety (sciana)";
			return false;
		}
		return true;
	}

	void IaClearCandidate() {
		IaReset(iaCand);
		IaReset(iaSkip);
	}

	void IaScan(zCArray<zCVob*>& found, const zVEC3& hp, const zVEC3& at) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !player || IaActive()) {
			IaClearCandidate();
			return;
		}
		ZPROF_SCOPE("Loot.IaScan");
		zdcore::IaSettings st = IaSettingsNow();
		zdcore::LootParams p = LootParamsNow();
		p.nearAny = 0.0f;   // MOB-y i ciala: kat zawsze (drzwi obok w przejsciu sie nie licza)
		std::vector<IaGeo> geo;
		if (st.doors > zdcore::IA_DOORS_OFF || st.chests) {
			for (int i = 0; i < found.GetNum(); i++) {
				zCVob* v = found[i];
				if (!v || v->homeWorld != world || !zDYNAMIC_CAST<oCMobInter>(v))
					continue;
				zdcore::IaKind k = IaKindOf(v);
				if ((k == zdcore::IA_DOOR && st.doors <= zdcore::IA_DOORS_OFF) || (k == zdcore::IA_CHEST && !st.chests) ||
					(k != zdcore::IA_DOOR && k != zdcore::IA_CHEST))
					continue;
				float score = 0.0f, dist = 0.0f;
				if (zdcore::IaBoxFits(p, at.n[0], at.n[2], IaRelBox(v->bbox3D, hp), score, dist))
					geo.push_back({ v, k, score, dist });
			}
		}
		if (st.corpses && world->voblist_npcs) {
			float r = p.range + 200.0f;
			for (auto it = world->voblist_npcs->next; it; it = it->next) {
				oCNpc* n = it->data;
				// homeWorld przed wszystkim innym (zdespawnowany NPC na liscie - pulapka GetModel)
				if (!n || n == player || !n->homeWorld)
					continue;
				zVEC3 np = n->GetPositionWorld();
				if (fabsf(np.n[0] - hp.n[0]) > r || fabsf(np.n[2] - hp.n[2]) > r)
					continue;
				if (!n->IsDead() && !n->IsUnconscious())
					continue;
				float score = 0.0f, dist = 0.0f;
				if (zdcore::IaBoxFits(p, at.n[0], at.n[2], IaRelBox(n->bbox3D, hp), score, dist))
					geo.push_back({ n, zdcore::IA_CORPSE, score, dist });
			}
		}
		std::stable_sort(geo.begin(), geo.end(), [](const IaGeo& a, const IaGeo& b) { return a.score < b.score; });
		IaCand best, skip;
		int checks = 0;
		for (const IaGeo& g : geo) {
			if (checks++ >= IA_MAX_CHECKS)
				break;
			IaCand c;
			if (IaEvaluate(g, hp, st, c)) {
				IaHold(c.vob, g.vob);
				best = c;
				c.vob = nullptr;
				break;
			}
			if (!skip.vob && c.plan != zdcore::IAP_SKIP_OFF) {
				IaHold(c.vob, g.vob);
				skip = c;
				c.vob = nullptr;
			}
		}
		IaCopy(iaCand, best);
		IaCopy(iaSkip, skip);
		IaReset(best);
		IaReset(skip);
	}

	bool IaCandidate(float& score) {
		if (!iaCand.vob || !IaInWorld(iaCand.vob))
			return false;
		score = iaCand.score;
		return true;
	}

	static std::string IaCandText(const IaCand& c) {
		char buf[600];
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s '%s' %.0f cm (wynik %.0f): %s [%s]", zdcore::IaKindText(c.kind), c.name.c_str(), c.dist, c.score,
			zdcore::IaPlanText(c.plan), c.desc.c_str());
		return buf;
	}

	void IaAccept(const char* src) {
		IaCopy(iaPending, iaCand);
		iaPendingSrc = src ? src : "";
	}

	void IaLogDeny(const char* src, zdcore::LootDeny d, const std::string& foeDesc) {
		if (!iaCand.vob)
			return;
		// w walce (wrog obok, cios / kombinacja trwa) kazdy cios przy ciele albo skrzyni dawalby wpis - tylko Debug=1
		if (d == zdcore::LOOT_ENEMY || d == zdcore::LOOT_ATTACK || d == zdcore::LOOT_BUSY)
			ZDebug("Interakcja z bronia: odmowa (%s) - %s%s; cel: %s; atak zostaje grze", src, zdcore::LootDenyText(d),
				foeDesc.c_str(), IaCandText(iaCand).c_str());
		else
			ZLog("Interakcja z bronia: odmowa (%s) - %s; cel: %s; atak zostaje grze", src, zdcore::LootDenyText(d),
				IaCandText(iaCand).c_str());
	}

	void IaLogSkipped(const char* src) {
		if (!iaSkip.vob || iaCand.vob)
			return;
		ZLog("Interakcja z bronia: cel pominiety (%s) - %s; atak zostaje grze", src, IaCandText(iaSkip).c_str());
	}

	bool IaActive() {
		return iaPending.vob != nullptr || zdcore::IaRunActive(iaRun);
	}

	static const char* IaWeaponName(int wm) {
		return wm == NPC_WEAPON_NONE ? "bez broni" : wm == NPC_WEAPON_FIST ? "piesci" : wm == NPC_WEAPON_DAG ? "sztylet" :
			wm == NPC_WEAPON_1HS ? "1H" : wm == NPC_WEAPON_2HS ? "2H" : "inna";
	}

	static bool IaAniActive(zCModel* m, int id) {
		if (!m || id < 0)
			return false;
		zCModelAni* a = m->GetAniFromAniID(id);
		return a && m->IsAniActive(a);
	}

	static zdcore::IaObs IaObserve(float dt) {
		zdcore::IaObs o;
		o.dt = dt;
		o.alive = !player->IsDead() && !player->IsUnconscious();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		o.standing = ac && ac->IsStanding() != 0;
		zCEventManager* em = player->GetEM(FALSE);
		o.emEmpty = !em || em->IsEmpty(TRUE) != 0;
		o.weaponMode = player->GetWeaponMode();
		oCMobInter* im = player->GetInteractMob();
		o.anyMob = im != nullptr;
		o.mobActive = im != nullptr && (zCVob*)im == iaCur.vob;
		o.invOpen = player->inventory2.IsOpen() != 0;
		o.dialog = !oCInformationManager::GetInformationManager().HasFinished();
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		o.bodyFree = bs == BS_STAND || bs == BS_WALK || bs == BS_RUN || bs == BS_SNEAK || bs == BS_SPRINT;
		// po T_..._2_STAND gra postawe bez broni (S_RUN) - z bronia w rece silnik ja gasi, my dopilnowujemy
		o.stanceWrong = o.weaponMode != NPC_WEAPON_NONE && ac && m &&
			(IaAniActive(m, ac->s_run[0]) || IaAniActive(m, ac->s_walk[0]) || IaAniActive(m, ac->s_sneak[0]));
		return o;
	}

	// metoda __thiscall bez argumentow na stosie = __fastcall (this w ECX, EDX bez znaczenia): ten sam stos i powrot
	typedef void(__fastcall* IaOpenDeadFn)(oCNpc* self, void* edx);

	// Cel wywolania OpenDeadNpc z StandActions (ten sam co gra przy zwyklym przeszukaniu).
	static IaOpenDeadFn IaOpenDeadTarget() {
		const BYTE* p = (const BYTE*)IA_OPENDEAD_CALLSITE;
		DWORD target = IA_OPENDEAD_ENGINE;
		if (p[0] == 0xE8)
			target = IA_OPENDEAD_CALLSITE + 5 + *(const int*)(p + 1);
		if (!iaOpenDeadLogged) {
			iaOpenDeadLogged = true;
			HMODULE mod = nullptr;
			char path[MAX_PATH] = "?";
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)target, &mod) && mod)
				GetModuleFileNameA(mod, path, MAX_PATH);
			const char* base = strrchr(path, '\\');
			ZLog("Interakcja z bronia: OpenDeadNpc z StandActions -> 0x%08X (%s)%s", target, base ? base + 1 : path,
				target == IA_OPENDEAD_ENGINE ? "" : " - wywolanie podmienione przez inny modul, wolamy jego cel");
		}
		return (IaOpenDeadFn)target;
	}

	static void IaLogEnd() {
		ZLog("Interakcja z bronia: %s '%s' - koniec po %.1f s: %s (bron %s)", zdcore::IaKindText(iaCur.kind), iaCur.name.c_str(),
			iaRun.t, iaRun.endReason.c_str(), player ? IaWeaponName(player->GetWeaponMode()) : "?");
		IaReset(iaCur);
	}

	// Akcja dla silnika; false + powod, gdy sie nie da.
	static bool IaPerform(zdcore::IaAction a, std::string& why) {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCEventManager* em = player->GetEM(FALSE);
		switch (a) {
		case zdcore::IAA_NONE:
		case zdcore::IAA_END:
			return true;
		case zdcore::IAA_STAND:
			if (!ac) {
				why = "brak sterowania animacja";
				return false;
			}
			ac->StopTurnAnis();
			if (!ac->IsStanding())
				ac->_Stand();
			ZDebug("Interakcja z bronia: bohater w ruchu - zatrzymanie (stan %d)", player->GetBodyState() & BS_ONLY_STATE);
			return true;
		case zdcore::IAA_REMOVE_WEAPON: {
			if (!em) {
				why = "brak kolejki komunikatow";
				return false;
			}
			// jak oCAIHuman::PC_WeaponMove na stojaco: EV_REMOVEWEAPON1 z wmode_last
			int last = player->human_ai ? player->human_ai->wmode_last : 0;
			em->OnMessage(new oCMsgWeapon(oCMsgWeapon::EV_REMOVEWEAPON1, last, 0), player);
			ZLog("Interakcja z bronia: %s '%s' - chowanie broni (%s) przed zwykla sciezka gry", zdcore::IaKindText(iaCur.kind),
				iaCur.name.c_str(), IaWeaponName(player->GetWeaponMode()));
			return true;
		}
		case zdcore::IAA_START_MOB: {
			oCMobInter* mob = IaInWorld(iaCur.vob) ? zDYNAMIC_CAST<oCMobInter>(iaCur.vob) : nullptr;
			if (!mob) {
				why = "MOB zniknal ze swiata";
				return false;
			}
			if (player->GetInteractMob()) {
				why = "bohater juz w innej interakcji";
				return false;
			}
			if (mob->IsInteractingWith(player)) {
				why = "MOB juz w interakcji z bohaterem";
				return false;
			}
			// jak StandActions: warunki silnika (zasieg / strona, zajety, przedmiot, warunek skryptu, zamek) - przy
			// odmowie gra sama wysyla skrypt PLAYER_MOB_* (komunikat)
			if (!mob->CanInteractWith(player)) {
				why = "CanInteractWith = 0 (za daleko / zla strona, zajety, zamek, przedmiot albo warunek skryptu)";
				return false;
			}
			zCEventManager* mem = mob->GetEM(FALSE);
			if (!mem) {
				why = "MOB bez kolejki komunikatow";
				return false;
			}
			*IA_USEMOB_TIMER = *IA_USEMOB_PERIOD;
			mem->OnMessage(new oCMobMsg(oCMobMsg::EV_STARTINTERACTION, player), player);
			ZLog("Interakcja z bronia: %s '%s' - start interakcji gry (%s, stan %d/%d, bron %s)", zdcore::IaKindText(iaCur.kind),
				iaCur.name.c_str(), mob->state == mob->state_num && mob->state_num > 0 ? "zamykanie" : "otwieranie", mob->state,
				mob->state_num, IaWeaponName(player->GetWeaponMode()));
			return true;
		}
		case zdcore::IAA_OPEN_CORPSE: {
			oCNpc* n = IaInWorld(iaCur.vob) ? zDYNAMIC_CAST<oCNpc>(iaCur.vob) : nullptr;
			if (!n || (!n->IsDead() && !n->IsUnconscious())) {
				why = !n ? "cialo zniknelo ze swiata" : "NPC wstal";
				return false;
			}
			int wm = player->GetWeaponMode();
			// OpenDeadNpc bierze cel z fokusu (GetFocusNpc) - jak przy zwyklym przeszukaniu
			player->SetFocusVob(n);
			IaOpenDeadTarget()(player, nullptr);
			bool open = player->inventory2.IsOpen() != 0;
			ZLog("Interakcja z bronia: zwloki '%s' - przeszukanie (bron %s): %s", iaCur.name.c_str(), IaWeaponName(wm),
				open ? (wm != NPC_WEAPON_NONE ? "okno otwarte, gra chowa bron" : "okno otwarte") :
				"okno sie nie otworzylo (puste - komunikat gry, albo inny plugin)");
			return true;
		}
		case zdcore::IAA_FIX_STANCE: {
			zCModel* m = player->GetModel();
			if (!m || !ac || ac->_s_walk < 0) {
				why = "brak modelu";
				return false;
			}
			zCModelAni* sa = m->GetAniFromAniID(ac->_s_walk);
			m->StartAni(ac->_s_walk, 0);
			ZDebug("Interakcja z bronia: po interakcji postawa bez broni - %s", sa ? sa->aniName.ToChar() : "?");
			return true;
		}
		case zdcore::IAA_DRAW_WEAPON: {
			if (!em) {
				why = "brak kolejki komunikatow";
				return false;
			}
			// jak PC_WeaponMove: na stojaco EV_DRAWWEAPON1, w ruchu EV_DRAWWEAPON - tryb sprzed (piesci / bron)
			bool standing = ac && ac->IsStanding();
			em->OnMessage(new oCMsgWeapon(standing ? oCMsgWeapon::EV_DRAWWEAPON1 : oCMsgWeapon::EV_DRAWWEAPON, iaRun.weaponBefore, 0), player);
			ZLog("Interakcja z bronia: %s '%s' - dobycie broni (%s, %s)", zdcore::IaKindText(iaCur.kind), iaCur.name.c_str(),
				IaWeaponName(iaRun.weaponBefore), standing ? "na stojaco" : "w ruchu");
			return true;
		}
		default:
			why = "nieznana akcja";
			return false;
		}
	}

	// Wykonanie akcji z obsluga odmowy silnika i konca przebiegu.
	static void IaDo(zdcore::IaAction a) {
		if (a == zdcore::IAA_NONE)
			return;
		if (a == zdcore::IAA_END) {
			IaLogEnd();
			return;
		}
		std::string why;
		if (IaPerform(a, why))
			return;
		ZLog("Interakcja z bronia: %s '%s' - %s nie wyszlo: %s", zdcore::IaKindText(iaCur.kind), iaCur.name.c_str(),
			zdcore::IaActionText(a), why.c_str());
		zdcore::IaAction b = zdcore::IaActionFailed(iaRun, a, IaSettingsNow(), why.c_str());
		if (b == zdcore::IAA_END)
			IaLogEnd();
	}

	static void IaStartPending() {
		IaCand c;
		IaCopy(c, iaPending);
		IaReset(iaPending);
		std::string src = iaPendingSrc;
		zdcore::LootDeny d = zdcore::LootHeroDeny(LootHeroNow());
		if (!IaInWorld(c.vob) || d != zdcore::LOOT_OK) {
			ZLog("Interakcja z bronia: %s '%s' - start odwolany: %s", zdcore::IaKindText(c.kind), c.name.c_str(),
				!IaInWorld(c.vob) ? "cel zniknal ze swiata" : zdcore::LootDenyText(d));
			IaReset(c);
			return;
		}
		ZLog("Interakcja z bronia: %s (%s, bron %s) - %s", IaCandText(c).c_str(), src.c_str(), IaWeaponName(player->GetWeaponMode()),
			c.plan == zdcore::IAP_ARMED ? "bez chowania broni" : c.plan == zdcore::IAP_SHEATHE ? "sciezka gry bez broni" :
			c.plan == zdcore::IAP_LOCKED_MSG ? "tylko komunikat gry" : "okno przeszukania");
		if (c.plan == zdcore::IAP_LOCKED_MSG) {
			// jak CanInteractWith -> CanOpen: skrypt PLAYER_MOB_MISSING_* (Print, T_DONTKNOW, glos), bez otwierania
			oCMobLockable* lk = zDYNAMIC_CAST<oCMobLockable>(c.vob);
			int ok = lk ? lk->CanOpen(player) : 0;
			ZLog("Interakcja z bronia: %s '%s' - zamkniete: %s", zdcore::IaKindText(c.kind), c.name.c_str(),
				ok ? "CanOpen = 1 (klucz / wytrych pojawil sie w miedzyczasie) - nastepne wcisniecie otworzy" :
				"komunikat gry, bron zostaje w rece");
			IaReset(c);
			return;
		}
		zdcore::IaObs o = IaObserve(0.0f);
		zdcore::IaAction a = zdcore::IAA_NONE;
		if (!zdcore::IaBegin(iaRun, c.kind, c.plan, o, a)) {
			ZLog("Interakcja z bronia: plan %s bez przebiegu", zdcore::IaPlanText(c.plan));
			IaReset(c);
			return;
		}
		IaCopy(iaCur, c);
		IaReset(c);
		IaDo(a);
	}

	void IaUpdate(float dt, bool allowStart) {
		if (iaPending.vob) {
			if (allowStart && player && ogame && ogame->GetGameWorld() && !zdcore::IaRunActive(iaRun)) {
				ZPROF_SCOPE("Loot.IaStart");
				IaStartPending();
			}
			else
				IaReset(iaPending);
		}
		if (!zdcore::IaRunActive(iaRun))
			return;
		if (!player || !ogame || !ogame->GetGameWorld()) {
			zdcore::IaAbort(iaRun, "brak bohatera / swiata");
			IaReset(iaCur);
			return;
		}
		ZPROF_SCOPE("Loot.IaRun");
		zdcore::IaObs o = IaObserve(dt);
		IaDo(zdcore::IaStep(iaRun, o, IaSettingsNow()));
	}

	void IaFinishNow(const char* why) {
		if (zdcore::IaRunActive(iaRun)) {
			zdcore::IaAbort(iaRun, why);
			ZLog("Interakcja z bronia: %s '%s' - przerwane (%s), faza do konca prowadzi gra", zdcore::IaKindText(iaCur.kind),
				iaCur.name.c_str(), why ? why : "?");
		}
		IaReset(iaCur);
		IaReset(iaPending);
		IaClearCandidate();
	}

	void IaDrawHint() {
		if (!iaCand.vob || !IaInWorld(iaCand.vob) || iaCand.name.empty())
			return;
		zSTRING text = iaCand.name.c_str();
		if (iaCand.plan == zdcore::IAP_LOCKED_MSG)
			text = text + " (zamkni\xeate)";
		LootPrintAbove(iaCand.vob->bbox3D, text);
	}

	std::string IaConsole() {
		std::string s = "cel interakcji: ";
		s += iaCand.vob ? IaCandText(iaCand) : "brak";
		if (iaSkip.vob)
			s += "; pominiety: " + IaCandText(iaSkip);
		char buf[300];
		zdcore::IaSettings st = IaSettingsNow();
		_snprintf_s(buf, sizeof(buf), _TRUNCATE, "; opcje: drzwi %d, skrzynie %d, ciala %d, dobycie %d; przebieg: %s", st.doors, st.chests, st.corpses,
			st.redraw, zdcore::IaRunActive(iaRun) ? zdcore::IaPhaseText(iaRun.phase) : "brak");
		s += buf;
		if (zdcore::IaRunActive(iaRun))
			s += std::string(" (") + zdcore::IaKindText(iaCur.kind) + " '" + iaCur.name + "')";
		return s;
	}
}
