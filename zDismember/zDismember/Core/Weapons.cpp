#include "Weapons.h"
#include "Text.h"

namespace zdcore {
	WeaponCut ClassifyWeapon(uint32_t damageTypes, const std::string& instance, const std::string& visual,
		const WeaponRules& rules, bool rapierStyle) {
		// G2: wszystkie maczugi, mloty, buzdygany i kostury maja DAM_BLUNT, miecze i topory DAM_EDGE (zadna bron obu naraz)
		if (!(damageTypes & DMG_EDGE) || (damageTypes & DMG_BLUNT))
			return WeaponCut::NoCut;
		// sieczne z wygladu obuchowe (Inkwizytor - model buzdyganu, Kostur sedziego, pogrzebacz) i bron z modow
		if (NameHasToken(instance, rules.bluntTokens) || NameHasToken(visual, rules.bluntTokens))
			return WeaponCut::NoCut;
		if (rules.rapierThrustOnly &&
			(rapierStyle || NameHasToken(instance, rules.rapierTokens) || NameHasToken(visual, rules.rapierTokens)))
			return WeaponCut::ThrustOnly;
		return WeaponCut::Cut;
	}

	const char* WeaponCutName(WeaponCut c) {
		switch (c) {
		case WeaponCut::Cut: return "ciecie";
		case WeaponCut::ThrustOnly: return "pchniecie";
		default: return "obuch";
		}
	}
}
