// Testy menu kolowego (Core\Wheel.*) - dolaczane z test_core.cpp.
#include "Core/Wheel.h"

// ---------- Wheel: klawisz ----------
TEST(wheel_tap_before_threshold_goes_to_game) {
	WheelKey k;
	CHECK(WheelKeyPress(k, 1000.0));
	CHECK(WheelKeyStep(k, 1050.0, true, true, 250) == WKA_NONE);
	CHECK(WheelKeyStep(k, 1100.0, false, true, 250) == WKA_TAP);
	CHECK(k.state == WK_IDLE);
}
TEST(wheel_hold_opens_and_release_confirms) {
	WheelKey k;
	WheelKeyPress(k, 0.0);
	CHECK(WheelKeyStep(k, 249.0, true, true, 250) == WKA_NONE);
	CHECK(WheelKeyStep(k, 250.0, true, true, 250) == WKA_OPEN);
	CHECK(k.state == WK_OPEN);
	CHECK(WheelKeyStep(k, 900.0, true, true, 250) == WKA_NONE);
	CHECK(WheelKeyStep(k, 950.0, false, true, 250) == WKA_CONFIRM);
	CHECK(k.state == WK_IDLE);
}
TEST(wheel_press_only_from_idle) {
	WheelKey k;
	CHECK(WheelKeyPress(k, 0.0));
	CHECK(!WheelKeyPress(k, 10.0));
	CHECK(k.pressMs == 0.0);
}
TEST(wheel_focus_loss_cancels_without_inventory) {
	WheelKey k;
	WheelKeyPress(k, 0.0);
	CHECK(WheelKeyStep(k, 50.0, true, false, 250) == WKA_LOST);
	CHECK(k.state == WK_IDLE);
	WheelKeyPress(k, 100.0);
	WheelKeyStep(k, 400.0, true, true, 250);
	CHECK(WheelKeyStep(k, 500.0, true, false, 250) == WKA_LOST);
}
TEST(wheel_closed_by_click_waits_for_release) {
	WheelKey k;
	WheelKeyPress(k, 0.0);
	WheelKeyStep(k, 300.0, true, true, 250);
	WheelKeyClosed(k, true);
	CHECK(k.state == WK_WAIT_RELEASE);
	CHECK(!WheelKeyPress(k, 350.0));                        // trzymany - bez ponownego uzbrojenia
	CHECK(WheelKeyStep(k, 2000.0, true, true, 250) == WKA_NONE);
	CHECK(WheelKeyStep(k, 2010.0, false, true, 250) == WKA_NONE);
	CHECK(k.state == WK_IDLE);
	WheelKeyClosed(k, false);
	CHECK(k.state == WK_IDLE);
}
TEST(wheel_zero_threshold_opens_at_once) {
	WheelKey k;
	WheelKeyPress(k, 0.0);
	CHECK(WheelKeyStep(k, 0.0, true, true, 0) == WKA_OPEN);
}

