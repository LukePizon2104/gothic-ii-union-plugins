// Testy 24.10 (odbijanie / teleporty tam i z powrotem przy przeskokach i skokach) - dolaczane z test_core.cpp.
// Dane z zDismember.log (NB, schodki swiatyni w Jarkendarze, seria A 49176-49388 z W i sprintem W3):
// Core\HeroGuard.h: HeroOscDecide (straznik oscylacji), TravMomentumFade / TravMomentumPhysicsVel (ped przez fizyke),
// TravLoopBoxTooLong / TravLoopBoxPush (ramka petli biegu przy wyjsciu w bieg).
#include "Core/HeroGuard.h"

namespace travbounce {
	struct P3 {
		float x, y, z;
	};
	// 49177.61-49177.69 (CLIMB_100_RF, wyjscie w bieg ze sprintu W3): silnik co klatke stawial bohatera tu ("zadane 0 cm,
	// kolizja zmienila o 325 cm"), straznik "skok pozycji po naszym ruchu" cofal go do (-132907, -3554, -16146)
	static const P3 kEngine[] = { { -132944, -3554, -15827 }, { -132955, -3554, -15831 }, { -132967, -3554, -15833 },
		{ -132980, -3554, -15841 }, { -132994, -3554, -15850 }, { -133008, -3555, -15859 }, { -133021, -3554, -15865 },
		{ -133030, -3555, -15872 }, { -133042, -3555, -15881 }, { -133050, -3555, -15888 }, { -133067, -3555, -15904 },
		{ -133075, -3555, -15916 } };
	static const P3 kOurs = { -132907, -3554, -16146 };
	static void F(const P3& p, float o[3]) {
		o[0] = p.x;
		o[1] = p.y;
		o[2] = p.z;
	}
	static float Len(const float a[3], const float b[3]) {
		return zdcore::HgDist(a, b);
	}
}

TEST(travbounce_oscillation_from_log_stops_after_two_repeats) {
	using namespace travbounce;
	zdcore::HeroOscState s;
	zdcore::HeroOscParams p;
	float to[3], from[3];
	F(kOurs, to);
	int applied = 0, osc = 0, hold = 0;
	unsigned frame = 1000;
	for (const P3& e : kEngine) {
		F(e, from);
		zdcore::HeroOscVerdict v = zdcore::HeroOscDecide(s, frame++, from, to, p);
		applied += v == zdcore::HOSC_APPLY;
		osc += v == zdcore::HOSC_OSCILLATION;
		hold += v == zdcore::HOSC_HOLD;
	}
	// dawniej: 12 korekt (i 12 wypchniec) w 0,08 s; teraz 2 korekty, trzecia = oscylacja, reszta wstrzymana
	CHECK(applied == 2 && osc == 1 && hold == 9 && s.trips == 1);
	// po holdFrames znowu wolno (inne miejsce)
	F(kEngine[0], from);
	CHECK(zdcore::HeroOscDecide(s, frame + p.holdFrames, from, to, p) == zdcore::HOSC_APPLY);
}

TEST(travbounce_one_correction_per_frame) {
	// 49177.61: w jednej klatce "Teleport kolizji ... cofniety" (hak EndMovement) i "Skok pozycji ... przywrocone" (petla)
	using namespace travbounce;
	zdcore::HeroOscState s;
	zdcore::HeroOscParams p;
	float to[3], a[3], b[3];
	F(kOurs, to);
	F(kEngine[0], a);
	F(kEngine[1], b);
	CHECK(zdcore::HeroOscDecide(s, 7, a, to, p) == zdcore::HOSC_APPLY);
	CHECK(zdcore::HeroOscDecide(s, 7, b, to, p) == zdcore::HOSC_SAME_FRAME);
	CHECK(zdcore::HeroOscDecide(s, 8, b, to, p) == zdcore::HOSC_APPLY);
}

TEST(travbounce_back_and_forth_is_oscillation_too) {
	// 49252.01-49252.08: ped dosuwal z kolizja, kolizja przenosila 351-364 cm do tylu, my z powrotem - A -> B, B -> A
	zdcore::HeroOscState s;
	zdcore::HeroOscParams p;
	float A[3] = { -132381.0f, -3548.0f, -16163.0f }, B[3] = { -132381.0f, -3548.0f, -15800.0f };
	CHECK(zdcore::HeroOscDecide(s, 1, B, A, p) == zdcore::HOSC_APPLY);
	CHECK(zdcore::HeroOscDecide(s, 2, A, B, p) == zdcore::HOSC_APPLY);
	CHECK(zdcore::HeroOscDecide(s, 3, B, A, p) == zdcore::HOSC_OSCILLATION);
}

