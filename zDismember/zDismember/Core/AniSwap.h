// Logika bez silnika: zamiana animacji walki 1H <-> 2H bohatera (AniSwap.cpp w pluginie).
// Silnik (G2A): nazwy animacji broni maja przedrostek oCAniCtrl_Human::GetWeaponString(tryb) 0x6AEC60 ("1H" dla trybow
// 2 sztylet i 3 miecz, "2H" dla 4). InitAllAnis 0x6A5BF0 wypelnia tablice id animacji per tryb broni (s_run[tryb]...,
// napisy wpisane w kod, bez GetWeaponString) i dla kazdego trybu wola InitFightAnis(tryb) 0x6AA0B0 (tablice walki).
// Zamiana = tablice trybu broni bohatera dostaja id animacji drugiego typu; tryb walki (oCNpc::GetWeaponMode), obrazenia,
// talent i zasieg zostaja z prawdziwej broni.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	// Tryby broni silnika (NPC_WEAPON_*): 2 sztylet, 3 bron 1H, 4 bron 2H; tablice silnika maja 8 trybow (0..7).
	enum { SWAP_WM_NONE = 0, SWAP_WM_DAG = 2, SWAP_WM_1H = 3, SWAP_WM_2H = 4, SWAP_WM_COUNT = 8 };
	// Zamiana animacji (WeaponSwap w Gothic.ini, numer pozycji w menu): 0 wyl., 1 1H jak 2H, 2 2H jak 1H, 3 obie.
	enum { SWAP_OFF = 0, SWAP_1H_AS_2H = 1, SWAP_2H_AS_1H = 2, SWAP_BOTH = 3, SWAP_MODE_COUNT = 4 };
	// Rodzaj broni wg animacji: 1 = 1H (tez sztylet), 2 = 2H, 0 = inna (piesci, luk, kusza, magia).
	enum { SWAP_TYPE_NONE = 0, SWAP_TYPE_1H = 1, SWAP_TYPE_2H = 2 };

	int SwapTypeOfWeaponMode(int wm);
	// Flagi przedmiotu (ITM_FLAG_*): DAG / SWD / AXE -> 1H, 2HD_SWD / 2HD_AXE -> 2H.
	int SwapTypeOfItemFlags(int flags);
	int SwapOtherType(int type);
	// Wariant zamiany obejmuje rodzaj broni.
	bool SwapModeCovers(int mode, int type);
	// "wyl." / "1H jak 2H" / "2H jak 1H" / "obie" (do logu i konsoli, bez polskich znakow).
	const char* SwapModeLabel(int mode);

	struct SwapSettings {
		int mode = SWAP_OFF;                 // WeaponSwap
		int last = SWAP_BOTH;                // WeaponSwapLast: ostatni niezerowy wariant (klawisz go przywraca)
		bool perWeapon = false;              // WeaponSwapPerWeapon: stan zamiany osobno dla kazdej broni
		std::vector<std::string> weapons;    // WeaponSwapWeapons: instancje z wlaczona zamiana (wielkie litery)
		int level = 0;                       // WeaponSwapLevel: 0 talent broni w rece, 1 talent drugiego typu
	};
	// Czy bron (rodzaj, nazwa instancji) gra animacjami drugiego typu. Globalnie: wg wariantu. Pamiec per bron:
	// bron na liscie i wariant obejmuje jej rodzaj (wariant "wyl." wylacza wszystko).
	bool SwapActiveFor(const SwapSettings& s, int type, const std::string& instance);

	// Tablica zamiany: map[tryb] = tryb, ktorego animacje (przedrostek "1H" / "2H") dostaje tryb broni. Bez zamiany
	// map[i] = i.
	struct SwapTable {
		int map[SWAP_WM_COUNT];
	};
	SwapTable SwapIdentity();
	bool SwapIsIdentity(const SwapTable& t);
	bool SwapSame(const SwapTable& a, const SwapTable& b);
	// Globalnie (perWeapon=0): oba rodzaje wg wariantu. Pamiec per bron: tylko rodzaj broni biezacej (w rece albo
	// zalozonej), gdy jest na liscie - inna bron przebuduje tablice przy zmianie.
	SwapTable SwapBuildTable(const SwapSettings& s, int curType, const std::string& curInstance);
	int SwapMapWeaponMode(const SwapTable& t, int wm);
	// Tablice per tryb broni w oCAniCtrl_Human (G2A, offsety od poczatku obiektu, int[10] co 0x28) przestawiane przy
	// zamianie: s_run..t_jump_2_runl i s_sneak..t_hitfrun. BEZ s_weapon (0x11B0, S_1H / S_2H): to poza w dobyciu broni,
	// czytaja ja tylko DrawWeapon1 0x6B2610 / DrawWeapon2 0x6B2CF0 / RemoveWeapon1/2 / ChooseNextWeapon2 /
	// EV_ChooseWeapon / IsInWeaponChoose - natywne T_RUN_2_1H konczy sie w S_1H, a DrawWeapon1 czeka na
	// s_weapon[tryb]; z S_2H w tablicy dobycie stalo z reka na rekojesci (test 17.10).
	struct SwapArrayRange {
		int begin, end;
	};
	const int SWAP_ARRAY_STRIDE = 0x28;
	const int SWAP_OFS_S_WEAPON = 0x11B0;
	const std::vector<SwapArrayRange>& SwapModeArrayRanges();
	bool SwapRemapsOffset(int ofs);
	// Koniec dobycia: natywny lancuch (T_1H_2_1HRUN, nastepna S_1HRUN z nazwy) konczy sie postawa natywna, a
	// oCNpc::EV_DrawWeapon2 0x74D580 czeka na _s_walk (po zamianie S_2HRUN). native / current = {S_xRUN, S_xWALK,
	// S_xSNEAK} trybu broni przed / po zamianie; zwraca id postawy po zamianie dla nastepnej animacji nextId albo -1.
	int SwapStanceRedirect(const int native[3], const int current[3], int nextId);
	// Straznik komunikatu broni (dobycie / schowanie / zmiana) przy zamianie: START nowy komunikat, STUCK ten sam dluzej
	// niz limit (raz), END komunikatu juz nie ma.
	enum { SWAP_WATCH_NONE = 0, SWAP_WATCH_START, SWAP_WATCH_STUCK, SWAP_WATCH_END };
	struct SwapWeaponWatch {
		const void* msg = nullptr;
		unsigned startMs = 0;
		bool stuck = false;
	};
	int SwapWatchStep(SwapWeaponWatch& w, const void* msg, unsigned nowMs, unsigned limitMs);
	// Styl drugiego typu (cykl U, styl W3): przedrostek _1H / _2H w nazwie nakladki zamieniony na toType
	// (HUMANS_ZDF_W3_1H -> HUMANS_ZDF_W3_2H, HUMANS_1HST1_REMASTER -> HUMANS_2HST1_REMASTER); "" = bez przedrostka
	// broni (HUMANS_RAPIER_ST1, HUMANS_1X2ST3). Czy odpowiednik istnieje w grze - sprawdza wolajacy.
	std::string SwapStyleCounterpart(const std::string& overlay, int toType);
	// Czyj wybor stylu (cykl U) i talent prowadzi zestaw animacji slotType: broni biezacej, gdy gra jego animacjami
	// (zamiana), inaczej wlasny typ zestawu.
	int SwapSlotDriver(const SwapTable& t, int curType, int slotType);
	// Diagnostyka drgania: ta sama animacja startowana w >= threshold ROZNYCH klatkach w oknie 'window' klatek - true raz
	// na serie. Kilka startow w jednej klatce liczy sie raz (23.10, log NB: skrypt RX_APPLYANIM_WEAPONS przy zalozeniu
	// broni zdejmuje / zaklada ok. 20 nakladek w jednej klatce, kazda zatrzymuje animacje, a migawka SwKeepAnis wznawia
	// postawe z ta sama faza - 22-44 "starty" S_RUN w klatce to nie drganie, a wpis RESTART i 40 klatek sledzenia
	// zasypywaly log).
	struct SwapRestartWatch {
		int aniId = -1;
		unsigned first = 0;
		unsigned last = 0;              // klatka ostatniego policzonego startu
		int count = 0;
		bool flagged = false;
	};
	bool SwapRestartStep(SwapRestartWatch& w, int aniId, unsigned frame, int threshold, unsigned window);

	// Bronie parowe New Balance (para *_RIGHT / *_LEFT): EQUIP_*_RIGHT ustawia zmienna skryptu RH_READY_2X2 (prawa:
	// flagi 2HD_SWD - tryb walki 2H, slot ZS_LONGSWORD), EQUIP_*_LEFT - LH_READY_2X2 (flagi tarczy); RX_APPLYANIM_WEAPONS
	// zaklada nakladke HUMANS_1X2ST3 (sama prawa) albo HUMANS_2X2ST3 (obie) - animacje 2H, ktore kluczuja tez wezel
	// ZS_LEFTHAND (lewa bron w dloni). Gra parowa = zawsze natywne animacje NB: bez zamiany 1H <-> 2H, bez stylu W3 /
	// talentu nad nakladkami NB, dobycie i polozenie broni natywne.
	enum { DUAL_NONE = 0, DUAL_RIGHT = 1, DUAL_PAIR = 2 };
	struct DualInput {
		int rhVar = -1, lhVar = -1;          // RH_READY_2X2 / LH_READY_2X2 (-1 = brak symbolu - Steam G2, Kroniki, inny mod)
		std::vector<std::string> overlays;   // natywne nakladki postaci (oCNpc::activeOverlays - tez po wczytaniu zapisu)
		std::string weapon;                  // instancja zalozonej broni bialej ("" = brak)
		int weaponType = SWAP_TYPE_NONE;     // rodzaj zalozonej broni (SwapTypeOfItemFlags)
	};
	// Zmienne skryptu rozstrzygaja (sa w zapisie); bez nich - nakladki NB: HUMANS_2X2ST3 = para, HUMANS_1X2ST3 przy broni
	// *_RIGHT = sama prawa (HUMANS_1X2ST3 to tez styl zEBA, wiec samej nakladki nie wystarcza). Bez broni bialej - brak.
	int DualState(const DualInput& in);
	const char* DualLabel(int state);    // "-" / "prawa (1x2)" / "para (2x2)"
	// oCAniCtrl_Human::CorrectAniStates 0x6AAFA0 (co klatke z oCAIHuman::DoAI 0x69BF04) wygasza grane postawy
	// s_walk / s_run / s_sneak wszystkich trybow innych niz biezacy (poza trybem 2). Po zamianie tryby z tym samym
	// zrodlem (map[i] == map[wm]) maja te same id - "1H jak 2H": s_run[4] == s_run[3] == S_2HRUN, wiec biezaca postawa
	// byla wygaszana co klatke, a _Stand / SearchStandAni startowal ja od nowa (drganie, test 17.10). Zwraca tryby do
	// zamaskowania na czas wywolania (rosnaco, bez wm).
	std::vector<int> SwapCollidingModes(const SwapTable& t, int wm);
	// Tablica id animacji per tryb broni (arr[0..SWAP_WM_COUNT)): arr[wm] = poprzednie arr[map[wm]].
	void SwapRemapModeArray(int* arr, const SwapTable& t);
	// "1H->2H 2H->1H" / "brak" (log).
	std::string SwapTableLabel(const SwapTable& t);

	// Klawisz zamiany (WeaponSwapKey, domyslnie Ctrl+Shift+U). Pamiec per bron wylaczona: wl./wyl. calej zamiany
	// (wylaczenie zapamietuje wariant w last, wlaczenie go przywraca). Pamiec per bron: dopisuje / wypisuje bron
	// biezaca z listy; gdy wariant nie obejmuje jej rodzaju, przechodzi na last (gdy obejmuje) albo "obie". Bez broni
	// bialej (curType 0) - jak bez pamieci per bron.
	struct SwapToggleResult {
		SwapSettings settings;
		bool perWeaponChange = false;        // zmiana dotyczyla listy broni
		bool active = false;                 // bron biezaca gra teraz animacjami drugiego typu
	};
	SwapToggleResult SwapToggle(const SwapSettings& s, int curType, const std::string& curInstance);
	std::string SwapJoinWeapons(const std::vector<std::string>& weapons);

	// Modyfikatory klawisza (WeaponSwapMods): suma.
	enum { SWAP_MOD_CTRL = 1, SWAP_MOD_SHIFT = 2, SWAP_MOD_ALT = 4 };
	const int SWAP_KEY_DEFAULT = 22;         // DIK_U
	const int SWAP_MODS_DEFAULT = SWAP_MOD_CTRL | SWAP_MOD_SHIFT;
	// Pozycja menu "Klawisz zamiany": 0 Ctrl+Shift+U, 1 wyl. (klawisz 0), 2 wlasny (Gothic.ini).
	int SwapKeyMenuIndex(int key, int mods);
	// Pozycja z menu -> klucze; 2 (wlasny) nic nie zmienia.
	void SwapKeyMenuApply(int index, int& key, int& mods);
	std::string SwapKeyLabel(int key, int mods);   // "Ctrl+Shift+U", "-" gdy wylaczony

	// Dobycie / schowanie / zmiana broni (zostaja natywne): przejscia T_..._2_... miedzy stanem bez broni i z bronia
	// (T_MOVE_2_1HMOVE, T_1HMOVE_2_MOVE, T_RUN_2_1H, T_1H_2_RUN) albo z sama poza broni (T_1H_2_1HRUN, T_1H_2_2H).
	bool SwapIsDrawAni(const std::string& aniName);
	// Nazwa z przedrostkiem broni fromType zamienionym na toType (na poczatku czlonu: po "S_", "T_" i "_2_"):
	// T_1HRUN_2_1HRUNL -> T_2HRUN_2_2HRUNL, T_1HSFINISH -> T_2HSFINISH. "" = nazwa bez przedrostka fromType.
	std::string SwapAniName(const std::string& aniName, int fromType, int toType);

	// Nakladki talentu (oCNpc::SetTalentSkill 0x730F60: "HUMANS_" + 1HS/2HS + "T" + poziom + ".MDS", poziom 1..2;
	// zEBA dodaje HUMANS_1HST3 / 2HST3 jako style). Nazwa jak w ModelOverlays: wielkie litery, bez .MDS.
	std::string SwapTalentOverlay(int type, int level);    // "" dla poziomu <= 0
	bool SwapParseTalentOverlay(const std::string& name, int& type, int& level);   // poziom 1..3, z .MDS albo bez
	// Nakladki, ktore maja lezec na modelu: natywne (oCNpc::activeOverlays, nazwy bez .MDS) - a przy zamianie broni
	// biezacej i poziomie 0 (talent broni w rece) nakladki talentu drugiego typu podmienione na poziom talentu broni
	// w rece (skill 0 = bez nakladki).
	std::vector<std::string> SwapWantedOverlays(const std::vector<std::string>& native, bool active, int levelOption,
		int curType, int skill1H, int skill2H);
	// Plan dla nakladek talentu (inne nakladki modelu bez zmian): zdejmij / zaloz i docelowa kolejnosc calej listy
	// (od dolu, bez modelu bazowego). Zalozona trafia w miejsce zdjetej tego samego rodzaju; bez niej - nad najblizsza
	// wczesniejsza natywna obecna na modelu (przywrocenie), a nakladka spoza natywnych na sam dol (style zEBA / W3
	// leza nad nia).
	struct SwapOverlayPlan {
		std::vector<std::string> remove, add, order;
	};
	SwapOverlayPlan SwapPlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& native,
		const std::vector<std::string>& wanted);
	// Podpis nakladek talentu z listy (posortowane) - zmiana natywnych / chcianych = nowy plan.
	std::string SwapTalentSignature(const std::vector<std::string>& overlays);

	// Odlozenie przebudowy / zmiany nakladek. Twarde powody czekaja zawsze (finisher, unik, przeskok, cios, parada,
	// dobycie broni w kolejce, stan ciala inny niz stanie / chod / bieg / skradanie / sprint). Miekkie (bieg, animacja,
	// ktorej nie wolno przerwac) - najwyzej SWAP_SOFT_WAIT_FRAMES klatek.
	struct SwapBusy {
		bool director = false, moves = false, traversal = false, attacking = false, parade = false, weaponMsg = false,
			body = false;
		bool soft = false;
	};
	const unsigned SWAP_SOFT_WAIT_FRAMES = 180;
	// nullptr = mozna teraz; inaczej powod (log).
	const char* SwapDeferReason(const SwapBusy& b, unsigned framesWaited);
}
