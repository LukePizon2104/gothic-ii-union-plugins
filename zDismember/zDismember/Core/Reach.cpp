#include "Reach.h"
#include "Moves.h"
#include <algorithm>
#include <cmath>

namespace zdcore {
	float ReachGap(float surfDist, float range, float margin, float rootAhead) {
		return surfDist - (range - margin) - rootAhead;
	}

	float ReachTimeToHit(float optFrame, float actFrame, float fps) {
		if (fps <= 0.0f || actFrame >= optFrame)
			return 0.0f;
		return (optFrame - actFrame) / fps;
	}

	float ReachStep(const ReachParams& p, ReachState& s, float gap, float surfAtHit, float timeToHit, float dt) {
		if (timeToHit <= 0.0f || dt <= 0.0f) {
			s.speed = 0.0f;
			return 0.0f;
		}
		float budget = std::max(0.0f, p.maxPath - s.path);
		if (gap > 0.0f) {
			float v = std::min(gap / std::max(timeToHit, dt), p.maxSpeed);
			float vMax = std::max(s.speed, 0.0f) + p.accel * dt;
			if (v > vMax)
				v = vMax;
			float step = std::min(std::min(v * dt, gap), budget);
			s.speed = step / dt;
			s.path += step;
			return step;
		}
		if (surfAtHit < p.minSurface) {
			float step = std::min(std::min(p.backSpeed * dt, p.minSurface - surfAtHit), budget);
			s.speed = -step / dt;
			s.path += step;
			return -step;
		}
		s.speed = 0.0f;
		return 0.0f;
	}

	static float ReachAngle(float heroYaw, float hx, float hz, float tx, float tz) {
		return std::fabs(AngleDiff(heroYaw, YawTo(hx, hz, tx, tz)));
	}

	bool ReachTargetOk(const ReachParams& p, float heroYaw, float hx, float hz, float tx, float tz, float gap) {
		if (gap > p.maxPath + p.startSlack)
			return false;
		return ReachAngle(heroYaw, hx, hz, tx, tz) <= p.coneDeg * 3.14159265f / 180.0f;
	}

	int PickReachTarget(const ReachParams& p, float heroYaw, float hx, float hz, const std::vector<ReachCandidate>& c) {
		auto ok = [&](const ReachCandidate& r) { return ReachTargetOk(p, heroYaw, hx, hz, r.x, r.z, r.gap); };
		for (size_t i = 0; i < c.size(); i++)
			if (c[i].hitTarget && ok(c[i]))
				return (int)i;
		for (size_t i = 0; i < c.size(); i++)
			if (c[i].focus && ok(c[i]))
				return (int)i;
		const float tie = 5.0f * 3.14159265f / 180.0f;
		int best = -1;
		float bestAng = 0.0f;
		for (size_t i = 0; i < c.size(); i++) {
			if (!ok(c[i]))
				continue;
			float a = ReachAngle(heroYaw, hx, hz, c[i].x, c[i].z);
			if (best < 0 || a < bestAng - tie || (std::fabs(a - bestAng) <= tie && c[i].gap < c[best].gap)) {
				best = (int)i;
				bestAng = a;
			}
		}
		return best;
	}
}
