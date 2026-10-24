#include "Cursor.h"
#include <algorithm>

namespace trcore {
	bool ShouldAutoRewind(bool enabled, bool hasRune, bool busy, float bufferSeconds, int mana, int manaStart) {
		return enabled && hasRune && !busy && bufferSeconds >= 1.0f && mana >= manaStart;
	}

	void RewindCursor::Begin(float now, float oldestTime, const CursorParams& params) {
		p = params;
		start = t = now;
		oldest = oldestTime < now ? oldestTime : now;
		manaFrac = 0.0f;
		released = false;
		reason = StopReason::Running;
	}

	int RewindCursor::Step(float realDt, int manaAvailable) {
		if (Stopped())
			return 0;
		if (released && Rewound() >= p.minSeconds) {
			reason = StopReason::Released;
			return 0;
		}
		float want = realDt * p.speed;
		if (released)
			want = std::min(want, p.minSeconds - Rewound());
		float room = t - oldest;
		bool hitBuffer = want >= room;
		if (hitBuffer)
			want = room;
		bool hitMana = false;
		if (p.manaPerSecond > 0.0f) {
			float maxWant = ((float)manaAvailable + 1.0f - manaFrac) / p.manaPerSecond;
			if (want >= maxWant) {
				want = std::max(0.0f, maxWant - 1e-4f);
				hitMana = true;
				hitBuffer = false;
			}
		}
		t -= want;
		manaFrac += want * p.manaPerSecond;
		int pay = (int)manaFrac;
		if (pay > manaAvailable)
			pay = manaAvailable;
		manaFrac -= (float)pay;
		if (hitBuffer)
			reason = StopReason::BufferEnd;
		else if (hitMana)
			reason = StopReason::NoMana;
		else if (released && Rewound() >= p.minSeconds - 1e-5f)
			reason = StopReason::Released;
		return pay;
	}
}
