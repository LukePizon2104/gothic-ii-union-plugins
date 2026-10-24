META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

// Znaki Wiedźmina (zDismember, moduł Signs): czary i runy. Kodowanie: UTF-8 bez BOM.
// Projekt: [dokument projektowy - niepublikowany] (nazwy wiążące z pluginem).
//
// Tu są tylko dane. Całą rozgrywkę (cele, obrażenia, odrzut, stany, strefa, tarcza, mana, cooldown) liczy
// zDismember.dll: hak oCSpell::CallScriptInvestedMana przejmuje sloty 90..94 (Spell_ProcessMana gry ich nie zna)
// i przy pierwszym Invest zwraca SPL_SENDCAST (rzut natychmiastowy). Tablic czarów (spellFxInstanceNames,
// spellFxAniLetters, TXT_SPELLS) NIE ruszamy w skryptach - plugin wpisuje AARD/IGNI/AXII/YRDEN/QUEN w runtime.
// Efekty SPELLFX_* są w zDismember_SignsVfx.d (parser VFX), cząsteczki w zDismember_SignsPfx.d, dźwięki
// w zDismember_SignsSfx.d.

// ---------------------------------------------------------------- sloty (= SignSpellIds w Gothic.ini [ZDISMEMBER])

const int SPL_Aard  = 90;
const int SPL_Igni  = 91;
const int SPL_Axii  = 92;
const int SPL_Yrden = 93;
const int SPL_Quen  = 94;

// Wartości pokazywane na runach = domyślne z ini (SignManaCost=10,15,15,20,15 i klucze znaków). Plugin liczy
// z ini, więc po zmianie ini opis runy pokaże stare liczby - to tylko tekst.
const int SPL_Cost_Aard  = 10;
const int SPL_Cost_Igni  = 15;
const int SPL_Cost_Axii  = 15;
const int SPL_Cost_Yrden = 20;
const int SPL_Cost_Quen  = 15;

const int ZDF_SIGN_AARD_DAMAGE   = 15;  // AardDamage (DAM_FLY)
const int ZDF_SIGN_AARD_RANGE_M  = 6;   // AardRange 600 cm
const int ZDF_SIGN_IGNI_DAMAGE   = 40;  // IgniDamage (DAM_FIRE)
const int ZDF_SIGN_IGNI_BURN_SEC = 3;   // IgniBurnTime
const int ZDF_SIGN_AXII_STUN_SEC = 4;   // AxiiStunTime
const int ZDF_SIGN_AXII_RANGE_M  = 8;   // AxiiRange 800 cm
const int ZDF_SIGN_YRDEN_SEC     = 15;  // YrdenTime
const int ZDF_SIGN_YRDEN_RADIUS_M = 5;  // YrdenRadius 500 cm
const int ZDF_SIGN_QUEN_SEC      = 30;  // QuenTime
const int ZDF_SIGN_QUEN_DAMAGE   = 10;  // QuenDamage

const int Value_Ru_ZdfSign = 1000;


// ---------------------------------------------------------------- czary
// Wspólne: time_per_mana = 0 (rzut natychmiastowy jak Sen/Leczenie), damage_per_level = 0 (silnik nie zadaje
// obrażeń - efekty SPELLFX_* nie mają kolizji, obrażenia daje plugin przez oCNpc::OnDamage).
//
// spelltype (Npc_GetActiveSpellCat / Npc_GetLastHitSpellCat):
// - SPELL_BAD dla Aard, Igni, Yrden: to znaki bojowe. Bohater z taką runą w trybie magii "grozi bronią"
//   (C_NpcIsBotheredByWeapon), a trafiony NPC w B_AssessMagic wysyła PERC_ASSESSFIGHTSOUND - jak przy każdym
//   wrogim czarze.
// - SPELL_NEUTRAL dla Axii (odstępstwo od wstępnej propozycji SPELL_BAD): Axii ma cel z fokusu, więc silnik
//   wywołuje na celu B_AssessMagic. Przy SPELL_BAD ten skrypt wysyła wszystkim w pobliżu ASSESSFIGHTSOUND,
//   a projekt wymaga, żeby Axii poza walką (niepamięć jak Spell_Charm) NIE budziło wrogości. Charm i Sleep
//   w grze też są SPELL_NEUTRAL. W walce oszołomienie i tak robi plugin, a wrogość już jest.
// - SPELL_NEUTRAL dla Quen: rzucany na siebie (TARGET_COLLECT_CASTER), jak wszystkie leczenia w G2.
//   SPELL_GOOD nie używa żaden czar gry, a w skryptach (C_NpcIsBotheredByWeapon) działa tak samo jak NEUTRAL.
//
// Runda 5 - Gothic Free Aim: GFA celuje tylko czarami z canTurnDuringInvest, canChangeTargetDuringInvest
// i targetCollectAlgo TARGET_COLLECT_FOCUS_FALLBACK_NONE (GFA_IsSpellEligible) - stąd te pola w Aard, Igni, Axii
// i Yrden. Bez GFA: Aard / Igni / Yrden mają kąty fokusu 0 (silnik nie zbiera fokusu - jak TARGET_COLLECT_NONE),
// Axii bierze fokus jak dotąd (bez fokusu czar też się rzuca - plugin wybiera cel w stożku).

