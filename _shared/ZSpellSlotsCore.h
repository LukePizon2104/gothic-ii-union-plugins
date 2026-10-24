// Sloty czarow ponad rozmiar skryptow - logika bez silnika (decyzje i rachunek rozmiarow). Czesc z silnikiem:
// ZSpellSlots.h. Testy: zTimeRewind\tests\test_core.cpp (sekcja "Sloty czarow").
//
// Silnik G2A czyta trzy tablice stalych parsera indeksem = ID czaru (oCSpell::spellID / C_Item.spell):
//   SPELLFXINSTANCENAMES  oCSpell::GetSpellInstanceName (instancja "SPELL_<x>" i efekt "SPELLFX_<x>")
//   SPELLFXANILETTERS     oCAIHuman::MagicInvestSpell, oCNpc::OnDamage_Anim (animacje T_<x>CAST itd.)
//   TXT_SPELLS            oCSpell::GetName, opis czaru w ekwipunku / ksiedze magii
// Kazde miejsce robi parser->GetSymbol(nazwa) i zCPar_Symbol::GetValue(zSTRING&, id), a GetValue NIE sprawdza
// zakresu (stringdata + id*20) - ID >= ele to odczyt poza tablica. Rozmiar = MAX_SPELL skryptow: wanilia G2A 100,
// New Balance 196 (wszystkie zajete). Wydluzamy tablice tylko w pamieci; stale nie trafiaja do zapisu gry.
#pragma once
#include <string>
#include <vector>
#include <cctype>

namespace zslotcore {
	const int MAX_ELE = 4095;                 // zCPar_Symbol::ele ma 12 bitow
	const int PAR_TYPE_STRING = 3;            // zPAR_TYPE_STRING
	const unsigned PAR_FLAG_CLASSVAR = 4;     // zPAR_FLAG_CLASSVAR - pole klasy, dane w instancji, nie w symbolu
	const int ARRAY_COUNT = 3;

	inline const char* ArrayName(int i) {
		static const char* const names[ARRAY_COUNT] = { "SPELLFXINSTANCENAMES", "SPELLFXANILETTERS", "TXT_SPELLS" };
		return i >= 0 && i < ARRAY_COUNT ? names[i] : nullptr;
	}

	inline bool ValidId(int id) { return id >= 0 && id < MAX_ELE; }
	inline int NeededCount(int id) { return id + 1; }

	enum class Action { Keep, Grow, Refuse };
	struct Plan {
		Action action;
		int size;                 // rozmiar po operacji (Keep: obecny, Grow: nowy)
		const char* why;          // powod odmowy (Refuse), inaczej nullptr
	};

	// found/type/flags/hasData/ele: stan symbolu tablicy; minCount: ile elementow potrzeba (id + 1).
	// Tablica jednoelementowa ma w silniku inny przydzial (pojedynczy zSTRING, nie tablica z licznikiem) - nie ruszamy.
	inline Plan PlanArray(bool found, int type, unsigned flags, bool hasData, int ele, int minCount) {
		if (!found)
			return { Action::Refuse, 0, "brak symbolu" };
		if (type != PAR_TYPE_STRING)
			return { Action::Refuse, ele, "symbol nie jest tablica stringow" };
		if (flags & PAR_FLAG_CLASSVAR)
			return { Action::Refuse, ele, "pole klasy" };
		if (!hasData || ele < 2)
			return { Action::Refuse, ele, "brak danych tablicy" };
		if (minCount < 1 || minCount > MAX_ELE)
			return { Action::Refuse, ele, "rozmiar poza zakresem 1..4095" };
		if (ele >= minCount)
			return { Action::Keep, ele, nullptr };
		return { Action::Grow, minCount, nullptr };
	}

	enum class SlotUse { Free, Ours, Taken };

	inline bool SameNoCase(const std::string& a, const std::string& b) {
		if (a.size() != b.size())
			return false;
		for (size_t i = 0; i < a.size(); i++)
			if (std::toupper((unsigned char)a[i]) != std::toupper((unsigned char)b[i]))
				return false;
		return true;
	}

	// values: SPELLFXINSTANCENAMES. Wolny slot = pusty, poza tablica albo wypelniacz (powtorzenie nizszego wpisu -
	// wanilia wypelnia wolne sloty "Light"). Taken = inny czar (np. NB 99 = UnlockChest) - tylko ostrzezenie w logu.
	inline SlotUse ClassifySlot(const std::vector<std::string>& values, int id, const std::string& ours) {
		if (id < 0 || id >= (int)values.size() || values[id].empty())
			return SlotUse::Free;
		if (SameNoCase(values[id], ours))
			return SlotUse::Ours;
		for (int i = 0; i < id; i++)
			if (SameNoCase(values[i], values[id]))
				return SlotUse::Free;
		return SlotUse::Taken;
	}
}
