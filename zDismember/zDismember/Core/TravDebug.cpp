#include "TravDebug.h"
#include <cmath>
#include <cstdio>

namespace zdcore {
	TravRay TravRecorder::Cast(float x, float y, float z, float dx, float dy, float dz, float len) {
		TravRay r = inner.Cast(x, y, z, dx, dy, dz, len);
		TravRayRec rec;
		rec.stage = stage;
		rec.o[0] = x, rec.o[1] = y, rec.o[2] = z;
		rec.d[0] = dx, rec.d[1] = dy, rec.d[2] = dz;
		rec.len = len;
		rec.hit = r.hit && r.dist <= len;
		rec.dist = r.dist;
		rays.push_back(rec);
		return r;
	}

	static void Line(std::vector<TravDrawItem>& out, float ax, float ay, float az, float bx, float by, float bz, unsigned c) {
		TravDrawItem it;
		it.kind = TRAVDRAW_LINE;
		it.a[0] = ax, it.a[1] = ay, it.a[2] = az;
		it.b[0] = bx, it.b[1] = by, it.b[2] = bz;
		it.color = c;
		out.push_back(it);
	}

	static void Mark(std::vector<TravDrawItem>& out, float x, float y, float z, const char* glyph, unsigned c) {
		TravDrawItem it;
		it.kind = TRAVDRAW_MARK;
		it.a[0] = x, it.a[1] = y, it.a[2] = z;
		it.color = c;
		it.text = glyph;
		out.push_back(it);
	}

	static void Text(std::vector<TravDrawItem>& out, float x, float y, float z, const std::string& s, unsigned c) {
		TravDrawItem it;
		it.kind = TRAVDRAW_TEXT;
		it.a[0] = x, it.a[1] = y, it.a[2] = z;
		it.color = c;
		it.text = s;
		out.push_back(it);
	}

	// %.0f bez "-0" (kat -0,2 st., podstawa -0,4 cm)
	static double Clean(double v) {
		return std::fabs(v) < 0.5 ? 0.0 : v;
	}

	static std::string Fmt(const char* fmt, double a, double b = 0.0, double c = 0.0) {
		char buf[200];
		snprintf(buf, sizeof(buf), fmt, Clean(a), Clean(b), Clean(c));
		return buf;
	}

	std::string TravDecisionLabel(const TravResult& r, const TravDef* d) {
		char buf[200];
		const char* kind = r.kind == TRAV_CLIMB ? (d && d->idle ? "WSPIECIE Z MIEJSCA" : "WSPIECIE")
			: (d && d->idle ? "PRZESKOK Z MIEJSCA" : "PRZESKOK");
		if (r.kind == TRAV_NONE)
			snprintf(buf, sizeof(buf), "BRAK: %s", r.reason);
		else if (!d)
			snprintf(buf, sizeof(buf), "%s %d - brak klipu", kind, r.bucket);
		else
			snprintf(buf, sizeof(buf), "%s %d -> %s", kind, r.bucket, d->id.c_str());
		return buf;
	}