INSTANCE Spell_Aard (C_Spell_Proto)
{
	time_per_mana			= 0;
	damage_per_level		= 0;
	damageType				= DAM_FLY;
	spelltype				= SPELL_BAD;
	canTurnDuringInvest		= 1;
	canChangeTargetDuringInvest = 1;						// Gothic Free Aim: celowanie (GFA_IsSpellEligible)
	targetCollectAlgo		= TARGET_COLLECT_FOCUS_FALLBACK_NONE;	// stożek liczy plugin; kąty 0 = bez fokusu
	targetCollectRange		= 600;						// = AardRange (promień celownika GFA)
	targetCollectAzi		= 0;
	targetCollectElev		= 0;
};

func void Spell_Cast_Aard()
{
};

INSTANCE Spell_Igni (C_Spell_Proto)
{
	time_per_mana			= 0;
	damage_per_level		= 0;
	damageType				= DAM_FIRE;
	spelltype				= SPELL_BAD;
	canTurnDuringInvest		= 1;
	canChangeTargetDuringInvest = 1;						// Gothic Free Aim: celowanie
	targetCollectAlgo		= TARGET_COLLECT_FOCUS_FALLBACK_NONE;	// stożek liczy plugin; kąty 0 = bez fokusu
	targetCollectRange		= 650;						// = IgniRange
	targetCollectAzi		= 0;
	targetCollectElev		= 0;
};

func void Spell_Cast_Igni()
{
};

INSTANCE Spell_Axii (C_Spell_Proto)
{
	time_per_mana			= 0;
	damage_per_level		= 0;
	damageType				= DAM_MAGIC;
	spelltype				= SPELL_NEUTRAL;			// uzasadnienie wyżej
	canTurnDuringInvest		= 1;
	canChangeTargetDuringInvest = 1;						// Gothic Free Aim: celowanie (cel pod celownikiem)
	targetCollectAlgo		= TARGET_COLLECT_FOCUS_FALLBACK_NONE;	// cel z fokusu; fokus albo najbliższy w stożku wybiera plugin
	targetCollectType		= TARGET_TYPE_NPCS;
	targetCollectRange		= 1000;						// = AxiiRange (domyślnie; cel i tak wybiera plugin w stożku AxiiCone)
	targetCollectAzi		= 20;							// = AxiiCone
	targetCollectElev		= 45;
};

func void Spell_Cast_Axii()
{
};

INSTANCE Spell_Yrden (C_Spell_Proto)
{
	time_per_mana			= 0;
	damage_per_level		= 0;
	damageType				= DAM_MAGIC;
	spelltype				= SPELL_BAD;
	canTurnDuringInvest		= 1;
	canChangeTargetDuringInvest = 1;						// Gothic Free Aim: krąg w punkcie celownika
	targetCollectAlgo		= TARGET_COLLECT_FOCUS_FALLBACK_NONE;	// strefa w miejscu rzucenia (z GFA: punkt celownika)
	targetCollectRange		= 1500;						// = SignAimRange
	targetCollectAzi		= 0;
	targetCollectElev		= 0;
};

