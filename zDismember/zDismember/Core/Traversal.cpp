#include "Traversal.h"
#include "Text.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace zdcore {
	bool ParseTraj3(const std::string& value, std::vector<float>& out) {
		out.clear();
		size_t pos = 0;
		while (pos < value.size()) {
			size_t start = value.find_first_not_of(" \t", pos);
			if (start == std::string::npos)
				break;
			size_t end = value.find_first_of(" \t", start);
			std::string item = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
			float p[3];
			if (!ParseFloatList(item, p, 3))
				return false;
			out.insert(out.end(), p, p + 3);
			pos = end == std::string::npos ? value.size() : end;
		}
		return !out.empty();
	}

	void SampleTraj3(const std::vector<float>& xyz, float fps, float t, float& x, float& y, float& z) {
		size_t n = xyz.size() / 3;
		x = y = z = 0.0f;
		if (n == 0)
			return;
		float f = t * fps;
		size_t i;
		float u;
		if (f <= 0.0f) {
			i = 0;
			u = 0.0f;
		}
		else if (f >= (float)(n - 1)) {
			i = n - 1;
			u = 0.0f;
		}
		else {
			i = (size_t)f;
			u = f - (float)i;
		}
		const float* a = &xyz[3 * i];
		const float* b = i + 1 < n ? &xyz[3 * (i + 1)] : a;
		x = a[0] + (b[0] - a[0]) * u;
		y = a[1] + (b[1] - a[1]) * u;
		z = a[2] + (b[2] - a[2]) * u;
	}

	static bool ReadFloat(const std::string& val, float& out) {
		char* e = nullptr;
		out = strtof(val.c_str(), &e);
		return e && e != val.c_str() && *e == '\0';
	}

	std::vector<TravDef> ParseTraversal(const std::string& text, std::vector<std::string>& errors) {
		std::vector<TravDef> out;
		TravDef cur;
		bool inSection = false, bad = false;
		std::string badReason;

		auto close = [&]() {
			if (!inSection)
				return;
			if (!bad) {
				if (cur.overlay.empty() || cur.traj.size() < 6 || cur.kind == TRAV_NONE)
					bad = true, badReason = "brak overlay, kind albo traj";
				else if (cur.edge <= 0.0f || cur.contact < cur.takeoff || cur.cancel <= cur.contact || cur.exit > cur.cancel)
					bad = true, badReason = "zle czasy albo edge";
				else if (cur.kind == TRAV_VAULT && (cur.release < cur.contact || cur.land <= cur.release || cur.land > cur.cancel))
					bad = true, badReason = "przeskok bez release/land";
			}
			if (bad)
				errors.push_back(cur.id + ": " + badReason);
			else
				out.push_back(cur);
		};

		size_t pos = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;
		while (pos < text.size()) {
			size_t nl = text.find('\n', pos);
			std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
			pos = nl == std::string::npos ? text.size() : nl + 1;
			size_t sc = line.find(';');
			if (sc != std::string::npos)
				line = line.substr(0, sc);
			line = Trim(line);
			if (line.empty())
				continue;
			if (line.front() == '[' && line.back() == ']') {
				close();
				cur = TravDef();
				cur.id = Upper(Trim(line.substr(1, line.size() - 2)));
				inSection = true;
				bad = false;
				continue;
			}
			if (!inSection) {
				errors.push_back("linia poza sekcja: " + line);
				continue;
			}
			if (bad)
				continue;
			size_t eq = line.find('=');
			if (eq == std::string::npos) {
				bad = true;
				badReason = "linia bez '=': " + line;
				continue;
			}
			std::string key = Upper(Trim(line.substr(0, eq)));
			std::string val = Trim(line.substr(eq + 1));
			bool ok = true;
			float f = 0.0f;
			if (key == "OVERLAY")
				ok = !(cur.overlay = val).empty();
			else if (key == "KIND") {
				std::string k = Upper(val);
				cur.kind = k == "CLIMB" ? TRAV_CLIMB : k == "VAULT" ? TRAV_VAULT : TRAV_NONE;
				ok = cur.kind != TRAV_NONE;
			}
			else if (key == "BUCKET")
				ok = ReadFloat(val, f) && f > 0.0f && (cur.bucket = (int)(f + 0.5f)) > 0;
			else if (key == "HEIGHT")
				ok = ReadFloat(val, cur.height) && cur.height > 0.0f;
			else if (key == "FOOT") {
				std::string k = Upper(val);
				ok = k == "L" || k == "R";
				cur.swingLeft = k == "L";
			}
			else if (key == "START") {
				std::string k = Upper(val);
				ok = k == "IDLE" || k == "RUN";
				cur.idle = k == "IDLE";
			}
			else if (key == "TRAJFPS")
				ok = ReadFloat(val, cur.trajFps) && cur.trajFps > 0.0f;
			else if (key == "TRAJ")
				ok = ParseTraj3(val, cur.traj);
			else if (key == "EDGE")
				ok = ReadFloat(val, cur.edge);
			else if (key == "TAKEOFF")
				ok = ReadFloat(val, cur.takeoff) && cur.takeoff >= 0.0f;
			else if (key == "CONTACT")
				ok = ReadFloat(val, cur.contact) && cur.contact >= 0.0f;
			else if (key == "RELEASE")
				ok = ReadFloat(val, cur.release) && cur.release >= 0.0f;
			else if (key == "LAND")
				ok = ReadFloat(val, cur.land) && cur.land >= 0.0f;
			else if (key == "DEPTH")
				ok = ReadFloat(val, cur.depth) && cur.depth >= 0.0f;
			else if (key == "THICK")
				ok = ReadFloat(val, cur.thick) && cur.thick >= 0.0f;
			else if (key == "EXIT")
				ok = ReadFloat(val, cur.exit) && cur.exit >= 0.0f;
			else if (key == "CANCEL")
				ok = ReadFloat(val, cur.cancel) && cur.cancel > 0.0f;
			else
				ok = false;
			if (!ok) {
				bad = true;
				badReason = "zla wartosc: " + line;
			}
		}
		close();
		return out;
	}

	// ---------- Wykrywanie ----------

	static bool IsWall(const TravRay& r, const TravLimits& lim, float& facing) {
		if (!r.hit)
			return false;
		if (!r.normal) {
			facing = 1.0f;   // vob bez normalnej - przyjmij sciane na wprost
			return true;
		}
		float h = std::sqrt(r.nx * r.nx + r.nz * r.nz);
		if (std::fabs(r.ny) > lim.wallMaxNy || h < 1e-3f)
			return false;
		facing = -r.nz / h;
		return facing >= lim.wallMinFacing;
	}

	int TravBucket(float height, const TravLimits& lim) {
		if (height < lim.minHeight || height > lim.maxHeight)
			return 0;
		if (height < 65.0f)
			return 30;
		if (height < 125.0f)
			return 100;
		if (height < 175.0f)
			return 150;
		if (height < 225.0f)
			return 200;
		return 250;
	}

	static TravResult Fail(TravResult r, const char* why) {
		r.kind = TRAV_NONE;
		r.reason = why;
		return r;
	}

	static void Logf(std::string* log, const char* fmt, ...) {
		if (!log)
			return;
		char buf[600];
		va_list a;
		va_start(a, fmt);
		vsnprintf(buf, sizeof(buf), fmt, a);
		va_end(a);
		*log += buf;
	}

	// Gora za sciana: najblizsza plaska z promieni w dol (zerdz plotu, wystep, gzyms - pierwszy trafia wierzch); przed
	// nia dopuszczalna warga (zaokraglona / pochyla krawedz nasypu: do lipMinNy, bardziej stroma tylko 20 cm pod gora,
	// najwyzej lipDrop ponizej gory). Wysokosc = plaska gora, na ktorej bohater stanie.
	struct TravTop {
		bool ok = false, steep = false;
		bool slope = false;               // bez plaskiej: pochyla powierzchnia do chodzenia od krawedzi (dach)
		float y = 0.0f, z = 0.0f;
		int lips = 0;
	};
	static TravTop FindTop(TravWorld& w, const TravLimits& lim, float wallZ, float yFrom, float yTo) {
		TravTop best;
		float lipY[8], lipNy[8], lipIns[8];
		int lips = 0;
		for (float ins : lim.topInsets) {
			TravRay t = w.Down(0.0f, wallZ + ins, yFrom, yTo);
			if (!t.hit)
				continue;
			float y = yFrom - t.dist;
			bool flat = !t.normal || t.ny >= lim.topMinNy;
			if (flat) {
				for (int i = 0; i < lips; i++) {
					bool steepOk = lipNy[i] >= lim.lipMinNy || lipY[i] >= y - 20.0f;
					if (lipY[i] > y + 5.0f || lipY[i] < y - lim.lipDrop || !steepOk) {
						best.steep = true;
						return best;
					}
				}
				best.ok = true;
				best.y = y;
				best.z = wallZ + ins;
				best.lips = lips;
				return best;
			}
			if (lips < 8) {
				lipY[lips] = y;
				lipNy[lips] = t.normal ? t.ny : 1.0f;
				lipIns[lips] = ins;
				lips++;
			}
			best.steep = true;
		}
		// bez plaskiej gory: pochyla powierzchnia do chodzenia od samej krawedzi (dach do ok. 45 st., stok) - wszystkie
		// trafienia do walkMinNy, ciagle (bez stopni ponad nachylenie); gora = krawedz, koniec ruchu na jej wysokosci dalej
		if (lips >= 2 && lipIns[0] <= 12.0f) {
			bool walk = true;
			for (int i = 0; i < lips && walk; i++) {
				if (lipNy[i] < lim.walkMinNy)
					walk = false;
				else if (i > 0 && std::fabs(lipY[i] - lipY[i - 1]) > (lipIns[i] - lipIns[i - 1]) * lim.walkTan + 3.0f)
					walk = false;
			}
			if (walk) {
				best.ok = true;
				best.steep = false;
				best.slope = true;
				best.y = lipY[0];
				best.z = wallZ + lipIns[0];
				best.lips = 0;
			}
		}
		return best;
	}

	// Wysokosc gory za sciana w z (od bohatera): profil topZ / topY, przed nim pierwszy punkt, za nim ostatni.
	static float TopYAt(const TravResult& r, float zq) {
		if (r.topZ.empty() || r.topZ.size() != r.topY.size())
			return r.top;
		if (zq <= r.topZ[0])
			return r.topY[0];
		for (size_t i = 1; i < r.topZ.size(); i++)
			if (zq <= r.topZ[i]) {
				float u = (zq - r.topZ[i - 1]) / std::fmax(1e-3f, r.topZ[i] - r.topZ[i - 1]);
				return r.topY[i - 1] + (r.topY[i] - r.topY[i - 1]) * u;
			}
		return r.topY.back();
	}

	// Wysokosci promieni sciany i profilu: co faceStep od faceFrom do faceTo (gesto - cienkie zerdzie); profil gesto do
	// 120 cm, wyzej co scanStep do scanMax.
	static std::vector<float> ProfileHeights(const TravLimits& lim) {
		std::vector<float> hs;
		for (float h = lim.faceFrom; h < 120.0f - 0.1f; h += lim.faceStep)
			hs.push_back(h);
		for (float h = 120.0f; h <= lim.scanMax + 0.1f; h += lim.scanStep)
			hs.push_back(h);
		return hs;
	}

	TravResult TravDetect(TravWorld& w, const TravLimits& lim, std::string* log) {
		TravResult r;
		// 1. sciana: marsz po terenie co marchStep - promien w dol (ziemia lokalna) i promienie poziome nad nia; trafienie
		// w podloze / stok (normalna w gore) nie konczy szukania, nastepny krok startuje z ziemi lokalnej. Bufor: marsz od
		// marchFrom (okolica znanej sciany) z ziemi marchFromY.
		float z = lim.marchFrom > 0.0f ? lim.marchFrom : 0.0f, g = lim.marchFrom > 0.0f ? lim.marchFromY : 0.0f;
		r.groundZ.push_back(z);
		r.groundY.push_back(g);
		bool wall = false;
		float wallZ = 0.0f, facing = 1.0f, wallH = 0.0f;
		TravRay wallRay;
		const char* stop = nullptr;
		const char* how = "";
		while (z < lim.maxFace - 0.5f) {
			w.Stage(TRAVST_MARCH_FWD);
			float seg = std::fmin(lim.marchStep, lim.maxFace - z);
			// sciana = co najmniej dwa promienie (rozne wysokosci) trafiaja pion w odstepie do 30 cm - pojedyncze trafienie
			// (kamien, krawleznik, belka nad glowa) to teren / drobiazg, szukanie idzie dalej
			// (drugi moze byc pochyly do |ny| wallPairNy - zerdz plotu trafiona przy wierzchu; stok sam nie da pary)
			struct Hit { float d, h, fc; TravRay r; bool strong; };
			std::vector<Hit> hits;
			Hit any = { -1.0f, 0.0f, 1.0f, TravRay(), false };   // najblizsze trafienie dowolne (skarpa bez ziemi za nia)
			for (float h = lim.faceFrom; h <= lim.faceTo + 0.1f; h += lim.faceStep) {
				TravRay fr = w.Forward(0.0f, g + h, z, seg + 5.0f);
				if (fr.hit && fr.dist <= seg + 5.0f && (any.d < 0.0f || fr.dist < any.d))
					any = { fr.dist, h, 1.0f, fr, false };
				float fc = 1.0f;
				if (IsWall(fr, lim, fc))
					hits.push_back({ fr.dist, h, fc, fr, true });
				else if (fr.hit && fr.normal && std::fabs(fr.ny) <= lim.wallPairNy) {
					float hz = std::sqrt(fr.nx * fr.nx + fr.nz * fr.nz);
					if (hz > 1e-3f && -fr.nz / hz >= lim.wallMinFacing)
						hits.push_back({ fr.dist, h, -fr.nz / hz, fr, false });
				}
			}
			for (size_t i = 0; i < hits.size(); i++)
				for (size_t j = 0; j < hits.size(); j++)
					if (i != j && (hits[i].strong || hits[j].strong) && std::fabs(hits[i].d - hits[j].d) <= 30.0f &&
						hits[i].d <= hits[j].d && (!wall || z + hits[i].d < wallZ)) {
						wall = true;
						wallZ = z + hits[i].d;
						facing = hits[i].fc;
						wallRay = hits[i].r;
						wallH = hits[i].h;
						how = "para promieni";
					}
			// jedna zerdz (plot z jedna poprzeczka / druga miedzy promieniami): wierzch cienki, za nim ziemia nizej
			if (!wall)
				for (auto& h : hits) {
					if (!h.strong || h.h < 30.0f)
						continue;
					TravRay t1 = w.Down(0.0f, z + h.d + 4.0f, g + h.h + 30.0f, g + h.h - 30.0f);
					TravRay t2 = w.Down(0.0f, z + h.d + 35.0f, g + h.h + 30.0f, g - lim.marchDown);
					float y1 = g + h.h + 30.0f - t1.dist, y2 = g + h.h + 30.0f - t2.dist;
					if (t1.hit && (!t2.hit || y2 < y1 - 25.0f) && (!wall || z + h.d < wallZ)) {
						wall = true;
						wallZ = z + h.d;
						facing = h.fc;
						wallRay = h.r;
						wallH = h.h;
						how = "zerdz (wierzch i ziemia za nia)";
					}
				}
			if (wall)
				break;
			float zn = z + seg;
			w.Stage(TRAVST_MARCH_DOWN);
			TravRay d = w.Down(0.0f, zn, g + lim.marchUp, g - lim.marchDown);
			if (!d.hit && any.d >= 0.0f) {
				// promien ziemi zaczal sie w bryle (skarpa / skala o poszarpanej scianie - normalne poza pionem): bez ziemi
				// za trafieniem to sciana, nie uskok
				// (normalna tu niewiarygodna - kat sciany z promieni bocznych)
				wall = true;
				wallZ = z + any.d;
				facing = 1.0f;
				wallRay = any.r;
				wallRay.normal = false;
				wallH = any.h;
				how = "sciana bez ziemi za nia";
				break;
			}
			if (!d.hit) {
				stop = "uskok przed bohaterem";
				break;
			}
			float gn = g + lim.marchUp - d.dist;
			if (gn - g > lim.marchSpike) {
				// skok terenu w gore: wierzch zerdzi / slupka, gdy 25 cm dalej ziemia znow nizej - przeszkoda, nie teren
				w.Stage(TRAVST_SPIKE);
				TravRay d2 = w.Down(0.0f, zn + 25.0f, gn + 30.0f, g - lim.marchDown);
				float g2 = gn + 30.0f - d2.dist;
				if (!d2.hit || g2 < gn - 25.0f) {
					float best = -1.0f;
					TravRay bestRay;
					float bestH = 0.0f;
					for (float off : { 3.0f, 8.0f, 15.0f, 25.0f, 35.0f, 50.0f }) {
						float h = gn - g - off;
						if (h < 5.0f)
							continue;
						TravRay fr = w.Forward(0.0f, g + h, z, seg + 30.0f);
						if (fr.hit && (best < 0.0f || fr.dist < best)) {
							best = fr.dist;
							bestRay = fr;
							bestH = h;
						}
					}
					wall = true;
					wallZ = best >= 0.0f ? z + best : zn - 5.0f;
					facing = 1.0f;
					wallRay = best >= 0.0f ? bestRay : TravRay();
					if (best >= 0.0f && bestRay.normal) {
						float hz = std::sqrt(bestRay.nx * bestRay.nx + bestRay.nz * bestRay.nz);
						facing = hz > 1e-3f && -bestRay.nz / hz > 0.3f ? -bestRay.nz / hz : 1.0f;
					}
					wallH = bestH;
					how = "skok terenu (wierzch przeszkody)";
					Logf(log, " [skok %.0f:%+.0f, dalej %+.0f]", zn, gn, d2.hit ? g2 : -999.0f);
					break;
				}
			}
			g = gn;
			z = zn;
			r.groundZ.push_back(z);
			r.groundY.push_back(g);
		}
		Logf(log, "teren(z:y)");
		for (size_t i = 0; i < r.groundZ.size(); i++)
			Logf(log, " %.0f:%+.0f", r.groundZ[i], r.groundY[i]);
		if (!wall) {
			Logf(log, "; sciany brak do %.0f cm", z);
			return Fail(r, stop ? stop : "brak sciany przed bohaterem");
		}
		r.wallDist = wallZ;
		r.face = wallZ * facing;   // odleglosc prostopadla do sciany
		r.wallYaw = wallRay.normal ? std::atan2(-wallRay.nx, -wallRay.nz) : 0.0f;
		r.wallRayY = g + wallH;
		// 1b. szerokosc sciany: promienie na wysokosci sciany w bok (pien drzewa, slup - waskie, ocena po wysokosci); boki
		// dalej niz plaszczyzna z normalnej (oba) = obiekt okragly (snopek, beczka) - bez obrotu do normalnej
		w.Stage(TRAVST_WIDTH);
		{
			float z0 = std::fmax(0.0f, wallZ - 60.0f), len = wallZ - z0 + 60.0f, s = lim.widthSide;
			TravRay sl = w.Forward(-s, r.wallRayY, z0, len), sr = w.Forward(s, r.wallRayY, z0, len);
			if (sl.hit && sl.dist <= len && std::fabs(z0 + sl.dist - wallZ) <= lim.widthTol)
				r.sideL = z0 + sl.dist;
			if (sr.hit && sr.dist <= len && std::fabs(z0 + sr.dist - wallZ) <= lim.widthTol)
				r.sideR = z0 + sr.dist;
			if (r.sideL >= 0.0f && r.sideR >= 0.0f && !wallRay.normal) {
				// bez normalnej (vob bez poly, skarpa bez ziemi za nia): kat sciany z promieni bocznych, do 35 st.
				float yaw = std::atan2(r.sideL - r.sideR, 2.0f * s);
				yaw = std::fmax(-0.61f, std::fmin(0.61f, yaw));
				r.wallYaw = yaw;
				r.face = wallZ * std::cos(yaw);
			}
			else if (r.sideL >= 0.0f && r.sideR >= 0.0f) {
				float t = std::tan(r.wallYaw);
				float eL = wallZ + s * t, eR = wallZ - s * t;   // plaszczyzna przez trafienie srodkowe wg normalnej
				if (r.sideL - eL > lim.roundSag && r.sideR - eR > lim.roundSag) {
					r.round = true;
					r.wallYaw = 0.0f;
					r.face = wallZ;
				}
			}
		}
		// 2. podstawa sciany: ziemia tuz przed nia (stok, stopien)
		w.Stage(TRAVST_BASE);
		float base = g;
		TravRay b = w.Down(0.0f, std::fmax(0.0f, wallZ - 10.0f), g + lim.marchUp, g - lim.marchDown);
		if (b.hit)
			base = g + lim.marchUp - b.dist;
		r.base = base;
		if (wallZ - 10.0f > r.groundZ.back() + 1.0f) {
			r.groundZ.push_back(wallZ - 10.0f);
			r.groundY.push_back(base);
		}
		Logf(log, "; sciana %.0f cm (%s, prostopadle %.0f, kat %.0f st., promien %.0f nad ziemia, ny %.2f, boki %.0f / %.0f%s); "
			"podstawa %+.0f", wallZ, how, r.face, r.wallYaw * 57.2958f, wallH, wallRay.normal ? wallRay.ny : 0.0f, r.sideL, r.sideR,
			r.round ? ", okragla - bez obrotu" : "", base);
		if (r.face < lim.minFace)
			return Fail(r, "sciana za blisko");
		w.Stage(TRAVST_PROFILE);
		// 3. profil nad podstawa: promienie poziome z przodu sciany (nie z bohatera - stok); gora = poziom, nad ktorym
		// wolne jest co najmniej 50 cm (przerwa miedzy zerdziami plotu i pod nim nie jest gora)
		float zp = std::fmax(0.0f, wallZ - lim.profileBack);
		float reach = wallZ - zp + lim.topProbe;
		std::string prof;
		float hPrev = -1.0f, hFree = -1.0f, lastBlocked = -1.0f, candFree = -1.0f, candPrev = -1.0f, lastH = 0.0f;
		bool freeRun = false;
		std::vector<float> heights = ProfileHeights(lim);
		for (size_t i = 0; i < heights.size(); i++) {
			float h = heights[i];
			TravRay fr = w.Forward(0.0f, base + h, zp, reach);
			bool blocked = fr.hit && fr.dist <= reach;
			prof += blocked ? 'x' : '.';
			lastH = h;
			if (blocked) {
				lastBlocked = h;
				freeRun = false;
				continue;
			}
			if (lastBlocked < 0.0f)
				continue;
			if (!freeRun) {
				candFree = h;
				candPrev = lastBlocked;
				freeRun = true;
			}
			float step = i + 1 < heights.size() ? heights[i + 1] - h : lim.scanStep;
			if (h + step - candFree >= 50.0f) {
				hFree = candFree;
				hPrev = candPrev;
				break;
			}
		}
		if (hFree < 0.0f && freeRun) {   // koniec skanu wolny po zablokowanych
			hFree = candFree;
			hPrev = candPrev;
		}
		if (lastBlocked < 0.0f && wallH > 0.0f && std::string(how) != "para promieni") {
			// cienka zerdz miedzy promieniami profilu (znaleziona skokiem terenu / pojedynczym promieniem)
			hPrev = wallH + (g - base);
			hFree = hPrev + 10.0f;
			lastBlocked = hPrev;
		}
		(void)lastH;
		r.scanTop = lastBlocked;
		Logf(log, "; profil nad podstawa (do 110 co %.0f, wyzej co %.0f): %s", lim.faceStep, lim.scanStep, prof.c_str());
		if (hFree < 0.0f)
			return Fail(r, lastBlocked < 0.0f ? "nic na wysokosci sciany" : "za wysoko");
		// 4. gora: najblizsza plaska za sciana (z warga przed nia)
		w.Stage(TRAVST_TOP);
		float yFrom = base + hFree + 40.0f;
		TravTop top = FindTop(w, lim, wallZ, yFrom, base + hPrev - 30.0f);
		if (!top.ok) {
			Logf(log, "; gory brak (%s)", top.steep ? "stroma" : "nic");
			return Fail(r, top.steep ? "gora za stroma" : "brak gory przeszkody");
		}
		r.top = top.y;
		r.height = top.y - base;
		r.slope = top.slope;
		Logf(log, "; gora %+.0f (nad podstawa %.0f, %.0f cm za sciana%s)", r.top, r.height, top.z - wallZ,
			top.lips ? ", za warga" : top.slope ? ", pochyla - dach / stok" : "");
		if (r.height < lim.minHeight)
			return Fail(r, "za nisko");
		if (r.height > lim.maxHeight)
			return Fail(r, "za wysoko");
		r.bucket = TravBucket(r.height, lim);
		// waska: oba promienie boczne obok (jeden wystarczy - murek przy innej przeszkodzie, podejscie przy rogu)
		if (r.height > lim.widthMinHeight && r.sideL < 0.0f && r.sideR < 0.0f)
			return Fail(r, "waska przeszkoda (pien, slup)");
		float H = r.height, T = r.top;
		// niska przeszkoda: rownosc gory i spadek za nia wzgledem jej wysokosci (ziemia 25 cm nizej to juz spadek)
		float tol = std::fmin(lim.topTol, 0.4f * H);
		float drop = std::fmax(10.0f, std::fmin(30.0f, 0.5f * H));
		// 5. boki gory (szerokosc barkow) na dwoch glebokosciach - wystarczy para na ktorejkolwiek
		w.Stage(TRAVST_SIDES);
		int sides = 0;
		for (float dz : { 0.0f, 20.0f }) {
			int n = 0;
			for (float sx : { -lim.side, lim.side }) {
				TravRay s = w.Down(sx, top.z + dz, T + 30.0f, T - 60.0f);
				if (s.hit && std::fabs(T + 30.0f - s.dist - T) <= tol)
					n++;
			}
			sides = n > sides ? n : sides;
		}
		// 6. profil za sciana: glebokosc rownej gory i grubosc (pierwszy spadek); gora tam, gdzie bohater stanie
		w.Stage(TRAVST_DEPTH);
		float inset = top.z - wallZ;
		float depth = inset, thick = -1.0f;
		bool flat = true;
		r.topZ.push_back(top.z);
		r.topY.push_back(T);
		float prevS = 0.0f, prevY = T;
		for (float s : TRAV_DEPTH_STEPS) {
			TravRay p = w.Down(0.0f, top.z + s, T + 30.0f, T - 400.0f);
			float y = p.hit ? T + 30.0f - p.dist : -1e9f;
			bool on = p.hit && std::fabs(y - T) <= tol && (!p.normal || p.ny >= lim.topMinNy);
			if (flat) {
				// pochyla powierzchnia do chodzenia ciagla od krawedzi (dach, stok do ok. 45 st.): promien z nad niej
				// (pierwszy startowal pod dachem); stopien ponad nachylenie albo stromizna - koniec gory. Runda 12: takze
				// gdy pierwszy trafil plaska gore - wyzsza ciagla powierzchnia wygrywa (dach 45 st. nad gora skrzyn: pierwszy
				// promien przechodzil pod plyta dachu i bral skrzynie pod nia)
				float rise = (s - prevS) * lim.walkTan + 5.0f;
				float from = prevY + rise + 30.0f;
				TravRay q = w.Down(0.0f, top.z + s, from, prevY - rise - 5.0f);
				float qy = q.hit ? from - q.dist : -1e9f;
				if (q.hit && (!q.normal || q.ny >= lim.walkMinNy) && std::fabs(qy - prevY) <= rise && (!on || qy > y + 5.0f)) {
					on = true;
					y = qy;
					if (std::fabs(qy - T) > tol)
						r.slope = true;
				}
			}
			if (flat && on) {
				depth = inset + s;
				r.topZ.push_back(top.z + s);
				r.topY.push_back(y);
				prevS = s;
				prevY = y;
			}
			else
				flat = false;
			if (thick < 0.0f && y < T - drop)
				thick = inset + s;
		}
		if (flat)
			depth = TRAV_DEEP;
		r.depth = depth;
		// wolne za sciana na wysokosci ciala stojacego na gorze - pochyla powierzchnia do chodzenia (dach, stok) nie
		// zastawia, tylko pionowa / stroma przeszkoda
		w.Stage(TRAVST_CLEAR);
		float clear = 200.0f;
		for (float dy : { 40.0f, 110.0f, 170.0f }) {
			TravRay c = w.Forward(0.0f, T + dy, zp, wallZ - zp + 200.0f);
			if (c.hit && !(c.normal && c.ny >= lim.walkMinNy))
				clear = std::fmin(clear, zp + c.dist - wallZ);
		}
		r.clear = clear;
		w.Stage(TRAVST_VAULT);
		// 7. przeskok (oceniany zawsze - zapas, gdy wspiecie nie ma klipu); ladowanie od landMin pod podstawa do
		// vaultMinDrop pod gora (parapet tarasu - ziemia za nim wyzej niz przed nim)
		const char* vWhy = nullptr;
		if (thick < 0.0f || thick > lim.vaultMaxThick)
			vWhy = "gora za gruba na przeskok";
		else if (sides < 1)
			vWhy = "waska gora (boki)";
		else {
			TravRay land = w.Down(0.0f, wallZ + lim.vaultLandZ, T + 30.0f, base + lim.vaultLandMin - 60.0f);
			if (!land.hit)
				vWhy = "brak ziemi za przeszkoda";
			else {
				r.landY = T + 30.0f - land.dist;
				float rel = r.landY - base;
				if (land.normal && land.ny < 0.64f)
					vWhy = "stromo za przeszkoda";
				else if (rel < lim.vaultLandMin)
					vWhy = "za gleboko za przeszkoda";
				else if (r.landY > T - lim.vaultMinDrop)
					vWhy = "za wysoko za przeszkoda";
				else if (w.Up(0.0f, top.z, T + 5.0f, T + lim.vaultHeadroom))
					vWhy = "brak miejsca nad przeszkoda";
				else
					for (float dy : { 30.0f, 90.0f }) {
						float len = wallZ - zp + lim.vaultLandZ + 20.0f;
						TravRay c = w.Forward(0.0f, T + dy, zp, len);
						if (c.hit && c.dist <= len)
							vWhy = "sciana za przeszkoda";
					}
			}
		}
		r.thick = thick;
		r.vaultOk = vWhy == nullptr;
		r.vaultWhy = vWhy ? vWhy : "";
		// 8. wspiecie: platforma - glebokosc, boki (oba; jeden wystarczy przy glebokiej gorze - murek przy innej
		// przeszkodzie z boku), miejsce nad gora i za sciana
		w.Stage(TRAVST_HEADROOM);
		const char* cWhy = nullptr;
		// miejsce nad gora od jej powierzchni (dach: wyzej niz krawedz)
		float headY = r.slope ? std::fmax(T, TopYAt(r, top.z + 15.0f)) : T;
		if (depth < lim.climbMinDepth)
			cWhy = "gora za plytka na wspiecie";
		else if (sides < 2 && !(sides >= 1 && depth >= lim.climbOneSideDepth))
			cWhy = "waska gora (boki)";
		else if (w.Up(0.0f, top.z + 15.0f, headY + 10.0f, headY + lim.headroom))
			cWhy = "brak miejsca nad gora";
		else if (clear < lim.climbMinClear)
			cWhy = "sciana za krawedzia";
		char slopeTxt[64] = "";
		if (r.slope)
			snprintf(slopeTxt, sizeof(slopeTxt), " (pochyla, %+.0f cm na koncu profilu)", r.topY.back() - T);
		Logf(log, "; boki %d, glebokosc %s%.0f%s, wolne %.0f, grubosc %.0f, ziemia za %+.0f; wspiecie: %s; przeskok: %s", sides,
			flat ? ">=" : "", depth, slopeTxt, clear, thick, r.landY, cWhy ? cWhy : "tak", vWhy ? vWhy : "tak");
		if (!cWhy) {
			r.kind = TRAV_CLIMB;
			r.reason = "wspiecie";
			return r;
		}
		if (r.vaultOk) {
			r.kind = TRAV_VAULT;
			r.reason = "przeskok";
			return r;
		}
		return Fail(r, depth >= lim.climbMinDepth ? cWhy : vWhy);
	}

	static bool PickFits(const TravDef& d, const TravResult& r, TravKind kind) {
		return kind == TRAV_CLIMB
			? (r.depth >= TRAV_DEEP || r.depth >= d.depth - 20.0f) && (r.clear >= TRAV_DEEP || r.clear >= d.depth - 20.0f)
			: r.thick <= d.thick + 10.0f;
	}

	// z biegu: kubelek z wykrywania i noga; z miejsca (idle): klip o najblizszej wysokosci (sa tez 300 cm), bez nogi
	static const TravDef* PickKind(const std::vector<TravDef>& defs, const TravResult& r, TravKind kind, bool swingLeft,
		bool idle) {
		const TravDef* other = nullptr;
		for (auto& d : defs) {
			if (d.kind != kind || d.idle != idle || !PickFits(d, r, kind))
				continue;
			if (idle) {
				if (!other || std::fabs(d.height - r.height) < std::fabs(other->height - r.height))
					other = &d;
				continue;
			}
			if (d.bucket != r.bucket)
				continue;
			if (d.swingLeft == swingLeft)
				return &d;
			if (!other)
				other = &d;
		}
		return other;
	}

	const TravDef* TravPick(const std::vector<TravDef>& defs, TravResult& r, bool swingLeft, bool idle) {
		if (r.kind == TRAV_NONE)
			return nullptr;
		const TravDef* d = PickKind(defs, r, r.kind, swingLeft, idle);
		if (!d && r.kind == TRAV_CLIMB && r.vaultOk) {
			d = PickKind(defs, r, TRAV_VAULT, swingLeft, idle);
			if (d) {
				r.kind = TRAV_VAULT;
				r.reason = "przeskok (wspiecie bez klipu)";
			}
		}
		if (d && idle)
			r.bucket = d->bucket;   // z miejsca: klip o najblizszej wysokosci (np. 300)
		return d;
	}

	TravStart TravIdleStart(float face, const TravTiming& tm) {
		return face <= tm.idleMaxFace ? TRAVSTART_NOW : TRAVSTART_TOO_FAR;
	}

	void TravEndPoint(const TravDef& d, const TravResult& r, float& x, float& y, float& z) {
		TravFit f = TravFitFor(d, r);
		float sx, sy, sz;
		TravSample(d, f, d.cancel, sx, sy, sz);
		// tor idzie wzdluz kierunku w sciane (przod obrocony o wallYaw), prawo obrocone tak samo
		float c = std::cos(r.wallYaw), s = std::sin(r.wallYaw);
		x = sx * c + sz * s;
		z = -sx * s + sz * c;
		y = TravEndLevel(d, f);
	}

	bool TravEndSupported(TravWorld& w, const TravDef& d, const TravResult& r, const TravLimits& lim, float* hitY) {
		float x, y, z;
		TravEndPoint(d, r, x, y, z);
		w.Stage(TRAVST_END);
		TravRay g = w.Down(x, z, y + 60.0f, y - 120.0f);
		float gy = g.hit ? y + 60.0f - g.dist : -1e9f;
		if (hitY)
			*hitY = gy;
		return g.hit && std::fabs(gy - y) <= lim.endTol;
	}

	bool TravEndHeadroom(TravWorld& w, const TravDef& d, const TravResult& r, const TravLimits& lim) {
		float x, y, z;
		TravEndPoint(d, r, x, y, z);
		// stopy konca i przod ciala (25 cm dalej w kierunku ruchu): nic nad nimi do wysokosci stojacej postaci
		float fx = std::sin(r.wallYaw), fz = std::cos(r.wallYaw);
		for (float ahead : { 0.0f, 25.0f })
			if (w.Up(x + fx * ahead, z + fz * ahead, y + 10.0f, y + lim.headroom))
				return false;
		return true;
	}

	const TravDef* TravPickChecked(TravWorld& w, const std::vector<TravDef>& defs, TravResult& r, bool swingLeft,
		const TravLimits& lim, std::string* log, bool idle) {
		const TravDef* d = TravPick(defs, r, swingLeft, idle);
		if (!d)
			return nullptr;
		float gy = 0.0f, x, y, z;
		bool supported = TravEndSupported(w, *d, r, lim, &gy);
		if (supported && TravEndHeadroom(w, *d, r, lim))
			return d;
		TravEndPoint(*d, r, x, y, z);
		if (supported) {
			// runda 12 (nawis z miejsca): na koncu ruchu glowa w murze / nawisie - ta sama odmowa z biegu i z miejsca
			Logf(log, "; koniec %s (%.0f, %.0f) bez miejsca na postac (sciana / nawis nad koncem, %.0f cm)", d->id.c_str(), x, z,
				lim.headroom);
			r.kind = TRAV_NONE;
			r.reason = "brak miejsca na koncu ruchu (sciana / nawis nad gora)";
			return nullptr;
		}
		Logf(log, "; koniec %s (%.0f, %.0f) bez podparcia (ziemia %s, koniec %+.0f)", d->id.c_str(), x, z,
			gy > -1e8f ? std::to_string((int)gy).c_str() : "brak", y);
		if (gy > y + lim.endTol)
			r.endBlockY = gy;   // dalej wyzej (nastepna skrzynia) - skok gry by w to wlecial
		if (d->kind == TRAV_CLIMB && r.vaultOk) {
			TravResult v = r;
			v.kind = TRAV_VAULT;
			const TravDef* dv = PickKind(defs, v, TRAV_VAULT, swingLeft, idle);
			if (dv && TravEndSupported(w, *dv, v, lim)) {
				v.reason = "przeskok (wspiecie bez podparcia)";
				if (idle)
					v.bucket = dv->bucket;
				r = v;
				return dv;
			}
		}
		r.kind = TRAV_NONE;
		r.reason = "brak podparcia na koncu ruchu";
		return nullptr;
	}

	bool TravFoliageName(const std::string& name) {
		// bez tokenow tekstur ziemi (GRASS, LAUB, NADEL, MOOS - podloze lasu i omszale skaly to nie roslinnosc do pominiecia)
		static const char* tokens[] = { "TREE", "BAUM", "TANNE", "FICHTE", "KIEFER", "BIRKE", "EICHE", "BUCHE", "WEIDE",
			"BUSH", "BUESCH", "BUSCH", "STRAUCH", "SHRUB", "HECKE", "HEDGE", "IVY", "EFEU", "VINE", "LIANE", "RANKE", "LEAF",
			"LEAV", "BLATT", "BLAETTER", "ZWEIG", "BRANCH", "FARN", "FERN", "PFLANZE", "PLANT", "PALM", "SCHILF", "REED" };
		std::string u = Upper(name);
		for (const char* t : tokens)
			if (u.find(t) != std::string::npos)
				return true;
		return false;
	}

	TravStart TravPlanStart(float face, float edge, float speed, bool waiting, const TravTiming& tm) {
		if (edge <= 1.0f)
			return TRAVSTART_NOW;
		float k = face / edge;
		if (k < tm.kMin)
			return TRAVSTART_TOO_CLOSE;
		float go = waiting ? tm.kGo : tm.kMax;
		if (k <= go)
			return TRAVSTART_NOW;
		float v = speed > tm.minSpeed ? speed : tm.minSpeed;
		return (face - edge * tm.kGo) / v <= tm.maxWait ? TRAVSTART_WAIT : TRAVSTART_TOO_FAR;
	}

	bool TravHintDue(const TravHint& h, bool canStart, bool picked, TravStart st, float gap) {
		return h.left > 0 && h.since >= gap && canStart && picked && (st == TRAVSTART_NOW || st == TRAVSTART_WAIT);
	}

	float TravLookahead(float speed, float maxEdge, const TravTiming& tm) {
		float d = speed * tm.lookTime + maxEdge;
		return d < tm.lookMin ? tm.lookMin : (d > tm.lookMax ? tm.lookMax : d);
	}

	TravBufferStep TravBufferDecide(const TravBufferInput& in, const TravTiming& tm, const char*& why) {
		if (!in.forwardKey) {
			why = "puszczone W";
			return TRAVBUF_ABORT;
		}
		if (!in.running) {
			why = "zwolnil / nie biegnie";
			return TRAVBUF_ABORT;
		}
		if (std::fabs(in.turn) > tm.maxTurn) {
			why = "skret od kierunku wcisniecia";
			return TRAVBUF_ABORT;
		}
		if (!in.obstacleOk) {
			why = "przeszkoda nie pasuje";
			return TRAVBUF_ABORT;
		}
		if (in.plan == TRAVSTART_NOW) {
			why = "start";
			return TRAVBUF_START;
		}
		if (in.plan == TRAVSTART_TOO_CLOSE) {
			why = "minal okno startu";
			return TRAVBUF_ABORT;
		}
		if (in.time > tm.bufferMax) {
			why = "za dlugo";
			return TRAVBUF_ABORT;
		}
		if (in.plan == TRAVSTART_TOO_FAR) {
			why = "nie zdazy dobiec";
			return TRAVBUF_ABORT;
		}
		why = "czeka";
		return TRAVBUF_KEEP;
	}

	float TravGameJumpFeet(const TravGameJumpParams& p, float z) {
		if (z <= 0.0f || z >= p.reach || p.reach <= 1.0f)
			return 0.0f;
		float za = p.apexAt * p.reach;
		float u = z <= za ? (za - z) / za : (z - za) / (p.reach - za);
		return p.rise * (1.0f - u * u);
	}

	// Teren pod promieniem silnika (stromy stok w zasiegu 400 cm) - tez anuluje skok w biegu.
	static bool TravRayHitsTerrain(const TravResult& r, const TravGameJumpParams& p) {
		for (size_t i = 0; i < r.groundZ.size() && i < r.groundY.size(); i++)
			if (r.groundZ[i] > 0.0f && r.groundZ[i] <= p.rayLen && r.groundY[i] > p.rayY + p.rayUp * r.groundZ[i])
				return true;
		return false;
	}

	TravGameJumpInfo TravGameJumpVerdict(const TravResult& r, const TravGameJumpParams& p) {
		TravGameJumpInfo g;
		g.reach = p.reach + p.landExtra;
		bool terrain = TravRayHitsTerrain(r, p);
		if (r.wallDist <= 0.0f) {
			g.engineCancels = terrain;
			g.verdict = terrain ? TRAVJUMP_SUPPRESS : TRAVJUMP_PASS;
			g.why = terrain ? "silnik anuluje skok w biegu (promien trafia stok)" : "brak sciany";
			return g;
		}
		if (r.height > 0.0f)
			g.obstacle = r.top;
		else if (r.scanTop >= 0.0f) {
			g.obstacle = r.base + r.scanTop + 10.0f;
			g.tall = true;
		}
		else
			g.obstacle = r.base;   // nic na wysokosci sciany (niska / cienka przeszkoda)
		// za niska przeszkoda wyzsza (stos skrzyn - koniec ruchu bez podparcia, bo wyzej): skok gry wlecialby w nia
		bool blocked = r.endBlockY > -1e8f && r.endBlockY > g.obstacle;
		if (blocked)
			g.obstacle = r.endBlockY;
		g.rayAtWall = p.rayY + p.rayUp * r.wallDist;
		g.feetAtWall = TravGameJumpFeet(p, r.wallDist);
		g.engineCancels = terrain || (r.wallDist <= p.rayLen && g.obstacle > g.rayAtWall);
		// krawedz w zasiegu podciagania Gothica: skok z miejsca przy scianie (dalej - dobieg do niej, plugin)
		bool ledge = r.face <= p.ledgeRun && (p.ledgeMax <= 0.0f || g.obstacle <= p.ledgeMax);
		bool chasm = r.base < -p.chasmDrop;   // przepasc przed sciana - dobieg do niej wpadlby
		if (chasm)
			ledge = false;
		if (g.engineCancels) {
			g.verdict = ledge ? TRAVJUMP_LEDGE : TRAVJUMP_SUPPRESS;
			g.why = ledge ? (r.face <= p.ledgeFace ? "skok w biegu anuluje silnik, przy scianie skok z miejsca (zawis / wspiecie gry)"
				: "skok w biegu anuluje silnik - dobieg do sciany i skok z miejsca (zawis / wspiecie gry)")
				: "silnik anuluje skok w biegu (promien trafia sciane)";
			return g;
		}
		if (r.wallDist >= g.reach) {
			g.why = "ladowanie przed sciana";
			return g;
		}
		if (!g.tall && !blocked && g.obstacle <= p.stepMax) {
			g.why = "niska przeszkoda (postac na nia wejdzie)";
			return g;
		}
		if (!g.tall && g.feetAtWall >= g.obstacle + p.clearMargin) {
			g.why = "skok przejdzie nad przeszkoda";
			return g;
		}
		// runda 12: przepasc przed sciana (podstawa duzo nizej) - dobieg wpadlby; chwyt w locie dokonczy skok, gdy krawedz
		// w zasiegu dloni z toru skoku (inaczej "bez skoku" - tor testowy: platforma, przepasc, polka)
		bool grab = p.airGrab && !g.tall && !blocked && g.obstacle >= p.grabTopMin && g.obstacle <= p.grabTopMax;
		if (ledge) {
			g.verdict = TRAVJUMP_LEDGE;
			g.why = r.face <= p.ledgeFace ? "w biegu odbilby sie, przy scianie skok z miejsca (zawis / wspiecie gry)"
				: "w biegu odbilby sie - dobieg do sciany i skok z miejsca (zawis / wspiecie gry)";
			return g;
		}
		if (grab) {
			g.verdict = TRAVJUMP_PASS;
			g.why = chasm ? "przepasc przed sciana - chwyt w locie dokonczy skok (krawedz w zasiegu dloni)"
				: "chwyt w locie dokonczy skok (krawedz w zasiegu dloni)";
			return g;
		}
		g.verdict = TRAVJUMP_SUPPRESS;
		g.why = chasm ? "przepasc przed sciana - dobieg wpadlby, krawedz poza zasiegiem chwytu w locie"
			: "w locie uderzylby w sciane i odbil sie";
		return g;
	}

	bool TravJumpTeleport(float along, float t, const TravJumpGuardParams& p) {
		return t <= p.window && along < -p.maxBack;
	}

	bool TravRootGlitch(float tx, float ty, float tz, float dt, const TravRootGlitchParams& p) {
		float lim = p.base + p.speed * (dt > 0.0f ? dt : 0.0f);
		return std::sqrt(tx * tx + tz * tz) > lim || std::fabs(ty) > lim;
	}

	float TravFlightMaxStep(float maxSpeed, float dt, const TravFlightClampParams& p) {
		return maxSpeed * p.slack * (dt > 0.0f ? dt : 0.0f) + p.extra;
	}

	float TravFlightGuardStep(float along, float lastAlong, float airT, float maxStep, const TravJumpGuardParams& g,
		int& fixed) {
		fixed = 0;
		if (TravJumpTeleport(along, airT, g)) {
			fixed = 1;
			float s = lastAlong > 0.0f ? lastAlong : 0.0f;
			return s < maxStep ? s : maxStep;
		}
		if (along > maxStep) {
			fixed = 2;
			return maxStep;
		}
		return along;
	}

	bool TravAirGrabOk(float ledgeY, float headY, float feetY, const TravAirGrabParams& p) {
		return ledgeY - feetY >= p.minAboveFeet && ledgeY >= headY - p.headBelow && ledgeY <= headY + p.headAbove;
	}

	bool TravAirGrabNow(float shiftH, float ledgeVsHead, float speedToward, const TravAirGrabParams& p,
		const TravAirGrabTiming& t) {
		return shiftH <= t.maxShift || speedToward < t.approachSpeed || ledgeVsHead > p.headAbove - 15.0f;
	}

	float TravEaseOut(float u) {
		u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
		float a = 1.0f - u;
		return 1.0f - a * a * a;
	}

	float TravMomentumTarget(float v0, float animSpeed, float landedT, const TravMomentumParams& p) {
		if (v0 < animSpeed + p.minGain)
			return 0.0f;
		float v = v0 < p.maxSpeed ? v0 : p.maxSpeed;
		if (landedT >= 0.0f)
			v *= landedT >= p.landFade ? 0.0f : 1.0f - landedT / p.landFade;
		return v;
	}

	float TravMomentumExtra(float target, float engineStep, float dt) {
		float want = target * dt;
		float extra = want - (engineStep > 0.0f ? engineStep : 0.0f);
		return extra > 0.0f ? extra : 0.0f;
	}

	TravGameJumpParams TravGameJumpWithSpeed(const TravGameJumpParams& p, float v0, float animSpeed,
		const TravMomentumParams& mp) {
		TravGameJumpParams q = p;
		float v = TravMomentumTarget(v0, animSpeed, -1.0f, mp);
		if (v <= 0.0f || animSpeed <= 1.0f)
			return q;
		float k = v / animSpeed;
		q.reach *= k;
		q.landExtra *= k;
		return q;
	}

	std::string TravGameJumpLabel(const TravGameJumpInfo& g) {
		const char* head = g.verdict == TRAVJUMP_PASS ? "skok gry" : g.verdict == TRAVJUMP_LEDGE ? "skok z miejsca" : "bez skoku";
		char buf[320];
		if (g.obstacle < 0.0f)
			snprintf(buf, sizeof(buf), "%s: %s (zasieg skoku %.0f cm)", head, g.why, g.reach);
		else
			snprintf(buf, sizeof(buf), "%s: %s (krawedz %s%.0f cm, stopy w locie %.0f cm, promien silnika %.0f cm, zasieg %.0f cm)",
				head, g.why, g.tall ? "ponad " : "", g.obstacle, g.feetAtWall, g.rayAtWall, g.reach);
		return buf;
	}

	TravFit TravFitFor(const TravDef& d, const TravResult& r) {
		TravFit f;
		f.dz = r.face - d.edge;
		f.dH = r.height - d.height;
		f.base = r.base;
		f.landY = d.kind == TRAV_VAULT ? r.landY : 0.0f;
		f.groundZ = r.groundZ;
		f.groundY = r.groundY;
		// wspiecie: koniec toru na gorze tam, gdzie bohater stanie (warga nasypu nizej niz gora dalej, dach / stok wyzej
		// niz krawedz) - roznica do 90 cm (dach 45 st. przy koncu ok. 80 cm za krawedzia); wiecej - bez dopasowania
		// (podparcie konca odrzuci ruch)
		if (d.kind == TRAV_CLIMB && r.topZ.size() >= 2 && r.topZ.size() == r.topY.size() && d.traj.size() >= 3) {
			float zq = r.wallDist + d.traj[d.traj.size() - 1] - d.edge;
			float adj = TopYAt(r, zq) - r.top;
			f.endAdj = std::fabs(adj) <= 90.0f ? adj : 0.0f;
		}
		return f;
	}

	float TravGround(const TravFit& f, float z) {
		size_t n = f.groundZ.size() < f.groundY.size() ? f.groundZ.size() : f.groundY.size();
		if (n == 0)
			return f.base;
		if (z <= f.groundZ[0])
			return f.groundY[0];
		for (size_t i = 1; i < n; i++)
			if (z <= f.groundZ[i]) {
				float span = f.groundZ[i] - f.groundZ[i - 1];
				float u = span > 1e-3f ? (z - f.groundZ[i - 1]) / span : 1.0f;
				return f.groundY[i - 1] + (f.groundY[i] - f.groundY[i - 1]) * u;
			}
		return f.base;
	}

	void TravSample(const TravDef& d, const TravFit& f, float t, float& x, float& y, float& z) {
		SampleTraj3(d.traj, d.trajFps, t, x, y, z);
		auto shift = [&](float tt) { return f.dz * (d.contact > 0.0f ? TravSmooth01(tt / d.contact) : 1.0f); };
		z += shift(t);
		float up = d.contact > d.takeoff ? TravSmooth01((t - d.takeoff) / (d.contact - d.takeoff)) : (t >= d.contact ? 1.0f : 0.0f);
		float down = 0.0f;
		if (d.kind == TRAV_VAULT && d.land > d.release)
			down = TravSmooth01((t - d.release) / (d.land - d.release));
		// rozbieg po terenie do odbicia, potem do podstawy sciany razem z dopasowaniem wysokosci
		float tx, ty, tz;
		SampleTraj3(d.traj, d.trajFps, d.takeoff, tx, ty, tz);
		float gTake = TravGround(f, tz + shift(d.takeoff));
		float gRun = t <= d.takeoff ? TravGround(f, z) : gTake + (f.base - gTake) * up;
		y += gRun * (1.0f - down) + f.dH * up * (1.0f - down) + f.landY * down;
		if (f.endAdj != 0.0f) {
			float tEnd = d.cancel - 0.3f > d.contact + 0.05f ? d.cancel - 0.3f : d.contact + 0.05f;
			y += f.endAdj * TravSmooth01((t - d.contact) / (tEnd - d.contact));
		}
	}

	float TravEndLevel(const TravDef& d, const TravFit& f) {
		float x, y, z;
		TravSample(d, f, d.cancel + 1.0f, x, y, z);
		return y;
	}

	FallForgive FallForgiveDecide(float centerDrop, float stepHeight, const float ring[8], float ahead, float speed,
		float forgivenTime, const FallForgiveParams& p) {
		if (centerDrop <= stepHeight)
			return FALL_NONE;
		if (forgivenTime >= p.maxTime)
			return FALL_REAL;
		auto support = [&](float d) { return d <= p.maxDrop && d >= -p.maxRise; };
		int n = 0;
		for (int i = 0; i < 8; i++)
			n += support(ring[i]) ? 1 : 0;
		bool ok = speed >= p.minSpeed ? support(ahead) && n >= p.minRing : n >= p.minRingStill;
		return ok ? FALL_FORGIVE : FALL_REAL;
	}

	bool TravRunExitReady(const TravDef& d, const TravFit& f, float t, float tol) {
		if (t < d.exit)
			return false;
		float x, y, z;
		TravSample(d, f, t, x, y, z);
		return std::fabs(y - TravEndLevel(d, f)) <= tol;
	}

	std::string TravDescribe(const TravResult& r) {
		char buf[320], gora[48];
		if (r.height > 0.0f)
			snprintf(gora, sizeof(gora), "%.0f cm", r.height);
		else if (r.scanTop >= 0.0f)
			snprintf(gora, sizeof(gora), "ponad %.0f cm", r.scanTop);
		else
			snprintf(gora, sizeof(gora), "?");
		if (r.kind == TRAV_NONE)
			snprintf(buf, sizeof(buf), "brak (%s; sciana %.0f cm, podstawa %+.0f, gora %s)", r.reason, r.face, r.base, gora);
		else if (r.kind == TRAV_CLIMB)
			snprintf(buf, sizeof(buf), "wspiecie %d (gora %s nad podstawa %+.0f, sciana %.0f cm, kat %.0f st., glebokosc %s%.0f, wolne %.0f)",
				r.bucket, gora, r.base, r.face, r.wallYaw * 57.2958f, r.depth >= TRAV_DEEP ? ">=" : "", r.depth, r.clear);
		else
			snprintf(buf, sizeof(buf), "przeskok %d (gora %s nad podstawa %+.0f, sciana %.0f cm, kat %.0f st., grubosc <=%.0f, ziemia za %+.0f)",
				r.bucket, gora, r.base, r.face, r.wallYaw * 57.2958f, r.thick, r.landY);
		return buf;
	}

	TravSheatheAct TravSheatheDecide(const TravSheatheIn& in, const char*& why) {
		why = nullptr;
		if (in.weaponMode <= 0)
			return TRAVSH_NONE;
		if (!in.enabled) {
			why = "TraversalSheathe=0";
			return TRAVSH_KEEP;
		}
		if (in.weaponMode > 4) {
			why = "bron dystansowa / magia";
			return TRAVSH_KEEP;
		}
		if (in.leftHandBusy) {
			why = "lewa reka zajeta (pochodnia / tarcza / lewa bron pary)";
			return TRAVSH_KEEP;
		}
		if (in.weaponMsg) {
			why = "trwa dobycie / chowanie broni";
			return TRAVSH_KEEP;
		}
		return TRAVSH_SHEATHE;
	}

	int TravRedrawMode(bool enabled, int savedMode, int weaponModeNow, bool alive, bool saveOrLoad, bool weaponEquipped,
		const char*& why) {
		why = nullptr;
		if (savedMode <= 0 || savedMode > 4)
			return 0;
		if (!enabled)
			why = "TraversalRedraw=0";
		else if (!alive)
			why = "bohater nie zyje / nieprzytomny";
		else if (saveOrLoad)
			why = "zapis / wczytanie";
		else if (weaponModeNow != 0)
			why = "bron juz w rece";
		else if (savedMode >= 2 && !weaponEquipped)
			why = "bron zdjeta w trakcie ruchu";
		return why ? 0 : savedMode;
	}
}
