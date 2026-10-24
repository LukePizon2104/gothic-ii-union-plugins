// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Zwrot przedmiotow zuzytych przez bohatera po chwili docelowej cofniecia (zwoje, mikstury, jedzenie,
	// amunicja). Rejestr zuzyc z hakow konkretnych sciezek silnika (logika w trcore, Core\Consume.h):
	//   oCMag_Book::Spell_Cast 0x4767A0         - zwoj (amount-- albo RemoveFromInv po rzucie)
	//   oCNpc::EV_UseItemToState 0x7558F0       - efekt mikstury / jedzenia (funkcja on_state przedmiotu)
	//   oCNpc::EV_DestroyInteractItem 0x754B40  - zniszczenie przedmiotu w rece po uzyciu (DEF_DESTROY_ITEM)
	//   oCNpc::UseItem 0x73BC10                 - natychmiastowe uzycie (jedzenie bez animacji)
	//   oCNpc::DoShootArrow 0x7446B0 (wirtualna) - strzala / belt z reki do swiata
	// W kazdym: licznik ekwipunku bohatera (z przedmiotami w rece, w slotach, w ksiedze czarow i ze spakowanego plecaka)
	// przed i po wywolaniu; w Spell_Cast dodatkowo amount zwoju ksiegi (spellitems[spellnr]) wprost.
	// Mana na czary (RestoreMana): mana bohatera przed / po Spell_Invest 0x476530, oCSpell::SetReleaseStatus 0x486670
	// i Spell_Cast (bez runy cofania) - suma wydatkow po chwili docelowej wraca przy Commit (do max many).

	void ConsumeReset();
	// Tick nagrywania: liczniki ekwipunku bohatera (tor zmian) - limit zwrotu do ubytku netto.
	void ConsumeSample(float t);
	void ConsumeTrim(float oldest);
	// Po Commit: przyszlosc rejestru i toru licznikow skasowana.
	void ConsumeCutAfter(float t);
	// Start cofania: strzaly usuwane w locie (RemoveProjectiles) wracaja do kolczana przy Commit.
	void ConsumeRewindBegin();
	void ConsumeArrowRemoved(zCVob* vob);
	// Commit: oddaje przedmioty zuzyte po t (CreateItems), oddaje mane wydana na czary po t (RestoreMana) i zabiera
	// przyrost many z oddanych mikstur (jedno ustawienie many, PlanManaRefund).
	void ConsumeRestoreAfter(float t);
	// "tr inv" - rejestr zuzyc i opcje (test w grze)
	void ConsumeRegisterCommands();
}
