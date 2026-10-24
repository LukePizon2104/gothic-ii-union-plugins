// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	TrConfig trcfg;
	static const zSTRING TR_SECTION = "ZTIMEREWIND";

	static int ReadIntDef(const char* key, int def, int lo, int hi) {
		if (!zoptions->EntryExists(TR_SECTION, key))
			zoptions->WriteInt(TR_SECTION, key, def, FALSE);
		int v = zoptions->ReadInt(TR_SECTION, key, def);
		return v < lo ? lo : v > hi ? hi : v;
	}

	static float ReadRealDef(const char* key, float def, float lo, float hi) {
		if (!zoptions->EntryExists(TR_SECTION, key))
			zoptions->WriteReal(TR_SECTION, key, def, FALSE);
		float v = zoptions->ReadReal(TR_SECTION, key, def);
		return v < lo ? lo : v > hi ? hi : v;
	}

	static std::string ReadStringDef(const char* key, const char* def) {
		if (!zoptions->EntryExists(TR_SECTION, key))
			zoptions->WriteString(TR_SECTION, key, def, FALSE);
		return zoptions->ReadString(TR_SECTION, key, def).ToChar();
	}

	// Menu "Union & Pluginy" (System\Autorun\zTimeRewind_Menu.d): choicebox zapisuje NUMER pozycji do klucza Menu*.
	// Przy wczytaniu ustawien klucz Menu* dostaje pozycje z wartosci wlasciwej, po wyjsciu z menu przestawiona pozycja
	// staje sie wartoscia wlasciwa (reczna wartosc z ini spoza listy zostaje, dopoki gracz jej nie przestawi w menu).
	// Kolejnosc wartosci MUSI zgadzac sie z listami w zTimeRewind_Menu.d. Klucze 0/1 i tryby choicebox pisze wprost.
	struct MenuChoice {
		const char* menuKey;
		const char* realKey;
		std::vector<int> values;
		int lastIndex = NOT_SYNCED;
		static const int NOT_SYNCED = -2;
	};
	static MenuChoice menuChoices[] = {
		{ "MenuBufferSeconds", "BufferSeconds", { 10, 20, 30, 45, 60 } },
		{ "MenuRewindSpeed", "RewindSpeed", { 1, 2, 3, 4, 5 } },
		{ "MenuManaStart", "ManaStart", { 0, 5, 10, 20, 30 } },
		{ "MenuManaPerSecond", "ManaPerSecond", { 0, 2, 5, 10 } },
		{ "MenuMinRewindSeconds", "MinRewindSeconds", { 0, 1, 2, 3, 5 } },
		{ "MenuAutoRewindSeconds", "AutoRewindSeconds", { 5, 10, 15, 20, 30 } },
	};

	// "Zwrot zuzytych rzeczy": jedna pozycja menu na cztery klucze Restore*. Pozycja "z ini" (RESTORE_MENU_CUSTOM)
	// pojawia sie przy recznym zestawie w ini i nic nie zmienia.
	static const char* RESTORE_MENU_KEY = "MenuRestoreItems";
	static int restoreMenuLast = MenuChoice::NOT_SYNCED;

	static void MenuToIni() {
		trcore::RestoreOptions ro;
		int idx = zoptions->ReadInt(TR_SECTION, RESTORE_MENU_KEY, -1);
		if (restoreMenuLast != MenuChoice::NOT_SYNCED && idx != restoreMenuLast && trcore::RestoreFromMenu(idx, ro)) {
			zoptions->WriteInt(TR_SECTION, "RestoreScrolls", ro.scrolls, FALSE);
			zoptions->WriteInt(TR_SECTION, "RestorePotions", ro.potions, FALSE);
			zoptions->WriteInt(TR_SECTION, "RestoreFood", ro.food, FALSE);
			zoptions->WriteInt(TR_SECTION, "RestoreAmmo", ro.ammo, FALSE);
			ZLog("Menu: zwrot zuzytych - pozycja %d (zwoje %d, mikstury %d, jedzenie %d, amunicja %d)", idx, ro.scrolls, ro.potions,
				ro.food, ro.ammo);
		}
		for (auto& c : menuChoices) {
			int v = 0;
			if (c.lastIndex == MenuChoice::NOT_SYNCED ||
				!trcore::MenuChangedValue(c.values, c.lastIndex, zoptions->ReadInt(TR_SECTION, c.menuKey, -1), v))
				continue;
			zoptions->WriteInt(TR_SECTION, c.realKey, v, FALSE);
			ZLog("Menu: %s=%d", c.realKey, v);
		}
	}

	static void IniToMenu() {
		restoreMenuLast = trcore::RestoreMenuIndex(trcfg.restore);
		zoptions->WriteInt(TR_SECTION, RESTORE_MENU_KEY, restoreMenuLast, FALSE);
		for (auto& c : menuChoices) {
			// klucze rzeczywiste (np. RewindSpeed=3.0) - zaokraglenie do najblizszej pozycji
			int value = (int)(zoptions->ReadReal(TR_SECTION, c.realKey, 0.0f) + 0.5f);
			int idx = trcore::MenuIndexForValue(c.values, value, true);
			zoptions->WriteInt(TR_SECTION, c.menuKey, idx, FALSE);
			c.lastIndex = idx;
		}
	}

	void TrConfig::Load(bool fromMenu) {
		if (fromMenu)
			MenuToIni();
		enabled = ReadIntDef("Enabled", 1, 0, 1) != 0;
		bufferSeconds = ReadRealDef("BufferSeconds", 30.0f, 5.0f, 120.0f);
		sampleInterval = ReadRealDef("SampleInterval", 0.1f, 0.05f, 1.0f);
		rewindSpeed = ReadRealDef("RewindSpeed", 3.0f, 0.5f, 10.0f);
		manaStart = ReadIntDef("ManaStart", 10, 0, 1000);
		manaPerSecond = ReadRealDef("ManaPerSecond", 5.0f, 0.0f, 100.0f);
		minRewindSeconds = ReadRealDef("MinRewindSeconds", 3.0f, 0.0f, 30.0f);
		spellId = ReadIntDef("SpellID", 99, 0, 255);
		// zrodlo prawdy dla przedmiotu jest stala skryptu runy (C_Item.spell = SPL_TimeRewind): przy niezgodnosci
		// z ini wygrywa skrypt (np. paczka NB z ID 201 przy starym SpellID=99 w ini)
		if (zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING("SPL_TIMEREWIND")) : nullptr) {
			int scriptId = -1;
			s->GetValue(scriptId, 0);
			if (scriptId != spellId && scriptId >= 0 && scriptId <= 255) {
				ZLog("Config: SpellID=%d w ini, a SPL_TimeRewind=%d w skrypcie runy - uzywam %d (popraw Gothic.ini)",
					spellId, scriptId, scriptId);
				spellId = scriptId;
			}
		}
		puppetAnims = ReadIntDef("PuppetAnims", 1, 0, 2);
		castFx = ReadStringDef("CastFx", "SPELLFX_TELEPORT");
		hardResetDistance = ReadRealDef("HardResetDistance", 50.0f, 0.0f, 1000.0f);
		rewindScope = ReadIntDef("RewindScope", 1, 0, 1);
		stateInterval = ReadRealDef("StateInterval", 1.0f, 0.5f, 5.0f);
		// NB (UNION_ABI, deasemblacja 23.10): efekty tempa bohatera dodaja / odejmuja kwote do ANIACCELERATION (wskaznik na
		// zawartosc symbolu, 0x1023CCD0 / 0x1023D420), RUNACCELERATION / ADDITIONALACCELERATION / RX_STAMINAZEROSLOW
		// czyta liczenie tempa bohatera (0x10242110); lista efektow jest w DLL, nie w skryptach. SPRINT_ACTIVE i
		// RX_SPEEDPOTION ida w parze z nakladka Humans_MageSprint i AIVRX_NPC_SPEED_POTION (nie cofane). Nazw nieobecnych
		// w skryptach (Steam G2) nie ma w parserze - nic nie robia.
		keepGlobals = ReadStringDef("KeepGlobals",
			"ANIACCELERATION,RUNACCELERATION,ADDITIONALACCELERATION,ACCELERATIONACTIVE,ACCELERATIONRATIO,"
			"RX_STAMINAZEROSLOW,SPRINT_ACTIVE,RX_SPEEDPOTION");
		keepGlobalsGen++;
		autoRewindOnDeath = ReadIntDef("AutoRewindOnDeath", 1, 0, 1) != 0;
		autoRewindSeconds = ReadRealDef("AutoRewindSeconds", 10.0f, 1.0f, 60.0f);
		restoreCombat = ReadIntDef("RestoreCombat", 1, 0, 1) != 0;
		// listy z analizy skryptow G2A i NB (B_Attack, B_AssessDamage, ZS_Attack*, ZS_ReactToDamage, B_MemorizePlayerCrime);
		// nazwy nieobecne w skryptach danej gry sa pomijane (wpis w logu). Bez aivarow wlasnych NB (81-87 bohatera, 89+).
		combatStates = ReadStringDef("CombatStates",
			"ZS_ATTACK,ZS_REACTTODAMAGE,ZS_REACTTOWEAPON,ZS_WATCHFIGHT,ZS_FLEE,ZS_OBSERVEPLAYER,ZS_CLEARROOM,ZS_RANSACKBODY,"
			"ZS_GETMEAT,ZS_HEALSELF,ZS_HEALSELFBATTLE,ZS_MM_ATTACK,ZS_MM_FLEE,ZS_MM_THREATENENEMY,ZS_MM_EATBODY");
		combatAivars = ReadStringDef("CombatAivars",
			"AIV_ATTACKREASON,AIV_LASTPLAYERAR,AIV_LASTTARGET,AIV_PURSUITEND,AIV_WAITBEFOREATTACK,AIV_HITBYOTHERNPC,"
			"AIV_LASTFIGHTAGAINSTPLAYER,AIV_LASTFIGHTCOMMENT,AIV_FIGHTDISTCANCEL,AIV_LASTABSOLUTIONLEVEL,AIV_GUARDPASSAGE_STATUS");
		crimeAivars = ReadStringDef("CrimeAivars",
			"AIV_NPCSAWPLAYERCOMMIT,AIV_NPCSAWPLAYERCOMMITDAY,AIV_CRIMEABSOLUTIONLEVEL,AIV_PLAYERHASPICKEDMYPOCKET,"
			"AIV_SEENLEFTROOM,AIV_DEFEATEDBYPLAYER,AIV_KILLEDBYPLAYER,AIV_DUELLOST,AIV_ARENAFIGHT,AIV_ENEMYOVERRIDE");
		RelationsInvalidate();
		restore.scrolls = ReadIntDef("RestoreScrolls", 1, 0, 1) != 0;
		restore.potions = ReadIntDef("RestorePotions", 1, 0, 1) != 0;
		restore.food = ReadIntDef("RestoreFood", 1, 0, 1) != 0;
		restore.ammo = ReadIntDef("RestoreAmmo", 1, 0, 1) != 0;
		restoreMana = ReadIntDef("RestoreMana", 1, 0, 1) != 0;
		debug = ReadIntDef("Debug", 0, 0, 1) != 0;
		profiler = ReadIntDef("Profiler", 1, 0, 1) != 0;
		profSpikeMs = ReadRealDef("ProfSpikeMs", 25.0f, 5.0f, 1000.0f);
		profSummarySeconds = ReadRealDef("ProfSummarySeconds", 0.0f, 0.0f, 3600.0f);
		IniToMenu();
		ZLog("Config: bufor %.0f s, probka %.2f s, predkosc %.1fx, mana %d + %.1f/s, min %.1f s, slot %d, marionetki %d, zakres %s, migawki co %.1f s, auto przy smierci %d (%.0f s), zwrot zuzytych: zwoje %d mikstury %d jedzenie %d amunicja %d, zwrot many na czary %d",
			bufferSeconds, sampleInterval, rewindSpeed, manaStart, manaPerSecond, minRewindSeconds, spellId, puppetAnims,
			rewindScope == 1 ? "pelny" : "fizyczny", stateInterval, (int)autoRewindOnDeath, autoRewindSeconds, (int)restore.scrolls,
			(int)restore.potions, (int)restore.food, (int)restore.ammo, (int)restoreMana);
	}
}
