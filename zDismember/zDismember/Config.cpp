// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	ZConfig zcfg;
	static const zSTRING ZDISM_SECTION = "ZDISMEMBER";

	static int ReadIntDef(const char* key, int def, int lo, int hi) {
		if (!zoptions->EntryExists(ZDISM_SECTION, key))
			zoptions->WriteInt(ZDISM_SECTION, key, def, FALSE);
		int v = zoptions->ReadInt(ZDISM_SECTION, key, def);
		return v < lo ? lo : v > hi ? hi : v;
	}

	static float ReadRealDef(const char* key, float def, float lo, float hi) {
		if (!zoptions->EntryExists(ZDISM_SECTION, key))
			zoptions->WriteReal(ZDISM_SECTION, key, def, FALSE);
		float v = zoptions->ReadReal(ZDISM_SECTION, key, def);
		return v < lo ? lo : v > hi ? hi : v;
	}

	static std::string ReadStringDef(const char* key, const char* def) {
		if (!zoptions->EntryExists(ZDISM_SECTION, key))
			zoptions->WriteString(ZDISM_SECTION, key, def, FALSE);
		return zoptions->ReadString(ZDISM_SECTION, key, def).ToChar();
	}

	static void ReadWeights(const char* key, const char* def, int* out) {
		if (!zoptions->EntryExists(ZDISM_SECTION, key))
			zoptions->WriteString(ZDISM_SECTION, key, def, FALSE);
		zSTRING s = zoptions->ReadString(ZDISM_SECTION, key, def);
		if (!zdcore::ParseIntList(s.ToChar(), out, 3)) {
			ZLog("Config: zla wartosc %s=%s, zostaje %s", key, s.ToChar(), def);
			zdcore::ParseIntList(def, out, 3);
		}
	}

	// Menu "Union & Pluginy" (System\Autorun\zDismember_Menu.d): choicebox zapisuje numer pozycji do klucza Menu*.
	// Przy wczytaniu ustawien klucz Menu* dostaje pozycje z wartosci wlasciwej, po wyjsciu z menu przestawiona pozycja
	// staje sie wartoscia wlasciwa (reczna wartosc z ini spoza listy zostaje, dopoki gracz jej nie przestawi w menu).
	struct MenuChoice {
		const char* menuKey;
		const char* realKey;
		std::vector<int> values;
		bool nearest;                 // skala (najblizsza pozycja) albo lista klawiszy (dokladnie, inaczej "wlasny")
		int lastIndex = NOT_SYNCED;
		static const int NOT_SYNCED = -2;
	};
	static const std::vector<int> PERCENT = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
	static MenuChoice menuChoices[] = {
		{ "MenuDismemberChance", "DismemberChancePlayer", PERCENT, true },
		{ "MenuFinisherChance", "FinisherChance", PERCENT, true },
		{ "MenuW3StyleMinTalent", "W3StyleMinTalent", PERCENT, true },
		{ "MenuCounterRepelChance", "CounterRepelChance", PERCENT, true },
		{ "MenuDodgeKey", "DodgeKey", { 47, 46, 45, 44, 56, 29, 58, 0 }, false },   // V C X Z LAlt LCtrl CapsLock wyl.
		{ "MenuRollKey", "RollKey", { 56, 44, 45, 46, 47, 29, 0 }, false },     // LAlt Z X C V LCtrl wyl.
		// strona "Tempo ciosow Wiedzmina" (AtkTempo.cpp): jak natywne | 70..200 % (zdcore::AtkSpeedMenuValues)
		{ "MenuW3AtkSpeed1HLeft", "W3AtkSpeed1HLeft", zdcore::AtkSpeedMenuValues(), true },
		{ "MenuW3AtkSpeed1HRight", "W3AtkSpeed1HRight", zdcore::AtkSpeedMenuValues(), true },
		{ "MenuW3AtkSpeed1HCombo", "W3AtkSpeed1HCombo", zdcore::AtkSpeedMenuValues(), true },
		{ "MenuW3AtkSpeed2HLeft", "W3AtkSpeed2HLeft", zdcore::AtkSpeedMenuValues(), true },
		{ "MenuW3AtkSpeed2HRight", "W3AtkSpeed2HRight", zdcore::AtkSpeedMenuValues(), true },
		{ "MenuW3AtkSpeed2HCombo", "W3AtkSpeed2HCombo", zdcore::AtkSpeedMenuValues(), true },
		// strona "Medytacja" (Meditate.cpp)
		{ "MenuMeditateKey", "MeditateKey", { 28, 156, 0 }, false },        // Enter, Enter numeryczny, wyl.
		{ "MenuMeditateKeyAlt", "MeditateKeyAlt", { 51, 0 }, false },       // przecinek (z MeditateKeyAltMods), wyl.
		{ "MenuMeditateSpeed", "MeditateSpeed", zdcore::MedSpeedValues(), true },
		{ "MenuMeditateRegenHours", "MeditateRegenHours", { 2, 4, 6, 8, 10, 12, 16, 24 }, true },
		{ "MenuMeditateSleepHours", "MeditateSleepHours", { 1, 2, 3, 4, 5, 6, 8, 10, 12 }, true },
		// strona "Miecze na plecach" (BackSwords.cpp): tempo % (zdcore::BackSpeedMenuValues / BeltSpeedMenuValues),
		// klawisz drugiego miecza G B Y R wyl. (ostatnia pozycja listy = "wlasny" z Gothic.ini)
		{ "MenuBackDrawSpeed", "BackDrawSpeed", zdcore::BackSpeedMenuValues(), true },
		{ "MenuBackSheatheSpeed", "BackSheatheSpeed", zdcore::BackSpeedMenuValues(), true },
		{ "MenuBeltDrawSpeed", "BeltDrawSpeed", zdcore::BeltSpeedMenuValues(), true },
		{ "MenuSecondSwordKey", "SecondSwordKey", zdcore::SecondKeyMenuValues(), false },
		// strona "Szybki strzal z kuszy" (Crossbow.cpp)
		{ "MenuXbowSpeed", "XbowSpeed", { 70, 85, 100, 115, 130, 150, 175, 200 }, true },
		{ "MenuXbowHandScale", "XbowHandScale", { 60, 70, 80, 90, 100, 110, 120 }, true },
		// strona "Kolo wyboru" (Wheel.cpp)
		{ "MenuWheelSlowmo", "WheelSlowmo", { 5, 10, 15, 20, 25, 30, 50, 100 }, true },
		{ "MenuWheelHoldMs", "WheelHoldMs", { 150, 200, 250, 300, 350, 400, 500 }, true },
		{ "MenuWheelMouseSens", "WheelMouseSens", { 50, 75, 100, 125, 150, 200 }, true },
		{ "MenuXbowAimSlowmo", "XbowAimSlowmo", { 100, 50, 30, 20, 15, 10 }, true },
	};

	// Przeskoki i wspinanie: jedna pozycja menu MenuTraversal (0 wyl., 1 poza walka, 2 tez w walce) na dwa klucze
	// Traversal i TraversalCombat (przy "wyl." TraversalCombat zostaje bez zmian).
	static int travMenuLast = MenuChoice::NOT_SYNCED;
	// Podnoszenie z bronia: pozycja MenuLoot (0 wyl., 1 wl., 2 wl. z podpowiedzia) na klucze LootArmed i LootHint
	// (zdcore::LootMenuApply; przy "wyl." LootHint zostaje bez zmian).
	static int lootMenuLast = MenuChoice::NOT_SYNCED;
	// Klawisz zamiany animacji: pozycja MenuWeaponSwapKey (0 Ctrl+Shift+U, 1 wyl., 2 wlasny) na klucze WeaponSwapKey
	// i WeaponSwapMods (zdcore::SwapKeyMenuApply; "wlasny" nic nie zmienia).
	static int swapKeyMenuLast = MenuChoice::NOT_SYNCED;

	// Strojenie Znakow (runda 5): strony "Znaki: ..." w zDismember_Menu.d. Klucz MenuSg* = numer pozycji, wartosci
	// liczbowe (tez ulamki) - SGM_REAL zapis tekstem %g, SGM_INT liczba calkowita, SGM_COST element listy SignManaCost.
	enum { SGM_REAL, SGM_INT, SGM_COST };
	struct SignMenuChoice {
		const char* menuKey;
		const char* realKey;
		int kind;
		int listIndex;
		std::vector<float> values;
		int lastIndex = MenuChoice::NOT_SYNCED;
	};
	static SignMenuChoice signMenu[] = {   // gen_sign_menu.py - kolejnosc jak w zDismember_Menu.d
		{ "MenuSgAardRange", "AardRange", SGM_REAL, -1, { 400.0f, 500.0f, 600.0f, 700.0f, 800.0f, 1000.0f, 1200.0f, 1500.0f } },
		{ "MenuSgAardDamage", "AardDamage", SGM_REAL, -1, { 5.0f, 10.0f, 15.0f, 20.0f, 30.0f, 40.0f, 60.0f, 80.0f, 100.0f, 150.0f, 200.0f } },
		{ "MenuSgIgniRange", "IgniRange", SGM_REAL, -1, { 450.0f, 550.0f, 650.0f, 750.0f, 850.0f, 1000.0f, 1200.0f, 1500.0f } },
		{ "MenuSgIgniStreamRange", "IgniStreamRange", SGM_REAL, -1, { 500.0f, 600.0f, 700.0f, 800.0f, 900.0f, 1000.0f, 1200.0f, 1500.0f } },
		{ "MenuSgIgniDamage", "IgniDamage", SGM_REAL, -1, { 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f, 100.0f, 150.0f, 200.0f, 250.0f, 300.0f } },
		{ "MenuSgIgniBurnDps", "IgniBurnDps", SGM_REAL, -1, { 2.0f, 3.0f, 5.0f, 8.0f, 10.0f, 15.0f, 20.0f, 30.0f, 50.0f, 75.0f, 100.0f } },
		{ "MenuSgIgniBurnTime", "IgniBurnTime", SGM_REAL, -1, { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f } },
		{ "MenuSgAxiiStunTime", "AxiiStunTime", SGM_REAL, -1, { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 10.0f } },
		{ "MenuSgAxiiPuppetTime", "AxiiPuppetTime", SGM_REAL, -1, { 5.0f, 10.0f, 15.0f, 20.0f, 30.0f } },
		{ "MenuSgYrdenRadius", "YrdenRadius", SGM_REAL, -1, { 300.0f, 400.0f, 500.0f, 600.0f, 800.0f } },
		{ "MenuSgYrdenTime", "YrdenTime", SGM_REAL, -1, { 8.0f, 10.0f, 15.0f, 20.0f, 30.0f, 45.0f } },
		{ "MenuSgYrdenSlow", "YrdenSlow", SGM_REAL, -1, { 0.4f, 0.5f, 0.55f, 0.6f, 0.65f, 0.7f, 0.8f } },
		{ "MenuSgYrdenArmor", "YrdenArmor", SGM_REAL, -1, { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f } },
		{ "MenuSgQuenTime", "QuenTime", SGM_REAL, -1, { 10.0f, 20.0f, 30.0f, 45.0f, 60.0f } },
		{ "MenuSgQuenDamage", "QuenDamage", SGM_REAL, -1, { 5.0f, 10.0f, 15.0f, 20.0f, 30.0f, 50.0f, 75.0f, 100.0f } },
		{ "MenuSgPowerPerMana", "SignPowerPerMana", SGM_REAL, -1, { 0.0f, 0.0005f, 0.001f, 0.002f } },
		{ "MenuSgPowerMax", "SignPowerMax", SGM_REAL, -1, { 0.5f, 1.0f, 1.5f, 2.0f } },
		{ "MenuSgAltHold", "SignAltHold", SGM_REAL, -1, { 0.0f, 0.25f, 0.35f, 0.5f, 0.75f } },
		{ "MenuSgCostAard", "SignManaCost", SGM_COST, 0, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgCostIgni", "SignManaCost", SGM_COST, 1, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgCostAxii", "SignManaCost", SGM_COST, 2, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgCostYrden", "SignManaCost", SGM_COST, 3, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgCostQuen", "SignManaCost", SGM_COST, 4, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgAardAltCost", "AardAltCost", SGM_INT, -1, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgYrdenAltCost", "YrdenAltCost", SGM_INT, -1, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgAxiiAltCost", "AxiiAltCost", SGM_INT, -1, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		// strona 3 "Znaki: poza walka" (runda 6, recznie w zDismember_Menu.d - ZDISM_SG3_*); SignFires, SignAardUnlock
		// i SignAxiiCharm to zwykle 0/1 (choicebox zapisuje je wprost)
		{ "MenuSgAxiiTranceTime", "AxiiTranceTime", SGM_REAL, -1, { 0.0f, 10.0f, 20.0f, 30.0f, 45.0f, 60.0f } },
		{ "MenuSgAxiiPickpocketBonus", "SignAxiiPickpocketBonus", SGM_INT, -1, { 0.0f, 10.0f, 20.0f, 25.0f, 30.0f, 40.0f, 50.0f } },
		{ "MenuSgAxiiPickpocketTime", "SignAxiiPickpocketTime", SGM_REAL, -1, { 0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 180.0f } },
		// strona 3, pozycje 7-8 (20.10): obrazenia z inteligencji (New Balance); SignHitNeutral (poz. 9) zapisuje 0/1 wprost
		{ "MenuSgPowerInt", "SignPowerInt", SGM_REAL, -1, { 0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 30.0f } },
		{ "MenuSgPowerIntMax", "SignPowerIntMax", SGM_REAL, -1, { 2.0f, 3.0f, 5.0f, 8.0f, 10.0f } },
		// strona 3, pozycje 11-13 (runda 7): zasieg Axii, podtrzymanie strumienia Igni i aktywnej tarczy Quen (many / s);
		// pozycja 10 - SignPowerSource (0/1/2 wprost)
		{ "MenuSgAxiiRange", "AxiiRange", SGM_REAL, -1, { 500.0f, 800.0f, 1000.0f, 1200.0f, 1500.0f } },
		{ "MenuSgIgniStreamMana", "IgniStreamManaPerSec", SGM_REAL, -1, { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 15.0f, 20.0f, 30.0f } },
		{ "MenuSgQuenActiveMana", "QuenActiveManaPerSec", SGM_REAL, -1, { 0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f, 15.0f, 20.0f, 30.0f } },
	};

	// Inne ulamkowe listy menu (poza generatorem znakow): ten sam zapis co SGM_REAL.
	static SignMenuChoice realMenu[] = {
		{ "MenuCounterWindow", "CounterWindow", SGM_REAL, -1, { 0.0f, 0.2f, 0.3f, 0.4f, 0.5f, 0.7f } },   // caly zamach | 0,2..0,7 s
		{ "MenuJumpAttackWindow", "JumpAttackWindow", SGM_REAL, -1, { 0.2f, 0.3f, 0.4f, 0.5f, 0.6f } },   // 0,2..0,6 s
	};

	static bool SignMenuCosts(int out[5]) {
		return zdcore::ParseIntList(std::string(zoptions->ReadString(ZDISM_SECTION, "SignManaCost", "10,15,15,20,15").ToChar()), out, 5);
	}

	static float SignMenuRead(const SignMenuChoice& c) {
		if (c.kind == SGM_COST) {
			int v[5];
			return SignMenuCosts(v) ? (float)v[c.listIndex] : c.values[0];
		}
		return zoptions->ReadReal(ZDISM_SECTION, c.realKey, c.values[0]);
	}

	static void SignMenuWrite(const SignMenuChoice& c, float val) {
		char buf[64];
		if (c.kind == SGM_COST) {
			int v[5] = { 10, 15, 15, 20, 15 };
			SignMenuCosts(v);
			v[c.listIndex] = (int)std::lround(val);
			sprintf_s(buf, "%d,%d,%d,%d,%d", v[0], v[1], v[2], v[3], v[4]);
			zoptions->WriteString(ZDISM_SECTION, "SignManaCost", buf, FALSE);
		}
		else if (c.kind == SGM_INT)
			zoptions->WriteInt(ZDISM_SECTION, c.realKey, (int)std::lround(val), FALSE);
		else {
			sprintf_s(buf, "%g", val);
			zoptions->WriteString(ZDISM_SECTION, c.realKey, buf, FALSE);
		}
		ZLog("Menu: %s%s = %g", c.realKey, c.kind == SGM_COST ? "[znak]" : "", val);
	}

	static void SignMenuToIni(SignMenuChoice& c) {
		int idx = zoptions->ReadInt(ZDISM_SECTION, c.menuKey, -1);
		if (c.lastIndex == MenuChoice::NOT_SYNCED || idx < 0 || idx >= (int)c.values.size() || idx == c.lastIndex)
			return;
		SignMenuWrite(c, c.values[idx]);
	}

	static void SignIniToMenu(SignMenuChoice& c) {
		int idx = zdcore::MenuIndexForReal(c.values, SignMenuRead(c));
		if (idx < 0)
			idx = 0;
		zoptions->WriteInt(ZDISM_SECTION, c.menuKey, idx, FALSE);
		c.lastIndex = idx;
	}

	static void SignMenuToIni() {
		for (auto& c : signMenu)
			SignMenuToIni(c);
		for (auto& c : realMenu)
			SignMenuToIni(c);
	}

	static void SignIniToMenu() {
		for (auto& c : signMenu)
			SignIniToMenu(c);
		for (auto& c : realMenu)
			SignIniToMenu(c);
	}

	static void MenuToIni() {
		for (auto& c : menuChoices) {
			int v = 0;
			if (c.lastIndex == MenuChoice::NOT_SYNCED ||
				!zdcore::MenuChangedValue(c.values, c.lastIndex, zoptions->ReadInt(ZDISM_SECTION, c.menuKey, -1), v))
				continue;
			zoptions->WriteInt(ZDISM_SECTION, c.realKey, v, FALSE);
			ZLog("Menu: %s=%d", c.realKey, v);
		}
		int trav = zoptions->ReadInt(ZDISM_SECTION, "MenuTraversal", -1);
		if (travMenuLast != MenuChoice::NOT_SYNCED && trav >= 0 && trav <= 2 && trav != travMenuLast) {
			zoptions->WriteInt(ZDISM_SECTION, "Traversal", trav > 0 ? 1 : 0, FALSE);
			if (trav > 0)
				zoptions->WriteInt(ZDISM_SECTION, "TraversalCombat", trav == 2 ? 1 : 0, FALSE);
			ZLog("Menu: Traversal=%d TraversalCombat=%d", trav > 0 ? 1 : 0, zoptions->ReadInt(ZDISM_SECTION, "TraversalCombat", 1));
		}
		int loot = zoptions->ReadInt(ZDISM_SECTION, "MenuLoot", -1);
		if (lootMenuLast != MenuChoice::NOT_SYNCED && loot >= 0 && loot <= 2 && loot != lootMenuLast) {
			bool armed = zoptions->ReadInt(ZDISM_SECTION, "LootArmed", 1) != 0, hint = zoptions->ReadInt(ZDISM_SECTION, "LootHint", 1) != 0;
			zdcore::LootMenuApply(loot, armed, hint);
			zoptions->WriteInt(ZDISM_SECTION, "LootArmed", armed ? 1 : 0, FALSE);
			zoptions->WriteInt(ZDISM_SECTION, "LootHint", hint ? 1 : 0, FALSE);
			ZLog("Menu: LootArmed=%d LootHint=%d", armed, hint);
		}
		int sk = zoptions->ReadInt(ZDISM_SECTION, "MenuWeaponSwapKey", -1);
		if (swapKeyMenuLast != MenuChoice::NOT_SYNCED && sk >= 0 && sk <= 2 && sk != swapKeyMenuLast) {
			int key = zoptions->ReadInt(ZDISM_SECTION, "WeaponSwapKey", zdcore::SWAP_KEY_DEFAULT);
			int mods = zoptions->ReadInt(ZDISM_SECTION, "WeaponSwapMods", zdcore::SWAP_MODS_DEFAULT);
			zdcore::SwapKeyMenuApply(sk, key, mods);
			zoptions->WriteInt(ZDISM_SECTION, "WeaponSwapKey", key, FALSE);
			zoptions->WriteInt(ZDISM_SECTION, "WeaponSwapMods", mods, FALSE);
			ZLog("Menu: WeaponSwapKey=%d WeaponSwapMods=%d", key, mods);
		}
		SignMenuToIni();
	}

	static void IniToMenu() {
		for (auto& c : menuChoices) {
			int idx = zdcore::MenuIndexForValue(c.values, zoptions->ReadInt(ZDISM_SECTION, c.realKey, 0), c.nearest);
			if (idx < 0)
				idx = (int)c.values.size();   // ostatnia pozycja listy klawiszy: "wlasny (Gothic.ini)"
			zoptions->WriteInt(ZDISM_SECTION, c.menuKey, idx, FALSE);
			c.lastIndex = idx;
		}
		int trav = zoptions->ReadInt(ZDISM_SECTION, "Traversal", 1) == 0 ? 0 :
			zoptions->ReadInt(ZDISM_SECTION, "TraversalCombat", 1) != 0 ? 2 : 1;
		zoptions->WriteInt(ZDISM_SECTION, "MenuTraversal", trav, FALSE);
		travMenuLast = trav;
		int loot = zdcore::LootMenuIndex(zoptions->ReadInt(ZDISM_SECTION, "LootArmed", 1) != 0,
			zoptions->ReadInt(ZDISM_SECTION, "LootHint", 1) != 0);
		zoptions->WriteInt(ZDISM_SECTION, "MenuLoot", loot, FALSE);
		lootMenuLast = loot;
		int sk = zdcore::SwapKeyMenuIndex(zoptions->ReadInt(ZDISM_SECTION, "WeaponSwapKey", zdcore::SWAP_KEY_DEFAULT),
			zoptions->ReadInt(ZDISM_SECTION, "WeaponSwapMods", zdcore::SWAP_MODS_DEFAULT));
		zoptions->WriteInt(ZDISM_SECTION, "MenuWeaponSwapKey", sk, FALSE);
		swapKeyMenuLast = sk;
		SignIniToMenu();
	}

	void ZConfig::Load(bool fromMenu) {
		if (fromMenu)
			MenuToIni();
		enabled = ReadIntDef("Enabled", 1, 0, 1) != 0;
		chancePlayer = ReadIntDef("DismemberChancePlayer", 100, 0, 100);
		chanceNpc = ReadIntDef("DismemberChanceNpc", 100, 0, 100);
		finisherChance = ReadIntDef("FinisherChance", 30, 0, 100);
		knockoutFinisher = ReadIntDef("KnockoutFinisher", 1, 0, 2);
		dazeEnabled = ReadIntDef("DazeEnabled", 1, 0, 1) != 0;
		dazeSeconds = ReadRealDef("DazeSeconds", 5.0f, 0.5f, 60.0f);
		dazeGrace = ReadRealDef("DazeGraceSeconds", 0.6f, 0.0f, 5.0f);
		heroInvulnerable = ReadIntDef("HeroInvulnerable", 1, 0, 1) != 0;
		aivarIndex = ReadIntDef("AivarIndex", 95, -1, 99);   // -1 = bez pamieci kikutow
		maxGibs = ReadIntDef("MaxGibs", 24, 0, 200);
		gibLifetime = ReadRealDef("GibLifetimeSeconds", 180.0f, 5.0f, 3600.0f);
		gibDespawnDist = ReadRealDef("GibDespawnDistance", 4000.0f, 500.0f, 100000.0f);
		maxVictimHeight = ReadRealDef("MaxVictimHeight", 230.0f, 50.0f, 2000.0f);
		staggerRadius = ReadRealDef("FinisherStaggerRadius", 800.0f, 0.0f, 5000.0f);
		movesEnabled = ReadIntDef("MovesEnabled", 1, 0, 1) != 0;
		dodgeKey = ReadIntDef("DodgeKey", 47, 0, 255);
		rollKey = ReadIntDef("RollKey", 56, 0, 255);
		dodgeKeyCombatOnly = ReadIntDef("DodgeKeyCombatOnly", 1, 0, 1) != 0;
		doubleTap = ReadIntDef("DoubleTapDodge", 0, 0, 2);
		doubleTapMove = ReadIntDef("DoubleTapMove", 0, 0, 1);
		doubleTapWindow = ReadRealDef("DoubleTapWindow", 0.25f, 0.1f, 0.6f);
		dodgeWithAction = ReadIntDef("DodgeWithAction", 1, 0, 1) != 0;
		jumpAttackWindow = ReadRealDef("JumpAttackWindow", 0.30f, 0.1f, 1.0f);
		traversal = ReadIntDef("Traversal", 1, 0, 1) != 0;
		traversalCombat = ReadIntDef("TraversalCombat", 1, 0, 1) != 0;
		traversalDebug = ReadIntDef("TraversalDebug", 0, 0, 1) != 0;
		traversalAirGrab = ReadIntDef("TraversalAirGrab", 1, 0, 1) != 0;
		traversalSheathe = ReadIntDef("TraversalSheathe", 1, 0, 1) != 0;
		traversalRedraw = ReadIntDef("TraversalRedraw", 1, 0, 1) != 0;
		traversalHint = ReadIntDef("TraversalHint", 3, 0, 100);
		traversalHintShown = ReadIntDef("TraversalHintShown", 0, 0, 100000);
		jumpKeepMomentum = ReadIntDef("JumpKeepMomentum", 1, 0, 1) != 0;
		fallForgiveness = ReadIntDef("FallForgiveness", 1, 0, 1) != 0;
		fallForgiveDrop = ReadIntDef("FallForgiveDrop", 40, 10, 60);
		teleportGuard = ReadIntDef("TeleportGuard", 1, 0, 1) != 0;
		counters = ReadIntDef("Counters", 1, 0, 1) != 0;
		counterRepelChance = ReadIntDef("CounterRepelChance", 40, 0, 100);
		counterOnBackstep = ReadIntDef("CounterOnBackstep", 0, 0, 1) != 0;
		counterWindow = ReadRealDef("CounterWindow", 0.0f, 0.0f, 2.0f);
		rapierThrustOnly = ReadIntDef("RapierThrustOnly", 1, 0, 1) != 0;
		rapierTokens = zdcore::SplitTokens(ReadStringDef("RapierWeapons", "RAPIER,DEGEN"), ',');
		// G2: obuch ma DAM_BLUNT (27 broni), lista lapie sieczne z wygladu obuchowe (Inkwizytor, Kostur sedziego, pogrzebacz)
		bluntTokens = zdcore::SplitTokens(ReadStringDef("BluntWeapons", "KEULE,HAMMER,STREITKOLBEN,MORGENSTERN,STAB,KNUEPPEL,MACE,CLUB,STAFF"), ',');
		weapons.rapierTokens = rapierTokens;
		weapons.bluntTokens = bluntTokens;
		weapons.rapierThrustOnly = rapierThrustOnly;
		finisherEarlyRelease = ReadIntDef("FinisherEarlyRelease", 1, 0, 1) != 0;
		finisherBlend = ReadRealDef("FinisherRecoverBlend", 0.3f, 0.05f, 1.0f);
		finisherSlowMo = ReadIntDef("FinisherSlowMo", zdcore::SLOWMO_FULL, 0, 2);
		finisherSlowMoLastEnemy = ReadIntDef("FinisherSlowMoLastEnemy", 0, 0, 1) != 0;
		w3Locomotion = ReadIntDef("W3Locomotion", 1, 0, 1) != 0;
		w3Sprint = ReadIntDef("W3Sprint", 1, 0, 1) != 0;
		w3SprintArmedSpeed = ReadRealDef("W3SprintArmedSpeed", 600.0f, 0.0f, 1500.0f);
		w3SprintUnarmedSpeed = ReadRealDef("W3SprintUnarmedSpeed", 0.0f, 0.0f, 1500.0f);
		evadeEarlyExit = ReadIntDef("EvadeEarlyExit", 1, 0, 1) != 0;
		evadeExitBuffer = ReadRealDef("EvadeExitBuffer", 1.0f, 0.1f, 3.0f);
		evadeFaceEnd = ReadIntDef("EvadeFaceEnd", 1, 0, 1) != 0;
		evadeFaceLead = ReadRealDef("EvadeFaceLead", 0.25f, 0.05f, 1.0f);
		evadeFaceRate = ReadRealDef("EvadeFaceRate", 540.0f, 60.0f, 2000.0f);
		perfectDodge = ReadIntDef("PerfectDodge", 1, 0, 1) != 0;
		perfectDodgeWindow = ReadRealDef("PerfectDodgeWindow", 0.35f, 0.05f, 1.0f);
		perfectDodgeRange = ReadRealDef("PerfectDodgeRange", 450.0f, 100.0f, 1500.0f);
		perfectDodgeLostTime = ReadRealDef("PerfectDodgeLostTime", 0.8f, 0.0f, 5.0f);
		perfectDodgeTurnRate = ReadRealDef("PerfectDodgeTurnRate", 60.0f, 0.0f, 720.0f);
		perfectDodgeHeroSpeed = ReadRealDef("PerfectDodgeHeroSpeed", 1.2f, 1.0f, 2.0f);
		perfectDodgeHeroWindow = ReadRealDef("PerfectDodgeHeroWindow", 1.0f, 0.0f, 5.0f);
		perfectDodgeFx = ReadIntDef("PerfectDodgeFx", 1, 0, 1) != 0;
		perfectDodgeSound = ReadStringDef("PerfectDodgeSound", "WHOOSH");
		w3Style = ReadIntDef("W3Style", 1, 0, 1) != 0;
		w3StyleMinTalent = ReadIntDef("W3StyleMinTalent", 60, 0, 100);
		w3AttackAssist = ReadIntDef("W3AttackAssist", 1, 0, 1) != 0;
		w3AttackReach = ReadRealDef("W3AttackReach", 40.0f, 0.0f, 150.0f);
		w3AssistMargin = ReadRealDef("W3AssistMargin", 25.0f, 0.0f, 100.0f);
		w3AssistMaxPath = ReadRealDef("W3AssistMaxPath", 130.0f, 0.0f, 300.0f);
		w3AssistSpeed = ReadRealDef("W3AssistSpeed", 450.0f, 50.0f, 1500.0f);
		w3AssistCone = ReadRealDef("W3AssistCone", 45.0f, 5.0f, 90.0f);
		for (int i = 0; i < zdcore::ATK_SLOT_COUNT; i++)
			w3AtkSpeed[i] = zdcore::ClampAtkSpeed(ReadIntDef(zdcore::AtkSlotIniKey(i), zdcore::ATK_SPEED_DEFAULT, 0, zdcore::ATK_SPEED_MAX));
		w3Side2H = ReadIntDef("W3Side2H", zdcore::SIDE2H_W3, 0, zdcore::SIDE2H_COUNT - 1);
		styleKey = ReadIntDef("StyleKey", 22, 0, 255);
		locoKey = ReadIntDef("LocoKey", 24, 0, 255);
		locoMode = ReadIntDef("LocoMode", 0, 0, zdcore::LOCO_MODE_COUNT - 1);
		// Zamiana animacji 1H <-> 2H (AniSwap.cpp); wariant z menu albo recznie zostaje ostatnim niezerowym (klawisz)
		swap.mode = ReadIntDef("WeaponSwap", zdcore::SWAP_OFF, 0, zdcore::SWAP_MODE_COUNT - 1);
		swap.last = ReadIntDef("WeaponSwapLast", zdcore::SWAP_BOTH, 1, zdcore::SWAP_MODE_COUNT - 1);
		if (swap.mode != zdcore::SWAP_OFF && swap.mode != swap.last) {
			swap.last = swap.mode;
			zoptions->WriteInt(ZDISM_SECTION, "WeaponSwapLast", swap.last, FALSE);
		}
		swap.level = ReadIntDef("WeaponSwapLevel", 0, 0, 1);
		swap.perWeapon = ReadIntDef("WeaponSwapPerWeapon", 0, 0, 1) != 0;
		swap.weapons = zdcore::SplitTokens(ReadStringDef("WeaponSwapWeapons", ""), ',');
		swapKey = ReadIntDef("WeaponSwapKey", zdcore::SWAP_KEY_DEFAULT, 0, 255);
		swapMods = ReadIntDef("WeaponSwapMods", zdcore::SWAP_MODS_DEFAULT, 0, 7);
		lootArmed = ReadIntDef("LootArmed", 1, 0, 1) != 0;
		lootRange = ReadRealDef("LootRange", 150.0f, 50.0f, 400.0f);
		lootEnemyRadius = ReadRealDef("LootEnemyRadius", 1000.0f, 0.0f, 5000.0f);
		lootHint = ReadIntDef("LootHint", 1, 0, 1) != 0;
		lootDoors = ReadIntDef("LootArmedDoors", zdcore::IA_DOORS_ALL, 0, zdcore::IA_DOORS_COUNT - 1);
		lootChests = ReadIntDef("LootArmedChests", 1, 0, 1) != 0;
		lootCorpses = ReadIntDef("LootArmedCorpses", 1, 0, 1) != 0;
		lootRedraw = ReadIntDef("LootArmedRedraw", 1, 0, 1) != 0;
		ragdoll = ReadIntDef("Ragdoll", 1, 0, 1) != 0;
		ragdollChance = ReadIntDef("RagdollChance", 100, 0, 100);
		ragdollImpulse = ReadRealDef("RagdollImpulse", 180.0f, 0.0f, 2000.0f);
		ragdollMaxActive = ReadIntDef("RagdollMaxActive", 4, 1, 16);
		parryFx = ReadIntDef("ParryFx", 1, 0, 1) != 0;
		parryRecoil = ReadIntDef("ParryRecoil", 1, 0, 1) != 0;
		parryRecoilDeg = ReadRealDef("ParryRecoilStrength", 5.0f, 0.0f, 20.0f);
		parryRecoilTime = ReadRealDef("ParryRecoilTime", 0.18f, 0.08f, 0.4f);
		// G2: palki, buzdygany i kostury maja MAT_WOOD - lista tylko dla modow z blednym materialem
		parryWoodTokens = zdcore::SplitTokens(ReadStringDef("ParryWoodWeapons", "STAFF,KEULE,CLUB,KNUEPPEL"), ',');
		ReadWeights("PartWeightsSwing", "50,35,15", weightsSwing);
		ReadWeights("PartWeightsThrust", "20,0,0", weightsThrust);
		debug = ReadIntDef("Debug", 0, 0, 1) != 0;
		profiler = ReadIntDef("Profiler", 1, 0, 1) != 0;
		profSpikeMs = ReadRealDef("ProfSpikeMs", 25.0f, 5.0f, 1000.0f);
		profSummarySeconds = ReadRealDef("ProfSummarySeconds", 0.0f, 0.0f, 3600.0f);
		// profiler probkujacy (_shared\ZSampler.h): co ProfSampleMs ms wstrzymuje watek gry na kilka-kilkanascie us i
		// zapisuje miejsce (modul / funkcja exe) - linia po kazdej przycince, tabele: zdism prof samples
		profSampler = ReadIntDef("ProfSampler", 1, 0, 1) != 0;
		profSampleMs = ReadIntDef("ProfSampleMs", 1, 1, 100);
		// prototypy nakladek bohatera (style, ruch W3, uniki, przeskoki, finishery) w pamieci od wczytania swiata - zalozenie
		// nie czyta MSB / MAN z dysku w klatce (Preload.cpp); kosztem kilkunastu MB i ok. sekundy przy pierwszym wczytaniu
		preloadOverlays = ReadIntDef("PreloadOverlays", 1, 0, 1) != 0;
		// Znaki Wiedzmina (Signs.cpp; projekt [dokument projektowy - niepublikowany])
		// Runda 5: wieksze domyslne zasiegi Igni (450 -> 650, strumien 500 -> 700) - raz zamieniamy w ini stara wartosc
		// domyslna (wartosc zmieniona recznie albo w menu zostaje). SignDefaultsRev = numer rundy zmian domyslnych.
		if (zoptions->ReadInt(ZDISM_SECTION, "SignDefaultsRev", 0) < 5) {
			if (zoptions->EntryExists(ZDISM_SECTION, "IgniRange") && fabsf(zoptions->ReadReal(ZDISM_SECTION, "IgniRange", 0) - 450.0f) < 0.5f)
				zoptions->WriteReal(ZDISM_SECTION, "IgniRange", 650.0f, FALSE);
			if (zoptions->EntryExists(ZDISM_SECTION, "IgniStreamRange") &&
				fabsf(zoptions->ReadReal(ZDISM_SECTION, "IgniStreamRange", 0) - 500.0f) < 0.5f)
				zoptions->WriteReal(ZDISM_SECTION, "IgniStreamRange", 700.0f, FALSE);
			zoptions->WriteInt(ZDISM_SECTION, "SignDefaultsRev", 5, FALSE);
		}
		// Runda 7: Axii wybiera cel sam w waskim stozku - zasieg 800 -> 1000 cm, stozek +-30 -> +-20 st. (tylko stare domyslne)
		if (zoptions->ReadInt(ZDISM_SECTION, "SignDefaultsRev", 0) < 7) {
			if (zoptions->EntryExists(ZDISM_SECTION, "AxiiRange"))
				zoptions->WriteReal(ZDISM_SECTION, "AxiiRange",
					zdcore::MigrateDefault(zoptions->ReadReal(ZDISM_SECTION, "AxiiRange", 0), 800.0f, 1000.0f), FALSE);
			if (zoptions->EntryExists(ZDISM_SECTION, "AxiiCone"))
				zoptions->WriteReal(ZDISM_SECTION, "AxiiCone",
					zdcore::MigrateDefault(zoptions->ReadReal(ZDISM_SECTION, "AxiiCone", 0), 30.0f, 20.0f), FALSE);
			zoptions->WriteInt(ZDISM_SECTION, "SignDefaultsRev", 7, FALSE);
		}
		signs = ReadIntDef("Signs", 1, 0, 1) != 0;
		{
			static const int defIds[5] = { 90, 91, 92, 93, 94 }, defCost[5] = { 10, 15, 15, 20, 15 };
			std::string s = ReadStringDef("SignSpellIds", "90,91,92,93,94");
			if (!zdcore::ParseIntList(s, signSpellIds, 5)) {
				ZLog("Config: zla wartosc SignSpellIds=%s, zostaje 90,91,92,93,94", s.c_str());
				std::copy(defIds, defIds + 5, signSpellIds);
			}
			// Zrodlo prawdy dla run jest skrypt (C_Item.spell = SPL_Aard ...): paczka New Balance ma 196..200, Steam 90..94.
			// Przy niezgodnosci z ini wygrywa skrypt (zdcore::ResolveSignIds) - ini sie nie zmienia, tylko log.
			if (parser) {
				static const char* const SPL[5] = { "SPL_AARD", "SPL_IGNI", "SPL_AXII", "SPL_YRDEN", "SPL_QUEN" };
				int script[5];
				for (int i = 0; i < 5; i++) {
					script[i] = -1;
					if (zCPar_Symbol* sym = parser->GetSymbol(zSTRING(SPL[i])))
						if (sym->type == zPAR_TYPE_INT && (sym->flags & zPAR_FLAG_CONST))
							sym->GetValue(script[i], 0);
				}
				int resolved[5];
				int r = zdcore::ResolveSignIds(signSpellIds, script, resolved);
				if (r > 0)
					ZLog("Config: SignSpellIds=%s w ini, a stale SPL_Aard..SPL_Quen w skrypcie = %s - uzywam skryptu (popraw Gothic.ini)",
						zdcore::SignIdsLabel(signSpellIds).c_str(), zdcore::SignIdsLabel(resolved).c_str());
				else if (r < 0)
					ZLog("Config: stale SPL_Aard..SPL_Quen ze skryptu dalyby dwa znaki w jednym slocie - zostaje SignSpellIds=%s z ini",
						zdcore::SignIdsLabel(signSpellIds).c_str());
				std::copy(resolved, resolved + 5, signSpellIds);
			}
			s = ReadStringDef("SignManaCost", "10,15,15,20,15");
			if (!zdcore::ParseIntList(s, signManaCost, 5)) {
				ZLog("Config: zla wartosc SignManaCost=%s, zostaje 10,15,15,20,15", s.c_str());
				std::copy(defCost, defCost + 5, signManaCost);
			}
			s = ReadStringDef("SignAniLetters", "WND,FBT,SLE,FEA,HEA");
			if (!zdcore::ParseSignLetters(s, signAniLetters)) {
				ZLog("Config: zla wartosc SignAniLetters=%s, zostaje WND,FBT,SLE,FEA,HEA", s.c_str());
				zdcore::ParseSignLetters("WND,FBT,SLE,FEA,HEA", signAniLetters);
			}
		}
		signCooldown = ReadRealDef("SignCooldown", 1.0f, 0.0f, 60.0f);
		signHitNeutral = ReadIntDef("SignHitNeutral", 1, 0, 1) != 0;
		aardDamage = ReadRealDef("AardDamage", 15.0f, 0.0f, 1000.0f);
		aardRange = ReadRealDef("AardRange", 600.0f, 100.0f, 3000.0f);
		aardCone = ReadRealDef("AardCone", 50.0f, 5.0f, 180.0f);
		aardPush = ReadRealDef("AardPush", 150.0f, 0.0f, 600.0f);
		aardFlyMaxProt = ReadIntDef("AardFlyMaxProt", 160, -1, 1000);
		aardFlyMaxHeight = ReadRealDef("AardFlyMaxHeight", 230.0f, 0.0f, 2000.0f);
		aardFlyCheck = ReadRealDef("AardFlyCheck", 0.25f, 0.05f, 2.0f);
		aardFlyMinMove = ReadRealDef("AardFlyMinMove", 50.0f, 0.0f, 500.0f);
		aardFallbackPush = ReadRealDef("AardFallbackPush", 250.0f, 0.0f, 800.0f);
		igniDamage = ReadRealDef("IgniDamage", 40.0f, 0.0f, 1000.0f);
		igniRange = ReadRealDef("IgniRange", 650.0f, 100.0f, 3000.0f);
		igniCone = ReadRealDef("IgniCone", 35.0f, 5.0f, 180.0f);
		igniBurnTime = ReadRealDef("IgniBurnTime", 3.0f, 0.0f, 60.0f);
		igniBurnDps = ReadRealDef("IgniBurnDps", 5.0f, 0.0f, 500.0f);
		igniReactTime = ReadRealDef("IgniReactTime", 1.5f, 0.0f, 10.0f);
		axiiStunTime = ReadRealDef("AxiiStunTime", 4.0f, 0.5f, 30.0f);
		axiiRange = ReadRealDef("AxiiRange", 1000.0f, 100.0f, 3500.0f);
		axiiCone = ReadRealDef("AxiiCone", 20.0f, 5.0f, 180.0f);
		axiiFinisher = ReadIntDef("AxiiFinisher", 1, 0, 1) != 0;
		{
			// WOUNDED / WOUNDEDB tylko z ini: to powalenie (4,9 / 3,4 s w dol, 8,5 / 10 s wstawania), nie oszolomienie 4 s
			std::string s = ReadStringDef("AxiiStunAnis", "PRAY,FEAR1,FEAR2,FEAR3");
			bool ok = true;
			axiiStunMask = zdcore::ParseStunAnis(s, &ok);
			if (!ok || !axiiStunMask) {
				ZLog("Config: zla wartosc AxiiStunAnis=%s (PRAY, FEAR1, FEAR2, FEAR3, WOUNDED, WOUNDEDB)%s", s.c_str(),
					axiiStunMask ? " - znane nazwy zostaja" : ", zostaje PRAY,FEAR1,FEAR2,FEAR3");
				if (!axiiStunMask)
					axiiStunMask = zdcore::ParseStunAnis("PRAY,FEAR1,FEAR2,FEAR3", nullptr);
			}
		}
		yrdenRadius = ReadRealDef("YrdenRadius", 500.0f, 100.0f, 3000.0f);
		yrdenTime = ReadRealDef("YrdenTime", 15.0f, 1.0f, 300.0f);
		yrdenSlow = ReadRealDef("YrdenSlow", 0.65f, 0.1f, 1.0f);
		yrdenArmor = ReadRealDef("YrdenArmor", 30.0f, 0.0f, 100.0f);
		quenTime = ReadRealDef("QuenTime", 30.0f, 1.0f, 600.0f);
		quenRadius = ReadRealDef("QuenRadius", 250.0f, 0.0f, 2000.0f);
		quenDamage = ReadRealDef("QuenDamage", 10.0f, 0.0f, 1000.0f);
		quenPush = ReadRealDef("QuenPush", 120.0f, 0.0f, 600.0f);
		// Znaki - runda 3 (projekt: sekcja "Runda 3")
		signPowerPerMana = ReadRealDef("SignPowerPerMana", 0.001f, 0.0f, 0.1f);
		signPowerMax = ReadRealDef("SignPowerMax", 1.0f, 0.0f, 10.0f);
		signPowerInt = ReadRealDef("SignPowerInt", 10.0f, 0.0f, 100.0f);
		signPowerIntMax = ReadRealDef("SignPowerIntMax", 5.0f, 1.0f, 50.0f);
		signPowerSource = ReadIntDef("SignPowerSource", 2, 0, 2);
		signDamageArray = ReadIntDef("SignDamageArray", -1, -1, 1);
		signAltHold = ReadRealDef("SignAltHold", 0.5f, 0.0f, 3.0f);   // 0,5 s od celowania od wcisniecia F (bylo 0,35)
		yrdenSlowMin = ReadRealDef("YrdenSlowMin", 0.40f, 0.1f, 1.0f);
		yrdenArmorMax = ReadRealDef("YrdenArmorMax", 50.0f, 0.0f, 100.0f);
		aardAltCost = ReadIntDef("AardAltCost", 20, 0, 1000);
		aardAltRadius = ReadRealDef("AardAltRadius", 400.0f, 100.0f, 3000.0f);
		aardAltDamage = ReadRealDef("AardAltDamage", 0.8f, 0.0f, 10.0f);
		igniStreamDps = ReadRealDef("IgniStreamDps", 30.0f, 0.0f, 1000.0f);
		igniStreamManaPerSec = ReadRealDef("IgniStreamManaPerSec", 12.0f, 0.0f, 500.0f);
		igniStreamCone = ReadRealDef("IgniStreamCone", 20.0f, 5.0f, 180.0f);
		igniStreamRange = ReadRealDef("IgniStreamRange", 700.0f, 100.0f, 3000.0f);
		igniStreamTick = ReadRealDef("IgniStreamTick", 0.25f, 0.05f, 2.0f);
		yrdenAltCost = ReadIntDef("YrdenAltCost", 25, 0, 1000);
		yrdenTrapRadius = ReadRealDef("YrdenTrapRadius", 300.0f, 100.0f, 3000.0f);
		yrdenTrapTime = ReadRealDef("YrdenTrapTime", 30.0f, 1.0f, 600.0f);
		yrdenTrapCharges = ReadIntDef("YrdenTrapCharges", 3, 1, 50);
		yrdenTrapDamage = ReadRealDef("YrdenTrapDamage", 35.0f, 0.0f, 1000.0f);
		yrdenTrapInterval = ReadRealDef("YrdenTrapInterval", 0.8f, 0.05f, 10.0f);
		quenActiveManaPerSec = ReadRealDef("QuenActiveManaPerSec", 10.0f, 0.0f, 500.0f);
		quenActiveHeal = ReadRealDef("QuenActiveHeal", 0.5f, 0.0f, 5.0f);
		axiiAltCost = ReadIntDef("AxiiAltCost", 25, 0, 1000);
		axiiPuppetTime = ReadRealDef("AxiiPuppetTime", 15.0f, 1.0f, 300.0f);
		axiiPuppetRange = ReadRealDef("AxiiPuppetRange", 1500.0f, 200.0f, 5000.0f);
		// Znaki - runda 4
		signChannelMoveScale = ReadRealDef("SignChannelMoveScale", 0.6f, 0.1f, 1.0f);
		signChannelFrame = ReadIntDef("SignChannelFrame", 10, 0, 100);
		signAltInPlace = ReadIntDef("SignAltInPlace", 1, 0, 1) != 0;
		axiiPuppetEndStun = ReadRealDef("AxiiPuppetEndStun", 1.5f, 0.0f, 30.0f);
		// Znaki - runda 5
		signFires = ReadIntDef("SignFires", 1, 0, 1) != 0;
		signAimRange = ReadRealDef("SignAimRange", 1500.0f, 200.0f, 5000.0f);
		axiiTranceTime = ReadRealDef("AxiiTranceTime", 20.0f, 0.0f, 300.0f);
		// Znaki - runda 6
		signAardUnlock = ReadIntDef("SignAardUnlock", 0, 0, 1) != 0;
		signAxiiCharm = ReadIntDef("SignAxiiCharm", 0, 0, 1) != 0;
		axiiPickpocketBonus = ReadIntDef("SignAxiiPickpocketBonus", 0, 0, 100);
		axiiPickpocketTime = ReadRealDef("SignAxiiPickpocketTime", 60.0f, 0.0f, 3600.0f);
		// Medytacja W3
		meditate = ReadIntDef("Meditate", 1, 0, 1) != 0;
		meditateKey = ReadIntDef("MeditateKey", 28, 0, 255);
		meditateKeyAlt = ReadIntDef("MeditateKeyAlt", 51, 0, 255);
		meditateKeyAltMods = ReadIntDef("MeditateKeyAltMods", 1, 0, 7);
		meditateSpeed = ReadIntDef("MeditateSpeed", 20, zdcore::MED_SPEED_MIN, zdcore::MED_SPEED_MAX);
		meditateRegen = ReadIntDef("MeditateRegen", zdcore::MED_REGEN_GRADUAL, 0, zdcore::MED_REGEN_COUNT - 1);
		meditateRegenHours = ReadIntDef("MeditateRegenHours", 8, 1, 48);
		meditateSleepHours = ReadIntDef("MeditateSleepHours", 6, 1, 24);
		meditateCam = ReadIntDef("MeditateCam", 1, 0, 1) != 0;
		meditateCamSens = ReadRealDef("MeditateCamSens", 1.0f, 0.1f, 5.0f);
		// Gildia i sciezki New Balance
		pathMenu = ReadIntDef("PathMenu", 1, 0, 1) != 0;
		pathMenuKey = ReadIntDef("PathMenuKey", 26, 0, 255);
		pathMenuKeyMods = ReadIntDef("PathMenuKeyMods", 0, 0, 7);
		pathDialogGuard = ReadIntDef("PathDialogGuard", 1, 0, 1) != 0;
		menuMouse = ReadIntDef("MenuMouse", 1, 0, 1) != 0;
		menuMouseSpeed = ReadRealDef("MenuMouseSpeed", 1.0f, 0.1f, 10.0f);
		// Miecze na plecach
		backSwords = ReadIntDef("BackSwords", 1, 0, 1) != 0;
		back1H = ReadIntDef("Back1H", zdcore::BACK1H_SWORDS, 0, zdcore::BACK1H_COUNT - 1);
		backDrawSpeed = ReadIntDef("BackDrawSpeed", 100, 25, 400);
		backSheatheSpeed = ReadIntDef("BackSheatheSpeed", 100, 25, 400);
		beltDrawSpeed = ReadIntDef("BeltDrawSpeed", 150, 25, 400);
		secondSword = ReadIntDef("SecondSword", 1, 0, 1) != 0;
		secondSwordKey = ReadIntDef("SecondSwordKey", 34, 0, 255);
		secondSwordKeyMods = ReadIntDef("SecondSwordKeyMods", 0, 0, 7);
		secondWithRanged = ReadIntDef("SecondWithRanged", zdcore::SECOND_RANGED_HIDE, 0, zdcore::SECOND_RANGED_COUNT - 1);
		for (int i = 0; i < 2; i++) {
			const char* key = i ? "BackAdjustB" : "BackAdjustA";
			std::string s = ReadStringDef(key, "0 0 0 0 0 0");
			if (!zdcore::ParseFloats(s, backAdjust[i], 6)) {
				ZLog("Config: zla wartosc %s=%s (x y z cm, rx ry rz st.) - bez przesuniecia", key, s.c_str());
				for (float& v : backAdjust[i])
					v = 0.0f;
			}
		}
		crossbow = ReadIntDef("Crossbow", 1, 0, 1) != 0;
		xbowSpeed = ReadIntDef("XbowSpeed", 100, 25, 400);
		{
			std::string s = ReadStringDef("XbowCooldownMs", "1320,1200,960");
			if (!zdcore::ParseIntList(s, xbowCooldownMs, 3)) {
				ZLog("Config: zla wartosc XbowCooldownMs=%s (3 liczby ms) - 1320,1200,960", s.c_str());
				xbowCooldownMs[0] = 1320, xbowCooldownMs[1] = 1200, xbowCooldownMs[2] = 960;
			}
		}
		xbowAutoRange = ReadRealDef("XbowAutoRange", 3000.0f, 300.0f, 10000.0f);
		xbowAutoCone = ReadRealDef("XbowAutoCone", 40.0f, 5.0f, 90.0f);
		xbowMoveCone = ReadRealDef("XbowMoveCone", 100.0f, 10.0f, 180.0f);
		xbowAutoNeutral = ReadIntDef("XbowAutoNeutral", 0, 0, 1) != 0;
		xbowWithShield = ReadIntDef("XbowWithShield", 1, 0, 1) != 0;
		xbowWithDual = ReadIntDef("XbowWithDual", 1, 0, 1) != 0;
		xbowWithTorch = ReadIntDef("XbowWithTorch", 0, 0, 1) != 0;
		xbowHitCancel = ReadIntDef("XbowHitCancel", 1, 0, 1) != 0;
		xbowHandScale = ReadIntDef("XbowHandScale", 100, 30, 200);
		{
			std::string s = ReadStringDef("XbowHandAdjust", "0 0 0 0 0 0");
			if (!zdcore::ParseFloats(s, xbowHandAdjust, 6)) {
				ZLog("Config: zla wartosc XbowHandAdjust=%s (x y z cm, rx ry rz st.) - bez przesuniecia", s.c_str());
				for (float& v : xbowHandAdjust)
					v = 0.0f;
			}
		}
		xbowMuzzleCm = ReadRealDef("XbowMuzzleGripCm", 10.0f, -30.0f, 80.0f);   // 23.10: od chwytu (stary XbowMuzzleCm - nieuzywany)
		xbowBoltSpeed = ReadRealDef("XbowBoltSpeed", 3600.0f, 1000.0f, 8000.0f);
		xbowAimSlowmo = ReadIntDef("XbowAimSlowmo", 100, 10, 100);
		wheel = ReadIntDef("Wheel", 1, 0, 1) != 0;
		wheelKey = ReadIntDef("WheelKey", 58, 1, zdcore::WHEEL_KEY_MAX);   // DIK albo przycisk myszy 0x20C..0x213
		wheelTapWalk = ReadIntDef("WheelTapWalk", 1, 0, 1) != 0;
		wheelCapsRestore = ReadIntDef("WheelCapsRestore", 1, 0, 1) != 0;
		wheelCapsPatch = ReadIntDef("WheelCapsPatch", 1, 0, 1) != 0;
		wheelHoldMs = ReadIntDef("WheelHoldMs", 250, 0, 2000);
		wheelSlowmo = ReadIntDef("WheelSlowmo", 15, 1, 100);
		wheelDeadZone = ReadIntDef("WheelDeadZone", 40, 2, 400);
		wheelRadius = ReadIntDef("WheelRadius", 300, 150, 500);
		wheelClickConfirm = ReadIntDef("WheelClickConfirm", 1, 0, 1) != 0;
		wheelMouseSens = ReadIntDef("WheelMouseSens", 100, 10, 500);
		IniToMenu();
		ZLog("Config: kolo wyboru %d - klawisz %d (stukniecie Caps Lock = chod %d, przywracanie Caps Lock %d, lata chodu %d), prog %d ms, tempo %d%%, martwa strefa %d, promien %d, LPM %d, czulosc %d%%; "
			"celowanie z kuszy %d%%", wheel, wheelKey, wheelTapWalk, wheelCapsRestore, wheelCapsPatch, wheelHoldMs, wheelSlowmo, wheelDeadZone, wheelRadius, wheelClickConfirm,
			wheelMouseSens, xbowAimSlowmo);
		ZLog("Config: enabled=%d odciecie gracz/npc=%d/%d finisher=%d daze=%d (%.1fs, karencja %.1fs) aivar=%d gibs=%d",
			enabled, chancePlayer, chanceNpc, finisherChance, dazeEnabled, dazeSeconds, dazeGrace, aivarIndex, maxGibs);
		ZLog("Config: z bronia w rece - podnoszenie %d (podpowiedz %d, %.0f cm, wrogowie %.0f cm), drzwi %d, skrzynie %d, "
			"ciala %d, ponowne dobycie %d", lootArmed, lootHint, lootRange, lootEnemyRadius, lootDoors, lootChests, lootCorpses,
			lootRedraw);
	}

	void ZConfig::SyncMenu() {
		IniToMenu();
	}
}
