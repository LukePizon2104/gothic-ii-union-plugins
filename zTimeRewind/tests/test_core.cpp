// Testy logiki zTimeRewind niezaleznej od silnika (Core\). Budowanie: tests\build_tests.cmd
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "Core/TrCore.h"
#include "../../_shared/ZSpellSlotsCore.h"

using namespace trcore;

static int g_pass = 0, g_fail = 0;
#define CHECK(e) do { if (e) g_pass++; else { g_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #e); } } while (0)

struct TestReg { const char* name; void (*fn)(); };
static std::vector<TestReg>& Tests() { static std::vector<TestReg> t; return t; }
struct Registrar { Registrar(const char* n, void (*f)()) { Tests().push_back({ n, f }); } };
#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()

// ---------- Text ----------
TEST(split_args_uppercases_and_drops_extra_spaces) {
	auto a = SplitArgs("tr  rewind   10");
	CHECK(a.size() == 3 && a[0] == "TR" && a[1] == "REWIND" && a[2] == "10");
	CHECK(SplitArgs("   ").empty());
}

// ---------- Timeline ----------
static Sample S(float t, float x, int hp = 100) {
	Sample s;
	s.t = t;
	s.x = x;
	s.hp = hp;
	return s;
}

TEST(push_skips_repeats_and_keeps_hold_before_change) {
	Track tr;
	tr.Push(S(0.0f, 0.0f));
	tr.Push(S(0.1f, 0.0f));
	tr.Push(S(0.2f, 0.0f));
	CHECK(tr.Size() == 1);
	CHECK(std::fabs(tr.LastTime() - 0.2f) < 1e-6f);
	tr.Push(S(0.3f, 10.0f));
	CHECK(tr.Size() == 3);                      // 0.0, trzymana 0.2, 0.3
	Sample o;
	CHECK(tr.At(0.15f, o) && o.x == 0.0f);      // stal w miejscu, nie rozmyty ruch
	CHECK(tr.At(0.25f, o) && std::fabs(o.x - 5.0f) < 1e-4f);
}
TEST(at_before_first_is_false_after_last_is_last_state) {
	Track tr;
	Sample o;
	CHECK(!tr.At(1.0f, o));
	tr.Push(S(1.0f, 3.0f));
	CHECK(!tr.At(0.5f, o));
	CHECK(tr.At(9.0f, o) && o.x == 3.0f && o.t == 9.0f);
}
TEST(interpolate_yaw_takes_short_way_round) {
	Sample a, b;
	a.t = 0.0f; a.yaw = 3.1f;
	b.t = 1.0f; b.yaw = -3.1f;
	Sample m = Interpolate(a, b, 0.5f);
	CHECK(std::fabs(std::fabs(m.yaw) - 3.14159f) < 0.01f);
	CHECK(std::fabs(AngleDelta(3.1f, -3.1f) - 0.0832f) < 0.001f);
}
TEST(interpolate_frame_lerps_same_ani_and_steps_on_wrap) {
	Sample a, b;
	a.t = 0.0f; a.aniId = 5; a.frame = 10.0f;
	b.t = 1.0f; b.aniId = 5; b.frame = 20.0f;
	CHECK(std::fabs(Interpolate(a, b, 0.5f).frame - 15.0f) < 1e-4f);
	b.frame = 2.0f;                              // petla animacji przeskoczyla na poczatek
	CHECK(Interpolate(a, b, 0.4f).frame == 10.0f);
	CHECK(Interpolate(a, b, 0.6f).frame == 2.0f);
	b.aniId = 7; b.frame = 30.0f;                // inna animacja: skok do blizszej probki
	Sample m = Interpolate(a, b, 0.7f);
	CHECK(m.aniId == 7 && m.frame == 30.0f);
}
TEST(interpolate_discrete_values_come_from_earlier_sample) {
	Sample a = S(0.0f, 0.0f, 10), b = S(1.0f, 0.0f, 0);
	a.life = LIFE_ALIVE; b.life = LIFE_DEAD;
	Sample m = Interpolate(a, b, 0.9f);
	CHECK(m.hp == 10 && m.life == LIFE_ALIVE);
}
TEST(trim_before_keeps_anchor) {
	Track tr;
	for (int i = 0; i < 4; i++)
		tr.Push(S((float)i, (float)i * 10.0f));
	tr.TrimBefore(1.5f);
	CHECK(tr.First().t == 1.0f);
	Sample o;
	CHECK(tr.At(1.5f, o) && std::fabs(o.x - 15.0f) < 1e-4f);
}
TEST(cut_after_drops_future_and_ends_at_cut) {
	Track tr;
	for (int i = 0; i < 4; i++)
		tr.Push(S((float)i, (float)i * 10.0f));
	tr.CutAfter(1.5f);
	CHECK(tr.Size() == 3);
	CHECK(std::fabs(tr.LastTime() - 1.5f) < 1e-6f);
	Sample o;
	CHECK(tr.At(1.5f, o) && std::fabs(o.x - 15.0f) < 1e-4f);
	tr.Push(S(1.6f, 99.0f));                     // nagrywanie po cofnieciu idzie dalej od punktu ciecia
	CHECK(tr.At(1.6f, o) && o.x == 99.0f);
}
TEST(cut_after_before_first_empties_track) {
	Track tr;
	tr.Push(S(5.0f, 1.0f));
	tr.Push(S(6.0f, 2.0f));
	tr.CutAfter(4.0f);
	CHECK(tr.Empty());
	CHECK(tr.LastTime() == 0.0f);
}
TEST(cut_after_drops_pending_hold) {
	Track tr;
	tr.Push(S(0.0f, 0.0f));
	tr.Push(S(1.0f, 0.0f));                      // trzymana
	tr.CutAfter(0.5f);
	CHECK(std::fabs(tr.LastTime() - 0.5f) < 1e-6f);
}

// ---------- Postacie poza swiatem (zdespawnowane, na voblist_npcs) ----------
TEST(decide_sample_skips_despawned_and_extends_known) {
	CHECK(DecideSample(true, false) == SampleAction::Sample);
	CHECK(DecideSample(true, true) == SampleAction::Sample);
	CHECK(DecideSample(false, false) == SampleAction::Skip);     // nowej poza swiatem nie nagrywamy
	CHECK(DecideSample(false, true) == SampleAction::Extend);    // znana: stan bez zmian, bez modelu
}
TEST(extend_keeps_state_through_gap_without_slide) {
	Track tr;
	tr.Push(S(0.0f, 0.0f));
	tr.Extend(5.0f);                              // zniknela ze swiata: stoi bez zmian do 5 s
	CHECK(std::fabs(tr.LastTime() - 5.0f) < 1e-6f);
	tr.Push(S(5.1f, 100.0f));                     // wraca do swiata gdzie indziej
	Sample o;
	CHECK(tr.At(2.5f, o) && o.x == 0.0f);         // w przerwie tam, gdzie zniknela - bez "plyniecia" przez mape
	CHECK(tr.At(5.05f, o) && std::fabs(o.x - 50.0f) < 1e-3f);   // zmiana tylko w jednym odstepie probek
}
TEST(extend_on_empty_or_past_time_does_nothing) {
	Track tr;
	tr.Extend(1.0f);
	CHECK(tr.Empty() && tr.LastTime() == 0.0f);
	tr.Push(S(2.0f, 1.0f));
	tr.Extend(1.5f);
	CHECK(std::fabs(tr.LastTime() - 2.0f) < 1e-6f && tr.Size() == 1);
}
TEST(extend_does_not_grow_track_and_keeps_actor_fresh) {
	Track tr;
	tr.Push(S(0.0f, 7.0f));
	for (int i = 1; i <= 300; i++)
		tr.Extend(i * 0.1f);
	CHECK(tr.Size() == 1);                        // bez nowych probek w torze
	tr.TrimBefore(29.0f);
	CHECK(!tr.Empty() && tr.LastTime() >= 29.0f); // nie wypada z nagrania (LastTime >= oldest)
	Sample o;
	CHECK(tr.At(29.5f, o) && o.x == 7.0f);
}
TEST(hard_reset_on_move_hp_life_or_weapon) {
	Sample a = S(0.0f, 0.0f, 50), b = S(1.0f, 30.0f, 50);
	CHECK(!NeedsHardReset(a, b, 50.0f));         // 30 cm - zostaje, jak stoi
	b.x = 80.0f;
	CHECK(NeedsHardReset(a, b, 50.0f));
	b.x = 0.0f; b.hp = 40;
	CHECK(NeedsHardReset(a, b, 50.0f));
	b.hp = 50; b.weapon = 3;
	CHECK(NeedsHardReset(a, b, 50.0f));
	b.weapon = 0; b.life = LIFE_DEAD;
	CHECK(NeedsHardReset(a, b, 50.0f));
}
TEST(clock_lerps_cuts_and_trims) {
	ClockTrack c;
	float v = 0.0f;
	CHECK(!c.At(0.0f, v));
	c.Push(0.0f, 100.0f);
	c.Push(1.0f, 200.0f);
	c.Push(2.0f, 300.0f);
	CHECK(c.At(0.5f, v) && std::fabs(v - 150.0f) < 1e-3f);
	CHECK(!c.At(-1.0f, v));
	c.CutAfter(1.25f);
	CHECK(std::fabs(c.LastTime() - 1.25f) < 1e-6f);
	CHECK(c.At(1.25f, v) && std::fabs(v - 225.0f) < 1e-3f);
	c.TrimBefore(1.1f);
	CHECK(c.FirstTime() == 1.0f);
}

