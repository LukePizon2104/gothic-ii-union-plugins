// Logika bez silnika - zbiorczy naglowek dla pluginu. Dolaczany z Headers.h w nawiasach ostrych, bo
// beforebuild.exe z Union SDK traktuje naglowki w cudzyslowie jako pliki interfejsu (zakaz #pragma once,
// usuwa im lokalne #include), a te pliki sa wspolne z testami (tests\test_core.cpp).
#pragma once
#include "Text.h"
#include "Timeline.h"
#include "Cursor.h"
#include "Keyframes.h"
#include "Globals.h"
#include "Combat.h"
#include "Consume.h"
#include "Cuts.h"
