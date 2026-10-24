// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Podnoszenie przedmiotow z dobyta bronia (jak w W3, LootArmed=1): z bronia biala / piesciami w rece klawisz ataku
	// gry (GAME_ACTION) podnosi przedmiot lezacy przed bohaterem (LootRange cm, +-45 st., widoczny), gdy w promieniu
	// LootEnemyRadius nie ma walczacego wroga - wtedy klawisz dalej atakuje. Bohater schyla sie natywna animacja
	// podnoszenia (T_STAND_2_IGET / S_IGET / T_IGET_2_STAND) z bronia w dloni i wraca do postawy broni stylu (garda W3);
	// przedmiot trafia do ekwipunku jak przy zwyklym podnoszeniu (komunikat EV_TAKEVOB, oCNpc::DoTakeVob). Wcisniecie
	// przejete nie zostawia ciosu w kolejce (silnik nie widzi klawisza do jego puszczenia). Podpowiedz (LootHint): nazwa
	// przedmiotu nad nim, gdy wcisniecie by go podnioslo. Logika bez silnika: Core\Loot.*.
	// Ten sam klawisz otwiera drzwi, skrzynie i przeszukuje zwloki (LootInteract.*): wygrywa cel z mniejszym wynikiem
	// (odleglosc + kara za kat), przy remisie przedmiot.

	// Klawisz ataku z hakow wejscia (Dodge.cpp, po decyzji unikow / ataku z wyskoku): -1 bez zmian, 0 = silnik ma go
	// nie widziec (wcisniecie przejete przez podnoszenie). toggled: wywolanie z GetToggled.
	int LootGameKeyOverride(unsigned short logicalKey, bool toggled);
	void LootUpdate(float dt);
	// Trwa podnoszenie zlecone przez plugin (od decyzji do powrotu do postawy).
	bool LootActive();
	// Zapis / wczytanie / zmiana swiata / zdism reload: bez sledzenia i kandydata (komunikat gry konczy sie sam).
	void LootFinishNow();
	// zdism loot [info] - kandydat przed bohaterem (przedmiot i cel interakcji), wrog w poblizu, powod odmowy, przebieg.
	std::string LootConsole(const std::string& arg);
	// Pierwszy walczacy wrog bohatera w promieniu r cm (zdcore::LootFoeBlocks), z pominieciem skip; nullptr gdy brak.
	// Tez spowolnienie finishera tylko przy ostatnim wrogu (Director.cpp).
	oCNpc* FindFightingFoe(float r, oCNpc* skip, float& distOut);
	// Wspolne z interakcjami z bronia (LootInteract.cpp): parametry zasiegu z ini, stan bohatera do odmowy, promien
	// widocznosci od piersi bohatera do punktu (statyczny swiat, bez ostatnich 15 cm) i napis nad bboxem (podpowiedz).
	zdcore::LootParams LootParamsNow();
	zdcore::LootHeroState LootHeroNow();
	bool LootRayClear(const zVEC3& hp, const zVEC3& to);
	void LootPrintAbove(const zTBBox3D& b, const zSTRING& text);
}