// ---------- Commit (reguly przywracania ze specu) ----------
static Sample L(float x, int hp, unsigned char life) {
	Sample s = S(0.0f, x, hp);
	s.life = life;
	return s;
}

TEST(commit_alive_target_far_away_is_hard_reset_to_target) {
	CHECK(DecideCommit(L(0, 50, LIFE_ALIVE), L(500, 50, LIFE_ALIVE), false, false, false, 50.0f) == CommitAction::HardTarget);
}
TEST(commit_alive_target_close_keeps_pre_rewind_pose) {
	// stal / siedzial prawie w tym samym miejscu: AI sie nie cofa, wiec poza wraca do stanu sprzed cofniecia
	CHECK(DecideCommit(L(0, 50, LIFE_ALIVE), L(30, 50, LIFE_ALIVE), false, false, false, 50.0f) == CommitAction::SoftLatest);
}
TEST(commit_dead_now_alive_then_is_revived) {
	CHECK(DecideCommit(L(0, 50, LIFE_ALIVE), L(0, 0, LIFE_DEAD), true, false, false, 50.0f) == CommitAction::HardTarget);
}
TEST(commit_knocked_now_alive_then_gets_up) {
	CHECK(DecideCommit(L(0, 50, LIFE_ALIVE), L(0, 1, LIFE_UNCONSCIOUS), false, true, false, 50.0f) == CommitAction::HardTarget);
}
TEST(commit_dead_then_and_now_stays_dead) {
	CHECK(DecideCommit(L(0, 0, LIFE_DEAD), L(0, 0, LIFE_DEAD), true, false, false, 50.0f) == CommitAction::Keep);
}
TEST(commit_npc_knocked_then_but_alive_now_is_not_knocked_again) {
	CHECK(DecideCommit(L(0, 1, LIFE_UNCONSCIOUS), L(800, 50, LIFE_ALIVE), false, false, false, 50.0f) == CommitAction::SoftLatest);
}
TEST(commit_hero_knocked_then_stands_at_target) {
	// bohater nie zostaje w miejscu sprzed cofniecia, gdy swiat sie cofnal
	CHECK(DecideCommit(L(0, 1, LIFE_UNCONSCIOUS), L(800, 50, LIFE_ALIVE), false, false, true, 50.0f) == CommitAction::HardTarget);
}
TEST(commit_hero_close_and_same_state_stays_soft) {
	CHECK(DecideCommit(L(0, 80, LIFE_ALIVE), L(10, 80, LIFE_ALIVE), false, false, true, 50.0f) == CommitAction::SoftLatest);
}

// ---------- Menu (choicebox zapisuje numer pozycji) ----------
TEST(menu_index_for_value_exact_or_nearest) {
	std::vector<int> secs = { 10, 20, 30, 45, 60 };
	CHECK(MenuIndexForValue(secs, 30) == 2);
	CHECK(MenuIndexForValue(secs, 33) == 2 && MenuIndexForValue(secs, 40) == 3);
	CHECK(MenuIndexForValue(secs, 1) == 0 && MenuIndexForValue(secs, 999) == 4);
	CHECK(MenuIndexForValue(secs, 33, false) == -1);
}
TEST(menu_changed_value_only_when_moved_in_menu) {
	std::vector<int> secs = { 10, 20, 30, 45, 60 };
	int v = 0;
	CHECK(MenuChangedValue(secs, 2, 4, v) && v == 60);   // gracz przestawil w menu
	CHECK(!MenuChangedValue(secs, 2, 2, v));             // bez zmiany - reczna wartosc z ini zostaje
	CHECK(!MenuChangedValue(secs, 2, 5, v));             // poza lista
	CHECK(!MenuChangedValue(secs, 2, -1, v));
}

// ---------- Auto-cofniecie przy smierci ----------
TEST(auto_rewind_needs_option_rune_buffer_and_start_mana) {
	CHECK(ShouldAutoRewind(true, true, false, 5.0f, 40, 10));
	CHECK(!ShouldAutoRewind(false, true, false, 5.0f, 40, 10));   // wylaczone
	CHECK(!ShouldAutoRewind(true, false, false, 5.0f, 40, 10));   // bez runy w plecaku
	CHECK(!ShouldAutoRewind(true, true, true, 5.0f, 40, 10));     // juz przewija
	CHECK(!ShouldAutoRewind(true, true, false, 0.5f, 40, 10));    // pusty bufor (swiezy load)
	CHECK(!ShouldAutoRewind(true, true, false, 5.0f, 9, 10));     // za malo many na start
	CHECK(ShouldAutoRewind(true, true, false, 1.0f, 10, 10));     // dokladnie na granicy
}

