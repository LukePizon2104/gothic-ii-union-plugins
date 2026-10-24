// Sloty czarow ponad rozmiar tablic skryptow (np. New Balance: MAX_SPELL = 196, wszystkie ID zajete) - czesc z
// silnikiem. Decyzje: ZSpellSlotsCore.h (testy w zTimeRewind\tests\test_core.cpp).
//
// Dolaczac RAZ z Headers.h pluginu (nawiasy ostre, jak ZProf.h): #include <../../_shared/ZSpellSlots.h>
// Plik otwiera namespace GOTHIC_ENGINE, wiec musi byc w kolejce Headers.h (po naglowkach Union SDK), bez #pragma once.
//
//   std::vector<std::string> log;
//   zslots::SetSpellSlot(201, "TIMEREWIND", "HEA", "Piaski Czasu", log);   // wydluza tablice, gdy trzeba
//   zslots::EnsureSpellSlots(202, log);                                   // samo wydluzenie (np. kilka slotow naraz)
//   for (auto& l : log) MyLog("%s", l.c_str());
// Wywolywac po wczytaniu skryptow (Game_Init) - przed pierwszym uzyciem runy. Na Steamie (100 slotow) dla ID < 100
// nic nie jest przydzielane.
//
// Wydluzenie tablicy stalych SPELLFXINSTANCENAMES / SPELLFXANILETTERS / TXT_SPELLS (zCPar_Symbol, typ string):
//  - nowa pamiec przydziela SILNIK: ele = nowy rozmiar, stringdata = 0, bit space = 0, zCPar_Symbol::AllocSpace()
//    (G2A 0x7A17C0: operator new silnika na ele*20+4, licznik w pierwszym slowie, konstruktory zSTRING, bit space).
//    Destruktor symbolu (0x7A16F0) zwalnia ja wirtualnym vector-deleting-dtor zSTRING - ten sam przydzial i uklad,
//    wiec przy zamknieciu gry albo czyszczeniu parsera nic nie jest zwalniane cudzym alokatorem;
//  - stare wpisy kopiowane (zSTRING::operator=), nowe puste; stara tablica zostaje w pamieci celowo (kilka KB):
//    ktos (silnik, DLL moda) mogl zapamietac wskaznik stringdata - dalej wskazuje na wazne dane;
//  - symbol zostaje ten sam (wskazniki zCPar_Symbol* i indeksy symboli, np. oCText::InitText, pozostaja wazne);
//  - stale nie ida do zapisu gry (oCSavegame zapisuje zmienne, nie stale); skrypty czytaja te tablice tylko
//    indeksami stalymi z czasu kompilacji (< MAX_SPELL), wiec wydluzenie jest dla nich niewidoczne;
//  - tablic PLAYER_TALENT_RUNES / MAKE / DAMUP_RUNES itp. (zmienne gracza) nie ruszamy.
#include "ZSpellSlotsCore.h"
#include <cstdarg>
#include <cstdio>

namespace GOTHIC_ENGINE {
	namespace zslots {
		inline std::string SlotFmt(const char* fmt, ...) {
			char buf[512];
			va_list a;
			va_start(a, fmt);
			vsnprintf(buf, sizeof(buf), fmt, a);
			va_end(a);
			return buf;
		}

		inline zCPar_Symbol* SlotSymbol(const char* name) {
			return parser ? parser->GetSymbol(zSTRING(name)) : nullptr;
		}

		// Wydluza jedna tablice do minCount (nigdy nie skraca). false = odmowa / blad (opis w log).
		inline bool GrowSpellArray(const char* name, int minCount, std::vector<std::string>& log) {
			zCPar_Symbol* sym = SlotSymbol(name);
			zslotcore::Plan p = sym
				? zslotcore::PlanArray(true, (int)sym->type, sym->flags, sym->stringdata != nullptr, (int)sym->ele, minCount)
				: zslotcore::PlanArray(false, 0, 0, false, 0, minCount);
			if (p.action == zslotcore::Action::Refuse) {
				log.push_back(SlotFmt("SpellSlots: %s - odmowa (%s)", name, p.why));
				return false;
			}
			if (p.action == zslotcore::Action::Keep)
				return true;
			zSTRING* old = sym->stringdata;
			int oldCount = (int)sym->ele;
			sym->stringdata = nullptr;
			sym->ele = (unsigned)p.size;
			sym->space = 0;
			sym->AllocSpace();
			if (!sym->stringdata || (int)sym->ele != p.size) {
				sym->stringdata = old;
				sym->ele = (unsigned)oldCount;
				sym->space = 1;
				log.push_back(SlotFmt("SpellSlots: %s - AllocSpace nie przydzielil %d elementow, bez zmian", name, p.size));
				return false;
			}
			for (int i = 0; i < oldCount; i++)
				sym->stringdata[i] = old[i];
			// stara tablica zostaje celowo (patrz wyzej)
			log.push_back(SlotFmt("SpellSlots: %s %d -> %d", name, oldCount, p.size));
			return true;
		}

		// Wszystkie trzy tablice maja co najmniej minCount elementow. false = choc jedna odmowila.
		inline bool EnsureSpellSlots(int minCount, std::vector<std::string>& log) {
			bool ok = true;
			for (int i = 0; i < zslotcore::ARRAY_COUNT; i++)
				ok = GrowSpellArray(zslotcore::ArrayName(i), minCount, log) && ok;
			return ok;
		}

		// Wpisuje czar do slotu id (wydluza tablice, gdy id >= rozmiar). Zajety slot (inny czar) - nadpisuje
		// z ostrzezeniem w logu: decyzja nalezy do ini pluginu.
		inline bool SetSpellSlot(int id, const char* fxInstanceName, const char* aniLetters, const char* txtName,
			std::vector<std::string>& log) {
			if (!zslotcore::ValidId(id)) {
				log.push_back(SlotFmt("SpellSlots: ID %d poza zakresem", id));
				return false;
			}
			if (!EnsureSpellSlots(zslotcore::NeededCount(id), log))
				return false;
			zCPar_Symbol* fx = SlotSymbol(zslotcore::ArrayName(0));
			std::vector<std::string> names;
			names.reserve(fx->ele);
			for (int i = 0; i < (int)fx->ele; i++)
				names.push_back(fx->stringdata[i].ToChar());
			if (zslotcore::ClassifySlot(names, id, fxInstanceName) == zslotcore::SlotUse::Taken)
				log.push_back(SlotFmt("SpellSlots: UWAGA slot %d zajety przez czar %s - nadpisuje (zmien SpellID)", id,
					names[id].c_str()));
			const char* values[zslotcore::ARRAY_COUNT] = { fxInstanceName, aniLetters, txtName };
			for (int i = 0; i < zslotcore::ARRAY_COUNT; i++) {
				zSTRING v(values[i]);
				SlotSymbol(zslotcore::ArrayName(i))->SetValue(v, id);
			}
			log.push_back(SlotFmt("SpellSlots: slot %d = %s (%s, %s), rozmiar tablic %d", id, fxInstanceName, aniLetters,
				txtName, (int)fx->ele));
			return true;
		}
	}
}
