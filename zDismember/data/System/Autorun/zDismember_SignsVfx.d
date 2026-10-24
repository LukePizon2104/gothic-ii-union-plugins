META
{
	Engine = G2A;
	Parser = VFX;
	MergeMode = 1;
};

// Znaki Wiedźmina (zDismember): efekty VisualFX. Kodowanie: UTF-8 bez BOM.
// Cząsteczki ZDF_SIGN_* są w zDismember_SignsPfx.d, dźwięki ZDF_SIGN_* w zDismember_SignsSfx.d.
//
// ŻADEN efekt nie ma kolizji ani obrażeń (emCheckCollision 0, puste emActionColl*/emFXColl*, sendAssessMagic 0):
// obrażenia, odrzut i stany liczy plugin, silnik nie może ich zadać drugi raz.
//
// 1) SPELLFX_AARD/IGNI/AXII/YRDEN/QUEN - efekt czaru, tworzy go silnik przy rzucie (jak SPELLFX_SLEEP i
//    SPELLFX_HEAL): poświata przy dłoni (ZS_RIGHTHAND, FIXED) od dobycia runy, przy rzucie _KEY_CAST wyłącza
//    jej pętlę (pfx_ppsIsLoopingChg) i tworzy SPELLFX_<ZNAK>_FLASH: iskierki przy dłoni z jednorazowym
//    dźwiękiem rzutu (żyją dłużej niż dźwięk) i rozbłysk ze światłem _FLASH_BURST.
//
// 2) Efekty skutku - plugin: oCVisualFX::CreateAndPlay(nazwa, vob, cel, 1, 0, 0, FALSE) (bez pocisku;
//    cel nie ma znaczenia przy FIXED). Pozycja i kierunek liczą się z voba-źródła:
//    - emTrjOriginNode "=" (węzła o tej nazwie nie ma) => trafo voba: dla bohatera środek modelu
//      (ok. 95 cm nad ziemią), oś +Z = przód. Stożki AARD_WAVE / IGNI_CONE lecą tam, gdzie patrzy bohater
//      w chwili wywołania (FIXED: efekt idzie za bohaterem, ale wystrzelone cząstki zostają w świecie).
//      Wołać z celem NULL albo bohaterem - bez różnicy.
//    - emTrjOriginNode "BIP01" / "BIP01 HEAD" => węzeł modelu (brak węzła => środek voba).
//    - YRDEN_ZONE: CreateAndPlay(nazwa, zVEC3 punkt_NA_ZIEMI, NULL, ...) - silnik stawia w tym punkcie
//      pomocniczy vob "<nazwa>_VFX" (prosto, bez obrotu) i bierze go za źródło. Cząstki strefy są 3-10 cm
//      nad tym punktem; emTrjMode "NONE" = efekt stoi w miejscu.
//
// 3) Pętle (AXII_TARGET, YRDEN_ZONE, YRDEN_SLOW, QUEN_SHIELD) trwają do zatrzymania przez plugin:
//    fx->Stop(TRUE) na zwróconym korzeniu (Stop schodzi też na dzieci z fxList/childList, gasi emisję,
//    cząstki dogasają, potem silnik usuwa efekt) i Release. Bezpiecznik na wypadek braku zatrzymania: każda
//    część pętli ma własny emFXLifeSpan = 2 x domyślny czas z ini (AXII_TARGET 8 s, YRDEN_ZONE 30 s,
//    YRDEN_SLOW 30 s, QUEN_SHIELD 60 s); po nim pętla gaśnie sama. Gdy ini da czas dłuższy niż bezpiecznik:
//    zaraz po CreateAndPlay fx->SetDuration(czas + 1) na korzeniu i na każdym fx->fxList[i] (SetDuration
//    ustawia emFXLifeSpan, licznik biegnie od utworzenia efektu).
//    Jednorazowe efekty mają emFXLifeSpan nieco dłuższy od życia cząstek; korzeń to zawsze najdłuższa część
//    (dzieci z emFXCreate_S żyją jako osobne voby i kończą się same).
//
// Światła tylko z presetów LIGHTPRESETS.ZEN gry: JUSTWHITE, FIRE, FIRESMALL, AURA, POISON, CATACLYSM, FX_LIGHT1
// (bez fioletowego - Yrden ma POISON: niebieski/turkus/biel/purpura; bez czystej zieleni - Axii ma FX_LIGHT1:
// ciemna zieleń/morska zieleń/biel, najbliżej zieleni). Presety o dużym zasięgu (JUSTWHITE,
// AURA, CATACLYSM) dostają lightRange w kluczach.