func void Spell_Cast_Yrden()
{
};

INSTANCE Spell_Quen (C_Spell_Proto)
{
	time_per_mana			= 0;
	damage_per_level		= 0;
	damageType				= DAM_MAGIC;
	spelltype				= SPELL_NEUTRAL;			// na siebie, jak leczenie
	canTurnDuringInvest		= 0;
	targetCollectAlgo		= TARGET_COLLECT_CASTER;
	targetCollectRange		= 0;
	targetCollectAzi		= 0;
	targetCollectElev		= 0;
};

func void Spell_Cast_Quen()
{
};


// Bazy z AI Gothica 1 (np. Cienie Przeszłości) nie mają tej stałej opisu.
test(!NAME_Sec_Duration)
{
	const string NAME_Sec_Duration = "Czas trwania (sek.):";
};


// ---------------------------------------------------------------- runy
// Jak runy paladyna i teleportu: bez mag_circle, więc działają bez nauki kręgów. Na razie tylko "insert ItRu_Aard"
// (bez sklepów i dropu). Modele to istniejące runy gry (Meshes.vdf), każdy znak inny. Poświata WEAR_EFFECT
// w kolorze znaku: SPELLFX_ZDF_SIGN_RUNE_* (zDismember_SignsVfx.d).

INSTANCE ItRu_Aard (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= Value_Ru_ZdfSign;
	visual				= "ItRu_Windfist.3ds";		// Pięść Wiatru - pchnięcie
	material			= MAT_STONE;
	spell				= SPL_Aard;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_ZDF_SIGN_RUNE_AARD";
	description			= "Znak Aard";
	TEXT[0]				= "Fala telekinetyczna w stożku";
	TEXT[1]				= "odrzuca wrogów przed tobą.";
	TEXT[2]				= NAME_Manakosten;
	COUNT[2]			= SPL_Cost_Aard;
	TEXT[3]				= NAME_Dam_Fly;
	COUNT[3]			= ZDF_SIGN_AARD_DAMAGE;
	TEXT[4]				= "Zasięg (m):";
	COUNT[4]			= ZDF_SIGN_AARD_RANGE_M;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};

INSTANCE ItRu_Igni (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= Value_Ru_ZdfSign;
	visual				= "ItRu_Firestorm.3ds";		// Burza Ognia - ogień w szerz
	material			= MAT_STONE;
	spell				= SPL_Igni;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_ZDF_SIGN_RUNE_IGNI";
	description			= "Znak Igni";
	TEXT[0]				= "Strumień ognia w stożku";
	TEXT[1]				= "podpala wrogów przed tobą.";
	TEXT[2]				= NAME_Manakosten;
	COUNT[2]			= SPL_Cost_Igni;
	TEXT[3]				= NAME_Dam_Fire;
	COUNT[3]			= ZDF_SIGN_IGNI_DAMAGE;
	TEXT[4]				= "Płonięcie (sek.):";
	COUNT[4]			= ZDF_SIGN_IGNI_BURN_SEC;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};

INSTANCE ItRu_Axii (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= Value_Ru_ZdfSign;
	visual				= "ItRu_Sleep.3ds";			// Sen - hipnoza
	material			= MAT_STONE;
	spell				= SPL_Axii;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_ZDF_SIGN_RUNE_AXII";
	description			= "Znak Axii";
	TEXT[0]				= "Poza walką cel zapomina winy,";
	TEXT[1]				= "w walce zostaje oszołomiony.";
	TEXT[2]				= NAME_Manakosten;
	COUNT[2]			= SPL_Cost_Axii;
	TEXT[3]				= "Oszołomienie (sek.):";
	COUNT[3]			= ZDF_SIGN_AXII_STUN_SEC;
	TEXT[4]				= "Zasięg (m):";
	COUNT[4]			= ZDF_SIGN_AXII_RANGE_M;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};

