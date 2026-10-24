// Logika bez silnika: katalog finisherow (finishers.ini) i wybor definicji.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "PartMask.h"

namespace zdcore {
	enum { WEAPON_NONE = 0, WEAPON_1H = 1, WEAPON_2H = 2 };
	enum : uint32_t { GROUP_HUMAN = 1, GROUP_ORC = 2, GROUP_ZOMBIE = 4, GROUP_MONSTER = 8 };
	enum : uint32_t { POSE_STANDING = 1, POSE_DAZED = 2, POSE_LYING = 4 };

	struct FinisherDef {
		std::string id;
		int weapon = WEAPON_NONE;
		uint32_t victimGroups = 0;
		uint32_t poses = 0;
		std::string overlay, heroAni, victimOverlay, victimAni, inplace;
		Part cut = Part::NoPart;                   // cut=NONE: zabija bez odciecia (pchniecie), krew z rany w wezle 'wound'
		std::string wound = "BIP01 SPINE2";
		float cutTime = 0.5f;
		float offset[3] = { 0.0f, 0.0f, 100.0f };
		float inplaceOffset[3] = { 0.0f, 0.0f, 100.0f };
		float slowmo = 1.0f, slowFrom = 0.0f, slowTo = 0.0f;
		int weight = 10;
		// Finisher parowany (W3): tory miednicy bohatera i ofiary w ukladzie bohatera na starcie (cm, x w prawo,
		// z do przodu), probki co 1/trajFps s. Vob ofiary stoi twarza do bohatera, jej animacja jest w tym ukladzie.
		float trajFps = 25.0f;
		std::vector<float> heroTraj, victimTraj;   // x0,z0,x1,z1,...
		bool paired() const { return !victimTraj.empty(); }
	};

	// "x,z x,z ..." (pary rozdzielone spacja - srednik w ini to komentarz) -> plaska lista; false przy zlej parze.
	bool ParseTraj(const std::string& value, std::vector<float>& out);

	// Punkt toru w czasie t (liniowo miedzy probkami, poza zakresem - skrajna probka; pusty tor = 0,0).
	void SampleTraj(const std::vector<float>& xz, float fps, float t, float& x, float& z);

	// Sekcje [ID], klucze klucz=wartosc, komentarze od ';'. Wpisy z bledami sa pomijane, opis bledu w errors.
	std::vector<FinisherDef> ParseCatalog(const std::string& text, std::vector<std::string>& errors);

	// Spowolnienie czasu w finisherze (FinisherSlowMo): 0 wyl., 1 lekkie (polowa spowolnienia z katalogu: 0,35 -> 0,675),
	// 2 jak w katalogu; inna wartosc = jak w katalogu. lastFoeOnly (FinisherSlowMoLastEnemy) i othersFighting (inny
	// walczacy wrog w poblizu) = bez spowolnienia. Wynik: mnoznik tempa gry (1 = bez spowolnienia).
	enum { SLOWMO_OFF = 0, SLOWMO_LIGHT = 1, SLOWMO_FULL = 2 };
	float FinisherSlowFactor(float slowmo, int mode, bool lastFoeOnly, bool othersFighting);

	// Pchniecie (cut=NONE) - jedyne finishery pasujace do rapiera.
	inline bool IsThrustFinisher(const FinisherDef& d) { return d.cut == Part::NoPart; }

	// Losuje z wagami sposrod pasujacych (bron, grupa ofiary, poza), z pominieciem 'skip'; nullptr gdy brak.
	const FinisherDef* PickFinisher(const std::vector<FinisherDef>& defs, int weapon, uint32_t victimGroup, uint32_t pose, uint32_t rnd,
		const std::vector<const FinisherDef*>* skip = nullptr);
}
