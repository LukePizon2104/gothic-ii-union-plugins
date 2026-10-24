META
{
	Engine = G2A;
	Parser = PFX;
	MergeMode = 1;
};

// Znaki Wiedźmina (zDismember): cząsteczki (C_PARTICLEFX) dla efektów SPELLFX_* z zDismember_SignsVfx.d.
// Kodowanie: UTF-8 bez BOM. Tylko tekstury z Textures.vdf / Textures_Addon.vdf gry (sprawdza je
// anims\pipeline\check_signs_scripts.py). Nazwy ZDF_SIGN_<ZNAK>_<CZĘŚĆ>.
//
// Jednostki silnika (sprawdzone w Gothic2.exe i GD3D11):
// - prędkość velAvg w cm/ms (1.0 = 10 m/s), grawitacja flyGravity_S w cm/ms^2, czasy lspPart* w ms,
//   visSizeStart_S = połowa boku kwadratu w cm (GD3D11 rysuje -size..+size);
// - kierunek DIR: dirAngleHead 0 / dirAngleElev 0 = oś +Z obiektu (przód), Elev 90 = w górę,
//   *Var = rozrzut +-; dirFOR "OBJECT" obraca kierunek razem z vobem efektu;
// - kształt CIRCLE leży w płaszczyźnie XZ (poziomo), shpDim_S = promień; WALK obiega obwód,
//   shpDistribWalkSpeed = obroty na ms (0.001 = 1 obrót/s);
// - visOrientation_S "VOB" = kwadrat w płaszczyźnie XZ voba (płasko na ziemi, gdy vob stoi prosto),
//   "VELO" = wzdłuż prędkości, "NONE" = do kamery;
// - wybuch jednorazowy: ppsIsLooping 0, ppsScaleKeys_S "1", ppsFPS = 1/czas wybuchu (s).
// Efekty przy bohaterze mają w VFX emTrjOriginNode "=" (brak węzła => trafo voba bohatera: środek modelu,
// ok. 95 cm nad ziemią wg logu zDismember "vob 95 nad ziemia", oś +Z = kierunek patrzenia), dlatego
// przesunięcia "0 -80 0".."0 -90 0" to tuż nad ziemią.
// Wyjątek: strefa Yrden (ZDF_SIGN_YRDEN_GLYPH ... _PULSE) - plugin tworzy ją w punkcie NA ZIEMI
// (CreateAndPlay z zVEC3 => pomocniczy vob w tym punkcie), więc tam przesunięcia są małe, dodatnie (+3..+10 cm).


// ================================================================ AARD - bladobłękitna fala powietrza

// dłoń, pętla do rzutu: wirujące powietrze
INSTANCE ZDF_SIGN_AARD_INIT (C_PARTICLEFX)
{
	ppsValue			= 40;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.015;
	velVar				= 0.005;
	lspPartAvg			= 450;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 220 255";
	visTexColorEnd_S	= "120 170 255";
	visSizeStart_S		= "3 3";
	visSizeEndScale		= 3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 180;
	visAlphaEnd			= 0;
};

// dłoń, rzut: podmuch
INSTANCE ZDF_SIGN_AARD_FLASH (C_PARTICLEFX)
{
	ppsValue			= 600;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "4";
	dirMode_S			= "RAND";
	velAvg				= 0.12;
	velVar				= 0.05;
	lspPartAvg			= 300;
	lspPartVar			= 80;
	flyGravity_S		= "0 0 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 230 255";
	visTexColorEnd_S	= "140 180 255";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 220;
	visAlphaEnd			= 0;
};