INSTANCE ItRu_Yrden (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= Value_Ru_ZdfSign;
	visual				= "ItRu_Fear.3DS";			// Strach - czar obszarowy
	material			= MAT_STONE;
	spell				= SPL_Yrden;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_ZDF_SIGN_RUNE_YRDEN";
	description			= "Znak Yrden";
	TEXT[0]				= "Magiczny krąg spowalnia wrogów";
	TEXT[1]				= "i osłabia ich obronę.";
	TEXT[2]				= NAME_Manakosten;
	COUNT[2]			= SPL_Cost_Yrden;
	TEXT[3]				= NAME_Sec_Duration;
	COUNT[3]			= ZDF_SIGN_YRDEN_SEC;
	TEXT[4]				= "Promień (m):";
	COUNT[4]			= ZDF_SIGN_YRDEN_RADIUS_M;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};

INSTANCE ItRu_Quen (C_Item)
{
	name				= NAME_Rune;
	mainflag			= ITEM_KAT_RUNE;
	flags				= 0;
	value				= Value_Ru_ZdfSign;
	visual				= "ItRu_PalFullHeal.3ds";	// paladyńska - ochrona
	material			= MAT_STONE;
	spell				= SPL_Quen;
	wear				= WEAR_EFFECT;
	effect				= "SPELLFX_ZDF_SIGN_RUNE_QUEN";
	description			= "Znak Quen";
	TEXT[0]				= "Tarcza pochłania jedno trafienie,";
	TEXT[1]				= "pękając odrzuca wrogów wokół.";
	TEXT[2]				= NAME_Manakosten;
	COUNT[2]			= SPL_Cost_Quen;
	TEXT[3]				= NAME_Sec_Duration;
	COUNT[3]			= ZDF_SIGN_QUEN_SEC;
	TEXT[4]				= NAME_Damage;
	COUNT[4]			= ZDF_SIGN_QUEN_DAMAGE;
	TEXT[5]				= NAME_Value;
	COUNT[5]			= value;
};


// ---------------------------------------------------------------- stany AI: oszołomienie Axii, reakcja palenia Igni
// Plugin (Signs.cpp) ustawia ZDF_Sign_State / ZDF_Sign_StunSec / ZDF_Sign_BurnSec i woła ZDF_Sign_StartState
// z self = cel, other = bohater - jak B_AssessMagic przy Lodowym bloku / Śnie: czyszczenie kolejki i percepcji,
// potem AI_StartState. Stan przerywa walkę czysto: AI nie wydaje już ataków, animacja wejścia rusza z bieżącej
// pozy (bez wymuszania co klatkę). Koniec (w pętli, jak B_StopMagicFreeze):
// - czas stanu >= ZDF_Sign_StunSec (plugin ustawia czas stanu dokładnie po AxiiStunTime / IgniReactTime) - animacja
//   wstawania wariantu;
// - trafienie (PERC_ASSESSDAMAGE, tylko oszołomienie) - czas stanu 2000 s, bez animacji wstawania (wstaje reakcja
//   na cios);
// potem powrót do walki: ludzie B_AssessDamage, potwory i orkowie wrogość (dalej rutyna -> atak).
// Numery wariantów = zdcore::StunVariant w pluginie (Core\Signs.h).

const int ZDF_SIGN_STATE_PRAY     = 1;	// klęczenie (jak Daze; jedyny wariant z egzekucją / finisherem)
const int ZDF_SIGN_STATE_FEAR1    = 2;	// skulenie ze strachu
const int ZDF_SIGN_STATE_FEAR2    = 3;
const int ZDF_SIGN_STATE_FEAR3    = 4;
const int ZDF_SIGN_STATE_WOUNDED  = 5;	// powalenie (tylko z ini AxiiStunAnis - długie animacje)
const int ZDF_SIGN_STATE_WOUNDEDB = 6;
const int ZDF_SIGN_STATE_MONSTER  = 7;	// potwory i orkowie: zatoczenie, potem stoją bez AI walki
const int ZDF_SIGN_STATE_BURN     = 8;	// Igni: miotanie się w ogniu (S_FIRE_VICTIM)
const int ZDF_SIGN_STATE_TRANCE   = 9;	// Axii poza walką (runda 5): trans - ręce skrzyżowane, bez percepcji bohatera
const int ZDF_SIGN_STATE_HIT_SEC  = 2000;
const int ZDF_SIGN_TRANCE_LIMIT_SEC = 600;	// bezpiecznik transu - koniec ustawia plugin (czas stanu 600 s + 1 ms)

