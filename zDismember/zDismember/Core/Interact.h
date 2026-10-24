// Logika bez silnika: interakcje z dobyta bronia - drzwi, skrzynie, zwloki (LootInteract.cpp w pluginie; rozszerzenie
// podnoszenia z Loot.*: ten sam klawisz ataku, ta sama blokada przy walczacym wrogu, wspolny wybor celu).
// Silnik (G2A, deasemblacja 22.10, pliki [notatki lokalne]):
// - oCAIHuman::StandActions 0x698EA0 (wolane z PC_ActionMove 0x699F60 TYLKO bez broni): fokus MOB -> IsStanding, klawisz
//   broni puszczony, !IsInteractingWith (vtable+0xE4), CanInteractWith (+0xE0) -> licznik AssessUseMob (0xAAD6A0 =
//   3000 z 0x83BE1C) i oCMobMsg(EV_STARTINTERACTION, gracz) do EM MOB-a; fokus martwy / nieprzytomny NPC ->
//   oCNpc::OpenDeadNpc 0x762970 (cel = GetFocusNpc, bez animacji; bez lupu skrypt PLAYER_PLUNDER_IS_EMPTY).
// - Sama interakcja nie patrzy na tryb broni: oCAIHuman::Moving -> CheckMobInteraction 0x6982B0 co klatke (kolejka pusta):
//   klawisz broni = przerwanie, AssessUseMob_S co 3 s (kradziez: B_AssessUseMob, CHESTBIG / CHESTSMALL), Interact.
//   StartInteraction 0x721580 ustawia kierunek UP / DOWN, wiec stany ida same: drzwi otworz / zamknij, skrzynia S1 = okno.
//   Po T_..._2_STAND gra S_RUN (postawa bez broni) - CorrectAniStates 0x6AAFA0 ja gasi i SearchStandAni daje postawe broni.
// - oCNpc::OpenInventory 0x762250 przy dobytej broni wysyla oCMsgWeapon(EV_REMOVEWEAPON): KAZDE okno ekwipunku (skrzynia
//   przez oCMobContainer::Open 0x726500, zwloki przez OpenDeadNpc) chowa bron. Ekwipunek z bronia w dloni to niezbadana
//   sciezka (oCNpcInventory::HandleEvent przestawia prawa reke przy pochodni: RemoveFromSlot / SetRightHand; uzycie
//   przedmiotu wklada go do dloni) - nie omijamy. Chowanie w trakcie interakcji z MOB-em (EV_RemoveWeapon -> StandUp przy
//   BS_MOBINTERACT) tez niezbadane, wiec skrzynia: najpierw schowanie broni, potem zwykle otwarcie gry, potem dobycie.
// - oCMobLockable::CanOpen 0x7244F0: otwarte albo klucz (keyInstance w ekwipunku) albo (pickLockStr i ITKE_LOCKPICK
//   i talent 5); inaczej EV_CALLSCRIPT PLAYER_MOB_MISSING_* / NEVER_OPEN (Print, T_DONTKNOW, glos) bez otwierania.
//   Wytrych = minigra w oCMobLockable::Interact 0x723CF0 (lewo / prawo).
// - SearchFreePosition 0x71DFC0 dla gracza bez wolnej pozycji wysyla PLAYER_MOB_TOO_FAR_AWAY / WRONG_SIDE - do skanu co
//   klatke nie wolno go wolac; zasieg liczymy z listy optimalPosList (ScanIdealPositions bez skutkow ubocznych).
// - oCMobDoor::Open / Close sa puste (0x71A430 / 0x71A440): drzwi = stan MOB-a, animacja modelu i wyzwalacze.
#pragma once
#include <string>
#include <vector>
#include "Loot.h"

namespace zdcore {
	enum IaKind { IA_NONE = 0, IA_ITEM, IA_DOOR, IA_CHEST, IA_CORPSE, IA_KIND_COUNT };
	const char* IaKindText(IaKind k);   // "przedmiot", "drzwi", "skrzynia", "zwloki"

	// Opcje (Gothic.ini [ZDISMEMBER], menu "Interakcje z bronia"): numer pozycji menu = wartosc.
	struct IaSettings {
		int doors = 2;        // LootArmedDoors: 0 wyl., 1 tylko otwarte (bez zamka i skryptu), 2 wszystkie
		bool chests = true;   // LootArmedChests
		bool corpses = true;  // LootArmedCorpses
		bool redraw = true;   // LootArmedRedraw: po schowaniu broni (przez nas albo okno gry) dobycie jej z powrotem
	};
	const int IA_DOORS_OFF = 0, IA_DOORS_OPEN = 1, IA_DOORS_ALL = 2, IA_DOORS_COUNT = 3;

