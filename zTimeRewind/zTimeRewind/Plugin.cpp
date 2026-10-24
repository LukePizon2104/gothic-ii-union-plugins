// This file added in headers queue
// File: "Sources.h"
#include "resource.h"

namespace GOTHIC_ENGINE {
	void Game_Entry() {
	}

	static void RegisterCommands() {
		TrConsoleRegister("CFG", "tr cfg - przeladuj [ZTIMEREWIND] z Gothic.ini", [](const std::vector<std::string>&) {
			trcfg.Load();
			zprof::Configure(trcfg.profiler, trcfg.profSpikeMs, trcfg.profSummarySeconds);
			return zSTRING("zTimeRewind: ustawienia przeladowane");
		});
		TrConsoleRegister("PROF", "tr prof [reset|on|off] - profiler: podsumowanie od resetu (tabela w zTimeRewind.log)",
			[](const std::vector<std::string>& a) -> zSTRING {
			std::string arg = a.size() > 1 ? a[1] : "";
			if (arg == "RESET") {
				zprof::Reset();
				ZLog("Prof: reset statystyk");
				return "zTimeRewind: profiler - statystyki od nowa";
			}
			if (arg == "OFF" || arg == "ON") {
				zprof::SetOn(arg == "ON");
				ZLog("Prof: %s z konsoli", zprof::IsOn() ? "wlaczony" : "wylaczony");
				return zprof::IsOn() ? "zTimeRewind: profiler wlaczony" : "zTimeRewind: profiler wylaczony (do restartu albo tr prof on)";
			}
			if (!zprof::IsOn())
				return "zTimeRewind: profiler wylaczony (tr prof on / Profiler=1)";
			return zprof::Summary().c_str();
		});
		SpikeRegister();
		StateRegisterCommands();
		RelationsRegisterCommands();
		ConsumeRegisterCommands();
		TrConsoleRegister("STATS", "tr stats - stan bufora cofania", [](const std::vector<std::string>&) {
			return zSTRING(RecorderStats().c_str());
		});
		TrConsoleRegister("REWIND", "tr rewind <s> - cofnij swiat o s sekund (bez many)", [](const std::vector<std::string>& a) {
			float s = a.size() > 1 ? (float)atof(a[1].c_str()) : 10.0f;
			if (RewindActive())
				return zSTRING("zTimeRewind: juz przewijam");
			if (RecorderSeconds() < 1.0f)
				return zSTRING("zTimeRewind: bufor pusty");
			RewindConsole(s);
			return zSTRING("zTimeRewind: cofam o ") + zSTRING(s) + zSTRING(" s");
		});
	}

	void Game_Init() {
		ZLog("zTimeRewind start");
		trcfg.Load();
		zprof::Init("zTimeRewind", ZProfLine, trcfg.profiler, trcfg.profSpikeMs, trcfg.profSummarySeconds);
		RegisterCommands();
		SpellPatchSlot();
		FxInit();
		TrCutsInit();   // kanal masek odciec zDismember (pamiec nazwana procesu)
		ZLogFlush();
	}

	void Game_Exit() {
		ZLogFlush();
	}

	void Game_PreLoop() {
	}

	// Kroki pluginu w klatce - kazdy w sekcji profilera (tr prof, przycinki w logu).
	static void LoopUpdate() {
		if (!trcfg.enabled || !ztimer)
			return;
		float dt = ztimer->frameTimeFloat / 1000.0f;
		if (dt > 0.1f)
			dt = 0.1f;
		{
			ZPROF_SCOPE("Spike");
			SpikeUpdate(dt);
		}
		{
			ZPROF_SCOPE("Rewind");
			RewindUpdate(dt);
		}
		{
			ZPROF_SCOPE("Spell");
			SpellUpdate();
		}
		if (!RewindActive() && !trFrozen) {
			ZPROF_SCOPE("Recorder");
			RecorderUpdate(dt);
		}
	}

	void Game_Loop() {
		zprof::FrameMark();   // klatka = odstep miedzy kolejnymi Game_Loop; przycinka -> linia w logu
		{
			ZPROF_SCOPE("Loop");   // czas wlasny = Game_Loop poza sekcjami krokow
			LoopUpdate();
		}
		ZLogFlush();   // linie z tej klatki na dysk - raz na klatke
	}

	void Game_PostLoop() {
		if (trcfg.enabled) {
			ZPROF_SCOPE("Fx.Render");
			FxRender();
		}
	}

	void Game_MenuLoop() {
		ZLogFlush();   // linie z menu bez czekania na Game_Loop
	}

	// Information about current saving or loading world
	TSaveLoadGameInfo& SaveLoadGameInfo = UnionCore::SaveLoadGameInfo;

	void Game_SaveBegin() {
		zprof::Break();   // zapis to nie klatka gry
		RewindFinishNow();
		ZLogFlush();
	}

	void Game_SaveEnd() {
		zprof::Break();
		ZLogFlush();
	}

	void LoadBegin() {
		zprof::Break();   // wczytanie to nie klatka gry
		RewindAbort();
		trFrozen = false;
		RecorderReset();
		ZLogFlush();
	}

	void LoadEnd() {
		TrCutsLogState();   // czy zDismember wystawil maski odciec (linia przy zmianie stanu)
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
		RewindFinishNow();
		ZLogFlush();
	}

	void Game_Unpause() {
		zprof::Break();
	}

	void Game_DefineExternals() {
	}

	void Game_ApplyOptions() {
		trcfg.Load(true);   // po wyjsciu z menu opcji - pozycje z menu Union wygrywaja
		zprof::Configure(trcfg.profiler, trcfg.profSpikeMs, trcfg.profSummarySeconds);
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
		Enabled(AppDefault) Game_Entry,
		Enabled(AppDefault) Game_Init,
		Enabled(AppDefault) Game_Exit,
		Enabled(AppDefault) Game_PreLoop,
		Enabled(AppDefault) Game_Loop,
		Enabled(AppDefault) Game_PostLoop,
		Enabled(AppDefault) Game_MenuLoop,
		Enabled(AppDefault) Game_SaveBegin,
		Enabled(AppDefault) Game_SaveEnd,
		Enabled(AppDefault) Game_LoadBegin_NewGame,
		Enabled(AppDefault) Game_LoadEnd_NewGame,
		Enabled(AppDefault) Game_LoadBegin_SaveGame,
		Enabled(AppDefault) Game_LoadEnd_SaveGame,
		Enabled(AppDefault) Game_LoadBegin_ChangeLevel,
		Enabled(AppDefault) Game_LoadEnd_ChangeLevel,
		Enabled(AppDefault) Game_LoadBegin_Trigger,
		Enabled(AppDefault) Game_LoadEnd_Trigger,
		Enabled(AppDefault) Game_Pause,
		Enabled(AppDefault) Game_Unpause,
		Enabled(AppDefault) Game_DefineExternals,
		Enabled(AppDefault) Game_ApplyOptions
	);
}