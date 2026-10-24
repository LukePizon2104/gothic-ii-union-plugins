// Logika bez silnika: wybor odcinanej czesci przy zwyklym zabiciu.
#pragma once
#include <cstdint>
#include "PartMask.h"

namespace zdcore {
	// side: -1 zamach z lewej, +1 z prawej, 0 pchniecie (wagi wThrust, strona losowa).
	// Wagi kategorii: [0] glowa, [1] reka, [2] noga. available = maska czesci, ktore istnieja i nie sa odciete.
	// Kategorie bez dostepnych czesci odpadaja z losowania; reka/noga: najpierw strona ciosu, potem druga.
	Part PickPart(int side, const int wSwing[3], const int wThrust[3], uint32_t rnd, uint32_t available);
}
