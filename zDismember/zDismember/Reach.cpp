// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Doskok do celu i zasieg ciosow W3 bohatera (opis w Reach.h, logika w Core/Reach). Dolaczany po Dodge.cpp:
	// korzysta z jego narzedzi ruchu (MvTryStep, MvSlide, MoveGroundAt, MvHeroTerrain, MvTrackCandidate, FoeReady).

	// Trwajace ciecie (segment kombinacji) bohatera.
	struct ReachSwing {
		bool active = false;          // ciecie z ciosem jeszcze przed testem trafienia
		bool w3 = false;              // animacja z nakladki stylu W3
		int aniId = -1, comboNr = -1;
		zCModelAni* ani = nullptr;
		float lastAct = 0.0f;         // klatka z ostatniego kroku (nowe ciecie tej samej animacji = klatka cofnieta)
		oCNpc* target = nullptr;      // cel doskoku (AddRef)
		bool retargeted = false;      // plugin ustawil hitTarget silnika
		float surf0 = 0.0f;           // odleglosc silnika (do bboxa) na starcie
		float root0 = 0.0f;           // ruch korzenia animacji do ciosu na starcie
		float time0 = 0.0f;           // czas do ciosu na starcie
		float moved = 0.0f;           // doskok ze znakiem (cm, + do celu)
		int blocked = 0;              // klatki z krokiem zablokowanym przez przeszkode
		zdcore::ReachState st;
	};
	static ReachSwing rs;
	// Test trafienia ciosu bohatera w toku (HitCombo) - premia zasiegu i odczyt odleglosci w haku IsInFightRange.
	static bool rchTesting = false;
	static bool rchTestW3 = false;
	struct ReachProbe {
		bool called = false;
		float dist = 0.0f;
		int orig = 0;
		bool bonusUsed = false;
	};
	static ReachProbe rchProbe;
	static int rchSwingCount = 0;     // numer ciosu w logu (od startu gry)

	// Diagnostyka zaklecia broni Wojownika Adanosa (NB, 23.10.2026). NB liczy je w swoim OnDamage_Hit
	// (UNION_ABI 0x103D7150 z 22.10): cios bronia bohatera (tryb 2..4), znacznik RX_ISADANOSWARRIORWAY, wyuczony czar
	// i RX_AW_SKILL_ENCHANTTYPE 1/2/3 (ogien / lod / blyskawica). Blyskawica: mana >= 1 (koszt 1) -> dodatkowe obrazenia
	// magiczne celu chwile po ciosie (kolejka obrazen NB), blysk SPELLFX_AW_LIGHTNING tylko z 'Gniewem'
	// (RX_AW_SKILL_ENCHANTELECTRODAMAGEPROC, szansa, mana >= 5), poswiata broni tylko z RX_AW_SKILL_ENCHANTSHOWVISUAL.
	// Po trafieniu bohatera z nalozonym czarem mierzymy ubytek HP celu po ciosie (do 0.6 s albo do nastepnego testu
	// trafienia) i mane - wpis 'zaklecie broni NB ... wyzwolone / brak'. Tylko z Debug=1.
	struct ReachEnchant {
		bool active = false;
		oCNpc* target = nullptr;      // AddRef
		int swing = 0, type = 0;
		bool w3 = false;
		int hp = 0, mana = 0;         // po ciosie (przed obrazeniami NB z kolejki)
		float time = 0.0f;
	};
	static ReachEnchant rchEnch;

	static int RchParInt(const char* name, int def) {
		zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(name)) : nullptr;
		int v = def;
		if (s)
			s->GetValue(v, 0);
		return v;
	}

	static const char* RchEnchName(int t) {
		return t == 1 ? "ogien" : t == 2 ? "lod" : t == 3 ? "blyskawica" : "brak";
	}

	static void RchEnchFinish(const char* why) {
		if (!rchEnch.active)
			return;
		oCNpc* t = rchEnch.target;
		int hpNow = t ? t->attribute[NPC_ATR_HITPOINTS] : 0;
		int manaNow = player ? player->attribute[NPC_ATR_MANA] : 0;
		int extra = rchEnch.hp - hpNow;
		if (extra > 0)
			ZDebug("Cios %s #%d: zaklecie broni NB (%s) wyzwolone - dodatkowe obrazenia %d po ciosie (HP %d -> %d w %.2f s), "
				"mana %d -> %d", rchEnch.w3 ? "W3" : "gry", rchEnch.swing, RchEnchName(rchEnch.type), extra, rchEnch.hp, hpNow, rchEnch.time,
				rchEnch.mana, manaNow);
		else
			ZDebug("Cios %s #%d: zaklecie broni NB (%s) - brak dodatkowych obrazen po ciosie w %.2f s (%s; HP %d, mana %d -> %d)",
				rchEnch.w3 ? "W3" : "gry", rchEnch.swing, RchEnchName(rchEnch.type), rchEnch.time, why, hpNow, rchEnch.mana, manaNow);
		if (t)
			t->Release();
		rchEnch = ReachEnchant();
	}

	// Po trafieniu: stan warunkow NB (wpis przy zmianie) i start pomiaru dodatkowych obrazen.
	static void RchEnchStart(oCNpc* target, bool w3) {
		RchEnchFinish("nastepny cios");
		int type = RchParInt("RX_AW_SKILL_ENCHANTTYPE", 0);
		if (type < 1 || type > 3 || !target || target->IsDead())
			return;
		static const char* LEARN[4] = { "", "RX_AW_SKILL_ENCHANT", "RX_AW_SKILL_ENCHANTCOLD", "RX_AW_SKILL_ENCHANTELECTRO" };
		int learned = RchParInt(LEARN[type], 0);
		int way = RchParInt("RX_ISADANOSWARRIORWAY", 0);
		int show = RchParInt("RX_AW_SKILL_ENCHANTSHOWVISUAL", 0);
		int proc = RchParInt("RX_AW_SKILL_ENCHANTELECTRODAMAGEPROC", 0);
		int trans = RchParInt("HEROTRANS", 0);
		int mana = player->attribute[NPC_ATR_MANA];
		char state[320];
		_snprintf_s(state, sizeof(state), _TRUNCATE,
			"czar %s (wyuczony %d), znacznik AW %s, tryb broni %d (NB: 2..4), przemiana %d, mana %s (blyskawica: >= 1, koszt 1), "
			"poswiata broni %d (RX_AW_SKILL_ENCHANTSHOWVISUAL), Gniew %d (blysk SPELLFX_AW_LIGHTNING)",
			RchEnchName(type), learned, way == 987412 ? "tak" : "NIE", player->fmode, trans, mana > 0 ? "jest" : "0", show, proc);
		static std::string lastState;
		if (lastState != state) {
			lastState = state;
			ZDebug("Cios %s #%d: zaklecie broni NB - %s", w3 ? "W3" : "gry", rchSwingCount, state);
		}
		target->AddRef();
		rchEnch.active = true;
		rchEnch.target = target;
		rchEnch.swing = rchSwingCount;
		rchEnch.w3 = w3;
		rchEnch.type = type;
		rchEnch.hp = target->attribute[NPC_ATR_HITPOINTS];
		rchEnch.mana = mana;
		rchEnch.time = 0.0f;
	}

	static void RchEnchUpdate(float dt) {
		if (!rchEnch.active)
			return;
		rchEnch.time += dt;
		oCNpc* t = rchEnch.target;
		if (!player || !t || t->IsDead() || rchEnch.hp - t->attribute[NPC_ATR_HITPOINTS] > 0 || rchEnch.time >= 0.6f)
			RchEnchFinish(t && t->IsDead() ? "cel zginal" : "0.6 s");
	}

	static zdcore::ReachParams RchParams() {
		zdcore::ReachParams p;
		p.margin = zcfg.w3AssistMargin;
		p.maxPath = zcfg.w3AssistMaxPath;
		p.maxSpeed = zcfg.w3AssistSpeed;
		p.coneDeg = zcfg.w3AssistCone;
		return p;
	}

	// oCNpc::IsInFightRange(zCVob*, float&) 0x67CB60: odleglosc w poziomie od bohatera do bboxa celu (do 'dist') <=
	// FIGHT_RANGE_BASE + zasieg broni i IsSameHeight. W tescie trafienia ciosu W3 bohatera (rchTesting z haka HitCombo)
	// zasieg + W3AttackReach. Poza testem - oryginal bez zmian (AI NPC, CheckHitTarget, inne wtyczki).
	static int __fastcall RchIsInFightRange(oCNpc* self, void* edx, zCVob* target, float* dist);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, zCVob*, float*)> Ivk_oCNpc_IsInFightRange(
		ENGINE == Engine_G2A ? 0x0067CB60 : 0, &RchIsInFightRange, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall RchIsInFightRange(oCNpc* self, void* edx, zCVob* target, float* dist) {
		ZHookHit(ZH_ISINFIGHTRANGE);
		int r = Ivk_oCNpc_IsInFightRange(self, edx, target, dist);
		if (!rchTesting || !player || self != player || !target || !dist)
			return r;
		ZPROF_SCOPE("Reach.Range");   // premia zasiegu W3 w tescie trafienia bohatera
		rchProbe.called = true;
		rchProbe.dist = *dist;
		rchProbe.orig = r;
		if (!r && rchTestW3 && zcfg.w3AttackReach > 0.0f &&
			zdcore::ReachInRange(*dist, (float)self->GetFightRange(), zcfg.w3AttackReach) && self->IsSameHeight(target)) {
			rchProbe.bonusUsed = true;
			return TRUE;
		}
		return r;
	}

	// Odleglosc silnika (do bboxa celu) bez premii i bez wpisow testu.
	static bool RchSurfDist(oCNpc* target, float& dist) {
		dist = 0.0f;
		if (!player || !target)
			return false;
		Ivk_oCNpc_IsInFightRange(player, nullptr, target, &dist);
		return true;
	}

	// Animacja ciosu pochodzi z nakladki stylu W3 (najwyzsza nakladka z tym wskaznikiem w protoAnis).
	static bool RchAniIsW3(zCModel* m, zCModelAni* ani) {
		int id = ani ? ani->aniID : -1;
		for (int i = m->modelProtoList.GetNum() - 1; i >= 1 && id >= 0; i--) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && id < p->protoAnis.GetNum() && p->protoAnis[id] == ani)
				return _strnicmp(p->modelProtoName.ToChar(), "HUMANS_ZDF_W3_", 14) == 0;
		}
		return false;
	}

	// Aktywna animacja ciosu bohatera (hitAniID) i jej klatka ciosu.
	static zCModelAniActive* RchHitAni(oCAniCtrl_Human* ac, zCModel* m, int& opt) {
		if (!ac || !m || ac->hitAniID < 0 || ac->comboNr < 0 || ac->comboNr >= ANI_MAX_HIT)
			return nullptr;
		zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
		if (!a || !a->protoAni)
			return nullptr;
		opt = ac->comboInfo[ac->comboNr].hitOptFrame;
		return a;
	}

	static float RchFps(zCModel* m, zCModelAni* ani) {
		float fps = ani && ani->fpsRate > 0.0f ? ani->fpsRate : 25.0f;
		return m && m->timeScale > 0.01f ? fps * m->timeScale : fps;
	}

	// Ruch korzenia animacji do przodu (os +Z modelu = przod voba) od biezacej klatki do klatki ciosu - silnik przesunie
	// o tyle bohatera sam (flaga M).
	static float RchRootAhead(zCModelAni* ani, float act, int opt) {
		int n = ani ? (int)ani->numFrames : 0;
		if (n < 2 || ani->rootNodeIndex < 0 || ani->rootNodeIndex >= (int)ani->numNodes || !ani->aniSampleMatrix)
			return 0.0f;
		int f0 = (int)act, f1 = opt;
		f0 = f0 < 0 ? 0 : f0 > n - 1 ? n - 1 : f0;
		f1 = f1 < 0 ? 0 : f1 > n - 1 ? n - 1 : f1;
		if (f1 <= f0)
			return 0.0f;
		zVEC3 a = ani->GetTrans(f0, ani->rootNodeIndex), b = ani->GetTrans(f1, ani->rootNodeIndex);
		return b.n[2] - a.n[2];
	}

	static float RchHeroYaw() {
		zVEC3 at = player->GetAtVectorWorld();
		return atan2f(at.n[0], at.n[2]);
	}

	static void RchEnd() {
		if (rs.target)
			rs.target->Release();
		rs = ReachSwing();
	}

	void ReachReset() {
		RchEnd();
		RchEnchFinish("reset");
		rchTesting = false;
	}

	// Cel doskoku: wrogowie w poblizu (jak cel uniku: walczy z bohaterem albo wrogo nastawiony) + cel ciosu silnika
	// i fokus; wybor w Core (PickReachTarget). Cel ciosu silnika bedacy NPC, ale nie wrogiem (bohater bije neutralnego,
	// przyjaciela) - bez doskoku i bez zmiany celu.
	static oCNpc* RchPickTarget(oCAniCtrl_Human* ac, zCModelAniActive* act, int opt, float& gapOut, float& surfOut) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return nullptr;
		oCNpc* engineTarget = zDYNAMIC_CAST<oCNpc>(ac->hitTarget);
		if (engineTarget && FoeReady(engineTarget) && !MvTrackCandidate(engineTarget))
			return nullptr;   // zywy nie-wrog (neutralny, przyjaciel) - jego bohater bije swiadomie
		if (engineTarget && !FoeReady(engineTarget))
			engineTarget = nullptr;   // fokus na zwlokach / powalonym - jak bez celu
		zdcore::ReachParams p = RchParams();
		float range = (float)player->GetFightRange();
		float root = RchRootAhead(act->protoAni, act->actFrame, opt);
		zVEC3 hp = player->GetPositionWorld();
		oCNpc* focus = player->GetFocusNpc();
		float maxCenter = range + p.maxPath + p.startSlack + 250.0f;   // srodek celu: zasieg + droga + promien bboxa
		std::vector<zdcore::ReachCandidate> cands;
		std::vector<oCNpc*> npcs;
		std::vector<float> surfs;
		auto add = [&](oCNpc* npc) {
			if (!npc || std::find(npcs.begin(), npcs.end(), npc) != npcs.end() || !MvTrackCandidate(npc))
				return;
			zVEC3 np = npc->GetPositionWorld();
			if (MvFlatDist(hp, np) > maxCenter || fabsf(np.n[1] - hp.n[1]) > MOVE_NPC_LEVEL)
				return;
			float surf;
			RchSurfDist(npc, surf);
			zdcore::ReachCandidate c;
			c.x = np.n[0];
			c.z = np.n[2];
			c.gap = zdcore::ReachGap(surf, range, p.margin, root);
			c.hitTarget = npc == engineTarget;
			c.focus = npc == focus;
			cands.push_back(c);
			npcs.push_back(npc);
			surfs.push_back(surf);
		};
		add(engineTarget);
		add(focus);
		for (auto it = world->voblist_npcs->next; it; it = it->next)
			add(it->data);
		int i = zdcore::PickReachTarget(p, RchHeroYaw(), hp.n[0], hp.n[2], cands);
		if (i < 0)
			return nullptr;
		gapOut = cands[i].gap;
		surfOut = surfs[i];
		return npcs[i];
	}

	static void RchBegin(oCAniCtrl_Human* ac, zCModel* m, zCModelAniActive* act, int opt) {
		RchEnd();
		rs.active = true;
		rs.aniId = ac->hitAniID;
		rs.comboNr = ac->comboNr;
		rs.ani = act->protoAni;
		rs.lastAct = act->actFrame;
		rs.w3 = RchAniIsW3(m, act->protoAni);
		rchSwingCount++;
		// atak z okna wyjscia uniku / przewrotu (MovesAttackHandoff): ruch jeszcze wygasa, doskok juz dziala
		if (!rs.w3 || !zcfg.w3AttackAssist || (MovesActive() && !MovesAttackHandoff()) || DirectorActive())
			return;
		float gap = 0.0f, surf = 0.0f;
		oCNpc* t = RchPickTarget(ac, act, opt, gap, surf);
		if (!t)
			return;
		t->AddRef();
		rs.target = t;
		rs.surf0 = surf;
		rs.root0 = RchRootAhead(act->protoAni, act->actFrame, opt);
		rs.time0 = zdcore::ReachTimeToHit((float)opt, act->actFrame, RchFps(m, act->protoAni));
		if (ac->hitTarget != t) {
			zCVob* old = ac->hitTarget;
			ac->hitTarget = t;   // test trafienia silnika w tym cieciu - w tego wroga
			rs.retargeted = true;
			// oCAIHuman::DoAI (0x69C120) w walce wrecz bez zywego enemy bohatera co klatke ustawia hitTarget = enemy
			// (SetComboHitTarget) - z enemy = nasz cel zostaje (fokus tez sie na nim zatrzyma, jak miekki namiar W3)
			oCNpc* e = player->enemy;
			if (!e || e->IsDead() || e->IsUnconscious())
				player->SetEnemy(t);
			ZDebug("Cios W3 #%d: cel ciosu silnika %s -> %s (wrog przed bohaterem)", rchSwingCount,
				old ? old->GetObjectName().ToChar() : "brak", t->GetName(0).ToChar());
		}
		float range = (float)player->GetFightRange();
		ZDebug("Cios W3 #%d %s [%d]: doskok do %s - odleglosc %.0f (do bboxa; zasieg %.0f, idealna %.0f), ruch animacji do ciosu %+.0f, brak %.0f cm, cios za %.2f s",
			rchSwingCount, act->protoAni->aniName.ToChar(), ac->comboNr + 1, t->GetName(0).ToChar(), surf, range,
			range - zcfg.w3AssistMargin, rs.root0, gap, rs.time0);
	}

	// Krok doskoku z kolizja (slizg wzdluz sciany, teren jak przy uniku). Zwraca wykonana droge (cm, ze znakiem).
	static float RchMove(oCNpc* t, float step) {
		zVEC3 cur = player->GetPositionWorld(), tp = t->GetPositionWorld();
		float dx = tp.n[0] - cur.n[0], dz = tp.n[2] - cur.n[2], l = sqrtf(dx * dx + dz * dz);
		if (l < 1.0f)
			return 0.0f;
		float g0;
		if (!MoveGroundAt(cur, g0))
			return 0.0f;
		float yOff = cur.n[1] - g0;
		zVEC3 d(dx / l * step, 0.0f, dz / l * step), n;
		float g = g0;
		zdcore::TerrainLimits lim = MvHeroTerrain();
		MvStepResult res = MvTryStep(player, t, cur, g0, d, lim, g, n);
		if (res == MVSTEP_WALL) {
			zVEC3 slid;
			if (!MvSlide(d, n, slid))
				return 0.0f;
			d = slid;
			res = MvTryStep(player, t, cur, g0, d, lim, g, n);
		}
		if (res != MVSTEP_OK)
			return 0.0f;
		player->SetPositionWorld(zVEC3(cur.n[0] + d.n[0], g + yOff, cur.n[2] + d.n[2]));
		float done = MvFlatLen(d);
		return step < 0.0f ? -done : done;
	}

	// Ustawienia w logu po starcie i po kazdej zmianie (menu, zdism reload).
	static void RchLogSettings() {
		static float last[6] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
		float now[6] = { zcfg.w3AttackAssist ? 1.0f : 0.0f, zcfg.w3AttackReach, zcfg.w3AssistMargin, zcfg.w3AssistMaxPath,
			zcfg.w3AssistSpeed, zcfg.w3AssistCone };
		if (memcmp(last, now, sizeof(now)) == 0)
			return;
		memcpy(last, now, sizeof(now));
		ZLog("Ciosy W3: doskok do celu %s (idealna odleglosc = zasieg - %.0f cm, droga do %.0f cm, %.0f cm/s, stozek %.0f st.), "
			"premia zasiegu %.0f cm%s", zcfg.w3AttackAssist ? "wl." : "wyl.", zcfg.w3AssistMargin, zcfg.w3AssistMaxPath,
			zcfg.w3AssistSpeed, zcfg.w3AssistCone, zcfg.w3AttackReach, ENGINE == Engine_G2A ? "" : " - TYLKO G2A (haki po adresie)");
	}

	void ReachUpdate(float dt) {
		RchLogSettings();
		if (zcfg.debug)
			RchEnchUpdate(dt);
		else if (rchEnch.active)
			RchEnchFinish("Debug=0");
		if (!zcfg.enabled || !player || !ogame || !ogame->GetGameWorld() || player->IsDead() || !HeroIsHuman()) {
			if (rs.active)
				RchEnd();
			return;
		}
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		int opt = 0;
		zCModelAniActive* act = RchHitAni(ac, m, opt);
		if (!act || act->isFadingOut || !ac->comboCanHit) {
			if (rs.active && !act)
				RchEnd();   // atak przerwany / skonczony przed testem trafienia
			return;
		}
		bool fresh = !rs.active || rs.aniId != ac->hitAniID || rs.comboNr != ac->comboNr || rs.ani != act->protoAni ||
			act->actFrame + 0.5f < rs.lastAct;
		if (fresh)
			RchBegin(ac, m, act, opt);
		rs.lastAct = act->actFrame;
		oCNpc* t = rs.target;
		if (!t || !FoeReady(t) || (MovesActive() && !MovesAttackHandoff()) || DirectorActive() || TraversalActive())
			return;
		if (rs.retargeted && ac->hitTarget != t)
			ac->hitTarget = t;   // AI gry nadpisala cel ciosu (enemy) - test trafienia ma isc w cel doskoku
		float time = zdcore::ReachTimeToHit((float)opt, act->actFrame, RchFps(m, act->protoAni));
		if (time <= 0.0f)
			return;
		ZPROF_SCOPE("Reach.Assist");   // doskok: promienie kroku z kolizja
		// obrot do celu (silnik trafia tylko w +-30 st.)
		zVEC3 hp = player->GetPositionWorld(), tp = t->GetPositionWorld();
		float yaw = RchHeroYaw();
		float ny = zdcore::TurnToward(yaw, zdcore::YawTo(hp.n[0], hp.n[2], tp.n[0], tp.n[2]), RchParams().turnRate * dt);
		if (fabsf(ny - yaw) > 1e-4f)
			player->SetHeadingAtWorld(zVEC3(sinf(ny), 0.0f, cosf(ny)));
		float surf;
		RchSurfDist(t, surf);
		float range = (float)player->GetFightRange();
		float root = RchRootAhead(act->protoAni, act->actFrame, opt);
		zdcore::ReachParams p = RchParams();
		float gap = zdcore::ReachGap(surf, range, p.margin, root);
		float step = zdcore::ReachStep(p, rs.st, gap, surf - root, time, dt);
		if (fabsf(step) < 0.05f)
			return;
		float done = RchMove(t, step);
		rs.moved += done;
		if (fabsf(done) < 0.5f * fabsf(step))
			rs.blocked++;
	}

	// oCAniCtrl_Human::HitCombo(int) 0x6B0260 - co klatke ataku z obslugi komunikatu ataku (EV_AttackLeft / Right /
	// Forward / Run). Test trafienia, gdy comboCanHit i (int)actFrame >= hitOptFrame biezacego ciecia: IsInFightRange,
	// IsInFightFocus, IsSameHeight, parada celu, CreateHit. Dla ciosu bohatera: premia zasiegu W3 (hak IsInFightRange)
	// i wpis w logu z wynikiem.
	static int __fastcall RchHitCombo(oCAniCtrl_Human* self, void* edx, int combo);
	static CInvoke<int(__fastcall*)(oCAniCtrl_Human*, void*, int)> Ivk_oCAniCtrl_Human_HitCombo(
		ENGINE == Engine_G2A ? 0x006B0260 : 0, &RchHitCombo, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall RchHitCombo(oCAniCtrl_Human* self, void* edx, int combo) {
		ZHookHit(ZH_HITCOMBO);
		if (!zcfg.enabled || !player || !self || self != player->GetAnictrl() || !self->comboCanHit)
			return Ivk_oCAniCtrl_Human_HitCombo(self, edx, combo);
		zCModel* m = player->GetModel();
		int opt = 0;
		zCModelAniActive* act = RchHitAni(self, m, opt);
		if (!act || (int)act->actFrame < opt)
			return Ivk_oCAniCtrl_Human_HitCombo(self, edx, combo);
		// test trafienia w tej klatce
		ZPROF_SCOPE("Reach.Hit");   // test trafienia ciosu bohatera (oryginal + wpis w logu)
		bool w3 = RchAniIsW3(m, act->protoAni);
		std::string aniName = act->protoAni->aniName.ToChar();
		int comboNr = self->comboNr;
		RchEnchFinish("nastepny cios");   // pomiar poprzedniego ciosu - zanim ten cios zada obrazenia
		oCNpc* target = zDYNAMIC_CAST<oCNpc>(self->hitTarget);
		int hp = target ? target->attribute[NPC_ATR_HITPOINTS] : 0;
		rchProbe = ReachProbe();
		rchTesting = true;
		rchTestW3 = w3;
		int r = Ivk_oCAniCtrl_Human_HitCombo(self, edx, combo);
		rchTesting = false;
		rchTestW3 = false;
		if (zcfg.debug) {
			bool same = rs.active && rs.target == target && target;
			char buf[640];
			if (!target)
				_snprintf_s(buf, sizeof(buf), _TRUNCATE, "Cios %s #%d %s [%d]: brak celu ciosu (bez fokusu) - pudlo",
					w3 ? "W3" : "gry", rchSwingCount, aniName.c_str(), comboNr + 1);
			else {
				zVEC3 a = player->GetPositionWorld(), b = target->GetPositionWorld();
				float ang = zdcore::AngleDiff(RchHeroYaw(), zdcore::YawTo(a.n[0], a.n[2], b.n[0], b.n[2])) * 57.2958f;
				float range = (float)player->GetFightRange();
				bool hit = target->attribute[NPC_ATR_HITPOINTS] != hp || target->IsDead() || target->IsUnconscious();
				bool inRange = rchProbe.called && (rchProbe.orig || rchProbe.bonusUsed);
				const char* why = hit ? "TRAFIL" : !rchProbe.called ? "pudlo (test nie doszedl do zasiegu)" :
					!inRange ? "pudlo - poza zasiegiem" : !player->IsInFightFocus(target) ? "pudlo - poza katem 30 st." :
					!player->IsSameHeight(target) ? "pudlo - inna wysokosc" :
					NpcImmortal(target) ? "bez obrazen (niesmiertelny)" : "bez obrazen (parada celu?)";
				char assist[240] = "";
				if (same)
					_snprintf_s(assist, sizeof(assist), _TRUNCATE,
						", odleglosc na starcie %.0f, doskok %+.0f cm (ruch animacji %+.0f, zamach %.2f s%s)",
						rs.surf0, rs.moved, rs.root0, rs.time0, rs.blocked > 1 ? ", przeszkoda" : "");
				else if (w3)
					_snprintf_s(assist, sizeof(assist), _TRUNCATE, "%s", zcfg.w3AttackAssist ?
						", bez doskoku (cel poza stozkiem / za daleko / nie wrog)" : ", doskok wylaczony");
				_snprintf_s(buf, sizeof(buf), _TRUNCATE, "Cios %s #%d %s [%d]: %s - odleglosc %.0f (zasieg %.0f%s), kat %.0f st.%s -> %s%s, HP %d -> %d",
					w3 ? "W3" : "gry", rchSwingCount, aniName.c_str(), comboNr + 1, target->GetName(0).ToChar(), rchProbe.dist, range,
					w3 && zcfg.w3AttackReach > 0.0f ? (rchProbe.bonusUsed ? " +premia W3 - UZYTA" : " +premia W3") : "",
					ang, assist, why, rchProbe.bonusUsed && hit ? " dzieki premii" : "", hp, target->attribute[NPC_ATR_HITPOINTS]);
			}
			ZDebug("%s", buf);
			if (target && (target->attribute[NPC_ATR_HITPOINTS] != hp || target->IsDead()))
				RchEnchStart(target, w3);   // zaklecie broni NB: pomiar obrazen po ciosie
		}
		if (rs.active) {
			// ciecie po tescie - doskok skonczony (kolejne ciecie kombinacji to nowy segment)
			if (rs.target)
				rs.target->Release();
			rs.target = nullptr;
			rs.active = false;
		}
		return r;
	}
}