// ---------- Globals (filtr jak zCParser::SaveGlobalVars 0x796C40) ----------
TEST(saved_global_is_nonconst_int_with_elements) {
	CHECK(IsSavedGlobal(PAR_TYPE_INT, 0, 1));
	CHECK(IsSavedGlobal(PAR_TYPE_INT, 0, 5));
	CHECK(!IsSavedGlobal(PAR_TYPE_INT, 1, 1));     // const
	CHECK(!IsSavedGlobal(PAR_TYPE_INT, 4, 1));     // pole klasy
	CHECK(!IsSavedGlobal(PAR_TYPE_INT, 16, 1));    // merged
	CHECK(!IsSavedGlobal(1, 0, 1));                // float
	CHECK(!IsSavedGlobal(3, 0, 1));                // string
	CHECK(!IsSavedGlobal(PAR_TYPE_INT, 0, 0));     // bez elementow
}
TEST(chunk_scan_splits_and_restarts_on_key_change) {
	ChunkScan s;
	int owner = 0, b = -1, e = -1;
	CHECK(!s.Done());                              // przed pierwszym Restart nic nie jest gotowe
	CHECK(s.Restart(&owner, 10));                  // pierwszy raz - od zera
	CHECK(!s.Done());
	CHECK(s.Next(4, b, e) && b == 0 && e == 4);
	CHECK(s.Next(4, b, e) && b == 4 && e == 8);
	CHECK(!s.Restart(&owner, 10));                 // ten sam parser i liczba symboli - przeglad trwa
	CHECK(s.Next(4, b, e) && b == 8 && e == 10);
	CHECK(s.Done() && !s.Next(4, b, e));
	CHECK(!s.Restart(&owner, 10) && s.Done());     // gotowa lista zostaje
	CHECK(s.Restart(&owner, 12));                  // przybylo symboli - od nowa
	CHECK(!s.Done() && s.Next(0, b, e) && b == 0 && e == 12);   // budzet 0 = reszta naraz
	CHECK(s.Done());
	int other = 0;
	CHECK(s.Restart(&other, 12) && !s.Done());     // inny parser - od nowa
}
TEST(keep_globals_list_matches_names_and_prefixes) {
	NameList n;
	n.Parse(" aniAcceleration, RUNACCELERATION;rx_speed*  SPRINT_ACTIVE,,");
	CHECK(n.Items().size() == 4);
	CHECK(n.Match("ANIACCELERATION") && n.Match("aniacceleration"));
	CHECK(n.Match("RUNACCELERATION"));
	CHECK(n.Match("RX_SPEEDPOTION") && n.Match("RX_SPEED"));    // przedrostek
	CHECK(!n.Match("RX_SPEE"));
	CHECK(n.Match("SPRINT_ACTIVE") && !n.Match("SPRINT_ACTIVE2") && !n.Match("SPRINT"));
	CHECK(!n.Match("ADDITIONALACCELERATION"));
	NameList empty;
	empty.Parse("");
	CHECK(empty.Empty() && !empty.Match("ANIACCELERATION"));
}
TEST(puppet_scale_after_restores_only_our_freeze) {
	// nasze 0 nietkniete -> tempo ze startu (zDismember: szybszy cios x1.2 wraca, zeby modul sam zdjal swoj czynnik)
	CHECK(PuppetScaleAfter(1.2f, 0.0f, 0.0f) == 1.2f);
	CHECK(PuppetScaleAfter(0.0f, 0.0f, 0.0f) == 1.0f);           // 0 na starcie = pozostalosc po zamrozeniu
	// wpis innego modulu w trakcie (NB co klatke, zDismember Yrden / podtrzymanie znaku) - jego wartosc zostaje
	CHECK(PuppetScaleAfter(1.2f, 0.0f, 1.0f) == 1.0f);
	CHECK(PuppetScaleAfter(1.0f, 0.0f, 0.6f) == 0.6f);
	// PuppetAnims=2 (bez wpisu): written = saved - bez zmian albo wartosc obca
	CHECK(PuppetScaleAfter(1.15f, 1.15f, 1.15f) == 1.15f);
	CHECK(PuppetScaleAfter(1.15f, 1.15f, 1.65f) == 1.65f);
}
TEST(chunk_scan_empty_list_is_done_at_once) {
	ChunkScan s;
	int b = -1, e = -1;
	CHECK(s.Restart(nullptr, 0));
	CHECK(s.Done() && !s.Next(5, b, e));
	CHECK(!s.Restart(nullptr, 0));
}

// ---------- Keyframes ----------
TEST(keyframes_find_at_or_before) {
	Keyframes<int> k;
	CHECK(k.FindAtOrBefore(5.0f) == -1);
	k.Push(1.0f, 10);
	k.Push(2.0f, 20);
	k.Push(3.0f, 30);
	CHECK(k.FindAtOrBefore(0.5f) == -1);
	CHECK(k.FindAtOrBefore(2.0f) == 1 && k.At(1) == 20);
	CHECK(k.FindAtOrBefore(2.9f) == 1);
	CHECK(k.FindAtOrBefore(9.0f) == 2 && k.TimeAt(2) == 3.0f);
}
TEST(keyframes_trim_keeps_anchor_and_cut_drops_future) {
	Keyframes<int> k;
	for (int i = 0; i < 5; i++)
		k.Push((float)i, i);
	k.TrimBefore(2.5f);
	CHECK(k.Size() == 3 && k.TimeAt(0) == 2.0f);   // 2 jako kotwica, 3, 4
	k.CutAfter(3.0f);
	CHECK(k.Size() == 2 && k.TimeAt(1) == 3.0f);
	k.Clear();
	CHECK(k.Size() == 0 && k.FindAtOrBefore(10.0f) == -1);
}
TEST(keyframes_trim_and_cut_recycle_buffers) {
	Keyframes<std::vector<int>> k;
	std::vector<int> big(1000, 7);
	const int* buf = big.data();
	k.Push(0.0f, std::move(big));
	k.Push(1.0f, std::vector<int>(10, 1));
	k.Push(2.0f, std::vector<int>(10, 2));
	const int* kept = k.At(1).data();
	k.TrimBefore(1.5f);                            // 0.0 odpada (1.0 zostaje jako kotwica)
	CHECK(k.Size() == 2 && k.PoolSize() == 1);
	CHECK(k.At(0).data() == kept && k.At(0)[0] == 1);   // przesuniecie nie rusza buforow zostajacych
	std::vector<int> r = k.Acquire();              // bufor z puli - bez nowej alokacji
	CHECK(r.data() == buf && r.capacity() >= 1000);
	CHECK(k.PoolSize() == 0);
	k.Release(std::move(r));                       // nieudana migawka wraca do puli
	CHECK(k.PoolSize() == 1);
	k.CutAfter(1.0f);                              // 2.0 odpada (przyszlosc po cofnieciu)
	CHECK(k.Size() == 1 && k.PoolSize() == 2 && k.At(0)[0] == 1);
	k.Clear();                                     // wczytanie gry - pamiec zwolniona
	CHECK(k.Size() == 0 && k.PoolSize() == 0);
	CHECK(k.Acquire().empty());                    // pusta pula - swiezy obiekt
}
TEST(keyframes_steady_rhythm_allocates_only_while_filling) {
	// rytm nagrywania: migawka co 1 s, bufor 3 s - po zapelnieniu kazda migawka bierze bufor z puli
	Keyframes<std::vector<int>> k;
	int fresh = 0;
	for (int i = 0; i < 20; i++) {
		if (k.PoolSize() == 0)
			fresh++;
		std::vector<int> v = k.Acquire();
		v.assign(500, i);
		k.Push((float)i, std::move(v));
		k.TrimBefore((float)i - 3.0f);
	}
	CHECK(fresh == 5);
	CHECK(k.Size() == 4 && k.At(3)[0] == 19);
}

// ---------- Cursor ----------
static CursorParams P(float speed, float mps, float minS) {
	CursorParams p;
	p.speed = speed;
	p.manaPerSecond = mps;
	p.minSeconds = minS;
	return p;
}

TEST(cursor_moves_back_at_speed) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(3.0f, 0.0f, 0.0f));
	CHECK(c.Step(1.0f, 100) == 0);
	CHECK(std::fabs(c.Time() - 27.0f) < 1e-4f && !c.Stopped());
}
TEST(cursor_stops_at_buffer_end) {
	RewindCursor c;
	c.Begin(10.0f, 8.0f, P(3.0f, 0.0f, 0.0f));
	c.Step(1.0f, 100);
	CHECK(c.Time() == 8.0f && c.Reason() == StopReason::BufferEnd);
	CHECK(c.Step(1.0f, 100) == 0 && c.Time() == 8.0f);
}
TEST(cursor_charges_whole_mana_points) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(1.0f, 5.0f, 0.0f));
	CHECK(c.Step(0.1f, 100) == 0);               // 0.5 punktu - jeszcze nic
	CHECK(c.Step(0.1f, 100) == 1);               // 1.0 punktu
	CHECK(c.Step(1.0f, 100) == 5);
}
TEST(cursor_stops_when_mana_runs_out) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(1.0f, 5.0f, 0.0f));
	int paid = c.Step(10.0f, 2);
	CHECK(paid == 2);
	CHECK(c.Reason() == StopReason::NoMana);
	CHECK(c.Rewound() > 0.55f && c.Rewound() < 0.6f);
}
TEST(release_before_min_keeps_rewinding_to_min) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(3.0f, 0.0f, 3.0f));
	c.Step(0.5f, 0);                             // 1.5 s
	c.Release();
	CHECK(!c.Stopped());
	c.Step(0.25f, 0);                            // 2.25 s
	CHECK(!c.Stopped());
	c.Step(1.0f, 0);                             // dojezdza do 3 s i staje
	CHECK(std::fabs(c.Rewound() - 3.0f) < 1e-4f && c.Reason() == StopReason::Released);
}
TEST(release_after_min_stops_in_place) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(3.0f, 0.0f, 3.0f));
	c.Step(2.0f, 0);                             // 6 s
	c.Release();
	c.Step(0.1f, 0);
	CHECK(std::fabs(c.Rewound() - 6.0f) < 1e-4f && c.Reason() == StopReason::Released);
}
TEST(release_before_min_with_short_buffer_ends_at_buffer) {
	RewindCursor c;
	c.Begin(1.0f, 0.0f, P(3.0f, 0.0f, 3.0f));
	c.Release();
	c.Step(1.0f, 0);
	CHECK(c.Time() == 0.0f && c.Reason() == StopReason::BufferEnd);
}
TEST(zero_mana_allows_less_than_one_point) {
	RewindCursor c;
	c.Begin(30.0f, 0.0f, P(1.0f, 5.0f, 0.0f));
	CHECK(c.Step(1.0f, 0) == 0);
	CHECK(c.Reason() == StopReason::NoMana && c.Rewound() < 0.2f);
}