// ================================================================ AARD

INSTANCE SPELLFX_AARD (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_INIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 0;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	lightPresetName			= "JUSTWHITE";
};

INSTANCE SPELLFX_AARD_KEY_OPEN (C_ParticleFXEmitKey)
{
	lightRange				= 60;
};

INSTANCE SPELLFX_AARD_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 60;
};

INSTANCE SPELLFX_AARD_KEY_CAST (C_ParticleFXEmitKey)
{
	pfx_ppsIsLoopingChg		= 1;
	emCreateFXID			= "SPELLFX_AARD_FLASH";
	lightRange				= 0.01;
};

// runda 5: korzeń rozbłysku niesie jednorazowy dźwięk rzutu (w kluczu dłoni urywał się ok. 0,8 s po rzucie,
// razem z efektem dłoni) i żyje dłużej niż dźwięk: bladobłękitne iskierki gasnące przy dłoni.
INSTANCE SPELLFX_AARD_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_ECHO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_FLASH_BURST";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_CAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2.3;
};

// dłoń: błysk bladobłękitny i krótki podmuch (dziecko korzenia _FLASH)
INSTANCE SPELLFX_AARD_FLASH_BURST (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_FLASH_STAR";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "JUSTWHITE";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_AARD_FLASH_BURST_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 220;
};

INSTANCE SPELLFX_AARD_FLASH_STAR (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_FLASH_STAR";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 0.5;
};

// bohater, kierunek patrzenia: stożkowa fala. Korzeń = kurz z ziemi (najdłuższy), dzieci: powietrze,
// pierścienie, smugi, poblask ze światłem. Dźwięk podmuchu (korzeń) i głuchego uderzenia (pierścienie).
INSTANCE SPELLFX_AARD_WAVE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_DUST";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_WAVE_AIR";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_BLAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2;
};

INSTANCE SPELLFX_AARD_WAVE_AIR (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_WAVE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_WAVE_RING";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 1.5;
};

INSTANCE SPELLFX_AARD_WAVE_RING (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_RING";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_WAVE_STREAKS";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_THUD";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1;
};

INSTANCE SPELLFX_AARD_WAVE_STREAKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_STREAKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_WAVE_GLOW";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.8;
};

INSTANCE SPELLFX_AARD_WAVE_GLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_GLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "JUSTWHITE";
	emFXLifeSpan			= 0.4;
};

INSTANCE SPELLFX_AARD_WAVE_GLOW_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 450;
};

// trafiony: podmuch kurzu i iskry powietrza
INSTANCE SPELLFX_AARD_HIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_HIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_HIT_SPARK";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_HIT";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 1.5;
};

INSTANCE SPELLFX_AARD_HIT_SPARK (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_HIT_SPARK";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 1;
};


// ================================================================ IGNI

INSTANCE SPELLFX_IGNI (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_INIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 0;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	lightPresetName			= "FIRESMALL";
};

INSTANCE SPELLFX_IGNI_KEY_CAST (C_ParticleFXEmitKey)
{
	pfx_ppsIsLoopingChg		= 1;
	emCreateFXID			= "SPELLFX_IGNI_FLASH";
};

// runda 5: korzeń rozbłysku niesie jednorazowy dźwięk rzutu (w kluczu dłoni urywał się ok. 0,8 s po rzucie,
// razem z efektem dłoni) i żyje dłużej niż dźwięk: żarzące się iskierki gasnące przy dłoni.
INSTANCE SPELLFX_IGNI_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_ECHO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_FLASH_BURST";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_IGNI_CAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2.2;
};

// dłoń: wybuch płomienia (dziecko korzenia _FLASH)
INSTANCE SPELLFX_IGNI_FLASH_BURST (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "FIRE";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_IGNI_FLASH_BURST_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 250;
};

// bohater, kierunek patrzenia: strumień płomieni w stożku. Korzeń = dym (najdłuższy), dzieci: kłęby
// ognia, rdzeń, iskry, żar, poblask ze światłem. Huk ognia (korzeń) i świst (iskry).
INSTANCE SPELLFX_IGNI_CONE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_SMOKE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_CONE_FLAMES";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_IGNI_ROAR";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 3.2;
};

