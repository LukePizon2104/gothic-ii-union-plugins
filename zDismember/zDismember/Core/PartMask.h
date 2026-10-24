// Logika bez silnika: czesci ciala i maska odciec zapisywana w aivar NPC.
#pragma once
#include <cstdint>
#include <string>
#include "Text.h"

namespace zdcore {
	// Kolejnosc = numer bitu w masce (spec: 0 glowa, 1-2 przedramie L/P, 3-4 ramie L/P, 5-6 lydka L/P, 7-8 udo L/P).
	enum class Part : int { Head, ArmL, ArmR, UpperArmL, UpperArmR, LegL, LegR, ThighL, ThighR, NoPart };

	inline uint32_t MaskSet(uint32_t mask, Part p) {
		return p == Part::NoPart ? mask : mask | (1u << (int)p);
	}

	inline bool MaskHas(uint32_t mask, Part p) {
		return p != Part::NoPart && ((mask >> (int)p) & 1u) != 0;
	}

	inline const char* PartName(Part p) {
		static const char* names[] = { "HEAD", "ARM_L", "ARM_R", "UPPERARM_L", "UPPERARM_R", "LEG_L", "LEG_R", "THIGH_L", "THIGH_R", "NONE" };
		return names[(int)p];
	}

	inline Part PartFromName(const std::string& name) {
		std::string n = Upper(Trim(name));
		for (int i = 0; i < (int)Part::NoPart; i++)
			if (n == PartName((Part)i))
				return (Part)i;
		return Part::NoPart;
	}
}