TEST(travbounce_small_or_moving_corrections_are_not_oscillation) {
	// "Skok gry: krok 12 cm w klatce ponad limit pedu - przyciety" co klatke (49632.34-49632.51): male, w nowych miejscach
	zdcore::HeroOscState s;
	zdcore::HeroOscParams p;
	for (unsigned f = 0; f < 20; f++) {
		float from[3] = { -129130.0f + 12.0f * f, -3508.0f, -10749.0f }, to[3] = { -129130.0f + 12.0f * f - 3.0f, -3508.0f, -10749.0f };
		CHECK(zdcore::HeroOscDecide(s, 100 + f, from, to, p) == zdcore::HOSC_APPLY);
	}
	// duze, ale w roznych miejscach (np. dwa rozne teleporty kolizji w odstepie sekund) - tez nie
	zdcore::HeroOscState t;
	float a1[3] = { 0, 0, 0 }, b1[3] = { 300, 0, 0 }, a2[3] = { 5000, 0, 0 }, b2[3] = { 5300, 0, 0 };
	CHECK(zdcore::HeroOscDecide(t, 1, a1, b1, p) == zdcore::HOSC_APPLY);
	CHECK(zdcore::HeroOscDecide(t, 2, a2, b2, p) == zdcore::HOSC_APPLY);
	CHECK(zdcore::HeroOscDecide(t, 3, a1, b1, p) == zdcore::HOSC_APPLY);
	CHECK(std::string(zdcore::HeroOscVerdictName(zdcore::HOSC_OSCILLATION)).find("oscylacja") != std::string::npos);
}

TEST(travbounce_momentum_only_through_physics_and_fades_at_obstacle) {
	// log: "Skok gry: ped 850 cm/s (animacja 641, T_RUNR_2_JUMP)", odbicie 1506 cm/s
	zdcore::TravMomentumParams mp;
	zdcore::TravMomentumFadeParams fp;
	float target = zdcore::TravMomentumTarget(1506.0f, 641.0f, -1.0f, mp);
	CHECK(std::fabs(target - 850.0f) < 0.5f);
	// wolna droga >= 0,25 s lotu z celem (212 cm) - pelny ped; 25 cm i mniej - zaden
	CHECK(zdcore::TravMomentumFade(400.0f, target, fp) == 1.0f);
	CHECK(zdcore::TravMomentumFade(212.5f, target, fp) > 0.99f);
	CHECK(zdcore::TravMomentumFade(25.0f, target, fp) == 0.0f && zdcore::TravMomentumFade(0.0f, target, fp) == 0.0f);
	float half = zdcore::TravMomentumFade(118.75f, target, fp);
	CHECK(half > 0.45f && half < 0.55f);
	// wolna droga: predkosc gry 641 podniesiona do 850
	CHECK(std::fabs(zdcore::TravMomentumPhysicsVel(target, 641.0f, 641.0f, 1.0f) - 850.0f) < 0.5f);
	// stopien tuz przed bohaterem: nasz dodatek zdjety (850 -> 641), predkosci gry nie ruszamy
	CHECK(std::fabs(zdcore::TravMomentumPhysicsVel(target, 641.0f, 850.0f, 0.0f) - 641.0f) < 0.5f);
	CHECK(std::fabs(zdcore::TravMomentumPhysicsVel(target, 641.0f, 600.0f, 0.0f) - 600.0f) < 0.5f);   // zderzenie - nie pchamy
	CHECK(std::fabs(zdcore::TravMomentumPhysicsVel(target, 641.0f, 900.0f, 1.0f) - 900.0f) < 0.5f);   // szybciej niz cel - bez zmian
	// polowa drogi - polowa dodatku
	float v = zdcore::TravMomentumPhysicsVel(target, 641.0f, 641.0f, 0.5f);
	CHECK(std::fabs(v - (641.0f + 0.5f * 209.0f)) < 0.5f);
	// bez pedu (odbicie ponizej animacji) - nic
	CHECK(zdcore::TravMomentumPhysicsVel(0.0f, 641.0f, 500.0f, 1.0f) == 500.0f);
}

