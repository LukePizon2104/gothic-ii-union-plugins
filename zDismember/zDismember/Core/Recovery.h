// Logika bez silnika: kiedy finisher oddaje sterowanie graczowi.
#pragma once
#include <vector>

namespace zdcore {
	// Kat obrotu (stopnie) miedzy kwaternionami (x,y,z,w); q i -q to ten sam obrot.
	float QuatAngleDeg(const float a[4], const float b[4]);

	struct ReleaseParams {
		float fps = 25.0f;
		float minTime = 0.0f;            // nie wczesniej (ciecie + zwolnione tempo)
		float stillDegPerSec = 37.5f;    // ponizej = postac stoi (najszybszy wezel; 1,5 st./kl. przy 25 kl./s)
		float minStillSec = 0.24f;       // najkrotszy bezruch liczony jako koniec ruchu (krotsze = pauza w zamachu)
		float maxTailSec = 0.4f;         // ruch po bezruchu na samym koncu = doklejone przejscie do postawy (8 kl.)
		float blendSec = 0.3f;           // przejscie silnika w postawe: bez bezruchu oddajemy tyle przed koncem
	};

	// motionDeg[i] = najwiekszy obrot wezla miedzy klatka i a i+1 (animacja ma motionDeg.size() + 1 klatek).
	// Czas (s od poczatku animacji), w ktorym sterowanie wraca do gracza: poczatek koncowego bezruchu (finishery W3 -
	// bohater stoi 1-2 s, az ofiara upadnie), a gdy go nie ma - blendSec przed koncem. Nie wczesniej niz minTime
	// i nie pozniej niz ostatnia klatka.
	float ReleaseTime(const std::vector<float>& motionDeg, const ReleaseParams& p);
}
