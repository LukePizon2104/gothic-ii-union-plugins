// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Wczytanie zawczasu prototypow nakladek bohatera (PreloadOverlays=1, [ZDISMEMBER]). Bez tego kazde zalozenie nakladki,
	// ktorej nikt nie trzyma, czyta MSB i wszystkie MAN-y z dysku (zCModelPrototype::Load 0x589250), a zdjecie ja usuwa
	// (Release 0x588EE0 przy refCtr 0) - przelaczenie stylu / unik / przeskok = 5-12 ms w klatce. Plugin trzyma wlasna
	// referencje na prototypach (i na bazie HUMANS.MDS bohatera - prototyp nakladki pamieta wskaznik bazy, +0xC4, bez
	// referencji) do konca gry. Wolane w LoadEnd (bohater i jego model juz sa) i po zdism reload; kolejne wywolania
	// laduja tylko nowe nazwy. Brak pliku (np. bez zDismember_W3.vdf) = pominiete, bez bledu.
	void PreloadOverlays(const char* when);
	// Zwalnia nasze referencje (PreloadOverlays=0 po zdism reload).
	void PreloadReleaseAll(const char* why);
	// Nakladka trzymana zawczasu (nazwa z albo bez .MDS).
	bool PreloadHolds(const std::string& overlay);
	// Zalozenie / zdjecie nakladki .MDS w silniku z sekcja profilera (Ovl.Apply / Ovl.Remove); zalozenie dluzsze niz
	// 2 ms idzie do logu (z informacja, czy nakladka byla wczytana zawczasu). 'who' = kto zaklada (do logu).
	int ZApplyOverlayTimed(zCModel* m, const std::string& mds, const char* who);
	void ZRemoveOverlayTimed(zCModel* m, const std::string& mds);
	// Zegar QPC w ms.
	double ZClockMs();
}
