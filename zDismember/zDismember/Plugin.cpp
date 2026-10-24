// This file added in headers queue
// File: "Sources.h"
#include "resource.h"

namespace GOTHIC_ENGINE {
	static std::string JoinArgs(const std::vector<std::string>& args, size_t from) {
		std::string s;
		for (size_t i = from; i < args.size(); i++)
			s += (s.empty() ? "" : " ") + args[i];
		return s;
	}

	// Profiler probkujacy (_shared\ZSampler.h): wlaczony przy Profiler=1 i ProfSampler=1. Tabela funkcji Gothic2.exe
	// (data\System\zDismember\g2a_funcs.txt, tools\gen_g2a_funcs.py) czytana tylko przed startem watku.
	static void SamplerConfigure() {
		bool on = zcfg.profiler && zcfg.profSampler;
		std::string text;
		if (on && !zsamp::IsStarted()) {
			static const char* names[] = { "\\SYSTEM\\ZDISMEMBER\\G2A_FUNCS.TXT", "SYSTEM\\ZDISMEMBER\\G2A_FUNCS.TXT" };
			ZReadGameFile(names, 2, text);
		}
		zsamp::Configure(on, zcfg.profSampleMs, text);
	}

	static void RegisterCommands() {
		ZConsoleRegister("INFO", "zdism info - stan celu", [](const std::vector<std::string>&) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			if (!npc)
				return "brak celu";
			char buf[256];
			sprintf_s(buf, "%s: maska %03X (sesja %03X) dostepne %03X grupa %u oszolomiony %d zywy %d gibs %d finisherow %d",
				npc->GetInstanceName().ToChar(), PersistGet(npc), CutMaskOf(npc), PartsAvailable(npc), VictimGroup(npc), IsDazed(npc),
				!npc->IsDead(), GibsCount(), (int)FinisherDefs().size());
			return buf;
		});
		ZConsoleRegister("UNCUT", "zdism uncut - przywroc odciete czesci celu (jak cofniecie czasu: sciskanie, odlamki, kikuty; zwloki zostaja zwlokami)",
			[](const std::vector<std::string>&) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			if (!npc)
				return "brak celu";
			uint32_t before = CutMaskOf(npc);
			uint32_t done = RewindRestoreCuts(npc, 0, 0, "konsola");
			char buf[160], what[96];
			sprintf_s(buf, "%s: maska %03X -> 000 (%s)", npc->GetInstanceName().ToChar(), before, zcut::DoneText(done, what, sizeof(what)));
			return buf;
		});
		ZConsoleRegister("CUT", "zdism cut <HEAD|ARM_L|ARM_R|UPPERARM_L|UPPERARM_R|LEG_L|LEG_R|THIGH_L|THIGH_R>", [](const std::vector<std::string>& a) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			zdcore::Part part = zdcore::PartFromName(a.size() > 1 ? a[1] : "HEAD");
			if (!npc || part == zdcore::Part::NoPart)
				return "brak celu albo zla czesc";
			zVEC3 d = npc->GetPositionWorld() - player->GetPositionWorld();
			float l = sqrtf(d.n[0] * d.n[0] + d.n[2] * d.n[2]);
			zVEC3 dir = l > 1.0f ? zVEC3(d.n[0] / l, 0.0f, d.n[2] / l) : zVEC3(1.0f, 0.0f, 0.0f);
			return CutPart(npc, part, GibVelocity(dir, 1)) ? "odcieto" : "czesc niedostepna";
		});
		ZConsoleRegister("SQUASH", "zdism squash <nazwa wezla>", [](const std::vector<std::string>& a) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			std::string name = JoinArgs(a, 1);
			int node = npc ? FindNode(npc->GetModel(), name.c_str()) : -1;
			if (node < 0)
				return "brak celu albo wezla";
			SquashNode(npc->GetModel(), node);
			return "scisniety";
		});
		ZConsoleRegister("FIN", "zdism fin <ID> - wymus finisher na celu", [](const std::vector<std::string>& a) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			const zdcore::FinisherDef* def = a.size() > 1 ? FinisherById(a[1]) : nullptr;
			if (!npc || npc->IsDead())
				return "brak zywego celu";
			if (!def)
				return "nieznany finisher (zdism list)";
			return DirectorTryStart(npc, IsDazed(npc) ? zdcore::POSE_DAZED : zdcore::POSE_STANDING, def) ? "start" : "nie wystartowal (log)";
		});
		ZConsoleRegister("LIST", "zdism list - finishery", [](const std::vector<std::string>&) -> zSTRING {
			std::string s = "finishery:";
			for (auto& d : FinisherDefs())
				s += " " + d.id;
			return s.c_str();
		});
		ZConsoleRegister("MOVES", "zdism moves - uniki i przewroty", [](const std::vector<std::string>&) -> zSTRING {
			return MovesList().c_str();
		});
		ZConsoleRegister("MOVE", "zdism move <ID> - wymus unik/przewrot (np. DODGE_L, ROLL_B)", [](const std::vector<std::string>& a) -> zSTRING {
			return MovesForce(a.size() > 1 ? a[1] : "");
		});
		ZConsoleRegister("TRAV", "zdism trav [info|go|list|tor [m]|tor usun] - przeskok / wspiecie: wykrywanie przed bohaterem (go = wykonaj); tor = przeszkody testowe przed bohaterem", [](const std::vector<std::string>& a) -> zSTRING {
			return TraversalConsole(a.size() > 1 ? JoinArgs(a, 1) : "INFO").c_str();
		});
		ZConsoleRegister("STYLE", "zdism style [next|prev|auto|list] - styl walki dla broni w reku (debug)", [](const std::vector<std::string>& a) -> zSTRING {
			std::string arg = a.size() > 1 ? zdcore::Upper(a[1]) : "NEXT";
			if (arg == "LIST")
				return StyleList().c_str();
			return StyleCycle(arg == "PREV" ? -1 : arg == "AUTO" ? 0 : +1).c_str();
		});
		ZConsoleRegister("LOCO", "zdism loco [next|prev|auto|list|info|watch|0-4] - zestaw ruchu bohatera (test)", [](const std::vector<std::string>& a) -> zSTRING {
			std::string arg = a.size() > 1 ? zdcore::Upper(a[1]) : "NEXT";
			if (arg == "LIST")
				return LocoList().c_str();
			if (arg == "INFO")
				return LocoInfo().c_str();
			if (arg == "WATCH")
				return LocoWatchToggle().c_str();
			if (arg.size() == 1 && arg[0] >= '0' && arg[0] <= '9')
				return LocoSetMode(arg[0] - '0').c_str();
			return LocoCycle(arg == "PREV" ? -1 : arg == "AUTO" ? 0 : +1).c_str();
		});
		ZConsoleRegister("SWAP", "zdism swap [info|toggle|0-3|reload|watch] - zamiana animacji 1H <-> 2H bohatera: stan, klawisz, wariant "
			"(0 wyl., 1 1H jak 2H, 2 2H jak 1H, 3 obie), przebudowa, watch = starty animacji bohatera do logu (4 s)", [](const std::vector<std::string>& a) -> zSTRING {
			return AniSwapConsole(a.size() > 1 ? a[1] : "INFO").c_str();
		});
		ZConsoleRegister("RAGDOLL", "zdism ragdoll - przelacz ragdolle zwlok", [](const std::vector<std::string>&) -> zSTRING {
			zcfg.ragdoll = !zcfg.ragdoll;
			if (!zcfg.ragdoll)
				CorpsesClearAll();
			return zcfg.ragdoll ? "ragdolle wlaczone" : "ragdolle wylaczone";
		});
		ZConsoleRegister("DAZE", "zdism daze - powal cel jak gracz", [](const std::vector<std::string>&) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			if (!npc || npc->IsDead())
				return "brak zywego celu";
			npc->DropUnconscious(1.0f, player);
			return IsDazed(npc) ? "oszolomiony" : "powalony (bez oszolomienia)";
		});
		ZConsoleRegister("DEBUG", "zdism debug - przelacz szczegolowy log (do restartu)", [](const std::vector<std::string>&) -> zSTRING {
			zcfg.debug = !zcfg.debug;
			ZLog("Debug %s", zcfg.debug ? "wlaczony" : "wylaczony");
			return zcfg.debug ? "debug wlaczony" : "debug wylaczony";
		});
		ZConsoleRegister("PARRY", "zdism parry [hit] - blok bez walki: cel uderza, bohater blokuje (hit: odwrotnie)", [](const std::vector<std::string>& a) -> zSTRING {
			oCNpc* npc = ZConsoleTarget();
			if (!npc || npc == player || npc->IsDead())
				return "brak zywego celu";
			bool heroHits = a.size() > 1 && a[1] == "HIT";
			return ParryTest(heroHits ? player : npc, heroHits ? npc : player).c_str();
		});
		ZConsoleRegister("PROF", "zdism prof [reset|on|off|samples [reset]] - profiler: podsumowanie od resetu (tabela w zDismember.log); "
			"samples = probki watku gry: przycinki vs klatki normalne",
			[](const std::vector<std::string>& a) -> zSTRING {
			std::string arg = a.size() > 1 ? a[1] : "";
			if (arg == "SAMPLES") {
				if (a.size() > 2 && a[2] == "RESET") {
					zsamp::Reset();
					ZLog("Prof: reset statystyk probek");
					return "probki: statystyki od nowa";
				}
				return zsamp::Summary().c_str();
			}
			if (arg == "RESET") {
				zprof::Reset();
				zsamp::Reset();
				ZLog("Prof: reset statystyk");
				return "profiler: statystyki od nowa";
			}
			if (arg == "OFF" || arg == "ON") {
				zprof::SetOn(arg == "ON");
				ZLog("Prof: %s z konsoli", zprof::IsOn() ? "wlaczony" : "wylaczony");
				return zprof::IsOn() ? "profiler wlaczony" : "profiler wylaczony (do restartu albo zdism prof on)";
			}
			if (!zprof::IsOn())
				return "profiler wylaczony (zdism prof on / Profiler=1)";
			return zprof::Summary().c_str();
		});
		ZConsoleRegister("SIGN", "zdism sign <aard|igni|axii|yrden|quen> [alt] | zdism sign info - Znak Wiedzmina bez runy (test, bez many; alt = tryb alternatywny) / stan znakow",
			[](const std::vector<std::string>& a) -> zSTRING {
			return SignsConsole(a.size() > 1 ? JoinArgs(a, 1) : "INFO").c_str();
		});
		ZConsoleRegister("FIRES", "zdism fires [m] - ognie w promieniu (domyslnie 10 m): klasa, wizual, stan, grupa, czy Aard / Igni zadziala (log)",
			[](const std::vector<std::string>& a) -> zSTRING {
			return SignsFiresConsole(a.size() > 1 ? a[1] : "").c_str();
		});
		ZConsoleRegister("LOOT", "zdism loot - podnoszenie z bronia: przedmiot przed bohaterem, wrog w poblizu, powod odmowy",
			[](const std::vector<std::string>& a) -> zSTRING {
			return LootConsole(a.size() > 1 ? a[1] : "INFO").c_str();
		});
		ZConsoleRegister("HOOKS", "zdism hooks [reset|chain] - wywolania naszych hakow od startu (0 = inny hak w lancuchu nie wola oryginalu); chain = wejscia funkcji do logu",
			[](const std::vector<std::string>& a) -> zSTRING {
			return HookDiagConsole(a.size() > 1 ? a[1] : "").c_str();
		});
		ZConsoleRegister("ATK", "zdism atk - tempo ciosow W3 (W3AtkSpeed*): cios i kolejny cios W3 / natywne, fps; ciecia boczne 2H (W3Side2H)",
			[](const std::vector<std::string>& a) -> zSTRING {
			return AtkTempoConsole(a.size() > 1 ? a[1] : "").c_str();
		});
		ZConsoleRegister("MEDITATE", "zdism meditate [info|stop] - medytacja W3: start po zamknieciu konsoli / stan (godzina, tempo, "
			"nakladka, dar aktywny) / wstan", [](const std::vector<std::string>& a) -> zSTRING {
			return MeditateConsole(a.size() > 1 ? a[1] : "").c_str();
		});
		ZConsoleRegister("PATHS", "zdism paths [info|skills|open] - gildia i sciezki New Balance: stan znacznikow, wylaczen i ochrony "
			"rozmow / wyuczone umiejetnosci / okno po zamknieciu konsoli", [](const std::vector<std::string>& a) -> zSTRING {
			return PathMenuConsole(a.size() > 1 ? a[1] : "").c_str();
		});
		ZConsoleRegister("BACK", "zdism back [info|second [INSTANCJA|clear|auto]|swap|adjust a|b x y z [rx ry rz]] - miecze na plecach: "
			"stan, drugi miecz, zamiana (po zamknieciu konsoli), przesuniecie miecza na plecach (cm, st.)",
			[](const std::vector<std::string>& a) -> zSTRING {
			return BackSwordsConsole(a.size() > 1 ? JoinArgs(a, 1) : "INFO").c_str();
		});
		ZConsoleRegister("XBOW", "zdism xbow [info|fire|shoot|aim|anis [info]|reset] - szybki strzal z kuszy: stan / belt bez animacji / "
			"stukniecie / celowanie 1,5 s (po zamknieciu konsoli) / test pozyczania wpisu animacji / reset",
			[](const std::vector<std::string>& a) -> zSTRING {
			return CrossbowConsole(a.size() > 1 ? JoinArgs(a, 1) : "INFO").c_str();
		});
		ZConsoleRegister("WHEEL", "zdism wheel [info|open|close] - menu kolowe (Tab trzymany): stan, eksporty, okna / otworz po "
			"zamknieciu konsoli / zamknij", [](const std::vector<std::string>& a) -> zSTRING {
			return WheelConsole(a.size() > 1 ? a[1] : "INFO").c_str();
		});
		ZConsoleRegister("RELOAD", "zdism reload - ini i finishery", [](const std::vector<std::string>&) -> zSTRING {
			MeditateFinishNow("zdism reload");
			PathMenuFinishNow("zdism reload");
			WheelFinishNow("zdism reload");
			DirectorFinishNow();
			MovesFinishNow();
			ReachReset();
			TraversalFinishNow();
			LootFinishNow();
			StylesRemove();   // StylesUpdate zalozy je znow, jesli nadal chciane
			zcfg.Load();
			zprof::Configure(zcfg.profiler, zcfg.profSpikeMs, zcfg.profSummarySeconds);
			SamplerConfigure();
			if (!zcfg.enabled || !zcfg.ragdoll)
				CorpsesClearAll();
			SignsClearAll();
			SignsPatchSlots();   // SignSpellIds / SignAniLetters mogly sie zmienic
			FinishersLoad();
			MovesLoad();
			TraversalLoad();
			StylesLoad();
			BackSwordsLoad();
			CrossbowReset("zdism reload");
			CrossbowLoad();
			PreloadOverlays("zdism reload");   // nowe nazwy z ini; PreloadOverlays=0 - zwalnia trzymane
			PathMenuLoadEnd();   // symbole parsera i dostepnosc menu sciezek od nowa
			return "przeladowano";
		});
	}

	void Game_Entry() {
	}

	void Game_Init() {
		ZLog("zDismember start");
		zcfg.Load();
		zprof::Init("zDismember", ZProfLine, zcfg.profiler, zcfg.profSpikeMs, zcfg.profSummarySeconds);
		SamplerConfigure();   // po zprof::Init, w watku gry (kalibracja kosztu probki - linia w logu)
		FinishersLoad();
		MovesLoad();
		TraversalLoad();
		StylesLoad();
		BackSwordsLoad();
		CrossbowLoad();
		DirectorInit();
		SignsInit();         // po DirectorInit - dopina sie do zdOnBeforeHit
		SignsPatchSlots();   // sloty czarow znakow w tablicach parsera (skrypty juz wczytane)
		RegisterCommands();
		RewindLinkInit();    // kanal masek odciec dla zTimeRewind (pamiec nazwana procesu) - po cofnieciu czasu czesci wracaja
		HookGuardInit();     // cele skokow na wejsciach haczonych funkcji, zanim skrypty moda zaloza haki Ikarusa (Kroniki)
		HookDiagInit();      // Debug=1: kto jest na wejsciu haczonych funkcji (New Balance: UNION_ABI.DLL) + zegar raportow
		ZLogFlush();
	}

	void Game_Exit() {
		WheelFinishNow("wyjscie z gry");   // factorMotion jak przed kolem
		zsamp::Stop();   // watek probkujacy konczony tu (w DllMain tylko flaga - loader lock)
		ZLogFlush();
	}

	void Game_PreLoop() {
	}

	// Kroki pluginu w klatce - kazdy w sekcji profilera (zdism prof, przycinki w logu).
	static void LoopUpdate() {
		if (!zcfg.enabled || !ztimer) {
			AniSwapRestoreIfDisabled();   // zamiana animacji wylaczona razem z pluginem
			AtkTempoRestore("plugin wylaczony");   // tempo ciosow W3 i ciecia boczne 2H z pliku
			BackSwordsRestore("plugin wylaczony");   // dobycie, tempo i wezly broni jak w grze
			CrossbowReset("plugin wylaczony");       // szybki strzal: kusza na plecy, wpisy animacji z pliku
			MeditateFinishNow("plugin wylaczony");   // bez medytacji - nic nie robi
			PathMenuFinishNow("plugin wylaczony");   // okno sciezek, ochrona rozmow odtworzona
			WheelFinishNow("plugin wylaczony");      // menu kolowe zamkniete, tempo czasu przywrocone
			return;
		}
		float dt = ztimer->frameTimeFloat / 1000.0f;
		if (dt > 0.1f)
			dt = 0.1f;
		{
			ZPROF_SCOPE("Moves");
			MovesUpdate(dt);
		}
		{
			ZPROF_SCOPE("Traversal");
			TraversalUpdate(dt);
		}
		{
			ZPROF_SCOPE("Traversal.Debug");   // debug wizualny przeskokow (zdism trav show) - poza nim nic
			TraversalDebugUpdate(dt);
		}
		{
			ZPROF_SCOPE("Reach");
			ReachUpdate(dt);
		}
		{
			ZPROF_SCOPE("AniSwap");   // zamiana animacji 1H <-> 2H: klawisz, przebudowa, nakladki talentu (przed stylami W3)
			AniSwapUpdate();
		}
		{
			ZPROF_SCOPE("Styles");
			StylesUpdate();
		}
		AtkTempoUpdate();   // tempo ciosow W3 i ciecia boczne 2H (po planie nakladek - styl W3 juz na modelu)
		{
			ZPROF_SCOPE("BackSwords");   // miecze na plecach: klawisz, zamiana, wariant dobycia, wezly i wizual drugiego miecza
			BackSwordsUpdate(dt);
		}
		CrossbowUpdate(dt);   // szybki strzal z kuszy (F w zQuickSign): animacje, kusza w dloni, belt, przerwania
		{
			ZPROF_SCOPE("Director");
			DirectorUpdate(dt);
		}
		HeroForwardDiagUpdate(dt);   // diagnostyka: W bez ruchu do przodu (wpis w logu)
		{
			ZPROF_SCOPE("Daze");
			DazeUpdate(dt);
		}
		{
			ZPROF_SCOPE("Gibs");
			GibsUpdate(dt);
		}
		{
			ZPROF_SCOPE("Corpses");
			CorpsesUpdate(dt);
		}
		{
			ZPROF_SCOPE("Blood");
			BloodUpdate(dt);
		}
		{
			ZPROF_SCOPE("Parry");
			ParryUpdate(dt);
		}
		{
			ZPROF_SCOPE("Signs");
			SignsUpdate(dt);
		}
		{
			ZPROF_SCOPE("Loot");
			LootUpdate(dt);
		}
		{
			ZPROF_SCOPE("Meditate");   // medytacja W3: klawisz, okno, czekanie (tempo czasu, zegar, regeneracja)
			MeditateUpdate();
		}
		{
			ZPROF_SCOPE("PathMenu");   // New Balance: gildia i sciezki - pilnowanie wylaczen, ochrona rozmow, okno (PathMenuKey)
			PathMenuUpdate();
		}
		{
			ZPROF_SCOPE("Wheel");   // menu kolowe: otwarcie / zatwierdzenie (Tab), przerwania, linia bazowa okien
			WheelUpdate();
		}
		if (zdFrame % 300 == 0) {
			ZPROF_SCOPE("Collect");
			SquashCollect();
			CutMasksCollect();
			CombatCollect();
		}
	}

	void Game_Loop() {
		zprof::FrameMark();   // klatka = odstep miedzy kolejnymi Game_Loop; przycinka -> linia w logu
		zdFrame++;
		HookDiagUpdate();   // Debug=1: liczniki wywolan hakow po 15 s, 1, 3, 10 min
		HookGuardUpdate();  // co 1 s: stub Ikarusa na wejsciu naszego haka -> poprawka skopiowanego skoku
		{
			ZPROF_SCOPE("Loop");   // czas wlasny = Game_Loop poza sekcjami krokow
			LoopUpdate();
		}
		ZLogFlush();   // linie z tej klatki na dysk - raz na klatke
	}

	void Game_PostLoop() {
		if (zcfg.enabled) {
			MeditateDraw();   // okno medytacji / pasek czekania - po swiecie (jak menu J zTransmog)
			PathMenuDraw();   // okno gildii i sciezek bez klekania (New Balance)
			WheelDraw();      // menu kolowe nad swiatem
		}
	}

	void Game_MenuLoop() {
		if (zcfg.enabled)
			WheelMenuLoop();   // strona "Kolo wyboru": biezacy klawisz kola / prosba o klawisz
		ZLogFlush();   // linie z menu (opcje, wczytanie) bez czekania na Game_Loop
	}

	// Information about current saving or loading world
	TSaveLoadGameInfo& SaveLoadGameInfo = UnionCore::SaveLoadGameInfo;

	// Zapis bez naszych tymczasowych rzeczy: finisher konczy sie od razu, oszolomieni leza, czesci poza swiatem.
	void Game_SaveBegin() {
		zprof::Break();   // zapis to nie klatka gry
		MeditateFinishNow("zapis");   // zapis bez nakladki medytacji, z tempem czasu i blokada sterowania gry
		PathMenuFinishNow("zapis");   // okno zamkniete; ochrona rozmow: wylaczone umiejetnosci w zapisie = 0 (stan menu w zmiennych)
		WheelFinishNow("zapis");      // kolo zamkniete, tempo czasu bez spowolnienia w zapisie
		MovesFinishNow();
		ReachReset();
		TraversalFinishNow();
		StylesRemove();   // bez naszych nakladek w zapisie (wracaja same po kilku klatkach)
		AniSwapBeforeSave();   // nakladki talentu natywne (zamiana animacji zaklada je znow po zapisie)
		DirectorFinishNow();
		DazeBeforeSave();
		BloodClearAll();
		ParryClearAll();
		SignsBeforeSave();   // Yrden: tempo i ochrona NPC oryginalne w zapisie; efekty tarczy / strefy zdjete
		BackSwordsBeforeSave();   // drugi miecz i miejsce broni w zmiennych skryptu (ZDF_BACK_*)
		CrossbowBeforeSave();     // strzal przerwany (kusza na plecy, wpisy z pliku), wybor slotu w ZDF_XBOW_SLOT
		GibsBeforeSave();
		ZLogFlush();
	}

	void Game_SaveEnd() {
		GibsAfterSave();
		SignsAfterSave();
		zprof::Break();
		ZLogFlush();
	}

	void LoadBegin() {
		zprof::Break();   // wczytanie to nie klatka gry
		ZLogFlush();      // linie sprzed wczytania na dysk (gdyby wczytanie sie wysypalo)
		MeditateFinishNow("wczytanie / zmiana swiata");
		PathMenuLoadBegin();
		WheelLoadBegin();   // kolo zamkniete, tempo przywrocone, linia bazowa okien od nowa
		MovesFinishNow();
		ReachReset();
		TraversalFinishNow();
		TraversalCourseClear();   // tor przeszkod (zdism trav tor) - voby z obecnego swiata, zanim go zwolni
		DirectorFinishNow();
		DazeClearAll();
		GibsClearAll();
		CorpsesClearAll();
		StylesForget();
		AtkTempoForget();
		AniSwapForget();
		BloodClearAll();
		ParryClearAll();
		SignsClearAll();
		SquashClearAll();
		CutMasksClear();
		CombatClear();
		LootFinishNow();
		BackSwordsLoadBegin();
		CrossbowLoadBegin();
		ZLogFlush();
	}

	void LoadEnd() {
		// Nakladki bohatera zawczasu: tu bohater ma juz model (baza HUMANS.MDS do PrepareAsModelProtoOverlay), a czas idzie
		// na ekran wczytywania (pierwszy raz w sesji ok. sekundy, potem tylko nowe nazwy). Nie w Game_Init: przed swiatem
		// nie ma modelu bohatera, wiec nie ma pewnej bazy (nakladka pod inna baza = odmowa zalozenia, 0x5788B5).
		HookGuardCheck("wczytanie swiata");   // przed pierwsza klatka gry: skrypty moda juz zalozyly haki Ikarusa
		PreloadOverlays("wczytanie swiata");
		BackSwordsLoadEnd();   // drugi miecz z zapisu, warianty B / BELT zawczasu
		CrossbowLoadEnd();     // slot kuszy pod F z ZDF_XBOW_SLOT
		PersistReapplyWorld();
		PathMenuLoadEnd();   // stan wylaczen siedzi w zmiennych zapisu - log stanu, pamiec pilnowania od nowa
		zprof::Break();
		ZLogFlush();
	}

	void Game_LoadBegin_NewGame() {
		LoadBegin();
	}

	void Game_LoadEnd_NewGame() {
		LoadEnd();
	}

	void Game_LoadBegin_SaveGame() {
		LoadBegin();
	}

	void Game_LoadEnd_SaveGame() {
		LoadEnd();
	}

	void Game_LoadBegin_ChangeLevel() {
		LoadBegin();
	}

	void Game_LoadEnd_ChangeLevel() {
		LoadEnd();
	}

	void Game_LoadBegin_Trigger() {
	}

	void Game_LoadEnd_Trigger() {
	}

	void Game_Pause() {
		zprof::Break();   // menu - czas w menu to nie klatka gry
		zsamp::SetPaused(true);   // bez probek w menu
		WheelFinishNow("pauza / menu gry");
		ZLogFlush();
	}

	void Game_Unpause() {
		zprof::Break();
		zsamp::SetPaused(false);
	}

	void Game_DefineExternals() {
	}

	// Wyjscie z menu opcji (takze z menu zDismember w "Union & Pluginy").
	void Game_ApplyOptions() {
		zcfg.Load(true);
		zprof::Configure(zcfg.profiler, zcfg.profSpikeMs, zcfg.profSummarySeconds);
		SamplerConfigure();
		if (!zcfg.enabled || !zcfg.ragdoll)
			CorpsesClearAll();
		if (!zcfg.enabled || !zcfg.parryRecoil)
			ParryClearAll();
		if (!zcfg.enabled || !zcfg.signs)
			SignsClearAll();   // strefa Yrden / tarcza / oszolomienie cofniete od razu (petla pluginu juz ich nie prowadzi)
		if (!zcfg.enabled || !zcfg.crossbow)
			CrossbowReset("wylaczone w menu");
		if (!zcfg.enabled || !zcfg.wheel)
			WheelFinishNow("wylaczone w menu");
		MovesReloadKeys();
	}

	/*
	Functions call order on Game initialization:
	  - Game_Entry           * Gothic entry point
	  - Game_DefineExternals * Define external script functions
	  - Game_Init            * After DAT files init

	Functions call order on Change level:
	  - Game_LoadBegin_Trigger     * Entry in trigger
	  - Game_LoadEnd_Trigger       *
	  - Game_Loop                  * Frame call window
	  - Game_LoadBegin_ChangeLevel * Load begin
	  - Game_SaveBegin             * Save previous level information
	  - Game_SaveEnd               *
	  - Game_LoadEnd_ChangeLevel   *

	Functions call order on Save game:
	  - Game_Pause     * Open menu
	  - Game_Unpause   * Click on save
	  - Game_Loop      * Frame call window
	  - Game_SaveBegin * Save begin
	  - Game_SaveEnd   *

	Functions call order on Load game:
	  - Game_Pause              * Open menu
	  - Game_Unpause            * Click on load
	  - Game_LoadBegin_SaveGame * Load begin
	  - Game_LoadEnd_SaveGame   *
	*/