TEST(travbounce_loop_bbox_gate_matches_log_pushes) {
	zdcore::TravLoopBoxParams p;
	// stare W3: sprint S_RUNL +466, trucht +310; gra: HUMANS-S_RUNL +47, HUMANS_SPRINT +78; nowe W3 +81..91
	CHECK(zdcore::TravLoopBoxTooLong(466.0f, p) && zdcore::TravLoopBoxTooLong(310.0f, p));
	CHECK(!zdcore::TravLoopBoxTooLong(47.0f, p) && !zdcore::TravLoopBoxTooLong(78.0f, p) && !zdcore::TravLoopBoxTooLong(91.0f, p));
	// log 49177.61: "kolizja zmienila o 326 cm" - przod ramki 466, wolna droga 140; nowa ramka 89 - bez wypchniecia
	CHECK(std::fabs(zdcore::TravLoopBoxPush(466.0f, 140.0f) - 326.0f) < 0.5f);
	CHECK(zdcore::TravLoopBoxPush(89.0f, 140.0f) == 0.0f);
}

// Symulacja wyjscia w bieg z logu (bez gry): kolizja liczona od nowa wypycha do tylu o (przod ramki - wolna droga).
// Liczy kazde przestawienie bohatera > 30 cm poza zamierzonym ruchem (krok biegu) - przez kogokolwiek (silnik, nasz
// straznik): stara logika = co klatke wypchniecie przez nasze ustawienie wysokosci Z KOLIZJA (stan kolizji od nowa)
// i cofniecie straznika (log 49177.61-49177.73: 19 korekt + 14 wypchniec w 0,12 s). Nowa: wysokosc bez kolizji,
// straznik oscylacji, ramka petli bez drogi cyklu albo wyjscie w bieg zablokowane.
namespace travbounce {
	struct SimOut {
		int bigJumps = 0;
		float maxJump = 0.0f;
	};
	static SimOut SimRunExit(float front, bool oldLogic) {
		const float clear = 140.0f, v = 1300.0f, dt = 0.007f;   // sprint W3 ok. 1300 cm/s, klatki 5-8 ms (log)
		zdcore::HeroOscState osc;
		zdcore::HeroOscParams op;
		zdcore::TravLoopBoxParams bp;
		SimOut o;
		if (!oldLogic && zdcore::TravLoopBoxTooLong(front, bp))
			return o;                // wyjscie w bieg zablokowane - koniec w postawie (tor do konca klipu, bez wypchniec)
		float pos = 0.0f;            // wzdluz biegu (cm), 0 = koniec toru na gorze
		auto set = [&](float to) {   // przestawienie bohatera (nie krok biegu)
			float j = std::fabs(to - pos);
			if (j > 30.0f) {
				o.bigJumps++;
				o.maxJump = j > o.maxJump ? j : o.maxJump;
			}
			pos = to;
		};
		bool reinit = true;          // pierwsza klatka z kolizja po wlaczeniu
		unsigned frame = 0;
		for (int f = 0; f < 40; f++, frame++) {
			float start = pos;
			pos += v * dt;                                       // ruch korzenia petli biegu
			if (reinit)
				set(pos - zdcore::TravLoopBoxPush(front, clear));  // kolizja od nowa
			reinit = false;
			if (start - pos > 100.0f) {                          // hak EndMovement: teleport kolizji cofniety
				float a[3] = { pos, 0, 0 }, b[3] = { start, 0, 0 };
				if (oldLogic || zdcore::HeroOscDecide(osc, frame, a, b, op) == zdcore::HOSC_APPLY)
					set(start);
			}
			if (oldLogic) {
				// petla pluginu po renderze: ustawienie wysokosci z kolizja -> wypchniecie; w nastepnej petli straznik
				// "skok pozycji po naszym ruchu" cofa (przed kolejnym ustawieniem wysokosci)
				float top = pos;
				set(pos - zdcore::TravLoopBoxPush(front, clear));
				set(top);
				set(pos - zdcore::TravLoopBoxPush(front, clear));
			}
		}
		return o;
	}
}

