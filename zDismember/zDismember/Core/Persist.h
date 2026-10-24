// Logika bez silnika: pamiec kikutow w aivarze NPC (AivarIndex) - kiedy mozna jej ufac i ktore aivary sa wolne.
#pragma once
#include <cstdint>
#include <string>
#include <cctype>
#include "PartMask.h"

namespace zdcore {
	const int AIVAR_COUNT = 100;            // C_NPC.aivar[100] = oCNpc::aiscriptvars (G2A: oCNpc+0x288)
	const uint32_t PERSIST_MASK_BITS = 0x1FFu;   // 9 czesci ciala (PartMask.h)

	// Wartosc z bitami spoza maski = ten aivar uzywa skrypt moda: nie czytamy go i nie nadpisujemy.
	inline bool AivarMaskValid(int v) {
		return ((uint32_t)v & ~PERSIST_MASK_BITS) == 0;
	}

	// Stala skryptu nazywajaca aivar: AIV_* / AIVRX_* (New Balance) itp. - typ int, stala, jeden element, 0..99.
	inline bool IsAivarConstName(const std::string& name) {
		return name.size() > 3 && toupper((unsigned char)name[0]) == 'A' && toupper((unsigned char)name[1]) == 'I' &&
			toupper((unsigned char)name[2]) == 'V';
	}
	inline void MarkAivarName(const std::string& name, int value, bool named[AIVAR_COUNT]) {
		if (IsAivarConstName(name) && value >= 0 && value < AIVAR_COUNT)
			named[value] = true;
	}

	// Spis aivara AivarIndex w swiecie przy wczytaniu.
	struct PersistCensus {
		int npcs = 0;           // NPC w swiecie
		int masks = 0;          // niezerowa wartosc mieszczaca sie w masce czesci
		int foreign = 0;        // wartosc spoza maski (np. licznik skryptu)
		int aliveMasked = 0;    // maska u zywego NPC - odcinamy przy smierci (DoDie, finisher), wiec zywy z maska to nie nasz
		int aliveHeadless = 0;  // z tego z glowa (bit 0) - widac od razu: bohater bez glowy, NB 18.10
	};

	inline void PersistCount(PersistCensus& c, int value, bool alive) {
		c.npcs++;
		if (!value)
			return;
		if (!AivarMaskValid(value)) {
			c.foreign++;
			return;
		}
		c.masks++;
		if (!alive)
			return;
		c.aliveMasked++;
		if (MaskHas((uint32_t)value, Part::Head))
			c.aliveHeadless++;
	}

	enum class PersistMode { Off, NamedByScript, ForeignValues, On };

	inline const char* PersistModeName(PersistMode m) {
		switch (m) {
		case PersistMode::Off: return "wylaczona (AivarIndex=-1)";
		case PersistMode::NamedByScript: return "wylaczona - aivar ma nazwe w skryptach moda";
		case PersistMode::ForeignValues: return "wylaczona - aivar zapisuje mod (wartosci nie sa maskami odciec)";
		default: return "wlaczona";
		}
	}

	// Decyzja przy wczytaniu swiata. Odcinamy przy smierci, wiec maski maja zwloki (po dlugiej grze nawet wiekszosc NPC
	// swiata - liczba masek nic nie mowi). Zywi z maska (poza pojedynczymi "zdism cut" na zywym) albo wartosci spoza maski
	// = aivar uzywa mod (New Balance: aivar 86 maly i niezerowy u 496 NPC -> bohater bez glowy).
	inline PersistMode DecidePersist(int aivarIndex, bool namedByScript, const PersistCensus& c) {
		if (aivarIndex < 0 || aivarIndex >= AIVAR_COUNT)
			return PersistMode::Off;
		if (namedByScript)
			return PersistMode::NamedByScript;
		if (c.aliveHeadless >= 2 || c.aliveMasked >= 5 || c.foreign >= 3)
			return PersistMode::ForeignValues;
		return PersistMode::On;
	}

	// Lista indeksow "1,5,7-9": bez nazwy w skryptach i (gdy usedBy != nullptr) zero u wszystkich NPC.
	inline std::string FreeAivarList(const bool named[AIVAR_COUNT], const int* usedBy) {
		std::string out;
		int i = 0;
		while (i < AIVAR_COUNT) {
			auto free = [&](int k) { return !named[k] && (!usedBy || usedBy[k] == 0); };
			if (!free(i)) {
				i++;
				continue;
			}
			int j = i;
			while (j + 1 < AIVAR_COUNT && free(j + 1))
				j++;
			if (!out.empty())
				out += ",";
			out += std::to_string(i);
			if (j > i)
				out += (j == i + 1 ? "," : "-") + std::to_string(j);
			i = j + 1;
		}
		return out.empty() ? "brak" : out;
	}
}
