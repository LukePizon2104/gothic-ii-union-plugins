#include "Parry.h"
#include "Text.h"
#include "Ragdoll.h"
#include <cstdlib>
#include <cstring>

namespace zdcore {
	static float Clamp(float v, float lo, float hi) {
		return v < lo ? lo : v > hi ? hi : v;
	}

	ParryMat ClassifyParryMat(bool hasWeapon, int material, const std::string& instance, const std::string& visual,
		const std::vector<std::string>& woodTokens) {
		if (!hasWeapon)
			return ParryMat::NoWeapon;
		if (NameHasToken(instance, woodTokens) || NameHasToken(visual, woodTokens))
			return ParryMat::Wood;
		switch (material) {
		case ITEM_MAT_WOOD: return ParryMat::Wood;
		case ITEM_MAT_LEATHER:
		case ITEM_MAT_CLAY:
		case ITEM_MAT_GLAS: return ParryMat::Soft;
		default: return ParryMat::Hard;   // metal, kamien, nieznany z modu
		}
	}

	const char* ParryMatName(ParryMat m) {
		switch (m) {
		case ParryMat::Hard: return "metal";
		case ParryMat::Wood: return "drewno";
		case ParryMat::Soft: return "miekki";
		default: return "brak broni";
		}
	}

	uint32_t ParryFx(ParryMat a, ParryMat b) {
		if (a == ParryMat::NoWeapon || b == ParryMat::NoWeapon)
			return PARRY_FX_NONE;
		if (a == ParryMat::Wood || b == ParryMat::Wood)
			return PARRY_FX_SPLINTERS;
		if (a == ParryMat::Hard && b == ParryMat::Hard)
			return PARRY_FX_SPARKS | PARRY_FX_FLASH;
		return PARRY_FX_NONE;
	}

	bool IsBlockParade(const std::string& aniName, const std::string& overlayName) {
		std::string a = Upper(aniName), o = Upper(overlayName);
		if (a.find("PARADE") == std::string::npos)
			return false;
		if (a.find("PARADEJUMP") == std::string::npos)
			return true;
		return o.find("_MV_COUNTER") != std::string::npos || o.find("_MV_BASH") != std::string::npos ||
			o.find("_MV_KICK") != std::string::npos;
	}

	// Real-Time Collision Detection (Ericson), 5.1.9.
	void ClosestSegmentPoints(const Vec3& p0, const Vec3& p1, const Vec3& q0, const Vec3& q1, float& s, float& t) {
		const float EPS = 1e-6f;
		Vec3 d1 = p1 - p0, d2 = q1 - q0, r = p0 - q0;
		float a = Dot(d1, d1), e = Dot(d2, d2), f = Dot(d2, r);
		s = t = 0.0f;
		if (a <= EPS && e <= EPS)
			return;
		if (a <= EPS) {
			t = Clamp(f / e, 0.0f, 1.0f);
			return;
		}
		float c = Dot(d1, r);
		if (e <= EPS) {
			s = Clamp(-c / a, 0.0f, 1.0f);
			return;
		}
		float b = Dot(d1, d2), denom = a * e - b * b;
		s = denom > EPS ? Clamp((b * f - c * e) / denom, 0.0f, 1.0f) : 0.0f;
		t = (b * s + f) / e;
		if (t < 0.0f) {
			t = 0.0f;
			s = Clamp(-c / a, 0.0f, 1.0f);
		}
		else if (t > 1.0f) {
			t = 1.0f;
			s = Clamp((b - c) / a, 0.0f, 1.0f);
		}
	}

	float BladeGuard(float length) {
		return Clamp(length * 0.18f, 6.0f, 20.0f);
	}

	static void BladeSegment(const Blade& b, Vec3& p0, Vec3& p1) {
		float g = BladeGuard(b.length);
		p0 = b.hand + b.dir * g;
		p1 = b.hand + b.dir * std::max(g, b.length);
	}

	Vec3 ParryContactPoint(const Blade& attacker, const Blade& defender, float maxGap, float fallbackT) {
		Vec3 a0, a1, d0, d1;
		BladeSegment(attacker, a0, a1);
		BladeSegment(defender, d0, d1);
		float s = 0.0f, t = 0.0f;
		ClosestSegmentPoints(a0, a1, d0, d1, s, t);
		Vec3 pa = a0 + (a1 - a0) * s, pd = d0 + (d1 - d0) * t;
		if (Length(pa - pd) <= maxGap)
			return (pa + pd) * 0.5f;
		return d0 + (d1 - d0) * Clamp(fallbackT, 0.0f, 1.0f);
	}

