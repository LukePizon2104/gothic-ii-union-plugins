#include "Recovery.h"
#include <algorithm>
#include <cmath>

namespace zdcore {
	float QuatAngleDeg(const float a[4], const float b[4]) {
		float d = std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]);
		if (d > 1.0f)
			d = 1.0f;
		return 2.0f * std::acos(d) * 57.29578f;
	}

	float ReleaseTime(const std::vector<float>& motionDeg, const ReleaseParams& p) {
		float fps = p.fps > 0.0f ? p.fps : 25.0f;
		int n = (int)motionDeg.size();
		float end = n / fps;
		float still = p.stillDegPerSec / fps;
		int minStill = std::max(1, (int)std::lround(p.minStillSec * fps));
		int maxTail = std::max(0, (int)std::lround(p.maxTailSec * fps));
		auto moving = [&](int i) { return motionDeg[i] >= still; };

		// koniec bezruchu: na samym koncu moze byc doklejone przejscie do postawy (krotki ruch)
		int e = n, tail = 0;
		while (e > 0 && moving(e - 1) && tail < maxTail) {
			e--;
			tail++;
		}
		float t = end - p.blendSec;
		if (e > 0 && !moving(e - 1)) {
			int s = e;
			while (s > 0 && !moving(s - 1))
				s--;
			if (e - s >= minStill)
				t = s / fps;
		}
		t = std::max(t, p.minTime);
		return std::max(0.0f, std::min(t, end));
	}
}