INSTANCE SPELLFX_IGNI_CONE_FLAMES (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_CONE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_CONE_CORE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 1.5;
};

INSTANCE SPELLFX_IGNI_CONE_CORE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_CORE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_CONE_SPARKS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 1.2;
};

INSTANCE SPELLFX_IGNI_CONE_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_CONE_EMBERS";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_IGNI_WHOOSH";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1.3;
};

INSTANCE SPELLFX_IGNI_CONE_EMBERS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_EMBERS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_CONE_GLOW";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 2.6;
};

INSTANCE SPELLFX_IGNI_CONE_GLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_GLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "FIRE";
	emFXLifeSpan			= 0.7;
};

INSTANCE SPELLFX_IGNI_CONE_GLOW_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 700;
};

// trafiony: wybuch płomieni (dalsze płonięcie robi silnik - VOB_BURN)
INSTANCE SPELLFX_IGNI_HIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_HIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_HIT_SPARKS";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "FIRE";
	sfxID					= "ZDF_SIGN_IGNI_HIT";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 1.2;
};

INSTANCE SPELLFX_IGNI_HIT_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_HIT_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 1;
};


// ================================================================ AXII

INSTANCE SPELLFX_AXII (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_INIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 0;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	lightPresetName			= "FX_LIGHT1";
};

INSTANCE SPELLFX_AXII_KEY_OPEN (C_ParticleFXEmitKey)
{
	lightRange				= 70;
};

INSTANCE SPELLFX_AXII_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 70;
};

// Runda 4: bez dźwięku w kluczu - efekt dłoni znika ok. 0,8 s po rzucie i silnik ucina wtedy jego dźwięk
// (StopSound przy usuwaniu efektu). Dźwięk rzutu niesie SPELLFX_AXII_FLASH (żyje 2,6 s).
INSTANCE SPELLFX_AXII_KEY_CAST (C_ParticleFXEmitKey)
{
	pfx_ppsIsLoopingChg		= 1;
	emCreateFXID			= "SPELLFX_AXII_FLASH";
	lightRange				= 0.01;
};

// dłoń po rzucie: zielone iskierki gasnące przy dłoni przez 2,6 s i jednorazowy dźwięk rzutu (bez pętli);
// dziecko: szmaragdowy wir ze światłem
INSTANCE SPELLFX_AXII_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_ECHO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_FLASH_BURST";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AXII_CAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2.6;
};

INSTANCE SPELLFX_AXII_FLASH_BURST (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_FLASH_STAR";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "FX_LIGHT1";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_AXII_FLASH_BURST_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 200;
};

INSTANCE SPELLFX_AXII_FLASH_STAR (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_FLASH_STAR";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 0.5;
};

// głowa celu, PĘTLA na czas oszołomienia: spirale, krążące gwiazdki, poświata. Stop(TRUE) zatrzymuje.
// Dźwięk jednorazowy (początek oszołomienia, ok. 2,7 s, bez pętli) - gra raz przy utworzeniu efektu.
INSTANCE SPELLFX_AXII_TARGET (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_SPIRAL";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_TARGET_ORBIT";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "FX_LIGHT1";
	sfxID					= "ZDF_SIGN_AXII_TRANCE";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 8;
};

INSTANCE SPELLFX_AXII_TARGET_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 120;
};

INSTANCE SPELLFX_AXII_TARGET_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 120;
};

INSTANCE SPELLFX_AXII_TARGET_ORBIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_ORBIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_TARGET_HAZE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 8;
};

INSTANCE SPELLFX_AXII_TARGET_HAZE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_HAZE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 8;
};

// cel poza walką: krótki rozbłysk mgły nad głową (niepamięć)
INSTANCE SPELLFX_AXII_FORGET (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_MIST";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_FORGET_STARS";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AXII_FORGET";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 2.2;
};

INSTANCE SPELLFX_AXII_FORGET_STARS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_MIST_STARS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 1.2;
};


// ================================================================ YRDEN

INSTANCE SPELLFX_YRDEN (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_INIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 0;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	lightPresetName			= "POISON";
};

INSTANCE SPELLFX_YRDEN_KEY_CAST (C_ParticleFXEmitKey)
{
	pfx_ppsIsLoopingChg		= 1;
	emCreateFXID			= "SPELLFX_YRDEN_FLASH";
};