// ---------- Walka i relacje (wrogosc po ciosie) ----------
static CombatSnap Calm(int state = 10) {
	CombatSnap c;
	c.attitude = 2; c.tmpAttitude = 2; c.npcType = 0; c.state = state;
	c.nAiv = 3; c.aiv[0] = 0; c.aiv[1] = 0; c.aiv[2] = 0;
	return c;
}
static const int HERO_TAG = 1;
static const void* HERO = &HERO_TAG;
static const std::vector<unsigned char> CRIME_MASK = { 0, 0, 1 };   // wpis 2 = przestepstwo (tylko RewindScope=1)

TEST(combat_snap_equality_ignores_att_timer) {
	CombatSnap a = Calm(), b = Calm();
	b.attTimer = 5.0f;
	CHECK(a == b);
	b.tmpAttitude = 0;
	CHECK(!(a == b));
	b = Calm(); b.aiv[2] = 7;
	CHECK(!(a == b));
	b = Calm(); b.aiv[5] = 7;   // poza nAiv - bez znaczenia
	CHECK(a == b);
}
TEST(step_track_stores_only_changes_and_finds_latest) {
	StepTrack<int> tr;
	CHECK(tr.Empty());
	tr.Push(0.0f, 1);
	tr.Push(0.1f, 1);
	tr.Push(0.2f, 2);
	tr.Push(0.3f, 2);
	CHECK(tr.Size() == 2);
	int v = 0;
	CHECK(tr.At(0.15f, v) && v == 1);
	CHECK(tr.At(0.25f, v) && v == 2);
	CHECK(tr.At(9.0f, v) && v == 2);
	CHECK(!tr.At(-1.0f, v));
	CHECK(tr.First() == 1 && tr.Last() == 2);
}
TEST(step_track_trim_keeps_anchor_and_cut_drops_future) {
	StepTrack<int> tr;
	tr.Push(0.0f, 1);
	tr.Push(1.0f, 2);
	tr.Push(2.0f, 3);
	tr.TrimBefore(1.5f);
	int v = 0;
	CHECK(tr.Size() == 2 && tr.At(1.5f, v) && v == 2);   // kotwica 1.0 zostaje
	tr.CutAfter(1.2f);
	CHECK(tr.Size() == 1 && tr.Last() == 2);
	tr.Push(1.3f, 2);                                    // stan po cofnieciu = ten sam - bez wpisu
	CHECK(tr.Size() == 1);
	tr.CutAfter(0.5f);
	CHECK(tr.Empty());
}
TEST(combat_calm_npc_unchanged_needs_nothing) {
	CombatPlan p = DecideCombat(Calm(), Calm(), false, false, HERO, CRIME_MASK, true);
	CHECK(!p.Any());
}
TEST(combat_npc_attacking_hero_now_but_calm_then_is_reset) {
	CombatSnap then = Calm(10), now = Calm(50);   // 50 = ZS_Attack
	now.enemy = HERO; now.tmpAttitude = 0; now.attitude = 1; now.aiv[0] = 2;
	CombatPlan p = DecideCombat(then, now, false, true, HERO, CRIME_MASK, false);
	CHECK(p.resetAI && p.attitude && p.enemy);
	CHECK(p.aivars.size() == 1 && p.aivars[0] == 0);
}
TEST(combat_crime_aivars_only_with_full_scope) {
	CombatSnap then = Calm(), now = Calm();
	now.aiv[2] = 3;   // AIV_NpcSawPlayerCommit
	CHECK(!DecideCombat(then, now, false, false, HERO, CRIME_MASK, false).Any());
	CombatPlan p = DecideCombat(then, now, false, false, HERO, CRIME_MASK, true);
	CHECK(!p.resetAI && p.aivars.size() == 1 && p.aivars[0] == 2);
}
TEST(combat_unknown_state_but_new_enemy_hero_is_reset) {
	// stan spoza listy (np. wlasny ZS moda), ale celuje w bohatera, a wtedy nie - AI od nowa
	CombatSnap then = Calm(10), now = Calm(77);
	now.enemy = HERO;
	CombatPlan p = DecideCombat(then, now, false, false, HERO, CRIME_MASK, true);
	CHECK(p.resetAI && p.enemy);
}
TEST(combat_same_fight_then_and_now_is_left_alone) {
	CombatSnap then = Calm(50), now = Calm(50);
	then.enemy = now.enemy = HERO;
	then.tmpAttitude = now.tmpAttitude = 0;
	CHECK(!DecideCombat(then, now, true, true, HERO, CRIME_MASK, true).Any());
}
TEST(combat_routine_change_without_hostility_keeps_ai) {
	// rutyna: stal, teraz siedzi na lawce - AI sie nie cofa (poza sprzed cofniecia)
	CombatPlan p = DecideCombat(Calm(10), Calm(11), false, false, HERO, CRIME_MASK, true);
	CHECK(!p.resetAI && !p.Any());
}
TEST(combat_hostile_state_against_other_enemy_is_reset_on_enemy_change) {
	int wolfTag = 0;
	CombatSnap then = Calm(50), now = Calm(50);
	then.enemy = &wolfTag; now.enemy = HERO;
	CombatPlan p = DecideCombat(then, now, true, true, HERO, CRIME_MASK, true);
	CHECK(p.resetAI && p.enemy);
}
TEST(combat_attitude_only_change_restores_without_reset) {
	CombatSnap then = Calm(), now = Calm();
	now.attitude = 1; now.tmpAttitude = 1;   // NB B_Attack: B_SetAttitude(ATT_ANGRY) - na stale
	now.npcType = 5;
	CombatPlan p = DecideCombat(then, now, false, false, HERO, CRIME_MASK, false);
	CHECK(!p.resetAI && p.attitude && p.npcType && !p.enemy);
}
TEST(combat_commit_action_upgrades_soft_to_hard_on_reset) {
	CHECK(CombatCommitAction(CommitAction::SoftLatest, true) == CommitAction::HardTarget);
	CHECK(CombatCommitAction(CommitAction::SoftLatest, false) == CommitAction::SoftLatest);
	CHECK(CombatCommitAction(CommitAction::Keep, true) == CommitAction::Keep);   // martwy / nieprzytomny
	CHECK(CombatCommitAction(CommitAction::HardTarget, false) == CommitAction::HardTarget);
}
TEST(name_list_splits_uppercases_and_dedupes) {
	std::vector<std::string> n = ParseNameList(" aiv_AttackReason, AIV_LastTarget;AIV_ATTACKREASON  zs_attack ");
	CHECK(n.size() == 3 && n[0] == "AIV_ATTACKREASON" && n[1] == "AIV_LASTTARGET" && n[2] == "ZS_ATTACK");
	CHECK(ParseNameList("  ,; ").empty());
}
TEST(resolve_aivars_from_parser_names) {
	auto lookup = [](const std::string& name, int& v) {
		if (name == "AIV_ATTACKREASON") { v = 9; return true; }
		if (name == "AIV_LASTTARGET") { v = 7; return true; }
		if (name == "AIV_NPCSAWPLAYERCOMMIT") { v = 1; return true; }
		if (name == "AIV_BROKEN") { v = 150; return true; }   // poza C_NPC.aivar[100]
		return false;
	};
	std::vector<std::string> unknown;
	std::vector<AivEntry> e = ResolveAivars({ "AIV_ATTACKREASON", "AIV_LASTTARGET", "AIV_MISSING", "AIV_BROKEN" },
		{ "AIV_NPCSAWPLAYERCOMMIT", "AIV_ATTACKREASON" }, lookup, &unknown);
	CHECK(e.size() == 3);
	CHECK(e[0].index == 9 && !e[0].crime && e[1].index == 7 && !e[1].crime);
	CHECK(e[2].index == 1 && e[2].crime && e[2].name == "AIV_NPCSAWPLAYERCOMMIT");   // AIV_ATTACKREASON drugi raz pominiety
	CHECK(unknown.size() == 2 && unknown[0] == "AIV_MISSING" && unknown[1] == "AIV_BROKEN");
}
TEST(resolve_aivars_caps_list) {
	std::vector<std::string> many;
	for (int i = 0; i < COMBAT_AIV_MAX + 5; i++)
		many.push_back("AIV_" + std::to_string(i));
	auto lookup = [](const std::string& name, int& v) { v = std::stoi(name.substr(4)); return true; };
	CHECK((int)ResolveAivars(many, {}, lookup, nullptr).size() == COMBAT_AIV_MAX);
}
TEST(news_to_remove_keeps_news_known_at_target) {
	int a = 0, b = 0, c = 0;
	std::vector<NewsKey> then = { { &a, 1, nullptr, nullptr }, { &b, 2, nullptr, nullptr } };
	std::vector<NewsKey> now = { { &a, 1, nullptr, nullptr }, { &c, 3, nullptr, nullptr }, { &b, 2, nullptr, nullptr } };
	std::vector<int> r = NewsToRemove(then, now);
	CHECK(r.size() == 1 && r[0] == 1);
	now[0].id = 9;   // ten sam adres, inny news (zwolniony i przydzielony od nowa) - nowszy
	r = NewsToRemove(then, now);
	CHECK(r.size() == 2 && r[0] == 0 && r[1] == 1);
	CHECK(NewsToRemove(now, now).empty());
}
TEST(attitude_names_for_log) {
	CHECK(std::string(AttitudeName(0)) == "wrogi");
	CHECK(std::string(AttitudeName(1)) == "zly");
	CHECK(std::string(AttitudeName(2)) == "neutralny");
	CHECK(std::string(AttitudeName(3)) == "przyjazny");
	CHECK(std::string(AttitudeName(9)) == "?");
}
// ---------- Sloty czarow (wspolne: plugins\_shared\ZSpellSlotsCore.h) ----------
TEST(spellslots_steam_size_is_enough_for_99) {
	auto p = zslotcore::PlanArray(true, zslotcore::PAR_TYPE_STRING, 1, true, 100, 100);
	CHECK(p.action == zslotcore::Action::Keep && p.size == 100);
}
TEST(spellslots_nb_grows_196_to_202_for_id_201) {
	auto p = zslotcore::PlanArray(true, zslotcore::PAR_TYPE_STRING, 1, true, 196, zslotcore::NeededCount(201));
	CHECK(p.action == zslotcore::Action::Grow && p.size == 202);
}
TEST(spellslots_never_shrinks) {
	auto p = zslotcore::PlanArray(true, zslotcore::PAR_TYPE_STRING, 1, true, 300, 202);
	CHECK(p.action == zslotcore::Action::Keep && p.size == 300);
}
TEST(spellslots_refuses_unsafe_symbols) {
	using namespace zslotcore;
	CHECK(PlanArray(false, PAR_TYPE_STRING, 1, true, 196, 202).action == Action::Refuse);       // brak symbolu
	CHECK(PlanArray(true, 2, 1, true, 196, 202).action == Action::Refuse);                      // int, nie string
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1 | PAR_FLAG_CLASSVAR, true, 196, 202).action == Action::Refuse);
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1, false, 196, 202).action == Action::Refuse);       // brak danych
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1, true, 1, 202).action == Action::Refuse);          // pojedynczy string
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1, true, 196, MAX_ELE + 1).action == Action::Refuse);// ele ma 12 bitow
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1, true, 196, 0).action == Action::Refuse);
	CHECK(PlanArray(true, PAR_TYPE_STRING, 1, true, 196, 202).why == nullptr);
	CHECK(PlanArray(false, PAR_TYPE_STRING, 1, true, 196, 202).why != nullptr);
}
TEST(spellslots_valid_id_range) {
	CHECK(!zslotcore::ValidId(-1));
	CHECK(zslotcore::ValidId(0) && zslotcore::ValidId(255) && zslotcore::ValidId(zslotcore::MAX_ELE - 1));
	CHECK(!zslotcore::ValidId(zslotcore::MAX_ELE));
	CHECK(zslotcore::NeededCount(99) == 100);
}
TEST(spellslots_array_names) {
	CHECK(zslotcore::ARRAY_COUNT == 3);
	CHECK(std::string(zslotcore::ArrayName(0)) == "SPELLFXINSTANCENAMES");
	CHECK(std::string(zslotcore::ArrayName(1)) == "SPELLFXANILETTERS");
	CHECK(std::string(zslotcore::ArrayName(2)) == "TXT_SPELLS");
	CHECK(zslotcore::ArrayName(3) == nullptr && zslotcore::ArrayName(-1) == nullptr);
}
TEST(spellslots_classify_slot) {
	using namespace zslotcore;
	std::vector<std::string> steam = { "PalLight", "Light", "Fireball", "Light", "Light" };   // wanilia: wolne = "Light"
	CHECK(ClassifySlot(steam, 4, "TIMEREWIND") == SlotUse::Free);     // powtorzenie nizszego wpisu = wypelniacz
	CHECK(ClassifySlot(steam, 2, "TIMEREWIND") == SlotUse::Taken);
	CHECK(ClassifySlot(steam, 9, "TIMEREWIND") == SlotUse::Free);     // poza tablica (przed wydluzeniem)
	CHECK(ClassifySlot(steam, -1, "TIMEREWIND") == SlotUse::Free);
	std::vector<std::string> nb = { "PalLight", "UnlockChest", "", "timeRewind" };
	CHECK(ClassifySlot(nb, 1, "TIMEREWIND") == SlotUse::Taken);       // NB: 99 = UnlockChest
	CHECK(ClassifySlot(nb, 2, "TIMEREWIND") == SlotUse::Free);        // pusty (wydluzona czesc)
	CHECK(ClassifySlot(nb, 3, "TIMEREWIND") == SlotUse::Ours);        // bez wzgledu na wielkosc liter
	CHECK(ClassifySlot(nb, 0, "TIMEREWIND") == SlotUse::Taken);       // pierwszy wpis nigdy nie jest wypelniaczem
}

