// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	struct LastHit {
		oCNpc* attacker = nullptr;
		int weaponMode = 0;        // NPC_WEAPON_*
		bool edge = false;         // obrazenia sieczne, bez czaru
		zdcore::WeaponCut cut = zdcore::WeaponCut::NoCut;   // bron ciosu: odcina / tylko pchniecie (rapier) / obuch
		int side = 0;              // -1 zamach z lewej, +1 z prawej, 0 nieznany
		unsigned int frame = 0;
		zVEC3 dir;                 // pozioma, od napastnika do ofiary
	};

	// Ostatni cios w ofiare; nullptr gdy brak.
	const LastHit* GetLastHit(oCNpc* victim);
	// Cios z tej samej (lub poprzedniej) klatki, bronia 1H/2H, ktora moze dobijac (sieczna albo rapier - nie obuch).
	// Odciecie wymaga dodatkowo zdcore::WeaponCanCut(hit->cut).
	bool IsFreshEdgeHit(const LastHit* hit);
	zVEC3 GibVelocity(const zVEC3& dir, int side);
	// Bron w reku zadaje obrazenia sieczne (sam typ obrazen - np. typ kontry w Dodge).
	bool NpcHasEdgeWeapon(oCNpc* npc);
	// Bron w reku wg regul odcinania: typ obrazen, BluntWeapons, RapierWeapons, styl rapierowy zEBA (bohater).
	zdcore::WeaponCut NpcWeaponCut(oCNpc* npc);
	// Rapier (NpcWeaponCut == ThrustOnly).
	bool NpcUsesRapier(oCNpc* npc);

	// true = trafienie polkniete (oryginalny OnDamage_Hit nie jest wolany): karencja oszolomienia, nietykalnosc w finisherze.
	extern std::function<bool(oCNpc* victim, oCNpc::oSDamageDescriptor& desc)> zdOnBeforeHit;
	// true = smierc odroczona (finisher), oryginalny DoDie nie jest wolany.
	extern std::function<bool(oCNpc* victim, oCNpc* killer)> zdOnBeforeDeath;
	// true = zamiast powalenia (smiertelny cios w czlowieka) ruszyl finisher; oryginalny DropUnconscious nie jest wolany.
	extern std::function<bool(oCNpc* victim, oCNpc* attacker)> zdOnBeforeKnockout;
	// Smierc z finishera/egzekucji: DoDie tego NPC przechodzi bez losowania odciecia.
	extern oCNpc* zdDeathPassThrough;

	uint32_t ZRand32();
	void CombatClear();
	void CombatCollect();
}
