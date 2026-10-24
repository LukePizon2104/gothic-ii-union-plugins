// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Style walki bohatera dla broni 1H i 2H osobno. AUTO = to, co ustawia gra / zExtendedBattleAnimations, a styl
	// Wiedzmina (HUMANS_ZDF_W3_1H / _2H) sam od W3StyleMinTalent % talentu. Debugowy cykl (StyleKey, zdism style)
	// wymusza dowolny dostepny styl: natywny Gothica (HUMANS_ZDF_NAT_*), zEBA albo W3 - nakladka ponad wszystkimi.
	// Ruch W3: trucht/chod (W3Locomotion) i sprint (W3Sprint) - sprint W3 jest zalozony na stale na spodzie listy
	// nakladek, a w sprincie zEBA tylko przekladany nad jej nakladke (bez zdejmowania, ktore zatrzymuje animacje).
	// Bohater na wierzchowcu (New Balance: nakladka HUMANS_MOUNT, zdcore::IsExclusiveForeignOverlay) - plugin zdejmuje
	// swoje nakladki ruchu / stylow i nie startuje unikow ani przeskokow. Steam: nigdy.
	bool HeroMounted();
	void StylesLoad();
	void StylesUpdate();
	// Nakladki stylow i ruchu W3, ktore plugin moze zalozyc bohaterowi (dostepne wg StylesLoad) - do PreloadOverlays.
	std::vector<std::string> StylesOverlayNames();
	// Zdejmuje nakladki zalozone przez plugin (przed zapisem gry); wracaja same po kilku klatkach.
	// Petle ruchu (S_RUNL...) startuja od razu z wersji z wierzchu.
	void StylesRemove();
	// Wczytanie gry / nowy swiat: zapomina zapis zalozonych nakladek (bez dotykania modelu).
	void StylesForget();
	// step +1 / -1 dla broni w reku (1H/2H); 0 = AUTO. Zwraca opis do konsoli/ekranu.
	std::string StyleCycle(int step);
	std::string StyleList();

	// Zestawy ruchu bohatera do testow (LocoKey, zdism loco): AUTO (ini) -> Gothic/zEBA -> W3 trucht -> W3 trucht+sprint
	// -> podglad petli sprintu W3. Zmiana zapisuje LocoMode w ini i pokazuje na ekranie, skad gra petla S_RUNL / S_xHRUNL
	// i jaki sprint gry jest aktywny. Sprint W3 w sprincie gry gra w tempie petli sprintu gry (fpsRate x skala).
	std::string LocoCycle(int step);       // +1 / -1, 0 = AUTO
	std::string LocoSetMode(int mode);     // 0..4 (zdcore::LocoMode)
	std::string LocoList();
	std::string LocoInfo();                // stan ruchu do konsoli i na ekran
	std::string LocoWatchToggle();         // stan ruchu na ekranie co 0,8 s
}