var int ZDF_Sign_State;
var int ZDF_Sign_StunSec;		// AxiiStunTime zaokrąglony w górę (plugin)
var int ZDF_Sign_BurnSec;		// IgniReactTime zaokrąglony w górę (plugin)

func int ZDF_Sign_StunLimit()
{
	if (ZDF_Sign_StunSec > 0)
	{
		return ZDF_Sign_StunSec;
	};
	return ZDF_SIGN_AXII_STUN_SEC;
};

func int ZDF_Sign_BurnLimit()
{
	if (ZDF_Sign_BurnSec > 0)
	{
		return ZDF_Sign_BurnSec;
	};
	return 2;
};

// ---------------------------------------------------------------- zgodność z bazami z AI Gothica 1
// Autorun parsuje się w każdym modzie. Bazy z AI G1 (np. Cienie Przeszłości) nie mają B_AssessDamage,
// B_MM_AssessDamage, B_Attack, B_ClearPerceptions ani AR_*: reakcja na cios to tam stan ZS_ReactToDamage
// (ludzie) i B_MM_ReactToDamage (potwory), atak = cel + ZS_Attack. W G2 gałęzie test() dają dawny kod.

// self = ofiara, other = napastnik
func void ZDF_ReactToDamage()
{
	test(B_AssessDamage)
	{
		B_AssessDamage();
	};
	test(!B_AssessDamage)
	{
		test(ZS_ReactToDamage)
		{
			AI_StartState(self, ZS_ReactToDamage, 0, "");
		};
	};
};

func void ZDF_MM_ReactToDamage()
{
	test(B_MM_AssessDamage)
	{
		B_MM_AssessDamage();
	};
	test(!B_MM_AssessDamage)
	{
		test(B_MM_ReactToDamage)
		{
			B_MM_ReactToDamage();
		};
	};
};

func void ZDF_ClearPerceptions(var C_NPC slf)
{
	test(B_ClearPerceptions)
	{
		B_ClearPerceptions(slf);
	};
	test(!B_ClearPerceptions)
	{
		// numery percepcji silnika (PERC_ASSESSPLAYER = 1 .. PERC_ASSESSUSEMOB = 32), te same w G1 i G2
		Npc_PercDisable(slf, 1);  Npc_PercDisable(slf, 2);  Npc_PercDisable(slf, 3);  Npc_PercDisable(slf, 4);
		Npc_PercDisable(slf, 5);  Npc_PercDisable(slf, 6);  Npc_PercDisable(slf, 7);  Npc_PercDisable(slf, 8);
		Npc_PercDisable(slf, 9);  Npc_PercDisable(slf, 10); Npc_PercDisable(slf, 11); Npc_PercDisable(slf, 12);
		Npc_PercDisable(slf, 13); Npc_PercDisable(slf, 14); Npc_PercDisable(slf, 15); Npc_PercDisable(slf, 16);
		Npc_PercDisable(slf, 17); Npc_PercDisable(slf, 18); Npc_PercDisable(slf, 19); Npc_PercDisable(slf, 20);
		Npc_PercDisable(slf, 21); Npc_PercDisable(slf, 22); Npc_PercDisable(slf, 23); Npc_PercDisable(slf, 24);
		Npc_PercDisable(slf, 25); Npc_PercDisable(slf, 26); Npc_PercDisable(slf, 27); Npc_PercDisable(slf, 28);
		Npc_PercDisable(slf, 29); Npc_PercDisable(slf, 30); Npc_PercDisable(slf, 31); Npc_PercDisable(slf, 32);
	};
};

// atak człowieka na oth (marionetka / koniec marionetki)
func void ZDF_HumanAttack(var C_NPC slf, var C_NPC oth)
{
	test(B_Attack)
	{
		test(AR_GuildEnemy)
		{
			B_Attack(slf, oth, AR_GuildEnemy, 0);
		};
	};
	test(!B_Attack)
	{
		Npc_SetTarget(slf, oth);
		Npc_SetTempAttitude(slf, ATT_HOSTILE);
		AI_StartState(slf, ZS_Attack, 0, "");
	};
};

