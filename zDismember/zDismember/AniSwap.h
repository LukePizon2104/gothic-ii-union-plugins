// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Zamiana animacji walki 1H <-> 2H bohatera (Core/AniSwap, [ZDISMEMBER] WeaponSwap*): bron jednoreczna walczy
	// animacjami dwurecznymi i odwrotnie - vanilla, nakladki talentu, zEBA i styl Wiedzmina. Tablice id animacji per tryb
	// broni bohatera (InitAllAnis / InitFightAnis) dostaja id drugiego typu, a GetWeaponString w funkcjach silnika
	// skladajacych nazwy w biegu (parada, walka, przelaczanie chodu) - przedrostek drugiego typu. Tryb walki, obrazenia,
	// talent i zasieg zostaja z prawdziwej broni; dobycie / schowanie broni natywne. NPC bez zmian.
	// Co klatke przed StylesUpdate: klawisz zamiany, przebudowa animacji i nakladki talentu (odlozone, gdy trwa akcja).
	void AniSwapUpdate();
	// Plugin wylaczony (Enabled=0): powrot do natywnych animacji i nakladek (tylko gdy cos bylo zmienione).
	void AniSwapRestoreIfDisabled();
	// Przed zapisem: natywne nakladki talentu na modelu (wracaja same w kolejnych klatkach).
	void AniSwapBeforeSave();
	// Wczytanie gry / zmiana swiata: zapomina stan modelu (bez dotykania go).
	void AniSwapForget();
	// Tryb broni, ktorego animacje gra bohater (zastosowana zamiana) - do nazw animacji ruchow i stylow pluginu.
	int AniSwapAniWeaponMode(int wm);
	// Talent decydujacy o stylu W3 zestawu animacji (NPC_TAL_1H / _2H): przy zamianie - talent broni w rece.
	int AniSwapSlotTalent(int talent);
	// Rodzaj broni (zdcore::SWAP_TYPE_*), ktorej wybor stylu (cykl U) prowadzi zestaw animacji slotType - Styles.
	int AniSwapSlotDriver(int slotType);
	// Klawisz zamiany z modyfikatorami wcisniety na tym kodzie DIK - klawisz stylu / ruchu ma wtedy nie reagowac.
	bool AniSwapComboHeld(int key);
	// Nakladki talentu, ktore zamiana moze zalozyc (PreloadOverlays).
	std::vector<std::string> AniSwapOverlayNames();
	// zdism swap [info|toggle|0-3|reload]
	std::string AniSwapConsole(const std::string& arg);
	// Bron parowa New Balance bohatera (zdcore::DUAL_NONE / DUAL_RIGHT 1x2 / DUAL_PAIR 2x2) - raz na klatke; para gra
	// zawsze natywnymi animacjami NB (Styles bez stylu 2H, BackSwords natywnie, tu bez zamiany i talentu).
	int HeroDualState();
}
