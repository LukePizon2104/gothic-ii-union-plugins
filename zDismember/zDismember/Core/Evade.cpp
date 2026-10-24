#include "Evade.h"
#include "Text.h"
#include <cmath>

namespace zdcore {
	static const float EXIT_DEFAULT_SHARE = 0.8f;

	float MoveExitFrom(const MoveDef& d, float endBlend) {
		float last = d.cancel - endBlend;
		float t;
		if (d.exitTime >= 0.0f)
			t = d.exitTime;
		else {
			t = TrajTimeAtShare(d.traj, d.trajFps, EXIT_DEFAULT_SHARE);
			if (t < d.iframeTo)
				t = d.iframeTo;
		}
		if (t > last)
			t = last;
		return t < 0.0f ? 0.0f : t;
	}

	bool IsBackstepParade(const std::string& aniName, const std::string& overlayName, bool fadingOut) {
		if (fadingOut || overlayName.empty() || Upper(aniName).find("PARADEJUMP") == std::string::npos)
			return false;
		return Upper(overlayName).find("_ZDF_MV_") == std::string::npos;
	}

	void ExitQueuePush(ExitQueue& q, int action, int dir) {
		q.action = action;
		q.dir = dir;
		q.age = 0.0f;
	}

	int ExitQueueStep(ExitQueue& q, bool windowOpen, float dt, float maxAge) {
		if (q.action == EXIT_NONE)
			return EXIT_NONE;
		if (windowOpen) {
			int a = q.action;
			q.action = EXIT_NONE;
			q.age = 0.0f;
			return a;
		}
		q.age += dt;
		if (q.age > maxAge) {
			q.action = EXIT_NONE;
			q.dir = -1;
			q.age = 0.0f;
		}
		return EXIT_NONE;
	}

	bool AttackPressed(bool gothic1, bool action, bool actionPrev, bool up, bool upPrev) {
		if (!gothic1)
			return action && !actionPrev;
		if (!action || !up)
			return false;
		return !upPrev || !actionPrev;
	}

	int AttackPressDir(bool gothic1, const AttackKeys& now, const AttackKeys& prev) {
		if (!gothic1) {
			if (now.actionLeft && !prev.actionLeft)
				return EXIT_ATK_LEFT;
			if (now.actionRight && !prev.actionRight)
				return EXIT_ATK_RIGHT;
			return now.action && !prev.action ? EXIT_ATK_FORWARD : EXIT_ATK_NONE;
		}
		if (!now.action)
			return EXIT_ATK_NONE;
		bool fresh = !prev.action;   // zbocze akcji - liczy sie trzymany kierunek
		if (now.up && (fresh || !prev.up))
			return EXIT_ATK_FORWARD;
		if (now.left && (fresh || !prev.left))
			return EXIT_ATK_LEFT;
		if (now.right && (fresh || !prev.right))
			return EXIT_ATK_RIGHT;
		return EXIT_ATK_NONE;
	}

	int ExitAttackAni(int& dir, int hitF, int hitL, int hitR) {
		if (dir == EXIT_ATK_LEFT && hitL >= 0)
			return hitL;
		if (dir == EXIT_ATK_RIGHT && hitR >= 0)
			return hitR;
		dir = EXIT_ATK_FORWARD;
		return hitF;
	}

	float LostTrackYaw(float prevYaw, float yaw, float maxRate, float dt) {
		return TurnToward(prevYaw, yaw, maxRate * dt);
	}

	float EndFaceYawStep(float yaw, float target, float t, float from, float arrive, float dt, float maxRate) {
		if (t < from || dt <= 0.0f)
			return yaw;
		float d = AngleDiff(yaw, target);
		float step = std::fabs(d);
		float rest = arrive - t;
		if (rest > dt)
			step = step * dt / rest;
		float cap = maxRate * dt;
		if (step > cap)
			step = cap;
		return yaw + (d >= 0.0f ? step : -step);
	}

	void HeroBoostArm(HeroBoost& b, float window) {
		b.armed = window > 0.0f;
		b.left = window;
	}

	float HeroBoostStep(HeroBoost& b, bool attacking, float dt, float cur, float boost) {
		if (b.active) {
			if (attacking)
				return cur;
			b.active = false;
			return std::fabs(cur - b.applied) < 1e-3f ? b.base : cur;
		}
		if (!b.armed)
			return cur;
		if (attacking) {
			b.armed = false;
			if (std::fabs(cur - 1.0f) > 0.02f || boost <= 0.0f)
				return cur;   // tempo zmienione przez inny modul - bez przyspieszenia
			b.active = true;
			b.base = cur;
			b.applied = cur * boost;
			return b.applied;
		}
		b.left -= dt;
		if (b.left <= 0.0f)
			b.armed = false;
		return cur;
	}
}