// percepcja obrażeń w oszołomieniu: koniec stanu w najbliższej pętli
func void B_ZDF_SignStateHit()
{
	Npc_SetStateTime(self, ZDF_SIGN_STATE_HIT_SEC);
};

func void B_ZDF_SignStateBegin(var int damageBreaks)
{
	if (damageBreaks)
	{
		Npc_PercEnable(self, PERC_ASSESSDAMAGE, B_ZDF_SignStateHit);
	};
	Npc_PercEnable(self, PERC_ASSESSMAGIC, B_AssessMagic);
	B_StopLookAt(self);
	AI_StopPointAt(self);
};

func void B_ZDF_SignStateEnd(var string getUp)
{
	Npc_PercEnable(self, PERC_ASSESSMAGIC, B_AssessMagic);
	Npc_ClearAIQueue(self);
	if (!Hlp_StrCmp(getUp, ""))
	{
		AI_PlayAni(self, getUp);
	};
	if (self.guild < GIL_SEPERATOR_HUM)
	{
		ZDF_ReactToDamage();
	}
	else
	{
		Npc_SetTempAttitude(self, ATT_HOSTILE);
	};
};

func int B_ZDF_StunLoop(var string getUp)
{
	var int t; t = Npc_GetStateTime(self);
	if (t >= ZDF_SIGN_STATE_HIT_SEC)
	{
		B_ZDF_SignStateEnd("");
		return LOOP_END;
	};
	if (t >= ZDF_Sign_StunLimit())
	{
		B_ZDF_SignStateEnd(getUp);
		return LOOP_END;
	};
	return LOOP_CONTINUE;
};

func void ZS_ZDF_StunPray()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_PRAY");
};
func int ZS_ZDF_StunPray_Loop()
{
	return B_ZDF_StunLoop("T_PRAY_2_STAND");
};
func void ZS_ZDF_StunPray_End()
{
};

func void ZS_ZDF_StunFear1()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_FEAR_VICTIM1");
};
func int ZS_ZDF_StunFear1_Loop()
{
	return B_ZDF_StunLoop("T_FEAR_VICTIM_2_STAND");
};
func void ZS_ZDF_StunFear1_End()
{
};

func void ZS_ZDF_StunFear2()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_FEAR_VICTIM2");
};
func int ZS_ZDF_StunFear2_Loop()
{
	return B_ZDF_StunLoop("T_FEAR_VICTIM_2_STAND");
};
func void ZS_ZDF_StunFear2_End()
{
};

func void ZS_ZDF_StunFear3()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_FEAR_VICTIM3");
};
func int ZS_ZDF_StunFear3_Loop()
{
	return B_ZDF_StunLoop("T_FEAR_VICTIM_2_STAND");
};
func void ZS_ZDF_StunFear3_End()
{
};

func void ZS_ZDF_StunWounded()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_WOUNDED");
};
func int ZS_ZDF_StunWounded_Loop()
{
	return B_ZDF_StunLoop("T_WOUNDED_2_STAND");
};
func void ZS_ZDF_StunWounded_End()
{
};

func void ZS_ZDF_StunWoundedB()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STAND_2_WOUNDEDB");
};
func int ZS_ZDF_StunWoundedB_Loop()
{
	return B_ZDF_StunLoop("T_WOUNDEDB_2_STAND");
};
func void ZS_ZDF_StunWoundedB_End()
{
};

func void ZS_ZDF_StunMonster()
{
	B_ZDF_SignStateBegin(TRUE);
	AI_PlayAni(self, "T_STUMBLEB");
};
func int ZS_ZDF_StunMonster_Loop()
{
	return B_ZDF_StunLoop("");
};
func void ZS_ZDF_StunMonster_End()
{
};