	// MOB przed bohaterem (z silnika, bez skutkow ubocznych).
	struct IaMobInfo {
		bool door = false;
		bool chest = false;
		bool locked = false;          // oCMobLockable::locked
		bool keyNeeded = false;       // keyInstance niepusty
		bool hasKey = false;          // klucz w ekwipunku
		bool pickLock = false;        // pickLockStr niepusty (zamek na wytrych)
		bool hasLockpick = false;     // ITKE_LOCKPICK w ekwipunku
		bool pickTalent = false;      // talent otwierania zamkow (HasTalent(5, 1))
		bool condition = false;       // conditionFunc - warunek w skrypcie
		bool stateFunc = false;       // onStateFuncName - skrypt przy zmianie stanu
		bool useWithItem = false;     // wymaga przedmiotu w rece
		bool occupied = false;        // uzywa go inny NPC (npcsCurrent > 0)
		bool destroyed = false;
		bool open = false;            // state == state_num (drzwi otwarte - interakcja je zamknie)
	};
	// Jak oCMobLockable::CanOpen: otwarte, albo klucz, albo zamek na wytrych + wytrych + talent.
	bool IaMobCanOpen(const IaMobInfo& m);
	// "zamkniete (klucz ITKE_X: brak, wytrych: jest, talent: nie), warunek skryptu" - do logu.
	std::string IaMobDesc(const IaMobInfo& m);

	enum IaPlan {
		IAP_NONE = 0,
		IAP_ARMED,        // drzwi bez zamka i skryptu: zwykla sciezka gry z bronia w rece
		IAP_SHEATHE,      // schowanie broni -> zwykla sciezka gry (zamki, wytrych, skrypty, okno skrzyni) -> dobycie
		IAP_LOCKED_MSG,   // zamkniete bez klucza i wytrycha: tylko komunikat gry (CanOpen), bron zostaje w rece
		IAP_CORPSE,       // przeszukanie: okno od razu (gra chowa bron przy otwarciu ekwipunku), po zamknieciu dobycie
		IAP_SKIP_OFF,     // ten rodzaj wylaczony w ini - atak zostaje grze
		IAP_SKIP_SCRIPT,  // drzwi z zamkiem / skryptem przy LootArmedDoors=1
		IAP_SKIP_BUSY,    // MOB uzywa ktos inny / zniszczony
		IAP_SKIP_EMPTY,   // zwloki bez lupu (nic poza zalozonymi rzeczami)
		IAP_SKIP_ALIVE,   // NPC zywy i przytomny
		IAP_COUNT
	};
	const char* IaPlanText(IaPlan p);
	// Plan, ktory przejmuje klawisz ataku (reszta = cel pomijany, atak zostaje grze).
	inline bool IaPlanActs(IaPlan p) {
		return p == IAP_ARMED || p == IAP_SHEATHE || p == IAP_LOCKED_MSG || p == IAP_CORPSE;
	}
	IaPlan IaPlanMob(const IaMobInfo& m, const IaSettings& s);

	struct IaCorpseInfo {
		bool dead = false;
		bool unconscious = false;
		bool hasLoot = true;          // !inventory2.IsEmpty(pomin zbroje, pomin zalozone)
	};
	IaPlan IaPlanCorpse(const IaCorpseInfo& c, const IaSettings& s);

	// ---------- wybor celu ----------
	// bbox celu wzgledem srodka voba bohatera (cm).
	struct IaBox {
		float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
		float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	};
	// MOB w zasiegu: najblizszy punkt bboxa w poziomie najwyzej p.range, kat do niego w stozku p.coneDeg (blizej niz
	// p.nearAny albo bohater w obrysie - bez kata), pionowo bbox zachodzi na [p.minDy, p.maxDy]. score jak LootItemFits
	// (odleglosc + kara za kat) - porownywalny z przedmiotami. dist = odleglosc w poziomie do bboxa.
	bool IaBoxFits(const LootParams& p, float fwdX, float fwdZ, const IaBox& b, float& score, float& dist);
	// Wolne pozycje interakcji MOB-a (TMobOptPos po ScanIdealPositions) wzgledem bohatera; free = pusta albo bohatera.
	struct IaPos {
		float dx = 0.0f, dy = 0.0f, dz = 0.0f;
		bool free = true;
	};
	const float IA_MOB_REACH = 150.0f;   // CanInteractWith -> SearchFreePosition(npc, 150): odleglosc 3D do pozycji
	// Silnik przyjmie interakcje: jakas wolna pozycja najwyzej reach cm (3D). Pusta lista = nie wiadomo -> true (silnik
	// zdecyduje przy starcie).
	bool IaMobReachable(const std::vector<IaPos>& pos, float reach);
	// Cel interakcji wygrywa z najlepszym przedmiotem: mniejszy score; remis - przedmiot.
	bool IaTargetBeatsItem(bool hasItem, float itemScore, bool hasTarget, float targetScore);

	// ---------- przebieg (jedna interakcja na wcisniecie) ----------
	enum IaPhase {
		IAPH_IDLE = 0,
		IAPH_STAND,        // czekanie na postawe (IsStanding) przed startem
		IAPH_SHEATHE,      // schowanie broni (EV_REMOVEWEAPON1) przed zwykla sciezka gry
		IAPH_MOB,          // interakcja z MOB-em (start, trwanie, okno skrzyni)
		IAPH_WINDOW,       // okno zwlok
		IAPH_SETTLE,       // po drzwiach z bronia: postawa broni z powrotem
		IAPH_REDRAW_WAIT,  // czekanie, az mozna dobyc broni (okna zamkniete, kolejka pusta)
		IAPH_REDRAW,       // dobycie wyslane
		IAPH_COUNT
	};
	const char* IaPhaseText(IaPhase p);

