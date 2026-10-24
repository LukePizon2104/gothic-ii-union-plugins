// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Znaki Wiedzmina (Aard, Igni, Axii, Yrden, Quen) jako runy w slotach 90-94 (SignSpellIds). Skrypty (czary, runy,
	// efekty) w data\System\Autorun\zDismember_Signs*.d; tu: rzut, cele, obrazenia, stany, strefa, tarcza.
	// Logika bez silnika: Core\Signs.*, projekt: [dokument projektowy - niepublikowany].

	// Nazwy, litery animacji i teksty slotow w tablicach parsera (SPELLFXINSTANCENAMES, SPELLFXANILETTERS, TXT_SPELLS).
	void SignsPatchSlots();
	// Po DirectorInit: dopina sie do zdOnBeforeHit (tarcza Quen, trafienie przerywa oszolomienie Axii).
	void SignsInit();
	void SignsUpdate(float dt);
	// Zapis: przed - cofniecie Yrden (tempo i ochrona NPC), efektow tarczy / stref / oszolomienia; po - przywrocenie.
	void SignsBeforeSave();
	void SignsAfterSave();
	// Wczytanie, nowa gra, zmiana swiata, wylaczenie w menu: wszystko cofniete i zapomniane.
	void SignsClearAll();
	// zdism sign <aard|igni|axii|yrden|quen|info>
	std::string SignsConsole(const std::string& arg);
	// zdism fires [m]: ognie w promieniu (domyslnie 10 m) do logu i na konsole - klasa, wizual, stan, grupa.
	std::string SignsFiresConsole(const std::string& arg);
}
