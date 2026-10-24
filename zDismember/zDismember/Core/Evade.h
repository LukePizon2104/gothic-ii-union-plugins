// Logika bez silnika: unik / przewrot (DODGE_* / ROLL_*) - okno wyjscia z buforem wcisniec, unik w ostatniej chwili
// (W3 "perfect dodge"), obrot do wroga na koncu ruchu, przyspieszenie pierwszego ciosu po idealnym uniku (17.10).
#pragma once
#include "Moves.h"

namespace zdcore {
	// Okno wyjscia (s od startu ruchu): od niego atak / blok / kolejny unik z bufora przerywa ruch. exit= z ini
	// (make_moves: po glownej drodze, stopa na ziemi), bez niego po 80% drogi, nie przed koncem nietykalnosci;
	// najpozniej poczatek zwyklego konczenia (cancel - endBlend).
	float MoveExitFrom(const MoveDef& d, float endBlend);

	// Bufor wcisniec w trakcie ruchu: ostatnie wcisniecie czeka na okno wyjscia (najwyzej maxAge s).
	enum ExitAction { EXIT_NONE = 0, EXIT_ATTACK, EXIT_PARADE, EXIT_DODGE, EXIT_ROLL };
	struct ExitQueue {
		int action = EXIT_NONE;
		int dir = -1;                    // kierunek kolejnego ruchu (MoveDir) z chwili wcisniecia; -1 = z klawiszy przy starcie
		float age = 0.0f;
	};
	void ExitQueuePush(ExitQueue& q, int action, int dir = -1);
	// Unik / przewrot wcisniety w trakcie ruchu idzie do bufora dopiero od minT s od startu - wczesniejsze wcisniecie to
	// zwykle powtorzenie tego samego zamiaru (Alt+A dwa razy szybko = jeden przewrot). minT <= 0 - zawsze.
	inline bool ChainPressAccepted(float t, float minT) {
		return minT <= 0.0f || t >= minT;
	}
	// Klatka: okno otwarte i cos w buforze -> akcja (bufor pusty); inaczej starzenie i EXIT_NONE.
	int ExitQueueStep(ExitQueue& q, bool windowOpen, float dt, float maxAge);

	// Wcisniecie ataku gry: sterowanie Gothica 1 (useGothic1Controls) = akcja trzymana i zbocze przodu albo zbocze akcji
	// przy trzymanym przodzie; Gothic 2 = zbocze akcji.
	bool AttackPressed(bool gothic1, bool action, bool actionPrev, bool up, bool upPrev);

	// Kierunek ciosu z okna wyjscia = podtyp oCMsgAttack (EV_ATTACKFORWARD / LEFT / RIGHT); w buforze w ExitQueue::dir.
	enum ExitAttackDir { EXIT_ATK_NONE = -1, EXIT_ATK_FORWARD = 0, EXIT_ATK_LEFT = 1, EXIT_ATK_RIGHT = 2 };
	struct AttackKeys {
		bool action = false, up = false;
		bool left = false, right = false;              // GAME_LEFT / GAME_STRAFELEFT (A, Q), GAME_RIGHT / GAME_STRAFERIGHT (D, E)
		bool actionLeft = false, actionRight = false;  // GAME_ACTIONLEFT / GAME_ACTIONRIGHT (sterowanie Gothica 2)
	};
	// Wcisniecie ciosu z kierunkiem jak oCAIHuman::FightMelee (G2A 0x696EC0). Gothic 1: akcja trzymana i zbocze przodu
	// (przod), lewo / prawo (bok) - kolejnosc gry: przod, lewo, prawo; albo zbocze akcji przy trzymanym kierunku (A
	// trzymane po przewrocie w lewo + akcja = cios w lewo). Gothic 2: zbocze GAME_ACTIONLEFT / RIGHT (bok), zbocze akcji
	// (przod). EXIT_ATK_NONE = bez ciosu.
	int AttackPressDir(bool gothic1, const AttackKeys& now, const AttackKeys& prev);
	// Animacja ciosu dla kierunku (_t_hitf / _t_hitl / _t_hitr): bez animacji boku (-1) albo kierunek spoza zakresu - przod
	// (dir poprawiany). -1 = brak ciosu w przod.
	int ExitAttackAni(int& dir, int hitF, int hitL, int hitR);

	// Unik w ostatniej chwili: czas do trafienia wroga (s) z klatki jego animacji ciosu (klatka ciosu hitOptFrame, tempo
	// animacji w kl./s); < 0 = po ciosie albo brak tempa.
	inline float EnemyTimeToHit(float hitFrame, float actFrame, float fps) {
		if (fps <= 0.0f || actFrame > hitFrame)
			return -1.0f;
		return (hitFrame - actFrame) / fps;
	}
	// Unik zaczety najwyzej 'window' s przed trafieniem (i jeszcze przed nim).
	inline bool PerfectDodgeTiming(float timeToHit, float window) {
		return timeToHit >= 0.0f && timeToHit <= window;
	}
	// Kontra W3 (blok w zamachu wroga): odskok w tyl gry z dobyta bronia (T_1HPARADEJUMPB / T_2HPARADEJUMPB /
	// T_FISTPARADEJUMPB - "PARADEJUMP" w nazwie, tez z nakladek stylow) to nie blok - kontra z niego tylko z
	// CounterOnBackstep. Ruch W3 z naszej nakladki (HUMANS_ZDF_MV_*) i wygasajaca animacja (ruch konczony blokiem - jego
	// T_xPARADEJUMPB schodzi pod parada) to nie odskok gracza; overlayName pusty = animacji nie ma w zadnej nakladce
	// modelu (nakladka ruchu juz zdjeta) - tez nie.
	bool IsBackstepParade(const std::string& aniName, const std::string& overlayName, bool fadingOut);
	// Okno kontry: window <= 0 = caly zamach wroga (wystarczy IsInPreHit); inaczej cios najwyzej 'window' s po bloku
	// (i jeszcze przed nim) - blok wczesniej to zwykla parada.
	inline bool CounterTiming(float timeToHit, float window) {
		return window <= 0.0f || (timeToHit >= 0.0f && timeToHit <= window);
	}
	// Wrog "traci namiar": obrot od poprzedniej klatki najwyzej maxRate rad/s (krotsza droga).
	float LostTrackYaw(float prevYaw, float yaw, float maxRate, float dt);

	// Obrot do wroga na koncu ruchu: przed 'from' bez zmian, od 'from' tak, by w 'arrive' patrzyl na cel (rowne tempo),
	// po 'arrive' nadaza za celem; zawsze najwyzej maxRate rad/s.
	float EndFaceYawStep(float yaw, float target, float t, float from, float arrive, float dt, float maxRate);

	// Pierwszy cios w oknie po idealnym uniku szybszy: tempo modelu bohatera x boost na czas ciosu. Tylko gdy tempo
	// wynosi 1 (Yrden / podtrzymanie znaku zostaja), przywracane tylko, gdy nikt go w miedzyczasie nie zmienil.
	struct HeroBoost {
		bool armed = false;              // okno na pierwszy cios
		bool active = false;             // cios trwa z przyspieszeniem
		float left = 0.0f;               // s okna
		float base = 1.0f, applied = 1.0f;
	};
	void HeroBoostArm(HeroBoost& b, float window);
	// Klatka: attacking = cios bohatera trwa; cur = biezace tempo modelu -> tempo do ustawienia.
	float HeroBoostStep(HeroBoost& b, bool attacking, float dt, float cur, float boost);
}
