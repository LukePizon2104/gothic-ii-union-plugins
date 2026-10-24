// Logika bez silnika: tempo ciosow stylu Wiedzmina (W3AtkSpeed* w [ZDISMEMBER]) i wariant ciec bocznych 2H (W3Side2H).
// Silnik (Gothic2.exe, oCAniCtrl_Human::HitCombo 0x6B0260): klatki zdarzen DEF_OPT_FRAME / DEF_HIT_END / DEF_WINDOW sa
// porownywane z actFrame aktywnej animacji, a actFrame rosnie o fpsRate x czas - wiec skala fpsRate animacji ciosu
// skaluje razem z nia trafienie, okno kombinacji i koniec ciosu. Wcisniecie ataku w oknie [ws, we] ustawia
// SetActFrame(we - 1), a przy actFrame >= DEF_HIT_END silnik przechodzi do kolejnego segmentu (albo konczy cios), wiec:
// najwczesniejszy kolejny cios po ciosie bocznym = poczatek okna (ws), odstep trafien kombinacji =
// (ws_i - opt_i) + (opt_{i+1} - (we_i - 1)) klatek.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	enum AtkSlot { ATK_1H_LEFT, ATK_1H_RIGHT, ATK_1H_COMBO, ATK_2H_LEFT, ATK_2H_RIGHT, ATK_2H_COMBO, ATK_SLOT_COUNT };
	const char* AtkSlotAni(int slot);         // T_1HATTACKL, T_1HATTACKR, S_1HATTACK, T_2HATTACKL...
	const char* AtkSlotIniKey(int slot);      // W3AtkSpeed1HLeft, W3AtkSpeed1HRight, W3AtkSpeed1HCombo, W3AtkSpeed2H...
	const char* AtkSlotMenuKey(int slot);     // MenuW3AtkSpeed1HLeft...
	int AtkSlotOfAni(const std::string& aniName);   // -1 = nie cios stylu
	bool AtkSlotIs2H(int slot);
	bool AtkSlotIsCombo(int slot);

	// Wartosc opcji: procent tempa animacji (100 = bez zmian), 0 = "jak natywne" (tempo animacji, ktora gralaby bez
	// nakladki W3: talent / zEBA / HUMANS). Inne wartosci przycinane do [ATK_SPEED_MIN, ATK_SPEED_MAX].
	const int ATK_SPEED_NATIVE = 0;
	const int ATK_SPEED_DEFAULT = 100;
	const int ATK_SPEED_MIN = 50, ATK_SPEED_MAX = 300;
	int ClampAtkSpeed(int percent);
	// Pozycje choiceboxa w menu (kolejnosc jak w zDismember_Menu.d).
	const std::vector<int>& AtkSpeedMenuValues();
	// Skala tempa "jak natywne" przycinana do tego zakresu.
	const float ATK_MATCH_MIN = 0.5f, ATK_MATCH_MAX = 3.0f;

	// Zdarzenia walki animacji ciosu.
	struct AtkEvents {
		std::vector<int> opt, end, win;
		int frames = 0;
		float fps = 25.0f;
	};
	// "4 37 74 110" -> {4,37,74,110}; separatory: spacje / tabulatory / przecinki, reszta (np. ZS_RIGHTHAND) pominieta.
	std::vector<int> ParseEventInts(const std::string& s);
	// Tag zdarzenia (tagString, pierwszy napis wartosci) -> AtkEvents (inne tagi bez zmian).
	void AtkEventsAddTag(AtkEvents& e, const std::string& tag, const std::string& values);
	// Czas (s przy fps) od startu do pierwszego trafienia; -1 bez DEF_OPT_FRAME.
	float AtkFirstHitSeconds(const AtkEvents& e);
	// Czas do najwczesniejszego kolejnego ciosu (s przy fps): cios pojedynczy = poczatek okna (bez okna: koniec ciosu,
	// bez niego: dlugosc); kombinacja = sredni odstep trafien (jedno trafienie - jak cios pojedynczy). -1 bez danych.
	float AtkPaceSeconds(const AtkEvents& e, bool combo);
	// Odstepy trafien kombinacji w klatkach (semantyka silnika z naglowka).
	std::vector<int> AtkComboGaps(const AtkEvents& e);

	// Skala fpsRate: percent / 100; ATK_SPEED_NATIVE -> oursPace / nativePace w [ATK_MATCH_MIN, ATK_MATCH_MAX]
	// (brak natywnej albo zlych danych = 1).
	float AtkTempoScale(int percent, float oursPace, float nativePace);
	// fpsRate docelowy = baza z pliku x skala; true, gdy obecny rozni sie o wiecej niz 1e-4 (bez zapisu co klatke i bez
	// dryfu - zawsze od bazy).
	bool AtkFpsTarget(float base, float scale, float current, float& target);

	// Ciecia boczne 2H w stylu W3 (W3Side2H): mocne ciecia Geralta (nakladka HUMANS_ZDF_W3_2H), szybkie dwureczne
	// (HUMANS_ZDF_W3_2HS wstawione w jej miejsce) albo natywne (pusty wpis - gra animacja z nakladki ponizej).
	enum Side2H { SIDE2H_W3 = 0, SIDE2H_FAST = 1, SIDE2H_NATIVE = 2, SIDE2H_COUNT };
	int ClampSide2H(int v);
	// Brak nakladki szybkich ciec -> W3.
	int Side2HEffective(int wanted, bool fastAvailable);
	const char* Side2HLabel(int v);

	// Wpis tablicy protoAnis[id] HUMANS_ZDF_W3_2H, ktory plugin podmienia. orig = animacja z pliku, placed = to, co
	// plugin tam wpisal (nullptr przy "natywne"), changed = wpis rozni sie od pliku.
	struct SideEntry {
		const void* orig = nullptr;
		const void* placed = nullptr;
		bool known = false;
		bool changed = false;
	};
	// Obserwacja wpisu w tej klatce: pierwszy raz albo wpis obcy (ani orig, ani nasz placed - prototyp wczytany od nowa)
	// -> orig = cur, bez zmian. Zwraca true, gdy wpis uznany za nowy.
	bool SideEntryObserve(SideEntry& s, const void* cur);
	// Co ma stac we wpisie dla wariantu (efektywnego): W3 -> orig, FAST -> fast (bez niego orig), NATIVE -> nullptr.
	const void* SideEntryWant(const SideEntry& s, int side2h, const void* fast);
	// Zapis wykonany: placed = want, changed = want != orig.
	void SideEntryWrote(SideEntry& s, const void* want);
}
