// Testy logiki zDismember niezaleznej od silnika (Core\). Budowanie: tests\build_tests.cmd
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "Core/Text.h"
#include "Core/PartMask.h"
#include "Core/Catalog.h"
#include "Core/PartPick.h"
#include "Core/Ballistics.h"
#include "Core/Moves.h"
#include "Core/Ragdoll.h"
#include "Core/Overlays.h"
#include "Core/Weapons.h"
#include "Core/Recovery.h"

using namespace zdcore;

static int g_pass = 0, g_fail = 0;
#define CHECK(e) do { if (e) g_pass++; else { g_fail++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #e); } } while (0)

struct TestReg { const char* name; void (*fn)(); };
static std::vector<TestReg>& Tests() { static std::vector<TestReg> t; return t; }
struct Registrar { Registrar(const char* n, void (*f)()) { Tests().push_back({ n, f }); } };
#define TEST(name) static void name(); static Registrar reg_##name(#name, name); static void name()

// ---------- Text ----------
TEST(split_args_uppercases_and_drops_extra_spaces) {
	auto a = SplitArgs("zdism  cut   head");
	CHECK(a.size() == 3);
	CHECK(a.size() == 3 && a[0] == "ZDISM" && a[1] == "CUT" && a[2] == "HEAD");
}
TEST(split_args_empty_line_gives_no_args) {
	CHECK(SplitArgs("").empty());
	CHECK(SplitArgs("   ").empty());
}
TEST(parse_int_list_reads_triple) {
	int v[3] = { 0, 0, 0 };
	CHECK(ParseIntList("50,35,15", v, 3));
	CHECK(v[0] == 50 && v[1] == 35 && v[2] == 15);
}
TEST(parse_int_list_rejects_garbage_without_touching_output) {
	int v[3] = { 1, 2, 3 };
	CHECK(!ParseIntList("50,x,1", v, 3));
	CHECK(!ParseIntList("50,35", v, 3));
	CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3);
}

// ---------- PartMask ----------
TEST(mask_bits_follow_spec_layout) {
	CHECK(MaskSet(0, Part::Head) == 1u);
	CHECK(MaskSet(0, Part::ArmL) == (1u << 1));
	CHECK(MaskSet(0, Part::ArmR) == (1u << 2));
	CHECK(MaskSet(0, Part::UpperArmL) == (1u << 3));
	CHECK(MaskSet(0, Part::ThighR) == (1u << 8));
}
TEST(mask_round_trip_every_part_and_no_crosstalk) {
	for (int i = 0; i < (int)Part::NoPart; i++) {
		uint32_t m = MaskSet(0, (Part)i);
		CHECK(MaskHas(m, (Part)i));
		for (int j = 0; j < (int)Part::NoPart; j++)
			if (j != i)
				CHECK(!MaskHas(m, (Part)j));
		CHECK(MaskSet(m, (Part)i) == m);
	}
	CHECK(!MaskHas(0xFFFFFFFFu, Part::NoPart));
}
TEST(part_names_parse) {
	CHECK(PartFromName("HEAD") == Part::Head);
	CHECK(PartFromName("arm_l") == Part::ArmL);
	CHECK(PartFromName("UPPERARM_R") == Part::UpperArmR);
	CHECK(PartFromName("LEG_L") == Part::LegL);
	CHECK(PartFromName("THIGH_R") == Part::ThighR);
	CHECK(PartFromName("TAIL") == Part::NoPart);
}

// ---------- Catalog ----------
static const char* kCatalog =
	"; komentarz\n"
	"[FIN_2H_SLASH]\n"
	"weapon=2H\n"
	"victim=HUMAN,ORC\n"
	"pose=STANDING,DAZED\n"
	"overlay=HUMANS_ZDF_2H_SLASH   ; nakladka\n"
	"heroAni=T_2HSFINISH\n"
	"cut=HEAD\n"
	"cutTime=0.92\n"
	"victimOffset=12,0,118\n"
	"inplace=HUMANS_ZDF_2H_SLASH_IP\n"
	"inplaceOffset=10,0,40\n"
	"slowmo=0.35,-0.25,0.35\n"
	"weight=10\n"
	"\n"
	"[FIN_NO_OVERLAY]\n"
	"weapon=1H\nvictim=HUMAN\npose=STANDING\nheroAni=T_1HSFINISH\ncut=HEAD\n"
	"[FIN_BAD_CUT]\n"
	"weapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=X\nheroAni=T_1HSFINISH\ncut=TAIL\n"
	"[FIN_1H]\n"
	"weapon=1H\nvictim=MONSTER\npose=STANDING\noverlay=HUMANS_ZDF_1H\nheroAni=T_1HSFINISH\ncut=HEAD\nweight=30\n";

TEST(catalog_parses_valid_entry_fields) {
	std::vector<std::string> err;
	auto defs = ParseCatalog(kCatalog, err);
	const FinisherDef* d = nullptr;
	for (auto& x : defs)
		if (x.id == "FIN_2H_SLASH")
			d = &x;
	CHECK(d != nullptr);
	if (!d)
		return;
	CHECK(d->weapon == WEAPON_2H);
	CHECK(d->victimGroups == (GROUP_HUMAN | GROUP_ORC));
	CHECK(d->poses == (POSE_STANDING | POSE_DAZED));
	CHECK(d->overlay == "HUMANS_ZDF_2H_SLASH");
	CHECK(d->heroAni == "T_2HSFINISH");
	CHECK(d->cut == Part::Head);
	CHECK(std::fabs(d->cutTime - 0.92f) < 1e-4f);
	CHECK(d->offset[0] == 12.0f && d->offset[2] == 118.0f);
	CHECK(d->inplace == "HUMANS_ZDF_2H_SLASH_IP" && d->inplaceOffset[2] == 40.0f);
	CHECK(std::fabs(d->slowmo - 0.35f) < 1e-4f && std::fabs(d->slowFrom + 0.25f) < 1e-4f);
	CHECK(d->weight == 10);
}
TEST(catalog_skips_entries_with_errors_and_reports_them) {
	std::vector<std::string> err;
	auto defs = ParseCatalog(kCatalog, err);
	CHECK(defs.size() == 2);
	CHECK(err.size() == 2);
	bool sawOverlay = false, sawCut = false;
	for (auto& e : err) {
		if (e.find("FIN_NO_OVERLAY") != std::string::npos) sawOverlay = true;
		if (e.find("FIN_BAD_CUT") != std::string::npos) sawCut = true;
	}
	CHECK(sawOverlay && sawCut);
}
TEST(catalog_accepts_utf8_bom_before_first_section) {
	std::vector<std::string> err;
	std::string text = std::string("\xEF\xBB\xBF") + "[FIN_A]\nweapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=X\nheroAni=T_1HSFINISH\ncut=HEAD\n";
	auto defs = ParseCatalog(text, err);
	CHECK(defs.size() == 1 && defs[0].id == "FIN_A");
	CHECK(err.empty());
}
TEST(catalog_reports_lines_outside_any_section) {
	std::vector<std::string> err;
	auto defs = ParseCatalog("weapon=1H\n[FIN_A]\nweapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=X\nheroAni=T_1HSFINISH\ncut=HEAD\n", err);
	CHECK(defs.size() == 1);
	CHECK(err.size() == 1);
}
TEST(catalog_parses_paired_trajectories) {
	std::vector<std::string> err;
	auto defs = ParseCatalog(
		"[PAIR]\nweapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=H\nheroAni=T_1HSFINISH\ncut=HEAD\n"
		"victimOverlay=V\nvictimAni=T_STAND_2_WOUNDED\ntrajFps=25\nheroTraj=0,0 10,5 20,10\nvictimTraj=0,120 0,110 5,100\n", err);
	CHECK(err.empty());
	CHECK(defs.size() == 1);
	if (defs.size() != 1)
		return;
	CHECK(defs[0].paired());
	CHECK(defs[0].trajFps == 25.0f);
	CHECK(defs[0].heroTraj.size() == 6 && defs[0].heroTraj[2] == 10.0f && defs[0].heroTraj[3] == 5.0f);
	CHECK(defs[0].victimTraj.size() == 6 && defs[0].victimTraj[1] == 120.0f);
}
TEST(catalog_accepts_cut_none_with_wound_node) {
	// pchniecie w klatke: zabija bez odciecia, krew z rany w podanym wezle
	std::vector<std::string> err;
	auto defs = ParseCatalog("[STAB]\nweapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=H\nheroAni=T_1HSFINISH\ncut=none\n"
		"wound=BIP01 SPINE2\n", err);
	CHECK(err.empty());
	CHECK(defs.size() == 1);
	if (defs.size() != 1)
		return;
	CHECK(defs[0].cut == Part::NoPart);
	CHECK(defs[0].wound == "BIP01 SPINE2");
}
TEST(catalog_rejects_bad_trajectory) {
	std::vector<std::string> err;
	auto defs = ParseCatalog("[PAIR]\nweapon=1H\nvictim=HUMAN\npose=STANDING\noverlay=H\nheroAni=T_1HSFINISH\ncut=HEAD\nheroTraj=0,0 10\n", err);
	CHECK(defs.empty());
	CHECK(err.size() == 1);
}
TEST(catalog_unpaired_entry_is_not_paired) {
	std::vector<std::string> err;
	auto defs = ParseCatalog(kCatalog, err);
	CHECK(!defs.empty() && !defs[0].paired());
}
TEST(finisher_slowmo_setting_scales_catalog_factor) {
	// FinisherSlowMo: 0 wyl. = bez spowolnienia, 1 lekkie = polowa spowolnienia z katalogu, 2 = jak w katalogu
	CHECK(FinisherSlowFactor(0.35f, SLOWMO_OFF, false, false) == 1.0f);
	CHECK(std::fabs(FinisherSlowFactor(0.35f, SLOWMO_LIGHT, false, false) - 0.675f) < 1e-4f);
	CHECK(FinisherSlowFactor(0.35f, SLOWMO_FULL, false, false) == 0.35f);
	CHECK(FinisherSlowFactor(1.0f, SLOWMO_LIGHT, false, false) == 1.0f);   // wpis bez spowolnienia zostaje bez
	CHECK(FinisherSlowFactor(0.35f, 7, false, false) == 0.35f);            // tryb spoza listy = jak w katalogu
	// FinisherSlowMoLastEnemy: inni walczacy wrogowie w poblizu = bez spowolnienia
	CHECK(FinisherSlowFactor(0.35f, SLOWMO_FULL, true, true) == 1.0f);
	CHECK(FinisherSlowFactor(0.35f, SLOWMO_LIGHT, true, false) == FinisherSlowFactor(0.35f, SLOWMO_LIGHT, false, false));
	CHECK(FinisherSlowFactor(0.35f, SLOWMO_FULL, false, true) == 0.35f);
}
TEST(trajectory_sampling_interpolates_and_clamps) {
	std::vector<float> xz = { 0, 0, 10, 20, 30, 40 };
	float x, z;
	SampleTraj(xz, 10.0f, 0.05f, x, z);
	CHECK(std::fabs(x - 5.0f) < 1e-4f && std::fabs(z - 10.0f) < 1e-4f);
	SampleTraj(xz, 10.0f, 0.2f, x, z);
	CHECK(x == 30.0f && z == 40.0f);
	SampleTraj(xz, 10.0f, 5.0f, x, z);
	CHECK(x == 30.0f && z == 40.0f);
	SampleTraj(xz, 10.0f, -1.0f, x, z);
	CHECK(x == 0.0f && z == 0.0f);
	std::vector<float> none;
	SampleTraj(none, 10.0f, 0.5f, x, z);
	CHECK(x == 0.0f && z == 0.0f);
}
TEST(pick_finisher_filters_by_weapon_group_pose) {
	std::vector<std::string> err;
	auto defs = ParseCatalog(kCatalog, err);
	const FinisherDef* a = PickFinisher(defs, WEAPON_2H, GROUP_ORC, POSE_DAZED, 0);
	CHECK(a && a->id == "FIN_2H_SLASH");
	CHECK(PickFinisher(defs, WEAPON_2H, GROUP_MONSTER, POSE_STANDING, 0) == nullptr);
	CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, 0) == nullptr);
	const FinisherDef* b = PickFinisher(defs, WEAPON_1H, GROUP_MONSTER, POSE_STANDING, 7);
	CHECK(b && b->id == "FIN_1H");
}
TEST(pick_finisher_uses_weights) {
	std::vector<FinisherDef> defs(2);
	defs[0].id = "A"; defs[0].weapon = WEAPON_1H; defs[0].victimGroups = GROUP_HUMAN; defs[0].poses = POSE_STANDING; defs[0].weight = 10;
	defs[1] = defs[0]; defs[1].id = "B"; defs[1].weight = 30;
	CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, 5)->id == "A");
	CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, 15)->id == "B");
	CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, 45)->id == "A");
}
TEST(pick_finisher_skips_excluded) {
	// finisher, ktory nie zmiescil sie w terenie, nie jest losowany ponownie
	std::vector<FinisherDef> defs(2);
	defs[0].id = "A"; defs[0].weapon = WEAPON_1H; defs[0].victimGroups = GROUP_HUMAN; defs[0].poses = POSE_STANDING; defs[0].weight = 10;
	defs[1] = defs[0]; defs[1].id = "B"; defs[1].weight = 30;
	std::vector<const FinisherDef*> skip = { &defs[1] };
	for (uint32_t r = 0; r < 40; r++)
		CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, r, &skip)->id == "A");
	skip.push_back(&defs[0]);
	CHECK(PickFinisher(defs, WEAPON_1H, GROUP_HUMAN, POSE_STANDING, 7, &skip) == nullptr);
}

// ---------- PartPick ----------
static const int W_HEAD[3] = { 100, 0, 0 }, W_ARM[3] = { 0, 100, 0 }, W_LEG[3] = { 0, 0, 100 }, W_NONE[3] = { 0, 0, 0 };
static const uint32_t ALL = 0x1FF;
TEST(part_pick_nothing_available_gives_none) {
	CHECK(PickPart(-1, W_HEAD, W_HEAD, 3, 0) == Part::NoPart);
}
TEST(part_pick_follows_weights_and_swing_side) {
	CHECK(PickPart(-1, W_HEAD, W_NONE, 3, ALL) == Part::Head);
	CHECK(PickPart(-1, W_ARM, W_NONE, 3, ALL) == Part::ArmL);
	CHECK(PickPart(+1, W_ARM, W_NONE, 3, ALL) == Part::ArmR);
	CHECK(PickPart(-1, W_LEG, W_NONE, 3, ALL) == Part::LegL);
}
TEST(part_pick_falls_back_to_other_side_and_skips_unavailable_categories) {
	uint32_t onlyArmR = MaskSet(0, Part::ArmR);
	CHECK(PickPart(-1, W_ARM, W_NONE, 3, onlyArmR) == Part::ArmR);
	CHECK(PickPart(-1, W_HEAD, W_NONE, 3, onlyArmR) == Part::NoPart);
	const int mixed[3] = { 50, 50, 0 };
	CHECK(PickPart(-1, mixed, W_NONE, 3, onlyArmR) == Part::ArmR);
}
TEST(part_pick_thrust_uses_thrust_weights) {
	CHECK(PickPart(0, W_ARM, W_NONE, 3, ALL) == Part::NoPart);
	CHECK(PickPart(0, W_NONE, W_HEAD, 3, ALL) == Part::Head);
}

// ---------- Moves (uniki / przewroty) ----------
static const char* kMoves =
	"; ruchy W3\n"
	"[DODGE_L]\noverlay=HUMANS_ZDF_MV_DODGE_L\ntrajFps=25\ntraj=0,0 -50,0 -100,5\niframes=0.05,0.4\ncancel=0.8\n"
	"[ROLL_B]\noverlay=HUMANS_ZDF_MV_ROLL_B\ntraj=0,0 0,-150\niframes=0.1,0.7\ncancel=1.1\n"
	"[ROLL_X]\ntraj=0,0 1,1\n";

TEST(moves_parse_fields_and_skip_incomplete) {
	std::vector<std::string> err;
	auto defs = ParseMoves(kMoves, err);
	CHECK(defs.size() == 2);
	CHECK(err.size() == 1 && err[0].find("ROLL_X") != std::string::npos);
	if (defs.size() != 2)
		return;
	const MoveDef& d = defs[0];
	CHECK(d.id == "DODGE_L" && d.overlay == "HUMANS_ZDF_MV_DODGE_L");
	CHECK(d.trajFps == 25.0f && d.traj.size() == 6 && d.traj[4] == -100.0f && d.traj[5] == 5.0f);
	CHECK(std::fabs(d.iframeFrom - 0.05f) < 1e-5f && std::fabs(d.iframeTo - 0.4f) < 1e-5f);
	CHECK(std::fabs(d.cancel - 0.8f) < 1e-5f);
}
TEST(moves_parse_counter_hit_time) {
	// kontra: hit = chwila ciecia (s) - wtedy plugin zadaje obrazenia; bez hit = -1 (zwykly ruch)
	std::vector<std::string> err;
	auto defs = ParseMoves("[COUNTER_L]\noverlay=HUMANS_ZDF_MV_COUNTER_L\ntraj=0,0 0,10\nhit=0.72\n[DODGE_B]\noverlay=X\ntraj=0,0 0,-10\n", err);
	CHECK(err.empty() && defs.size() == 2);
	if (defs.size() != 2)
		return;
	CHECK(std::fabs(defs[0].hitTime - 0.72f) < 1e-5f);
	CHECK(defs[1].hitTime < 0.0f);
	CHECK(FindMove(defs, "COUNTER", MOVE_LEFT) == &defs[0]);
}
TEST(moves_find_by_kind_and_direction) {
	std::vector<std::string> err;
	auto defs = ParseMoves(kMoves, err);
	CHECK(FindMove(defs, "DODGE", MOVE_LEFT) == &defs[0]);
	CHECK(FindMove(defs, "ROLL", MOVE_BACK) == &defs[1]);
	CHECK(FindMove(defs, "DODGE", MOVE_RIGHT) == nullptr);
}
TEST(moves_direction_from_keys) {
	// bok ma pierwszenstwo, potem tyl i przod; nic albo lewo+prawo naraz = tyl (jak unik w W3)
	CHECK(MoveDirFromKeys(true, false, true, false) == MOVE_LEFT);
	CHECK(MoveDirFromKeys(false, true, false, true) == MOVE_RIGHT);
	CHECK(MoveDirFromKeys(false, false, true, false) == MOVE_FORWARD);
	CHECK(MoveDirFromKeys(false, false, false, true) == MOVE_BACK);
	CHECK(MoveDirFromKeys(false, false, false, false) == MOVE_BACK);
	CHECK(MoveDirFromKeys(true, true, false, false) == MOVE_BACK);
}
TEST(roll_on_jump_key_needs_side_or_back_direction) {
	// klawisz przewrotu = klawisz skoku (Alt): sam albo z W = skok gry, z bokiem / tylem = przewrot
	CHECK(RollDirFromKeys(true, false, false, false, false) == -1);
	CHECK(RollDirFromKeys(true, false, false, true, false) == -1);
	CHECK(RollDirFromKeys(true, true, false, false, false) == MOVE_LEFT);
	CHECK(RollDirFromKeys(true, false, true, true, false) == MOVE_RIGHT);
	CHECK(RollDirFromKeys(true, false, false, false, true) == MOVE_BACK);
	// osobny klawisz przewrotu: jak unik (nic = w tyl, W = w przod)
	CHECK(RollDirFromKeys(false, false, false, false, false) == MOVE_BACK);
	CHECK(RollDirFromKeys(false, false, false, true, false) == MOVE_FORWARD);
}
TEST(counter_hit_fires_once_when_hit_time_is_crossed) {
	MoveDef d;
	d.hitTime = 0.6f;
	CHECK(!MoveHitDue(d, 0.5f, 0.59f));
	CHECK(MoveHitDue(d, 0.59f, 0.61f));
	CHECK(MoveHitDue(d, 0.5f, 0.6f));
	CHECK(!MoveHitDue(d, 0.6f, 0.7f));   // juz zadany w poprzedniej klatce
	MoveDef dodge;                       // bez hit= nigdy
	CHECK(!MoveHitDue(dodge, -1.0f, 5.0f));
}
TEST(counter_reach_needs_enemy_close_and_in_front) {
	// bohater w (0,0) patrzy w +z; zasieg 300 cm, przod = cos kata > 0.3 (do ok. 72 st. od osi)
	CHECK(CounterInReach(0, 0, 0, 1, 0, 150, 300.0f, 0.3f));
	CHECK(CounterInReach(0, 0, 0, 1, 100, 100, 300.0f, 0.3f));    // 45 st.
	CHECK(!CounterInReach(0, 0, 0, 1, 0, 350, 300.0f, 0.3f));     // za daleko
	CHECK(!CounterInReach(0, 0, 0, 1, 150, 0, 300.0f, 0.3f));     // z boku
	CHECK(!CounterInReach(0, 0, 0, 1, 0, -100, 300.0f, 0.3f));    // z tylu
	CHECK(CounterInReach(0, 0, 0, 1, 0, 0, 300.0f, 0.3f));        // wtulony w bohatera - zawsze w zasiegu
}
TEST(moves_invulnerable_only_in_window) {
	MoveDef d;
	d.iframeFrom = 0.1f;
	d.iframeTo = 0.4f;
	CHECK(!MoveInvulnerable(d, 0.05f));
	CHECK(MoveInvulnerable(d, 0.1f) && MoveInvulnerable(d, 0.3f));
	CHECK(!MoveInvulnerable(d, 0.41f));
}

static bool ReadText(const char* path, std::string& out) {
	FILE* f = nullptr;
	if (fopen_s(&f, path, "rb") != 0 || !f)
		return false;
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		out.append(buf, n);
	fclose(f);
	return true;
}
TEST(real_data_files_parse_without_errors) {
	// pliki z paczek (generowane przez pipeline) musza przejsc parser z gry bez bledow
	std::string text;
	std::vector<std::string> err;
	CHECK(ReadText("..\\data\\System\\zDismember\\finishers.ini", text));
	CHECK(ParseCatalog(text, err).size() == 2 && err.empty());
	text.clear();
	if (ReadText("..\\data_w3\\System\\zDismember\\finishers_w3.ini", text)) {
		auto defs = ParseCatalog(text, err);
		CHECK(defs.size() == 46 && err.empty());   // 17 par + 6 wariantow dla oszolomionych, x 1H/2H
		int stabs = 0, dazedStabs = 0;
		for (auto& d : defs) {
			stabs += d.cut == Part::NoPart ? 1 : 0;
			dazedStabs += d.cut == Part::NoPart && (d.poses & POSE_DAZED) ? 1 : 0;
		}
		// pchniecia: w klatke W3_03, W3_04, W3_05, W3_06 (+ warianty _D 05/06), w gardlo W3_08 x 1H/2H (reszta odcina)
		CHECK(stabs == 14);
		CHECK(dazedStabs == 4);   // W3_05_D, W3_06_D x 1H/2H - egzekucja oszolomionego rapierem
	}
	text.clear();
	if (ReadText("..\\data_w3\\System\\zDismember\\moves_w3.ini", text)) {
		auto moves = ParseMoves(text, err);
		// 4 uniki, 4 przewroty, 4 kontry, 6 atakow z wyskoku (po 3 warianty 1H / 2H), 4 ataki w biegu (po 2)
		CHECK(moves.size() == 22 && err.empty());
		// atak z wyskoku: cios po wyskoku, przed koncem, zasieg ostrza, bez nietykalnosci
		for (bool twoHanded : { false, true }) {
			const MoveDef* j = FindJumpAttack(moves, twoHanded);
			CHECK(j && j->id == (twoHanded ? "JUMPATK2H_F" : "JUMPATK_F") && j->hitTime > 0.3f && j->hitTime < j->cancel &&
				j->reach > 60.0f && !MoveInvulnerable(*j, j->hitTime));
			auto jumps = JumpAttackVariants(moves, twoHanded);
			CHECK(jumps.size() == 3);
			for (const MoveDef* v : jumps)
				CHECK(v->hitTime > 0.25f && v->hitTime < v->cancel && v->reach > 50.0f && !v->overlayW3.empty() &&
					!MoveInvulnerable(*v, v->hitTime));
			// atak w biegu: wariant z daleka (_F) i z bliska (_F2) - dluzszy wypad do ciosu w pierwszym
			auto runs = RunAttackVariants(moves, twoHanded);
			CHECK(runs.size() == 2 && runs[0]->id == (twoHanded ? "RUNATK2H_F" : "RUNATK_F"));
			for (const MoveDef* v : runs)
				CHECK(IsRunAttack(*v) && v->hitTime > 0.3f && v->hitTime < v->cancel && v->reach > 50.0f &&
					!v->overlayW3.empty() && !MoveInvulnerable(*v, v->hitTime));
			if (runs.size() == 2) {
				float farPos, farNeg, nearPos, nearNeg;
				TrajForward(runs[0]->traj, runs[0]->trajFps, 0.0f, runs[0]->hitTime, farPos, farNeg);
				TrajForward(runs[1]->traj, runs[1]->trajFps, 0.0f, runs[1]->hitTime, nearPos, nearNeg);
				CHECK(farPos > 1.6f * nearPos);
			}
		}
		for (const char* kind : { "DODGE", "ROLL" })
			for (MoveDir d : { MOVE_BACK, MOVE_FORWARD, MOVE_LEFT, MOVE_RIGHT })
				CHECK(FindMove(moves, kind, d) != nullptr);
		// kontry: cios po odbiciu, nietykalnosc do ciosu, powrot po ciosie
		for (MoveDir d : { MOVE_LEFT, MOVE_RIGHT }) {
			const MoveDef* c = FindMove(moves, "COUNTER", d);
			CHECK(c && c->hitTime > 0.3f && c->hitTime < c->cancel && MoveInvulnerable(*c, c->hitTime - 0.05f));
		}
		// kontry repel (bark, kopniecie) - tez z chwila uderzenia
		for (const char* kind : { "BASH", "KICK" }) {
			const MoveDef* c = FindMove(moves, kind, MOVE_FORWARD);
			CHECK(c && c->hitTime > 0.3f && c->hitTime < c->cancel);
		}
	}
	for (auto& e : err)
		printf("  blad: %s\n", e.c_str());
}

TEST(knockout_finisher_mode_filters_by_permanent_attitude) {
	// 0 = wylaczone, 1 = tylko trwale wrogowie (arena, bojki, questy bezpieczne), 2 = wszyscy poza przyjaciolmi
	const int HOSTILE = 0, ANGRY = 1, NEUTRAL = 2, FRIENDLY = 3;
	CHECK(!KnockoutFinisherAllowed(0, HOSTILE));
	CHECK(KnockoutFinisherAllowed(1, HOSTILE));
	CHECK(!KnockoutFinisherAllowed(1, ANGRY) && !KnockoutFinisherAllowed(1, NEUTRAL) && !KnockoutFinisherAllowed(1, FRIENDLY));
	CHECK(KnockoutFinisherAllowed(2, NEUTRAL) && KnockoutFinisherAllowed(2, ANGRY));
	CHECK(!KnockoutFinisherAllowed(2, FRIENDLY));
}
TEST(key_binding_parses_little_endian_codes) {
	// Gothic.ini [KEYS]: pary bajtow little-endian, np. keySMove=38009d00 -> lewy Alt (0x38) i prawy Ctrl (0x9D)
	std::vector<int> k = ParseKeyBinding("38009d00");
	CHECK(k.size() == 2 && k[0] == 0x38 && k[1] == 0x9D);
	k = ParseKeyBinding("1D000C02");
	CHECK(k.size() == 2 && k[0] == 0x1D && k[1] == 0x020C);
	CHECK(ParseKeyBinding("0000").empty());
	CHECK(ParseKeyBinding("zz").empty());
}

TEST(style_cycle_wraps_through_auto) {
	// -1 = AUTO (gra / zEBA, W3 od progu talentu); cykl: AUTO -> 0 -> ... -> n-1 -> AUTO
	CHECK(CycleStyle(-1, 3, +1) == 0);
	CHECK(CycleStyle(0, 3, +1) == 1);
	CHECK(CycleStyle(2, 3, +1) == -1);
	CHECK(CycleStyle(-1, 3, -1) == 2);
	CHECK(CycleStyle(0, 3, -1) == -1);
	CHECK(CycleStyle(-1, 0, +1) == -1);   // brak stylow = zostaje AUTO
	CHECK(CycleStyle(7, 3, +1) == -1);    // indeks spoza listy (po przeladowaniu) - od nowa
}
TEST(style_auto_w3_from_talent_threshold) {
	CHECK(AutoW3Style(60, 60, true, true));
	CHECK(!AutoW3Style(59, 60, true, true));
	CHECK(!AutoW3Style(100, 60, false, true));   // W3Style=0
	CHECK(!AutoW3Style(100, 60, true, false));   // brak paczki z W3
}
TEST(style_label_from_overlay_name) {
	CHECK(StyleLabel("HUMANS_ZDF_W3_1H") == "Wiedzmin: szybkie ciecia");
	CHECK(StyleLabel("HUMANS_ZDF_W3_2H") == "Wiedzmin: mocne ciecia");
	CHECK(StyleLabel("HUMANS_ZDF_NAT_1HST2") == "Gothic: 1HST2");
	CHECK(StyleLabel("HUMANS_1HDEXST3") == "zEBA: 1HDEXST3");
}

// ---------- Ragdoll ----------
static TraceFn GroundAt(float groundY, float wallX = 1e9f);   // definicja przy testach balistyki
static RagdollState StandingRagdoll() {
	// stojaca postac (cm, Y w gore): miednica 95, klatka 140, glowa 170, rece w dol, nogi proste
	std::vector<Vec3> p(RAG_COUNT);
	p[RAG_PELVIS] = Vec3(0, 95, 0); p[RAG_CHEST] = Vec3(0, 140, 0); p[RAG_HEAD] = Vec3(0, 170, 0);
	p[RAG_L_SHOULDER] = Vec3(-20, 140, 0); p[RAG_L_ELBOW] = Vec3(-22, 112, 0); p[RAG_L_HAND] = Vec3(-24, 85, 0);
	p[RAG_R_SHOULDER] = Vec3(20, 140, 0); p[RAG_R_ELBOW] = Vec3(22, 112, 0); p[RAG_R_HAND] = Vec3(24, 85, 0);
	p[RAG_L_HIP] = Vec3(-10, 90, 0); p[RAG_L_KNEE] = Vec3(-10, 50, 0); p[RAG_L_FOOT] = Vec3(-10, 8, 0);
	p[RAG_R_HIP] = Vec3(10, 90, 0); p[RAG_R_KNEE] = Vec3(10, 50, 0); p[RAG_R_FOOT] = Vec3(10, 8, 0);
	RagdollState s;
	RagdollInit(s, p, Vec3(0, 0, 150));   // pchniecie do przodu (+Z) jak po ciosie w klatke
	return s;
}
TEST(ragdoll_falls_over_and_rests_on_ground) {
	RagdollState s = StandingRagdoll();
	auto trace = GroundAt(0.0f);
	for (int i = 0; i < 600 && !s.resting; i++)
		RagdollStep(s, 1.0f / 60.0f, trace);
	CHECK(s.resting);
	CHECK(s.p[RAG_PELVIS].pos.y < 40.0f);            // lezy, nie stoi
	CHECK(s.p[RAG_HEAD].pos.z > 30.0f);              // upadl w strone pchniecia
	for (auto& q : s.p)
		CHECK(q.pos.y > -1.0f);                      // nic pod ziemia
}
TEST(ragdoll_keeps_bone_lengths) {
	RagdollState s = StandingRagdoll();
	auto trace = GroundAt(0.0f);
	for (int i = 0; i < 300; i++)
		RagdollStep(s, 1.0f / 60.0f, trace);
	for (auto& l : s.links) {
		if (l.minOnly)
			continue;
		float d = Length(s.p[l.a].pos - s.p[l.b].pos);
		CHECK(std::fabs(d - l.rest) < 0.08f * l.rest + 1.0f);
	}
}
TEST(ragdoll_elbow_does_not_fold_completely) {
	RagdollState s = StandingRagdoll();
	auto trace = GroundAt(0.0f);
	for (int i = 0; i < 300; i++)
		RagdollStep(s, 1.0f / 60.0f, trace);
	float upper = Length(Vec3(-20, 140, 0) - Vec3(-22, 112, 0)), fore = Length(Vec3(-22, 112, 0) - Vec3(-24, 85, 0));
	CHECK(Length(s.p[RAG_L_SHOULDER].pos - s.p[RAG_L_HAND].pos) > 0.4f * (upper + fore));
	CHECK(Length(s.p[RAG_L_HIP].pos - s.p[RAG_L_FOOT].pos) > 0.4f * 82.0f);
}
// Mini-szkielet: korzen (miednica) -> klatka -> reka (lokiec) -> przedramie (dlon); punkty ragdolla na wezlach.
struct MiniRig {
	RagPoseRig rig;
	std::vector<Vec3> particles;
};
static Mat34 Tr(float x, float y, float z) {
	Mat34 m = Mat34::Identity();
	m.m[0][3] = x; m.m[1][3] = y; m.m[2][3] = z;
	return m;
}
static MiniRig MakeMiniRig() {
	MiniRig r;
	// wezly: 0 korzen (95), 1 klatka (+45), 2 ramie L (barki -20), 3 przedramie L (-28 w dol), 4 dlon (-27)
	std::vector<int> parent = { -1, 0, 1, 2, 3 };
	std::vector<Mat34> local = { Tr(0, 95, 0), Tr(0, 45, 0), Tr(-20, 0, 0), Tr(0, -28, 0), Tr(0, -27, 0) };
	std::vector<int> role = { RAG_ROLE_ROOT, RAG_ROLE_CHEST, RAG_ROLE_LIMB, RAG_ROLE_LIMB, RAG_ROLE_RIGID };
	std::vector<int> from = { -1, -1, RAG_L_SHOULDER, RAG_L_ELBOW, -1 }, to = { -1, -1, RAG_L_ELBOW, RAG_L_HAND, -1 };
	r.particles.assign(RAG_COUNT, Vec3());
	r.particles[RAG_PELVIS] = Vec3(0, 95, 0); r.particles[RAG_CHEST] = Vec3(0, 140, 0); r.particles[RAG_HEAD] = Vec3(0, 170, 0);
	r.particles[RAG_L_SHOULDER] = Vec3(-20, 140, 0); r.particles[RAG_L_ELBOW] = Vec3(-20, 112, 0); r.particles[RAG_L_HAND] = Vec3(-20, 85, 0);
	r.particles[RAG_R_SHOULDER] = Vec3(20, 140, 0); r.particles[RAG_L_HIP] = Vec3(-10, 90, 0); r.particles[RAG_R_HIP] = Vec3(10, 90, 0);
	r.rig = BuildRagPoseRig(parent, local, role, from, to, r.particles, Mat34::Identity());
	return r;
}
static float MaxDiff(const Mat34& a, const Mat34& b) {
	float d = 0;
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 4; j++)
			d = std::fmax(d, std::fabs(a.m[i][j] - b.m[i][j]));
	return d;
}
TEST(ragdoll_pose_at_start_equals_start_pose) {
	MiniRig r = MakeMiniRig();
	std::vector<Mat34> locals;
	RagdollPose(r.rig, r.particles, Mat34::Identity(), locals);
	for (size_t i = 0; i < locals.size(); i++)
		CHECK(MaxDiff(locals[i], r.rig.startLocal[i]) < 1e-3f);
}
TEST(ragdoll_pose_follows_turned_body_and_bent_elbow) {
	MiniRig r = MakeMiniRig();
	// cale cialo obrocone o 90 st. wokol pionu przez miednice (X -> -Z), lokiec zgiety: dlon do przodu
	std::vector<Vec3> p = r.particles;
	for (auto& q : p) {
		Vec3 d = q - Vec3(0, 95, 0);
		q = Vec3(0, 95, 0) + Vec3(d.z, d.y, -d.x);
	}
	p[RAG_L_HAND] = p[RAG_L_ELBOW] + Vec3(27, 0, 0);   // przedramie poziomo w +X
	std::vector<Mat34> locals;
	RagdollPose(r.rig, p, Mat34::Identity(), locals);
	// uklad modelu z lokalnych: klatka obrocona o 90 st., przedramie wskazuje +X
	Mat34 o0 = locals[0], o1 = Mul(o0, locals[1]), o2 = Mul(o1, locals[2]), o3 = Mul(o2, locals[3]);
	Vec3 chestX = Rotate(o1, Vec3(1, 0, 0));
	CHECK(std::fabs(chestX.z + 1.0f) < 0.05f);                 // os X klatki obrocona na -Z
	Vec3 elbow = o3.Translation(), hand = Transform(o3, Vec3(0, -27, 0));
	CHECK(Length(elbow - p[RAG_L_ELBOW]) < 3.0f);               // lokiec tam, gdzie punkt
	CHECK(Dot(Normalize(hand - elbow), Vec3(1, 0, 0)) > 0.98f);  // przedramie w strone dloni
}
TEST(frame_from_axes_is_orthonormal_and_follows_up) {
	Mat34 f = FrameFromAxes(Vec3(0, 2, 0.1f), Vec3(1, 0.3f, 0));
	Vec3 x(f.m[0][0], f.m[1][0], f.m[2][0]), y(f.m[0][1], f.m[1][1], f.m[2][1]), z(f.m[0][2], f.m[1][2], f.m[2][2]);
	CHECK(std::fabs(Length(x) - 1) < 1e-4f && std::fabs(Length(y) - 1) < 1e-4f && std::fabs(Length(z) - 1) < 1e-4f);
	CHECK(std::fabs(Dot(x, y)) < 1e-4f && std::fabs(Dot(y, z)) < 1e-4f && std::fabs(Dot(x, z)) < 1e-4f);
	CHECK(Dot(y, Normalize(Vec3(0, 2, 0.1f))) > 0.9999f);
	Mat34 r = RotationBetween(Vec3(1, 0, 0), Vec3(0, 0, 1));
	Vec3 v = Rotate(r, Vec3(1, 0, 0));
	CHECK(std::fabs(v.z - 1) < 1e-4f && std::fabs(v.x) < 1e-4f);
}

// ---------- ZMath ----------
TEST(lerp_yaw_takes_shorter_way_round) {
	const float d = 3.14159265f / 180.0f;
	CHECK(std::fabs(LerpYaw(0.0f, 90 * d, 0.5f) - 45 * d) < 1e-4f);
	// 170 -> -170 to 20 stopni przez 180, nie 340 przez 0
	float mid = LerpYaw(170 * d, -170 * d, 0.5f);
	CHECK(std::fabs(std::fabs(mid) - 180 * d) < 1e-4f);
	CHECK(std::fabs(std::sin(LerpYaw(170 * d, -170 * d, 1.0f)) - std::sin(-170 * d)) < 1e-4f);
	CHECK(std::fabs(LerpYaw(-170 * d, 170 * d, 0.25f) - (-175 * d)) < 1e-4f);
}

// ---------- Ballistics ----------
static TraceFn GroundAt(float groundY, float wallX) {
	return [=](const Vec3& from, const Vec3& ray, Vec3& hit, Vec3& normal) -> bool {
		float best = 2.0f;
		Vec3 n;
		if (ray.y < 0 && from.y >= groundY) {
			float t = (groundY - from.y) / ray.y;
			if (t >= 0 && t <= 1 && t < best) { best = t; n = Vec3(0, 1, 0); }
		}
		if (ray.x > 0 && from.x <= wallX) {
			float t = (wallX - from.x) / ray.x;
			if (t >= 0 && t <= 1 && t < best) { best = t; n = Vec3(-1, 0, 0); }
		}
		if (best > 1.0f)
			return false;
		hit = from + ray * best;
		normal = n;
		return true;
	};
}
static BallisticState MakeLimb(float y, Vec3 vel) {
	BallisticState s;
	s.trafo = Mat34::Identity();
	s.trafo.m[1][3] = y;
	BallisticSetShape(s, { Vec3(0, 0, 0), Vec3(25, 0, 0), Vec3(45, 0, 0) });
	s.vel = vel;
	s.axis = Vec3(0, 0, 1);
	s.degPerSec = 500.0f;
	return s;
}
static float LowestY(const BallisticState& s) {
	float low = 1e9f;
	for (auto& p : s.points)
		low = std::fmin(low, Transform(s.trafo, p).y);
	return low;
}
TEST(ballistic_falls_and_rests_on_flat_ground) {
	BallisticState s = MakeLimb(150.0f, Vec3(150, 300, 0));
	auto trace = GroundAt(0.0f);
	float t = 0;
	while (!s.resting && t < 10.0f) {
		BallisticStep(s, 1.0f / 60.0f, trace);
		t += 1.0f / 60.0f;
	}
	CHECK(s.resting);
	CHECK(t < 5.0f);
	CHECK(LowestY(s) > -1.0f);
	CHECK(LowestY(s) < 15.0f);
	CHECK(s.bounces >= 1);
}
TEST(ballistic_wall_stops_horizontal_motion) {
	BallisticState s = MakeLimb(150.0f, Vec3(600, 50, 0));
	auto trace = GroundAt(0.0f, 80.0f);
	for (int i = 0; i < 600 && !s.resting; i++)
		BallisticStep(s, 1.0f / 60.0f, trace);
	CHECK(Transform(s.trafo, s.center).x < 80.0f);
}
// Kat obrotu miedzy dwoma ukladami (stopnie).
static float RotDeg(const Mat34& a, const Mat34& b) {
	float tr = 0.0f;
	for (int i = 0; i < 3; i++)
		for (int k = 0; k < 3; k++)
			tr += a.m[k][i] * b.m[k][i];
	float c = std::fmax(-1.0f, std::fmin(1.0f, (tr - 1.0f) / 2.0f));
	return std::acos(c) * 180.0f / 3.14159265f;
}
TEST(ballistic_boxy_head_settles_without_rolling_on) {
	// glowa (bbox ~ szescian): po upadku na sciane ma lezec, a nie przewracac sie przez krawedzie do limitu czasu
	// (dluga os = przekatna nigdy nie lezy poziomo - stary warunek "dluga os stroma" krecil glowa ~1,5 s = drganie)
	BallisticState s;
	s.trafo = Mat34::Identity();
	std::vector<Vec3> box;
	for (int i = 0; i < 8; i++)
		box.push_back(Vec3((i & 1) ? 10.0f : -10.0f, (i & 2) ? 12.0f : -12.0f, (i & 4) ? 10.0f : -10.0f));
	BallisticSetShape(s, box);
	RotateAround(s.trafo, Vec3(0, 0, 0), Vec3(1, 0, 0), 25.0f);   // lekko przechylona
	s.trafo.m[1][3] = 30.0f;
	auto trace = GroundAt(0.0f);
	float contactAt = -1.0f, t = 0.0f, turned = 0.0f;
	Mat34 prev = s.trafo;
	for (int i = 0; i < 900 && !s.resting; i++) {
		BallisticStep(s, 1.0f / 60.0f, trace);
		t += 1.0f / 60.0f;
		if (contactAt < 0.0f && s.contactTime > 0.0f)
			contactAt = t;
		if (contactAt >= 0.0f)
			turned += RotDeg(prev, s.trafo);
		prev = s.trafo;
	}
	CHECK(s.resting);
	CHECK(contactAt >= 0.0f && t - contactAt < 0.8f);   // spoczynek szybko po kontakcie
	CHECK(turned < 60.0f);                              // co najwyzej opadniecie na sciane, bez toczenia
}
TEST(ballistic_long_limb_ends_lying_not_standing) {
	BallisticState s = MakeLimb(100.0f, Vec3(0, 0, 0));
	s.degPerSec = 0;
	// stoi pionowo: dluga os wzdluz Y
	s.trafo = Mat34::Identity();
	s.trafo.m[0][0] = 0; s.trafo.m[1][0] = 1;
	s.trafo.m[0][1] = -1; s.trafo.m[1][1] = 0;
	s.trafo.m[1][3] = 10.0f;
	auto trace = GroundAt(0.0f);
	for (int i = 0; i < 600 && !s.resting; i++)
		BallisticStep(s, 1.0f / 60.0f, trace);
	Vec3 axisW = Rotate(s.trafo, s.longAxis);
	CHECK(std::fabs(Normalize(axisW).y) < 0.5f);
}

using Names = std::vector<std::string>;
static bool Same(const Names& a, const Names& b) { return a == b; }

TEST(menu_index_is_nearest_listed_value) {
	// choicebox menu zapisuje numer pozycji; wartosc z ini spoza listy pokazuje najblizsza pozycje
	std::vector<int> pct = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
	CHECK(MenuIndexForValue(pct, 30) == 3);
	CHECK(MenuIndexForValue(pct, 34) == 3 && MenuIndexForValue(pct, 36) == 4);
	CHECK(MenuIndexForValue(pct, -5) == 0 && MenuIndexForValue(pct, 250) == 10);
	std::vector<int> keys = { 47, 46, 45, 44, 56, 29, 58, 0 };   // V C X Z LAlt LCtrl CapsLock wyl. (jak Config.cpp)
	CHECK(MenuIndexForValue(keys, 56, false) == 4);
	CHECK(MenuIndexForValue(keys, 58, false) == 6);
	CHECK(MenuIndexForValue(keys, 0, false) == 7);
	CHECK(MenuIndexForValue(keys, 33, false) == -1);   // klawisz spoza listy - dokladnie, bez najblizszego
}
TEST(menu_change_detected_only_when_index_moved) {
	std::vector<int> pct = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 };
	int v = 0;
	CHECK(MenuChangedValue(pct, 3, 5, v) && v == 50);    // gracz przestawil w menu
	CHECK(!MenuChangedValue(pct, 3, 3, v));              // bez zmiany - reczna wartosc z ini zostaje
	CHECK(!MenuChangedValue(pct, 3, 11, v));             // poza lista
	CHECK(!MenuChangedValue(pct, 3, -1, v));
}
TEST(rapier_detected_by_weapon_instance_tokens) {
	// RapierWeapons=RAPIER,DEGEN - fragmenty nazwy instancji broni (ItMw_Rapier, ItMw_Meisterdegen), bez wielkosci liter
	std::vector<std::string> tokens = SplitTokens(" rapier, Degen ,", ',');
	CHECK(tokens.size() == 2 && tokens[0] == "RAPIER" && tokens[1] == "DEGEN");
	CHECK(NameHasToken("ITMW_RAPIER", tokens) && NameHasToken("ItMw_MeisterDegen", tokens));
	CHECK(!NameHasToken("ITMW_1H_SWORD_01", tokens));
	CHECK(!NameHasToken("ITMW_RAPIER", {}));
}
TEST(thrust_finishers_are_those_without_cut) {
	FinisherDef stab, chop;
	chop.cut = Part::Head;
	CHECK(IsThrustFinisher(stab) && !IsThrustFinisher(chop));
}
TEST(overlay_plan_nothing_when_wanted_on_top) {
	OverlayPlan p = PlanOverlays({ "HUMANS_REMASTER", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" },
		{ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty() && p.apply.empty());
}
TEST(overlay_plan_tolerates_foreign_sprint_above_ours) {
	// sprint zEBA nad naszymi nakladkami, niechciana nakladka ruchu schodzi, styl zostaje - nic nie przekladamy
	// (od poprawki sprintu ruch W3 przy W3Sprint=0 zostaje - patrz motion_sprint_parked_when_idle_and_on_top_in_game_sprint)
	OverlayPlan p = PlanOverlays({ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER" },
		{ "HUMANS_ZDF_W3_1H" }, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" });
	CHECK(Same(p.remove, { "HUMANS_ZDF_W3_LOCO" }));
	CHECK(p.apply.empty());
}
TEST(overlay_plan_puts_only_our_sprint_on_top) {
	// W3Sprint=1: tylko nasz sprint ponad sprintem zEBA - styl i petla nie sa przekladane (bez szarpniecia)
	OverlayPlan p = PlanOverlays({ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT" },
		{ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" }, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty());
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_SPRINT" }));
	// nasz sprint pod sprintem gry (zEBA zalozyl swoj ponownie) - tylko on wraca na wierzch
	p = PlanOverlays({ "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT", "HUMANS_SPRINT" },
		{ "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" }, { "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" });
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_SPRINT" }));
}
TEST(overlay_plan_reapplies_only_missing_suffix) {
	// po sprincie wraca petla biegu: styl zostaje na miejscu, dokladamy tylko brakujace
	OverlayPlan p = PlanOverlays({ "HUMANS_REMASTER", "HUMANS_ZDF_W3_1H" },
		{ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_1H" });
	CHECK(p.remove.empty());
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO" }));
}
TEST(overlay_plan_tolerates_transient_gesture_overlays) {
	// gest Znaku (zQuickSign) i ruch (unik) zakladaja nakladke na chwile - nie przekladamy przez nie stylow
	OverlayPlan p = PlanOverlays({ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_SIGN_AARD" },
		{ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty() && p.apply.empty());
	p = PlanOverlays({ "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_MV_DODGE_L" }, { "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty() && p.apply.empty());
	// stala nakladka gestow zQuickSign (wszystkie Znaki w jednej) - tez nie przekladamy przez nia
	p = PlanOverlays({ "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_SIGNS" }, { "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty() && p.apply.empty());
}
TEST(overlay_plan_full_reapply_when_foreign_style_on_top) {
	// obca nakladka (np. styl zEBA) nad naszymi - wszystkie nasze na wierzch, w kolejnosci
	OverlayPlan p = PlanOverlays({ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_1HST2" },
		{ "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty());
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO" }));
}
TEST(overlay_plan_never_removes_foreign_overlays) {
	// niechciana, ale nie nasza (np. styl zEBA wymuszony wczesniej w cyklu) zostaje na modelu
	OverlayPlan p = PlanOverlays({ "HUMANS_1HST3", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_LOCO" });
	CHECK(p.remove.empty() && p.apply.empty());
	CHECK(IsForeignSprintOverlay("HUMANS_SPRINT_REMASTER") && IsForeignSprintOverlay("HUMANS_SPRINT"));
	CHECK(!IsForeignSprintOverlay("HUMANS_ZDF_W3_SPRINT") && !IsForeignSprintOverlay("HUMANS_1HST1"));
}

// ---------- Ruchy: podwojny Alt, zbocza, utkniecie, wypad kontry ----------
TEST(roll_tap_forward_waits_then_double_press_rolls) {
	// Alt+W w walce: pierwsze wcisniecie czeka (skok polkniety), drugie w oknie = przewrot w przod
	RollTap s;
	CHECK(RollTapStep(s, true, true, 0.016f, 0.22f) == ROLLTAP_WAIT && s.waiting);
	CHECK(RollTapStep(s, false, true, 0.1f, 0.22f) == ROLLTAP_WAIT);
	CHECK(RollTapStep(s, true, false, 0.016f, 0.22f) == ROLLTAP_ROLL);   // drugie Alt juz bez W - nadal w przod
	CHECK(!s.waiting);
	CHECK(RollTapStep(s, false, false, 0.016f, 0.22f) == ROLLTAP_NONE);
}
TEST(roll_tap_window_runs_out_into_game_jump) {
	RollTap s;
	CHECK(RollTapStep(s, true, true, 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(RollTapStep(s, false, true, 0.1f, 0.22f) == ROLLTAP_WAIT);
	CHECK(RollTapStep(s, false, true, 0.1f, 0.22f) == ROLLTAP_WAIT);    // 0,20 s
	CHECK(RollTapStep(s, false, false, 0.05f, 0.22f) == ROLLTAP_JUMP);  // 0,25 s - skok gry (raz)
	CHECK(!s.waiting && RollTapStep(s, false, false, 0.05f, 0.22f) == ROLLTAP_NONE);
	// kolejne Alt+W zaczyna od nowa
	CHECK(RollTapStep(s, true, true, 0.016f, 0.22f) == ROLLTAP_WAIT);
}
TEST(roll_tap_ignores_presses_without_forward) {
	// sam Alt (skok) albo Alt z bokiem / tylem (przewrot od razu) - maszynka nic nie robi
	RollTap s;
	CHECK(RollTapStep(s, true, false, 0.016f, 0.22f) == ROLLTAP_NONE && !s.waiting);
	CHECK(RollTapStep(s, false, false, 0.5f, 0.22f) == ROLLTAP_NONE);
}
TEST(step_classifier_walks_slopes_and_stops_at_walls) {
	TerrainLimits lim;   // 50 st. jak chod w Gothicu, stopien 30 cm, uskok 100 cm
	CHECK(ClassifyStep(0.0f, 17.0f, 1.0f, lim) == STEP_OK);              // plasko
	CHECK(ClassifyStep(9.8f, 17.0f, 0.866f, lim) == STEP_OK);            // 30 st. pod gore
	CHECK(ClassifyStep(17.0f, 17.0f, 0.707f, lim) == STEP_OK);           // 45 st. pod gore
	CHECK(ClassifyStep(29.4f, 17.0f, 0.5f, lim) == STEP_WALL);           // 60 st. - za stromo
	CHECK(ClassifyStep(25.0f, 17.0f, 1.0f, lim) == STEP_OK);             // stopien schodow
	CHECK(ClassifyStep(80.0f, 17.0f, 1.0f, lim) == STEP_WALL);           // murek / skrzynia
	CHECK(ClassifyStep(-17.0f, 17.0f, 0.707f, lim) == STEP_OK);          // z gorki
	CHECK(ClassifyStep(-29.4f, 17.0f, 0.5f, lim) == STEP_OK);            // stromo w dol - zjezdza
	CHECK(ClassifyStep(-150.0f, 17.0f, 1.0f, lim) == STEP_DROP);         // urwisko
	CHECK(ClassifyStep(1.0f, 17.0f, 0.3f, lim) == STEP_OK);              // wzdluz stromego zbocza (bez wzniosu)
}
TEST(climb_limited_by_slope_over_path) {
	TerrainLimits lim;
	CHECK(ClimbWithinSlope(250.0f, 250.0f, lim));    // 45 st. przez 2,5 m (stary limit 70 cm blokowal)
	CHECK(!ClimbWithinSlope(200.0f, 100.0f, lim));   // srednio 63 st.
	CHECK(ClimbWithinSlope(20.0f, 0.0f, lim));       // stopien na starcie
}
TEST(move_block_aborts_only_when_really_stuck) {
	MoveBlock b;
	CHECK(!MoveBlockStep(b, 17.0f, 17.0f, 1.0f));    // swobodnie
	CHECK(!MoveBlockStep(b, 17.0f, 0.0f, 1.0f));     // jedna klatka na kamieniu
	CHECK(!MoveBlockStep(b, 17.0f, 17.0f, 1.0f));    // i dalej
	for (int i = 0; i < 20; i++)                     // slizg wzdluz sciany (70 % kroku) - bez przerwania
		CHECK(!MoveBlockStep(b, 17.0f, 12.0f, 1.0f));
	MoveBlock w;
	CHECK(!MoveBlockStep(w, 17.0f, 0.0f, 1.0f));
	CHECK(!MoveBlockStep(w, 17.0f, 0.0f, 1.0f));
	CHECK(MoveBlockStep(w, 17.0f, 0.0f, 1.0f));      // sciana: ~50 cm straty = koniec ruchu
	MoveBlock e;                                     // pod koniec (przejscie do postawy) - dogrywa w miejscu
	for (int i = 0; i < 10; i++)
		CHECK(!MoveBlockStep(e, 17.0f, 0.0f, 0.2f));
	float before = e.lost;
	CHECK(!MoveBlockStep(e, 0.2f, 0.0f, 0.2f) && e.lost == before);   // stojacy tor nic nie dodaje
}
TEST(traj_forward_splits_recoil_and_lunge) {
	std::vector<float> xz = { 0, 0, 0, -5, 0, -10, 0, -5, 0, 10, 0, 30, 0, 50 };   // odrzut -10, potem wypad +60
	float pos = 0, neg = 0;
	TrajForward(xz, 25.0f, 0.0f, 6.0f / 25.0f, pos, neg);
	CHECK(std::fabs(pos - 60.0f) < 1e-3f && std::fabs(neg + 10.0f) < 1e-3f);
	TrajForward(xz, 25.0f, 3.0f / 25.0f, 6.0f / 25.0f, pos, neg);
	CHECK(std::fabs(pos - 55.0f) < 1e-3f && std::fabs(neg) < 1e-3f);
	TrajForward(xz, 25.0f, 4.5f / 25.0f, 10.0f, pos, neg);   // od polowy probki do konca (dalej tor stoi)
	CHECK(std::fabs(pos - 30.0f) < 1e-3f && std::fabs(neg) < 1e-3f);
}
TEST(lunge_scale_closes_the_gap_at_hit) {
	// tor: odrzut -10, wypad +60 do ciosu; przeciwnik 100 cm dalej niz zasieg ciosu -> wypad x1,83
	CHECK(std::fabs(LungeScale(100.0f, 60.0f, -10.0f, 2.0f) - 110.0f / 60.0f) < 1e-4f);
	CHECK(std::fabs(LungeScale(50.0f, 60.0f, -10.0f, 2.0f) - 1.0f) < 1e-4f);
	CHECK(LungeScale(-30.0f, 60.0f, -10.0f, 2.0f) == 0.0f);       // za blisko - bez wypadu
	CHECK(LungeScale(500.0f, 60.0f, -10.0f, 1.6f) == 1.6f);       // za daleko - najwiecej, ile mozna
	CHECK(LungeScale(50.0f, 0.0f, 0.0f, 1.6f) == 1.0f);           // tor bez wypadu
	CHECK(LungeReachable(80.0f, 60.0f, -10.0f, 1.6f, 20.0f));     // 86 cm wypadu + zapas
	CHECK(!LungeReachable(150.0f, 60.0f, -10.0f, 1.6f, 20.0f));
}
TEST(turn_toward_is_rate_limited_and_takes_short_way) {
	CHECK(std::fabs(TurnToward(0.0f, 1.0f, 0.3f) - 0.3f) < 1e-5f);
	CHECK(std::fabs(TurnToward(0.0f, 0.2f, 0.3f) - 0.2f) < 1e-5f);
	CHECK(std::fabs(TurnToward(0.0f, -1.0f, 0.3f) + 0.3f) < 1e-5f);
	float y = TurnToward(3.0f, -3.0f, 0.1f);                     // przez 180 st., nie przez 0
	CHECK(y > 3.0f && std::fabs(y - 3.1f) < 1e-4f);
}
TEST(push_offset_eases_out_to_full_distance) {
	CHECK(PushOffset(0.0f, 80.0f, 0.3f) == 0.0f);
	CHECK(std::fabs(PushOffset(0.3f, 80.0f, 0.3f) - 80.0f) < 1e-4f && PushOffset(1.0f, 80.0f, 0.3f) == 80.0f);
	CHECK(PushOffset(0.15f, 80.0f, 0.3f) > 50.0f);               // szybko na poczatku
	float prev = 0.0f;
	bool mono = true;
	for (int i = 1; i <= 30; i++) {
		float p = PushOffset(i * 0.01f, 80.0f, 0.3f);
		mono = mono && p >= prev;
		prev = p;
	}
	CHECK(mono);
}
TEST(moves_parse_counter_reach) {
	// reach = odleglosc (cm) od voba do punktu kontaktu w chwili ciosu (ostrze / bark / stopa); bez klucza -1
	std::vector<std::string> err;
	auto defs = ParseMoves("[KICK_F]\noverlay=X\ntraj=0,0 0,10\nhit=0.76\nreach=95.5\n[DODGE_B]\noverlay=Y\ntraj=0,0 0,-10\n", err);
	CHECK(err.empty() && defs.size() == 2);
	if (defs.size() == 2)
		CHECK(std::fabs(defs[0].reach - 95.5f) < 1e-4f && defs[1].reach < 0.0f);
	ParseMoves("[KICK_F]\noverlay=X\ntraj=0,0 0,10\nreach=-3\n", err);
	CHECK(!err.empty());
}
TEST(moves_parse_w3_style_overlay_variant) {
	// overlayW3 = ta sama animacja z koncem w gardzie stylu W3 (wybierana, gdy postawa bohatera jest z HUMANS_ZDF_W3_*)
	std::vector<std::string> err;
	auto defs = ParseMoves("[ROLL_F]\noverlay=HUMANS_ZDF_MV_ROLL_F\noverlayW3=HUMANS_ZDF_MV_ROLL_F_W3\ntraj=0,0 0,10\n"
		"[DODGE_B]\noverlay=Y\ntraj=0,0 0,-10\n", err);
	CHECK(err.empty() && defs.size() == 2);
	if (defs.size() == 2)
		CHECK(defs[0].overlayW3 == "HUMANS_ZDF_MV_ROLL_F_W3" && defs[1].overlayW3.empty());
}

// ---------- Bron: odcina / tylko pchniecie (rapier) / wcale (obuch) ----------
static WeaponRules TestWeaponRules() {
	WeaponRules r;
	r.bluntTokens = SplitTokens("KEULE,HAMMER,STREITKOLBEN,MORGENSTERN,STAB,KNUEPPEL,MACE,CLUB,STAFF", ',');
	r.rapierTokens = SplitTokens("RAPIER,DEGEN", ',');
	return r;
}
TEST(weapon_edge_swords_and_axes_cut) {
	WeaponRules r = TestWeaponRules();
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_SCHWERT1", "ItMw_035_1h_Sword_04.3DS", r) == WeaponCut::Cut);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_2H_ORCAXE_01", "ItMw_2H_OrcAxe_01.3DS", r) == WeaponCut::Cut);
	// ostrze z dodatkowym ogniem (mody) dalej tnie
	CHECK(ClassifyWeapon(DMG_EDGE | 8, "ITMW_MOD_FLAMMENSCHWERT", "ItMw_Mod_Fire.3DS", r) == WeaponCut::Cut);
	// maczeta to nie buzdygan (MACHETE nie zawiera MACE)
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_ADDON_HACKER_1H_01", "ItMw_1H_Machete_02.3DS", r) == WeaponCut::Cut);
}
TEST(weapon_blunt_damage_never_cuts) {
	WeaponRules r = TestWeaponRules();
	// G2: wszystkie maczugi, mloty, buzdygany i kostury maja DAM_BLUNT
	CHECK(ClassifyWeapon(DMG_BLUNT, "ITMW_STREITKOLBEN", "ItMw_050_1h_mace_war_03.3DS", r) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(DMG_BLUNT, "ITMW_1H_BAU_MACE", "ItMw_010_1h_Club_01.3DS", r) == WeaponCut::NoCut);
	// mod: obuch z dodatkiem siecznych - obuch wygrywa, nawet bez fragmentu nazwy z listy
	CHECK(ClassifyWeapon(DMG_BLUNT | DMG_EDGE, "ITMW_MOD_NAGELBRETT", "ItMw_Mod_01.3DS", r) == WeaponCut::NoCut);
	// bez obrazen siecznych (klujace, piesci, nic) tez nie
	CHECK(ClassifyWeapon(DMG_POINT, "ITMW_MOD_SPEER", "ItMw_Mod_Spear.3DS", r) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(0, "", "", r) == WeaponCut::NoCut);
}
TEST(weapon_blunt_tokens_override_edge_damage) {
	WeaponRules r = TestWeaponRules();
	// G2 (GOTHIC.DAT): EDGE, ale model buzdyganu / kostura / pogrzebacza
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_INQUISITOR", "ItMw_060_1h_mace_war_04.3DS", r) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_RICHTSTAB", "ItMw_025_2h_Staff_02.3DS", r) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_1H_MACE_L_01", "Itmw_005_1h_poker_01.3DS", r) == WeaponCut::NoCut);
	// bez listy zostaje sama regula obrazen
	WeaponRules none;
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_INQUISITOR", "ItMw_060_1h_mace_war_04.3DS", none) == WeaponCut::Cut);
}
TEST(weapon_rapier_only_thrusts) {
	WeaponRules r = TestWeaponRules();
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_RAPIER", "ItMw_050_1h_Sword_Rapier_01.3ds", r) == WeaponCut::ThrustOnly);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_MEISTERDEGEN", "ItMw_065_1h_SwordCane_02.3ds", r) == WeaponCut::ThrustOnly);
	// po modelu (mod z inna nazwa instancji) i po stylu walki rapierem (zEBA HUMANS_RAPIER_*)
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_MOD_FLORETT", "ItMw_Rapier_Gold.3ds", r) == WeaponCut::ThrustOnly);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_SCHWERT1", "ItMw_035_1h_Sword_04.3DS", r, true) == WeaponCut::ThrustOnly);
}
TEST(weapon_rapier_rule_off_but_blunt_still_never_cuts) {
	WeaponRules r = TestWeaponRules();
	r.rapierThrustOnly = false;   // RapierThrustOnly=0: rapier jak zwykly miecz
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_RAPIER", "ItMw_050_1h_Sword_Rapier_01.3ds", r, true) == WeaponCut::Cut);
	CHECK(ClassifyWeapon(DMG_BLUNT, "ITMW_STREITKOLBEN", "ItMw_050_1h_mace_war_03.3DS", r) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_INQUISITOR", "ItMw_060_1h_mace_war_04.3DS", r) == WeaponCut::NoCut);
}
TEST(weapon_blunt_beats_rapier) {
	WeaponRules r = TestWeaponRules();
	CHECK(ClassifyWeapon(DMG_BLUNT, "ITMW_RAPIER", "", r, true) == WeaponCut::NoCut);
	CHECK(ClassifyWeapon(DMG_EDGE, "ITMW_MOD_DEGENKEULE", "", r) == WeaponCut::NoCut);
}
TEST(weapon_cut_helpers) {
	CHECK(WeaponCanCut(WeaponCut::Cut) && !WeaponCanCut(WeaponCut::ThrustOnly) && !WeaponCanCut(WeaponCut::NoCut));
	CHECK(WeaponCanFinish(WeaponCut::Cut) && WeaponCanFinish(WeaponCut::ThrustOnly) && !WeaponCanFinish(WeaponCut::NoCut));
	CHECK(std::string(WeaponCutName(WeaponCut::ThrustOnly)) == "pchniecie");
}

// ---------- Powrot sterowania po finisherze ----------
static bool Near(float a, float b) { return fabsf(a - b) < 1e-4f; }
TEST(quat_angle_ignores_sign_and_measures_rotation) {
	const float id[4] = { 0, 0, 0, 1 }, neg[4] = { 0, 0, 0, -1 };
	const float y90[4] = { 0, sinf(0.785398f), 0, cosf(0.785398f) };
	CHECK(Near(QuatAngleDeg(id, id), 0.0f));
	CHECK(Near(QuatAngleDeg(id, neg), 0.0f));
	CHECK(fabsf(QuatAngleDeg(id, y90) - 90.0f) < 0.01f);
}
TEST(release_at_start_of_final_hold) {
	// jak W3_02: 50 kl. ruchu, 30 kl. bezruchu (bohater stoi, az ofiara upadnie), 8 kl. doklejonego przejscia
	std::vector<float> m(50, 20.0f);
	m.resize(80, 0.0f);
	m.insert(m.end(), 8, 15.0f);
	ReleaseParams p;
	p.minTime = 1.0f;
	CHECK(Near(ReleaseTime(m, p), 50 / 25.0f));
}
TEST(release_counts_slow_drift_as_hold) {
	// jak W3_01: po ciosie powolny dryf ~1 st./kl. (ponizej progu 1,5 st./kl.)
	std::vector<float> m(50, 20.0f);
	m.resize(76, 1.1f);
	m.insert(m.end(), 8, 15.0f);
	ReleaseParams p;
	CHECK(Near(ReleaseTime(m, p), 50 / 25.0f));
}
TEST(release_without_hold_is_blend_before_end) {
	// ruch do samego konca (finisher Mixamo): postawa wchodzi przejsciem na ostatnie blendSec
	std::vector<float> m(45, 20.0f);   // 46 klatek, ostatnia w 1,8 s
	ReleaseParams p;
	p.minTime = 0.5f;
	CHECK(Near(ReleaseTime(m, p), 45 / 25.0f - p.blendSec));
}
TEST(release_ignores_short_pause) {
	// zatrzymanie na 3 kl. w pol zamachu to nie koniec ruchu
	std::vector<float> m(50, 20.0f);
	m.insert(m.end(), 3, 0.0f);
	m.insert(m.end(), 8, 15.0f);
	ReleaseParams p;
	CHECK(Near(ReleaseTime(m, p), 61 / 25.0f - p.blendSec));
}
TEST(release_ignores_hold_followed_by_long_motion) {
	// po bezruchu jeszcze 15 kl. ruchu (dluzej niz doklejone przejscie) - bohater cos robi, czekamy do konca
	std::vector<float> m(50, 20.0f);
	m.insert(m.end(), 20, 0.0f);
	m.insert(m.end(), 15, 15.0f);
	ReleaseParams p;
	CHECK(Near(ReleaseTime(m, p), 85 / 25.0f - p.blendSec));
}
TEST(release_never_before_min_time_nor_after_end) {
	// bezruch od 0,4 s, ale ciecie i zwolnione tempo do 1,0 s
	std::vector<float> m(10, 20.0f);
	m.insert(m.end(), 60, 0.0f);
	m.insert(m.end(), 8, 15.0f);
	ReleaseParams p;
	p.minTime = 1.0f;
	CHECK(Near(ReleaseTime(m, p), 1.0f));
	// minTime za koncem animacji - koniec
	p.minTime = 10.0f;
	CHECK(Near(ReleaseTime(m, p), 78 / 25.0f));
	// krotka animacja: przejscie dluzsze niz animacja nie schodzi ponizej zera
	std::vector<float> one(3, 20.0f);
	ReleaseParams q;
	CHECK(Near(ReleaseTime(one, q), 0.0f));
	CHECK(Near(ReleaseTime({}, q), 0.0f));
}
TEST(release_threshold_scales_with_fps) {
	// 2 st./kl. przy 50 kl./s = 100 st./s - ruch, przy progu 37,5 st./s
	std::vector<float> m(50, 20.0f);
	m.insert(m.end(), 30, 2.0f);
	ReleaseParams p;
	p.fps = 50.0f;
	CHECK(Near(ReleaseTime(m, p), 80 / 50.0f - p.blendSec));
	// 0,5 st./kl. przy 50 kl./s = 25 st./s - bezruch
	std::vector<float> s(50, 20.0f);
	s.insert(s.end(), 30, 0.5f);
	CHECK(Near(ReleaseTime(s, p), 50 / 50.0f));
}

// Sprint W3 nad sprintem zEBA: nakladki ruchu zalozone raz, potem tylko przekladane (zalozenie / zdjecie nakladki
// zatrzymuje wszystkie animacje, a zEBA bez aktywnej petli biegu - oCAniCtrl_Human::IsRunning - konczy sprint).
static MotionSetup W3Setup(bool w3Sprint, bool weapon1H) {
	MotionSetup s;
	if (weapon1H) {
		s.styles = { "HUMANS_ZDF_W3_1H" };
		s.weaponLoco = { "HUMANS_ZDF_W3_LOCO_1H" };
		if (w3Sprint)
			s.weaponSprint = { "HUMANS_ZDF_W3_SPRINT_1H" };
	}
	s.loco = "HUMANS_ZDF_W3_LOCO";
	if (w3Sprint)
		s.sprint = "HUMANS_ZDF_W3_SPRINT";
	return s;
}
static Names Ours(const WantedSet& w) {
	Names all = w.top;
	all.insert(all.end(), w.parked.begin(), w.parked.end());
	return all;
}
static bool PlanEmpty(const OverlayPlan& p) { return p.remove.empty() && p.park.empty() && p.apply.empty(); }

TEST(motion_sprint_parked_when_idle_and_on_top_in_game_sprint) {
	WantedSet idle = WantedMotion(W3Setup(true, true), false);
	CHECK(Same(idle.top, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO" }));
	CHECK(Same(idle.parked, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H" }));
	WantedSet run = WantedMotion(W3Setup(true, true), true);
	CHECK(Same(run.top, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT",
		"HUMANS_ZDF_W3_SPRINT_1H" }));
	CHECK(run.parked.empty());
	// W3Sprint=0: zadnych nakladek sprintu, ruch W3 zostaje takze w sprincie gry (bez zbednego zdjecia)
	WantedSet off = WantedMotion(W3Setup(false, false), true);
	CHECK(Same(off.top, { "HUMANS_ZDF_W3_LOCO" }) && off.parked.empty());
}
TEST(overlay_plan_parks_sprint_on_bottom_first_time) {
	// pierwsze zalozenie (bohater stoi): sprint W3 na spod, nad bazowym modelem - nie przykrywa truchtu ani zEBA
	Names model = { "HUMANS_REMASTER", "HUMANS_ACROBATIC_REMASTER" };
	WantedSet w = WantedMotion(W3Setup(true, false), false);
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, {});
	CHECK(Same(p.park, { "HUMANS_ZDF_W3_SPRINT" }) && Same(p.apply, { "HUMANS_ZDF_W3_LOCO" }) && p.remove.empty());
	CHECK(PlanStopsAnis(model, p));
	Names after = ApplyPlan(model, p, false);
	CHECK(Same(after, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_REMASTER", "HUMANS_ACROBATIC_REMASTER", "HUMANS_ZDF_W3_LOCO" }));
	CHECK(PlanEmpty(PlanOverlays(after, w.top, w.parked, Ours(w))));
}
TEST(overlay_plan_game_sprint_start_only_moves_our_sprint) {
	// zEBA dopisal HUMANS_SPRINT_REMASTER na wierzch: nasz sprint tylko przekladamy nad niego - bez zatrzymania animacji
	Names model = { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H",
		"HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER" };
	WantedSet w = WantedMotion(W3Setup(true, true), !ForeignSprintOverlay(model).empty());
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(p.remove.empty() && p.park.empty());
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H" }));
	CHECK(!PlanStopsAnis(model, p));
	Names after = ApplyPlan(model, p, false);
	CHECK(Same(after, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER",
		"HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H" }));
	CHECK(PlanEmpty(PlanOverlays(after, w.top, w.parked, Ours(w))));
}
TEST(overlay_plan_game_sprint_end_parks_our_sprint_again) {
	// zEBA zdjal swoj sprint: nasz wraca na spod (przelozenie), trucht W3 znow z wierzchu
	Names model = { "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" };
	WantedSet w = WantedMotion(W3Setup(true, false), !ForeignSprintOverlay(model).empty());
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(Same(p.park, { "HUMANS_ZDF_W3_SPRINT" }) && p.apply.empty() && p.remove.empty());
	CHECK(!PlanStopsAnis(model, p));
	Names after = ApplyPlan(model, p, false);
	CHECK(Same(after, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }));
	CHECK(PlanEmpty(PlanOverlays(after, w.top, w.parked, Ours(w))));
}
TEST(overlay_plan_without_w3_sprint_never_covers_game_sprint) {
	// W3Sprint=0: w sprincie zEBA nic nie zdejmujemy; wymuszone przelozenie (klawisz stylu) zostaje pod sprintem gry
	Names model = { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER" };
	WantedSet w = WantedMotion(W3Setup(false, false), true);
	w.top.insert(w.top.begin(), "HUMANS_ZDF_W3_1H");
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(PlanEmpty(p));
	p.apply = w.top;   // force
	CHECK(Same(ApplyPlan(model, p, true), model));
	// wylaczenie sprintu W3 w menu: nasz sprint schodzi (jedyne zdjecie)
	p = PlanOverlays({ "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }, { "HUMANS_ZDF_W3_LOCO" }, {},
		{ "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" });
	CHECK(Same(p.remove, { "HUMANS_ZDF_W3_SPRINT" }) && p.park.empty() && p.apply.empty());
	CHECK(PlanStopsAnis({ "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }, p));
}
TEST(overlay_plan_our_sprint_goes_under_transient_overlays) {
	// gesty Znakow zQuickSign zostaja na samej gorze; nasz sprint laduje pod nimi, nad sprintem gry
	Names model = { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_SIGNS" };
	WantedSet w = WantedMotion(W3Setup(true, false), true);
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_SPRINT" }));
	Names after = ApplyPlan(model, p, false);
	CHECK(Same(after, { "HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_SIGNS" }));
	CHECK(PlanEmpty(PlanOverlays(after, w.top, w.parked, Ours(w))));
	CHECK(TopInsertIndex({ "A", "HUMANS_SPRINT", "HUMANS_ZDF_MV_DODGE_L" }, true) == 1);
	CHECK(TopInsertIndex({ "A", "HUMANS_SPRINT", "HUMANS_ZDF_MV_DODGE_L" }, false) == 2);
}
TEST(foreign_sprint_overlay_is_topmost_game_sprint) {
	CHECK(ForeignSprintOverlay({ "HUMANS_SPRINT", "HUMANS_ZDF_W3_SPRINT", "NINJA_SPRINT_DEFAULT" }) == "NINJA_SPRINT_DEFAULT");
	CHECK(ForeignSprintOverlay({ "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }).empty());
}
TEST(restorable_anis_cover_run_checks_but_not_weapon_draw) {
	// petle i przejscia, ktore sprawdza oCAniCtrl_Human::IsRunning (s_runl, t_run_2_runl, t_runl_2_runr...)
	CHECK(IsRestorableAni("S_RUNL") && IsRestorableAni("s_1hrunl") && IsRestorableAni("S_RUN"));
	CHECK(IsRestorableAni("T_RUN_2_RUNL") && IsRestorableAni("T_RUNL_2_RUN") && IsRestorableAni("T_RUNL_2_RUNR"));
	CHECK(IsRestorableAni("T_RUNR_2_RUN") && IsRestorableAni("T_1HRUN_2_1HRUNL") && IsRestorableAni("T_CBOWRUNL_2_CBOWRUN"));
	CHECK(IsRestorableAni("T_WALKBL_2_WALK") && IsRestorableAni("T_RUN_2_SNEAK") && IsRestorableAni("T_WALK_2_WALKWL"));
	// dobycie broni, skok, uzycie przedmiotu, cios - nie
	CHECK(!IsRestorableAni("T_RUN_2_1H") && !IsRestorableAni("T_1H_2_1HRUN") && !IsRestorableAni("T_MOVE_2_1HMOVE"));
	CHECK(!IsRestorableAni("T_RUNL_2_JUMP") && !IsRestorableAni("T_POTION_STAND_2_S0") && !IsRestorableAni("T_1HATTACKL"));
	CHECK(!IsRestorableAni("T_BOWRUN_2_BOWAIM") && !IsRestorableAni("C_LOOK_1") && !IsRestorableAni("R_SCRATCHHEAD"));
	CHECK(IsBlockingAni("T_RUN_2_1H", 1) && IsBlockingAni("T_MOVE_2_1HMOVE", 2) && IsBlockingAni("T_GOTHIT", 2));
	CHECK(!IsBlockingAni("T_RUN_2_RUNL", 1) && !IsBlockingAni("T_RUNTURNL", 20) && !IsBlockingAni("S_RUNL", 1));
	CHECK(!IsBlockingAni("T_DIALOGGESTURE_01", 20) && !IsBlockingAni("C_LOOK_1", 9));
}
TEST(move_loop_anis_for_diagnostics) {
	CHECK(IsMoveLoopAni("S_RUNL") && IsMoveLoopAni("S_1HWALKL") && IsMoveLoopAni("S_MAGSNEAKL") && IsMoveLoopAni("S_WALKBL"));
	CHECK(!IsMoveLoopAni("S_RUN") && !IsMoveLoopAni("S_1HRUN") && !IsMoveLoopAni("T_RUN_2_RUNL") && !IsMoveLoopAni("S_SWIMF"));
}

// ---------- Ruch bohatera: przelacznik zestawow (LocoKey / zdism loco), sprint gry, tempo sprintu W3 ----------
TEST(loco_mode_cycle_wraps_both_ways) {
	// AUTO (ini) -> Gothic/zEBA -> W3 trucht -> W3 trucht+sprint -> podglad sprintu -> AUTO
	CHECK(LOCO_MODE_COUNT == 5);
	CHECK(CycleLocoMode(LOCO_AUTO, +1) == LOCO_GAME);
	CHECK(CycleLocoMode(LOCO_GAME, +1) == LOCO_W3_RUN);
	CHECK(CycleLocoMode(LOCO_W3_RUN, +1) == LOCO_W3_FULL);
	CHECK(CycleLocoMode(LOCO_W3_FULL, +1) == LOCO_SPRINT_PREVIEW);
	CHECK(CycleLocoMode(LOCO_SPRINT_PREVIEW, +1) == LOCO_AUTO);
	CHECK(CycleLocoMode(LOCO_AUTO, -1) == LOCO_SPRINT_PREVIEW);
	CHECK(CycleLocoMode(LOCO_GAME, -1) == LOCO_AUTO);
	CHECK(CycleLocoMode(42, +1) == LOCO_AUTO);   // reczna wartosc spoza listy - od AUTO
	CHECK(CycleLocoMode(-3, -1) == LOCO_AUTO);
}
TEST(loco_mode_resolves_flags) {
	LocoFlags a = ResolveLocoMode(LOCO_AUTO, true, false);
	CHECK(a.loco && !a.sprint && !a.sprintOnTop);
	a = ResolveLocoMode(LOCO_AUTO, false, true);
	CHECK(!a.loco && a.sprint && !a.sprintOnTop);
	LocoFlags g = ResolveLocoMode(LOCO_GAME, true, true);
	CHECK(!g.loco && !g.sprint && !g.sprintOnTop);
	LocoFlags r = ResolveLocoMode(LOCO_W3_RUN, false, true);
	CHECK(r.loco && !r.sprint && !r.sprintOnTop);
	LocoFlags f = ResolveLocoMode(LOCO_W3_FULL, false, false);
	CHECK(f.loco && f.sprint && !f.sprintOnTop);
	LocoFlags p = ResolveLocoMode(LOCO_SPRINT_PREVIEW, false, false);
	CHECK(p.loco && p.sprint && p.sprintOnTop);
	LocoFlags bad = ResolveLocoMode(9, true, false);   // jak AUTO
	CHECK(bad.loco && !bad.sprint && !bad.sprintOnTop);
}
TEST(loco_mode_labels_are_distinct) {
	for (int i = 0; i < LOCO_MODE_COUNT; i++)
		for (int j = i + 1; j < LOCO_MODE_COUNT; j++)
			CHECK(std::string(LocoModeLabel(i)) != LocoModeLabel(j));
	CHECK(std::string(LocoModeLabel(99)) == LocoModeLabel(LOCO_AUTO));
}
TEST(motion_sprint_preview_puts_sprint_on_top_without_game_sprint) {
	// podglad petli sprintu: sprint W3 nad truchtem, choc gra nie sprintuje - samo przelozenie ze spodu
	WantedSet w = WantedMotion(W3Setup(true, false), true, {});
	CHECK(Same(w.top, { "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" }) && w.parked.empty());
	Names model = { "HUMANS_ZDF_W3_SPRINT", "HUMANS_REMASTER", "HUMANS_ZDF_W3_LOCO" };
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(Same(p.apply, { "HUMANS_ZDF_W3_SPRINT" }) && p.remove.empty() && p.park.empty());
	CHECK(!PlanStopsAnis(model, p));
	// tryb Gothic / zEBA: zadnych naszych nakladek ruchu - nasze schodza
	MotionSetup none;
	WantedSet g = WantedMotion(none, false, {});
	CHECK(g.top.empty() && g.parked.empty());
	p = PlanOverlays(model, g.top, g.parked, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" });
	CHECK(Same(p.remove, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }) && p.apply.empty());
}
TEST(motion_sprint_stays_parked_where_game_sprint_has_no_loop) {
	// mikstura szybkosci (HUMANS_SPRINT ma tylko S_RUNL): sprint W3 1H zostaje na spodzie, bez broni idzie na wierzch
	WantedSet w = WantedMotion(W3Setup(true, true), true, { "HUMANS_ZDF_W3_SPRINT_1H" });
	CHECK(Same(w.top, { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" }));
	CHECK(Same(w.parked, { "HUMANS_ZDF_W3_SPRINT_1H" }));
	Names model = { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H",
		"HUMANS_ZDF_W3_LOCO", "HUMANS_SPRINT" };
	OverlayPlan p = PlanOverlays(model, w.top, w.parked, Ours(w));
	CHECK(!PlanStopsAnis(model, p));
	Names after = ApplyPlan(model, p, false);
	CHECK(Same(after, { "HUMANS_ZDF_W3_SPRINT_1H", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_LOCO",
		"HUMANS_SPRINT", "HUMANS_ZDF_W3_SPRINT" }));
	CHECK(PlanEmpty(PlanOverlays(after, w.top, w.parked, Ours(w))));
	// bez sprintu gry lista "zostaje na spodzie" nic nie zmienia
	WantedSet idle = WantedMotion(W3Setup(true, true), false, { "HUMANS_ZDF_W3_SPRINT_1H" });
	CHECK(Same(idle.parked, { "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_SPRINT_1H" }));
}
TEST(sprint_tempo_scale_matches_game_sprint_speed) {
	// fpsRate petli sprintu W3 x skala = predkosc korzenia jak w petli sprintu gry (bez podwojnego przyspieszenia)
	CHECK(std::fabs(SprintTempoScale(692.0f, 692.0f) - 1.0f) < 1e-5f);
	CHECK(std::fabs(SprintTempoScale(600.0f, 500.0f) - 1.2f) < 1e-5f);
	CHECK(std::fabs(SprintTempoScale(400.0f, 500.0f) - 0.8f) < 1e-5f);
	CHECK(std::fabs(SprintTempoScale(1000.0f, 500.0f) - 2.0f) < 1e-5f);   // granice
	CHECK(std::fabs(SprintTempoScale(250.0f, 500.0f) - 0.5f) < 1e-5f);
	// niepewne (petla w miejscu, brak danych, absurdalny stosunek) - zostaje tempo wlasne
	CHECK(SprintTempoScale(0.0f, 500.0f) == 1.0f);
	CHECK(SprintTempoScale(500.0f, 0.0f) == 1.0f);
	CHECK(SprintTempoScale(2000.0f, 500.0f) == 1.0f);
	CHECK(SprintTempoScale(100.0f, 500.0f) == 1.0f);
	CHECK(SprintTempoScale(NAN, 500.0f) == 1.0f);
	CHECK(SprintTempoScale(500.0f, INFINITY) == 1.0f);
}
TEST(sprint_ani_names_from_loop) {
	CHECK(Same(SprintAniNames("S_RUNL"), { "S_RUNL", "T_RUN_2_RUNL", "T_RUNL_2_RUN" }));
	CHECK(Same(SprintAniNames("s_1hrunl"), { "S_1HRUNL", "T_1HRUN_2_1HRUNL", "T_1HRUNL_2_1HRUN" }));
	CHECK(Same(SprintAniNames("S_2HRUNL"), { "S_2HRUNL", "T_2HRUN_2_2HRUNL", "T_2HRUNL_2_2HRUN" }));
	CHECK(SprintAniNames("S_RUN").empty() && SprintAniNames("T_RUN_2_RUNL").empty() && SprintAniNames("").empty());
	CHECK(Same(SprintAniNames("S_MAGRUNL"), { "S_MAGRUNL", "T_MAGRUN_2_MAGRUNL", "T_MAGRUNL_2_MAGRUN" }));
}
TEST(our_sprint_loops_include_magic_mode) {
	// czar w reku (NPC_WEAPON_MAG): sprint W3 bez broni ma tez S_MAGRUNL, w tempie bez broni (zgloszenie 21.10)
	CHECK(Same(OurSprintLoops("HUMANS_ZDF_W3_SPRINT"), { "S_RUNL", "S_MAGRUNL" }));
	CHECK(Same(OurSprintLoops("humans_zdf_w3_sprint_1h"), { "S_1HRUNL" }));
	CHECK(Same(OurSprintLoops("HUMANS_ZDF_W3_SPRINT_2H"), { "S_2HRUNL" }));
	CHECK(OurSprintLoops("HUMANS_ZDF_W3_LOCO").empty() && OurSprintLoops("HUMANS_SPRINT_REMASTER").empty() &&
		OurSprintLoops("HUMANS_MAGESPRINT").empty());
	CHECK(SprintLoopArmed("S_1HRUNL") && SprintLoopArmed("s_2hrunl"));
	CHECK(!SprintLoopArmed("S_RUNL") && !SprintLoopArmed("S_MAGRUNL") && !SprintLoopArmed(""));
}
TEST(foreign_sprint_overlays_all_and_mod_labels) {
	CHECK(Same(ForeignSprintOverlays({ "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_W3_SPRINT", "HUMANS_REMASTER",
		"NINJA_SPRINT_ALTERNATIVE_FAST" }), { "HUMANS_SPRINT_REMASTER", "NINJA_SPRINT_ALTERNATIVE_FAST" }));
	CHECK(ForeignSprintOverlays({ "HUMANS_ZDF_W3_SPRINT", "HUMANS_ZDF_W3_LOCO" }).empty());
	CHECK(IsOurSprintOverlay("HUMANS_ZDF_W3_SPRINT_1H") && IsOurSprintOverlay("humans_zdf_w3_sprint"));
	CHECK(!IsOurSprintOverlay("HUMANS_SPRINT") && !IsOurSprintOverlay("HUMANS_ZDF_W3_LOCO"));
	CHECK(SprintModLabel("HUMANS_SPRINT_REMASTER") == "zEBA");
	CHECK(SprintModLabel("NINJA_SPRINT_ALTERNATIVE_FAST") == "Ninja Sprint");
	CHECK(SprintModLabel("humans_sprint") == "mikstura szybkosci / zEBA");
	CHECK(SprintModLabel("HUMANS_ZDF_W3_SPRINT_1H") == "W3 (zDismember)");
	CHECK(SprintModLabel("XYZ_SPRINT") == "inny mod");
}
TEST(motion_source_labels_for_screen) {
	CHECK(MotionSourceLabel("model") == "Gothic");
	CHECK(MotionSourceLabel("HUMANS_ZDF_W3_LOCO") == "W3 trucht");
	CHECK(MotionSourceLabel("HUMANS_ZDF_W3_LOCO_1H") == "W3 trucht");
	CHECK(MotionSourceLabel("HUMANS_ZDF_W3_SPRINT_2H") == "W3 sprint");
	CHECK(MotionSourceLabel("HUMANS_SPRINT_REMASTER") == "sprint zEBA");
	CHECK(MotionSourceLabel("NINJA_SPRINT_DEFAULT") == "sprint Ninja Sprint");
	CHECK(MotionSourceLabel("HUMANS_REMASTER") == "HUMANS_REMASTER");   // obca nakladka - sama nazwa
	CHECK(MotionSourceLabel("") == "-");
}
TEST(sprint_keys_from_mod_settings) {
	// zEBA SprintKeyChoice: LSHIFT|RSHIFT|LCTRL|RCTRL|z Gothic.ini (SprintKeyName)
	CHECK(ZebaSprintKey(0, "") == 0x2A && ZebaSprintKey(1, "") == 0x36);
	CHECK(ZebaSprintKey(2, "") == 0x1D && ZebaSprintKey(3, "") == 0x9D);
	CHECK(ZebaSprintKey(4, "KEY_V") == 0x2F && ZebaSprintKey(4, "") == 0 && ZebaSprintKey(9, "") == 0);
	CHECK(DikFromKeyName("key_lshift") == 0x2A && DikFromKeyName("LEWY SHIFT") == 0x2A && DikFromKeyName("Left Shift") == 0x2A);
	CHECK(DikFromKeyName("KEY_O") == 0x18 && DikFromKeyName("o") == 0x18 && DikFromKeyName("KEY_LCONTROL") == 0x1D);
	CHECK(DikFromKeyName("PRAWY SHIFT") == 0x36 && DikFromKeyName("KEY_F5") == 0x3F && DikFromKeyName("???") == 0);
	CHECK(KeyLabel(0x18) == "O" && KeyLabel(0x2A) == "LSHIFT" && KeyLabel(0) == "-" && KeyLabel(0x16) == "U");
	CHECK(KeyLabel(0x020C) == "0x20C");   // przycisk myszy z [KEYS] - bez nazwy
}
TEST(sprint_key_clashes_between_mods) {
	SprintSource zeba{ "zEBA", { 0x2A }, false, true };
	SprintSource ninja{ "Ninja Sprint", { 0x2A, 0x33 }, false, true };
	auto c = SprintKeyClashes({ zeba, ninja });
	CHECK(c.size() == 1);
	CHECK(c.size() == 1 && c[0].find("zEBA") != std::string::npos && c[0].find("Ninja Sprint") != std::string::npos &&
		c[0].find("LSHIFT") != std::string::npos);
	// Ninja_Sprint_UseKey=0000 przy Perma=1: nie sprintuje wcale
	ninja.keys.clear();
	CHECK(SprintKeyClashes({ zeba, ninja }).empty());
	// Perma=0: Ninja sprintuje przy kazdym biegu do przodu - koliduje z kazdym sprintem
	ninja.onForward = true;
	c = SprintKeyClashes({ zeba, ninja });
	CHECK(c.size() == 1 && c[0].find("biegu") != std::string::npos);
	// mod nieobecny / wylaczony - bez kolizji
	ninja.active = false;
	CHECK(SprintKeyClashes({ zeba, ninja }).empty());
	SprintSource other{ "NaturalSprint", { 0x36 }, false, true };
	CHECK(SprintKeyClashes({ zeba, other }).empty());
}

TEST(real_data_rapier_executes_dazed_with_thrust) {
	// oszolomiony (POSE_DAZED) + rapier: Director pomija finishery z odcinaniem - zostaja pchniecia W3_05_D / W3_06_D
	// (ofiara wstaje z kleczenia i dostaje pchniecie W3), nie natywne dobicie gry; bron sieczna losuje z obu rodzajow
	std::string text;
	std::vector<std::string> err;
	if (!ReadText("..\\data_w3\\System\\zDismember\\finishers_w3.ini", text))
		return;
	auto defs = ParseCatalog(text, err);
	CHECK(err.empty());
	std::vector<const FinisherDef*> chops;   // jak 'tried' w DirectorTryStart dla WeaponCut::ThrustOnly
	for (auto& d : defs)
		if (!IsThrustFinisher(d))
			chops.push_back(&d);
	for (int weapon : { WEAPON_1H, WEAPON_2H }) {
		bool seen05 = false, seen06 = false;
		for (uint32_t r = 0; r < 40; r++) {
			const FinisherDef* f = PickFinisher(defs, weapon, GROUP_HUMAN, POSE_DAZED, r, &chops);
			CHECK(f && IsThrustFinisher(*f) && (f->poses & POSE_DAZED) && f->weapon == weapon && f->paired());
			if (!f)
				continue;
			CHECK(f->wound == "BIP01 SPINE2" && f->cutTime > 1.0f);   // rana w klatce, cios po wstaniu ofiary
			seen05 = seen05 || f->id.rfind("W3_05_D", 0) == 0;
			seen06 = seen06 || f->id.rfind("W3_06_D", 0) == 0;
		}
		CHECK(seen05 && seen06);
		bool thrust = false, chop = false;
		for (uint32_t r = 0; r < 200; r++)
			if (const FinisherDef* f = PickFinisher(defs, weapon, GROUP_HUMAN, POSE_DAZED, r)) {
				thrust = thrust || IsThrustFinisher(*f);
				chop = chop || !IsThrustFinisher(*f);
			}
		CHECK(thrust && chop);
		// stojacy: rapier dalej tylko W3_05 / W3_06 (wariant _D nie wchodzi w egzekucje na stojaco)
		const FinisherDef* s = PickFinisher(defs, weapon, GROUP_HUMAN, POSE_STANDING, 3, &chops);
		CHECK(s && IsThrustFinisher(*s) && (s->poses & POSE_STANDING) && s->id.find("_D_") == std::string::npos);
	}
}

// ---------- Klawisz skoku w walce: przewrot w przod, atak z wyskoku, skok gry ----------
static JumpTapInput JT(bool press, bool fwd, bool noDir, bool attack, bool rollArmed = true, bool attackArmed = true) {
	JumpTapInput in;
	in.press = press;
	in.forwardOnly = fwd;
	in.noDir = noDir;
	in.attack = attack;
	in.rollArmed = rollArmed;
	in.attackArmed = attackArmed;
	return in;
}
TEST(jump_now_alt_w_jumps_at_once_and_second_alt_rolls) {
	// runda 7: Alt+W w walce - skok gry od razu (bez czekania), drugie Alt w oknie (0,30 s) = przewrot w przod
	JumpTapWindows w;
	RollTap s;
	JumpTapInput first = JT(true, true, false, false);
	first.down = true;
	CHECK(JumpTapNowStep(s, first, 0.016f, w) == JUMPNOW_JUMP && s.waiting && s.forward);
	CHECK(!JumpTapSwallow(s));                                   // pierwsze Alt trzymane - idzie do gry
	JumpTapInput hold = JT(false, true, false, false);
	hold.down = true;
	CHECK(JumpTapNowStep(s, hold, 0.05f, w) == JUMPNOW_WAIT && !JumpTapSwallow(s));
	CHECK(JumpTapNowStep(s, JT(false, true, false, false), 0.05f, w) == JUMPNOW_WAIT && JumpTapSwallow(s));   // puszczone
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w) == JUMPNOW_ROLL && !s.waiting);
}

TEST(jump_now_window_closes_and_late_alt_is_ignored) {
	JumpTapWindows w;
	RollTap s;
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w) == JUMPNOW_JUMP);
	CHECK(JumpTapNowStep(s, JT(false, true, false, false), 0.2f, w) == JUMPNOW_WAIT);
	CHECK(JumpTapNowStep(s, JT(false, true, false, false), 0.12f, w) == JUMPNOW_CLOSED && !s.waiting);   // 0,32 s
	// drugie Alt po oknie - nowe pierwsze Alt (kolejny skok gry), nie przewrot
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w) == JUMPNOW_JUMP);
	// drugie Alt bez przodu (puszczone W) - pominiete, okno zamkniete
	RollTap s2;
	CHECK(JumpTapNowStep(s2, JT(true, false, true, false, true, true), 0.016f, w) == JUMPNOW_JUMP && !s2.forward);
	CHECK(JumpTapNowStep(s2, JT(true, false, true, false, true, true), 0.05f, w) == JUMPNOW_NONE && !s2.waiting);
}

TEST(jump_now_attack_in_window_or_chord_is_jump_attack) {
	JumpTapWindows w;
	RollTap s;
	// sam Alt (bez kierunku, atak dostepny): skok od razu, atak w oknie 0,22 s = atak z wyskoku
	CHECK(JumpTapNowStep(s, JT(true, false, true, false, false, true), 0.016f, w) == JUMPNOW_JUMP && s.left <= w.attack);
	CHECK(JumpTapNowStep(s, JT(false, false, true, true, false, true), 0.1f, w) == JUMPNOW_ATTACK && !s.waiting);
	// Alt+W i atak w oknie
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w) == JUMPNOW_JUMP);
	CHECK(JumpTapNowStep(s, JT(false, true, false, true), 0.2f, w) == JUMPNOW_ATTACK);
	// akord (Alt i atak w tej samej klatce) - atak od razu, bez okna
	CHECK(JumpTapNowStep(s, JT(true, true, false, true), 0.016f, w) == JUMPNOW_ATTACK && !s.waiting);
	// bez ataku z wyskoku (piesci) sam Alt - zwykly skok gry bez okna; bok / tyl - poza oknem (przewrot)
	CHECK(JumpTapNowStep(s, JT(true, false, true, false, true, false), 0.016f, w) == JUMPNOW_NONE && !s.waiting);
	CHECK(JumpTapNowStep(s, JT(true, false, false, false), 0.016f, w) == JUMPNOW_NONE && !s.waiting);
}

TEST(jump_now_cancel_rules_and_drop) {
	JumpCancelLimits lim;
	JumpCancelInput in;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_BEFORE);              // skok jeszcze nie ruszyl (czeka na krok)
	in.started = in.takeoff = true;
	in.height = 45.0f;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_TAKEOFF);             // odbicie T_RUNL_2_JUMP, nisko
	in.height = 120.0f;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_LATE);                // odbicie, ale juz wysoko
	in.height = 30.0f;
	in.flight = true;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_LATE);                // S_JUMP (lot)
	in.flight = false;
	in.physics = true;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_LATE);                // fizyka lotu
	in = JumpCancelInput();
	in.started = true;
	in.height = 40.0f;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_TAKEOFF);             // stan skoku bez znanej animacji, nisko
	in.height = 60.0f;
	CHECK(JumpCancelDecide(in, lim) == JUMPCANCEL_LATE);
	CHECK(IsJumpTakeoffAni("T_RUNL_2_JUMP") && IsJumpTakeoffAni("t_1hrunr_2_jump") && IsJumpTakeoffAni("T_STAND_2_JUMP"));
	CHECK(IsJumpTakeoffAni("T_JUMPB") && IsJumpTakeoffAni("T_1HJUMPB"));
	CHECK(!IsJumpTakeoffAni("T_1HPARADEJUMPB") && !IsJumpTakeoffAni("T_STAND_2_JUMPUP") && !IsJumpTakeoffAni("S_JUMP"));
	CHECK(IsJumpFlightAni("S_JUMP") && IsJumpFlightAni("S_1HJUMP") && IsJumpFlightAni("S_FALLDN") && !IsJumpFlightAni("S_JUMPUP"));
	CHECK(!IsJumpFlightAni("T_RUNL_2_JUMP") && !IsJumpFlightAni("S_RUNL"));
	// zejscie na ziemie: 45 cm w 0,12 s plynnie (polowa czasu - polowa), potem 0
	CHECK(MoveDropLeft(45.0f, 0.0f, 0.12f) == 45.0f && std::fabs(MoveDropLeft(45.0f, 0.06f, 0.12f) - 22.5f) < 0.01f);
	CHECK(MoveDropLeft(45.0f, 0.12f, 0.12f) == 0.0f && MoveDropLeft(-5.0f, 0.0f, 0.12f) == 0.0f);
}

TEST(jump_press_latch_keeps_state_from_key_hook_before_engine_jump) {
	// runda 10 (log 134418.02): hak klawisza (AI silnika) -> silnik w tej samej klatce startuje skok (stan 8) ->
	// MovesUpdate. Okno przewrotu / ataku wg stanu z chwili wcisniecia (pierwsza obserwacja w haku), nie po starcie skoku.
	JumpPressLatch l;
	JumpPressObserve(l, true);                       // hak: bohater biegnie, ruch moze ruszyc
	JumpPressObserve(l, false);                      // kolejne odczyty w tej klatce - silnik juz w skoku
	CHECK(JumpPressCanWait(l, false, true));         // MovesUpdate: zbocze Alt, stan 8 - okno jednak otwarte
	CHECK(!JumpPressCanWait(l, false, true));        // zuzyte (nastepna klatka bez obserwacji)
	JumpPressObserve(l, false);                      // wcisniecie w powietrzu / w ataku - bez okna
	CHECK(!JumpPressCanWait(l, false, true));
	JumpPressObserve(l, true);
	CHECK(!JumpPressCanWait(l, false, false));       // bez zbocza w MovesUpdate - nic (i zuzyte)
	CHECK(!JumpPressCanWait(l, false, true));
	CHECK(JumpPressCanWait(l, true, true));          // bez haka (inna kolejnosc) - stan biezacy jak dotad
}

TEST(jump_now_round8_wider_window_and_higher_takeoff_cancel) {
	// runda 8 (Dodge.cpp): okno drugiego Alt 0,40 s (bylo 0,30 - wolniejsze podwojne stukniecie dawalo skok) i
	// przerwanie w odbiciu do 110 cm (cale T_RUNL_2_JUMP, szczyt ok. 82 cm)
	const JumpTapWindows w8 = { 0.40f, 0.30f };
	RollTap s;
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w8) == JUMPNOW_JUMP);
	CHECK(JumpTapNowStep(s, JT(false, true, false, false), 0.2f, w8) == JUMPNOW_WAIT);
	CHECK(JumpTapNowStep(s, JT(false, true, false, false), 0.15f, w8) == JUMPNOW_WAIT);    // 0,35 s - dawniej zamkniete
	CHECK(JumpTapNowStep(s, JT(true, true, false, false), 0.016f, w8) == JUMPNOW_ROLL);
	RollTap s2;
	CHECK(JumpTapNowStep(s2, JT(true, true, false, false), 0.016f, w8) == JUMPNOW_JUMP);
	CHECK(JumpTapNowStep(s2, JT(false, true, false, false), 0.42f, w8) == JUMPNOW_CLOSED);
	const JumpCancelLimits c8 = { 50.0f, 110.0f };
	JumpCancelInput in;
	in.started = in.takeoff = true;
	in.height = 100.0f;
	CHECK(JumpCancelDecide(in, c8) == JUMPCANCEL_TAKEOFF);
	CHECK(JumpCancelDecide(in, JumpCancelLimits()) == JUMPCANCEL_LATE);
	in.height = 115.0f;
	CHECK(JumpCancelDecide(in, c8) == JUMPCANCEL_LATE);
}

TEST(jump_tap_plain_alt_then_attack_is_jump_attack) {
	// sam Alt (bez kierunku) czeka na atak; atak w oknie = atak z wyskoku, skok gry przepada
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT && s.waiting);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.1f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, false, true, true), 0.016f, 0.22f) == ROLLTAP_ATTACK && !s.waiting);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.5f, 0.22f) == ROLLTAP_NONE);
}
TEST(jump_tap_plain_alt_without_attack_runs_out_into_game_jump) {
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.1f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.15f, 0.22f) == ROLLTAP_JUMP && !s.waiting);
	// drugie Alt bez W: od razu skok gry (przewrot w przod tylko z W przy pierwszym wcisnieciu)
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_JUMP);
}
TEST(jump_tap_plain_alt_passes_through_without_jump_attack) {
	// bez ataku z wyskoku (brak ruchu w ini) sam Alt to od razu skok gry - jak dotad
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false, true, false), 0.016f, 0.22f) == ROLLTAP_NONE && !s.waiting);
}
TEST(jump_tap_alt_w_waits_for_second_alt_or_attack) {
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_ROLL);
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, true), 0.05f, 0.22f) == ROLLTAP_ATTACK);
	// bez przewrotu w przod (brak ROLL_F) Alt+W tez czeka na atak, a drugie Alt to skok gry
	CHECK(JumpTapStep(s, JT(true, true, false, false, false, true), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(true, true, false, false, false, true), 0.016f, 0.22f) == ROLLTAP_JUMP);
	// bez obu - Alt+W od razu do gry
	CHECK(JumpTapStep(s, JT(true, true, false, false, false, false), 0.016f, 0.22f) == ROLLTAP_NONE);
}
TEST(jump_tap_forward_on_second_alt_also_rolls) {
	// Alt i W wcisniete prawie naraz: pierwsze Alt zlapane przed W (okno bez przodu), drugie Alt z W = przewrot w przod
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT && !s.forward);
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.05f, 0.22f) == ROLLTAP_ROLL);
	// bez przodu przy obu wcisnieciach - skok gry
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.05f, 0.22f) == ROLLTAP_JUMP);
}
TEST(turn_keys_are_not_side_while_running_forward) {
	// A/D = obrot (GAME_LEFT/RIGHT, tez mysz): w biegu do przodu to nie bok; bokiem sa strafe (Q/E)
	bool l, r;
	SideKeys(false, true, false, false, true, l, r);    // W + obrot w lewo
	CHECK(!l && !r);
	SideKeys(true, false, false, false, true, l, r);    // W + strafe w lewo
	CHECK(l && !r);
	SideKeys(false, true, false, false, false, l, r);   // sam obrot w lewo (bez W) - bok jak dotad
	CHECK(l && !r);
	SideKeys(false, false, false, true, true, l, r);    // W + obrot w prawo
	CHECK(!l && !r);
}
TEST(dodge_side_keys_count_turn_keys_even_while_running_forward) {
	// unik (klawisz uniku): A/D (obrot) i Q/E (strafe) to bok zawsze, tez z W (prosba autora 16.10) -
	// przewroty spod Alt zostaja przy SideKeys (W + obrot nie jest bokiem, zeby W + 2x Alt dawalo przewrot w przod)
	bool l, r;
	DodgeSideKeys(false, true, false, false, l, r);    // obrot w lewo (z W albo bez)
	CHECK(l && !r);
	DodgeSideKeys(false, false, false, true, l, r);    // obrot w prawo
	CHECK(!l && r);
	DodgeSideKeys(true, false, false, false, l, r);    // strafe w lewo
	CHECK(l && !r);
	DodgeSideKeys(false, false, true, false, l, r);    // strafe w prawo
	CHECK(!l && r);
	DodgeSideKeys(false, true, true, false, l, r);     // obrot w lewo + strafe w prawo - oba boki (MoveDirFromKeys: przod/tyl)
	CHECK(l && r);
	DodgeSideKeys(false, false, false, false, l, r);
	CHECK(!l && !r);
	CHECK(MoveDirFromKeys(true, false, true, false) == MOVE_LEFT);   // W + A przy uniku = unik w lewo
}
TEST(jump_tap_attack_without_window_is_ignored) {
	// zwykly atak (bez Alt) nie jest atakiem z wyskoku; Alt z bokiem / tylem to przewrot (poza oknem)
	RollTap s;
	CHECK(JumpTapStep(s, JT(false, false, true, true), 0.016f, 0.22f) == ROLLTAP_NONE);
	CHECK(JumpTapStep(s, JT(true, false, false, false), 0.016f, 0.22f) == ROLLTAP_NONE && !s.waiting);
	// atak w oknie bez ataku z wyskoku (np. piesci) - okno trwa dalej (przewrot / skok)
	CHECK(JumpTapStep(s, JT(true, true, false, false, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, true, true, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
}
TEST(roll_tap_step_keeps_old_behaviour) {
	// stara funkcja = przewrot w przod bez ataku z wyskoku
	RollTap s;
	CHECK(RollTapStep(s, true, false, 0.016f, 0.22f) == ROLLTAP_NONE);   // sam Alt - skok gry od razu
	CHECK(RollTapStep(s, true, true, 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(RollTapStep(s, true, false, 0.016f, 0.22f) == ROLLTAP_ROLL);
}
TEST(jump_key_taken_over_matches_tap_rules) {
	// gra nie widzi skoku: bok / tyl (przewrot), sam przod przy przewrocie lub ataku z wyskoku, sam Alt przy ataku
	CHECK(JumpKeyTakenOver(true, false, false, false, false, false));
	CHECK(JumpKeyTakenOver(false, false, false, true, false, false));
	CHECK(JumpKeyTakenOver(false, false, true, false, true, false));
	CHECK(JumpKeyTakenOver(false, false, true, false, false, true));
	CHECK(!JumpKeyTakenOver(false, false, true, false, false, false));
	CHECK(JumpKeyTakenOver(false, false, false, false, false, true));
	CHECK(!JumpKeyTakenOver(false, false, false, false, true, false));
	CHECK(!JumpKeyTakenOver(true, true, false, false, true, false));   // lewo + prawo = bez kierunku
	CHECK(JumpKeyTakenOver(true, true, false, false, false, true));
}

// ---------- Obrazenia repel ----------
TEST(strike_damage_is_share_of_weapon_and_strength) {
	// bark / kopniak: czesc ciosu bronia (obrazenia broni + sila); co najmniej 1 (minimum po ochronie liczy gra)
	CHECK(std::fabs(StrikeDamage(60, 40, 0.5f) - 50.0f) < 1e-4f);
	CHECK(std::fabs(StrikeDamage(60, 40, 0.35f) - 35.0f) < 1e-4f);
	CHECK(StrikeDamage(0, 0, 0.5f) == 1.0f);
	CHECK(StrikeDamage(-20, 5, 0.5f) == 1.0f);
	CHECK(StrikeDamage(100, 100, 0.0f) == 1.0f);
}

// ---------- Ochrona ruchu przed postawa silnika (oCAniCtrl_Human::_Stand przy movlocku) ----------
TEST(locomotion_ani_names_are_recognised) {
	CHECK(IsLocomotionAni("S_2HRUN"));
	CHECK(IsLocomotionAni("S_2HRUNL"));
	CHECK(IsLocomotionAni("T_2HRUNL_2_2HRUN"));
	CHECK(IsLocomotionAni("T_1HRUN_2_1HRUNL"));
	CHECK(IsLocomotionAni("S_1HWALKL"));
	CHECK(IsLocomotionAni("s_fistrun"));
	CHECK(!IsLocomotionAni("T_STUMBLEB"));
	CHECK(!IsLocomotionAni("T_2HPARADE_0"));
	CHECK(!IsLocomotionAni("T_1HATTACKL"));
	CHECK(!IsLocomotionAni("S_FALLDN"));
	CHECK(!IsLocomotionAni("T_RUN_2_JUMP"));   // skok to nie postawa
	CHECK(!IsLocomotionAni(""));
}
TEST(move_resumes_only_after_engine_stance_or_full_stop) {
	// wznowienie: nasza nakladka dalej na modelu, bohater nie dostal, na warstwie 1 tylko postawa / bieg silnika
	// (_Stand) albo nic (zalozenie / zdjecie cudzej nakladki zatrzymalo wszystko); limit prob na ruch
	CHECK(MoveResumable({ "S_2HRUN" }, false, true, 0, 3));
	CHECK(MoveResumable({ "S_2HRUN", "S_2HRUNL" }, false, true, 2, 3));
	CHECK(MoveResumable({}, false, true, 0, 3));
	CHECK(!MoveResumable({ "S_2HRUN" }, false, true, 3, 3));     // limit
	CHECK(!MoveResumable({ "S_2HRUN" }, true, true, 0, 3));      // trafienie - reakcja gry wygrywa
	CHECK(!MoveResumable({ "S_2HRUN" }, false, false, 0, 3));    // nakladka ruchu zniknela
	CHECK(!MoveResumable({ "T_STUMBLE" }, false, true, 0, 3));
	CHECK(!MoveResumable({ "S_2HRUN", "T_2HHITF" }, false, true, 0, 3));
}

// ---------- Atak z wyskoku ----------
TEST(moves_jump_attack_kind_by_weapon) {
	std::vector<std::string> err;
	auto defs = ParseMoves("[JUMPATK_F]\noverlay=A\ntraj=0,0 0,100\nhit=0.6\nreach=120\n"
		"[JUMPATK2H_F]\noverlay=B\ntraj=0,0 0,120\nhit=0.64\nreach=140\n[ROLL_F]\noverlay=C\ntraj=0,0 0,10\n", err);
	CHECK(err.empty() && defs.size() == 3);
	const MoveDef* one = FindJumpAttack(defs, false);
	const MoveDef* two = FindJumpAttack(defs, true);
	CHECK(one && one->id == "JUMPATK_F" && IsJumpAttack(*one));
	CHECK(two && two->id == "JUMPATK2H_F" && IsJumpAttack(*two));
	CHECK(!IsJumpAttack(defs[2]));
	// bez wariantu 2H - 1H dla obu broni; bez obu - brak
	auto only1 = ParseMoves("[JUMPATK_F]\noverlay=A\ntraj=0,0 0,100\nhit=0.6\n", err);
	CHECK(FindJumpAttack(only1, true) && FindJumpAttack(only1, true)->id == "JUMPATK_F");
	auto none = ParseMoves("[ROLL_F]\noverlay=C\ntraj=0,0 0,10\n", err);
	CHECK(!FindJumpAttack(none, false) && !FindJumpAttack(none, true));
}

// ---------- Warianty atakow z wypadem (z wyskoku, w biegu) ----------
static const char* kLungeVariants =
	"[JUMPATK_F]\noverlay=A\ntraj=0,0 0,100\nhit=0.5\n"
	"[JUMPATK_F2]\noverlay=B\ntraj=0,0 0,100\nhit=0.3\n"
	"[JUMPATK_F3]\noverlay=C\ntraj=0,0 0,300\nhit=0.7\n"
	"[JUMPATK_FX]\noverlay=D\ntraj=0,0 0,10\n"
	"[JUMPATK2H_F]\noverlay=E\ntraj=0,0 0,120\nhit=0.5\n"
	"[JUMPATK2H_F2]\noverlay=F\ntraj=0,0 0,150\nhit=0.7\n"
	"[RUNATK_F]\noverlay=G\ntraj=0,0 0,300\nhit=0.64\n"
	"[RUNATK_F2]\noverlay=H\ntraj=0,0 0,130\nhit=0.4\n"
	"[ROLL_F]\noverlay=I\ntraj=0,0 0,10\n";

TEST(move_variant_ids_are_base_with_number) {
	CHECK(IsMoveVariant("JUMPATK_F", "JUMPATK_F"));
	CHECK(IsMoveVariant("JUMPATK_F2", "JUMPATK_F"));
	CHECK(IsMoveVariant("JUMPATK_F12", "JUMPATK_F"));
	CHECK(!IsMoveVariant("JUMPATK_FX", "JUMPATK_F"));
	CHECK(!IsMoveVariant("JUMPATK2H_F", "JUMPATK_F"));
	CHECK(!IsMoveVariant("JUMPATK_", "JUMPATK_F"));
	CHECK(!IsMoveVariant("RUNATK_F2", "JUMPATK_F"));
}
TEST(weapon_move_variants_in_file_order_with_1h_fallback) {
	std::vector<std::string> err;
	auto defs = ParseMoves(kLungeVariants, err);
	CHECK(err.empty() && defs.size() == 9);
	auto one = JumpAttackVariants(defs, false);
	CHECK(one.size() == 3 && one[0]->id == "JUMPATK_F" && one[1]->id == "JUMPATK_F2" && one[2]->id == "JUMPATK_F3");
	auto two = JumpAttackVariants(defs, true);
	CHECK(two.size() == 2 && two[0]->id == "JUMPATK2H_F" && two[1]->id == "JUMPATK2H_F2");
	// bez wariantow 2H - 1H dla obu broni
	auto run2 = RunAttackVariants(defs, true);
	CHECK(run2.size() == 2 && run2[0]->id == "RUNATK_F" && run2[1]->id == "RUNATK_F2");
	CHECK(WeaponMoveVariants(defs, "RUNATK_F", "RUNATK2H_F", false).size() == 2);
	CHECK(FindJumpAttack(defs, true)->id == "JUMPATK2H_F" && FindRunAttack(defs, false)->id == "RUNATK_F");
	CHECK(IsRunAttack(*FindRunAttack(defs, false)) && !IsJumpAttack(*FindRunAttack(defs, false)));
	CHECK(!IsRunAttack(*FindJumpAttack(defs, false)));
	auto none = ParseMoves("[ROLL_F]\noverlay=C\ntraj=0,0 0,10\n", err);
	CHECK(RunAttackVariants(none, false).empty() && !FindRunAttack(none, true) && JumpAttackVariants(none, true).empty());
}
TEST(pick_move_variant_random_without_repeating_last) {
	std::vector<std::string> err;
	auto defs = ParseMoves(kLungeVariants, err);
	auto vars = JumpAttackVariants(defs, false);
	// bez celu: losowo, nigdy ten sam co poprzednio (przy kilku wariantach)
	int seen[3] = { 0, 0, 0 }, all[3] = { 0, 0, 0 };
	for (unsigned r = 0; r < 60; r++) {
		const MoveDef* d = PickMoveVariant(vars, {}, r, "JUMPATK_F2");
		const MoveDef* e = PickMoveVariant(vars, {}, r, "");
		CHECK(d && d->id != "JUMPATK_F2" && e);
		for (int i = 0; i < 3; i++) {
			seen[i] += d == vars[i] ? 1 : 0;
			all[i] += e == vars[i] ? 1 : 0;
		}
	}
	CHECK(seen[0] > 0 && seen[1] == 0 && seen[2] > 0);
	CHECK(all[0] > 0 && all[1] > 0 && all[2] > 0);
	// jeden wariant - zawsze on (takze po sobie); brak wariantow - nic
	std::vector<const MoveDef*> single = { vars[0] };
	CHECK(PickMoveVariant(single, {}, 7, "JUMPATK_F") == vars[0]);
	CHECK(PickMoveVariant({}, {}, 7, "") == nullptr);
	// skale nie do kazdego wariantu = jak bez celu
	CHECK(PickMoveVariant(vars, { 1.0f }, 1, "") != nullptr);
}
TEST(pick_move_variant_prefers_lunge_that_fits_target) {
	std::vector<std::string> err;
	auto defs = ParseMoves(kLungeVariants, err);
	auto vars = JumpAttackVariants(defs, false);
	// cel blisko: dlugi wypad F3 (skala 0.3 - nogi slizgalyby sie w miejscu) odpada, losowanie miedzy F i F2
	int seen[3] = { 0, 0, 0 };
	for (unsigned r = 0; r < 40; r++) {
		const MoveDef* d = PickMoveVariant(vars, { 0.9f, 1.2f, 0.3f }, r, "");
		for (int i = 0; i < 3; i++)
			seen[i] += d == vars[i] ? 1 : 0;
	}
	CHECK(seen[0] > 0 && seen[1] > 0 && seen[2] == 0);
	// pasuje jeden - zawsze on, takze gdy byl poprzednio (cel daleko: tylko dlugi wypad)
	CHECK(PickMoveVariant(vars, { 2.4f, 2.6f, 0.95f }, 5, "JUMPATK_F3") == vars[2]);
	// nie pasuje zaden - najblizszy skali 1 (proporcjonalnie: 2.0 blizej niz 0.1 i 4.0; cel za blisko = skala ujemna)
	CHECK(PickMoveVariant(vars, { 0.1f, 2.0f, 4.0f }, 3, "") == vars[1]);
	CHECK(PickMoveVariant(vars, { -0.5f, 0.4f, 6.0f }, 3, "") == vars[1]);
}
TEST(lunge_need_scale_is_unclamped_lunge_scale) {
	// cel 150 cm dalej niz zasieg, wypad 100 cm do ciosu: skala 1.5 (LungeScale przycina do maxScale, ta nie)
	CHECK(std::fabs(LungeNeedScale(150.0f, 100.0f, 0.0f) - 1.5f) < 1e-5f);
	CHECK(std::fabs(LungeNeedScale(40.0f, 100.0f, -20.0f) - 0.6f) < 1e-5f);   // odrzut zostaje
	CHECK(LungeNeedScale(-50.0f, 100.0f, 0.0f) < 0.0f);                         // cel za blisko
	CHECK(LungeNeedScale(80.0f, 0.5f, 0.0f) == 1.0f);                           // tor bez wypadu
	CHECK(std::fabs(LungeScale(150.0f, 100.0f, 0.0f, 1.7f) - LungeNeedScale(150.0f, 100.0f, 0.0f)) < 1e-5f);
	CHECK(LungeFitsWell(1.0f) && LungeFitsWell(0.6f) && LungeFitsWell(1.5f));
	CHECK(!LungeFitsWell(0.5f) && !LungeFitsWell(1.6f) && !LungeFitsWell(-1.0f));
}

// ---------- Okna klawisza skoku: dluzsze na drugie Alt, krotkie na sam atak ----------
TEST(jump_tap_windows_defaults) {
	// log sesji 15.10: 35 skokow gry na 61 ruchow, para skokow 0,33 s po sobie = drugie Alt po oknie 0,22 s
	JumpTapWindows w;
	CHECK(w.doubleTap >= 0.3f - 1e-6f && w.attack < w.doubleTap && w.attack >= 0.2f);
}
TEST(jump_tap_windows_follow_attack_setting) {
	// JumpAttackWindow: sam Alt czeka na atak tyle s; z W okno drugiego Alt / ataku co najmniej 0,40 s
	JumpTapWindows w = JumpTapWindowsFor(0.30f);
	CHECK(w.attack == 0.30f && w.doubleTap == 0.40f);
	w = JumpTapWindowsFor(0.60f);
	CHECK(w.attack == 0.60f && w.doubleTap == 0.60f);
	w = JumpTapWindowsFor(0.20f);
	CHECK(w.attack == 0.20f && w.doubleTap == 0.40f);
	// dluzsze okno: atak po 0,5 s od Alt (bez W) to jeszcze atak z wyskoku
	RollTap s;
	JumpTapInput in;
	in.attackArmed = true;
	in.noDir = true;
	in.press = true;
	CHECK(JumpTapNowStep(s, in, 0.02f, JumpTapWindowsFor(0.60f)) == JUMPNOW_JUMP);
	in.press = false;
	for (int i = 0; i < 25; i++)
		CHECK(JumpTapNowStep(s, in, 0.02f, JumpTapWindowsFor(0.60f)) == JUMPNOW_WAIT);
	in.attack = true;
	CHECK(JumpTapNowStep(s, in, 0.02f, JumpTapWindowsFor(0.60f)) == JUMPNOW_ATTACK);
}
TEST(jump_tap_second_alt_after_old_window_still_rolls) {
	JumpTapWindows w;
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, w) == ROLLTAP_WAIT && s.forward);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.1f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.1f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.05f, w) == ROLLTAP_WAIT);   // 0,25 s - stare okno minelo
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, w) == ROLLTAP_ROLL);   // drugie Alt po 0,25 s
	// bez drugiego Alt: skok gry dopiero po dluzszym oknie
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.2f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.08f, w) == ROLLTAP_WAIT);   // 0,28 s
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.05f, w) == ROLLTAP_JUMP);   // 0,33 s
}
TEST(jump_tap_attack_only_window_stays_short) {
	// sam Alt (bez kierunku) czeka tylko na atak - skok gry bez dodatkowego opoznienia
	JumpTapWindows w;
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, w) == ROLLTAP_WAIT && !s.forward);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.1f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.1f, w) == ROLLTAP_WAIT);    // 0,20 s
	CHECK(JumpTapStep(s, JT(false, false, true, false), 0.05f, w) == ROLLTAP_JUMP);   // 0,25 s > okno ataku
	// atak przy Alt+W w dluzszym oknie
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.26f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, true), 0.016f, w) == ROLLTAP_ATTACK);
}
TEST(jump_tap_w_pressed_after_alt_extends_window) {
	// Alt chwile przed W: okno bez przodu (sam atak), W w trakcie - okno jak przy Alt+W (drugie Alt = przewrot)
	JumpTapWindows w;
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, false), 0.016f, w) == ROLLTAP_WAIT && !s.forward);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.05f, w) == ROLLTAP_WAIT && s.forward);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.2f, w) == ROLLTAP_WAIT);    // 0,25 s
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, w) == ROLLTAP_ROLL);
	// bez przewrotu w przod (brak ROLL_F) W nie wydluza okna
	CHECK(JumpTapStep(s, JT(true, false, true, false, false, true), 0.016f, w) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false, false, true), 0.2f, w) == ROLLTAP_WAIT && !s.forward);
	CHECK(JumpTapStep(s, JT(false, true, false, false, false, true), 0.05f, w) == ROLLTAP_JUMP);
}
TEST(jump_tap_tracks_window_age_and_release) {
	// diagnostyka w logu: jak dlugo trwalo okno i kiedy puszczono pierwsze Alt
	JumpTapWindows w;
	RollTap s;
	JumpTapInput in = JT(true, true, false, false);
	in.down = true;
	CHECK(JumpTapStep(s, in, 0.016f, w) == ROLLTAP_WAIT && s.held < 0.0f && s.age == 0.0f);
	in = JT(false, true, false, false);
	in.down = true;
	CHECK(JumpTapStep(s, in, 0.05f, w) == ROLLTAP_WAIT && s.held < 0.0f);
	in.down = false;
	CHECK(JumpTapStep(s, in, 0.05f, w) == ROLLTAP_WAIT && std::fabs(s.held - 0.05f) < 1e-5f);
	CHECK(JumpTapStep(s, in, 0.05f, w) == ROLLTAP_WAIT && std::fabs(s.held - 0.05f) < 1e-5f);
	CHECK(std::fabs(s.age - 0.15f) < 1e-5f);
}
TEST(jump_tap_alt_and_attack_in_same_frame_is_jump_attack) {
	// Alt i atak wcisniete w tej samej klatce (akord): wczesniej okno otwieralo sie, a zbocze ataku przepadalo - skok gry
	JumpTapWindows w;
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, false, true, true), 0.016f, w) == ROLLTAP_ATTACK && !s.waiting);
	CHECK(JumpTapStep(s, JT(true, true, false, true), 0.016f, w) == ROLLTAP_ATTACK && !s.waiting);
	// bez ataku z wyskoku (piesci) - Alt+W czeka na drugie Alt jak dotad, sam Alt idzie do gry
	CHECK(JumpTapStep(s, JT(true, true, false, true, true, false), 0.016f, w) == ROLLTAP_WAIT);
	s = RollTap();
	CHECK(JumpTapStep(s, JT(true, false, true, true, true, false), 0.016f, w) == ROLLTAP_NONE);
	// Alt z bokiem / tylem (przewrot od razu, poza oknem) - atak w tej klatce nic nie zmienia
	CHECK(JumpTapStep(s, JT(true, false, false, true), 0.016f, w) == ROLLTAP_NONE && !s.waiting);
}
TEST(jump_tap_single_window_overload_keeps_old_timing) {
	// stara sygnatura (jedno okno) = oba okna rowne
	RollTap s;
	CHECK(JumpTapStep(s, JT(true, true, false, false), 0.016f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.2f, 0.22f) == ROLLTAP_WAIT);
	CHECK(JumpTapStep(s, JT(false, true, false, false), 0.05f, 0.22f) == ROLLTAP_JUMP);
}

// ---------- Kamera przy ruchu w strone kamery ----------
TEST(move_toward_camera_by_world_direction) {
	// bohater patrzy w +Z, kamera za nim (bohater -> kamera = -Z): przewrot / unik w tyl idzie w kamere
	CHECK(MoveTowardCamera(0.0f, 1.0f, 12.7f, -509.0f, 0.0f, -1.0f, 100.0f));
	CHECK(!MoveTowardCamera(0.0f, 1.0f, 0.0f, 500.0f, 0.0f, -1.0f, 100.0f));      // w przod - od kamery
	CHECK(!MoveTowardCamera(0.0f, 1.0f, -300.0f, 10.0f, 0.0f, -1.0f, 100.0f));    // w bok
	CHECK(!MoveTowardCamera(0.0f, 1.0f, 0.0f, -60.0f, 0.0f, -1.0f, 100.0f));      // za krotko
	// kamera z lewej (bohater -> kamera = -X): unik w lewo idzie w kamere
	CHECK(MoveTowardCamera(0.0f, 1.0f, -330.0f, 18.0f, -1.0f, 0.0f, 100.0f));
	// bohater patrzy w +X (prawo voba = -Z): ruch w tyl idzie w -X, kamera tam
	CHECK(MoveTowardCamera(1.0f, 0.0f, 0.0f, -300.0f, -1.0f, 0.0f, 100.0f));
	CHECK(!MoveTowardCamera(1.0f, 0.0f, 0.0f, -300.0f, 1.0f, 0.0f, 100.0f));
	// kierunek do kamery nieznormalizowany / zerowy
	CHECK(MoveTowardCamera(0.0f, 1.0f, 0.0f, -300.0f, 0.0f, -250.0f, 100.0f));
	CHECK(!MoveTowardCamera(0.0f, 1.0f, 0.0f, -300.0f, 0.0f, 0.0f, 100.0f));
}
TEST(camera_assist_samples_only_shorten_smoothing) {
	// liczba probek wygladzania kamery: najwyzej cap (krocej = kamera szybciej za bohaterem), nigdy ponizej 1
	CHECK(CameraAssistSamples(10, 4) == 4);
	CHECK(CameraAssistSamples(11, 4) == 4);
	CHECK(CameraAssistSamples(3, 4) == 3);
	CHECK(CameraAssistSamples(10, 0) == 1);
	CHECK(CameraAssistSamples(0, 4) == 1);
	CHECK(CameraAssistSamples(-5, 4) == 1);
}

// ---------- Effects (ogien przy smierci, krew, animacja smierci po finisherze) ----------
#include "Core/Effects.h"

static bool NearV(const Vec3& a, const Vec3& b, float eps = 1e-3f) {
	return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}
TEST(repeat_death_only_when_body_state_already_dead) {
	CHECK(IsRepeatDeath(23));
	CHECK(IsRepeatDeath(23 | 0x400));   // modyfikator (np. BS_MOD_BURNING) poza maska 0x7F
	CHECK(!IsRepeatDeath(22));          // nieprzytomny - pierwsza smierc (egzekucja oszolomionego)
	CHECK(!IsRepeatDeath(0));
	CHECK(!IsRepeatDeath(1));
}
TEST(burn_effect_names_match_vanilla_and_children_only) {
	CHECK(IsBurnEffectName("VOB_BURN"));
	CHECK(IsBurnEffectName("VOB_MAGICBURN"));
	CHECK(IsBurnEffectName("vob_magicburn_child3"));
	CHECK(IsBurnEffectName("SPELLFX_MAGICFIRE_HUMANSMOKE"));
	CHECK(!IsBurnEffectName("SPELLFX_FIREBOLT"));   // pocisk w locie nie gasnie ze smiercia rzucajacego
	CHECK(!IsBurnEffectName("SPELLFX_FIREBALL"));
	CHECK(!IsBurnEffectName(""));
}
TEST(emitter_dir_of_pfx_blood_points_along_local_minus_x_and_down) {
	// PFX_BLOOD: DIRANGLEHEAD 90, DIRANGLEELEV -45
	Vec3 d = EmitterDir(90.0f, -45.0f);
	CHECK(NearV(d, Vec3(-0.70711f, -0.70711f, 0.0f)));
	CHECK(NearV(EmitterDir(0.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)));
	CHECK(NearV(EmitterDir(0.0f, 90.0f), Vec3(0.0f, 1.0f, 0.0f)));
}
TEST(spray_direction_never_points_at_camera) {
	Vec3 camToHit(0.0f, -150.0f, 300.0f);   // kamera za bohaterem i wyzej, patrzy w +Z
	// cios od przodu: krew od napastnika leci w strone kamery -> odbita od kamery
	Vec3 d = SprayDirection(Vec3(0.0f, 0.0f, -1.0f), camToHit);
	CHECK(NearV(d, Vec3(0.0f, 0.0f, 1.0f)));
	// juz od kamery - bez zmian
	CHECK(NearV(SprayDirection(Vec3(0.6f, 0.0f, 0.8f), camToHit), Vec3(0.6f, 0.0f, 0.8f)));
	// w bok - zostaje w bok, ale z odrobina "od kamery"
	Vec3 s = SprayDirection(Vec3(1.0f, 0.0f, 0.0f), camToHit);
	CHECK(std::fabs(Length(s) - 1.0f) < 1e-3f);
	CHECK(s.x > 0.9f && std::fabs(s.z - 0.35f) < 1e-3f && s.y == 0.0f);
	// skos w strone kamery - skladowa od kamery dodatnia, boczna zachowana co do znaku
	Vec3 t = SprayDirection(Vec3(-0.6f, 0.5f, -0.8f), camToHit);
	CHECK(t.z >= 0.35f - 1e-3f && t.x < 0.0f && t.y == 0.0f);
	// brak preferowanego - prosto od kamery
	CHECK(NearV(SprayDirection(Vec3(), camToHit), Vec3(0.0f, 0.0f, 1.0f)));
	// kamera prosto nad punktem - preferowany poziomy
	CHECK(NearV(SprayDirection(Vec3(0.0f, 0.0f, -2.0f), Vec3(0.0f, -100.0f, 0.0f)), Vec3(0.0f, 0.0f, -1.0f)));
	for (int i = 0; i < 16; i++) {
		float a = i * 3.14159265f / 8.0f;
		Vec3 r = SprayDirection(Vec3(std::cos(a), 0.0f, std::sin(a)), camToHit);
		CHECK(Dot(r, Normalize(Flat(camToHit))) >= 0.35f - 1e-3f);
	}
}
TEST(spray_direction_with_zero_min_away_only_reflects) {
	// plama krwi (promien AddBlood): bez dociagania "od kamery", tylko odbicie skladowej w strone kamery
	Vec3 camToHit(0.0f, -150.0f, 300.0f);
	CHECK(NearV(SprayDirection(Vec3(1.0f, 0.0f, 0.0f), camToHit, 0.0f), Vec3(1.0f, 0.0f, 0.0f)));
	CHECK(NearV(SprayDirection(Vec3(0.0f, 0.0f, -1.0f), camToHit, 0.0f), Vec3(0.0f, 0.0f, 1.0f)));
	CHECK(NearV(SprayDirection(Vec3(0.6f, -3.0f, -0.8f), camToHit, 0.0f), Vec3(0.6f, 0.0f, 0.8f)));
	CHECK(NearV(SprayDirection(Vec3(0.6f, 0.0f, 0.8f), camToHit, 0.0f), Vec3(0.6f, 0.0f, 0.8f)));
}
TEST(spray_basis_turns_pfx_blood_emitter_along_direction) {
	Vec3 dir = Normalize(Vec3(0.3f, 0.0f, -0.9f));
	Mat34 m = SprayBasis(dir, Vec3(10.0f, 20.0f, 30.0f));
	CHECK(NearV(Rotate(m, Vec3(-1.0f, 0.0f, 0.0f)), dir));
	CHECK(NearV(m.Translation(), Vec3(10.0f, 20.0f, 30.0f)));
	// czysty obrot wokol pionu (wyznacznik +1)
	Vec3 x(m.m[0][0], m.m[1][0], m.m[2][0]), y(m.m[0][1], m.m[1][1], m.m[2][1]), z(m.m[0][2], m.m[1][2], m.m[2][2]);
	CHECK(NearV(y, Vec3(0.0f, 1.0f, 0.0f)));
	CHECK(std::fabs(Dot(Cross(x, y), z) - 1.0f) < 1e-4f);
	// emiter PFX_BLOOD (90, -45) w swiecie: poziomo wzdluz dir i w dol
	Vec3 w = Rotate(m, EmitterDir(90.0f, -45.0f));
	CHECK(NearV(Normalize(Flat(w)), dir));
	CHECK(w.y < -0.7f);
	// skladowa pionowa kierunku jest ignorowana
	CHECK(NearV(Rotate(SprayBasis(Vec3(0.0f, 5.0f, 2.0f), Vec3()), Vec3(-1.0f, 0.0f, 0.0f)), Vec3(0.0f, 0.0f, 1.0f)));
}
TEST(finisher_death_ani_starts_second_half_of_paired_victim) {
	FinisherDef paired;
	paired.victimOverlay = "HUMANS_ZDF_W3_06_D_V";
	paired.victimAni = "T_STAND_2_WOUNDED";
	paired.victimTraj = { 0.0f, 0.0f, 1.0f, 1.0f };
	CHECK(std::string(FinisherDeathAni(paired, POSE_DAZED)) == "T_DEAD");
	CHECK(std::string(FinisherDeathAni(paired, POSE_STANDING)) == "T_DEAD");
	FinisherDef plain;   // FIN_2H_SLASH: bez animacji ofiary
	CHECK(std::string(FinisherDeathAni(plain, POSE_STANDING)) == "T_DEADB");
	CHECK(std::string(FinisherDeathAni(plain, POSE_DAZED)).empty());
	CHECK(std::string(FinisherDeathAni(plain, POSE_LYING)).empty());
	FinisherDef overlayOnly;   // nakladka ofiary bez jej animacji - nie zgadujemy
	overlayOnly.victimOverlay = "X";
	CHECK(std::string(FinisherDeathAni(overlayOnly, POSE_DAZED)).empty());
}

// ---------- Ruchy: sledzenie celu, sterowanie, przenikanie postaci, anulowanie blokiem, atak w biegu ----------
static bool MvNear(float a, float b, float eps) { return fabsf(a - b) <= eps; }
static const float MV_PI = 3.14159265f;

TEST(move_dir_and_kind_from_id) {
	CHECK(MoveDirOfId("DODGE_L") == MOVE_LEFT);
	CHECK(MoveDirOfId("ROLL_B") == MOVE_BACK);
	CHECK(MoveDirOfId("DODGE_F") == MOVE_FORWARD);
	CHECK(MoveDirOfId("ROLL_R") == MOVE_RIGHT);
	CHECK(MoveDirOfId("JUMPATK_F2") == -1);   // atak, nie kierunek
	CHECK(MoveDirOfId("X") == -1);
	MoveDef d;
	d.id = "DODGE_B";
	CHECK(IsEvadeMove(d));
	d.id = "ROLL_F";
	CHECK(IsEvadeMove(d));
	d.id = "COUNTER_L";
	CHECK(!IsEvadeMove(d));
	d.id = "RUNATK_F";
	CHECK(!IsEvadeMove(d));
	// przod sledzi tylko na zyczenie (przewrot obok wroga = flankowanie)
	CHECK(MoveTracksTarget(MOVE_LEFT, false));
	CHECK(MoveTracksTarget(MOVE_BACK, false));
	CHECK(!MoveTracksTarget(MOVE_FORWARD, false));
	CHECK(MoveTracksTarget(MOVE_FORWARD, true));
}

TEST(angle_diff_wraps_shorter_way) {
	CHECK(MvNear(AngleDiff(0.0f, 0.5f), 0.5f, 1e-5f));
	CHECK(MvNear(AngleDiff(3.0f, -3.0f), 2.0f * MV_PI - 6.0f, 1e-4f));   // przez +-pi, nie dookola
	CHECK(MvNear(AngleDiff(-3.0f, 3.0f), -(2.0f * MV_PI - 6.0f), 1e-4f));
	CHECK(MvNear(YawTo(0.0f, 0.0f, 0.0f, 100.0f), 0.0f, 1e-5f));         // przod +Z
	CHECK(MvNear(YawTo(0.0f, 0.0f, 100.0f, 0.0f), MV_PI / 2, 1e-5f));    // prawo +X
}

TEST(track_starts_only_with_target_ahead_and_in_range) {
	TrackParams p;
	// bohater w (0,0) patrzy w +Z
	CHECK(TrackStartAllowed(0.0f, 0.0f, 0.0f, 0.0f, 300.0f, p));        // na wprost
	CHECK(TrackStartAllowed(0.0f, 0.0f, 0.0f, 300.0f, 10.0f, p));       // z boku (ok. 88 st.)
	CHECK(!TrackStartAllowed(0.0f, 0.0f, 0.0f, 0.0f, -300.0f, p));      // za plecami - bez piruetu
	CHECK(!TrackStartAllowed(0.0f, 0.0f, 0.0f, 0.0f, p.maxDist + 50.0f, p));   // za daleko
}

TEST(track_turns_toward_target_with_rate_limit) {
	TrackParams p;
	p.turnRate = 8.0f;
	bool released = false;
	// cel 45 st. w prawo, 0,05 s: najwyzej 0,4 rad
	float y = TrackYawStep(0.0f, 0.0f, 0.0f, 300.0f, 300.0f, 0.0f, 0.05f, p, released);
	CHECK(MvNear(y, 0.4f, 1e-4f));
	CHECK(!released);
	// blisko celu: dociaga dokladnie
	y = TrackYawStep(0.7f, 0.0f, 0.0f, 300.0f, 300.0f, 0.0f, 0.05f, p, released);
	CHECK(MvNear(y, MV_PI / 4, 1e-4f));
	// przesuniecie mysza: cel + offset
	y = TrackYawStep(MV_PI / 4, 0.0f, 0.0f, 300.0f, 300.0f, 0.2f, 1.0f, p, released);
	CHECK(MvNear(y, MV_PI / 4 + 0.2f, 1e-4f));
}

TEST(track_holds_when_close_and_releases_after_passing) {
	TrackParams p;
	bool released = false;
	// tuz przy celu (przenikanie) - bez obrotu
	float y = TrackYawStep(0.0f, 0.0f, 0.0f, 50.0f, 20.0f, 0.0f, 0.1f, p, released);
	CHECK(MvNear(y, 0.0f, 1e-6f));
	CHECK(!released);
	// minal cel (wrog za plecami) - koniec sledzenia, bez obrotu o 180 st.
	y = TrackYawStep(0.0f, 0.0f, 0.0f, 0.0f, -300.0f, 0.0f, 0.1f, p, released);
	CHECK(MvNear(y, 0.0f, 1e-6f));
	CHECK(released);
	// po zwolnieniu juz nie wraca
	y = TrackYawStep(0.0f, 0.0f, 0.0f, 0.0f, 300.0f, 0.0f, 0.1f, p, released);
	CHECK(MvNear(y, 0.0f, 1e-6f));
	CHECK(released);
}

TEST(track_side_dodge_circles_target) {
	// unik w bok z obrotem do celu co klatke: tor w ukladzie voba -> luk wokol celu (promien prawie staly)
	TrackParams p;
	bool released = false;
	float hx = 0.0f, hz = 0.0f, yaw = 0.0f, ex = 0.0f, ez = 250.0f;
	const float dt = 1.0f / 50.0f, speed = 330.0f;   // 3,3 m w 1 s w lewo
	for (int i = 0; i < 50; i++) {
		yaw = TrackYawStep(yaw, hx, hz, ex, ez, 0.0f, dt, p, released);
		float dirX = sinf(yaw), dirZ = cosf(yaw);
		float step = -speed * dt;   // w lewo: x ujemne w ukladzie voba
		hx += dirZ * step;          // prawo voba = (dirZ, -dirX)
		hz += -dirX * step;
	}
	float r = sqrtf((hx - ex) * (hx - ex) + (hz - ez) * (hz - ez));
	CHECK(!released);
	CHECK(r > 245.0f && r < 280.0f);           // promien bez duzego dryfu
	CHECK(hx < -150.0f);                       // bohater obszedl cel w lewo
	float arc = fabsf(AngleDiff(YawTo(ex, ez, 0.0f, 0.0f), YawTo(ex, ez, hx, hz)));
	CHECK(arc > 1.1f && arc < 1.45f);          // ok. 3,3 m luku na promieniu 2,5 m (1,32 rad)
	CHECK(fabsf(AngleDiff(yaw, YawTo(hx, hz, ex, ez))) < 0.15f);   // dalej przodem do celu
}

TEST(game_turn_matches_engine_mouse_and_keys) {
	// klawisze w walce: 0,1 * ms * 2 st. = 200 st./s
	CHECK(MvNear(GameTurnRad(1, true, 0.0f, 2.0f, 1000.0f), 200.0f * MV_PI / 180.0f, 1e-3f));
	CHECK(MvNear(GameTurnRad(-1, false, 0.0f, 2.0f, 1000.0f), -100.0f * MV_PI / 180.0f, 1e-3f));
	// mysz: arg = clamp(x * skala, +-100) * 0,1; st. = 0,1 * ms * arg
	CHECK(MvNear(GameTurnRad(0, true, 10.0f, 2.0f, 16.0f), 3.2f * MV_PI / 180.0f, 1e-4f));
	CHECK(MvNear(GameTurnRad(0, true, 1000.0f, 2.0f, 16.0f), 16.0f * MV_PI / 180.0f, 1e-4f));
	CHECK(MvNear(GameTurnRad(0, true, -1000.0f, 2.0f, 16.0f), -16.0f * MV_PI / 180.0f, 1e-4f));
	// klawisz ma pierwszenstwo przed mysza (jak PC_Turnings)
	CHECK(MvNear(GameTurnRad(-1, true, 50.0f, 2.0f, 10.0f), -2.0f * MV_PI / 180.0f, 1e-4f));
	CHECK(MvNear(GameTurnRad(0, true, 0.0f, 2.0f, 16.0f), 0.0f, 1e-6f));
}

TEST(steer_keys_held_at_move_start_do_not_turn) {
	SteerKeyGate g;
	SteerGateStart(g, true, false);          // A trzymane przy uniku w lewo (wybor kierunku)
	CHECK(SteerGateDir(g, true, false) == 0);
	CHECK(SteerGateDir(g, false, false) == 0);   // puszczone
	CHECK(SteerGateDir(g, true, false) == -1);   // wcisniete ponownie = obrot
	CHECK(SteerGateDir(g, false, true) == 1);
	CHECK(SteerGateDir(g, true, true) == 0);     // oba = nic
	SteerKeyGate h;
	SteerGateStart(h, false, false);
	CHECK(SteerGateDir(h, false, true) == 1);
}

TEST(steer_offset_with_target_is_small_and_clamped) {
	CHECK(MvNear(SteerOffsetStep(0.0f, 0.4f, 0.25f, 0.5f), 0.1f, 1e-6f));
	CHECK(MvNear(SteerOffsetStep(0.45f, 1.0f, 0.25f, 0.5f), 0.5f, 1e-6f));
	CHECK(MvNear(SteerOffsetStep(-0.45f, -1.0f, 0.25f, 0.5f), -0.5f, 1e-6f));
}

TEST(deflect_around_npc_keeps_moving_past_it) {
	// NPC w (0,100), promien 30; bohater idzie prosto w +Z przez niego (lekko z prawej)
	float px = 5.0f, pz = 60.0f, dx = 0.0f, dz = 20.0f;
	DeflectAroundCircle(px, pz, dx, dz, 0.0f, 100.0f, 30.0f, 1.0f, 0.0f);
	float l = sqrtf(dx * dx + dz * dz);
	CHECK(l > 15.0f && l <= 20.5f);                       // bez wyraznego hamowania (slizg po kole)
	float qx = px + dx, qz = pz + dz;
	CHECK(sqrtf(qx * qx + (qz - 100.0f) * (qz - 100.0f)) >= 29.9f);   // nie wchodzi w kolo
	CHECK(dx > 0.0f);                                     // omija strona, po ktorej jest
	// krok, ktory nie wchodzi w kolo - bez zmian
	dx = 0.0f; dz = 10.0f;
	DeflectAroundCircle(100.0f, 0.0f, dx, dz, 0.0f, 100.0f, 30.0f, 1.0f, 0.0f);
	CHECK(dx == 0.0f && dz == 10.0f);
	// czolowo dokladnie w srodek: strona z podpowiedzi (lewo)
	dx = 0.0f; dz = 20.0f;
	DeflectAroundCircle(0.0f, 60.0f, dx, dz, 0.0f, 100.0f, 30.0f, -1.0f, 0.0f);
	CHECK(dx < -5.0f);
	// juz w srodku (wrog wszedl w bohatera): wyjscie wolne, w glab nie
	dx = 0.0f; dz = -10.0f;
	DeflectAroundCircle(0.0f, 90.0f, dx, dz, 0.0f, 100.0f, 30.0f, 1.0f, 0.0f);
	CHECK(MvNear(dz, -10.0f, 1e-4f));
	dx = 0.0f; dz = 5.0f;
	DeflectAroundCircle(0.0f, 90.0f, dx, dz, 0.0f, 100.0f, 30.0f, 1.0f, 0.0f);
	CHECK(dz <= 1e-4f);
}

TEST(deflect_many_steps_passes_npc_and_ends_beyond) {
	// przewrot w przod obok wroga (przesuniecie 15 cm od osi): przechodzi obok i konczy za nim
	float px = 15.0f, pz = 0.0f;
	for (int i = 0; i < 40; i++) {
		float dx = 0.0f, dz = 12.0f;
		DeflectAroundCircle(px, pz, dx, dz, 0.0f, 200.0f, 30.0f, 1.0f, 0.0f);
		px += dx;
		pz += dz;
		CHECK(sqrtf(px * px + (pz - 200.0f) * (pz - 200.0f)) >= 29.5f);
	}
	CHECK(pz > 300.0f);
}

TEST(separation_pushes_end_out_of_npc) {
	float ox = 0.0f, oz = 0.0f;
	CHECK(!CircleSeparation(100.0f, 0.0f, 0.0f, 0.0f, 60.0f, 1.0f, 0.0f, ox, oz));
	CHECK(CircleSeparation(20.0f, 0.0f, 0.0f, 0.0f, 60.0f, 0.0f, 1.0f, ox, oz));
	CHECK(MvNear(ox, 40.0f, 1e-3f) && MvNear(oz, 0.0f, 1e-3f));
	// dokladnie w srodku: kierunek zapasowy
	CHECK(CircleSeparation(0.0f, 0.0f, 0.0f, 0.0f, 60.0f, 0.0f, -2.0f, ox, oz));
	CHECK(MvNear(ox, 0.0f, 1e-3f) && MvNear(oz, -60.0f, 1e-3f));
}

TEST(traj_time_at_share_of_path) {
	std::vector<float> xz = { 0, 0, 0, 10, 0, 20, 0, 30, 0, 40 };   // 10 cm na klatke, 25 kl./s
	CHECK(MvNear(TrajTimeAtShare(xz, 25.0f, 0.5f), 0.08f, 1e-4f));
	CHECK(MvNear(TrajTimeAtShare(xz, 25.0f, 1.0f), 0.16f, 1e-4f));
	CHECK(MvNear(TrajTimeAtShare(xz, 25.0f, 0.0f), 0.0f, 1e-6f));
	std::vector<float> still = { 0, 0, 0, 0 };
	CHECK(MvNear(TrajTimeAtShare(still, 25.0f, 0.5f), 0.0f, 1e-6f));
}

TEST(parade_cancel_window_after_hit_or_travel) {
	MoveDef c;
	c.hitTime = 0.68f;
	c.cancel = 1.36f;
	c.iframeTo = 0.68f;
	CHECK(MvNear(ParadeCancelFrom(c, 0.12f, 0.75f), 0.80f, 1e-4f));
	MoveDef d;   // unik: 40 cm/kl., nietykalnosc do 0,1 s, 75% drogi przy 0,12 s
	d.trajFps = 25.0f;
	d.traj = { 0, 0, -40, 0, -80, 0, -120, 0, -160, 0, -160, 0 };
	d.iframeTo = 0.1f;
	d.cancel = 1.0f;
	CHECK(MvNear(ParadeCancelFrom(d, 0.12f, 0.75f), 0.12f, 1e-4f));
	d.iframeTo = 0.3f;   // dluzsza nietykalnosc - okno dopiero po niej
	CHECK(MvNear(ParadeCancelFrom(d, 0.12f, 0.75f), 0.3f, 1e-4f));
	d.cancel = 0.2f;     // nigdy po koncu ruchu
	CHECK(ParadeCancelFrom(d, 0.12f, 0.75f) <= 0.2f);
}

TEST(parade_cancel_needs_press_during_move) {
	ParadeBuffer b;
	// blok trzymany od startu (kontra odpalona blokiem) - bez wcisniecia nic
	CHECK(!ParadeCancelStep(b, false, true, true, 0.02f, 0.35f));
	// wcisniecie przed oknem i trzymanie - anuluje, gdy okno sie otworzy
	CHECK(!ParadeCancelStep(b, true, true, false, 0.02f, 0.35f));
	for (int i = 0; i < 30; i++)
		CHECK(!ParadeCancelStep(b, false, true, false, 0.02f, 0.35f));
	CHECK(ParadeCancelStep(b, false, true, true, 0.02f, 0.35f));
	CHECK(!ParadeCancelStep(b, false, true, true, 0.02f, 0.35f));   // raz
	// klikniecie i puszczenie: pamietane przez bufor
	ParadeBuffer c;
	CHECK(!ParadeCancelStep(c, true, false, false, 0.02f, 0.35f));
	CHECK(!ParadeCancelStep(c, false, false, false, 0.2f, 0.35f));
	CHECK(ParadeCancelStep(c, false, false, true, 0.02f, 0.35f));
	ParadeBuffer e;
	CHECK(!ParadeCancelStep(e, true, false, false, 0.02f, 0.35f));
	CHECK(!ParadeCancelStep(e, false, false, false, 0.4f, 0.35f));   // za dawno
	CHECK(!ParadeCancelStep(e, false, false, true, 0.02f, 0.35f));
	// w oknie od razu
	ParadeBuffer f;
	CHECK(ParadeCancelStep(f, true, true, true, 0.02f, 0.35f));
}

TEST(run_attack_exit_and_entry_speed) {
	MoveDef d;
	d.hitTime = 0.64f;
	d.cancel = 1.32f;
	CHECK(MvNear(RunExitTime(d, 0.24f, 0.3f), 0.88f, 1e-4f));
	d.cancel = 1.0f;   // krotki ogon: najpozniej na poczatku przejscia
	CHECK(MvNear(RunExitTime(d, 0.24f, 0.3f), 0.7f, 1e-4f));
	MoveDef n;   // bez ciosu
	n.cancel = 1.0f;
	CHECK(MvNear(RunExitTime(n, 0.24f, 0.3f), 0.7f, 1e-4f));
	// predkosc toru na starcie (cm/s)
	std::vector<float> xz = { 0, 0, 0, 14, 0, 28, 0, 42, 0, 56 };   // 14 cm/kl. @25 = 350 cm/s
	CHECK(MvNear(TrajSpeedAt(xz, 25.0f, 0.0f, 0.12f), 350.0f, 0.5f));
	CHECK(MvNear(EntrySpeedRatio(500.0f, 350.0f, 1.6f), 500.0f / 350.0f, 1e-4f));
	CHECK(MvNear(EntrySpeedRatio(900.0f, 350.0f, 1.6f), 1.6f, 1e-6f));
	CHECK(MvNear(EntrySpeedRatio(200.0f, 350.0f, 1.6f), 1.0f, 1e-6f));   // wolniej niz klip - bez zwalniania
	CHECK(MvNear(EntrySpeedRatio(600.0f, 10.0f, 1.6f), 1.0f, 1e-6f));    // klip bez ruchu na starcie
	// rozped wygasa do chwili ciosu
	CHECK(MvNear(CarryScale(1.5f, 0.0f, 0.64f), 1.5f, 1e-6f));
	CHECK(MvNear(CarryScale(1.5f, 0.32f, 0.64f), 1.25f, 1e-5f));
	CHECK(MvNear(CarryScale(1.5f, 0.7f, 0.64f), 1.0f, 1e-6f));
	CHECK(MvNear(CarryScale(1.5f, 0.1f, -1.0f), 1.0f, 1e-6f));
}

TEST(real_data_parade_and_run_exit_windows_are_sane) {
	std::string text;
	if (!ReadText("..\\data_w3\\System\\zDismember\\moves_w3.ini", text)) {
		printf("  (brak data_w3 - pominiety)\n");
		return;
	}
	std::vector<std::string> errors;
	auto defs = ParseMoves(text, errors);
	for (auto& d : defs) {
		float from = ParadeCancelFrom(d, 0.12f, 0.75f);
		CHECK(from > 0.0f && from <= d.cancel);
		if (IsEvadeMove(d))
			CHECK(from >= d.iframeTo);
		if (IsRunAttack(d)) {
			float ex = RunExitTime(d, 0.24f, 0.3f);
			CHECK(ex > d.hitTime && ex < d.cancel);
		}
	}
}

// ---------- Blok (parada): iskry / drzazgi, punkt zetkniecia broni, odrzut broni i rak ----------
#include "Core/Parry.h"

static bool PyNear(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
static bool PyNearV(const Vec3& a, const Vec3& b, float eps = 1e-3f) {
	return PyNear(a.x, b.x, eps) && PyNear(a.y, b.y, eps) && PyNear(a.z, b.z, eps);
}
// Macierz obrotu wokol pionu (Y) o kat w stopniach z przesunieciem.
static Mat34 PyYaw(float deg, const Vec3& t) {
	Mat34 m = Mat34::Identity();
	RotateAround(m, Vec3(), Vec3(0.0f, 1.0f, 0.0f), deg);
	m.m[0][3] = t.x; m.m[1][3] = t.y; m.m[2][3] = t.z;
	return m;
}

TEST(parry_material_from_item_material_and_wood_tokens) {
	std::vector<std::string> wood = { "STAFF", "KEULE", "CLUB" };
	CHECK(ClassifyParryMat(false, ITEM_MAT_METAL, "ITMW_SCHWERT", "", wood) == ParryMat::NoWeapon);   // piesci, pazury
	CHECK(ClassifyParryMat(true, ITEM_MAT_METAL, "ITMW_SCHWERT", "ITMW_030_1H_SWORD_01.3DS", wood) == ParryMat::Hard);
	CHECK(ClassifyParryMat(true, ITEM_MAT_STONE, "ITMW_X", "", wood) == ParryMat::Hard);
	CHECK(ClassifyParryMat(true, ITEM_MAT_WOOD, "ITMW_1H_BAU_MACE", "ITMW_010_1H_CLUB_01.3DS", wood) == ParryMat::Wood);
	CHECK(ClassifyParryMat(true, ITEM_MAT_LEATHER, "ITMW_X", "", wood) == ParryMat::Soft);
	CHECK(ClassifyParryMat(true, ITEM_MAT_GLAS, "ITMW_X", "", wood) == ParryMat::Soft);
	CHECK(ClassifyParryMat(true, 77, "ITMW_X", "", wood) == ParryMat::Hard);   // nieznany material z modu - metal
	// mod z blednym materialem: kostur po nazwie instancji albo modelu
	CHECK(ClassifyParryMat(true, ITEM_MAT_METAL, "ITMW_MOD_MAGESTAFF", "", wood) == ParryMat::Wood);
	CHECK(ClassifyParryMat(true, ITEM_MAT_METAL, "ITMW_MOD_X", "itmw_club_2h_01.3ds", wood) == ParryMat::Wood);
	CHECK(ClassifyParryMat(true, ITEM_MAT_METAL, "ITMW_MOD_MAGESTAFF", "", {}) == ParryMat::Hard);
	CHECK(std::string(ParryMatName(ParryMat::Wood)) == "drewno");
}

TEST(parry_fx_sparks_for_two_hard_weapons_splinters_with_wood_nothing_bare_handed) {
	CHECK(ParryFx(ParryMat::Hard, ParryMat::Hard) == (PARRY_FX_SPARKS | PARRY_FX_FLASH));
	CHECK(ParryFx(ParryMat::Hard, ParryMat::Wood) == PARRY_FX_SPLINTERS);
	CHECK(ParryFx(ParryMat::Wood, ParryMat::Hard) == PARRY_FX_SPLINTERS);
	CHECK(ParryFx(ParryMat::Wood, ParryMat::Wood) == PARRY_FX_SPLINTERS);
	CHECK(ParryFx(ParryMat::Hard, ParryMat::Soft) == PARRY_FX_NONE);
	CHECK(ParryFx(ParryMat::Soft, ParryMat::Wood) == PARRY_FX_SPLINTERS);
	// bez broni po ktorejkolwiek stronie (piesci, potwory) - jak w silniku: bez czasteczek
	CHECK(ParryFx(ParryMat::NoWeapon, ParryMat::Hard) == PARRY_FX_NONE);
	CHECK(ParryFx(ParryMat::Wood, ParryMat::NoWeapon) == PARRY_FX_NONE);
}

TEST(block_parade_counts_blocks_and_w3_counters_but_not_jump_back_or_dodges) {
	CHECK(IsBlockParade("T_1HPARADE_0", "HUMANS"));
	CHECK(IsBlockParade("T_2HPARADE_0_A3", "HUMANS_ZDF_W3_2H"));
	CHECK(IsBlockParade("t_1hparade_0_a2", "humans_1hdexst2"));
	CHECK(IsBlockParade("T_FISTPARADE_0", "STONEGUARDIAN"));
	CHECK(IsBlockParade("T_1HPARADE_O", ""));
	// odskok parady Gothica i ruchy W3 na T_xHPARADEJUMPB (uniki, przewroty, ataki z wyskoku) - bez zetkniecia broni
	CHECK(!IsBlockParade("T_1HPARADEJUMPB", "HUMANS"));
	CHECK(!IsBlockParade("T_1HPARADEJUMPB", "HUMANS_1HDEXST2"));
	CHECK(!IsBlockParade("T_2HPARADEJUMPB", "HUMANS_ZDF_MV_DODGE_L_W3"));
	CHECK(!IsBlockParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_ROLL_B"));
	CHECK(!IsBlockParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_JUMPATK_F2_W3"));
	// kontra W3 zaczyna sie odbiciem bronia (riposta, bark, kopniak)
	CHECK(IsBlockParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_COUNTER_L"));
	CHECK(IsBlockParade("T_2HPARADEJUMPB", "HUMANS_ZDF_MV_COUNTER_R_W3"));
	CHECK(IsBlockParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_BASH_F_W3"));
	CHECK(IsBlockParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_KICK_F"));
	CHECK(!IsBlockParade("T_1HATTACKL", "HUMANS"));
	CHECK(!IsBlockParade("", ""));
}

TEST(closest_segment_points_crossing_parallel_and_clamped) {
	float s = -1.0f, t = -1.0f;
	// krzyz: X wzdluz osi x, drugi wzdluz y, 10 cm wyzej w z
	ClosestSegmentPoints(Vec3(0, 0, 0), Vec3(100, 0, 0), Vec3(50, -50, 10), Vec3(50, 50, 10), s, t);
	CHECK(PyNear(s, 0.5f) && PyNear(t, 0.5f));
	// rownolegle - dowolny punkt wspolnego odcinka, w zakresie [0,1]
	ClosestSegmentPoints(Vec3(0, 0, 0), Vec3(100, 0, 0), Vec3(20, 5, 0), Vec3(60, 5, 0), s, t);
	CHECK(s >= 0.0f && s <= 1.0f && t >= 0.0f && t <= 1.0f);
	CHECK(PyNear(s * 100.0f, 20.0f + t * 40.0f, 0.01f));
	// koniec drugiego odcinka poza pierwszym - przyciete do konca
	ClosestSegmentPoints(Vec3(0, 0, 0), Vec3(100, 0, 0), Vec3(150, -20, 0), Vec3(150, 20, 0), s, t);
	CHECK(PyNear(s, 1.0f) && PyNear(t, 0.5f));
	// zdegenerowany (punkt)
	ClosestSegmentPoints(Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(-10, 30, 0), Vec3(10, 30, 0), s, t);
	CHECK(PyNear(s, 0.0f) && PyNear(t, 0.5f));
}

TEST(parry_contact_point_where_blades_meet_or_on_blockers_blade) {
	Blade att{ Vec3(0, 100, 0), Vec3(1, 0, 0), 100.0f };
	Blade def{ Vec3(60, 50, 10), Vec3(0, 1, 0), 100.0f };
	// ostrza krzyzuja sie w (60,100,0)/(60,100,10) - srodek
	CHECK(PyNearV(ParryContactPoint(att, def), Vec3(60, 100, 5), 0.05f));
	// mijaja sie o 2 m - punkt na ostrzu blokujacego (fallbackT od jelca do konca)
	Blade far{ Vec3(60, 50, 210), Vec3(0, 1, 0), 100.0f };
	float guard = BladeGuard(100.0f);
	CHECK(guard > 0.0f && guard < 30.0f);
	Vec3 p = ParryContactPoint(att, far, 45.0f, 0.6f);
	CHECK(PyNearV(p, Vec3(60, 50 + guard + (100.0f - guard) * 0.6f, 210), 0.05f));
	// ostrze broniacego przy samej rekojesci napastnika: po stronie napastnika liczy sie jelec, nie dlon
	Blade def2{ Vec3(0, 50, 0), Vec3(0, 1, 0), 100.0f };
	CHECK(PyNearV(ParryContactPoint(att, def2), Vec3(guard * 0.5f, 100.0f, 0.0f), 0.05f));
}

TEST(blade_from_visual_box_or_item_range) {
	float len = 0.0f, sign = 0.0f;
	// miecz wzdluz -X slotu ZS_RIGHTHAND (jak w modelach Gothica)
	CHECK(BladeFromBox(-95.0f, 8.0f, 90, len, sign));
	CHECK(PyNear(len, 95.0f) && sign < 0.0f);
	// model odwrocony - wzdluz +X
	CHECK(BladeFromBox(-6.0f, 120.0f, 90, len, sign));
	CHECK(PyNear(len, 120.0f) && sign > 0.0f);
	// bez modelu albo dziwny bbox - z zasiegu broni (RANGE_*), przyciete
	CHECK(!BladeFromBox(0.0f, 0.0f, 70, len, sign));
	CHECK(PyNear(len, 70.0f) && sign < 0.0f);
	CHECK(!BladeFromBox(-900.0f, 0.0f, 0, len, sign));
	CHECK(PyNear(len, 90.0f));
	BladeFromBox(0.0f, 0.0f, 10, len, sign);
	CHECK(len >= 35.0f);
	BladeFromBox(0.0f, 0.0f, 400, len, sign);
	CHECK(len <= 160.0f);
}

TEST(recoil_curve_rises_to_peak_rebounds_and_settles_at_zero) {
	const float T = 0.18f;
	CHECK(RecoilCurve(0.0f, T) == 0.0f);
	CHECK(RecoilCurve(-0.1f, T) == 0.0f);
	CHECK(RecoilCurve(T, T) == 0.0f);
	CHECK(RecoilCurve(T + 1.0f, T) == 0.0f);
	CHECK(RecoilCurve(0.05f, 0.0f) == 0.0f);
	CHECK(PyNear(RecoilCurve(0.2f * T, T), 1.0f));
	float prev = 0.0f, mx = 0.0f, mn = 0.0f;
	bool rising = true;
	for (int i = 1; i < 200; i++) {
		float t = T * i / 200.0f;
		float f = RecoilCurve(t, T);
		if (t <= 0.2f * T && f < prev)
			rising = false;
		mx = std::max(mx, f);
		mn = std::min(mn, f);
		prev = f;
	}
	CHECK(rising);
	CHECK(mx <= 1.0f + 1e-4f && mx > 0.99f);
	CHECK(mn < -0.05f && mn > -0.25f);          // male odbicie w druga strone ("pogłos")
	CHECK(std::fabs(RecoilCurve(0.97f * T, T)) < 0.02f);   // gasnie gladko
	CHECK(std::fabs(RecoilCurve(0.6f * T, T)) < 0.5f);
}

TEST(recoil_push_blocker_backwards_attacker_back_and_up) {
	Vec3 d(0.0f, -30.0f, 200.0f);   // od napastnika do blokujacego (pochylosc terenu ignorowana)
	Vec3 pd = RecoilPush(d, true), pa = RecoilPush(d, false);
	CHECK(PyNear(Length(pd), 1.0f) && PyNear(Length(pa), 1.0f));
	CHECK(pd.z > 0.9f);
	CHECK(pa.z < -0.8f && pa.y > 0.2f);
	CHECK(RecoilRoleScale(false) > RecoilRoleScale(true));   // odbity cios mocniej niz blok
	CHECK(RecoilRoleDuration(true) > 0.0f && RecoilRoleDuration(false) >= RecoilRoleDuration(true));
	CHECK(PyNear(Length(RecoilPush(Vec3(), true)), 1.0f));   // stoja w jednym miejscu - jakis kierunek
}

TEST(recoil_axis_moves_contact_point_along_push) {
	Vec3 joint(0, 140, 0), contact(10, 130, 60), push(0, 0, 1);
	Vec3 a = RecoilAxis(joint, contact, push);
	CHECK(PyNear(Length(a), 1.0f));
	Mat34 m = Mat34::Identity();
	m.m[0][3] = contact.x; m.m[1][3] = contact.y; m.m[2][3] = contact.z;
	RotateAround(m, joint, a, 5.0f);
	Vec3 moved = m.Translation() - contact;
	CHECK(Dot(moved, push) > 0.0f);
	// ramie rownolegle do pchniecia albo zerowe - bez osi
	CHECK(Length(RecoilAxis(Vec3(0, 0, 0), Vec3(0, 0, 50), Vec3(0, 0, 1))) < 1e-4f);
	CHECK(Length(RecoilAxis(contact, contact, push)) < 1e-4f);
}

TEST(recoil_rotation_in_parent_frame_equals_rotation_about_model_axis) {
	// rodzic obrocony o 90 st. wokol pionu, dziecko z wlasnym obrotem i przesunieciem
	Mat34 parent = PyYaw(90.0f, Vec3(5, 100, -3));
	Mat34 local = PyYaw(30.0f, Vec3(0, 20, 15));
	RotateAround(local, local.Translation(), Vec3(1, 0, 0), 25.0f);
	Vec3 axisModel = Normalize(Vec3(0.3f, 0.2f, -0.9f));
	float deg = 7.0f;
	// silnik: obrot bazy lokalnej wokol osi w ukladzie rodzica
	Mat34 l2 = local;
	RotateBasis(l2, AxisInParent(parent, axisModel), deg);
	Mat34 viaLocal = Mul(parent, l2);
	// oczekiwane: obrot wezla w ukladzie modelu wokol jego stawu
	Mat34 world = Mul(parent, local);
	Mat34 expected = world;
	RotateAround(expected, world.Translation(), axisModel, deg);
	bool same = true;
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 4; c++)
			same = same && PyNear(viaLocal.m[r][c], expected.m[r][c], 1e-3f);
	CHECK(same);
	CHECK(PyNearV(l2.Translation(), local.Translation()));   // staw zostaje na miejscu
	// rodzic przeskalowany (scisniety kikut) - os nadal jednostkowa
	Mat34 scaled = parent;
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 3; c++)
			scaled.m[r][c] *= 0.01f;
	CHECK(PyNear(Length(AxisInParent(scaled, axisModel)), 1.0f));
}

TEST(recoil_base_survives_frames_without_engine_recompute) {
	RecoilNodeState st;
	Mat34 anim = PyYaw(10.0f, Vec3(1, 2, 3));
	Mat34 b = RecoilBase(st, anim);
	CHECK(SameMat(b, anim));
	Mat34 out = b;
	RotateBasis(out, Vec3(0, 0, 1), 6.0f);
	RecoilWritten(st, out);
	// silnik nie przeliczyl macierzy - w wezle dalej nasza z poprzedniej klatki: baza bez zmian (bez kumulacji)
	CHECK(SameMat(RecoilBase(st, out), anim));
	// nowa klatka animacji - nowa baza
	Mat34 anim2 = PyYaw(12.0f, Vec3(1, 2, 3));
	CHECK(SameMat(RecoilBase(st, anim2), anim2));
	// na koniec odrzutu przywracamy baze tylko, gdy w wezle zostala nasza macierz
	RecoilWritten(st, out);
	Mat34 restore;
	CHECK(RecoilRestore(st, out, restore) && SameMat(restore, anim2));
	CHECK(!RecoilRestore(st, anim, restore));
}

TEST(recoil_joints_cover_weapon_arm_and_both_arms_for_two_handed) {
	auto has = [](const std::vector<RecoilJoint>& v, const char* n) {
		for (auto& j : v)
			if (std::string(j.node) == n)
				return j.weight > 0.0f;
		return false;
	};
	const auto& one = RecoilJoints(false);
	const auto& two = RecoilJoints(true);
	CHECK(has(one, "BIP01 R UPPERARM") && has(one, "BIP01 R FOREARM") && has(one, "BIP01 R HAND") && has(one, "ZS_RIGHTHAND"));
	CHECK(!has(one, "BIP01 L HAND"));
	CHECK(has(two, "BIP01 L UPPERARM") && has(two, "BIP01 L FOREARM") && has(two, "BIP01 L HAND"));
	CHECK(!has(two, "ZS_RIGHTHAND"));   // 2H: slot broni sam rozerwalby chwyt lewej dloni
	CHECK(has(one, "BIP01 SPINE2") && has(two, "BIP01 SPINE2"));
	// suma wag (kat broni w szczycie = sila x suma) - kilka-kilkanascie stopni przy sile 5
	float s1 = 0.0f, s2 = 0.0f;
	for (auto& j : one) s1 += j.weight;
	for (auto& j : two) s2 += j.weight;
	CHECK(s1 > 1.5f && s1 < 4.0f && s2 > 1.5f && s2 < 4.0f);
}

TEST(parry_emitters_are_visible_one_shot_bursts) {
	EmitterSpec sp = ParrySparksSpec(), fl = ParryFlashSpec(), wd = ParrySplintersSpec();
	CHECK(sp.name == "ZDISM_PARRY_SPARKS" && fl.name == "ZDISM_PARRY_FLASH" && wd.name == "ZDISM_PARRY_SPLINTERS");
	for (const EmitterSpec* e : { &sp, &fl, &wd }) {
		CHECK(e->ppsIsLooping == 0);                       // jednorazowy wytrysk, PFX sam umiera
		CHECK(e->ppsFPS > 0.0f && !e->ppsScaleKeys.empty());
		CHECK(EmitterLifetime(*e) > 0.1f && EmitterLifetime(*e) < 3.0f);
		float w = 0.0f, h = 0.0f;
		CHECK(sscanf_s(e->visSizeStart.c_str(), "%f %f", &w, &h) == 2);
		CHECK(w >= 3.0f && h >= 3.0f);                     // silnikowe CPFX_IAI_METAL: 2x2 cm, ~8 czasteczek
	}
	CHECK(EmitterBurstCount(sp) >= 15.0f && EmitterBurstCount(sp) <= 60.0f);
	CHECK(EmitterBurstCount(wd) >= 10.0f && EmitterBurstCount(wd) <= 40.0f);
	CHECK(EmitterBurstCount(fl) >= 1.0f && EmitterBurstCount(fl) < 3.0f);
	CHECK(sp.visName == "ZSPARK1.TGA" && sp.visOrientation == "VELO" && sp.visAlphaFunc == "ADD");
	CHECK(wd.visName == "CPFX_WOOD.TGA" && wd.visAlphaFunc == "BLEND");
	CHECK(fl.visName == "CFLARESTAR.TGA" && fl.dirMode == "NONE");
	CHECK(sp.velAvg > 0.2f && wd.velAvg > 0.1f);         // cm/ms
	CHECK(sp.dirFOR == "world" && wd.dirFOR == "world");
}

// ---------- Traversal (przeskoki i wspinanie w biegu) ----------
#include <algorithm>
#include "Core/Traversal.h"
#include "Core/TravDebug.h"

// Swiat z pudelek w ukladzie bohatera (ziemia y = 0 to tez pudelko): promien trafia sciane wejscia (promienie
// zaczynajace sie w pudelku je pomijaja - jak silnik bez tylnych scian).
struct TravBox { float x0, y0, z0, x1, y1, z1; };
struct TravBoxWorld : TravWorld {
	std::vector<TravBox> boxes;
	int rays = 0;
	TravRay CastAll(float ox, float oy, float oz, float dx, float dy, float dz, float len) {
		TravRay best;
		rays++;
		for (size_t bi = 0; bi < boxes.size(); bi++) {
			auto& b = boxes[bi];
			if (ox > b.x0 && ox < b.x1 && oy > b.y0 && oy < b.y1 && oz > b.z0 && oz < b.z1)
				continue;
			float tmin = 0.0f, tmax = len;
			int axis = -1;
			float o[3] = { ox, oy, oz }, d[3] = { dx, dy, dz }, lo[3] = { b.x0, b.y0, b.z0 }, hi[3] = { b.x1, b.y1, b.z1 };
			bool miss = false;
			for (int a = 0; a < 3 && !miss; a++) {
				if (std::fabs(d[a]) < 1e-6f) {
					if (o[a] < lo[a] || o[a] > hi[a])
						miss = true;
					continue;
				}
				float t0 = (lo[a] - o[a]) / d[a], t1 = (hi[a] - o[a]) / d[a];
				if (t0 > t1)
					std::swap(t0, t1);
				if (t0 > tmin) {
					tmin = t0;
					axis = a;
				}
				if (t1 < tmax)
					tmax = t1;
				if (tmin > tmax)
					miss = true;
			}
			if (miss || axis < 0 || (best.hit && tmin >= best.dist))
				continue;
			best.hit = true;
			best.dist = tmin;
			best.normal = true;
			best.nx = axis == 0 ? (d[0] > 0 ? -1.0f : 1.0f) : 0.0f;
			best.ny = axis == 1 ? (d[1] > 0 ? -1.0f : 1.0f) : 0.0f;
			best.nz = axis == 2 ? (d[2] > 0 ? -1.0f : 1.0f) : 0.0f;
			if ((int)bi == jagged && axis == 2) {
				// poszarpana sciana skarpy: normalne odchylone (ani sciana, ani para)
				best.nx = 0.7f;
				best.ny = 0.3f;
				best.nz = -0.648f;
			}
		}
		// slupy / pnie / snopki (walce pionowe z plaska gora)
		for (auto& p : pillars) {
			float px = ox - p.x, pz = oz - p.z;
			if (px * px + pz * pz < p.r * p.r && oy > p.y0 && oy < p.y1)
				continue;
			if (std::fabs(dy) > 0.5f) {
				if (px * px + pz * pz > p.r * p.r)
					continue;
				float t = dy < 0.0f ? oy - p.y1 : p.y0 - oy;
				if (t < 0.0f || t > len || (best.hit && t >= best.dist))
					continue;
				best.hit = true;
				best.dist = t;
				best.normal = true;
				best.nx = 0.0f;
				best.ny = dy < 0.0f ? 1.0f : -1.0f;
				best.nz = 0.0f;
				continue;
			}
			float a = dx * dx + dz * dz, bq = px * dx + pz * dz, c = px * px + pz * pz - p.r * p.r;
			float disc = bq * bq - a * c;
			if (a < 1e-6f || disc < 0.0f || oy < p.y0 || oy > p.y1)
				continue;
			float t = (-bq - std::sqrt(disc)) / a;
			if (t < 0.0f || t > len || (best.hit && t >= best.dist))
				continue;
			best.hit = true;
			best.dist = t;
			best.normal = true;
			best.nx = (px + t * dx) / p.r;
			best.ny = 0.0f;
			best.nz = (pz + t * dz) / p.r;
		}
		// zerdzie (walce wzdluz x): normalna odchylona jak na okraglym drewnie
		for (auto& c : rails) {
			if (ox < c.x0 || ox > c.x1)
				continue;
			float py = oy - c.y, pz = oz - c.z;
			float b = py * dy + pz * dz, cc = py * py + pz * pz - c.r * c.r;
			float disc = b * b - cc;
			if (cc <= 0.0f || disc < 0.0f)
				continue;
			float t = -b - std::sqrt(disc);
			if (t < 0.0f || t > len || (best.hit && t >= best.dist))
				continue;
			best.hit = true;
			best.dist = t;
			best.normal = true;
			best.nx = 0.0f;
			best.ny = (oy + t * dy - c.y) / c.r;
			best.nz = (oz + t * dz - c.z) / c.r;
		}
		// teren: ziemia y = G(z) liniowo miedzy punktami (ramp), poza nimi skrajna wysokosc; promien spod ziemi jej nie widzi
		if (!ramp.empty()) {
			float t = -1.0f;
			if (dy < -0.5f) {
				float gy = Ground(oz);
				if (oy >= gy)
					t = oy - gy;
			}
			else if (dz > 0.5f && oy >= Ground(oz) - 1e-3f) {
				for (float s = 0.0f; s <= len; s += 0.5f)
					if (Ground(oz + s) > oy) {
						t = s;
						break;
					}
			}
			if (t >= 0.0f && t <= len && (!best.hit || t < best.dist)) {
				float slope = (Ground(oz + t * dz + 0.5f) - Ground(oz + t * dz - 0.5f));
				float l = std::sqrt(1.0f + slope * slope);
				best.hit = true;
				best.dist = t;
				best.normal = true;
				best.nx = 0.0f;
				best.ny = 1.0f / l;
				best.nz = -slope / l;
			}
		}
		return best;
	}
	TravRay Cast(float x, float y, float z, float dx, float dy, float dz, float len) override {
		return CastAll(x, y, z, dx, dy, dz, len);
	}
	struct Rail { float y, z, r, x0, x1; };
	std::vector<Rail> rails;
	struct Pillar { float x, z, r, y0, y1; };
	std::vector<Pillar> pillars;
	int jagged = -1;   // pudelko o poszarpanej scianie przedniej (normalne poza pionem)
	std::vector<std::pair<float, float>> ramp;   // (z, y) teren
	float Ground(float z) const {
		if (z <= ramp.front().first)
			return ramp.front().second;
		for (size_t i = 1; i < ramp.size(); i++)
			if (z <= ramp[i].first) {
				float u = (z - ramp[i - 1].first) / (ramp[i].first - ramp[i - 1].first);
				return ramp[i - 1].second + (ramp[i].second - ramp[i - 1].second) * u;
			}
		return ramp.back().second;
	}
};

static TravBoxWorld TravFloorWorld() {
	TravBoxWorld w;
	w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 2000.0f });
	return w;
}

static const char* TRAV_INI =
	"; test\n"
	"[CLIMB_100_LF]\noverlay=HUMANS_ZDF_MV_TRAV_CLIMB_100_LF\nkind=CLIMB\nbucket=100\nheight=100\nfoot=L\ntrajFps=25\n"
	"traj=0,0,0 0,0,50 0,20,100 0,60,150 0,90,180 0,100,200 0,100,210\n"
	"edge=150\ntakeoff=0.04\ncontact=0.16\ndepth=85\nexit=0.2\ncancel=0.24\n"
	"[CLIMB_100_RF]\noverlay=HUMANS_ZDF_MV_TRAV_CLIMB_100_RF\nkind=CLIMB\nbucket=100\nheight=100\nfoot=R\n"
	"traj=0,0,0 0,0,50 0,20,100 0,60,150 0,90,180 0,100,200 0,100,210\n"
	"edge=150\ntakeoff=0.04\ncontact=0.16\ndepth=110\nexit=0.2\ncancel=0.24\n"
	"[VAULT_100_LF]\noverlay=HUMANS_ZDF_MV_TRAV_VAULT_100_LF\nkind=VAULT\nbucket=100\nheight=100\nfoot=L\n"
	"traj=0,0,0 0,10,40 0,30,80 0,40,120 0,30,160 0,10,200 0,0,240 0,0,280\n"
	"edge=90\ntakeoff=0.0\ncontact=0.08\nrelease=0.12\nland=0.24\nthick=95\nexit=0.24\ncancel=0.28\n";

TEST(trav_parse_reads_climb_and_vault) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	CHECK(err.empty());
	CHECK(defs.size() == 3);
	if (defs.size() == 3) {
		CHECK(defs[0].id == "CLIMB_100_LF" && defs[0].kind == TRAV_CLIMB && defs[0].bucket == 100 && defs[0].swingLeft);
		CHECK(defs[0].traj.size() == 21 && defs[0].edge == 150.0f && defs[0].depth == 85.0f);
		CHECK(!defs[1].swingLeft);
		CHECK(defs[2].kind == TRAV_VAULT && defs[2].release == 0.12f && defs[2].land == 0.24f && defs[2].thick == 95.0f);
	}
}

TEST(trav_parse_rejects_broken_sections) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[VAULT_X]\noverlay=O\nkind=VAULT\nbucket=100\nheight=100\ntraj=0,0,0 0,0,10\nedge=50\ncontact=0.1\ncancel=1\n"
		"[CLIMB_Y]\noverlay=O\nkind=CLIMB\ntraj=0,0 0,0\nedge=50\ncancel=1\n"
		"[CLIMB_Z]\noverlay=O\nkind=JUMP\ntraj=0,0,0 0,0,10\nedge=50\ncancel=1\n"
		"[CLIMB_OK]\noverlay=O\nkind=CLIMB\nbucket=30\nheight=30\ntraj=0,0,0 0,0,10\nedge=50\ncontact=0.1\ncancel=1\n", err);
	CHECK(defs.size() == 1 && defs[0].id == "CLIMB_OK");
	CHECK(err.size() == 3);
}

TEST(trav_sample_traj3_interpolates_and_clamps) {
	std::vector<float> t = { 0, 0, 0, 10, 20, 30 };
	float x, y, z;
	SampleTraj3(t, 25.0f, 0.02f, x, y, z);
	CHECK(std::fabs(x - 5.0f) < 1e-4f && std::fabs(y - 10.0f) < 1e-4f && std::fabs(z - 15.0f) < 1e-4f);
	SampleTraj3(t, 25.0f, 5.0f, x, y, z);
	CHECK(x == 10.0f && y == 20.0f && z == 30.0f);
	SampleTraj3(t, 25.0f, -1.0f, x, y, z);
	CHECK(x == 0.0f && y == 0.0f && z == 0.0f);
}

TEST(trav_bucket_ranges) {
	TravLimits lim;
	CHECK(TravBucket(15.0f, lim) == 0);
	CHECK(TravBucket(30.0f, lim) == 30 && TravBucket(64.0f, lim) == 30);
	CHECK(TravBucket(70.0f, lim) == 100 && TravBucket(124.0f, lim) == 100);
	CHECK(TravBucket(150.0f, lim) == 150 && TravBucket(220.0f, lim) == 200 && TravBucket(260.0f, lim) == 250);
	CHECK(TravBucket(300.0f, lim) == 250 && TravBucket(350.0f, lim) == 250);   // 250 rozciagniety do maxHeight
	CHECK(TravBucket(360.0f, lim) == 0);
}

TEST(trav_detect_climbs_crate) {
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -100.0f, 0.0f, 150.0f, 100.0f, 100.0f, 350.0f });
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_CLIMB);
	CHECK(std::fabs(r.face - 150.0f) < 1.0f && std::fabs(r.height - 100.0f) < 1.0f && r.bucket == 100);
	CHECK(r.depth >= TRAV_DEEP && std::fabs(r.wallYaw) < 0.01f);
	CHECK(w.rays < 80);   // promieni na jedno wcisniecie (marsz po terenie, profil, gora, glebokosc)
}

TEST(trav_detect_shallow_ledge_is_not_climb) {
	// gora 40 cm glebokosci, za nia sciana do sufitu - ani wspiecie, ani przeskok
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -100.0f, 0.0f, 150.0f, 100.0f, 100.0f, 190.0f });
	w.boxes.push_back({ -100.0f, 0.0f, 190.0f, 100.0f, 500.0f, 260.0f });
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_NONE);
}

TEST(trav_detect_vaults_fence) {
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -150.0f, 0.0f, 100.0f, 150.0f, 105.0f, 115.0f });
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_VAULT);
	CHECK(std::fabs(r.face - 100.0f) < 1.0f && std::fabs(r.height - 105.0f) < 1.0f && r.bucket == 100);
	CHECK(r.thick > 15.0f && r.thick <= 40.0f && std::fabs(r.landY) < 1.0f);
}

TEST(trav_detect_vault_lands_lower_behind) {
	// mur 120 cm, za nim ziemia 60 cm nizej
	TravBoxWorld w;
	w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 140.0f });
	w.boxes.push_back({ -2000.0f, -160.0f, 140.0f, 2000.0f, -60.0f, 2000.0f });
	w.boxes.push_back({ -150.0f, -60.0f, 120.0f, 150.0f, 120.0f, 150.0f });
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_VAULT);
	CHECK(std::fabs(r.landY + 60.0f) < 1.0f && r.bucket == 100);
}

TEST(trav_detect_rejects_deep_drop_high_wall_step_and_post) {
	TravLimits lim;
	{
		TravBoxWorld w;   // za murem urwisko 3 m
		w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 140.0f });
		w.boxes.push_back({ -2000.0f, -400.0f, 140.0f, 2000.0f, -300.0f, 2000.0f });
		w.boxes.push_back({ -150.0f, -300.0f, 110.0f, 150.0f, 100.0f, 140.0f });
		CHECK(TravDetect(w, lim).kind == TRAV_NONE);
	}
	{
		TravBoxWorld w = TravFloorWorld();   // mur 4 m
		w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 400.0f, 200.0f });
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_NONE && std::string(r.reason) == "za wysoko");
	}
	{
		TravBoxWorld w = TravFloorWorld();   // krawleznik 15 cm
		w.boxes.push_back({ -300.0f, 0.0f, 100.0f, 300.0f, 15.0f, 400.0f });
		CHECK(TravDetect(w, lim).kind == TRAV_NONE);
	}
	{
		TravBoxWorld w = TravFloorWorld();   // slupek 20x20 cm, 120 cm - waski na wspiecie i przeskok
		w.boxes.push_back({ -10.0f, 0.0f, 100.0f, 10.0f, 120.0f, 120.0f });
		CHECK(TravDetect(w, lim).kind == TRAV_NONE);
	}
	{
		TravBoxWorld w = TravFloorWorld();   // nic przed bohaterem
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_NONE && std::string(r.reason) == "brak sciany przed bohaterem");
	}
	{
		TravBoxWorld w = TravFloorWorld();   // sciana za daleko
		w.boxes.push_back({ -300.0f, 0.0f, 600.0f, 300.0f, 100.0f, 800.0f });
		CHECK(TravDetect(w, lim).kind == TRAV_NONE);
	}
}

TEST(trav_detect_climb_needs_headroom) {
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -100.0f, 0.0f, 150.0f, 100.0f, 100.0f, 350.0f });
	w.boxes.push_back({ -100.0f, 200.0f, 140.0f, 100.0f, 260.0f, 350.0f });   // belka / strop 1 m nad gora
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_NONE);
}

// Sciana pod katem: promien wzdluz biegu trafia ja dalej niz prostopadle; kierunek w sciane = odchylenie normalnej.
struct TravSlantWorld : TravWorld {
	float D = 150.0f, a = 0.3f, H = 100.0f;
	TravRay Cast(float, float y, float z, float, float dy, float dz, float len) override {
		TravRay r;
		if (dz > 0.5f) {
			if (y < H && y >= 0.0f && z < D && D - z <= len) {
				r.hit = true;
				r.dist = D - z;
				r.normal = true;
				r.nx = std::sin(a);
				r.nz = -std::cos(a);
			}
		}
		else if (dy < -0.5f) {
			float top = z > D ? H : 0.0f;
			if (y >= top && top >= y - len) {
				r.hit = true;
				r.dist = y - top;
				r.normal = true;
				r.ny = 1.0f;
			}
		}
		return r;
	}
};

TEST(trav_detect_angled_wall_gives_perpendicular_distance_and_yaw) {
	TravSlantWorld w;
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_CLIMB);
	CHECK(std::fabs(r.face - 150.0f * std::cos(0.3f)) < 0.5f);
	CHECK(std::fabs(r.wallYaw + 0.3f) < 0.01f);   // normalna odchylona w prawo -> sciana "w lewo"
	w.a = 0.8f;   // ok. 46 st. - za skosem
	CHECK(TravDetect(w, TravLimits()).kind == TRAV_NONE);
}

TEST(trav_pick_matches_foot_kind_and_geometry) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	TravResult r;
	r.kind = TRAV_CLIMB;
	r.bucket = 100;
	r.depth = TRAV_DEEP;
	const TravDef* d = TravPick(defs, r, false);
	CHECK(d && d->id == "CLIMB_100_RF");
	d = TravPick(defs, r, true);
	CHECK(d && d->id == "CLIMB_100_LF");
	r.depth = 80.0f;   // RF chce 110 (-20) - zostaje LF mimo nogi
	d = TravPick(defs, r, false);
	CHECK(d && d->id == "CLIMB_100_LF");
	r.depth = 50.0f;
	CHECK(TravPick(defs, r, true) == nullptr);
	r.kind = TRAV_VAULT;
	r.thick = 30.0f;
	d = TravPick(defs, r, false);
	CHECK(d && d->id == "VAULT_100_LF");   // jedyny przeskok 100 - druga noga
	r.thick = 150.0f;
	CHECK(TravPick(defs, r, true) == nullptr);
	r.bucket = 150;
	r.thick = 30.0f;
	CHECK(TravPick(defs, r, true) == nullptr);
}

TEST(trav_swing_foot_from_lower_planted_foot) {
	CHECK(TravSwingLeft(40.0f, 10.0f));    // prawa nizej (podparta) - lewa w powietrzu
	CHECK(!TravSwingLeft(8.0f, 35.0f));
}

TEST(trav_plan_start_now_wait_far_close) {
	TravTiming tm;
	CHECK(TravPlanStart(150.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_NOW);
	CHECK(TravPlanStart(175.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_NOW);     // do kMax od razu
	CHECK(TravPlanStart(175.0f, 150.0f, 345.0f, true, tm) == TRAVSTART_WAIT);     // w czekaniu dopiero od kGo
	CHECK(TravPlanStart(260.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_WAIT);    // 0,27 s biegu
	CHECK(TravPlanStart(400.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_WAIT);    // 0,7 s biegu (maxWait 1 s)
	CHECK(TravPlanStart(600.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_TOO_FAR);
	CHECK(TravPlanStart(400.0f, 150.0f, 100.0f, false, tm) == TRAVSTART_TOO_FAR); // wolno - za dlugo
	CHECK(TravPlanStart(70.0f, 150.0f, 345.0f, false, tm) == TRAVSTART_TOO_CLOSE);
}

TEST(trav_sample_fits_wall_height_and_landing) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	const TravDef& c = defs[0];
	TravResult r;
	r.kind = TRAV_CLIMB;
	r.face = 130.0f;
	r.height = 115.0f;
	TravFit f = TravFitFor(c, r);
	CHECK(f.dz == -20.0f && f.dH == 15.0f);
	float x, y, z;
	TravSample(c, f, 0.0f, x, y, z);
	CHECK(std::fabs(y) < 1e-4f && std::fabs(z) < 1e-4f);                 // start w miejscu voba
	TravSample(c, f, c.takeoff, x, y, z);
	CHECK(std::fabs(y - 0.0f) < 1e-3f);                                   // przed odbiciem bez korekty wysokosci
	TravSample(c, f, c.contact, x, y, z);
	CHECK(std::fabs(z - (180.0f - 20.0f)) < 1e-3f && std::fabs(y - (90.0f + 15.0f)) < 1e-3f);
	CHECK(std::fabs(TravEndLevel(c, f) - 115.0f) < 1e-3f);                // stoi na prawdziwej gorze
	// ciaglosc - bez skokow miedzy klatkami
	float px, py, pz;
	TravSample(c, f, 0.0f, px, py, pz);
	float worst = 0.0f;
	for (int i = 1; i <= 30; i++) {
		TravSample(c, f, i * 0.01f, x, y, z);
		worst = std::max(worst, std::fabs(y - py) + std::fabs(z - pz));
		px = x, py = y, pz = z;
	}
	CHECK(worst < 25.0f);
	// przeskok: po ladowaniu wysokosc toru + ziemia za przeszkoda, gora dopasowana w oknie podparcia
	const TravDef& v = defs[2];
	r.kind = TRAV_VAULT;
	r.face = 90.0f;
	r.height = 110.0f;
	r.landY = -40.0f;
	f = TravFitFor(v, r);
	TravSample(v, f, v.contact, x, y, z);
	CHECK(std::fabs(y - (30.0f + 10.0f)) < 1e-3f);
	TravSample(v, f, v.land, x, y, z);
	CHECK(std::fabs(y - (0.0f - 40.0f)) < 1e-3f);
	CHECK(std::fabs(TravEndLevel(v, f) + 40.0f) < 1e-3f);
}

TEST(trav_real_ini_parses_and_has_every_bucket) {
	// traversal_w3.ini z make_traversal.py (paczka prywatna): 5 kubelkow x 2 nogi x wspiecie/przeskok, czasy w kolejnosci,
	// tor startuje w miejscu voba, wspiecie konczy sie na gorze, przeskok na ziemi startu
	std::string text;
	if (!ReadText("..\\data_w3\\System\\zDismember\\traversal_w3.ini", text))
		return;
	std::vector<std::string> err;
	auto defs = ParseTraversal(text, err);
	CHECK(defs.size() == 32 && err.empty());   // 20 z biegu + 12 z miejsca (CLIMB_IDLE_30..300, VAULT_IDLE_30..300)
	int idle = 0;
	for (auto& d : defs)
		idle += d.idle ? 1 : 0;
	CHECK(idle == 12);
	for (auto& d : defs) {
		CHECK(d.takeoff <= d.contact && d.contact < d.exit + 1e-3f && d.exit <= d.cancel);
		CHECK(d.edge > 25.0f && d.edge < 260.0f);
		float x, y, z;
		SampleTraj3(d.traj, d.trajFps, 0.0f, x, y, z);
		CHECK(std::fabs(x) < 0.5f && std::fabs(y) < 0.5f && std::fabs(z) < 0.5f);
		TravFit f;
		float end = TravEndLevel(d, f);
		CHECK(d.kind == TRAV_CLIMB ? std::fabs(end - d.height) < 2.0f : std::fabs(end) < 2.0f);
		CHECK(d.kind == TRAV_CLIMB ? d.depth > 30.0f : (d.thick > 15.0f && d.release >= d.contact && d.land <= d.exit + 1e-3f));
	}
	for (TravKind k : { TRAV_CLIMB, TRAV_VAULT })
		for (int b : { 30, 100, 150, 200, 250 })
			for (bool left : { true, false }) {
				TravResult r;
				r.kind = k;
				r.bucket = b;
				r.depth = TRAV_DEEP;
				r.thick = 20.0f;
				const TravDef* d = TravPick(defs, r, left);
				CHECK(d && d->swingLeft == left && !d->idle);
			}
	// z miejsca: klip o najblizszej wysokosci, takze 300 cm
	for (TravKind k : { TRAV_CLIMB, TRAV_VAULT })
		for (int b : { 30, 100, 150, 200, 250, 300 }) {
			TravResult r;
			r.kind = k;
			r.height = (float)b + 8.0f;
			r.bucket = TravBucket(r.height, TravLimits());
			r.depth = TRAV_DEEP;
			r.thick = 20.0f;
			const TravDef* d = TravPick(defs, r, true, true);
			CHECK(d && d->idle && d->bucket == b && r.bucket == b && d->kind == k);
		}
}

TEST(trav_real_clips_fit_boxes_without_entering_them) {
	// Caly lancuch pluginu na prawdziwych klipach: pudelko o wysokosci kubelka (+-15 cm) i sciana w roznej odleglosci ->
	// wykrycie, wybor klipu, start, dopasowanie toru; srodek postaci (vob ~ miednica, 100 cm nad torem) nigdy nie
	// wchodzi w przeszkode (kontrola drogi w pluginie przerwalaby ruch), wspiecie konczy sie na gorze, przeskok na ziemi.
	std::string text;
	if (!ReadText("..\\data_w3\\System\\zDismember\\traversal_w3.ini", text))
		return;
	std::vector<std::string> err;
	auto defs = ParseTraversal(text, err);
	TravLimits lim;
	TravTiming tm;
	int runs = 0, bad = 0;
	for (int bucket : { 30, 100, 150, 200, 250 })
		for (TravKind kind : { TRAV_CLIMB, TRAV_VAULT })
			for (float dh : bucket == 250 ? std::vector<float>{ -12.0f, 0.0f, 12.0f, 45.0f, 68.0f, 85.0f, 98.0f } : std::vector<float>{ -12.0f, 0.0f, 12.0f })
				for (float kf : { 0.8f, 1.0f, 1.15f }) {
					float H = (bucket == 30 ? 40.0f : (float)bucket) + dh;
					float thick = kind == TRAV_CLIMB ? 400.0f : (bucket == 30 ? 25.0f : 30.0f);
					for (bool left : { true, false }) {
						TravResult probe;
						probe.kind = kind;
						probe.bucket = TravBucket(H, lim);
						probe.depth = TRAV_DEEP;
						probe.thick = thick + 12.0f;
						const TravDef* d = TravPick(defs, probe, left);
						CHECK(d != nullptr);
						if (!d)
							continue;
						float face = d->edge * kf;
						TravBoxWorld w = TravFloorWorld();
						w.boxes.push_back({ -150.0f, 0.0f, face, 150.0f, H, face + thick });
						TravResult r = TravDetect(w, lim);
						CHECK(r.kind == kind && r.bucket == d->bucket);
						if (r.kind != kind || r.bucket != d->bucket) {
							printf("  %s H %.0f face %.0f: %s\n", d->id.c_str(), H, face, TravDescribe(r).c_str());
							continue;
						}
						const TravDef* p = TravPick(defs, r, left);
						CHECK(p == d);
						CHECK(TravPlanStart(r.face, d->edge, 345.0f, false, tm) == TRAVSTART_NOW);
						TravFit f = TravFitFor(*d, r);
						runs++;
						bool inside = false;
						for (float t = 0.0f; t <= d->cancel; t += 0.01f) {
							float x, y, z;
							TravSample(*d, f, t, x, y, z);
							float py = y + 100.0f;
							if (z > face + 3.0f && z < face + thick - 3.0f && py < H - 3.0f)
								inside = true;
						}
						float end = TravEndLevel(*d, f);
						bool ends = kind == TRAV_CLIMB ? std::fabs(end - H) < 1.0f : std::fabs(end) < 1.0f;
						if (inside || !ends) {
							bad++;
							printf("  %s H %.0f face %.0f: %s\n", d->id.c_str(), H, face, inside ? "w przeszkodzie" : "zly koniec");
						}
					}
				}
	CHECK(runs > 100);
	CHECK(bad == 0);
}

TEST(trav_real_clips_trajectory_and_fit_have_no_jumps) {
	// runda 10 (nagranie 16.10 23:49: model ok. 1 m do tylu w 1,15-1,3 s CLIMB_200_RF): tor voba z danych (co 1/25 s)
	// i tor po dopasowaniu w pluginie (TravSample: rozbieg, sciana, wysokosc, dach / stok, ladowanie - co 0,01 s) bez
	// skoku w calym klipie, we wszystkich sekcjach traversal_w3.ini (skok szukany w danych - brak: przyczyna poza torem).
	std::string text;
	if (!ReadText("..\\data_w3\\System\\zDismember\\traversal_w3.ini", text))
		return;
	std::vector<std::string> err;
	auto defs = ParseTraversal(text, err);
	CHECK(defs.size() >= 30);
	for (const TravDef& d : defs) {
		size_t n = d.traj.size() / 3;
		float worst = 0.0f;
		for (size_t i = 1; i < n; i++) {
			float dx = d.traj[i * 3] - d.traj[i * 3 - 3], dy = d.traj[i * 3 + 1] - d.traj[i * 3 - 2],
				dz = d.traj[i * 3 + 2] - d.traj[i * 3 - 1];
			worst = std::fmax(worst, std::sqrt(dx * dx + dy * dy + dz * dz));
		}
		CHECK(worst <= 50.0f);   // najszybszy (opadanie po przeskoku 250) ok. 44 cm / klatke 25 fps
		for (float fdz : { -25.0f, 25.0f }) {
			TravFit f;
			f.dz = fdz;
			f.dH = 12.0f;
			f.base = 8.0f;
			f.landY = d.kind == TRAV_VAULT ? -30.0f : 0.0f;
			f.endAdj = d.kind == TRAV_CLIMB ? 80.0f : 0.0f;   // dach: koniec 80 cm wyzej
			float px = 0.0f, py = 0.0f, pz = 0.0f, step = 0.0f, at = 0.0f;
			for (float t = 0.0f; t <= d.cancel + 0.3f; t += 0.01f) {
				float x, y, z;
				TravSample(d, f, t, x, y, z);
				if (t > 0.0f) {
					float s = std::sqrt((x - px) * (x - px) + (y - py) * (y - py) + (z - pz) * (z - pz));
					if (s > step) {
						step = s;
						at = t;
					}
				}
				px = x;
				py = y;
				pz = z;
			}
			CHECK(step <= 22.0f);   // 0,01 s: najwyzej 2200 cm/s (opadanie + dopasowania)
			if (step > 22.0f || worst > 50.0f)
				printf("  %s: tor %.1f cm/kl., po dopasowaniu %.1f cm / 0,01 s w %.2f s\n", d.id.c_str(), worst, step, at);
		}
	}
}

TEST(trav_real_idle_clips_fit_boxes_from_standing) {
	// Z miejsca (bohater stoi 40-130 cm od sciany, klipy W3 zakladaja 26-57 cm): wykrycie, klip o najblizszej wysokosci
	// (300 = prawdziwy klip 300), start od razu, tor dochodzi do sciany i nie wchodzi w przeszkode, koniec na gorze /
	// na ziemi. Dalej niz idleMaxFace - za daleko (skok gry).
	std::string text;
	if (!ReadText("..\\data_w3\\System\\zDismember\\traversal_w3.ini", text))
		return;
	std::vector<std::string> err;
	auto defs = ParseTraversal(text, err);
	TravLimits lim;
	TravTiming tm;
	lim.maxFace = tm.idleLook;
	int runs = 0, bad = 0;
	for (int bucket : { 30, 100, 150, 200, 250, 300 })
		for (TravKind kind : { TRAV_CLIMB, TRAV_VAULT })
			for (float face : { 40.0f, 80.0f, 125.0f }) {
				float H = (bucket == 30 ? 40.0f : (float)bucket) + 6.0f;
				float thick = kind == TRAV_CLIMB ? 400.0f : 30.0f;
				TravBoxWorld w = TravFloorWorld();
				w.boxes.push_back({ -150.0f, 0.0f, face, 150.0f, H, face + thick });
				TravResult r = TravDetect(w, lim);
				const TravDef* d = r.kind == kind ? TravPickChecked(w, defs, r, true, lim, nullptr, true) : nullptr;
				CHECK(d && d->idle && d->bucket == bucket);
				if (!d) {
					printf("  idle %d %s face %.0f: %s\n", bucket, kind == TRAV_CLIMB ? "CLIMB" : "VAULT", face, TravDescribe(r).c_str());
					continue;
				}
				CHECK(TravIdleStart(r.face, tm) == TRAVSTART_NOW);
				TravFit f = TravFitFor(*d, r);
				runs++;
				bool inside = false;
				for (float t = 0.0f; t <= d->cancel; t += 0.01f) {
					float x, y, z;
					TravSample(*d, f, t, x, y, z);
					if (z > face + 3.0f && z < face + thick - 3.0f && y + 100.0f < H - 3.0f)
						inside = true;
				}
				float end = TravEndLevel(*d, f);
				bool ends = kind == TRAV_CLIMB ? std::fabs(end - H) < 1.0f : std::fabs(end) < 1.0f;
				if (inside || !ends) {
					bad++;
					printf("  %s H %.0f face %.0f: %s\n", d->id.c_str(), H, face, inside ? "w przeszkodzie" : "zly koniec");
				}
			}
	CHECK(runs >= 30);
	CHECK(bad == 0);
	CHECK(TravIdleStart(tm.idleMaxFace + 10.0f, tm) == TRAVSTART_TOO_FAR);
}

TEST(trav_parse_start_idle_and_run_pick_ignores_idle) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(std::string(TRAV_INI) +
		"[CLIMB_IDLE_100]\noverlay=HUMANS_ZDF_MV_TRAV_CLIMB_IDLE_100\nkind=CLIMB\nbucket=100\nheight=100\nfoot=L\nstart=IDLE\n"
		"traj=0,0,0 0,20,20 0,60,40 0,100,60 0,100,90\nedge=45\ntakeoff=0.04\ncontact=0.12\ndepth=70\nexit=0.16\ncancel=0.2\n", err);
	CHECK(err.empty() && defs.size() == 4 && defs[3].idle && !defs[0].idle);
	TravResult r;
	r.kind = TRAV_CLIMB;
	r.bucket = 100;
	r.height = 104.0f;
	r.depth = TRAV_DEEP;
	const TravDef* run = TravPick(defs, r, true);
	const TravDef* idle = TravPick(defs, r, true, true);
	CHECK(run && !run->idle && idle && idle->id == "CLIMB_IDLE_100");
	CHECK(TravDecisionLabel(r, idle) == "WSPIECIE Z MIEJSCA 100 -> CLIMB_IDLE_100");
	auto bad = ParseTraversal(std::string(TRAV_INI) + "[X]\nkind=CLIMB\nstart=FLY\n", err);
	CHECK(bad.size() == 3);
}

// Pochylnia ok. 18 st. od z = 50 do sciany (podworko przy murze): ziemia wznosi sie ku scianie.
static TravBoxWorld TravSlopeWorld(float wallZ, float wallAboveBase) {
	TravBoxWorld w;
	const float slope = 0.325f;   // tan 18 st.
	float base = (wallZ - 50.0f) * slope;
	w.ramp = { { -2000.0f, 0.0f }, { 50.0f, 0.0f }, { wallZ, base }, { 3000.0f, base } };
	w.boxes.push_back({ -300.0f, -200.0f, wallZ, 300.0f, base + wallAboveBase, wallZ + 400.0f });
	return w;
}

TEST(trav_detect_follows_rising_ground_to_wall_base) {
	// poziome promienie z ziemi bohatera trafialyby stok (log: "brak sciany") - marsz po terenie mierzy od podstawy
	for (float above : { 120.0f, 250.0f }) {
		TravBoxWorld w = TravSlopeWorld(250.0f, above);
		std::string log;
		TravResult r = TravDetect(w, TravLimits(), &log);
		CHECK(r.kind == TRAV_CLIMB);
		CHECK(std::fabs(r.base - 61.75f) < 3.0f);                       // ziemia u podstawy (10 cm przed sciana)
		CHECK(std::fabs(r.height - (above + 65.0f - r.base)) < 2.0f);    // gora mierzona od podstawy
		CHECK(r.bucket == (above < 200.0f ? 100 : 250));
		CHECK(r.groundZ.size() >= 4 && r.groundY.back() > 50.0f);        // probki terenu po drodze
		CHECK(log.find("teren(z:y)") != std::string::npos && log.find("profil") != std::string::npos);
		if (r.kind != TRAV_CLIMB)
			printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	}
}

TEST(trav_detect_ignores_bare_slope_and_small_stone) {
	{
		// sam stok 30 st. (ny 0,87) bez sciany - nie sciana
		TravBoxWorld w;
		w.ramp = { { -2000.0f, 0.0f }, { 100.0f, 0.0f }, { 1000.0f, 520.0f } };
		CHECK(TravDetect(w, TravLimits()).kind == TRAV_NONE);
	}
	{
		// kamien 15 cm po drodze (jeden promien) nie zatrzymuje szukania - skrzynia 120 cm dalej
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -20.0f, 0.0f, 100.0f, 20.0f, 15.0f, 130.0f });
		w.boxes.push_back({ -150.0f, 0.0f, 300.0f, 150.0f, 120.0f, 600.0f });
		TravResult r = TravDetect(w, TravLimits());
		CHECK(r.kind == TRAV_CLIMB && std::fabs(r.face - 300.0f) < 1.0f && std::fabs(r.height - 120.0f) < 1.0f);
		CHECK(std::fabs(r.base - 15.0f) > 10.0f);   // podstawa przy skrzyni, nie na kamieniu
	}
}

TEST(trav_sample_runs_up_the_slope_without_digging_in) {
	// rozbieg toru po terenie: do odbicia vob nad stokiem tak jak klip nad plaska ziemia, na koncu gora od podstawy
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	TravBoxWorld w = TravSlopeWorld(150.0f, 100.0f);
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_CLIMB);
	const TravDef* d = TravPick(defs, r, true);
	CHECK(d != nullptr);
	if (!d)
		return;
	TravFit f = TravFitFor(*d, r);
	for (float t = 0.0f; t <= d->takeoff + 1e-4f; t += 0.01f) {
		float x, y, z, cx, cy, cz;
		TravSample(*d, f, t, x, y, z);
		SampleTraj3(d->traj, d->trajFps, t, cx, cy, cz);
		CHECK(std::fabs(y - cy - TravGround(f, z)) < 0.5f);
	}
	CHECK(std::fabs(TravEndLevel(*d, f) - r.top) < 1.0f);
}

TEST(trav_detect_vaults_round_rail_fence) {
	// plot z okraglych zerdzi (30 i 78 cm, promien 6), przerwy pod i miedzy nimi, slupki co 150 cm obok osi
	TravBoxWorld w = TravFloorWorld();
	w.rails.push_back({ 30.0f, 110.0f, 6.0f, -400.0f, 400.0f });
	w.rails.push_back({ 78.0f, 110.0f, 6.0f, -400.0f, 400.0f });
	w.boxes.push_back({ -80.0f, 0.0f, 104.0f, -70.0f, 92.0f, 116.0f });
	w.boxes.push_back({ 70.0f, 0.0f, 104.0f, 80.0f, 92.0f, 116.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_VAULT);
	CHECK(std::fabs(r.height - 84.0f) < 2.0f && r.bucket == 100);
	CHECK(std::fabs(r.landY) < 1.0f && r.thick <= 45.0f);
	if (r.kind != TRAV_VAULT)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	// zerdz trafiona nad osia (normalna odchylona do gory o ok. 40 st.) nadal jest sciana
	TravRay hit = w.Forward(0.0f, 82.0f, 0.0f, 300.0f);
	CHECK(hit.hit && hit.ny > 0.5f && hit.ny < 0.75f);
}

TEST(trav_detect_ledge_under_top_and_true_height) {
	TravLimits lim;
	{
		// mur 280 cm z wystepem 15 cm na 230-250 cm - wspiecie 250 (rozciagniete), gora od podstawy
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 280.0f, 350.0f });
		w.boxes.push_back({ -300.0f, 230.0f, 135.0f, 300.0f, 250.0f, 150.0f });
		std::string log;
		TravResult r = TravDetect(w, lim, &log);
		CHECK(r.kind == TRAV_CLIMB && r.bucket == 250 && std::fabs(r.height - 280.0f) < 1.0f);
		CHECK(std::fabs(r.face - 135.0f) < 1.0f || std::fabs(r.face - 150.0f) < 1.0f);
		if (r.kind != TRAV_CLIMB)
			printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	}
	{
		// mur 360 cm: za wysoko, ale log zna prawdziwa gore
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 360.0f, 350.0f });
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_NONE && std::string(r.reason) == "za wysoko" && std::fabs(r.height - 360.0f) < 1.0f);
		CHECK(TravDescribe(r).find("360 cm") != std::string::npos);
	}
	{
		// mur 450 cm: ponad skan
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 450.0f, 350.0f });
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_NONE && r.scanTop >= 390.0f && TravDescribe(r).find("ponad") != std::string::npos);
	}
}

TEST(trav_detect_far_wall_within_lookahead) {
	// wczesne wcisniecie w sprincie: sciana 6 m przed bohaterem w zasiegu wg predkosci
	TravTiming tm;
	TravLimits lim;
	lim.maxFace = TravLookahead(692.0f, 193.0f, tm);
	CHECK(lim.maxFace == tm.lookMax);
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 600.0f, 300.0f, 100.0f, 900.0f });
	TravResult r = TravDetect(w, lim);
	CHECK(r.kind == TRAV_CLIMB && std::fabs(r.face - 600.0f) < 1.0f);
	CHECK(TravPlanStart(r.face, 151.0f, 692.0f, false, tm) == TRAVSTART_WAIT);
	CHECK(std::fabs(TravLookahead(345.0f, 193.0f, tm) - 538.0f) < 0.5f);
	CHECK(TravLookahead(0.0f, 193.0f, tm) == tm.lookMin);
}

TEST(trav_buffer_waits_starts_and_aborts_into_game_jump) {
	TravTiming tm;
	TravBufferInput in;
	const char* why = "";
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_KEEP);
	in.plan = TRAVSTART_NOW;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_START);
	in.plan = TRAVSTART_WAIT;
	in.time = 1.3f;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT && std::string(why) == "za dlugo");
	in.time = 0.2f;
	in.turn = 0.8f;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT);
	in.turn = 0.1f;
	in.forwardKey = false;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT && std::string(why) == "puszczone W");
	in.forwardKey = true;
	in.running = false;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT);
	in.running = true;
	in.obstacleOk = false;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT);
	in.obstacleOk = true;
	in.plan = TRAVSTART_TOO_CLOSE;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT);
	in.plan = TRAVSTART_TOO_FAR;
	CHECK(TravBufferDecide(in, tm, why) == TRAVBUF_ABORT);
}

TEST(trav_pick_falls_back_to_vault_when_climb_has_no_clip) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	TravResult r;
	r.kind = TRAV_CLIMB;
	r.bucket = 100;
	r.depth = 55.0f;      // LF chce 85 - 20
	r.vaultOk = true;
	r.thick = 40.0f;
	const TravDef* d = TravPick(defs, r, true);
	CHECK(d && d->id == "VAULT_100_LF" && r.kind == TRAV_VAULT);
	r.kind = TRAV_CLIMB;
	r.vaultOk = false;
	CHECK(TravPick(defs, r, true) == nullptr);
	r.depth = TRAV_DEEP;
	r.clear = 50.0f;      // sciana tuz za krawedzia (miejsce na postac za male)
	CHECK(TravPick(defs, r, true) == nullptr);
}

TEST(trav_detect_climbs_wall_up_to_350) {
	// mur przy kuzni Harada (330 cm) i 345 cm: wspiecie 250 rozciagniete; 360 cm dalej za wysoko
	TravLimits lim;
	for (float h : { 330.0f, 345.0f }) {
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -300.0f, 0.0f, 200.0f, 300.0f, h, 500.0f });
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_CLIMB && r.bucket == 250 && std::fabs(r.height - h) < 1.0f);
	}
}

TEST(trav_detect_embankment_with_rounded_lip) {
	// nasyp: kamienna sciana 100 cm, nad nia warga 45 st. do 130 cm (30 cm w glab), potem 26 st. do 140 i plasko -
	// gora szukana dalej za warga, wysokosc tam, gdzie bohater stanie, koniec toru na prawdziwej gorze
	TravBoxWorld w;
	w.ramp = { { -2000.0f, 0.0f }, { 200.0f, 0.0f }, { 200.01f, 100.0f }, { 230.0f, 130.0f }, { 250.0f, 140.0f },
		{ 3000.0f, 140.0f } };
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_CLIMB);
	CHECK(r.height > 125.0f && r.height < 141.0f && r.bucket == 150);
	CHECK(log.find("za warga") != std::string::npos);
	if (r.kind != TRAV_CLIMB)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[CLIMB_150_LF]\noverlay=O\nkind=CLIMB\nbucket=150\nheight=150\nfoot=L\ntraj=0,0,0 0,100,150 0,150,220 0,150,260\n"
		"edge=200\ntakeoff=0.04\ncontact=0.08\ndepth=60\nexit=0.1\ncancel=0.6\n", err);
	const TravDef* d = TravPick(defs, r, true);
	CHECK(d != nullptr);
	if (d) {
		TravFit f = TravFitFor(*d, r);
		CHECK(std::fabs(TravEndLevel(*d, f) - 140.0f) < 1.5f);   // stoi na plaskiej gorze (140), nie na wardze
	}
	// warga zbyt stroma i za nisko pod gora (druga sciana za krawedzia) - nie gora
	TravBoxWorld w2;
	w2.ramp = { { -2000.0f, 0.0f }, { 200.0f, 0.0f }, { 200.01f, 60.0f }, { 240.0f, 60.0f }, { 240.01f, 140.0f },
		{ 3000.0f, 140.0f } };
	TravResult r2 = TravDetect(w2, TravLimits());
	CHECK(!(r2.kind == TRAV_CLIMB && r2.height > 100.0f && r2.height < 141.0f && std::fabs(r2.face - 200.0f) < 2.0f &&
		r2.topZ.size() > 0 && r2.topZ[0] < 225.0f));
}

// Dom: sciana 200 cm (z = 200), od krawedzi dach w gore pod katem (tan k) - teren rampy (promien spod dachu go nie widzi).
static TravBoxWorld TravRoofWorld(float k) {
	TravBoxWorld w;
	w.ramp = { { -2000.0f, 0.0f }, { 200.0f, 0.0f }, { 200.01f, 200.0f }, { 1200.0f, 200.0f + 1000.0f * k } };
	return w;
}

TEST(trav_detect_climbs_onto_walkable_roof_behind_edge) {
	// runda 8 (20-23.png): sciana domu, za krawedzia dach 35 / 42 st. - wspiecie (gora = krawedz, pochyla), dach nie jest
	// "sciana za krawedzia", koniec toru na wysokosci dachu w punkcie konca (podparcie)
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[CLIMB_200_LF]\noverlay=O\nkind=CLIMB\nbucket=200\nheight=200\nfoot=L\ntraj=0,0,0 0,150,150 0,200,230 0,200,280\n"
		"edge=200\ntakeoff=0.04\ncontact=0.08\ndepth=60\nexit=0.1\ncancel=0.6\n", err);
	CHECK(err.empty() && defs.size() == 1);
	for (float k : { 0.70f, 0.90f }) {
		TravBoxWorld w = TravRoofWorld(k);
		std::string log;
		TravLimits lim;
		TravResult r = TravDetect(w, lim, &log);
		CHECK(r.kind == TRAV_CLIMB && r.slope);
		CHECK(std::fabs(r.height - 200.0f) < 6.0f && r.bucket == 200);
		CHECK(r.depth >= lim.climbMinDepth && r.clear >= lim.climbMinClear);
		CHECK(log.find("pochyla") != std::string::npos);
		if (r.kind != TRAV_CLIMB)
			printf("  dach %.2f: %s\n  %s\n", k, TravDescribe(r).c_str(), log.c_str());
		if (defs.size() != 1)
			continue;
		std::string plog;
		const TravDef* d = TravPickChecked(w, defs, r, true, lim, &plog, false);
		CHECK(d != nullptr);
		if (d) {
			TravFit f = TravFitFor(*d, r);
			float roofAtEnd = 200.0f + 80.0f * k;   // koniec 80 cm za krawedzia
			CHECK(f.endAdj > 40.0f);
			CHECK(std::fabs(TravEndLevel(*d, f) - roofAtEnd) < 4.0f);
		}
		else
			printf("  dach %.2f: %s\n", k, plog.c_str());
	}
	// dach 60 st. - nie do chodzenia: bez wspiecia
	TravBoxWorld steep = TravRoofWorld(1.73f);
	CHECK(TravDetect(steep, TravLimits()).kind != TRAV_CLIMB);
}

// Plyta pochyla (dach na skrzyniach): gorna powierzchnia y = py0 + (z - pz0) * pk dla z w [pz0, pz1]; promien z gory
// trafia ja, spod niej / ze srodka nie (bez tylnych scian), poziomy - tam, gdzie powierzchnia osiaga jego wysokosc.
struct TravPlateWorld : TravBoxWorld {
	float pz0 = 0.0f, pz1 = 0.0f, py0 = 0.0f, pk = 0.0f, px0 = -200.0f, px1 = 200.0f;
	TravRay Cast(float x, float y, float z, float dx, float dy, float dz, float len) override {
		TravRay r = CastAll(x, y, z, dx, dy, dz, len);
		if (x < px0 || x > px1)
			return r;
		float l = std::sqrt(1.0f + pk * pk), t = -1.0f;
		if (dy < -0.5f && z >= pz0 && z <= pz1) {
			float top = py0 + (z - pz0) * pk;
			if (y >= top)
				t = y - top;
		}
		else if (dz > 0.5f && pk > 1e-3f) {
			float zh = pz0 + (y - py0) / pk;
			if (zh >= pz0 && zh <= pz1 && zh >= z)
				t = zh - z;
		}
		if (t >= 0.0f && t <= len && (!r.hit || t < r.dist)) {
			r.hit = true;
			r.dist = t;
			r.normal = true;
			r.nx = 0.0f;
			r.ny = 1.0f / l;
			r.nz = -pk / l;
		}
		return r;
	}
};

TEST(trav_detect_steep_roof_plate_over_crate_top_is_followed) {
	// runda 12 (tor, dach 45 st.: "glebokosc 85 (pochyla, -12)", "brak podparcia na koncu ruchu (ziemia 247)"): pierwszy
	// promien glebokosci startowal pod plyta dachu i bral gore skrzyn pod nia - wyzsza ciagla powierzchnia wygrywa
	TravPlateWorld w;
	w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 2000.0f });
	w.boxes.push_back({ -120.0f, 0.0f, 200.0f, 120.0f, 200.0f, 300.0f });   // skrzynie 2 wysokie, 1 gleboka
	w.pz0 = 200.0f;
	w.pz1 = 341.0f;
	w.py0 = 200.0f;
	w.pk = 1.0f;   // 45 st.
	std::string log;
	TravLimits lim;
	TravResult r = TravDetect(w, lim, &log);
	CHECK(r.kind == TRAV_CLIMB && r.slope);
	CHECK(!r.topY.empty() && r.topY.back() > r.top + 60.0f);   // profil idzie po plycie w gore, nie po skrzyniach
	if (r.kind != TRAV_CLIMB)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[CLIMB_200_LF]\noverlay=O\nkind=CLIMB\nbucket=200\nheight=200\nfoot=L\ntraj=0,0,0 0,150,150 0,200,230 0,200,270\n"
		"edge=200\ntakeoff=0.04\ncontact=0.08\ndepth=60\nexit=0.1\ncancel=0.6\n", err);
	std::string plog;
	const TravDef* d = TravPickChecked(w, defs, r, true, lim, &plog, false);
	CHECK(d != nullptr);
	if (d)
		CHECK(std::fabs(TravEndLevel(*d, TravFitFor(*d, r)) - 270.0f) < 6.0f);   // stoi na plycie 70 cm za krawedzia
	else
		printf("  %s\n", plog.c_str());
}

TEST(trav_pick_refuses_end_under_overhang_also_from_standing) {
	// runda 12 (nawis z miejsca: "wolne 63" -> CLIMB_IDLE_100, glowa w murze): mur 60 cm za krawedzia, dolem 43 cm nad
	// gora - wykrywanie przepuszcza (wolne >= 40), koniec ruchu pod murem - odmowa z biegu i z miejsca
	auto world = [](bool wall) {
		TravBoxWorld w = TravFloorWorld();
		w.boxes.push_back({ -120.0f, 0.0f, 200.0f, 120.0f, 100.0f, 400.0f });
		if (wall)
			w.boxes.push_back({ -175.0f, 143.0f, 260.0f, 175.0f, 443.0f, 360.0f });
		return w;
	};
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[CLIMB_100_LF]\noverlay=O\nkind=CLIMB\nbucket=100\nheight=100\nfoot=L\ntraj=0,0,0 0,60,100 0,100,150 0,100,202\n"
		"edge=150\ntakeoff=0.04\ncontact=0.16\ndepth=60\nexit=0.2\ncancel=0.5\n"
		"[CLIMB_IDLE_100]\noverlay=O\nkind=CLIMB\nbucket=100\nheight=100\nfoot=L\nstart=IDLE\n"
		"traj=0,0,0 0,60,30 0,100,60 0,100,106\nedge=54\ntakeoff=0.04\ncontact=0.16\ndepth=60\nexit=0.2\ncancel=0.5\n", err);
	CHECK(err.empty() && defs.size() == 2);
	TravLimits lim;
	for (bool idle : { false, true }) {
		TravBoxWorld w = world(true);
		if (idle)
			w.boxes[1].z0 = 54.0f, w.boxes[1].z1 = 254.0f, w.boxes[2].z0 = 114.0f, w.boxes[2].z1 = 214.0f;
		TravResult r = TravDetect(w, lim);
		CHECK(r.kind == TRAV_CLIMB);
		std::string log;
		const TravDef* d = TravPickChecked(w, defs, r, true, lim, &log, idle);
		CHECK(d == nullptr && std::string(r.reason).find("miejsca na koncu") != std::string::npos);
		TravBoxWorld free = world(false);
		if (idle)
			free.boxes[1].z0 = 54.0f, free.boxes[1].z1 = 254.0f;
		TravResult rf = TravDetect(free, lim);
		CHECK(TravPickChecked(free, defs, rf, true, lim, nullptr, idle) != nullptr);
	}
}

TEST(trav_detect_vertical_obstacle_behind_edge_still_blocks_climb) {
	// skrzynia 120 cm (gleboka), 36 cm za krawedzia belka od 30 cm nad gora - pionowa przeszkoda na wysokosci ciala
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 200.0f, 300.0f, 120.0f, 600.0f });
	w.boxes.push_back({ -300.0f, 150.0f, 236.0f, 300.0f, 400.0f, 270.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind != TRAV_CLIMB);
	CHECK(std::string(r.reason) == "sciana za krawedzia");
	if (std::string(r.reason) != "sciana za krawedzia")
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
}

TEST(trav_detect_parapet_lands_on_higher_terrace) {
	// parapet 140 cm przed tarasem 100 cm wyzej niz ulica: przeskok z ladowaniem na tarasie (25+ cm pod gora parapetu)
	TravBoxWorld w;
	w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 190.0f });
	w.boxes.push_back({ -2000.0f, -100.0f, 190.0f, 2000.0f, 100.0f, 2000.0f });
	w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 140.0f, 190.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_VAULT && std::fabs(r.landY - 100.0f) < 1.0f && r.bucket == 150);
	if (r.kind != TRAV_VAULT)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	// taras tylko 15 cm pod gora - za wysoko na przeskok
	TravBoxWorld w2;
	w2.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 190.0f });
	w2.boxes.push_back({ -2000.0f, -100.0f, 190.0f, 2000.0f, 125.0f, 2000.0f });
	w2.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 140.0f, 190.0f });
	CHECK(TravDetect(w2, TravLimits()).kind != TRAV_VAULT);
}

TEST(trav_detect_thin_rail_fence_on_slope) {
	// plot z dwiema zerdziami 10 cm (45-55 i 85-95 cm nad ziemia), slupki co 200 cm obok osi, stok 10 st.
	TravBoxWorld w;
	const float s = 0.176f;   // tan 10 st.
	w.ramp = { { -2000.0f, 0.0f }, { 0.0f, 0.0f }, { 3000.0f, 3000.0f * s } };
	float gz = 200.0f * s;
	w.rails.push_back({ gz + 50.0f, 200.0f, 5.0f, -400.0f, 400.0f });
	w.rails.push_back({ gz + 90.0f, 200.0f, 5.0f, -400.0f, 400.0f });
	w.boxes.push_back({ -105.0f, gz - 20.0f, 195.0f, -95.0f, gz + 100.0f, 205.0f });
	w.boxes.push_back({ 95.0f, gz - 20.0f, 195.0f, 105.0f, gz + 100.0f, 205.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_VAULT && r.bucket == 100);
	CHECK(std::fabs(r.top - (gz + 95.0f)) < 3.0f);
	if (r.kind != TRAV_VAULT)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
}

TEST(trav_detect_rail_between_rays_found_by_ground_spike) {
	// cienka listwa 6 cm (62-68 cm) - promienie sciany co 10 cm chybiaja, promien w dol trafia jej wierzch; plot z jedna
	// zerdzia: wierzch cienki, za nim ziemia
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 62.0f, 148.0f, 300.0f, 68.0f, 152.0f });
	w.boxes.push_back({ -300.0f, 0.0f, 149.0f, 300.0f, 62.0f, 151.0f });   // deska pod nia (sciana plotu, 2 cm)
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.wallDist > 140.0f && r.wallDist < 152.0f);
	CHECK(r.kind == TRAV_VAULT && std::fabs(r.height - 68.0f) < 2.0f);
	if (r.kind != TRAV_VAULT)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	// sama listwa nad ziemia (bez deski): skok terenu w promieniu w dol = przeszkoda, nie teren
	TravBoxWorld w2 = TravFloorWorld();
	w2.boxes.push_back({ -300.0f, 62.0f, 140.0f, 300.0f, 68.0f, 160.0f });
	std::string log2;
	TravResult r2 = TravDetect(w2, TravLimits(), &log2);
	CHECK(r2.wallDist > 130.0f && r2.wallDist < 161.0f);
	CHECK(r2.groundY.back() < 10.0f);   // teren nie wspial sie na listwe
}

TEST(trav_detect_sides_checked_at_two_depths) {
	// mur 286 z wyszczerbieniem krawedzi po jednej stronie: boki na 5 cm za sciana 1, na 25 cm 2 - wspiecie
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 165.0f, 300.0f, 286.0f, 500.0f });
	w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 10.0f, 286.0f, 165.0f });
	w.boxes.push_back({ 10.0f, 0.0f, 150.0f, 300.0f, 250.0f, 165.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_CLIMB && r.bucket == 250);
	if (r.kind != TRAV_CLIMB)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
}

TEST(trav_detect_local_march_from_known_wall) {
	// bufor: szukanie od okolicy znanej sciany (marchFrom) - teren od tam, mniej promieni
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 450.0f, 300.0f, 100.0f, 700.0f });
	TravLimits lim;
	lim.maxFace = 600.0f;
	TravResult full = TravDetect(w, lim);
	int raysFull = w.rays;
	lim.marchFrom = 350.0f;
	w.rays = 0;
	TravResult local = TravDetect(w, lim);
	CHECK(full.kind == TRAV_CLIMB && local.kind == TRAV_CLIMB && std::fabs(local.face - 450.0f) < 1.0f);
	CHECK(local.groundZ.front() == 350.0f && w.rays < raysFull);
}

TEST(trav_describe_mentions_kind_and_reason) {
	TravResult r;
	r.reason = "za wysoko";
	CHECK(TravDescribe(r).find("za wysoko") != std::string::npos);
	r.kind = TRAV_CLIMB;
	r.bucket = 150;
	CHECK(TravDescribe(r).find("wspiecie 150") != std::string::npos);
	r.kind = TRAV_VAULT;
	CHECK(TravDescribe(r).find("przeskok 150") != std::string::npos);
}

TEST(trav_detect_one_side_enough_on_deep_top) {
	// murek 108 cm przy innej przeszkodzie: lewy bok gory pusty (1 bok), gora gleboka - wspiecie (log 103935: "waska gora")
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -10.0f, 0.0f, 150.0f, 300.0f, 108.0f, 400.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.kind == TRAV_CLIMB && r.bucket == 100);
	if (r.kind != TRAV_CLIMB)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	// slupek 16 cm (oba boki puste) nadal nie jest platforma
	TravBoxWorld w2 = TravFloorWorld();
	w2.boxes.push_back({ -8.0f, 0.0f, 150.0f, 8.0f, 108.0f, 400.0f });
	TravResult r2 = TravDetect(w2, TravLimits());
	CHECK(r2.kind == TRAV_NONE && std::string(r2.reason) == "waska przeszkoda (pien, slup)");
}

TEST(trav_detect_narrow_trunk_is_not_obstacle) {
	// pien / slup 44 cm z plaska gora 150 cm: promienie boczne +-32 cm na wysokosci sciany obok - waska przeszkoda
	TravBoxWorld w = TravFloorWorld();
	w.pillars.push_back({ 0.0f, 200.0f, 22.0f, 0.0f, 150.0f });
	TravResult r = TravDetect(w, TravLimits());
	CHECK(r.kind == TRAV_NONE && std::string(r.reason) == "waska przeszkoda (pien, slup)");
	CHECK(r.sideL < 0.0f && r.sideR < 0.0f);
	// szeroka skrzynia: oba boki trafiaja te sama sciane
	TravBoxWorld w2 = TravFloorWorld();
	w2.boxes.push_back({ -100.0f, 0.0f, 200.0f, 100.0f, 150.0f, 400.0f });
	TravResult r2 = TravDetect(w2, TravLimits());
	CHECK(r2.kind == TRAV_CLIMB && std::fabs(r2.sideL - 200.0f) < 1.0f && std::fabs(r2.sideR - 200.0f) < 1.0f && !r2.round);
}

TEST(trav_detect_round_sheaf_keeps_run_direction) {
	// okragly snopek (promien 60, 100 cm) trafiony 15 cm obok osi: normalna odchylona o ok. 15 st., boki dalej niz jej
	// plaszczyzna - obiekt okragly, bez obrotu (inaczej tor idzie stycznie i konczy obok snopka)
	TravBoxWorld w = TravFloorWorld();
	w.pillars.push_back({ 15.0f, 210.0f, 60.0f, 0.0f, 100.0f });
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(r.round && r.wallYaw == 0.0f && std::fabs(r.face - r.wallDist) < 0.01f);
	if (!r.round)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
	// plaska sciana pod katem (pudelko obrocone nie istnieje - plaszczyzna z TravSlantWorld w innym tescie): skrzynia
	TravBoxWorld w2 = TravFloorWorld();
	w2.boxes.push_back({ -100.0f, 0.0f, 200.0f, 100.0f, 100.0f, 400.0f });
	CHECK(!TravDetect(w2, TravLimits()).round);
}

TEST(trav_end_support_follows_turn_to_wall) {
	// waska wzdluzna skrzynia (szer. 120, gora 100): prosto - koniec wspiecia na gorze; obrot o 34 st. (normalna
	// odchylona jak na snopku) - koniec obok skrzyni w powietrzu: bez podparcia, wspiecie odrzucone
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -60.0f, 0.0f, 150.0f, 60.0f, 100.0f, 500.0f });
	TravLimits lim;
	TravResult r = TravDetect(w, lim);
	CHECK(r.kind == TRAV_CLIMB);
	TravResult a = r;
	const TravDef* d = TravPickChecked(w, defs, a, true, lim);
	CHECK(d != nullptr && d->kind == TRAV_CLIMB);
	if (d) {
		float x, y, z;
		TravEndPoint(*d, a, x, y, z);
		CHECK(std::fabs(x) < 1.0f && z > 150.0f && std::fabs(y - 100.0f) < 1.0f);
	}
	TravResult b = r;
	b.wallYaw = 0.6f;
	b.face = r.wallDist * std::cos(0.6f);
	std::string log;
	CHECK(TravPickChecked(w, defs, b, true, lim, &log) == nullptr);
	CHECK(b.kind == TRAV_NONE && std::string(b.reason) == "brak podparcia na koncu ruchu");
	CHECK(log.find("bez podparcia") != std::string::npos);
}

TEST(trav_detect_scarp_without_ground_behind_face) {
	// skarpa 250 cm o poszarpanej scianie (normalne poza pionem, bez pary sciany); ziemia konczy sie u podstawy - promien
	// ziemi za sciana startuje w bryle i nic nie trafia: to sciana, nie uskok (log 107530, sprint przed skarpa)
	TravBoxWorld w;
	w.boxes.push_back({ -2000.0f, -100.0f, -2000.0f, 2000.0f, 0.0f, 280.0f });
	w.boxes.push_back({ -2000.0f, -100.0f, 280.0f, 2000.0f, 250.0f, 2000.0f });
	w.jagged = 1;
	std::string log;
	TravResult r = TravDetect(w, TravLimits(), &log);
	CHECK(std::fabs(r.wallDist - 280.0f) < 1.0f && std::string(r.reason) != "uskok przed bohaterem");
	CHECK(r.kind == TRAV_CLIMB && r.bucket == 250 && std::fabs(r.height - 250.0f) < 1.0f && std::fabs(r.wallYaw) < 0.01f);
	if (r.kind != TRAV_CLIMB)
		printf("  %s\n  %s\n", TravDescribe(r).c_str(), log.c_str());
}

TEST(trav_foliage_names) {
	CHECK(TravFoliageName("NW_NATURE_BIGTREE_01.3DS") && TravFoliageName("nw_nature_tanne_02.3ds"));
	CHECK(TravFoliageName("OW_EFEU_01.3DS") && TravFoliageName("NW_BUSH_BIG_02.3DS") && TravFoliageName("IVY_WALL.TGA"));
	CHECK(TravFoliageName("NW_LEAVES_A0.TGA") && TravFoliageName("FARN_01.3DS"));
	CHECK(!TravFoliageName("NW_NATURE_STONE_CULT_01.3DS") && !TravFoliageName("NW_GROUND_GRASS_01.TGA"));
	CHECK(!TravFoliageName("NW_CITY_WALL_01.3DS") && !TravFoliageName("NW_HARBOUR_CRATE_01.3DS") && !TravFoliageName("HAY.TGA"));
	CHECK(!TravFoliageName("NW_ROCK_MOOS_01.TGA") && !TravFoliageName(""));
}

TEST(trav_jump_teleport_and_air_grab_rules) {
	TravJumpGuardParams gp;
	CHECK(TravJumpTeleport(-157.0f, 0.16f, gp) && TravJumpTeleport(-206.0f, 0.02f, gp));
	CHECK(!TravJumpTeleport(-8.0f, 0.1f, gp) && !TravJumpTeleport(25.0f, 0.1f, gp) && !TravJumpTeleport(-150.0f, 0.8f, gp));
	TravAirGrabParams ap;
	// stopy 1000, glowa 1180: krawedz 1190 (na wysokosci glowy) - chwyt; 1100 (pod piersia) i 1260 (za wysoko) - nie
	CHECK(TravAirGrabOk(1190.0f, 1180.0f, 1000.0f, ap) && TravAirGrabOk(1140.0f, 1180.0f, 1000.0f, ap));
	CHECK(!TravAirGrabOk(1260.0f, 1180.0f, 1000.0f, ap) && !TravAirGrabOk(1100.0f, 1180.0f, 1000.0f, ap));
	CHECK(!TravAirGrabOk(1080.0f, 1100.0f, 1000.0f, ap));   // stopien 80 cm nad stopami
}

static TravResult TravWallRes(float dist, float base, float height, float scanTop);

TEST(trav_jump_momentum_keeps_takeoff_speed) {
	TravMomentumParams p;
	const float anim = 372.0f;   // T_RUNL_2_JUMP HUMANS_REMASTER: 268 cm w 0,72 s
	// sprint 831 cm/s: utrzymane, 1000 - ograniczone do 850; bieg 390 - za malo ponad animacje, bez dodatku
	CHECK(std::fabs(TravMomentumTarget(831.0f, anim, -1.0f, p) - 831.0f) < 0.01f);
	CHECK(std::fabs(TravMomentumTarget(1000.0f, anim, -1.0f, p) - 850.0f) < 0.01f);
	CHECK(TravMomentumTarget(390.0f, anim, -1.0f, p) == 0.0f);
	// po ladowaniu wygasa liniowo w landFade
	CHECK(std::fabs(TravMomentumTarget(800.0f, anim, 0.1f, p) - 400.0f) < 0.01f && TravMomentumTarget(800.0f, anim, 0.25f, p) == 0.0f);
	// dosuniecie: brakujaca czesc ponad krok silnika (animacja niesie 6 cm w klatce 1/60 s, cel 800 cm/s = 13,3 cm)
	CHECK(std::fabs(TravMomentumExtra(800.0f, 6.0f, 1.0f / 60.0f) - (800.0f / 60.0f - 6.0f)) < 0.01f);
	CHECK(TravMomentumExtra(800.0f, 20.0f, 1.0f / 60.0f) == 0.0f);                     // silnik juz szybciej - bez dublowania
	CHECK(std::fabs(TravMomentumExtra(800.0f, -5.0f, 1.0f / 60.0f) - 800.0f / 60.0f) < 0.01f);   // krok w tyl nie zwieksza
	// luk skoku gry na debugu i werdykt: zasieg rozciagniety
	TravGameJumpParams jp;
	TravGameJumpParams q = TravGameJumpWithSpeed(jp, 744.0f, anim, p);
	CHECK(std::fabs(q.reach - jp.reach * 2.0f) < 0.5f && std::fabs(q.landExtra - jp.landExtra * 2.0f) < 0.5f);
	CHECK(TravGameJumpWithSpeed(jp, 300.0f, anim, p).reach == jp.reach);
	// sciana 471 cm: z pedem 744 cm/s skok juz nie laduje przed nia (zasieg ok. 760)
	TravResult r = TravWallRes(471.0f, 43.0f, 0.0f, 300.0f);
	CHECK(TravGameJumpVerdict(r, jp).verdict == TRAVJUMP_PASS && TravGameJumpVerdict(r, q).verdict != TRAVJUMP_PASS);
}

TEST(trav_air_grab_waits_for_smaller_shift) {
	TravAirGrabParams p;
	TravAirGrabTiming t;
	CHECK(!TravAirGrabNow(120.0f, -20.0f, 400.0f, p, t));   // leci do krawedzi, dosuniecie 120 cm - czekac
	CHECK(TravAirGrabNow(60.0f, -20.0f, 400.0f, p, t));     // blisko
	CHECK(TravAirGrabNow(120.0f, -20.0f, 50.0f, p, t));     // juz sie nie zbliza (odbity / pionowo)
	CHECK(TravAirGrabNow(120.0f, 35.0f, 400.0f, p, t));     // krawedz zaraz ponad zasiegiem dloni
	CHECK(TravEaseOut(0.0f) == 0.0f && TravEaseOut(1.0f) == 1.0f && TravEaseOut(2.0f) == 1.0f);
	CHECK(TravEaseOut(0.5f) > 0.8f && TravEaseOut(0.5f) < 0.9f);   // szybko na poczatku, lagodnie na koncu
}

TEST(trav_root_glitch_is_one_frame_root_jump_beyond_animation_speed) {
	// runda 9: log 131330 - cofniecia 236-371 cm w klatce (ok. dlugosc petli biegu), 131357 - teleport z dachu
	TravRootGlitchParams p;
	CHECK(!TravRootGlitch(0.0f, 0.0f, 15.0f, 0.016f, p));       // bieg 900 cm/s
	CHECK(!TravRootGlitch(5.0f, 3.0f, -10.0f, 0.016f, p));      // obrot / krok w tyl
	CHECK(TravRootGlitch(0.0f, 0.0f, -348.0f, 0.016f, p));
	CHECK(TravRootGlitch(0.0f, 0.0f, 236.0f, 0.016f, p));       // powrot silnika do przodu - tez blad
	CHECK(TravRootGlitch(0.0f, -298.0f, -40.0f, 0.016f, p));    // z dachu w dol
	CHECK(!TravRootGlitch(0.0f, 0.0f, 110.0f, 0.13f, p));       // przycinka 130 ms przy sprincie - ruch
	CHECK(TravRootGlitch(0.0f, 0.0f, 260.0f, 0.13f, p));
}

TEST(trav_flight_guard_restores_position_without_stacking_steps) {
	// runda 9: silnik cofa bohatera (-348) i w nastepnej klatce oddaje (+348) - dawny straznik poprawial cofniecie
	// (krok z poprzedniej klatki) i przepuszczal powrot: +348 cm na pare klatek (6 poprawek = predkosc 1246 cm/s).
	// Teraz: cofniecie - pozycja sprzed + zwykly krok, skok do przodu ponad limit pedu - przyciety.
	TravJumpGuardParams g;
	TravFlightClampParams c;
	const float dt = 1.0f / 60.0f, maxStep = TravFlightMaxStep(850.0f, dt, c);
	CHECK(maxStep > 850.0f * dt && maxStep < 1000.0f * dt + 3.5f);
	float pos = 0.0f, last = 14.0f, airT = 0.0f;
	const float engine[] = { 14.0f, -348.0f, 348.0f + 14.0f, -371.0f, 371.0f + 14.0f, 14.0f, 14.0f };
	int fixes = 0;
	for (float step : engine) {
		airT += dt;
		int fixed = 0;
		float s = TravFlightGuardStep(step, last, airT, maxStep, g, fixed);
		fixes += fixed != 0 ? 1 : 0;
		CHECK(s >= 0.0f && s <= maxStep + 1e-3f);
		pos += s;
		last = s;
	}
	CHECK(fixes == 4);
	CHECK(pos <= 7.0f * maxStep + 1e-3f);   // nigdy szybciej niz limit pedu
	CHECK(pos >= 7.0f * 13.0f);              // i bez gubienia zwyklego kroku
	int f0 = 0;
	CHECK(TravFlightGuardStep(-20.0f, 14.0f, 0.1f, maxStep, g, f0) == -20.0f && f0 == 0);   // kolizja - kilka cm w tyl
	CHECK(TravFlightGuardStep(-200.0f, 14.0f, 0.9f, maxStep, g, f0) == -200.0f && f0 == 0); // po oknie startu - bez zmian
}

TEST(trav_air_grab_hands_lock_on_ledge_while_pose_blends_into_hang) {
	// runda 8: dawniej przesuniecie z AlignModelToLedge liczone z dloni pierwszej klatki (poza lotu, dlon 60 cm nad
	// vobem) - w pozie zawisu (dlon 120) dlonie 60 cm nad krawedzia, potem silnik "dosadzal". Teraz co klatke cel z dloni
	// biezacej pozy: na koncu dlon na krawedzi, bez ruchu po dosunieciu.
	TravAirGrabTiming t;
	const float ledge = 300.0f, before = 250.0f, dt = 1.0f / 60.0f;
	float vob = before, last = vob, hand = 0.0f;
	float moveAfter = 0.0f;
	for (float st = dt; st <= 0.40f; st += dt) {
		float wgt = st / t.smoothTime < 1.0f ? st / t.smoothTime : 1.0f;   // blend lot -> S_HANG
		float handOff = 60.0f + 60.0f * wgt;
		float e = TravEaseOut(st / t.smoothTime);
		vob = TravGrabLockAxis(before, vob, vob + handOff, ledge, e);
		hand = vob + handOff;
		if (st > t.smoothTime + dt)
			moveAfter = std::fmax(moveAfter, std::fabs(vob - last));
		last = vob;
	}
	CHECK(std::fabs(hand - ledge) < 0.5f);
	CHECK(std::fabs(vob - 180.0f) < 0.5f);
	CHECK(moveAfter < 0.5f);
	// trzymanie zawisu: do dosuniecia + 0,08 s; S gracza / koniec zawisu / zabezpieczenie - puszczenie
	TravGrabHold h;
	CHECK(TravGrabHolding(0.10f, t.smoothTime, true, false, h));
	CHECK(TravGrabHolding(0.28f, t.smoothTime, true, false, h));
	CHECK(!TravGrabHolding(0.31f, t.smoothTime, true, false, h));
	CHECK(!TravGrabHolding(0.10f, t.smoothTime, true, true, h));
	CHECK(!TravGrabHolding(0.10f, t.smoothTime, false, false, h));
	CHECK(!TravGrabHolding(0.70f, 1.0f, true, false, h));
}

// ---------- Skok gry po odmowie (werdykt) i debug wizualny ----------
#include "Core/TravDebug.h"

static TravResult TravWallRes(float dist, float base, float height, float scanTop = -1.0f) {
	TravResult r;
	r.wallDist = r.face = dist;
	r.base = base;
	if (height > 0.0f) {
		r.height = height;
		r.top = base + height;
	}
	r.scanTop = scanTop;
	return r;
}

TEST(trav_game_jump_air_grab_passes_jump_over_chasm_to_ledge) {
	// runda 12 (tor: platforma 100, przepasc 250, sciana z krawedzia 200 nad platforma): dawniej "dobieg do sciany" -
	// wpadal w przepasc; z chwytem w locie skok gry dozwolony (chwyt go dokonczy)
	TravGameJumpParams p;
	p.ledgeMax = 384.0f;
	TravResult r = TravWallRes(250.0f, -100.0f, 300.0f);   // gora +200 nad bohaterem
	TravGameJumpInfo g = TravGameJumpVerdict(r, p);
	CHECK(g.verdict == TRAVJUMP_SUPPRESS);                  // bez chwytu: przepasc - nie dobieg
	p.airGrab = true;
	g = TravGameJumpVerdict(r, p);
	CHECK(g.verdict == TRAVJUMP_PASS && std::string(g.why).find("chwyt") != std::string::npos);
	// krawedz za wysoko na chwyt - bez skoku
	TravResult hi = TravWallRes(250.0f, -100.0f, 450.0f);
	CHECK(TravGameJumpVerdict(hi, p).verdict == TRAVJUMP_SUPPRESS);
	// sciana na tej samej ziemi (bez przepasci): dalej dobieg i skok z miejsca gry (pewniejszy zawis)
	TravResult same = TravWallRes(250.0f, 0.0f, 200.0f);
	CHECK(TravGameJumpVerdict(same, p).verdict == TRAVJUMP_LEDGE);
}

TEST(trav_run_exit_waits_until_feet_reach_end_level) {
	// runda 12 (log 138345 CLIMB_IDLE_200: wyjscie w bieg przy stopach w skrzyniach, kolizja wypchnela 4 m do tylu)
	std::vector<std::string> err;
	auto defs = ParseTraversal(
		"[CLIMB_200_LF]\noverlay=O\nkind=CLIMB\nbucket=200\nheight=200\nfoot=L\ntrajFps=25\n"
		"traj=0,0,0 0,60,50 0,120,100 0,160,140 0,185,170 0,200,200 0,200,210\n"
		"edge=150\ntakeoff=0.04\ncontact=0.12\ndepth=60\nexit=0.08\ncancel=0.24\n", err);
	CHECK(defs.size() == 1);
	if (defs.empty())
		return;
	TravFit f;
	CHECK(!TravRunExitReady(defs[0], f, 0.05f));    // przed exit
	CHECK(!TravRunExitReady(defs[0], f, 0.08f));    // exit, ale stopy 80 cm pod gora
	CHECK(TravRunExitReady(defs[0], f, 0.19f));     // 200 - 4 cm od konca
	f.endAdj = 30.0f;                               // dach: koniec wyzej
	CHECK(!TravRunExitReady(defs[0], f, 0.12f) && TravRunExitReady(defs[0], f, 0.30f));
}

TEST(fall_forgive_small_gaps_but_not_real_edges) {
	// runda 13: szczelina miedzy skrzyniami / dziura przy kamyku pod srodkiem - silnik spada (nad podloga > krok),
	// cialo ma podparcie na obwodzie -> bez spadania; prawdziwa krawedz (przed bohaterem spadek) -> spada
	FallForgiveParams p;
	const float none = 1e9f, step = 30.0f;
	float gap[8] = { 0, 0, 0, 2, 0, 0, 0, 1 };
	CHECK(FallForgiveDecide(100.0f, step, gap, 0.0f, 600.0f, 0.0f, p) == FALL_FORGIVE);
	CHECK(FallForgiveDecide(20.0f, step, gap, 0.0f, 600.0f, 0.0f, p) == FALL_NONE);       // silnik i tak nie spada
	float pebble[8] = { 15, 12, 18, 60, 70, 14, 10, 16 };
	CHECK(FallForgiveDecide(65.0f, step, pebble, 12.0f, 400.0f, 0.0f, p) == FALL_FORGIVE);
	// krawedz platformy: z przodu i w przedniej polowie obwodu spadek 100, z tylu jeszcze podloga
	float edge[8] = { 100, 100, 100, 0, 0, 0, 100, 100 };
	CHECK(FallForgiveDecide(100.0f, step, edge, 100.0f, 600.0f, 0.0f, p) == FALL_REAL);
	float edgeNone[8] = { none, none, none, 0, 0, 0, none, none };
	CHECK(FallForgiveDecide(none, step, edgeNone, none, 600.0f, 0.0f, p) == FALL_REAL);
	// stopien w dol 45 cm (ponad FallForgiveDrop 40) - spada jak w grze
	float down45[8] = { 45, 45, 45, 45, 45, 45, 45, 45 };
	CHECK(FallForgiveDecide(45.0f, step, down45, 45.0f, 300.0f, 0.0f, p) == FALL_REAL);
	// bez ruchu: potrzeba wiekszosci obwodu
	float half[8] = { 0, 0, 0, 0, 100, 100, 100, 100 };
	CHECK(FallForgiveDecide(100.0f, step, half, 0.0f, 0.0f, 0.0f, p) == FALL_REAL);
	CHECK(FallForgiveDecide(100.0f, step, gap, none, 0.0f, 0.0f, p) == FALL_FORGIVE);
	// wybaczanie najwyzej maxTime pod rzad
	CHECK(FallForgiveDecide(100.0f, step, gap, 0.0f, 600.0f, 0.45f, p) == FALL_REAL);
	// probka wysoko nad stopami (wnetrze skrzyni / sciana) to nie podparcie
	float wall[8] = { -90, -90, -90, -90, 100, 100, 100, 100 };
	CHECK(FallForgiveDecide(100.0f, step, wall, -90.0f, 600.0f, 0.0f, p) == FALL_REAL);
}

TEST(trav_game_jump_feet_arc_follows_animation) {
	TravGameJumpParams p;
	CHECK(TravGameJumpFeet(p, 0.0f) == 0.0f && TravGameJumpFeet(p, p.reach) == 0.0f && TravGameJumpFeet(p, 400.0f) == 0.0f);
	CHECK(std::fabs(TravGameJumpFeet(p, p.apexAt * p.reach) - p.rise) < 0.01f);
	// T_RUNL_2_JUMP: 151 cm od startu korzen +78 cm
	CHECK(std::fabs(TravGameJumpFeet(p, 151.0f) - 78.0f) < 3.0f);
}

TEST(trav_game_jump_verdicts) {
	TravGameJumpParams p;
	{
		TravGameJumpInfo g = TravGameJumpVerdict(TravResult(), p);
		CHECK(g.verdict == TRAVJUMP_PASS && std::string(g.why) == "brak sciany");
	}
	{
		// log 103926: sciana 471 cm (wzdluz biegu), podstawa +43, gora ponad 300 - skok laduje przed nia
		TravResult r = TravWallRes(471.0f, 43.0f, 0.0f, 300.0f);
		TravGameJumpInfo g = TravGameJumpVerdict(r, p);
		CHECK(g.verdict == TRAVJUMP_PASS && !g.engineCancels && g.tall);
		CHECK(std::string(g.why) == "ladowanie przed sciana");
	}
	{
		// wysoka sciana w zasiegu lotu nad promieniem silnika - odbicie; krawedz (co najmniej 290) w zasiegu podciagania
		// Gothica (377 jak w logu: GetJumpUpHeight 200, wzrost 194) - dobieg i skok z miejsca, poza nim - bez skoku
		TravResult r = TravWallRes(300.0f, 0.0f, 0.0f, 280.0f);
		TravGameJumpParams p2 = p;
		p2.ledgeMax = 377.0f;
		TravGameJumpInfo g = TravGameJumpVerdict(r, p2);
		CHECK(g.verdict == TRAVJUMP_LEDGE && !g.engineCancels);
		CHECK(TravGameJumpLabel(g).find("skok z miejsca: w biegu odbilby sie - dobieg") == 0);
		p2.ledgeMax = 250.0f;
		g = TravGameJumpVerdict(r, p2);
		CHECK(g.verdict == TRAVJUMP_SUPPRESS && TravGameJumpLabel(g).find("bez skoku: w locie uderzylby") == 0);
		p2.ledgeMax = 377.0f;
		p2.ledgeRun = 250.0f;   // sciana dalej niz dobieg
		CHECK(TravGameJumpVerdict(r, p2).verdict == TRAVJUMP_SUPPRESS);
	}
	{
		// sciana 150 cm, gora ponad 300: promien silnika (90 + 0,766 * 150) trafia - silnik anuluje; dobieg do sciany
		TravResult r = TravWallRes(150.0f, 0.0f, 0.0f, 300.0f);
		TravGameJumpParams p2 = p;
		p2.ledgeMax = 377.0f;
		TravGameJumpInfo g = TravGameJumpVerdict(r, p2);
		CHECK(g.verdict == TRAVJUMP_LEDGE && g.engineCancels);
		CHECK(std::fabs(g.rayAtWall - (p.rayY + p.rayUp * 150.0f)) < 0.01f);
		p2.ledgeMax = 300.0f;
		CHECK(TravGameJumpVerdict(r, p2).verdict == TRAVJUMP_SUPPRESS);
	}
	{
		// przy scianie (60 cm), gora 180 nad promieniem: skok z miejsca gry; krawedz poza zasiegiem chwytu - bez skoku
		TravResult r = TravWallRes(60.0f, 0.0f, 180.0f);
		CHECK(TravGameJumpVerdict(r, p).verdict == TRAVJUMP_LEDGE);
		TravGameJumpParams p2 = p;
		p2.ledgeMax = 150.0f;
		CHECK(TravGameJumpVerdict(r, p2).verdict == TRAVJUMP_SUPPRESS);
		p2.ledgeMax = 230.0f;
		CHECK(TravGameJumpVerdict(r, p2).verdict == TRAVJUMP_LEDGE);
	}
	{
		// niski plot 60 cm 150 cm przed bohaterem: stopy w locie ok. 80 cm - skok przejdzie; 40 cm - krok
		TravGameJumpInfo g = TravGameJumpVerdict(TravWallRes(150.0f, 0.0f, 60.0f), p);
		CHECK(g.verdict == TRAVJUMP_PASS && std::string(g.why) == "skok przejdzie nad przeszkoda");
		CHECK(TravGameJumpVerdict(TravWallRes(30.0f, 0.0f, 25.0f), p).verdict == TRAVJUMP_PASS);   // krawleznik
		// 43 cm 34 cm przed bohaterem (log 111838): stopy w locie ok. 28 cm - skok w nia wlatuje; nie "postac wejdzie"
		CHECK(TravGameJumpVerdict(TravWallRes(34.0f, 0.0f, 43.0f), p).verdict != TRAVJUMP_PASS);
		// stos skrzyn: 43 cm, a w punkcie konca wspiecia 100 cm (endBlockY) - przeszkoda 100, skok gry wstrzymany / z miejsca
		TravResult crates = TravWallRes(150.0f, 0.0f, 43.0f);
		crates.endBlockY = 100.0f;
		TravGameJumpInfo gc = TravGameJumpVerdict(crates, p);
		CHECK(gc.verdict != TRAVJUMP_PASS && std::fabs(gc.obstacle - 100.0f) < 0.01f);
	}
	{
		// log 103935: murek 117 nad ziemia 138 cm przed bohaterem - promien nad nim, stopy ok. 77 cm: odbicie w biegu,
		// krawedz w zasiegu Gothica - dobieg i skok z miejsca (JUMPUPMID)
		TravGameJumpParams p2 = p;
		p2.ledgeMax = 377.0f;
		TravGameJumpInfo g = TravGameJumpVerdict(TravWallRes(138.0f, 9.0f, 108.0f), p2);
		CHECK(g.verdict == TRAVJUMP_LEDGE && !g.engineCancels && g.feetAtWall < 117.0f);
		p2.ledgeMax = 100.0f;
		CHECK(TravGameJumpVerdict(TravWallRes(138.0f, 9.0f, 108.0f), p2).verdict == TRAVJUMP_SUPPRESS);
	}
	{
		// bez sciany, ale stromy stok pod promieniem silnika
		TravResult r;
		r.groundZ = { 0.0f, 50.0f, 100.0f };
		r.groundY = { 0.0f, 80.0f, 200.0f };
		TravGameJumpInfo g = TravGameJumpVerdict(r, p);
		CHECK(g.verdict == TRAVJUMP_SUPPRESS && g.engineCancels);
	}
}

TEST(trav_recorder_keeps_rays_and_stages) {
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 200.0f, 300.0f, 100.0f, 400.0f });
	TravRecorder rec(w);
	TravResult r = TravDetect(rec, TravLimits());
	CHECK(r.kind == TRAV_CLIMB);
	CHECK((int)rec.rays.size() == w.rays && w.rays > 10);
	bool march = false, profile = false, top = false, sides = false;
	for (auto& ray : rec.rays) {
		march |= ray.stage == TRAVST_MARCH_FWD;
		profile |= ray.stage == TRAVST_PROFILE;
		top |= ray.stage == TRAVST_TOP && ray.hit;
		sides |= ray.stage == TRAVST_SIDES;
	}
	CHECK(march && profile && top && sides);
}

static bool TravHasText(const std::vector<TravDrawItem>& items, const char* s) {
	for (auto& it : items)
		if (it.kind != TRAVDRAW_LINE && it.text.find(s) != std::string::npos)
			return true;
	return false;
}

static bool TravHasColor(const std::vector<TravDrawItem>& items, TravDrawKind kind, unsigned color) {
	for (auto& it : items)
		if (it.kind == kind && it.color == color)
			return true;
	return false;
}

TEST(trav_debug_build_draws_detection) {
	std::vector<std::string> err;
	auto defs = ParseTraversal(TRAV_INI, err);
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 200.0f, 300.0f, 100.0f, 400.0f });
	TravRecorder rec(w);
	TravDebugInfo in;
	in.res = TravDetect(rec, in.lim);
	in.rays = rec.rays;
	in.def = TravPick(defs, in.res, true);
	CHECK(in.def != nullptr);
	in.decision = TravDecisionLabel(in.res, in.def);
	CHECK(in.decision == "WSPIECIE 100 -> CLIMB_100_LF");
	auto items = TravDebugBuild(in);
	CHECK(TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_HIT) && TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_FREE));
	CHECK(TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_TOP) && TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_WALL));
	CHECK(TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_WINDOW) && TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_HERO));
	CHECK(TravHasText(items, "SCIANA 200 cm, kat 0") && TravHasText(items, "GORA 100 (podstawa +0)"));
	CHECK(TravHasText(items, "WSPIECIE 100 -> CLIMB_100_LF") && TravHasText(items, "KONIEC") && TravHasText(items, "OKNO STARTU"));
	// okno startu: od kMax * edge do kMin * edge przed sciana (edge 150)
	bool window = false;
	for (auto& it : items)
		if (it.kind == TRAVDRAW_LINE && it.color == TRAVCOL_WINDOW && std::fabs(it.a[2] - (200.0f - 1.2f * 150.0f)) < 0.5f &&
			std::fabs(it.b[2] - (200.0f - 0.55f * 150.0f)) < 0.5f)
			window = true;
	CHECK(window);
	// gora: zolta linia na wysokosci gory, szerokosc barkow
	bool topLine = false;
	for (auto& it : items)
		if (it.kind == TRAVDRAW_LINE && it.color == TRAVCOL_TOP && std::fabs(it.a[1] - 100.0f) < 0.5f && it.a[0] < 0.0f && it.b[0] > 0.0f)
			topLine = true;
	CHECK(topLine);
	// profil: znaczniki x (trafiony) i . (wolny) na scianie
	bool px = false, pdot = false;
	for (auto& it : items)
		if (it.kind == TRAVDRAW_MARK && std::fabs(it.a[2] - 200.0f) < 0.5f) {
			px |= it.text == "x";
			pdot |= it.text == ".";
		}
	CHECK(px && pdot);
}

TEST(trav_debug_build_refusal_shows_game_jump) {
	TravBoxWorld w = TravFloorWorld();
	w.boxes.push_back({ -300.0f, 0.0f, 150.0f, 300.0f, 400.0f, 400.0f });   // mur 4 m 150 cm przed bohaterem
	TravRecorder rec(w);
	TravDebugInfo in;
	in.res = TravDetect(rec, in.lim);
	in.rays = rec.rays;
	CHECK(in.res.kind == TRAV_NONE);
	in.decision = TravDecisionLabel(in.res, nullptr);
	CHECK(in.decision.find("BRAK: ") == 0);
	in.jp.ledgeMax = 377.0f;   // gora ponad 400 - poza zasiegiem podciagania Gothica
	in.jump = TravGameJumpVerdict(in.res, in.jp);
	in.gameJump = TravGameJumpLabel(in.jump);
	in.showJump = true;
	CHECK(in.jump.verdict == TRAVJUMP_SUPPRESS && in.jump.engineCancels);
	auto items = TravDebugBuild(in);
	CHECK(TravHasText(items, "BRAK: ") && TravHasText(items, "bez skoku: silnik anuluje"));
	// promien silnika od srodka postaci, czerwony do sciany
	bool ray = false;
	for (auto& it : items)
		if (it.kind == TRAVDRAW_LINE && it.color == TRAVCOL_HIT && std::fabs(it.a[1] - in.jp.rayY) < 0.5f && it.a[2] == 0.0f &&
			std::fabs(it.b[2] - 150.0f) < 0.5f && std::fabs(it.b[1] - (in.jp.rayY + in.jp.rayUp * 150.0f)) < 0.5f)
			ray = true;
	CHECK(ray && TravHasColor(items, TRAVDRAW_LINE, TRAVCOL_JUMP_NO));
	CHECK(!TravHasText(items, "OKNO STARTU") && !TravHasText(items, "KONIEC"));
}

TEST(trav_debug_status_line) {
	std::string s = TravStatusLine(450.0f, false, 635.0f, "czeka");
	CHECK(s.find("predkosc 450 (bieg)") != std::string::npos && s.find("zasieg 635 cm") != std::string::npos);
	CHECK(s.find("bufor: czeka") != std::string::npos);
	CHECK(TravStatusLine(800.0f, true, 800.0f, nullptr).find("(sprint)") != std::string::npos);
	CHECK(TravStatusLine(50.0f, false, 330.0f, "").find("(wolno)") != std::string::npos);
}

// ---------- Profiler (wspolny dla pluginow: plugins\_shared\ZProfCore.h) ----------
#include <memory>
#include <cstring>
#include "../../_shared/ZProfCore.h"

static const double PROF_TPM = 1000.0;   // tykniec na ms w testach (1 tykniecie = 1 us)
static uint64_t PMs(double ms) {
	return (uint64_t)(ms * PROF_TPM + 0.5);
}
static bool PNear(double a, double b, double eps = 1e-3) {
	return std::fabs(a - b) < eps;
}
static bool PHas(const std::string& s, const char* part) {
	return s.find(part) != std::string::npos;
}
static std::unique_ptr<zprof::Profiler> PMake(const char* name, float spikeMs = 25.0f, float summarySec = 0.0f) {
	auto p = std::make_unique<zprof::Profiler>();
	p->SetName(name);
	p->SetTicksPerMs(PROF_TPM);
	p->Configure(spikeMs, summarySec);
	return p;
}
// n klatek po ms (pierwsze Mark tylko startuje pomiar, gdy t == 0)
static uint64_t PFrames(zprof::Profiler& p, uint64_t t, int n, double ms) {
	for (int i = 0; i < n; i++) {
		t += PMs(ms);
		p.Mark(t, nullptr);
	}
	return t;
}

TEST(prof_median_odd_even_and_window) {
	zprof::Median m;
	CHECK(m.Count() == 0 && m.Value() == 0.0f);
	m.Push(5.0f);
	m.Push(1.0f);
	m.Push(3.0f);
	CHECK(m.Value() == 3.0f);
	m.Push(10.0f);
	CHECK(PNear(m.Value(), 4.0));   // (3 + 5) / 2
	zprof::Median w;
	for (int i = 0; i < zprof::WINDOW; i++)
		w.Push(100.0f);
	for (int i = 0; i < zprof::WINDOW; i++)
		w.Push(10.0f);
	CHECK(w.Count() == zprof::WINDOW && w.Value() == 10.0f);   // stare klatki wypadly z okna
	w.Clear();
	CHECK(w.Count() == 0);
}

TEST(prof_spike_threshold_is_max_of_floor_and_twice_median) {
	zprof::Median m;
	for (int i = 0; i < 5; i++)
		m.Push(20.0f);
	CHECK(zprof::SpikeThreshold(25.0f, m) == 25.0f);   // za malo klatek - sam prog z ini
	for (int i = 0; i < 30; i++)
		m.Push(16.0f);
	CHECK(PNear(zprof::SpikeThreshold(25.0f, m), 32.0));
	zprof::Median fast;
	for (int i = 0; i < 30; i++)
		fast.Push(5.0f);
	CHECK(zprof::SpikeThreshold(25.0f, fast) == 25.0f);   // 2 x 5 ms < 25 ms
}

TEST(prof_classify_ok_spike_gap) {
	CHECK(zprof::Classify(30.0f, 1.0f, 32.0f) == zprof::FRAME_OK);
	CHECK(zprof::Classify(40.0f, 1.0f, 32.0f) == zprof::FRAME_SPIKE);
	CHECK(zprof::Classify(1500.0f, 10.0f, 32.0f) == zprof::FRAME_GAP);       // alt-tab / okno systemowe
	CHECK(zprof::Classify(1500.0f, 1200.0f, 32.0f) == zprof::FRAME_SPIKE);   // dluga klatka przez plugin to przycinka
}

TEST(prof_sections_dedupe_and_overflow_to_other) {
	auto p = PMake("zTest");
	int a = p->Section("Traversal");
	CHECK(p->Section("Traversal") == a);
	CHECK(p->Section("Traversal.Probe") != a);
	int last = -1;
	for (int i = 0; i < zprof::MAX_SECTIONS + 5; i++) {
		char name[32];
		sprintf_s(name, "S%d", i);
		last = p->Section(name);
	}
	CHECK(last == zprof::MAX_SECTIONS - 1);
	CHECK(std::strcmp(p->Own().sec[last].name, "(inne)") == 0);
	CHECK(p->Own().sectionCount == zprof::MAX_SECTIONS);
}

TEST(prof_nested_sections_self_and_inclusive_time) {
	auto p = PMake("zTest");
	int a = p->Section("Traversal"), b = p->Section("Traversal.Probe");
	CHECK(p->Mark(PMs(0), nullptr) == zprof::FRAME_NONE);   // pierwsze wywolanie tylko startuje pomiar
	p->Begin(a, PMs(1));
	p->Begin(b, PMs(2));
	p->End(PMs(5));
	p->End(PMs(6));
	p->Begin(b, PMs(7));   // ta sama sekcja na najwyzszym poziomie
	p->End(PMs(8));
	CHECK(p->Depth() == 0);
	CHECK(p->Mark(PMs(16), nullptr) == zprof::FRAME_OK);
	const zprof::FrameView& f = p->LastFrame();
	CHECK(PNear(f.frameMs, 16.0));
	CHECK(PNear(f.pluginMs, 6.0));          // Traversal 5 + Probe 1 (zagniezdzone liczone raz)
	CHECK(PNear(f.sec[a].selfMs, 2.0) && PNear(f.sec[a].inclMs, 5.0) && f.sec[a].calls == 1);
	CHECK(PNear(f.sec[b].selfMs, 4.0) && PNear(f.sec[b].inclMs, 4.0) && f.sec[b].calls == 2);
	// nastepna klatka bez sekcji - zera (delty, nie sumy narastajace)
	CHECK(p->Mark(PMs(32), nullptr) == zprof::FRAME_OK);
	CHECK(p->LastFrame().pluginMs == 0.0f && p->LastFrame().sec[b].calls == 0);
}

TEST(prof_break_skips_interval_after_pause) {
	auto p = PMake("zTest");
	p->Mark(PMs(0), nullptr);
	CHECK(p->Mark(PMs(16), nullptr) == zprof::FRAME_OK);
	p->Break();   // menu / wczytanie / zapis
	CHECK(p->Mark(PMs(5000), nullptr) == zprof::FRAME_NONE);
	CHECK(p->Mark(PMs(5016), nullptr) == zprof::FRAME_OK);
	CHECK(PNear(p->LastFrame().frameMs, 16.0));
	CHECK(p->Total().frames == 2 && p->Total().spikes == 0 && p->Total().gaps == 0);
}

TEST(prof_spike_line_names_top_sections_and_cause) {
	auto p = PMake("zTest");
	int snap = p->Section("Recorder.Snap"), lg = p->Section("Log"), idle = p->Section("Fx.Render");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 30, 16.0);
	p->Begin(snap, t + PMs(1));
	p->End(t + PMs(31));
	p->Begin(lg, t + PMs(32));
	p->End(t + PMs(32.5));
	p->Begin(lg, t + PMs(33));
	p->End(t + PMs(33.5));
	(void)idle;   // bez wywolan w tej klatce - nie trafia do linii
	t += PMs(50);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_SPIKE);
	CHECK(out.size() == 1);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "Prof: przycinka 50.0 ms"));
	CHECK(PHas(s, "mediana 16.0, prog 32.0"));
	CHECK(PHas(s, "zTest 31.0 ms: Recorder.Snap 30.0 ms (1x), Log 1.0 ms (2x)"));
	CHECK(!PHas(s, "Fx.Render"));
	CHECK(PHas(s, "reszta gry 19.0 ms"));
	CHECK(PHas(s, "przyczyna: zTest"));
	CHECK(p->Total().spikes == 1);
}

TEST(prof_spike_outside_plugin_says_so) {
	auto p = PMake("zTest");
	int a = p->Section("Moves");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 30, 16.0);
	p->Begin(a, t + PMs(1));
	p->End(t + PMs(1.3));
	t += PMs(60);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_SPIKE);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "zTest 0.3 ms: Moves 0.3 ms (1x)"));
	CHECK(PHas(s, "poza pluginem"));
	CHECK(!PHas(s, "przyczyna"));
}

TEST(prof_spike_before_window_fills_shows_no_median) {
	auto p = PMake("zTest");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 3, 16.0);
	t += PMs(40);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_SPIKE);   // za malo klatek na mediane - prog z ini (25 ms)
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "mediana -, prog 25.0"));
}

TEST(prof_spike_partial_share) {
	auto p = PMake("zTest");
	int a = p->Section("Corpses");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 30, 16.0);
	p->Begin(a, t + PMs(1));
	p->End(t + PMs(13));   // 12 ms z nadwyzki 34 ms
	t += PMs(50);
	std::vector<std::string> out;
	p->Mark(t, &out);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "czesciowo zTest"));
}

TEST(prof_gap_not_counted_as_spike_nor_in_median) {
	auto p = PMake("zTest");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 30, 16.0);
	t += PMs(3000);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_GAP);
	CHECK(out.size() == 1 && PHas(out[0], "Prof: przerwa 3000 ms"));
	CHECK(p->Total().spikes == 0 && p->Total().gaps == 1 && p->Total().frames == 30);
	CHECK(PNear(p->Total().maxFrameMs, 16.0));
}

TEST(prof_shared_table_two_plugins) {
	auto table = std::make_unique<zprof::Table>();
	std::memset(table.get(), 0, sizeof(zprof::Table));
	auto d = PMake("zDismember"), r = PMake("zTimeRewind");
	int early = d->Section("Early");   // przed podpieciem tablicy - nazwa przechodzi do slotu
	CHECK(d->Attach(table.get()) && r->Attach(table.get()));
	CHECK(d->SlotIndex() >= 0 && r->SlotIndex() >= 0 && d->SlotIndex() != r->SlotIndex());
	CHECK(std::strcmp(table->slots[d->SlotIndex()].sec[early].name, "Early") == 0);
	CHECK(std::strcmp(table->slots[r->SlotIndex()].plugin, "zTimeRewind") == 0);
	int trav = d->Section("Traversal"), snap = r->Section("Recorder.Snap");
	uint64_t t = 0;
	d->Mark(t, nullptr);
	r->Mark(t + PMs(0.1), nullptr);
	for (int i = 0; i < 30; i++) {
		t += PMs(16);
		d->Mark(t, nullptr);
		r->Mark(t + PMs(0.1), nullptr);
	}
	// klatka z przycinka: zTimeRewind 30 ms w Recorder.Snap, zDismember 0,8 ms w Traversal
	d->Begin(trav, t + PMs(1));
	d->End(t + PMs(1.8));
	r->Begin(snap, t + PMs(2));
	r->End(t + PMs(32));
	t += PMs(50);
	std::vector<std::string> out;
	CHECK(d->Mark(t, &out) == zprof::FRAME_SPIKE);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "zDismember 0.8 ms: Traversal 0.8 ms (1x)"));
	CHECK(PHas(s, "zTimeRewind 30.0 ms (Recorder.Snap)"));
	CHECK(PHas(s, "reszta gry 19.2 ms"));
	CHECK(PHas(s, "przyczyna: zTimeRewind (poza zDismember)"));
	// zTimeRewind widzi te sama klatke ze swojej strony
	out.clear();
	CHECK(r->Mark(t + PMs(0.1), &out) == zprof::FRAME_SPIKE);
	s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "zTimeRewind 30.0 ms: Recorder.Snap 30.0 ms (1x)"));
	CHECK(PHas(s, "zDismember 0.8 ms (Traversal)"));
	CHECK(PHas(s, "przyczyna: zTimeRewind"));
}

TEST(prof_table_attach_checks_version_and_slots) {
	auto table = std::make_unique<zprof::Table>();
	std::memset(table.get(), 0, sizeof(zprof::Table));
	CHECK(zprof::TableAttach(*table));   // swieza (wyzerowana) - zainicjowana
	CHECK(table->magic == zprof::TABLE_MAGIC && table->version == zprof::TABLE_VERSION);
	CHECK(zprof::TableAttach(*table));   // kolejny plugin - zgodna
	int a = zprof::TableClaim(*table, "zA");
	CHECK(a >= 0 && zprof::TableClaim(*table, "zA") == a);   // ten sam plugin - ten sam slot
	for (int i = 0; i < zprof::MAX_SLOTS - 1; i++) {
		char name[16];
		sprintf_s(name, "z%d", i);
		zprof::TableClaim(*table, name);
	}
	CHECK(zprof::TableClaim(*table, "zPelna") == -1);
	table->version = 99;   // inna wersja ukladu (stary plugin) - bez wspolnej tablicy
	CHECK(!zprof::TableAttach(*table));
	auto p = PMake("zTest");
	CHECK(!p->Attach(table.get()) && p->SlotIndex() == -1);
	int s = p->Section("Moves");   // dziala dalej na slocie lokalnym
	p->Mark(PMs(0), nullptr);
	p->Begin(s, PMs(1));
	p->End(PMs(2));
	p->Mark(PMs(16), nullptr);
	CHECK(PNear(p->LastFrame().pluginMs, 1.0));
}

TEST(prof_summary_averages_p99_max_and_sections) {
	auto p = PMake("zTest");
	int mv = p->Section("Moves"), an = p->Section("Anis.Hook");
	p->Section("Unused");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	for (int i = 0; i < 101; i++) {
		bool spike = i == 100;
		for (int k = 0; k < 20; k++) {   // 20 x 50 us = 1 ms na klatke
			p->Begin(an, t + 100 + k * 100);
			p->End(t + 150 + k * 100);
		}
		p->Begin(mv, t + PMs(3.0));
		p->End(t + PMs(spike ? 33.0 : 4.0));
		t += PMs(spike ? 40.0 : 10.0);
		p->Mark(t, nullptr);
	}
	const zprof::Stats& st = p->Total();
	CHECK(st.frames == 101 && st.spikes == 1 && st.gaps == 0);
	CHECK(PNear(st.AvgFrameMs(), 1040.0 / 101.0));
	CHECK(PNear(st.maxFrameMs, 40.0));
	CHECK(st.Percentile(0.99f) >= 10.0f && st.Percentile(0.99f) <= 10.11f);   // 100. z 101 klatek
	CHECK(st.Percentile(1.0f) >= 40.0f && st.Percentile(1.0f) <= 40.11f);
	CHECK(PNear(st.sec[mv].sumSelf / st.frames, 130.0 / 101.0));
	CHECK(PNear(st.sec[mv].maxSelf, 30.0));
	CHECK(PNear(st.sec[an].sumSelf / st.frames, 1.0));
	CHECK(st.sec[an].calls == 2020);
	CHECK(PNear(st.sumPluginMs / st.frames, 231.0 / 101.0) && PNear(st.maxPluginMs, 31.0));
	std::vector<std::string> lines;
	p->SummaryLines(lines);
	std::string all;
	for (auto& l : lines)
		all += l + "\n";
	CHECK(lines.size() == 5);   // naglowek, plugin, kolumny, 2 sekcje z wywolaniami
	CHECK(PHas(all, "Prof: podsumowanie zTest od resetu: 101 klatek (1.0 s), klatka sr 10.30 / p99 10.1 / max 40.0 ms, przycinek 1, przerw 0"));
	CHECK(PHas(all, "plugin sr 2.287 ms/kl"));
	CHECK(PHas(all, "Moves") && PHas(all, "1.287") && PHas(all, "30.000") && PHas(all, "1.00"));
	CHECK(PHas(all, "20.00"));   // Anis.Hook: wywolan na klatke
	CHECK(all.find("Moves") < all.find("Anis.Hook"));   // od najdrozszej
	CHECK(!PHas(all, "Unused"));
	std::string sh = p->ShortSummary();
	CHECK(PHas(sh, "zTest: 101 kl.") && PHas(sh, "p99 10.1") && PHas(sh, "przycinek 1") && PHas(sh, "Moves 1.29 ms/kl"));
	p->Reset();
	CHECK(p->Total().frames == 0 && p->Total().spikes == 0 && p->Total().sec[mv].calls == 0);
}

TEST(prof_periodic_summary_every_n_seconds) {
	auto p = PMake("zTest", 25.0f, 1.0f);
	uint64_t t = 0;
	p->Mark(t, nullptr);
	int summaries = 0;
	for (int i = 0; i < 130; i++) {   // 130 x 16 ms = 2,08 s
		t += PMs(16);
		std::vector<std::string> out;
		p->Mark(t, &out);
		for (auto& l : out)
			if (PHas(l, "Prof: podsumowanie zTest z ostatnich 1 s"))
				summaries++;
	}
	CHECK(summaries == 2);
	CHECK(p->Total().frames == 130);   // podsumowanie okresowe nie zeruje statystyk od resetu
}

// ---------- Reach (doskok do celu i zasieg ciosow W3) ----------
#include "Core/Reach.h"

TEST(reach_gap_subtracts_ideal_and_root_motion_ahead) {
	// odleglosc silnika 160 (do bboxa), zasieg 120, margines 25 -> idealna 95; animacja i tak zrobi 30 cm
	CHECK(std::fabs(ReachGap(160.0f, 120.0f, 25.0f, 30.0f) - 35.0f) < 0.01f);
	CHECK(ReachGap(80.0f, 120.0f, 25.0f, 0.0f) < 0.0f);           // blizej niz idealnie
	CHECK(std::fabs(ReachGap(160.0f, 120.0f, 25.0f, -10.0f) - 75.0f) < 0.01f);   // animacja cofa - brakuje wiecej
}

TEST(reach_time_to_hit_from_frames) {
	CHECK(std::fabs(ReachTimeToHit(10.0f, 4.0f, 25.0f) - 0.24f) < 1e-4f);
	CHECK(ReachTimeToHit(10.0f, 10.5f, 25.0f) == 0.0f);   // po klatce ciosu
	CHECK(ReachTimeToHit(10.0f, 4.0f, 0.0f) == 0.0f);     // bez tempa
}

TEST(reach_step_spreads_gap_over_time_to_hit) {
	ReachParams p;
	p.accel = 1e9f;
	ReachState s;
	float step = ReachStep(p, s, 90.0f, 150.0f, 0.3f, 0.02f);
	CHECK(std::fabs(step - 6.0f) < 0.01f);   // 90 cm w 0,3 s = 300 cm/s
	CHECK(std::fabs(s.path - 6.0f) < 0.01f);
	CHECK(std::fabs(s.speed - 300.0f) < 0.1f);
}

TEST(reach_step_caps_speed_and_acceleration) {
	ReachParams p;
	p.accel = 1e9f;
	ReachState s;
	CHECK(std::fabs(ReachStep(p, s, 500.0f, 600.0f, 0.2f, 0.02f) - p.maxSpeed * 0.02f) < 0.01f);
	ReachParams q;   // domyslne przyspieszenie: z miejsca najwyzej accel * dt
	ReachState s2;
	float step = ReachStep(q, s2, 90.0f, 150.0f, 0.3f, 0.02f);
	CHECK(step > 0.0f && step <= q.accel * 0.02f * 0.02f + 1e-3f);
}

TEST(reach_step_never_overshoots_gap_or_path_budget) {
	ReachParams p;
	p.accel = 1e9f;
	ReachState s;
	CHECK(ReachStep(p, s, 3.0f, 100.0f, 0.01f, 0.02f) <= 3.0f + 1e-4f);
	ReachState t;
	t.path = p.maxPath - 2.0f;
	CHECK(ReachStep(p, t, 90.0f, 150.0f, 0.1f, 0.05f) <= 2.0f + 1e-4f);
	CHECK(t.path <= p.maxPath + 1e-3f);
	CHECK(ReachStep(p, t, 90.0f, 150.0f, 0.1f, 0.05f) == 0.0f);   // budzet wyczerpany
}

TEST(reach_step_no_move_when_closer_than_ideal) {
	ReachParams p;
	ReachState s;
	s.speed = 200.0f;
	CHECK(ReachStep(p, s, -20.0f, 60.0f, 0.3f, 0.02f) == 0.0f);
	CHECK(s.speed == 0.0f);
}

TEST(reach_step_backs_off_when_target_overlaps) {
	ReachParams p;
	ReachState s;
	float step = ReachStep(p, s, -90.0f, 5.0f, 0.3f, 0.02f);   // po ruchu animacji 5 cm od bboxa (< minSurface 15)
	CHECK(step < 0.0f);
	CHECK(-step <= p.backSpeed * 0.02f + 1e-4f);
	CHECK(s.path > 0.0f);   // odsuniecie tez z budzetu drogi
	ReachState u;
	CHECK(-ReachStep(p, u, -90.0f, 14.0f, 0.3f, 1.0f) <= 1.0f + 1e-4f);   // najwyzej do minSurface
}

TEST(reach_step_stops_after_hit_frame) {
	ReachParams p;
	ReachState s;
	CHECK(ReachStep(p, s, 80.0f, 150.0f, 0.0f, 0.02f) == 0.0f);
	CHECK(ReachStep(p, s, -90.0f, 5.0f, 0.0f, 0.02f) == 0.0f);
}

TEST(reach_step_arrives_at_ideal_by_hit_on_static_target) {
	ReachParams p;   // domyslne: 450 cm/s, przyspieszenie 3000
	ReachState s;
	float gap = 100.0f, t = 0.4f, dt = 1.0f / 60.0f;
	while (t > 0.0f) {
		float step = ReachStep(p, s, gap, 95.0f + gap, t, dt);
		CHECK(step >= 0.0f && step <= gap + 1e-4f);
		gap -= step;
		t -= dt;
	}
	CHECK(gap < 2.0f && gap > -1e-3f);
	CHECK(s.path <= p.maxPath);
}

TEST(reach_step_follows_retreating_target_within_budget) {
	ReachParams p;
	ReachState s;
	float gap = 40.0f, t = 0.48f, dt = 1.0f / 60.0f;
	while (t > 0.0f) {
		gap += 300.0f * dt;   // wilk odskakuje 3 m/s
		gap -= ReachStep(p, s, gap, 95.0f + gap, t, dt);
		t -= dt;
	}
	CHECK(s.path <= p.maxPath + 1e-3f);
	CHECK(s.path > 100.0f);   // nadaza, az do limitu drogi / predkosci
}

TEST(reach_target_ok_cone_and_distance) {
	ReachParams p;   // stozek 45 st., maxPath 130 + startSlack 60
	const float D = 3.14159265f / 180.0f;
	CHECK(ReachTargetOk(p, 0.0f, 0, 0, 100.0f * std::sin(40 * D), 100.0f * std::cos(40 * D), 50.0f));
	CHECK(!ReachTargetOk(p, 0.0f, 0, 0, 100.0f * std::sin(50 * D), 100.0f * std::cos(50 * D), 50.0f));
	CHECK(!ReachTargetOk(p, 0.0f, 0, 0, 0.0f, -100.0f, 50.0f));   // za plecami
	CHECK(ReachTargetOk(p, 0.0f, 0, 0, 0.0f, 300.0f, p.maxPath + p.startSlack - 1.0f));
	CHECK(!ReachTargetOk(p, 0.0f, 0, 0, 0.0f, 300.0f, p.maxPath + p.startSlack + 1.0f));   // za daleko
	CHECK(ReachTargetOk(p, 0.0f, 0, 0, 0.0f, 50.0f, -40.0f));     // blisko - nadaje sie (bez ruchu albo w tyl)
	CHECK(ReachTargetOk(p, 1.5708f, 0, 0, 100.0f, 0.0f, 10.0f));  // bohater patrzy w +X
}

TEST(reach_pick_prefers_engine_target_then_focus_then_best_angle) {
	ReachParams p;
	std::vector<ReachCandidate> c(3);
	c[0].x = 0.0f;  c[0].z = 120.0f; c[0].gap = 20.0f;              // prosto, blisko
	c[1].x = 60.0f; c[1].z = 120.0f; c[1].gap = 30.0f; c[1].focus = true;
	c[2].x = -50.0f; c[2].z = 150.0f; c[2].gap = 40.0f; c[2].hitTarget = true;
	CHECK(PickReachTarget(p, 0.0f, 0, 0, c) == 2);
	c[2].z = -150.0f;   // cel silnika za plecami -> fokus
	CHECK(PickReachTarget(p, 0.0f, 0, 0, c) == 1);
	c[1].focus = false;  // bez fokusu -> najmniejszy kat
	CHECK(PickReachTarget(p, 0.0f, 0, 0, c) == 0);
	c[0].gap = 500.0f;   // za daleko -> nastepny w stozku
	CHECK(PickReachTarget(p, 0.0f, 0, 0, c) == 1);
	std::vector<ReachCandidate> none(1);
	none[0].x = 0.0f; none[0].z = -100.0f;
	CHECK(PickReachTarget(p, 0.0f, 0, 0, none) == -1);
	CHECK(PickReachTarget(p, 0.0f, 0, 0, std::vector<ReachCandidate>()) == -1);
}

TEST(reach_pick_equal_angle_prefers_closer) {
	ReachParams p;
	std::vector<ReachCandidate> c(2);
	c[0].x = 0.0f; c[0].z = 200.0f; c[0].gap = 80.0f;
	c[1].x = 0.0f; c[1].z = 140.0f; c[1].gap = 20.0f;
	CHECK(PickReachTarget(p, 0.0f, 0, 0, c) == 1);
}

TEST(reach_in_range_with_bonus) {
	CHECK(ReachInRange(150.0f, 120.0f, 40.0f));
	CHECK(ReachInRange(160.0f, 120.0f, 40.0f));
	CHECK(!ReachInRange(161.0f, 120.0f, 40.0f));
	CHECK(!ReachInRange(121.0f, 120.0f, 0.0f));
}

// ---------- Znaki Wiedzmina (Core/Signs) ----------
#include "Core/Signs.h"

TEST(signs_names_and_spell_ids) {
	CHECK(SignFromName("AARD") == SIGN_AARD);
	CHECK(SignFromName("quen") == SIGN_QUEN);
	CHECK(SignFromName("INFO") == -1);
	CHECK(std::string(SignName(SIGN_YRDEN)) == "Yrden");
	int ids[SIGN_COUNT] = { 90, 91, 92, 93, 94 };
	CHECK(SignFromSpellId(ids, 92) == SIGN_AXII);
	CHECK(SignFromSpellId(ids, 99) == -1);
	CHECK(SignFromSpellId(ids, -1) == -1);
}
TEST(signs_parse_ani_letters) {
	std::string out[SIGN_COUNT];
	CHECK(ParseSignLetters("WND,FBT,SLE,FEA,HEA", out));
	CHECK(out[0] == "WND" && out[2] == "SLE" && out[4] == "HEA");
	std::string keep[SIGN_COUNT] = { "A", "B", "C", "D", "E" };
	CHECK(!ParseSignLetters("WND,FBT", keep));                  // za malo
	CHECK(!ParseSignLetters("WND,FBT,S E,FEA,HEA", keep));      // smieci
	CHECK(!ParseSignLetters("WND,FBT,SLEEPY,FEA,HEA", keep));   // za dlugie
	CHECK(keep[0] == "A" && keep[4] == "E");                    // bez zmian przy bledzie
	CHECK(ParseSignLetters("wnd, fbt ,sle,fea,hea", out) && out[1] == "FBT");
}
TEST(signs_cast_check_mana_cooldown_disabled) {
	CHECK(SignCheckCast(true, 20, 15, 0.0f) == SIGNCAST_OK);
	CHECK(SignCheckCast(true, 15, 15, 0.0f) == SIGNCAST_OK);
	CHECK(SignCheckCast(true, 14, 15, 0.0f) == SIGNCAST_NO_MANA);
	CHECK(SignCheckCast(true, 100, 15, 0.4f) == SIGNCAST_COOLDOWN);
	CHECK(SignCheckCast(true, 0, 15, 0.4f) == SIGNCAST_COOLDOWN);   // odnowienie pierwsze (komunikat o czasie)
	CHECK(SignCheckCast(false, 100, 15, 0.0f) == SIGNCAST_DISABLED);
	CHECK(SignCheckCast(true, 0, 0, 0.0f) == SIGNCAST_OK);          // koszt 0 - bez many
}
TEST(signs_cooldowns_per_sign) {
	SignCooldowns cd;
	cd.Start(SIGN_IGNI, 1.0f);
	CHECK(cd.Left(SIGN_IGNI) > 0.99f && cd.Left(SIGN_AARD) == 0.0f);
	cd.Update(0.6f);
	CHECK(std::fabs(cd.Left(SIGN_IGNI) - 0.4f) < 1e-4f);
	cd.Update(0.6f);
	CHECK(cd.Left(SIGN_IGNI) == 0.0f);   // nie schodzi ponizej zera
	cd.Start(SIGN_QUEN, 2.0f);
	cd.Clear();
	CHECK(cd.Left(SIGN_QUEN) == 0.0f);
	CHECK(cd.Left(-1) == 0.0f && cd.Left(SIGN_COUNT) == 0.0f);   // zly indeks bez wyjscia poza tablice
}
TEST(signs_cone_front_side_back_and_range) {
	// bohater w (0,0,0) patrzy w +Z, zasieg 600, +-50 st.
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 0, 500, 600, 50, 250).inside);
	CHECK(ConeTest(0, 0, 0, 0, 1, 300, 0, 300, 600, 50, 250).inside);     // 45 st.
	CHECK(!ConeTest(0, 0, 0, 0, 1, 400, 0, 300, 600, 50, 250).inside);    // ~53 st.
	CHECK(!ConeTest(0, 0, 0, 0, 1, 0, 0, -300, 600, 50, 250).inside);     // za plecami
	CHECK(!ConeTest(0, 0, 0, 0, 1, 0, 0, 601, 600, 50, 250).inside);      // za daleko
	CHECK(!ConeTest(0, 0, 0, 0, 1, 0, 300, 300, 600, 50, 250).inside);    // pietro wyzej
	ConeHit h = ConeTest(100, 0, 100, 1, 0, 400, 0, 100, 600, 50, 250);   // patrzy w +X
	CHECK(h.inside && std::fabs(h.dist - 300.0f) < 0.01f && h.angleDeg < 0.01f);
	// kierunek nieznormalizowany daje to samo
	CHECK(ConeTest(0, 0, 0, 0, 5, 300, 0, 300, 600, 50, 250).inside);
}
TEST(signs_cone_near_target_counts_even_at_side) {
	// tuz przy bohaterze (wtulony w bok) - kat bez znaczenia, ale nie za plecami
	CHECK(ConeTest(0, 0, 0, 0, 1, 50, 0, 10, 600, 35, 250, 60).inside);
	CHECK(!ConeTest(0, 0, 0, 0, 1, 0, 0, -50, 600, 35, 250, 60).inside);
}
TEST(signs_attitude_values_match_constants_d) {
	// Constants.d (G2, New Balance): ATT_HOSTILE 0, ATT_ANGRY 1, ATT_NEUTRAL 2, ATT_FRIENDLY 3 - jak enum NPC_ATT_* silnika.
	// Blad 20.10: plugin mial 1..4, wiec wrogi potwor (postawa 0) nie byl "wrogiem" i znaki trafialy tylko walczacych.
	CHECK(ATTITUDE_HOSTILE == 0 && ATTITUDE_ANGRY == 1 && ATTITUDE_NEUTRAL == 2 && ATTITUDE_FRIENDLY == 3);
	SignTargetInfo d;
	CHECK(d.attitude == 2);
}
TEST(signs_target_filter_monsters_without_aggro) {
	// log NB 20.10: "Igni: pominiety SPIDER 567 cm: filtr (postawa 0, walczy z bohaterem 0, ...)" - pajak wrogi, jeszcze
	// nie zaatakowal; ma byc celem
	SignTargetInfo spider;
	spider.attitude = 0;
	CHECK(SignIsEnemy(spider) && SignAffects(spider, false));
	CHECK(SignRejectReason(spider, false) == nullptr);
	SignTargetInfo sheep;                          // neutralny potwor (owca, scierwojad pod NPC_ATT_NEUTRAL) - jak czar gry
	sheep.attitude = ATTITUDE_NEUTRAL;
	CHECK(!SignIsEnemy(sheep) && SignAffects(sheep, false));
	SignTargetInfo angryBeast = sheep;
	angryBeast.attitude = ATTITUDE_ANGRY;
	CHECK(SignAffects(angryBeast, false));
	SignTargetInfo pet = sheep;                    // przyjazny potwor (zwierze bohatera bez AIV_PARTYMEMBER)
	pet.attitude = ATTITUDE_FRIENDLY;
	CHECK(!SignAffects(pet, true));
	CHECK(SignRejectReason(pet, true) != nullptr && std::string(SignRejectReason(pet, true)) == "przyjaciel");
}
TEST(signs_target_filter_humans_neutral_by_option_immortal_never) {
	SignTargetInfo citizen;
	citizen.human = true;
	citizen.attitude = ATTITUDE_NEUTRAL;
	CHECK(!SignAffects(citizen, false));           // mieszczanin - tylko z SignHitNeutral (przestepstwo jak czar gry)
	CHECK(SignAffects(citizen, true));
	CHECK(std::string(SignRejectReason(citizen, false)) == "neutralny czlowiek (SignHitNeutral=0)");
	SignTargetInfo angry = citizen;
	angry.attitude = ATTITUDE_ANGRY;
	CHECK(!SignAffects(angry, false) && SignAffects(angry, true));
	SignTargetInfo bandit = citizen;
	bandit.attitude = ATTITUDE_HOSTILE;
	CHECK(SignAffects(bandit, false));
	SignTargetInfo story = bandit;                 // NPC_FLAG_IMMORTAL - chroniony skryptem (jak B_MagicHurtNpc w NB)
	story.immortal = true;
	CHECK(!SignAffects(story, true));
	CHECK(std::string(SignRejectReason(story, true)) == "niesmiertelny (NPC_FLAG_IMMORTAL)");
	SignTargetInfo ally = bandit;
	ally.partyMember = true;
	CHECK(std::string(SignRejectReason(ally, true)) == "druzyna (AIV_PARTYMEMBER)");
	SignTargetInfo corpse = bandit;
	corpse.dead = true;
	CHECK(std::string(SignRejectReason(corpse, true)) == "martwy");
}
TEST(signs_power_from_intellect) {
	// New Balance: zmienna skryptu ATR_INTELLECT; Steam G2 jej nie ma (-1) = bez premii
	CHECK(std::fabs(SignIntPower(-1, 10.0f, 5.0f) - 1.0f) < 1e-6f);
	CHECK(std::fabs(SignIntPower(0, 10.0f, 5.0f) - 1.0f) < 1e-6f);
	CHECK(std::fabs(SignIntPower(100, 10.0f, 5.0f) - 2.0f) < 1e-5f);     // +10% za 10 INT
	CHECK(std::fabs(SignIntPower(55, 10.0f, 5.0f) - 1.55f) < 1e-5f);     // bez zaokraglania do dziesiatek
	CHECK(std::fabs(SignIntPower(1000, 10.0f, 5.0f) - 5.0f) < 1e-5f);    // limit
	CHECK(std::fabs(SignIntPower(300, 0.0f, 5.0f) - 1.0f) < 1e-6f);      // wylaczone
	CHECK(std::fabs(SignIntPower(300, 10.0f, 0.5f) - 1.0f) < 1e-6f);     // limit ponizej x1 = bez premii (nie oslabia)
}
TEST(signs_damage_array_split_for_nb) {
	// NB (UNION_ABI, wlasne OnDamage_Hit): przy napastniku-NPC bierze aryDamage[typ], a nie fDamageTotal - pusta tablica
	// = 0 obrazen -> NPC_MINIMAL_DAMAGE (5). Znak wpisuje obrazenia w tablice.
	unsigned long a[8] = { 0 };
	CHECK(SignSplitDamage(8 /* DAM_FIRE */, 47.6f, a));
	CHECK(a[3] == 48 && a[0] == 0 && a[4] == 0);
	unsigned long b[8] = { 0 };
	CHECK(SignSplitDamage(16 | 32 /* FLY + MAGIC */, 30.0f, b));
	CHECK(b[4] == 15 && b[5] == 15);
	unsigned long c[8] = { 0 };
	c[3] = 7;
	CHECK(!SignSplitDamage(8, 40.0f, c) && c[3] == 7);     // tablica juz wypelniona - bez zmian
	unsigned long z[8] = { 0 };
	CHECK(!SignSplitDamage(0, 40.0f, z) && !SignSplitDamage(8, 0.0f, z));
	CHECK(SignDamageArrayWanted(-1, true) && !SignDamageArrayWanted(-1, false));
	CHECK(SignDamageArrayWanted(1, false) && !SignDamageArrayWanted(0, true));
}
TEST(signs_target_filter_enemies_only_by_default) {
	SignTargetInfo t;
	t.human = true;
	t.attitude = ATTITUDE_HOSTILE;
	CHECK(SignIsEnemy(t) && SignAffects(t, false));
	t.attitude = ATTITUDE_NEUTRAL;
	CHECK(!SignIsEnemy(t) && !SignAffects(t, false));
	CHECK(SignAffects(t, true));                           // SignHitNeutral=1
	t.targetsHero = true;                                  // neutralny, ale walczy z bohaterem
	CHECK(SignIsEnemy(t) && SignAffects(t, false));
	SignTargetInfo f;
	f.human = true;
	f.attitude = ATTITUDE_FRIENDLY;
	CHECK(!SignAffects(f, true));                          // przyjaciel nigdy
	SignTargetInfo e;
	e.attitude = ATTITUDE_HOSTILE;
	e.partyMember = true;
	CHECK(!SignAffects(e, true));
	e.partyMember = false; e.dead = true;
	CHECK(!SignAffects(e, true));
	e.dead = false; e.unconscious = true;
	CHECK(!SignAffects(e, true));
	e.unconscious = false; e.isPlayer = true;
	CHECK(!SignAffects(e, true));
}
TEST(signs_axii_combat_decision) {
	SignTargetInfo t;
	t.attitude = ATTITUDE_HOSTILE; t.human = true;
	CHECK(AxiiInCombat(t));                                // wrogi bandyta - oszolomienie
	t.attitude = ATTITUDE_NEUTRAL;
	CHECK(!AxiiInCombat(t));                               // mieszczanin - niepamiec
	t.attitude = ATTITUDE_ANGRY;
	CHECK(!AxiiInCombat(t));                               // zly, ale nie walczy - niepamiec (Charm kasuje zlosc)
	t.targetsHero = true;
	CHECK(!AxiiInCombat(t));                               // gapi sie na bohatera bez broni - jeszcze nie walka
	t.weaponDrawn = true;
	CHECK(AxiiInCombat(t));                                // straznik z bronia na bohatera
	SignTargetInfo m;
	m.attitude = ATTITUDE_NEUTRAL; m.human = false; m.targetsHero = true;
	CHECK(AxiiInCombat(m));                                // potwor atakujacy bohatera (bez "broni")
	m.targetsHero = false;
	CHECK(!AxiiInCombat(m));                               // owca
}
TEST(signs_axii_pick_focus_in_cone_then_nearest) {
	// cele zbudowane z ConeTest: bohater w (0,0,0) patrzy w +Z (kierunek celownika), zasieg 1000, +-20 st.
	auto cand = [](int id, float x, float y, float z, bool focus = false) {
		ConeHit h = ConeTest(0, 0, 0, 0, 1, x, y, z, 1000.0f, 20.0f, 250.0f);
		AxiiCandidate c;
		c.id = id; c.dist = h.dist; c.angleDeg = h.angleDeg; c.cone = h.reason; c.focus = focus;
		return c;
	};
	std::vector<AxiiCandidate> c = { cand(1, 0, 0, 900), cand(2, 100, 0, 600), cand(3, 300, 0, 300), cand(4, 0, 0, 1200) };
	CHECK(PickAxiiTarget(c) == 2);                       // najblizszy w stozku (3 - 45 st., 4 - za daleko)
	c[0].focus = true;
	CHECK(PickAxiiTarget(c) == 1);                       // fokus w stozku ma pierwszenstwo, nawet dalszy
	c[0].focus = false;
	c[2].focus = true;
	CHECK(PickAxiiTarget(c) == 2);                       // fokus poza stozkiem - nie wygrywa
	c[3].focus = true;
	CHECK(PickAxiiTarget(c) == 2);                       // fokus za daleko - tez nie
	// 9-10 m: daleko, ale w zasiegu 10 m (log NB 20.10: Sonja 852-912 cm odrzucona przy starym 800)
	std::vector<AxiiCandidate> far = { cand(5, 0, 0, 912) };
	CHECK(PickAxiiTarget(far) == 5);
	// filtr: martwy / druzyna odpada, nastepny najblizszy wygrywa
	c = { cand(1, 0, 0, 300), cand(2, 0, 0, 500) };
	c[0].reject = "martwy";
	CHECK(PickAxiiTarget(c) == 2);
	// za sciana (bez linii wzroku) - pomijany, takze jako fokus
	c = { cand(1, 0, 0, 300, true), cand(2, 0, 0, 500) };
	c[0].visible = false;
	CHECK(PickAxiiTarget(c) == 2);
	c[1].visible = false;
	CHECK(PickAxiiTarget(c) == -1);
	CHECK(PickAxiiTarget({}) == -1);
	// rowna odleglosc - mniejszy kat
	c = { cand(1, 100, 0, 500), cand(2, 0, 0, 510) };
	c[0].dist = c[1].dist = 510.0f;
	CHECK(PickAxiiTarget(c) == 2);
}
TEST(signs_axii_candidate_reason_for_log) {
	AxiiCandidate c;
	c.id = 1; c.dist = 500.0f; c.cone = CONE_OK;
	CHECK(AxiiCandidateReason(c) == nullptr);
	c.cone = CONE_RANGE;
	CHECK(std::string(AxiiCandidateReason(c)) == "zasieg");
	c.cone = CONE_ANGLE;
	CHECK(std::string(AxiiCandidateReason(c)) == "kat");
	c.cone = CONE_HEIGHT;
	CHECK(std::string(AxiiCandidateReason(c)) == "wysokosc");
	c.cone = CONE_OK;
	c.visible = false;
	CHECK(std::string(AxiiCandidateReason(c)) == "zasloniety");
	c.reject = "druzyna (AIV_PARTYMEMBER)";
	CHECK(std::string(AxiiCandidateReason(c)) == "druzyna (AIV_PARTYMEMBER)");   // filtr przed geometria
}
TEST(signs_axii_target_filter_allows_neutral_and_friends) {
	// Axii poza walka (Urok / trans) to nie atak: neutralni i przyjazni ludzie sa celem; wrogowie - oszolomienie
	SignTargetInfo t;
	t.human = true; t.attitude = ATTITUDE_FRIENDLY;
	CHECK(AxiiRejectReason(t) == nullptr);
	t.attitude = ATTITUDE_NEUTRAL;
	CHECK(AxiiRejectReason(t) == nullptr);
	t.attitude = ATTITUDE_HOSTILE;
	CHECK(AxiiRejectReason(t) == nullptr);
	t.immortal = true;                                     // odpornosc rozstrzyga lista Uroku (Spell_Logic_Charm)
	CHECK(AxiiRejectReason(t) == nullptr);
	SignTargetInfo d = t; d.dead = true;
	CHECK(std::string(AxiiRejectReason(d)) == "martwy");
	SignTargetInfo u = t; u.unconscious = true;
	CHECK(std::string(AxiiRejectReason(u)) == "nieprzytomny");
	SignTargetInfo p = t; p.partyMember = true;
	CHECK(std::string(AxiiRejectReason(p)) == "druzyna (AIV_PARTYMEMBER)");
	SignTargetInfo h = t; h.isPlayer = true;
	CHECK(std::string(AxiiRejectReason(h)) == "bohater");
}
TEST(signs_cone_angle_known_even_out_of_range) {
	// diagnostyka: kat liczony tez dla celow za daleko (log "pominiety ... kat 45 st.: zasieg", a nie zawsze "kat 0")
	ConeHit h = ConeTest(0, 0, 0, 0, 1, 1000, 0, 1000, 600, 50, 250);
	CHECK(!h.inside && h.reason == CONE_RANGE && std::fabs(h.angleDeg - 45.0f) < 0.1f);
	ConeHit up = ConeTest(0, 0, 0, 0, 1, 0, 400, 300, 600, 50, 250);
	CHECK(!up.inside && up.reason == CONE_HEIGHT && up.angleDeg < 0.1f);
}
TEST(signs_damage_power_source) {
	// SignPowerSource: 0 = moc z maks. many, 1 = mnoznik z INT, 2 = iloczyn obu (domyslnie - jak runda 20.10)
	SignDamagePower m = SignDamagePowerOf(SIGN_POWER_MANA, 1.5f, 100, 10.0f, 5.0f);
	CHECK(std::fabs(m.mul - 1.5f) < 1e-5f && m.source == SIGN_POWER_MANA && !m.fellBack);
	SignDamagePower i = SignDamagePowerOf(SIGN_POWER_INT, 1.5f, 100, 10.0f, 5.0f);
	CHECK(std::fabs(i.mul - 2.0f) < 1e-5f && i.source == SIGN_POWER_INT && !i.fellBack);
	SignDamagePower b = SignDamagePowerOf(SIGN_POWER_BOTH, 1.5f, 100, 10.0f, 5.0f);
	CHECK(std::fabs(b.mul - 3.0f) < 1e-5f && b.source == SIGN_POWER_BOTH && !b.fellBack);
	// Steam G2 (brak ATR_INTELLECT = -1): INT i oba zachowuja sie jak mana, z flaga do logu
	SignDamagePower si = SignDamagePowerOf(SIGN_POWER_INT, 1.5f, -1, 10.0f, 5.0f);
	CHECK(std::fabs(si.mul - 1.5f) < 1e-5f && si.source == SIGN_POWER_MANA && si.fellBack);
	SignDamagePower sb = SignDamagePowerOf(SIGN_POWER_BOTH, 1.5f, -1, 10.0f, 5.0f);
	CHECK(std::fabs(sb.mul - 1.5f) < 1e-5f && sb.source == SIGN_POWER_MANA && sb.fellBack);
	// INT 0 w NB (jest zmienna) - nie spada na mane: mnoznik INT x1
	SignDamagePower z = SignDamagePowerOf(SIGN_POWER_INT, 1.5f, 0, 10.0f, 5.0f);
	CHECK(std::fabs(z.mul - 1.0f) < 1e-5f && z.source == SIGN_POWER_INT && !z.fellBack);
	// zla wartosc ustawienia = oba
	CHECK(SignDamagePowerOf(7, 1.0f, 10, 10.0f, 5.0f).source == SIGN_POWER_BOTH);
	CHECK(std::string(SignPowerSourceName(SIGN_POWER_MANA)) == "mana");
	CHECK(std::string(SignPowerSourceName(SIGN_POWER_INT)) == "INT");
	CHECK(std::string(SignPowerSourceName(SIGN_POWER_BOTH)) == "oba");
}
TEST(signs_axii_range_migration) {
	// SignDefaultsRev 7: stary domyslny zasieg Axii 800 -> 1000, stozek 30 -> 20 (zmienione recznie zostaja)
	CHECK(std::fabs(MigrateDefault(800.0f, 800.0f, 1000.0f) - 1000.0f) < 1e-4f);
	CHECK(std::fabs(MigrateDefault(1200.0f, 800.0f, 1000.0f) - 1200.0f) < 1e-4f);
	CHECK(std::fabs(MigrateDefault(30.0f, 30.0f, 20.0f) - 20.0f) < 1e-4f);
}
TEST(signs_quen_absorbs_one_hit_then_breaks) {
	QuenState q;
	CHECK(!q.Absorb());                    // bez tarczy nic
	q.Cast(30.0f);
	CHECK(q.active);
	CHECK(q.Absorb());                     // pierwsze trafienie pochloniete
	CHECK(!q.active);
	CHECK(!q.Absorb());                    // drugie juz nie
	CHECK(q.TakeBreak());                  // pekniecie do obsluzenia (odrzut wrogow)
	CHECK(!q.TakeBreak());                 // tylko raz
}
TEST(signs_quen_expires_and_recast_renews) {
	QuenState q;
	q.Cast(30.0f);
	CHECK(!q.Update(29.0f));
	q.Cast(30.0f);                         // odnowienie przed koncem
	CHECK(!q.Update(29.0f) && q.active);
	CHECK(q.Update(1.5f));                 // wygasla w tej klatce
	CHECK(!q.active && !q.Update(1.0f));   // jednorazowo
	CHECK(!q.TakeBreak());                 // wygasniecie to nie pekniecie
	q.Cast(30.0f);
	q.Clear();
	CHECK(!q.active && !q.Absorb());
}
TEST(signs_yrden_zone_contains_and_expires) {
	YrdenZone z;
	CHECK(!z.Contains(0, 0, 0));
	z.Start(100, 0, 100, 500, 15);
	CHECK(z.Contains(100, 0, 100));
	CHECK(z.Contains(500, 0, 400));        // 500 cm po przekatnej
	CHECK(!z.Contains(500, 0, 500));       // ~566 cm
	CHECK(!z.Contains(100, 400, 100));     // inne pietro
	CHECK(!z.Update(14.0f));
	CHECK(z.Update(1.5f) && !z.active);
	CHECK(!z.Contains(100, 0, 100));
	z.Start(0, 0, 0, 500, 15);
	z.Start(1000, 0, 0, 500, 15);          // ponowny rzut przenosi strefe
	CHECK(!z.Contains(0, 0, 0) && z.Contains(1000, 0, 0));
}
TEST(signs_yrden_enter_leave_plan) {
	YrdenPlan p = YrdenStep({ 1, 2, 3 }, { 2, 3, 4 });
	CHECK(p.enter.size() == 1 && p.enter[0] == 4);
	CHECK(p.leave.size() == 1 && p.leave[0] == 1);
	YrdenPlan q = YrdenStep({}, {});
	CHECK(q.enter.empty() && q.leave.empty());
}
TEST(signs_yrden_apply_and_exact_restore) {
	NpcCombatStats cur;
	cur.timeScale = 1.0f;
	int prot[SIGN_PROT_COUNT] = { 0, 25, 25, 10, 50, 7, 25, -1 };   // barrier blunt edge fire fly magic point fall
	for (int i = 0; i < SIGN_PROT_COUNT; i++) cur.prot[i] = prot[i];
	cur.prot[1] = -1;                                                // blunt IMMUNE
	YrdenMod m = YrdenApply(cur, 0.65f, 30.0f, YrdenArmorMask());
	CHECK(std::fabs(m.applied.timeScale - 0.65f) < 1e-6f);
	CHECK(m.applied.prot[1] == -1);                                  // odpornosc zostaje
	CHECK(m.applied.prot[2] == 18);                                  // 25 * 0,7 = 17,5 -> 18
	CHECK(m.applied.prot[3] == 7 && m.applied.prot[5] == 5 && m.applied.prot[6] == 18);
	CHECK(m.applied.prot[4] == 50 && m.applied.prot[0] == 0);       // lot i bariera bez zmian
	NpcCombatStats back = YrdenRestore(m, m.applied);
	CHECK(back.timeScale == 1.0f);
	for (int i = 0; i < SIGN_PROT_COUNT; i++)
		CHECK(back.prot[i] == cur.prot[i]);
}
TEST(signs_yrden_restore_keeps_foreign_changes) {
	NpcCombatStats cur;
	cur.timeScale = 1.0f;
	for (int i = 0; i < SIGN_PROT_COUNT; i++) cur.prot[i] = 20;
	YrdenMod m = YrdenApply(cur, 0.5f, 50.0f, YrdenArmorMask());
	NpcCombatStats now = m.applied;
	now.prot[2] += 30;                     // w strefie skrypt dodal ochrony (np. zbroja)
	now.timeScale = 0.25f;                 // ktos inny tez spowolnil model
	NpcCombatStats back = YrdenRestore(m, now);
	CHECK(back.prot[2] == 50);             // 20 + 30, nasza czesc cofnieta
	CHECK(back.prot[3] == 20);
	CHECK(std::fabs(back.timeScale - 0.5f) < 1e-6f);   // cofniete tylko nasze x0,5
}
TEST(signs_yrden_tempo_deferred_while_model_frozen_by_rewind) {
	NpcCombatStats cur;
	cur.timeScale = 1.0f;
	YrdenMod m = YrdenApply(cur, 0.6f, 0.0f, 0u);   // podtrzymanie znaku: tylko tempo bohatera
	// zTimeRewind zamrozil model (0) - powrot teraz dalby 0, wiec odlozony
	CHECK(YrdenTempoDeferred(m, 0.0f));
	CHECK(YrdenRestoreTempo(m, 0.0f) == 0.0f);      // (dlatego nie liczymy go przy 0)
	CHECK(!YrdenTempoDeferred(m, 0.6f) && !YrdenTempoDeferred(m, 1.15f));
	// po cofnieciu zTimeRewind oddaje nasze 0,6 -> oryginal 1,0 (bez odlozenia zostawaloby 0,6 na stale)
	CHECK(YrdenRestoreTempo(m, 0.6f) == 1.0f);
	// NB przepisal tempo w miedzyczasie (1,15) - cofamy tylko nasze x0,6 (jak dotad)
	CHECK(std::fabs(YrdenRestoreTempo(m, 1.15f) - 1.15f / 0.6f) < 1e-5f);
	// nalozone na zamrozony model (applied 0) - nic do odkladania, tempo nie ruszane
	YrdenMod z = YrdenApply(NpcCombatStats{ 0.0f, {} }, 0.6f, 0.0f, 0u);
	CHECK(!YrdenTempoDeferred(z, 0.0f) && YrdenRestoreTempo(z, 1.0f) == 1.0f);
	// bez spowolnienia (slow 1) - nic do odkladania
	YrdenMod one = YrdenApply(cur, 1.0f, 30.0f, YrdenArmorMask());
	CHECK(!YrdenTempoDeferred(one, 0.0f));
}
TEST(signs_yrden_save_cycle_restores_and_reapplies_exactly) {
	NpcCombatStats npc;
	npc.timeScale = 1.0f;
	int prot[SIGN_PROT_COUNT] = { 0, 33, 41, 12, 15, 9, 41, 0 };
	for (int i = 0; i < SIGN_PROT_COUNT; i++) npc.prot[i] = prot[i];
	const NpcCombatStats orig = npc;
	YrdenMod m = YrdenApply(npc, 0.65f, 30.0f, YrdenArmorMask());
	npc = m.applied;
	// zapis gry: cofniecie (do pliku ida wartosci oryginalne)
	npc = YrdenRestore(m, npc);
	CHECK(npc.timeScale == orig.timeScale);
	for (int i = 0; i < SIGN_PROT_COUNT; i++)
		CHECK(npc.prot[i] == orig.prot[i]);
	// po zapisie: strefa trwa - ponowne nalozenie daje te same wartosci co przed zapisem
	YrdenMod m2 = YrdenApply(npc, 0.65f, 30.0f, YrdenArmorMask());
	CHECK(m2.applied.timeScale == m.applied.timeScale);
	for (int i = 0; i < SIGN_PROT_COUNT; i++)
		CHECK(m2.applied.prot[i] == m.applied.prot[i]);
	npc = m2.applied;
	// wyjscie ze strefy - dokladnie oryginal
	npc = YrdenRestore(m2, npc);
	CHECK(npc.timeScale == orig.timeScale);
	for (int i = 0; i < SIGN_PROT_COUNT; i++)
		CHECK(npc.prot[i] == orig.prot[i]);
}
TEST(signs_yrden_mask_covers_spec_types) {
	uint32_t m = YrdenArmorMask();
	CHECK((m & (1u << 1)) && (m & (1u << 2)) && (m & (1u << 3)) && (m & (1u << 5)) && (m & (1u << 6)));
	CHECK(!(m & (1u << 0)) && !(m & (1u << 4)) && !(m & (1u << 7)));
}
TEST(signs_burn_override_sets_duration_and_rate) {
	// komunikat palenia silnika (oCMsgDamage EV_DAMAGE_PER_FRAME): czas i obrazenia na sekunde z ini
	// nowe palenie od Igni: silnik dal 1000 ms na punkt obrazen (40 s przy 40 pkt) - skracamy do IgniBurnTime
	BurnTiming b = BurnOverride(40000.0f, true, false, 3.0f, 5.0f);
	CHECK(b.durationMs == 3000.0f && b.intervalMs == 1000.0f && b.damagePerInterval == 5.0f && b.setRate);
	CHECK(!b.setTotal);                    // jeszcze nie przetworzony - silnik sam przepisze obrazenia na takt
	BurnTiming n = BurnOverride(40000.0f, true, true, 3.0f, 5.0f);
	CHECK(n.setTotal);                     // przetworzony od razu w OnMessage - takt trzeba poprawic recznie
	// palenie juz trwalo (NPC plonal): tylko przedluzenie, tempo bez zmian (np. kula ognia z gry)
	BurnTiming a = BurnOverride(1200.0f, false, true, 3.0f, 5.0f);
	CHECK(a.durationMs == 3000.0f && !a.setRate && !a.setTotal);
	BurnTiming l = BurnOverride(5000.0f, false, true, 3.0f, 5.0f);
	CHECK(l.durationMs == 5000.0f);        // dluzsze palenie nie jest skracane
}
TEST(signs_burn_tick_covers_fire_protection) {
	// runda 5 (blad "palenie zadaje 0"): silnik odejmuje ochrone przed ogniem od KAZDEGO taktu (scierwojad: ochrona 35,
	// takt 7 -> 0). Takt = obrazenia na sekunde (zaokraglone) + ochrona - po odjeciu zostaje dokladnie IgniBurnDps x P.
	BurnTiming b = BurnOverride(18000.0f, true, false, 3.0f, 6.66f, 35);
	CHECK(b.damagePerInterval == 42.0f);   // 7 + 35
	BurnTiming z = BurnOverride(18000.0f, true, false, 3.0f, 5.0f, 0);
	CHECK(z.damagePerInterval == 5.0f);
	BurnTiming i = BurnOverride(18000.0f, true, false, 3.0f, 5.0f, -1);
	CHECK(i.damagePerInterval == 5.0f);    // IMMUNE (-1) - silnik i tak nie pali, bez dodatku
	CHECK(BurnTickEffective(42.0f, 35) == 7);
	CHECK(BurnTickEffective(7.0f, 35) == 0);
	CHECK(BurnTickEffective(10.0f, -1) == 0);
}
TEST(signs_menu_real_nearest_index) {
	std::vector<float> v = { 0.4f, 0.5f, 0.55f, 0.6f, 0.65f };
	CHECK(MenuIndexForReal(v, 0.65f) == 4);
	CHECK(MenuIndexForReal(v, 0.56f) == 2);      // reczna wartosc z ini - najblizsza pozycja
	CHECK(MenuIndexForReal(v, 9.0f) == 4);
	CHECK(MenuIndexForReal({}, 1.0f) == -1);
	CHECK(MenuIndexForReal({ 0.0f, 0.001f, 0.002f }, 0.001f) == 1);   // male wartosci (moc z many)
}
TEST(signs_axii_trance_out_of_combat) {
	// runda 5: Axii poza walka = niepamiec + trans (czlowiek nie zagada, nie zatrzyma, nie zauwazy bohatera)
	SignTargetInfo guard;
	guard.human = true;
	guard.attitude = ATTITUDE_NEUTRAL;
	CHECK(AxiiTranceApplies(guard));
	SignTargetInfo friendNpc = guard;
	friendNpc.attitude = ATTITUDE_FRIENDLY;
	CHECK(AxiiTranceApplies(friendNpc));        // straz w porcie / przyjaciel tez (zagaduje)
	SignTargetInfo fighting = guard;
	fighting.attitude = ATTITUDE_HOSTILE;
	CHECK(!AxiiTranceApplies(fighting));         // w walce - oszolomienie / marionetka jak dotad
	SignTargetInfo wolf = guard;
	wolf.human = false;
	CHECK(!AxiiTranceApplies(wolf));             // potwory nie zagaduja - zwykle oszolomienie
	SignTargetInfo party = guard;
	party.partyMember = true;
	CHECK(!AxiiTranceApplies(party));
	CHECK(StunName(STUN_TRANCE) == std::string("TRANCE"));
	bool ok = true;
	CHECK(ParseStunAnis("PRAY,TRANCE", &ok) == StunBit(STUN_PRAY) && !ok);   // trans nie jest wariantem walki z ini
	// koniec transu: plugin ustawia czas stanu za bezpiecznikiem skryptu (600 s), ponizej progu trafienia (2000 s)
	CHECK(TranceStateEndMs() > 600.0f * 1000.0f && TranceStateEndMs() < 2000.0f * 1000.0f);
}
TEST(signs_aim_point_clamped_to_range) {
	float x = 0, z = 0;
	AimPointInRange(0, 0, 300, 400, 1000, x, z);   // 500 cm - w zasiegu, bez zmian
	CHECK(x == 300.0f && z == 400.0f);
	AimPointInRange(0, 0, 600, 800, 500, x, z);    // 1000 cm -> skrocone do 500 w tym samym kierunku
	CHECK(fabsf(x - 300.0f) < 0.01f && fabsf(z - 400.0f) < 0.01f);
	AimPointInRange(100, 100, 100, 100, 500, x, z);
	CHECK(x == 100.0f && z == 100.0f);
}
TEST(signs_aard_fly_light_vs_heavy) {
	// lot silnika wymaga obrazen lotu > PROT_FLY: Aard (15) liczy lekkich po ochronie i wysokosci modelu
	CHECK(AardShouldFly(0, 180.0f, 160, 230.0f));          // czlowiek bez ochrony przed lotem
	CHECK(AardShouldFly(150, 200.0f, 160, 230.0f));        // ork wojownik (PROT_FLY 150)
	CHECK(AardShouldFly(160, 210.0f, 160, 230.0f));        // ork elitarny - granica wlacznie
	CHECK(!AardShouldFly(170, 200.0f, 160, 230.0f));       // za mocny
	CHECK(!AardShouldFly(SIGN_PROT_IMMUNE, 150.0f, 160, 230.0f));   // troll / golem kamienny: IMMUNE
	CHECK(!AardShouldFly(100, 400.0f, 160, 230.0f));       // za duzy (smok, troll)
	CHECK(!AardShouldFly(0, 180.0f, -1, 230.0f));          // AardFlyMaxProt=-1: nikt nie leci, tylko potkniecie
}

// ---------- Znaki: runda 2 (lot Aard, powody stozka, widocznosc, warianty oszolomienia, Quen) ----------
TEST(signs_cone_reports_reject_reason) {
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 0, 500, 600, 50, 250).reason == CONE_OK);
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 0, 700, 600, 50, 250).reason == CONE_RANGE);
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 300, 300, 600, 50, 250).reason == CONE_HEIGHT);
	CHECK(ConeTest(0, 0, 0, 0, 1, 400, 0, 300, 600, 50, 250).reason == CONE_ANGLE);
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 0, -50, 600, 50, 250, 60).reason == CONE_BEHIND);   // tuz za plecami
	CHECK(std::string(ConeReasonName(CONE_ANGLE)) == "kat");
}
TEST(signs_los_points_cover_low_and_tall_targets) {
	// wilk: vob przy ziemi, bbox 0..80 cm nad ziemia (1000) - punkty na polowie i 3/4 bboxa, nie tylko w poz. voba
	std::vector<float> w = LosTargetHeights(1010.0f, 1000.0f, 1080.0f);
	CHECK(w.size() == 3);
	CHECK(std::fabs(w[0] - 1040.0f) < 0.01f && std::fabs(w[1] - 1060.0f) < 0.01f);
	CHECK(std::fabs(w[2] - 1030.0f) < 0.01f);       // pozycja voba + 20, ale nie wyzej niz gora bboxa
	// czlowiek: srodek modelu 90 cm nad ziemia
	std::vector<float> h = LosTargetHeights(1090.0f, 1000.0f, 1180.0f);
	CHECK(std::fabs(h[0] - 1090.0f) < 0.01f && std::fabs(h[1] - 1135.0f) < 0.01f);
	// bez bboxa (zly / pusty): sama pozycja voba
	std::vector<float> e = LosTargetHeights(500.0f, 0.0f, 0.0f);
	CHECK(e.size() == 1 && e[0] == 500.0f);
	// skad patrzy bohater: piersi (3/4 wysokosci bboxa)
	CHECK(std::fabs(LosEyeHeight(1000.0f, 1180.0f) - 1135.0f) < 0.01f);
}
TEST(signs_aard_fly_verdict) {
	// lot sprawdzany po AardFlyCheck (0,25 s): przesuniecie wzdluz pchniecia albo NPC w powietrzu
	CHECK(AardFlyVerdict(0.10f, 10.0f, false, 0.25f, 50.0f) == FLY_WAIT);
	CHECK(AardFlyVerdict(0.10f, 80.0f, true, 0.25f, 50.0f) == FLY_OK);       // juz poleciał
	CHECK(AardFlyVerdict(0.26f, 10.0f, false, 0.25f, 50.0f) == FLY_FAILED);  // stoi - silnik nie wykonal lotu
	CHECK(AardFlyVerdict(0.26f, 10.0f, true, 0.25f, 50.0f) == FLY_OK);       // w locie (fizyka / S_FALL), wolno
	CHECK(AardFlyVerdict(0.30f, 60.0f, false, 0.25f, 50.0f) == FLY_OK);
	CHECK(AardFlyVerdict(0.30f, -90.0f, false, 0.25f, 50.0f) == FLY_FAILED); // ruch w strone bohatera (atak) to nie lot
}
TEST(signs_stun_variants_parse_and_pick_without_repeat) {
	bool ok = false;
	uint32_t m = ParseStunAnis("PRAY,FEAR1,FEAR2,FEAR3", &ok);
	CHECK(ok && m == (StunBit(STUN_PRAY) | StunBit(STUN_FEAR1) | StunBit(STUN_FEAR2) | StunBit(STUN_FEAR3)));
	CHECK(ParseStunAnis("pray, wounded ,woundedb", &ok) == (StunBit(STUN_PRAY) | StunBit(STUN_WOUNDED) | StunBit(STUN_WOUNDEDB)) && ok);
	CHECK(ParseStunAnis("PRAY,DANCE", &ok) == StunBit(STUN_PRAY) && !ok);   // nieznana nazwa - pominieta, blad zgloszony
	CHECK(ParseStunAnis("", &ok) == 0);
	CHECK(std::string(StunName(STUN_FEAR2)) == "FEAR2");
	// losowanie bez powtorki pod rzad
	uint32_t all = StunBit(STUN_PRAY) | StunBit(STUN_FEAR1) | StunBit(STUN_FEAR2);
	for (uint32_t r = 0; r < 50; r++) {
		int v = PickStunVariant(all, STUN_FEAR1, r * 2654435761u);
		CHECK(v == STUN_PRAY || v == STUN_FEAR2);
	}
	CHECK(PickStunVariant(StunBit(STUN_PRAY), STUN_PRAY, 7) == STUN_PRAY);   // jedyny wariant - powtorka dozwolona
	CHECK(PickStunVariant(0, STUN_PRAY, 7) == STUN_NONE);
	// kazdy dozwolony wariant wypada
	bool seen[STUN_COUNT] = {};
	for (uint32_t r = 0; r < 200; r++)
		seen[PickStunVariant(all, STUN_NONE, r * 2246822519u + 12345u)] = true;
	CHECK(seen[STUN_PRAY] && seen[STUN_FEAR1] && seen[STUN_FEAR2] && !seen[STUN_FEAR3]);
}
TEST(signs_stun_state_end_markers) {
	// stan skryptu konczy sie, gdy czas stanu >= StunSec (koniec z animacja wstawania) albo >= 2000 s (trafienie)
	CHECK(StunStateEndMs(4.0f) == 4001.0f);       // plugin ustawia czas stanu tuz za progiem petli
	CHECK(StunStateEndMs(1.5f) == 2001.0f);       // prog w skrypcie w pelnych sekundach (ceil)
	CHECK(StunScriptSeconds(4.0f) == 4 && StunScriptSeconds(1.5f) == 2 && StunScriptSeconds(0.2f) == 1);
}
TEST(signs_quen_absorbs_hits_and_falls_not_ticks) {
	CHECK(QuenShouldAbsorb(true, false, true, false));     // cios / strzala / czar z napastnikiem
	CHECK(QuenShouldAbsorb(true, false, false, true));     // upadek (DAM_FALL, bez napastnika)
	CHECK(!QuenShouldAbsorb(true, true, true, false));     // takt palenia
	CHECK(!QuenShouldAbsorb(true, true, false, true));
	CHECK(!QuenShouldAbsorb(true, false, false, false));   // bez napastnika i nie upadek (skrypt, pulapka bez zrodla)
	CHECK(!QuenShouldAbsorb(false, false, true, false));   // bez tarczy
}
TEST(signs_quen_fx_guard) {
	// tarcza nieaktywna, efekt zostal (np. po rzucie innego znaku) - zatrzymac
	CHECK(QuenFxGuard(false, true, false, 0.0f, 150.0f) == QFX_STOP);
	CHECK(QuenFxGuard(false, false, false, 0.0f, 150.0f) == QFX_NONE);
	// aktywna: brak efektu - stworzyc, efekt skonczony albo oderwany od bohatera - zatrzymac i stworzyc od nowa
	CHECK(QuenFxGuard(true, false, false, 0.0f, 150.0f) == QFX_CREATE);
	CHECK(QuenFxGuard(true, true, true, 0.0f, 150.0f) == QFX_RESPAWN);
	CHECK(QuenFxGuard(true, true, false, 400.0f, 150.0f) == QFX_RESPAWN);
	CHECK(QuenFxGuard(true, true, false, 40.0f, 150.0f) == QFX_NONE);
}

// ---------- Loot (podnoszenie z dobyta bronia) ----------
#include "Core/Loot.h"

TEST(loot_hero_deny_follows_check_order) {
	LootHeroState s;
	CHECK(LootHeroDeny(s) == LOOT_OK);
	LootHeroState a = s; a.enabled = false;
	CHECK(LootHeroDeny(a) == LOOT_OFF);
	a = s; a.human = false;
	CHECK(LootHeroDeny(a) == LOOT_NOT_HUMAN);
	a = s; a.melee = false;
	CHECK(LootHeroDeny(a) == LOOT_NOT_MELEE);
	a = s; a.window = true;
	CHECK(LootHeroDeny(a) == LOOT_WINDOW);
	a = s; a.movLock = true;
	CHECK(LootHeroDeny(a) == LOOT_MOVLOCK);
	a = s; a.pluginMove = true;
	CHECK(LootHeroDeny(a) == LOOT_PLUGIN_MOVE);
	a = s; a.attacking = true;
	CHECK(LootHeroDeny(a) == LOOT_ATTACK);
	a = s; a.busy = true;
	CHECK(LootHeroDeny(a) == LOOT_BUSY);
	a = s; a.bodyOk = false;
	CHECK(LootHeroDeny(a) == LOOT_BODY);
	a = s; a.water = true;
	CHECK(LootHeroDeny(a) == LOOT_WATER);
	// kilka naraz - pierwszy wg kolejnosci (bez broni wygrywa z atakiem i oknem)
	a = s; a.melee = false; a.attacking = true; a.window = true;
	CHECK(LootHeroDeny(a) == LOOT_NOT_MELEE);
	a = s; a.attacking = true; a.bodyOk = false;
	CHECK(LootHeroDeny(a) == LOOT_ATTACK);
}
TEST(loot_deny_texts_and_silent_set) {
	for (int i = 0; i < LOOT_DENY_COUNT; i++)
		CHECK(LootDenyText((LootDeny)i) != nullptr && LootDenyText((LootDeny)i)[0] != '\0');
	CHECK(LootDenySilent(LOOT_OFF) && LootDenySilent(LOOT_NOT_HUMAN) && LootDenySilent(LOOT_NOT_MELEE));
	CHECK(!LootDenySilent(LOOT_ENEMY) && !LootDenySilent(LOOT_NO_ITEM) && !LootDenySilent(LOOT_ATTACK));
}
TEST(loot_decide_hero_then_enemy_then_item) {
	CHECK(LootDecide(LOOT_OK, false, true) == LOOT_OK);
	CHECK(LootDecide(LOOT_OK, true, true) == LOOT_ENEMY);
	CHECK(LootDecide(LOOT_OK, false, false) == LOOT_NO_ITEM);
	CHECK(LootDecide(LOOT_OK, true, false) == LOOT_ENEMY);
	CHECK(LootDecide(LOOT_ATTACK, false, true) == LOOT_ATTACK);
	CHECK(LootDecide(LOOT_BODY, true, false) == LOOT_BODY);
}
TEST(loot_log_deny_only_with_item_or_outside_combat) {
	CHECK(LootLogDeny(LOOT_ENEMY, true, true));          // przedmiot byl, wrog obok - wpis (dlaczego cios)
	CHECK(LootLogDeny(LOOT_ATTACK, true, false));
	CHECK(LootLogDeny(LOOT_NO_ITEM, false, false));      // poza walka, nic przed bohaterem
	CHECK(!LootLogDeny(LOOT_NO_ITEM, false, true));      // zwykly cios w walce - bez spamu
	CHECK(!LootLogDeny(LOOT_ENEMY, false, true));
	CHECK(!LootLogDeny(LOOT_ATTACK, false, false));      // kombinacja w powietrze bez przedmiotu
	CHECK(!LootLogDeny(LOOT_NOT_MELEE, true, false));    // bez broni - nie nasza sprawa
	CHECK(!LootLogDeny(LOOT_OK, true, false));
}
TEST(loot_foe_blocks_only_fighting_enemies) {
	LootFoe f;
	f.dist = 500.0f;
	CHECK(!LootFoeBlocks(f, 1000.0f));                   // neutralny, spokojny
	f.targetsHero = true; f.hostile = true;
	CHECK(LootFoeBlocks(f, 1000.0f));                    // wrogi, celuje w bohatera
	CHECK(!LootFoeBlocks(f, 400.0f));                    // poza promieniem
	f.dist = 1000.0f;
	CHECK(LootFoeBlocks(f, 1000.0f));                    // na granicy - liczy sie
	f.dist = 500.0f;
	LootFoe d = f; d.alive = false;
	CHECK(!LootFoeBlocks(d, 1000.0f));
	d = f; d.conscious = false;
	CHECK(!LootFoeBlocks(d, 1000.0f));
	// stary enemy == bohater u przyjaznego bez broni - nie walczy
	LootFoe s; s.dist = 300.0f; s.targetsHero = true;
	CHECK(!LootFoeBlocks(s, 1000.0f));
	s.fighting = true;                                   // ten sam z bronia w rece (arena, bojka) - walczy
	CHECK(LootFoeBlocks(s, 1000.0f));
	// wrogi spokojny (nie zauwazyl bohatera) - nie; wrogi w walce z kims innym - tak; cel bohatera - tak
	LootFoe h; h.dist = 300.0f; h.hostile = true;
	CHECK(!LootFoeBlocks(h, 1000.0f));
	h.fighting = true;
	CHECK(LootFoeBlocks(h, 1000.0f));
	h.fighting = false; h.heroTargets = true;
	CHECK(LootFoeBlocks(h, 1000.0f));
	// neutralny walczacy z kims innym - nie
	LootFoe n; n.dist = 300.0f; n.fighting = true;
	CHECK(!LootFoeBlocks(n, 1000.0f));
	// promien 0 = wylaczone sprawdzanie
	CHECK(!LootFoeBlocks(f, 0.0f));
}
TEST(loot_item_fits_range_cone_height) {
	LootParams p;
	float s = 0.0f;
	CHECK(LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, 100.0f, s));      // prosto przed, na ziemi
	CHECK(!LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, 151.0f, s));     // za daleko
	CHECK(LootItemFits(p, 0.0f, 2.0f, 0.0f, -90.0f, 149.0f, s));      // przod nieznormalizowany
	// stozek 45 st.: 40 st. tak, 50 st. nie
	float r = 100.0f, a40 = 40.0f * 3.14159265f / 180.0f, a50 = 50.0f * 3.14159265f / 180.0f;
	CHECK(LootItemFits(p, 0.0f, 1.0f, r * std::sin(a40), -90.0f, r * std::cos(a40), s));
	CHECK(!LootItemFits(p, 0.0f, 1.0f, -r * std::sin(a50), -90.0f, r * std::cos(a50), s));
	// za plecami, ale pod nogami - tak; za plecami dalej - nie
	CHECK(LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, -30.0f, s));
	CHECK(!LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, -60.0f, s));
	// wysokosc: polka nad glowa nie, dziura pod nogami nie, stol tak
	CHECK(!LootItemFits(p, 0.0f, 1.0f, 0.0f, 100.0f, 80.0f, s));
	CHECK(!LootItemFits(p, 0.0f, 1.0f, 0.0f, -200.0f, 80.0f, s));
	CHECK(LootItemFits(p, 0.0f, 1.0f, 0.0f, -10.0f, 80.0f, s));
	// przod w bok (+X): przedmiot w +X pasuje, w +Z (90 st.) nie
	CHECK(LootItemFits(p, 1.0f, 0.0f, 100.0f, -90.0f, 0.0f, s));
	CHECK(!LootItemFits(p, 1.0f, 0.0f, 0.0f, -90.0f, 100.0f, s));
	// bez kierunku przodu - tylko pod nogami
	CHECK(!LootItemFits(p, 0.0f, 0.0f, 0.0f, -90.0f, 100.0f, s));
	CHECK(LootItemFits(p, 0.0f, 0.0f, 0.0f, -90.0f, 20.0f, s));
}
TEST(loot_item_score_prefers_near_and_centered) {
	LootParams p;
	float near = 0.0f, far = 0.0f, side = 0.0f;
	CHECK(LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, 60.0f, near));
	CHECK(LootItemFits(p, 0.0f, 1.0f, 0.0f, -90.0f, 120.0f, far));
	CHECK(near < far);
	// ta sama odleglosc, 40 st. w bok - gorszy niz na osi
	float a = 40.0f * 3.14159265f / 180.0f;
	CHECK(LootItemFits(p, 0.0f, 1.0f, 60.0f * std::sin(a), -90.0f, 60.0f * std::cos(a), side));
	CHECK(side > near);
}
TEST(loot_rank_items_filters_and_orders) {
	LootParams p;
	std::vector<LootItem> v(5);
	v[0].dx = 0.0f; v[0].dy = -90.0f; v[0].dz = 120.0f;                  // dalszy
	v[1].dx = 0.0f; v[1].dy = -90.0f; v[1].dz = 50.0f;                   // najblizszy
	v[2].dx = 0.0f; v[2].dy = -90.0f; v[2].dz = 40.0f; v[2].takeable = false;   // blizej, ale nie do wziecia
	v[3].dx = 0.0f; v[3].dy = -90.0f; v[3].dz = -120.0f;                 // za plecami
	v[4].dx = 0.0f; v[4].dy = -90.0f; v[4].dz = 90.0f;
	std::vector<int> r = LootRankItems(p, 0.0f, 1.0f, v);
	CHECK(r.size() == 3);
	CHECK(r.size() == 3 && r[0] == 1 && r[1] == 4 && r[2] == 0);
	CHECK(LootRankItems(p, 0.0f, 1.0f, std::vector<LootItem>()).empty());
}
TEST(loot_latch_decides_once_per_press) {
	LootKeyLatch l;
	CHECK(!LootLatchNeedsDecision(l, false));
	CHECK(!LootLatchSwallow(l, false));
	CHECK(LootLatchNeedsDecision(l, true));               // pierwsza obserwacja wcisniecia
	LootLatchSet(l, true);
	CHECK(!LootLatchNeedsDecision(l, true));              // kolejne odczyty w tej i nastepnych klatkach
	CHECK(LootLatchSwallow(l, true));                     // nasze - silnik nie widzi klawisza
	CHECK(!LootLatchSwallow(l, false));
	CHECK(!LootLatchNeedsDecision(l, false));             // puszczenie zeruje
	CHECK(l.state == 0);
	CHECK(LootLatchNeedsDecision(l, true));               // nowe wcisniecie - nowa decyzja
	LootLatchSet(l, false);
	CHECK(!LootLatchSwallow(l, true));                    // gry - cios jak dotad
	CHECK(!LootLatchNeedsDecision(l, true));              // trzymany klawisz po odmowie nie wraca do decyzji
	CHECK(!LootLatchNeedsDecision(l, false));
	CHECK(LootLatchNeedsDecision(l, true));
}
TEST(loot_menu_index_round_trip) {
	CHECK(LootMenuIndex(false, true) == 0);
	CHECK(LootMenuIndex(false, false) == 0);
	CHECK(LootMenuIndex(true, false) == 1);
	CHECK(LootMenuIndex(true, true) == 2);
	bool armed = true, hint = true;
	LootMenuApply(0, armed, hint);
	CHECK(!armed && hint);                                // wylaczone - podpowiedz zostaje w ini
	LootMenuApply(1, armed, hint);
	CHECK(armed && !hint);
	LootMenuApply(2, armed, hint);
	CHECK(armed && hint);
	LootMenuApply(5, armed, hint);
	CHECK(armed && hint);
	LootMenuApply(-1, armed, hint);
	CHECK(armed && hint);
	for (int i = 0; i < 3; i++) {
		bool a = false, h = false;
		LootMenuApply(i, a, h);
		CHECK(i == 0 || LootMenuIndex(a, h) == i);
	}
}

// ---------- Interakcje z dobyta bronia (drzwi, skrzynie, zwloki) ----------
#include "Core/Interact.h"

static IaMobInfo IaDoor() {
	IaMobInfo m;
	m.door = true;
	return m;
}
static IaMobInfo IaChest() {
	IaMobInfo m;
	m.chest = true;
	return m;
}
TEST(ia_can_open_follows_engine_canopen) {
	IaMobInfo m = IaDoor();
	CHECK(IaMobCanOpen(m));                                // bez zamka
	m.locked = true;
	CHECK(!IaMobCanOpen(m));                               // zamek bez klucza i wytrycha (NEVER_OPEN)
	m.keyNeeded = true;
	CHECK(!IaMobCanOpen(m));
	m.hasKey = true;
	CHECK(IaMobCanOpen(m));                                // klucz w ekwipunku
	IaMobInfo p = IaChest();
	p.locked = true; p.pickLock = true;
	CHECK(!IaMobCanOpen(p));
	p.hasLockpick = true;
	CHECK(!IaMobCanOpen(p));                               // wytrych bez talentu
	p.pickTalent = true;
	CHECK(IaMobCanOpen(p));
	p.pickLock = false;                                    // talent i wytrych, ale zamek nie na wytrych
	CHECK(!IaMobCanOpen(p));
	p.hasKey = true;                                       // klucz bez keyInstance sie nie liczy
	CHECK(!IaMobCanOpen(p));
}
TEST(ia_plan_doors_armed_only_when_simple) {
	IaSettings s;
	IaMobInfo d = IaDoor();
	CHECK(IaPlanMob(d, s) == IAP_ARMED);
	d.open = true;                                         // otwarte - zamkniecie tez z bronia
	CHECK(IaPlanMob(d, s) == IAP_ARMED);
	IaMobInfo c = d; c.condition = true;
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);                 // warunek skryptu - zwykla sciezka bez broni
	c = d; c.stateFunc = true;
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);
	c = d; c.useWithItem = true;
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);
	c = d; c.locked = true; c.keyNeeded = true; c.hasKey = true;
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);                 // zamek z kluczem - zwykle otwarcie gry
	c.hasKey = false;
	CHECK(IaPlanMob(c, s) == IAP_LOCKED_MSG);              // bez klucza - tylko komunikat, bron zostaje
	// LootArmedDoors=1: zamki i skrypty zostaja grze, komunikat o zamku dalej
	s.doors = IA_DOORS_OPEN;
	CHECK(IaPlanMob(d, s) == IAP_ARMED);
	c = d; c.condition = true;
	CHECK(IaPlanMob(c, s) == IAP_SKIP_SCRIPT);
	c = d; c.locked = true; c.keyNeeded = true; c.hasKey = true;
	CHECK(IaPlanMob(c, s) == IAP_SKIP_SCRIPT);
	c.hasKey = false;
	CHECK(IaPlanMob(c, s) == IAP_LOCKED_MSG);
	s.doors = IA_DOORS_OFF;
	CHECK(IaPlanMob(d, s) == IAP_SKIP_OFF);
	CHECK(IaPlanMob(c, s) == IAP_SKIP_OFF);
	// zajete / zniszczone
	s.doors = IA_DOORS_ALL;
	c = d; c.occupied = true;
	CHECK(IaPlanMob(c, s) == IAP_SKIP_BUSY);
	c = d; c.destroyed = true;
	CHECK(IaPlanMob(c, s) == IAP_SKIP_BUSY);
}
TEST(ia_plan_chests_always_sheathe_path) {
	IaSettings s;
	IaMobInfo c = IaChest();
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);                 // okno skrzyni i tak chowa bron (OpenInventory)
	c.locked = true; c.pickLock = true; c.hasLockpick = true; c.pickTalent = true;
	CHECK(IaPlanMob(c, s) == IAP_SHEATHE);                 // wytrych - minigra gry bez broni
	c.hasLockpick = false;
	CHECK(IaPlanMob(c, s) == IAP_LOCKED_MSG);
	s.chests = false;
	CHECK(IaPlanMob(IaChest(), s) == IAP_SKIP_OFF);
	IaMobInfo other;                                       // inny MOB (lawka, kowadlo) - nie nasz
	CHECK(IaPlanMob(other, IaSettings()) == IAP_SKIP_OFF);
	CHECK(IaPlanActs(IAP_ARMED) && IaPlanActs(IAP_SHEATHE) && IaPlanActs(IAP_LOCKED_MSG) && IaPlanActs(IAP_CORPSE));
	CHECK(!IaPlanActs(IAP_SKIP_OFF) && !IaPlanActs(IAP_SKIP_SCRIPT) && !IaPlanActs(IAP_SKIP_BUSY) &&
		!IaPlanActs(IAP_SKIP_EMPTY) && !IaPlanActs(IAP_SKIP_ALIVE) && !IaPlanActs(IAP_NONE));
	for (int i = 0; i < IAP_COUNT; i++)
		CHECK(IaPlanText((IaPlan)i)[0] != '\0');
}
TEST(ia_plan_corpses) {
	IaSettings s;
	IaCorpseInfo c;
	CHECK(IaPlanCorpse(c, s) == IAP_SKIP_ALIVE);
	c.dead = true;
	CHECK(IaPlanCorpse(c, s) == IAP_CORPSE);
	c.dead = false; c.unconscious = true;
	CHECK(IaPlanCorpse(c, s) == IAP_CORPSE);               // nieprzytomny - jak w grze (OpenDeadNpc)
	c.hasLoot = false;
	CHECK(IaPlanCorpse(c, s) == IAP_SKIP_EMPTY);
	c.hasLoot = true; s.corpses = false;
	CHECK(IaPlanCorpse(c, s) == IAP_SKIP_OFF);
}
TEST(ia_mob_desc_mentions_lock_and_scripts) {
	IaMobInfo m = IaDoor();
	m.locked = true; m.keyNeeded = true; m.pickLock = true; m.hasLockpick = true; m.condition = true;
	std::string d = IaMobDesc(m);
	CHECK(d.find("drzwi zamkniete") != std::string::npos);
	CHECK(d.find("klucz: brak") != std::string::npos);
	CHECK(d.find("wytrych: jest") != std::string::npos);
	CHECK(d.find("talent: nie") != std::string::npos);
	CHECK(d.find("warunek skryptu") != std::string::npos);
	CHECK(IaMobDesc(IaChest()).find("bez zamka") != std::string::npos);
}
TEST(ia_box_fits_front_side_height) {
	LootParams p;
	p.nearAny = 0.0f;
	float s = 0.0f, d = 0.0f;
	IaBox b;
	// skrzynia 60 x 40 cm, 80 cm przed bohaterem, od ziemi (-90) do -40
	b.minX = -30.0f; b.maxX = 30.0f; b.minZ = 80.0f; b.maxZ = 120.0f; b.minY = -90.0f; b.maxY = -40.0f;
	CHECK(IaBoxFits(p, 0.0f, 1.0f, b, s, d));
	CHECK(std::fabs(d - 80.0f) < 0.01f);
	CHECK(std::fabs(s - 80.0f) < 0.01f);                   // na osi - bez kary za kat
	CHECK(!IaBoxFits(p, 0.0f, -1.0f, b, s, d));            // za plecami
	CHECK(!IaBoxFits(p, 1.0f, 0.0f, b, s, d));             // z boku (90 st.)
	IaBox far = b; far.minZ = 160.0f; far.maxZ = 200.0f;
	CHECK(!IaBoxFits(p, 0.0f, 1.0f, far, s, d));           // za daleko (150)
	IaBox high = b; high.minY = 120.0f; high.maxY = 200.0f;
	CHECK(!IaBoxFits(p, 0.0f, 1.0f, high, s, d));          // nad glowa (polka)
	// szerokie drzwi przesuniete w bok: najblizszy punkt prosto przed bohaterem
	IaBox door; door.minX = -20.0f; door.maxX = 160.0f; door.minZ = 60.0f; door.maxZ = 70.0f; door.minY = -90.0f; door.maxY = 130.0f;
	CHECK(IaBoxFits(p, 0.0f, 1.0f, door, s, d));
	CHECK(std::fabs(d - 60.0f) < 0.01f);
	// bohater w obrysie: liczy sie kierunek do srodka
	IaBox around; around.minX = -50.0f; around.maxX = 50.0f; around.minZ = -10.0f; around.maxZ = 90.0f; around.minY = -90.0f; around.maxY = 100.0f;
	CHECK(IaBoxFits(p, 0.0f, 1.0f, around, s, d));
	CHECK(d == 0.0f);
	CHECK(!IaBoxFits(p, 0.0f, -1.0f, around, s, d));       // srodek za plecami
	// odwrocone min / max (zle zlozony bbox) - to samo
	IaBox rev = b; std::swap(rev.minX, rev.maxX); std::swap(rev.minZ, rev.maxZ);
	CHECK(IaBoxFits(p, 0.0f, 1.0f, rev, s, d) && std::fabs(d - 80.0f) < 0.01f);
	// bez kierunku przodu - nie
	CHECK(!IaBoxFits(p, 0.0f, 0.0f, b, s, d));
}
TEST(ia_box_score_comparable_with_items) {
	LootParams p;
	p.nearAny = 0.0f;
	float sb = 0.0f, d = 0.0f, si = 0.0f;
	IaBox b; b.minX = -30.0f; b.maxX = 30.0f; b.minZ = 100.0f; b.maxZ = 140.0f; b.minY = -90.0f; b.maxY = -40.0f;
	CHECK(IaBoxFits(p, 0.0f, 1.0f, b, sb, d));
	LootParams pi;
	CHECK(LootItemFits(pi, 0.0f, 1.0f, 0.0f, -90.0f, 60.0f, si));   // przedmiot przed skrzynia
	CHECK(!IaTargetBeatsItem(true, si, true, sb));                    // blizszy przedmiot wygrywa
	CHECK(IaTargetBeatsItem(true, 120.0f, true, sb));                 // przedmiot dalej niz skrzynia
	CHECK(IaTargetBeatsItem(false, 0.0f, true, sb));
	CHECK(!IaTargetBeatsItem(true, 50.0f, false, 0.0f));
	CHECK(!IaTargetBeatsItem(true, 50.0f, true, 50.0f));              // remis - przedmiot
}
TEST(ia_mob_reachable_like_search_free_position) {
	std::vector<IaPos> v;
	CHECK(IaMobReachable(v, IA_MOB_REACH));                // brak listy - silnik zdecyduje
	IaPos a; a.dx = 0.0f; a.dy = 0.0f; a.dz = 160.0f;
	v.push_back(a);
	CHECK(!IaMobReachable(v, IA_MOB_REACH));               // jedyna pozycja za daleko
	IaPos b; b.dx = 90.0f; b.dy = 40.0f; b.dz = 100.0f;    // 3D: ok. 140 cm
	v.push_back(b);
	CHECK(IaMobReachable(v, IA_MOB_REACH));
	v[1].free = false;                                     // zajeta przez innego NPC
	CHECK(!IaMobReachable(v, IA_MOB_REACH));
	IaPos c; c.dx = 100.0f; c.dy = 100.0f; c.dz = 30.0f;   // ok. 144 cm w 3D - liczy sie wysokosc
	v.push_back(c);
	CHECK(IaMobReachable(v, IA_MOB_REACH));
	v[2].dy = 120.0f;                                      // ok. 159 cm
	CHECK(!IaMobReachable(v, IA_MOB_REACH));
}

static IaObs IaObsStand(int wm) {
	IaObs o;
	o.dt = 0.05f;
	o.weaponMode = wm;
	return o;
}
// Kroki symulacji az do akcji innej niz NONE (albo limitu).
static IaAction IaRunUntil(IaRun& r, const IaObs& o, const IaSettings& s, int maxSteps) {
	for (int i = 0; i < maxSteps; i++) {
		IaAction a = IaStep(r, o, s);
		if (a != IAA_NONE)
			return a;
	}
	return IAA_NONE;
}
TEST(ia_run_door_armed_sequence) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(3);                               // 1H, stoi
	CHECK(IaBegin(r, IA_DOOR, IAP_ARMED, o, a));
	CHECK(a == IAA_START_MOB && r.phase == IAPH_MOB && r.weaponBefore == 3);
	CHECK(IaStep(r, o, s) == IAA_NONE);                    // czeka na GetInteractMob
	o.mobActive = o.anyMob = true; o.emEmpty = true; o.standing = false; o.bodyFree = false;
	CHECK(IaStep(r, o, s) == IAA_NONE && r.started);
	for (int i = 0; i < 40; i++)
		CHECK(IaStep(r, o, s) == IAA_NONE);                // otwieranie trwa
	o.mobActive = o.anyMob = false; o.stanceWrong = true; o.bodyFree = true; o.standing = true;
	CHECK(IaStep(r, o, s) == IAA_NONE && r.phase == IAPH_SETTLE);
	CHECK(IaStep(r, o, s) == IAA_FIX_STANCE);              // S_RUN z bronia w rece - postawa broni
	o.stanceWrong = false;
	CHECK(IaRunUntil(r, o, s, 40) == IAA_END);
	CHECK(r.phase == IAPH_IDLE && r.endReason.find("postawa broni przywrocona") != std::string::npos);
	CHECK(!r.sheathedByUs);
}
TEST(ia_run_door_armed_waits_for_stand_and_times_out) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(3);
	o.standing = false;                                    // w biegu
	CHECK(IaBegin(r, IA_DOOR, IAP_ARMED, o, a));
	CHECK(a == IAA_STAND && r.phase == IAPH_STAND);
	CHECK(IaStep(r, o, s) == IAA_NONE);
	o.standing = true;
	CHECK(IaStep(r, o, s) == IAA_START_MOB);               // stanal - start
	// start bez reakcji silnika: po 1,5 s pilnowanie postawy, potem koniec (z bronia w rece - bez dobycia)
	o.mobActive = o.anyMob = false;
	CHECK(IaRunUntil(r, o, s, 100) == IAA_END);
	CHECK(r.phase == IAPH_IDLE && r.endReason.find("nie ruszyla") != std::string::npos);
	// bohater nie staje
	IaRun r2;
	IaObs run = IaObsStand(3);
	run.standing = false;
	CHECK(IaBegin(r2, IA_DOOR, IAP_ARMED, run, a) && a == IAA_STAND);
	CHECK(IaRunUntil(r2, run, s, 100) == IAA_END);
	CHECK(r2.endReason.find("nie stanal") != std::string::npos);
}
TEST(ia_run_chest_sheathe_open_redraw) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(4);                               // 2H
	CHECK(IaBegin(r, IA_CHEST, IAP_SHEATHE, o, a));
	CHECK(a == IAA_REMOVE_WEAPON && r.phase == IAPH_SHEATHE && r.sheathedByUs);
	o.emEmpty = false;                                     // animacja chowania
	for (int i = 0; i < 10; i++)
		CHECK(IaStep(r, o, s) == IAA_NONE);
	o.weaponMode = 0; o.emEmpty = true;
	CHECK(IaStep(r, o, s) == IAA_START_MOB && r.phase == IAPH_MOB);
	o.mobActive = o.anyMob = true; o.standing = false; o.bodyFree = false;
	CHECK(IaStep(r, o, s) == IAA_NONE && r.started);
	o.invOpen = true;                                      // okno skrzyni - bez limitu czasu
	for (int i = 0; i < 2000; i++)
		CHECK(IaStep(r, o, s) == IAA_NONE);
	CHECK(r.phase == IAPH_MOB);
	o.invOpen = false;                                     // zamykanie skrzyni (animacja)
	CHECK(IaStep(r, o, s) == IAA_NONE && r.phase == IAPH_MOB);
	o.mobActive = o.anyMob = false; o.standing = true; o.bodyFree = true;
	CHECK(IaStep(r, o, s) == IAA_NONE && r.phase == IAPH_REDRAW_WAIT);
	CHECK(IaStep(r, o, s) == IAA_DRAW_WEAPON && r.phase == IAPH_REDRAW);
	CHECK(IaStep(r, o, s) == IAA_NONE);
	o.weaponMode = 4;
	CHECK(IaStep(r, o, s) == IAA_END);
	CHECK(r.endReason.find("okno zamkniete") != std::string::npos && r.endReason.find("bron dobyta") != std::string::npos);
}
TEST(ia_run_chest_redraw_rules) {
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(3);
	// LootArmedRedraw=0 - bez dobycia
	IaSettings noRedraw; noRedraw.redraw = false;
	CHECK(IaBegin(r, IA_CHEST, IAP_SHEATHE, o, a) && a == IAA_REMOVE_WEAPON);
	o.weaponMode = 0;
	CHECK(IaStep(r, o, noRedraw) == IAA_START_MOB);
	o.mobActive = o.anyMob = true;
	IaStep(r, o, noRedraw);
	o.mobActive = o.anyMob = false;
	IaStep(r, o, noRedraw);
	CHECK(IaStep(r, o, noRedraw) == IAA_END && r.endReason.find("LootArmedRedraw=0") != std::string::npos);
	// rozmowa w trakcie (wlasciciel) - bez dobycia
	IaSettings s;
	IaRun r2;
	IaObs p = IaObsStand(3);
	CHECK(IaBegin(r2, IA_CHEST, IAP_SHEATHE, p, a));
	p.weaponMode = 0;
	CHECK(IaStep(r2, p, s) == IAA_START_MOB);
	p.mobActive = p.anyMob = true;
	IaStep(r2, p, s);
	p.mobActive = p.anyMob = false; p.dialog = true;       // interakcja przerwana rozmowa
	for (int i = 0; i < 30; i++)
		CHECK(IaStep(r2, p, s) == IAA_NONE);               // czeka na koniec rozmowy
	p.dialog = false;
	CHECK(IaStep(r2, p, s) == IAA_END && r2.endReason.find("rozmowa") != std::string::npos);
	// gracz sam dobyl broni - nic do dobycia
	IaRun r3;
	IaObs q = IaObsStand(3);
	CHECK(IaBegin(r3, IA_CHEST, IAP_SHEATHE, q, a));
	q.weaponMode = 0;
	CHECK(IaStep(r3, q, s) == IAA_START_MOB);
	CHECK(IaActionFailed(r3, IAA_START_MOB, s, "CanInteractWith = 0") == IAA_NONE);   // odmowa silnika - dalej dobycie
	CHECK(r3.phase == IAPH_REDRAW_WAIT);
	q.weaponMode = 3;
	CHECK(IaStep(r3, q, s) == IAA_END && r3.endReason.find("bron w rece") != std::string::npos);
	CHECK(r3.endReason.find("silnik odmowil") != std::string::npos);
}
TEST(ia_run_sheathe_interrupted) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(3);
	CHECK(IaBegin(r, IA_DOOR, IAP_SHEATHE, o, a) && a == IAA_REMOVE_WEAPON);
	// kolejka pusta, bron dalej w rece (np. unik przerwal chowanie) - koniec bez akcji
	CHECK(IaRunUntil(r, o, s, 100) == IAA_END);
	CHECK(r.endReason.find("przerwane") != std::string::npos);
	// bez broni na starcie (piesci schowane miedzy decyzja a startem) - od razu start
	IaRun r2;
	IaObs n = IaObsStand(0);
	CHECK(IaBegin(r2, IA_CHEST, IAP_SHEATHE, n, a) && a == IAA_START_MOB && !r2.sheathedByUs);
	CHECK(!IaBegin(r2, IA_DOOR, IAP_LOCKED_MSG, n, a));   // komunikat o zamku - bez przebiegu
	CHECK(!IaBegin(r2, IA_DOOR, IAP_SKIP_OFF, n, a));
}
TEST(ia_run_corpse_window_and_redraw) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(1);                               // piesci
	CHECK(IaBegin(r, IA_CORPSE, IAP_CORPSE, o, a));
	CHECK(a == IAA_OPEN_CORPSE && r.phase == IAPH_WINDOW);
	o.invOpen = true; o.emEmpty = false;                   // okno + chowanie broni od gry
	CHECK(IaStep(r, o, s) == IAA_NONE && r.started);
	o.weaponMode = 0; o.emEmpty = true;
	for (int i = 0; i < 500; i++)
		CHECK(IaStep(r, o, s) == IAA_NONE);
	o.invOpen = false;
	CHECK(IaStep(r, o, s) == IAA_NONE && r.phase == IAPH_REDRAW_WAIT);
	CHECK(IaStep(r, o, s) == IAA_DRAW_WEAPON);
	o.weaponMode = 1;
	CHECK(IaStep(r, o, s) == IAA_END && r.endReason.find("okno zwlok zamkniete; bron dobyta") != std::string::npos);
	// okno zamkniete, zanim gra skonczyla chowac bron - czeka na pusta kolejke
	IaRun r2;
	IaObs p = IaObsStand(3);
	CHECK(IaBegin(r2, IA_CORPSE, IAP_CORPSE, p, a));
	p.invOpen = true; p.emEmpty = false;
	IaStep(r2, p, s);
	p.invOpen = false;
	CHECK(IaStep(r2, p, s) == IAA_NONE && r2.phase == IAPH_REDRAW_WAIT);
	CHECK(IaStep(r2, p, s) == IAA_NONE);                   // bron jeszcze w rece, kolejka zajeta
	p.weaponMode = 0; p.emEmpty = true;
	CHECK(IaStep(r2, p, s) == IAA_DRAW_WEAPON);
	CHECK(IaActionFailed(r2, IAA_DRAW_WEAPON, s, "brak EM") == IAA_END && r2.phase == IAPH_IDLE);
	// okno sie nie otworzylo (puste / AutoQuickLoot) - bron w rece, bez dobycia
	IaRun r3;
	IaObs q = IaObsStand(3);
	CHECK(IaBegin(r3, IA_CORPSE, IAP_CORPSE, q, a));
	IaAction e = IaRunUntil(r3, q, s, 100);
	CHECK(e == IAA_END && r3.endReason.find("nie otworzylo") != std::string::npos);
	CHECK(r3.endReason.find("bron w rece") != std::string::npos);
}
TEST(ia_run_redraw_waits_for_free_body_and_gives_up) {
	IaSettings s;
	IaRun r;
	IaAction a = IAA_NONE;
	IaObs o = IaObsStand(3);
	CHECK(IaBegin(r, IA_CORPSE, IAP_CORPSE, o, a));
	o.invOpen = true;
	IaStep(r, o, s);
	o.invOpen = false; o.weaponMode = 0; o.bodyFree = false;   // skok / upadek po zamknieciu okna
	IaStep(r, o, s);
	CHECK(r.phase == IAPH_REDRAW_WAIT);
	o.dt = 1.0f;
	for (int i = 0; i < 20; i++)
		CHECK(IaStep(r, o, s) == IAA_NONE);
	CHECK(IaStep(r, o, s) == IAA_END && r.endReason.find("nie doczekano") != std::string::npos);
	// smierc w trakcie
	IaRun r2;
	IaObs d = IaObsStand(3);
	CHECK(IaBegin(r2, IA_DOOR, IAP_ARMED, d, a));
	d.alive = false;
	CHECK(IaStep(r2, d, s) == IAA_END && r2.endReason.find("martwy") != std::string::npos);
	IaRun r3;
	CHECK(IaBegin(r3, IA_DOOR, IAP_ARMED, IaObsStand(3), a));
	IaAbort(r3, "zapis gry");
	CHECK(!IaRunActive(r3) && r3.endReason == "zapis gry");
	CHECK(IaStep(r3, IaObsStand(3), s) == IAA_NONE);
}
TEST(ia_texts_and_clamp) {
	for (int i = 0; i < IA_KIND_COUNT; i++)
		CHECK(IaKindText((IaKind)i)[0] != '\0');
	for (int i = 0; i < IAPH_COUNT; i++)
		CHECK(IaPhaseText((IaPhase)i)[0] != '\0');
	for (int i = 0; i < IAA_COUNT; i++)
		CHECK(IaActionText((IaAction)i)[0] != '\0');
	CHECK(IaClampDoors(-1) == 0 && IaClampDoors(0) == 0 && IaClampDoors(2) == 2 && IaClampDoors(7) == 2);
}

// ---------- Znaki: runda 3 (moc z maks. many, tryby alternatywne przez przytrzymanie) ----------
#include "Core/Signs.h"
TEST(signs_power_from_max_mana) {
	CHECK(std::fabs(SignPower(0, 0.001f, 1.0f) - 1.0f) < 1e-6f);
	CHECK(std::fabs(SignPower(100, 0.001f, 1.0f) - 1.1f) < 1e-5f);       // +1% za 10 pkt
	CHECK(std::fabs(SignPower(500, 0.001f, 1.0f) - 1.5f) < 1e-5f);
	CHECK(std::fabs(SignPower(3000, 0.001f, 1.0f) - 2.0f) < 1e-5f);      // limit +100%
	CHECK(std::fabs(SignPower(-50, 0.001f, 1.0f) - 1.0f) < 1e-6f);       // ujemna mana (bledny stan) = bez premii
	CHECK(std::fabs(SignPower(800, 0.0f, 1.0f) - 1.0f) < 1e-6f);         // skalowanie wylaczone
}
TEST(signs_power_quen_hits_and_yrden_levels) {
	CHECK(QuenHitsAt(1.0f) == 1 && QuenHitsAt(1.49f) == 1);
	CHECK(QuenHitsAt(1.5f) == 2 && QuenHitsAt(1.99f) == 2);
	CHECK(QuenHitsAt(2.0f) == 3 && QuenHitsAt(5.0f) == 3);
	CHECK(std::fabs(YrdenSlowAt(1.0f, 0.65f, 0.40f) - 0.65f) < 1e-5f);
	CHECK(std::fabs(YrdenSlowAt(1.5f, 0.65f, 0.40f) - 0.525f) < 1e-5f);
	CHECK(std::fabs(YrdenSlowAt(2.0f, 0.65f, 0.40f) - 0.40f) < 1e-5f);
	CHECK(std::fabs(YrdenSlowAt(3.0f, 0.65f, 0.40f) - 0.40f) < 1e-5f);  // za limitem bez zmian
	CHECK(std::fabs(YrdenArmorAt(1.0f, 30.0f, 50.0f) - 30.0f) < 1e-5f);
	CHECK(std::fabs(YrdenArmorAt(1.5f, 30.0f, 50.0f) - 40.0f) < 1e-5f);
	CHECK(std::fabs(YrdenArmorAt(2.0f, 30.0f, 50.0f) - 50.0f) < 1e-5f);
}
TEST(signs_quen_multi_hit_shield) {
	QuenState q;
	q.Cast(30.0f, 2);
	CHECK(q.active && q.hitsLeft == 2);
	CHECK(q.Absorb());                  // pierwsze - tarcza trwa
	CHECK(q.active && q.hitsLeft == 1 && !q.TakeBreak());
	CHECK(q.Absorb());                  // drugie - peka
	CHECK(!q.active && q.TakeBreak());
	q.Cast(30.0f, 3);
	q.Absorb();
	q.Cast(30.0f, 3);                   // odnowienie wraca pelna liczbe trafien
	CHECK(q.hitsLeft == 3);
	q.Cast(30.0f, 0);                   // co najmniej jedno trafienie
	CHECK(q.hitsLeft == 1);
}
TEST(signs_hold_tap_cast_normal_on_release) {
	SignHold h;
	CHECK(h.Begin(0.35f) == HOLD_WAIT);
	CHECK(h.Tick(0.1f, 0.35f, false) == HOLD_WAIT);
	CHECK(h.Release() == HOLD_CAST_NORMAL);       // krotkie wcisniecie = tryb zwykly przy puszczeniu
	CHECK(h.phase == HOLD_NONE);
}
TEST(signs_hold_one_shot_alt_fires_at_threshold) {
	SignHold h;
	h.Begin(0.35f);
	CHECK(h.Tick(0.2f, 0.35f, false) == HOLD_WAIT);
	CHECK(h.Tick(0.2f, 0.35f, false) == HOLD_CAST_ALT);   // 0,4 s >= 0,35 s
	CHECK(h.phase == HOLD_FIRED);
	CHECK(h.Release() == HOLD_STOP);              // juz rzucone - puszczenie nic nie robi
}
TEST(signs_hold_sustain_until_release_or_stop) {
	SignHold h;
	h.Begin(0.35f);
	CHECK(h.Tick(0.4f, 0.35f, true) == HOLD_START_SUSTAIN);
	CHECK(h.Tick(0.1f, 0.35f, true) == HOLD_SUSTAIN);
	CHECK(h.held > 0.49f);
	CHECK(h.Release() == HOLD_STOP);              // puszczenie konczy podtrzymanie
	SignHold k;
	k.Begin(0.35f);
	k.Tick(0.5f, 0.35f, true);
	k.RequestStop();                              // koniec many / zapis
	CHECK(k.Tick(0.1f, 0.35f, true) == HOLD_STOP);
}
TEST(signs_hold_disabled_casts_immediately) {
	SignHold h;
	CHECK(h.Begin(0.0f) == HOLD_CAST_NORMAL);     // SignAltHold=0: jak dotad, rzut natychmiast
	CHECK(h.phase == HOLD_NONE);
}
// ---------- Znaki: celownik zQuickSign od wcisniecia F (pasek ladowania trybu alternatywnego) ----------
TEST(signs_aim_wanted_per_sign_and_mode) {
	CHECK(SignAimsAt(SIGN_AARD, false) && !SignAimsAt(SIGN_AARD, true));     // fala wokol - bez kierunku
	CHECK(SignAimsAt(SIGN_IGNI, false) && SignAimsAt(SIGN_IGNI, true));      // strumien idzie za celownikiem
	CHECK(SignAimsAt(SIGN_AXII, false) && SignAimsAt(SIGN_AXII, true));      // marionetka - cel spod celownika
	CHECK(SignAimsAt(SIGN_YRDEN, false) && SignAimsAt(SIGN_YRDEN, true));    // krag i pulapka w punkt celownika
	CHECK(!SignAimsAt(SIGN_QUEN, false) && !SignAimsAt(SIGN_QUEN, true));    // tarcza na bohaterze (TARGET_COLLECT_CASTER)
	CHECK(!SignAimsAt(-1, false) && !SignAimsAt(SIGN_COUNT, true));
}
TEST(signs_aim_hud_charge_fraction) {
	SignAimHud n = SignHoldHud(HOLD_NONE, 0.0f, 0.5f, SIGN_AARD);
	CHECK(!n.held && !n.showBar && !n.alt && n.frac == 0.0f);
	SignAimHud p = SignHoldHud(HOLD_PENDING, 0.25f, 0.5f, SIGN_AARD);
	CHECK(p.held && p.showBar && !p.alt && p.wantsAim);
	CHECK(std::fabs(p.frac - 0.5f) < 1e-4f);                                  // polowa progu = pol paska
	CHECK(SignHoldHud(HOLD_PENDING, 0.8f, 0.5f, SIGN_AARD).frac == 1.0f);   // nie ponad pelny
	CHECK(SignHoldHud(HOLD_PENDING, -1.0f, 0.5f, SIGN_AARD).frac == 0.0f);
	CHECK(!SignHoldHud(HOLD_PENDING, 0.1f, 0.0f, SIGN_AARD).showBar);       // SignAltHold=0: bez trybow, bez paska
	SignAimHud qp = SignHoldHud(HOLD_PENDING, 0.1f, 0.5f, SIGN_QUEN);
	CHECK(qp.showBar && !qp.wantsAim);                                       // Quen: pasek tak, celownika nie ma
}
TEST(signs_aim_hud_alt_hides_only_directionless) {
	SignAimHud a = SignHoldHud(HOLD_FIRED, 0.5f, 0.5f, SIGN_AARD);
	CHECK(a.held && a.alt && a.frac == 1.0f && !a.wantsAim && !a.showBar);   // fala wokol - celownik i pasek znikaja
	SignAimHud q = SignHoldHud(HOLD_SUSTAINING, 1.2f, 0.5f, SIGN_QUEN);
	CHECK(q.alt && !q.wantsAim && !q.showBar);                               // podtrzymanie Quen - tez
	SignAimHud i = SignHoldHud(HOLD_SUSTAINING, 1.2f, 0.5f, SIGN_IGNI);
	CHECK(i.alt && i.wantsAim && i.showBar && i.frac == 1.0f);               // strumien Igni dalej celuje (pelny pasek)
	CHECK(SignHoldHud(HOLD_FIRED, 0.5f, 0.5f, SIGN_YRDEN).wantsAim);         // pulapka w punkt celownika
	CHECK(SignHoldHud(HOLD_FIRED, 0.5f, 0.5f, SIGN_AXII).showBar);
}
TEST(signs_sustain_mana_drain) {
	SustainMana m;
	CHECK(m.Take(0.25f, 12.0f) == 3);
	CHECK(m.Take(0.05f, 12.0f) == 0);             // 0,6 - czesci przechodza na kolejne klatki
	CHECK(m.Take(0.05f, 12.0f) == 1);             // 1,2 -> 1
	int total = 0;
	SustainMana n;
	for (int i = 0; i < 100; i++)
		total += n.Take(0.01f, 10.0f);
	CHECK(total == 10);                           // 1 s po 10/s
}
TEST(signs_yrden_trap_charges_and_interval) {
	YrdenTrapState t;
	t.Arm(3, 0.8f);
	CHECK(t.CanFire(7, 0.0f));
	t.Fire(7, 0.0f);
	CHECK(!t.CanFire(7, 0.5f));                   // ten sam wrog za szybko
	CHECK(t.CanFire(8, 0.5f));                    // inny wrog od razu
	t.Fire(8, 0.5f);
	CHECK(t.CanFire(7, 0.8f));
	t.Fire(7, 0.8f);
	CHECK(t.charges == 0 && t.Spent());
	CHECK(!t.CanFire(9, 5.0f));                   // bez ladunkow nic
}
TEST(signs_quen_active_heal) {
	CHECK(QuenActiveHealTo(100, 200, 40.0f, 0.5f) == 120);
	CHECK(QuenActiveHealTo(190, 200, 40.0f, 0.5f) == 200);   // nie ponad maksimum
	CHECK(QuenActiveHealTo(100, 200, 0.0f, 0.5f) == 100);
	CHECK(QuenActiveHealTo(100, 200, 3.0f, 0.5f) == 102);    // 1,5 -> 2 (zaokraglenie od polowy w gore)
	CHECK(QuenActiveHealTo(100, 200, 1.0f, 0.5f) == 101);    // 0,5 -> 1
}
TEST(signs_puppet_picks_nearest_enemy_of_hero) {
	std::vector<PuppetCandidate> c = {
		{ 1, 300.0f, true, true }, { 2, 150.0f, false, true }, { 3, 500.0f, true, true }, { 4, 100.0f, true, false } };
	CHECK(PickPuppetTarget(c, 1500.0f) == 1);      // 2 nie wrog bohatera, 4 martwy / powalony
	std::vector<PuppetCandidate> far = { { 5, 1600.0f, true, true } };
	CHECK(PickPuppetTarget(far, 1500.0f) == -1);
	CHECK(PickPuppetTarget({}, 1500.0f) == -1);
}
TEST(signs_cone_full_circle_for_sweep) {
	// fala Aard wokol: polkat 180 st. obejmuje tez cel za plecami
	CHECK(ConeTest(0, 0, 0, 0, 1, 0, 0, -300, 400, 180, 250).inside);
	CHECK(ConeTest(0, 0, 0, 0, 1, 300, 0, 0, 400, 180, 250).inside);
	CHECK(!ConeTest(0, 0, 0, 0, 1, 0, 0, -450, 400, 180, 250).inside);
}
// ---------- Znaki: runda 4 (ruch przy podtrzymaniu, poza, marionetka potworow, szacunek ciosu) ----------
TEST(signs_channel_skips_magic_mode_only_while_held) {
	CHECK(ChannelSkipMagicMode(true, true, true));      // podtrzymanie z runa w rece, klawisz akcji trzymany - ruch
	CHECK(!ChannelSkipMagicMode(true, true, false));    // puszczony - silnik wykrywa puszczenie (SetReleaseStatus)
	CHECK(!ChannelSkipMagicMode(false, true, true));    // bez podtrzymania - zwykla magia (ladowanie, rzut)
	CHECK(!ChannelSkipMagicMode(true, false, true));    // zQuickSign (miecz pod F) - tryb magii nie gra
}
TEST(signs_channel_pose_freeze_and_restart) {
	CHECK(ChannelHoldFrame(17, 10) == 10.0f);
	CHECK(ChannelHoldFrame(8, 10) == 6.0f);             // krotsza animacja - przedostatnia klatka
	CHECK(ChannelHoldFrame(1, 10) == 0.0f);
	CHECK(ChannelHoldFrame(17, -3) == 0.0f);
	CHECK(ChannelPoseStep(false, 0.0f, 10.0f) == CPOSE_RESTART);   // gest zgaszony (trafienie, przerwanie)
	CHECK(ChannelPoseStep(true, 4.0f, 10.0f) == CPOSE_NONE);       // jeszcze dochodzi do pozy
	CHECK(ChannelPoseStep(true, 10.6f, 10.0f) == CPOSE_FREEZE);    // za poza - z powrotem na klatke trzymania
	CHECK(ChannelPoseStep(true, 10.0f, 10.0f) == CPOSE_NONE);
}
// ---------- Znaki: tryb alternatywny w miejscu (23.10, gest "w ziemie" z zQuickSign) ----------
TEST(signs_alt_in_place_plan_from_quicksign_gesture) {
	// Aard / Yrden w trybie alternatywnym przez zQuickSign: 440 ms do dotkniecia ziemi, gest 1120 ms
	AltInPlacePlan p = PlanAltInPlace(true, true, 1, 1, 440, 1120);
	CHECK(p.use && fabsf(p.delay - 0.44f) < 1e-4f && fabsf(p.hold - 1.12f) < 1e-4f);
	// ziemia juz minela (pytanie spoznione) - skutek od razu, blokada do konca gestu
	p = PlanAltInPlace(true, true, 1, 1, 0, 300);
	CHECK(p.use && p.delay == 0.0f && fabsf(p.hold - 0.3f) < 1e-4f);
	// skutek najpozniej z koncem gestu
	p = PlanAltInPlace(true, true, 1, 1, 900, 500);
	CHECK(p.use && fabsf(p.delay - 0.5f) < 1e-4f);
	// jak dotad: tryb zwykly, gest zwykly (alt 0 - Igni / Quen / Axii), brak gestu / starego zQuickSign, opcja wylaczona
	CHECK(!PlanAltInPlace(true, false, 1, 1, 440, 1120).use);
	CHECK(!PlanAltInPlace(true, true, 1, 0, 320, 680).use);
	CHECK(!PlanAltInPlace(true, true, 0, 0, 0, 0).use);
	CHECK(!PlanAltInPlace(true, true, -1, 0, 0, 0).use);
	CHECK(!PlanAltInPlace(false, true, 1, 1, 440, 1120).use);
	CHECK(!PlanAltInPlace(true, true, 1, 1, 440, 0).use);
	p = PlanAltInPlace(true, false, 1, 1, 440, 1120);
	CHECK(p.delay == 0.0f && p.hold == 0.0f);
}
TEST(signs_pending_step_counts_down_delay) {
	float d = 0.0f;
	CHECK(PendingStep(d, 0.016f));          // bez opoznienia - od razu (jak dotad)
	d = 0.05f;
	CHECK(!PendingStep(d, 0.03f));          // 0,02 s zostalo
	CHECK(!PendingStep(d, 0.03f));          // teraz <= 0 - w nastepnej klatce
	CHECK(PendingStep(d, 0.03f));
	d = 0.1f;
	CHECK(!PendingStep(d, -1.0f) && d == 0.1f);   // ujemny czas klatki nie cofa
}
TEST(signs_still_hold_locks_only_free_and_unlocks_only_own) {
	StillHold h;
	CHECK(h.Start(1.0f, false) == STILL_LOCK && h.active && h.owned);
	CHECK(h.Step(0.5f, true, false) == STILL_NONE && h.active);
	CHECK(h.Step(0.6f, true, false) == STILL_UNLOCK && !h.active && !h.owned);
	CHECK(h.Step(0.1f, true, false) == STILL_NONE);                 // po koncu nic
	// blokada juz trzymana (dialog, medytacja) - nie zakladamy i nie zdejmujemy
	StillHold a;
	CHECK(a.Start(1.0f, true) == STILL_NONE && a.active && !a.owned);
	CHECK(a.Step(2.0f, true, false) == STILL_NONE && !a.active);
	// ktos zdjal nasza blokade w trakcie - na koncu jej nie ruszamy (bylby to cudzy stan)
	StillHold b;
	b.Start(1.0f, false);
	CHECK(b.Step(0.2f, false, false) == STILL_NONE && !b.owned);
	CHECK(b.Step(1.0f, true, false) == STILL_NONE && !b.active);
	// finisher przejal blokade (Director zaklada ja bez sprawdzania) - nie zdejmujemy mu jej
	StillHold c;
	c.Start(1.0f, false);
	CHECK(c.Step(0.2f, true, true) == STILL_NONE && !c.owned);
	CHECK(c.Step(1.0f, true, false) == STILL_NONE);
	// wymuszony koniec (zapis, czyszczenie): zdejmuje tylko wlasna
	StillHold d;
	d.Start(1.0f, false);
	CHECK(d.End(true) == STILL_UNLOCK && !d.active);
	CHECK(d.End(true) == STILL_NONE);
	StillHold e;
	e.Start(1.0f, false);
	CHECK(e.End(true, true) == STILL_NONE);   // w trakcie finishera
	StillHold f;
	f.Start(1.0f, false);
	CHECK(f.End(false) == STILL_NONE);        // juz zdjeta
	// nowy gest w trakcie - dluzszy czas wygrywa, blokada jedna
	StillHold g;
	g.Start(0.5f, false);
	CHECK(g.Start(1.2f, true) == STILL_NONE && g.owned && fabsf(g.left - 1.2f) < 1e-5f);
	CHECK(g.Start(0.3f, true) == STILL_NONE && fabsf(g.left - 1.2f) < 1e-5f);
	CHECK(StillHold().Start(0.0f, false) == STILL_NONE);
}
TEST(signs_puppet_orders) {
	CHECK(PuppetDecide(true, true, false) == PUPPET_KEEP);      // walczy z wrogiem bohatera - bez zmian
	CHECK(PuppetDecide(false, true, false) == PUPPET_ATTACK);   // wrog w zasiegu - atak
	CHECK(PuppetDecide(false, true, true) == PUPPET_ATTACK);    // stoi przy bohaterze, pojawil sie wrog
	CHECK(PuppetDecide(false, false, false) == PUPPET_FOLLOW);  // brak wrogow - do bohatera
	CHECK(PuppetDecide(false, false, true) == PUPPET_KEEP);     // juz idzie za bohaterem
	CHECK(PuppetHumanAi(1));                                    // gildie ludzi (< GIL_SEPERATOR_HUM 16) - ZS_Attack
	CHECK(!PuppetHumanAi(16));
	CHECK(!PuppetHumanAi(59));                                  // orkowie i potwory - ZS_MM_Attack
}
TEST(signs_estimate_melee_damage) {
	int dmg[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	CHECK(DamageMainIndex(dmg, 4) == 2);                // DAM_EDGE (bit 4) - indeks 2
	CHECK(DamageMainIndex(dmg, 2) == 1);                // DAM_BLUNT
	CHECK(DamageMainIndex(dmg, 0) == 2);                // bez typu - ostrze
	int fire[8] = { 0, 0, 10, 40, 0, 0, 0, 0 };
	CHECK(DamageMainIndex(fire, 4) == 3);               // przewazajaca czesc tablicy
	CHECK(EstimateMeleeDamage(0, 60, 20, 5) == 40);     // potwor: sila 60, ochrona 20
	CHECK(EstimateMeleeDamage(50, 40, 30, 5) == 60);    // bron 50 + sila 40 - ochrona 30
	CHECK(EstimateMeleeDamage(10, 10, 100, 5) == 5);    // minimum
	CHECK(EstimateMeleeDamage(10, 10, -1, 5) == 0);     // ochrona IMMUNE - bez obrazen
}

// ---------- Profiler: werdykt przycinki (kto winien) ----------
TEST(prof_attribute_needs_real_share) {
	float none[1] = { 0.0f };
	// bez mediany (start, po wczytaniu) cala klatka to nadwyzka - 0,1 ms pluginu to nie przyczyna
	zprof::Cause c = zprof::Attribute(48.4f, -1.0f, 0.1f, none, 0);
	CHECK(c.kind == zprof::CAUSE_OUTSIDE);
	c = zprof::Attribute(48.4f, -1.0f, 30.0f, none, 0);   // bez mediany, ale plugin zajal wiekszosc klatki
	CHECK(c.kind == zprof::CAUSE_PLUGIN && c.who == -1);
	c = zprof::Attribute(48.4f, -1.0f, 15.0f, none, 0);   // ok. 30% klatki - czesciowo
	CHECK(c.kind == zprof::CAUSE_PARTIAL && c.who == -1);
	// z mediana: nadwyzka ponad mediane
	c = zprof::Attribute(30.0f, 10.0f, 12.0f, none, 0);
	CHECK(c.kind == zprof::CAUSE_PLUGIN);
	c = zprof::Attribute(30.0f, 10.0f, 6.0f, none, 0);
	CHECK(c.kind == zprof::CAUSE_PARTIAL);
	c = zprof::Attribute(30.0f, 10.0f, 3.0f, none, 0);
	CHECK(c.kind == zprof::CAUSE_OUTSIDE);
	// ponizej CAUSE_MIN_MS nikt nie jest winny, nawet przy malej nadwyzce
	c = zprof::Attribute(26.0f, 25.0f, 1.5f, none, 0);
	CHECK(c.kind == zprof::CAUSE_OUTSIDE);
	float two[2] = { 1.9f, 1.9f };
	c = zprof::Attribute(50.0f, 10.0f, 1.9f, two, 2);
	CHECK(c.kind == zprof::CAUSE_OUTSIDE);
	// inny plugin z najwiekszym udzialem
	float other[2] = { 0.2f, 25.0f };
	c = zprof::Attribute(50.0f, 10.0f, 0.5f, other, 2);
	CHECK(c.kind == zprof::CAUSE_PLUGIN && c.who == 1);
	float otherPart[1] = { 11.0f };
	c = zprof::Attribute(50.0f, 10.0f, 0.5f, otherPart, 1);
	CHECK(c.kind == zprof::CAUSE_PARTIAL && c.who == 0);
}
// Log 16.10: "przycinka 48.4 ms (kl. 16, mediana -, prog 25.0) | zDismember 0.1 ms | ... - przyczyna: zDismember" -
// mediana z kilku klatek po ~50 ms dawala nadwyzke ~0 i 0,1 ms pluginu wystarczalo na werdykt.
TEST(prof_verdict_first_frames_after_load_not_blamed_on_plugin) {
	auto p = PMake("zTest");
	int a = p->Section("Styles");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 5, 49.5);   // pierwsze klatki po wczytaniu - wszystkie dlugie, okno mediany niepelne
	p->Begin(a, t + PMs(1));
	p->End(t + PMs(1.1));
	t += PMs(48.4);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_SPIKE);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "mediana -, prog 25.0"));
	CHECK(PHas(s, "zTest 0.1 ms"));
	CHECK(PHas(s, "- poza pluginem"));
	CHECK(!PHas(s, "przyczyna"));
	CHECK(!PHas(s, "czesciowo"));
}
TEST(prof_verdict_first_frames_two_plugins_tiny_shares) {
	auto table = std::make_unique<zprof::Table>();
	std::memset(table.get(), 0, sizeof(zprof::Table));
	auto d = PMake("zDismember"), r = PMake("zTimeRewind");
	CHECK(d->Attach(table.get()) && r->Attach(table.get()));
	int sd = d->Section("Styles"), sr = r->Section("Recorder.Snap");
	uint64_t t = 0;
	d->Mark(t, nullptr);
	r->Mark(t, nullptr);
	for (int i = 0; i < 5; i++) {
		t += PMs(49.5);
		d->Mark(t, nullptr);
		r->Mark(t, nullptr);
	}
	d->Begin(sd, t + PMs(1));
	d->End(t + PMs(1.1));
	r->Begin(sr, t + PMs(2));
	r->End(t + PMs(2.1));
	t += PMs(49.7);
	std::vector<std::string> out;
	CHECK(d->Mark(t, &out) == zprof::FRAME_SPIKE);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "- poza pluginami"));
	CHECK(!PHas(s, "przyczyna") && !PHas(s, "czesciowo"));
	out.clear();
	CHECK(r->Mark(t, &out) == zprof::FRAME_SPIKE);
	s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "- poza pluginami"));
}
TEST(prof_verdict_without_median_still_blames_big_share) {
	auto p = PMake("zTest");
	int a = p->Section("Styles.Apply");
	uint64_t t = 0;
	p->Mark(t, nullptr);
	t = PFrames(*p, t, 3, 16.0);
	p->Begin(a, t + PMs(1));
	p->End(t + PMs(31));
	t += PMs(40);
	std::vector<std::string> out;
	CHECK(p->Mark(t, &out) == zprof::FRAME_SPIKE);
	std::string s = out.empty() ? "" : out[0];
	CHECK(PHas(s, "mediana -"));
	CHECK(PHas(s, "przyczyna: zTest"));
}

// ---------- Nakladki bohatera wczytywane zawczasu (PreloadOverlays) ----------
#include <algorithm>
#include "Core/Preload.h"

static bool PIn(const std::vector<std::string>& v, const char* s) {
	return std::find(v.begin(), v.end(), std::string(s)) != v.end();
}
TEST(preload_overlay_key_normalizes) {
	CHECK(OverlayKey("humans_zdf_w3_1h.mds") == "HUMANS_ZDF_W3_1H");
	CHECK(OverlayKey("  HUMANS_ZDF_MV_DODGE_L.MSB ") == "HUMANS_ZDF_MV_DODGE_L");
	CHECK(OverlayKey("HUMANS_ZDF_W3_LOCO") == "HUMANS_ZDF_W3_LOCO");
	CHECK(OverlayKey("").empty());
	CHECK(OverlayKey(" .MDS").empty());
}
TEST(preload_list_collects_hero_overlays_from_all_sources) {
	std::vector<std::string> styles = { "HUMANS_ZDF_NAT_1HST1", "HUMANS_1HST1", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_LOCO_1H",
		"HUMANS_ZDF_W3_SPRINT_1H", "HUMANS_ZDF_W3_LOCO", "HUMANS_ZDF_W3_SPRINT" };
	std::vector<std::string> extra = { "HUMANS_ZDF_SIGNS", "HUMANS_ZDF_SIGNS_STAND" };
	std::vector<MoveDef> moves(2);
	moves[0].id = "DODGE_L";
	moves[0].overlay = "HUMANS_ZDF_MV_DODGE_L";
	moves[0].overlayW3 = "HUMANS_ZDF_MV_DODGE_L_W3";
	moves[1].id = "ROLL_F";
	moves[1].overlay = "HUMANS_ZDF_MV_ROLL_F";   // bez wariantu W3
	std::vector<TravDef> trav(1);
	trav[0].id = "CLIMB_100_LF";
	trav[0].overlay = "HUMANS_ZDF_MV_TRAV_CLIMB_100_LF";
	std::vector<FinisherDef> fins(2);
	fins[0].id = "FIN_2H_SLASH";
	fins[0].overlay = "HUMANS_ZDF_FIN_2H_SLASH";
	fins[0].inplace = "HUMANS_ZDF_FIN_2H_SLASH_IP";
	fins[1].id = "W3_01_1H";
	fins[1].overlay = "HUMANS_ZDF_W3_01";
	fins[1].victimOverlay = "HUMANS_ZDF_W3_01_V";
	auto list = HeroOverlaysToPreload(styles, extra, moves, trav, fins);
	CHECK(list.size() == 7 + 2 + 3 + 1 + 3);
	CHECK(!list.empty() && list.front() == "HUMANS_ZDF_NAT_1HST1");   // kolejnosc zrodel: najpierw style
	CHECK(PIn(list, "HUMANS_ZDF_SIGNS_STAND"));
	CHECK(PIn(list, "HUMANS_ZDF_MV_DODGE_L") && PIn(list, "HUMANS_ZDF_MV_DODGE_L_W3") && PIn(list, "HUMANS_ZDF_MV_ROLL_F"));
	CHECK(PIn(list, "HUMANS_ZDF_MV_TRAV_CLIMB_100_LF"));
	CHECK(PIn(list, "HUMANS_ZDF_FIN_2H_SLASH") && PIn(list, "HUMANS_ZDF_FIN_2H_SLASH_IP") && PIn(list, "HUMANS_ZDF_W3_01"));
	// nakladka ofiary trafia na inny model (moze miec inna baze) - nie bohatera
	CHECK(!PIn(list, "HUMANS_ZDF_W3_01_V"));
}
TEST(preload_list_dedupes_normalizes_and_skips_non_humans) {
	std::vector<std::string> styles = { "HUMANS_ZDF_W3_1H", "humans_zdf_w3_1h.mds", "", "  ", "ORC_ZDF_TEST", "HUMANS_ZDF_W3_2H" };
	std::vector<MoveDef> moves(2);
	moves[0].overlay = "HUMANS_ZDF_MV_DODGE_L";
	moves[0].overlayW3 = "HUMANS_ZDF_W3_2H";        // powtorzenie z innego zrodla
	moves[1].overlay = "HUMANS_ZDF_MV_DODGE_L.MDS";   // ta sama co wyzej
	std::vector<FinisherDef> fins(2);
	fins[0].overlay = "HUMANS_ZDF_W3_01";
	fins[1].overlay = "HUMANS_ZDF_W3_01";            // 1H i 2H wariant tej samej pary
	auto list = HeroOverlaysToPreload(styles, {}, moves, {}, fins);
	CHECK(list.size() == 4);
	CHECK(list.size() == 4 && list[0] == "HUMANS_ZDF_W3_1H" && list[1] == "HUMANS_ZDF_W3_2H" &&
		list[2] == "HUMANS_ZDF_MV_DODGE_L" && list[3] == "HUMANS_ZDF_W3_01");
	CHECK(!PIn(list, "ORC_ZDF_TEST"));   // bohater to HUMANS.MDS - nakladka innego modelu nie pasuje do bazy
	CHECK(HeroOverlaysToPreload({}, {}, {}, {}, {}).empty());
}
TEST(preload_missing_skips_held_names) {
	std::vector<std::string> wanted = { "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_2H", "HUMANS_ZDF_MV_ROLL_F" };
	std::vector<std::string> held = { "humans_zdf_w3_2h.mds", "HUMANS_ZDF_OLD" };
	auto miss = PreloadMissing(wanted, held);
	CHECK(miss.size() == 2 && miss[0] == "HUMANS_ZDF_W3_1H" && miss[1] == "HUMANS_ZDF_MV_ROLL_F");
	CHECK(PreloadMissing(wanted, wanted).empty());   // ponowne wczytanie swiata - nic nowego
	CHECK(PreloadMissing({}, held).empty());
}

// ---------- Tor przeszkod (zdism trav tor) ----------
#include "Core/Course.h"

// bboxy modeli z Meshes.vdf (pozycje wierzcholkow MRM, skan 17.10)
static std::vector<CoursePiece> CourseTestCatalog() {
	auto box = [](const char* v, float w, float y0, float y1, float d) {
		CoursePiece p;
		p.visual = v;
		p.minX = -w / 2;
		p.maxX = w / 2;
		p.minY = y0;
		p.maxY = y1;
		p.minZ = -d / 2;
		p.maxZ = d / 2;
		return p;
	};
	return { box(COURSE_STONE, 100, -15, 15, 100), box(COURSE_CRATE, 120, -50, 50, 100), box(COURSE_WALL, 350, -150, 150, 100),
		box(COURSE_PARAPET, 179, -53.5f, 53.5f, 23), box(COURSE_LOG, 161, -7, 24, 29), box(COURSE_POST, 23, -1, 157, 23),
		box(COURSE_BARREL, 80, -56, 55, 80), box(COURSE_PLATE, 370, -5, 5, 200), box(COURSE_PILLAR, 40, -150, 150, 24),
		box(COURSE_PILE, 307, -105, 105, 287) };
}

static const CourseObstacle* CourseFind(const CoursePlan& p, const std::string& name) {
	for (auto& o : p.obstacles)
		if (o.name == name)
			return &o;
	return nullptr;
}

TEST(course_stack_reaches_step_heights_with_few_pieces) {
	auto cat = CourseTestCatalog();
	std::vector<CoursePiece> pieces = { cat[0], cat[1], cat[2] };   // plyta 30, skrzynia 100, mur 300
	for (float target : { 30.0f, 60.0f, 100.0f, 150.0f, 200.0f, 250.0f, 300.0f, 350.0f }) {
		std::vector<int> st = CourseStack(pieces, target);
		float h = 0.0f;
		for (int i : st)
			h += pieces[i].SizeY();
		CHECK(!st.empty() && std::fabs(h - target) <= 10.0f && st.size() <= 6);
		if (st.empty() || std::fabs(h - target) > 10.0f)
			printf("  cel %.0f: %.0f cm z %d klockow\n", target, h, (int)st.size());
	}
	CHECK(CourseStack(pieces, 300.0f).size() == 1);   // mur w calosci
	CHECK(CourseStack({}, 100.0f).empty());
}

TEST(course_build_places_all_obstacles_in_three_rows) {
	CoursePlan p = CourseBuild(CourseTestCatalog());
	CHECK(p.skipped.empty());
	CHECK(p.obstacles.size() == 18);
	CHECK(p.length > 3000.0f && p.length <= 7000.0f);     // pas ok. 60-70 m
	CHECK(p.maxX - p.minX < 2200.0f);                     // 4 rzedy
	// stopnie: wysokosc z bboxow, obrys szerszy niz bohater, gora gleboka na 6 m (runda 12: po wspieciu w biegu bohater
	// zbiegal z tylu stopnia 2 m i spadal) - blok z przodu i poklad z plyt za nim, warstwy jedna na drugiej
	for (int h : { 30, 60, 100, 150, 200, 250, 300, 350 }) {
		const CourseObstacle* o = CourseFind(p, "stopien " + std::to_string(h));
		CHECK(o != nullptr);
		if (!o)
			continue;
		CHECK(std::fabs(o->height - h) <= 11.0f && o->width >= 200.0f && o->depth >= 600.0f);
		float top = 0.0f;
		for (auto& part : o->parts)
			for (auto& c : CourseTestCatalog())
				if (c.visual == part.visual)
					top = std::fmax(top, part.y + c.maxY);
		CHECK(std::fabs(top - o->height) < 0.5f);
	}
	// odstepy w rzedzie: rozbieg miedzy przeszkodami
	for (size_t i = 0; i < p.obstacles.size(); i++)
		for (size_t j = 0; j < p.obstacles.size(); j++) {
			const CourseObstacle& a = p.obstacles[i];
			const CourseObstacle& b = p.obstacles[j];
			if (i != j && a.x == b.x && b.z > a.z)
				CHECK(b.z - (a.z + a.depth) >= 500.0f);
		}
	for (const char* n : { "cienki murek 107", "plot z zerdzi", "beczka (okragla)", "waski slup", "nawis 35 cm nad podestem",
		"stos skrzyn z portu", "dach 30 st.", "dach 45 st.", "przepasc 250 cm", "chwyt w locie: polka 200 nad platforma" })
		CHECK(CourseFind(p, n) != nullptr);
}

TEST(course_air_grab_test_has_runway_chasm_and_ledge_with_top) {
	// runda 12: dawniej "wysoka polka" (mur 1 m za platforma) - silnik anulowal skok (promien w mur), chwytu nie bylo.
	// Teraz: platforma 100 cm, 8 m rozbiegu, przepasc 250, mur z krawedzia 200 nad platforma i poklad 4 m na gorze
	auto cat = CourseTestCatalog();
	CoursePlan p = CourseBuild(cat);
	const CourseObstacle* o = CourseFind(p, "chwyt w locie: polka 200 nad platforma");
	CHECK(o != nullptr);
	if (!o)
		return;
	auto topAt = [&](float z) {   // najwyzsza gora czesci nad z (od przodu przeszkody), -1 = nic
		float best = -1.0f;
		for (auto& part : o->parts)
			for (auto& c : cat)
				if (c.visual == part.visual && part.pitchDeg == 0.0f && z >= part.z + c.minZ - o->z && z <= part.z + c.maxZ - o->z)
					best = std::fmax(best, part.y + c.maxY);
		return best;
	};
	CHECK(std::fabs(topAt(50.0f) - 101.0f) < 1.0f && std::fabs(topAt(750.0f) - 101.0f) < 1.0f);   // platforma 8 m (24.10: plyty 1 cm nad skrzyniami)
	CHECK(topAt(900.0f) < 0.0f);                                                                    // przepasc
	CHECK(std::fabs(topAt(1100.0f) - 300.0f) < 1.0f && std::fabs(topAt(1400.0f) - 300.0f) < 1.0f);  // mur i poklad
	CHECK(std::fabs(o->height - 300.0f) < 1.0f);
	// przepasc 250: obie platformy gleboko na 6 m
	const CourseObstacle* g = CourseFind(p, "przepasc 250 cm");
	CHECK(g && g->depth >= 600.0f + 250.0f + 600.0f);
}

TEST(course_special_obstacles_have_their_geometry) {
	auto cat = CourseTestCatalog();
	CoursePlan p = CourseBuild(cat);
	const CourseObstacle* pil = CourseFind(p, "waski slup");
	CHECK(pil && pil->width < 64.0f && pil->height > 60.0f);   // promienie boczne +-32 cm obok - odrzucony
	const CourseObstacle* par = CourseFind(p, "cienki murek 107");
	CHECK(par && par->depth <= 50.0f && par->height > 100.0f && par->height < 140.0f);
	// dach: gorna krawedz plyty w krawedzi muru 200 cm, plyta pod katem
	for (float deg : { 30.0f, 45.0f }) {
		const CourseObstacle* r = CourseFind(p, deg < 40.0f ? "dach 30 st." : "dach 45 st.");
		CHECK(r != nullptr);
		if (!r)
			continue;
		const CoursePlace* plate = nullptr;
		for (auto& part : r->parts)
			if (part.visual == COURSE_PLATE)
				plate = &part;
		CHECK(plate && std::fabs(plate->pitchDeg - deg) < 0.1f);
		if (plate) {
			// gorna powierzchnia, przednia krawedz (lokalnie y = maxY, z = minZ) -> uklad toru
			float c = std::cos(deg * 3.14159265f / 180.0f), s = std::sin(deg * 3.14159265f / 180.0f);
			float ey = plate->y + 5.0f * c + (-100.0f) * s, ez = plate->z - 5.0f * s + (-100.0f) * c;
			CHECK(std::fabs(ey - 200.0f) < 1.0f && std::fabs(ez - r->z) < 1.0f);
		}
	}
	// nawis (24.10): podest 100 cm przed murem, mur dolem 35 cm nad gora skrzyn (szczelina pod nawisem)
	const CourseObstacle* ov = CourseFind(p, "nawis 35 cm nad podestem");
	CHECK(ov != nullptr);
	if (ov)
		for (auto& part : ov->parts)
			if (part.visual == COURSE_WALL)
				CHECK(std::fabs(part.z - 50.0f - (ov->z + 100.0f)) < 1.0f && std::fabs(part.y - 150.0f - (100.0f + 35.0f)) < 1.0f);
	// przepasc: dwie platformy z przerwa 250 cm
	const CourseObstacle* gp = CourseFind(p, "przepasc 250 cm");
	CHECK(gp && std::fabs(gp->height - 100.0f) <= 1.0f && gp->parts.size() >= 8);   // 24.10: +1 cm plyt
	// brak modelu: przeszkody z nim pominiete, reszta jest
	std::vector<CoursePiece> noPile;
	for (auto& c : cat)
		if (c.visual != COURSE_PILE && c.visual != COURSE_PLATE)
			noPile.push_back(c);
	CoursePlan q = CourseBuild(noPile);
	CHECK(!CourseFind(q, "stos skrzyn z portu") && !CourseFind(q, "dach 30 st.") && q.skipped.size() == 3);
	CHECK(CourseFind(q, "stopien 100") != nullptr);
	CHECK(CourseBuild({}).obstacles.empty());
	CHECK(CourseDescribe(*CourseFind(p, "stopien 100")).find("stopien 100") != std::string::npos);
}

// ---------- Unik / przewrot: okno wyjscia, bufor, unik w ostatniej chwili, obrot do wroga (17.10) ----------
#include "Core/Evade.h"

TEST(moves_parse_exit_time) {
	std::vector<std::string> err;
	auto defs = ParseMoves("[DODGE_L]\noverlay=A\ntraj=0,0 10,0\niframes=0.03,0.36\ncancel=0.88\nexit=0.48\n"
		"[ROLL_F]\noverlay=B\ntraj=0,0 0,10\ncancel=1\n[BAD_F]\noverlay=C\ntraj=0,0 1,1\nexit=-1\n", err);
	CHECK(defs.size() == 2 && err.size() == 1);
	CHECK(defs.size() == 2 && std::fabs(defs[0].exitTime - 0.48f) < 1e-5f && defs[1].exitTime < 0.0f);
}

TEST(evade_exit_from_uses_ini_or_path_share) {
	MoveDef d;
	d.trajFps = 25.0f;
	for (int k = 0; k <= 20; k++) {   // 0..200 cm w 0,8 s, rowno
		d.traj.push_back(0.0f);
		d.traj.push_back(10.0f * k);
	}
	d.iframeTo = 0.2f;
	d.cancel = 1.0f;
	d.exitTime = 0.48f;
	CHECK(std::fabs(MoveExitFrom(d, 0.24f) - 0.48f) < 1e-5f);
	d.exitTime = 0.9f;   // po poczatku konczenia - najpozniej wtedy
	CHECK(std::fabs(MoveExitFrom(d, 0.24f) - 0.76f) < 1e-5f);
	d.exitTime = -1.0f;  // bez exit= : po 80% drogi (0,64 s), nie przed koncem nietykalnosci
	CHECK(std::fabs(MoveExitFrom(d, 0.24f) - 0.64f) < 1e-3f);
	d.iframeTo = 0.7f;
	CHECK(std::fabs(MoveExitFrom(d, 0.24f) - 0.7f) < 1e-3f);
}

TEST(chain_press_ignored_right_after_move_start) {
	// Alt+A dwa razy szybko = jeden przewrot (17.10): unik / przewrot wcisniety w pierwszych 0,3 s ruchu nie idzie do bufora
	CHECK(!ChainPressAccepted(0.0f, 0.3f));
	CHECK(!ChainPressAccepted(0.15f, 0.3f));
	CHECK(!ChainPressAccepted(0.29f, 0.3f));
	CHECK(ChainPressAccepted(0.3f, 0.3f));
	CHECK(ChainPressAccepted(0.7f, 0.3f));
	CHECK(ChainPressAccepted(0.05f, 0.0f));   // 0 = bez ograniczenia
}
TEST(exit_queue_keeps_last_press_until_window) {
	ExitQueue q;
	CHECK(ExitQueueStep(q, true, 0.02f, 1.0f) == EXIT_NONE);   // pusta
	ExitQueuePush(q, EXIT_ATTACK);
	CHECK(ExitQueueStep(q, false, 0.3f, 1.0f) == EXIT_NONE);
	ExitQueuePush(q, EXIT_DODGE, 2);                             // ostatnie wcisniecie wygrywa
	CHECK(ExitQueueStep(q, false, 0.3f, 1.0f) == EXIT_NONE);
	CHECK(q.dir == 2);
	CHECK(ExitQueueStep(q, true, 0.02f, 1.0f) == EXIT_DODGE);
	CHECK(q.action == EXIT_NONE);                                // zuzyte raz
	CHECK(ExitQueueStep(q, true, 0.02f, 1.0f) == EXIT_NONE);
	ExitQueuePush(q, EXIT_ATTACK);
	CHECK(ExitQueueStep(q, false, 0.6f, 1.0f) == EXIT_NONE);
	CHECK(ExitQueueStep(q, false, 0.6f, 1.0f) == EXIT_NONE);    // przeterminowane (1,2 s > 1 s)
	CHECK(ExitQueueStep(q, true, 0.02f, 1.0f) == EXIT_NONE);
}

TEST(attack_pressed_gothic1_and_gothic2_controls) {
	// Gothic 2: zbocze akcji
	CHECK(AttackPressed(false, true, false, false, false));
	CHECK(!AttackPressed(false, true, true, false, false));
	// Gothic 1: akcja trzymana + zbocze przodu, albo zbocze akcji przy trzymanym przodzie; sama akcja - nie
	CHECK(AttackPressed(true, true, true, true, false));
	CHECK(AttackPressed(true, true, false, true, true));
	CHECK(!AttackPressed(true, true, false, false, false));
	CHECK(!AttackPressed(true, true, true, true, true));
	CHECK(!AttackPressed(true, false, false, true, false));
}

// Wyjscie atakiem z uniku / przewrotu (17.10): kierunek ciosu jak oCAIHuman::FightMelee - Gothic 1: akcja trzymana +
// zbocze przodu / lewo (A, Q) / prawo (D, E), albo zbocze akcji przy trzymanym kierunku; Gothic 2: zbocze akcji (przod),
// GAME_ACTIONLEFT / GAME_ACTIONRIGHT (boki).
static AttackKeys AK(bool action, bool up, bool left, bool right, bool aLeft = false, bool aRight = false) {
	AttackKeys k;
	k.action = action; k.up = up; k.left = left; k.right = right; k.actionLeft = aLeft; k.actionRight = aRight;
	return k;
}
TEST(attack_press_dir_gothic1_sides_like_fight_melee) {
	AttackKeys none;
	// akcja trzymana, zbocze boku = cios w bok (log 17.10: wczesniej tylko przod - bok czekal do konca ruchu)
	CHECK(AttackPressDir(true, AK(true, false, true, false), AK(true, false, false, false)) == EXIT_ATK_LEFT);
	CHECK(AttackPressDir(true, AK(true, false, false, true), AK(true, false, false, false)) == EXIT_ATK_RIGHT);
	// zbocze akcji przy trzymanym boku (A trzymane po przewrocie w lewo) - tez cios w bok
	CHECK(AttackPressDir(true, AK(true, false, true, false), AK(false, false, true, false)) == EXIT_ATK_LEFT);
	CHECK(AttackPressDir(true, AK(true, false, false, true), AK(false, false, false, true)) == EXIT_ATK_RIGHT);
	// przod jak dotad (AttackPressed)
	CHECK(AttackPressDir(true, AK(true, true, false, false), AK(true, false, false, false)) == EXIT_ATK_FORWARD);
	CHECK(AttackPressDir(true, AK(true, true, false, false), AK(false, true, false, false)) == EXIT_ATK_FORWARD);
	// zbocze akcji z przodem i bokiem: przod pierwszy (gra: GAME_UP przed bokami); oba boki - lewo (gra: lewo pierwsze)
	CHECK(AttackPressDir(true, AK(true, true, true, false), AK(false, true, true, false)) == EXIT_ATK_FORWARD);
	CHECK(AttackPressDir(true, AK(true, false, true, true), AK(false, false, true, true)) == EXIT_ATK_LEFT);
	// przod trzymany (bez zbocza), zbocze boku - bok (gra: GetToggled(GAME_UP) puste -> boki)
	CHECK(AttackPressDir(true, AK(true, true, true, false), AK(true, true, false, false)) == EXIT_ATK_LEFT);
	// bez zbocza / bez akcji / sama akcja bez kierunku - nic
	CHECK(AttackPressDir(true, AK(true, false, true, false), AK(true, false, true, false)) == EXIT_ATK_NONE);
	CHECK(AttackPressDir(true, AK(false, false, true, false), none) == EXIT_ATK_NONE);
	CHECK(AttackPressDir(true, AK(true, false, false, false), none) == EXIT_ATK_NONE);
	// GAME_ACTIONLEFT / RIGHT w sterowaniu Gothica 1 nie licza sie (FightMelee ich nie czyta)
	CHECK(AttackPressDir(true, AK(false, false, false, false, true, false), none) == EXIT_ATK_NONE);
	// zgodne z AttackPressed dla przodu
	CHECK((AttackPressDir(true, AK(true, true, false, false), AK(true, true, false, false)) == EXIT_ATK_FORWARD) ==
		AttackPressed(true, true, true, true, true));
}
TEST(attack_press_dir_gothic2_action_keys) {
	AttackKeys none;
	CHECK(AttackPressDir(false, AK(true, false, false, false), none) == EXIT_ATK_FORWARD);
	CHECK(AttackPressDir(false, AK(true, false, false, false), AK(true, false, false, false)) == EXIT_ATK_NONE);
	CHECK(AttackPressDir(false, AK(false, false, false, false, true, false), none) == EXIT_ATK_LEFT);
	CHECK(AttackPressDir(false, AK(false, false, false, false, false, true), none) == EXIT_ATK_RIGHT);
	CHECK(AttackPressDir(false, AK(false, false, false, false, true, false), AK(false, false, false, false, true, false)) == EXIT_ATK_NONE);
	// A / D z akcja w sterowaniu Gothica 2 to obrot, nie cios w bok
	CHECK(AttackPressDir(false, AK(true, false, true, false), AK(true, false, false, false)) == EXIT_ATK_NONE);
}
TEST(exit_attack_keeps_direction_from_press_and_falls_back_to_forward) {
	ExitQueue q;
	ExitQueuePush(q, EXIT_ATTACK, EXIT_ATK_RIGHT);       // wcisniete przed oknem - kierunek z chwili wcisniecia
	CHECK(ExitQueueStep(q, false, 0.2f, 1.0f) == EXIT_NONE);
	CHECK(q.dir == EXIT_ATK_RIGHT);
	CHECK(ExitQueueStep(q, true, 0.02f, 1.0f) == EXIT_ATTACK);
	// animacja ciosu: bok, a bez animacji boku (-1) przod
	int dir = EXIT_ATK_LEFT;
	CHECK(ExitAttackAni(dir, 10, 11, 12) == 11 && dir == EXIT_ATK_LEFT);
	dir = EXIT_ATK_RIGHT;
	CHECK(ExitAttackAni(dir, 10, 11, 12) == 12 && dir == EXIT_ATK_RIGHT);
	dir = EXIT_ATK_RIGHT;
	CHECK(ExitAttackAni(dir, 10, 11, -1) == 10 && dir == EXIT_ATK_FORWARD);
	dir = -1;                                            // stary wpis bufora bez kierunku = przod
	CHECK(ExitAttackAni(dir, 10, 11, 12) == 10 && dir == EXIT_ATK_FORWARD);
	dir = EXIT_ATK_FORWARD;
	CHECK(ExitAttackAni(dir, -1, 11, 12) == -1);         // bez ciosu w przod - brak
}

TEST(perfect_dodge_timing_window) {
	CHECK(EnemyTimeToHit(20.0f, 14.0f, 25.0f) > 0.239f && EnemyTimeToHit(20.0f, 14.0f, 25.0f) < 0.241f);
	CHECK(EnemyTimeToHit(20.0f, 21.0f, 25.0f) < 0.0f);        // po trafieniu
	CHECK(EnemyTimeToHit(20.0f, 14.0f, 0.0f) < 0.0f);
	CHECK(PerfectDodgeTiming(0.24f, 0.35f));
	CHECK(PerfectDodgeTiming(0.0f, 0.35f));
	CHECK(!PerfectDodgeTiming(0.5f, 0.35f));                   // za wczesnie - zwykly unik
	CHECK(!PerfectDodgeTiming(-0.1f, 0.35f));                  // cios juz byl
}

TEST(counter_backstep_parade_detected_by_name) {
	// odskok w tyl gry z dobyta bronia / piesciami (tez z nakladek stylow: zEBA, W3, ruchy 1H/2H)
	CHECK(IsBackstepParade("T_1HPARADEJUMPB", "HUMANS", false));
	CHECK(IsBackstepParade("T_2HPARADEJUMPB", "HUMANS_2HST2", false));
	CHECK(IsBackstepParade("t_fistparadejumpb", "humans", false));
	CHECK(IsBackstepParade("T_1HPARADEJUMPB", "HUMANS_ZDF_W3_1H", false));
	// blok to nie odskok
	CHECK(!IsBackstepParade("T_1HPARADE_0", "HUMANS", false));
	CHECK(!IsBackstepParade("T_2HPARADE_0_A3", "HUMANS_ZDF_W3_2H", false));
	CHECK(!IsBackstepParade("", "", false));
	// ruch W3 (unik / przewrot / kontra z nakladki HUMANS_ZDF_MV_*) albo jego wygasajaca animacja pod blokiem - nie odskok
	CHECK(!IsBackstepParade("T_1HPARADEJUMPB", "HUMANS_ZDF_MV_DODGE_L_W3", false));
	CHECK(!IsBackstepParade("T_2HPARADEJUMPB", "HUMANS_ZDF_MV_TRAV_3", false));
	CHECK(!IsBackstepParade("T_1HPARADEJUMPB", "HUMANS", true));
	// animacja spoza nakladek modelu (nakladka ruchu juz zdjeta, animacja jeszcze gra) - nie odskok
	CHECK(!IsBackstepParade("T_1HPARADEJUMPB", "", false));
}

TEST(counter_window_timing) {
	CHECK(CounterTiming(0.9f, 0.0f));                          // 0 = caly zamach
	CHECK(CounterTiming(-1.0f, 0.0f));                         // caly zamach: bez klatki ciosu tez (wystarczy zamach)
	CHECK(CounterTiming(0.25f, 0.3f));
	CHECK(CounterTiming(0.0f, 0.3f));
	CHECK(CounterTiming(0.3f, 0.3f));
	CHECK(!CounterTiming(0.45f, 0.3f));                        // za wczesnie - zwykla parada
	CHECK(!CounterTiming(-0.1f, 0.3f));                        // po ciosie / brak tempa animacji
	CHECK(CounterTiming(0.69f, 0.7f) && !CounterTiming(0.71f, 0.7f));
}

TEST(lost_track_yaw_limits_turn_rate) {
	const float pi = 3.14159265f;
	float y = LostTrackYaw(0.0f, 1.0f, pi / 2.0f, 0.1f);       // 90 st./s, 0,1 s - najwyzej 9 st.
	CHECK(std::fabs(y - pi / 20.0f) < 1e-4f);
	CHECK(std::fabs(LostTrackYaw(0.0f, 0.05f, pi / 2.0f, 0.1f) - 0.05f) < 1e-6f);   // mniejszy obrot bez zmian
	y = LostTrackYaw(3.0f, -3.0f, pi / 2.0f, 0.1f);            // krotsza droga przez +-pi
	CHECK(y > 3.0f && y < 3.0f + pi / 20.0f + 1e-4f);
}

TEST(end_face_turns_to_target_by_arrival) {
	const float pi = 3.14159265f;
	// przed 'from' bez obrotu
	CHECK(EndFaceYawStep(0.0f, 1.0f, 0.1f, 0.3f, 0.5f, 0.02f, 20.0f) == 0.0f);
	// od 'from' tak, by w 'arrive' patrzyl na cel: symulacja co 0,02 s
	float yaw = 0.0f, target = 2.0f;
	for (float t = 0.3f; t < 0.5f - 1e-4f; t += 0.02f)
		yaw = EndFaceYawStep(yaw, target, t, 0.3f, 0.5f, 0.02f, 20.0f);
	CHECK(std::fabs(AngleDiff(yaw, target)) < 0.02f);
	// limit predkosci: 180 st. w 0,1 s przy 10 rad/s - nie dochodzi, ale obraca sie krotsza droga
	yaw = 0.0f;
	for (float t = 0.4f; t < 0.5f - 1e-4f; t += 0.02f)
		yaw = EndFaceYawStep(yaw, pi - 0.1f, t, 0.3f, 0.5f, 0.02f, 10.0f);
	CHECK(yaw > 0.9f && yaw < 1.01f);
	// po 'arrive' dalej nadaza za celem (najwyzej maxRate)
	CHECK(std::fabs(EndFaceYawStep(0.0f, 0.1f, 0.6f, 0.3f, 0.5f, 0.02f, 10.0f) - 0.1f) < 1e-5f);
	CHECK(std::fabs(EndFaceYawStep(0.0f, 1.0f, 0.6f, 0.3f, 0.5f, 0.02f, 10.0f) - 0.2f) < 1e-5f);
}

TEST(hero_boost_scale_applies_once_and_restores) {
	HeroBoost b;
	HeroBoostArm(b, 1.0f);
	CHECK(b.armed);
	// atak w oknie: skala x1,2 na czas ciosu, potem powrot do 1
	float s = HeroBoostStep(b, true, 0.02f, 1.0f, 1.2f);
	CHECK(std::fabs(s - 1.2f) < 1e-6f && b.active && !b.armed);
	s = HeroBoostStep(b, true, 0.02f, 1.2f, 1.2f);
	CHECK(std::fabs(s - 1.2f) < 1e-6f);
	s = HeroBoostStep(b, false, 0.02f, 1.2f, 1.2f);            // cios skonczony
	CHECK(std::fabs(s - 1.0f) < 1e-6f && !b.active);
	// bez ataku w oknie - wygasa, skala bez zmian
	HeroBoostArm(b, 0.1f);
	CHECK(HeroBoostStep(b, false, 0.2f, 1.0f, 1.2f) == 1.0f && !b.armed);
	CHECK(HeroBoostStep(b, true, 0.02f, 1.0f, 1.2f) == 1.0f && !b.active);
	// inny modul zmienil tempo (Yrden, podtrzymanie znaku) - bez przyspieszenia
	HeroBoostArm(b, 1.0f);
	CHECK(HeroBoostStep(b, true, 0.02f, 0.6f, 1.2f) == 0.6f && !b.active);
	// w trakcie ktos zmienil tempo - nie przywracamy na sile
	HeroBoostArm(b, 1.0f);
	HeroBoostStep(b, true, 0.02f, 1.0f, 1.2f);
	CHECK(HeroBoostStep(b, false, 0.02f, 0.6f, 1.2f) == 0.6f && !b.active);
}

TEST(sprint_armed_target_speed_overrides_game_tempo) {
	// petla z bronia 600 cm/s, sprint gry z bronia 474: z celem 600 tempo wlasne (x1), bez celu jak dotad (x0,79)
	CHECK(std::fabs(SprintTargetScale(0.600f, 0.474f, 0.600f) - 1.0f) < 1e-5f);
	CHECK(std::fabs(SprintTargetScale(0.0f, 0.474f, 0.600f) - 0.79f) < 1e-3f);
	CHECK(std::fabs(SprintTargetScale(0.700f, 0.474f, 0.600f) - 0.7f / 0.6f) < 1e-4f);
	CHECK(SprintTargetScale(5.0f, 0.474f, 0.600f) == 1.0f);   // poza 0,5..2 - tempo wlasne
}

// ---------- Znaki: ognie z efektu (Aard gasi, Igni podpala voby z PFX / decalem ognia) ----------
TEST(signs_fire_visual_kind_world_fires) {
	// ogniska, pochodnie, kominki, lampy ze swiatow G2 (NEWWORLD / OLDWORLD / ADDONWORLD / DRAGONISLAND)
	CHECK(FireVisualKind("FIRE.PFX", "FIRE") == FIREVIS_FLAME);
	CHECK(FireVisualKind("FIRE_MEDIUM.PFX", "") == FIREVIS_FLAME);
	CHECK(FireVisualKind("fire_hot.pfx", "") == FIREVIS_FLAME);          // wielkosc liter bez znaczenia
	CHECK(FireVisualKind("TORCH.PFX", "") == FIREVIS_FLAME);
	CHECK(FireVisualKind("COALGLOW.PFX", "") == FIREVIS_FLAME);          // zar paleniska
	CHECK(FireVisualKind("FIRE_COMPLETE_A0.TGA", "") == FIREVIS_FLAME);  // animowany decal plomienia (lampy, swiece)
	CHECK(FireVisualKind("FIRE_MEDIUM", "") == FIREVIS_FLAME);           // bez rozszerzenia
	// dodatki - tylko w grupie plomienia
	CHECK(FireVisualKind("FIRE_SMOKE.PFX", "SMOKE") == FIREVIS_EXTRA);
	CHECK(FireVisualKind("FIRE_SPARKS.PFX", "") == FIREVIS_EXTRA);
	CHECK(FireVisualKind("TORCH_SMOKE.PFX", "") == FIREVIS_EXTRA);
	CHECK(FireVisualKind("ZFLARE6.TGA", "") == FIREVIS_EXTRA);           // poswiata pochodni
}
TEST(signs_fire_visual_kind_never_spells_or_ambient) {
	CHECK(FireVisualKind("MFX_FIREBALL_INIT.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("MFX_FIRESPELL_HUMANBURN.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("FIRE_SWORD.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("FIRE_ARROW.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("FIRE_BOW.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("FIRE_MODEL.PFX", "") == FIREVIS_EFFECT);       // palaca sie postac
	CHECK(FireVisualKind("DEMON_FLAMETHROWER.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("DRAGON_FIREATTACK.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("LAVAFOG_BIG.PFX", "") == FIREVIS_EFFECT);      // lawy sie nie gasi
	CHECK(FireVisualKind("FIRE_MENU.PFX", "") == FIREVIS_EFFECT);
	CHECK(FireVisualKind("SPELLFX_FIRESWORD.PFX", "FIRE") == FIREVIS_EFFECT);   // nazwa voba nie ratuje czaru
	CHECK(FireVisualKind("GLOWWORMS_SMALL.PFX", "") == FIREVIS_NONE);
	CHECK(FireVisualKind("GROUNDFOG1_OUTDOOR_MID.PFX", "") == FIREVIS_NONE);
	CHECK(FireVisualKind("CAULDRON_BUBBLES.PFX", "") == FIREVIS_NONE);
	CHECK(FireVisualKind("", "") == FIREVIS_NONE);
	// nazwa voba pomocniczo (wizual o innej nazwie w modzie)
	CHECK(FireVisualKind("MYMOD_BLAZE.PFX", "FIRE") == FIREVIS_FLAME);
	CHECK(FireVisualKind("MYMOD_PUFF.PFX", "SMOKE") == FIREVIS_EXTRA);
	CHECK(std::string(FireVisKindName(FIREVIS_FLAME)) == "plomien");
	CHECK(std::string(FireVisKindName(99)) == "?");
}
TEST(signs_fire_sound_names) {
	CHECK(FireSoundName("FIRE_MEDIUM"));
	CHECK(FireSoundName("torch_burn"));
	CHECK(FireSoundName("CAMPFIRE_BURN"));
	CHECK(FireSoundName("CAMPFIRE1"));
	CHECK(FireSoundName("FIRE_SMALL.WAV"));
	CHECK(FireSoundName("FIRE_LARGE"));
	CHECK(!FireSoundName("MFX_FIREBALL_CAST"));
	CHECK(!FireSoundName("BIRDS_01"));
	CHECK(!FireSoundName(""));
}
TEST(signs_fire_flame_check_aard_and_igni) {
	FireFlame f;
	f.kind = FIREVIS_FLAME;
	CHECK(FireFlameCheck(f, false) == FIRESKIP_OK);          // Aard: widoczny plomien - gasi
	CHECK(FireFlameCheck(f, true) == FIRESKIP_ALREADY);      // Igni: juz plonie
	f.shown = false;
	CHECK(FireFlameCheck(f, true) == FIRESKIP_OK);           // Igni: ukryty (zgaszony, takze po wczytaniu) - zapala
	CHECK(FireFlameCheck(f, false) == FIRESKIP_ALREADY);
	f.shown = true;
	FireFlame e = f;
	e.kind = FIREVIS_EXTRA;
	CHECK(FireFlameCheck(e, false) == FIRESKIP_EXTRA);       // sam dym / iskry nie sa celem
	e.kind = FIREVIS_EFFECT;
	CHECK(FireFlameCheck(e, false) == FIRESKIP_EFFECT);
	e.kind = FIREVIS_NONE;
	CHECK(FireFlameCheck(e, false) == FIRESKIP_NOT_FIRE);
	FireFlame r = f;
	r.runtime = true;                                        // vob tymczasowy (efekt czaru, VFX) - nie
	CHECK(FireFlameCheck(r, false) == FIRESKIP_RUNTIME);
	FireFlame o = f;
	o.owned = true;                                          // pochodnia w rece / przedmiot na ziemi
	CHECK(FireFlameCheck(o, false) == FIRESKIP_OWNER);
	FireFlame m = f;
	m.mobTree = true;                                        // drzewo ognia oCMobFire - zalatwia mob
	CHECK(FireFlameCheck(m, false) == FIRESKIP_MOBTREE);
	CHECK(std::string(FireSkipName(FIRESKIP_MOBTREE)) == "drzewo oCMobFire");
	CHECK(std::string(FireSkipName(-1)) == "?");
}
TEST(signs_fire_member_check_tree_and_loose) {
	// pochodnia: dzieci uchwytu (to samo drzewo) - dym, poswiata, swiatlo, dzwiek
	FireMember v;
	v.type = FMEM_VISUAL;
	v.visKind = FIREVIS_EXTRA;
	v.sameTree = true;
	v.dist = 40.0f;
	CHECK(FireMemberCheck(v, 200.0f, false) == FMSKIP_OK);
	v.dist = 350.0f;
	CHECK(FireMemberCheck(v, 200.0f, false) == FMSKIP_OK);       // drzewo: do 2x promienia (duze paleniska)
	v.dist = 450.0f;
	CHECK(FireMemberCheck(v, 200.0f, false) == FMSKIP_FAR);
	FireMember mesh = v;
	mesh.dist = 20.0f;
	mesh.visKind = FIREVIS_NONE;                                 // mesh uchwytu, krzak obok - zostaje
	CHECK(FireMemberCheck(mesh, 200.0f, false) == FMSKIP_NOT_FIRE);
	// luzny (rodzic = kompozycja poziomu): tylko w promieniu
	FireMember l;
	l.type = FMEM_LIGHT;
	l.loose = true;
	l.dist = 150.0f;
	CHECK(FireMemberCheck(l, 200.0f, false) == FMSKIP_OK);
	l.dist = 250.0f;
	CHECK(FireMemberCheck(l, 200.0f, false) == FMSKIP_FAR);
	l.dist = 100.0f;
	l.staticLight = true;                                        // wypalone w lightmapie - nie da sie
	CHECK(FireMemberCheck(l, 200.0f, false) == FMSKIP_STATIC_LIGHT);
	// cudzy uchwyt (sasiednia pochodnia 1,5 m dalej) - nie
	FireMember other;
	other.type = FMEM_SOUND;
	other.fireSound = true;
	other.dist = 150.0f;
	CHECK(FireMemberCheck(other, 200.0f, false) == FMSKIP_OTHER_HOLDER);
	// dzwiek: luzny tylko z nazwa ognia, w drzewie kazdy
	FireMember s;
	s.type = FMEM_SOUND;
	s.loose = true;
	s.dist = 50.0f;
	CHECK(FireMemberCheck(s, 200.0f, false) == FMSKIP_NOT_FIRE);
	s.fireSound = true;
	CHECK(FireMemberCheck(s, 200.0f, false) == FMSKIP_OK);
	FireMember ts = s;
	ts.loose = false;
	ts.sameTree = true;
	ts.fireSound = false;
	CHECK(FireMemberCheck(ts, 200.0f, false) == FMSKIP_OK);
	// stan: Aard bierze tylko wlaczone, Igni (bez pamieci, po wczytaniu) tylko wylaczone
	s.active = false;
	CHECK(FireMemberCheck(s, 200.0f, false) == FMSKIP_STATE);
	CHECK(FireMemberCheck(s, 200.0f, true) == FMSKIP_OK);
	s.active = true;
	CHECK(FireMemberCheck(s, 200.0f, true) == FMSKIP_STATE);
	// postac / przedmiot / tymczasowy - nigdy
	FireMember own = s;
	own.owned = true;
	CHECK(FireMemberCheck(own, 200.0f, false) == FMSKIP_OWNER);
	FireMember rt = s;
	rt.runtime = true;
	CHECK(FireMemberCheck(rt, 200.0f, false) == FMSKIP_OWNER);
	CHECK(std::string(FireMemberSkipName(FMSKIP_STATIC_LIGHT)) == "swiatlo statyczne (lightmapa)");
}
TEST(signs_fire_nearest_indices) {
	std::vector<float> d = { 500.0f, 20.0f, 300.0f, 20.0f, 900.0f, 100.0f, 50.0f };
	std::vector<int> n = NearestIndices(d, 5);
	CHECK(n.size() == 5);
	CHECK(n.size() == 5 && n[0] == 1 && n[1] == 3 && n[2] == 6 && n[3] == 5 && n[4] == 2);   // rowne - kolejnosc wejscia
	CHECK(NearestIndices(d, 0).empty());
	CHECK(NearestIndices({}, 5).empty());
	CHECK(NearestIndices({ 3.0f, 1.0f }, 5).size() == 2);
}

// ---------- Profiler probkujacy (plugins\_shared\ZSampleCore.h) ----------
#include "../../_shared/ZSampleCore.h"

TEST(prof_period_count_counts_periodic_summaries) {
	auto p = PMake("zTest", 25.0f, 1.0f);
	uint64_t t = 0;
	p->Mark(t, nullptr);
	CHECK(p->PeriodCount() == 0);
	t = PFrames(*p, t, 62, 16.0);   // 0,99 s - jeszcze nie
	CHECK(p->PeriodCount() == 0);
	t = PFrames(*p, t, 2, 16.0);
	CHECK(p->PeriodCount() == 1);
	auto q = PMake("zTest", 25.0f, 0.0f);   // bez podsumowan okresowych - zawsze 0
	PFrames(*q, 0, 200, 16.0);
	CHECK(q->PeriodCount() == 0);
}

static zsamp::Sample SMake(uint64_t t, uint32_t eip, std::initializer_list<uint32_t> rets = {}, uint32_t hostRet = 0) {
	zsamp::Sample s;
	s.t = t;
	s.eip = eip;
	for (uint32_t r : rets)
		if (s.nret < zsamp::MAX_RET)
			s.ret[s.nret++] = r;
	s.hostRet = hostRet;
	return s;
}

TEST(samp_ring_get_not_yet_and_lost) {
	static zsamp::Ring<4> r;
	zsamp::Sample out;
	CHECK(r.Written() == 0);
	CHECK(r.Get(0, out) == zsamp::Ring<4>::NOT_YET);
	for (int i = 0; i < 3; i++)
		r.Push(SMake(100 + i, 0x401000 + i));
	CHECK(r.Written() == 3 && r.Oldest() == 0);
	CHECK(r.Get(1, out) == zsamp::Ring<4>::GOT && out.t == 101 && out.eip == 0x401001);
	CHECK(r.Get(3, out) == zsamp::Ring<4>::NOT_YET);
	for (int i = 3; i < 7; i++)
		r.Push(SMake(100 + i, 0x401000 + i));
	CHECK(r.Written() == 7 && r.Oldest() == 3);
	CHECK(r.Get(1, out) == zsamp::Ring<4>::LOST);   // nadpisana
	CHECK(r.Get(2, out) == zsamp::Ring<4>::LOST);
	CHECK(r.Get(3, out) == zsamp::Ring<4>::GOT && out.t == 103);
	CHECK(r.Get(6, out) == zsamp::Ring<4>::GOT && out.t == 106);
}

TEST(samp_is_call_before_recognizes_call_forms) {
	auto chk = [](std::initializer_list<int> tail) {   // bajty tuz przed adresem powrotu (ostatni = ret-1)
		uint8_t b[7] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
		int n = (int)tail.size(), i = 7 - n;
		for (int v : tail)
			b[i++] = (uint8_t)v;
		return zsamp::IsCallBefore(b);
	};
	CHECK(chk({ 0xE8, 0x10, 0x20, 0x30, 0x40 }));               // call rel32
	CHECK(chk({ 0xFF, 0x15, 0x00, 0x10, 0x82, 0x00 }));         // call [abs]
	CHECK(chk({ 0xFF, 0xD0 }));                                 // call eax
	CHECK(chk({ 0xFF, 0xD6 }));                                 // call esi
	CHECK(chk({ 0xFF, 0x10 }));                                 // call [eax]
	CHECK(chk({ 0xFF, 0x50, 0x08 }));                           // call [eax+8]
	CHECK(chk({ 0xFF, 0x54, 0x24, 0x08 }));                     // call [esp+8] (SIB)
	CHECK(chk({ 0xFF, 0x90, 0x00, 0x01, 0x00, 0x00 }));         // call [eax+100h]
	CHECK(chk({ 0xFF, 0x94, 0x81, 0x00, 0x01, 0x00, 0x00 }));   // call [ecx+eax*4+100h]
	CHECK(chk({ 0xFF, 0x14, 0x85, 0x00, 0x10, 0x82, 0x00 }));   // call [eax*4+abs] (SIB, bez bazy)
	CHECK(chk({ 0xFF, 0x14, 0x24 }));                           // call [esp]
	CHECK(!chk({ 0x8B, 0x45, 0x08 }));                          // mov eax,[ebp+8]
	CHECK(!chk({ 0xFF, 0xE0 }));                                // jmp eax - nie call
	CHECK(!chk({ 0xFF, 0x50 }));                                // FF 50 bez disp8 - za krotkie
	CHECK(!chk({ 0x90, 0x90, 0x90, 0x90, 0x90 }));
}

// Stos testowy: slowa od esp; adresy "kodu" to 0x4xxxxx (exe) i 0x7xxxxx (dll).
struct FakeCode {
	bool operator()(uint32_t v) const {
		return (v >= 0x401000 && v < 0x82E000) || (v >= 0x77000000 && v < 0x77100000);
	}
};

TEST(samp_walk_follows_ebp_chain) {
	const uint32_t esp = 0x100000;
	std::vector<uint32_t> w(64, 0);
	// ramki: ebp0 = esp+0x20 -> [ebp0] = esp+0x40, [ebp0+4] = 0x77001234; ebp1 = esp+0x40 -> [ebp1] = esp+0x80, ret 0x500100
	w[0x20 / 4] = esp + 0x40;
	w[0x24 / 4] = 0x77001234;
	w[0x40 / 4] = esp + 0x80;
	w[0x44 / 4] = 0x500100;
	w[0x80 / 4] = 0;   // koniec lancucha (poprzedni ebp ponizej)
	w[0x84 / 4] = 0x600200;
	uint32_t out[zsamp::MAX_RET];
	int n = zsamp::WalkStack(w.data(), (int)w.size(), esp, esp + 0x20, FakeCode(), out, zsamp::MAX_RET);
	CHECK(n == 3);
	CHECK(n == 3 && out[0] == 0x77001234 && out[1] == 0x500100 && out[2] == 0x600200);
}

TEST(samp_walk_scans_below_ebp_and_after_chain) {
	const uint32_t esp = 0x100000;
	std::vector<uint32_t> w(64, 0);
	w[1] = 0x77000500;           // powrot z funkcji bez ramki (np. stub ntdll) - miedzy esp a ebp
	w[2] = 0x12345678;           // nie kod
	w[0x20 / 4] = 0x00000010;    // [ebp] = smiec ponizej ramki - lancuch sie urywa
	w[0x24 / 4] = 0x500100;
	w[0x30 / 4] = 0x600300;      // dalej skanem
	uint32_t out[zsamp::MAX_RET];
	int n = zsamp::WalkStack(w.data(), (int)w.size(), esp, esp + 0x20, FakeCode(), out, zsamp::MAX_RET);
	CHECK(n == 3);
	CHECK(n == 3 && out[0] == 0x77000500 && out[1] == 0x500100 && out[2] == 0x600300);
}

TEST(samp_walk_without_frame_pointer_scans_from_esp_and_limits) {
	const uint32_t esp = 0x100000;
	std::vector<uint32_t> w(40, 0);
	for (int i = 0; i < 20; i++)
		w[i * 2] = 0x500000 + i * 0x10;
	uint32_t out[zsamp::MAX_RET];
	int n = zsamp::WalkStack(w.data(), (int)w.size(), esp, 0xDEADBEEF, FakeCode(), out, zsamp::MAX_RET);   // ebp poza stosem
	CHECK(n == zsamp::MAX_RET);
	CHECK(out[0] == 0x500000 && out[7] == 0x500070);
	n = zsamp::WalkStack(w.data(), 0, esp, esp, FakeCode(), out, zsamp::MAX_RET);   // pusty stos
	CHECK(n == 0);
	// pierwszy adres exe na calej kopii (dalej niz MAX_RET ramek)
	std::vector<uint32_t> d(200, 0);
	for (int i = 0; i < 12; i++)
		d[i] = 0x77000100 + i;
	d[150] = 0x456789;
	CHECK(zsamp::FindFirst(d.data(), (int)d.size(), [](uint32_t v) { return v >= 0x401000 && v < 0x82E000; }) == 0x456789);
	CHECK(zsamp::FindFirst(d.data(), 100, [](uint32_t v) { return v >= 0x401000 && v < 0x82E000; }) == 0);
}

TEST(samp_code_ranges_contains_and_host) {
	static zsamp::CodeRanges c;
	c.Clear();
	CHECK(!c.Contains(0x401000));
	CHECK(c.Add(0x77000000, 0x77100000));
	CHECK(c.Add(0x401000, 0x82E000));
	CHECK(c.Add(0x10001000, 0x10002000));
	c.SetHost(0x401000, 0x82E000);
	c.Finish();
	CHECK(c.Count() == 3);
	CHECK(c.Contains(0x401000) && c.Contains(0x82DFFF) && !c.Contains(0x82E000) && !c.Contains(0x400FFF));
	CHECK(c.Contains(0x77000000) && !c.Contains(0x77100000) && c.Contains(0x10001FFF));
	CHECK(c.InHost(0x500000) && !c.InHost(0x77000010));
}

TEST(samp_symtable_parse_find) {
	zsamp::SymTable s;
	int n = s.Parse("# komentarz\n00402000 B::two\n00401000 A::one\r\n\n00403000 C::three with space\nzle\n00401000 A::dup\n");
	CHECK(n == 3 && s.Size() == 3);
	CHECK(s.Find(0x400FFF) == -1);
	int i = s.Find(0x401000);
	CHECK(i >= 0 && std::string(s.Name(i)) == "A::one" && s.Addr(i) == 0x401000);
	i = s.Find(0x401FFF);
	CHECK(i >= 0 && std::string(s.Name(i)) == "A::one");
	i = s.Find(0x402000);
	CHECK(i >= 0 && std::string(s.Name(i)) == "B::two");
	i = s.Find(0x7FFFFFFF);
	CHECK(i >= 0 && std::string(s.Name(i)) == "C::three with space");
	zsamp::SymTable e;   // eksporty: przy tym samym adresie wygrywa pierwsza alfabetycznie (Nt przed Zw)
	e.Add(0x77001000, "ZwWaitForSingleObject");
	e.Add(0x77001000, "NtWaitForSingleObject");
	e.Add(0x77000500, "NtClose");
	e.Finish();
	CHECK(e.Size() == 2 && std::string(e.Name(e.Find(0x77001004))) == "NtWaitForSingleObject");
}

TEST(samp_real_g2a_funcs_file) {
	// data\System\zDismember\g2a_funcs.txt (tools\gen_g2a_funcs.py z Names_G2A.hpp) - adresy z naglowkow Union SDK
	std::string text;
	CHECK(ReadText("..\\data\\System\\zDismember\\g2a_funcs.txt", text));
	zsamp::SymTable s;
	CHECK(s.Parse(text) > 10000);
	int i = s.Find(0x005DD040 + 0x20);   // zCResourceManager::CacheIn (zResource.h: zCall( 0x005DD040 ))
	CHECK(i >= 0 && std::string(s.Name(i)) == "zCResourceManager::CacheIn" && s.Addr(i) == 0x005DD040);
	i = s.Find(0x004010C0);
	CHECK(i >= 0 && std::string(s.Name(i)) == "zSTRING::zSTRING");
	CHECK(s.Find(0x00400FFF) == -1);
}
TEST(samp_classify_paths) {
	const std::string win = "C:\\Windows";
	CHECK(zsamp::BaseName("C:\\Windows\\SysWOW64\\ntdll.dll") == "ntdll.dll");
	CHECK(zsamp::BaseName("ddraw.dll") == "ddraw.dll");
	CHECK(zsamp::BaseName("a/b/c.dll") == "c.dll");
	CHECK(zsamp::ClassifyPath("C:\\Windows\\SysWOW64\\ntdll.dll", win, false) == zsamp::MOD_SYSTEM);
	CHECK(zsamp::ClassifyPath("c:\\windows\\System32\\DriverStore\\FileRepository\\nv\\nvwgf2um.dll", win, false) == zsamp::MOD_SYSTEM);
	CHECK(zsamp::ClassifyPath("\\Device\\HarddiskVolume3\\Windows\\System32\\wow64cpu.dll", win, false) == zsamp::MOD_SYSTEM);
	CHECK(zsamp::ClassifyPath("D:\\SteamLibrary\\steamapps\\common\\Gothic II\\system\\ddraw.dll", win, false) == zsamp::MOD_OTHER);
	CHECK(zsamp::ClassifyPath("D:\\Games\\Other\\x.dll", win, false) == zsamp::MOD_OTHER);
	CHECK(zsamp::ClassifyPath("D:\\SteamLibrary\\steamapps\\common\\Gothic II\\system\\Gothic2.exe", win, true) == zsamp::MOD_HOST);
	CHECK(zsamp::IsTransit("WOW64CPU.dll") && zsamp::IsTransit("wow64.dll") && !zsamp::IsTransit("ntdll.dll"));
}

// Mapa testowa: exe 0x400000 (funkcje), ntdll (eksporty), ddraw (GD3D11, bez symboli), zEBA (plugin), pamiec anonimowa.
static zsamp::ModuleMap SMap() {
	auto host = std::make_shared<zsamp::SymTable>();
	host->Parse("00401000 zCWorld::Render\n00402000 oCGame::Render\n00403000 zCResourceManager::CacheIn\n00404000 oCNpc::Move\n");
	auto nt = std::make_shared<zsamp::SymTable>();
	nt->Add(0x77001000, "NtWaitForSingleObject");
	nt->Add(0x77002000, "RtlAllocateHeap");
	nt->Finish();
	zsamp::ModuleMap m;
	zsamp::Module ntdll;
	ntdll.base = 0x77000000;
	ntdll.end = 0x77100000;
	ntdll.kind = zsamp::MOD_SYSTEM;
	ntdll.name = "ntdll.dll";
	ntdll.syms = nt;
	m.Add(ntdll);
	zsamp::Module exe;
	exe.base = 0x400000;
	exe.end = 0xB52000;
	exe.kind = zsamp::MOD_HOST;
	exe.name = "Gothic2.exe";
	exe.syms = host;
	exe.exact = true;
	m.Add(exe);
	zsamp::Module ddraw;
	ddraw.base = 0x10000000;
	ddraw.end = 0x10800000;
	ddraw.name = "ddraw.dll";
	m.Add(ddraw);
	zsamp::Module eba;
	eba.base = 0x20000000;
	eba.end = 0x20100000;
	eba.name = "zEBA.dll";
	m.Add(eba);
	zsamp::Module anon;
	anon.base = 0x30000000;
	anon.end = 0x30010000;
	anon.kind = zsamp::MOD_ANON;
	anon.name = "pamiec 30000000";
	m.Add(anon);
	zsamp::Module w64;
	w64.base = 0x77700000;
	w64.end = 0x77710000;
	w64.kind = zsamp::MOD_SYSTEM;
	w64.name = "wow64cpu.dll";
	w64.transit = true;
	m.Add(w64);
	m.Finish();
	return m;
}

TEST(samp_module_map_find_and_host) {
	zsamp::ModuleMap m = SMap();
	CHECK(m.Count() == 6);
	CHECK(m.Host() >= 0 && m.At(m.Host()).name == "Gothic2.exe");
	CHECK(m.Find(0x400000) == m.Host() && m.Find(0xB51FFF) == m.Host());
	CHECK(m.Find(0xB52000) == -1 && m.Find(0x3FFFFF) == -1 && m.Find(0) == -1);
	int k = m.Find(0x77001000);
	CHECK(k >= 0 && m.At(k).name == "ntdll.dll");
	k = m.Find(0x200FFFFF);
	CHECK(k >= 0 && m.At(k).name == "zEBA.dll");
}

TEST(samp_resolve_host_and_foreign_samples) {
	zsamp::ModuleMap m = SMap();
	int host = m.Host();
	// w exe: funkcja z tabeli
	zsamp::Hit h = zsamp::Resolve(SMake(1, 0x403456, { 0x402100 }), m);
	CHECK(h.mod == host && h.sym >= 0 && std::string(m.At(host).syms->Name(h.sym)) == "zCResourceManager::CacheIn");
	CHECK(h.caller == -1 && !h.moved);
	// czekanie w ntdll, wywolane z ddraw (GD3D11), ktore wywolal oCGame::Render; plugin zEBA wyzej na stosie
	h = zsamp::Resolve(SMake(2, 0x7700100C, { 0x10001234, 0x20000100, 0x402010, 0x401010 }), m);
	CHECK(h.mod == m.Find(0x77000000) && h.sym >= 0 && std::string(m.At(h.mod).syms->Name(h.sym)) == "NtWaitForSingleObject");
	CHECK(h.caller >= 0 && std::string(m.At(host).syms->Name(h.caller)) == "oCGame::Render");
	CHECK(h.via == m.Find(0x10000000));   // pierwszy obcy modul nad miejscem: ddraw.dll
	// eksport daleko przed adresem - funkcja nieznana; exe tylko glebiej (hostRet)
	h = zsamp::Resolve(SMake(3, 0x77050000, { 0x10001234 }, 0x404010), m);
	CHECK(h.sym == -1 && h.caller >= 0 && std::string(m.At(host).syms->Name(h.caller)) == "oCNpc::Move");
	// EIP w pamieci anonimowej (trampolina) albo w przejsciu wow64 - miejsce = pierwsza ramka
	h = zsamp::Resolve(SMake(4, 0x30000010, { 0x7700100C, 0x402010 }), m);
	CHECK(h.moved && h.mod == m.Find(0x77000000) && h.loc == 0x7700100C);
	CHECK(h.caller >= 0 && std::string(m.At(host).syms->Name(h.caller)) == "oCGame::Render");
	h = zsamp::Resolve(SMake(5, 0x77700005, { 0x7700100C }), m);
	CHECK(h.moved && h.mod == m.Find(0x77000000));
	// EIP poza modulami i bez stosu
	h = zsamp::Resolve(SMake(6, 0x05000000), m);
	CHECK(h.mod == -1 && h.sym == -1 && h.caller == -1);
}

TEST(samp_spike_line_modules_functions_callers) {
	zsamp::ModuleMap m = SMap();
	zsamp::Names names;
	zsamp::FrameAgg a;
	for (int i = 0; i < 6; i++)   // 6 x CacheIn
		a.Add(zsamp::Resolve(SMake(i, 0x403010), m), m, names);
	for (int i = 0; i < 2; i++)   // 2 x oCNpc::Move
		a.Add(zsamp::Resolve(SMake(i, 0x404010), m), m, names);
	for (int i = 0; i < 3; i++)   // 3 x ntdll NtWait z oCGame::Render przez ddraw
		a.Add(zsamp::Resolve(SMake(i, 0x7700100C, { 0x10001234, 0x402010 }), m), m, names);
	a.Add(zsamp::Resolve(SMake(9, 0x10002000, { 0x401010 }), m), m, names);   // ddraw z zCWorld::Render
	CHECK(a.samples == 12);
	std::string s = zsamp::SpikeLine(a, 73.2f, names);
	CHECK(PHas(s.c_str(), "Prof: probki 73.2 ms (12 probek): Gothic2.exe 67% [zCResourceManager::CacheIn 50%, oCNpc::Move 17%]"));
	CHECK(PHas(s.c_str(), ", ntdll.dll 25% [NtWaitForSingleObject 25%; z oCGame::Render 25%; przez ddraw.dll 25%]"));
	CHECK(PHas(s.c_str(), ", ddraw.dll 8% [z zCWorld::Render 8%]"));
	CHECK(s.find("Gothic2.exe") < s.find("ntdll.dll") && s.find("ntdll.dll") < s.find(", ddraw.dll"));
	zsamp::FrameAgg empty;
	CHECK(PHas(zsamp::SpikeLine(empty, 50.0f, names).c_str(), "Prof: probki 50.0 ms - brak probek"));
}

TEST(samp_spike_line_top_modules_and_rest) {
	zsamp::ModuleMap m;
	zsamp::Names names;
	for (int i = 0; i < 8; i++) {
		zsamp::Module x;
		x.base = 0x10000000u + 0x1000000u * i;
		x.end = x.base + 0x100000;
		x.name = "m" + std::to_string(i) + ".dll";
		m.Add(x);
	}
	m.Finish();
	zsamp::FrameAgg a;
	for (int i = 0; i < 8; i++)
		for (int k = 0; k <= 8 - i; k++)   // m0 9x, m1 8x ... m7 2x
			a.Add(zsamp::Resolve(SMake(k, 0x10000010u + 0x1000000u * i), m), m, names);
	a.Add(zsamp::Resolve(SMake(1, 0x05000000), m), m, names);   // poza modulami
	std::string s = zsamp::SpikeLine(a, 60.0f, names);
	CHECK(PHas(s.c_str(), "m0.dll") && PHas(s.c_str(), "m4.dll") && !PHas(s.c_str(), "m5.dll"));
	CHECK(PHas(s.c_str(), ", inne "));
}

TEST(samp_stats_excess_per_frame_and_summary) {
	zsamp::ModuleMap m = SMap();
	zsamp::Names names;
	zsamp::Stats st;
	// 10 klatek normalnych po 10 ms: 10 probek, 9 x oCNpc::Move, 1 x CacheIn
	for (int f = 0; f < 10; f++) {
		zsamp::FrameAgg a;
		for (int i = 0; i < 9; i++)
			a.Add(zsamp::Resolve(SMake(i, 0x404010), m), m, names);
		a.Add(zsamp::Resolve(SMake(9, 0x403010), m), m, names);
		st.Add(a, false, 10.0f);
	}
	// 2 przycinki po 50 ms: 50 probek, 9 x Move, 41 x CacheIn
	for (int f = 0; f < 2; f++) {
		zsamp::FrameAgg a;
		for (int i = 0; i < 9; i++)
			a.Add(zsamp::Resolve(SMake(i, 0x404010), m), m, names);
		for (int i = 0; i < 41; i++)
			a.Add(zsamp::Resolve(SMake(i, 0x403010), m), m, names);
		st.Add(a, true, 50.0f);
	}
	zsamp::FrameAgg none;
	st.Add(none, true, 80.0f);   // klatka bez probek (probkowanie wstrzymane) - pomijana
	CHECK(st.spike.frames == 2 && st.spike.samples == 100 && st.normal.frames == 10 && st.normal.samples == 100);
	uint64_t keyCache = ((uint64_t)0x400000 << 32) | 0x403000;
	CHECK(PNear(st.spike.MsPerFrame(st.spike.funcs[keyCache]), 41.0));
	CHECK(PNear(st.normal.MsPerFrame(st.normal.funcs[keyCache]), 1.0));
	std::vector<std::string> out;
	st.Lines(names, "od resetu", out);
	CHECK(out.size() > 6);
	CHECK(PHas(out[0].c_str(), "Prof: probki od resetu: przycinki 2 kl."));
	bool cacheFirst = false, moveRow = false;
	for (size_t i = 0; i < out.size(); i++) {
		if (PHas(out[i].c_str(), "funkcje - przycinki") && i + 2 < out.size())
			cacheFirst = PHas(out[i + 2].c_str(), "zCResourceManager::CacheIn") && PHas(out[i + 2].c_str(), "+40.00");
		if (PHas(out[i].c_str(), "oCNpc::Move") && PHas(out[i].c_str(), "9.00"))
			moveRow = true;
	}
	CHECK(cacheFirst);
	CHECK(moveRow);
	std::string sh = st.Short(names);
	CHECK(PHas(sh.c_str(), "przycinek 2") && PHas(sh.c_str(), "zCResourceManager::CacheIn +40.0 ms"));
	st.Clear();
	CHECK(st.spike.frames == 0 && st.normal.samples == 0 && st.spike.funcs.empty());
}

// ---------- AniSwap (zamiana animacji 1H <-> 2H bohatera) ----------
#include "Core/AniSwap.h"

TEST(swap_weapon_types_from_mode_and_item_flags) {
	CHECK(SwapTypeOfWeaponMode(SWAP_WM_DAG) == SWAP_TYPE_1H);
	CHECK(SwapTypeOfWeaponMode(SWAP_WM_1H) == SWAP_TYPE_1H);
	CHECK(SwapTypeOfWeaponMode(SWAP_WM_2H) == SWAP_TYPE_2H);
	CHECK(SwapTypeOfWeaponMode(0) == SWAP_TYPE_NONE && SwapTypeOfWeaponMode(1) == SWAP_TYPE_NONE);
	CHECK(SwapTypeOfWeaponMode(5) == SWAP_TYPE_NONE && SwapTypeOfWeaponMode(7) == SWAP_TYPE_NONE);
	CHECK(SwapTypeOfItemFlags(1 << 13) == SWAP_TYPE_1H);   // ITM_FLAG_DAG
	CHECK(SwapTypeOfItemFlags(1 << 14) == SWAP_TYPE_1H);   // SWD
	CHECK(SwapTypeOfItemFlags(1 << 15) == SWAP_TYPE_1H);   // AXE
	CHECK(SwapTypeOfItemFlags(1 << 16) == SWAP_TYPE_2H);   // 2HD_SWD
	CHECK(SwapTypeOfItemFlags(1 << 17) == SWAP_TYPE_2H);   // 2HD_AXE
	CHECK(SwapTypeOfItemFlags(1 << 19) == SWAP_TYPE_NONE); // BOW
	CHECK(SwapTypeOfItemFlags(0) == SWAP_TYPE_NONE);
	CHECK(SwapOtherType(SWAP_TYPE_1H) == SWAP_TYPE_2H && SwapOtherType(SWAP_TYPE_2H) == SWAP_TYPE_1H);
	CHECK(SwapOtherType(SWAP_TYPE_NONE) == SWAP_TYPE_NONE);
}
TEST(swap_mode_covers_types) {
	CHECK(!SwapModeCovers(SWAP_OFF, SWAP_TYPE_1H) && !SwapModeCovers(SWAP_OFF, SWAP_TYPE_2H));
	CHECK(SwapModeCovers(SWAP_1H_AS_2H, SWAP_TYPE_1H) && !SwapModeCovers(SWAP_1H_AS_2H, SWAP_TYPE_2H));
	CHECK(!SwapModeCovers(SWAP_2H_AS_1H, SWAP_TYPE_1H) && SwapModeCovers(SWAP_2H_AS_1H, SWAP_TYPE_2H));
	CHECK(SwapModeCovers(SWAP_BOTH, SWAP_TYPE_1H) && SwapModeCovers(SWAP_BOTH, SWAP_TYPE_2H));
	CHECK(!SwapModeCovers(SWAP_BOTH, SWAP_TYPE_NONE));
	CHECK(!SwapModeCovers(7, SWAP_TYPE_1H));
	CHECK(std::string(SwapModeLabel(SWAP_OFF)) == "wyl." && std::string(SwapModeLabel(SWAP_BOTH)) == "obie");
	CHECK(std::string(SwapModeLabel(SWAP_1H_AS_2H)) == "1H jak 2H" && std::string(SwapModeLabel(SWAP_2H_AS_1H)) == "2H jak 1H");
}
TEST(swap_active_global_follows_mode) {
	SwapSettings s;
	s.mode = SWAP_1H_AS_2H;
	CHECK(SwapActiveFor(s, SWAP_TYPE_1H, "ITMW_1H_SWORD_01"));
	CHECK(!SwapActiveFor(s, SWAP_TYPE_2H, "ITMW_2H_AXE_L_01"));
	CHECK(!SwapActiveFor(s, SWAP_TYPE_NONE, ""));
	s.mode = SWAP_OFF;
	CHECK(!SwapActiveFor(s, SWAP_TYPE_1H, "ITMW_1H_SWORD_01"));
}
TEST(swap_active_per_weapon_needs_list_and_cover) {
	SwapSettings s;
	s.perWeapon = true;
	s.mode = SWAP_BOTH;
	s.weapons = { "ITMW_1H_SWORD_01", "ITMW_2H_AXE_L_01" };
	CHECK(SwapActiveFor(s, SWAP_TYPE_1H, "ITMW_1H_SWORD_01"));
	CHECK(SwapActiveFor(s, SWAP_TYPE_1H, "itmw_1h_sword_01"));   // nazwa bez wielkosci liter
	CHECK(!SwapActiveFor(s, SWAP_TYPE_1H, "ITMW_1H_MACE_01"));
	CHECK(SwapActiveFor(s, SWAP_TYPE_2H, "ITMW_2H_AXE_L_01"));
	s.mode = SWAP_1H_AS_2H;
	CHECK(!SwapActiveFor(s, SWAP_TYPE_2H, "ITMW_2H_AXE_L_01"));   // wariant nie obejmuje 2H
	s.mode = SWAP_OFF;
	CHECK(!SwapActiveFor(s, SWAP_TYPE_1H, "ITMW_1H_SWORD_01"));   // wyl. = nic
}
TEST(swap_table_identity_and_global_variants) {
	SwapTable id = SwapIdentity();
	CHECK(SwapIsIdentity(id));
	for (int i = 0; i < SWAP_WM_COUNT; i++)
		CHECK(id.map[i] == i);
	SwapSettings s;
	s.mode = SWAP_1H_AS_2H;
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_NONE, "");
	CHECK(!SwapIsIdentity(t));
	CHECK(t.map[SWAP_WM_DAG] == SWAP_WM_2H && t.map[SWAP_WM_1H] == SWAP_WM_2H && t.map[SWAP_WM_2H] == SWAP_WM_2H);
	CHECK(t.map[0] == 0 && t.map[1] == 1 && t.map[5] == 5 && t.map[7] == 7);
	s.mode = SWAP_2H_AS_1H;
	t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	CHECK(t.map[SWAP_WM_1H] == SWAP_WM_1H && t.map[SWAP_WM_2H] == SWAP_WM_1H && t.map[SWAP_WM_DAG] == SWAP_WM_DAG);
	s.mode = SWAP_BOTH;
	t = SwapBuildTable(s, SWAP_TYPE_2H, "X");
	CHECK(t.map[SWAP_WM_1H] == SWAP_WM_2H && t.map[SWAP_WM_2H] == SWAP_WM_1H && t.map[SWAP_WM_DAG] == SWAP_WM_2H);
	s.mode = SWAP_OFF;
	CHECK(SwapIsIdentity(SwapBuildTable(s, SWAP_TYPE_1H, "X")));
	CHECK(SwapSame(SwapIdentity(), SwapBuildTable(s, SWAP_TYPE_1H, "X")));
	s.mode = SWAP_BOTH;
	CHECK(!SwapSame(SwapIdentity(), SwapBuildTable(s, SWAP_TYPE_1H, "X")));
}
TEST(swap_table_per_weapon_only_current_type) {
	SwapSettings s;
	s.perWeapon = true;
	s.mode = SWAP_BOTH;
	s.weapons = { "ITMW_1H_SWORD_01" };
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_1H, "ITMW_1H_SWORD_01");
	CHECK(t.map[SWAP_WM_1H] == SWAP_WM_2H && t.map[SWAP_WM_DAG] == SWAP_WM_2H);
	CHECK(t.map[SWAP_WM_2H] == SWAP_WM_2H);   // 2H nie jest bronia biezaca - natywnie
	CHECK(SwapIsIdentity(SwapBuildTable(s, SWAP_TYPE_1H, "ITMW_1H_MACE_01")));   // innej broni nie ma na liscie
	CHECK(SwapIsIdentity(SwapBuildTable(s, SWAP_TYPE_NONE, "")));
}
TEST(swap_map_weapon_mode_clamps_and_maps) {
	SwapSettings s;
	s.mode = SWAP_1H_AS_2H;
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	CHECK(SwapMapWeaponMode(t, SWAP_WM_1H) == SWAP_WM_2H);
	CHECK(SwapMapWeaponMode(t, SWAP_WM_2H) == SWAP_WM_2H);
	CHECK(SwapMapWeaponMode(t, 1) == 1);
	CHECK(SwapMapWeaponMode(t, -1) == -1);   // poza tablica - bez zmian
	CHECK(SwapMapWeaponMode(t, 9) == 9);
}
TEST(swap_remap_mode_array_copies_from_source_modes) {
	SwapSettings s;
	s.mode = SWAP_BOTH;
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	int arr[10] = { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109 };
	SwapRemapModeArray(arr, t);
	CHECK(arr[0] == 100 && arr[1] == 101 && arr[5] == 105 && arr[7] == 107);
	CHECK(arr[SWAP_WM_DAG] == 104 && arr[SWAP_WM_1H] == 104 && arr[SWAP_WM_2H] == 103);   // zamiana, nie kopia po kopii
	CHECK(arr[8] == 108 && arr[9] == 109);   // poza 8 trybami nic nie rusza
	int same[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	SwapRemapModeArray(same, SwapIdentity());
	CHECK(same[2] == 3 && same[3] == 4 && same[4] == 5);
	CHECK(SwapTableLabel(SwapIdentity()) == "brak");
	CHECK(SwapTableLabel(t) == "1H->2H 2H->1H");
	s.mode = SWAP_1H_AS_2H;
	CHECK(SwapTableLabel(SwapBuildTable(s, SWAP_TYPE_1H, "X")) == "1H->2H");
}
TEST(swap_toggle_global_remembers_last_variant) {
	SwapSettings s;
	s.mode = SWAP_2H_AS_1H;
	s.last = SWAP_BOTH;
	SwapToggleResult r = SwapToggle(s, SWAP_TYPE_2H, "ITMW_2H_SWORD");
	CHECK(r.settings.mode == SWAP_OFF && r.settings.last == SWAP_2H_AS_1H && !r.active && !r.perWeaponChange);
	r = SwapToggle(r.settings, SWAP_TYPE_2H, "ITMW_2H_SWORD");
	CHECK(r.settings.mode == SWAP_2H_AS_1H && r.active);
	SwapSettings fresh;   // domyslnie: wyl., ostatni = obie
	r = SwapToggle(fresh, SWAP_TYPE_1H, "X");
	CHECK(r.settings.mode == SWAP_BOTH && r.active);
	SwapSettings bad;
	bad.last = 0;         // zly wpis w ini - wlaczenie daje "obie"
	r = SwapToggle(bad, SWAP_TYPE_NONE, "");
	CHECK(r.settings.mode == SWAP_BOTH && !r.active);   // bez broni bialej nic nie gra zamiany
}
TEST(swap_toggle_per_weapon_adds_and_removes_current) {
	SwapSettings s;
	s.perWeapon = true;
	s.mode = SWAP_1H_AS_2H;
	s.last = SWAP_1H_AS_2H;
	SwapToggleResult r = SwapToggle(s, SWAP_TYPE_1H, "ItMw_1h_Sword_01");
	CHECK(r.perWeaponChange && r.active);
	CHECK(r.settings.weapons.size() == 1 && r.settings.weapons[0] == "ITMW_1H_SWORD_01");
	CHECK(r.settings.mode == SWAP_1H_AS_2H);
	r = SwapToggle(r.settings, SWAP_TYPE_1H, "ITMW_1H_SWORD_01");
	CHECK(r.perWeaponChange && !r.active && r.settings.weapons.empty());
	CHECK(r.settings.mode == SWAP_1H_AS_2H);   // wariant zostaje
	// 2H przy wariancie 1H jak 2H: dopisana, wariant przechodzi na "obie" (last nie obejmuje 2H)
	r = SwapToggle(s, SWAP_TYPE_2H, "ITMW_2H_AXE");
	CHECK(r.active && r.settings.mode == SWAP_BOTH && r.settings.weapons.size() == 1);
	// wyl. -> wlaczenie przywraca last, gdy obejmuje rodzaj
	SwapSettings off = s;
	off.mode = SWAP_OFF;
	off.last = SWAP_2H_AS_1H;
	r = SwapToggle(off, SWAP_TYPE_2H, "ITMW_2H_AXE");
	CHECK(r.active && r.settings.mode == SWAP_2H_AS_1H && r.settings.last == SWAP_2H_AS_1H);
	// bron na liscie, ale wariant wyl. - wlaczenie wariantu bez wypisywania
	SwapSettings listed = off;
	listed.weapons = { "ITMW_2H_AXE" };
	r = SwapToggle(listed, SWAP_TYPE_2H, "ITMW_2H_AXE");
	CHECK(r.active && r.settings.weapons.size() == 1 && r.settings.mode == SWAP_2H_AS_1H);
	// bez broni bialej - jak globalnie
	r = SwapToggle(s, SWAP_TYPE_NONE, "");
	CHECK(!r.perWeaponChange && r.settings.mode == SWAP_OFF && r.settings.last == SWAP_1H_AS_2H);
}
TEST(swap_join_weapons_for_ini) {
	CHECK(SwapJoinWeapons({}) == "");
	CHECK(SwapJoinWeapons({ "A", "B" }) == "A,B");
}
TEST(swap_key_menu_round_trip) {
	CHECK(SwapKeyMenuIndex(SWAP_KEY_DEFAULT, SWAP_MODS_DEFAULT) == 0);
	CHECK(SwapKeyMenuIndex(0, SWAP_MODS_DEFAULT) == 1);
	CHECK(SwapKeyMenuIndex(0, 0) == 1);
	CHECK(SwapKeyMenuIndex(SWAP_KEY_DEFAULT, 0) == 2);   // samo U - wlasny
	CHECK(SwapKeyMenuIndex(23, SWAP_MODS_DEFAULT) == 2);
	int key = 23, mods = 4;
	SwapKeyMenuApply(2, key, mods);
	CHECK(key == 23 && mods == 4);
	SwapKeyMenuApply(0, key, mods);
	CHECK(key == SWAP_KEY_DEFAULT && mods == SWAP_MODS_DEFAULT);
	SwapKeyMenuApply(1, key, mods);
	CHECK(key == 0 && mods == SWAP_MODS_DEFAULT);
	CHECK(SwapKeyLabel(SWAP_KEY_DEFAULT, SWAP_MODS_DEFAULT) == "Ctrl+Shift+U");
	CHECK(SwapKeyLabel(SWAP_KEY_DEFAULT, SWAP_MOD_ALT) == "Alt+U");
	CHECK(SwapKeyLabel(0, SWAP_MODS_DEFAULT) == "-");
}
TEST(swap_draw_anis_stay_native) {
	CHECK(SwapIsDrawAni("T_MOVE_2_1HMOVE"));
	CHECK(SwapIsDrawAni("T_1HMOVE_2_MOVE"));
	CHECK(SwapIsDrawAni("T_MOVE_2_2HMOVE"));
	CHECK(SwapIsDrawAni("T_RUN_2_1H"));
	CHECK(SwapIsDrawAni("T_1H_2_1HRUN"));
	CHECK(SwapIsDrawAni("T_1HRUN_2_1H"));
	CHECK(SwapIsDrawAni("T_1H_2_RUN"));
	CHECK(SwapIsDrawAni("t_2h_2_1h"));       // zmiana broni
	CHECK(!SwapIsDrawAni("T_1HRUN_2_1HRUNL"));
	CHECK(!SwapIsDrawAni("T_1HWALK_2_1HSNEAK"));
	CHECK(!SwapIsDrawAni("T_1HATTACKL"));
	CHECK(!SwapIsDrawAni("S_1HRUN"));
	CHECK(!SwapIsDrawAni("T_RUN_2_WALK"));
	CHECK(!SwapIsDrawAni("T_1HPARADE_0"));
	CHECK(!SwapIsDrawAni("T_BOWWALK_2_BOWAIM"));
}
TEST(swap_ani_name_replaces_weapon_prefix_per_segment) {
	CHECK(SwapAniName("S_1HRUN", SWAP_TYPE_1H, SWAP_TYPE_2H) == "S_2HRUN");
	CHECK(SwapAniName("T_1HRUN_2_1HRUNL", SWAP_TYPE_1H, SWAP_TYPE_2H) == "T_2HRUN_2_2HRUNL");
	CHECK(SwapAniName("T_1HSFINISH", SWAP_TYPE_1H, SWAP_TYPE_2H) == "T_2HSFINISH");
	CHECK(SwapAniName("T_1HPARADE_0_A2", SWAP_TYPE_1H, SWAP_TYPE_2H) == "T_2HPARADE_0_A2");
	CHECK(SwapAniName("s_2hattack", SWAP_TYPE_2H, SWAP_TYPE_1H) == "S_1HATTACK");
	CHECK(SwapAniName("S_RUN", SWAP_TYPE_1H, SWAP_TYPE_2H) == "");
	CHECK(SwapAniName("S_2HRUN", SWAP_TYPE_1H, SWAP_TYPE_2H) == "");
	CHECK(SwapAniName("T_STAND_2_1H", SWAP_TYPE_1H, SWAP_TYPE_2H) == "T_STAND_2_2H");
	CHECK(SwapAniName("C_1H_X", SWAP_TYPE_1H, SWAP_TYPE_2H) == "");   // tylko S_ / T_
}
TEST(swap_talent_overlay_names) {
	CHECK(SwapTalentOverlay(SWAP_TYPE_1H, 1) == "HUMANS_1HST1");
	CHECK(SwapTalentOverlay(SWAP_TYPE_2H, 2) == "HUMANS_2HST2");
	CHECK(SwapTalentOverlay(SWAP_TYPE_2H, 0) == "");
	CHECK(SwapTalentOverlay(SWAP_TYPE_NONE, 1) == "");
	int type = 0, level = 0;
	CHECK(SwapParseTalentOverlay("HUMANS_2HST1.MDS", type, level) && type == SWAP_TYPE_2H && level == 1);
	CHECK(SwapParseTalentOverlay("humans_1hst3", type, level) && type == SWAP_TYPE_1H && level == 3);
	CHECK(!SwapParseTalentOverlay("HUMANS_1HST1_REMASTER", type, level));
	CHECK(!SwapParseTalentOverlay("HUMANS_2HST2B", type, level));
	CHECK(!SwapParseTalentOverlay("HUMANS_1HST4", type, level));
	CHECK(!SwapParseTalentOverlay("HUMANS_ZDF_W3_1H", type, level));
}
TEST(swap_wanted_overlays_level_of_weapon_in_hand) {
	std::vector<std::string> native = { "HUMANS_ACROBATIC", "HUMANS_1HST2", "HUMANS_2HST1", "HUMANS_SPRINT_REMASTER" };
	// 1H w rece (talent 2), zamiana: 2H na poziomie 2
	auto w = SwapWantedOverlays(native, true, 0, SWAP_TYPE_1H, 2, 1);
	CHECK((w == std::vector<std::string>{ "HUMANS_ACROBATIC", "HUMANS_1HST2", "HUMANS_2HST2", "HUMANS_SPRINT_REMASTER" }));
	// poziom 1 = talent drugiego typu: bez zmian
	CHECK(SwapWantedOverlays(native, true, 1, SWAP_TYPE_1H, 2, 1) == native);
	// bez zamiany: natywne
	CHECK(SwapWantedOverlays(native, false, 0, SWAP_TYPE_1H, 2, 1) == native);
	// 2H w rece bez talentu: 1H bez nakladki
	w = SwapWantedOverlays(native, true, 0, SWAP_TYPE_2H, 2, 0);
	CHECK((w == std::vector<std::string>{ "HUMANS_ACROBATIC", "HUMANS_2HST1", "HUMANS_SPRINT_REMASTER" }));
	// natywnie bez nakladki 2H (talent 0), 1H mistrz: 2H dopisana na koncu listy chcianych
	std::vector<std::string> n2 = { "HUMANS_1HST2" };
	w = SwapWantedOverlays(n2, true, 0, SWAP_TYPE_1H, 2, 0);
	CHECK((w == std::vector<std::string>{ "HUMANS_1HST2", "HUMANS_2HST2" }));
	// styl zEBA 2HST3 tez podmieniany (nazwa talentu)
	std::vector<std::string> n3 = { "HUMANS_2HST3", "HUMANS_2HST2B" };
	w = SwapWantedOverlays(n3, true, 0, SWAP_TYPE_1H, 1, 2);
	CHECK((w == std::vector<std::string>{ "HUMANS_2HST1", "HUMANS_2HST2B" }));
	// .MDS i male litery w natywnych
	std::vector<std::string> n4 = { "humans_2hst1.mds" };
	w = SwapWantedOverlays(n4, true, 0, SWAP_TYPE_1H, 2, 1);
	CHECK((w == std::vector<std::string>{ "HUMANS_2HST2" }));
	// bron inna niz 1H / 2H - natywne
	CHECK(SwapWantedOverlays(native, true, 0, SWAP_TYPE_NONE, 2, 1) == native);
}
TEST(swap_plan_overlays_replaces_in_place) {
	std::vector<std::string> model = { "HUMANS_1HST2", "HUMANS_2HST1", "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_W3_2H" };
	std::vector<std::string> native = { "HUMANS_1HST2", "HUMANS_2HST1", "HUMANS_SPRINT_REMASTER" };
	auto wanted = SwapWantedOverlays(native, true, 0, SWAP_TYPE_1H, 2, 1);
	SwapOverlayPlan p = SwapPlanOverlays(model, native, wanted);
	CHECK((p.remove == std::vector<std::string>{ "HUMANS_2HST1" }));
	CHECK((p.add == std::vector<std::string>{ "HUMANS_2HST2" }));
	CHECK((p.order == std::vector<std::string>{ "HUMANS_1HST2", "HUMANS_2HST2", "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_W3_2H" }));
	// przywrocenie: model po zamianie, chciane = natywne
	SwapOverlayPlan back = SwapPlanOverlays(p.order, native, native);
	CHECK((back.remove == std::vector<std::string>{ "HUMANS_2HST2" }));
	CHECK((back.add == std::vector<std::string>{ "HUMANS_2HST1" }));
	CHECK(back.order == model);
	// nic do zmiany
	SwapOverlayPlan none = SwapPlanOverlays(model, native, native);
	CHECK(none.remove.empty() && none.add.empty() && none.order == model);
}
TEST(swap_plan_overlays_adds_at_bottom_and_restores_native_position) {
	// natywnie bez 2H: dopisana na sam dol (nad nia style zEBA / W3)
	std::vector<std::string> model = { "HUMANS_1HST2", "HUMANS_ZDF_W3_2H" };
	std::vector<std::string> native = { "HUMANS_1HST2" };
	auto wanted = SwapWantedOverlays(native, true, 0, SWAP_TYPE_1H, 2, 0);
	SwapOverlayPlan p = SwapPlanOverlays(model, native, wanted);
	CHECK(p.remove.empty() && (p.add == std::vector<std::string>{ "HUMANS_2HST2" }));
	CHECK((p.order == std::vector<std::string>{ "HUMANS_2HST2", "HUMANS_1HST2", "HUMANS_ZDF_W3_2H" }));
	// zdjeta natywna bez zastepstwa (talent 0) - przy przywroceniu wraca nad poprzednia natywna
	std::vector<std::string> model2 = { "HUMANS_ACROBATIC", "HUMANS_1HST1", "HUMANS_REMASTER" };
	std::vector<std::string> native2 = { "HUMANS_ACROBATIC", "HUMANS_1HST1", "HUMANS_2HST2", "HUMANS_REMASTER" };
	SwapOverlayPlan r = SwapPlanOverlays(model2, native2, native2);
	CHECK(r.remove.empty() && (r.add == std::vector<std::string>{ "HUMANS_2HST2" }));
	CHECK((r.order == std::vector<std::string>{ "HUMANS_ACROBATIC", "HUMANS_1HST1", "HUMANS_2HST2", "HUMANS_REMASTER" }));
	// nakladki inne niz talentu nie sa ruszane, nawet gdy ich nie ma w natywnych
	std::vector<std::string> model3 = { "HUMANS_ZDF_NAT_1HST1", "HUMANS_2HST1" };
	SwapOverlayPlan q = SwapPlanOverlays(model3, { "HUMANS_2HST1" }, { "HUMANS_2HST1" });
	CHECK(q.remove.empty() && q.add.empty() && q.order == model3);
	CHECK(SwapTalentSignature({ "HUMANS_2HST1", "HUMANS_ACROBATIC", "HUMANS_1HST2.MDS" }) == "HUMANS_1HST2,HUMANS_2HST1");
	CHECK(SwapTalentSignature({}) == "");
}
TEST(swap_defer_reasons_hard_and_soft) {
	SwapBusy b;
	CHECK(SwapDeferReason(b, 0) == nullptr);
	b.attacking = true;
	CHECK(SwapDeferReason(b, 0) != nullptr && SwapDeferReason(b, 100000) != nullptr);   // twarde czekaja zawsze
	b = SwapBusy();
	b.director = true;
	CHECK(SwapDeferReason(b, 500) != nullptr);
	b = SwapBusy();
	b.weaponMsg = true;
	CHECK(SwapDeferReason(b, 500) != nullptr);
	b = SwapBusy();
	b.body = true;
	CHECK(SwapDeferReason(b, 500) != nullptr);
	b = SwapBusy();
	b.soft = true;
	CHECK(SwapDeferReason(b, 0) != nullptr);
	CHECK(SwapDeferReason(b, SWAP_SOFT_WAIT_FRAMES - 1) != nullptr);
	CHECK(SwapDeferReason(b, SWAP_SOFT_WAIT_FRAMES) == nullptr);   // miekkie najwyzej tyle klatek
	b.parade = true;
	CHECK(SwapDeferReason(b, SWAP_SOFT_WAIT_FRAMES) != nullptr);
	b = SwapBusy();
	b.moves = true;
	CHECK(std::string(SwapDeferReason(b, 0)) == "unik / przewrot");
}

TEST(swap_mode_arrays_skip_draw_pose_s_weapon) {
	// s_weapon (0x11B0, S_1H / S_2H) = poza w dobyciu broni - DrawWeapon1 czeka na s_weapon[tryb] po natywnym T_RUN_2_1H
	CHECK(!SwapRemapsOffset(SWAP_OFS_S_WEAPON));
	CHECK(!SwapRemapsOffset(SWAP_OFS_S_WEAPON + 3 * 4));
	CHECK(SwapRemapsOffset(0x2D8));    // s_run
	CHECK(SwapRemapsOffset(0x698));    // s_walk
	CHECK(SwapRemapsOffset(0x9E0));    // t_jump_2_runl (ostatnia w pierwszym zakresie)
	CHECK(!SwapRemapsOffset(0xA08));   // t_stand_2_jumpuplow - pojedyncza wartosc
	CHECK(SwapRemapsOffset(0xA20));    // s_sneak
	CHECK(SwapRemapsOffset(0xF98));    // t_hitfrun
	CHECK(!SwapRemapsOffset(0xFC0));   // t_stumble
	CHECK(!SwapRemapsOffset(0x2D4));
	int arrays = 0;
	for (auto& r : SwapModeArrayRanges()) {
		CHECK((r.end - r.begin) % SWAP_ARRAY_STRIDE == 0);
		CHECK(!(SWAP_OFS_S_WEAPON >= r.begin && SWAP_OFS_S_WEAPON < r.end));
		arrays += (r.end - r.begin) / SWAP_ARRAY_STRIDE;
	}
	CHECK(arrays == 46 + 36);
}
TEST(swap_stance_redirect_at_end_of_draw) {
	const int nat[3] = { 101, 201, 301 };   // S_1HRUN, S_1HWALK, S_1HSNEAK
	const int cur[3] = { 102, 202, 302 };   // po zamianie: S_2HRUN...
	CHECK(SwapStanceRedirect(nat, cur, 101) == 102);
	CHECK(SwapStanceRedirect(nat, cur, 201) == 202);
	CHECK(SwapStanceRedirect(nat, cur, 301) == 302);
	CHECK(SwapStanceRedirect(nat, cur, 102) == -1);   // juz postawa po zamianie
	CHECK(SwapStanceRedirect(nat, cur, 555) == -1);
	CHECK(SwapStanceRedirect(nat, cur, -1) == -1);
	const int same[3] = { 101, 201, 301 };
	CHECK(SwapStanceRedirect(nat, same, 101) == -1);  // bez zamiany
	const int missing[3] = { -1, 202, 302 };
	CHECK(SwapStanceRedirect(nat, missing, 101) == -1);   // brak animacji po zamianie - zostaje natywna
}
TEST(swap_weapon_watch_start_stuck_end) {
	SwapWeaponWatch w;
	int a = 0, b = 0;
	CHECK(SwapWatchStep(w, nullptr, 1000, 2000) == SWAP_WATCH_NONE);
	CHECK(SwapWatchStep(w, &a, 1000, 2000) == SWAP_WATCH_START);
	CHECK(SwapWatchStep(w, &a, 2500, 2000) == SWAP_WATCH_NONE);
	CHECK(SwapWatchStep(w, &a, 3000, 2000) == SWAP_WATCH_STUCK);   // 2 s bez konca komunikatu
	CHECK(SwapWatchStep(w, &a, 9000, 2000) == SWAP_WATCH_NONE);    // raz na komunikat
	CHECK(SwapWatchStep(w, nullptr, 9100, 2000) == SWAP_WATCH_END);
	CHECK(SwapWatchStep(w, nullptr, 9200, 2000) == SWAP_WATCH_NONE);
	CHECK(SwapWatchStep(w, &a, 10000, 2000) == SWAP_WATCH_START);
	CHECK(SwapWatchStep(w, &b, 10500, 2000) == SWAP_WATCH_START);  // kolejny komunikat - od nowa
	CHECK(SwapWatchStep(w, &b, 12400, 2000) == SWAP_WATCH_NONE);
	CHECK(SwapWatchStep(w, &b, 12500, 2000) == SWAP_WATCH_STUCK);
	CHECK(SwapWatchStep(w, &b, 12600, 2000) == SWAP_WATCH_NONE);
	CHECK(w.stuck);
	CHECK(SwapWatchStep(w, nullptr, 12700, 2000) == SWAP_WATCH_END && !w.stuck);
}

TEST(swap_style_counterpart_of_other_type) {
	CHECK(SwapStyleCounterpart("HUMANS_ZDF_W3_1H", SWAP_TYPE_2H) == "HUMANS_ZDF_W3_2H");
	CHECK(SwapStyleCounterpart("HUMANS_ZDF_W3_2H", SWAP_TYPE_1H) == "HUMANS_ZDF_W3_1H");
	CHECK(SwapStyleCounterpart("HUMANS_ZDF_NAT_1HST2", SWAP_TYPE_2H) == "HUMANS_ZDF_NAT_2HST2");
	CHECK(SwapStyleCounterpart("HUMANS_1HST1_REMASTER", SWAP_TYPE_2H) == "HUMANS_2HST1_REMASTER");
	CHECK(SwapStyleCounterpart("humans_2hst3", SWAP_TYPE_1H) == "HUMANS_1HST3");
	CHECK(SwapStyleCounterpart("HUMANS_ZDF_W3_1H", SWAP_TYPE_1H) == "HUMANS_ZDF_W3_1H");   // juz tego typu
	CHECK(SwapStyleCounterpart("HUMANS_RAPIER_ST1", SWAP_TYPE_2H) == "");   // bez odpowiednika
	CHECK(SwapStyleCounterpart("HUMANS_1X2ST3", SWAP_TYPE_1H) == "");
	CHECK(SwapStyleCounterpart("HUMANS_2X2B", SWAP_TYPE_1H) == "");
	CHECK(SwapStyleCounterpart("", SWAP_TYPE_2H) == "");
}
TEST(swap_slot_driver_follows_weapon_in_hand) {
	SwapSettings s;
	s.mode = SWAP_1H_AS_2H;
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	// miecz 1H gra animacjami 2H: zestaw 2H prowadzi wybor broni 1H, zestaw 1H - wlasny
	CHECK(SwapSlotDriver(t, SWAP_TYPE_1H, SWAP_TYPE_2H) == SWAP_TYPE_1H);
	CHECK(SwapSlotDriver(t, SWAP_TYPE_1H, SWAP_TYPE_1H) == SWAP_TYPE_1H);
	CHECK(SwapSlotDriver(t, SWAP_TYPE_2H, SWAP_TYPE_2H) == SWAP_TYPE_2H);   // 2H bez zamiany w tym wariancie
	CHECK(SwapSlotDriver(SwapIdentity(), SWAP_TYPE_1H, SWAP_TYPE_2H) == SWAP_TYPE_2H);
	CHECK(SwapSlotDriver(t, SWAP_TYPE_NONE, SWAP_TYPE_2H) == SWAP_TYPE_2H);
	s.mode = SWAP_BOTH;
	t = SwapBuildTable(s, SWAP_TYPE_2H, "X");
	CHECK(SwapSlotDriver(t, SWAP_TYPE_2H, SWAP_TYPE_1H) == SWAP_TYPE_2H);
	CHECK(SwapSlotDriver(t, SWAP_TYPE_2H, SWAP_TYPE_2H) == SWAP_TYPE_2H);   // zestaw 2H nieuzywany - wlasny
}
TEST(swap_restart_watch_flags_repeated_starts) {
	SwapRestartWatch w;
	CHECK(!SwapRestartStep(w, 7, 100, 4, 60));
	CHECK(!SwapRestartStep(w, 7, 102, 4, 60));
	CHECK(!SwapRestartStep(w, 7, 104, 4, 60));
	CHECK(SwapRestartStep(w, 7, 106, 4, 60));    // 4 starty w 60 klatkach
	CHECK(!SwapRestartStep(w, 7, 108, 4, 60));   // raz na serie
	CHECK(!SwapRestartStep(w, 9, 110, 4, 60));   // inna animacja - od nowa
	CHECK(!SwapRestartStep(w, 9, 300, 4, 60));
	CHECK(!SwapRestartStep(w, 9, 400, 4, 60));   // poza oknem - licznik od nowa
	CHECK(!SwapRestartStep(w, 9, 401, 4, 60));
	CHECK(!SwapRestartStep(w, 9, 402, 4, 60));
	CHECK(SwapRestartStep(w, 9, 403, 4, 60));
	CHECK(!SwapRestartStep(w, 7, 500, 4, 60));
	CHECK(!SwapRestartStep(w, 7, 501, 4, 60) && !SwapRestartStep(w, 7, 502, 4, 60));
	CHECK(SwapRestartStep(w, 7, 503, 4, 60));    // nowa seria po zmianie animacji
}

TEST(swap_colliding_modes_for_correct_ani_states) {
	// CorrectAniStates wygasza postawy (s_walk / s_run / s_sneak) trybow innych niz biezacy (bez 2): przy "1H jak 2H"
	// s_run[4] == s_run[3] (S_2HRUN) - biezaca postawa wygaszana co klatke
	SwapSettings s;
	s.mode = SWAP_1H_AS_2H;
	SwapTable t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	auto c = SwapCollidingModes(t, SWAP_WM_1H);
	CHECK((c == std::vector<int>{ SWAP_WM_DAG, SWAP_WM_2H }));
	c = SwapCollidingModes(t, SWAP_WM_2H);   // prawdziwa bron 2H przy tym wariancie - tez kolizja z trybem 3
	CHECK((c == std::vector<int>{ SWAP_WM_DAG, SWAP_WM_1H }));
	s.mode = SWAP_BOTH;
	t = SwapBuildTable(s, SWAP_TYPE_1H, "X");
	CHECK((SwapCollidingModes(t, SWAP_WM_1H) == std::vector<int>{ SWAP_WM_DAG }));
	CHECK(SwapCollidingModes(t, SWAP_WM_2H).empty());
	CHECK(SwapCollidingModes(SwapIdentity(), SWAP_WM_1H).empty());
	CHECK(SwapCollidingModes(t, 0).empty());     // bez broni - tablice trybu 0 bez zmian
	CHECK(SwapCollidingModes(t, -1).empty());
	CHECK(SwapCollidingModes(t, 8).empty());
}

// ---------- Sloty znakow: ini vs stale skryptu (port New Balance) ----------
TEST(sign_ids_script_equal_to_ini_keeps_ini) {
	int ini[SIGN_COUNT] = { 90, 91, 92, 93, 94 }, script[SIGN_COUNT] = { 90, 91, 92, 93, 94 }, out[SIGN_COUNT] = {};
	CHECK(ResolveSignIds(ini, script, out) == 0);
	CHECK(out[0] == 90 && out[4] == 94);
}
TEST(sign_ids_script_wins_over_old_ini) {
	// paczka NB (SPL_Aard = 196 ...) przy starym SignSpellIds=90..94 w ini
	int ini[SIGN_COUNT] = { 90, 91, 92, 93, 94 }, script[SIGN_COUNT] = { 196, 197, 198, 199, 200 }, out[SIGN_COUNT] = {};
	CHECK(ResolveSignIds(ini, script, out) == 5);
	CHECK(out[0] == 196 && out[1] == 197 && out[2] == 198 && out[3] == 199 && out[4] == 200);
}
TEST(sign_ids_missing_script_constant_keeps_ini_for_that_sign) {
	int ini[SIGN_COUNT] = { 196, 197, 198, 199, 200 }, script[SIGN_COUNT] = { -1, -1, -1, -1, -1 }, out[SIGN_COUNT] = {};
	CHECK(ResolveSignIds(ini, script, out) == 0);
	CHECK(out[0] == 196 && out[4] == 200);
	int part[SIGN_COUNT] = { 50, -1, -1, -1, -1 };
	CHECK(ResolveSignIds(ini, part, out) == 1);
	CHECK(out[0] == 50 && out[1] == 197);
}
TEST(sign_ids_invalid_script_value_ignored) {
	int ini[SIGN_COUNT] = { 90, 91, 92, 93, 94 }, script[SIGN_COUNT] = { 5000, 91, 92, 93, 94 }, out[SIGN_COUNT] = {};
	CHECK(ResolveSignIds(ini, script, out) == 0);
	CHECK(out[0] == 90);
}
TEST(sign_ids_duplicate_after_merge_falls_back_to_ini) {
	// skrypt czesciowo inny: Aard = 91 zderzylby sie z Igni z ini - zostaje ini, wynik -1 (do logu)
	int ini[SIGN_COUNT] = { 90, 91, 92, 93, 94 }, script[SIGN_COUNT] = { 91, -1, -1, -1, -1 }, out[SIGN_COUNT] = {};
	CHECK(ResolveSignIds(ini, script, out) == -1);
	CHECK(out[0] == 90 && out[1] == 91);
}
TEST(sign_ids_list_label) {
	int ids[SIGN_COUNT] = { 196, 197, 198, 199, 200 };
	CHECK(SignIdsLabel(ids) == "196,197,198,199,200");
}

// ---------- Nakladki wylaczne obcych mechanik (New Balance: jazda konna HUMANS_MOUNT) ----------
TEST(exclusive_overlay_mount_detected_case_insensitive) {
	CHECK(IsExclusiveForeignOverlay("HUMANS_MOUNT"));
	CHECK(IsExclusiveForeignOverlay("humans_mount.mds"));
	CHECK(!IsExclusiveForeignOverlay("HUMANS_SPRINT"));
	CHECK(!IsExclusiveForeignOverlay("HUMANS_1HST1"));
	CHECK(!IsExclusiveForeignOverlay("HUMANS_ZDF_MOUNT_TEST"));   // nasze nigdy
	std::vector<std::string> model = { "HUMANS_1HST1", "HUMANS_MOUNT", "HUMANS_ZDF_W3_LOCO" };
	CHECK(HasExclusiveForeignOverlay(model));
	std::vector<std::string> steam = { "HUMANS_1HST1", "HUMANS_SPRINT_REMASTER", "HUMANS_ZDF_W3_LOCO" };
	CHECK(!HasExclusiveForeignOverlay(steam));
}

// ---------- Diagnostyka hakow (Core/HookDiag.h) ----------
#include "Core/HookDiag.h"

TEST(hookdiag_decode_rel32_jump) {
	// E9 rel32: cel = adres + 5 + rel
	uint8_t b[8] = { 0xE9, 0x10, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90 };
	uint32_t target = 0, slot = 0;
	CHECK(DecodeEntryJump(b, 0x00401000, target, slot) == HOOKJMP_REL);
	CHECK(target == 0x00401015);
	uint8_t back[5] = { 0xE9, 0xF0, 0xFF, 0xFF, 0xFF };   // -16
	CHECK(DecodeEntryJump(back, 0x00401000, target, slot) == HOOKJMP_REL && target == 0x00400FF5);
}
TEST(hookdiag_decode_short_and_indirect_and_push_ret) {
	uint32_t target = 0, slot = 0;
	uint8_t s[2] = { 0xEB, 0x05 };
	CHECK(DecodeEntryJump(s, 0x1000, target, slot) == HOOKJMP_REL && target == 0x1007);
	uint8_t ind[6] = { 0xFF, 0x25, 0x78, 0x56, 0x34, 0x12 };
	CHECK(DecodeEntryJump(ind, 0x1000, target, slot) == HOOKJMP_INDIRECT && slot == 0x12345678);
	uint8_t pr[6] = { 0x68, 0xEF, 0xBE, 0xAD, 0xDE, 0xC3 };
	CHECK(DecodeEntryJump(pr, 0x1000, target, slot) == HOOKJMP_REL && target == 0xDEADBEEF);
}
TEST(hookdiag_plain_prologue_is_not_a_jump) {
	uint32_t target = 7, slot = 7;
	uint8_t p[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };   // push ebp; mov ebp,esp
	CHECK(DecodeEntryJump(p, 0x1000, target, slot) == HOOKJMP_NONE);
	uint8_t q[6] = { 0x68, 0x01, 0x02, 0x03, 0x04, 0x90 };   // push imm bez ret - nie skok
	CHECK(DecodeEntryJump(q, 0x1000, target, slot) == HOOKJMP_NONE);
}
TEST(hookdiag_decode_call_site) {
	uint32_t target = 0;
	uint8_t c[5] = { 0xE8, 0x00, 0x01, 0x00, 0x00 };
	CHECK(DecodeCallSite(c, 0x0069A0CA, target) && target == 0x0069A1CF);
	uint8_t j[5] = { 0xE9, 0x00, 0x01, 0x00, 0x00 };
	CHECK(!DecodeCallSite(j, 0x0069A0CA, target));
}
TEST(hookdiag_counts_line_lists_zero_hooks_separately) {
	const char* names[3] = { "OnDamage", "DoDie", "HitCombo" };
	unsigned counts[3] = { 12, 0, 3 };
	CHECK(HookCountsLine(names, counts, 3) == "OnDamage 12, DoDie 0, HitCombo 3 | bez wywolan: DoDie");
	unsigned all[3] = { 1, 2, 3 };
	CHECK(HookCountsLine(names, all, 3) == "OnDamage 1, DoDie 2, HitCombo 3 | bez wywolan: -");
}
TEST(hookdiag_report_marks_fire_once_each_in_order) {
	const float marks[3] = { 15.0f, 60.0f, 180.0f };
	int done = 0;
	CHECK(!HookReportDue(10.0f, done, marks, 3));
	CHECK(HookReportDue(15.0f, done, marks, 3) && done == 1);
	CHECK(!HookReportDue(20.0f, done, marks, 3));
	// po dlugiej przerwie (menu, wczytanie) jeden raport, potem kolejny znacznik
	CHECK(HookReportDue(500.0f, done, marks, 3) && done == 3);
	CHECK(!HookReportDue(1000.0f, done, marks, 3));
}

// Stub HookEngineI z Ikarusa (Kroniki Myrtany): sub esp,8; pusha; ...; popa; add esp,8; <OLDINSTR bajtow wejscia>;
// push funkcja+OLDINSTR; ret. Wejscie funkcji przed hakiem Ikarusa: nasz detour Union = E9 rel32 CC CC.
static std::vector<uint8_t> IkStub(const uint8_t* copied, uint32_t oldInstr, uint32_t funcAddr) {
	std::vector<uint8_t> s = { 0x83, 0xEC, 0x08, 0x60, 0x68, 0x44, 0x33, 0x22, 0x11, 0xE8, 0, 0, 0, 0, 0x61, 0x83, 0xC4, 0x08 };
	s.insert(s.end(), copied, copied + oldInstr);
	uint32_t ret = funcAddr + oldInstr;
	s.push_back(0x68);
	for (int i = 0; i < 4; i++)
		s.push_back((uint8_t)(ret >> (8 * i)));
	s.push_back(0xC3);
	return s;
}
TEST(hookguard_ikarus_stub_with_raw_copied_jump_gets_patched) {
	const uint32_t func = 0x006660E0, detour = 0x52161230, stubAddr = 0x0B4F0000;
	int32_t rel = (int32_t)(detour - (func + 5));
	uint8_t entry[7] = { 0xE9, 0, 0, 0, 0, 0xCC, 0xCC };
	memcpy(entry + 1, &rel, 4);
	std::vector<uint8_t> s = IkStub(entry, 7, func);
	IkarusStubPlan p = PlanIkarusStubFix(s.data(), (uint32_t)s.size(), stubAddr, func, detour);
	CHECK(p.kind == IKSTUB_PATCH);
	CHECK(p.oldInstr == 7 && p.copyOffset == 18);
	CHECK(p.jumpsTo == stubAddr + 18 + 5 + (uint32_t)rel);   // surowa kopia skacze obok
	// po wpisaniu newRel skopiowany skok trafia w detour
	CHECK(stubAddr + p.copyOffset + 5 + (uint32_t)p.newRel == detour);
	memcpy(s.data() + p.copyOffset + 1, &p.newRel, 4);
	CHECK(PlanIkarusStubFix(s.data(), (uint32_t)s.size(), stubAddr, func, detour).kind == IKSTUB_ALREADY_OK);
}
TEST(hookguard_ikarus_stub_other_cases_left_alone) {
	const uint32_t func = 0x00751AF0, detour = 0x52170000, stubAddr = 0x0C000000;
	uint8_t prolog[7] = { 0x6A, 0xFF, 0x68, 0xC3, 0x5B, 0x82, 0x00 };   // Ikarus przed Union: zwykly prolog
	std::vector<uint8_t> a = IkStub(prolog, 7, func);
	CHECK(PlanIkarusStubFix(a.data(), (uint32_t)a.size(), stubAddr, func, detour).kind == IKSTUB_NOT_JUMP);
	int32_t relOther = 0x1000;   // skok, ktory nie byl nasz
	uint8_t other[7] = { 0xE9, 0, 0, 0, 0, 0xCC, 0xCC };
	memcpy(other + 1, &relOther, 4);
	std::vector<uint8_t> b = IkStub(other, 7, func);
	CHECK(PlanIkarusStubFix(b.data(), (uint32_t)b.size(), stubAddr, func, detour).kind == IKSTUB_FOREIGN);
	// kod innego DLL (hak Union kolejnego pluginu) - bez ogona push funkcja+N; ret
	uint8_t dll[16] = { 0x55, 0x8B, 0xEC, 0x68, 0x00, 0x10, 0x00, 0x00, 0xC3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
	CHECK(PlanIkarusStubFix(dll, 16, stubAddr, func, detour).kind == IKSTUB_NOT_FOUND);
	// ogon za blisko poczatku bufora (mniej bajtow niz OLDINSTR) - nie stub
	std::vector<uint8_t> c = IkStub(other, 7, func);
	CHECK(PlanIkarusStubFix(c.data() + 19, (uint32_t)c.size() - 19, stubAddr + 19, func, detour).kind == IKSTUB_NOT_FOUND);
}
TEST(hookguard_script_penetration_index_like_km_script) {
	CHECK(ScriptPenetrationProtIndex(2) == 1);    // DAM_BLUNT
	CHECK(ScriptPenetrationProtIndex(4) == 2);    // DAM_EDGE
	CHECK(ScriptPenetrationProtIndex(32) == 5);   // DAM_MAGIC
	CHECK(ScriptPenetrationProtIndex(64) == 6);   // DAM_POINT
	CHECK(ScriptPenetrationProtIndex(6) == -1);   // mieszane - skrypt porownuje ==
	CHECK(ScriptPenetrationProtIndex(8) == -1);
}

// ---------- Persist (pamiec kikutow w aivarze) ----------
#include "Core/Persist.h"
TEST(persist_mask_valid_only_9_bits) {
	CHECK(AivarMaskValid(0) && AivarMaskValid(1) && AivarMaskValid(0x1FF));
	CHECK(!AivarMaskValid(0x200) && !AivarMaskValid(-1) && !AivarMaskValid(1000));
}
TEST(persist_aivar_names_mark_aiv_and_aivrx) {
	bool named[AIVAR_COUNT] = {};
	MarkAivarName("AIV_PARTYMEMBER", 15, named);
	MarkAivarName("AIVRX_IGNORE_FIGHT", 98, named);
	MarkAivarName("aiv_lower", 3, named);
	MarkAivarName("ATR_HITPOINTS", 0, named);       // nie aivar
	MarkAivarName("AIV_BAD", 100, named);           // poza tablica
	MarkAivarName("AIV_NEG", -1, named);
	MarkAivarName("AIV", 7, named);                 // sam przedrostek
	CHECK(named[15] && named[98] && named[3]);
	CHECK(!named[0] && !named[7] && !named[99]);
}
TEST(persist_census_counts_kinds) {
	PersistCensus c;
	PersistCount(c, 0, true);
	PersistCount(c, 1, true);       // glowa u zywego - nie nasza
	PersistCount(c, 1, false);      // glowa u zwlok - nasza
	PersistCount(c, 0x60, true);    // lydki u zywego - tez nie nasze (odcinamy przy smierci)
	PersistCount(c, 5000, true);    // licznik moda
	CHECK(c.npcs == 5 && c.masks == 3 && c.aliveMasked == 2 && c.aliveHeadless == 1 && c.foreign == 1);
}
TEST(persist_decide_off_and_named) {
	PersistCensus c;
	CHECK(DecidePersist(-1, false, c) == PersistMode::Off);
	CHECK(DecidePersist(100, false, c) == PersistMode::Off);
	CHECK(DecidePersist(95, true, c) == PersistMode::NamedByScript);
	CHECK(DecidePersist(95, false, c) == PersistMode::On);
}
TEST(persist_decide_new_balance_aivar86_is_foreign) {
	// NB: aivar 86 (bez nazwy AIV_* - uzywa go UNION_ABI.DLL) niezerowy i maly u 496 NPC, wielu zywych "bez glowy"
	PersistCensus c;
	for (int i = 0; i < 700; i++)
		PersistCount(c, i < 496 ? 1 + (i % 3) : 0, true);
	CHECK(c.masks == 496 && c.aliveHeadless > 2);
	CHECK(DecidePersist(86, false, c) == PersistMode::ForeignValues);
}
TEST(persist_decide_many_corpses_stay_on) {
	// Steam po dlugiej grze: 300 zwlok z odcieciami (takze glowy) na 900 NPC - pamiec zostaje
	PersistCensus c;
	for (int i = 0; i < 900; i++)
		PersistCount(c, i < 300 ? (i % 2 ? 1 : 0x20) : 0, i >= 300);
	CHECK(c.masks == 300 && c.aliveHeadless == 0);
	CHECK(DecidePersist(95, false, c) == PersistMode::On);
	// jeden zywy bez glowy (np. zdism cut HEAD na zywym) to jeszcze nie mod
	PersistCount(c, 1, true);
	CHECK(DecidePersist(95, false, c) == PersistMode::On);
}
TEST(persist_decide_alive_masked_or_foreign_values) {
	PersistCensus maj;            // wiekszosc swiata to zwloki z odcieciami (oczyszczony Jharkendar) - nadal nasze
	for (int i = 0; i < 50; i++)
		PersistCount(maj, i < 45 ? 0x40 : 0, false);
	CHECK(DecidePersist(90, false, maj) == PersistMode::On);
	PersistCensus alive;          // parzyste wartosci (bez bitu glowy) u zywych - mod
	for (int i = 0; i < 4; i++)
		PersistCount(alive, 2 * (i + 1), true);
	CHECK(DecidePersist(90, false, alive) == PersistMode::On);   // 4 testowe "zdism cut" na zywych - jeszcze nie
	PersistCount(alive, 2, true);
	CHECK(DecidePersist(90, false, alive) == PersistMode::ForeignValues);
	PersistCensus f;
	for (int i = 0; i < 3; i++)
		PersistCount(f, 4096 + i, true);
	CHECK(DecidePersist(90, false, f) == PersistMode::ForeignValues);
}
TEST(persist_free_aivar_list_ranges) {
	bool named[AIVAR_COUNT];
	for (bool& b : named)
		b = true;
	CHECK(FreeAivarList(named, nullptr) == "brak");
	for (int i = 81; i <= 87; i++)
		named[i] = false;
	named[99] = false;
	named[5] = false;
	named[6] = false;
	CHECK(FreeAivarList(named, nullptr) == "5,6,81-87,99");
	int used[AIVAR_COUNT] = {};
	used[86] = 496;
	used[5] = 1;
	CHECK(FreeAivarList(named, used) == "6,81-85,87,99");
}
TEST(persist_mode_names) {
	CHECK(std::string(PersistModeName(PersistMode::On)) == "wlaczona");
	CHECK(std::string(PersistModeName(PersistMode::ForeignValues)).find("mod") != std::string::npos);
}

// ---------- ProtoAnis (wyszukiwanie animacji bez SearchAni) ----------
#include "Core/ProtoAnis.h"
TEST(protoanis_aligned_lookup_never_touches_null_entries) {
	int a = 1, b = 2, c = 3;
	const void* base[4] = { &a, &b, &c, nullptr };
	const void* overlay[4] = { nullptr, &b, nullptr, nullptr };   // nakladka zmienia tylko id 1
	CHECK(ProtoAniIndex(base, 4, 2, &c) == 2);
	CHECK(ProtoAniIndex(overlay, 4, 2, &c) == -1);
	CHECK(ProtoAniIndex(overlay, 4, 1, &b) == 1);
	CHECK(ProtoAniIndex(overlay, 4, 0, &a) == -1);
	CHECK(ProtoNullCount(overlay, 4) == 3 && ProtoNullCount(base, 4) == 1);
}
TEST(protoanis_short_or_bad_arrays) {
	int a = 1, b = 2;
	const void* shortArr[2] = { &b, &a };
	CHECK(ProtoAniIndex(shortArr, 2, 7, &a) == 1);    // tablica krotsza niz id - liniowo
	CHECK(ProtoAniIndex(shortArr, 2, -1, &b) == 0);
	CHECK(ProtoAniIndex(shortArr, 2, 0, &a) == -1);   // pod id inny wpis
	CHECK(ProtoAniIndex(nullptr, 5, 0, &a) == -1);
	CHECK(ProtoAniIndex(shortArr, 0, 0, &a) == -1);
	CHECK(ProtoAniIndex(shortArr, 2, 0, nullptr) == -1);
	CHECK(ProtoNullCount(nullptr, 3) == 0);
}

// ---------- HeroGuard (korekty pozycji bohatera a zewnetrzne przestawienia, NB 18.10) ----------
#include "Core/HeroGuard.h"
TEST(heroguard_classify_immediate_caller_decides) {
	int ours[] = { HG_MOD_OURS, HG_MOD_EXE };
	int foreign[] = { HG_MOD_FOREIGN, HG_MOD_OURS };            // zTimeRewind z wlasnej petli (pod nim SHW32 / nasz stos nie liczy sie)
	int engine[] = { HG_MOD_EXE, HG_MOD_EXE, HG_MOD_SYSTEM };
	int engineViaUs[] = { HG_MOD_EXE, HG_MOD_OURS };            // nasze wywolanie funkcji silnika (AlignModelToLedge)
	int engineViaNb[] = { HG_MOD_EXE, HG_MOD_EXE, HG_MOD_FOREIGN };   // silnik pod hakiem NB (DoAI) - nadal silnik
	int sys[] = { HG_MOD_SYSTEM };
	CHECK(ClassifyHeroSet(ours, 2, false) == HG_SET_OURS);
	CHECK(ClassifyHeroSet(foreign, 2, false) == HG_SET_EXTERNAL);
	CHECK(ClassifyHeroSet(engine, 3, false) == HG_SET_ENGINE);
	CHECK(ClassifyHeroSet(engineViaUs, 2, false) == HG_SET_OURS);
	CHECK(ClassifyHeroSet(engineViaNb, 3, false) == HG_SET_ENGINE);
	CHECK(ClassifyHeroSet(sys, 1, false) == HG_SET_ENGINE);
	CHECK(ClassifyHeroSet(nullptr, 0, false) == HG_SET_ENGINE);
}
TEST(heroguard_classify_engine_teleport_functions_are_external) {
	// ResetPos (zTimeRewind HardSet), BeamTo (skrypt), konsola goto, SetIdealPosition (mob) - przestawienie, nie ruch
	int viaRewind[] = { HG_MOD_EXE, HG_MOD_FOREIGN };
	int console[] = { HG_MOD_EXE, HG_MOD_EXE };
	int oursResetPos[] = { HG_MOD_EXE, HG_MOD_OURS };   // nawet nasze wywolanie teleportu silnika = przestawienie
	CHECK(ClassifyHeroSet(viaRewind, 2, true) == HG_SET_EXTERNAL);
	CHECK(ClassifyHeroSet(console, 2, true) == HG_SET_EXTERNAL);
	CHECK(ClassifyHeroSet(oursResetPos, 2, true) == HG_SET_EXTERNAL);
	// teleport silnika liczy sie tylko jako bezposredni wywolujacy
	int deeper[] = { HG_MOD_OURS, HG_MOD_EXE };
	CHECK(ClassifyHeroSet(deeper, 2, true) == HG_SET_OURS);
}
TEST(heroguard_classify_scans_only_few_frames_for_ours) {
	int far[] = { HG_MOD_EXE, HG_MOD_EXE, HG_MOD_EXE, HG_MOD_EXE, HG_MOD_EXE, HG_MOD_OURS };   // nasz hak konsoli gleboko
	CHECK(ClassifyHeroSet(far, 6, false) == HG_SET_ENGINE);
	int near3[] = { HG_MOD_EXE, HG_MOD_SYSTEM, HG_MOD_EXE, HG_MOD_OURS };
	CHECK(ClassifyHeroSet(near3, 4, false) == HG_SET_OURS);
}
TEST(heroguard_external_tracker_window) {
	HeroExtTracker t;
	CHECK(!HeroExtRecent(t, 0.0f, 0.35f));
	HeroExtNote(t, 10.0f, "NB_UNION_PLUGIN_DIFF_02.DLL+0x9317");
	CHECK(HeroExtRecent(t, 10.0f, 0.35f));
	CHECK(HeroExtRecent(t, 10.3f, 0.35f));
	CHECK(!HeroExtRecent(t, 10.4f, 0.35f));
	CHECK(t.who == "NB_UNION_PLUGIN_DIFF_02.DLL+0x9317" && t.count == 1);
	HeroExtNote(t, 11.0f, "Gothic2.exe oCNpc::ResetPos");
	CHECK(t.count == 2 && HeroExtRecent(t, 11.1f, 0.35f));
}
TEST(heroguard_suspend_reason_frozen_or_external) {
	CHECK(HeroGuardSuspendWhy(false, false) == nullptr);
	CHECK(std::string(HeroGuardSuspendWhy(true, false)).find("zamrozony") != std::string::npos);
	CHECK(std::string(HeroGuardSuspendWhy(false, true)).find("zewnetrzne") != std::string::npos);
	CHECK(HeroModelFrozen(0.0f) && HeroModelFrozen(0.0005f) && !HeroModelFrozen(1.0f) && !HeroModelFrozen(0.5f));
}
TEST(heroguard_collision_teleport_glitch_from_logs) {
	CollTeleportParams p;
	// NB 49677.31: zadane 55 cm, kolizja zmienila o 272 cm (start skoku przy scianie jaskini)
	float from[3] = { 21209, 3246, -13343 }, req[3] = { 21200, 3250, -13290 }, to[3] = { 21313, 3300, -13582 };
	CHECK(CollTeleportGlitch(from, req, to, p));
	// zwykla kolizja: zadane 30 cm w sciane, zostaje w miejscu (kolizja zmienila 30 cm)
	float r2[3] = { 21239, 3246, -13343 }, t2[3] = { 21209, 3246, -13343 };
	CHECK(!CollTeleportGlitch(from, r2, t2, p));
	// zjazd po scianie 80 cm - ponizej progu
	float t3[3] = { 21209, 3246, -13263 };
	CHECK(!CollTeleportGlitch(from, from, t3, p));
	// skok korzenia (zadane 230 cm, kolizja 0) - nie ten straznik (zadanie ponad maxRequest)
	float r4[3] = { 21439, 3246, -13343 };
	CHECK(!CollTeleportGlitch(from, r4, r4, p));
	// kolizja odpycha 120 cm, ale wynik blisko startu (|to - from| < minJump) - nie teleport
	float r5[3] = { 21309, 3246, -13343 }, t5[3] = { 21189, 3246, -13343 };
	CHECK(!CollTeleportGlitch(from, r5, t5, p));
}
TEST(heroguard_collision_teleport_streak_limit) {
	CollTeleportParams p;
	p.maxStreak = 3;
	int streak = 0;
	CHECK(CollTeleportDecide(true, streak, p) && streak == 1);
	CHECK(CollTeleportDecide(true, streak, p) && streak == 2);
	CHECK(CollTeleportDecide(true, streak, p) && streak == 3);
	CHECK(!CollTeleportDecide(true, streak, p));   // silnik upiera sie - wynik kolizji przyjety
	CHECK(!CollTeleportDecide(false, streak, p) && streak == 0);
	CHECK(CollTeleportDecide(true, streak, p) && streak == 1);
}
TEST(heroguard_fix_undone_detects_ping_pong) {
	// NB 49677.31-.39: silnik wraca do (21313,-13582), nasza poprawka stawia (21205,-13332) - co klatke od nowa
	float from[3] = { 21313, 3300, -13582 }, to[3] = { 21205, 3300, -13332 };
	float back[3] = { 21314, 3300, -13582 };
	CHECK(TravFixUndone(from, to, back));
	float stays[3] = { 21200, 3301, -13321 };   // poprawka trzyma, dalszy lot
	CHECK(!TravFixUndone(from, to, stays));
	float half[3] = { 21270, 3300, -13480 };    // w polowie drogi z powrotem - jeszcze nie
	CHECK(!TravFixUndone(from, to, half));
	float tiny[3] = { 0, 0, 0 }, tiny2[3] = { 10, 0, 0 }, tinyCur[3] = { 0, 0, 0 };   // poprawka 10 cm - bez wniosku
	CHECK(!TravFixUndone(tiny, tiny2, tinyCur));
}
TEST(heroguard_flight_fix_only_in_own_context) {
	CHECK(TravFlightFixAllowed(true, false, false));
	CHECK(!TravFlightFixAllowed(false, false, false));   // spadanie / S_FALLDN bez skoku z biegu - bez cofania
	CHECK(!TravFlightFixAllowed(true, true, false));     // cofanie czasu / teleport
	CHECK(!TravFlightFixAllowed(true, false, true));     // poprawka cofnieta przez silnik - koniec w tym skoku
}
TEST(heroguard_settle_offsets_order_and_shape) {
	float o[TRAV_SETTLE_MAX][3];
	int n = TravSettleOffsets(0.0f, 1.0f, true, o);
	CHECK(n == 7);
	// najpierw sama pozycja, potem w gore (stopy nad gorna krawedzia kolizji vobu), potem do przodu na trzech wysokosciach
	CHECK(o[0][0] == 0.0f && o[0][1] == 0.0f && o[0][2] == 0.0f);
	CHECK(o[1][1] == 15.0f && o[2][1] == 30.0f && o[3][1] == 45.0f && o[3][2] == 0.0f);
	CHECK(o[4][2] == 25.0f && o[4][1] == 0.0f && o[6][1] == 30.0f && o[6][2] == 25.0f);
	float o2[TRAV_SETTLE_MAX][3];
	int n2 = TravSettleOffsets(1.0f, 0.0f, false, o2);   // start: tylko w gore (tor konczy sie w tym miejscu)
	CHECK(n2 == 4 && o2[3][1] == 45.0f && o2[3][0] == 0.0f);
	TravSettleOffsets(1.0f, 0.0f, true, o);
	CHECK(o[4][0] == 25.0f && o[4][2] == 0.0f);   // przod = kierunek ruchu
}
TEST(heroguard_settle_pick_first_accepted) {
	// log 58119.62: wyjscie w bieg na beczkach - kolizja przenosi o 225 cm (silnik nie przyjmuje pozycji konca)
	float shifts[] = { 225.0f, 226.0f, 4.0f, 0.0f };
	CHECK(TravSettlePick(shifts, 4, 20.0f) == 2);
	float bad[] = { 225.0f, 180.0f, 60.0f };
	CHECK(TravSettlePick(bad, 3, 20.0f) == -1);
	float okNow[] = { 3.0f };
	CHECK(TravSettlePick(okNow, 1, 20.0f) == 0);
	CHECK(TravSettlePick(nullptr, 0, 20.0f) == -1);
}
TEST(heroguard_run_exit_path_blocked_by_nearest_hit) {
	TravExitPathParams p;
	float none[3] = { -1.0f, -1.0f, -1.0f };
	CHECK(TravRunExitBlockDist(none, 3, p) < 0.0f);
	float wall[3] = { -1.0f, 85.0f, 140.0f };   // kolejna krawedz / mur 85 cm przed bohaterem na wysokosci piersi
	CHECK(TravRunExitBlockDist(wall, 3, p) == 85.0f);
	float far[3] = { 200.0f, -1.0f, 170.0f };   // dalej niz droga pierwszych krokow - wolne
	CHECK(TravRunExitBlockDist(far, 3, p) < 0.0f);
	float edge[3] = { p.need, -1.0f, -1.0f };
	CHECK(TravRunExitBlockDist(edge, 3, p) == p.need);
	CHECK(TravRunExitBlockDist(nullptr, 0, p) < 0.0f);
}
TEST(heroguard_run_exit_step_probe) {
	TravExitPathParams p;
	CHECK(TravRunExitStepOk(20.0f, 18.0f, p));    // krok 20 cm z kolizja - wynik 18 cm (zwykle tarcie)
	CHECK(!TravRunExitStepOk(20.0f, 259.0f, p));  // log 65213.97: kolizja przenosi o 259 cm
	CHECK(!TravRunExitStepOk(20.0f, 70.0f, p));
}
TEST(heroguard_chain_needs_new_alt) {
	// zaraz po koncu ruchu sam trzymany W nie startuje kolejnego (chwyt w locie, dobieg); nowe Alt albo Alt w oknie wyjscia - tak
	CHECK(TravChainBlocked(0.1f, false, 0.3f));
	CHECK(!TravChainBlocked(0.1f, true, 0.3f));
	CHECK(!TravChainBlocked(0.35f, false, 0.3f));
	CHECK(!TravChainBlocked(-1.0f, false, 0.3f));   // nie bylo ruchu
}
TEST(heroguard_move_key_mask_in_traversal) {
	CHECK(TravMaskMoveKey(true, true, false, false));    // W / S / A / D / skok w trakcie animacji - gra nie widzi
	CHECK(!TravMaskMoveKey(true, true, true, false));    // wyjscie w bieg: silnik prowadzi - W dla gry
	CHECK(!TravMaskMoveKey(true, false, false, false));  // bez przeskoku
	CHECK(!TravMaskMoveKey(false, true, false, false));  // inne klawisze (akcja, bron) bez zmian
	CHECK(!TravMaskMoveKey(true, true, false, true));    // nasz odczyt surowy (MvRawState - okno wyjscia czyta W)
}

// ---------- Aard otwiera zamki, Axii jako Urok, premia kieszonkowa (runda 6) ----------
TEST(signs_lock_rules_from_script) {
	// NB: externale runy otwierania (MOB_SETUNLOCKED_RUNE) w parserze -> zasady runy NB; Steam -> tylko zamki do wytrycha
	CHECK(LockRulesFor(true) == LOCKRULES_NB);
	CHECK(LockRulesFor(false) == LOCKRULES_PICKLOCK);
	CHECK(std::string(LockRulesName(LOCKRULES_NB)) == "runa NB");
	CHECK(std::string(LockRulesName(LOCKRULES_PICKLOCK)) == "wytrych");
}
TEST(signs_lock_decide_steam_picklock_only) {
	LockInfo pick{ true, false, true };            // zamek do wytrycha (pickLockStr "LRRL")
	LockInfo both{ true, true, true };             // klucz albo wytrych
	LockInfo keyOnly{ true, true, false };         // tylko na klucz
	LockInfo bare{ true, false, false };           // zamkniety bez klucza i wytrycha
	LockInfo open{ false, false, true };
	CHECK(LockDecide(pick, LOCKRULES_PICKLOCK) == LOCK_OPEN_PICK);
	CHECK(LockDecide(both, LOCKRULES_PICKLOCK) == LOCK_OPEN_PICK);
	CHECK(LockDecide(keyOnly, LOCKRULES_PICKLOCK) == LOCK_SKIP_KEY_ONLY);
	CHECK(LockDecide(bare, LOCKRULES_PICKLOCK) == LOCK_SKIP_NO_PICKLOCK);
	CHECK(LockDecide(open, LOCKRULES_PICKLOCK) == LOCK_SKIP_UNLOCKED);
	CHECK(LockVerdictOpens(LOCK_OPEN_PICK) && !LockVerdictOpens(LOCK_SKIP_KEY_ONLY) && !LockVerdictOpens(LOCK_SKIP_UNLOCKED));
}
TEST(signs_lock_decide_nb_rune_rules) {
	// UNION_ABI Mob_SetUnLocked_Rune (0x1016A170): zamkniety i (keyInstance == "" albo pickLockStr niepusty) -> SetLocked(0);
	// klucz bez wytrycha -> CantUnlock ("brak klucza")
	CHECK(LockDecide(LockInfo{ true, false, true }, LOCKRULES_NB) == LOCK_OPEN_PICK);
	CHECK(LockDecide(LockInfo{ true, true, true }, LOCKRULES_NB) == LOCK_OPEN_PICK);
	CHECK(LockDecide(LockInfo{ true, false, false }, LOCKRULES_NB) == LOCK_OPEN_BARE);
	CHECK(LockDecide(LockInfo{ true, true, false }, LOCKRULES_NB) == LOCK_SKIP_KEY_ONLY);
	CHECK(LockDecide(LockInfo{ false, true, false }, LOCKRULES_NB) == LOCK_SKIP_UNLOCKED);
	CHECK(LockVerdictOpens(LOCK_OPEN_BARE));
	CHECK(std::string(LockVerdictName(LOCK_SKIP_KEY_ONLY)) == "tylko na klucz");
	CHECK(std::string(LockVerdictName(LOCK_OPEN_PICK)) == "zamek do wytrycha");
	CHECK(std::string(LockVerdictName(99)) == "?");
}
TEST(signs_axii_charm_plan_modes) {
	const int SEND_CAST = 2, SEND_STOP = 3;
	// Steam: Spell_Logic_Charm robi niepamiec i zwraca SENDCAST; Spell_Cast_Charm tylko mana - Urok wylaczony
	AxiiCharmPlan s = PlanAxiiCharm(true, SEND_CAST, SEND_STOP, true, false, true);
	CHECK(!s.immune && !s.callCast && s.trance);
	// NB z Urokiem: logika przepuszcza -> Spell_Cast_Charm (przestepstwa, przylapanie na kradziezy, questy) + trans
	AxiiCharmPlan n = PlanAxiiCharm(true, SEND_CAST, SEND_STOP, true, true, true);
	CHECK(!n.immune && n.callCast && n.trance);
	// odporny z listy SPELL_LOGIC_CHARM (mana na zapas, wiec SENDSTOP = odpornosc): bez Uroku i bez transu
	AxiiCharmPlan im = PlanAxiiCharm(true, SEND_STOP, SEND_STOP, true, true, true);
	CHECK(im.immune && !im.callCast && !im.trance);
	// Urok wylaczony: zachowanie sprzed rundy 6 (komunikat skryptu i trans tez u odpornych)
	AxiiCharmPlan old = PlanAxiiCharm(true, SEND_STOP, SEND_STOP, true, false, true);
	CHECK(!old.immune && !old.callCast && old.trance);
	// bez funkcji skryptu (mody): bez Uroku, trans jak dotad
	AxiiCharmPlan none = PlanAxiiCharm(false, 0, SEND_STOP, false, true, true);
	CHECK(!none.immune && !none.callCast && none.trance);
	AxiiCharmPlan monster = PlanAxiiCharm(true, SEND_CAST, SEND_STOP, true, true, false);   // potwor: bez transu
	CHECK(monster.callCast && !monster.trance);
}
TEST(signs_thief_bonus_seconds) {
	CHECK(ThiefBonusSeconds(60.0f, 20.0f) == 60.0f);
	CHECK(ThiefBonusSeconds(0.0f, 30.0f) == 30.0f);    // 0 = tyle co trans (AxiiTranceTime x moc)
	CHECK(ThiefBonusSeconds(0.0f, 0.0f) == 0.0f);
	CHECK(ThiefBonusSeconds(-5.0f, 12.0f) == 12.0f);
}
TEST(signs_thief_bonus_list_add_get_expire) {
	ThiefBonusList l;
	int a = 0, b = 0;
	CHECK(l.Add(&a, 1234, 25, 100.0f) == 1);
	CHECK(l.PctFor(&a, 50.0f) == 25);
	CHECK(l.PctFor(&b, 50.0f) == 0);
	CHECK(l.PctFor(&a, 100.0f) == 0);       // koniec czasu - juz bez premii
	CHECK(l.Add(&a, 1234, 30, 200.0f) == 1);   // ponowne Axii odswieza ten sam wpis
	CHECK(l.PctFor(&a, 150.0f) == 30);
	CHECK(l.Add(&b, 77, 25, 120.0f) == 2);
	CHECK(l.Prune(130.0f) == 1);              // b wygasl
	CHECK(l.Size() == 1 && l.PctFor(&a, 130.0f) == 30);
	CHECK(l.Add(&b, 77, 0, 500.0f) == 1);     // 0 % = bez wpisu
	l.Clear();
	CHECK(l.Size() == 0);
}
TEST(signs_thief_bonus_export_slots) {
	// skrypt NB (zDismember_NB_Thief.d) czyta tablice ZDF_AxiiThiefId / ZDF_AxiiThiefPct po instancji NPC
	ThiefBonusList l;
	int k[10];
	for (int i = 0; i < 10; i++)
		l.Add(&k[i], 100 + i, 20 + i, 50.0f + i);   // 10 wpisow, sloty 8 - zostaja najdluzej trwajace
	int ids[8], pcts[8];
	CHECK(l.Export(ids, pcts, 8, 10.0f) == 8);
	CHECK(ids[0] == 109 && pcts[0] == 29);
	CHECK(ids[7] == 102 && pcts[7] == 22);
	CHECK(l.Export(ids, pcts, 8, 55.5f) == 4);   // wygasle pominiete, reszta slotow wyzerowana
	CHECK(ids[3] == 106 && ids[4] == 0 && pcts[4] == 0 && pcts[7] == 0);
	ThiefBonusList same;
	int x = 0, y = 0;
	same.Add(&x, 5, 25, 90.0f);
	same.Add(&y, 5, 40, 60.0f);                 // dwa NPC jednej instancji (klony) - slot z wieksza premia
	CHECK(same.Export(ids, pcts, 8, 0.0f) == 1 && ids[0] == 5 && pcts[0] == 40);
}

// ---------- AtkTempo (tempo ciosow W3, ciecia boczne 2H) ----------
#include "Core/AtkTempo.h"
TEST(atk_slots_map_attack_names_and_ini_keys) {
	CHECK(AtkSlotOfAni("T_1HATTACKL") == ATK_1H_LEFT && AtkSlotOfAni("t_1hattackr") == ATK_1H_RIGHT);
	CHECK(AtkSlotOfAni("S_1HATTACK") == ATK_1H_COMBO && AtkSlotOfAni("S_2HATTACK") == ATK_2H_COMBO);
	CHECK(AtkSlotOfAni("T_2HATTACKL") == ATK_2H_LEFT && AtkSlotOfAni("T_2HATTACKR") == ATK_2H_RIGHT);
	CHECK(AtkSlotOfAni("T_1HATTACKMOVE") == -1 && AtkSlotOfAni("S_1HRUN") == -1 && AtkSlotOfAni("") == -1);
	CHECK(std::string(AtkSlotIniKey(ATK_1H_LEFT)) == "W3AtkSpeed1HLeft");
	CHECK(std::string(AtkSlotIniKey(ATK_2H_COMBO)) == "W3AtkSpeed2HCombo");
	CHECK(std::string(AtkSlotMenuKey(ATK_2H_RIGHT)) == "MenuW3AtkSpeed2HRight");
	CHECK(AtkSlotIs2H(ATK_2H_LEFT) && !AtkSlotIs2H(ATK_1H_COMBO) && AtkSlotIsCombo(ATK_2H_COMBO) && !AtkSlotIsCombo(ATK_1H_RIGHT));
	for (int i = 0; i < ATK_SLOT_COUNT; i++)
		CHECK(AtkSlotOfAni(AtkSlotAni(i)) == i);
}
TEST(atk_speed_clamps_and_keeps_native_marker) {
	CHECK(ClampAtkSpeed(0) == ATK_SPEED_NATIVE);
	CHECK(ClampAtkSpeed(10) == ATK_SPEED_MIN && ClampAtkSpeed(-5) == ATK_SPEED_MIN && ClampAtkSpeed(999) == ATK_SPEED_MAX);
	CHECK(ClampAtkSpeed(130) == 130);
	const std::vector<int>& v = AtkSpeedMenuValues();
	CHECK(v.size() == 13 && v[0] == ATK_SPEED_NATIVE && v[4] == 100 && v.back() == 200);
	CHECK(MenuIndexForValue(v, 100) == 4 && MenuIndexForValue(v, 0) == 0 && MenuIndexForValue(v, 115) == 5);
}
TEST(atk_event_ints_skip_names_and_separators) {
	CHECK((ParseEventInts("4 37 74 110") == std::vector<int>{ 4, 37, 74, 110 }));
	CHECK((ParseEventInts(" 12\t26,35 ") == std::vector<int>{ 12, 26, 35 }));
	CHECK(ParseEventInts("ZS_RIGHTHAND").empty() && ParseEventInts("").empty());
	AtkEvents e;
	AtkEventsAddTag(e, "DEF_HIT_LIMB", "ZS_RIGHTHAND");
	AtkEventsAddTag(e, "def_opt_frame", "10");
	AtkEventsAddTag(e, "DEF_HIT_END", "29");
	AtkEventsAddTag(e, "DEF_WINDOW", "13 29");
	CHECK(e.opt.size() == 1 && e.opt[0] == 10 && e.end[0] == 29 && e.win.size() == 2 && e.win[0] == 13);
}
static AtkEvents Ev(std::vector<int> opt, std::vector<int> end, std::vector<int> win, int frames, float fps = 25.0f) {
	AtkEvents e;
	e.opt = opt; e.end = end; e.win = win; e.frames = frames; e.fps = fps;
	return e;
}
TEST(atk_pace_side_attack_is_window_start_like_engine) {
	// HitCombo 0x6B0260: wcisniecie w oknie -> SetActFrame(koniec okna - 1) -> koniec ciosu; kolejny cios od poczatku okna
	AtkEvents w3 = Ev({ 10 }, { 29 }, { 13, 29 }, 29), st2 = Ev({ 3 }, { 19 }, { 8, 19 }, 19);
	CHECK(fabsf(AtkPaceSeconds(w3, false) - 0.52f) < 1e-4f && fabsf(AtkPaceSeconds(st2, false) - 0.32f) < 1e-4f);
	CHECK(fabsf(AtkFirstHitSeconds(w3) - 0.40f) < 1e-4f);
	CHECK(fabsf(AtkPaceSeconds(Ev({ 5 }, { 20 }, {}, 22), false) - 0.80f) < 1e-4f);   // bez okna - koniec ciosu
	CHECK(fabsf(AtkPaceSeconds(Ev({}, {}, {}, 25, 50.0f), false) - 0.50f) < 1e-4f);    // bez zdarzen - dlugosc
	CHECK(AtkPaceSeconds(Ev({}, {}, {}, 0), false) < 0.0f && AtkFirstHitSeconds(Ev({}, {}, {}, 10)) < 0.0f);
	CHECK(fabsf(AtkPaceSeconds(Ev({ 10 }, { 29 }, { 13, 29 }, 29, 50.0f), false) - 0.26f) < 1e-4f);   // fps z animacji
}
TEST(atk_pace_combo_is_mean_engine_gap) {
	// W3 1H: OPT 10 33 56 79, WIN 12-26 35-49 58-72 -> odstepy (12-10)+(33-25) = 10 kl. (log gry: min 0,39 s)
	AtkEvents w3 = Ev({ 10, 33, 56, 79 }, { 26, 49, 72, 98 }, { 12, 26, 35, 49, 58, 72, 81, 98 }, 98);
	CHECK((AtkComboGaps(w3) == std::vector<int>{ 10, 10, 10 }));
	CHECK(fabsf(AtkPaceSeconds(w3, true) - 0.40f) < 1e-4f);
	// natywne HUMANS_1HST2: 0,40 / 0,60 / 0,64 s
	AtkEvents st2 = Ev({ 4, 36, 73, 107 }, { 28, 60, 92, 113 }, { 10, 33, 42, 65, 78, 97, 110, 113 }, 114);
	CHECK((AtkComboGaps(st2) == std::vector<int>{ 10, 15, 16 }));
	CHECK(fabsf(AtkPaceSeconds(st2, true) - (41.0f / 3.0f) / 25.0f) < 1e-4f);
	// jedno trafienie - jak cios pojedynczy
	CHECK(fabsf(AtkPaceSeconds(Ev({ 6 }, { 21 }, { 12, 26 }, 40), true) - 0.48f) < 1e-4f);
	// okno krotsze niz trafienia (zEBA 2HST1: 3 trafienia, 2 okna) - tylko pelne pary
	CHECK(AtkComboGaps(Ev({ 6, 46, 86 }, { 35, 75, 100 }, { 13, 40, 53, 80 }, 116)).size() == 2);
}
TEST(atk_tempo_scale_percent_and_native_match) {
	CHECK(fabsf(AtkTempoScale(100, 0.52f, 0.32f) - 1.0f) < 1e-6f);
	CHECK(fabsf(AtkTempoScale(150, 0.52f, 0.32f) - 1.5f) < 1e-6f);
	CHECK(fabsf(AtkTempoScale(20, -1, -1) - 0.5f) < 1e-6f);                       // przyciete do 50%
	CHECK(fabsf(AtkTempoScale(ATK_SPEED_NATIVE, 0.52f, 0.32f) - 1.625f) < 1e-5f); // W3 1H vs talent 1HST2
	CHECK(fabsf(AtkTempoScale(ATK_SPEED_NATIVE, 0.60f, 0.56f) - 0.60f / 0.56f) < 1e-5f);
	CHECK(fabsf(AtkTempoScale(ATK_SPEED_NATIVE, 0.40f, 0.547f) - 0.40f / 0.547f) < 1e-5f);   // kombo: wolniej
	CHECK(AtkTempoScale(ATK_SPEED_NATIVE, 0.52f, -1.0f) == 1.0f && AtkTempoScale(ATK_SPEED_NATIVE, 0.0f, 0.3f) == 1.0f);
	CHECK(AtkTempoScale(ATK_SPEED_NATIVE, 2.0f, 0.1f) == ATK_MATCH_MAX && AtkTempoScale(ATK_SPEED_NATIVE, 0.1f, 2.0f) == ATK_MATCH_MIN);
}
TEST(atk_fps_target_from_base_without_drift) {
	float t = 0.0f;
	CHECK(AtkFpsTarget(25.0f, 1.5f, 25.0f, t) && fabsf(t - 37.5f) < 1e-5f);
	CHECK(!AtkFpsTarget(25.0f, 1.5f, 37.5f, t));                 // juz ustawione - bez zapisu
	float cur = 25.0f;
	for (int i = 0; i < 100; i++)                                 // wielokrotne zastosowanie nie kumuluje skali
		if (AtkFpsTarget(25.0f, 1.2f, cur, t))
			cur = t;
	CHECK(fabsf(cur - 30.0f) < 1e-4f);
	CHECK(AtkFpsTarget(25.0f, 1.0f, cur, t) && fabsf(t - 25.0f) < 1e-6f);   // powrot do bazy
	CHECK(!AtkFpsTarget(0.0f, 1.5f, 25.0f, t) && t == 25.0f);    // bez bazy - nic
}
TEST(atk_side2h_variant_fallbacks) {
	CHECK(ClampSide2H(-1) == SIDE2H_W3 && ClampSide2H(3) == SIDE2H_W3 && ClampSide2H(2) == SIDE2H_NATIVE);
	CHECK(Side2HEffective(SIDE2H_FAST, false) == SIDE2H_W3 && Side2HEffective(SIDE2H_FAST, true) == SIDE2H_FAST);
	CHECK(Side2HEffective(SIDE2H_NATIVE, false) == SIDE2H_NATIVE && Side2HEffective(7, true) == SIDE2H_W3);
	CHECK(std::string(Side2HLabel(SIDE2H_NATIVE)).find("natywne") == 0);
}
TEST(atk_side_entry_tracks_original_and_reloaded_prototype) {
	int a = 0, fast = 0, fresh = 0;
	SideEntry s;
	CHECK(SideEntryObserve(s, &a) && s.orig == &a && !s.changed);
	CHECK(SideEntryWant(s, SIDE2H_W3, &fast) == &a);
	CHECK(SideEntryWant(s, SIDE2H_FAST, &fast) == &fast && SideEntryWant(s, SIDE2H_FAST, nullptr) == &a);
	CHECK(SideEntryWant(s, SIDE2H_NATIVE, &fast) == nullptr);
	SideEntryWrote(s, nullptr);                                   // natywne: pusty wpis
	CHECK(s.changed && s.placed == nullptr);
	CHECK(!SideEntryObserve(s, nullptr) && s.orig == &a);          // nasz pusty wpis - oryginal zapamietany
	SideEntryWrote(s, &fast);
	CHECK(!SideEntryObserve(s, &fast) && s.orig == &a && s.changed);
	SideEntryWrote(s, &a);                                        // powrot do pliku
	CHECK(!s.changed && !SideEntryObserve(s, &a));
	SideEntryWrote(s, &fast);
	CHECK(SideEntryObserve(s, &fresh) && s.orig == &fresh && !s.changed);   // obcy wpis = prototyp od nowa
	SideEntry n;
	CHECK(SideEntryObserve(n, nullptr) && n.orig == nullptr && !SideEntryObserve(n, nullptr));
}

// ---------- Dwuklik kierunku, unik z trzymana akcja ----------
static int Tap(DoubleTap& s, int dir, float dt, float window = 0.25f, bool armed = true) {
	bool down[4] = { false, false, false, false };
	if (dir >= 0)
		down[dir] = true;
	return DoubleTapStep(s, down, dt, window, armed);
}
TEST(double_tap_same_direction_in_window_fires_once) {
	DoubleTap s;
	CHECK(Tap(s, MOVE_LEFT, 0.016f) < 0);          // pierwsze wcisniecie
	CHECK(Tap(s, -1, 0.08f) < 0);                  // puszczone
	CHECK(Tap(s, MOVE_LEFT, 0.08f) == MOVE_LEFT);  // drugie po 0,16 s
	CHECK(Tap(s, MOVE_LEFT, 0.016f) < 0);          // trzymane - bez powtorki
	CHECK(Tap(s, -1, 0.05f) < 0);
	CHECK(Tap(s, MOVE_LEFT, 0.05f) < 0);           // trzecie = nowe pierwsze, nie kolejny unik
}
TEST(double_tap_too_slow_or_other_direction_does_not_fire) {
	DoubleTap s;
	Tap(s, MOVE_BACK, 0.016f);
	Tap(s, -1, 0.2f);
	CHECK(Tap(s, MOVE_BACK, 0.1f) < 0);            // 0,3 s > okno 0,25 s
	DoubleTap o;
	Tap(o, MOVE_LEFT, 0.016f);
	Tap(o, -1, 0.05f);
	CHECK(Tap(o, MOVE_RIGHT, 0.05f) < 0);          // inny kierunek
	Tap(o, -1, 0.05f);
	CHECK(Tap(o, MOVE_RIGHT, 0.05f) == MOVE_RIGHT); // ale prawo dwa razy - tak
}
TEST(double_tap_disarmed_forgets_first_press_and_held_key_is_no_tap) {
	DoubleTap s;
	Tap(s, MOVE_FORWARD, 0.016f, 0.25f, false);    // bez broni / bez Shiftu
	Tap(s, -1, 0.05f, 0.25f, true);
	CHECK(Tap(s, MOVE_FORWARD, 0.05f) < 0);        // pierwsze wcisniecie bylo nieuzbrojone
	DoubleTap h;
	Tap(h, MOVE_FORWARD, 0.016f, 0.25f, false);    // trzymane przy uzbrojeniu
	CHECK(Tap(h, MOVE_FORWARD, 0.05f) < 0);        // trzymanie to nie zbocze
	Tap(h, -1, 0.05f);
	CHECK(Tap(h, MOVE_FORWARD, 0.05f) < 0);        // pierwsze zbocze po uzbrojeniu
	Tap(h, -1, 0.05f);
	CHECK(Tap(h, MOVE_FORWARD, 0.05f) == MOVE_FORWARD);
}
TEST(double_tap_two_directions_in_one_frame_cancels) {
	DoubleTap s;
	Tap(s, MOVE_LEFT, 0.016f);
	Tap(s, -1, 0.05f);
	bool both[4] = { false, false, true, true };
	CHECK(DoubleTapStep(s, both, 0.05f, 0.25f, true) < 0);
}
TEST(attack_hold_idle_only_in_ready_stance) {
	CHECK(AttackHoldIdle({ "S_1HATTACK" }));
	CHECK(AttackHoldIdle({ "S_FISTATTACK", "S_1HRUN" }));
	CHECK(AttackHoldIdle({ "s_2hattack" }));
	CHECK(!AttackHoldIdle({ "S_1HATTACK", "T_1HATTACKL" }));       // cios w toku
	CHECK(!AttackHoldIdle({ "S_2HATTACK", "T_2HATTACKMOVE" }));
	CHECK(!AttackHoldIdle({ "S_1HRUN" }));                         // bez postawy ataku
	CHECK(!AttackHoldIdle({}));
	CHECK(AttackHoldIdle({ "S_1HATTACK", "T_1HRUN_2_1HATTACK" })); // przejscie do postawy to nie cios
}

// ---------- Podpowiedz przeskoku (TraversalHint, 21.10) ----------
TEST(trav_hint_only_when_alt_would_start_and_budget_left) {
	TravHint h;
	h.left = 3;
	CHECK(TravHintDue(h, true, true, TRAVSTART_NOW, 20.0f));
	CHECK(TravHintDue(h, true, true, TRAVSTART_WAIT, 20.0f));       // dobieg - Alt teraz tez zadziala
	CHECK(!TravHintDue(h, true, true, TRAVSTART_TOO_FAR, 20.0f));
	CHECK(!TravHintDue(h, true, true, TRAVSTART_TOO_CLOSE, 20.0f));
	CHECK(!TravHintDue(h, true, false, TRAVSTART_NOW, 20.0f));       // brak klipu
	CHECK(!TravHintDue(h, false, true, TRAVSTART_NOW, 20.0f));       // bohater nie moze zaczac (bez W, walka dystansowa)
	h.left = 0;
	CHECK(!TravHintDue(h, true, true, TRAVSTART_NOW, 20.0f));        // limit wyczerpany
	h.left = 2;
	h.since = 5.0f;
	CHECK(!TravHintDue(h, true, true, TRAVSTART_NOW, 20.0f));        // przerwa od ostatniej podpowiedzi
	h.since = 25.0f;
	CHECK(TravHintDue(h, true, true, TRAVSTART_NOW, 20.0f));
}
TEST(dik_label_for_hints) {
	CHECK(DikLabel(0x38) == "Alt");
	CHECK(DikLabel(0xB8) == "prawy Alt");
	CHECK(DikLabel(0x39) == "Spacja");
	CHECK(DikLabel(0x2E) == "C");
	CHECK(DikLabel(0x10) == "Q");
	CHECK(DikLabel(0x02) == "1" && DikLabel(0x0B) == "0");
	CHECK(DikLabel(0x2A) == "Shift");
	CHECK(DikLabel(0x1D) == "Ctrl");
	CHECK(DikLabel(0x3A) == "CapsLock");
	CHECK(DikLabel(0x58) == "F12");          // reszta jak KeyLabel
	CHECK(DikLabel(0x45) == KeyLabel(0x45));
}

// ---------- Okno listy (Core/MenuList, 21.10) ----------
#include "Core/MenuList.h"
#include "Core/Meditate.h"

static MenuList TestMenu() {
	MenuList m;
	MenuRow a; a.id = 1; a.kind = MROW_CHOICE; a.label = "Czekaj"; a.values = { "1 h", "2 h", "3 h" }; a.activates = true;
	MenuRow t; t.id = 2; t.kind = MROW_TEXT; t.label = "---";
	MenuRow b; b.id = 3; b.kind = MROW_CHOICE; b.label = "Regeneracja"; b.values = { "brak", "stopniowa", "jak sen" }; b.wrap = true;
	MenuRow h; h.id = 4; h.kind = MROW_CHOICE; h.label = "Dar"; h.values = { "auto", "Innos" }; h.visible = false;
	MenuRow off; off.id = 5; off.label = "Wylaczony"; off.enabled = false;
	MenuRow r; r.id = 6; r.label = "Wstan";
	m.rows = { a, t, b, h, off, r };
	return m;
}

TEST(menu_keys_from_dik) {
	CHECK(MenuKeyFromDik(0xC8) == MKEY_UP && MenuKeyFromDik(0x11) == MKEY_UP && MenuKeyFromDik(0x48) == MKEY_UP);
	CHECK(MenuKeyFromDik(0xD0) == MKEY_DOWN && MenuKeyFromDik(0x1F) == MKEY_DOWN);
	CHECK(MenuKeyFromDik(0xCB) == MKEY_LEFT && MenuKeyFromDik(0x1E) == MKEY_LEFT);
	CHECK(MenuKeyFromDik(0xCD) == MKEY_RIGHT && MenuKeyFromDik(0x20) == MKEY_RIGHT);
	CHECK(MenuKeyFromDik(0x1C) == MKEY_ENTER && MenuKeyFromDik(0x9C) == MKEY_ENTER);
	CHECK(MenuKeyFromDik(0x01) == MKEY_ESC && MenuKeyFromDik(0x0E) == MKEY_ESC);
	CHECK(MenuKeyFromDik(0x33) == MKEY_NONE);    // przecinek
	CHECK(MenuKeyFromDik(0x39) == MKEY_NONE);    // spacja
}
TEST(menu_navigation_skips_hidden_disabled_and_text_rows_and_wraps) {
	MenuList m = TestMenu();
	MenuFixCursor(m);
	CHECK(m.cursor == 0);
	MenuEvent e = MenuHandleKey(m, MKEY_DOWN);
	CHECK(e.type == MEV_MOVED && e.rowId == 3 && m.cursor == 2);   // tekst pominiety
	e = MenuHandleKey(m, MKEY_DOWN);
	CHECK(e.type == MEV_MOVED && e.rowId == 6);                    // ukryty i wylaczony pominiete
	e = MenuHandleKey(m, MKEY_DOWN);
	CHECK(e.rowId == 1 && m.cursor == 0);                          // w kolko
	e = MenuHandleKey(m, MKEY_UP);
	CHECK(e.rowId == 6);
	MenuHandleKey(m, MKEY_HOME);
	CHECK(m.cursor == 0);
	MenuHandleKey(m, MKEY_END);
	CHECK(m.cursor == 5);
}
TEST(menu_choice_clamps_or_wraps_and_enter_activates) {
	MenuList m = TestMenu();
	MenuFixCursor(m);
	CHECK(MenuHandleKey(m, MKEY_LEFT).type == MEV_NONE);           // pierwsza wartosc, bez zawijania
	MenuEvent e = MenuHandleKey(m, MKEY_RIGHT);
	CHECK(e.type == MEV_CHANGED && e.rowId == 1 && e.value == 1);
	MenuHandleKey(m, MKEY_RIGHT);
	CHECK(MenuHandleKey(m, MKEY_RIGHT).type == MEV_NONE && m.rows[0].value == 2);
	e = MenuHandleKey(m, MKEY_ENTER);
	CHECK(e.type == MEV_ACTIVATE && e.rowId == 1 && e.value == 2);
	MenuHandleKey(m, MKEY_DOWN);                                    // Regeneracja (zawija, bez Enter)
	e = MenuHandleKey(m, MKEY_LEFT);
	CHECK(e.type == MEV_CHANGED && e.value == 2);
	CHECK(MenuHandleKey(m, MKEY_ENTER).type == MEV_NONE);
	MenuHandleKey(m, MKEY_DOWN);
	e = MenuHandleKey(m, MKEY_ENTER);
	CHECK(e.type == MEV_ACTIVATE && e.rowId == 6);
	CHECK(MenuHandleKey(m, MKEY_ESC).type == MEV_CLOSE);
}
TEST(menu_fix_cursor_moves_off_hidden_row) {
	MenuList m = TestMenu();
	m.cursor = 3;                                                   // ukryty
	MenuFixCursor(m);
	CHECK(m.cursor == 5);
	m.rows[5].visible = false;
	m.cursor = 4;
	MenuFixCursor(m);
	CHECK(m.cursor == 2);                                           // w gore, gdy w dol nic
	CHECK(MenuSelectRow(m, 6) == false);
	CHECK(MenuSelectRow(m, 1) && m.cursor == 0);
	MenuList empty;
	MenuFixCursor(empty);
	CHECK(empty.cursor == -1);
	CHECK(MenuHandleKey(empty, MKEY_DOWN).type == MEV_NONE);
}
TEST(menu_value_text_shows_possible_directions) {
	MenuRow r; r.kind = MROW_CHOICE; r.values = { "x5", "x10", "x15" };
	CHECK(MenuValueText(r) == "  x5 >");
	r.value = 1;
	CHECK(MenuValueText(r) == "< x10 >");
	r.value = 2;
	CHECK(MenuValueText(r) == "< x15  ");
	r.wrap = true;
	CHECK(MenuValueText(r) == "< x15 >");
	MenuRow a; a.label = "Wstan";
	CHECK(MenuValueText(a).empty());
}
TEST(menu_visible_rows_scroll_with_cursor) {
	MenuList m;
	for (int i = 0; i < 10; i++) {
		MenuRow r; r.id = i; r.label = "r";
		r.visible = i != 3;
		m.rows.push_back(r);
	}
	m.cursor = 0;
	std::vector<int> v = MenuVisibleRows(m, 4);
	CHECK(v.size() == 4 && v[0] == 0 && v[3] == 4);                // ukryty 3 pominiety
	m.cursor = 8;
	v = MenuVisibleRows(m, 4);
	CHECK(v.size() == 4 && v.back() == 8 && m.first == 4);
	m.cursor = 1;
	v = MenuVisibleRows(m, 4);
	CHECK(v.front() == 1);
	CHECK(MenuVisibleRows(m, 0).empty());
}
TEST(menu_repeat_after_delay_then_rate) {
	MenuRepeat r;
	MenuRepeatPress(r, MKEY_ENTER);
	CHECK(r.key == MKEY_NONE);                                      // Enter sie nie powtarza
	MenuRepeatPress(r, MKEY_DOWN);
	CHECK(MenuRepeatStep(r, 0.2f) == 0);
	CHECK(MenuRepeatStep(r, 0.25f) == 1);                           // 0,45 s
	CHECK(MenuRepeatStep(r, 0.05f) == 1);                           // 0,50 >= 0,48
	CHECK(MenuRepeatStep(r, 0.02f) == 0);
	CHECK(MenuRepeatStep(r, 5.0f) <= 8);                            // przycinka - bez serii
	MenuRepeatRelease(r, MKEY_UP);
	CHECK(r.key == MKEY_DOWN);
	MenuRepeatRelease(r, MKEY_DOWN);
	CHECK(r.key == MKEY_NONE && MenuRepeatStep(r, 1.0f) == 0);
}

// ---------- Medytacja W3 (Core/Meditate, 21.10) ----------
TEST(med_game_minutes_and_clock) {
	CHECK(std::fabs(MedGameMinutes(0, 0.0)) < 1e-9);
	CHECK(std::fabs(MedGameMinutes(2, 250000.0 * 14.5) - (2 * 1440 + 14 * 60 + 30)) < 1e-6);
	int d, h, m;
	MedSplitMinutes(3 * 1440 + 22 * 60 + 7.9, d, h, m);
	CHECK(d == 3 && h == 22 && m == 7);
	CHECK(MedClockText(5 * 60 + 3) == "5:03");
	CHECK(MedClockText(1440 + 23 * 60 + 59) == "23:59");
	MedSplitMinutes(-5.0, d, h, m);
	CHECK(d == 0 && h == 0 && m == 0);
}
TEST(med_target_hours_and_until) {
	double now = 1440 + 14 * 60 + 35;                              // dzien 1, 14:35
	CHECK(std::fabs(MedTargetAfterHours(now, 3) - (now + 180)) < 1e-9);
	CHECK(std::fabs(MedTargetUntilHour(now, 18) - (1440 + 18 * 60)) < 1e-9);   // dzis wieczorem
	CHECK(std::fabs(MedTargetUntilHour(now, 6) - (2 * 1440 + 6 * 60)) < 1e-9);  // jutro rano
	CHECK(std::fabs(MedTargetUntilHour(now, 0) - (2 * 1440)) < 1e-9);           // polnoc = poczatek jutra
	CHECK(std::fabs(MedTargetUntilHour(1440 + 6 * 60 + 0.5, 6) - (2 * 1440 + 6 * 60)) < 1e-9);   // 6:00:30 - jutro
	CHECK(std::fabs(MedTargetUntilHour(1440 + 5 * 60 + 58.5, 6) - (1440 + 6 * 60)) < 1e-9);      // 1,5 min przed
	CHECK(MedUntilHour(MED_UNTIL_MORNING) == 6 && MedUntilHour(MED_UNTIL_NOON) == 12 &&
		MedUntilHour(MED_UNTIL_EVENING) == 18 && MedUntilHour(MED_UNTIL_MIDNIGHT) == 0);
}
TEST(med_speed_values_and_clamp) {
	const std::vector<int>& v = MedSpeedValues();
	CHECK(v.size() == 11 && v.front() == 5 && v.back() == 100);
	CHECK(MedClampSpeed(1) == 5 && MedClampSpeed(150) == 100 && MedClampSpeed(20) == 20 && MedClampSpeed(99) == 99);
	CHECK(v[MedSpeedIndex(20)] == 20 && v[MedSpeedIndex(22)] == 20 && v[MedSpeedIndex(100)] == 100 && v[MedSpeedIndex(70)] == 75);
}
TEST(med_clock_top_up_fills_engine_cap_without_crossing_midnight) {
	// tempo 20, klatka 16,7 ms: chciane 333 ms, silnik dodal 100 (przyciecie) - dokladamy 233
	double e = MedClockTopUp(20.0, 16.7, 100.0, 1000.0, MED_TICKS_PER_DAY, 1e9);
	CHECK(std::fabs(e - (20.0 * 16.7 - 100.0)) < 1e-6);
	CHECK(MedClockTopUp(20.0, 4.0, 80.0, 1000.0, MED_TICKS_PER_DAY, 1e9) == 0.0);   // silnik wystarczyl
	CHECK(MedClockTopUp(20.0, 16.7, 100.0, MED_TICKS_PER_DAY - 50.0, MED_TICKS_PER_DAY, 1e9) == 49.0);   // do polnocy - 1
	CHECK(MedClockTopUp(20.0, 16.7, 100.0, 1000.0, MED_TICKS_PER_DAY, 10.0) == 10.0);   // nie za cel
	CHECK(MedClockTopUp(0.0, 16.7, 0.0, 0.0, MED_TICKS_PER_DAY, 1e9) == 0.0);
	CHECK(MedClockTopUp(20.0, 16.7, 100.0, 1000.0, MED_TICKS_PER_DAY, -5.0) == 0.0);
}
TEST(med_hero_time_scale) {
	CHECK(std::fabs(MedHeroTimeScale(16.7, 100.0) - 0.167f) < 1e-4);
	CHECK(MedHeroTimeScale(16.7, 10.0) == 1.0f);
	CHECK(MedHeroTimeScale(1.0, 1000.0) == 0.02f);
	CHECK(MedHeroTimeScale(16.7, 0.0) == 1.0f);
}
TEST(med_regen_gradual_full_after_regen_hours) {
	MedRegenAcc acc;
	int hp = 10, mana = 0, total = 0;
	// 8 h po 0,01 h: od 10/250 do pelnego
	for (int i = 0; i < 800; i++) {
		int ah, am;
		MedRegenStep(MED_REGEN_GRADUAL, 0.01, 8.0, hp, 250, mana, 40, acc, ah, am);
		hp += ah;
		mana += am;
		total += ah;
	}
	CHECK(hp == 250 && mana == 40 && total == 240);
	// 2 h z 0 = cwierc
	MedRegenAcc a2;
	int h2 = 0, ah, am;
	for (int i = 0; i < 200; i++) {
		MedRegenStep(MED_REGEN_GRADUAL, 0.01, 8.0, h2, 200, 0, 0, a2, ah, am);
		h2 += ah;
		CHECK(am == 0);
	}
	CHECK(h2 >= 49 && h2 <= 50);
	MedRegenStep(MED_REGEN_NONE, 5.0, 8.0, 0, 100, 0, 100, a2, ah, am);
	CHECK(ah == 0 && am == 0);
	MedRegenStep(MED_REGEN_SLEEP, 5.0, 8.0, 0, 100, 0, 100, a2, ah, am);
	CHECK(ah == 0 && am == 0);
	MedRegenStep(MED_REGEN_GRADUAL, -1.0, 8.0, 0, 100, 0, 100, a2, ah, am);
	CHECK(ah == 0 && am == 0);
}
TEST(med_sleep_restores_only_after_sleep_hours) {
	CHECK(MedSleepRestores(MED_REGEN_SLEEP, 6.0, 6.0));
	CHECK(MedSleepRestores(MED_REGEN_SLEEP, 8.5, 6.0));
	CHECK(!MedSleepRestores(MED_REGEN_SLEEP, 5.9, 6.0));
	CHECK(!MedSleepRestores(MED_REGEN_GRADUAL, 9.0, 6.0));
	CHECK(!MedSleepRestores(MED_REGEN_NONE, 9.0, 6.0));
}
TEST(med_start_deny_order) {
	MedHeroState s;
	CHECK(MedStartDeny(s) == MDENY_OK);
	s.enemy = true;
	CHECK(MedStartDeny(s) == MDENY_ENEMY);
	s.water = true;
	CHECK(MedStartDeny(s) == MDENY_WATER);
	s.window = true;
	CHECK(MedStartDeny(s) == MDENY_WINDOW);
	s.anis = false;
	CHECK(MedStartDeny(s) == MDENY_NO_ANIS);
	s.enabled = false;
	CHECK(MedStartDeny(s) == MDENY_OFF);
	CHECK(MedDenyScreen(MDENY_ENEMY) != nullptr && MedDenyScreen(MDENY_WINDOW) == nullptr && MedDenyScreen(MDENY_OFF) == nullptr);
	CHECK(MedDenyScreen(MDENY_MOVLOCK) == nullptr && MedDenyScreen(MDENY_BUSY) != nullptr);
}
TEST(med_machine_full_cycle_with_wait) {
	MedMachine m;
	MedSense s;
	s.now = 600.0;
	MedStep st = MedBegin(m, false);
	CHECK(st.act == MACT_KNEEL && m.state == MED_KNEEL);
	s.startPlaying = true;
	CHECK(MedUpdate(m, s, 0.5f).act == MACT_NONE);
	s.startPlaying = false;
	s.loopPlaying = true;
	st = MedUpdate(m, s, 0.5f);
	CHECK(st.act == MACT_OPEN_MENU && m.state == MED_MENU);
	CHECK(!MedStartWait(m, 600.0, 600.0));                            // cel nie pozniej
	CHECK(MedStartWait(m, 600.0, 780.0) && m.state == MED_WAIT);
	s.now = 700.0;
	CHECK(MedUpdate(m, s, 0.1f).act == MACT_NONE);
	CHECK(std::fabs(MedWaitProgress(m, 700.0) - 100.0 / 180.0) < 1e-9);
	s.now = 780.0;
	st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_WAIT_END && st.why == MSTOP_TARGET && st.from == MED_WAIT && m.state == MED_MENU);
	st = MedRise(m, MSTOP_MENU);
	CHECK(st.act == MACT_RISE && m.state == MED_RISE);
	s.loopPlaying = false;
	st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_NONE);                                       // 0,1 s - wstawanie moze jeszcze nie grac
	s.stopPlaying = true;
	CHECK(MedUpdate(m, s, 1.0f).act == MACT_NONE);
	s.stopPlaying = false;
	st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_FINISH && st.why == MSTOP_MENU && m.state == MED_IDLE);
	CHECK(!MedActive(m));
}
TEST(med_machine_sheathe_then_kneel_or_timeout) {
	MedMachine m;
	MedSense s;
	s.weaponOut = true;
	CHECK(MedBegin(m, true).act == MACT_NONE && m.state == MED_SHEATHE);
	CHECK(MedUpdate(m, s, 1.0f).act == MACT_NONE);
	s.weaponOut = false;
	CHECK(MedUpdate(m, s, 0.1f).act == MACT_KNEEL && m.state == MED_KNEEL);
	MedMachine m2;
	MedBegin(m2, true);
	s.weaponOut = true;
	MedUpdate(m2, s, 2.0f);
	MedStep st = MedUpdate(m2, s, 1.5f);
	CHECK(st.act == MACT_ABORT && st.why == MSTOP_SHEATHE && m2.state == MED_IDLE);
}
TEST(med_machine_wait_interrupts) {
	auto waiting = [](MedMachine& m, MedSense& s) {
		MedBegin(m, false);
		s = MedSense();
		s.now = 100.0;
		s.loopPlaying = true;
		MedUpdate(m, s, 0.1f);
		MedStartWait(m, 100.0, 400.0);
	};
	MedMachine m;
	MedSense s;
	waiting(m, s);
	s.cancelKey = true;
	MedStep st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_WAIT_END && st.why == MSTOP_KEY && m.state == MED_MENU);   // klawisz = z powrotem menu
	waiting(m, s);
	s.enemy = true;
	st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_RISE && st.why == MSTOP_ENEMY && st.from == MED_WAIT);     // wrog = wstaje
	waiting(m, s);
	s.hit = true;
	st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_ABORT && st.why == MSTOP_HIT && st.from == MED_WAIT);      // trafienie = od razu
	waiting(m, s);
	s.dialog = true;
	CHECK(MedUpdate(m, s, 0.1f).why == MSTOP_DIALOG);
	waiting(m, s);
	s.loopPlaying = false;
	CHECK(MedUpdate(m, s, 0.1f).act == MACT_NONE);
	st = MedUpdate(m, s, 0.3f);
	CHECK(st.act == MACT_ABORT && st.why == MSTOP_LOST);                            // gra przerwala kleczenie
}
TEST(med_machine_kneel_cancel_lost_and_abort) {
	MedMachine m;
	MedSense s;
	MedBegin(m, false);
	s.startPlaying = true;
	s.cancelKey = true;
	MedStep st = MedUpdate(m, s, 0.1f);
	CHECK(st.act == MACT_RISE && st.why == MSTOP_KEY && m.state == MED_RISE);
	MedBegin(m, false);
	s = MedSense();
	MedUpdate(m, s, 0.2f);
	st = MedUpdate(m, s, 0.2f);
	CHECK(st.act == MACT_ABORT && st.why == MSTOP_LOST);                            // klekanie nie ruszylo
	MedBegin(m, false);
	st = MedAbort(m, MSTOP_EXTERNAL);
	CHECK(st.act == MACT_ABORT && st.from == MED_KNEEL && m.state == MED_IDLE);
	CHECK(MedAbort(m, MSTOP_EXTERNAL).act == MACT_NONE);
	CHECK(MedRise(m, MSTOP_MENU).act == MACT_NONE);
	MedBegin(m, false);
	s.startPlaying = true;
	s.down = true;
	CHECK(MedUpdate(m, s, 0.1f).why == MSTOP_DOWN);
}
TEST(med_god_choices_only_with_two_patrons) {
	CHECK(MedGodChoices(1, 0, 0).empty());
	CHECK(MedGodChoices(0, 0, 0).empty());
	std::vector<int> ib = MedGodChoices(1, 2, 0);
	CHECK(ib.size() == 3 && ib[0] == MGOD_AUTO && ib[1] == MGOD_INNOS && ib[2] == MGOD_BELIAR);
	std::vector<int> all = MedGodChoices(1, 1, 1);
	CHECK(all.size() == 4 && all[1] == MGOD_INNOS && all[2] == MGOD_ADANOS && all[3] == MGOD_BELIAR);
	CHECK(MedGodChoiceIndex(ib, MGOD_BELIAR) == 2);
	CHECK(MedGodChoiceIndex(ib, MGOD_ADANOS) == 0);                                 // Adanos bez patronatu = auto
	CHECK(MedGodChoiceIndex(ib, 0) == 0);
	CHECK(std::string(MedGodLabel(MGOD_ADANOS)) == "Adanos" && std::string(MedGodLabel(7)) == "auto");
}

// ---------- Medytacja: kamera orbitalna (Core/Meditate, 21.10) ----------
static bool CamNear(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// Kroki po 1/fps przez secs sekund (bez myszy).
static void CamRun(MedCam& c, float secs, float fps = 60.0f) {
	for (int i = 0; i < (int)(secs * fps + 0.5f); i++)
		MedCamStep(c, 1.0f / fps);
}

TEST(med_cam_wrap_and_delta) {
	CHECK(CamNear(MedCamWrap(0.0f), 0.0f));
	CHECK(CamNear(MedCamWrap(190.0f), -170.0f));
	CHECK(CamNear(MedCamWrap(-190.0f), 170.0f));
	CHECK(CamNear(MedCamWrap(180.0f), 180.0f) && CamNear(MedCamWrap(-180.0f), 180.0f));
	CHECK(CamNear(MedCamWrap(540.0f), 180.0f) && CamNear(MedCamWrap(-725.0f), -5.0f));
	CHECK(CamNear(MedCamDelta(170.0f, -170.0f), 20.0f));     // przez +-180, nie -340
	CHECK(CamNear(MedCamDelta(-170.0f, 170.0f), -20.0f));
	CHECK(CamNear(MedCamDelta(10.0f, 30.0f), 20.0f));
}
TEST(med_cam_approach_exponential_with_min_speed_and_no_overshoot) {
	CHECK(CamNear(MedCamApproach(0.0f, 100.0f, 0.08f, 0.08f, 0.0f), 100.0f * (1.0f - std::exp(-1.0f)), 1e-2f));
	CHECK(CamNear(MedCamApproach(0.0f, -100.0f, 0.08f, 0.08f, 0.0f), -100.0f * (1.0f - std::exp(-1.0f)), 1e-2f));
	CHECK(MedCamApproach(0.0f, 1.0f, 0.1f, 0.25f, 40.0f) == 1.0f);           // min. predkosc 4 st. > reszta - cel
	CHECK(CamNear(MedCamApproach(0.0f, 30.0f, 0.1f, 100.0f, 40.0f), 4.0f));       // wolny wykladniczy - co najmniej 40 st./s
	CHECK(MedCamApproach(5.0f, 50.0f, 0.0f, 0.1f, 40.0f) == 5.0f);
	CHECK(MedCamApproach(5.0f, 50.0f, 1.0f, 0.0f, 0.0f) == 50.0f);             // tau 0 = od razu
}
TEST(med_cam_begin_keeps_engine_values_and_widens_elevation_range) {
	MedCam c;
	MedCamBegin(c, 10.0f, 30.0f, 10.0f);
	CHECK(c.phase == MCAM_ACTIVE && CamNear(MedCamAzi(c), 10.0f) && CamNear(c.elev, 30.0f));
	CHECK(CamNear(c.elevMin, MED_CAM_ELEV_MIN) && CamNear(c.elevMax, MED_CAM_ELEV_MAX));
	CHECK(MedCamStep(c, 1.0f / 60.0f) && CamNear(MedCamAzi(c), 10.0f) && CamNear(c.elev, 30.0f));   // bez myszy bez ruchu
	MedCamBegin(c, 200.0f, 85.0f, -5.0f);
	CHECK(CamNear(MedCamAzi(c), -160.0f) && CamNear(c.baseAzi, -5.0f) && CamNear(c.elevMax, 85.0f));   // start poza zakresem - bez skoku
	MedCamBegin(c, 0.0f, -45.0f, 0.0f);
	CHECK(CamNear(c.elevMin, -45.0f) && CamNear(c.elevMax, MED_CAM_ELEV_MAX));
}
TEST(med_cam_mouse_right_turns_view_right_and_clamps_elevation) {
	MedCam c;
	MedCamBegin(c, 0.0f, 30.0f, 0.0f);
	MedCamMouse(c, 100.0f, 0.0f, 0.15f, 1);
	CHECK(CamNear(c.aziGoal, -15.0f) && CamNear(c.turned, 15.0f));
	CamRun(c, 1.0f);
	CHECK(CamNear(MedCamAzi(c), -15.0f));
	MedCamMouse(c, 0.0f, 1000.0f, 0.15f, 1);
	CHECK(CamNear(c.elevGoal, MED_CAM_ELEV_MAX));
	MedCamMouse(c, 0.0f, 100.0f, 0.15f, -1);                                   // odwrocenie gora / dol (s_iLookAroundSgn)
	CHECK(CamNear(c.elevGoal, MED_CAM_ELEV_MAX - 15.0f));
	MedCamMouse(c, 0.0f, 100000.0f, 0.15f, -1);
	CHECK(CamNear(c.elevGoal, MED_CAM_ELEV_MIN));
	CamRun(c, 1.0f);
	CHECK(CamNear(c.elev, MED_CAM_ELEV_MIN));
	MedCam off;
	MedCamMouse(off, 100.0f, 100.0f, 0.15f, 1);
	CHECK(off.aziGoal == 0.0f && off.elevGoal == 0.0f && !MedCamStep(off, 0.016f));
}
TEST(med_cam_orbit_crosses_180_and_big_swipe_keeps_direction) {
	MedCam c;
	MedCamBegin(c, 170.0f, 30.0f, 0.0f);
	MedCamMouse(c, -200.0f, 0.0f, 0.15f, 1);                                  // +30 st. przez szew +-180
	float prev = MedCamAzi(c);
	bool forward = true;
	for (int i = 0; i < 60; i++) {
		MedCamStep(c, 1.0f / 60.0f);
		float now = MedCamAzi(c);
		forward = forward && MedCamDelta(prev, now) >= -1e-4f;
		prev = now;
	}
	CHECK(forward && CamNear(MedCamAzi(c), -160.0f, 1e-2f));
	// zamach 300 st. w jednej klatce: kamera jedzie +300, nie krotsza droga -60
	MedCamBegin(c, 0.0f, 30.0f, 0.0f);
	MedCamMouse(c, -2000.0f, 0.0f, 0.15f, 1);
	MedCamStep(c, 1.0f / 60.0f);
	CHECK(MedCamDelta(0.0f, MedCamAzi(c)) > 0.0f);
	float total = 0.0f;
	prev = MedCamAzi(c);
	for (int i = 0; i < 120; i++) {
		MedCamStep(c, 1.0f / 60.0f);
		total += MedCamDelta(prev, MedCamAzi(c));
		prev = MedCamAzi(c);
	}
	CHECK(CamNear(MedCamAzi(c), -60.0f, 1e-2f) && total > 0.0f && CamNear(c.turned, 300.0f));
	CHECK(std::fabs(c.azi) <= 180.0f && std::fabs(c.aziGoal - c.azi) < 1e-2f);   // liczby nie rosna z obrotami
}
TEST(med_cam_mouse_angle_independent_of_frame_rate) {
	// ten sam ruch myszy rozlozony na klatki 30 i 144 kl./s - ten sam kat (mysz to droga, nie predkosc)
	MedCam a, b;
	MedCamBegin(a, 0.0f, 30.0f, 0.0f);
	MedCamBegin(b, 0.0f, 30.0f, 0.0f);
	for (int i = 0; i < 30; i++) {
		MedCamMouse(a, 20.0f, -4.0f, 0.15f, 1);
		MedCamStep(a, 1.0f / 30.0f);
	}
	for (int i = 0; i < 144; i++) {
		MedCamMouse(b, 20.0f * 30.0f / 144.0f, -4.0f * 30.0f / 144.0f, 0.15f, 1);
		MedCamStep(b, 1.0f / 144.0f);
	}
	CamRun(a, 1.0f, 30.0f);
	CamRun(b, 1.0f, 144.0f);
	CHECK(CamNear(MedCamAzi(a), -90.0f, 1e-2f) && CamNear(MedCamAzi(b), -90.0f, 1e-2f));
	CHECK(CamNear(a.elev, 12.0f, 1e-2f) && CamNear(b.elev, 12.0f, 1e-2f));
}
TEST(med_cam_return_goes_short_way_to_base_and_ends_exactly) {
	MedCam c;
	MedCamBegin(c, 0.0f, 30.0f, 0.0f);
	MedCamMouse(c, -1000.0f, 0.0f, 0.15f, 1);                                 // +150
	CamRun(c, 1.0f);
	MedCamRelease(c);
	CHECK(c.phase == MCAM_RETURN && CamNear(c.aziGoal - c.azi, -150.0f, 1e-2f));
	MedCamMouse(c, -1000.0f, 0.0f, 0.15f, 1);                                 // w powrocie mysz obraca bohatera, nie kamere
	CHECK(CamNear(c.aziGoal - c.azi, -150.0f, 1e-2f));
	int frames = 0;
	bool lastWrite = false;
	while (c.phase == MCAM_RETURN && frames < 600) {
		lastWrite = MedCamStep(c, 1.0f / 60.0f);
		frames++;
	}
	CHECK(c.phase == MCAM_OFF && lastWrite && MedCamAzi(c) == 0.0f);
	CHECK(frames > 20 && frames < 90);                                         // ok. 1 s, nie skok i nie wleczenie
	CHECK(!MedCamStep(c, 1.0f / 60.0f));
	// przez szew: -160 -> 10 krotsza droga +170
	MedCamBegin(c, -160.0f, 30.0f, 10.0f);
	MedCamRelease(c);
	CHECK(CamNear(c.aziGoal - c.azi, 170.0f, 1e-2f));
	CamRun(c, 3.0f);
	CHECK(c.phase == MCAM_OFF && CamNear(MedCamAzi(c), 10.0f));
	MedCam idle;
	MedCamRelease(idle);
	CHECK(idle.phase == MCAM_OFF);
}
TEST(med_cam_return_after_gap_snaps_mode_change_and_timeout_end_it) {
	MedCam c;
	MedCamBegin(c, 0.0f, 30.0f, 5.0f);
	MedCamMouse(c, -600.0f, 0.0f, 0.15f, 1);
	CamRun(c, 1.0f);
	MedCamRelease(c);
	CHECK(MedCamStep(c, 2.0f) && c.phase == MCAM_OFF && CamNear(MedCamAzi(c), 5.0f));   // po dialogu / menu - od razu cel
	MedCamBegin(c, 0.0f, 30.0f, 0.0f);
	MedCamModeChanged(c, 20.0f);                                              // ACTIVE: nowy cel powrotu
	CHECK(c.phase == MCAM_ACTIVE && CamNear(c.baseAzi, 20.0f));
	MedCamRelease(c);
	CHECK(CamNear(c.aziGoal - c.azi, 20.0f));
	MedCamModeChanged(c, 0.0f);                                               // RETURN: silnik przejal
	CHECK(c.phase == MCAM_OFF && !MedCamStep(c, 0.016f));
	MedCamBegin(c, 90.0f, 30.0f, 0.0f);
	MedCamRelease(c);
	c.t = MED_CAM_RETURN_MAX - 0.01f;
	CHECK(MedCamStep(c, 0.05f) && c.phase == MCAM_OFF && MedCamAzi(c) == 0.0f);
}
TEST(med_cam_modes_and_tracker_frame_time) {
	CHECK(MedCamModeOrbits("CAMMODNORMAL", false) && MedCamModeOrbits("CAMMODRUN", false));
	CHECK(MedCamModeOrbits("CAMMODLOOKBACK", false) && MedCamModeOrbits("CAMMODINVENTORY", false));
	CHECK(!MedCamModeOrbits("CAMMODLOOK", false) && !MedCamModeOrbits("CAMMODFIRSTPERSON", false));
	CHECK(!MedCamModeOrbits("CAMMODDIALOG", false) && !MedCamModeOrbits("CAMMODNORMAL", true));
	CHECK(CamNear(MedCamTrackerDt(0.001f, 1.0f / 60.0f), 1.0f / 60.0f, 1e-6f));   // czekanie x100: 1 ms -> 16,7 ms
	CHECK(CamNear(MedCamTrackerDt(0.016f, 0.010f), 0.016f, 1e-6f));              // silnikowy wiekszy - bez zmian
	CHECK(CamNear(MedCamTrackerDt(0.016f, 0.020f), 0.016f, 1e-6f));              // rozrzut pomiaru poza czekaniem - bez zmian
	CHECK(CamNear(MedCamTrackerDt(0.005f, 1.0f / 60.0f), 1.0f / 60.0f, 1e-6f));  // tempo x20 przy 60 kl./s (5 ms)
	CHECK(CamNear(MedCamTrackerDt(0.001f, 0.5f), MED_CAM_DT_MAX, 1e-6f));
	CHECK(CamNear(MedCamTrackerDt(0.02f, -1.0f), 0.02f, 1e-6f));
}

// ---------- Gildia i sciezki NB (Core/PathMenu) ----------
#include <map>
#include "Core/PathMenu.h"

TEST(menu_tabs_switch_with_q_e_tab) {
	CHECK(MenuKeyFromDik(0x10) == MKEY_TAB_PREV && MenuKeyFromDik(0x12) == MKEY_TAB_NEXT && MenuKeyFromDik(0x0F) == MKEY_TAB_NEXT);
	MenuList m = TestMenu();
	MenuFixCursor(m);
	CHECK(MenuHandleKey(m, MKEY_TAB_NEXT).type == MEV_NONE);         // bez zakladek nic (Steam: medytacja bez zmian)
	m.tabs = { "Medytacja", "Gildia", "Sciezki" };
	MenuEvent e = MenuHandleKey(m, MKEY_TAB_NEXT);
	CHECK(e.type == MEV_TAB && e.value == 1 && m.tab == 1);
	e = MenuHandleKey(m, MKEY_TAB_PREV);
	CHECK(e.type == MEV_TAB && e.value == 0);
	e = MenuHandleKey(m, MKEY_TAB_PREV);
	CHECK(e.type == MEV_TAB && e.value == 2);                        // w kolko
	m.rows.clear();
	m.cursor = -1;
	CHECK(MenuHandleKey(m, MKEY_TAB_NEXT).type == MEV_TAB);          // zakladki dzialaja tez bez wierszy
}

struct PmMapVars : PmVars {
	std::map<std::string, std::vector<int>> m;
	void Def(const std::string& n, int count = 1, int v = 0) {
		m[n].assign(count, 0);
		m[n][0] = v;
	}
	int Count(const std::string& n) const override {
		auto it = m.find(n);
		return it == m.end() ? 0 : (int)it->second.size();
	}
	int Get(const std::string& n, int i = 0) const override {
		auto it = m.find(n);
		return it == m.end() || i < 0 || i >= (int)it->second.size() ? 0 : it->second[i];
	}
	void Set(const std::string& n, int v, int i = 0) override {
		auto it = m.find(n);
		if (it != m.end() && i >= 0 && i < (int)it->second.size())
			it->second[i] = v;
	}
};

// Symbole jak w NB + fake guilds + zDismember_NB_Paths.d (wszystko 0).
static PmMapVars PmNb() {
	PmMapVars v;
	v.Def(PM_V_VER);
	v.Def(PM_V_OFF);
	v.Def(PM_V_KEEP, PM_PATHS);
	v.Def(PM_V_SKILL_OFF, PM_SKILL_SLOTS / 32);
	v.Def(PM_V_SKILL_KEEP, PM_SKILL_SLOTS);
	v.Def(PM_V_GUILD_LAST);
	for (int p = 0; p < PM_PATHS; p++) {
		v.Def(PmPath(p).flag);
		v.Def(PmPath(p).trail);
	}
	for (const PmSkillDef& d : PmSkills()) {
		if (d.var)
			v.Def(d.var);
		if (d.var2)
			v.Def(d.var2);
	}
	for (const char* n : { "VALUE_GPAL", "VALUE_GMIL", "VALUE_GFMM", "VALUE_GFMN", "VALUE_GDGH", "VALUE_GMRC", "VALUE_GSEC",
			"VALUE_GWMM", "VALUE_GGUR", "VALUE_GTPL", "VALUE_GWMN", "VALUE_GDMN", "VALUE_GDDS", "VALUE_GCVM", "VALUE_GCVF",
			"VALUE_GCVW", "VALUE_GUILD_HOOK", "VALUE_TEMP_HERO_GUILD", "VALUE_HERO_GUILD", "RX_VAMPIREWAYTYPE",
			"RX_AW_SKILL_USEOUTOFCOMBAT" })
		v.Def(n);
	return v;
}

// Stan jak savegame18 (mapa A4): gildia 2, sciezki wampir (typ 1), zwiadowca, DH, AW, czarnoksieznik, przywolywacz;
// szaman i truciciel tylko slad (przepaska).
static PmMapVars PmSave18() {
	PmMapVars v = PmNb();
	for (int p : { PM_VAMP, PM_SCOUT, PM_DH, PM_AW, PM_DARKMAGE, PM_SUMMONER }) {
		v.Set(PmPath(p).flag, PmPath(p).magic);
		v.Set(PmPath(p).trail, PmPath(p).magic);
	}
	v.Set("VALUE_GSHM", 19871233);
	v.Set("VALUE_GAPP", 7391745);
	for (const char* n : { "VALUE_GMIL", "VALUE_GFMN", "VALUE_GFMM", "VALUE_GMRC", "VALUE_GSEC", "VALUE_GTPL", "VALUE_GGUR",
			"VALUE_GWMN", "VALUE_GDDS", "VALUE_GCVM", "VALUE_GCVF", "VALUE_GCVW" })
		v.Set(n, 1);
	v.Set("RX_VAMPIREWAYTYPE", 1);
	v.Set("RX_VAMP_SKILL_EVADE", 1);
	v.Set("RX_VAMP_SKILL_SPRINT", 1);
	v.Set("RX_VAMP_SKILL_ACROBAT", 1);
	v.Set("RX_VAMP_SKILLMAGIC_BLOODCIRCLE", 2);
	v.Set("RX_VAMP_SKILLFIST_NEWOVERLAY", 1);
	v.Set("RX_DH_KNOWUNDEADSKILL", 1);
	v.Set("RX_DARKMAGEDIALOGSKILL", 1);
	v.Set("RX_NECRORUNEAURARUNELEARNED", 1);
	v.Set("RX_NECRORUNEAURA_1", 1);
	v.Set("RX_NECRORUNEAURA_ARMOR", 1);
	return v;
}

static PmWorld PmW(int guild = 2) {
	PmWorld w;
	w.guild = guild;
	return w;
}

static bool PmHasChange(const PmResult& r, const std::string& var, int from, int to, int idx = 0) {
	for (const PmChange& c : r.changes)
		if (c.var == var && c.idx == idx && c.from == from && c.to == to)
			return true;
	return false;
}

TEST(pm_available_needs_menu_state_and_nb_symbols) {
	PmMapVars steam;
	std::string miss;
	CHECK(!PmAvailable(steam, &miss) && miss.find("ZDF_PATHS_OFF") != std::string::npos);
	PmMapVars nb = PmNb();
	CHECK(PmAvailable(nb, &miss) && miss.empty());
	PmMapVars noFile = PmNb();
	noFile.m.erase(PM_V_SKILL_KEEP);
	CHECK(!PmAvailable(noFile, &miss) && miss == "ZDF_SKILL_KEEP");
}

TEST(pm_skill_table_indices_unique_names_and_vars) {
	std::vector<int> seen(PM_SKILL_SLOTS, 0);
	int toggles = 0, locked = 0;
	for (const PmSkillDef& d : PmSkills()) {
		CHECK(d.name && d.name[0]);
		CHECK(d.path >= 0 && d.path < PM_GROUPS);
		if (d.kind == PSK_GROUP) {
			CHECK(d.idx == -1 && !d.var);
			continue;
		}
		CHECK(d.idx >= 0 && d.idx < 250);
		CHECK(d.var && d.var[0]);
		if (d.idx >= 0 && d.idx < PM_SKILL_SLOTS)
			seen[d.idx]++;
		if (d.var2Mode == PV2_PAIR) {
			CHECK(d.keep2 >= 250 && d.keep2 < PM_SKILL_SLOTS);
			if (d.keep2 >= 0 && d.keep2 < PM_SKILL_SLOTS)
				seen[d.keep2]++;
		}
		toggles += d.kind == PSK_TOGGLE;
		locked += d.kind == PSK_LOCKED;
		CHECK(PmSkillByIdx(d.idx) == &d);
	}
	bool unique = true;
	for (int n : seen)
		unique = unique && n <= 1;
	CHECK(unique);
	CHECK(locked == 15);    // trwale premie z decyzji autora (mapa C, decyzja 2) + Potega Guru, 2 linie run Straznika
	CHECK(toggles > 120);
	// mapa: zakresy indeksow sciezek (stale w zapisie)
	CHECK(PmSkillByIdx(1)->path == PM_VAMP && PmSkillByIdx(80)->path == PM_DH && PmSkillByIdx(165)->path == PM_SUMMONER);
	CHECK(PmSkillByIdx(232)->path == PM_POISON && PmSkillByIdx(217)->kind == PSK_LOCKED);
}

TEST(pm_paths_magic_values_from_map) {
	CHECK(PmPath(PM_VAMP).magic == 7345012 && std::string(PmPath(PM_VAMP).flag) == "RX_ISVAMPWAY");
	CHECK(PmPath(PM_SCOUT).magic == 9980312 && PmPath(PM_DH).magic == 8891273 && PmPath(PM_DK).magic == 98044518);
	CHECK(PmPath(PM_AW).magic == 987412 && PmPath(PM_DARKMAGE).magic == 17889123 && PmPath(PM_SUMMONER).magic == 179935);
	CHECK(PmPath(PM_SHAMAN).magic == 19871233 && PmPath(PM_DRUID).magic == 7789023 && PmPath(PM_POISON).magic == 1);
	// bity nakladek *_isway.d: 8 DK, 32 czarnoksieznik, 64 przywolywacz
	CHECK((1 << PM_DK) == 8 && (1 << PM_DARKMAGE) == 32 && (1 << PM_SUMMONER) == 64);
	CHECK(PmIsTplPath(PM_VAMP) && PmIsTplPath(PM_AW) && !PmIsTplPath(PM_DARKMAGE) && !PmIsTplPath(PM_SHAMAN));
}

TEST(pm_guild_choices_from_trails) {
	PmMapVars v = PmSave18();
	std::vector<PmGuildChoice> c = PmGuildChoices(v, 2);
	std::vector<int> g;
	for (auto& x : c)
		g.push_back(x.guild);
	CHECK((g == std::vector<int>{ 0, 2, 4, 5, 7, 12, 15, 16, 17, 18, 19 }));   // 18 razem z 19 (NDM bez sladu)
	CHECK(c[7].via.find("Wampir") != std::string::npos && c[7].via.find("Zwiadowca") != std::string::npos);
	CHECK(PmGuildChoices(v, 13).size() == c.size());                         // pirat - nie dodawany jako "obecna"
	CHECK(PmGuildChoices(v, 3).size() == c.size() + 1);                      // VLK dozwolona - dodana jako obecna
	CHECK(!PmGuildSafe(9) && !PmGuildSafe(10) && !PmGuildSafe(11) && !PmGuildSafe(13) && !PmGuildSafe(20) && !PmGuildSafe(42));
	CHECK(PmGuildSafe(0) && PmGuildSafe(16) && PmGuildSafe(19));
}

TEST(pm_guild_deny_rules) {
	PmMapVars v = PmSave18();
	PmWorld w = PmW(2);
	CHECK(PmGuildDeny(w, v, 16, true) == PMD_OK);
	CHECK(PmGuildDeny(w, v, 2, true) == PMD_SAME);
	CHECK(PmGuildDeny(w, v, 13, true) == PMD_GUILD_FORBIDDEN);
	CHECK(PmGuildDeny(w, v, 1, false) == PMD_NOT_EARNED);
	w.joint = true;
	CHECK(PmGuildDeny(w, v, 16, true) == PMD_JOINT && PmGuildDeny(w, v, 7, true) == PMD_OK);
	w = PmW(16);
	w.joint = true;
	CHECK(PmGuildDeny(w, v, 7, true) == PMD_JOINT);
	w = PmW(17);
	w.island = true;
	CHECK(PmGuildDeny(w, v, 2, true) == PMD_ISLAND_GUILD);
	CHECK(PmGuildDeny(w, v, 16, true) == PMD_OK);                              // AW wlaczony - 16 daje status wody
	v.Set("RX_ISADANOSWARRIORWAY", 0);
	CHECK(PmGuildDeny(w, v, 16, true) == PMD_ISLAND_GUILD);
	w = PmW(2);
	w.trans = true;
	CHECK(PmGuildDeny(w, v, 16, true) == PMD_TRANS);
}

TEST(pm_guild_apply_undoes_bandit_hook_and_keeps_base_guild) {
	PmMapVars v = PmSave18();
	v.Set("VALUE_GUILD_HOOK", PM_GUILD_HOOK_MAGIC);
	v.Set("VALUE_TEMP_HERO_GUILD", 0);
	v.Set("VALUE_HERO_GUILD", 17);
	PmGuildPlan p = PmGuildApply(v, 16);
	CHECK(p.hookUndone && v.Get("VALUE_GUILD_HOOK") == 0);
	CHECK(v.Get(PM_V_GUILD_LAST) == 16 && v.Get("VALUE_HERO_GUILD") == 17 && v.Get(PM_V_VER) == PM_VERSION);
	CHECK(!PmGuildApply(v, 2).hookUndone);
	CHECK(PmGuildPathsHint(v, 16).find("Wampir") != std::string::npos);
}

TEST(pm_path_states) {
	PmMapVars v = PmSave18();
	CHECK(PmReadPath(v, PM_VAMP).state == PMS_ON);
	CHECK(PmReadPath(v, PM_SHAMAN).state == PMS_OFF_GAME);   // slad jest, znacznik 0 (przepaska)
	CHECK(PmReadPath(v, PM_DK).state == PMS_NONE && PmReadPath(v, PM_DRUID).state == PMS_NONE);
	PmMapVars steam;
	CHECK(PmReadPath(steam, PM_VAMP).state == PMS_NONE);
}

TEST(pm_path_off_on_keeps_flag_and_type) {
	PmMapVars v = PmSave18();
	PmResult r = PmSetPath(v, PmW(16), PM_VAMP, false);
	CHECK(r.deny == PMD_OK && v.Get("RX_ISVAMPWAY") == 0 && v.Get(PM_V_KEEP, PM_VAMP) == 7345012);
	CHECK((v.Get(PM_V_OFF) & 1) && v.Get("RX_VAMPIREWAYTYPE") == 1);         // typ zostaje
	CHECK(PmHasChange(r, "RX_ISVAMPWAY", 7345012, 0) && PmHasChange(r, PM_V_OFF, 0, 1));
	CHECK(PmReadPath(v, PM_VAMP).state == PMS_OFF_MENU);
	CHECK(PmSetPath(v, PmW(16), PM_VAMP, false).deny == PMD_SAME);
	r = PmSetPath(v, PmW(16), PM_VAMP, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_ISVAMPWAY") == 7345012 && (v.Get(PM_V_OFF) & 1) == 0);
	CHECK(PmSetPath(v, PmW(16), PM_VAMP, true).deny == PMD_SAME);
	// wylaczona przez gre (bez KEEP) - wlaczenie stala magiczna
	r = PmSetPath(v, PmW(2), PM_POISON, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_POISONMAIN") == 1);
	CHECK(PmSetPath(v, PmW(2), PM_DRUID, true).deny == PMD_NOT_EARNED);
	// przywolywacz: kara x0,25 w DLL zdejmuje samo wyzerowanie znacznika, bit 64 dla nakladki
	r = PmSetPath(v, PmW(2), PM_SUMMONER, false);
	CHECK(r.deny == PMD_OK && v.Get("RX_NECROSUMMONWAY") == 0 && (v.Get(PM_V_OFF) & 64));
}

TEST(pm_path_blockers) {
	PmMapVars v = PmSave18();
	PmWorld w = PmW(16);
	w.trans = true;
	CHECK(PmSetPath(v, w, PM_VAMP, false).deny == PMD_TRANS && v.Get("RX_ISVAMPWAY") == 7345012);
	w = PmW(16);
	w.dialog = true;
	CHECK(PmPathDeny(w, v, PM_VAMP, false) == PMD_DIALOG);
	w = PmW(16);
	w.joint = true;
	CHECK(PmPathDeny(w, v, PM_SCOUT, false) == PMD_JOINT && PmPathDeny(w, v, PM_SUMMONER, false) == PMD_OK);
	w = PmW(2);
	w.holyLight = true;
	CHECK(PmPathDeny(w, v, PM_SHAMAN, true) == PMD_HOLYLIGHT);
	w = PmW(16);
	w.island = true;
	CHECK(PmPathDeny(w, v, PM_AW, false) == PMD_ISLAND_AW);
	w.guild = 17;
	CHECK(PmPathDeny(w, v, PM_AW, false) == PMD_OK);                         // Adept Wody - status wody bez AW
	v.m.erase("RX_ISDRUIDWAY");
	CHECK(PmPathDeny(PmW(), v, PM_DRUID, true) == PMD_NO_VAR);
}

TEST(pm_shaman_and_dark_warrior_exclude_each_other) {
	PmMapVars v = PmSave18();
	v.Set("VALUE_GDWN", 98044518);
	v.Set("RX_ISDARKWAYWARRIOR", 98044518);
	PmResult r = PmSetPath(v, PmW(16), PM_SHAMAN, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_ISSHAMANWAY") == 19871233);
	CHECK(v.Get("RX_ISDARKWAYWARRIOR") == 0 && (v.Get(PM_V_OFF) & 8) && v.Get(PM_V_KEEP, PM_DK) == 98044518 && !r.notes.empty());
	CHECK(PmSetPath(v, PmW(16), PM_DK, true).deny == PMD_SHAMAN_DK);
	CHECK(PmSetPath(v, PmW(16), PM_SHAMAN, false).deny == PMD_OK);
	r = PmSetPath(v, PmW(16), PM_DK, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_ISDARKWAYWARRIOR") == 98044518);
	// joint: wlaczenie szamana wylaczyloby DK (znacznik TPL)
	PmWorld w = PmW(16);
	w.joint = true;
	CHECK(PmPathDeny(w, v, PM_SHAMAN, true) == PMD_JOINT);
}

TEST(pm_adanos_warrior_enable_restores_out_of_combat) {
	PmMapVars v = PmSave18();
	CHECK(PmSetPath(v, PmW(2), PM_AW, false).deny == PMD_OK && v.Get("RX_ISADANOSWARRIORWAY") == 0);
	CHECK(v.Get("RX_AW_SKILL_USEOUTOFCOMBAT") == 0);
	PmResult r = PmSetPath(v, PmW(2), PM_AW, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_ISADANOSWARRIORWAY") == 987412 && v.Get("RX_AW_SKILL_USEOUTOFCOMBAT") == 1);
	CHECK(PmHasChange(r, "RX_AW_SKILL_USEOUTOFCOMBAT", 0, 1));
}

TEST(pm_watch_forces_off_and_reports_game_changes) {
	PmMapVars v = PmSave18();
	PmWatchMem mem;
	std::vector<PmEvent> ev;
	PmWatch(v, mem, false, ev);
	CHECK(ev.empty() && mem.init);
	PmSetPath(v, PmW(16), PM_DH, false);
	v.Set("RX_ISDHWAY", 8891273);                                             // zbroja DH fake guilds
	ev.clear();
	PmWatch(v, mem, false, ev);
	CHECK(ev.size() == 1 && ev[0].kind == PME_PATH_FORCED_OFF && ev[0].id == PM_DH && v.Get("RX_ISDHWAY") == 0);
	v.Set("RX_ISSCOUTWAY", 0);                                                // przepaska
	ev.clear();
	PmWatch(v, mem, false, ev);
	CHECK(ev.size() == 1 && ev[0].kind == PME_PATH_GAME_OFF && ev[0].id == PM_SCOUT);
	CHECK(PmReadPath(v, PM_SCOUT).state == PMS_OFF_GAME && v.Get("RX_ISSCOUTWAY") == 0);   // bez automatycznego powrotu
	v.Set("RX_ISSCOUTWAY", 9980312);
	ev.clear();
	PmWatch(v, mem, false, ev);
	CHECK(ev.size() == 1 && ev[0].kind == PME_PATH_GAME_ON);
}

TEST(pm_skill_toggle_and_locked) {
	PmMapVars v = PmSave18();
	const PmSkillDef* ev = PmSkillByIdx(1);       // RX_VAMP_SKILL_EVADE
	CHECK(ev && std::string(ev->var) == "RX_VAMP_SKILL_EVADE");
	PmResult r = PmSetSkill(v, PmW(16), *ev, false);
	CHECK(r.deny == PMD_OK && v.Get("RX_VAMP_SKILL_EVADE") == 0 && v.Get(PM_V_SKILL_KEEP, 1) == 1 && (v.Get(PM_V_SKILL_OFF, 0) & 2));
	PmSkillView s = PmReadSkill(v, *ev);
	CHECK(s.learned && s.off && s.keep == 1);
	CHECK(PmSetSkill(v, PmW(16), *ev, false).deny == PMD_SAME);
	v.Set("RX_VAMP_SKILL_EVADE", 2);                                          // kupiona ponownie wyzej (bez ochrony)
	r = PmSetSkill(v, PmW(16), *ev, true);
	CHECK(r.deny == PMD_OK && v.Get("RX_VAMP_SKILL_EVADE") == 2 && v.Get(PM_V_SKILL_KEEP, 1) == 0 && !(v.Get(PM_V_SKILL_OFF, 0) & 2));
	const PmSkillDef* acro = PmSkillByIdx(10);    // Zrecznosc Potepionych - talent akrobatyki na stale
	CHECK(acro->kind == PSK_LOCKED && PmReadSkill(v, *acro).learned);
	CHECK(PmSetSkill(v, PmW(16), *acro, false).deny == PMD_LOCKED && v.Get("RX_VAMP_SKILL_ACROBAT") == 1);
	CHECK(PmSetSkill(v, PmW(16), *PmSkillByIdx(0), false).deny == PMD_NOT_EARNED);   // niewyuczona regeneracja
	CHECK(PmSkillByIdx(11)->kind == PSK_LOCKED && PmSkillByIdx(146)->kind == PSK_LOCKED && PmSkillByIdx(171)->kind == PSK_LOCKED);
}

TEST(pm_skill_second_variables) {
	PmMapVars v = PmSave18();
	// Duchowa tarcza: poziom zapamietany, wartosc zerowana i nie przywracana
	v.Set("RX_DH_SKILLSHIELDLEVEL", 3);
	v.Set("RX_DH_SKILLSHIELDVALUE", 250);
	const PmSkillDef* sh = PmSkillByIdx(80);
	CHECK(PmSetSkill(v, PmW(16), *sh, false).deny == PMD_OK && v.Get("RX_DH_SKILLSHIELDVALUE") == 0);
	CHECK(PmSetSkill(v, PmW(16), *sh, true).deny == PMD_OK && v.Get("RX_DH_SKILLSHIELDLEVEL") == 3 && v.Get("RX_DH_SKILLSHIELDVALUE") == 0);
	// Aura wladcy nieumarlych: runa + aura 1 razem
	const PmSkillDef* aura = PmSkillByIdx(165);
	CHECK(PmSetSkill(v, PmW(2), *aura, false).deny == PMD_OK);
	CHECK(v.Get("RX_NECRORUNEAURARUNELEARNED") == 0 && v.Get("RX_NECRORUNEAURA_1") == 0 && v.Get(PM_V_SKILL_KEEP, 250) == 1);
	CHECK(PmSetSkill(v, PmW(2), *aura, true).deny == PMD_OK);
	CHECK(v.Get("RX_NECRORUNEAURARUNELEARNED") == 1 && v.Get("RX_NECRORUNEAURA_1") == 1 && v.Get(PM_V_SKILL_KEEP, 250) == 0);
	// DARKSKILLOWN0: SKILLCOUNTCURM -1 / +1, tylko przy wylaczonej sciezce DK
	v.Set("VALUE_GDWN", 98044518);
	v.Set("RX_ISDARKWAYWARRIOR", 98044518);
	v.Set("DARKSKILLOWN0", 1);
	v.Set("SKILLCOUNTCURM", 2);
	const PmSkillDef* own = PmSkillByIdx(108);
	CHECK(PmSetSkill(v, PmW(16), *own, false).deny == PMD_DK_ON && v.Get("SKILLCOUNTCURM") == 2);
	PmSetPath(v, PmW(16), PM_DK, false);
	CHECK(PmSetSkill(v, PmW(16), *own, false).deny == PMD_OK && v.Get("SKILLCOUNTCURM") == 1 && v.Get("DARKSKILLOWN0") == 0);
	CHECK(PmSetSkill(v, PmW(16), *own, true).deny == PMD_OK && v.Get("SKILLCOUNTCURM") == 2 && v.Get("DARKSKILLOWN0") == 1);
	// chowaniec tylko przy odwolanym
	v.Set("RX_VAMP_SKILLWARRIOR_FAMILIAR", 2);
	PmWorld w = PmW(16);
	w.familiar = true;
	CHECK(PmSetSkill(v, w, *PmSkillByIdx(35), false).deny == PMD_FAMILIAR);
	CHECK(PmSetSkill(v, PmW(16), *PmSkillByIdx(35), false).deny == PMD_OK);
}

TEST(pm_dialog_guard_restores_and_reapplies) {
	PmMapVars v = PmSave18();
	PmSetSkill(v, PmW(16), *PmSkillByIdx(1), false);      // unik (keep 1)
	PmSetSkill(v, PmW(16), *PmSkillByIdx(165), false);    // aura (para)
	PmSetSkill(v, PmW(16), *PmSkillByIdx(160), false);    // czarna magia - tylko u Xardasa
	std::vector<PmEvent> ev;
	int n = PmDialogRestore(v, "PAL_2000_VAMP_MAINCASTLE", ev);
	CHECK(n == 2 && v.Get("RX_VAMP_SKILL_EVADE") == 1 && v.Get("RX_NECRORUNEAURA_1") == 1 && v.Get("RX_DARKMAGEDIALOGSKILL") == 0);
	// w trakcie rozmowy pilnowanie nie zeruje umiejetnosci
	PmWatchMem mem;
	ev.clear();
	PmWatch(v, mem, true, ev);
	CHECK(v.Get("RX_VAMP_SKILL_EVADE") == 1);
	v.Set("RX_VAMP_SKILL_EVADE", 2);                      // nauka wyzszego poziomu w rozmowie
	ev.clear();
	PmDialogReapply(v, ev);
	CHECK(v.Get("RX_VAMP_SKILL_EVADE") == 0 && v.Get(PM_V_SKILL_KEEP, 1) == 2 && v.Get("RX_NECRORUNEAURA_1") == 0);
	bool bought = false;
	for (auto& e : ev)
		bought = bought || (e.kind == PME_SKILL_BOUGHT && e.id == 1 && e.from == 1 && e.to == 2);
	CHECK(bought);
	ev.clear();
	CHECK(PmDialogRestore(v, "NONE_100_XARDAS", ev) == 3 && v.Get("RX_DARKMAGEDIALOGSKILL") == 1);
	PmDialogReapply(v, ev);
	CHECK(v.Get("RX_DARKMAGEDIALOGSKILL") == 0 && v.Get("RX_VAMP_SKILL_EVADE") == 0);
	// poza rozmowa: ustawiona ponownie (np. ksiega) - zerowana, keep = max
	v.Set("RX_VAMP_SKILL_EVADE", 1);
	ev.clear();
	PmWatch(v, mem, false, ev);
	CHECK(v.Get("RX_VAMP_SKILL_EVADE") == 0 && v.Get(PM_V_SKILL_KEEP, 1) == 2 && ev.size() == 1 && ev[0].kind == PME_SKILL_REZERO);
}

TEST(pm_vampire_type_and_fist_overlay) {
	PmMapVars v = PmSave18();
	CHECK((PmVampTypes(v) == std::vector<int>{ 1, 2, 3 }));
	PmWorld w = PmW(16);
	w.bloodGift = true;
	CHECK(PmSetVampType(v, w, 2).deny == PMD_BLOODGIFT && v.Get("RX_VAMPIREWAYTYPE") == 1);
	CHECK(PmSetVampType(v, PmW(16), 1).deny == PMD_SAME);
	CHECK(PmSetVampType(v, PmW(16), 2).deny == PMD_OK && v.Get("RX_VAMPIREWAYTYPE") == 2);
	CHECK(PmFistOverlayWanted(v, 16) && !PmFistOverlayWanted(v, 2));
	PmSetPath(v, PmW(16), PM_VAMP, false);
	CHECK(!PmFistOverlayWanted(v, 16));
	v.Set("VALUE_GCVF", 0);
	v.Set("VALUE_GCVW", 0);
	v.Set("RX_VAMPIREWAYTYPE", 1);
	CHECK((PmVampTypes(v) == std::vector<int>{ 1 }) && PmSetVampType(v, PmW(16), 3).deny == PMD_NOT_EARNED);
}

TEST(pm_summary_lists_paths_and_disabled_skills) {
	PmMapVars v = PmSave18();
	PmSetPath(v, PmW(2), PM_SUMMONER, false);
	PmSetSkill(v, PmW(2), *PmSkillByIdx(4), false);
	std::string s = PmSummary(v, 2);
	CHECK(s.find("RX_NECROSUMMONWAY wylaczona w menu") != std::string::npos);
	CHECK(s.find("RX_VAMP_SKILL_SPRINT(keep 1)") != std::string::npos && s.find("ZDF_PATHS_OFF 0x40") != std::string::npos);
	CHECK(PmChangesText(PmSetPath(v, PmW(2), PM_SUMMONER, true)).find("RX_NECROSUMMONWAY 0->179935") != std::string::npos);
}

// ---------- mysz okna i opisy umiejetnosci (Core/MenuList, Core/PathSkillInfo) ----------
static MenuHitMap MouseHit(const MenuList& m) {
	// wiersze po 400 jednostek od y=1000, wartosc w x 6000..7700, strzalki po 200; zakladki w y 500..800
	MenuHitMap h;
	h.valid = true;
	int y = 1000;
	for (size_t i = 0; i < m.rows.size(); i++) {
		MenuHitRow r;
		r.row = (int)i;
		r.y0 = y;
		r.y1 = y + 400;
		if (m.rows[i].kind == MROW_CHOICE) {
			r.valX0 = 6000;
			r.valX1 = 7700;
			r.arrowW = 200;
		}
		h.rows.push_back(r);
		y += 400;
	}
	for (int t = 0; t < (int)m.tabs.size(); t++)
		h.tabs.push_back({ t, 1000 + t * 2000, 2800 + t * 2000, 500, 800 });
	return h;
}

TEST(menu_mouse_hover_click_wheel_and_back) {
	MenuList m = TestMenu();      // 0 Czekaj (wybor, activates), 1 tekst, 2 Regeneracja (wybor, wrap), 3 ukryty, 4 wylaczony, 5 Wstan
	MenuFixCursor(m);
	MenuHitMap h = MouseHit(m);
	MenuMouseResult r = MenuMouse(m, h, MMOUSE_MOVE, 1000, 1000 + 2 * 400 + 100);   // nad "Regeneracja"
	CHECK(r.ev.type == MEV_MOVED && m.cursor == 2 && r.inside);
	r = MenuMouse(m, h, MMOUSE_MOVE, 1000, 1000 + 1 * 400 + 100);                   // tekst - kursor zostaje
	CHECK(r.ev.type == MEV_NONE && m.cursor == 2);
	r = MenuMouse(m, h, MMOUSE_MOVE, 1000, 1000 + 4 * 400 + 100);                   // wylaczony - zostaje
	CHECK(m.cursor == 2);
	r = MenuMouse(m, h, MMOUSE_LEFT, 7600, 1000 + 2 * 400 + 100);                   // strzalka ">"
	CHECK(r.ev.type == MEV_CHANGED && r.ev.rowId == 3 && m.rows[2].value == 1);
	r = MenuMouse(m, h, MMOUSE_LEFT, 6050, 1000 + 2 * 400 + 100);                   // strzalka "<"
	CHECK(r.ev.type == MEV_CHANGED && m.rows[2].value == 0);
	r = MenuMouse(m, h, MMOUSE_LEFT, 1000, 1000 + 2 * 400 + 100);                   // srodek wiersza bez activates = nastepna wartosc
	CHECK(r.ev.type == MEV_CHANGED && m.rows[2].value == 1);
	r = MenuMouse(m, h, MMOUSE_LEFT, 1000, 1000 + 100);                             // "Czekaj" activates = Enter
	CHECK(r.ev.type == MEV_ACTIVATE && r.ev.rowId == 1 && m.cursor == 0);
	r = MenuMouse(m, h, MMOUSE_LEFT, 1000, 1000 + 5 * 400 + 100);                   // akcja "Wstan"
	CHECK(r.ev.type == MEV_ACTIVATE && r.ev.rowId == 6);
	r = MenuMouse(m, h, MMOUSE_WHEEL_UP, 6500, 1000 + 2 * 400 + 100);               // kolko nad wartoscia = zmiana
	CHECK(r.ev.type == MEV_CHANGED && m.cursor == 2 && m.rows[2].value == 2);
	r = MenuMouse(m, h, MMOUSE_WHEEL_DOWN, 1000, 1000 + 2 * 400 + 100);             // kolko nad etykieta = przewijanie
	CHECK(r.ev.type == MEV_MOVED && m.cursor == 5);
	r = MenuMouse(m, h, MMOUSE_RIGHT, 1000, 1000);
	CHECK(r.back && r.ev.type == MEV_NONE);
	r = MenuMouse(m, h, MMOUSE_LEFT, 9000, 1000 + 100);                             // poza oknem
	CHECK(!r.inside && r.ev.type == MEV_NONE);
	m.tabs = { "Medytacja", "Gildia", "Sciezki" };
	h = MouseHit(m);
	r = MenuMouse(m, h, MMOUSE_LEFT, 3500, 600);                                    // klik w zakladke 1
	CHECK(r.ev.type == MEV_TAB && r.ev.value == 1 && m.tab == 1);
	r = MenuMouse(m, h, MMOUSE_LEFT, 3500, 600);                                    // ta sama zakladka - nic
	CHECK(r.ev.type == MEV_NONE);
	MenuRow dim;
	dim.id = 9;
	dim.kind = MROW_CHOICE;
	dim.values = { "nie wyuczona" };
	dim.dim = true;
	m.rows.push_back(dim);
	h = MouseHit(m);
	r = MenuMouse(m, h, MMOUSE_MOVE, 1000, 1000 + 6 * 400 + 100);                   // podglad: wybieralny
	CHECK(r.ev.type == MEV_MOVED && m.cursor == 6);
	r = MenuMouse(m, h, MMOUSE_LEFT, 7600, 1000 + 6 * 400 + 100);                   // jedna wartosc - bez zmiany
	CHECK(r.ev.type == MEV_NONE);
}

TEST(pm_skill_info_covers_table_and_cost_text) {
	int n = 0;
	for (const PmSkillDef& d : PmSkills()) {
		if (d.kind == PSK_GROUP)
			continue;
		const PmSkillInfo* inf = PmSkillInfoFor(d.idx);
		CHECK(inf != nullptr);
		if (!inf)
			continue;
		n++;
		CHECK(inf->levels >= 0 && inf->levels <= 10);
		CHECK(inf->effect && inf->effect[0]);
		CHECK(inf->cost && inf->req && inf->teacher);
		CHECK(!inf->lpFunc || (inf->lpConst && inf->lpConst[0]));
	}
	CHECK(n == PmSkillInfoCount());
	const PmSkillInfo* ev = PmSkillInfoFor(1);   // unik wampira: 2 poziomy, 3 / 6 PN, nauczyciel Remus
	CHECK(ev && ev->levels == 2 && std::string(ev->cost).find("3 / 6 PN") == 0 && std::string(ev->teacher) == "Remus");
	CHECK(ev && ev->lpFunc && std::string(ev->lpFunc) == "RX_GETCURSEDKILLCOST_LP" && ev->lpLevelArg == -1);
	const PmSkillInfo* dk = PmSkillInfoFor(91);  // runa Wampiryzm DK: 3 poziomy
	CHECK(dk && dk->levels == 3);
	CHECK(PmCostText({ 3, 6 }, { 1000, 2000 }) == "3 / 6 PN, 1000 / 2000 z\xB3");
	CHECK(PmCostText({ 5 }, {}) == "5 PN" && PmCostText({ 0 }, { 500 }) == "500 z\xB3" && PmCostText({}, {}).empty());
	CHECK(PmCostText({ 4, 4 }, { 0, 0 }) == "4 PN");
}

TEST(pm_paths_names_and_how_to_acquire) {
	CHECK(std::string(PmPath(PM_DK).name) == "Mroczny rycerz");   // nazwa NB (DMT_DARKTEACHER), nie "Mroczny wojownik"
	for (int p = 0; p < PM_PATHS; p++) {
		const char* h = PmPath(p).howTo;
		CHECK(h && std::string(h).rfind("Zdobycie:", 0) == 0);
	}
	CHECK(std::string(PmPath(PM_DK).howTo).find("DMT_DARKTEACHER") != std::string::npos);
	CHECK(std::string(PmPath(PM_DK).howTo).find("szaman") != std::string::npos);
	CHECK(std::string(PmPath(PM_SHAMAN).howTo).find("mroczny rycerz") != std::string::npos);
	CHECK(std::string(PmPath(PM_AW).howTo).find("Nefarius") != std::string::npos);
	CHECK(std::string(PmPathStateText(PMS_NONE)) == "nie zdobyta");
	// niezdobyta sciezka (zapis testowy 22.10: DK - znacznik i slad 0) - stan NONE, bez przelaczania
	PmMapVars v = PmSave18();
	CHECK(PmReadPath(v, PM_DK).state == PMS_NONE && PmPathDeny(PmW(16), v, PM_DK, true) == PMD_NOT_EARNED);
	bool guildDk = false;
	for (auto& c : PmGuildChoices(v, 2))
		guildDk = guildDk || c.via.find("Mroczny rycerz") != std::string::npos;
	CHECK(!guildDk);
	v.Set("VALUE_GDWN", 98044518);
	for (auto& c : PmGuildChoices(v, 2))
		guildDk = guildDk || (c.guild == 16 && c.via.find("Mroczny rycerz") != std::string::npos);
	CHECK(guildDk);
}

TEST(pm_guild_classes_skills_and_earned) {
	CHECK(PmIsGuildClass(PM_GC_PAL) && !PmIsGuildClass(PM_POISON) && PM_GROUPS == PM_PATHS + 6);
	CHECK(PmGuildClass(PM_GC_PAL).guild1 == 1 && PmGuildClass(PM_GC_KDF).guild1 == 4 && PmGuildClass(PM_GC_KDW).guild1 == 14);
	CHECK(PmGuildClass(PM_GC_GUR).guild1 == 15 && PmGuildClass(PM_GC_SLD).guild1 == 7 && PmGuildClass(PM_GC_SLD).guild2 == 6);
	CHECK(PmGuildClass(PM_GC_TPL).anyGuild && PmGuildClassActive(PM_GC_TPL, 2));       // patch_tplskills
	CHECK(PmGuildClassActive(PM_GC_SLD, 6) && PmGuildClassActive(PM_GC_SLD, 7) && !PmGuildClassActive(PM_GC_SLD, 2));
	CHECK(std::string(PmGroupLogName(PM_GC_KDF)) == "GIL_KDF" && std::string(PmGroupLogName(PM_DK)) == "RX_ISDARKWAYWARRIOR");
	CHECK(std::string(PmGroupName(PM_GC_GUR)) == "Guru" && std::string(PmGroupName(PM_SCOUT)) == "Zwiadowca");
	int per[PM_GROUPS] = {};
	for (const PmSkillDef& d : PmSkills())
		if (d.kind != PSK_GROUP)
			per[d.path]++;
	CHECK(per[PM_GC_KDF] == 5 && per[PM_GC_KDW] == 3 && per[PM_GC_PAL] == 3 && per[PM_GC_GUR] == 3 && per[PM_GC_TPL] == 4 && per[PM_GC_SLD] == 4);
	CHECK(PmSkillByIdx(242)->kind == PSK_LOCKED && PmSkillByIdx(44)->kind == PSK_LOCKED);   // Potega Guru (+3 COUNTLEARNSPELL), linia run
	// zdobycie: slad gildii albo obecna gildia
	PmMapVars v = PmSave18();       // slady: MIL, NOV, KDF, SLD, SEK, TPL, GUR, NDW
	CHECK(PmGuildClassEarned(v, PM_GC_KDF, 2) && PmGuildClassEarned(v, PM_GC_SLD, 2) && PmGuildClassEarned(v, PM_GC_TPL, 2));
	CHECK(!PmGuildClassEarned(v, PM_GC_PAL, 2) && !PmGuildClassEarned(v, PM_GC_KDW, 2));
	CHECK(PmGuildClassEarned(v, PM_GC_PAL, 1));                                               // obecna gildia
	// umiejetnosc z wartoscia magiczna (Mag Ognia: RX_SKILL_DOT1 = 37771) - wylaczenie i przywrocenie tej samej wartosci
	v.Set("RX_SKILL_DOT1", 37771);
	const PmSkillDef* dot = PmSkillByIdx(233);
	CHECK(PmSetSkill(v, PmW(4), *dot, false).deny == PMD_OK && v.Get("RX_SKILL_DOT1") == 0 && v.Get(PM_V_SKILL_KEEP, 233) == 37771);
	CHECK(PmSetSkill(v, PmW(4), *dot, true).deny == PMD_OK && v.Get("RX_SKILL_DOT1") == 37771);
	const PmSkillInfo* inf = PmSkillInfoFor(238);
	CHECK(inf && std::string(inf->teacher) == "Albrecht" && std::string(inf->cost).find("3 PN") == 0);
}

TEST(pm_guild_skill_gates_after_guildskills_patch) {
	// brama kazdej umiejetnosci klasy gildii jest zdefiniowana (zablokowane - bez bramy)
	for (const PmSkillDef& d : PmSkills()) {
		if (d.kind == PSK_GROUP || !PmIsGuildClass(d.path))
			continue;
		CHECK(d.kind == PSK_LOCKED ? PmSkillGuildGate(d.idx) == PGG_NONE : PmSkillGuildGate(d.idx) != PGG_NONE);
	}
	CHECK(PmSkillGuildGate(233) == PGG_ANY && PmSkillGuildGate(240) == PGG_ANY);       // GetEngineVersion / bez gildii
	CHECK(PmSkillGuildGate(236) == PGG_PATCH && PmSkillGuildGate(244) == PGG_PATCH && PmSkillGuildGate(247) == PGG_PATCH);
	CHECK(PmSkillGuildGate(243) == PGG_PATCH && PmSkillGuildGate(238) == PGG_SCRIPT && PmSkillGuildGate(41) == PGG_SCRIPT);   // 243: guildskills_isway.d
	CHECK(PmSkillGuildGate(42) == PGG_TPL && PmSkillGuildGate(1) == PGG_NONE);
	CHECK(PmGuildClassOwn(PM_GC_SLD, 6) && PmGuildClassOwn(PM_GC_SLD, 7) && !PmGuildClassOwn(PM_GC_SLD, 16));
	CHECK(!PmGuildClassOwn(PM_VAMP, 16));
	// gildia 16 (zapis testowy): bez laty / z lata
	const PmSkillDef* heal = PmSkillByIdx(239);
	const PmSkillDef* orcs = PmSkillByIdx(238);
	const PmSkillDef* magic = PmSkillByIdx(240);
	CHECK(!PmGuildSkillWorks(*heal, 16, false) && PmGuildSkillWorks(*heal, 16, true) && PmGuildSkillWorks(*heal, 1, false));
	CHECK(!PmGuildSkillWorks(*orcs, 16, true) && PmGuildSkillWorks(*orcs, 1, false));
	CHECK(PmGuildSkillWorks(*magic, 16, false));
	CHECK(PmGuildSkillWorks(*PmSkillByIdx(243), 16, true) && !PmGuildSkillWorks(*PmSkillByIdx(243), 16, false));
	// zasieg klas przy gildii 16
	CHECK(PmGuildClassReach(PM_GC_KDF, 16, true) == 2 && PmGuildClassReach(PM_GC_KDF, 16, false) == 1);
	CHECK(PmGuildClassReach(PM_GC_PAL, 16, true) == 1 && PmGuildClassReach(PM_GC_PAL, 16, false) == 1);   // orki: skrypt
	CHECK(PmGuildClassReach(PM_GC_KDW, 16, true) == 1 && PmGuildClassReach(PM_GC_KDW, 16, false) == 0);
	CHECK(PmGuildClassReach(PM_GC_GUR, 16, true) == 2 && PmGuildClassReach(PM_GC_GUR, 16, false) == 0);   // Spokoj - lata + isway
	CHECK(PmGuildClassReach(PM_GC_SLD, 16, true) == 1 && PmGuildClassReach(PM_GC_SLD, 16, false) == 0);
	CHECK(PmGuildClassReach(PM_GC_TPL, 2, false) == 2);
	CHECK(PmGuildClassReach(PM_GC_SLD, 6, false) == 2 && PmGuildClassReach(PM_GC_GUR, 15, false) == 2);
	CHECK(PmGuildClassReach(PM_VAMP, 16, true) == 0);
}

// ---------- Cofniecie czasu: przywracanie odciec (Core\CutRestore.h) i kanal z zTimeRewind (_shared\ZCutLinkCore.h) ----------
#include "Core/CutRestore.h"
#include "../../_shared/ZCutLinkCore.h"

TEST(cut_restore_plan_splits_restore_and_apply) {
	uint32_t head = MaskSet(0, Part::Head), armL = MaskSet(0, Part::ArmL), thighR = MaskSet(0, Part::ThighR);
	// glowa odcieta, cofniecie do chwili przed smiercia: przywrocic glowe, nic nie sciskac
	CutRestorePlan p = PlanCutRestore(head, 0);
	CHECK(p.restore == head && p.apply == 0 && p.result == 0 && p.Any());
	// zwloki z reka i glowa, cofniete do chwili, gdy byla tylko reka: glowa wraca
	p = PlanCutRestore(head | armL, armL);
	CHECK(p.restore == head && p.apply == 0 && p.result == armL);
	// maska docelowa z czescia, ktorej teraz nie ma (inna sesja zapisu): tylko sciskanie
	p = PlanCutRestore(armL, armL | thighR);
	CHECK(p.restore == 0 && p.apply == thighR && p.result == (armL | thighR));
	// bez zmian
	p = PlanCutRestore(armL, armL);
	CHECK(!p.Any() && p.result == armL);
	// bity spoza 9 czesci odrzucane (aivar moda)
	p = PlanCutRestore(0xFFFFu, 0x10000u);
	CHECK(p.restore == CUT_MASK_BITS && p.apply == 0 && p.result == 0);
}

TEST(cut_restore_mask_part_names_for_log) {
	CHECK(MaskPartNames(0) == "-");
	CHECK(MaskPartNames(MaskSet(0, Part::Head)) == "HEAD");
	CHECK(MaskPartNames(MaskSet(MaskSet(0, Part::ArmL), Part::ThighR)) == "ARM_L THIGH_R");
}

TEST(cut_restore_victim_overlays_dedup_and_match) {
	std::vector<std::string> list = VictimOverlayList({ "HUMANS_ZDF_W3_01_V", "humans_zdf_w3_01_v.mds", "", "HUMANS_ZDF_W3_02_V" },
		"HUMANS_ZDF_DAZE.MDS");
	CHECK(list.size() == 3);
	CHECK(list.size() == 3 && list[0] == "HUMANS_ZDF_W3_01_V" && list[1] == "HUMANS_ZDF_W3_02_V" && list[2] == "HUMANS_ZDF_DAZE");
	CHECK(IsListedOverlay("HUMANS_ZDF_DAZE.MDS", list));
	CHECK(IsListedOverlay("humans_zdf_w3_02_v", list));
	CHECK(!IsListedOverlay("HUMANS_ZDF_W3_1H.MDS", list));   // styl bohatera - nie ofiary
	CHECK(!IsListedOverlay("HUMANS.MDS", list));
	CHECK(OverlayBaseName(" Humans_Relaxed.mds ") == "HUMANS_RELAXED");
}

static uint32_t g_cutLinkMask = 0;
static uint32_t g_cutLinkFlags = 0;
static uint32_t __cdecl CutLinkGet(void*) { return g_cutLinkMask; }
static uint32_t __cdecl CutLinkSet(void*, uint32_t mask, uint32_t flags, const char*) {
	g_cutLinkMask = mask;
	g_cutLinkFlags = flags;
	return zcut::DONE_PARTS | zcut::DONE_GIBS;
}

TEST(cutlink_table_publish_join_and_ready) {
	zcut::Table t;
	std::memset(&t, 0, sizeof(t));
	CHECK(!zcut::Ready(&t));
	CHECK(!zcut::Ready(nullptr));
	// klient pierwszy (kolejnosc ladowania DLL dowolna): naglowek jest, funkcji jeszcze nie
	CHECK(zcut::Join(&t, "zTimeRewind"));
	CHECK(t.magic == zcut::LINK_MAGIC && t.consumers == 1 && std::strcmp(t.consumer, "zTimeRewind") == 0);
	CHECK(!zcut::Ready(&t));
	CHECK(!zcut::Publish(&t, "zDismember", nullptr, CutLinkSet));
	CHECK(zcut::Publish(&t, "zDismember", CutLinkGet, CutLinkSet));
	CHECK(zcut::Ready(&t) && std::strcmp(t.provider, "zDismember") == 0);
	// wywolanie przez tablice
	g_cutLinkMask = 0x1;
	CHECK(t.getMask(nullptr) == 0x1);
	CHECK(t.setMask(nullptr, 0, zcut::FLAG_ALIVE | zcut::FLAG_REVIVE, "test") == (zcut::DONE_PARTS | zcut::DONE_GIBS));
	CHECK(g_cutLinkMask == 0 && g_cutLinkFlags == (zcut::FLAG_ALIVE | zcut::FLAG_REVIVE));
	// inny uklad (stara wersja innego pluginu) - nie dolaczamy i nie wolamy
	zcut::Table bad = t;
	bad.version = zcut::LINK_VERSION + 1;
	CHECK(!zcut::Ready(&bad) && !zcut::Join(&bad, "x") && !zcut::Publish(&bad, "x", CutLinkGet, CutLinkSet));
	bad = t;
	bad.size = 4;
	CHECK(!zcut::Ready(&bad));
}

TEST(cutlink_done_text_lists_actions) {
	char buf[96];
	CHECK(std::strcmp(zcut::DoneText(0, buf, sizeof(buf)), "nic") == 0);
	CHECK(std::strcmp(zcut::DoneText(zcut::DONE_PARTS | zcut::DONE_RAGDOLL, buf, sizeof(buf)), "czesci, ragdoll") == 0);
	char small[10];
	zcut::DoneText(zcut::DONE_PARTS | zcut::DONE_GIBS | zcut::DONE_STUMPS, small, sizeof(small));
	CHECK(std::strlen(small) < sizeof(small));   // obciete, ale z zerem na koncu
}

// ---------- miecze na plecach (Core\BackSwords) ----------
#include "Core/BackSwords.h"

static BackState BsState(int type, bool blade = true) {
	BackState st;
	st.primaryType = type;
	st.primaryBlade = blade;
	st.overlays = true;
	return st;
}

TEST(back_plan_off_or_without_w3_pack_is_native_everywhere) {
	BackSettings s;
	BackState st = BsState(BACK_TYPE_2H);
	st.secondValid = true;
	st.overlays = false;
	BackPlan p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_NATIVE && p.primaryNode == BNODE_NONE && !p.second);
	CHECK(p.draw1H == DRAWV_NATIVE && p.draw2H == DRAWV_NATIVE);
	s.on = false;
	st.overlays = true;
	p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_NATIVE && !p.second && p.draw2H == DRAWV_NATIVE);
}

TEST(back_plan_2h_hangs_on_back_a_and_draws_w3) {
	BackSettings s;
	BackPlan p = PlanBack(s, BsState(BACK_TYPE_2H));
	CHECK(p.primarySpot == SPOT_A && p.primaryNode == BNODE_LONGSWORD && p.draw2H == DRAWV_A);
	BackState st = BsState(BACK_TYPE_2H);
	st.primaryAtB = true;
	p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_B && p.draw2H == DRAWV_B && p.secondSpot == SPOT_A);
}

TEST(back_plan_1h_sword_on_back_only_with_option) {
	BackSettings s;
	BackPlan p = PlanBack(s, BsState(BACK_TYPE_1H, true));
	CHECK(p.primarySpot == SPOT_A && p.primaryNode == BNODE_SWORD && p.draw1H == DRAWV_A);
	// obuch / topor przy Back1H=1 zostaje przy pasie, natywne dobycie szybsze (BELT)
	p = PlanBack(s, BsState(BACK_TYPE_1H, false));
	CHECK(p.primarySpot == SPOT_NATIVE && p.primaryNode == BNODE_NONE && p.draw1H == DRAWV_BELT);
	s.back1H = BACK1H_ALL;
	p = PlanBack(s, BsState(BACK_TYPE_1H, false));
	CHECK(p.primarySpot == SPOT_A && p.primaryNode == BNODE_SWORD);
	s.back1H = BACK1H_OFF;
	p = PlanBack(s, BsState(BACK_TYPE_1H, true));
	CHECK(p.primarySpot == SPOT_NATIVE && p.draw1H == DRAWV_BELT);
	// tempo 100% = pusty wpis (dobycie z nakladki talentu / zEBA bez zmian)
	s.beltSpeed = 100;
	p = PlanBack(s, BsState(BACK_TYPE_1H, true));
	CHECK(p.draw1H == DRAWV_NATIVE);
}

TEST(back_plan_second_sword_takes_free_back_spot) {
	BackSettings s;
	BackState st = BsState(BACK_TYPE_2H);
	st.secondValid = true;
	BackPlan p = PlanBack(s, st);
	CHECK(p.second && p.secondSpot == SPOT_B);
	st.primaryAtB = true;
	p = PlanBack(s, st);
	CHECK(p.second && p.secondSpot == SPOT_A);
	// bron przy pasie: drugi miecz w B (w miejscu luku)
	s.back1H = BACK1H_OFF;
	st = BsState(BACK_TYPE_1H);
	st.secondValid = true;
	st.primaryAtB = true;
	p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_NATIVE && p.second && p.secondSpot == SPOT_B);
	s.second = false;
	CHECK(!PlanBack(s, st).second);
}

TEST(back_plan_second_with_ranged_hidden_or_replacing_bow) {
	BackSettings s;
	BackState st = BsState(BACK_TYPE_1H);
	st.secondValid = true;
	st.bowEquipped = true;
	CHECK(!PlanBack(s, st).second);
	s.withRanged = SECOND_RANGED_REPLACE;
	BackPlan p = PlanBack(s, st);
	CHECK(p.second && !p.hideCrossbow);
	st.bowEquipped = false;
	st.cbowEquipped = true;
	p = PlanBack(s, st);
	CHECK(p.second && p.hideCrossbow);
	s.withRanged = SECOND_RANGED_HIDE;
	p = PlanBack(s, st);
	CHECK(!p.second && !p.hideCrossbow);
}

TEST(back_swap_keeps_swords_in_their_back_spots) {
	// zalozona w A, drugi w B -> po zamianie nowa zalozona w B (tam wisiala), dawna w A
	BackSettings s;
	BackState st = BsState(BACK_TYPE_2H);
	st.secondValid = true;
	BackPlan before = PlanBack(s, st);
	CHECK(PrimaryAtBAfterSwap(before));
	st.primaryAtB = true;
	BackPlan after = PlanBack(s, st);
	CHECK(after.primarySpot == before.secondSpot && after.secondSpot == before.primarySpot);
	CHECK(!PrimaryAtBAfterSwap(after));
}

TEST(back_mat_mul_inverse_and_adjust) {
	const float adjA[6] = { 10.0f, -5.0f, 2.0f, 30.0f, -20.0f, 45.0f };
	BackMat a = BackAdjust(BackIdentity(), adjA);
	BackMat i = BackMul(a, BackInverse(a));
	for (int r = 0; r < 3; r++)
		for (int c = 0; c < 4; c++)
			CHECK(fabsf(i.m[r][c] - (r == c ? 1.0f : 0.0f)) < 1e-4f);
	// przesuniecie w ukladzie miecza: obrot podstawy dziala na wektor przesuniecia
	const float rz90[6] = { 0, 0, 0, 0, 0, 90.0f };   // Rz 90: x -> y
	const float x5[6] = { 5.0f, 0, 0, 0, 0, 0 };
	BackMat base = BackAdjust(BackIdentity(), rz90);
	BackMat moved = BackAdjust(base, x5);
	CHECK(fabsf(moved.m[0][3]) < 1e-4f && fabsf(moved.m[1][3] - 5.0f) < 1e-4f);
	CHECK(fabsf(BackMatDistance(moved, base) - 5.0f) < 1e-4f);
}

TEST(back_ini_parses_positions_and_rejects_broken) {
	BackMat a, b;
	std::string err;
	std::string ok = "; komentarz\r\n[BACKSWORDS]\r\nPosA=1 0 0 35.5 0 1 0 -17.5 0 0 1 -11.4\r\nPosB=1 0 0 28.7 0 1 0 -27.9 0 0 1 -12\r\n"
		"Draw1HA=9 8 16\r\n";
	CHECK(ParseBackIni(ok, a, b, err));
	CHECK(fabsf(a.m[0][3] - 35.5f) < 1e-4f && fabsf(b.m[1][3] + 27.9f) < 1e-4f && fabsf(b.m[2][2] - 1.0f) < 1e-6f);
	CHECK(!ParseBackIni("PosA=1 2 3\r\nPosB=1 0 0 0 0 1 0 0 0 0 1 0\r\n", a, b, err) && err.find("POSA") != std::string::npos);
	CHECK(!ParseBackIni("PosA=1 0 0 0 0 1 0 0 0 0 1 0\r\n", a, b, err) && err == "brak PosB");
	float v[6];
	CHECK(ParseFloats("1 2.5 -3 0 0 15", v, 6) && v[1] == 2.5f && v[5] == 15.0f);
	CHECK(!ParseFloats("1 2 x", v, 3));
	CHECK(ParseFloats("1,2,3 ; uwaga", v, 3) && v[2] == 3.0f);
}

TEST(back_name_hash_is_case_insensitive_and_never_zero) {
	CHECK(BackNameHash("ITMW_1H_SWORD_01") == BackNameHash("ItMw_1h_Sword_01"));
	CHECK(BackNameHash("ITMW_1H_SWORD_01") != BackNameHash("ITMW_1H_SWORD_02"));
	CHECK(BackNameHash("") != 0u && (BackNameHash("X") & 0x80000000u) == 0u);
}

TEST(back_swap_sheathe_equip_draw_sequence) {
	BackSwapRun r;
	BackSwapStart(r);
	BackSwapTick t;
	t.dt = 0.02f;
	t.weaponMode = 3;
	CHECK(BackSwapStep(r, t) == SWA_REMOVE);
	t.weaponMsg = true;
	CHECK(BackSwapStep(r, t) == SWA_NONE);
	t.weaponMode = 0;
	CHECK(BackSwapStep(r, t) == SWA_NONE);   // komunikat schowania jeszcze w kolejce
	t.weaponMsg = false;
	CHECK(BackSwapStep(r, t) == SWA_EQUIP && r.phase == SWP_EQUIP);
	t.newEquipped = true;
	CHECK(BackSwapStep(r, t) == SWA_DRAW && r.phase == SWP_DRAW);
	t.weaponMsg = true;
	CHECK(BackSwapStep(r, t) == SWA_NONE);
	t.weaponMsg = false;
	t.weaponMode = 4;
	CHECK(BackSwapStep(r, t) == SWA_DONE && r.phase == SWP_IDLE);
}

TEST(back_swap_fails_safely) {
	BackSwapRun r;
	BackSwapStart(r);
	BackSwapTick t;
	t.dt = 0.1f;
	t.weaponMode = 3;
	CHECK(BackSwapStep(r, t) == SWA_REMOVE);
	t.weaponMsg = true;
	for (int i = 0; i < 29; i++)
		CHECK(BackSwapStep(r, t) == SWA_NONE);
	CHECK(BackSwapStep(r, t) == SWA_FAIL && r.phase == SWP_IDLE);   // zaciecie chowania > 3 s
	BackSwapStart(r);
	t.weaponMsg = false;
	t.weaponMode = 0;
	CHECK(BackSwapStep(r, t) == SWA_REMOVE);
	CHECK(BackSwapStep(r, t) == SWA_EQUIP);
	t.newEquipped = false;   // wymagania - gra nie zalozyla
	CHECK(BackSwapStep(r, t) == SWA_FAIL_REDRAW && r.phase == SWP_IDLE);
	// komunikat chowania zniknal (trafienie), bron dalej w rece - koniec po BACK_SWAP_GRACE
	BackSwapStart(r);
	t.newEquipped = false;
	t.weaponMode = 3;
	t.weaponMsg = false;
	CHECK(BackSwapStep(r, t) == SWA_REMOVE);
	CHECK(BackSwapStep(r, t) == SWA_NONE);   // 0,2 s - jeszcze w karencji
	CHECK(BackSwapStep(r, t) == SWA_FAIL && r.why.find("przerwane") != std::string::npos);
	BackSwapStart(r);
	t.interrupted = true;
	CHECK(BackSwapStep(r, t) == SWA_FAIL && r.why == "przerwana");
	CHECK(BackSwapStep(r, t) == SWA_NONE);   // po koncu nic
}

TEST(back_swap_refusal_reasons) {
	BackSwapGate g;
	CHECK(BackSwapRefusal(g).find("brak drugiego") == 0);
	g.secondValid = true;
	CHECK(BackSwapRefusal(g).empty());
	g.busy = true;
	g.busyWhat = "finisher";
	CHECK(BackSwapRefusal(g) == "trwa finisher");
	g.busy = false;
	g.dialog = true;
	CHECK(BackSwapRefusal(g) == "rozmowa");
	g.swapping = true;
	CHECK(BackSwapRefusal(g) == "zamiana juz trwa");
}

TEST(back_menu_values_cover_defaults) {
	auto has = [](const std::vector<int>& v, int x) { for (int y : v) if (y == x) return true; return false; };
	CHECK(has(BackSpeedMenuValues(), 100));
	CHECK(has(BeltSpeedMenuValues(), 150) && BeltSpeedMenuValues()[0] == 100);
	CHECK(SecondKeyMenuValues()[0] == 34 && SecondKeyMenuValues().back() == 0);
}

// ---------- 23.10 dualfix: bronie parowe NB, RESTART w jednej klatce, straznik miecza na plecach, chowanie w przeskoku
TEST(swap_restart_counts_frames_not_starts_in_one_frame) {
	SwapRestartWatch w;
	// skrypt NB przy zalozeniu broni: 22 zdjecia / zalozenia nakladek w jednej klatce, kazde wznawia postawe
	for (int i = 0; i < 22; i++)
		CHECK(!SwapRestartStep(w, 5, 16598, 4, 60));
	for (int i = 0; i < 2; i++)
		CHECK(!SwapRestartStep(w, 5, 16599, 4, 60));
	CHECK(w.count == 2);
	// prawdziwe drganie: postawa od nowa w kolejnych klatkach
	SwapRestartWatch v;
	CHECK(!SwapRestartStep(v, 5, 10, 4, 60));
	CHECK(!SwapRestartStep(v, 5, 11, 4, 60) && !SwapRestartStep(v, 5, 11, 4, 60));
	CHECK(!SwapRestartStep(v, 5, 12, 4, 60));
	CHECK(SwapRestartStep(v, 5, 13, 4, 60));
}

TEST(dual_state_from_nb_script_vars) {
	DualInput in;
	in.weapon = "ITMW_1H_DUAL_STEEL_RIGHT";
	in.weaponType = SWAP_TYPE_2H;
	in.rhVar = 1;
	in.lhVar = 0;
	CHECK(DualState(in) == DUAL_RIGHT);
	in.lhVar = 1;
	CHECK(DualState(in) == DUAL_PAIR);
	in.rhVar = 0;
	CHECK(DualState(in) == DUAL_NONE);            // RX_UNEQUIPWEAPON zeruje - zwykla bron
	in.rhVar = 1;
	in.weaponType = SWAP_TYPE_NONE;               // bez broni bialej zmiennym nie ufamy
	CHECK(DualState(in) == DUAL_NONE);
	CHECK(std::string(DualLabel(DUAL_PAIR)) == "para (2x2)" && std::string(DualLabel(DUAL_NONE)) == "-");
}

TEST(dual_state_without_script_vars_uses_nb_overlays) {
	DualInput in;                                 // Steam G2 / Kroniki: brak zmiennych NB
	in.weapon = "ITMW_2H_ORCAXE_01";
	in.weaponType = SWAP_TYPE_2H;
	in.overlays = { "HUMANS_2HST1", "HUMANS_1X2ST3.MDS" };
	CHECK(DualState(in) == DUAL_NONE);            // HUMANS_1X2ST3 to tez styl zEBA - bez broni *_RIGHT nie para
	in.weapon = "itmw_2h_cheldrak_right";
	CHECK(DualState(in) == DUAL_RIGHT);
	in.overlays = { "Humans_2x2ST3.MDS" };
	CHECK(DualState(in) == DUAL_PAIR);
	in.overlays.clear();
	CHECK(DualState(in) == DUAL_NONE);
}

TEST(back_plan_dual_pair_is_native) {
	BackSettings s;
	BackState st = BsState(BACK_TYPE_2H);
	st.dualPair = true;
	BackPlan p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_NATIVE && p.primaryNode == BNODE_NONE);
	CHECK(p.draw2H == DRAWV_NATIVE);
	st.secondValid = true;                        // drugi miecz moze wisiec dalej (ZS_BOW)
	p = PlanBack(s, st);
	CHECK(p.second && p.secondSpot == SPOT_B && p.primarySpot == SPOT_NATIVE);
	st.dualPair = false;
	p = PlanBack(s, st);
	CHECK(p.primarySpot == SPOT_A && p.primaryNode == BNODE_LONGSWORD && p.draw2H == DRAWV_A);
}

TEST(back_swap_refused_for_dual_pair) {
	BackSwapGate g;
	g.secondValid = true;
	g.dualPair = true;
	CHECK(BackSwapRefusal(g).find("parowa") != std::string::npos);
	g.dualPair = false;
	CHECK(BackSwapRefusal(g).empty());
}

TEST(back_offset_error_detects_stale_chain) {
	BackMat want = BackIdentity();
	want.m[0][3] = 35.5f;
	want.m[1][3] = -17.5f;
	want.m[2][3] = -11.4f;
	BackMat spine = BackIdentity();
	spine.m[1][3] = 120.0f;                       // kregoslup 120 cm nad korzeniem
	BackMat node = BackMul(spine, want);
	CHECK(BackOffsetError(spine, node, want) < 0.01f);
	BackMat stale = node;
	stale.m[1][3] -= 100.0f;                      // miecz metr nizej (lancuch wezlow nieaktualny)
	CHECK(BackOffsetError(spine, stale, want) > BACK_GUARD_TOL);
	BackMat bad = node;
	bad.m[0][0] = std::nanf("");
	CHECK(BackOffsetError(spine, bad, want) < 0.0f);
	CHECK(!BackMatFinite(bad) && BackMatFinite(node));
}

TEST(back_render_drift_ignores_stale_renderer) {
	float d = 0.0f;
	CHECK(!BackRenderDrift(42.0f, 41.2f, 30.0f, 30.0f, d) && d < 1.0f);
	CHECK(BackRenderDrift(120.0f, 41.2f, 30.0f, 31.0f, d) && d > 70.0f);   // miecz daleko od plecow
	CHECK(!BackRenderDrift(120.0f, 41.2f, 150.0f, 30.0f, d));             // kontrolne tez rozjechane - renderer
}

TEST(trav_sheathe_decide) {
	TravSheatheIn in;
	const char* why = nullptr;
	CHECK(TravSheatheDecide(in, why) == TRAVSH_NONE);
	in.weaponMode = 3;
	CHECK(TravSheatheDecide(in, why) == TRAVSH_SHEATHE && !why);
	in.weaponMode = 1;                            // piesci - nic do chowania w slocie, ale tryb walki tak
	CHECK(TravSheatheDecide(in, why) == TRAVSH_SHEATHE);
	in.weaponMode = 4;
	in.leftHandBusy = true;                       // pochodnia / tarcza / lewa bron pary NB
	CHECK(TravSheatheDecide(in, why) == TRAVSH_KEEP && why);
	in.leftHandBusy = false;
	in.weaponMsg = true;
	CHECK(TravSheatheDecide(in, why) == TRAVSH_KEEP);
	in.weaponMsg = false;
	in.enabled = false;
	CHECK(TravSheatheDecide(in, why) == TRAVSH_KEEP && std::string(why) == "TraversalSheathe=0");
	in.enabled = true;
	in.weaponMode = 5;
	CHECK(TravSheatheDecide(in, why) == TRAVSH_KEEP);
}

TEST(trav_redraw_mode) {
	const char* why = nullptr;
	CHECK(TravRedrawMode(true, 3, 0, true, false, true, why) == 3 && !why);
	CHECK(TravRedrawMode(true, 1, 0, true, false, false, why) == 1);   // piesci bez przedmiotu
	CHECK(TravRedrawMode(true, 0, 0, true, false, true, why) == 0 && !why);
	CHECK(TravRedrawMode(false, 3, 0, true, false, true, why) == 0 && why);
	CHECK(TravRedrawMode(true, 3, 0, false, false, true, why) == 0);
	CHECK(TravRedrawMode(true, 3, 0, true, true, true, why) == 0);
	CHECK(TravRedrawMode(true, 4, 3, true, false, true, why) == 0);    // gracz sam dobyl
	CHECK(TravRedrawMode(true, 4, 0, true, false, false, why) == 0);   // bron zdjeta
}

// ---------- Kusza: szybki strzal (Core\Crossbow) ----------
#include "Core/Crossbow.h"

TEST(xbow_gate_order) {
	XbowGate g;
	g.hasCrossbow = true;
	g.hasBolts = true;
	CHECK(XbowGateCheck(g) == XR_OK);
	XbowGate a = g; a.enabled = false; CHECK(XbowGateCheck(a) == XR_OFF);
	a = g; a.pack = false; CHECK(XbowGateCheck(a) == XR_NOPACK);
	a = g; a.fmode = 6; CHECK(XbowGateCheck(a) == XR_MODE);
	a = g; a.fmode = 7; CHECK(XbowGateCheck(a) == XR_MODE);
	a = g; a.fmode = 5; CHECK(XbowGateCheck(a) == XR_MODE);
	a = g; a.fmode = 3; CHECK(XbowGateCheck(a) == XR_OK);
	a = g; a.fmode = 8; CHECK(XbowGateCheck(a) == XR_OK);          // NB bron parowa
	a = g; a.fmode = 9; CHECK(XbowGateCheck(a) == XR_OK);          // NB tarcza
	a = g; a.hasCrossbow = false; CHECK(XbowGateCheck(a) == XR_NOXBOW);
	a = g; a.hasCrossbow = false; a.isBow = true; CHECK(XbowGateCheck(a) == XR_BOW);
	a = g; a.torch = true; CHECK(XbowGateCheck(a) == XR_TORCH);
	a = g; a.torch = true; a.allowTorch = true; CHECK(XbowGateCheck(a) == XR_OK);
	a = g; a.shield = true; CHECK(XbowGateCheck(a) == XR_OK);       // decyzja 23.10: tarcza dozwolona
	a = g; a.shield = true; a.allowShield = false; CHECK(XbowGateCheck(a) == XR_SHIELD);
	a = g; a.dual = true; CHECK(XbowGateCheck(a) == XR_OK);         // druga bron ukryta na czas strzalu
	a = g; a.dual = true; a.allowDual = false; CHECK(XbowGateCheck(a) == XR_DUAL);
	a = g; a.hasBolts = false; CHECK(XbowGateCheck(a) == XR_NOBOLTS);
	a = g; a.hasBolts = false; a.createAmmo = true; CHECK(XbowGateCheck(a) == XR_OK);
	a = g; a.cooldownLeftMs = 100; CHECK(XbowGateCheck(a) == XR_COOLDOWN);
	a = g; a.water = true; a.hasCrossbow = false; CHECK(XbowGateCheck(a) == XR_WATER);   // woda przed kusza
	a = g; a.active = true; CHECK(XbowGateCheck(a) == XR_ACTIVE);
	a = g; a.dialog = true; CHECK(XbowGateCheck(a) == XR_DIALOG);
	for (int r = 0; r < XR_COUNT; r++)
		CHECK(XbowRefuseLabel(r) && *XbowRefuseLabel(r));
	CHECK(*XbowRefuseMessage(XR_NOBOLTS) && *XbowRefuseMessage(XR_TORCH) && !*XbowRefuseMessage(XR_DIALOG));
}

TEST(xbow_ini_parse) {
	XbowTimes t;
	std::string err;
	CHECK(ParseXbowIni("; kusza\n[XBOW]\nInMs=520\nLoopMs=1040\nShootMs=1040\nGrabMs=200\nFireMs=0\nReleaseMs=560\nTurnMs=210\n", t, err));
	CHECK(fabsf(t.inLen - 0.52f) < 1e-4f && fabsf(t.release - 0.56f) < 1e-4f && fabsf(t.turn - 0.21f) < 1e-4f);
	XbowTimes u;
	CHECK(!ParseXbowIni("GrabMs=200\n", u, err) && err.find("InMs") != std::string::npos);
	CHECK(ParseXbowIni("InMs=500\nLoopMs=1000\nShootMs=800\nReleaseMs=5000\nGrabMs=900\n", u, err));
	CHECK(fabsf(u.release - 0.8f) < 1e-4f && fabsf(u.grab - 0.5f) < 1e-4f);   // przyciete do dlugosci klipow
	XbowTimes v;
	CHECK(!ParseXbowIni("InMs=abc\nLoopMs=1\nShootMs=1\n", v, err));
	CHECK(fabsf(XbowTimeScale(100) - 1.0f) < 1e-5f && fabsf(XbowTimeScale(200) - 0.5f) < 1e-5f);
	CHECK(fabsf(XbowTimeScale(1) - 4.0f) < 1e-5f && fabsf(XbowTimeScale(1000) - 0.25f) < 1e-5f);
}

static XbowEvents XbowRunFor(XbowRun& r, const XbowTimes& tm, float scale, float seconds, XbowEvents* acc) {
	XbowEvents last;
	for (float t = 0.0f; t < seconds && r.Active(); t += 0.01f) {
		last = XbowStep(r, tm, scale, 0.01f);
		acc->startLoop |= last.startLoop; acc->startShoot |= last.startShoot; acc->grab |= last.grab;
		acc->fire |= last.fire; acc->release |= last.release; acc->end |= last.end;
	}
	return last;
}

TEST(xbow_tap_fires_after_entry) {
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	CHECK(r.phase == XP_IN);
	CHECK(XbowApplyInput(r, XIN_RELEASE) && r.intent == XI_TAP);   // stukniecie: puszczenie przed progiem
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, tm.inLen - 0.02f, &acc);
	CHECK(acc.grab && !acc.fire && r.phase == XP_IN);
	XbowRunFor(r, tm, 1.0f, 0.05f, &acc);
	CHECK(acc.startShoot && acc.fire && !acc.startLoop && r.fired);
	XbowRun r2;
	XbowStart(r2);
	XbowApplyInput(r2, XIN_RELEASE);
	while (r2.Active() && !XbowStep(r2, tm, 1.0f, 0.01f).fire) {}
	CHECK(fabsf(r2.total - XbowTapFireAt(tm, 1.0f)) < 0.015f);
	XbowRunFor(r, tm, 1.0f, 5.0f, &acc);
	CHECK(acc.release && acc.end && !r.Active());
}

TEST(xbow_no_decision_is_tap) {
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, 3.0f, &acc);
	CHECK(acc.fire && acc.end && !r.aimed);
}

TEST(xbow_hold_loops_until_release) {
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	CHECK(XbowApplyInput(r, XIN_HOLD) && r.intent == XI_HOLD);
	CHECK(!XbowApplyInput(r, XIN_HOLD));
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, 3.0f, &acc);
	CHECK(acc.startLoop && !acc.fire && r.phase == XP_LOOP && r.aimed);
	CHECK(XbowApplyInput(r, XIN_RELEASE) && r.intent == XI_RELEASED);
	XbowEvents one = XbowStep(r, tm, 1.0f, 0.01f);
	CHECK(one.startShoot && one.fire && r.phase == XP_SHOOT);
	XbowRunFor(r, tm, 1.0f, 3.0f, &acc);
	CHECK(acc.end && !r.Active());
}

TEST(xbow_hold_released_during_entry_shoots_at_entry_end) {
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	XbowApplyInput(r, XIN_HOLD);
	XbowStep(r, tm, 1.0f, 0.35f);
	CHECK(XbowApplyInput(r, XIN_RELEASE) && r.intent == XI_RELEASED);
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, 0.3f, &acc);
	CHECK(acc.fire && !acc.startLoop);
}

TEST(xbow_speed_scales_moments) {
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	XbowApplyInput(r, XIN_RELEASE);
	XbowEvents acc;
	XbowRunFor(r, tm, XbowTimeScale(200), tm.inLen * 0.5f + 0.02f, &acc);
	CHECK(acc.fire);   // 200 % = dwa razy szybciej
	CHECK(fabsf(XbowTapFireAt(tm, 0.5f) - (tm.inLen + tm.fire) * 0.5f) < 1e-5f);
}

TEST(xbow_cancel_and_cuts) {
	XbowTimes tm;
	XbowRun r;
	CHECK(XbowCutVerdict(r, XC_HIT) == XCA_IGNORE);   // nic nie trwa
	XbowStart(r);
	CHECK(XbowCutVerdict(r, XC_HIT) == XCA_ABORT_FREE);
	CHECK(XbowCutVerdict(r, XC_MOVE) == XCA_ABORT_FREE);
	CHECK(XbowApplyInput(r, XIN_CANCEL) && r.intent == XI_CANCEL);
	XbowStart(r);
	XbowApplyInput(r, XIN_RELEASE);
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, tm.inLen + 0.05f, &acc);
	CHECK(r.fired && r.Active());
	CHECK(XbowCutVerdict(r, XC_HIT) == XCA_IGNORE);          // po belcie trafienie nie przerywa
	CHECK(XbowCutVerdict(r, XC_MOVE) == XCA_ABORT_AFTER);
	CHECK(XbowCutVerdict(r, XC_INVENTORY) == XCA_ABORT_AFTER);
	CHECK(XbowCutVerdict(r, XC_CANCEL) == XCA_IGNORE);
	CHECK(!XbowApplyInput(r, XIN_CANCEL));                   // po belcie PPM nic nie zmienia
	for (int c = 0; c < XC_COUNT; c++)
		CHECK(*XbowCutLabel(c) != '?');
}

TEST(xbow_target_pick) {
	XbowAimRules rules;
	std::vector<XbowCand> c(3);
	c[0].dx = 0.0f; c[0].dz = 1000.0f; c[0].hostile = true;            // na wprost, 10 m
	c[1].dx = 300.0f; c[1].dz = 600.0f; c[1].hostile = true;           // 27 st., blizej
	c[2].dx = -50.0f; c[2].dz = 400.0f; c[2].hostile = false;          // neutralny, 7 st. i 4 m
	int i = XbowPickTarget(c, 0.0f, 1.0f, 0.0f, 1.0f, false, rules);
	CHECK(i == 0 || i == 1);
	c[2].hostile = true;
	CHECK(XbowPickTarget(c, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == 2);   // najlepsza ocena
	c[2].hostile = false;
	rules.neutral = true;
	CHECK(XbowPickTarget(c, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == 2);   // XbowAutoNeutral=1
	rules.neutral = false;
	// fokus wygrywa nawet poza stozkiem kamery, byle przed bohaterem
	std::vector<XbowCand> f(2);
	f[0].dx = 0.0f; f[0].dz = 500.0f; f[0].hostile = true;
	f[1].dx = 800.0f; f[1].dz = 300.0f; f[1].hostile = true; f[1].focus = true;   // 69 st. od kamery
	CHECK(XbowPickTarget(f, 0.0f, 1.0f, 1.0f, 0.4f, false, rules) == 1);
	f[1].dx = 0.0f; f[1].dz = -500.0f;                                             // za bohaterem
	CHECK(XbowPickTarget(f, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == 0);
	// martwy, zakryty, za daleko, za wysoko
	std::vector<XbowCand> d(1);
	d[0].dz = 500.0f; d[0].hostile = true; d[0].alive = false;
	CHECK(XbowPickTarget(d, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == -1);
	d[0].alive = true; d[0].visible = false;
	CHECK(XbowPickTarget(d, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == -1);
	d[0].visible = true; d[0].dz = 4000.0f;
	CHECK(XbowPickTarget(d, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == -1);
	d[0].dz = 500.0f; d[0].dy = 900.0f;
	CHECK(XbowPickTarget(d, 0.0f, 1.0f, 0.0f, 1.0f, false, rules) == -1);
	// w ruchu: cel w stozku kamery, ale 120 st. od kierunku biegu - odrzucony
	std::vector<XbowCand> m(1);
	m[0].dx = 500.0f; m[0].dz = 0.0f; m[0].hostile = true;
	CHECK(XbowPickTarget(m, 1.0f, 0.0f, -0.5f, 0.87f, false, rules) == 0);
	CHECK(XbowPickTarget(m, 1.0f, 0.0f, -0.5f, 0.87f, true, rules) == -1);
	CHECK(fabsf(XbowAngleDeg(0.0f, 1.0f, 1.0f, 0.0f) - 90.0f) < 0.01f);
	CHECK(XbowAngleDeg(0.0f, 0.0f, 1.0f, 0.0f) == 180.0f);
}

TEST(xbow_cooldown_by_talent) {
	int ms[3] = { 1320, 1200, 960 };
	CHECK(XbowCooldownMs(ms, 0) == 1320 && XbowCooldownMs(ms, 1) == 1200 && XbowCooldownMs(ms, 2) == 960);
	CHECK(XbowCooldownMs(ms, -3) == 1320 && XbowCooldownMs(ms, 7) == 960);
	int neg[3] = { -5, 0, 0 };
	CHECK(XbowCooldownMs(neg, 0) == 0);
}

TEST(xbow_borrow_slot) {
	int why = -1;
	// baza, nasza (signs_stand) z wpisem - pisze w nasza
	std::vector<XbowProtoInfo> p = { { false, true }, { true, true } };
	CHECK(XbowBorrowSlot(p, &why) == 1 && why == XB_OK);
	// obca nad nasza z wpisem (np. zEBA HUMANS_REMASTER) - odmowa
	p = { { false, true }, { true, true }, { false, true } };
	CHECK(XbowBorrowSlot(p, &why) == -1 && why == XB_FOREIGN_ABOVE);
	// obca nad nasza bez wpisu - bez znaczenia
	p = { { false, true }, { true, true }, { false, false } };
	CHECK(XbowBorrowSlot(p, &why) == 1);
	// rozwiazuje baza: najwyzsza nasza (pusty wpis)
	p = { { false, true }, { true, false }, { false, false }, { true, false } };
	CHECK(XbowBorrowSlot(p, &why) == 3 && why == XB_OK);
	// brak naszych
	p = { { false, true }, { false, false } };
	CHECK(XbowBorrowSlot(p, &why) == -1 && why == XB_NO_OURS);
	p.clear();
	CHECK(XbowBorrowSlot(p, &why) == -1 && why == XB_EMPTY);
	// obca z wpisem nizej, nasza wyzej z wpisem - nasza
	p = { { false, true }, { false, true }, { true, true } };
	CHECK(XbowBorrowSlot(p, &why) == 2);
}

TEST(xbow_clip_names_and_slot_bits) {
	CHECK(std::string(XbowClipAni(XCLIP_IN, true)) == "T_FOOD_RANDOM_1");
	CHECK(std::string(XbowClipAni(XCLIP_SHOOT, false)) == "R_CHAIR_RANDOM_3");
	CHECK(*XbowClipAni(XCLIP_COUNT, true) == 0);
	for (int c = 0; c < XCLIP_COUNT; c++)
		CHECK(std::string(XbowClipAni(c, true)) != XbowClipAni(c, false));
	CHECK(XbowSlotSelected(XbowSlotValue(0, true)) && !XbowSlotSelected(XbowSlotValue(5, false)));
	CHECK(XbowSlotValue(4, true) == 5 && XbowSlotValue(5, false) == 4);
}

TEST(xbow_cancel_out_plays_holster_without_bolt) {
	// PPM z kusza w dloni: odlozenie z XBOW_SHOOT od ShootOutMs, bez zdarzenia fire (bez beltu, bez przeladowania)
	XbowTimes tm;
	XbowRun r;
	XbowStart(r);
	CHECK(!XbowCancelOut(r, tm, 1.0f));                      // przed chwytem - zwykle przerwanie
	XbowApplyInput(r, XIN_HOLD);
	XbowEvents acc;
	XbowRunFor(r, tm, 1.0f, tm.inLen + 0.2f, &acc);
	CHECK(r.grabbed && r.phase == XP_LOOP && !r.fired);
	CHECK(XbowCancelOut(r, tm, 1.0f));
	CHECK(r.phase == XP_SHOOT && fabsf(r.t - tm.outStart) < 1e-5f && r.intent == XI_CANCEL);
	XbowEvents out;
	XbowRunFor(r, tm, 1.0f, 3.0f, &out);
	CHECK(!out.fire && out.release && out.end && !r.Active());
	CHECK(!XbowCancelOut(r, tm, 1.0f));                      // nic nie trwa
	// po belcie anulowanie nic nie zmienia
	XbowStart(r);
	XbowApplyInput(r, XIN_RELEASE);
	XbowEvents a2;
	XbowRunFor(r, tm, 1.0f, tm.inLen + 0.05f, &a2);
	CHECK(r.fired && !XbowCancelOut(r, tm, 1.0f));
}

TEST(xbow_ini_out_and_grip) {
	XbowTimes t;
	std::string err;
	CHECK(ParseXbowIni("InMs=520\nLoopMs=1040\nShootMs=1040\nReleaseMs=720\nShootOutMs=320\nGripCm=-35\n", t, err));
	CHECK(fabsf(t.outStart - 0.32f) < 1e-4f && fabsf(t.gripCm + 35.0f) < 1e-4f);
	XbowTimes u;
	CHECK(ParseXbowIni("InMs=520\nLoopMs=1040\nShootMs=1040\nReleaseMs=700\nShootOutMs=900\n", u, err));
	CHECK(fabsf(u.outStart - 0.7f) < 1e-4f);                // nie po odlozeniu
	XbowTimes v;
	CHECK(!ParseXbowIni("InMs=520\nLoopMs=1040\nShootMs=1040\nGripCm=900\n", v, err));
}

#include "test_wheel.inc"
#include "test_backkey.inc"
#include "test_travbounce.inc"

int main() {
	for (auto& t : Tests()) {
		int before = g_fail;
		t.fn();
		printf("%s %s\n", g_fail == before ? "ok  " : "FAIL", t.name);
	}
	printf("\n%d sprawdzen OK, %d bledow\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
