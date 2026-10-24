META
{
	Engine = G2A;
	Parser = SFX;
	MergeMode = 1;
};

// Znaki Wiedźmina (zDismember): dźwięki ZDF_SIGN_* dla efektów z zDismember_SignsVfx.d. Kodowanie: UTF-8 bez BOM.
// Tylko pliki .wav istniejące w Sounds.vdf / Sounds_Addon.vdf gry (sprawdza anims\pipeline\check_signs_scripts.py).
// Warianty _A1, _A2 silnik losuje sam (jak Whoosh_A1 w SfxInst.d).

// ---------------------------------------------------------------- AARD
instance ZDF_SIGN_AARD_CAST		(C_SFX_DEF)	{ file = "MFX_Windfist_Cast.wav";			vol = 110; loop = 0; };	// rzut (2,1 s)
instance ZDF_SIGN_AARD_BLAST	(C_SFX_DEF)	{ file = "MFX_Windfist_Investblast.wav";	vol = 127; };	// podmuch
instance ZDF_SIGN_AARD_THUD		(C_SFX_DEF)	{ file = "Gol_StepBoom01.wav";				vol = 80;  pitchOff = -2; };	// głuche uderzenie
instance ZDF_SIGN_AARD_HIT		(C_SFX_DEF)	{ file = "woosh_02.wav";					vol = 90;  pitchVar = 2; };
instance ZDF_SIGN_AARD_HIT_A1	(C_SFX_DEF)	{ file = "woosh_04.wav";					vol = 90;  pitchVar = 2; };
instance ZDF_SIGN_AARD_HIT_A2	(C_SFX_DEF)	{ file = "woosh_06.wav";					vol = 90;  pitchVar = 2; };

// ---------------------------------------------------------------- IGNI
instance ZDF_SIGN_IGNI_CAST		(C_SFX_DEF)	{ file = "Torch_Enlight.wav";				vol = 110; loop = 0; };	// rozpalenie dłoni (2,0 s)
instance ZDF_SIGN_IGNI_ROAR		(C_SFX_DEF)	{ file = "MFX_Firestorm_Cast.wav";			vol = 127; };	// huk ognia
instance ZDF_SIGN_IGNI_WHOOSH	(C_SFX_DEF)	{ file = "Spit_Fire.wav";					vol = 100; };
instance ZDF_SIGN_IGNI_HIT		(C_SFX_DEF)	{ file = "MFX_Fireball_Collide1.wav";		vol = 100; };
instance ZDF_SIGN_IGNI_HIT_A1	(C_SFX_DEF)	{ file = "MFX_Fireball_Collide2.wav";		vol = 100; };

// ---------------------------------------------------------------- AXII
// Runda 4: tylko krótkie (do 3 s), jednorazowe dźwięki, loop = 0. Wcześniejsze MFX_Charm_Cast (4,1 s),
// MFX_Control_Cast (4,6 s) i MFX_Sleep_Cast (7,8 s) nakładały się przy kolejnych rzutach jak pętla.
instance ZDF_SIGN_AXII_CAST		(C_SFX_DEF)	{ file = "MFX_Transform_Cast.wav";			vol = 100; loop = 0; };	// rzut (2,4 s)
instance ZDF_SIGN_AXII_TRANCE	(C_SFX_DEF)	{ file = "CS_Prayer_FokusPrepare.wav";		vol = 90;  loop = 0; };	// początek oszołomienia (2,7 s)
instance ZDF_SIGN_AXII_FORGET	(C_SFX_DEF)	{ file = "MFX_Teleport_Cast.wav";			vol = 80;  loop = 0; };	// niepamięć (1,8 s)

// ---------------------------------------------------------------- YRDEN
instance ZDF_SIGN_YRDEN_CAST	(C_SFX_DEF)	{ file = "MFX_Barriere_Warning.wav";		vol = 100; loop = 0; };	// rzut (2,1 s; runda 5, było 4,9 s)
instance ZDF_SIGN_YRDEN_ZONE	(C_SFX_DEF)	{ file = "MFX_Light_Cast.wav";				vol = 70;  };	// cichy brzęk glifu
instance ZDF_SIGN_YRDEN_SLOW	(C_SFX_DEF)	{ file = "MFX_Thunderbolt_Cast.wav";		vol = 35;  loop = 0; };	// krótki cichy trzask przy wrogu (0,9 s; runda 5, było 7,1 s)

// ---------------------------------------------------------------- QUEN
instance ZDF_SIGN_QUEN_CAST		(C_SFX_DEF)	{ file = "MFX_Teleport_Cast.wav";			vol = 90;  loop = 0; };	// rzut (1,8 s)
instance ZDF_SIGN_QUEN_SHIELD	(C_SFX_DEF)	{ file = "MFX_Heal_Cast.wav";				vol = 110; };	// tarcza się rozpina
instance ZDF_SIGN_QUEN_BREAK	(C_SFX_DEF)	{ file = "MFX_Icecube_Release.wav";			vol = 127; };	// pęknięcie
instance ZDF_SIGN_QUEN_BOOM		(C_SFX_DEF)	{ file = "MFX_DestroyUndead_Collide.wav";	vol = 110; };	// złoty wybuch

// ---------------------------------------------------------------- runda 3: tryby alternatywne
// Pętle (loop = 1) grają, dopóki efekt żyje: silnik zatrzymuje dźwięk przy usuwaniu efektu (po Stop i dogaśnięciu).
instance ZDF_SIGN_AARD_SWEEP		(C_SFX_DEF)	{ file = "MFX_Stormfist_Cast.wav";			vol = 127; };	// fala wokół
instance ZDF_SIGN_IGNI_STREAM_LOOP	(C_SFX_DEF)	{ file = "MFX_Pyrokinesis_Target.wav";		vol = 110; loop = 1; };	// huk strumienia
instance ZDF_SIGN_YRDEN_BOLT		(C_SFX_DEF)	{ file = "MFX_Barriere_Shoot.wav";			vol = 100; pitchVar = 2; };	// wystrzał kuli
instance ZDF_SIGN_YRDEN_BOLT_HIT	(C_SFX_DEF)	{ file = "MFX_Thunderball_Collide1.wav";	vol = 90;  };	// trafienie
instance ZDF_SIGN_QUEN_ACTIVE_LOOP	(C_SFX_DEF)	{ file = "MFX_Telekinesis_Bridge.wav";		vol = 80;  loop = 1; };	// szum aktywnej tarczy
instance ZDF_SIGN_QUEN_ABSORB		(C_SFX_DEF)	{ file = "MFX_DestroyUndead_Cast.wav";		vol = 90;  pitchVar = 2; };	// pochłonięcie
instance ZDF_SIGN_AXII_PUPPET		(C_SFX_DEF)	{ file = "MFX_BeliarWeap_01.wav";			vol = 100; loop = 0; };	// przejęcie marionetki (1,7 s, runda 4)