#define AppDefault True
	CApplication* lpApplication = !CHECK_THIS_ENGINE ? Null : CApplication::CreateRefApplication(
		Enabled( AppDefault ) Game_Entry,
		Enabled( AppDefault ) Game_Init,
		Enabled( AppDefault ) Game_Exit,
		Enabled( AppDefault ) Game_PreLoop,
		Enabled( AppDefault ) Game_Loop,
		Enabled( AppDefault ) Game_PostLoop,
		Enabled( AppDefault ) Game_MenuLoop,
		Enabled( AppDefault ) Game_SaveBegin,
		Enabled( AppDefault ) Game_SaveEnd,
		Enabled( AppDefault ) Game_LoadBegin_NewGame,
		Enabled( AppDefault ) Game_LoadEnd_NewGame,
		Enabled( AppDefault ) Game_LoadBegin_SaveGame,
		Enabled( AppDefault ) Game_LoadEnd_SaveGame,
		Enabled( AppDefault ) Game_LoadBegin_ChangeLevel,
		Enabled( AppDefault ) Game_LoadEnd_ChangeLevel,
		Enabled( AppDefault ) Game_LoadBegin_Trigger,
		Enabled( AppDefault ) Game_LoadEnd_Trigger,
		Enabled( AppDefault ) Game_Pause,
		Enabled( AppDefault ) Game_Unpause,
		Enabled( AppDefault ) Game_DefineExternals,
		Enabled( AppDefault ) Game_ApplyOptions
	);
}