// ---------- Consume (zwrot przedmiotow zuzytych po chwili docelowej) ----------
TEST(consume_classify_kinds) {
	CHECK(ClassifyItem(ITEM_KAT_RUNE, ITEM_MULTI | (1 << 12)) == ItemKind::Scroll);   // zwoj: RUNE + MULTI (+ MISSION w NB)
	CHECK(ClassifyItem(ITEM_KAT_RUNE, 0) == ItemKind::Other);                          // runa - nie zuzywa sie
	CHECK(ClassifyItem(ITEM_KAT_POTIONS, ITEM_MULTI) == ItemKind::Potion);
	CHECK(ClassifyItem(ITEM_KAT_FOOD, ITEM_MULTI) == ItemKind::Food);
	CHECK(ClassifyItem(ITEM_KAT_MUN, ITEM_MULTI) == ItemKind::Ammo);
	CHECK(ClassifyItem(1 << 1, 0) == ItemKind::Other);                                 // bron
	CHECK(ClassifyItem(1 << 6, ITEM_MULTI) == ItemKind::Other);                        // dokumenty
	CHECK(ClassifyItem(0, ITEM_KAT_FOOD) == ItemKind::Other);                          // kategoria tylko z mainflag
}
TEST(consume_options_and_menu_presets) {
	RestoreOptions o;
	CHECK(o.Allows(ItemKind::Scroll) && o.Allows(ItemKind::Potion) && o.Allows(ItemKind::Food) && o.Allows(ItemKind::Ammo));
	CHECK(!o.Allows(ItemKind::Other));
	CHECK(RestoreMenuIndex(o) == RESTORE_MENU_ALL);
	CHECK(RestoreFromMenu(RESTORE_MENU_SCROLLS, o) && o.scrolls && !o.potions && !o.food && !o.ammo);
	CHECK(RestoreMenuIndex(o) == RESTORE_MENU_SCROLLS);
	CHECK(RestoreFromMenu(RESTORE_MENU_OFF, o) && !o.scrolls && !o.Any());
	CHECK(RestoreMenuIndex(o) == RESTORE_MENU_OFF);
	o.potions = true;                                                 // reczny zestaw z ini
	CHECK(RestoreMenuIndex(o) == RESTORE_MENU_CUSTOM);
	CHECK(!RestoreFromMenu(RESTORE_MENU_CUSTOM, o) && o.potions && !o.scrolls);   // "z ini" nic nie zmienia
	CHECK(!RestoreFromMenu(7, o) && !RestoreFromMenu(-1, o));
}
TEST(consume_counts_and_decreases) {
	InvCounts a, b;
	a.Add(10, 3);
	a.Add(5, 1);
	a.Add(10, 2);                                                     // ten sam stos w dwoch obiektach (np. w rece)
	a.Add(7, 0);                                                      // zero - pomijane
	CHECK(a.Get(10) == 5 && a.Get(5) == 1 && a.Get(7) == 0 && a.items.size() == 2);
	CHECK(a.items[0].first == 5);                                     // posortowane po instancji
	b.Add(10, 4);
	b.Add(8, 2);                                                      // przybylo - nie jest ubytkiem
	auto d = CountDecreases(a, b);
	CHECK(d.size() == 2);
	CHECK(d[0].first == 5 && d[0].second == 1);                       // znikla cala instancja
	CHECK(d[1].first == 10 && d[1].second == 1);
	CHECK(CountDecreases(b, b).empty());
	InvCounts c = a;
	CHECK(c == a && !(c == b));
}
static HeroStats Stats(int hp, int mana) {
	HeroStats s;
	s.v[HS_HP] = hp;
	s.v[HS_MANA] = mana;
	s.v[HS_MANA + 1] = 50;   // MANAMAX
	return s;
}
TEST(consume_effect_reversible_only_hp_mana) {
	UseEffect e;
	e.Add(Stats(10, 5), Stats(60, 45));                               // mikstura leczenia + many
	CHECK(!e.lasting && e.manaGain == 40 && e.hpGain == 50);
	HeroStats perm = Stats(60, 45);
	perm.v[4] += 3;                                                   // sila (ItPo_Perm_Str)
	UseEffect p;
	p.Add(Stats(60, 45), perm);
	CHECK(p.lasting);
	HeroStats spawn = Stats(10, 5);
	spawn.v[HS_NPCS] = 1;                                             // przyzwanie (zwoj)
	UseEffect s;
	s.Add(Stats(10, 5), spawn);
	CHECK(s.lasting);
	UseEffect m;
	m.Merge(e);
	m.Merge(p);
	CHECK(m.lasting && m.manaGain == 40);
	UseEffect cost;
	cost.Add(Stats(10, 30), Stats(10, 20));                           // zwoj zabiera mane - to nie trwaly efekt
	CHECK(!cost.lasting && cost.manaGain == -10);
}
TEST(consume_judge_use_exclusions) {
	UseContext c;
	UseEffect none;
	CHECK(JudgeUse(c, none) == UseSkip::Keep);
	UseEffect lasting;
	lasting.lasting = true;
	CHECK(JudgeUse(c, lasting) == UseSkip::Lasting);
	c.inWorld = true;
	CHECK(JudgeUse(c, none) == UseSkip::InWorld);                     // lezy w swiecie - wyrzucony, nie zuzyty
	c.mob = true;
	CHECK(JudgeUse(c, none) == UseSkip::Mob);                         // gotowanie / kuznia - przedmiot przerobiony
	c.dialog = true;
	CHECK(JudgeUse(c, none) == UseSkip::Dialog);                      // handel, dawanie, skrypty dialogu
	c.cutscene = true;
	CHECK(JudgeUse(c, none) == UseSkip::Cutscene);
	c.rewinding = true;
	CHECK(JudgeUse(c, none) == UseSkip::Rewinding);
	CHECK(std::string(UseSkipName(UseSkip::Dialog)).size() > 0);
}
static UseRecord Rec(float t, int inst, int n, ItemKind k, int mana = 0, std::uintptr_t token = 0, UseSkip skip = UseSkip::Keep) {
	UseRecord r;
	r.t = t;
	r.instance = inst;
	r.amount = n;
	r.kind = k;
	r.manaGain = mana;
	r.token = token;
	r.skip = skip;
	return r;
}
static bool AllReturnable(std::uintptr_t) { return true; }
TEST(consume_refund_only_after_target_and_allowed) {
	std::vector<UseRecord> recs = { Rec(1.0f, 10, 1, ItemKind::Scroll), Rec(3.0f, 10, 1, ItemKind::Scroll),
		Rec(4.5f, 20, 1, ItemKind::Potion, 40), Rec(5.0f, 30, 1, ItemKind::Food) };
	InvCounts atT, now;
	atT.Add(10, 2);
	atT.Add(20, 1);
	atT.Add(30, 1);
	now.Add(10, 0);
	RestoreOptions o;
	o.food = false;
	std::vector<RefundSkip> skipped;
	auto r = PlanRefunds(recs, 2.0f, o, atT, now, AllReturnable, &skipped);
	CHECK(r.size() == 2);
	CHECK(r[0].instance == 10 && r[0].amount == 1 && std::fabs(r[0].firstDt - 1.0f) < 1e-4f);   // 1.0 przed celem - nie
	CHECK(r[1].instance == 20 && r[1].amount == 1 && r[1].manaRemove == 40 && r[1].kind == ItemKind::Potion);
	CHECK(skipped.size() == 1 && skipped[0].instance == 30);                                     // jedzenie wylaczone
}
TEST(consume_refund_capped_by_net_loss) {
	// mial 3 mikstury, wypil 2, kupil/podniosl 1 - zwrot najwyzej ubytku netto (bez dublowania)
	std::vector<UseRecord> recs = { Rec(3.0f, 20, 1, ItemKind::Potion, 10), Rec(4.0f, 20, 1, ItemKind::Potion, 20) };
	InvCounts atT, now;
	atT.Add(20, 3);
	now.Add(20, 2);
	std::vector<RefundSkip> skipped;
	auto r = PlanRefunds(recs, 2.0f, RestoreOptions(), atT, now, AllReturnable, &skipped);
	CHECK(r.size() == 1 && r[0].amount == 1 && r[0].manaRemove == 10);   // pierwsza chronologicznie
	CHECK(skipped.size() == 1 && skipped[0].amount == 1);
	// przedmiot zdobyty po chwili docelowej i zuzyty - w punkcie celu go nie bylo
	InvCounts none;
	auto r2 = PlanRefunds(recs, 2.0f, RestoreOptions(), none, InvCounts(), AllReturnable, nullptr);
	CHECK(r2.empty());
}
TEST(consume_refund_skips_marked_records) {
	std::vector<UseRecord> recs = { Rec(3.0f, 20, 1, ItemKind::Potion, 0, 0, UseSkip::Lasting),
		Rec(3.5f, 21, 1, ItemKind::Food, 0, 0, UseSkip::Mob) };
	InvCounts atT;
	atT.Add(20, 1);
	atT.Add(21, 1);
	std::vector<RefundSkip> skipped;
	auto r = PlanRefunds(recs, 2.0f, RestoreOptions(), atT, InvCounts(), AllReturnable, &skipped);
	CHECK(r.empty() && skipped.size() == 2);
}
TEST(consume_refund_ammo_needs_returnable_arrow) {
	std::vector<UseRecord> recs = { Rec(3.0f, 40, 1, ItemKind::Ammo, 0, 111), Rec(3.2f, 40, 1, ItemKind::Ammo, 0, 222),
		Rec(3.4f, 40, 1, ItemKind::Ammo, 0, 0) };
	InvCounts atT, now;
	atT.Add(40, 10);
	now.Add(40, 7);
	auto ret = [](std::uintptr_t tok) { return tok == 222; };          // 111 podniesiona / wbita w NPC, 0 = brak strzaly
	std::vector<RefundSkip> skipped;
	auto r = PlanRefunds(recs, 2.0f, RestoreOptions(), atT, now, ret, &skipped);
	CHECK(r.size() == 1 && r[0].amount == 1 && r[0].tokens.size() == 1 && r[0].tokens[0] == 222);
	CHECK(skipped.size() == 2);
}
TEST(consume_ledger_trim_cut_and_record_time) {
	UseLedger l;
	l.Add(Rec(1.0f, 1, 1, ItemKind::Scroll));
	l.Add(Rec(2.0f, 2, 1, ItemKind::Scroll, 0, 5));
	l.Add(Rec(3.0f, 3, 1, ItemKind::Scroll));
	std::vector<UseRecord> gone;
	l.TrimBefore(1.5f, &gone);
	CHECK(l.Records().size() == 2 && gone.size() == 1 && gone[0].instance == 1);
	gone.clear();
	l.CutAfter(2.0f, &gone);
	CHECK(l.Records().size() == 1 && l.Records()[0].instance == 2 && gone.size() == 1 && gone[0].instance == 3);
	gone.clear();
	l.Clear(&gone);
	CHECK(l.Records().empty() && gone.size() == 1 && gone[0].token == 5);
	// zuzycie w klatce, w ktorej juz policzono ekwipunek do probki: zapis tuz po niej (zwrot przy celu = ta probka)
	CHECK(UseRecordTime(4.0f, 4.0f) > 4.0f);
	CHECK(UseRecordTime(4.0f, 3.9f) == 4.0f);
}
TEST(consume_direct_use_merges_with_count_decreases) {
	// zwoj spoza listy ekwipunku (spakowany plecak / tylko w ksiedze): liczniki nic, pomiar bezposredni 1
	auto a = MergeDirectUse({}, 7, 1);
	CHECK(a.size() == 1 && a[0].first == 7 && a[0].second == 1);
	// ten sam rzut widziany w obu pomiarach - jeden ubytek, nie dwa
	auto b = MergeDirectUse({ { 3, 2 }, { 7, 1 } }, 7, 1);
	CHECK(b.size() == 2 && b[1].first == 7 && b[1].second == 1);
	// kolejnosc po instancji zachowana, brak bezposredniego zuzycia - bez zmian
	auto c = MergeDirectUse({ { 3, 2 }, { 9, 1 } }, 5, 1);
	CHECK(c.size() == 3 && c[0].first == 3 && c[1].first == 5 && c[2].first == 9);
	CHECK(MergeDirectUse({ { 3, 2 } }, 5, 0).size() == 1);
	CHECK(MergeDirectUse({ { 3, 2 } }, -1, 1).size() == 1);
}
TEST(consume_pack_string_parse) {
	auto p = ParsePackString("ITSC_FIREBOLT:5,ITPO_HEALTH_01:2:E,ITMI_GOLD:0,:3,ZLE,ITFO_APPLE:12,");
	CHECK(p.size() == 3);
	CHECK(p[0].first == "ITSC_FIREBOLT" && p[0].second == 5);
	CHECK(p[1].first == "ITPO_HEALTH_01" && p[1].second == 2);   // dodatkowe pola po ilosci pomijane
	CHECK(p[2].first == "ITFO_APPLE" && p[2].second == 12);
	CHECK(ParsePackString("").empty());
	auto q = ParsePackString("ITAR_X:1");   // bez przecinka na koncu
	CHECK(q.size() == 1 && q[0].second == 1);
}
TEST(mana_ledger_spent_after_trim_cut) {
	ManaLedger l;
	l.Add(1.0f, 10);
	l.Add(2.0f, 0);    // zero i ujemne (czar oddal mane) - bez wpisu
	l.Add(2.5f, -5);
	l.Add(3.0f, 25);
	l.Add(4.0f, 5);
	CHECK(l.Records().size() == 3);
	int n = 0;
	CHECK(l.SpentAfter(1.0f, &n) == 30 && n == 2);   // wydatek dokladnie w chwili docelowej nie wraca
	CHECK(l.SpentAfter(0.5f) == 40);
	l.CutAfter(3.0f);
	CHECK(l.SpentAfter(0.0f) == 35);
	l.TrimBefore(2.0f);
	CHECK(l.Records().size() == 1 && l.SpentAfter(0.0f) == 25);
	l.Clear();
	CHECK(l.SpentAfter(0.0f) == 0);
}
TEST(mana_refund_limits_and_potion_offset) {
	ManaRefund r = PlanManaRefund(20, 100, 30, 0);
	CHECK(r.mana == 50 && r.give == 30 && r.clipped == 0);
	r = PlanManaRefund(90, 100, 30, 0);              // limit max many
	CHECK(r.mana == 100 && r.give == 10 && r.clipped == 20);
	r = PlanManaRefund(70, 100, 30, 50);             // mikstura +50 i czar -30 po celu: mana sprzed obu (50)
	CHECK(r.mana == 50 && r.give == 30 && r.clipped == 0);
	r = PlanManaRefund(10, 100, 0, 50);              // sama mikstura: nie ponizej zera
	CHECK(r.mana == 0 && r.give == 0);
	r = PlanManaRefund(120, 100, 10, 0);             // mana ponad max (skrypt) - nie ucinamy w dol, zwrot przepada
	CHECK(r.mana == 120 && r.give == 0 && r.clipped == 10);
}

