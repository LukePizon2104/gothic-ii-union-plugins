// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Wolane przed oryginalnym DropUnconscious: czlowiek powalony przez gracza dostaje nakladke klęczenia
	// (HUMANS_ZDF_DAZE podmienia T_STAND_2_WOUNDED[B]/S_WOUNDED[B]/T_WOUNDED[B]_2_DEAD[B]).
	void DazeBeforeDrop(oCNpc* victim, oCNpc* attacker);
	bool IsDazed(oCNpc* npc);
	// Po okresie karencji (ciosy z tej samej kombinacji nie robia egzekucji).
	bool DazeCanExecute(oCNpc* npc);
	// Egzekucja przejela ofiare: wpis znika, nakladka zostaje (smierc z kleczek gra z nakladki).
	void DazeRelease(oCNpc* npc);
	void DazeUpdate(float dt);
	void DazeBeforeSave();
	void DazeClearAll();
	bool IsHumanGuild(oCNpc* npc);
}