// runda 5: korzeń rozbłysku niesie jednorazowy dźwięk rzutu (w kluczu dłoni urywał się ok. 0,8 s po rzucie,
// razem z efektem dłoni) i żyje dłużej niż dźwięk: fioletowe iskierki gasnące przy dłoni.
INSTANCE SPELLFX_YRDEN_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ECHO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_FLASH_BURST";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_YRDEN_CAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2.4;
};

// dłoń: fioletowy znak rozbłyska (dziecko korzenia _FLASH)
INSTANCE SPELLFX_YRDEN_FLASH_BURST (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_FLASH_SPARKS";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "POISON";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_YRDEN_FLASH_BURST_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 250;
};

INSTANCE SPELLFX_YRDEN_FLASH_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_FLASH_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 0.6;
};

// ziemia w miejscu rzucenia, PĘTLA na czas strefy: glif 5 m pulsujący, pierścień cząstek, dwie iskry run
// biegnące po obwodzie, fale pierścieni, światło. Wywołanie: CreateAndPlay(nazwa, zVEC3 punkt na ziemi, ...)
// (cząstki 3-10 cm nad punktem). emTrjMode "NONE" = stoi w miejscu.
// Przeniesienie strefy (ponowny rzut): Stop(TRUE) starej + nowe CreateAndPlay.
INSTANCE SPELLFX_YRDEN_ZONE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_GLYPH";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_ZONE_START";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "POISON";
	sfxID					= "ZDF_SIGN_YRDEN_ZONE";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_YRDEN_ZONE_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 500;
};

INSTANCE SPELLFX_YRDEN_ZONE_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 500;
};

INSTANCE SPELLFX_YRDEN_ZONE_START (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_GLYPH_START";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_ZONE_EDGE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 2;
};

INSTANCE SPELLFX_YRDEN_ZONE_EDGE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_EDGE";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_ZONE_RUNES";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_YRDEN_ZONE_RUNES (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_RUNES";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_ZONE_RUNES2";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_YRDEN_ZONE_RUNES2 (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_RUNES2";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_ZONE_PULSE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_YRDEN_ZONE_PULSE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_PULSE";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 30;
};

// wróg w strefie, PĘTLA (idzie za wrogiem): fioletowe nici wokół nóg. Stop(TRUE) przy wyjściu ze strefy.
INSTANCE SPELLFX_YRDEN_SLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_SLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_SLOW_MOTES";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_YRDEN_SLOW";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_YRDEN_SLOW_MOTES (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_SLOW_MOTES";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 30;
};


// ================================================================ QUEN

INSTANCE SPELLFX_QUEN (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_INIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 0;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	lightPresetName			= "CATACLYSM";
};

INSTANCE SPELLFX_QUEN_KEY_OPEN (C_ParticleFXEmitKey)
{
	lightRange				= 80;
};

INSTANCE SPELLFX_QUEN_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 80;
};

INSTANCE SPELLFX_QUEN_KEY_CAST (C_ParticleFXEmitKey)
{
	pfx_ppsIsLoopingChg		= 1;
	emCreateFXID			= "SPELLFX_QUEN_FLASH";
	lightRange				= 0.01;
};

// runda 5: korzeń rozbłysku niesie jednorazowy dźwięk rzutu (w kluczu dłoni urywał się ok. 0,8 s po rzucie,
// razem z efektem dłoni) i żyje dłużej niż dźwięk: złote iskierki gasnące przy dłoni.
INSTANCE SPELLFX_QUEN_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ECHO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_FLASH_BURST";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_QUEN_CAST";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2.1;
};

// dłoń: złoty błysk (dziecko korzenia _FLASH)
INSTANCE SPELLFX_QUEN_FLASH_BURST (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_FLASH_SPARKS";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "CATACLYSM";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_QUEN_FLASH_BURST_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 220;
};

INSTANCE SPELLFX_QUEN_FLASH_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_FLASH_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 0.6;
};

// bohater, PĘTLA do pęknięcia: bursztynowa bańka, światełka na kuli, dwie orbity złotych iskier, światło.
// Stop(TRUE) przy pęknięciu (potem SPELLFX_QUEN_BREAK) albo po czasie tarczy.
INSTANCE SPELLFX_QUEN_SHIELD (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_BUBBLE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_SHIELD_SHELL";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "CATACLYSM";
	sfxID					= "ZDF_SIGN_QUEN_SHIELD";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_QUEN_SHIELD_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 220;
};

INSTANCE SPELLFX_QUEN_SHIELD_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 220;
};