// ---------- Wheel: uklad ----------
TEST(wheel_spell_order_by_key_then_index_unbound_last) {
	std::vector<int> key = { 0, 3, 1, 3, 0, 2 };
	std::vector<int> o = WheelSpellOrder(key);
	CHECK(o.size() == 6);
	CHECK(o.size() == 6 && o[0] == 2 && o[1] == 5 && o[2] == 1 && o[3] == 3 && o[4] == 0 && o[5] == 4);
}
TEST(wheel_pages_seven_with_xbow_eight_without) {
	CHECK(WheelSpellsPerPage(true) == 7 && WheelSpellsPerPage(false) == 8);
	CHECK(WheelPageCount(0, true) == 1);
	CHECK(WheelPageCount(7, true) == 1);
	CHECK(WheelPageCount(8, true) == 2);
	CHECK(WheelPageCount(8, false) == 1);
	CHECK(WheelPageCount(22, true) == 4);
}
TEST(wheel_xbow_pinned_bottom_every_page) {
	std::vector<int> order;
	for (int i = 0; i < 20; i++)
		order.push_back(100 + i);
	for (int p = 0; p < 3; p++) {
		WheelEntry e = WheelSlotEntry(order, true, p, WHEEL_XBOW_SLOT);
		CHECK(e.kind == WE_XBOW);
	}
	// strona 0: pola 0..3 = czary 0..3, pole 5 = czar 4, pole 7 = czar 6
	CHECK(WheelSlotEntry(order, true, 0, 0).spell == 100);
	CHECK(WheelSlotEntry(order, true, 0, 3).spell == 103);
	CHECK(WheelSlotEntry(order, true, 0, 5).spell == 104);
	CHECK(WheelSlotEntry(order, true, 0, 7).spell == 106);
	CHECK(WheelSlotEntry(order, true, 1, 0).spell == 107);
	// strona 2: czary 14..19, pola 6 i 7 puste (20 czarow)
	CHECK(WheelSlotEntry(order, true, 2, 5).spell == 118);
	CHECK(WheelSlotEntry(order, true, 2, 6).spell == 119);
	CHECK(WheelSlotEntry(order, true, 2, 7).kind == WE_EMPTY);
	// bez kuszy dol to zwykle pole
	CHECK(WheelSlotEntry(order, false, 0, WHEEL_XBOW_SLOT).spell == 104);
	CHECK(WheelSlotEntry(order, false, 1, 0).spell == 108);
	CHECK(WheelSlotEntry(order, true, 0, -1).kind == WE_EMPTY && WheelSlotEntry(order, true, 0, 8).kind == WE_EMPTY);
}
TEST(wheel_find_active_spell_and_xbow) {
	std::vector<int> order;
	for (int i = 0; i < 20; i++)
		order.push_back(i);
	int page = -1, slot = -1;
	CHECK(WheelFindActive(order, true, false, 4, page, slot));
	CHECK(page == 0 && slot == 5);
	CHECK(WheelFindActive(order, true, false, 9, page, slot));
	CHECK(page == 1 && slot == 2);
	CHECK(WheelSlotEntry(order, true, page, slot).spell == 9);
	CHECK(WheelFindActive(order, true, true, 9, page, slot));
	CHECK(slot == WHEEL_XBOW_SLOT);
	CHECK(!WheelFindActive(order, true, false, 55, page, slot));
	CHECK(page == 0 && slot == -1);
	// kazdy czar znajdowany tam, gdzie lezy
	for (int s = 0; s < 20; s++) {
		bool ok = WheelFindActive(order, false, false, s, page, slot);
		CHECK(ok && WheelSlotEntry(order, false, page, slot).spell == s);
		ok = WheelFindActive(order, true, false, s, page, slot);
		CHECK(ok && WheelSlotEntry(order, true, page, slot).spell == s);
	}
}
TEST(wheel_slot_centers_clockwise_from_top) {
	float x = 0, y = 0;
	WheelSlotCenter(0, 100.0f, x, y);
	CHECK(fabsf(x) < 0.01f && fabsf(y + 100.0f) < 0.01f);
	WheelSlotCenter(2, 100.0f, x, y);
	CHECK(fabsf(x - 100.0f) < 0.01f && fabsf(y) < 0.01f);
	WheelSlotCenter(4, 100.0f, x, y);
	CHECK(fabsf(x) < 0.01f && fabsf(y - 100.0f) < 0.01f);
	WheelSlotCenter(6, 100.0f, x, y);
	CHECK(fabsf(x + 100.0f) < 0.01f);
	for (int s = 0; s < WHEEL_SLOTS; s++) {
		WheelSlotCenter(s, 100.0f, x, y);
		CHECK(WheelSlotFromVec(x, y, 40.0f) == s);
	}
}

