// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static bool HeroHasRune() {
		static int runeIndex = -1;
		if (runeIndex < 0 && parser)
			runeIndex = parser->GetIndex(zSTRING("ITRU_TIMEREWIND"));
		return runeIndex >= 0 && player && player->IsInInv(runeIndex, 1) != nullptr;
	}

	bool DeathSaveTry() {
		if (!player)
			return false;
		// kolejne trafienie w tej samej klatce albo smierc w trakcie przewijania - cofniecie juz ruszylo
		if (RewindPending()) {
			player->attribute[NPC_ATR_HITPOINTS] = 1;
			return true;
		}
		bool hasRune = HeroHasRune();
		int mana = player->attribute[NPC_ATR_MANA];
		if (!trcore::ShouldAutoRewind(trcfg.enabled && trcfg.autoRewindOnDeath, hasRune, false, RecorderSeconds(), mana, trcfg.manaStart)) {
			if (trcfg.enabled && trcfg.autoRewindOnDeath)
				ZLog("Smierc: bez ratunku (runa %d, mana %d / %d, bufor %.1f s)", hasRune, mana, trcfg.manaStart, RecorderSeconds());
			return false;
		}
		// smierc nie nastepuje: HP 1 do startu przewijania, Commit wpisze HP z nagrania
		player->attribute[NPC_ATR_HITPOINTS] = 1;
		player->attribute[NPC_ATR_MANA] = mana - trcfg.manaStart;
		RewindRequestAuto(trcfg.autoRewindSeconds);
		if (ogame && ogame->GetTextView())
			ogame->GetTextView()->PrintTimedCXY(zSTRING("Piaski Czasu cię ocaliły."), 3000.0f, nullptr);
		ZLog("Smierc: przechwycona - auto cofniecie %.0f s, mana %d -> %d, bufor %.1f s", trcfg.autoRewindSeconds, mana,
			mana - trcfg.manaStart, RecorderSeconds());
		return true;
	}

	// zDismember ma ten sam hook (dla NPC) - detoury AS lacza sie w lancuch, DoDie potwierdzone jako dzialajace
	HOOK Ivk_oCNpc_DoDie AS(&oCNpc::DoDie, &oCNpc::DoDie_Union);
	void oCNpc::DoDie_Union(oCNpc* killer) {
		if (this == player && DeathSaveTry())
			return;
		THISCALL(Ivk_oCNpc_DoDie)(killer);
	}
}