INSTANCE SPELLFX_QUEN_SHIELD_SHELL (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_SHELL";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_SHIELD_ORBIT";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_QUEN_SHIELD_ORBIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ORBIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_SHIELD_ORBIT2";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_QUEN_SHIELD_ORBIT2 (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ORBIT2";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 60;
};

// bohater: pęknięcie tarczy - złoty wybuch, odłamki, fala po ziemi (2,5 m), pierścień, błysk ze światłem.
// Korzeń = wybuch (najdłuższy). Dźwięk pęknięcia (korzeń) i uderzenia (odłamki).
INSTANCE SPELLFX_QUEN_BREAK (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_BURST";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_BREAK_SHARDS";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_QUEN_BREAK";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1.5;
};

INSTANCE SPELLFX_QUEN_BREAK_SHARDS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_SHARDS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_BREAK_WAVE";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_QUEN_BOOM";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1.4;
};

INSTANCE SPELLFX_QUEN_BREAK_WAVE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_WAVE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_BREAK_RING";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.8;
};

INSTANCE SPELLFX_QUEN_BREAK_RING (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_RING";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_BREAK_FLARE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_QUEN_BREAK_FLARE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_FLARE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "CATACLYSM";
	emFXLifeSpan			= 0.45;
};

INSTANCE SPELLFX_QUEN_BREAK_FLARE_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 450;
};


// ================================================================ poświata run (WEAR_EFFECT), kopia SPELLFX_WEAKGLIMMER

INSTANCE SPELLFX_ZDF_SIGN_RUNE_AARD (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_RUNE_AARD";
	visAlpha				= 1;
	visAlphaBlendFunc_S		= "ADD";
	emTrjMode_S				= "FIXED";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 2000000;
	emFXLifeSpan			= -1;
	emSelfRotVel_S			= "0 0 0";
	secsPerDamage			= -1;
	emAdjustShpToOrigin		= 1;
};

INSTANCE SPELLFX_ZDF_SIGN_RUNE_IGNI (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_RUNE_IGNI";
	visAlpha				= 1;
	visAlphaBlendFunc_S		= "ADD";
	emTrjMode_S				= "FIXED";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 2000000;
	emFXLifeSpan			= -1;
	emSelfRotVel_S			= "0 0 0";
	secsPerDamage			= -1;
	emAdjustShpToOrigin		= 1;
};

INSTANCE SPELLFX_ZDF_SIGN_RUNE_AXII (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_RUNE_AXII";
	visAlpha				= 1;
	visAlphaBlendFunc_S		= "ADD";
	emTrjMode_S				= "FIXED";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 2000000;
	emFXLifeSpan			= -1;
	emSelfRotVel_S			= "0 0 0";
	secsPerDamage			= -1;
	emAdjustShpToOrigin		= 1;
};

INSTANCE SPELLFX_ZDF_SIGN_RUNE_YRDEN (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_RUNE_YRDEN";
	visAlpha				= 1;
	visAlphaBlendFunc_S		= "ADD";
	emTrjMode_S				= "FIXED";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 2000000;
	emFXLifeSpan			= -1;
	emSelfRotVel_S			= "0 0 0";
	secsPerDamage			= -1;
	emAdjustShpToOrigin		= 1;
};

INSTANCE SPELLFX_ZDF_SIGN_RUNE_QUEN (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_RUNE_QUEN";
	visAlpha				= 1;
	visAlphaBlendFunc_S		= "ADD";
	emTrjMode_S				= "FIXED";
	emTrjLoopMode_S			= "NONE";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjDynUpdateDelay		= 2000000;
	emFXLifeSpan			= -1;
	emSelfRotVel_S			= "0 0 0";
	secsPerDamage			= -1;
	emAdjustShpToOrigin		= 1;
};


