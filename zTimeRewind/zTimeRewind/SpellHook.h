// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Wpisuje czar TIMEREWIND do slotu SpellID (spellFxInstanceNames / spellFxAniLetters / TXT_SPELLS); ID poza
	// tablicami skryptow (NB: 201 przy 196 slotach) - wydluzenie przez _shared\ZSpellSlots.h.
	void SpellPatchSlot();
	// Po zakonczeniu przewijania runa czeka na puszczenie akcji, zanim zadziala znowu.
	void SpellUpdate();
	// Bohater trzyma w reku nasza rune.
	bool SpellRuneReady();
}