	std::vector<TravDrawItem> TravDebugBuild(const TravDebugInfo& in) {
		std::vector<TravDrawItem> out;
		const TravResult& r = in.res;
		// bohater: krzyzyk na ziemi i strzalka kierunku
		Line(out, -20.0f, 1.0f, 0.0f, 20.0f, 1.0f, 0.0f, TRAVCOL_HERO);
		Line(out, 0.0f, 1.0f, -20.0f, 0.0f, 1.0f, 20.0f, TRAVCOL_HERO);
		Line(out, 0.0f, 5.0f, 0.0f, 0.0f, 5.0f, 90.0f, TRAVCOL_HERO);
		Line(out, -12.0f, 5.0f, 72.0f, 0.0f, 5.0f, 90.0f, TRAVCOL_HERO);
		Line(out, 12.0f, 5.0f, 72.0f, 0.0f, 5.0f, 90.0f, TRAVCOL_HERO);
		// teren po drodze
		for (size_t i = 0; i < r.groundZ.size() && i < r.groundY.size(); i++) {
			Mark(out, 0.0f, r.groundY[i], r.groundZ[i], ".", TRAVCOL_GROUND);
			if (i > 0)
				Line(out, 0.0f, r.groundY[i - 1] + 1.0f, r.groundZ[i - 1], 0.0f, r.groundY[i] + 1.0f, r.groundZ[i], TRAVCOL_GROUNDLINE);
		}
		// promienie: szukania sciany z ostatniego kroku marszu, skoku terenu, profilu, gory, bokow, glebokosci, nad gora
		float lastZ = -1.0f;
		for (auto& ray : in.rays)
			if (ray.stage == TRAVST_MARCH_FWD && ray.d[2] > 0.5f && ray.o[2] > lastZ)
				lastZ = ray.o[2];
		float profileZ = r.wallDist > 0.0f ? r.wallDist : 0.0f;
		for (auto& ray : in.rays) {
			float t = ray.hit ? ray.dist : ray.len;
			float e[3] = { ray.o[0] + ray.d[0] * t, ray.o[1] + ray.d[1] * t, ray.o[2] + ray.d[2] * t };
			unsigned col = ray.hit ? TRAVCOL_HIT : TRAVCOL_FREE;
			bool fwd = ray.d[2] > 0.5f, down = ray.d[1] < -0.5f, upw = ray.d[1] > 0.5f;
			switch (ray.stage) {
			case TRAVST_MARCH_FWD:
				if (fwd && std::fabs(ray.o[2] - lastZ) < 0.5f)
					Line(out, ray.o[0], ray.o[1], ray.o[2], e[0], e[1], e[2], col);
				else if (down && ray.hit)
					Mark(out, e[0], e[1], e[2], "+", TRAVCOL_SIDE);   // potwierdzenie zerdzi (wierzch / ziemia za nia)
				break;
			case TRAVST_SPIKE:
				if (fwd)
					Line(out, ray.o[0], ray.o[1], ray.o[2], e[0], e[1], e[2], col);
				else if (ray.hit)
					Mark(out, e[0], e[1], e[2], "+", TRAVCOL_SIDE);
				break;
			case TRAVST_PROFILE:
				if (fwd)
					Mark(out, ray.o[0], ray.o[1], profileZ, ray.hit ? "x" : ".", col);
				break;
			case TRAVST_TOP:
				if (down && ray.hit)
					Mark(out, e[0], e[1], e[2], "^", TRAVCOL_TOP);
				break;
			case TRAVST_SIDES:
				if (down && ray.hit)
					Mark(out, e[0], e[1], e[2], "s", TRAVCOL_SIDE);
				break;
			case TRAVST_DEPTH:
				if (down && ray.hit)
					Mark(out, e[0], e[1], e[2], "o", std::fabs(e[1] - r.top) <= 22.0f ? TRAVCOL_TOP : TRAVCOL_THICK);
				break;
			case TRAVST_WIDTH:
				if (fwd)
					Line(out, ray.o[0], ray.o[1], ray.o[2], e[0], e[1], e[2], col);
				break;
			case TRAVST_END:
				if (down)
					Line(out, ray.o[0], ray.o[1], ray.o[2], e[0], e[1], e[2], ray.hit ? TRAVCOL_THICK : TRAVCOL_BAD);
				break;
			case TRAVST_CLEAR:
			case TRAVST_VAULT:
			case TRAVST_HEADROOM:
				if (fwd || upw)
					Line(out, ray.o[0], ray.o[1], ray.o[2], e[0], e[1], e[2], col);
				else if (down && ray.hit)
					Mark(out, e[0], e[1], e[2], "v", TRAVCOL_LAND);
				break;
			default:
				break;
			}
		}
		// sciana i podstawa
		if (r.wallDist > 0.0f) {
			float hTop = r.height > 0.0f ? r.top : (r.scanTop >= 0.0f ? r.base + r.scanTop : r.base + 150.0f);
			Line(out, 0.0f, r.base, r.wallDist, 0.0f, hTop, r.wallDist, TRAVCOL_WALL);
			Line(out, -25.0f, r.base, r.wallDist, 25.0f, r.base, r.wallDist, TRAVCOL_WALL);
			Mark(out, 0.0f, r.base, r.wallDist - 10.0f, "+", TRAVCOL_WALL);
			Text(out, -30.0f, r.base + 10.0f, r.wallDist - 10.0f, Fmt("podstawa %+.0f", r.base), TRAVCOL_WALL);
			Text(out, -40.0f, r.base + 0.5f * (hTop - r.base), r.wallDist,
				Fmt(r.round ? "SCIANA %.0f cm, kat %.0f, okragla" : "SCIANA %.0f cm, kat %.0f", r.face, r.wallYaw * 57.2958f),
				TRAVCOL_WALL);
		}
		// gorna krawedz (tu chwyta), glebokosc, grubosc
		if (r.height > 0.0f && !r.topZ.empty()) {
			float tz = r.topZ[0];
			Line(out, -in.lim.side, r.top, tz, in.lim.side, r.top, tz, TRAVCOL_TOP);
			Line(out, -in.lim.side, r.top + 2.0f, tz, in.lim.side, r.top + 2.0f, tz, TRAVCOL_TOP);
			Text(out, in.lim.side + 5.0f, r.top + 15.0f, tz, Fmt("GORA %.0f (podstawa %+.0f)", r.height, r.base), TRAVCOL_TOP);
			for (size_t i = 1; i < r.topZ.size() && i < r.topY.size(); i++)
				Mark(out, 0.0f, r.topY[i], r.topZ[i], "o", TRAVCOL_TOP);
			if (r.thick > 0.0f) {
				Mark(out, 0.0f, r.top, r.wallDist + r.thick, "|", TRAVCOL_THICK);
				Text(out, 10.0f, r.top - 15.0f, r.wallDist + r.thick, Fmt("grubosc %.0f", r.thick), TRAVCOL_THICK);
			}
		}
		// ziemia za przeszkoda (przeskok)
		if (r.wallDist > 0.0f && (r.vaultOk || r.kind == TRAV_VAULT)) {
			Mark(out, 0.0f, r.landY, r.wallDist + in.lim.vaultLandZ, "v", TRAVCOL_LAND);
			Text(out, 10.0f, r.landY + 15.0f, r.wallDist + in.lim.vaultLandZ, Fmt("ZIEMIA ZA %+.0f", r.landY), TRAVCOL_LAND);
		}
		// koniec ruchu i okno startu (gdzie musi byc bohater, zeby ruch ruszyl)
		if (in.def && r.kind != TRAV_NONE) {
			TravFit f = TravFitFor(*in.def, r);
			float x, y, z;
			TravEndPoint(*in.def, r, x, y, z);   // tor obrocony do sciany (wallYaw) jak w ruchu
			Mark(out, x, y, z, "K", TRAVCOL_THICK);
			Text(out, x + 10.0f, y + 20.0f, z, "KONIEC", TRAVCOL_THICK);
			float e = in.def->edge;
			float z1 = r.wallDist - in.tm.kMax * e, z2 = r.wallDist - in.tm.kMin * e, zg = r.wallDist - in.tm.kGo * e;
			if (in.def->idle) {   // z miejsca: od sciany do idleMaxFace (tor dochodzi do niej)
				z1 = r.wallDist - in.tm.idleMaxFace;
				z2 = r.wallDist - in.lim.minFace;
				zg = z2;
			}
			float g1 = TravGround(f, z1) + 2.0f, g2 = TravGround(f, z2) + 2.0f, gg = TravGround(f, zg) + 2.0f;
			for (float ox : { -15.0f, 0.0f, 15.0f })
				Line(out, ox, g1, z1, ox, g2, z2, TRAVCOL_WINDOW);
			Line(out, -15.0f, g1, z1, 15.0f, g1, z1, TRAVCOL_WINDOW);
			Line(out, -15.0f, g2, z2, 15.0f, g2, z2, TRAVCOL_WINDOW);
			Mark(out, 0.0f, gg, zg, ">", TRAVCOL_WINDOW);
			Text(out, 20.0f, g1 + 10.0f, 0.5f * (z1 + z2),
				in.def->idle ? Fmt("OKNO STARTU %.0f-%.0f cm od sciany (z miejsca)", in.lim.minFace, in.tm.idleMaxFace)
					: Fmt("OKNO STARTU %.0f-%.0f cm od sciany", in.tm.kMin * e, in.tm.kMax * e), TRAVCOL_WINDOW);
		}
		// co zrobi silnik: promien skoku w biegu (do sciany, gdy ja trafia) i luk lotu stop (do sciany albo zasiegu)
		if (in.showJump) {
			const TravGameJumpParams& p = in.jp;
			const TravGameJumpInfo& g = in.jump;
			float rayEnd = g.engineCancels && r.wallDist > 0.0f && r.wallDist <= p.rayLen ? r.wallDist : p.rayLen;
			Line(out, 0.0f, p.rayY, 0.0f, 0.0f, p.rayY + p.rayUp * rayEnd, rayEnd, g.engineCancels ? TRAVCOL_HIT : TRAVCOL_RAY);
			unsigned jc = g.verdict == TRAVJUMP_PASS ? TRAVCOL_JUMP_OK : g.verdict == TRAVJUMP_LEDGE ? TRAVCOL_JUMP_LEDGE : TRAVCOL_JUMP_NO;
			float zEnd = g.reach;
			if (r.wallDist > 0.0f && r.wallDist < zEnd)
				zEnd = r.wallDist;
			float zPrev = 0.0f, yPrev = 0.0f;
			for (float z = 20.0f; zPrev < zEnd - 0.5f; z += 20.0f) {
				float zc = z < zEnd ? z : zEnd;
				float y = TravGameJumpFeet(p, zc);   // nad ziemia startu (luk animacji, teren pod nim bez znaczenia)
				Line(out, 0.0f, yPrev, zPrev, 0.0f, y, zc, jc);
				zPrev = zc;
				yPrev = y;
			}
			if (r.wallDist > 0.0f && r.wallDist < g.reach && g.verdict != TRAVJUMP_PASS)
				Mark(out, 0.0f, yPrev, zPrev, "X", jc);
		}
		// decyzja i skok gry nad sciana (albo przed bohaterem)
		float yText = r.height > 0.0f ? r.top + 50.0f : (r.wallDist > 0.0f ? r.base + 200.0f : 150.0f);
		float zText = r.wallDist > 0.0f ? r.wallDist : 300.0f;
		if (!in.decision.empty())
			Text(out, 0.0f, yText, zText, in.decision, r.kind == TRAV_NONE ? TRAVCOL_BAD : TRAVCOL_TEXT);
		if (!in.gameJump.empty())
			Text(out, 0.0f, yText - 25.0f, zText, in.gameJump, !in.showJump ? TRAVCOL_BAD : in.jump.verdict == TRAVJUMP_PASS ?
				TRAVCOL_JUMP_OK : in.jump.verdict == TRAVJUMP_LEDGE ? TRAVCOL_JUMP_LEDGE : TRAVCOL_JUMP_NO);
		return out;
	}

	std::string TravStatusLine(float speed, bool sprint, float range, const char* buffer) {
		char buf[200];
		snprintf(buf, sizeof(buf), "PRZESKOK DEBUG | predkosc %.0f (%s) | zasieg %.0f cm | bufor: %s", speed,
			sprint ? "sprint" : (speed >= 150.0f ? "bieg" : "wolno"), range, buffer && *buffer ? buffer : "-");
		return buf;
	}
}
