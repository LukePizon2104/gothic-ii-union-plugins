// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Diagnostyka hakow (port New Balance: UNION_ABI.DLL haczy te same funkcje silnika, kolejnosc w lancuchu Union
	// i to, czy NB wola oryginal, rozstrzyga dopiero gra). Licznik wywolan kazdego naszego haka (zawsze, koszt = jedno
	// ++), przy Debug=1 w zDismember.log: przy starcie modul na wejsciu kazdej funkcji (kto jest pierwszy w lancuchu),
	// potem liczniki po 15 s, 1, 3 i 10 min gry. Konsola: zdism hooks.
	enum ZHookId {
		ZH_ONDAMAGE, ZH_ONDAMAGE_HIT, ZH_DODIE, ZH_DROPUNCONSCIOUS, ZH_EV_ATTACKFINISH, ZH_ADVANCEANIS, ZH_DOAISTATE,
		ZH_INITALLANIS, ZH_INITFIGHTANIS, ZH_HITCOMBO, ZH_STARTPARADEFX, ZH_EV_PARADE, ZH_FIGHTMELEE, ZH_APPLYOVERLAY,
		ZH_REMOVEOVERLAY, ZH_EV_ATTACKRUN, ZH_EV_ATTACKFORWARD, ZH_EV_ATTACKLEFT, ZH_EV_ATTACKRIGHT, ZH_EV_DAMAGEPERFRAME,
		ZH_SPELL_INVESTEDMANA, ZH_SPELL_RELEASE, ZH_SPELL_CAST, ZH_CHECKPHYSICS, ZH_ENDMOVEMENT, ZH_STAND,
		ZH_ISINFIGHTRANGE, ZH_INPUT_GETSTATE, ZH_INPUT_GETTOGGLED, ZH_CONSOLE_EVALUATE, ZH_INPUT_PROCESS,
		ZH_CAM_CHECKKEYS, ZH_CAM_AINORMAL, ZH_INFO_SETNPC, ZH_WORLD_ADVANCECLOCK,
		ZH_COUNT
	};
	extern unsigned zdHookCalls[ZH_COUNT];
	inline void ZHookHit(ZHookId id) {
		zdHookCalls[id]++;
	}

	void HookGuardInit();    // Game_Init (przed skryptami moda): cele skokow na wejsciach haczonych funkcji
	void HookGuardCheck(const char* when);   // wczytanie swiata: naprawa skopiowanych skokow w stubach Ikarusa (Kroniki)
	void HookGuardUpdate();  // Game_Loop: HookGuardCheck co 1 s
	void HookDiagInit();     // Game_Init: wejscia funkcji (Debug=1) i start zegara raportow
	void HookDiagUpdate();   // Game_Loop: raporty licznikow (Debug=1)
	std::string HookDiagConsole(const std::string& arg);   // zdism hooks [reset]
}