	bool BladeFromBox(float minX, float maxX, int itemRange, float& length, float& dirSign) {
		float neg = -minX, pos = maxX, l = std::max(neg, pos);
		if (l >= 20.0f && l <= 250.0f) {
			length = l;
			dirSign = neg >= pos ? -1.0f : 1.0f;
			return true;
		}
		length = itemRange > 0 ? Clamp((float)itemRange, 35.0f, 160.0f) : 90.0f;
		dirSign = -1.0f;   // bron w slocie ZS_RIGHTHAND lezy wzdluz -X
		return false;
	}

	float RecoilCurve(float t, float duration, float peakFrac) {
		if (duration <= 0.0f || t <= 0.0f || t >= duration)
			return 0.0f;
		const float pi = 3.14159265f;
		float pk = Clamp(peakFrac, 0.05f, 0.9f);
		float u = t / duration;
		if (u < pk)
			return std::sin(0.5f * pi * u / pk);
		float v = (u - pk) / (1.0f - pk);
		float k = 1.0f - v;
		return k * k * std::cos(1.5f * pi * v);   // zero w 1/3, odbicie ~-0,14, gladko do zera
	}

	Vec3 RecoilPush(const Vec3& attackerToDefender, bool defender) {
		Vec3 f(attackerToDefender.x, 0.0f, attackerToDefender.z);
		f = Length(f) > 1e-3f ? Normalize(f) : Vec3(0.0f, 0.0f, 1.0f);
		if (defender)
			return Normalize(f + Vec3(0.0f, -0.2f, 0.0f));
		return Normalize(f * -1.0f + Vec3(0.0f, 0.45f, 0.0f));
	}

	Vec3 RecoilAxis(const Vec3& joint, const Vec3& contact, const Vec3& push) {
		Vec3 r = contact - joint;
		float lr = Length(r), lp = Length(push);
		if (lr < 1.0f || lp < 1e-4f)
			return Vec3();
		Vec3 a = Cross(r, push);
		if (Length(a) < 0.05f * lr * lp)   // prawie rownolegle (< ~3 st.)
			return Vec3();
		return Normalize(a);
	}

	float RecoilRoleScale(bool defender) {
		return defender ? 1.0f : 1.3f;
	}

	float RecoilRoleDuration(bool defender) {
		return defender ? 1.0f : 1.15f;
	}

	const std::vector<RecoilJoint>& RecoilJoints(bool twoHanded) {
		static const std::vector<RecoilJoint> oneHand = {
			{ "BIP01 SPINE2", 0.25f },
			{ "BIP01 R UPPERARM", 0.5f },
			{ "BIP01 R FOREARM", 0.6f },
			{ "BIP01 R HAND", 0.8f },
			{ "ZS_RIGHTHAND", 0.6f },
		};
		static const std::vector<RecoilJoint> twoHands = {
			{ "BIP01 SPINE2", 0.6f },
			{ "BIP01 R UPPERARM", 0.35f },
			{ "BIP01 R FOREARM", 0.35f },
			{ "BIP01 R HAND", 0.3f },
			{ "BIP01 L UPPERARM", 0.35f },
			{ "BIP01 L FOREARM", 0.35f },
			{ "BIP01 L HAND", 0.3f },
		};
		return twoHanded ? twoHands : oneHand;
	}

	Vec3 AxisInParent(const Mat34& parentToModel, const Vec3& axisModel) {
		Vec3 v = Rotate(Transposed3(parentToModel), axisModel);   // skala rodzica znika w normalizacji
		float l = Length(v);
		return l > 1e-9f ? v * (1.0f / l) : Vec3();
	}

	void RotateBasis(Mat34& local, const Vec3& axis, float deg) {
		if (Length(axis) < 1e-6f || deg == 0.0f)
			return;
		RotateAround(local, local.Translation(), axis, deg);
	}

	bool SameMat(const Mat34& a, const Mat34& b) {
		return std::memcmp(a.m, b.m, sizeof(a.m)) == 0;
	}

	Mat34 RecoilBase(RecoilNodeState& s, const Mat34& current) {
		if (!s.has || !s.wrote || !SameMat(current, s.written))
			s.base = current;   // nowa poza z animacji
		s.has = true;
		return s.base;
	}

	void RecoilWritten(RecoilNodeState& s, const Mat34& written) {
		s.written = written;
		s.wrote = true;
	}

	bool RecoilRestore(const RecoilNodeState& s, const Mat34& current, Mat34& out) {
		if (!s.has || !s.wrote || !SameMat(current, s.written))
			return false;
		out = s.base;
		return true;
	}