// ---------- Maska odciec zDismember (Core\Cuts.h, kanal _shared\ZCutLinkCore.h) ----------
#include <cstring>
#include "../../_shared/ZCutLinkCore.h"

static const uint32_t HEAD = 1u, ARM_L = 2u, THIGH_R = 1u << 8;

TEST(cut_flags_match_shared_link) {
	CHECK(CUT_FLAG_ALIVE == zcut::FLAG_ALIVE && CUT_FLAG_REVIVE == zcut::FLAG_REVIVE && CUT_FLAG_PREVIEW == zcut::FLAG_PREVIEW);
	CHECK(CUT_MASK_BITS == zcut::MASK_BITS);
}

TEST(cut_mask_after_follows_commit_action) {
	// wstaje (zywy wtedy, martwy teraz): glowa wraca
	CHECK(CutMaskAfter(CommitAction::HardTarget, LIFE_ALIVE, 0, HEAD) == 0);
	// martwy wtedy i teraz: zwloki jak wtedy (reka odcieta przy smierci, glowa pozniej z konsoli)
	CHECK(CutMaskAfter(CommitAction::Keep, LIFE_DEAD, ARM_L, ARM_L | HEAD) == ARM_L);
	// nieprzytomny wtedy, martwy teraz: smierc zostaje, odciecie tez
	CHECK(CutMaskAfter(CommitAction::Keep, LIFE_UNCONSCIOUS, 0, HEAD) == HEAD);
	// zywy wtedy i teraz (zdism cut na zywym): jak wtedy
	CHECK(CutMaskAfter(CommitAction::SoftLatest, LIFE_ALIVE, 0, ARM_L) == 0);
	// martwy wtedy, zywy teraz (wskrzeszony skryptem): bez zmian
	CHECK(CutMaskAfter(CommitAction::SoftLatest, LIFE_DEAD, HEAD, 0) == 0);
	// HardTarget z "martwy wtedy" (bohater / AI od nowa) - postac i tak wstaje: bez zmian zamiast bezglowego
	CHECK(CutMaskAfter(CommitAction::HardTarget, LIFE_DEAD, HEAD, 0) == 0);
	CHECK(CutMaskAfter(CommitAction::HardTarget, LIFE_ALIVE, 0xFFFFFFFFu, 0) == CUT_MASK_BITS);
}