// Axii poza walką (runda 5): trans. Stan bez PERC_ASSESSPLAYER / ASSESSTALK / ASSESSENEMY / ASSESSWARN itd., więc NPC
// nie widzi bohatera: nie zagaduje go (B_AssessPlayer -> Npc_CheckInfo important -> B_AssessTalk: straż przy bramie
// górnego miasta, w porcie, strażnicy przejść z AIV_Guardpassage_Status), nie zatrzymuje, nie reaguje na skradanie
// i dobytą broń; rozmowy z nim nie da się też zacząć. Plugin przed transem robi niepamięć (Spell_Logic_Charm - jak
// zwykła Axii poza walką). Trafienie przerywa trans: zwykła reakcja na cios (B_AssessDamage). Koniec po czasie
// (AxiiTranceTime x moc - plugin ustawia czas stanu za bezpiecznikiem): ręce w dół i powrót do rutyny bez wrogości
// i bez pamięci, że bohater przechodził (B_AssessPlayer liczy od nowa - strażnik bramy, który już nie widzi bohatera
// w zasięgu rozmowy, kasuje swój AIV_Guardpassage_Status). Przed zapisem plugin kończy stan (EndCurrentState).
func void B_ZDF_TranceHit()
{
	Npc_SetStateTime(self, ZDF_SIGN_STATE_HIT_SEC);
	ZDF_ReactToDamage();
};

func void ZS_ZDF_StunTrance()
{
	Npc_PercEnable(self, PERC_ASSESSDAMAGE, B_ZDF_TranceHit);
	Npc_PercEnable(self, PERC_ASSESSMAGIC, B_AssessMagic);
	B_StopLookAt(self);
	AI_StopPointAt(self);
	AI_StandUp(self);
	AI_PlayAni(self, "T_STAND_2_LGUARD");
};
func int ZS_ZDF_StunTrance_Loop()
{
	var int t; t = Npc_GetStateTime(self);
	if (t >= ZDF_SIGN_STATE_HIT_SEC)
	{
		return LOOP_END;			// trafienie - B_AssessDamage już wystartował reakcję
	};
	if (t >= ZDF_SIGN_TRANCE_LIMIT_SEC)
	{
		Npc_ClearAIQueue(self);
		AI_PlayAni(self, "T_LGUARD_2_STAND");
		return LOOP_END;			// dalej rutyna (start_aistate / plan dnia) - bez wrogości
	};
	return LOOP_CONTINUE;
};
func void ZS_ZDF_StunTrance_End()
{
};

// Igni: jak ZS_MagicBurnShort (S_FIRE_VICTIM), bez percepcji obrażeń - takty palenia nie przerywają reakcji.
// Wyjście zatoczeniem (T_STUMBLEB ma przejście 0,1 s) zamiast skoku do postawy.
func void ZS_ZDF_Burn()
{
	B_ZDF_SignStateBegin(FALSE);
	Npc_PlayAni(self, "S_FIRE_VICTIM");
};
func int ZS_ZDF_Burn_Loop()
{
	if (Npc_GetStateTime(self) >= ZDF_Sign_BurnLimit())
	{
		B_ZDF_SignStateEnd("T_STUMBLEB");
		return LOOP_END;
	};
	return LOOP_CONTINUE;
};
func void ZS_ZDF_Burn_End()
{
};