TEST(travbounce_sim_run_exit_no_jumps_over_30cm) {
	using namespace travbounce;
	// stara paczka + stara logika: wypchniecia co klatke (log: 19 korekt w 0,12 s)
	SimOut old = SimRunExit(466.0f, true);
	CHECK(old.bigJumps >= 10);
	// nowa paczka (ramka 89 cm) + nowa logika: zadnego przesuniecia > 30 cm na klatke poza biegiem
	SimOut fixed = SimRunExit(89.0f, false);
	CHECK(fixed.bigJumps == 0);
	// stara paczka + nowa logika: wyjscie w bieg zablokowane (koniec w postawie) - tez bez skokow
	SimOut gate = SimRunExit(466.0f, false);
	CHECK(gate.bigJumps == 0);
	// natywny bieg gry (+47) - jak dotad bez wypchniec
	CHECK(SimRunExit(47.0f, false).bigJumps == 0);
}

TEST(travbounce_footing_rejects_crack_between_crates) {
	// stopien 100 na torze (zrzut 24.10): srodek postaci nad szpara stosu 2x2, obwod na skrzyniach
	zdcore::TravFootingParams p;
	float crack[5] = { 60.0f, 1.0f, 2.0f, 0.0f, 1.0f };
	float flat[5] = { 0.5f, 0.0f, 1.0f, -1.0f, 0.0f };
	float edge[5] = { 2.0f, 1.0f, 101.0f, 0.0f, 0.0f };      // tyl obwodu za krawedzia stopnia - nadal pewnie
	float barrel[5] = { 0.0f, 18.0f, 20.0f, 4.0f, 3.0f };    // okragla gora beczki: 2 z 4 punktow podparte
	float wall[5] = { -40.0f, 0.0f, 0.0f, 0.0f, 0.0f };      // srodek w przeszkodzie
	float air[5] = { 1e9f, 1e9f, 1e9f, 1e9f, 1e9f };
	int ring = -1;
	CHECK(!zdcore::TravFootingOk(crack, 5, p, &ring) && ring == 4);
	CHECK(zdcore::TravFootingOk(flat, 5, p) && zdcore::TravFootingOk(edge, 5, p) && zdcore::TravFootingOk(barrel, 5, p));
	CHECK(!zdcore::TravFootingOk(wall, 5, p) && !zdcore::TravFootingOk(air, 5, p) && !zdcore::TravFootingOk(nullptr, 0, p));
}

TEST(travbounce_stuck_only_after_our_move_with_w_and_bad_footing) {
	zdcore::TravStuckParams p;
	CHECK(zdcore::TravStuck(1.0f, 0.6f, true, false, p));
	CHECK(!zdcore::TravStuck(1.0f, 0.3f, true, false, p));    // za krotko
	CHECK(!zdcore::TravStuck(1.0f, 0.6f, false, false, p));   // bez W - stoi, bo chce
	CHECK(!zdcore::TravStuck(1.0f, 0.6f, true, true, p));     // pewny grunt (np. opiera sie o sciane)
	CHECK(!zdcore::TravStuck(5.0f, 0.6f, true, false, p) && !zdcore::TravStuck(-1.0f, 0.6f, true, false, p));
}

TEST(travbounce_climb_limit_at_least_native_jump_up) {
	// log 51050: stopien 350 (gora 361 cm) - "za wysoko" z biegu (TravLimits 355), gra z miejsca do 389-401 cm
	zdcore::TravClimbMaxParams p;
	zdcore::TravLimits lim;
	float h = zdcore::TravClimbMaxHeight(lim.maxHeight, 396.0f, p);
	CHECK(h >= 396.0f * 1.10f && h >= 361.0f && h <= p.cap);
	CHECK(zdcore::TravClimbMaxHeight(lim.maxHeight, 0.0f, p) == lim.maxHeight);    // nieznane - jak dotad
	CHECK(zdcore::TravClimbMaxHeight(lim.maxHeight, 250.0f, p) == lim.maxHeight);  // nizsza gra - nie mniej niz dotad
	CHECK(zdcore::TravClimbMaxHeight(lim.maxHeight, 900.0f, p) == p.cap);
	// kubelek klipu dla 361 przy podniesionym limicie: 250 (rozciagniety w pionie), nie 0
	zdcore::TravLimits l2 = lim;
	l2.maxHeight = h;
	CHECK(zdcore::TravBucket(361.0f, l2) == 250 && zdcore::TravBucket(361.0f, lim) == 0);
}