// ################################################################ RUNDA 3: tryby alternatywne
// Wywołania (plugin):
//   SPELLFX_AARD_SWEEP      CreateAndPlay(nazwa, bohater, NULL)          jednorazowy
//   SPELLFX_IGNI_STREAM     CreateAndPlay(nazwa, bohater, NULL), trzymać  PĘTLA, Stop(TRUE) po puszczeniu, bezpiecznik 20 s
//   SPELLFX_YRDEN_TRAP      CreateAndPlay(nazwa, zVEC3 punkt NA ZIEMI)    PĘTLA, bezpiecznik 60 s (sam glif, bez kul)
//   SPELLFX_YRDEN_TRAP_ORB  CreateAndPlay(nazwa, zVEC3 ŚRODEK KULI) x3    PĘTLA każda kula osobno, bezpiecznik 60 s;
//                           proponowane środki: punkt glifu + (0, 90, 110), (95, 90, -55), (-95, 90, -55) cm.
//                           Zużyty ładunek: Stop(TRUE) tej kuli + SPELLFX_YRDEN_BOLT z jej środka.
//   SPELLFX_YRDEN_BOLT      CreateAndPlay(nazwa, zVEC3 środek kuli/glifu, wróg)   jednorazowy, leci do węzła BIP01 wroga
//   SPELLFX_QUEN_ACTIVE     CreateAndPlay(nazwa, bohater, bohater), trzymać  PĘTLA, Stop(TRUE) po puszczeniu, bezpiecznik 30 s
//   SPELLFX_QUEN_ABSORB     CreateAndPlay(nazwa, bohater, bohater)       jednorazowy (pochłonięcie / leczenie)
//   SPELLFX_AXII_PUPPET     CreateAndPlay(nazwa, marionetka, marionetka), trzymać  PĘTLA, bezpiecznik 30 s
// Pocisk "od-do" bez kolizji: emTrjMode "TARGET" (tor od źródła do węzła celu, emTrjDynUpdateDelay = nadąża za
// celem), emTrjLoopMode "HALT" (na końcu toru staje - silnik nie leci dalej), emCheckCollision 0 i puste emFXColl*,
// więc nic nie trafia i nie zadaje obrażeń. Tak samo jak lot SPELLFX_FIREBOLT po KEY_CAST, ale bez kolizji.


// ================================================================ AARD - fala wokół (360 st.)
// Korzeń = kurz (najdłuższy), dzieci: druga połówka kurzu, powietrze (dwie połówki), płaskie pierścienie do 4 m,
// pierścień-kopuła, poblask ze światłem. Dźwięk fali (korzeń) i głuchego uderzenia (pierścienie).

INSTANCE SPELLFX_AARD_SWEEP (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_DUST_F";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_DUST_B";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_SWEEP";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 2;
};

INSTANCE SPELLFX_AARD_SWEEP_DUST_B (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_DUST_B";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_AIR_F";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 2;
};

INSTANCE SPELLFX_AARD_SWEEP_AIR_F (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_AIR_F";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_AIR_B";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 1.4;
};

INSTANCE SPELLFX_AARD_SWEEP_AIR_B (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_AIR_B";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_RING";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 1.4;
};

INSTANCE SPELLFX_AARD_SWEEP_RING (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_RING";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_DOME";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_AARD_THUD";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1;
};

INSTANCE SPELLFX_AARD_SWEEP_DOME (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_DOME";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AARD_SWEEP_GLOW";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.8;
};

INSTANCE SPELLFX_AARD_SWEEP_GLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AARD_SWEEP_GLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "JUSTWHITE";
	emFXLifeSpan			= 0.4;
};

INSTANCE SPELLFX_AARD_SWEEP_GLOW_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 500;
};


// ================================================================ IGNI - strumień ognia (PĘTLA do Stop)
// Bohater, kierunek patrzenia (FIXED "=": strumień obraca się z bohaterem). Korzeń = kłęby ognia ze światłem
// i pętlą dźwięku; dzieci (też pętle): rdzeń, iskry, żar, dym, poblask, płonąca dłoń; raz: wybuch na starcie.

INSTANCE SPELLFX_IGNI_STREAM (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_FLAMES";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_CORE";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "FIRE";
	sfxID					= "ZDF_SIGN_IGNI_STREAM_LOOP";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 750;
};

INSTANCE SPELLFX_IGNI_STREAM_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 750;
};

INSTANCE SPELLFX_IGNI_STREAM_CORE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_CORE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_SPARKS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_EMBERS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_EMBERS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_EMBERS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_SMOKE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_SMOKE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_SMOKE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_GLOW";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_GLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_GLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_HAND";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_HAND (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_STREAM_HAND";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_IGNI_STREAM_START";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 20;
};

INSTANCE SPELLFX_IGNI_STREAM_START (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_IGNI_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "ZS_RIGHTHAND";
	emTrjDynUpdateDelay		= 0;
	sfxID					= "ZDF_SIGN_IGNI_ROAR";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1;
};


