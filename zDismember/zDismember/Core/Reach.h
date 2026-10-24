// Logika bez silnika: doskok do celu i zasieg ciosow stylu W3 (Reach.cpp w pluginie).
// Silnik (G2A, oCAniCtrl_Human::HitCombo 0x6B0260) w klatce DEF_OPT_FRAME sprawdza cel ciosu (hitTarget = fokus
// z chwili startu ataku): oCNpc::IsInFightRange 0x67CB60 - odleglosc w poziomie od srodka bohatera do bboxa celu <=
// FIGHT_RANGE_BASE + zasieg broni (item.range; bez broni FIGHT_RANGE_FIST), oCNpc::IsInFightFocus 0x735290 - azymut
// do celu < 30 st., elewacja < 50 st., oCNpc::IsSameHeight. Ciecia W3 trafiaja w 10. / 12. klatce (0,40 / 0,48 s),
// natywne w 3.-6. (0,08-0,20 s) - cel zdazy odejsc. Doskok: w zamachu bohater dosuwa sie do celu tak, by w chwili ciosu
// stal w idealnej odleglosci (zasieg - margines), a premia zasiegu doklada kilka cm do testu silnika.
#pragma once
#include <vector>

namespace zdcore {
	struct ReachParams {
		float margin = 25.0f;      // idealna odleglosc w chwili ciosu = zasieg silnika - margin (cm, do bboxa celu)
		float maxPath = 130.0f;    // najwieksza droga doskoku na jedno ciecie (cm, w obie strony)
		float maxSpeed = 450.0f;   // cm/s
		float accel = 3000.0f;     // cm/s^2 - rozpedzanie (bez skoku z miejsca do pelnej predkosci)
		float startSlack = 60.0f;  // cel za daleko: brakuje wiecej niz maxPath + tyle - bez doskoku
		float coneDeg = 45.0f;     // cel najwyzej tyle st. od przodu bohatera
		float minSurface = 15.0f;  // cel blizej niz tyle (do bboxa, po ruchu animacji) - lekko w tyl
		float backSpeed = 120.0f;  // cm/s odsuwania
		float turnRate = 9.0f;     // rad/s obrotu do celu w zamachu
	};

	struct ReachState {
		float path = 0.0f;         // wykonana droga doskoku w tym cieciu (cm, w obie strony)
		float speed = 0.0f;        // predkosc z ostatniej klatki (cm/s, ujemna = w tyl)
	};

	// Brakujaca droga do idealnej odleglosci w chwili ciosu: odleglosc silnika (od bohatera do bboxa celu) minus
	// idealna (zasieg - margin) minus ruch korzenia animacji do przodu, ktory jeszcze nastapi przed ciosem.
	// Ujemna = bohater bedzie blizej niz idealnie.
	float ReachGap(float surfDist, float range, float margin, float rootAhead);

	// Czas do klatki ciosu (s): (optFrame - actFrame) / fps; po niej albo bez tempa 0.
	float ReachTimeToHit(float optFrame, float actFrame, float fps);

	// Krok doskoku w tej klatce (cm wzdluz kierunku do celu; ujemny = odsuniecie). surfAtHit = odleglosc do bboxa
	// w chwili ciosu bez doskoku (po ruchu animacji).
	// - gap > 0: brakujaca droga rozlozona rowno na czas do ciosu, predkosc <= maxSpeed, przyspieszenie <= accel, nie
	//   dalej niz gap, laczna droga w cieciu <= maxPath;
	// - cel nachodzi (surfAtHit < minSurface): odsuniecie z backSpeed, najwyzej do minSurface (tez z budzetu drogi);
	// - inaczej 0 (cel blizej niz idealnie - bez ruchu). Po chwili ciosu (timeToHit <= 0) zawsze 0.
	float ReachStep(const ReachParams& p, ReachState& s, float gap, float surfAtHit, float timeToHit, float dt);

	// Cel nadaje sie do doskoku: w stozku coneDeg przed bohaterem (heroYaw = kierunek voba, atan2(x, z)) i nie za
	// daleko (gap <= maxPath + startSlack). Blizszy niz idealnie tez sie nadaje (bez ruchu / lekko w tyl, obrot).
	bool ReachTargetOk(const ReachParams& p, float heroYaw, float hx, float hz, float tx, float tz, float gap);

	struct ReachCandidate {
		float x = 0.0f, z = 0.0f;
		float gap = 0.0f;          // ReachGap dla tego celu
		bool hitTarget = false;    // cel ciosu silnika (oCAniCtrl_Human::hitTarget)
		bool focus = false;        // fokus bohatera
	};

	// Wybor celu: cel ciosu silnika (jesli sie nadaje), potem fokus, potem najmniejszy kat od przodu bohatera
	// (przy roznicy do 5 st. - mniejszy brak drogi). -1 = brak.
	int PickReachTarget(const ReachParams& p, float heroYaw, float hx, float hz, const std::vector<ReachCandidate>& c);

	// Trafienie z premia zasiegu (styl W3): odleglosc silnika <= zasieg + premia.
	inline bool ReachInRange(float surfDist, float range, float bonus) {
		return surfDist <= range + bonus;
	}
}
