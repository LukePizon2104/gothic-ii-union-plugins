#include "Ballistics.h"
#include <algorithm>

namespace zdcore {
	void BallisticSetShape(BallisticState& s, const std::vector<Vec3>& points) {
		s.points = points;
		Vec3 sum;
		for (auto& p : points)
			sum += p;
		s.center = points.empty() ? Vec3() : sum * (1.0f / (float)points.size());
		s.longAxis = Vec3(0, 1, 0);
		float best = 0.0f;
		for (auto& p : points) {
			Vec3 d = p - s.center;
			float l = Length(d);
			if (l > best) {
				best = l;
				s.longAxis = d;
			}
		}
	}

	static Vec3 LowestPoint(const BallisticState& s) {
		Vec3 low = Transform(s.trafo, s.points.empty() ? Vec3() : s.points[0]);
		for (auto& p : s.points) {
			Vec3 w = Transform(s.trafo, p);
			if (w.y < low.y)
				low = w;
		}
		return low;
	}

	// Podparcie: punkty ksztaltu do 'tol' nad najnizszym. Stabilnie, gdy rzut srodka (poziomo) lezy w prostokacie
	// punktow styku (3+ punkty) albo przy odcinku miedzy dwoma (lezaca reka, noga). Jeden punkt = balans -> przewrot.
	static bool SupportStable(const BallisticState& s, float lowY, const Vec3& center, float tol) {
		std::vector<Vec3> c;
		for (auto& pt : s.points) {
			Vec3 w = Transform(s.trafo, pt);
			if (w.y <= lowY + tol)
				c.push_back(w);
		}
		if (c.size() < 2)
			return false;
		const float margin = 3.0f;
		if (c.size() == 2) {
			float ax = c[1].x - c[0].x, az = c[1].z - c[0].z, l2 = ax * ax + az * az;
			if (l2 < 25.0f)
				return false;
			float u = ((center.x - c[0].x) * ax + (center.z - c[0].z) * az) / l2;
			float px = c[0].x + ax * u - center.x, pz = c[0].z + az * u - center.z;
			return u >= 0.0f && u <= 1.0f && std::sqrt(px * px + pz * pz) <= margin + 0.15f * std::sqrt(l2);
		}
		float x0 = c[0].x, x1 = c[0].x, z0 = c[0].z, z1 = c[0].z;
		for (auto& w : c) {
			x0 = std::min(x0, w.x); x1 = std::max(x1, w.x);
			z0 = std::min(z0, w.z); z1 = std::max(z1, w.z);
		}
		return center.x >= x0 - margin && center.x <= x1 + margin && center.z >= z0 - margin && center.z <= z1 + margin;
	}

	void BallisticStep(BallisticState& s, float dt, const TraceFn& trace, const BallisticParams& p) {
		if (s.resting || dt <= 0.0f)
			return;
		s.age += dt;
		s.vel.y -= p.gravity * dt;

		Vec3 c = Transform(s.trafo, s.center);
		Vec3 move = s.vel * dt;
		Vec3 hit, normal;
		if (Length(move) > 0.01f && trace(c, move + Normalize(move) * p.skin, hit, normal)) {
			float vn = Dot(s.vel, normal);
			if (vn < 0.0f)
				s.vel = s.vel + normal * (-p.wallBounce * vn);
			s.vel = s.vel * 0.7f;
			s.degPerSec *= 0.6f;
			move = Vec3();
		}
		s.trafo.Translate(move);
		c = c + move;
		if (s.degPerSec > 1.0f)
			RotateAround(s.trafo, c, s.axis, s.degPerSec * dt);

		Vec3 low = LowestPoint(s);
		if (trace(Vec3(low.x, low.y + 60.0f, low.z), Vec3(0.0f, -200.0f, 0.0f), hit, normal)) {
			float bottom = low.y - p.skin;
			if (bottom <= hit.y + 0.5f) {
				s.trafo.m[1][3] += hit.y - bottom;
				low.y += hit.y - bottom;
				if (s.vel.y < 0.0f) {
					if (s.vel.y < -120.0f)
						s.bounces++;
					s.vel.y = -s.vel.y * p.restitution;
					s.vel.x *= p.friction;
					s.vel.z *= p.friction;
					s.degPerSec *= 0.5f;
				}
				float damp = std::max(0.0f, 1.0f - p.rollDamp * dt);
				s.vel.x *= damp;
				s.vel.z *= damp;
				s.degPerSec *= damp;
				s.contactTime += dt;

				// brak podparcia (balans na punkcie / krawedzi obok srodka) -> przewracanie wokol punktu w strone srodka;
				// stara regula "dluga os stroma" krecila bryla (glowa, zgieta reka) przez krawedzie do limitu = drganie
				Vec3 cw = Transform(s.trafo, s.center);
				Vec3 d = cw - low;
				float h = std::sqrt(d.x * d.x + d.z * d.z);
				if (!SupportStable(s, low.y, cw, p.contactTol) && s.toppleTime < p.toppleMaxTime) {
					s.toppleTime += dt;
					Vec3 dh = h > 0.5f ? Vec3(d.x / h, 0.0f, d.z / h) : Vec3(1, 0, 0);
					RotateAround(s.trafo, low, Cross(Vec3(0, 1, 0), dh), p.toppleDegPerSec * dt);
				}
				else if (Length(s.vel) < p.restSpeed || s.contactTime > p.maxContactTime) {
					s.resting = true;
				}
			}
		}
		if (s.age > p.maxAge)
			s.resting = true;
		if (s.resting) {
			s.vel = Vec3();
			s.degPerSec = 0.0f;
		}
	}
}