// ================================================================ YRDEN - pułapka, kule-ładunki, pocisk

// glif 3 m na ziemi, PĘTLA (bez kul - kule to osobne efekty SPELLFX_YRDEN_TRAP_ORB, gaszone pojedynczo)
INSTANCE SPELLFX_YRDEN_TRAP (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_TRAP_GLYPH";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_START";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "POISON";
	sfxID					= "ZDF_SIGN_YRDEN_ZONE";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 350;
};

INSTANCE SPELLFX_YRDEN_TRAP_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 350;
};

INSTANCE SPELLFX_YRDEN_TRAP_START (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_TRAP_GLYPH_START";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_EDGE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 2;
};

INSTANCE SPELLFX_YRDEN_TRAP_EDGE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_TRAP_EDGE";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_RUNES";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_RUNES (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_TRAP_RUNES";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 60;
};

// jedna kula-ładunek w punkcie wywołania (środek kuli), PĘTLA. Plugin trzyma uchwyt każdej kuli osobno;
// zużyty ładunek = Stop(TRUE) tej kuli (rdzeń, brzeg, wyładowania, iskry i smuga gasną razem).
INSTANCE SPELLFX_YRDEN_TRAP_ORB (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ORB_CORE";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_ORB_SHELL";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_ORB_SHELL (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ORB_SHELL";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_ORB_ARCS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_ORB_ARCS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ORB_ARCS";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_ORB_MOTES";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_ORB_MOTES (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ORB_MOTES";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_TRAP_ORB_BEAM";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 60;
};

INSTANCE SPELLFX_YRDEN_TRAP_ORB_BEAM (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_ORB_BEAM";
	visAlpha				= 1;
	emTrjMode_S				= "NONE";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 60;
};

// pocisk od kuli/glifu do wroga: głowa i łuki lecą torem TARGET (2600 cm/s, HALT na końcu, nadąża za celem),
// rozbłysk w punkcie startu, trafienie (cząstki opóźnione o czas lotu) ze światłem na celu. Bez kolizji.
INSTANCE SPELLFX_YRDEN_BOLT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_BOLT_HEAD";
	visAlpha				= 1;
	emTrjMode_S				= "TARGET";
	emTrjOriginNode			= "=";
	emTrjTargetNode			= "BIP01";
	emTrjLoopMode_S			= "HALT";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjEaseVel			= 2600;
	emTrjDynUpdateDelay		= 0.05;
	emCheckCollision		= 0;
	sendAssessMagic			= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_BOLT_ARCS";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_YRDEN_BOLT";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 0.45;
};

INSTANCE SPELLFX_YRDEN_BOLT_ARCS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_BOLT_ARCS";
	visAlpha				= 1;
	emTrjMode_S				= "TARGET";
	emTrjOriginNode			= "=";
	emTrjTargetNode			= "BIP01";
	emTrjLoopMode_S			= "HALT";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjEaseVel			= 2600;
	emTrjDynUpdateDelay		= 0.05;
	emCheckCollision		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_BOLT_MUZZLE";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.4;
};

INSTANCE SPELLFX_YRDEN_BOLT_MUZZLE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_BOLT_MUZZLE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_BOLT_HIT";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.5;
};

// na celu od razu (bardzo szybki tor TARGET + HALT); cząstki ruszają po ok. 0,12 s (klucze ppsScaleKeys)
INSTANCE SPELLFX_YRDEN_BOLT_HIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_BOLT_HIT";
	visAlpha				= 1;
	emTrjMode_S				= "TARGET";
	emTrjOriginNode			= "=";
	emTrjTargetNode			= "BIP01";
	emTrjLoopMode_S			= "HALT";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjEaseVel			= 100000;
	emTrjDynUpdateDelay		= 0.05;
	emCheckCollision		= 0;
	emFXCreate_S			= "SPELLFX_YRDEN_BOLT_HIT_FLASH";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_YRDEN_BOLT_HIT";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 0.8;
};

INSTANCE SPELLFX_YRDEN_BOLT_HIT_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_YRDEN_BOLT_HIT_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "TARGET";
	emTrjOriginNode			= "=";
	emTrjTargetNode			= "BIP01";
	emTrjLoopMode_S			= "HALT";
	emTrjEaseFunc_S			= "LINEAR";
	emTrjEaseVel			= 100000;
	emTrjDynUpdateDelay		= 0.05;
	emCheckCollision		= 0;
	lightPresetName			= "POISON";
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_YRDEN_BOLT_HIT_FLASH_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 250;
};