// ---------- Wheel: mysz ----------
TEST(wheel_mouse_dead_zone_keeps_selection) {
	CHECK(WheelSlotFromVec(10.0f, -10.0f, 40.0f) == -1);
	WheelMouse m;
	CHECK(!WheelMouseMove(m, 5.0f, 0.0f, 120.0f, 40.0f));
	CHECK(m.slot == -1);
	CHECK(WheelMouseMove(m, 60.0f, 0.0f, 120.0f, 40.0f));   // w prawo
	CHECK(m.slot == 2);
	CHECK(!WheelMouseMove(m, -65.0f, 0.0f, 120.0f, 40.0f)); // powrot do srodka - wybor zostaje
	CHECK(m.slot == 2);
	CHECK(WheelMouseMove(m, 0.0f, 80.0f, 120.0f, 40.0f));   // w dol
	CHECK(m.slot == 4);
}
TEST(wheel_mouse_vector_clamped_so_reverse_is_quick) {
	WheelMouse m;
	WheelMouseMove(m, 5000.0f, 0.0f, 120.0f, 40.0f);
	CHECK(fabsf(m.x - 120.0f) < 0.01f && fabsf(m.y) < 0.01f);
	CHECK(WheelMouseMove(m, -250.0f, 0.0f, 120.0f, 40.0f)); // bez przycinania trzeba by 5000
	CHECK(m.slot == 6);
}
TEST(wheel_mouse_diagonals_and_boundaries) {
	CHECK(WheelSlotFromVec(100.0f, -100.0f, 40.0f) == 1);   // prawo-gora
	CHECK(WheelSlotFromVec(-100.0f, -100.0f, 40.0f) == 7);  // lewo-gora
	CHECK(WheelSlotFromVec(-100.0f, 100.0f, 40.0f) == 5);   // lewo-dol
	CHECK(WheelSlotFromVec(-1.0f, -100.0f, 40.0f) == 0);    // tuz w lewo od gory - nadal gora
}

// ---------- Wheel: czas ----------
TEST(wheel_time_keeps_fraction_where_engine_drops_it) {
	// NB: klatka 6,7 ms ~ 7 ms, tempo 0,10 - silnik dalby 0 ms w kazdej klatce
	WheelTime t;
	unsigned sumInt = 0;
	double sumF = 0.0;
	int zeros = 0;
	for (int i = 0; i < 1000; i++) {
		WheelTimeOut o = WheelTimeStep(t, 7, 0.10f, 100.0f);
		sumInt += o.frameInt;
		sumF += o.frameMs;
		zeros += o.frameMs <= 0.0f ? 1 : 0;
		CHECK(o.frameInt <= 1);
	}
	CHECK(zeros == 0);
	CHECK(fabs(sumF - 700.0) < 0.5);
	CHECK(sumInt >= 699 && sumInt <= 700);
}
TEST(wheel_time_cap_and_full_speed) {
	WheelTime t;
	WheelTimeOut o = WheelTimeStep(t, 500, 1.0f, 100.0f);
	CHECK(o.frameInt == 100 && fabsf(o.frameMs - 100.0f) < 1e-3f);
	WheelTime u;
	o = WheelTimeStep(u, 16, 1.0f, 100.0f);
	CHECK(o.frameInt == 16 && u.carry < 1e-6);
	WheelTime v;
	o = WheelTimeStep(v, 16, 0.0f, 100.0f);
	CHECK(o.frameInt == 0 && o.frameMs == 0.0f);
}
TEST(wheel_time_fifteen_percent_no_stutter) {
	// tempo 0,15 przy 6 ms: silnik daje 0 / 1 na przemian (0,9 -> 0); u nas float 0,9 w kazdej klatce
	WheelTime t;
	unsigned sumInt = 0;
	for (int i = 0; i < 100; i++) {
		WheelTimeOut o = WheelTimeStep(t, 6, 0.15f, 100.0f);
		CHECK(fabsf(o.frameMs - 0.9f) < 1e-4f);
		sumInt += o.frameInt;
	}
	CHECK(sumInt >= 89 && sumInt <= 90);
}
TEST(wheel_tempo_ramp_and_target) {
	CHECK(fabsf(WheelTempoApproach(1.0f, 0.15f, 60.0, 120.0) - 0.5f) < 1e-4f);
	CHECK(WheelTempoApproach(1.0f, 0.15f, 500.0, 120.0) == 0.15f);
	CHECK(WheelTempoApproach(0.15f, 1.0f, 500.0, 120.0) == 1.0f);
	CHECK(WheelTempoApproach(0.5f, 0.15f, 10.0, 0.0) == 0.15f);
	CHECK(fabsf(WheelTempoTarget(true, 15, false, 100) - 0.15f) < 1e-6f);
	CHECK(WheelTempoTarget(false, 15, false, 30) == 1.0f);
	CHECK(fabsf(WheelTempoTarget(false, 15, true, 30) - 0.30f) < 1e-6f);
	CHECK(WheelTempoTarget(false, 15, true, 100) == 1.0f);   // XbowAimSlowmo=100 = wylaczone
	CHECK(fabsf(WheelTempoTarget(true, 15, true, 30) - 0.15f) < 1e-6f);
	CHECK(WheelTempoTarget(true, 0, false, 100) >= 0.01f);
}

