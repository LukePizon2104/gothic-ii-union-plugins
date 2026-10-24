// Testy klawisza drugiego miecza (Core\BackSwords: SecondKeyStep) - dolaczane z test_core.cpp.
#include "Core/BackSwords.h"

static SecondKeyInput SkIn(bool engine, bool physical, bool focus = true, bool typing = false) {
	SecondKeyInput in;
	in.engine = engine;
	in.physical = physical;
	in.focus = focus;
	in.typing = typing;
	return in;
}

TEST(second_key_engine_press_once_per_hold) {
	SecondKeyState s;
	CHECK(SecondKeyStep(s, SkIn(true, true)).pressed == SKS_ENGINE);
	CHECK(SecondKeyStep(s, SkIn(true, true)).pressed == SKS_NONE);   // trzymany - bez powtorzenia
	SecondKeyEvent r = SecondKeyStep(s, SkIn(false, false));
	CHECK(r.pressed == SKS_NONE && !r.releasedUnseen);
	CHECK(SecondKeyStep(s, SkIn(true, false)).pressed == SKS_ENGINE);   // nowe wcisniecie
}

TEST(second_key_physical_fallback_when_engine_cleared) {
	// ekwipunek HSI czysci klawiature gry: silnik caly czas 0, GetAsyncKeyState 1
	SecondKeyState s;
	CHECK(SecondKeyStep(s, SkIn(false, true)).pressed == SKS_PHYSICAL);
	CHECK(SecondKeyStep(s, SkIn(false, true)).pressed == SKS_NONE);
	CHECK(SecondKeyStep(s, SkIn(false, true)).pressed == SKS_NONE);
	SecondKeyEvent r = SecondKeyStep(s, SkIn(false, false));
	CHECK(r.releasedUnseen && r.heldFrames == 3);
}

TEST(second_key_engine_lag_is_not_unseen) {
	// stan fizyczny klatke przed buforem DirectInput - jedno wcisniecie, bez wpisu "silnik nie widzial"
	SecondKeyState s;
	CHECK(SecondKeyStep(s, SkIn(false, true)).pressed == SKS_PHYSICAL);
	CHECK(SecondKeyStep(s, SkIn(true, true)).pressed == SKS_NONE);
	SecondKeyEvent r = SecondKeyStep(s, SkIn(false, false));
	CHECK(r.pressed == SKS_NONE && !r.releasedUnseen);
}

TEST(second_key_physical_needs_focus_and_no_typing) {
	SecondKeyState s;
	CHECK(SecondKeyStep(s, SkIn(false, true, false)).pressed == SKS_NONE);         // inne okno na pierwszym planie
	CHECK(SecondKeyStep(s, SkIn(false, true, true, true)).pressed == SKS_NONE);    // wyszukiwarka HSI / konsola
	CHECK(!s.held);
	CHECK(SecondKeyStep(s, SkIn(true, true, true, true)).pressed == SKS_ENGINE);   // silnik widzi - jak dotad
	CHECK(SecondKeyStep(s, SkIn(false, false)).releasedUnseen == false);
}

TEST(second_key_source_labels) {
	CHECK(std::string(SecondKeySourceLabel(SKS_ENGINE)).find("zinput") != std::string::npos);
	CHECK(std::string(SecondKeySourceLabel(SKS_PHYSICAL)).find("GetAsyncKeyState") != std::string::npos);
	CHECK(std::string(SecondKeySourceLabel(SKS_NONE)) == "-");
}