// ================================================================ QUEN - aktywna tarcza (PĘTLA do Stop), pochłonięcie
// Aktywna: korzeń = jasny puls bańki co ok. 0,6 s ze światłem i szumem w pętli; dzieci (pętle): stała bańka,
// światełka na kuli, dwie szybsze orbity, pulsujący pierścień pod stopami; raz: rozbłysk i dźwięk startu.

INSTANCE SPELLFX_QUEN_ACTIVE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_PULSE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_BUBBLE";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "CATACLYSM";
	sfxID					= "ZDF_SIGN_QUEN_ACTIVE_LOOP";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 320;
};

INSTANCE SPELLFX_QUEN_ACTIVE_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 320;
};

INSTANCE SPELLFX_QUEN_ACTIVE_BUBBLE (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_BUBBLE";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_SHELL";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_SHELL (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_SHELL";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_ORBIT";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_ORBIT (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_ORBIT";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_ORBIT2";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_ORBIT2 (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_ORBIT2";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_GROUND";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_GROUND (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ACTIVE_GROUND";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ACTIVE_START";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_QUEN_ACTIVE_START (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ABSORB_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	sfxID					= "ZDF_SIGN_QUEN_SHIELD";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 0.6;
};

// pochłonięcie trafienia i leczenie: rozbłysk bańki, iskry na kuli, złoto-zielone iskierki leczenia, światło.
// Korzeń = iskierki leczenia (najdłuższe).
INSTANCE SPELLFX_QUEN_ABSORB (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ABSORB_HEAL";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ABSORB_FLASH";
	emFXCreatedOwnTrj		= 1;
	sfxID					= "ZDF_SIGN_QUEN_ABSORB";
	sfxIsAmbient			= 1;
	emFXLifeSpan			= 1.4;
};

INSTANCE SPELLFX_QUEN_ABSORB_FLASH (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ABSORB_FLASH";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ABSORB_SPARKS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.6;
};

INSTANCE SPELLFX_QUEN_ABSORB_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ABSORB_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_QUEN_ABSORB_GLOW";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 0.7;
};

INSTANCE SPELLFX_QUEN_ABSORB_GLOW (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_QUEN_ABSORB_GLOW";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "=";
	emTrjDynUpdateDelay		= 0;
	lightPresetName			= "CATACLYSM";
	emFXLifeSpan			= 0.35;
};

INSTANCE SPELLFX_QUEN_ABSORB_GLOW_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 300;
};


// ================================================================ AXII - marionetka (PĘTLA do Stop)
// Korzeń = szmaragdowa aura wokół ciała (BIP01) ze światłem; dzieci: drobiny, nitki lalkarza spływające
// ku głowie, wir i poświata nad głową (BIP01 HEAD).

INSTANCE SPELLFX_AXII_PUPPET (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_PUPPET_AURA";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_PUPPET_SPARKS";
	emFXCreatedOwnTrj		= 1;
	lightPresetName			= "FX_LIGHT1";
	sfxID					= "ZDF_SIGN_AXII_PUPPET";
	sfxIsAmbient			= 0;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_AXII_PUPPET_KEY_INIT (C_ParticleFXEmitKey)
{
	lightRange				= 160;
};

INSTANCE SPELLFX_AXII_PUPPET_KEY_CAST (C_ParticleFXEmitKey)
{
	lightRange				= 160;
};

INSTANCE SPELLFX_AXII_PUPPET_SPARKS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_PUPPET_SPARKS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_PUPPET_STRINGS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_AXII_PUPPET_STRINGS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_PUPPET_STRINGS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_PUPPET_CROSS";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_AXII_PUPPET_CROSS (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_PUPPET_CROSS";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXCreate_S			= "SPELLFX_AXII_PUPPET_HALO";
	emFXCreatedOwnTrj		= 1;
	emFXLifeSpan			= 30;
};

INSTANCE SPELLFX_AXII_PUPPET_HALO (CFx_Base_Proto)
{
	visName_S				= "ZDF_SIGN_AXII_PUPPET_HALO";
	visAlpha				= 1;
	emTrjMode_S				= "FIXED";
	emTrjOriginNode			= "BIP01 HEAD";
	emTrjDynUpdateDelay		= 0;
	emFXLifeSpan			= 30;
};
