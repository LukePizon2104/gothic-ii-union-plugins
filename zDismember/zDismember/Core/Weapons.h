// Logika bez silnika: czy bron odcina (sieczna), tylko przebija (rapier) czy wcale (obuch).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zdcore {
	// Maski typow obrazen jak DAM_* w skryptach (= oEDamageType w silniku).
	enum : uint32_t { DMG_BLUNT = 2, DMG_EDGE = 4, DMG_POINT = 64 };

	enum class WeaponCut {
		NoCut,        // nie odcina i bez finisherow: obuch (BLUNT, takze EDGE+BLUNT z modow), bez obrazen siecznych, BluntWeapons
		ThrustOnly,   // rapier: tylko finishery pchnieciem (cut=NONE), bez odcinania
		Cut,          // bron sieczna: odcinanie i wszystkie finishery
	};

	struct WeaponRules {
		std::vector<std::string> bluntTokens;    // BluntWeapons (wielkie litery)
		std::vector<std::string> rapierTokens;   // RapierWeapons (wielkie litery)
		bool rapierThrustOnly = true;            // RapierThrustOnly=0: rapier jak zwykly miecz
	};

	// damageTypes: maska obrazen ciosu / broni; instance: nazwa instancji (ITMW_RAPIER); visual: plik modelu broni
	// (ItMw_050_1h_Sword_Rapier_01.3ds) - fragmenty z list szukane w obu, bez wielkosci liter; rapierStyle: styl walki
	// rapierem na modelu (zEBA HUMANS_RAPIER_*). Obuch wygrywa z rapierem, rapier z ostrzem.
	WeaponCut ClassifyWeapon(uint32_t damageTypes, const std::string& instance, const std::string& visual,
		const WeaponRules& rules, bool rapierStyle = false);

	inline bool WeaponCanCut(WeaponCut c) { return c == WeaponCut::Cut; }
	inline bool WeaponCanFinish(WeaponCut c) { return c != WeaponCut::NoCut; }
	const char* WeaponCutName(WeaponCut c);
}
