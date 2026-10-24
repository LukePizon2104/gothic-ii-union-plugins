// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	std::function<bool(oCNpc*, oCNpc::oSDamageDescriptor&)> zdOnBeforeHit;
	std::function<bool(oCNpc*, oCNpc*)> zdOnBeforeDeath;
	std::function<bool(oCNpc*, oCNpc*)> zdOnBeforeKnockout;
	oCNpc* zdDeathPassThrough = nullptr;

	static std::unordered_map<oCNpc*, LastHit> lastHits;

	uint32_t ZRand32() {
		return ((uint32_t)rand() << 17) ^ ((uint32_t)rand() << 4) ^ (uint32_t)rand();
	}

	const LastHit* GetLastHit(oCNpc* victim) {
		auto it = lastHits.find(victim);
		return it == lastHits.end() ? nullptr : &it->second;
	}

	bool IsFreshEdgeHit(const LastHit* h) {
		return h && h->attacker && zdFrame - h->frame <= 1 && h->edge && zdcore::WeaponCanFinish(h->cut) &&
			(h->weaponMode == NPC_WEAPON_1HS || h->weaponMode == NPC_WEAPON_2HS);
	}

	bool NpcHasEdgeWeapon(oCNpc* npc) {
		oCItem* weapon = npc ? npc->GetWeapon() : nullptr;
		return weapon && (weapon->GetDamageTypes() & oEDamageType_Edge) != 0;
	}

	// Styl walki rapierem zEBA (HUMANS_RAPIER_ST*) na modelu bohatera z dobyta bronia 1H.
	static bool RapierStyleActive(oCNpc* npc) {
		zCModel* m = npc && npc == player ? npc->GetModel() : nullptr;
		if (!m || npc->GetWeaponMode() != NPC_WEAPON_1HS)
			return false;
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && _strnicmp(p->modelProtoName.ToChar(), "HUMANS_RAPIER", 13) == 0)
				return true;
		}
		return false;
	}

	// Plik modelu broni ze skryptu (C_Item.visual), a bez niego - nazwa wizualu voba.
	static std::string WeaponVisualName(oCItem* weapon) {
		if (!weapon)
			return "";
		if (weapon->file.Length() > 0)
			return weapon->file.ToChar();
		zCVisual* v = weapon->GetVisual();
		return v ? std::string(v->GetVisualName().ToChar()) : std::string();
	}

	// damageTypes: maska obrazen ciosu (OR z bronia - obuch z dowolnej strony wylacza odcinanie).
	static zdcore::WeaponCut ItemWeaponCut(oCItem* weapon, uint32_t damageTypes, bool rapierStyle) {
		if (weapon)
			damageTypes |= (uint32_t)weapon->GetDamageTypes();
		std::string instance = weapon ? std::string(weapon->GetInstanceName().ToChar()) : std::string();
		return zdcore::ClassifyWeapon(damageTypes, instance, WeaponVisualName(weapon), zcfg.weapons, rapierStyle);
	}

	zdcore::WeaponCut NpcWeaponCut(oCNpc* npc) {
		oCItem* weapon = npc ? npc->GetWeapon() : nullptr;
		if (!weapon)
			return zdcore::WeaponCut::NoCut;
		return ItemWeaponCut(weapon, 0, RapierStyleActive(npc));
	}

	bool NpcUsesRapier(oCNpc* npc) {
		return NpcWeaponCut(npc) == zdcore::WeaponCut::ThrustOnly;
	}

	zVEC3 GibVelocity(const zVEC3& dir, int side) {
		zVEC3 right(dir.n[2], 0.0f, -dir.n[0]);
		float s = (side ? (float)side : 1.0f) * (60.0f + (float)(rand() % 120));
		float fwd = 120.0f + (float)(rand() % 120);
		return zVEC3(dir.n[0] * fwd + right.n[0] * s, 300.0f + (float)(rand() % 120), dir.n[2] * fwd + right.n[2] * s);
	}

	// Strona zamachu z aktywnej animacji ataku napastnika (T_1HATTACKL / T_2HATTACKR ...).
	static int SwingSide(oCNpc* attacker) {
		zCModel* m = attacker ? attacker->GetModel() : nullptr;
		if (!m)
			return 0;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni)
				continue;
			const char* name = a->protoAni->aniName.ToChar();
			if (strstr(name, "ATTACKL"))
				return -1;
			if (strstr(name, "ATTACKR"))
				return 1;
		}
		return 0;
	}

	// Kroniki Myrtany: ONDAMAGE_HOOK (hak Ikarusa na wejsciu OnDamage, wykonuje sie przed nami) zdejmuje ofierze ochrone
	// o wage broni bohatera (ONDAMAGEARMORPENETRATION), a ONDAMAGESCRIPT_HOOK oddaje ja w OnDamage_Script. Polkniete
	// trafienie tam nie dochodzi - oddajemy ja tak jak skrypt. Bez tej zmiennej w skryptach (Steam, NB) nic nie robi.
	static void RestoreScriptPenetration(oCNpc* victim, oCNpc::oSDamageDescriptor& desc) {
		static int sym = -2;
		if (sym == -2)
			sym = parser ? parser->GetIndex(zSTRING("ONDAMAGEARMORPENETRATION")) : -1;
		if (sym < 0 || !player || !victim || desc.pNpcAttacker != player)
			return;
		zCPar_Symbol* s = parser->GetSymbol(sym);
		int pen = 0;
		if (!s || s->type != zPAR_TYPE_INT)
			return;
		s->GetValue(pen, 0);
		if (!pen)
			return;
		oCItem* w = player->GetWeapon();
		int idx = w && w->weight > 0 ? zdcore::ScriptPenetrationProtIndex((uint32_t)w->damageTypes) : -1;
		if (idx >= 0)
			victim->protection[idx] += pen;
		s->SetValue(0, 0);
		ZDebug("Trafienie polkniete: ochrona %s [%d] +%d z powrotem (ONDAMAGEARMORPENETRATION moda)",
			victim->GetInstanceName().ToChar(), idx, pen);
	}

	// Polkniete trafienie nie moze odpalic ani obrazen, ani reakcji (animacja, efekty, dzwiek, percepcje) -
	// OnDamage wola je niezaleznie od OnDamage_Hit, wiec przechwytujemy cale OnDamage.
	HOOK Ivk_oCNpc_OnDamage AS(&oCNpc::OnDamage, &oCNpc::OnDamage_Union);
	void oCNpc::OnDamage_Union(oSDamageDescriptor& desc) {
		ZHookHit(ZH_ONDAMAGE);
		int hpXbow = attribute[NPC_ATR_HITPOINTS];
		XbowDamageGuard xbowGuard(this, desc);   // nasz belt poza oknem trafienia (wiadomosc z kolejki) - tryb kuszy udawany
		if (zcfg.enabled && zdOnBeforeHit) {
			bool swallowed;
			{
				ZPROF_SCOPE("Hit.Before");   // polykanie trafien (finisher, uniki, kontry) - bez obrazen silnika
				swallowed = zdOnBeforeHit(this, desc);
			}
			if (swallowed) {
				RestoreScriptPenetration(this, desc);
				XbowOnDamage(this, desc, hpXbow, true);
				return;
			}
		}
		int hp = attribute[NPC_ATR_HITPOINTS];
		THISCALL(Ivk_oCNpc_OnDamage)(desc);
		XbowOnDamage(this, desc, hpXbow, false);   // szybki strzal z kuszy: log trafienia beltem, trafienie bohatera
		// diagnoza krwi silnika na bohaterze (krwawi tylko trafiony smiertelny - nie cel z NPC_FLAG_IMMORTAL)
		if (zcfg.debug && this == player && attribute[NPC_ATR_HITPOINTS] != hp)
			ZDebug("Bohater trafiony: HP %d -> %d, napastnik %s", hp, attribute[NPC_ATR_HITPOINTS],
				desc.pNpcAttacker ? desc.pNpcAttacker->GetInstanceName().ToChar() : "?");
	}

	HOOK Ivk_oCNpc_OnDamage_Hit AS(&oCNpc::OnDamage_Hit, &oCNpc::OnDamage_Hit_Union);
	void oCNpc::OnDamage_Hit_Union(oSDamageDescriptor& desc) {
		ZHookHit(ZH_ONDAMAGE_HIT);
		oCNpc* att = desc.pNpcAttacker;
		if (zcfg.enabled && att) {
			ZPROF_SCOPE("Hit.Record");   // zapamietanie ciosu (bron, strona) do odciecia przy smierci
			LastHit& h = lastHits[this];
			h.attacker = att;
			h.weaponMode = att->GetWeaponMode();
			h.edge = (desc.enuModeDamage & oEDamageType_Edge) != 0 && !desc.pFXHit && !desc.nSpellID;
			// bron, ktora padl cios (z deskryptora, gdy wazna) - obuch i rapier nie odcinaja
			oCItem* weapon = (desc.dwFieldsValid & oCNpc::oEDamageDescFlag_Weapon) && desc.pItemWeapon ? desc.pItemWeapon : att->GetWeapon();
			h.cut = h.edge && (h.weaponMode == NPC_WEAPON_1HS || h.weaponMode == NPC_WEAPON_2HS)
				? ItemWeaponCut(weapon, (uint32_t)desc.enuModeDamage, RapierStyleActive(att)) : zdcore::WeaponCut::NoCut;
			h.side = SwingSide(att);
			h.frame = zdFrame;
			zVEC3 a = att->GetPositionWorld(), b = GetPositionWorld();
			float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2], l = sqrtf(dx * dx + dz * dz);
			h.dir = l > 1.0f ? zVEC3(dx / l, 0.0f, dz / l) : zVEC3(1.0f, 0.0f, 0.0f);
		}
		THISCALL(Ivk_oCNpc_OnDamage_Hit)(desc);
	}

	// Odciecie na zwlokach po zwyklej smierci od ciosu (gracz i NPC-NPC).
	static void DismemberOnDeath(oCNpc* victim, oCNpc* killer) {
		const LastHit* h = GetLastHit(victim);
		if (!IsFreshEdgeHit(h) || (killer && killer != h->attacker))
			return;
		if (!zdcore::WeaponCanCut(h->cut)) {
			ZDebug("Odciecie pominiete: %s, bron %s", victim->GetInstanceName().ToChar(), zdcore::WeaponCutName(h->cut));
			return;   // rapier przebija, obuch miazdzy - nie odcinaja
		}
		int chance = h->attacker == player ? zcfg.chancePlayer : zcfg.chanceNpc;
		if (rand() % 100 >= chance)
			return;
		int side = h->side ? h->side : ((rand() & 1) ? 1 : -1);
		zdcore::Part part = zdcore::PickPart(side, zcfg.weightsSwing, zcfg.weightsThrust, ZRand32(), PartsAvailable(victim));
		if (part != zdcore::Part::NoPart)
			CutPart(victim, part, GibVelocity(h->dir, side));
	}

	HOOK Ivk_oCNpc_DoDie AS(&oCNpc::DoDie, &oCNpc::DoDie_Union);
	void oCNpc::DoDie_Union(oCNpc* killer) {
		ZHookHit(ZH_DODIE);
		if (!zcfg.enabled) {
			THISCALL(Ivk_oCNpc_DoDie)(killer);
			return;
		}
		bool pass, repeat, taken = false;
		{
			ZPROF_SCOPE("Die.Before");   // gaszenie ognia, decyzja o finisherze (Director.Start w srodku)
			// Ogien umierajacego gasnie od razu (takze przy smierci odroczonej do ciecia finishera): plonace zwloki leza
			// tam, gdzie bohater walczy dalej, i plomienie wygladaly jak plonacy bohater.
			BurnEffectsStop(this);
			// Smierc z finishera i ponowne DoDie martwego (awaryjne z oCAniCtrl_Human::IsDead, gdy martwy stoi bez
			// animacji smierci - po pchnieciu W3 po ~1,5 s): bez losowania odciecia i bez ragdolla.
			pass = zdDeathPassThrough == this;
			repeat = zdcore::IsRepeatDeath(GetBodyState());
			if (repeat && !pass)
				ZDebug("DoDie ponownie: %s - bez odciecia i ragdolla", GetInstanceName().ToChar());
			if (!pass && !repeat)
				taken = zdOnBeforeDeath && zdOnBeforeDeath(this, killer);
		}
		if (pass || repeat) {
			THISCALL(Ivk_oCNpc_DoDie)(killer);
			return;
		}
		if (taken)
			return;
		int hpBefore = attribute[NPC_ATR_HITPOINTS];   // DoDie (0x736760) sam HP nie zmienia - wolany przy HP 0
		THISCALL(Ivk_oCNpc_DoDie)(killer);
		ZPROF_SCOPE("Die.After");   // odciecie (Cut) i start ragdolla zwlok
		if (hpBefore <= 0 && !IsDead()) {
			// smierc polknieta przez hak dalej w lancuchu (ratunek zTimeRewind: bohater z HP 1 i cofniecie, mod) - postac
			// zyje dalej, wiec bez odciecia (bohater bez glowy po cofnieciu) i bez ragdolla
			ZLog("DoDie %s: po smierci HP %d - smierc przechwycona przez inny plugin (np. ratunek zTimeRewind), bez odciecia i ragdolla",
				GetInstanceName().ToChar(), attribute[NPC_ATR_HITPOINTS]);
			return;
		}
		DismemberOnDeath(this, killer);
		CorpsesOnDeath(this, killer);   // finisher / egzekucja ida przez zdDeathPassThrough wyzej - maja wlasna animacje smierci
	}

	HOOK Ivk_oCNpc_DropUnconscious AS(&oCNpc::DropUnconscious, &oCNpc::DropUnconscious_Union);
	void oCNpc::DropUnconscious_Union(float t, oCNpc* attacker) {
		ZHookHit(ZH_DROPUNCONSCIOUS);
		if (zcfg.enabled) {
			bool taken;
			{
				ZPROF_SCOPE("Knockout");   // finisher zamiast powalenia / oszolomienie
				taken = zdOnBeforeKnockout && zdOnBeforeKnockout(this, attacker);
				if (!taken)
					DazeBeforeDrop(this, attacker);
			}
			if (taken)
				return;
		}
		THISCALL(Ivk_oCNpc_DropUnconscious)(t, attacker);
	}

	// Gracz dobija oszolomionego: w karencji nic sie nie dzieje, potem egzekucja zamiast pchniecia z gry.
	HOOK Ivk_oCNpc_EV_AttackFinish AS(&oCNpc::EV_AttackFinish, &oCNpc::EV_AttackFinish_Union);
	int oCNpc::EV_AttackFinish_Union(oCMsgAttack* msg) {
		ZHookHit(ZH_EV_ATTACKFINISH);
		static oCMsgAttack* failedMsg = nullptr;
		if (zcfg.enabled && this == player && msg && msg != failedMsg) {
			ZPROF_SCOPE("AttackFinish");   // egzekucja oszolomionego zamiast dobicia gry
			oCNpc* target = zDYNAMIC_CAST<oCNpc>(msg->target);
			if (target && IsDazed(target)) {
				if (!DazeCanExecute(target))
					return TRUE;
				// obuchem bez egzekucji (zwykle dobicie z gry), rapier - tylko pchnieciem (wybor w DirectorTryStart)
				if (zdcore::WeaponCanFinish(NpcWeaponCut(this)) && DirectorTryStart(target, zdcore::POSE_DAZED))
					return TRUE;
				failedMsg = msg;   // zwykle dobicie z gry, bez ponownych prob w kolejnych klatkach
			}
		}
		return THISCALL(Ivk_oCNpc_EV_AttackFinish)(msg);
	}

	void CombatClear() {
		lastHits.clear();
		zdDeathPassThrough = nullptr;
	}

	void CombatCollect() {
		for (auto it = lastHits.begin(); it != lastHits.end();)
			if (zdFrame - it->second.frame > 600)
				it = lastHits.erase(it);
			else
				++it;
	}
}
