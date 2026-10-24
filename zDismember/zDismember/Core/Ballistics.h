// Logika bez silnika: lot odcietej czesci (wlasna balistyka - fizyka silnika liczy kolizje bboxem calego modelu).
#pragma once
#include <functional>
#include <vector>
#include "ZMath.h"

namespace zdcore {
	// Promien od 'from' o dlugosci i kierunku 'ray'; true + punkt i normalna przy trafieniu.
	using TraceFn = std::function<bool(const Vec3& from, const Vec3& ray, Vec3& hit, Vec3& normal)>;

	struct BallisticParams {
		float gravity = 981.0f;        // cm/s^2
		float skin = 6.0f;             // grubosc miesa poza punktami szkieletu
		float restitution = 0.3f;      // odbicie od ziemi
		float wallBounce = 1.4f;       // odbicie od sciany (1 + wspolczynnik)
		float friction = 0.55f;        // utrata predkosci poziomej przy uderzeniu w ziemie
		float rollDamp = 1.5f;         // wytracanie predkosci przy kontakcie, 1/s
		float restSpeed = 40.0f;       // ponizej - spoczynek
		float contactTol = 2.5f;       // punkty do tylu cm nad najnizszym = podparcie
		float toppleDegPerSec = 240.0f;
		float toppleMaxTime = 1.5f;
		float maxContactTime = 2.5f;   // po tylu sekundach kontaktu z ziemia - spoczynek
		float maxAge = 10.0f;
	};

	struct BallisticState {
		Mat34 trafo = Mat34::Identity();   // uklad czesci -> swiat
		std::vector<Vec3> points;          // punkty ksztaltu w ukladzie czesci
		Vec3 center, longAxis = Vec3(0, 1, 0);
		Vec3 vel, axis = Vec3(0, 0, 1);
		float degPerSec = 0.0f;
		float age = 0.0f, toppleTime = 0.0f, contactTime = 0.0f;
		int bounces = 0;
		bool resting = false;
	};

	// Ustawia punkty, srodek (srednia) i dluga os (od srodka do najdalszego punktu).
	void BallisticSetShape(BallisticState& s, const std::vector<Vec3>& points);
	void BallisticStep(BallisticState& s, float dt, const TraceFn& trace, const BallisticParams& p = BallisticParams());
}
