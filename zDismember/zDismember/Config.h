// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Ustawienia z Gothic.ini [ZDISMEMBER]; brakujace klucze sa dopisywane z wartosciami domyslnymi.
	struct ZConfig {
		bool enabled = true;
		int chancePlayer = 100;
		int chanceNpc = 100;
		int finisherChance = 30;
		int knockoutFinisher = 1;       // cios, ktory powalilby czlowieka: 0 nie, 1 tylko trwale wrogowie, 2 wszyscy poza przyjaciolmi
		bool dazeEnabled = true;
		float dazeSeconds = 5.0f;
		float dazeGrace = 0.6f;
		bool heroInvulnerable = true;
		int aivarIndex = 95;
		int maxGibs = 24;
		float gibLifetime = 180.0f;
		float gibDespawnDist = 4000.0f;
		float maxVictimHeight = 230.0f;
		float staggerRadius = 800.0f;   // walczacy w poblizu zataczaja sie i czekaja w trakcie finishera (0 = wylaczone)
		bool movesEnabled = true;       // uniki/przewroty z moves_w3.ini
		int dodgeKey = 47;              // kod DIK: 47 = V (lewy Alt to w Gothicu skok)
		int rollKey = 56;               // kod DIK: 56 = lewy Alt (w walce przejety ze skoku)
		bool dodgeKeyCombatOnly = true; // DodgeKeyCombatOnly: w walce klawisz uniku nie dochodzi do gry ani innych wtyczek (C = receptury NB)
		int doubleTap = 0;              // DoubleTapDodge: dwuklik kierunku - 0 wyl., 1 z dobyta bronia, 2 z bronia i Shiftem
		int doubleTapMove = 0;          // DoubleTapMove: dwuklik daje 0 unik, 1 przewrot
		float doubleTapWindow = 0.25f;  // DoubleTapWindow: drugie wcisniecie najwyzej tyle s po pierwszym
		bool dodgeWithAction = true;    // DodgeWithAction: unik / przewrot takze z trzymana akcja (postawa ataku Gothica 1)
		float jumpAttackWindow = 0.30f; // JumpAttackWindow: atak najwyzej tyle s po klawiszu skoku = atak z wyskoku (z W co najmniej 0,40)
		bool traversal = true;          // Alt w biegu przed przeszkoda: przeskok / wspiecie W3 (traversal_w3.ini)
		bool traversalCombat = true;    // tez z dobyta bronia / piesciami (pierwszenstwo przed przewrotem Alt+W, Alt)
		bool traversalDebug = false;    // debug wizualny przeskokow (jak zdism trav show): promienie, sciana, gora, decyzja
		bool traversalAirGrab = true;   // chwyt krawedzi w locie (skok gry, spadanie) - zawis Gothica
		bool traversalSheathe = true;   // TraversalSheathe: przeskok / wspiecie z bronia w reku - bron chowana od razu, ruch bez broni (W3)
		bool traversalRedraw = true;    // TraversalRedraw: po wyladowaniu bron schowana przez TraversalSheathe od razu z powrotem w rece
		int traversalHint = 3;          // TraversalHint: napis "Alt - przeskocz" przy biegu na przeszkode - tyle pierwszych razy (0 = wyl.)
		int traversalHintShown = 0;     // TraversalHintShown: ile juz pokazano (licznik zapisywany przez plugin; 0 = od nowa)
		bool jumpKeepMomentum = true;   // skok gry z biegu / sprintu utrzymuje pozioma predkosc odbicia (do 850 cm/s)
		bool fallForgiveness = true;    // bez spadania nad szczelina / dziura pod srodkiem, gdy cialo ma podparcie
		int fallForgiveDrop = 40;       // ...podloga na obwodzie ciala najwyzej tyle cm pod stopami
		bool teleportGuard = true;      // teleport kolizji (silnik przenosi bohatera o metry w klatce) - cofniety do startu klatki
		bool counters = true;           // blok w trakcie zamachu przeciwnika = kontra (odbicie + riposta)
		int counterRepelChance = 40;    // % kontr jako repel W3 (bark / kopniecie - przeciwnik zatacza sie do tylu)
		bool counterOnBackstep = false; // CounterOnBackstep: odskok w tyl (T_xPARADEJUMPB) tez daje kontre (0 = zwykly odskok)
		float counterWindow = 0.0f;     // CounterWindow: blok najwyzej tyle s przed trafieniem wroga (0 = caly zamach)
		bool rapierThrustOnly = true;   // rapier: finishery tylko pchnieciem, bez odcinania
		std::vector<std::string> rapierTokens;   // RapierWeapons: fragmenty nazwy instancji albo modelu broni (RAPIER,DEGEN)
		std::vector<std::string> bluntTokens;    // BluntWeapons: bron z wygladu obuchowa (instancja / model) - nie odcina, bez finisherow
		zdcore::WeaponRules weapons;    // reguly broni zlozone z trzech kluczy powyzej (zdcore::ClassifyWeapon)
		bool finisherEarlyRelease = true;   // sterowanie wraca, gdy bohater przestaje sie ruszac (0 = na koniec animacji)
		float finisherBlend = 0.3f;     // przejscie z pozy finishera w postawe walki (s)
		int finisherSlowMo = zdcore::SLOWMO_FULL;   // FinisherSlowMo: spowolnienie czasu w finisherze - 0 wyl., 1 lekkie, 2 jak w katalogu
		bool finisherSlowMoLastEnemy = false;       // FinisherSlowMoLastEnemy: spowolnienie tylko, gdy w poblizu nie walczy inny wrog
		bool w3Locomotion = true;       // chod i trucht Geralta (bez broni zawsze, z bronia w stylu W3)
		bool w3Sprint = true;           // w sprincie gry / zEBA animacja sprintu Geralta (logika i tempo zostaja z gry)
		float w3SprintArmedSpeed = 600.0f;   // W3SprintArmedSpeed: sprint W3 z bronia w tym tempie (cm/s; 0 = tempo sprintu gry/zEBA, 474)
		float w3SprintUnarmedSpeed = 0.0f;   // W3SprintUnarmedSpeed: sprint W3 bez broni (cm/s; 0 = tempo sprintu gry - np. sprint maga NB 451)
		// Unik / przewrot (Dodge.cpp, Core/Evade; 17.10): okno wyjscia z buforem, obrot do wroga na koncu, unik w ostatniej chwili
		bool evadeEarlyExit = true;     // EvadeEarlyExit: atak / blok / kolejny ruch wcisniety w trakcie wychodzi od exit= z moves_w3.ini
		float evadeExitBuffer = 1.0f;   // EvadeExitBuffer: wcisniecie w trakcie ruchu pamietane najwyzej tyle s
		bool evadeFaceEnd = true;       // EvadeFaceEnd: koniec uniku / przewrotu przodem do wroga
		float evadeFaceLead = 0.25f;    // EvadeFaceLead: obrot zaczyna sie tyle s przed oknem wyjscia (w nim patrzy na wroga)
		float evadeFaceRate = 540.0f;   // EvadeFaceRate: najszybszy obrot (st./s)
		bool perfectDodge = true;       // PerfectDodge: unik / przewrot tuz przed ciosem wroga = wrog traci namiar, pierwszy cios szybszy
		float perfectDodgeWindow = 0.35f;   // PerfectDodgeWindow: najwyzej tyle s przed trafieniem wroga
		float perfectDodgeRange = 450.0f;   // PerfectDodgeRange: wrog w zamachu najwyzej tyle cm od bohatera
		float perfectDodgeLostTime = 0.8f;  // PerfectDodgeLostTime: tyle s wrog obraca sie wolniej i nie zaczyna ataku
		float perfectDodgeTurnRate = 60.0f; // PerfectDodgeTurnRate: jego obrot najwyzej tyle st./s
		float perfectDodgeHeroSpeed = 1.2f; // PerfectDodgeHeroSpeed: pierwszy cios bohatera w tym tempie (x)
		float perfectDodgeHeroWindow = 1.0f;   // PerfectDodgeHeroWindow: ...gdy zacznie sie najwyzej tyle s po uniku
		bool perfectDodgeFx = true;     // PerfectDodgeFx: blysk przy wrogu
		std::string perfectDodgeSound = "WHOOSH";   // PerfectDodgeSound: dzwiek (instancja C_SFX; pusty = bez)
		bool w3Style = true;            // styl walki Wiedzmina sam od progu talentu (1H szybkie, 2H mocne ciecia)
		int w3StyleMinTalent = 60;      // prog talentu (%) dla stylu W3 w AUTO
		bool w3AttackAssist = true;     // ciosy W3 bohatera: doskok do wroga w zamachu (obrot + dosuniecie do zasiegu)
		float w3AttackReach = 40.0f;    // ciosy W3 bohatera: premia zasiegu w tescie trafienia silnika (cm, 0 = brak)
		float w3AssistMargin = 25.0f;   // doskok: idealna odleglosc w chwili ciosu = zasieg walki - tyle (cm)
		float w3AssistMaxPath = 130.0f; // doskok: najdluzsza droga na jedno ciecie (cm)
		float w3AssistSpeed = 450.0f;   // doskok: najwieksza predkosc (cm/s)
		float w3AssistCone = 45.0f;     // doskok: wrog najwyzej tyle st. od przodu bohatera
		// Tempo ciosow stylu W3 (AtkTempo.cpp): W3AtkSpeed1HLeft/1HRight/1HCombo/2HLeft/2HRight/2HCombo = % tempa animacji
		// (fpsRate nakladki W3 x %/100; 100 = bez zmian, 0 = jak natywne - tempo kolejnego ciosu animacji spod nakladki W3)
		int w3AtkSpeed[zdcore::ATK_SLOT_COUNT] = { 100, 100, 100, 100, 100, 100 };
		int w3Side2H = 0;               // W3Side2H: ciecia boczne 2H stylu W3 - 0 W3 (mocne), 1 W3 szybkie (dwureczne), 2 natywne
		bool ragdoll = true;            // zwloki padaja fizycznie (poza finisherami)
		int ragdollChance = 100;
		float ragdollImpulse = 180.0f;  // predkosc pchniecia gornej czesci ciala po ciosie (cm/s)
		int ragdollMaxActive = 4;       // ile zwlok symuluje sie naraz
		int styleKey = 22;              // kod DIK cyklu stylow (22 = U, z Shiftem wstecz); 0 = tylko konsola
		int locoKey = 24;               // kod DIK cyklu zestawow ruchu do testow (24 = O, z Shiftem wstecz); 0 = tylko konsola
		int locoMode = 0;               // zestaw ruchu: 0 AUTO (W3Locomotion/W3Sprint), 1 Gothic/zEBA, 2 W3 trucht, 3 W3 trucht+sprint, 4 podglad sprintu
		// Zamiana animacji 1H <-> 2H bohatera (AniSwap.cpp): WeaponSwap (0 wyl., 1 1H jak 2H, 2 2H jak 1H, 3 obie),
		// WeaponSwapLast, WeaponSwapLevel (0 talent broni w rece, 1 talent drugiego typu), WeaponSwapPerWeapon,
		// WeaponSwapWeapons (instancje broni z wlaczona zamiana, przecinki)
		zdcore::SwapSettings swap;
		int swapKey = zdcore::SWAP_KEY_DEFAULT;     // WeaponSwapKey: kod DIK (22 = U); 0 = tylko konsola (zdism swap)
		int swapMods = zdcore::SWAP_MODS_DEFAULT;   // WeaponSwapMods: 1 Ctrl + 2 Shift + 4 Alt (3 = Ctrl+Shift)
		// Podnoszenie z dobyta bronia (Loot.cpp): klawisz ataku podnosi przedmiot przed bohaterem, gdy wrogow brak
		bool lootArmed = true;          // LootArmed
		float lootRange = 150.0f;       // LootRange: przedmiot najwyzej tyle cm od bohatera (w poziomie, +-45 st.)
		float lootEnemyRadius = 1000.0f;   // LootEnemyRadius: walczacy wrog blizej niz tyle cm = klawisz atakuje (0 = bez sprawdzania)
		bool lootHint = true;           // LootHint: nazwa przedmiotu / celu nad nim, gdy atak by go wzial
		// Interakcje z dobyta bronia (LootInteract.cpp; dzialaja przy LootArmed=1, numer pozycji menu = wartosc)
		int lootDoors = 2;              // LootArmedDoors: 0 wyl., 1 tylko bez zamka i skryptu (z bronia w rece), 2 tez zamkniete / ze skryptem (schowanie broni)
		bool lootChests = true;         // LootArmedChests: skrzynie (schowanie broni, zwykle otwarcie gry, dobycie)
		bool lootCorpses = true;        // LootArmedCorpses: zwloki i nieprzytomni (okno przeszukania, potem dobycie)
		bool lootRedraw = true;         // LootArmedRedraw: dobycie broni po oknie / interakcji, gdy bron zostala schowana
		bool parryFx = true;            // udany blok: iskry z blyskiem (metal o metal) / drzazgi (drewno) w punkcie zetkniecia broni
		bool parryRecoil = true;        // udany blok: krotki odrzut broni, rak i tulowia u broniacego i napastnika
		float parryRecoilDeg = 5.0f;    // ParryRecoilStrength: kat wezla w szczycie przy wadze 1 (stopnie; bron ~10-15 st.)
		float parryRecoilTime = 0.18f;  // ParryRecoilTime: czas odrzutu broniacego (s; napastnik x1,15)
		std::vector<std::string> parryWoodTokens;   // ParryWoodWeapons: bron drewniana mimo materialu (instancja / model)
		int weightsSwing[3] = { 50, 35, 15 };
		int weightsThrust[3] = { 20, 0, 0 };
		bool debug = false;
		bool profiler = true;           // Profiler: lekki pomiar sekcji + przycinki do logu (konsola: zdism prof)
		float profSpikeMs = 25.0f;      // ProfSpikeMs: przycinka = klatka dluzsza niz max(tyle, 2 x mediana 120 klatek)
		float profSummarySeconds = 0.0f;   // ProfSummarySeconds: podsumowanie do logu co tyle s (0 = tylko na zadanie)
		bool profSampler = true;        // ProfSampler: probkowanie watku gry (gdzie idzie czas w przycinkach; dziala przy Profiler=1)
		int profSampleMs = 1;           // ProfSampleMs: odstep probek w ms
		bool preloadOverlays = true;    // PreloadOverlays: nakladki bohatera wczytane przy wczytaniu swiata i trzymane w pamieci
		// Znaki Wiedzmina (Signs.cpp, kolejnosc tablic: Aard, Igni, Axii, Yrden, Quen)
		bool signs = true;              // Signs: skutki run znakow (0 = runy znakow nic nie robia, komunikat)
		int signSpellIds[5] = { 90, 91, 92, 93, 94 };   // SignSpellIds: sloty czarow (SPL_Aard..SPL_Quen w skryptach)
		int signManaCost[5] = { 10, 15, 15, 20, 15 };   // SignManaCost
		std::string signAniLetters[5] = { "WND", "FBT", "SLE", "FEA", "HEA" };   // SignAniLetters: gest rzutu (tryb magii)
		float signCooldown = 1.0f;      // SignCooldown: odnowienie kazdego znaku (s)
		bool signHitNeutral = true;     // SignHitNeutral: Aard / Igni / pekniecie Quen tez w neutralnych / zlych ludziach
		                                // (przestepstwo jak czar gry); potwory nie-przyjaciele zawsze, przyjaciele nigdy
		float aardDamage = 15.0f, aardRange = 600.0f, aardCone = 50.0f, aardPush = 150.0f;   // obrazenia DAM_FLY, cm, +-st., cm
		int aardFlyMaxProt = 160;       // AardFlyMaxProt: leca (lot silnika) cele z PROT_FLY do tylu (orkowie 130-160); -1 = nikt
		float aardFlyMaxHeight = 230.0f;   // AardFlyMaxHeight: i model nie wyzszy niz tyle cm (troll, smok - potkniecie)
		float aardFlyCheck = 0.25f;     // AardFlyCheck: po tylu s sprawdzenie, czy cel naprawde polecial (silnik)
		float aardFlyMinMove = 50.0f;   // AardFlyMinMove: tyle cm od bohatera = polecial
		float aardFallbackPush = 250.0f;   // AardFallbackPush: nie polecial - potkniecie + odsuniecie o tyle cm
		float igniDamage = 40.0f, igniRange = 650.0f, igniCone = 35.0f;   // obrazenia DAM_FIRE, cm, +-st.
		float igniBurnTime = 3.0f, igniBurnDps = 5.0f, igniReactTime = 1.5f;   // plonienie silnika (s, pkt/s), reakcja ludzi (s)
		float axiiStunTime = 4.0f, axiiRange = 1000.0f, axiiCone = 20.0f;   // oszolomienie (s), AxiiRange (cm), AxiiCone (+-st.)
		bool axiiFinisher = true;       // AxiiFinisher: cios w kleczacego oszolomionego = egzekucja (finisher POSE_DAZED)
		uint32_t axiiStunMask = 0;      // AxiiStunAnis: warianty oszolomienia ludzi (zdcore::ParseStunAnis)
		float yrdenRadius = 500.0f, yrdenTime = 15.0f, yrdenSlow = 0.65f, yrdenArmor = 30.0f;   // cm, s, tempo x, ochrona -%
		float quenTime = 30.0f, quenRadius = 250.0f, quenDamage = 10.0f, quenPush = 120.0f;     // s, cm, obrazenia, cm
		// Znaki - runda 3: moc z maks. many (P = 1 + min(SignPowerMax, mana max x SignPowerPerMana)) i tryby alternatywne
		float signPowerPerMana = 0.001f, signPowerMax = 1.0f;
		// SignPowerInt: obrazenia znakow +N% za kazde 10 INT (New Balance, ATR_INTELLECT; Steam bez INT - bez zmian),
		// SignPowerIntMax: najwiekszy mnoznik obrazen z INT
		float signPowerInt = 10.0f, signPowerIntMax = 5.0f;
		// SignPowerSource: skalowanie WSZYSTKICH obrazen znakow (trafienia, palenie, strumien, fala, Quen, pulapka Yrden):
		// 0 = moc z maks. many, 1 = INT, 2 = iloczyn obu (domyslnie - jak 20.10); czasy zawsze z many. Bez INT (Steam) 1/2 = 0.
		int signPowerSource = 2;
		int signDamageArray = -1;       // SignDamageArray: obrazenia w aryDamage (-1 samo przy NB / UNION_ABI, 0 nie, 1 tak)
		float signAltHold = 0.5f;       // SignAltHold: przytrzymanie rzutu dluzej = tryb alternatywny (0 = bez trybow)
		float yrdenSlowMin = 0.40f, yrdenArmorMax = 50.0f;   // Yrden przy P = 2 (tempo x, ochrona -%)
		int aardAltCost = 20;           // Aard - fala wokol (360 st.)
		float aardAltRadius = 400.0f, aardAltDamage = 0.8f;  // promien (cm), mnoznik obrazen Aarda
		float igniStreamDps = 30.0f, igniStreamManaPerSec = 12.0f, igniStreamCone = 20.0f, igniStreamRange = 700.0f;
		float igniStreamTick = 0.25f;   // Igni - strumien: takt obrazen (s)
		int yrdenAltCost = 25;          // Yrden - pulapka-mina
		float yrdenTrapRadius = 300.0f, yrdenTrapTime = 30.0f, yrdenTrapDamage = 35.0f, yrdenTrapInterval = 0.8f;
		int yrdenTrapCharges = 3;
		float quenActiveManaPerSec = 10.0f, quenActiveHeal = 0.5f;   // Quen - aktywna tarcza
		int axiiAltCost = 25;           // Axii - marionetka
		float axiiPuppetTime = 15.0f, axiiPuppetRange = 1500.0f;   // s, zasieg szukania wrogow (cm)
		// Znaki - runda 4: ruch w trakcie podtrzymania (strumien Igni, aktywna tarcza Quen), koniec marionetki
		float signChannelMoveScale = 0.6f;   // SignChannelMoveScale: tempo bohatera (ruch i animacje) w trakcie podtrzymania
		int signChannelFrame = 10;           // SignChannelFrame: klatka gestu znaku trzymana w podtrzymaniu (wyciagnieta dlon)
		// SignAltInPlace (23.10): tryb alternatywny Aard / Yrden z zQuickSign (gest "w ziemie" calym cialem) - bohater stoi
		// w miejscu do konca gestu (blokada ruchu), skutek w chwili dotkniecia ziemi; 0 = jak dawniej (od razu, bez blokady)
		bool signAltInPlace = true;
		float axiiPuppetEndStun = 1.5f;      // AxiiPuppetEndStun: po czasie marionetki krotkie oszolomienie (s, 0 = od razu walka)
		// Znaki - runda 5
		bool signFires = true;               // SignFires: Aard gasi, Igni podpala ogniska / piece / pochodnie (oCMobFire)
		float axiiTranceTime = 20.0f;        // AxiiTranceTime: Axii poza walka - trans czlowieka (s x moc, 0 = tylko niepamiec)
		float signAimRange = 1500.0f;        // SignAimRange: celowanie (GFA / celownik F zQuickSign): najdalszy punkt kregu Yrden (cm)
		// Znaki - runda 6 (Steam domyslnie wylaczone, New Balance: SignAardUnlock=1, SignAxiiCharm=1, premia 25)
		bool signAardUnlock = false;         // SignAardUnlock: Aard otwiera zamki w stozku (NB jak runa otwierania, Steam - wytrych)
		bool signAxiiCharm = false;          // SignAxiiCharm: Axii poza walka tez jak Urok (Spell_Cast_Charm, lista odpornych)
		int axiiPickpocketBonus = 0;         // SignAxiiPickpocketBonus: +% szansy kradziezy u celu Axii (NB, 0 = wyl.)
		float axiiPickpocketTime = 60.0f;    // SignAxiiPickpocketTime: czas premii (s, 0 = tyle co trans)
		// Medytacja W3 (Meditate.cpp, spec [dokument projektowy - niepublikowany])
		bool meditate = true;                // Meditate
		int meditateKey = 28;                // MeditateKey: kod DIK (28 = Enter; 0 = brak)
		int meditateKeyAlt = 51;             // MeditateKeyAlt: drugi klawisz (51 = przecinek) - przejety od gry
		int meditateKeyAltMods = 1;          // MeditateKeyAltMods: modyfikatory drugiego klawisza (1 Ctrl, 2 Shift, 4 Alt)
		int meditateSpeed = 20;              // MeditateSpeed: tempo czasu w czekaniu (5..100, zmieniane tez w oknie medytacji)
		int meditateRegen = 1;               // MeditateRegen: 0 brak, 1 stopniowa, 2 jak sen (zapamietywana z okna medytacji)
		int meditateRegenHours = 8;          // MeditateRegenHours: stopniowa - pelne HP / mana po tylu godzinach gry
		int meditateSleepHours = 6;          // MeditateSleepHours: jak sen - pelne po co najmniej tylu godzinach czekania
		bool meditateCam = true;             // MeditateCam: kamera orbitalna w medytacji (mysz obraca kamere wokol bohatera)
		float meditateCamSens = 1.0f;        // MeditateCamSens: mnoznik czulosci orbity (na czulosc myszy z opcji gry, 0,1..5)
		// Gildia i sciezki New Balance (PathMenu.cpp, mapa [dokument projektowy - niepublikowany]) - tylko gdy sa symbole NB
		bool pathMenu = true;                // PathMenu: zakladki Gildia / Sciezki w medytacji, klawisz i pilnowanie wylaczen
		int pathMenuKey = 26;                // PathMenuKey: kod DIK okna bez klekania (26 = '[', wolny w NB / Steam / Kronikach; 0 = brak)
		int pathMenuKeyMods = 0;             // PathMenuKeyMods: modyfikatory (1 Ctrl, 2 Shift, 4 Alt)
		bool pathDialogGuard = true;         // PathDialogGuard: w rozmowie wylaczone umiejetnosci wracaja (nauczyciel nie sprzeda drugi raz)
		// Mysz w oknach pluginu (ZMenu.cpp): kursor, najechanie, LPM / PPM / kolko; w medytacji obrot kamery z PPM
		bool menuMouse = true;               // MenuMouse
		float menuMouseSpeed = 1.0f;         // MenuMouseSpeed: piksele kursora na jednostke myszy (z czuloscia z opcji gry)
		// Miecze na plecach (BackSwords.cpp, spec [dokument projektowy - niepublikowany]; animacje make_draw.py)
		bool backSwords = true;              // BackSwords: dobywanie zza plecow W3 (2H, 1H z Back1H), drugi miecz na plecach
		int back1H = 1;                      // Back1H: 0 przy pasie, 1 miecze (ostrze) na plecach, 2 kazda bron 1H na plecach
		int backDrawSpeed = 100;             // BackDrawSpeed: % tempa dobycia zza plecow (100 = ok. 0,64 s, chwyt 0,32 s)
		int backSheatheSpeed = 100;          // BackSheatheSpeed: % tempa chowania na plecy (100 = ok. 0,68 s)
		int beltDrawSpeed = 150;             // BeltDrawSpeed: % tempa natywnego dobycia / chowania 1H przy pasie (100 = gra)
		bool secondSword = true;             // SecondSword: drugi miecz na plecach i zamiana klawiszem
		int secondSwordKey = 34;             // SecondSwordKey: kod DIK (34 = G; 0 = tylko konsola); w ekwipunku = wybor
		int secondSwordKeyMods = 0;          // SecondSwordKeyMods: 1 Ctrl, 2 Shift, 4 Alt
		int secondWithRanged = 0;            // SecondWithRanged: przy luku / kuszy drugi miecz 0 ukryty, 1 zamiast luku
		float backAdjust[2][6] = {};         // BackAdjustA / BackAdjustB: x y z (cm) rx ry rz (st.) w ukladzie miecza
		// Szybki strzal z kuszy (Crossbow.cpp, spec [dokument projektowy - niepublikowany]; animacje make_xbow.py)
		bool crossbow = true;                // Crossbow: szybki strzal z kuszy spod F (slot kuszy w zQuickSign)
		int xbowSpeed = 100;                 // XbowSpeed: % tempa animacji i chwil strzalu (100 = jak W3: belt po ok. 0,5 s)
		int xbowCooldownMs[3] = { 1320, 1200, 960 };   // XbowCooldownMs: przeladowanie wg talentu kuszy 0 / 1 / 2 (ms)
		float xbowAutoRange = 3000.0f;       // XbowAutoRange: zasieg celu automatycznego (cm)
		float xbowAutoCone = 40.0f;          // XbowAutoCone: pol kata stozka kamery (st.)
		float xbowMoveCone = 100.0f;         // XbowMoveCone: w ruchu cel najwyzej tyle st. od kierunku biegu
		bool xbowAutoNeutral = false;        // XbowAutoNeutral: cel automatyczny takze nie-wrogi (przestepstwo!)
		bool xbowWithShield = true;          // XbowWithShield: strzal z tarcza NB (1) / odmowa (0)
		bool xbowWithDual = true;            // XbowWithDual: strzal z bronia parowa NB - druga bron ukryta (1) / odmowa (0)
		bool xbowWithTorch = false;          // XbowWithTorch: strzal z pochodnia w lewej rece (0 = odmowa z komunikatem)
		bool xbowHitCancel = true;           // XbowHitCancel: trafienie bohatera przed beltem przerywa strzal (jak W3)
		int xbowHandScale = 100;             // XbowHandScale: % wielkosci kuszy w lewej dloni
		float xbowHandAdjust[6] = {};        // XbowHandAdjust: x y z (cm) rx ry rz (st.) kuszy w dloni
		float xbowMuzzleCm = 10.0f;          // XbowMuzzleGripCm: wylot beltu - tyle cm przed chwytem kuszy (dlonia)
		float xbowBoltSpeed = 3600.0f;       // XbowBoltSpeed: predkosc beltu (cm/s; silnik 2000, New Balance 3600)
		int xbowAimSlowmo = 100;             // XbowAimSlowmo: % tempa gry w celowaniu (100 = bez; modul czasu menu kolowego)
		// Menu kolowe szybkiego wyboru (Wheel.cpp, spec [dokument projektowy - niepublikowany])
		bool wheel = true;                   // Wheel: przytrzymanie WheelKey otwiera kolo (stukniecie = klawisz dla gry)
		int wheelKey = 58;                   // WheelKey: kod DIK (58 = Caps Lock; 15 = Tab - stukniecie wraca do gry jako ekwipunek)
		bool wheelTapWalk = true;            // WheelTapWalk: stukniecie Caps Lock = przelacznik chodu pluginu (zamiast przelacznika Windows)
		bool wheelCapsRestore = true;        // WheelCapsRestore: po puszczeniu Caps Lock przelacznik Windows (dioda) cofniety
		bool wheelCapsPatch = true;          // WheelCapsPatch: kolo pod Caps Lock - silnik nie czyta przelacznika Caps Lock (chod)
		int wheelHoldMs = 250;               // WheelHoldMs: prog przytrzymania (ms)
		int wheelSlowmo = 15;                // WheelSlowmo: % tempa gry przy otwartym kole (100 = bez spowolnienia)
		int wheelDeadZone = 40;              // WheelDeadZone: martwa strefa myszy (jednostki ruchu myszy)
		int wheelRadius = 300;               // WheelRadius: promien kola w pikselach przy 1080p (skalowany z ekranem)
		bool wheelClickConfirm = true;       // WheelClickConfirm: LPM zatwierdza bez puszczania Tab
		int wheelMouseSens = 100;            // WheelMouseSens: % czulosci myszy w kole

		// fromMenu: po wyjsciu z menu opcji - pozycje przestawione w menu Union (klucze Menu*) wygrywaja z ini.
		void Load(bool fromMenu = false);
		// Klucze Menu* menu Union wg obecnych wartosci w ini (po zmianie wartosci przez plugin w grze, np. tempo medytacji).
		void SyncMenu();
	};

	extern ZConfig zcfg;
}
