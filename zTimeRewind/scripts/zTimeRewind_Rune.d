// zTimeRewind - runa "Piaski Czasu" (zParserExtender). Logika czaru jest w zTimeRewind.dll:
// plugin wpisuje TIMEREWIND do slotu czaru (Gothic.ini [ZTIMEREWIND] SpellID, domyslnie 99)
// i sam obsluguje rzucenie (hook oCSpell::CallScriptInvestedMana). Tutaj tylko przedmiot i instancja czaru.
META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

const int SPL_TimeRewind = 99;      // = SpellID w Gothic.ini

INSTANCE Spell_TimeRewind (C_Spell_Proto)
{
	time_per_mana			= 0;
	spelltype				= SPELL_NEUTRAL;
	targetCollectAlgo		= TARGET_COLLECT_CASTER;
	canTurnDuringInvest		= 0;
	targetCollectRange		= 0;
	targetCollectAzi		= 0;
	targetCollectElev		= 0;
};

func void Spell_Cast_TimeRewind()
{
};

// bez mag_circle (jak runy teleportu) - dziala bez nauki kregow
INSTANCE ItRu_TimeRewind (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= 1000;
	visual				= "ItRu_TeleportMonastery.3ds";
	material			= MAT_STONE;
	spell				= SPL_TimeRewind;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_WEAKGLIMMER";
	description			= "Piaski Czasu";
	TEXT[0]				= "Trzymaj, by cofać świat (do 30 s).";
	TEXT[1]				= NAME_Manakosten;
	COUNT[1]			= 10;
	TEXT[2]				= "Mana za sekundę cofnięcia:";
	COUNT[2]			= 5;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};
