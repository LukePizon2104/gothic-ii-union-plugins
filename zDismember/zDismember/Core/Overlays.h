// Logika bez silnika: ktore nakladki bohatera zdjac, zalozyc i jak je ulozyc na liscie modelu.
// Silnik (Gothic2.exe): zCModel::ApplyModelProtoOverlay (0x578840, petla pod 0x578933) i RemoveModelProtoOverlay
// (0x578C40) zatrzymuja WSZYSTKIE animacje postaci. Przestawienie wpisow w zCModel::modelProtoList nie zatrzymuje
// niczego - dlatego nakladki obecne na modelu tylko przekladamy, a zdejmujemy / zakladamy jedynie brakujace.
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	struct OverlayPlan {
		std::vector<std::string> remove;   // nasze, juz niechciane (zdjecie = stop wszystkich animacji)
		std::vector<std::string> park;     // na spod listy (tuz nad modelem bazowym), w tej kolejnosci
		std::vector<std::string> apply;    // na wierzch w tej kolejnosci (obecne - przelozyc, brakujace - zalozyc)
	};

	// Sprint gry / zEBA (HUMANS_SPRINT, HUMANS_SPRINT_REMASTER, Ninja NINJA_SPRINT_*) - nie nasza nakladka.
	bool IsForeignSprintOverlay(const std::string& name);
	// Najwyzszy obcy sprint na liscie (od dolu) albo "".
	std::string ForeignSprintOverlay(const std::vector<std::string>& model);
	// Nakladki chwilowe cudzych mechanik (gesty Znakow zQuickSign, uniki) - zostaja na wierzchu.
	bool IsTransientOverlay(const std::string& name);
	// Nakladka obcej mechaniki, nad ktora nie wolno klasc naszego ruchu / stylow (New Balance: jazda konna HUMANS_MOUNT -
	// petle biegu W3 nad nia psulyby siedzenie w siodle). Na modelu = plugin zdejmuje swoje nakladki ruchu i stylow.
	bool IsExclusiveForeignOverlay(const std::string& name);
	bool HasExclusiveForeignOverlay(const std::vector<std::string>& model);

	// Co bohater ma miec na modelu. Puste nazwy / listy = wylaczone albo brak nakladki (wypelnia wolajacy wg ini).
	struct MotionSetup {
		std::vector<std::string> styles;        // style broni (powtorzenia pomijane), w kolejnosci
		std::vector<std::string> weaponLoco;    // ruch W3 z bronia - sloty w stylu W3 (W3Locomotion)
		std::vector<std::string> weaponSprint;  // sprint W3 z bronia - sloty w stylu W3 (W3Sprint)
		std::string loco;                       // ruch W3 bez broni (W3Locomotion)
		std::string sprint;                     // sprint W3 bez broni (W3Sprint)
	};
	struct WantedSet {
		std::vector<std::string> top;           // od dolu do gory, nad nakladkami obcymi
		std::vector<std::string> parked;        // zalozone na stale, ale na spodzie (nic nie przykrywaja)
	};
	// Sprint W3 lezy na stale na spodzie listy; gdy gra sprintuje (obcy sprint na modelu), idzie na wierzch -
	// samo przelozenie, bez zakladania w trakcie biegu. Bez sprintu W3 ruch W3 zostaje pod sprintem gry.
	WantedSet WantedMotion(const MotionSetup& setup, bool foreignSprint);
	// sprintOnTop: sprint gry na modelu albo podglad sprintu (LOCO_SPRINT_PREVIEW). keepParked: nasze nakladki sprintu,
	// ktorych petli sprint gry nie ma (np. HUMANS_SPRINT mikstury szybkosci ma tylko S_RUNL) - zostaja na spodzie,
	// zeby z bronia nie biec szybciej, niz chce gra.
	WantedSet WantedMotion(const MotionSetup& setup, bool sprintOnTop, const std::vector<std::string>& keepParked);

	// Zestawy ruchu bohatera do testow (LocoKey, zdism loco). AUTO = wg ini (W3Locomotion, W3Sprint).
	enum LocoMode {
		LOCO_AUTO = 0,
		LOCO_GAME,             // Gothic / zEBA: bez naszych nakladek ruchu (styl walki zostaje)
		LOCO_W3_RUN,           // trucht i chod W3, sprint gry / zEBA
		LOCO_W3_FULL,          // trucht i chod W3 + sprint W3 w sprincie gry
		LOCO_SPRINT_PREVIEW,   // petla sprintu W3 jako zwykly bieg (podglad), w sprincie gry tez W3
		LOCO_MODE_COUNT
	};
	struct LocoFlags {
		bool loco = false;         // nakladki truchtu / chodu W3
		bool sprint = false;       // nakladki sprintu W3
		bool sprintOnTop = false;  // sprint W3 na wierzchu takze bez sprintu gry
	};
	// Nastepny / poprzedni tryb (w kolko); wartosc spoza listy -> AUTO.
	int CycleLocoMode(int mode, int step);
	LocoFlags ResolveLocoMode(int mode, bool iniLoco, bool iniSprint);
	const char* LocoModeLabel(int mode);

	// Nasza nakladka sprintu (HUMANS_ZDF_*SPRINT*).
	bool IsOurSprintOverlay(const std::string& name);
	// Wszystkie obce nakladki sprintu na liscie (od dolu) - dwie naraz = dwa mody sprintu w jednej chwili.
	std::vector<std::string> ForeignSprintOverlays(const std::vector<std::string>& model);
	// Ktory mod zaklada te nakladke sprintu: "zEBA", "Ninja Sprint", "mikstura szybkosci / zEBA", "W3 (zDismember)"...
	std::string SprintModLabel(const std::string& overlay);
	// Skad petla ruchu na ekranie: "Gothic" (model bazowy), "W3 trucht", "W3 sprint", "sprint zEBA", inaczej nazwa.
	std::string MotionSourceLabel(const std::string& overlay);
	// Skala fpsRate naszej petli sprintu: predkosc korzenia petli sprintu gry / naszej (zCModelAni::GetAniVelocity).
	// 0,5..2 - inaczej (petla w miejscu, brak danych) 1 = tempo wlasne.
	float SprintTempoScale(float refVelocity, float ourVelocity);
	// Jak wyzej, ale z tempem docelowym (W3SprintArmedSpeed - sprint z bronia 600 cm/s zamiast tempa sprintu gry):
	// target > 0 = do tej predkosci (te same jednostki), 0 = do predkosci petli gry.
	float SprintTargetScale(float targetVelocity, float refVelocity, float ourVelocity);
	// Petla sprintu i jej przejscia: S_1HRUNL -> S_1HRUNL, T_1HRUN_2_1HRUNL, T_1HRUNL_2_1HRUN; inna nazwa -> {}.
	std::vector<std::string> SprintAniNames(const std::string& loop);
	// Petle naszej nakladki sprintu: HUMANS_ZDF_W3_SPRINT -> S_RUNL i S_MAGRUNL (tryb magii = sprint W3 bez broni),
	// HUMANS_ZDF_W3_SPRINT_1H / _2H -> S_1HRUNL / S_2HRUNL; inna nakladka -> {}.
	std::vector<std::string> OurSprintLoops(const std::string& overlay);
	// Czy petla sprintu W3 jest z bronia (tempo W3SprintArmedSpeed): S_1HRUNL / S_2HRUNL tak, S_RUNL i S_MAGRUNL nie
	// (czar w reku = ruch Geralta bez broni, W3SprintUnarmedSpeed).
	bool SprintLoopArmed(const std::string& loop);

	// Klawisze modow sprintu (kody DIK). zEBA SprintKeyChoice: LSHIFT|RSHIFT|LCTRL|RCTRL|SprintKeyName z ini.
	int ZebaSprintKey(int choice, const std::string& keyName);
	// KEY_LSHIFT, LSHIFT, "LEWY SHIFT", "Left Shift", KEY_O, O, KEY_F5... -> kod DIK; nieznana -> 0.
	int DikFromKeyName(const std::string& name);
	// Kod DIK -> krotka nazwa ("O", "LSHIFT"), 0 -> "-", nieznany -> szesnastkowo.
	std::string KeyLabel(int dik);
	// Kod DIK -> nazwa dla gracza w napisach (lewy Alt "Alt", "prawy Alt", "Shift", "Ctrl", "Spacja", "CapsLock"),
	// reszta jak KeyLabel.
	std::string DikLabel(int dik);
	struct SprintSource {
		std::string name;
		std::vector<int> keys;     // klawisze sprintu (puste = brak klawisza)
		bool onForward = false;    // sprintuje przy kazdym biegu do przodu (Ninja Sprint Perma=0)
		bool active = true;        // paczka moda jest w grze i sprint wlaczony w jego ustawieniach
	};
	// Pary modow, ktore odpala to samo wcisniecie (wspolny klawisz albo sprint przy kazdym biegu) - opisy do logu.
	std::vector<std::string> SprintKeyClashes(const std::vector<SprintSource>& sources);

	// model: nakladki na modelu od dolu (bez bazowego), want: chciane na wierzchu w kolejnosci, parked: chciane na
	// spodzie, mine: zalozone przez nas. Chciane maja byc w tej kolejnosci i ponad obcymi; wyjatek - sprint gry moze
	// lezec nad nimi (podmienia tylko petle biegu), chyba ze chcemy nad nim wlasnego sprintu. Przekladany jest tylko
	// najkrotszy koniec listy. Nakladki chwilowe (gest Znaku, unik) moga lezec nad nimi.
	OverlayPlan PlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& want,
		const std::vector<std::string>& parked, const std::vector<std::string>& mine);
	OverlayPlan PlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& want,
		const std::vector<std::string>& mine);

	// Gdzie wstawiac "na wierzch": pod koncowymi nakladkami chwilowymi, a przy sprintGameOnTop tez pod sprintem gry
	// (gra sprintuje bez sprintu W3 - nasze nakladki nie moga przykryc jej petli biegu).
	size_t TopInsertIndex(const std::vector<std::string>& model, bool sprintGameOnTop);
	// Lista po wykonaniu planu (brakujace dopisane w swoich miejscach).
	std::vector<std::string> ApplyPlan(const std::vector<std::string>& model, const OverlayPlan& plan, bool sprintGameOnTop);
	// Czy wykonanie planu zatrzyma animacje (zdjecie albo zalozenie brakujacej nakladki).
	bool PlanStopsAnis(const std::vector<std::string>& model, const OverlayPlan& plan);

	// Animacje do odtworzenia po zmianie nakladek (po id - wezmie wersje z wierzchu): stany S_* i przejscia ruchu
	// (T_RUN_2_RUNL, T_1HRUNL_2_1HRUN...). Tego wymaga oCAniCtrl_Human::IsRunning (0x6AE200), na ktorym opiera sie
	// sprint zEBA - bez aktywnej petli biegu zEBA konczy sprint.
	bool IsRestorableAni(const std::string& aniName);
	// Petla ruchu (S_RUNL, S_1HWALKL, S_SNEAKL...) - do diagnostyki.
	bool IsMoveLoopAni(const std::string& aniName);
	// Animacja, ktorej nie wolno przerwac zdjeciem / zalozeniem nakladki (dobycie broni, uzycie przedmiotu, trafienie):
	// T_* inne niz przejscia ruchu, na warstwach 1-2.
	bool IsBlockingAni(const std::string& aniName, int layer);
}
