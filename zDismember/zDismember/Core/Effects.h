// Logika bez silnika: efekty przy smierci (ogien, animacja smierci po finisherze) i kierunek wytrysku krwi.
// Tylko naglowek (funkcje inline) - bez nowego pliku .cpp w projekcie.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <string>
#include "ZMath.h"
#include "Catalog.h"

namespace zdcore {
	// oCNpc::BS_DEAD - DoDie ustawia go w bodyState (oCNpc::GetBodyState zwraca bity 0x7F).
	const int BODYSTATE_DEAD = 23;

	// Ponowne DoDie na juz martwym NPC. Silnik wola je sam z oCAniCtrl_Human::IsDead (0x6AD540, co klatke z
	// oCAIHuman::DoAI), gdy martwy NPC stoi bez zadnej animacji smierci: DoDie(0) + T_DEADB. Odciecie, ragdoll i reszta
	// naszej obslugi smierci zapadly przy pierwszym DoDie (finisher: wcale) - nie powtarzamy ich.
	inline bool IsRepeatDeath(int bodyStateBefore) {
		return (bodyStateBefore & 0x7F) == BODYSTATE_DEAD;
	}

	// Efekty palenia przypiete do NPC: VOB_BURN (obrazenia od ognia, oCNpc::OnDamage_Effects_Start), VOB_MAGICBURN
	// (oCVisualFX::ProcessCollision, emFXCollDynPerc_S ognistego pocisku), ich dzieci *_CHILD1..6 i dym
	// SPELLFX_MAGICFIRE_HUMANSMOKE; mody nazywaja je podobnie.
	inline bool IsBurnEffectName(const std::string& name) {
		std::string u = name;
		for (auto& c : u)
			c = (char)std::toupper((unsigned char)c);
		return u.find("BURN") != std::string::npos || u.find("HUMANSMOKE") != std::string::npos;
	}

	// Kierunek emitera czasteczek z katow skryptu (zCParticleEmitter::UpdateInternals 0x5B3FEF):
	// (-cos(elev) sin(head), sin(elev), cos(elev) cos(head)) w ukladzie voba (dirFOR_S = "object").
	inline Vec3 EmitterDir(float headDeg, float elevDeg) {
		const float k = 3.14159265f / 180.0f;
		float h = headDeg * k, e = elevDeg * k;
		return Vec3(-std::cos(e) * std::sin(h), std::sin(e), std::cos(e) * std::cos(h));
	}

	inline Vec3 Flat(const Vec3& v) { return Vec3(v.x, 0.0f, v.z); }

	// Poziomy kierunek wytrysku krwi: preferowany (np. od napastnika do ofiary), ale nigdy w strone kamery - skladowa
	// w strone kamery jest odbijana, a skladowa "od kamery" ma co najmniej minAway (rozrzut emitera +-20 stopni nie
	// trafia wtedy w kamere stojaca za bohaterem). camToHit: od kamery do punktu wytrysku. Pusty preferowany = od kamery.
	inline Vec3 SprayDirection(const Vec3& preferred, const Vec3& camToHit, float minAway = 0.35f) {
		Vec3 a = Flat(camToHit);
		Vec3 d = Flat(preferred);
		bool hasA = Length(a) > 1e-3f, hasD = Length(d) > 1e-3f;
		if (!hasA)
			return hasD ? Normalize(d) : Vec3(0.0f, 0.0f, 1.0f);   // kamera prosto nad punktem - dowolny poziomy
		a = Normalize(a);
		d = hasD ? Normalize(d) : a;
		float k = Dot(d, a);
		if (k < 0.0f) {
			d = d - a * (2.0f * k);   // odbicie od plaszczyzny prostopadlej do kierunku kamery
			k = -k;
		}
		if (k < minAway) {
			Vec3 perp = d - a * k;
			perp = Length(perp) > 1e-4f ? Normalize(perp) : Vec3(a.z, 0.0f, -a.x);
			d = perp * std::sqrt(std::max(0.0f, 1.0f - minAway * minAway)) + a * minAway;
		}
		return Normalize(d);
	}

	// Macierz voba PFX_BLOOD w punkcie pos: emiter krwi (head 90, elev -45 w ukladzie voba) strzela wzdluz lokalnej -X,
	// wiec kolumna X = -dir, Y = pion, Z = X x Y (obrot wokol pionu, wyznacznik +1).
	inline Mat34 SprayBasis(const Vec3& dir, const Vec3& pos) {
		Vec3 d = Normalize(Flat(dir));
		Vec3 x = d * -1.0f, y(0.0f, 1.0f, 0.0f), z = Cross(x, y);
		Mat34 m = Mat34::Identity();
		m.m[0][0] = x.x; m.m[1][0] = x.y; m.m[2][0] = x.z;
		m.m[0][1] = y.x; m.m[1][1] = y.y; m.m[2][1] = y.z;
		m.m[0][2] = z.x; m.m[1][2] = z.y; m.m[2][2] = z.z;
		m.m[0][3] = pos.x; m.m[1][3] = pos.y; m.m[2][3] = pos.z;
		return m;
	}

	// Animacja smierci ofiary finishera startowana w klatce ciecia. DoDie jej nie startuje (robi to dopiero awaryjne
	// DoDie z oCAniCtrl_Human::IsDead, gdy martwy stoi), a pierwsza polowa animacji ofiary (T_STAND_2_WOUNDED, next '')
	// konczy sie w klatce ciecia - ofiara stala sztywno. Para W3: druga polowa animacji ofiary (T_DEAD z jej nakladki,
	// T_DEADB tam taki sam). Bez animacji ofiary, stojaca: upadek z gry. Oszolomiona/lezaca: silnik (z nieprzytomnosci).
	inline const char* FinisherDeathAni(const FinisherDef& d, uint32_t pose) {
		if (!d.victimOverlay.empty() && !d.victimAni.empty())
			return "T_DEAD";
		if (pose == POSE_STANDING)
			return "T_DEADB";
		return "";
	}
}
