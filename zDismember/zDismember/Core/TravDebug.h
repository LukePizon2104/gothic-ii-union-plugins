// Logika bez silnika: debug wizualny przeskokow i wspinania (zdism trav show) - zapis promieni wykrywania i lista
// elementow do narysowania w ukladzie bohatera (ziemia pod nim y = 0, przod +z, prawo +x). Plugin przelicza je na swiat
// i rysuje (linie 3D, znaczniki i napisy w rzucie na ekran).
#pragma once
#include <string>
#include <vector>
#include "Traversal.h"

namespace zdcore {
	// Promien z wykrywania z etapem (TravStage).
	struct TravRayRec {
		int stage = 0;
		float o[3] = { 0, 0, 0 }, d[3] = { 0, 0, 0 };
		float len = 0.0f;
		bool hit = false;
		float dist = 0.0f;
	};
	// Swiat posredni: przekazuje promienie dalej i je zapisuje (z etapem z TravDetect).
	struct TravRecorder : TravWorld {
		TravWorld& inner;
		int stage = 0;
		std::vector<TravRayRec> rays;
		explicit TravRecorder(TravWorld& w) : inner(w) {}
		void Stage(int s) override { stage = s; }
		TravRay Cast(float x, float y, float z, float dx, float dy, float dz, float len) override;
	};

	// Kolory ARGB.
	enum : unsigned {
		TRAVCOL_HERO = 0xFF00E0FF,       // bohater: krzyzyk i strzalka
		TRAVCOL_GROUND = 0xFFFFFFFF,     // probki terenu
		TRAVCOL_GROUNDLINE = 0xFF909090, // teren miedzy probkami
		TRAVCOL_HIT = 0xFFFF3030,        // promien trafiony (do punktu trafienia)
		TRAVCOL_FREE = 0xFF30FF30,       // promien wolny
		TRAVCOL_WALL = 0xFFFF40FF,       // sciana (pion) i podstawa
		TRAVCOL_TOP = 0xFFFFFF00,        // gorna krawedz (tu chwyta), glebokosc gory
		TRAVCOL_THICK = 0xFFFF9000,      // grubosc gory (przeskok), koniec ruchu
		TRAVCOL_SIDE = 0xFF00FFFF,       // boki gory
		TRAVCOL_LAND = 0xFF80C0FF,       // ziemia za przeszkoda (przeskok)
		TRAVCOL_WINDOW = 0xFF4090FF,     // okno startu na ziemi
		TRAVCOL_TEXT = 0xFFFFFFFF,       // napisy
		TRAVCOL_BAD = 0xFFFF6060,        // napis odmowy
		TRAVCOL_RAY = 0xFFB0B0B0,        // promien silnika skoku w biegu (szary; trafia = czerwony do sciany)
		TRAVCOL_JUMP_OK = 0xFF90FF90,    // luk skoku gry: skok przepuszczony
		TRAVCOL_JUMP_NO = 0xFFFF9090,    // luk skoku gry: wstrzymany (odbicie / anulowany przez silnik)
		TRAVCOL_JUMP_LEDGE = 0xFFFFC040  // skok z miejsca gry (zawis / wspiecie)
	};
	enum TravDrawKind { TRAVDRAW_LINE, TRAVDRAW_MARK, TRAVDRAW_TEXT };
	struct TravDrawItem {
		TravDrawKind kind = TRAVDRAW_LINE;
		float a[3] = { 0, 0, 0 }, b[3] = { 0, 0, 0 };   // linia a-b; znacznik / napis w a
		unsigned color = TRAVCOL_TEXT;
		std::string text;                // znacznik: jeden znak (x . o + itp.), napis: tekst
	};

	struct TravDebugInfo {
		TravResult res;
		const TravDef* def = nullptr;    // wybrany klip (albo brak)
		std::vector<TravRayRec> rays;
		TravLimits lim;
		TravTiming tm;
		std::string decision;            // napis decyzji (TravDecisionLabel)
		std::string gameJump;            // co zrobi skok gry po odmowie (TravGameJumpLabel; puste = nic)
		bool showJump = false;           // rysowac promien silnika i luk skoku gry (jp, jump)
		TravGameJumpParams jp;
		TravGameJumpInfo jump;
	};

	// "WSPIECIE 150 -> CLIMB_150_RF", "PRZESKOK 100 -> VAULT_100_LF", "WSPIECIE 150 - brak klipu", "BRAK: <powod>".
	std::string TravDecisionLabel(const TravResult& r, const TravDef* d);
	// Lista elementow do narysowania (uklad bohatera).
	std::vector<TravDrawItem> TravDebugBuild(const TravDebugInfo& in);
	// Linia stanu w rogu ekranu.
	std::string TravStatusLine(float speed, bool sprint, float range, const char* buffer);
}
