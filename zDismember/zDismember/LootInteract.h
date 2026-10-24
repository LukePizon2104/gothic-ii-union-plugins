// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Interakcje z dobyta bronia (rozszerzenie podnoszenia z Loot.cpp, logika w Core\Interact.*): z bronia biala albo
	// piesciami w rece klawisz ataku na celu przed bohaterem (zasieg LootRange, +-45 st., widoczny), gdy w promieniu
	// LootEnemyRadius nikt nie walczy:
	// - drzwi (oCMobDoor) bez zamka i skryptu: otwarcie / zamkniecie zwykla interakcja gry Z BRONIA W RECE;
	//   zamkniete na klucz / wytrych albo ze skryptem (conditionFunc, onStateFunc, przedmiot): przy LootArmedDoors=2
	//   schowanie broni, zwykla interakcja gry (klucz, minigra wytrycha, skrypty), potem dobycie;
	// - skrzynie (oCMobContainer, LootArmedChests): schowanie broni, zwykle otwarcie gry (zamki, wytrych, kradziez -
	//   AssessUseMob jak przy zwyklym otwarciu), po zamknieciu dobycie (okno ekwipunku i tak chowa bron w silniku);
	// - zwloki i nieprzytomni (LootArmedCorpses): okno przeszukania od razu jak w grze (OpenDeadNpc; silnik chowa bron
	//   przy otwarciu okna), po zamknieciu dobycie; puste ciala pomijane (atak zostaje grze);
	// - zamek bez klucza i wytrycha: tylko komunikat gry ("brak klucza"), bron zostaje w rece.
	// Ponowne dobycie (LootArmedRedraw) tym samym trybem broni, chyba ze w trakcie byla rozmowa albo gracz dobyl sam.
	// Podpowiedz (LootHint): nazwa celu nad nim. Log: "Interakcja z bronia: ..." (start, etapy, koniec z powodem,
	// odmowy z celem przed bohaterem).

	// Skan celow (z vobow zebranych przez podnoszenie: bbox wokol bohatera) - co klatke, gdy bohater moze.
	void IaScan(zCArray<zCVob*>& found, const zVEC3& hp, const zVEC3& at);
	void IaClearCandidate();
	// Najlepszy cel z ostatniego skanu (bez promieni, tez z haka klawisza): true + wynik (jak podnoszenie - mniejszy lepszy).
	bool IaCandidate(float& score);
	// Decyzja: cel z kandydata do startu w petli (IaUpdate).
	void IaAccept(const char* src);
	// Odmowa z celem przed bohaterem (powod z podnoszenia) / pominiety cel przy braku innych (np. zamek przy LootArmedDoors=1).
	void IaLogDeny(const char* src, zdcore::LootDeny d, const std::string& foeDesc);
	void IaLogSkipped(const char* src);
	// Start zleconej interakcji i przebieg (co klatke z LootUpdate); allowStart=false (LootArmed=0) - tylko dokonczenie.
	void IaUpdate(float dt, bool allowStart);
	// Trwa interakcja pluginu albo czeka na start.
	bool IaActive();
	// Zapis / wczytanie / zmiana swiata / zdism reload: bez przebiegu i kandydatow (gra konczy swoje sama).
	void IaFinishNow(const char* why);
	// Podpowiedz nad celem (gdy wygrywa z przedmiotem; warunki sprawdza LootDrawHint).
	void IaDrawHint();
	// Opis do konsoli (zdism loot): cel, plan, pominiety cel, przebieg.
	std::string IaConsole();
}
