// Logika bez silnika: wyszukiwanie animacji w tablicy zCModelPrototype::protoAnis bez dereferencji wpisow.
// Tablice nakladek sa wyrownane z bazowa wg aniID, a puste wpisy (nullptr) = nakladka nie zmienia tej animacji
// (silnik: zCModel::GetAniFromAniID 0x472F50 szuka od gory pierwszego niepustego protoAnis[id]). Dlatego
// zCModelPrototype::SearchAni (wyszukiwanie binarne po nazwie, czyta ani->aniName) wywraca sie na nakladce
// z pustymi wpisami - crash w New Balance przy bloku (PryParadeAni, SearchAniIndex 0x58A004 czyta 0x30).
#pragma once

namespace zdcore {
	// Indeks 'ani' w protoAnis: pod 'id', gdy miesci sie w tablicy (inny wpis = ta nakladka tej animacji nie ma),
	// liniowo tylko gdy tablica jest krotsza niz id (nie wyrownana). -1 = brak.
	inline int ProtoAniIndex(const void* const* anis, int n, int id, const void* ani) {
		if (!anis || n <= 0 || !ani)
			return -1;
		if (id >= 0 && id < n)
			return anis[id] == ani ? id : -1;
		for (int i = 0; i < n; i++)
			if (anis[i] == ani)
				return i;
		return -1;
	}

	inline int ProtoNullCount(const void* const* anis, int n) {
		int k = 0;
		for (int i = 0; anis && i < n; i++)
			if (!anis[i])
				k++;
		return k;
	}
}