TEST(cut_commit_call_revive_and_skip) {
	// wskrzeszenie z glowa: wywolanie z ALIVE|REVIVE (przed ResetPos)
	CutCall c = CutCommitCall(CommitAction::HardTarget, LIFE_ALIVE, 0, HEAD, HEAD, true, false);
	CHECK(c.send && c.mask == 0 && c.flags == (CUT_FLAG_ALIVE | CUT_FLAG_REVIVE));
	// wskrzeszenie bez odciec (ragdoll / nakladka ofiary): i tak wywolanie
	c = CutCommitCall(CommitAction::HardTarget, LIFE_ALIVE, 0, 0, 0, true, false);
	CHECK(c.send && c.mask == 0 && (c.flags & CUT_FLAG_REVIVE));
	// koniec nokautu (oszolomienie): REVIVE
	c = CutCommitCall(CommitAction::HardTarget, LIFE_ALIVE, 0, 0, 0, false, true);
	CHECK(c.send && c.flags == (CUT_FLAG_ALIVE | CUT_FLAG_REVIVE));
	// zywy, bez zmian: nic
	c = CutCommitCall(CommitAction::SoftLatest, LIFE_ALIVE, 0, 0, 0, false, false);
	CHECK(!c.send);
	c = CutCommitCall(CommitAction::HardTarget, LIFE_ALIVE, 0, 0, 0, false, false);   // przesuniety daleko, zywy
	CHECK(!c.send && c.flags == 0);
	// podglad juz przywrocil glowe (live 0), Commit w tej samej chwili: tylko REVIVE
	c = CutCommitCall(CommitAction::HardTarget, LIFE_ALIVE, 0, HEAD, 0, true, false);
	CHECK(c.send && c.mask == 0);
	// martwy wtedy i teraz, maska ta sama: nic
	c = CutCommitCall(CommitAction::Keep, LIFE_DEAD, HEAD, HEAD, HEAD, true, false);
	CHECK(!c.send);
}