// ---------- Wheel: odmowa i opisy ----------
TEST(wheel_deny_order) {
	WheelSense s;
	CHECK(WheelDenyCheck(s) == WDENY_OK);
	s.empty = true;
	CHECK(WheelDenyCheck(s) == WDENY_EMPTY);
	s.rewind = true;
	CHECK(WheelDenyCheck(s) == WDENY_REWIND);
	s.quickCast = true;
	CHECK(WheelDenyCheck(s) == WDENY_QUICKCAST);
	s.foreignWindow = true;
	CHECK(WheelDenyCheck(s) == WDENY_FOREIGN_WINDOW);
	s.window = "dialog";
	CHECK(WheelDenyCheck(s) == WDENY_WINDOW);
	s.down = true;
	CHECK(WheelDenyCheck(s) == WDENY_DOWN);
	s.enabled = false;
	CHECK(WheelDenyCheck(s) == WDENY_OFF);
	for (int d = 0; d < WDENY_COUNT; d++)
		CHECK(std::string(WheelDenyText(d)) != "?");
}
TEST(wheel_key_bind_rules) {
	const char* why = nullptr;
	CHECK(WheelKeyBindable(0x3A, &why) && why == nullptr);          // Caps Lock
	CHECK(WheelKeyBindable(0x10, &why));                            // Q
	CHECK(WheelKeyBindable(WHEEL_MOUSE_X1, &why) && WheelKeyBindable(WHEEL_MOUSE_X2, &why) && WheelKeyBindable(WHEEL_MOUSE_MID, &why));
	CHECK(!WheelKeyBindable(0x01, &why) && why != nullptr);         // Esc
	CHECK(!WheelKeyBindable(WHEEL_MOUSE_LEFT, &why) && !WheelKeyBindable(WHEEL_MOUSE_RIGHT, &why));
	CHECK(!WheelKeyBindable(0x20A, &why) && !WheelKeyBindable(0, &why) && !WheelKeyBindable(0x300, &why));
	CHECK(WheelMouseVk(WHEEL_MOUSE_X1) == 5 && WheelMouseVk(WHEEL_MOUSE_X2) == 6 && WheelMouseVk(WHEEL_MOUSE_MID) == 4);
	CHECK(WheelMouseVk(0x3A) == 0);
	CHECK(WheelMouseCodeFromVk(5) == WHEEL_MOUSE_X1 && WheelMouseCodeFromVk(6) == WHEEL_MOUSE_X2 && WheelMouseCodeFromVk(0x41) == 0);
	CHECK(WheelMouseName(WHEEL_MOUSE_X1) == "Mysz 4" && WheelMouseName(WHEEL_MOUSE_X2) == "Mysz 5");
	CHECK(WheelMouseName(0x3A).empty() && WheelIsMouseCode(WHEEL_MOUSE_MID) && !WheelIsMouseCode(0x3A));
}
TEST(wheel_texts) {
	CHECK(WheelKindText(WSK_RUNE, 1) == "Runa");
	CHECK(WheelKindText(WSK_SCROLL, 3) == "Zw\xF3j x3");
	CHECK(WheelKindText(WSK_SIGN, 1) == "Znak Wied\x9Fmina");
	CHECK(WheelCooldownText(0).empty());
	CHECK(WheelCooldownText(1350) == "Odnowienie: 1,4 s");
	CHECK(WheelCooldownText(40) == "Odnowienie: 0,1 s");
	CHECK(WheelItemLine("Koszt many:", 10) == "Koszt many:  10");
	CHECK(WheelItemLine("Opis", 0) == "Opis");
	CHECK(WheelItemLine("", 5).empty());
	CHECK(WheelPageText(0, 1).empty());
	CHECK(WheelPageText(1, 3) == "Strona 2/3");
	CHECK(WheelShortName("Ognista strzala", 20) == "Ognista strzala");
	CHECK(WheelShortName("Bardzo dluga nazwa czaru", 12) == "Bardzo dl...");
}
