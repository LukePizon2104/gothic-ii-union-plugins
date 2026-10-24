// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static const int TR_SPL_SENDSTOP = 3;   // Constants.d: SPL_SENDSTOP
	static bool waitRelease = false;

	// Tablice SPELLFX*/TXT_SPELLS maja MAX_SPELL elementow (Steam 100, New Balance 196): ID poza nimi (NB: 201)
	// wydluza wspolny helper _shared\ZSpellSlots.h (w pamieci, nie trafia do zapisu gry).
	void SpellPatchSlot() {
		std::vector<std::string> log;
		bool ok = zslots::SetSpellSlot(trcfg.spellId, "TIMEREWIND", "HEA", "Piaski Czasu", log);
		for (auto& l : log)
			ZLog("%s", l.c_str());
		ZLog(ok ? "SpellHook: czar TIMEREWIND w slocie %d" : "SpellHook: slot %d niedostepny - runa nie zadziala", trcfg.spellId);
	}

	bool SpellRuneReady() {
		return player && player->GetWeaponMode() == NPC_WEAPON_MAG && player->GetActiveSpellNr() == trcfg.spellId;
	}

	void SpellUpdate() {
		if (waitRelease && !RewindActive() && zinput->GetState(GAME_ACTION) <= 0.0f)
			waitRelease = false;
	}

	static void TellPlayer(const std::string& text) {
		if (!text.empty() && ogame && ogame->GetTextView())
			ogame->GetTextView()->PrintTimedCXY(zSTRING(text.c_str()), 2500.0f, nullptr);
	}

	// Spell_ProcessMana nie zna naszego czaru - decyzje podejmuje plugin. Czar zawsze konczy sie SENDSTOP
	// (bez animacji rzucania); przewijanie startuje w nastepnej klatce.
	HOOK Ivk_oCSpell_CallScriptInvestedMana AS(&oCSpell::CallScriptInvestedMana, &oCSpell::CallScriptInvestedMana_Union);
	void oCSpell::CallScriptInvestedMana_Union() {
		if (spellID != trcfg.spellId || !player || spellCasterNpc != player) {
			THISCALL(Ivk_oCSpell_CallScriptInvestedMana)();
			return;
		}
		ZPROF_SCOPE("Spell.Hook");
		spellStatus = TR_SPL_SENDSTOP;
		if (!trcfg.enabled || RewindActive() || waitRelease)
			return;
		std::string why;
		if (!RewindCanStart(why)) {
			ZLog("Runa: blokada (%s), mana %d, bufor %.1f s, stan ciala %d", why.empty() ? "cicha" : why.c_str(),
				player->attribute[NPC_ATR_MANA], RecorderSeconds(), player->GetBodyState());
			TellPlayer(why);
			waitRelease = true;
			return;
		}
		ZLog("Runa: start, mana %d -> %d, bufor %.1f s, akcja wcisnieta %d", player->attribute[NPC_ATR_MANA],
			player->attribute[NPC_ATR_MANA] - trcfg.manaStart, RecorderSeconds(), zinput->GetState(GAME_ACTION) > 0.0f);
		player->attribute[NPC_ATR_MANA] -= trcfg.manaStart;
		waitRelease = true;
		RewindRequest();
	}

	// Efekt dloni dla runy: SPELLFX_TIMEREWIND nie istnieje w VisualFX - bierzemy CastFx z ini.
	// oCVisualFX::SetByScript jest wirtualna: wskaznik na nia to thunk kompilatora, ktorego Union nie znajduje
	// ("HOOK: address not found" - hak sie nie zakladal), wiec hak po adresie G2A.
	static void __fastcall Hk_oCVisualFX_SetByScript(oCVisualFX* self, void* edx, zSTRING const& id);
	static CInvoke<void(__fastcall*)(oCVisualFX*, void*, zSTRING const&)> Ivk_oCVisualFX_SetByScript(0x0048D4B0,
		&Hk_oCVisualFX_SetByScript, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCVisualFX_SetByScript(oCVisualFX* self, void* edx, zSTRING const& id) {
		if (_stricmp(id.ToChar(), "SPELLFX_TIMEREWIND") == 0) {
			zSTRING alt(trcfg.castFx.c_str());
			Ivk_oCVisualFX_SetByScript(self, edx, alt);
			return;
		}
		Ivk_oCVisualFX_SetByScript(self, edx, id);
	}
}