	enum IaAction {
		IAA_NONE = 0,
		IAA_STAND,          // zatrzymac obrot i bieg (StopTurnAnis + _Stand)
		IAA_REMOVE_WEAPON,  // oCMsgWeapon(EV_REMOVEWEAPON1) jak PC_WeaponMove na stojaco
		IAA_START_MOB,      // jak StandActions: CanInteractWith -> licznik AssessUseMob + oCMobMsg(EV_STARTINTERACTION)
		IAA_OPEN_CORPSE,    // SetFocusVob(zwloki) + OpenDeadNpc
		IAA_FIX_STANCE,     // bron w rece, a gra postawa bez broni - StartAni(_s_walk)
		IAA_DRAW_WEAPON,    // oCMsgWeapon(EV_DRAWWEAPON1 / EV_DRAWWEAPON w ruchu, tryb sprzed)
		IAA_END,            // koniec (IaRun::endReason)
		IAA_COUNT
	};
	const char* IaActionText(IaAction a);

	// Obserwacje z silnika co klatke.
	struct IaObs {
		float dt = 0.0f;
		bool alive = true;         // bohater zywy i przytomny
		bool standing = true;      // oCAniCtrl_Human::IsStanding
		bool emEmpty = true;       // kolejka komunikatow bohatera pusta
		int weaponMode = 0;        // oCNpc::GetWeaponMode
		bool mobActive = false;    // GetInteractMob() == nasz MOB
		bool anyMob = false;       // GetInteractMob() != nullptr
		bool invOpen = false;      // okno ekwipunku / skrzyni / zwlok
		bool dialog = false;       // rozmowa (InformationManager)
		bool bodyFree = true;      // stoi / idzie / biegnie / skrada sie (nie skok, upadek, MOBINTERACT...)
		bool stanceWrong = false;  // bron w rece, a gra postawa bez broni (S_RUN / S_WALK / S_SNEAK trybu 0)
	};

	const float IA_STAND_MAX = 1.0f;         // s na postawe przed startem (bieg -> postawa ok. 0,3-0,5 s)
	const float IA_SHEATHE_LOST = 0.6f;      // po tylu s kolejka pusta, a bron dalej w rece = schowanie przerwane
	const float IA_SHEATHE_MAX = 4.0f;
	const float IA_MOB_START_MAX = 1.5f;     // interakcja musi ruszyc (GetInteractMob)
	const float IA_MOB_MAX = 300.0f;         // bezpiecznik interakcji bez okna (minigra wytrycha moze trwac)
	const float IA_WINDOW_OPEN_MAX = 0.3f;   // okno zwlok musi sie otworzyc
	const float IA_SETTLE_TIME = 0.8f;
	const float IA_REDRAW_WAIT_MAX = 20.0f;
	const float IA_REDRAW_MAX = 3.0f;

	struct IaRun {
		IaPhase phase = IAPH_IDLE;
		IaPlan plan = IAP_NONE;
		IaKind kind = IA_NONE;
		int weaponBefore = 0;      // tryb broni przed (cel ponownego dobycia)
		float t = 0.0f;            // od startu
		float phaseT = 0.0f;       // od wejscia w faze
		bool sheathedByUs = false;
		bool started = false;      // interakcja ruszyla / okno sie otworzylo
		bool sawWindow = false;
		bool sawDialog = false;    // rozmowa w trakcie (np. wlasciciel skrzyni) - bez ponownego dobycia
		bool fixedStance = false;
		std::string note;          // wynik etapu (np. "okno skrzyni zamkniete") - poczatek endReason
		std::string endReason;
	};
	inline bool IaRunActive(const IaRun& r) { return r.phase != IAPH_IDLE; }
	// Start po decyzji (plan ARMED / SHEATHE / CORPSE; LOCKED_MSG to jedna akcja silnika bez przebiegu - false).
	// Zwraca pierwsza akcje w a.
	bool IaBegin(IaRun& r, IaKind kind, IaPlan plan, const IaObs& o, IaAction& a);
	// Krok co klatke (po obserwacji). Jedna akcja na klatke; IAA_END = koniec, powod w r.endReason, r.phase = IDLE.
	IaAction IaStep(IaRun& r, const IaObs& o, const IaSettings& s);
	// Silnik nie wykonal akcji (np. CanInteractWith = 0, okno sie nie otworzylo) - dalszy ciag jak po nieudanym kroku.
	IaAction IaActionFailed(IaRun& r, IaAction a, const IaSettings& s, const char* why);
	// Przerwanie z zewnatrz (zapis, wczytanie, zdism reload) - bez akcji.
	void IaAbort(IaRun& r, const char* why);

	// Menu: pozycja = wartosc (LootArmedDoors 0-2, reszta 0/1) - poza zakresem bez zmian.
	int IaClampDoors(int v);
}