func void ZDF_Sign_StartState()
{
	Npc_ClearAIQueue(self);
	ZDF_ClearPerceptions(self);
	if (ZDF_Sign_State == ZDF_SIGN_STATE_PRAY)
	{
		AI_StartState(self, ZS_ZDF_StunPray, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_FEAR1)
	{
		AI_StartState(self, ZS_ZDF_StunFear1, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_FEAR2)
	{
		AI_StartState(self, ZS_ZDF_StunFear2, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_FEAR3)
	{
		AI_StartState(self, ZS_ZDF_StunFear3, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_WOUNDED)
	{
		AI_StartState(self, ZS_ZDF_StunWounded, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_WOUNDEDB)
	{
		AI_StartState(self, ZS_ZDF_StunWoundedB, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_MONSTER)
	{
		AI_StartState(self, ZS_ZDF_StunMonster, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_BURN)
	{
		AI_StartState(self, ZS_ZDF_Burn, 0, "");
	}
	else if (ZDF_Sign_State == ZDF_SIGN_STATE_TRANCE)
	{
		AI_StartState(self, ZS_ZDF_StunTrance, 0, "");
	};
};


// ---------------------------------------------------------------- Axii - tryb alternatywny: Marionetka
// Ludzie, orkowie i potwory - jak przywołaniec (ZS_MM_Rtn_Summoned): plugin ustawia postawę marionetki do bohatera
// chwilowo PRZYJAZNĄ (pola attitude / tmp_attitude) i AIV_PARTYMEMBER = TRUE (ZS_Attack / ZS_MM_Attack ignorują wtedy
// przypadkowe ciosy bohatera), co 0,5 s wybiera najbliższego wroga bohatera i woła ZDF_Sign_PuppetAttack (self =
// marionetka, other = wróg) - atak jak B_Attack / B_MM_AssessEnemy przywołańca, bez B_SetAttitude i bez przestępstw.
// Bez wrogów w pobliżu: ZDF_Sign_PuppetFollow - stan ZS_ZDF_Puppet (idzie za bohaterem, stoi przy nim; bez licznika
// czasu przywołania i bez śmierci przy rozmowie z ZS_MM_Rtn_Summoned). Koniec / zapis / śmierć: plugin przywraca
// DOKŁADNIE stare wartości; po czasie krótkie oszołomienie (AxiiPuppetEndStun) albo ZDF_Sign_PuppetEnd (self =
// marionetka, other = bohater). Przed zapisem plugin kończy stan ZS_ZDF_Puppet (nazwa stanu nie trafia do zapisu).

func void ZDF_Sign_PuppetAttack()
{
	Npc_ClearAIQueue(self);
	ZDF_ClearPerceptions(self);
	Npc_SetTarget(self, other);
	if (self.guild < GIL_SEPERATOR_HUM)
	{
		test(AR_GuildEnemy) { self.aivar[AIV_ATTACKREASON] = AR_GuildEnemy; };
		AI_StartState(self, ZS_Attack, 0, "");
	}
	else
	{
		AI_StartState(self, ZS_MM_Attack, 0, "");
	};
};

// trafienie w stanie ZS_ZDF_Puppet: od bohatera ignorowane, od innych - zwykła reakcja (atak napastnika)
func void B_ZDF_PuppetDamage()
{
	if (Npc_IsPlayer(other))
	{
		return;
	};
	if (self.guild < GIL_SEPERATOR_HUM)
	{
		ZDF_ReactToDamage();
	}
	else
	{
		ZDF_MM_ReactToDamage();
	};
};

func void ZS_ZDF_Puppet()
{
	Npc_SetPercTime(self, 1);
	Npc_PercEnable(self, PERC_ASSESSDAMAGE, B_ZDF_PuppetDamage);
	Npc_PercEnable(self, PERC_ASSESSMAGIC, B_AssessMagic);
	AI_StandUp(self);
	AI_SetWalkmode(self, NPC_RUN);
};
func int ZS_ZDF_Puppet_Loop()
{
	if (Npc_GetDistToNpc(self, hero) > 500)
	{
		AI_GotoNpc(self, hero);
	}
	else if (Npc_GetStateTime(self) >= 1)
	{
		if (!Npc_CanSeeNpc(self, hero))
		{
			AI_TurnToNpc(self, hero);
		};
		Npc_SetStateTime(self, 0);
	};
	return LOOP_CONTINUE;
};
func void ZS_ZDF_Puppet_End()
{
};

func void ZDF_Sign_PuppetFollow()
{
	Npc_ClearAIQueue(self);
	ZDF_ClearPerceptions(self);
	AI_StartState(self, ZS_ZDF_Puppet, 0, "");
};

func void ZDF_Sign_PuppetEnd()
{
	Npc_ClearAIQueue(self);
	ZDF_ClearPerceptions(self);
	if (Npc_GetAttitude(self, other) == ATT_HOSTILE)
	{
		if (self.guild < GIL_SEPERATOR_HUM)
		{
			ZDF_HumanAttack(self, other);
		}
		else
		{
			Npc_SetTarget(self, other);
			AI_StartState(self, ZS_MM_Attack, 0, "");
		};
	}
	else
	{
		AI_ContinueRoutine(self);
	};
};