TEST(cut_view_watch_only_relevant_actors) {
	StepTrack<uint32_t> tr;
	CHECK(!CutsTrackHasCut(tr));
	tr.Push(0.0f, 0u);
	tr.Push(1.0f, 0u);
	CHECK(!CutsTrackHasCut(tr) && tr.Size() == 1);
	tr.Push(2.0f, HEAD);
	CHECK(CutsTrackHasCut(tr));
	StepTrack<uint32_t> loaded;   // zwloki z zapisu: pierwsza probka juz z maska
	loaded.Push(0.0f, ARM_L);
	CHECK(CutsTrackHasCut(loaded));
	CHECK(!CutViewBegin(0, false, false, false).watch);   // zywy, caly, bez odciec w nagraniu - bez liczenia w klatkach
	CHECK(CutViewBegin(0, true, false, false).watch);     // martwy: ragdoll do porzucenia
	CHECK(CutViewBegin(0, false, true, false).watch);
	CHECK(CutViewBegin(HEAD, false, false, false).watch);
	CHECK(CutViewBegin(0, false, false, true).watch);
}

// Przebieg: zywy do t=5, w t=5 smierc z odcieciem glowy (ta sama probka), przewijanie od t=8 w dol.
TEST(cut_preview_restores_head_once_when_cursor_passes_death) {
	Track life;
	Sample a = S(0.0f, 0.0f, 100), d = S(5.0f, 0.0f, 0);
	d.life = LIFE_DEAD;
	life.Push(a);
	life.Push(S(4.9f, 10.0f, 100));
	life.Push(d);
	StepTrack<uint32_t> cuts;
	cuts.Push(0.0f, 0u);
	cuts.Push(5.0f, HEAD);
	Sample latest;
	CHECK(life.At(8.0f, latest));
	CutView v = CutViewBegin(HEAD, true, false, CutsTrackHasCut(cuts));
	int sends = 0;
	uint32_t lastMask = HEAD, lastFlags = 0;
	for (float c = 8.0f; c >= 3.0f; c -= 0.25f) {
		Sample s;
		CHECK(life.At(c, s));
		uint32_t m = 0;
		cuts.At(c, m);
		CommitAction act = DecideCommit(s, latest, true, false, false, 50.0f);
		CutCall call = CutPreviewStep(v, act, s.life, m);
		if (call.send) {
			sends++;
			lastMask = call.mask;
			lastFlags = call.flags;
			CHECK(c < 5.0f);   // po smierci (t >= 5) zwloki bez zmian
		}
	}
	CHECK(sends == 1 && lastMask == 0 && lastFlags == (CUT_FLAG_PREVIEW | CUT_FLAG_ALIVE));
	CHECK(v.shown == 0 && v.alive);
	// Commit w t=3: podglad juz przywrocil (live 0) - zostaje REVIVE przed ResetPos
	Sample target;
	life.At(3.0f, target);
	uint32_t tm = 0;
	cuts.At(3.0f, tm);
	CommitAction act = DecideCommit(target, latest, true, false, false, 50.0f);
	CHECK(act == CommitAction::HardTarget);
	CutCall c = CutCommitCall(act, target.life, tm, v.start, v.shown, v.dead, v.knocked);
	CHECK(c.send && c.mask == 0 && c.flags == (CUT_FLAG_ALIVE | CUT_FLAG_REVIVE));
}

// Nokaut w t=3, dobicie z odcieciem w t=5: cofniecie do chwili nokautu zostawia smierc i odciecie (Keep),
// dopiero chwila sprzed nokautu (zywy) przywraca glowe.
TEST(cut_preview_knocked_then_killed_keeps_cut_until_alive) {
	Track life;
	Sample a = S(0.0f, 0.0f, 100), k = S(3.0f, 0.0f, 1), d = S(5.0f, 0.0f, 0);
	k.life = LIFE_UNCONSCIOUS;
	d.life = LIFE_DEAD;
	life.Push(a);
	life.Push(S(2.9f, 5.0f, 100));
	life.Push(k);
	life.Push(d);
	StepTrack<uint32_t> cuts;
	cuts.Push(0.0f, 0u);
	cuts.Push(5.0f, HEAD);
	Sample latest;
	life.At(9.0f, latest);
	CutView v = CutViewBegin(HEAD, true, false, true);
	auto step = [&](float c) {
		Sample s;
		life.At(c, s);
		uint32_t m = 0;
		cuts.At(c, m);
		return CutPreviewStep(v, DecideCommit(s, latest, true, false, false, 50.0f), s.life, m);
	};
	CHECK(!step(4.0f).send);   // nieprzytomny wtedy: Keep, glowa dalej odcieta
	CHECK(v.shown == HEAD && !v.alive);
	CutCall c = step(2.0f);   // zywy wtedy: glowa wraca, ragdoll porzucony
	CHECK(c.send && c.mask == 0 && (c.flags & CUT_FLAG_ALIVE));
	// Commit w chwili nokautu (gdyby kursor zatrzymal sie w 4): Keep z maska startowa - bez wywolania
	Sample t4;
	life.At(4.0f, t4);
	CutCall k4 = CutCommitCall(DecideCommit(t4, latest, true, false, false, 50.0f), t4.life, 0, HEAD, HEAD, true, false);
	CHECK(!k4.send);
}

TEST(cut_preview_ragdoll_drop_without_cuts) {
	CutView v = CutViewBegin(0, true, false, false);
	Sample alive = S(1.0f, 0.0f, 100), dead = S(2.0f, 0.0f, 0);
	dead.life = LIFE_DEAD;
	CHECK(!CutPreviewStep(v, CommitAction::Keep, LIFE_DEAD, 0).send);
	CutCall c = CutPreviewStep(v, DecideCommit(alive, dead, true, false, false, 50.0f), LIFE_ALIVE, 0);
	CHECK(c.send && c.mask == 0 && c.flags == (CUT_FLAG_PREVIEW | CUT_FLAG_ALIVE));
	CHECK(!CutPreviewStep(v, CommitAction::HardTarget, LIFE_ALIVE, 0).send);   // raz
	CutView idle;   // bez watch - nic
	CHECK(!CutPreviewStep(idle, CommitAction::HardTarget, LIFE_ALIVE, HEAD).send);
}

int main() {
	for (auto& t : Tests()) {
		int before = g_fail;
		t.fn();
		printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", t.name);
	}
	printf("\n%d sprawdzen OK, %d bledow\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
