META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

// zDismember (runda 6) - TYLKO paczka New Balance (build_vdf.py --nb dokłada katalog data_nb). Kodowanie: UTF-8 bez BOM.
// Premia kieszonkowa Axii (Gothic.ini [ZDISMEMBER] SignAxiiPickpocketBonus / SignAxiiPickpocketTime): plugin po rzucie
// Axii poza walką wpisuje instancję celu i premię (punkty procentowe) do tablic niżej, po czasie zeruje. Przed zapisem
// gry tablice są zerowane (zmienne skryptu idą do zapisu), po zapisie / wczytaniu plugin wpisuje je od nowa.
//
// Szansa kradzieży NB = RX_GetThieftChance(thief, target) - dialog DIA_Addon_Steal (opis w B_Addon_Steal), kradzież
// od tyłu (KEY_DOSTEAL), retoryka po przyłapaniu. Obie funkcje przepisane W CAŁOŚCI z NB (dekompilat PL 18.10.2026,
// DECOMPILED.d ok. linii 44825 i 418311), bez _Old: hak _Old funkcji int z parametrami C_NPC zwraca w NB 0
// (pułapka z RX_CALCNPCEXP, 13.10). Jedyna zmiana: + ZDF_AxiiThiefBonus(target) przed obcięciem do 0..100
// i dopisek ", Axii +N%" w opisie opcji kradzieży. Po aktualizacji NB porównać z nowym dekompilatem.

const int ZDF_AXII_THIEF_SLOTS = 8;
var int ZDF_AxiiThiefId[8];		// Hlp_GetInstanceID celu Axii (0 = wolny slot)
var int ZDF_AxiiThiefPct[8];	// premia w punktach procentowych

func int ZDF_AxiiThiefBonus(var C_NPC target)
{
	var int id;
	id = Hlp_GetInstanceID(target);
	if (id <= 0) { return 0; };
	if (ZDF_AxiiThiefPct[0] > 0) && (ZDF_AxiiThiefId[0] == id) { return ZDF_AxiiThiefPct[0]; };
	if (ZDF_AxiiThiefPct[1] > 0) && (ZDF_AxiiThiefId[1] == id) { return ZDF_AxiiThiefPct[1]; };
	if (ZDF_AxiiThiefPct[2] > 0) && (ZDF_AxiiThiefId[2] == id) { return ZDF_AxiiThiefPct[2]; };
	if (ZDF_AxiiThiefPct[3] > 0) && (ZDF_AxiiThiefId[3] == id) { return ZDF_AxiiThiefPct[3]; };
	if (ZDF_AxiiThiefPct[4] > 0) && (ZDF_AxiiThiefId[4] == id) { return ZDF_AxiiThiefPct[4]; };
	if (ZDF_AxiiThiefPct[5] > 0) && (ZDF_AxiiThiefId[5] == id) { return ZDF_AxiiThiefPct[5]; };
	if (ZDF_AxiiThiefPct[6] > 0) && (ZDF_AxiiThiefId[6] == id) { return ZDF_AxiiThiefPct[6]; };
	if (ZDF_AxiiThiefPct[7] > 0) && (ZDF_AxiiThiefId[7] == id) { return ZDF_AxiiThiefPct[7]; };
	return 0;
};

func int RX_GetThieftChance(var C_NPC thief, var C_NPC target)
{
	var int sChanceProc;
	var int sChance;
	var int targetDex;
	targetDex = target.attribute[ATR_DEXTERITY];
	if (RX_GetNpcVar(target, AIVRX_IsDualWarrior) > 0)
	{
		targetDex = targetDex / 10;
		if (RX_MonsterPower > 100)
		{
			targetDex = targetDex * 100 / RX_MonsterPower;
		};
	};
	if (thief.attribute[ATR_DEXTERITY] < targetDex)
	{
		sChance = targetDex - thief.attribute[ATR_DEXTERITY];
		if (Npc_IsInState(target, ZS_Unconscious))
		{
			sChance = 0;
		};
		if (sChance >= 100)
		{
			if (RX_IsLeg())
			{
				sChanceProc = 10 + PickPocketBonusCount / 12;
			}
			else
			{
				sChanceProc = 10 + PickPocketBonusCount / 6;
			};
		}
		else if (RX_IsLeg())
		{
			sChanceProc = 10 + (100 - sChance) + PickPocketBonusCount / 12;
		}
		else
		{
			sChanceProc = 10 + (100 - sChance) + PickPocketBonusCount / 6;
		};
	}
	else
	{
		sChanceProc = 100;
	};
	sChanceProc += RX_ThiefRing * 10;
	if (RX_FingersDexLetterDone)
	{
		if (RX_IsLeg())
		{
			sChanceProc += 10;
		}
		else
		{
			sChanceProc += 15;
		};
	};
	if (StrCmp("SNEAK", GetWalkModeString(hero)))
	{
		sChanceProc += 10;
	};
	if (Npc_GetTalentSkill(hero, NPC_TALENT_SNEAK) > 0)
	{
		sChanceProc += 5;
	};
	if (Npc_IsInState(target, ZS_Sleep) || Npc_IsInState(target, ZS_MagicSleep))
	{
		sChanceProc += 10;
	};
	if (Wld_IsTime(22, 0, 4, 0))
	{
		sChanceProc += 5;
	};
	if (RX_NpcHasOverlay(target, "Humans_Drunken.MDS") || RX_NpcHasOverlay(target, "Humans_DrunkenExt.MDS"))
	{
		sChanceProc = 100;
	};
	sChanceProc += ZDF_AxiiThiefBonus(target);	// zDismember: premia Axii
	if (sChanceProc > 100)
	{
		sChanceProc = 100;
	};
	if (sChanceProc < 0)
	{
		sChanceProc = 0;
	};
	return sChanceProc;
};

func void B_Addon_Steal(var C_NPC slf)
{
	var int sChance;
	var int sChanceProc;
	var string concatText;
	var int zdfBonus;
	DIA_Addon_Steal.npc = Hlp_GetInstanceID(slf);
	sChanceProc = RX_GetThieftChance(hero, slf);
	concatText = ConcatStrings(DIALOG_PICKPOCKET_NEW, IntToString(sChanceProc));
	zdfBonus = ZDF_AxiiThiefBonus(slf);	// zDismember: dopisek premii Axii
	if (zdfBonus > 0)
	{
		concatText = ConcatStrings(concatText, "%, Axii +");
		concatText = ConcatStrings(concatText, IntToString(zdfBonus));
	};
	concatText = ConcatStrings(concatText, "%)");
	DIA_Addon_Steal.description = concatText;
};
