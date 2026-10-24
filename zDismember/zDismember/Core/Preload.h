// Logika bez silnika: nakladki bohatera wczytywane zawczasu (PreloadOverlays w [ZDISMEMBER]).
// Silnik (Gothic2.exe): zCModel::ApplyModelProtoOverlay(nazwa) 0x5787F0 = zCModelPrototype::Load 0x589250 (szuka na
// liscie prototypow po nazwie pliku; brak -> LoadModelScript: MSB + kazda animacja zCModelAni::LoadMAN 0x587740 z dysku),
// ApplyModelProtoOverlay(proto) 0x578840 (refCtr++) i Release 0x588EE0. Zdjecie nakladki (0x578C40 / 0x578E40) robi
// Release - gdy nikt inny nie trzyma prototypu, refCtr = 0 i prototyp jest usuwany, wiec KAZDE ponowne zalozenie
// czyta MSB i MAN-y z dysku od nowa (Styles.Apply 5-12 ms w logu 16.10). Plugin trzyma wlasna referencje na
// prototypach swoich nakladek (Load z baza HUMANS.MDS bohatera) - zalozenie znajduje je wtedy w pamieci.
#pragma once
#include <string>
#include <vector>
#include "Catalog.h"
#include "Moves.h"
#include "Traversal.h"

namespace zdcore {
	// Nazwa nakladki do porownan i dla zCModelPrototype::Load (wolajacy dopisuje ".MDS"): wielkie litery, bez bialych
	// znakow na brzegach, bez rozszerzenia .MDS / .MSB.
	std::string OverlayKey(const std::string& name);

	// Nakladki, ktore plugin moze zalozyc bohaterowi, bez powtorzen, w kolejnosci zrodel: style (cykl debug: Gothic,
	// zEBA, natywne kopie, W3; ruch i sprint W3), inne (np. gesty Znakow zQuickSign z naszej paczki), ruchy (overlay i
	// overlayW3), przeskoki, finishery bohatera (overlay i inplace). Nakladki ofiary (victimOverlay) NIE - ida na inne
	// modele. Pomijane puste i nie-HUMANS*: prototyp nakladki jest przygotowany pod jedna baze (baseModelProto +0xC4,
	// sprawdzane w zalozeniu pod 0x5788AF), a baza bohatera to HUMANS.MDS.
	std::vector<std::string> HeroOverlaysToPreload(const std::vector<std::string>& styles, const std::vector<std::string>& extra,
		const std::vector<MoveDef>& moves, const std::vector<TravDef>& trav, const std::vector<FinisherDef>& finishers);

	// Chciane, ktorych jeszcze nie trzymamy (po kluczu OverlayKey) - kolejne wczytanie swiata laduje tylko nowe.
	std::vector<std::string> PreloadMissing(const std::vector<std::string>& wanted, const std::vector<std::string>& held);
}
