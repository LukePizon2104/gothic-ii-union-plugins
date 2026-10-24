#include "Timeline.h"
#include <algorithm>
#include <cmath>

namespace trcore {
	static const float PI = 3.14159265358979f;

	static float WrapAngle(float a) {
		a = std::fmod(a, 2.0f * PI);
		if (a > PI)
			a -= 2.0f * PI;
		if (a <= -PI)
			a += 2.0f * PI;
		return a;
	}

	float AngleDelta(float a, float b) {
		return WrapAngle(b - a);
	}

	float LerpAngle(float a, float b, float alpha) {
		return WrapAngle(a + AngleDelta(a, b) * alpha);
	}

	bool SameState(const Sample& a, const Sample& b) {
		return std::fabs(a.x - b.x) < 0.5f && std::fabs(a.y - b.y) < 0.5f && std::fabs(a.z - b.z) < 0.5f
			&& std::fabs(AngleDelta(a.yaw, b.yaw)) < 0.001f
			&& a.aniId == b.aniId && std::fabs(a.frame - b.frame) < 0.01f
			&& a.hp == b.hp && a.life == b.life && a.weapon == b.weapon;
	}

	Sample Interpolate(const Sample& a, const Sample& b, float t) {
		float span = b.t - a.t;
		float k = span > 0.0f ? (t - a.t) / span : 0.0f;
		k = k < 0.0f ? 0.0f : k > 1.0f ? 1.0f : k;
		Sample r = a;
		r.t = t;
		r.x = a.x + (b.x - a.x) * k;
		r.y = a.y + (b.y - a.y) * k;
		r.z = a.z + (b.z - a.z) * k;
		r.yaw = LerpAngle(a.yaw, b.yaw, k);
		if (a.aniId == b.aniId && b.frame >= a.frame)
			r.frame = a.frame + (b.frame - a.frame) * k;
		else if (k >= 0.5f) {
			r.aniId = b.aniId;
			r.frame = b.frame;
		}
		return r;
	}

	bool NeedsHardReset(const Sample& target, const Sample& latest, float distance) {
		float dx = target.x - latest.x, dy = target.y - latest.y, dz = target.z - latest.z;
		return dx * dx + dy * dy + dz * dz > distance * distance
			|| target.hp != latest.hp || target.life != latest.life || target.weapon != latest.weapon;
	}

	CommitAction DecideCommit(const Sample& target, const Sample& latest, bool deadNow, bool knockedNow, bool isHero, float distance) {
		if (target.life == LIFE_ALIVE || isHero) {
			if (deadNow || knockedNow)
				return CommitAction::HardTarget;
			return NeedsHardReset(target, latest, distance) ? CommitAction::HardTarget : CommitAction::SoftLatest;
		}
		if (deadNow || knockedNow)
			return CommitAction::Keep;
		return CommitAction::SoftLatest;
	}

	SampleAction DecideSample(bool inWorld, bool known) {
		if (inWorld)
			return SampleAction::Sample;
		return known ? SampleAction::Extend : SampleAction::Skip;
	}

	void Track::Extend(float t) {
		if (samples.empty())
			return;
		const Sample& last = hasHeld ? held : samples.back();
		if (t <= last.t)
			return;
		Sample s = last;
		s.t = t;
		held = s;
		hasHeld = true;
	}

	void Track::Push(const Sample& s) {
		if (!samples.empty() && SameState(samples.back(), s)) {
			held = s;
			hasHeld = true;
			return;
		}
		if (hasHeld && !samples.empty() && held.t > samples.back().t)
			samples.push_back(held);
		hasHeld = false;
		samples.push_back(s);
	}

	bool Track::At(float t, Sample& out) const {
		if (samples.empty() || t < samples.front().t)
			return false;
		if (t >= samples.back().t) {
			out = samples.back();
			out.t = t;
			return true;
		}
		auto it = std::upper_bound(samples.begin(), samples.end(), t,
			[](float v, const Sample& s) { return v < s.t; });
		out = Interpolate(*(it - 1), *it, t);
		return true;
	}

	void Track::TrimBefore(float t) {
		size_t keep = 0;
		while (keep + 1 < samples.size() && samples[keep + 1].t <= t)
			keep++;
		if (keep > 0)
			samples.erase(samples.begin(), samples.begin() + keep);
	}

	void Track::CutAfter(float t) {
		Sample at;
		bool ok = At(t, at);
		hasHeld = false;
		while (!samples.empty() && samples.back().t > t)
			samples.pop_back();
		if (ok && (samples.empty() || samples.back().t < t)) {
			at.t = t;
			samples.push_back(at);
		}
	}

	float Track::LastTime() const {
		if (hasHeld)
			return held.t;
		return samples.empty() ? 0.0f : samples.back().t;
	}

	void ClockTrack::Push(float t, float fullTime) {
		samples.push_back({ t, fullTime });
	}

	bool ClockTrack::At(float t, float& fullTime) const {
		if (samples.empty() || t < samples.front().t)
			return false;
		if (t >= samples.back().t) {
			fullTime = samples.back().fullTime;
			return true;
		}
		auto it = std::upper_bound(samples.begin(), samples.end(), t,
			[](float v, const ClockSample& s) { return v < s.t; });
		const ClockSample& a = *(it - 1);
		const ClockSample& b = *it;
		float span = b.t - a.t;
		float k = span > 0.0f ? (t - a.t) / span : 0.0f;
		fullTime = a.fullTime + (b.fullTime - a.fullTime) * k;
		return true;
	}

	void ClockTrack::TrimBefore(float t) {
		size_t keep = 0;
		while (keep + 1 < samples.size() && samples[keep + 1].t <= t)
			keep++;
		if (keep > 0)
			samples.erase(samples.begin(), samples.begin() + keep);
	}

	void ClockTrack::CutAfter(float t) {
		float v = 0.0f;
		bool ok = At(t, v);
		while (!samples.empty() && samples.back().t > t)
			samples.pop_back();
		if (ok && (samples.empty() || samples.back().t < t))
			samples.push_back({ t, v });
	}
}
