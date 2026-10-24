#include "PartPick.h"

namespace zdcore {
	// [kategoria][strona 0=L 1=P][typ 0=dalszy 1=blizszy]
	static const Part kParts[3][2][2] = {
		{ { Part::Head, Part::Head }, { Part::Head, Part::Head } },
		{ { Part::ArmL, Part::UpperArmL }, { Part::ArmR, Part::UpperArmR } },
		{ { Part::LegL, Part::ThighL }, { Part::LegR, Part::ThighR } },
	};

	static bool CategoryAvailable(int cat, uint32_t available) {
		for (int s = 0; s < 2; s++)
			for (int t = 0; t < 2; t++)
				if (MaskHas(available, kParts[cat][s][t]))
					return true;
		return false;
	}

	Part PickPart(int side, const int wSwing[3], const int wThrust[3], uint32_t rnd, uint32_t available) {
		const int* w = side == 0 ? wThrust : wSwing;
		int total = 0;
		for (int c = 0; c < 3; c++)
			if (w[c] > 0 && CategoryAvailable(c, available))
				total += w[c];
		if (total <= 0)
			return Part::NoPart;

		int r = (int)(rnd % (uint32_t)total), cat = 0;
		for (int c = 0; c < 3; c++) {
			if (!(w[c] > 0 && CategoryAvailable(c, available)))
				continue;
			if (r < w[c]) {
				cat = c;
				break;
			}
			r -= w[c];
		}

		int s = side < 0 ? 0 : side > 0 ? 1 : (int)((rnd >> 8) & 1u);
		int t = ((rnd >> 12) % 10u) < 7u ? 0 : 1;   // 70% dalszy segment (przedramie/lydka)
		const int order[4][2] = { { s, t }, { s, 1 - t }, { 1 - s, t }, { 1 - s, 1 - t } };
		for (auto& o : order)
			if (MaskHas(available, kParts[cat][o[0]][o[1]]))
				return kParts[cat][o[0]][o[1]];
		return Part::NoPart;
	}
}