	EmitterSpec ParrySparksSpec() {
		EmitterSpec e;
		e.name = "ZDISM_PARRY_SPARKS";
		e.ppsValue = 240.0f;          // x 0,1 s = ~24 iskry
		e.ppsScaleKeys = "1";
		e.ppsFPS = 10.0f;
		e.shpType = "SPHERE";
		e.shpDim = "4";
		e.shpIsVolume = 1;
		e.dirMode = "DIR";
		e.dirAngleHead = 0.0f;
		e.dirAngleHeadVar = 180.0f;   // na wszystkie strony
		e.dirAngleElev = 20.0f;
		e.dirAngleElevVar = 55.0f;
		e.velAvg = 0.4f;              // 4 m/s
		e.velVar = 0.18f;
		e.lspPartAvg = 280.0f;
		e.lspPartVar = 120.0f;
		e.flyGravity = "0 -0.0012 0";
		e.visName = "ZSPARK1.TGA";    // smuga iskry (PFX_METALSPARKS, kowadlo)
		e.visOrientation = "VELO";
		e.visTexColorStart = "255 235 170";
		e.visTexColorEnd = "255 120 40";
		e.visSizeStart = "4 13";
		e.visSizeEndScale = 0.35f;
		e.visAlphaFunc = "ADD";
		e.visAlphaStart = 255.0f;
		e.visAlphaEnd = 0.0f;
		return e;
	}

	EmitterSpec ParryFlashSpec() {
		EmitterSpec e;
		e.name = "ZDISM_PARRY_FLASH";
		e.ppsValue = 30.0f;           // x 1/20 s = 1-2 czasteczki
		e.ppsScaleKeys = "1";
		e.ppsFPS = 20.0f;
		e.dirMode = "NONE";
		e.lspPartAvg = 110.0f;
		e.lspPartVar = 20.0f;
		e.visName = "CFLARESTAR.TGA";
		e.visOrientation = "NONE";
		e.visTexColorStart = "255 245 210";
		e.visTexColorEnd = "255 170 90";
		e.visSizeStart = "24 24";
		e.visSizeEndScale = 1.5f;
		e.visAlphaFunc = "ADD";
		e.visAlphaStart = 230.0f;
		e.visAlphaEnd = 0.0f;
		return e;
	}

	EmitterSpec ParrySplintersSpec() {
		EmitterSpec e;
		e.name = "ZDISM_PARRY_SPLINTERS";
		e.ppsValue = 170.0f;          // x 0,1 s = ~17 drzazg
		e.ppsScaleKeys = "1";
		e.ppsFPS = 10.0f;
		e.shpType = "SPHERE";
		e.shpDim = "3";
		e.shpIsVolume = 1;
		e.dirMode = "DIR";
		e.dirAngleHead = 0.0f;
		e.dirAngleHeadVar = 180.0f;
		e.dirAngleElev = 35.0f;
		e.dirAngleElevVar = 45.0f;
		e.velAvg = 0.28f;
		e.velVar = 0.12f;
		e.lspPartAvg = 650.0f;
		e.lspPartVar = 200.0f;
		e.flyGravity = "0 -0.002 0";
		e.visName = "CPFX_WOOD.TGA";  // wiorek z CPFX_IAI_WOOD
		e.visOrientation = "VELO";
		e.visTexColorStart = "255 255 255";
		e.visTexColorEnd = "220 200 170";
		e.visSizeStart = "5 5";
		e.visSizeEndScale = 0.8f;
		e.visAlphaFunc = "BLEND";
		e.visAlphaStart = 255.0f;
		e.visAlphaEnd = 120.0f;
		return e;
	}

	static float SumKeys(const std::string& keys) {
		float sum = 0.0f;
		const char* p = keys.c_str();
		char* end = nullptr;
		for (;;) {
			float v = std::strtof(p, &end);
			if (end == p)
				break;
			sum += v;
			p = end;
		}
		return sum;
	}

	float EmitterBurstCount(const EmitterSpec& s) {
		return s.ppsFPS > 0.0f ? s.ppsValue * SumKeys(s.ppsScaleKeys) / s.ppsFPS : 0.0f;
	}

	static float CountKeys(const std::string& keys) {
		float n = 0.0f;
		const char* p = keys.c_str();
		char* end = nullptr;
		for (;;) {
			std::strtof(p, &end);
			if (end == p)
				break;
			n += 1.0f;
			p = end;
		}
		return n;
	}

	float EmitterLifetime(const EmitterSpec& s) {
		float emit = s.ppsFPS > 0.0f ? CountKeys(s.ppsScaleKeys) / s.ppsFPS : 0.0f;
		return emit + (s.lspPartAvg + s.lspPartVar) / 1000.0f + 0.3f;
	}
}
