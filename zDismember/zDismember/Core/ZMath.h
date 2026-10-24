// Logika bez silnika: wektory i macierze w konwencji ZenGin (kolumnowej: swiat = M * p, przesuniecie w kolumnie 3).
#pragma once
#include <cmath>

namespace zdcore {
	struct Vec3 {
		float x = 0, y = 0, z = 0;
		Vec3() = default;
		Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
		Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
		Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
		Vec3 operator*(float f) const { return Vec3(x * f, y * f, z * f); }
		Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
	};

	inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	inline Vec3 Cross(const Vec3& a, const Vec3& b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
	inline float Length(const Vec3& a) { return std::sqrt(Dot(a, a)); }
	inline Vec3 Normalize(const Vec3& a) {
		float l = Length(a);
		return l > 1e-6f ? a * (1.0f / l) : Vec3(1, 0, 0);
	}

	// Kat obrotu wokol pionu (radiany) miedzy a i b w ulamku f, krotsza droga (przez +-180, gdy blizej).
	inline float LerpYaw(float a, float b, float f) {
		const float pi = 3.14159265f;
		float d = std::fmod(b - a, 2.0f * pi);
		if (d > pi)
			d -= 2.0f * pi;
		else if (d < -pi)
			d += 2.0f * pi;
		return a + d * f;
	}

	struct Mat34 {
		float m[3][4];
		static Mat34 Identity() {
			Mat34 r;
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 4; j++)
					r.m[i][j] = i == j ? 1.0f : 0.0f;
			return r;
		}
		Vec3 Translation() const { return Vec3(m[0][3], m[1][3], m[2][3]); }
		void Translate(const Vec3& d) { m[0][3] += d.x; m[1][3] += d.y; m[2][3] += d.z; }
	};

	inline Vec3 Rotate(const Mat34& m, const Vec3& v) {
		return Vec3(m.m[0][0] * v.x + m.m[0][1] * v.y + m.m[0][2] * v.z,
			m.m[1][0] * v.x + m.m[1][1] * v.y + m.m[1][2] * v.z,
			m.m[2][0] * v.x + m.m[2][1] * v.y + m.m[2][2] * v.z);
	}

	inline Vec3 Transform(const Mat34& m, const Vec3& p) {
		return Rotate(m, p) + m.Translation();
	}

	// Obrot obiektu (macierz obiekt->swiat) wokol punktu swiata o kat w stopniach (Rodrigues).
	inline void RotateAround(Mat34& m, const Vec3& pivot, const Vec3& axisIn, float deg) {
		Vec3 k = Normalize(axisIn);
		float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a), t = 1.0f - c;
		float R[3][3] = {
			{ c + k.x * k.x * t,       k.x * k.y * t - k.z * s, k.x * k.z * t + k.y * s },
			{ k.y * k.x * t + k.z * s, c + k.y * k.y * t,       k.y * k.z * t - k.x * s },
			{ k.z * k.x * t - k.y * s, k.z * k.y * t + k.x * s, c + k.z * k.z * t }
		};
		Mat34 o = m;
		float p[3] = { pivot.x, pivot.y, pivot.z };
		for (int r = 0; r < 3; r++) {
			for (int col = 0; col < 3; col++)
				m.m[r][col] = R[r][0] * o.m[0][col] + R[r][1] * o.m[1][col] + R[r][2] * o.m[2][col];
			float d0 = o.m[0][3] - p[0], d1 = o.m[1][3] - p[1], d2 = o.m[2][3] - p[2];
			m.m[r][3] = p[r] + R[r][0] * d0 + R[r][1] * d1 + R[r][2] * d2;
		}
	}
}