// dłoń, rzut: błysk
INSTANCE ZDF_SIGN_AARD_FLASH_STAR (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 220;
	flyGravity_S		= "0 0 0";
	visName_S			= "BLUESPARKLE0000.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "230 240 255";
	visTexColorEnd_S	= "120 170 255";
	visSizeStart_S		= "18 18";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater: stożek podmuchu (+-45 st., ok. 6 m), 0,15 s
INSTANCE ZDF_SIGN_AARD_WAVE (C_PARTICLEFX)
{
	ppsValue			= 700;
	ppsScaleKeys_S		= "1 0.8 0.4";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 25 45";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 45;
	dirAngleElev		= 2;
	dirAngleElevVar		= 10;
	velAvg				= 1.1;
	velVar				= 0.3;
	lspPartAvg			= 520;
	lspPartVar			= 120;
	flyGravity_S		= "0 0 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 225 255";
	visTexColorEnd_S	= "140 185 255";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 9;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 120;
	visAlphaEnd			= 0;
};

// bohater: przezroczyste pierścienie lecące do przodu i rosnące (z kamery za plecami wyglądają jak stożek fali)
INSTANCE ZDF_SIGN_AARD_RING (C_PARTICLEFX)
{
	ppsValue			= 50;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 8;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 40";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 0;
	dirAngleElev		= 0;
	dirAngleElevVar		= 0;
	velAvg				= 1.3;
	velVar				= 0.15;
	lspPartAvg			= 430;
	lspPartVar			= 40;
	flyGravity_S		= "0 0 0";
	visName_S			= "WASSERRINGE_A0.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 0;
	visTexColorStart_S	= "200 230 255";
	visTexColorEnd_S	= "120 170 255";
	visSizeStart_S		= "30 30";
	visSizeEndScale		= 13;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// bohater: kurz z ziemi wzdłuż stożka
INSTANCE ZDF_SIGN_AARD_DUST (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 15;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 -80 70";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "30";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 48;
	dirAngleElev		= 6;
	dirAngleElevVar		= 6;
	velAvg				= 0.75;
	velVar				= 0.25;
	lspPartAvg			= 900;
	lspPartVar			= 250;
	flyGravity_S		= "0 0.00012 0";
	visName_S			= "STOMPERDUST.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "175 165 150";
	visTexColorEnd_S	= "130 125 115";
	visSizeStart_S		= "16 16";
	visSizeEndScale		= 7;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// bohater: smugi wiatru
INSTANCE ZDF_SIGN_AARD_STREAKS (C_PARTICLEFX)
{
	ppsValue			= 350;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 10;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 30";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "30";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 40;
	dirAngleElev		= 0;
	dirAngleElevVar		= 12;
	velAvg				= 1.9;
	velVar				= 0.4;
	lspPartAvg			= 280;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZSPARK1.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "220 240 255";
	visTexColorEnd_S	= "150 190 255";
	visSizeStart_S		= "3 26";
	visSizeEndScale		= 1.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// bohater: miękki niebieski poblask w chwili fali (nosi światło)
INSTANCE ZDF_SIGN_AARD_GLOW (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 60";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 300;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "170 210 255";
	visTexColorEnd_S	= "90 140 255";
	visSizeStart_S		= "60 60";
	visSizeEndScale		= 2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 140;
	visAlphaEnd			= 0;
};

// trafiony: podmuch kurzu
INSTANCE ZDF_SIGN_AARD_HIT (C_PARTICLEFX)
{
	ppsValue			= 800;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 12;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "20";
	dirMode_S			= "RAND";
	velAvg				= 0.3;
	velVar				= 0.15;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.0001 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "210 225 245";
	visTexColorEnd_S	= "150 160 180";
	visSizeStart_S		= "10 10";
	visSizeEndScale		= 5;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 140;
	visAlphaEnd			= 0;
};

// trafiony: iskry powietrza
INSTANCE ZDF_SIGN_AARD_HIT_SPARK (C_PARTICLEFX)
{
	ppsValue			= 300;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 15;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "RAND";
	velAvg				= 0.25;
	velVar				= 0.1;
	lspPartAvg			= 350;
	lspPartVar			= 80;
	flyGravity_S		= "0 0 0";
	visName_S			= "BLUESPARKLE0000.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "220 235 255";
	visTexColorEnd_S	= "100 150 255";
	visSizeStart_S		= "8 8";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};


// ================================================================ IGNI - pomarańczowo-czerwony strumień ognia

// dłoń, pętla: rozpalona dłoń
INSTANCE ZDF_SIGN_IGNI_INIT (C_PARTICLEFX)
{
	ppsValue			= 35;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "4";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 30;
	dirAngleElev		= 90;
	dirAngleElevVar		= 20;
	velAvg				= 0.04;
	velVar				= 0.02;
	lspPartAvg			= 400;
	lspPartVar			= 100;
	flyGravity_S		= "0 0.0002 0";
	visName_S			= "FIREFLARE.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 220 160";
	visTexColorEnd_S	= "255 80 10";
	visSizeStart_S		= "4 4";
	visSizeEndScale		= 2.2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// dłoń, rzut: wybuch płomienia
INSTANCE ZDF_SIGN_IGNI_FLASH (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "5";
	dirMode_S			= "RAND";
	velAvg				= 0.15;
	velVar				= 0.06;
	lspPartAvg			= 320;
	lspPartVar			= 80;
	flyGravity_S		= "0 0.0003 0";
	visName_S			= "MFX_FIREBALL.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 240 200";
	visTexColorEnd_S	= "255 60 0";
	visSizeStart_S		= "8 8";
	visSizeEndScale		= 3.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater: kłęby płomieni w stożku (+-28 st., ok. 4,5 m), 0,42 s
INSTANCE ZDF_SIGN_IGNI_CONE (C_PARTICLEFX)
{
	ppsValue			= 300;
	ppsScaleKeys_S		= "1 1 1 0.7 0.4";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 12;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 55";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "12";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 28;
	dirAngleElev		= -2;
	dirAngleElevVar		= 8;
	velAvg				= 1.1;
	velVar				= 0.28;
	lspPartAvg			= 540;
	lspPartVar			= 120;
	flyGravity_S		= "0 0.0003 0";
	visName_S			= "MFX_FIREBALL.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 190";
	visTexColorEnd_S	= "255 60 10";
	visSizeStart_S		= "10 10";
	visSizeEndScale		= 8;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 190;
	visAlphaEnd			= 0;
};

// bohater: jasny rdzeń strumienia
INSTANCE ZDF_SIGN_IGNI_CORE (C_PARTICLEFX)
{
	ppsValue			= 260;
	ppsScaleKeys_S		= "1 1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 10;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 50";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 12;
	dirAngleElev		= -2;
	dirAngleElevVar		= 5;
	velAvg				= 1.45;
	velVar				= 0.2;
	lspPartAvg			= 330;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "FIRE_COMPLETE_A0.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 18;
	visTexAniIsLooping	= 1;
	visTexColorStart_S	= "255 255 220";
	visTexColorEnd_S	= "255 150 40";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater: iskry (spadają)
INSTANCE ZDF_SIGN_IGNI_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 240;
	ppsScaleKeys_S		= "1 1 0.6";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 8;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 55";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "15";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 35;
	dirAngleElev		= 6;
	dirAngleElevVar		= 14;
	velAvg				= 1.8;
	velVar				= 0.55;
	lspPartAvg			= 520;
	lspPartVar			= 200;
	flyGravity_S		= "0 -0.0012 0";
	visName_S			= "ZSPARK1.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 140";
	visTexColorEnd_S	= "255 70 0";
	visSizeStart_S		= "2 11";
	visSizeEndScale		= 0.6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater: żar unoszący się po strumieniu
INSTANCE ZDF_SIGN_IGNI_EMBERS (C_PARTICLEFX)
{
	ppsValue			= 140;
	ppsScaleKeys_S		= "1 1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 8;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 20 170";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "30";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 35;
	dirAngleElev		= 10;
	dirAngleElevVar		= 20;
	velAvg				= 0.45;
	velVar				= 0.15;
	lspPartAvg			= 1500;
	lspPartVar			= 400;
	flyGravity_S		= "0 0.00025 0";
	visName_S			= "SPARK.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 160 60";
	visTexColorEnd_S	= "200 30 0";
	visSizeStart_S		= "2 2";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater: dym po ogniu (najdłuższa część - korzeń SPELLFX_IGNI_CONE)
INSTANCE ZDF_SIGN_IGNI_SMOKE (C_PARTICLEFX)
{
	ppsValue			= 90;
	ppsScaleKeys_S		= "0 1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 6;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 40 210";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "40";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 30;
	dirAngleElev		= 25;
	dirAngleElevVar		= 15;
	velAvg				= 0.18;
	velVar				= 0.06;
	lspPartAvg			= 1800;
	lspPartVar			= 400;
	flyGravity_S		= "0 0.0003 0";
	visName_S			= "FIRESMOKE.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "90 80 70";
	visTexColorEnd_S	= "45 45 45";
	visSizeStart_S		= "25 25";
	visSizeEndScale		= 5;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 110;
	visAlphaEnd			= 0;
};

// bohater: pomarańczowy poblask (nosi światło)
INSTANCE ZDF_SIGN_IGNI_GLOW (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 30 125";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 550;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 170 70";
	visTexColorEnd_S	= "255 60 0";
	visSizeStart_S		= "110 110";
	visSizeEndScale		= 1.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// trafiony: wybuch płomieni
INSTANCE ZDF_SIGN_IGNI_HIT (C_PARTICLEFX)
{
	ppsValue			= 900;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 14;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "18";
	dirMode_S			= "RAND";
	velAvg				= 0.22;
	velVar				= 0.1;
	lspPartAvg			= 480;
	lspPartVar			= 120;
	flyGravity_S		= "0 0.0004 0";
	visName_S			= "MFX_FIREBALL.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 240 190";
	visTexColorEnd_S	= "255 50 0";
	visSizeStart_S		= "10 10";
	visSizeEndScale		= 4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// trafiony: iskry
INSTANCE ZDF_SIGN_IGNI_HIT_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 450;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 15;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "15";
	dirMode_S			= "RAND";
	velAvg				= 0.6;
	velVar				= 0.25;
	lspPartAvg			= 450;
	lspPartVar			= 150;
	flyGravity_S		= "0 -0.001 0";
	visName_S			= "ZSPARK1.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 140";
	visTexColorEnd_S	= "255 70 0";
	visSizeStart_S		= "2 10";
	visSizeEndScale		= 0.6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};


// ================================================================ AXII - szmaragdowo-zielona spirala, hipnoza
// (runda 4: cały Axii zielony, bez fioletu - fiolet ma tylko Yrden. DEMONSIGN jest purpurowy i po
// zabarwieniu na zielono wychodził fioletowo-czarny; wiry robi SHOCKLIGHT, który barwi się czysto.)

// dłoń, pętla: małe wiry
INSTANCE ZDF_SIGN_AXII_INIT (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "3";
	dirMode_S			= "RAND";
	velAvg				= 0.004;
	velVar				= 0.002;
	lspPartAvg			= 650;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "SHOCKLIGHT.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "150 255 190";
	visTexColorEnd_S	= "30 210 100";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 1.6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 220;
	visAlphaEnd			= 0;
};

// dłoń, rzut: wir
INSTANCE ZDF_SIGN_AXII_FLASH (C_PARTICLEFX)
{
	ppsValue			= 120;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 350;
	lspPartVar			= 50;
	flyGravity_S		= "0 0 0";
	visName_S			= "SHOCKLIGHT.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "170 255 200";
	visTexColorEnd_S	= "30 220 110";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 2.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 240;
	visAlphaEnd			= 0;
};

// dłoń, rzut: zielona gwiazda
INSTANCE ZDF_SIGN_AXII_FLASH_STAR (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 250;
	flyGravity_S		= "0 0 0";
	visName_S			= "FLAREGREEN0000.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 255 220";
	visTexColorEnd_S	= "40 220 110";
	visSizeStart_S		= "16 16";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// dłoń, po rzucie (pętla do emFXLifeSpan efektu): zielone iskierki gasnące przy dłoni - niosą dźwięk rzutu,
// żeby nie urwał się razem z efektem dłoni
INSTANCE ZDF_SIGN_AXII_ECHO (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	velVar				= 0.01;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.00005 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 255 210";
	visTexColorEnd_S	= "30 200 100";
	visSizeStart_S		= "2.5 2.5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// głowa celu, pętla: hipnotyczne spirale (nad głową, losowo obrócone przez VELO)
INSTANCE ZDF_SIGN_AXII_SPIRAL (C_PARTICLEFX)
{
	ppsValue			= 5;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 28 0";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	velVar				= 0.001;
	lspPartAvg			= 1400;
	lspPartVar			= 200;
	flyGravity_S		= "0 0 0";
	visName_S			= "SHOCKLIGHT.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "130 255 180";
	visTexColorEnd_S	= "20 190 90";
	visSizeStart_S		= "20 20";
	visSizeEndScale		= 1.7;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 170;
	visAlphaEnd			= 0;
};

// głowa celu, pętla: gwiazdki krążące wokół głowy
INSTANCE ZDF_SIGN_AXII_ORBIT (C_PARTICLEFX)
{
	ppsValue			= 70;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 18 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0011;
	shpIsVolume			= 0;
	shpDim_S			= "26";
	dirMode_S			= "RAND";
	velAvg				= 0.002;
	lspPartAvg			= 520;
	lspPartVar			= 80;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 255 210";
	visTexColorEnd_S	= "30 220 110";
	visSizeStart_S		= "4 4";
	visSizeEndScale		= 0.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// głowa celu, pętla: zielona poświata
INSTANCE ZDF_SIGN_AXII_HAZE (C_PARTICLEFX)
{
	ppsValue			= 3;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 1200;
	flyGravity_S		= "0 0 0";
	visName_S			= "SIMPLEGLOW.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "60 230 130";
	visTexColorEnd_S	= "20 150 70";
	visSizeStart_S		= "30 30";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 80;
	visAlphaEnd			= 0;
};

// cel poza walką: rozbłysk mgły nad głową (niepamięć)
INSTANCE ZDF_SIGN_AXII_MIST (C_PARTICLEFX)
{
	ppsValue			= 90;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 3;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 22 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "12";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 45;
	dirAngleElev		= 90;
	dirAngleElevVar		= 35;
	velAvg				= 0.035;
	velVar				= 0.015;
	lspPartAvg			= 1200;
	lspPartVar			= 250;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_MAGICCLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "140 255 180";
	visTexColorEnd_S	= "40 190 110";
	visSizeStart_S		= "8 8";
	visSizeEndScale		= 4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// cel poza walką: gwiazdki
INSTANCE ZDF_SIGN_AXII_MIST_STARS (C_PARTICLEFX)
{
	ppsValue			= 400;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 16;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 20 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "15";
	dirMode_S			= "RAND";
	velAvg				= 0.06;
	velVar				= 0.03;
	lspPartAvg			= 700;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.0001 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 255 220";
	visTexColorEnd_S	= "40 220 120";
	visSizeStart_S		= "4 4";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};


// ================================================================ YRDEN - fioletowy krąg-glif 5 m

// dłoń, pętla: mały fioletowy znak
INSTANCE ZDF_SIGN_YRDEN_INIT (C_PARTICLEFX)
{
	ppsValue			= 6;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.002;
	lspPartAvg			= 700;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 110 255";
	visTexColorEnd_S	= "110 40 255";
	visSizeStart_S		= "7 7";
	visSizeEndScale		= 1.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// dłoń, rzut: znak rozbłyska
INSTANCE ZDF_SIGN_YRDEN_FLASH (C_PARTICLEFX)
{
	ppsValue			= 90;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 380;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "220 160 255";
	visTexColorEnd_S	= "120 40 255";
	visSizeStart_S		= "10 10";
	visSizeEndScale		= 3.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// dłoń, rzut: iskry
INSTANCE ZDF_SIGN_YRDEN_FLASH_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "5";
	dirMode_S			= "RAND";
	velAvg				= 0.15;
	velVar				= 0.05;
	lspPartAvg			= 380;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "230 190 255";
	visTexColorEnd_S	= "130 40 255";
	visSizeStart_S		= "4 4";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// strefa, pętla: wielki glif płasko na ziemi (r = 5 m), kolejne cząstki nakładają się => pulsuje
INSTANCE ZDF_SIGN_YRDEN_GLYPH (C_PARTICLEFX)
{
	ppsValue			= 1.4;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 3 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 1600;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "170 80 255";
	visTexColorEnd_S	= "120 50 255";
	visSizeStart_S		= "480 480";
	visSizeEndScale		= 1.04;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 110;
	visAlphaEnd			= 0;
};

// strefa, raz na początku: glif rozbłyska od razu (pętla wyżej startuje z opóźnieniem 1/pps)
INSTANCE ZDF_SIGN_YRDEN_GLYPH_START (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 60;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 4 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 1500;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "220 160 255";
	visTexColorEnd_S	= "130 50 255";
	visSizeStart_S		= "380 380";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// strefa, pętla: pierścień cząstek na obwodzie
INSTANCE ZDF_SIGN_YRDEN_EDGE (C_PARTICLEFX)
{
	ppsValue			= 110;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 6 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "500";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 0;
	dirAngleElev		= 90;
	dirAngleElevVar		= 8;
	velAvg				= 0.045;
	velVar				= 0.02;
	lspPartAvg			= 1100;
	lspPartVar			= 250;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 120 255";
	visTexColorEnd_S	= "100 30 220";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// strefa, pętla: iskry run biegnące po obwodzie (ok. 4,5 s na okrążenie)
INSTANCE ZDF_SIGN_YRDEN_RUNES (C_PARTICLEFX)
{
	ppsValue			= 45;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.00022;
	shpIsVolume			= 0;
	shpDim_S			= "500";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	dirAngleElevVar		= 10;
	velAvg				= 0.09;
	velVar				= 0.03;
	lspPartAvg			= 800;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "CFLARESTAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "235 200 255";
	visTexColorEnd_S	= "140 60 255";
	visSizeStart_S		= "9 9";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// strefa, pętla: druga iskra, szybsza (nie biegnie razem z pierwszą)
INSTANCE ZDF_SIGN_YRDEN_RUNES2 (C_PARTICLEFX)
{
	ppsValue			= 35;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.00031;
	shpIsVolume			= 0;
	shpDim_S			= "490";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	dirAngleElevVar		= 10;
	velAvg				= 0.07;
	velVar				= 0.02;
	lspPartAvg			= 700;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 220 255";
	visTexColorEnd_S	= "150 60 255";
	visSizeStart_S		= "7 7";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// strefa, pętla: fale pierścieni płasko od środka do krawędzi (pulsowanie)
INSTANCE ZDF_SIGN_YRDEN_PULSE (C_PARTICLEFX)
{
	ppsValue			= 0.8;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 5 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 1700;
	flyGravity_S		= "0 0 0";
	visName_S			= "WATERRING.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 100 255";
	visTexColorEnd_S	= "120 40 255";
	visSizeStart_S		= "40 40";
	visSizeEndScale		= 12.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 170;
	visAlphaEnd			= 0;
};

// wróg w strefie, pętla: fioletowe nici wokół nóg
INSTANCE ZDF_SIGN_YRDEN_SLOW (C_PARTICLEFX)
{
	ppsValue			= 34;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -45 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0009;
	shpIsVolume			= 0;
	shpDim_S			= "32";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	dirAngleElevVar		= 25;
	velAvg				= 0.035;
	velVar				= 0.01;
	lspPartAvg			= 900;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "ROOTNOOSE.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 90 255";
	visTexColorEnd_S	= "110 30 230";
	visSizeStart_S		= "9 20";
	visSizeEndScale		= 1.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// wróg w strefie, pętla: drobne fioletowe drobinki
INSTANCE ZDF_SIGN_YRDEN_SLOW_MOTES (C_PARTICLEFX)
{
	ppsValue			= 16;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -20 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "40";
	dirMode_S			= "RAND";
	velAvg				= 0.01;
	lspPartAvg			= 900;
	lspPartVar			= 200;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "210 140 255";
	visTexColorEnd_S	= "110 30 220";
	visSizeStart_S		= "4 4";
	visSizeEndScale		= 0.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 220;
	visAlphaEnd			= 0;
};


// ================================================================ QUEN - bursztynowo-złota bańka

// dłoń, pętla: złote rozbłyski
INSTANCE ZDF_SIGN_QUEN_INIT (C_PARTICLEFX)
{
	ppsValue			= 16;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "4";
	dirMode_S			= "RAND";
	velAvg				= 0.01;
	lspPartAvg			= 450;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 220 140";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "5 5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// dłoń, rzut: złoty błysk
INSTANCE ZDF_SIGN_QUEN_FLASH (C_PARTICLEFX)
{
	ppsValue			= 120;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 60;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 300;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE6.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 225 150";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 2.2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// dłoń, rzut: złote iskry
INSTANCE ZDF_SIGN_QUEN_FLASH_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 480;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "4";
	dirMode_S			= "RAND";
	velAvg				= 0.18;
	velVar				= 0.06;
	lspPartAvg			= 350;
	lspPartVar			= 80;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE3.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 160";
	visTexColorEnd_S	= "255 130 10";
	visSizeStart_S		= "5 5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// bohater, pętla: przezroczysta bańka z jasnym brzegiem (cząstki idą z bohaterem: useEmittersFOR)
INSTANCE ZDF_SIGN_QUEN_BUBBLE (C_PARTICLEFX)
{
	ppsValue			= 4;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 1000;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 205 110";
	visTexColorEnd_S	= "255 160 40";
	visSizeStart_S		= "112 112";
	visSizeEndScale		= 1.06;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 120;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// bohater, pętla: miękkie światełka na powierzchni kuli
INSTANCE ZDF_SIGN_QUEN_SHELL (C_PARTICLEFX)
{
	ppsValue			= 80;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "100";
	dirMode_S			= "RAND";
	velAvg				= 0.002;
	lspPartAvg			= 650;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "SIMPLEGLOW.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 200 100";
	visTexColorEnd_S	= "255 140 30";
	visSizeStart_S		= "12 12";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 75;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// bohater, pętla: złote iskry krążące na wysokości piersi
INSTANCE ZDF_SIGN_QUEN_ORBIT (C_PARTICLEFX)
{
	ppsValue			= 100;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 30 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0009;
	shpIsVolume			= 0;
	shpDim_S			= "95";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 450;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE3.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 150";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// bohater, pętla: druga orbita niżej, szybsza
INSTANCE ZDF_SIGN_QUEN_ORBIT2 (C_PARTICLEFX)
{
	ppsValue			= 80;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -30 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0013;
	shpIsVolume			= 0;
	shpDim_S			= "88";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 400;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 225 140";
	visTexColorEnd_S	= "255 120 10";
	visSizeStart_S		= "5 5";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pęknięcie: złoty wybuch (najdłuższa część - korzeń SPELLFX_QUEN_BREAK)
INSTANCE ZDF_SIGN_QUEN_BURST (C_PARTICLEFX)
{
	ppsValue			= 1600;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 25;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "40";
	dirMode_S			= "RAND";
	velAvg				= 0.5;
	velVar				= 0.2;
	lspPartAvg			= 700;
	lspPartVar			= 150;
	flyGravity_S		= "0 -0.0004 0";
	visName_S			= "PFXFLARE1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 240 170";
	visTexColorEnd_S	= "255 120 0";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pęknięcie: odłamki bańki (złote smugi spadające)
INSTANCE ZDF_SIGN_QUEN_SHARDS (C_PARTICLEFX)
{
	ppsValue			= 1200;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 25;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "70";
	dirMode_S			= "RAND";
	velAvg				= 0.65;
	velVar				= 0.25;
	lspPartAvg			= 650;
	lspPartVar			= 200;
	flyGravity_S		= "0 -0.0018 0";
	visName_S			= "ZSPARK1.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 150";
	visTexColorEnd_S	= "255 110 0";
	visSizeStart_S		= "2.5 13";
	visSizeEndScale		= 0.7;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pęknięcie: wielki bursztynowy błysk (nosi światło)
INSTANCE ZDF_SIGN_QUEN_FLARE (C_PARTICLEFX)
{
	ppsValue			= 120;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 60;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 380;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 200 90";
	visTexColorEnd_S	= "255 120 20";
	visSizeStart_S		= "120 120";
	visSizeEndScale		= 1.8;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// pęknięcie: fala płasko po ziemi do 2,5 m (QuenRadius)
INSTANCE ZDF_SIGN_QUEN_WAVE (C_PARTICLEFX)
{
	ppsValue			= 90;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 25;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -90 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 450;
	flyGravity_S		= "0 0 0";
	visName_S			= "WATERRING.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 215 120";
	visTexColorEnd_S	= "255 130 20";
	visSizeStart_S		= "30 30";
	visSizeEndScale		= 9;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// pęknięcie: pierścień fali uderzeniowej na wysokości piersi (do kamery)
INSTANCE ZDF_SIGN_QUEN_RING (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 15 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 350;
	flyGravity_S		= "0 0 0";
	visName_S			= "WASSERRINGE_A0.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 0;
	visTexColorStart_S	= "255 225 140";
	visTexColorEnd_S	= "255 130 20";
	visSizeStart_S		= "40 40";
	visSizeEndScale		= 7;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 220;
	visAlphaEnd			= 0;
};


// ================================================================ poświata run (WEAR_EFFECT), jak WEAK_GLIMMER gry

PROTOTYPE ZDF_SIGN_RUNE_PROTO (C_PARTICLEFX)
{
	ppsValue			= 100;
	ppsScaleKeys_S		= "1.0";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "MESH";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RANDOM";
	shpDim_S			= "7";
	shpScaleKeys_S		= "1";
	shpScaleIsLooping	= 1;
	shpScaleIsSmooth	= 1;
	shpScaleFPS			= 2;
	dirMode_S			= "NONE";
	velAvg				= 0.00000000001;
	lspPartAvg			= 300;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_LIGHT_SINGLERAY.TGA";
	visOrientation_S	= "VELO3D";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 15;
	visTexAniIsLooping	= 1;
	visTexColorStart_S	= "0 0 0";
	visSizeStart_S		= "5 5";
	visSizeEndScale		= 3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	useEmittersFOR		= 0;
};

INSTANCE ZDF_SIGN_RUNE_AARD (ZDF_SIGN_RUNE_PROTO)
{
	visTexColorEnd_S	= "140 190 255";
};

INSTANCE ZDF_SIGN_RUNE_IGNI (ZDF_SIGN_RUNE_PROTO)
{
	visTexColorEnd_S	= "255 110 20";
};

INSTANCE ZDF_SIGN_RUNE_AXII (ZDF_SIGN_RUNE_PROTO)
{
	visTexColorStart_S	= "0 0 0";
	visTexColorEnd_S	= "40 230 110";
};

INSTANCE ZDF_SIGN_RUNE_YRDEN (ZDF_SIGN_RUNE_PROTO)
{
	visTexColorEnd_S	= "150 60 255";
};

INSTANCE ZDF_SIGN_RUNE_QUEN (ZDF_SIGN_RUNE_PROTO)
{
	visTexColorEnd_S	= "255 170 30";
};


// ################################################################ RUNDA 3: tryby alternatywne
// Uwaga (Gothic2.exe): kierunek DIR to pudełko z narożników head+-var / elev+-var i kierunku środkowego, potem
// normalizacja. Rozrzut +-180 st. zapada się do osi przód-tył, więc pełne 360 st. = dwie połówki
// (head 0 +-90 i head 180 +-90).

// ================================================================ AARD - fala wokół (360 st., 4 m)

// kurz po ziemi: przednia połówka
INSTANCE ZDF_SIGN_AARD_SWEEP_DUST_F (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 15;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 -80 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 90;
	dirAngleElev		= 4;
	dirAngleElevVar		= 4;
	velAvg				= 0.6;
	velVar				= 0.15;
	lspPartAvg			= 800;
	lspPartVar			= 200;
	flyGravity_S		= "0 0.00012 0";
	visName_S			= "STOMPERDUST.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "175 165 150";
	visTexColorEnd_S	= "130 125 115";
	visSizeStart_S		= "16 16";
	visSizeEndScale		= 7;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// kurz po ziemi: tylna połówka
INSTANCE ZDF_SIGN_AARD_SWEEP_DUST_B (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "1 1 0.5";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 15;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 -80 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 180;
	dirAngleHeadVar		= 90;
	dirAngleElev		= 4;
	dirAngleElevVar		= 4;
	velAvg				= 0.6;
	velVar				= 0.15;
	lspPartAvg			= 800;
	lspPartVar			= 200;
	flyGravity_S		= "0 0.00012 0";
	visName_S			= "STOMPERDUST.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "175 165 150";
	visTexColorEnd_S	= "130 125 115";
	visSizeStart_S		= "16 16";
	visSizeEndScale		= 7;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// powietrze na wysokości pasa: przednia połówka
INSTANCE ZDF_SIGN_AARD_SWEEP_AIR_F (C_PARTICLEFX)
{
	ppsValue			= 600;
	ppsScaleKeys_S		= "1 0.8 0.4";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 20 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 90;
	dirAngleElev		= 3;
	dirAngleElevVar		= 10;
	velAvg				= 0.85;
	velVar				= 0.2;
	lspPartAvg			= 480;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 225 255";
	visTexColorEnd_S	= "140 185 255";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 9;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 120;
	visAlphaEnd			= 0;
};

// powietrze na wysokości pasa: tylna połówka
INSTANCE ZDF_SIGN_AARD_SWEEP_AIR_B (C_PARTICLEFX)
{
	ppsValue			= 600;
	ppsScaleKeys_S		= "1 0.8 0.4";
	ppsIsLooping		= 0;
	ppsIsSmooth			= 1;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 20 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "25";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 180;
	dirAngleHeadVar		= 90;
	dirAngleElev		= 3;
	dirAngleElevVar		= 10;
	velAvg				= 0.85;
	velVar				= 0.2;
	lspPartAvg			= 480;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "WHITECLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 225 255";
	visTexColorEnd_S	= "140 185 255";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 9;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 120;
	visAlphaEnd			= 0;
};

// płaski pierścień fali po ziemi do 4 m (dwa pierścienie co 0,15 s)
INSTANCE ZDF_SIGN_AARD_SWEEP_RING (C_PARTICLEFX)
{
	ppsValue			= 40;
	ppsScaleKeys_S		= "1 0 0 1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -88 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 500;
	flyGravity_S		= "0 0 0";
	visName_S			= "WASSERRINGE_A0.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 0;
	visTexColorStart_S	= "200 230 255";
	visTexColorEnd_S	= "120 170 255";
	visSizeStart_S		= "40 40";
	visSizeEndScale		= 11;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// pierścień do kamery na wysokości pasa (kopuła fali)
INSTANCE ZDF_SIGN_AARD_SWEEP_DOME (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 20 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 400;
	flyGravity_S		= "0 0 0";
	visName_S			= "WASSERRINGE_A0.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 0;
	visTexColorStart_S	= "210 235 255";
	visTexColorEnd_S	= "120 170 255";
	visSizeStart_S		= "30 30";
	visSizeEndScale		= 10;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// błękitny poblask w środku (nosi światło)
INSTANCE ZDF_SIGN_AARD_SWEEP_GLOW (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 20 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 300;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "170 210 255";
	visTexColorEnd_S	= "90 140 255";
	visSizeStart_S		= "90 90";
	visSizeEndScale		= 2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};


// ================================================================ IGNI - strumień ognia (pętla, +-20 st., 5 m)

INSTANCE ZDF_SIGN_IGNI_STREAM_FLAMES (C_PARTICLEFX)
{
	ppsValue			= 220;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 35 55";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "10";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 18;
	dirAngleElev		= -3;
	dirAngleElevVar		= 6;
	velAvg				= 1.15;
	velVar				= 0.19;
	lspPartAvg			= 560;
	lspPartVar			= 100;
	flyGravity_S		= "0 0.0003 0";
	visName_S			= "MFX_FIREBALL.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 190";
	visTexColorEnd_S	= "255 60 10";
	visSizeStart_S		= "8 8";
	visSizeEndScale		= 8;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 170;
	visAlphaEnd			= 0;
};

INSTANCE ZDF_SIGN_IGNI_STREAM_CORE (C_PARTICLEFX)
{
	ppsValue			= 200;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 35 50";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 8;
	dirAngleElev		= -3;
	dirAngleElevVar		= 4;
	velAvg				= 1.45;
	velVar				= 0.13;
	lspPartAvg			= 380;
	lspPartVar			= 50;
	flyGravity_S		= "0 0 0";
	visName_S			= "FIRE_COMPLETE_A0.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 18;
	visTexAniIsLooping	= 1;
	visTexColorStart_S	= "255 255 220";
	visTexColorEnd_S	= "255 150 40";
	visSizeStart_S		= "5 5";
	visSizeEndScale		= 6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

INSTANCE ZDF_SIGN_IGNI_STREAM_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 110;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 35 55";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "12";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 24;
	dirAngleElev		= 6;
	dirAngleElevVar		= 12;
	velAvg				= 1.7;
	velVar				= 0.5;
	lspPartAvg			= 500;
	lspPartVar			= 180;
	flyGravity_S		= "0 -0.0012 0";
	visName_S			= "ZSPARK1.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 140";
	visTexColorEnd_S	= "255 70 0";
	visSizeStart_S		= "2 11";
	visSizeEndScale		= 0.6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

INSTANCE ZDF_SIGN_IGNI_STREAM_EMBERS (C_PARTICLEFX)
{
	ppsValue			= 50;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 25 280";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "30";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 25;
	dirAngleElev		= 10;
	dirAngleElevVar		= 20;
	velAvg				= 0.25;
	velVar				= 0.1;
	lspPartAvg			= 1400;
	lspPartVar			= 400;
	flyGravity_S		= "0 0.00025 0";
	visName_S			= "SPARK.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 160 60";
	visTexColorEnd_S	= "200 30 0";
	visSizeStart_S		= "2 2";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

INSTANCE ZDF_SIGN_IGNI_STREAM_SMOKE (C_PARTICLEFX)
{
	ppsValue			= 30;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 50 420";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "50";
	dirMode_S			= "DIR";
	dirFOR_S			= "OBJECT";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 25;
	dirAngleElev		= 25;
	dirAngleElevVar		= 15;
	velAvg				= 0.15;
	velVar				= 0.05;
	lspPartAvg			= 1800;
	lspPartVar			= 400;
	flyGravity_S		= "0 0.0003 0";
	visName_S			= "FIRESMOKE.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "90 80 70";
	visTexColorEnd_S	= "45 45 45";
	visSizeStart_S		= "25 25";
	visSizeEndScale		= 5;
	visAlphaFunc_S		= "BLEND";
	visAlphaStart		= 90;
	visAlphaEnd			= 0;
};

// pomarańczowy poblask wzdłuż strumienia
INSTANCE ZDF_SIGN_IGNI_STREAM_GLOW (C_PARTICLEFX)
{
	ppsValue			= 3;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 35 220";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 700;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 170 70";
	visTexColorEnd_S	= "255 60 0";
	visSizeStart_S		= "80 80";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 90;
	visAlphaEnd			= 0;
};

// płonąca dłoń w trakcie strumienia
INSTANCE ZDF_SIGN_IGNI_STREAM_HAND (C_PARTICLEFX)
{
	ppsValue			= 45;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "OBJECT";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "5";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 30;
	dirAngleElev		= 90;
	dirAngleElevVar		= 20;
	velAvg				= 0.05;
	velVar				= 0.02;
	lspPartAvg			= 380;
	lspPartVar			= 80;
	flyGravity_S		= "0 0.0002 0";
	visName_S			= "FIREFLARE.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 220 160";
	visTexColorEnd_S	= "255 80 10";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};


// ================================================================ YRDEN - pułapka (glif 3 m, kule-ładunki, pociski)

// glif 3 m, pętla (punkt na ziemi)
INSTANCE ZDF_SIGN_YRDEN_TRAP_GLYPH (C_PARTICLEFX)
{
	ppsValue			= 1.6;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 3 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 1500;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "180 90 255";
	visTexColorEnd_S	= "120 50 255";
	visSizeStart_S		= "290 290";
	visSizeEndScale		= 1.04;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 130;
	visAlphaEnd			= 0;
};

// glif rozbłyska raz na początku
INSTANCE ZDF_SIGN_YRDEN_TRAP_GLYPH_START (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 60;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 4 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 1300;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "225 165 255";
	visTexColorEnd_S	= "130 50 255";
	visSizeStart_S		= "230 230";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// pierścień drobin na obwodzie 3 m
INSTANCE ZDF_SIGN_YRDEN_TRAP_EDGE (C_PARTICLEFX)
{
	ppsValue			= 70;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 6 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "300";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	dirAngleElevVar		= 8;
	velAvg				= 0.045;
	velVar				= 0.02;
	lspPartAvg			= 1000;
	lspPartVar			= 250;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 120 255";
	visTexColorEnd_S	= "100 30 220";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// iskry run biegnące po obwodzie 3 m
INSTANCE ZDF_SIGN_YRDEN_TRAP_RUNES (C_PARTICLEFX)
{
	ppsValue			= 35;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.00035;
	shpIsVolume			= 0;
	shpDim_S			= "300";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	dirAngleElevVar		= 10;
	velAvg				= 0.08;
	velVar				= 0.03;
	lspPartAvg			= 750;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "CFLARESTAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "235 200 255";
	visTexColorEnd_S	= "140 60 255";
	visSizeStart_S		= "9 9";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// kula-ładunek: świecący rdzeń (pętla; lekko unosi się i pulsuje). Środek = punkt wywołania.
INSTANCE ZDF_SIGN_YRDEN_ORB_CORE (C_PARTICLEFX)
{
	ppsValue			= 5;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.012;
	lspPartAvg			= 800;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "225 165 255";
	visTexColorEnd_S	= "140 50 255";
	visSizeStart_S		= "16 16";
	visSizeEndScale		= 1.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// kula: jasny brzeg
INSTANCE ZDF_SIGN_YRDEN_ORB_SHELL (C_PARTICLEFX)
{
	ppsValue			= 3;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 900;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "210 150 255";
	visTexColorEnd_S	= "130 50 255";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 1.1;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 150;
	visAlphaEnd			= 0;
};

// kula: trzaskające wyładowania
INSTANCE ZDF_SIGN_YRDEN_ORB_ARCS (C_PARTICLEFX)
{
	ppsValue			= 25;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "12";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	lspPartAvg			= 160;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "LIGHTNING_BIG_A0.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 18;
	visTexAniIsLooping	= 1;
	visTexColorStart_S	= "230 180 255";
	visTexColorEnd_S	= "150 60 255";
	visSizeStart_S		= "3 9";
	visSizeEndScale		= 1;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// kula: iskry krążące wokół
INSTANCE ZDF_SIGN_YRDEN_ORB_MOTES (C_PARTICLEFX)
{
	ppsValue			= 40;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0012;
	shpIsVolume			= 0;
	shpDim_S			= "20";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 400;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "240 210 255";
	visTexColorEnd_S	= "150 60 255";
	visSizeStart_S		= "2.5 2.5";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// kula: cienka smuga do glifu pod spodem
INSTANCE ZDF_SIGN_YRDEN_ORB_BEAM (C_PARTICLEFX)
{
	ppsValue			= 4;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -45 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.04;
	lspPartAvg			= 600;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_LIGHT_SINGLERAY.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 130 255";
	visTexColorEnd_S	= "120 40 255";
	visSizeStart_S		= "2 45";
	visSizeEndScale		= 1;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 90;
	visAlphaEnd			= 0;
};

// pocisk: głowa (emiter leci od kuli do celu, cząstki zostają w świecie = smuga wyładowań)
INSTANCE ZDF_SIGN_YRDEN_BOLT_HEAD (C_PARTICLEFX)
{
	ppsValue			= 900;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 2.5;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.04;
	velVar				= 0.02;
	lspPartAvg			= 200;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "ELECTRICBLUE0000.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "240 200 255";
	visTexColorEnd_S	= "140 50 255";
	visSizeStart_S		= "12 12";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pocisk: łuki błyskawic wzdłuż toru
INSTANCE ZDF_SIGN_YRDEN_BOLT_ARCS (C_PARTICLEFX)
{
	ppsValue			= 400;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 2.5;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "10";
	dirMode_S			= "RAND";
	velAvg				= 0.03;
	velVar				= 0.01;
	lspPartAvg			= 140;
	lspPartVar			= 40;
	flyGravity_S		= "0 0 0";
	visName_S			= "LIGHTNING_BIG_A0.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexAniFPS		= 18;
	visTexAniIsLooping	= 1;
	visTexColorStart_S	= "235 190 255";
	visTexColorEnd_S	= "150 60 255";
	visSizeStart_S		= "3 16";
	visSizeEndScale		= 1;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pocisk: rozbłysk w punkcie startu (kula)
INSTANCE ZDF_SIGN_YRDEN_BOLT_MUZZLE (C_PARTICLEFX)
{
	ppsValue			= 300;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 20;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "8";
	dirMode_S			= "RAND";
	velAvg				= 0.12;
	velVar				= 0.05;
	lspPartAvg			= 250;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "240 200 255";
	visTexColorEnd_S	= "130 40 255";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pocisk: trafienie (opóźnione ok. 0,12 s - czas lotu)
INSTANCE ZDF_SIGN_YRDEN_BOLT_HIT (C_PARTICLEFX)
{
	ppsValue			= 500;
	ppsScaleKeys_S		= "0 1";
	ppsIsLooping		= 0;
	ppsFPS				= 8;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "15";
	dirMode_S			= "RAND";
	velAvg				= 0.22;
	velVar				= 0.1;
	lspPartAvg			= 380;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "240 200 255";
	visTexColorEnd_S	= "120 30 230";
	visSizeStart_S		= "7 7";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};

// pocisk: znak rozbłyska na trafionym (opóźniony jak wyżej; nosi światło)
INSTANCE ZDF_SIGN_YRDEN_BOLT_HIT_FLASH (C_PARTICLEFX)
{
	ppsValue			= 16;
	ppsScaleKeys_S		= "0 1";
	ppsIsLooping		= 0;
	ppsFPS				= 8;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 320;
	flyGravity_S		= "0 0 0";
	visName_S			= "DECAL_TELEPORTATION.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "230 170 255";
	visTexColorEnd_S	= "130 40 255";
	visSizeStart_S		= "18 18";
	visSizeEndScale		= 2.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
};


// ================================================================ QUEN - aktywna tarcza (pętla) i pochłonięcie

// pulsująca bańka: co ok. 0,6 s jasny puls rośnie i gaśnie
INSTANCE ZDF_SIGN_QUEN_ACTIVE_PULSE (C_PARTICLEFX)
{
	ppsValue			= 1.6;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 650;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 225 140";
	visTexColorEnd_S	= "255 160 40";
	visSizeStart_S		= "100 100";
	visSizeEndScale		= 1.25;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// stała bańka pod pulsem (jaśniejsza niż zwykła tarcza)
INSTANCE ZDF_SIGN_QUEN_ACTIVE_BUBBLE (C_PARTICLEFX)
{
	ppsValue			= 4;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 1000;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 215 120";
	visTexColorEnd_S	= "255 160 40";
	visSizeStart_S		= "108 108";
	visSizeEndScale		= 1.05;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 130;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

INSTANCE ZDF_SIGN_QUEN_ACTIVE_SHELL (C_PARTICLEFX)
{
	ppsValue			= 140;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "102";
	dirMode_S			= "RAND";
	velAvg				= 0.002;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0 0";
	visName_S			= "SIMPLEGLOW.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 210 110";
	visTexColorEnd_S	= "255 140 30";
	visSizeStart_S		= "13 13";
	visSizeEndScale		= 1.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 110;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

INSTANCE ZDF_SIGN_QUEN_ACTIVE_ORBIT (C_PARTICLEFX)
{
	ppsValue			= 140;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 30 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0016;
	shpIsVolume			= 0;
	shpDim_S			= "100";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 420;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE3.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 240 160";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "7 7";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

INSTANCE ZDF_SIGN_QUEN_ACTIVE_ORBIT2 (C_PARTICLEFX)
{
	ppsValue			= 110;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -30 0";
	shpDistribType_S	= "WALK";
	shpDistribWalkSpeed	= 0.0022;
	shpIsVolume			= 0;
	shpDim_S			= "92";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	lspPartAvg			= 380;
	lspPartVar			= 60;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 150";
	visTexColorEnd_S	= "255 120 10";
	visSizeStart_S		= "6 6";
	visSizeEndScale		= 0.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pulsujący pierścień płasko pod stopami
INSTANCE ZDF_SIGN_QUEN_ACTIVE_GROUND (C_PARTICLEFX)
{
	ppsValue			= 1.6;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 -90 0";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= 90;
	velAvg				= 0.00001;
	lspPartAvg			= 650;
	flyGravity_S		= "0 0 0";
	visName_S			= "WATERRING.TGA";
	visOrientation_S	= "VOB";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 220 130";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "60 60";
	visSizeEndScale		= 2.6;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 180;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pochłonięcie: bańka rozbłyska
INSTANCE ZDF_SIGN_QUEN_ABSORB_FLASH (C_PARTICLEFX)
{
	ppsValue			= 120;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 60;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 320;
	flyGravity_S		= "0 0 0";
	visName_S			= "MAGICDUMMY_2.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 245 200";
	visTexColorEnd_S	= "255 170 50";
	visSizeStart_S		= "106 106";
	visSizeEndScale		= 1.2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pochłonięcie: iskry na powierzchni bańki
INSTANCE ZDF_SIGN_QUEN_ABSORB_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 1200;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 25;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 8 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "102";
	dirMode_S			= "RAND";
	velAvg				= 0.08;
	velVar				= 0.04;
	lspPartAvg			= 380;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "PFXFLARE3.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 240 170";
	visTexColorEnd_S	= "255 130 10";
	visSizeStart_S		= "7 7";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pochłonięcie: leczenie - złoto-zielone iskierki unoszą się z bohatera (najdłuższa część)
INSTANCE ZDF_SIGN_QUEN_ABSORB_HEAL (C_PARTICLEFX)
{
	ppsValue			= 400;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 6;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "40";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 45;
	dirAngleElev		= 90;
	dirAngleElevVar		= 20;
	velAvg				= 0.12;
	velVar				= 0.04;
	lspPartAvg			= 800;
	lspPartVar			= 200;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 140";
	visTexColorEnd_S	= "150 255 120";
	visSizeStart_S		= "3.5 3.5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 255;
	visAlphaEnd			= 0;
	useEmittersFOR		= 1;
};

// pochłonięcie: złoty poblask (nosi światło)
INSTANCE ZDF_SIGN_QUEN_ABSORB_GLOW (C_PARTICLEFX)
{
	ppsValue			= 60;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 0;
	ppsFPS				= 30;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 20 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 250;
	flyGravity_S		= "0 0 0";
	visName_S			= "ZFLARE6.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 230 150";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "40 40";
	visSizeEndScale		= 2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 220;
	visAlphaEnd			= 0;
};


// ================================================================ AXII - marionetka (pętla)

// szmaragdowa aura wokół ciała (węzeł BIP01)
INSTANCE ZDF_SIGN_AXII_PUPPET_AURA (C_PARTICLEFX)
{
	ppsValue			= 16;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "45";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleHead		= 0;
	dirAngleHeadVar		= 45;
	dirAngleElev		= 90;
	dirAngleElevVar		= 30;
	velAvg				= 0.03;
	velVar				= 0.01;
	lspPartAvg			= 1400;
	lspPartVar			= 300;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_MAGICCLOUD.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "110 255 160";
	visTexColorEnd_S	= "20 170 80";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 2.5;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 90;
	visAlphaEnd			= 0;
};

// drobiny wokół ciała
INSTANCE ZDF_SIGN_AXII_PUPPET_SPARKS (C_PARTICLEFX)
{
	ppsValue			= 28;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 10 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "55";
	dirMode_S			= "RAND";
	velAvg				= 0.01;
	lspPartAvg			= 900;
	lspPartVar			= 200;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "190 255 210";
	visTexColorEnd_S	= "40 210 110";
	visSizeStart_S		= "3 3";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 230;
	visAlphaEnd			= 0;
};

// nitki lalkarza: pionowe świetliste linie spływające ku głowie (węzeł BIP01 HEAD)
INSTANCE ZDF_SIGN_AXII_PUPPET_STRINGS (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "CIRCLE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 75 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 0;
	shpDim_S			= "22";
	dirMode_S			= "DIR";
	dirFOR_S			= "WORLD";
	dirAngleElev		= -90;
	dirAngleElevVar		= 4;
	velAvg				= 0.06;
	lspPartAvg			= 1100;
	lspPartVar			= 100;
	flyGravity_S		= "0 0 0";
	visName_S			= "MFX_LIGHT_SINGLERAY.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 255 220";
	visTexColorEnd_S	= "40 220 120";
	visSizeStart_S		= "1.2 22";
	visSizeEndScale		= 1;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 210;
	visAlphaEnd			= 0;
};

// krzyżak lalkarza: wir nad głową
INSTANCE ZDF_SIGN_AXII_PUPPET_CROSS (C_PARTICLEFX)
{
	ppsValue			= 3;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 82 0";
	dirMode_S			= "RAND";
	velAvg				= 0.003;
	velVar				= 0.001;
	lspPartAvg			= 900;
	flyGravity_S		= "0 0 0";
	visName_S			= "SHOCKLIGHT.TGA";
	visOrientation_S	= "VELO";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "150 255 190";
	visTexColorEnd_S	= "30 200 100";
	visSizeStart_S		= "10 10";
	visSizeEndScale		= 1.4;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// krzyżak lalkarza: miękka poświata
INSTANCE ZDF_SIGN_AXII_PUPPET_HALO (C_PARTICLEFX)
{
	ppsValue			= 3;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "POINT";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 82 0";
	dirMode_S			= "RAND";
	velAvg				= 0.0001;
	lspPartAvg			= 900;
	flyGravity_S		= "0 0 0";
	visName_S			= "SIMPLEGLOW.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "80 240 140";
	visTexColorEnd_S	= "20 160 80";
	visSizeStart_S		= "14 14";
	visSizeEndScale		= 1.2;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 120;
	visAlphaEnd			= 0;
};


// ################################################################ RUNDA 5: iskierki po rzucie (niosą dźwięk rzutu)

// dłoń po rzucie (pętla do emFXLifeSpan efektu SPELLFX_AARD_FLASH): bladobłękitne iskierki - niosą dźwięk rzutu
INSTANCE ZDF_SIGN_AARD_ECHO (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	velVar				= 0.01;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.00005 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "200 230 255";
	visTexColorEnd_S	= "100 160 255";
	visSizeStart_S		= "2.5 2.5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// dłoń po rzucie (pętla do emFXLifeSpan efektu SPELLFX_IGNI_FLASH): żar unoszący się z dłoni - niosą dźwięk rzutu
INSTANCE ZDF_SIGN_IGNI_ECHO (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	velVar				= 0.01;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.00025 0";
	visName_S			= "SPARK.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 190 90";
	visTexColorEnd_S	= "200 40 0";
	visSizeStart_S		= "1.6 1.6";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// dłoń po rzucie (pętla do emFXLifeSpan efektu SPELLFX_YRDEN_FLASH): fioletowe iskierki - niosą dźwięk rzutu
INSTANCE ZDF_SIGN_YRDEN_ECHO (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	velVar				= 0.01;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.00005 0";
	visName_S			= "MFX_SLEEP_STAR.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "225 180 255";
	visTexColorEnd_S	= "120 40 255";
	visSizeStart_S		= "2.5 2.5";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};

// dłoń po rzucie (pętla do emFXLifeSpan efektu SPELLFX_QUEN_FLASH): złote iskierki - niosą dźwięk rzutu
INSTANCE ZDF_SIGN_QUEN_ECHO (C_PARTICLEFX)
{
	ppsValue			= 14;
	ppsScaleKeys_S		= "1";
	ppsIsLooping		= 1;
	ppsFPS				= 1;
	shpType_S			= "SPHERE";
	shpFOR_S			= "WORLD";
	shpOffsetVec_S		= "0 0 0";
	shpDistribType_S	= "RAND";
	shpIsVolume			= 1;
	shpDim_S			= "6";
	dirMode_S			= "RAND";
	velAvg				= 0.02;
	velVar				= 0.01;
	lspPartAvg			= 600;
	lspPartVar			= 150;
	flyGravity_S		= "0 0.00005 0";
	visName_S			= "PFXFLARE1.TGA";
	visOrientation_S	= "NONE";
	visTexIsQuadPoly	= 1;
	visTexColorStart_S	= "255 235 150";
	visTexColorEnd_S	= "255 140 20";
	visSizeStart_S		= "3 3";
	visSizeEndScale		= 0.3;
	visAlphaFunc_S		= "ADD";
	visAlphaStart		= 200;
	visAlphaEnd			= 0;
};
