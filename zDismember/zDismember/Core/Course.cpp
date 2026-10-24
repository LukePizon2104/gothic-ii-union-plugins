#include "Course.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zdcore {

	const char* const COURSE_STONE = "TPL_EVT_NORMALSTONE_1X1M.3DS";
	const char* const COURSE_CRATE = "NW_HARBOUR_CRATE_01.3DS";
	const char* const COURSE_WALL = "NW_EVT_MONASTERYWALL_01.3DS";
	const char* const COURSE_PARAPET = "TPL_EVT_SECRETSTONE_01.3DS";
	const char* const COURSE_LOG = "NC_LOB_LOG1.3DS";
	const char* const COURSE_POST = "PC_LOB_BRIDGE_PLANK_MINI.3DS";
	const char* const COURSE_BARREL = "NW_HARBOUR_BARREL_01.3DS";
	const char* const COURSE_PLATE = "EVT_CORNERROOMS_LIFTGATEWALL_01.3DS";
	const char* const COURSE_PILLAR = "ADDON_MISC_WOODPILLAR_M02.3DS";
	const char* const COURSE_PILE = "NW_HARBOUR_CRATEPILE_01.3DS";

	const std::vector<std::string>& CourseVisuals() {
		static const std::vector<std::string> v = { COURSE_STONE, COURSE_CRATE, COURSE_WALL, COURSE_PARAPET, COURSE_LOG,
			COURSE_POST, COURSE_BARREL, COURSE_PLATE, COURSE_PILLAR, COURSE_PILE };
		return v;
	}

	std::vector<int> CourseStack(const std::vector<CoursePiece>& pieces, float target, int maxLayers) {
		std::vector<int> best;
		if (pieces.empty())
			return best;
		float bestCost = 1e9f;
		std::vector<int> counts(pieces.size(), 0);
		// wszystkie liczby klockow kazdego rodzaju, razem najwyzej maxLayers
		struct Rec {
			const std::vector<CoursePiece>& p;
			float target;
			int maxLayers;
			std::vector<int>& counts;
			float& bestCost;
			std::vector<int>& best;
			void Go(size_t i, int used, float h) {
				if (i == p.size()) {
					if (used == 0)
						return;
					float cost = std::fabs(h - target) + 3.0f * used;
					if (cost < bestCost - 1e-3f) {
						bestCost = cost;
						best.clear();
						for (size_t k = 0; k < counts.size(); k++)
							for (int n = 0; n < counts[k]; n++)
								best.push_back((int)k);
					}
					return;
				}
				for (int n = 0; used + n <= maxLayers; n++) {
					if (n > 0 && h + p[i].SizeY() * n > target + 60.0f)
						break;
					counts[i] = n;
					Go(i + 1, used + n, h + p[i].SizeY() * n);
				}
				counts[i] = 0;
			}
		} rec{ pieces, target, maxLayers, counts, bestCost, best };
		rec.Go(0, 0, 0.0f);
		// najwyzsze klocki na dole
		std::stable_sort(best.begin(), best.end(), [&](int a, int b) { return pieces[a].SizeY() > pieces[b].SizeY(); });
		return best;
	}

	namespace {
		struct Builder {
			const std::vector<CoursePiece>& cat;
			CoursePlan plan;

			const CoursePiece* Get(const char* v) const {
				for (auto& c : cat)
					if (c.visual == v)
						return &c;
				return nullptr;
			}

			// Klocek tak, by dol bboxa byl na y0, przod (minZ) na z0, srodek x na x0.
			static CoursePlace Put(const CoursePiece& p, float x0, float y0, float z0) {
				CoursePlace c;
				c.visual = p.visual;
				c.x = x0 - (p.minX + p.maxX) * 0.5f;
				c.y = y0 - p.minY;
				c.z = z0 - p.minZ;
				return c;
			}

			// Warstwa klockow pokrywajaca szerokosc w x glebokosc d (przod na z0), dol na y0; zwraca obrys (szer, gleb).
			static void Layer(CourseObstacle& o, const CoursePiece& p, float w, float d, float y0, float z0) {
				int nx = (int)std::ceil(w / p.SizeX() - 0.01f), nz = (int)std::ceil(d / p.SizeZ() - 0.01f);
				nx = nx < 1 ? 1 : nx;
				nz = nz < 1 ? 1 : nz;
				for (int ix = 0; ix < nx; ix++)
					for (int iz = 0; iz < nz; iz++)
						o.parts.push_back(Put(p, o.x + (ix - (nx - 1) * 0.5f) * p.SizeX(), y0, z0 + iz * p.SizeZ()));
				o.width = std::fmax(o.width, nx * p.SizeX());
				o.depth = std::fmax(o.depth, z0 + nz * p.SizeZ() - o.z);
			}

			void Skip(const std::string& name, const char* visual) {
				plan.skipped.push_back(name + " (brak modelu " + visual + ")");
			}

			// Poklad z plyt (plasko, gora na wysokosci top) od zFrom do zTo za przodem przeszkody - miejsce na gorze
			// (runda 12: po wspieciu w biegu bohater zbiegal z tylu stopnia 2 m i spadal); bez plyty - nic.
			void Deck(CourseObstacle& o, float top, float zFrom, float zTo) const {
				const CoursePiece* pl = Get(COURSE_PLATE);
				if (!pl)
					return;
				for (float z = zFrom; z < zTo - 1.0f; z += pl->SizeZ())
					o.parts.push_back(Put(*pl, o.x, top - pl->SizeY(), o.z + z));
				o.width = std::fmax(o.width, pl->SizeX());
				o.depth = std::fmax(o.depth, zTo);
			}

			// Stopien: stos warstw (plyta / skrzynia / mur) do wysokosci target, obrys >= stepWidth x stepDepth.
			bool Step(CourseObstacle& o, float target, float w, float d) {
				std::vector<CoursePiece> ps;
				for (const char* v : { COURSE_STONE, COURSE_CRATE, COURSE_WALL })
					if (const CoursePiece* p = Get(v))
						ps.push_back(*p);
				std::vector<int> st = CourseStack(ps, target);
				if (st.empty())
					return false;
				float y = 0.0f;
				for (int i : st) {
					Layer(o, ps[i], w, d, y, o.z);
					y += ps[i].SizeY();
				}
				o.height = y;
				return true;
			}
		};
	}

	CoursePlan CourseBuild(const std::vector<CoursePiece>& catalog, const CourseParams& prm) {
		Builder b{ catalog, CoursePlan() };
		// 4 rzedy: stopnie 30-150, stopnie 200-350, rodzaje przeszkod, dachy i skoki
		const float rowA = -1.5f * prm.rowSpacing, rowA2 = -0.5f * prm.rowSpacing, rowB = 0.5f * prm.rowSpacing,
			rowC = 1.5f * prm.rowSpacing;
		float zA = prm.start, zA2 = prm.start, zB = prm.start, zC = prm.start;
		auto add = [&](CourseObstacle& o, float& cursor) {
			b.plan.obstacles.push_back(o);
			cursor = o.z + o.depth + prm.gap;
		};
		auto make = [](const std::string& name, const std::string& expect, float x, float z) {
			CourseObstacle o;
			o.name = name;
			o.expect = expect;
			o.x = x;
			o.z = z;
			return o;
		};
		// rzedy A / A2: stopnie 30..150 i 200..350 - blok z przodu, za nim poklad do deckDepth
		for (int h : { 30, 60, 100, 150, 200, 250, 300, 350 }) {
			bool second = h >= 200;
			CourseObstacle o = make("stopien " + std::to_string(h), h <= 30 ? "wspiecie 30 / wejscie w biegu" : "wspiecie",
				second ? rowA2 : rowA, second ? zA2 : zA);
			if (b.Step(o, (float)h, prm.stepWidth, prm.stepDepth)) {
				// 24.10: gora bloku przykryta plytami 1 cm wyzej (szczeliny miedzy skrzyniami 2x2 - bohater utykal w szparze)
				float blockDepth = o.depth, top = o.height + (b.Get(COURSE_PLATE) ? 1.0f : 0.0f);
				b.Deck(o, top, 0.0f, blockDepth);
				b.Deck(o, top, blockDepth, prm.deckDepth);
				o.height = top;
				add(o, second ? zA2 : zA);
			}
			else
				b.Skip(o.name, COURSE_CRATE);
		}
		const CoursePiece* crate = b.Get(COURSE_CRATE);
		const CoursePiece* wall = b.Get(COURSE_WALL);
		const CoursePiece* plate = b.Get(COURSE_PLATE);
		// rzad B: rodzaje przeszkod
		if (const CoursePiece* p = b.Get(COURSE_PARAPET)) {
			CourseObstacle o = make("cienki murek " + std::to_string((int)std::lround(p->SizeY())), "przeskok (ziemia za nim)", rowB, zB);
			o.parts.push_back(Builder::Put(*p, o.x, 0.0f, o.z));
			o.height = p->SizeY();
			o.width = p->SizeX();
			o.depth = p->SizeZ();
			add(o, zB);
		}
		else
			b.Skip("cienki murek", COURSE_PARAPET);
		const CoursePiece* log = b.Get(COURSE_LOG);
		const CoursePiece* post = b.Get(COURSE_POST);
		if (log && post) {
			CourseObstacle o = make("plot z zerdzi", "przeskok (zerdzie 45 / 90 cm, slupki 105 cm)", rowB, zB);
			float d = std::fmax(log->SizeZ(), post->SizeZ());
			float half = log->SizeX() * 0.5f - post->SizeX() * 0.5f;
			for (float sx : { -half, half }) {
				CoursePlace c = Builder::Put(*post, o.x + sx, 0.0f, o.z + (d - post->SizeZ()) * 0.5f);
				c.y = 105.0f - post->maxY;   // slupek wbity w ziemie, gora 105 cm
				o.parts.push_back(c);
			}
			for (float cy : { 45.0f, 90.0f }) {
				CoursePlace c = Builder::Put(*log, o.x, 0.0f, o.z + (d - log->SizeZ()) * 0.5f);
				c.y = cy - (log->minY + log->maxY) * 0.5f;   // srodek zerdzi na cy
				o.parts.push_back(c);
			}
			o.height = 105.0f;
			o.width = log->SizeX();
			o.depth = d;
			add(o, zB);
		}
		else
			b.Skip("plot z zerdzi", log ? COURSE_POST : COURSE_LOG);
		if (const CoursePiece* p = b.Get(COURSE_BARREL)) {
			CourseObstacle o = make("beczka (okragla)", "przeskok / wspiecie bez obrotu do normalnej (obiekt okragly)", rowB, zB);
			o.parts.push_back(Builder::Put(*p, o.x, 0.0f, o.z));
			o.height = p->SizeY();
			o.width = p->SizeX();
			o.depth = p->SizeZ();
			add(o, zB);
		}
		else
			b.Skip("beczka (okragla)", COURSE_BARREL);
		if (const CoursePiece* p = b.Get(COURSE_PILLAR)) {
			CourseObstacle o = make("waski slup", "odmowa: waska przeszkoda (pien, slup) - skok gry", rowB, zB);
			o.parts.push_back(Builder::Put(*p, o.x, 0.0f, o.z));
			o.height = p->SizeY();
			o.width = p->SizeX();
			o.depth = p->SizeZ();
			add(o, zB);
		}
		else
			b.Skip("waski slup", COURSE_PILLAR);
		if (crate && wall) {
			// 24.10: dawniej mur 35 cm za krawedzia i 35 cm nad skrzyniami - na gorze 35 cm miejsca, wspiecie gry wchodzilo
			// i bohater spadal po zderzeniu z murem. Teraz podest 100 cm przed murem, mur wisi 35 cm nad podestem (szczelina
			// pod nawisem): koniec toru ma stanac przed murem, nie pod nim (sonda / miejsce na glowe).
			CourseObstacle o = make("nawis 35 cm nad podestem", "wspiecie: koniec przed murem (podest 100 cm, mur wisi 35 cm nad nim) - nie pod nawisem", rowB, zB);
			Builder::Layer(o, *crate, prm.stepWidth, prm.stepDepth, 0.0f, o.z);
			float top = crate->SizeY(), depth0 = o.depth;
			b.Deck(o, top + 1.0f, 0.0f, depth0);
			o.parts.push_back(Builder::Put(*wall, o.x, top + 35.0f, o.z + 100.0f));
			o.height = top;
			add(o, zB);
		}
		else
			b.Skip("nawis 35 cm nad podestem", crate ? COURSE_WALL : COURSE_CRATE);
		if (const CoursePiece* p = b.Get(COURSE_PILE)) {
			CourseObstacle o = make("stos skrzyn z portu", "nierowna gora (przypadek z nagrania 131330)", rowB, zB);
			o.parts.push_back(Builder::Put(*p, o.x, 0.0f, o.z));
			o.height = p->SizeY();
			o.width = p->SizeX();
			o.depth = p->SizeZ();
			add(o, zB);
		}
		else
			b.Skip("stos skrzyn z portu", COURSE_PILE);
		// rzad C: dachy i skoki
		for (float deg : { 30.0f, 45.0f }) {
			std::string name = deg < 40.0f ? "dach 30 st." : "dach 45 st.";
			if (!crate || !plate) {
				b.Skip(name, crate ? COURSE_PLATE : COURSE_CRATE);
				continue;
			}
			CourseObstacle o = make(name, "wspiecie na pochyly dach (koniec na wysokosci dachu)", rowC, zC);
			Builder::Layer(o, *crate, prm.stepWidth, crate->SizeZ(), 0.0f, o.z);
			Builder::Layer(o, *crate, prm.stepWidth, crate->SizeZ(), crate->SizeY(), o.z);
			float edge = 2.0f * crate->SizeY();
			float c = std::cos(deg * 3.14159265f / 180.0f), s = std::sin(deg * 3.14159265f / 180.0f);
			// plyta: lokalnie (x, y, z) -> tor (x, y c + z s, -y s + z c); gorna przednia krawedz (maxY, minZ) na (z0, edge)
			CoursePlace pl;
			pl.visual = plate->visual;
			pl.x = o.x - (plate->minX + plate->maxX) * 0.5f;
			pl.y = edge - (plate->maxY * c + plate->minZ * s);
			pl.z = o.z - (-plate->maxY * s + plate->minZ * c);
			pl.pitchDeg = deg;
			o.parts.push_back(pl);
			o.height = edge;
			o.width = std::fmax(o.width, plate->SizeX());
			o.depth = std::fmax(o.depth, plate->SizeZ() * c);
			add(o, zC);
		}
		if (crate) {
			// dwie platformy (blok skrzyn i poklad - po 6 m) z przerwa 250 cm
			CourseObstacle o = make("przepasc 250 cm", "wspiecie na platforme 100, bieg i skok w dal nad przerwa 250 cm", rowC, zC);
			float top = crate->SizeY(), second = prm.deckDepth + 250.0f;
			Builder::Layer(o, *crate, prm.stepWidth, prm.stepDepth, 0.0f, o.z);
			b.Deck(o, top + 1.0f, 0.0f, prm.stepDepth);   // 24.10: bez szczelin miedzy skrzyniami
			b.Deck(o, top + 1.0f, prm.stepDepth, prm.deckDepth);
			Builder::Layer(o, *crate, prm.stepWidth, prm.stepDepth, 0.0f, o.z + second);
			b.Deck(o, top + 1.0f, second, second + prm.stepDepth);
			b.Deck(o, top + 1.0f, second + prm.stepDepth, second + prm.deckDepth);
			o.height = top + (b.Get(COURSE_PLATE) ? 1.0f : 0.0f);
			add(o, zC);
		}
		else
			b.Skip("przepasc 250 cm", COURSE_CRATE);
		if (crate && wall) {
			// runda 12: rozbieg 8 m na platformie 100, przepasc 250, mur z krawedzia 200 nad platforma i poklad 4 m na
			// nim - skok gry z krawedzi platformy (promien silnika nad murem, nie anuluje) i chwyt w locie
			CourseObstacle o = make("chwyt w locie: polka 200 nad platforma",
				"rozbieg po platformie 100 (8 m), skok nad przepascia 250 i chwyt w locie krawedzi muru (200 nad platforma)", rowC, zC);
			float top = crate->SizeY(), runway = 800.0f, wallZ = runway + 250.0f;
			Builder::Layer(o, *crate, prm.stepWidth, prm.stepDepth, 0.0f, o.z);
			b.Deck(o, top + 1.0f, 0.0f, prm.stepDepth);   // 24.10: bez szczelin miedzy skrzyniami
			b.Deck(o, top + 1.0f, prm.stepDepth, runway);
			o.parts.push_back(Builder::Put(*wall, o.x, 0.0f, o.z + wallZ));
			b.Deck(o, wall->SizeY(), wallZ + wall->SizeZ(), wallZ + wall->SizeZ() + 400.0f);
			o.height = wall->SizeY();
			o.featureZ = wallZ;   // 24.10: napis i pomiar nad murem (krawedz 200 nad platforma)
			o.featureH = wall->SizeY();
			o.width = std::fmax(o.width, wall->SizeX());
			o.depth = std::fmax(o.depth, wallZ + wall->SizeZ());
			add(o, zC);
		}
		else
			b.Skip("chwyt w locie", crate ? COURSE_WALL : COURSE_CRATE);
		// pas toru
		bool first = true;
		for (auto& o : b.plan.obstacles) {
			float x0 = o.x - o.width * 0.5f, x1 = o.x + o.width * 0.5f;
			b.plan.minX = first ? x0 : std::fmin(b.plan.minX, x0);
			b.plan.maxX = first ? x1 : std::fmax(b.plan.maxX, x1);
			b.plan.length = std::fmax(b.plan.length, o.z + o.depth);
			first = false;
		}
		return b.plan;
	}

	std::string CourseDescribe(const CourseObstacle& o) {
		// modele i liczby klockow
		std::vector<std::pair<std::string, int>> n;
		for (auto& p : o.parts) {
			std::string v = p.visual;
			size_t dot = v.rfind('.');
			if (dot != std::string::npos)
				v = v.substr(0, dot);
			auto it = std::find_if(n.begin(), n.end(), [&](const std::pair<std::string, int>& e) { return e.first == v; });
			if (it == n.end())
				n.push_back({ v, 1 });
			else
				it->second++;
		}
		std::string models;
		for (auto& e : n)
			models += (models.empty() ? "" : ", ") + e.first + (e.second > 1 ? " x" + std::to_string(e.second) : "");
		char buf[200];
		snprintf(buf, sizeof(buf), "%s: %.1f m od startu, rzad %+.0f m, wys. %.0f cm, szer. %.0f, gleb. %.0f cm", o.name.c_str(),
			o.z / 100.0f, o.x / 100.0f, o.height, o.width, o.depth);
		return std::string(buf) + " (" + models + ") - " + o.expect;
	}
}
