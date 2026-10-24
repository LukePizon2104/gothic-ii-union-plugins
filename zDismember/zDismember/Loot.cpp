// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Podnoszenie przedmiotow z dobyta bronia (opis w Loot.h, logika w Core\Loot.*).
	// Silnik (G2A): atak gracza = oCAIHuman::_WalkCycle -> PC_ActionMove(GetState(GAME_ACTION)) 0x699F60 -> FightMelee
	// 0x696EC0 (piesci..2H). useGothic1Controls=1 (0x8B21C0): akcja trzymana + GetToggled(GAME_UP) = cios w przod (bez
	// kierunku tylko postawa), =0: GetToggled(GAME_ACTION) = cios. Przejecie: hak klawisza (Dodge.cpp) - od decyzji do
	// puszczenia GetState/GetToggled(GAME_ACTION) = 0, wiec FightMelee nie tworzy oCMsgAttack (nic w kolejce).
	// Podniesienie jak oCAIHuman::StandActions 0x698EA0 (bez broni, fokus): oCMsgManipulate(EV_TAKEVOB, przedmiot) do EM
	// bohatera, startObserveIntruder (_WalkCycle po powrocie do postawy: ObserveIntruder_S) i zCAICamera::SubTarget.
	// oCNpc::EV_TakeVob 0x7534E0 nie patrzy na tryb broni: czeka na postawe _s_walk (z bronia S_1HRUN / S_2HRUN, wersja
	// z najwyzszej nakladki - IsStateAniActive), gra T_STAND_2_IGET (wysokosc z przedmiotu), w S_IGET oCNpc::DoTakeVob
	// 0x7449C0 (ze swiata, PutInInv, skrypty, percepcja kradziezy), T_IGET_2_STAND z next = _s_walk (garda stylu, W3 z
	// nakladki). Bron zostaje w ZS_RIGHTHAND (animacja calego ciala, dlon z bronia schodzi do przedmiotu). Kolejka
	// komunikatow niepusta = oCAIHuman::DoAI nie czyta sterowania (oCAIHuman::Moving tylko przy IsEmpty).
	static const float LOOT_CONE = 45.0f;            // +- st. od przodu bohatera
	static const float LOOT_TURN_RATE = 5.0f;        // rad/s obrotu do przedmiotu przy schylaniu
	static const float LOOT_KICK_TIME = 0.4f;        // tyle s bez animacji podnoszenia: postawa broni z wierzchu nakladek
	static const float LOOT_GIVEUP_TIME = 1.5f;      // tyle s bez animacji podnoszenia: komunikat usuniety
	static const float LOOT_MAX_TIME = 6.0f;         // bezpiecznik sledzenia
	static const float LOOT_ENEMY_RECHECK = 0.2f;    // s miedzy przegladami wrogow dla podpowiedzi
	static const int LOOT_MAX_TRACES = 4;            // promieni widocznosci na klatke (kandydaci od najlepszego)
	static const DWORD LOOT_HOOK_STALE_MS = 250;     // hak klawisza bez petli gry dluzej (menu, pauza) - bez decyzji
	static const int LOOT_TRACE = zTRACERAY_STAT_POLY | zTRACERAY_VOB_IGNORE | zTRACERAY_POLY_IGNORE_TRANSP;

	struct LootRun {
		oCItem* item = nullptr;            // AddRef do konca sledzenia
		zCEventMessage* msg = nullptr;     // tylko porownanie z kolejka EM (bez odwolan po zniknieciu z kolejki)
		float t = 0.0f;
		bool started = false;              // animacja podnoszenia silnika ruszyla
		bool kicked = false;
		bool deleted = false;
		bool turning = true;
		std::string name;
		int instance = -1;
		int invBefore = 0;
	};
	static LootRun lr;
	static oCItem* lootPending = nullptr;   // decyzja z haka klawisza - start w LootUpdate (poza AI silnika), AddRef
	static oCItem* lootCand = nullptr;      // kandydat z ostatniej klatki (AddRef)
	static float lootCandDist = 0.0f, lootCandAngle = 0.0f, lootCandScore = 0.0f;
	static zdcore::LootKeyLatch lootLatch;
	static bool lootRawRead = false;        // odczyt klawisza przez plugin - bez naszej podmiany
	static DWORD lootLoopTick = 0;
	static float lootEnemyAge = 1e9f;
	static bool lootEnemyNear = false;
	static int lootTorchIdx = -2;           // indeks ITLSTORCHBURNING w parserze (-2 = nie szukany)

	static void LootHold(oCItem*& slot, oCItem* v) {
		if (slot == v)
			return;
		if (v)
			v->AddRef();
		if (slot)
			slot->Release();
		slot = v;
	}

	zdcore::LootParams LootParamsNow() {
		zdcore::LootParams p;
		p.range = zcfg.lootRange;
		p.coneDeg = LOOT_CONE;
		return p;
	}

	// Stan klawisza ataku po decyzji ruchow (Dodge), bez naszej podmiany.
	static bool LootRawDown() {
		if (!zinput)
			return false;
		lootRawRead = true;
		bool down = zinput->GetState(GAME_ACTION) > 0.0f;
		lootRawRead = false;
		return down;
	}

	static bool LootMelee(int wm) {
		return wm == NPC_WEAPON_FIST || wm == NPC_WEAPON_DAG || wm == NPC_WEAPON_1HS || wm == NPC_WEAPON_2HS;
	}

	static const char* LootWeaponName(int wm) {
		return wm == NPC_WEAPON_FIST ? "piesci" : wm == NPC_WEAPON_DAG ? "sztylet" : wm == NPC_WEAPON_1HS ? "1H" :
			wm == NPC_WEAPON_2HS ? "2H" : "inna";
	}

	// Aktywna animacja ciosu (hitAniID) - jak HeroAttacking w Styles.cpp (IsInPostHit zwraca 1 takze bez ciosu).
	static bool LootHeroAttacking() {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		if (!ac || !m || ac->hitAniID < 0)
			return false;
		zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
		return a && !a->isFadingOut;
	}

	zdcore::LootHeroState LootHeroNow() {
		zdcore::LootHeroState s;
		s.enabled = zcfg.enabled && zcfg.lootArmed;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !world || player->IsDead() || player->IsUnconscious() || !player->GetModel() || !player->GetAnictrl()) {
			s.melee = false;
			return s;
		}
		s.human = HeroIsHuman();
		s.melee = LootMelee(player->GetWeaponMode());
		s.window = (zcon && zcon->IsVisible()) || player->inventory2.IsOpen() ||
			!oCInformationManager::GetInformationManager().HasFinished();
		s.movLock = player->IsMovLock() != 0;
		s.pluginMove = MovesActive() || TraversalActive() || DirectorActive();
		s.attacking = LootHeroAttacking();
		zCEventManager* em = player->GetEM(FALSE);
		s.busy = em && !em->IsEmpty(TRUE);
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		s.bodyOk = bs == BS_STAND || bs == BS_WALK || bs == BS_RUN || bs == BS_SNEAK;
		s.water = player->GetAnictrl()->GetWaterLevel() > 0;
		return s;
	}

	oCNpc* FindFightingFoe(float r, oCNpc* skip, float& distOut) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (r <= 0.0f || !player || !world || !world->voblist_npcs)
			return nullptr;
		zVEC3 hp = player->GetPositionWorld();
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || npc == skip || !npc->homeWorld)
				continue;
			zVEC3 d = npc->GetPositionWorld() - hp;
			float dist = d.Length();
			if (dist > r)
				continue;
			zdcore::LootFoe f;
			f.dist = dist;
			f.alive = !npc->IsDead();
			f.conscious = !npc->IsUnconscious();
			f.targetsHero = npc->enemy == player;
			f.heroTargets = player->enemy == npc;
			int att = npc->GetAttitude(player);
			f.hostile = att == NPC_ATT_HOSTILE || att == NPC_ATT_ANGRY;
			f.fighting = npc->GetWeaponMode() != NPC_WEAPON_NONE;
			if (zdcore::LootFoeBlocks(f, r)) {
				distOut = dist;
				return npc;
			}
		}
		return nullptr;
	}

	// Walczacy wrog w promieniu LootEnemyRadius.
	static oCNpc* LootFindEnemy(float& distOut) {
		return FindFightingFoe(zcfg.lootEnemyRadius, nullptr, distOut);
	}

	// Plonaca pochodnia: DoTakeVob wklada ja do lewej reki (oCNpc::SetTorchAni) - z bronia w reku nie bierzemy.
	static bool LootIsBurningTorch(oCItem* item) {
		if (lootTorchIdx == -2)
			lootTorchIdx = parser ? parser->GetIndex("ITLSTORCHBURNING") : -1;
		return lootTorchIdx >= 0 && item->GetInstance() == lootTorchIdx;
	}

	// Jak zwykle podniesienie: w swiecie, nie leci, bez ITM_FLAG_NFOCUS (fokus gry go nie lapie), nie plonaca pochodnia,
	// nie w gniezdzie oCMobItemSlot (StandActions daje wtedy interakcje z mobem). Udzwig (CanCarry) i widocznosc - pozniej,
	// tylko dla najlepszych.
	static bool LootTakeable(oCItem* item, oCWorld* world, const std::vector<oCItem*>& slotted) {
		if (!item || item->homeWorld != world || item->HasFlag(ITM_FLAG_NFOCUS))
			return false;
		if (item->physicsEnabled && item->rigidBody && item->rigidBody->v.Length() > 30.0f)
			return false;   // spada / leci
		if (LootIsBurningTorch(item))
			return false;
		for (oCItem* s : slotted)
			if (s == item)
				return false;
		return true;
	}

	// Promien od piersi bohatera do punktu (bez ostatnich 15 cm - przedmiot w zaglebieniu ziemi, punkt na obrysie MOB-a),
	// tylko statyczny swiat bez przezroczystych poly (sciany, skaly; stoly, skrzynie i drzwi - voby - nie zaslaniaja).
	bool LootRayClear(const zVEC3& hp, const zVEC3& to) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world)
			return false;
		zVEC3 from(hp.n[0], hp.n[1] + 40.0f, hp.n[2]);
		zVEC3 ray = to - from;
		float len = ray.Length();
		if (len < 20.0f)
			return true;
		float k = (len - 15.0f) / len;
		ray = zVEC3(ray.n[0] * k, ray.n[1] * k, ray.n[2] * k);
		if (!world->TraceRayNearestHit(from, ray, (zCVob*)nullptr, LOOT_TRACE))
			return true;
		return !world->traceRayReport.foundHit;
	}

	static bool LootVisible(const zVEC3& hp, oCItem* item) {
		const zTBBox3D& b = item->bbox3D;
		zVEC3 to((b.mins.n[0] + b.maxs.n[0]) * 0.5f, (b.mins.n[1] + b.maxs.n[1]) * 0.5f + 5.0f, (b.mins.n[2] + b.maxs.n[2]) * 0.5f);
		return LootRayClear(hp, to);
	}

	// Voby w bboxie wokol bohatera (zasieg + 50 cm, wysokosc podnoszenia) - raz na klatke dla przedmiotow i MOB-ow.
	static bool LootCollect(zCArray<zCVob*>& found, zVEC3& hp, zVEC3& at) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !world || !world->bspTree.bspRoot)
			return false;
		zdcore::LootParams p = LootParamsNow();
		hp = player->GetPositionWorld();
		at = player->GetAtVectorWorld();
		float r = p.range + 50.0f;
		zTBBox3D box;
		box.mins = zVEC3(hp.n[0] - r, hp.n[1] + p.minDy - 40.0f, hp.n[2] - r);
		box.maxs = zVEC3(hp.n[0] + r, hp.n[1] + p.maxDy + 40.0f, hp.n[2] + r);
		world->bspTree.bspRoot->CollectVobsInBBox3D(found, box);
		return true;
	}

	// Najlepszy przedmiot przed bohaterem (zdcore::LootRankItems, potem udzwig i widocznosc); scoreOut jak w Core
	// (porownanie z celem interakcji).
	static oCItem* LootFindCandidate(zCArray<zCVob*>& found, const zVEC3& hp, const zVEC3& at, float& distOut, float& angOut,
		float& scoreOut) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !world)
			return nullptr;
		zdcore::LootParams p = LootParamsNow();
		std::vector<oCItem*> items, slotted;
		std::vector<zdcore::LootItem> geo;
		for (int i = 0; i < found.GetNum(); i++) {
			zCVob* v = found[i];
			if (oCMobItemSlot* slot = zDYNAMIC_CAST<oCMobItemSlot>(v)) {
				if (oCItem* in = slot->GetInsertedItem())
					slotted.push_back(in);
				continue;
			}
			oCItem* item = zDYNAMIC_CAST<oCItem>(v);
			if (!item)
				continue;
			zVEC3 ip = item->GetPositionWorld();
			zdcore::LootItem g;
			g.dx = ip.n[0] - hp.n[0];
			g.dy = ip.n[1] - hp.n[1];
			g.dz = ip.n[2] - hp.n[2];
			items.push_back(item);
			geo.push_back(g);
		}
		for (size_t i = 0; i < items.size(); i++)
			geo[i].takeable = LootTakeable(items[i], world, slotted);
		std::vector<int> order = zdcore::LootRankItems(p, at.n[0], at.n[2], geo);
		int traces = 0;
		for (int idx : order) {
			oCItem* item = items[idx];
			if (!player->CanCarry(item))
				continue;
			if (traces++ >= LOOT_MAX_TRACES)
				break;
			if (!LootVisible(hp, item))
				continue;
			const zdcore::LootItem& g = geo[idx];
			distOut = sqrtf(g.dx * g.dx + g.dz * g.dz);
			angOut = fabsf(zdcore::AngleDiff(atan2f(at.n[0], at.n[2]), atan2f(g.dx, g.dz))) * 57.2958f;
			scoreOut = 0.0f;
			zdcore::LootItemFits(p, at.n[0], at.n[2], g.dx, g.dy, g.dz, scoreOut);
			return item;
		}
		return nullptr;
	}

	static std::string LootItemDesc(oCItem* item) {
		if (!item)
			return "brak";
		char buf[200];
		sprintf_s(buf, "%s (%s x%d)", item->name.ToChar(), item->GetInstanceName().ToChar(), item->amount);
		return buf;
	}

	static std::string LootFoeDesc(oCNpc* npc, float dist) {
		if (!npc)
			return "";
		char buf[200];
		sprintf_s(buf, ": %s %.0f cm (enemy=bohater %d, nastawienie %d, tryb broni %d)", npc->GetInstanceName().ToChar(), dist,
			npc->enemy == player, npc->GetAttitude(player), npc->GetWeaponMode());
		return buf;
	}

	// Rozstrzygniecie wcisniecia: true = nasze (podniesienie albo interakcja, silnik nie widzi klawisza do puszczenia).
	// Wolane z haka klawisza (w trakcie AI silnika) albo z petli - bez promieni i zbierania vobow (kandydaci z ostatniej
	// klatki). Przedmiot i cel interakcji (drzwi, skrzynia, zwloki): wygrywa mniejszy wynik, przy remisie przedmiot.
	static bool LootDecideNow(const char* src) {
		zdcore::LootDeny heroDeny = zdcore::LootHeroDeny(LootHeroNow());
		if (zdcore::LootDenySilent(heroDeny))
			return false;
		if (IaActive())
			return false;   // trwa nasza interakcja (np. dobycie broni po oknie) - wcisniecie zostaje grze
		oCWorld* world = ogame->GetGameWorld();
		oCItem* item = lootCand && lootCand->homeWorld == world ? lootCand : nullptr;
		float iaScore = 0.0f;
		bool ia = IaCandidate(iaScore);
		bool useIa = zdcore::IaTargetBeatsItem(item != nullptr, lootCandScore, ia, iaScore);
		float foeDist = 0.0f;
		oCNpc* foe = LootFindEnemy(foeDist);
		zdcore::LootDeny d = zdcore::LootDecide(heroDeny, foe != nullptr, item != nullptr || ia);
		if (d != zdcore::LOOT_OK) {
			if (zdcore::LootLogDeny(d, item != nullptr || ia, foe != nullptr)) {
				if (useIa)
					IaLogDeny(src, d, LootFoeDesc(foe, foeDist));
				else
					ZDebug("Podnoszenie: odmowa (%s) - %s%s; przedmiot: %s; atak zostaje grze", src, zdcore::LootDenyText(d),
						LootFoeDesc(foe, foeDist).c_str(), LootItemDesc(item).c_str());
			}
			// cel pominiety (np. zamek przy LootArmedDoors=1, puste cialo) - wpis, gdy nic innego nie przejelo klawisza
			if (d == zdcore::LOOT_NO_ITEM)
				IaLogSkipped(src);
			return false;
		}
		if (useIa) {
			IaAccept(src);
			return true;
		}
		LootHold(lootPending, item);
		return true;
	}

	int LootGameKeyOverride(unsigned short logicalKey, bool toggled) {
		if (logicalKey != GAME_ACTION || lootRawRead || !zcfg.enabled || !zcfg.lootArmed)
			return -1;
		ZPROF_SCOPE("Loot.Keys");
		bool down = LootRawDown();
		if (zdcore::LootLatchNeedsDecision(lootLatch, down)) {
			// hak wolany tez z menu / pauzy (petla gry stoi) - wtedy wcisniecie zostaje grze
			bool inGame = lootLoopTick && GetTickCount() - lootLoopTick <= LOOT_HOOK_STALE_MS;
			zdcore::LootLatchSet(lootLatch, inGame && LootDecideNow(toggled ? "hak GetToggled" : "hak GetState"));
		}
		return zdcore::LootLatchSwallow(lootLatch, down) ? 0 : -1;
	}

	bool LootActive() {
		return lr.item != nullptr || lootPending != nullptr || IaActive();
	}

	static void LootStart(oCItem* item) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zdcore::LootDeny d = zdcore::LootHeroDeny(LootHeroNow());
		if (!world || !item || item->homeWorld != world || d != zdcore::LOOT_OK) {
			ZDebug("Podnoszenie: start odwolany - %s", !world || !item || item->homeWorld != world ? "przedmiot zniknal ze swiata" :
				zdcore::LootDenyText(d));
			return;
		}
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCEventManager* em = player->GetEM(FALSE);
		if (!ac || !em)
			return;
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		bool standing = ac->IsStanding() != 0;
		// EV_TakeVob czeka na sama postawe _s_walk na warstwie 1 - obrot i bieg trzeba zgasic (sterowanie stoi, gdy
		// kolejka niepusta, wiec gra sama by ich nie zatrzymala)
		ac->StopTurnAnis();
		if (!standing)
			ac->_Stand();
		oCMsgManipulate* msg = new oCMsgManipulate(oCMsgManipulate::EV_TAKEVOB, item);
		em->OnMessage(msg, player);
		if (player->human_ai)
			player->human_ai->startObserveIntruder = TRUE;
		if (zCAICamera* cam = zCAICamera::GetCurrent())
			cam->SubTarget(item);
		LootHold(lr.item, item);
		lr.msg = msg;
		lr.t = 0.0f;
		lr.started = lr.kicked = lr.deleted = false;
		lr.turning = true;
		lr.name = item->name.ToChar();
		lr.instance = item->GetInstance();
		lr.invBefore = player->inventory2.GetAmount(lr.instance);
		zVEC3 ip = item->GetPositionWorld(), hp = player->GetPositionWorld();
		ZDebug("Podnoszenie: start %s - %.0f cm, %.0f st., wysokosc %+.0f cm, bron %s, %s (stan %d)", LootItemDesc(item).c_str(),
			lootCandDist, lootCandAngle, ip.n[1] - hp.n[1], LootWeaponName(player->GetWeaponMode()), standing ? "stoi" : "w ruchu - _Stand",
			bs);
	}

	static bool LootAniActive(zCModel* m, int id) {
		if (!m || id < 0)
			return false;
		zCModelAni* a = m->GetAniFromAniID(id);
		return a && m->IsAniActive(a);
	}

	static void LootRunEnd() {
		LootHold(lr.item, nullptr);
		lr = LootRun();
	}

	static bool LootEmHas(zCEventManager* em, zCEventMessage* msg) {
		if (!em || !msg)
			return false;
		for (int i = 0; i < em->GetNumMessages(); i++)
			if (em->GetEventMessage(i) == msg)
				return true;
		return false;
	}

	// Sledzenie zleconego podniesienia: start animacji silnika, obrot do przedmiotu, wynik (log), ratunek gdy postawa
	// broni nie jest z najwyzszej nakladki (EV_TakeVob czekalby bez konca).
	static void LootRunUpdate(float dt) {
		if (!lr.item)
			return;
		ZPROF_SCOPE("Loot.Run");
		lr.t += dt;
		if (!player || !ogame || !ogame->GetGameWorld()) {
			LootRunEnd();
			return;
		}
		zCEventManager* em = player->GetEM(FALSE);
		zCModel* m = player->GetModel();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		if (!lr.started && ac && (LootAniActive(m, ac->t_stand_2_iget) || LootAniActive(m, ac->s_iget))) {
			lr.started = true;
			ZDebug("Podnoszenie: animacja podnoszenia po %.2f s", lr.t);
		}
		if (!LootEmHas(em, lr.msg)) {
			int now = player->inventory2.GetAmount(lr.instance);
			if (!lr.item->homeWorld)
				ZDebug("Podnoszenie: %s w ekwipunku po %.2f s (w ekwipunku %d -> %d), bron %s, postawa %s", lr.name.c_str(), lr.t,
					lr.invBefore, now, LootWeaponName(player->GetWeaponMode()),
					m && ac && ac->_s_walk >= 0 && m->GetAniFromAniID(ac->_s_walk) ? m->GetAniFromAniID(ac->_s_walk)->aniName.ToChar() : "?");
			else
				ZDebug("Podnoszenie: przerwane po %.2f s - %s zostal w swiecie (%s)", lr.t, lr.name.c_str(),
					lr.deleted ? "animacja nie ruszyla" : lr.started ? "animacje przerwala gra" : "komunikat usuniety przez gre");
			LootRunEnd();
			return;
		}
		// obrot do przedmiotu w trakcie schylania (stozek +-45 st. - bez skoku kamery)
		if (lr.turning && lr.item->homeWorld) {
			zVEC3 hp = player->GetPositionWorld(), ip = lr.item->GetPositionWorld(), at = player->GetAtVectorWorld();
			float dx = ip.n[0] - hp.n[0], dz = ip.n[2] - hp.n[2];
			if (dx * dx + dz * dz < 25.0f * 25.0f)
				lr.turning = false;
			else {
				float yaw = atan2f(at.n[0], at.n[2]), target = zdcore::YawTo(hp.n[0], hp.n[2], ip.n[0], ip.n[2]);
				float ny = zdcore::TurnToward(yaw, target, LOOT_TURN_RATE * dt);
				player->SetHeadingAtWorld(zVEC3(sinf(ny), 0.0f, cosf(ny)));
				if (fabsf(zdcore::AngleDiff(ny, target)) < 0.01f)
					lr.turning = false;
			}
		}
		if (!lr.started && ac && m) {
			if (!lr.kicked && lr.t >= LOOT_KICK_TIME && ac->_s_walk >= 0 && !ac->IsStateAniActive(ac->_s_walk)) {
				lr.kicked = true;
				zCModelAni* a = m->GetAniFromAniID(ac->_s_walk);
				m->StartAni(ac->_s_walk, 0);
				ZDebug("Podnoszenie: po %.2f s bez animacji podnoszenia - postawa %s z najwyzszej nakladki (stan %d)", lr.t,
					a ? a->aniName.ToChar() : "?", player->GetBodyState() & BS_ONLY_STATE);
			}
			if (!lr.deleted && lr.t >= LOOT_GIVEUP_TIME) {
				lr.deleted = true;
				lr.msg->Delete();
				ZDebug("Podnoszenie: animacja nie ruszyla w %.1f s - komunikat usuniety (stan %d)", LOOT_GIVEUP_TIME,
					player->GetBodyState() & BS_ONLY_STATE);
			}
		}
		if (lr.t >= LOOT_MAX_TIME) {
			ZDebug("Podnoszenie: bezpiecznik %.0f s - koniec sledzenia (komunikat dalej w kolejce)", LOOT_MAX_TIME);
			LootRunEnd();
		}
	}

	// Kandydaci co klatke (z bronia biala / piesciami w rece): przedmiot i cel interakcji z jednego zbioru vobow, wrog dla
	// podpowiedzi co LOOT_ENEMY_RECHECK s.
	static void LootRefresh(float dt) {
		zdcore::LootHeroState hs = LootHeroNow();
		if (!hs.enabled || !hs.human || !hs.melee || hs.window || lr.item || IaActive()) {
			LootHold(lootCand, nullptr);
			IaClearCandidate();
			lootEnemyAge = 1e9f;
			return;
		}
		{
			ZPROF_SCOPE("Loot.Scan");
			zCArray<zCVob*> found;
			zVEC3 hp, at;
			oCItem* c = nullptr;
			float dist = 0.0f, ang = 0.0f, score = 0.0f;
			if (LootCollect(found, hp, at)) {
				c = LootFindCandidate(found, hp, at, dist, ang, score);
				IaScan(found, hp, at);
			}
			else
				IaClearCandidate();
			LootHold(lootCand, c);
			lootCandDist = dist;
			lootCandAngle = ang;
			lootCandScore = score;
		}
		lootEnemyAge += dt;
		if (lootEnemyAge >= LOOT_ENEMY_RECHECK) {
			lootEnemyAge = 0.0f;
			float d = 0.0f;
			lootEnemyNear = LootFindEnemy(d) != nullptr;
		}
	}

	// Podpowiedz: nazwa przedmiotu (jak fokus gry) albo celu interakcji nad nim, gdy wcisniecie ataku teraz by go wzielo.
	static void LootDrawHint() {
		if (!zcfg.lootHint || lr.item || lootPending || IaActive() || lootEnemyNear || !screen || !zCCamera::activeCam)
			return;
		float iaScore = 0.0f;
		bool ia = IaCandidate(iaScore);
		if (!lootCand && !ia)
			return;
		if (zdcore::LootHeroDeny(LootHeroNow()) != zdcore::LOOT_OK)
			return;
		ZPROF_SCOPE("Loot.Hint");
		if (zdcore::IaTargetBeatsItem(lootCand != nullptr, lootCandScore, ia, iaScore)) {
			IaDrawHint();
			return;
		}
		zSTRING text = lootCand->name;
		if (text.Length() == 0)
			return;
		LootPrintAbove(lootCand->bbox3D, text);
	}

	void LootPrintAbove(const zTBBox3D& b, const zSTRING& textIn) {
		zSTRING text = textIn;   // zCView::FontSize / Print biora zSTRING&
		if (!screen || !zCCamera::activeCam || text.Length() == 0)
			return;
		zCCamera* cam = zCCamera::activeCam;
		zVEC3 top((b.mins.n[0] + b.maxs.n[0]) * 0.5f, b.maxs.n[1] + 15.0f, (b.mins.n[2] + b.maxs.n[2]) * 0.5f);
		cam->Activate();
		zVEC3 v = cam->camMatrix * top;
		if (v.n[2] < 10.0f)
			return;   // za kamera
		float px = 0.0f, py = 0.0f;
		cam->Project(&v, px, py);
		int w = screen->FontSize(text), h = screen->FontY();
		int x = screen->anx((int)px) - w / 2, y = screen->any((int)py) - h;
		if (x < 0 || y < 0 || x + w > 8191 || y + h > 8191)
			return;
		screen->Print(x, y, text);
	}

	void LootUpdate(float dt) {
		lootLoopTick = GetTickCount();
		if (!zcfg.lootArmed) {
			LootHold(lootCand, nullptr);
			LootHold(lootPending, nullptr);
			IaClearCandidate();
			lootLatch.state = 0;
			IaUpdate(dt, false);   // trwajaca interakcja (np. dobycie broni) do konca, bez nowych
			LootRunUpdate(dt);
			return;
		}
		// zatrzask tez z petli (gdy silnik w tej klatce nie czytal klawisza, np. kolejka bohatera zajeta)
		bool down = LootRawDown();
		if (zdcore::LootLatchNeedsDecision(lootLatch, down))
			zdcore::LootLatchSet(lootLatch, LootDecideNow("petla"));
		if (lootPending) {
			ZPROF_SCOPE("Loot.Start");
			oCItem* it = lootPending;
			it->AddRef();
			LootHold(lootPending, nullptr);
			LootStart(it);
			it->Release();
		}
		IaUpdate(dt, true);    // drzwi, skrzynie, zwloki (LootInteract.cpp): start zleconej i przebieg
		LootRunUpdate(dt);
		LootRefresh(dt);
		LootDrawHint();
	}

	void LootFinishNow() {
		LootHold(lootCand, nullptr);
		LootHold(lootPending, nullptr);
		LootRunEnd();
		IaFinishNow("zapis / wczytanie / zmiana swiata / zdism reload");
		lootLatch.state = 0;
		lootEnemyAge = 1e9f;
		lootEnemyNear = false;
	}

	std::string LootConsole(const std::string& arg) {
		if (!player || !ogame || !ogame->GetGameWorld())
			return "brak bohatera";
		zdcore::LootDeny hd = zdcore::LootHeroDeny(LootHeroNow());
		float foeDist = 0.0f, dist = 0.0f, ang = 0.0f, score = 0.0f;
		oCNpc* foe = LootFindEnemy(foeDist);
		zCArray<zCVob*> found;
		zVEC3 hp, at;
		oCItem* item = nullptr;
		if (LootCollect(found, hp, at)) {
			item = LootFindCandidate(found, hp, at, dist, ang, score);
			if (!IaActive())
				IaScan(found, hp, at);   // konsola zamknieta w tej klatce - swiezy cel interakcji
		}
		float iaScore = 0.0f;
		bool ia = IaCandidate(iaScore);
		bool useIa = zdcore::IaTargetBeatsItem(item != nullptr, score, ia, iaScore);
		zdcore::LootDeny d = zdcore::LootDecide(hd, foe != nullptr, item != nullptr || ia);
		char buf[700];
		sprintf_s(buf, "podnoszenie %s (zasieg %.0f cm, wrogowie %.0f cm, podpowiedz %d): %s; przedmiot: %s%s%s; wrog: %s",
			zcfg.lootArmed ? "wl." : "wyl.", zcfg.lootRange, zcfg.lootEnemyRadius, zcfg.lootHint,
			d != zdcore::LOOT_OK ? zdcore::LootDenyText(d) : useIa ? "atak uzyje celu interakcji" : "atak podniesie",
			LootItemDesc(item).c_str(), item ? " " : "",
			item ? (std::to_string((int)dist) + " cm " + std::to_string((int)ang) + " st.").c_str() : "",
			foe ? LootFoeDesc(foe, foeDist).c_str() + 2 : "brak");
		std::string s = std::string(buf) + "; " + IaConsole();
		ZLog("Podnoszenie (konsola): %s", s.c_str());
		return s;
	}
}
