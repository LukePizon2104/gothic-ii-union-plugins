// Supported with union (c) 2020 Union team
// Union SOURCE file

// Przeskoki i wspinanie w biegu (parkour W3). Dolaczany po Dodge.cpp (jeden TU przez Sources.h) - korzysta z jego
// pomocnikow ruchu: MvTrace (promienie), MvSetBlend / MvMotionLoops / MvActiveAni (przejscia
// animacji), MvRawState (klawisze bez podmian), heroSpeed, jumpKey, ms (trwajacy unik).
namespace GOTHIC_ENGINE {
	static std::vector<zdcore::TravDef> travDefs;
	static const zdcore::TravLimits TRAV_LIMITS;
	static const zdcore::TravClimbMaxParams TRAV_CLIMB_MAX;   // 24.10: wspiecie co najmniej jak gra z miejsca (+12%)
	static const zdcore::TravTiming TRAV_TIMING;
	// Promienie wykrywania i kontroli drogi: swiat i voby z kolizja, bez postaci (NPC to nie przeszkoda do wspinania).
	static const int TRAV_TRACE = MOVE_TRACE | zTRACERAY_VOB_IGNORE_CHARACTER;
	static const float TRAV_MIN_SPEED = 150.0f;      // cm/s - wolniej to nie bieg (zwykly skok gry)
	static const float TRAV_ENTRY_BLEND = 0.12f;     // wejscie z petli biegu / sprintu
	static const float TRAV_TURN_TIME = 0.15f;       // obrot przodem do sciany (skos do 35 st.)
	static const float TRAV_RUN_EXIT_BLEND = 0.3f;   // wyjscie w bieg (W trzymane od chwili exit)
	static const float TRAV_END_BLEND = 0.24f;       // koniec w postawe (= doklejone przejscie klipu, REC_FRAMES)
	static const float TRAV_GROUND_SNAP = 45.0f;     // koniec: ziemia najwyzej tyle od toru - vob dosuniety do niej
	static const float TRAV_UNSTICK = 32.0f;         // przerwanie: odsuniecie od sciany na tyle (promien ciala)
	static const int TRAV_MAX_RESUMES = 3;           // wznowienia animacji zgaszonej przez inna wtyczke / silnik
	static float travMaxEdge = 200.0f;               // najdalsza sciana klipow (zasieg szukania = predkosc * 1 s + tyle)

	// Trwajacy ruch: animacja w miejscu (korzen na stalej wysokosci) z nakladki HUMANS_ZDF_MV_TRAV_*, vob prowadzony
	// po torze 3D (TravSample) w kierunku sciany, bez kolizji ze swiatem i postaciami.
	struct TravState {
		const zdcore::TravDef* def = nullptr;
		zdcore::TravResult res;
		zdcore::TravFit fit;
		std::string overlay;          // z .MDS
		zCModelAni* ani = nullptr;    // wystartowana animacja (z naszej nakladki)
		int weaponMode = 0;
		float t = 0.0f;
		zVEC3 start, dir;             // start voba; kierunek w sciane (poziomy)
		float startGround = 0.0f;     // ziemia pod bohaterem na starcie
		float yOff = 0.0f;            // vob nad ziemia na starcie (srodek postaci)
		float yaw0 = 0.0f, yaw1 = 0.0f;   // obrot przodem do sciany w TRAV_TURN_TIME
		bool cdStat = true, cdDyn = true, cdOff = false;   // kolizja voba przed ruchem / wylaczona przez nas
		float ending = -1.0f;         // >= 0: konczenie (postawa albo bieg wchodzi) - tyle s do zdjecia nakladki
		bool runExit = false;         // konczenie w bieg (blokada zdjeta, silnik prowadzi voba w poziomie)
		float exitY0 = 0.0f, exitT = 0.0f;   // wyjscie w bieg: wysokosc voba na starcie przejscia, czas przejscia
		float progress = 0.0f;        // postep animacji z ostatniej klatki (wznowienie)
		int resumes = 0;
		int heroHp = 0;
		zVEC3 lastTarget;             // ostatnia pozycja toru ustawiona przez TravStep (diagnostyka runda 10)
		bool hasTarget = false;
		bool exitWaitLogged = false;  // runda 12: W trzymane, wyjscie w bieg czeka na wysokosc konca (log raz)
		int diagFrame = 0;
		bool cdOnLogged = false;      // 18.10: kolizja wlaczona w trakcie ruchu przez kogos (log raz)
		bool keysLogged = false;      // 18.10: klawisze ruchu w trakcie animacji ukryte przed gra (log raz)
		bool settled = false;         // 18.10: koniec przesuniety sonda kolizji - bez dociagania wysokosci w wyjsciu w bieg
		bool runExitBlocked = false;  // 19.10: wyjscie w bieg odrzucone (przeszkoda / sonda kroku) - koniec w postawie
		bool jumpPrev = true;         // 19.10: klawisz skoku w poprzedniej klatce ruchu (zbocze = Alt w trakcie)
		bool chainAlt = false;        // 19.10: Alt wcisniety w oknie wyjscia - swiadomy lancuch kolejnego ruchu
		int redrawMode = 0;           // 23.10: tryb broni schowanej od razu na starcie (TraversalSheathe) - powrot po koncu
	};
	static TravState ts;
	// 23.10: powrot broni do reki po ruchu - czeka na ziemie (bohater stoi / biegnie, bez fizyki lotu), najwyzej
	// TRAV_REDRAW_WAIT s; kolejny ruch w lancuchu przejmuje go (bron dalej schowana).
	static int travRedrawPending = 0;
	static float travRedrawWait = 0.0f;
	static const char* travRedrawFrom = "";
	static bool travFinishSaveLoad = false;       // TravFinish z TraversalFinishNow (zapis / wczytanie) - bez powrotu
	static const float TRAV_REDRAW_WAIT = 5.0f;

	// Bufor wcisniecia: sciana w zasiegu, bieg do niej, start gdy bohater wejdzie w okno startu klipu - rozstrzygane
	// w TraversalUpdate; przerwanie (puszczone W, zwolnienie, skret, za dlugo, przeszkoda nie pasuje) = skok gry teraz.
	struct TravPending {
		bool active = false;
		bool first = true;            // pierwsza klatka: start od razu do kMax, potem dopiero od kGo
		float time = 0.0f;
		float yaw0 = 0.0f;            // kierunek biegu przy wcisnieciu
		float face0 = 0.0f;           // sciana przy wcisnieciu (log)
		// szukanie w buforze tylko w okolicy znanej sciany: teren z pelnego szukania (wzgledem p0, wzdluz fwd0)
		zVEC3 p0, fwd0;
		std::vector<float> gz, gy;
		float wall0 = 0.0f;           // sciana z pelnego szukania (od p0)
		int fullProbes = 0, localProbes = 0;
	};
	static TravPending tp;
	static int travLatch = 0;         // klawisz skoku: 0 puszczony / nierozstrzygniety, 1 nasz (polykany), 2 dla gry
	// 19.10: lancuch ruchow - koniec poprzedniego (czas petli pluginu), Alt w jego oknie wyjscia, ostatnie nowe Alt
	static float travLastEndT = -1e9f, travAltT = -1e9f;
	static bool travChainAlt = false;
	static const float TRAV_CHAIN_MIN = 0.3f;    // s po koncu ruchu bez nowego Alt - bez kolejnego ruchu z trzymanego W
	static const zdcore::TravExitPathParams TRAV_EXIT_PATH;
	static float travClockNow();
	static bool TravChainNow(const char* what);

	// 18.10: powod naszego ustawienia pozycji / kolizji bohatera (log "Przesuniecie bohatera", klasyfikacja w hakach) -
	// ustawiany na czas wywolania (TravWhy), nullptr = nie nasza korekta.
	static const char* travOwnWhy = nullptr;           // powod naszego ustawienia pozycji (log "Przesuniecie bohatera")
	struct TravWhy {
		const char* prev;
		TravWhy(const char* w) : prev(travOwnWhy) { travOwnWhy = w; }
		~TravWhy() { travOwnWhy = prev; }
	};
	static std::string travCdOnBy;   // kto wlaczyl kolizje bohatera w trakcie naszego toru (SetCollDetStat / Dyn)
	static void TravTeleportHero(const zVEC3& to, const char* why);
	static void TravDiagBudgetReset();
	static int travSrcLogs = 0;      // wpisy "Przesuniecie bohatera" (limit na ruch - zerowany przy starcie przeskoku)

	static float TravLen(const zVEC3& v) {
		return sqrtf(v.n[0] * v.n[0] + v.n[1] * v.n[1] + v.n[2] * v.n[2]);
	}

	static const char* TravAniName(int wm) {
		wm = AniSwapAniWeaponMode(wm);   // zamiana animacji 1H <-> 2H
		return wm == NPC_WEAPON_2HS ? "T_2HPARADEJUMPB" : wm == NPC_WEAPON_1HS ? "T_1HPARADEJUMPB" :
			wm == NPC_WEAPON_FIST ? "T_FISTPARADEJUMPB" : "T_CANNOTTAKE";
	}

	static const char* TravStanceName(int wm) {
		return wm == NPC_WEAPON_NONE ? "S_RUN" : MoveStanceName(wm);
	}

	static int TravJumpKey() {
		return jumpKey > 0 ? jumpKey : 56;
	}

	// Ziemia pod vobem (srodek postaci): promien z tuz nad nim w dol - nie z wysoka jak MoveGroundAt, bo pod niskim
	// stropem (wnetrza, mosty) trafialby wierzch stropu.
	static bool TravGroundAt(const zVEC3& p, float& y) {
		zVEC3 from(p.n[0], p.n[1] + 20.0f, p.n[2]);
		if (!MvTrace(from, zVEC3(0.0f, -280.0f, 0.0f), player, nullptr, MOVE_GROUND_TRACE))
			return false;
		y = ogame->GetGameWorld()->traceRayReport.foundIntersection.n[1];
		return true;
	}

	static zVEC3 TravHeading() {
		zVEC3 at = player->GetAtVectorWorld();
		float l = MvFlatLen(at);
		return l > 0.01f ? zVEC3(at.n[0] / l, 0.0f, at.n[2] / l) : zVEC3(0.0f, 0.0f, 1.0f);
	}

	// Trafienie w roslinnosc: vob z wizualem o nazwie drzewa / krzaka / bluszczu (Core TravFoliageName) albo poly swiata
	// z tekstura z kanalem alfa o takiej nazwie (bluszcz, liscie na skale). Tekstury ziemi / omszalych skal - nie.
	// Opis trafienia do logu: wizual voba albo tekstura / material poly swiata (+ alfa).
	static std::string TravHitDesc(const zTTraceRayReport& rep) {
		std::string s;
		if (rep.foundVob) {
			zCVisual* v = rep.foundVob->GetVisual();
			s = std::string("vob ") + (v ? v->GetVisualName().ToChar() : "?");
		}
		zCMaterial* m = rep.foundPoly ? rep.foundPoly->material : nullptr;
		if (m)
			s += std::string(s.empty() ? "" : ", ") + "tekstura " + (m->texture ? m->texture->GetObjectName().ToChar() : "-") +
				" / material " + m->GetObjectName().ToChar() + (m->texture && m->texture->hasAlpha ? " (alfa)" : "");
		return s.empty() ? "?" : s;
	}

	static bool TravFoliageHit(const zTTraceRayReport& rep) {
		bool hit = false;
		if (rep.foundVob) {
			zCVisual* v = rep.foundVob->GetVisual();
			hit = v && zdcore::TravFoliageName(v->GetVisualName().ToChar());
		}
		zCMaterial* m = rep.foundPoly ? rep.foundPoly->material : nullptr;
		if (!hit && m && m->texture && m->texture->hasAlpha &&
			(zdcore::TravFoliageName(m->texture->GetObjectName().ToChar()) || zdcore::TravFoliageName(m->GetObjectName().ToChar())))
			hit = true;
		if (hit && zcfg.debug) {
			// raz na nazwe w sesji - co wykrywanie pomija jako roslinnosc
			static std::vector<std::string> seen;
			std::string d = TravHitDesc(rep);
			if (std::find(seen.begin(), seen.end(), d) == seen.end() && seen.size() < 200) {
				seen.push_back(d);
				ZDebug("Przeskok: roslinnosc pominieta w wykrywaniu: %s", d.c_str());
			}
		}
		return hit;
	}

	// ---------- Promienie wykrywania (uklad bohatera: ziemia pod nim y = 0, przod +z, prawo +x) ----------
	struct TravGameWorld : zdcore::TravWorld {
		zVEC3 o, fwd, right;
		zVEC3 P(float x, float y, float z) const {
			return zVEC3(o.n[0] + right.n[0] * x + fwd.n[0] * z, o.n[1] + y, o.n[2] + right.n[2] * x + fwd.n[2] * z);
		}
		zdcore::TravRay Ray(const zVEC3& from0, const zVEC3& ray0) {
			zdcore::TravRay r;
			// roslinnosc (drzewa, krzaki, bluszcz - nazwa wizualu voba albo tekstury z alfa) nie jest sciana ani gora:
			// promien idzie dalej za nia (najwyzej kilka razy)
			zVEC3 from = from0, ray = ray0;
			float skipped = 0.0f, total = TravLen(ray0);
			oCWorld* world = ogame->GetGameWorld();
			for (int pass = 0;; pass++) {
				if (!MvTrace(from, ray, player, nullptr, TRAV_TRACE))
					return r;
				if (pass >= 4 || !TravFoliageHit(world->traceRayReport))
					break;
				float d = TravLen(world->traceRayReport.foundIntersection - from) + 2.0f;
				float left = TravLen(ray) - d;
				if (left <= 1.0f || total < 1e-3f)
					return r;
				zVEC3 dir(ray0.n[0] / total, ray0.n[1] / total, ray0.n[2] / total);
				const zVEC3& hp = world->traceRayReport.foundIntersection;
				from = zVEC3(hp.n[0] + dir.n[0] * 2.0f, hp.n[1] + dir.n[1] * 2.0f, hp.n[2] + dir.n[2] * 2.0f);
				ray = zVEC3(dir.n[0] * left, dir.n[1] * left, dir.n[2] * left);
				skipped += d;
			}
			r.hit = true;
			r.dist = skipped + TravLen(world->traceRayReport.foundIntersection - from);
			const zVEC3& n = world->traceRayReport.foundPolyNormal;
			float nl = TravLen(n);
			if (nl > 0.5f && nl < 1.5f) {
				r.normal = true;
				r.nx = (n.n[0] * right.n[0] + n.n[2] * right.n[2]) / nl;
				r.ny = n.n[1] / nl;
				r.nz = (n.n[0] * fwd.n[0] + n.n[2] * fwd.n[2]) / nl;
			}
			return r;
		}
		zdcore::TravRay Cast(float x, float y, float z, float dx, float dy, float dz, float len) override {
			zVEC3 d(right.n[0] * dx + fwd.n[0] * dz, dy, right.n[2] * dx + fwd.n[2] * dz);
			return Ray(P(x, y, z), zVEC3(d.n[0] * len, d.n[1] * len, d.n[2] * len));
		}
	};

	static zdcore::TravGameJumpParams TravJumpParams(float groundY);
	static float travClimbLimLogged[3] = { -1.0f, -1.0f, -1.0f };

	// Wykrywanie przed bohaterem w kierunku fwd, sciana szukana do zasiegu wg predkosci (TravLookahead). log: opis
	// diagnostyczny (teren, sciana, profil, gora) albo nullptr.
	// rays: zapis promieni z etapami (debug wizualny), origin: ziemia pod bohaterem (poczatek ukladu wyniku).
	static zdcore::TravResult TravProbe(const zVEC3& fwd, std::string* log = nullptr, float range = -1.0f,
		float marchFrom = 0.0f, float marchFromY = 0.0f, std::vector<zdcore::TravRayRec>* rays = nullptr, zVEC3* origin = nullptr) {
		ZPROF_SCOPE("Traversal.Probe");   // ok. 25-30 promieni na jedno wykrywanie (w buforze Alt - co klatke)
		zdcore::TravResult r;
		zVEC3 p = player->GetPositionWorld();
		float g;
		if (!TravGroundAt(p, g)) {
			r.reason = "brak ziemi pod bohaterem";
			return r;
		}
		TravGameWorld w;
		w.o = zVEC3(p.n[0], g, p.n[2]);
		w.fwd = fwd;
		w.right = zVEC3(fwd.n[2], 0.0f, -fwd.n[0]);
		if (origin)
			*origin = w.o;
		zdcore::TravLimits lim = TRAV_LIMITS;
		// 24.10 (stopien 350 na torze, gora 361 cm: "za wysoko" z biegu, a z miejsca wspiecie gry siega 389-401 cm):
		// najwyzsza wspinaczka = wieksza z TravLimits i krawedzi skoku z miejsca gry (GetJumpUpHeight) z zapasem
		float native = TravJumpParams(g).ledgeMax;
		lim.maxHeight = zdcore::TravClimbMaxHeight(lim.maxHeight, native, TRAV_CLIMB_MAX);
		if (lim.scanMax < lim.maxHeight + 40.0f)
			lim.scanMax = lim.maxHeight + 40.0f;
		if (fabsf(lim.maxHeight - travClimbLimLogged[0]) > 5.0f || fabsf(native - travClimbLimLogged[1]) > 5.0f) {
			travClimbLimLogged[0] = lim.maxHeight;
			travClimbLimLogged[1] = native;
			ZDebug("Przeskok: limity wysokosci - wspiecie z biegu i z miejsca do %.0f cm (klipy: bieg do 250, z miejsca do 300 - "
				"wyzej tor rozciagniety w pionie), wspiecie gry z miejsca do %.0f cm (GetJumpUpHeight + wzrost), zapas x%.2f, profil do %.0f cm",
				lim.maxHeight, native, TRAV_CLIMB_MAX.margin, lim.scanMax);
		}
		lim.maxFace = range > 0.0f ? range : zdcore::TravLookahead(heroSpeed, travMaxEdge, TRAV_TIMING);
		lim.marchFrom = marchFrom;
		lim.marchFromY = marchFromY;
		if (rays) {
			zdcore::TravRecorder rec(w);
			r = zdcore::TravDetect(rec, lim, log);
			*rays = std::move(rec.rays);
			return r;
		}
		return zdcore::TravDetect(w, lim, log);
	}

	static float TravInterp(const std::vector<float>& zs, const std::vector<float>& ys, float z) {
		if (zs.empty() || zs.size() != ys.size())
			return 0.0f;
		if (z <= zs.front())
			return ys.front();
		for (size_t i = 1; i < zs.size(); i++)
			if (z <= zs[i]) {
				float span = zs[i] - zs[i - 1];
				return ys[i - 1] + (ys[i] - ys[i - 1]) * (span > 1e-3f ? (z - zs[i - 1]) / span : 1.0f);
			}
		return ys.back();
	}

	// Bufor: pelne szukanie zapamietuje teren i sciane; kolejne klatki szukaja od ok. 1 m przed oczekiwana sciana
	// (teren z zapamietanego profilu) - zamiast marszu od bohatera co klatke.
	static void TravRememberProbe(const zdcore::TravResult& r, const zVEC3& fwd) {
		if (r.wallDist <= 0.0f || r.groundZ.empty() || r.groundZ.front() > 0.5f)
			return;   // tylko pelne szukanie (teren od bohatera)
		tp.p0 = player->GetPositionWorld();
		tp.fwd0 = fwd;
		tp.gz = r.groundZ;
		tp.gy = r.groundY;
		tp.wall0 = r.wallDist;
	}

	static zdcore::TravResult TravProbeBuffered(const zVEC3& fwd, bool full) {
		if (!full && !tp.gz.empty()) {
			zVEC3 d = player->GetPositionWorld() - tp.p0;
			float traveled = d.n[0] * tp.fwd0.n[0] + d.n[2] * tp.fwd0.n[2];
			float z0 = tp.wall0 - traveled - 100.0f;
			if (z0 >= 100.0f) {
				tp.localProbes++;
				float y0 = TravInterp(tp.gz, tp.gy, traveled + z0) - TravInterp(tp.gz, tp.gy, traveled);
				return TravProbe(fwd, nullptr, -1.0f, z0, y0);
			}
		}
		tp.fullProbes++;
		zdcore::TravResult r = TravProbe(fwd);
		TravRememberProbe(r, fwd);
		return r;
	}

	// Noga w powietrzu teraz (nizsza stopa = podparta) - klip _LF / _RF.
	static bool TravSwingLeftNow() {
		zCModel* m = player->GetModel();
		int l = m ? FindNode(m, "BIP01 L FOOT") : -1, r = m ? FindNode(m, "BIP01 R FOOT") : -1;
		if (l < 0 || r < 0)
			return true;
		float ly = player->GetTrafoModelNodeToWorld(m->nodeList[l]).GetTranslation().n[1];
		float ry = player->GetTrafoModelNodeToWorld(m->nodeList[r]).GetTranslation().n[1];
		return zdcore::TravSwingLeft(ly, ry);
	}

	// Swiat promieni w ukladzie bohatera teraz (ziemia pod nim, kierunek fwd) - jak w TravProbe.
	static bool TravWorldHere(const zVEC3& fwd, TravGameWorld& w) {
		zVEC3 p = player->GetPositionWorld();
		float g;
		if (!TravGroundAt(p, g))
			return false;
		w.o = zVEC3(p.n[0], g, p.n[2]);
		w.fwd = fwd;
		w.right = zVEC3(fwd.n[2], 0.0f, -fwd.n[0]);
		return true;
	}

	// Debug: co lezy na linii sciany (pierwsze trafienie, takze roslinnosc) - nazwa wizualu / tekstury do logu.
	static std::string TravWallHitName(const zVEC3& fwd, const zdcore::TravResult& res) {
		TravGameWorld w;
		if (!zcfg.debug || res.wallDist <= 0.0f || !TravWorldHere(fwd, w))
			return "";
		float z0 = res.wallDist > 40.0f ? res.wallDist - 40.0f : 0.0f;
		zVEC3 from = w.P(0.0f, res.wallRayY, z0);
		if (!MvTrace(from, zVEC3(fwd.n[0] * 80.0f, 0.0f, fwd.n[2] * 80.0f), player, nullptr, TRAV_TRACE))
			return "";
		return TravHitDesc(ogame->GetGameWorld()->traceRayReport);
	}

	// Runda 9 (log 131330 - stos skrzyn w porcie nie trafiony): gdy wykrywanie nie znalazlo sciany, co trafiaja proste
	// promienie przed bohaterem (40 / 90 / 140 cm nad ziemia) - z filtrem wykrywania (TRAV_TRACE: voby bez cdDyn
	// pominiete przez silnik, roslinnosc pomijana przez nas) i bez filtra cdDyn (vob, flagi kolizji cdStat / cdDyn).
	static std::string TravRayAudit(const zVEC3& fwd, float range) {
		TravGameWorld w;
		if (!TravWorldHere(fwd, w))
			return "";
		oCWorld* world = ogame->GetGameWorld();
		std::string s;
		for (float h : { 40.0f, 90.0f, 140.0f }) {
			zVEC3 from = w.P(0.0f, h, 0.0f), ray(fwd.n[0] * range, 0.0f, fwd.n[2] * range);
			char buf[400];
			std::string a = "nic", b = "nic";
			if (MvTrace(from, ray, player, nullptr, TRAV_TRACE)) {
				const zTTraceRayReport& rep = world->traceRayReport;
				sprintf_s(buf, "%.0f cm %s%s", TravLen(rep.foundIntersection - from), TravHitDesc(rep).c_str(),
					TravFoliageHit(rep) ? " (roslinnosc - pomijana)" : "");
				a = buf;
			}
			if (MvTrace(from, ray, player, nullptr, TRAV_TRACE & ~zTRACERAY_VOB_IGNORE_NO_CD_DYN)) {
				const zTTraceRayReport& rep = world->traceRayReport;
				zCVob* v = rep.foundVob;
				sprintf_s(buf, "%.0f cm %s%s", TravLen(rep.foundIntersection - from), TravHitDesc(rep).c_str(),
					v ? (std::string(" (cdStat ") + (v->collDetectionStatic ? "1" : "0") + ", cdDyn " + (v->collDetectionDynamic ?
					"1" : "0") + ")").c_str() : "");
				b = buf;
			}
			sprintf_s(buf, "%s%.0f cm: %s", s.empty() ? "" : " | ", h, a.c_str());
			s += buf;
			if (b != a)
				s += " [bez filtra cdDyn: " + b + "]";
		}
		return s;
	}

	// Klip i podparcie w punkcie konca ruchu (Core TravPickChecked) - snopek: wspiecie konczace sie obok obiektu
	// odrzucone (przeskok albo brak). idle: klipy z miejsca. rays: zapis promienia podparcia (debug).
	static const zdcore::TravDef* TravPickHere(zdcore::TravResult& res, const zVEC3& fwd, bool idle, std::string* log = nullptr,
		std::vector<zdcore::TravRayRec>* rays = nullptr) {
		if (res.kind == zdcore::TRAV_NONE)
			return nullptr;
		TravGameWorld w;
		if (!TravWorldHere(fwd, w))
			return nullptr;
		bool left = TravSwingLeftNow();
		if (!rays)
			return zdcore::TravPickChecked(w, travDefs, res, left, TRAV_LIMITS, log, idle);
		zdcore::TravRecorder rec(w);
		const zdcore::TravDef* d = zdcore::TravPickChecked(rec, travDefs, res, left, TRAV_LIMITS, log, idle);
		rays->insert(rays->end(), rec.rays.begin(), rec.rays.end());
		return d;
	}

	// Bohater moze zaczac ruch. running: tez bieg do przodu (W bez S, stan biegu, predkosc); why = powod odmowy (log)
	// albo nullptr, gdy oczywisty (konsola, dialog, inny ruch).
	static bool TravHeroCanStart(bool running, int& wm, const char*& why) {
		why = nullptr;
		if (!zcfg.enabled || !zcfg.traversal || travDefs.empty() || !player || !ogame || !ogame->GetGameWorld() || !zinput)
			return false;
		if (player->IsDead() || player->IsUnconscious() || !player->GetModel())
			return false;
		if (DirectorActive() || ms.def || ts.def || player->IsMovLock() || (zcon && zcon->IsVisible()) || player->inventory2.IsOpen())
			return false;
		if (!oCInformationManager::GetInformationManager().HasFinished())
			return false;
		wm = player->GetWeaponMode();
		if (wm != NPC_WEAPON_NONE) {
			if (wm != NPC_WEAPON_FIST && wm != NPC_WEAPON_1HS && wm != NPC_WEAPON_2HS) {
				why = "bron dystansowa / magia";
				return false;
			}
			if (!zcfg.traversalCombat) {
				why = "w walce wylaczone (TraversalCombat=0)";
				return false;
			}
		}
		if (!HeroIsHuman() || HeroWeaponMessagePending() || HeroMounted())
			return false;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		if (!ac || ac->state != zCAIPlayer::zMV_STATE_STAND || ac->waterLevel > 0 || player->physicsEnabled) {
			why = "w powietrzu / w wodzie";
			return false;
		}
		if (!running)
			return true;
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		if (bs != BS_RUN && bs != BS_SPRINT && bs != BS_WALK) {
			why = "nie biegnie";
			return false;
		}
		if (!MvRawState(GAME_UP) || MvRawState(GAME_DOWN)) {
			why = "bez W";
			return false;
		}
		if (heroSpeed < TRAV_MIN_SPEED) {
			why = "za wolno";
			return false;
		}
		return true;
	}

	static const char* TravStartName(zdcore::TravStart s) {
		return s == zdcore::TRAVSTART_NOW ? "start" : s == zdcore::TRAVSTART_WAIT ? "czeka na odleglosc" :
			s == zdcore::TRAVSTART_TOO_FAR ? "za daleko" : "za blisko";
	}

	// Obiekt kolizji postaci bohatera (zCCollObjectCharacter - trwaly miedzy klatkami: podloga / sufit) albo nullptr.
	// Klasy postaci: zCCollObjectCharacter::s_oCollObjClass 0x8D835C, oCCollObjectCharacter 0xAAC26C (Names_G2A).
	static zCCollObjectCharacter* TravCharColl() {
		if (!player)
			return nullptr;
		zCCollisionObject* co = player->m_poCollisionObject;
		zCCollisionObjectDef* cls = player->m_poCollisionObjectClass;
		if (!co || (cls != (zCCollisionObjectDef*)0x008D835C && cls != (zCCollisionObjectDef*)0x00AAC26C))
			return nullptr;
		return static_cast<zCCollObjectCharacter*>(co);
	}

	// Runda 9: stan kolizji po naszym przeniesieniu bohatera jak po przeniesieniu silnika. Trafo kolizji (stary / nowy)
	// silnik i tak odswieza na poczatku kazdego ruchu (BeginMovement 0x61DA80; SetPositionWorld 0x61BB70 poza ruchem
	// sam robi Begin / EndMovement), ale stan przestrzenny postaci (podloga, sufit - m_oSpatialState) przy wylaczonej
	// kolizji nie jest liczony (CheckAndResolveCollisions 0x61F060 pomija voba bez cdStat / cdDyn) - po ruchu zostaje
	// podloga sprzed ruchu (np. ulica pod dachem). Stary = nowy = biezacy (ResetCollisionObjectState 0x61E690) i stan
	// przestrzenny od nowa (MarkSpatialStateAsUninited 0x551100: podloga / sufit w biezacej pozycji).
	static void TravSyncCollision() {
		zCCollObjectCharacter* cc = TravCharColl();
		if (!cc)
			return;
		player->ResetCollisionObjectState();
		cc->MarkSpatialStateAsUninited(TRUE);
	}

	// Log co klatke po koncu naszego ruchu (Debug=1, runda 9): y, przesuniecie, stan, fizyka, kolizja, podloga
	// kolizji, animacje - teleport po wspieciu (log 131357) ma byc widoczny klatka po klatce.
	struct TravPostWatch {
		float t = -1.0f;
		zVEC3 p0, prev;
		int frame = 0;
		std::string what;
	};
	static TravPostWatch tpw;
	static const float TRAV_POST_WATCH_TIME = 0.6f;
	static float travPostWin = 0.0f;   // runda 12: okno po koncu naszego ruchu - straznik skoku pozycji przywraca (s)

	static void TravPostWatchStart(const char* what) {
		travPostWin = TRAV_POST_WATCH_TIME;
		if (!zcfg.debug || !player)
			return;
		tpw = TravPostWatch();
		tpw.t = 0.0f;
		tpw.p0 = tpw.prev = player->GetPositionWorld();
		tpw.what = what;
	}

	// Kolizja voba (swiat i postacie) wraca do stanu sprzed ruchu - ze stanem kolizji z biezacej pozycji.
	static void TravRestoreCD() {
		if (!ts.cdOff || !player)
			return;
		TravWhy w("koniec ruchu: kolizja wraca");
		TravSyncCollision();
		player->SetCollDetStat(ts.cdStat ? TRUE : FALSE);
		player->SetCollDetDyn(ts.cdDyn ? TRUE : FALSE);
		ts.cdOff = false;
	}

	// Nasza animacja, dopoki nakladka ruchu jest na modelu.
	static zCModelAni* TravAni(zCModel* m) {
		return m && ts.ani && MvOverlayOn(m, ts.overlay) ? ts.ani : nullptr;
	}

	// Vob na ziemi pod nim, gdy blisko toru (koniec ruchu bez przeskoku na wysokosc gry).
	static void TravSnapToGround() {
		zVEC3 p = player->GetPositionWorld();
		float g;
		if (TravGroundAt(p, g) && fabsf(p.n[1] - ts.yOff - g) <= TRAV_GROUND_SNAP)
			player->SetPositionWorld(zVEC3(p.n[0], g + ts.yOff, p.n[2]));
	}

	// Przerwanie w powietrzu: vob odsuniety od sciany przed soba (promien ciala), dalej spada silnik.
	static void TravUnstick() {
		zVEC3 p = player->GetPositionWorld();
		zVEC3 ray(ts.dir.n[0] * TRAV_UNSTICK, 0.0f, ts.dir.n[2] * TRAV_UNSTICK);
		float worst = 0.0f;
		for (float dy : { -40.0f, 0.0f, 40.0f }) {
			zVEC3 from(p.n[0], p.n[1] + dy, p.n[2]);
			if (MvTrace(from, ray, player, nullptr, TRAV_TRACE)) {
				float d = MvFlatDist(from, ogame->GetGameWorld()->traceRayReport.foundIntersection);
				if (TRAV_UNSTICK - d > worst)
					worst = TRAV_UNSTICK - d;
			}
		}
		if (worst <= 0.5f)
			return;
		zVEC3 back(-ts.dir.n[0] * worst, 0.0f, -ts.dir.n[2] * worst);
		if (!MvTrace(p, back, player, nullptr, TRAV_TRACE)) {
			player->SetPositionWorld(p + back);
			ZDebug("Przeskok: odsuniety od sciany o %.0f cm", worst);
		}
	}

	// ---------- 18.10 (log NB 58087 / 58119, beczki NW_HARBOUR_BARRELGROUP_01 pod murem w porcie): koniec ruchu, ktorego
	// kolizja silnika nie przyjmuje. Wyjscie w bieg wlacza kolizje, a pierwszy ruch z kolizja ("zadane 0 cm, kolizja
	// zmienila o 225 cm") wypycha bohatera ok. 2,25 m do tylu na wysokosci gory - nad ulice, S_FALLDN. Nasz straznik
	// "skok pozycji po naszym ruchu" cofal to co klatke (po cichu - limit 20 wpisow na sesje), a "Teleport kolizji"
	// pomijal wyjscie w bieg (MovesActive() obejmuje TraversalActive()). Gdy wypchniecie spadlo ponizej 150 cm, straznik
	// odpuscil i bohater zostal w pozycji silnika (kl. 42: vob przod +138, bok +130). "BIP01 wzgl. voba" w kl. 31-41 to
	// nie ruch korzenia animacji: macierze wezlow sa z pozycji, w ktorej silnik postawil voba w tej klatce, a my
	// cofalismy voba dopiero w petli pluginu. Teraz: sonda kolizji (TravCollProbe) - przy starcie dla konca toru (koniec
	// niedostepny = ruch nie startuje, skok gry), przy wyjsciu w bieg i przy koncu w postawie dla biezacej pozycji z
	// kandydatami w gore / dalej (Core TravSettleOffsets); zaden nie pasuje - powrot na start toru zamiast spadania. ----------
	static const float TRAV_SETTLE_TOL = 20.0f;    // cm - sonda: kolizja przesunela najwyzej tyle = pozycja przyjeta
	static const zdcore::TravLoopBoxParams TRAV_LOOP_BOX;   // 24.10: ramka petli biegu przy wyjsciu w bieg
	static int travProbeDepth = 0;

	// Sonda: bohater w p (przeniesienie bez kolizji, stan kolizji od nowa), zerowy ruch z kolizja - o ile silnik go
	// przesunal. Kolizja bohatera musi byc wlaczona; po sondzie bohater stoi tam, gdzie postawil go silnik.
	static float TravCollProbe(const zVEC3& p) {
		travProbeDepth++;
		TravTeleportHero(p, "sonda kolizji (przeniesienie)");
		// 19.10 (log 65213.97: sonda 0 cm, nastepna klatka "kolizja zmienila o 259 cm"): pierwszy ruch po wyzerowaniu stanu
		// kolizji tylko liczy podloge - drugi ruch zerowy pokazuje, co zrobi silnik w kolejnej klatce
		float shift = 0.0f;
		for (int i = 0; i < 2; i++) {
			TravWhy w("sonda kolizji (ruch zerowy z kolizja)");
			player->SetPositionWorld(player->GetPositionWorld());
			float d = TravLen(player->GetPositionWorld() - p);
			shift = d > shift ? d : shift;
		}
		travProbeDepth--;
		return shift;
	}

	// 24.10: grunt pod obrysem postaci (srodek + 4 punkty na 25 cm) dla voba w p (stopy = p.y - yOff). drops jak w Core
	// TravFootingOk; desc: "srodek/obwod" do logu.
	static const zdcore::TravFootingParams TRAV_FOOTING;
	static const float TRAV_FOOT_RING = 25.0f;
	static bool TravFootingAt(const zVEC3& p, float yOff, const zVEC3& dir, std::string* desc = nullptr) {
		zVEC3 right(dir.n[2], 0.0f, -dir.n[0]);
		float feet = p.n[1] - yOff, drops[5];
		const float off[5][2] = { { 0, 0 }, { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int i = 0; i < 5; i++) {
			zVEC3 q(p.n[0] + (dir.n[0] * off[i][0] + right.n[0] * off[i][1]) * TRAV_FOOT_RING, feet + 25.0f,
				p.n[2] + (dir.n[2] * off[i][0] + right.n[2] * off[i][1]) * TRAV_FOOT_RING);
			float g;
			drops[i] = TravGroundAt(q, g) ? feet - g : 1e9f;
		}
		int ring = 0;
		bool ok = zdcore::TravFootingOk(drops, 5, TRAV_FOOTING, &ring);
		if (desc) {
			char b[96];
			sprintf_s(b, "srodek %s, obwod %d/4", drops[0] > 1e8f ? "bez ziemi" : (std::to_string((int)lroundf(drops[0])) + " cm").c_str(), ring);
			*desc = b;
		}
		return ok;
	}

	// Pozycja przyjeta przez kolizje silnika w p albo obok (kandydaci); out = wybrana. Bohater zostaje w out (ok) albo
	// w p (bez kolizji zmian - wolajacy decyduje). Log z przesunieciami sond.
	// yOff: vob nad stopami (24.10: kandydat bez pewnego gruntu pod obrysem - szpara miedzy skrzyniami - odrzucony, "szpara").
	static bool TravSettle(const zVEC3& p, const zVEC3& dir, bool withForward, zVEC3& out, std::string& desc, float yOff) {
		float off[zdcore::TRAV_SETTLE_MAX][3], shifts[zdcore::TRAV_SETTLE_MAX];
		int n = zdcore::TravSettleOffsets(dir.n[0], dir.n[2], withForward, off);
		int k = 0;
		char b[96];
		desc.clear();
		for (; k < n; k++) {
			zVEC3 c(p.n[0] + off[k][0], p.n[1] + off[k][1], p.n[2] + off[k][2]);
			std::string foot;
			if (!TravFootingAt(zVEC3(c.n[0], p.n[1], c.n[2]), yOff, dir, &foot)) {
				shifts[k] = 1e9f;
				sprintf_s(b, "%s%+.0f/%+.0f: szpara (%s)", k ? ", " : "", off[k][1], sqrtf(off[k][0] * off[k][0] + off[k][2] * off[k][2]),
					foot.c_str());
				desc += b;
				continue;
			}
			shifts[k] = TravCollProbe(c);
			sprintf_s(b, "%s%+.0f/%+.0f: %.0f", k ? ", " : "", off[k][1], sqrtf(off[k][0] * off[k][0] + off[k][2] * off[k][2]), shifts[k]);
			desc += b;
			if (zdcore::TravSettlePick(shifts, k + 1, TRAV_SETTLE_TOL) == k) {
				out = c;
				TravTeleportHero(c, "koniec ruchu: pozycja przyjeta przez kolizje");
				return true;
			}
		}
		TravTeleportHero(p, "koniec ruchu: sonda bez wyniku - powrot do pozycji");
		return false;
	}

	// Koniec ruchu (kolizja juz wlaczona): pozycja dla kolizji silnika; bez wyniku - start toru (tam bohater stal).
	static void TravSettleEnd(const char* when) {
		if (!zcfg.teleportGuard || !ts.def || !player->collDetectionStatic)
			return;
		zVEC3 p = player->GetPositionWorld(), out = p;
		std::string desc;
		if (TravSettle(p, ts.dir, true, out, desc, ts.yOff)) {
			ts.settled = TravLen(out - p) > 0.5f;
			if (TravLen(out - p) > 0.5f)
				ZDebug("Przeskok %s (%s): kolizja nie przyjmuje pozycji konca - przesuniety o %.0f cm (w gore %+.0f); sondy (gora/przod: "
					"przesuniecie) %s", ts.def->id.c_str(), when, TravLen(out - p), out.n[1] - p.n[1], desc.c_str());
			return;
		}
		ZDebug("Przeskok %s (%s): koniec ruchu niedostepny dla kolizji silnika (sondy gora/przod: przesuniecie %s) - powrot na "
			"start toru zamiast wypchniecia i spadania", ts.def->id.c_str(), when, desc.c_str());
		TravTeleportHero(ts.start, "koniec ruchu niedostepny - start toru");
		ts.settled = true;
	}

	// ---------- 23.10: bron na czas ruchu (W3) - TraversalSheathe / TraversalRedraw ----------
	// Dzis (przed 23.10) przeskok z bronia: Alt przejmowany tylko w biegu (z miejsca, chwyt w locie i dobieg do krawedzi
	// tylko bez broni), animacja T_1HPARADEJUMPB / T_2HPARADEJUMPB / T_FISTPARADEJUMPB z nakladki ruchu (bez broni
	// T_CANNOTTAKE), koniec w postawie broni; gdy klip nie pasuje - "bron w reku: Alt dla gry / unikow" (przewrot, skok
	// walki). Teraz z TraversalSheathe=1 bron chowana od razu na starcie (oCNpc::SetWeaponMode2(0) - to samo, co zdarzenie
	// DEF_FIGHTMODE "" w animacji chowania: bron z reki do slotu, animacje bez broni), ruch torem i klipem bez broni,
	// po wyladowaniu SetWeaponMode2(tryb) - bron od razu w rece (TraversalRedraw=1).
	static std::string TravLeftHandBusy() {
		for (const char* slot : { "ZS_LEFTHAND", "ZS_LEFTARM" })
			if (oCItem* it = player->GetSlotItem(zSTRING(slot)))
				return std::string(slot) + " " + it->GetInstanceName().ToChar();
		if (HeroDualState() == zdcore::DUAL_PAIR)
			return "para broni NB (lewa bron - chowa ja NB w swoich animacjach)";
		return "";
	}

	// Natychmiast schowana bron (zwraca tryb sprzed chowania; 0 = nic nie schowano / zostaje w rece).
	static int TravSheatheNow(const char* id) {
		int wm = player->GetWeaponMode();
		zdcore::TravSheatheIn in;
		in.enabled = zcfg.traversalSheathe;
		in.weaponMode = wm;
		std::string left = wm > 0 ? TravLeftHandBusy() : std::string();
		in.leftHandBusy = !left.empty();
		in.weaponMsg = HeroWeaponMessagePending();
		const char* why = nullptr;
		zdcore::TravSheatheAct act = zdcore::TravSheatheDecide(in, why);
		if (act == zdcore::TRAVSH_NONE)
			return 0;
		if (act == zdcore::TRAVSH_KEEP) {
			ZDebug("Przeskok %s: bron w rece (tryb %d) zostaje - %s%s%s", id, wm, why ? why : "?", left.empty() ? "" : ": ", left.c_str());
			return 0;
		}
		player->SetWeaponMode2(NPC_WEAPON_NONE);
		ResetWeaponGrip(player);
		if (player->GetWeaponMode() != NPC_WEAPON_NONE) {
			ZLog("Przeskok %s: chowanie broni nie wyszlo (tryb %d -> %d) - ruch z bronia jak dotad", id, wm, player->GetWeaponMode());
			return 0;
		}
		ZDebug("Przeskok %s: bron schowana od razu (tryb %d) - ruch bez broni, po wyladowaniu %s", id, wm,
			zcfg.traversalRedraw ? "z powrotem w rece" : "zostaje schowana (TraversalRedraw=0)");
		return wm;
	}

	// Powrot do reki teraz (bohater na ziemi). false = jeszcze nie (czekaj), true = zrobione albo porzucone.
	static bool TravRedrawStep(float dt) {
		if (!travRedrawPending)
			return true;
		if (travRedrawWait < 0.0f) {
			travRedrawWait += dt;   // zwloka po lancuchu (Alt w oknie wyjscia) - kolejny ruch przejmie powrot
			return false;
		}
		bool alive = player && player->homeWorld && !player->IsDead() && !player->IsUnconscious();
		oCAniCtrl_Human* ac = alive ? player->GetAnictrl() : nullptr;
		travRedrawWait += dt;
		bool grounded = ac && ac->state == zCAIPlayer::zMV_STATE_STAND && !player->physicsEnabled && ac->waterLevel == 0 &&
			!player->IsMovLock() && !HeroWeaponMessagePending() && !DirectorActive() && !ms.def;
		if (alive && !grounded && travRedrawWait < TRAV_REDRAW_WAIT && player->GetWeaponMode() == NPC_WEAPON_NONE)
			return false;
		int mode = travRedrawPending;
		travRedrawPending = 0;
		oCItem* w = alive ? player->GetEquippedMeleeWeapon() : nullptr;
		const char* why = nullptr;
		int m = zdcore::TravRedrawMode(zcfg.traversalRedraw, mode, alive ? player->GetWeaponMode() : 0, alive, false,
			mode == NPC_WEAPON_FIST || w != nullptr, why);
		if (!m && !why && !grounded)
			why = "bohater nie wyladowal";
		if (m && !grounded) {
			m = 0;
			why = "bohater nie stoi na ziemi po 5 s (woda / lot / blokada)";
		}
		if (!m) {
			ZDebug("Przeskok %s: bron (tryb %d) nie wraca do reki - %s", travRedrawFrom, mode, why ? why : "?");
			return true;
		}
		player->SetWeaponMode2(m);
		ResetWeaponGrip(player);
		zCModel* hm = player->GetModel();
		const char* stance = TravStanceName(player->GetWeaponMode());
		if (hm && ac && ac->IsStanding() && player->GetWeaponMode() == m && !MvActiveAni(hm, stance))
			hm->StartAni(stance, 0);
		ZDebug("Przeskok %s: bron z powrotem w rece od razu po %.2f s (tryb %d -> %d)", travRedrawFrom, travRedrawWait, m,
			player->GetWeaponMode());
		return true;
	}

	// interrupted: przerwany (trafienie, przeszkoda po drodze, zapis) - bez wymuszania postawy i dosuwania do ziemi
	// (w powietrzu spada silnik). Nakladka schodzi z migawka animacji (postawa / bieg / reakcja gry graja dalej).
	static void TravFinish(bool interrupted, const char* why) {
		zCModel* hm = player ? player->GetModel() : nullptr;
		const char* id = ts.def ? ts.def->id.c_str() : "?";
		if (hm) {
			zCModelAni* ani = TravAni(hm);
			if (ani && hm->IsAniActive(ani))
				hm->StopAni(ani);
			if (!ts.overlay.empty() && MvOverlayOn(hm, ts.overlay))
				ZRemoveOverlayKeepAnis(hm, ts.overlay);   // zdjecie (nawet cudzej nazwy) zatrzymuje w silniku wszystko
			const char* stance = TravStanceName(ts.weaponMode);
			if (!interrupted && !ts.runExit && player->GetWeaponMode() == ts.weaponMode && !MvActiveAni(hm, stance))
				hm->StartAni(stance, 0);
		}
		if (player) {
			if (!ts.runExit) {
				if (interrupted)
					TravUnstick();
				else
					TravSnapToGround();
			}
			TravRestoreCD();
			if (!interrupted && !ts.runExit)
				TravSettleEnd("koniec w postawie");
			player->SetMovLock(FALSE);
			ResetWeaponGrip(player);
		}
		if (interrupted)
			ZDebug("Przeskok %s: przerwany po %.2f s (%s)", id, ts.t, why ? why : "?");
		else
			ZDebug("Przeskok %s: koniec po %.2f s (%s)", id, ts.t, ts.runExit ? "w biegu" : "postawa");
		TravPostWatchStart((std::string(id) + (interrupted ? " przerwany" : " koniec")).c_str());
		travLastEndT = travClockNow();
		travChainAlt = ts.chainAlt;
		int redraw = ts.redrawMode;
		ts = TravState();
		if (redraw) {
			if (travFinishSaveLoad)
				ZDebug("Przeskok %s: bron (tryb %d) zostaje schowana - zapis / wczytanie w trakcie ruchu", id, redraw);
			else {
				// po wyladowaniu (TravRedrawStep w petli) - zaraz albo po skonczonym spadaniu; Alt w oknie wyjscia (lancuch
				// kolejnego ruchu) - chwila zwloki, zeby bron nie mignela w rece miedzy ruchami
				travRedrawPending = redraw;
				travRedrawWait = travChainAlt ? -0.25f : 0.0f;
				travRedrawFrom = id;
				if (!travChainAlt)
					TravRedrawStep(0.0f);
			}
		}
	}

	// Start ruchu: nakladka, animacja dla broni w reku (wejscie z petli biegu), blokada sterowania, kolizja wylaczona,
	// tor dopasowany do przeszkody (sciana, wysokosc, ziemia za nia), kierunek w sciane.
	static bool TravBegin(const zdcore::TravDef* def, const zdcore::TravResult& res, const zVEC3& fwd) {
		zCModel* hm = player->GetModel();
		zVEC3 p = player->GetPositionWorld();
		float g;
		if (!TravGroundAt(p, g)) {
			ZDebug("Przeskok %s: brak ziemi pod bohaterem - skok gry", def->id.c_str());
			return false;
		}
		if (TravChainNow(def->id.c_str()))
			return false;
		// 18.10: koniec toru musi byc przyjety przez kolizje silnika (sonda w punkcie konca i 15-45 cm wyzej) - inaczej
		// wyjscie w bieg / postawa wypchnelyby bohatera z gory (beczki w porcie: 2,25 m do tylu i spadek)
		if (zcfg.teleportGuard && player->collDetectionStatic && !player->physicsEnabled) {
			zdcore::TravFit fit = zdcore::TravFitFor(*def, res);
			float c = cosf(res.wallYaw), s = sinf(res.wallYaw);
			zVEC3 right0(fwd.n[2], 0.0f, -fwd.n[0]);
			zVEC3 dir(fwd.n[0] * c + right0.n[0] * s, 0.0f, fwd.n[2] * c + right0.n[2] * s);
			zVEC3 right(dir.n[2], 0.0f, -dir.n[0]);
			float x, y, z;
			zdcore::TravSample(*def, fit, def->cancel, x, y, z);
			zVEC3 end(p.n[0] + right.n[0] * x + dir.n[0] * z, g + (p.n[1] - g) + y, p.n[2] + right.n[2] * x + dir.n[2] * z);
			zVEC3 out = end;
			std::string desc;
			bool ok = TravSettle(end, dir, false, out, desc, p.n[1] - g);
			TravTeleportHero(p, "sonda konca toru - powrot na start");
			if (!ok) {
				ZDebug("Przeskok %s: koniec ruchu (%.0f, %.0f, %.0f) niedostepny dla kolizji silnika (sondy gora/przod: przesuniecie %s) "
					"- bez ruchu", def->id.c_str(), end.n[0], end.n[1], end.n[2], desc.c_str());
				return false;
			}
			if (TravLen(out - end) > 0.5f)
				ZDebug("Przeskok %s: koniec ruchu przyjety przez kolizje %+.0f cm wyzej niz tor (sondy %s)", def->id.c_str(),
					out.n[1] - end.n[1], desc.c_str());
		}
		TravDiagBudgetReset();   // diagnostyka na kazdy ruch od nowa (limity na sesje ukrywaly wpisy przy wyjsciu w bieg)
		// 23.10 (W3): bron chowana od razu - ruch bez broni; kolejny ruch w lancuchu przejmuje czekajacy powrot do reki
		int redraw = TravSheatheNow(def->id.c_str());
		if (!redraw && travRedrawPending && player->GetWeaponMode() == NPC_WEAPON_NONE) {
			redraw = travRedrawPending;
			travRedrawPending = 0;
		}
		auto failRedraw = [&]() {
			if (redraw) {
				travRedrawPending = redraw;
				travRedrawWait = 0.0f;
				travRedrawFrom = def->id.c_str();
				TravRedrawStep(0.0f);
			}
		};
		int wm = player->GetWeaponMode();
		std::string overlay = def->overlay + ".MDS";
		if (!ZApplyOverlayKeepAnis(hm, overlay)) {
			ZLog("Przeskok: nie podpieto nakladki %s", overlay.c_str());
			failRedraw();
			return false;
		}
		const char* aniName = TravAniName(wm);
		int id = hm->GetAniIDFromAniName(aniName);
		if (id < 0) {
			ZRemoveOverlayKeepAnis(hm, overlay);
			ZLog("Przeskok: brak animacji %s w %s", aniName, overlay.c_str());
			failRedraw();
			return false;
		}
		if (zCEventManager* em = player->GetEM(FALSE))
			em->Clear();
		std::vector<zCModelAni*> loops = MvMotionLoops(hm);
		hm->StartAni(id, 0);
		player->SetMovLock(TRUE);
		ts = TravState();
		ts.def = def;
		ts.res = res;
		ts.fit = zdcore::TravFitFor(*def, res);
		ts.overlay = overlay;
		ts.ani = hm->GetAniFromAniID(id);
		ts.weaponMode = wm;
		ts.redrawMode = redraw;
		ts.heroHp = player->attribute[NPC_ATR_HITPOINTS];
		if (!loops.empty()) {
			// petla biegu / sprintu wygasa pod ruchem (hak _Stand pomijany w trakcie ruchu - MvStand)
			MvSetBlend(hm, ts.ani ? hm->GetActiveAni(ts.ani) : nullptr, nullptr, TRAV_ENTRY_BLEND);
			for (zCModelAni* a : loops)
				if (zCModelAniActive* la = hm->GetActiveAni(a))
					MvSetBlend(hm, nullptr, la, TRAV_ENTRY_BLEND);
		}
		// bez kolizji: vob przechodzi tuz nad krawedzia i przez sciane ciala (droge sprawdza promien w TravStep)
		ts.cdStat = player->collDetectionStatic != 0;
		ts.cdDyn = player->collDetectionDynamic != 0;
		player->SetCollDetStat(FALSE);
		player->SetCollDetDyn(FALSE);
		ts.cdOff = true;
		travCdOnBy.clear();
		ts.start = p;
		ts.startGround = g;
		ts.yOff = p.n[1] - g;
		float c = cosf(res.wallYaw), s = sinf(res.wallYaw);
		zVEC3 right(fwd.n[2], 0.0f, -fwd.n[0]);
		ts.dir = zVEC3(fwd.n[0] * c + right.n[0] * s, 0.0f, fwd.n[2] * c + right.n[2] * s);
		zVEC3 at = TravHeading();
		ts.yaw0 = atan2f(at.n[0], at.n[2]);
		ts.yaw1 = atan2f(ts.dir.n[0], ts.dir.n[2]);
		ZDebug("Przeskok %s (%s, bron %d%s): %s; tor: sciana %+.0f cm, gora %+.0f cm (klip %.0f), podstawa %+.0f, ziemia za %+.0f, "
			"teren %d probek; vob %.0f nad ziemia, predkosc %.0f", def->id.c_str(), aniName, wm, redraw ? (", schowana z trybu " +
			std::to_string(redraw)).c_str() : "", zdcore::TravDescribe(res).c_str(), ts.fit.dz, ts.fit.dH, def->height, ts.fit.base,
			ts.fit.landY, (int)ts.fit.groundZ.size(), ts.yOff, heroSpeed);
		return true;
	}

	// Konczenie w postawe: animacja ruchu wygasa (doklejone przejscie klipu celuje w postawe Gothica), postawa broni
	// w reku wchodzi przez TRAV_END_BLEND.
	static void TravBeginEnding() {
		zCModel* hm = player->GetModel();
		zCModelAni* ani = TravAni(hm);
		zCModelAniActive* out = ani ? hm->GetActiveAni(ani) : nullptr;
		if (out)
			out->blendOutOverride = -1.0f / TRAV_END_BLEND;
		if (player->GetWeaponMode() == ts.weaponMode) {
			const char* stance = TravStanceName(ts.weaponMode);
			hm->StartAni(stance, 0);
			MvSetBlend(hm, MvActiveAni(hm, stance), out ? hm->GetActiveAni(ani) : nullptr, TRAV_END_BLEND);
		}
		ts.ending = TRAV_END_BLEND;
	}

	// Z W trzymanym od chwili exit prosto w bieg: petla biegu (najwyzsza nakladka: trucht W3 / Gothic / sprint)
	// startuje pod wygasajaca animacja, blokada i kolizja wracaja od razu (silnik prowadzi voba), wysokosc voba
	// dociagana do ziemi w czasie przejscia.
	// 19.10: czy wolno wyjsc w bieg (W trzymane od exit). Log NB: wyjscie w bieg 8/27 z wypchnieciem kolizji o 1,3-2,6 m
	// w pierwszej klatce biegu, koniec w postawie 0/14. (1) droga pierwszych krokow: promienie z pozycji voba w kierunku
	// ruchu na 30 / 90 / 150 cm nad stopami, need cm (kolejna krawedz / mur za koncem = przeszkoda); (2) sonda z kolizja:
	// stopy na gorze, dwa ruchy zerowe i krok stepCm do przodu - silnik musi zostawic bohatera przy zadanym miejscu.
	// Niepowodzenie: tor dogrywa do konca klipu z wylaczona kolizja i koniec w postawie (TravFinish: sonda konca).
	static bool TravRunExitAllowed(std::string& why) {
		zVEC3 p = player->GetPositionWorld();
		float feet = p.n[1] - ts.yOff;
		float hits[3];
		int k = 0;
		for (float h : { 30.0f, 90.0f, 150.0f }) {
			zVEC3 from(p.n[0], feet + h, p.n[2]);
			float len = TRAV_EXIT_PATH.need;
			hits[k] = MvTrace(from, zVEC3(ts.dir.n[0] * len, 0.0f, ts.dir.n[2] * len), player, nullptr, TRAV_TRACE) ?
				MvFlatDist(from, ogame->GetGameWorld()->traceRayReport.foundIntersection) : -1.0f;
			k++;
		}
		float bd = zdcore::TravRunExitBlockDist(hits, k, TRAV_EXIT_PATH);
		if (bd >= 0.0f) {
			char b[160];
			sprintf_s(b, "przeszkoda %.0f cm przed bohaterem (promienie 30/90/150 cm nad stopami: %.0f / %.0f / %.0f, -1 = wolne)", bd,
				hits[0], hits[1], hits[2]);
			why = b;
			return false;
		}
		// 24.10: petla biegu z ramka animacji siegajaca metry przed postac (stare W3 truchtu / sprintu: 310 / 466 cm z droga
		// cyklu) - kolizja liczona od nowa po wlaczeniu wypycha do tylu o (przod ramki - wolna droga): log NB 49176-49388
		// "zadane 0 cm, kolizja zmienila o 326 cm" przy kazdym wyjsciu w bieg ze sprintu W3, 0 w biegu gry
		{
			oCAniCtrl_Human* ac = player->GetAnictrl();
			zCModel* hm = player->GetModel();
			zCModelAni* loop = ac && hm && ac->_s_walkl >= 0 ? hm->GetAniFromAniID(ac->_s_walkl) : nullptr;
			float front = loop ? loop->aniBBox3DObjSpace.maxs.n[2] : 0.0f;
			if (loop && zdcore::TravLoopBoxTooLong(front, TRAV_LOOP_BOX)) {
				float clear = TRAV_EXIT_PATH.need;
				for (int i = 0; i < k; i++)
					if (hits[i] >= 0.0f && hits[i] < clear)
						clear = hits[i];
				char b[260];
				sprintf_s(b, "petla %s z %s: ramka animacji siega %.0f cm przed postac (gra do ok. 100) - kolizja wypchnelaby do "
					"tylu o ok. %.0f cm (stara paczka zDismember_W3.vdf?)", loop->aniName.ToChar(), MvOverlayOfAni(hm, loop).c_str(),
					front, zdcore::TravLoopBoxPush(front, clear));
				why = b;
				return false;
			}
		}
		if (!zcfg.teleportGuard || !ts.cdOff)
			return true;
		// sonda kroku: kolizja na chwile wlaczona, potem z powrotem wylaczona (tor dalej prowadzi voba)
		zVEC3 q = p;
		float g;
		if (TravGroundAt(p, g) && p.n[1] - ts.yOff < g)
			q.n[1] = g + ts.yOff;
		TravWhy w("sonda wyjscia w bieg");
		player->SetCollDetStat(ts.cdStat ? TRUE : FALSE);
		player->SetCollDetDyn(ts.cdDyn ? TRUE : FALSE);
		float zero = TravCollProbe(q);
		zVEC3 step(q.n[0] + ts.dir.n[0] * TRAV_EXIT_PATH.stepCm, q.n[1], q.n[2] + ts.dir.n[2] * TRAV_EXIT_PATH.stepCm);
		player->SetPositionWorld(step);
		zVEC3 got = player->GetPositionWorld();
		float moved = sqrtf((got.n[0] - q.n[0]) * (got.n[0] - q.n[0]) + (got.n[2] - q.n[2]) * (got.n[2] - q.n[2]));
		TravTeleportHero(p, "sonda wyjscia w bieg - powrot");
		player->SetCollDetStat(FALSE);
		player->SetCollDetDyn(FALSE);
		if (zero > TRAV_SETTLE_TOL || !zdcore::TravRunExitStepOk(TRAV_EXIT_PATH.stepCm, moved, TRAV_EXIT_PATH)) {
			char b[200];
			sprintf_s(b, "sonda kolizji: ruch zerowy %.0f cm, krok %.0f cm do przodu dal %.0f cm (silnik wypchnalby bohatera z gory)", zero,
				TRAV_EXIT_PATH.stepCm, moved);
			why = b;
			return false;
		}
		return true;
	}

	static bool TravBeginRunExit() {
		zCModel* hm = player->GetModel();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		if (!hm || !ac || ac->_s_walkl < 0)
			return false;
		std::string block;
		if (!TravRunExitAllowed(block)) {
			ts.runExitBlocked = true;
			ZDebug("Przeskok %s: wyjscie w bieg zablokowane po %.2f s - %s; koniec w postawie", ts.def->id.c_str(), ts.t, block.c_str());
			return false;
		}
		zCModelAni* ani = TravAni(hm);
		zCModelAniActive* out = ani ? hm->GetActiveAni(ani) : nullptr;
		if (out)
			out->blendOutOverride = -1.0f / TRAV_RUN_EXIT_BLEND;
		hm->StartAni(ac->_s_walkl, 0);
		zCModelAni* loop = hm->GetAniFromAniID(ac->_s_walkl);
		zCModelAniActive* in = loop ? hm->GetActiveAni(loop) : nullptr;
		if (!in) {
			ZDebug("Przeskok %s: petla biegu nie wystartowala - koniec w postawie", ts.def->id.c_str());
			return false;
		}
		MvSetBlend(hm, in, ani ? hm->GetActiveAni(ani) : nullptr, TRAV_RUN_EXIT_BLEND);
		// runda 12: stopy nie ponizej gory przed wlaczeniem kolizji (inaczej kolizja wypycha z przeszkody)
		zVEC3 p = player->GetPositionWorld();
		float g;
		if (TravGroundAt(p, g) && p.n[1] - ts.yOff < g)
			player->SetPositionWorld(zVEC3(p.n[0], g + ts.yOff, p.n[2]));
		TravRestoreCD();
		TravSettleEnd("wyjscie w bieg");
		player->SetMovLock(FALSE);
		ts.runExit = true;
		ts.ending = TRAV_RUN_EXIT_BLEND;
		ts.exitY0 = player->GetPositionWorld().n[1];
		ts.exitT = 0.0f;
		ZDebug("Przeskok %s: po %.2f s z W prosto w bieg (%s)", ts.def->id.c_str(), ts.t, loop->aniName.ToChar());
		TravPostWatchStart((ts.def->id + " bieg").c_str());
		return true;
	}

	// Ochrona animacji: 1 gra dalej, 0 dobiegla konca (zwykly koniec), -1 przerwana przez gre (trafienie, smierc,
	// reakcja) - koniec ruchu. Zgaszona przez inna wtyczke (zalozenie / zdjecie nakladki zatrzymuje wszystkie animacje)
	// - wznowienie w tym samym miejscu (najwyzej TRAV_MAX_RESUMES).
	static int TravGuard(zCModel* hm) {
		int hpNow = player->attribute[NPC_ATR_HITPOINTS];
		bool hit = hpNow < ts.heroHp;
		ts.heroHp = hpNow;
		if (hit || player->IsDead() || player->IsUnconscious())
			return -1;
		zCModelAni* ani = TravAni(hm);
		zCModelAniActive* act = ani ? hm->GetActiveAni(ani) : nullptr;
		if (act && !act->isFadingOut) {
			ts.progress = act->GetProgressPercent();
			return 1;
		}
		if (ani && !act && ts.t <= 0.1f)
			return 1;
		if (!act && ts.progress >= 0.9f)
			return 0;
		if (!ani || ts.resumes >= TRAV_MAX_RESUMES)
			return -1;
		ts.resumes++;
		if (act)
			hm->StopAni(ani);
		hm->StartAni(ani, 0);
		if (zCModelAniActive* a = hm->GetActiveAni(ani))
			a->SetProgressPercent(ts.progress);
		for (zCModelAni* a : MvMotionLoops(hm))
			hm->StopAni(a);
		ZDebug("Przeskok %s: animacje zgasila inna wtyczka / silnik po %.2f s - wznowiona w %.0f%% (%d/%d)", ts.def->id.c_str(),
			ts.t, ts.progress * 100.0f, ts.resumes, TRAV_MAX_RESUMES);
		return 1;
	}

	// Klatka ruchu: vob w punkcie toru (start + prawo * x + sciana * z, ziemia startu + y), z kontrola drogi promieniem
	// (cos weszlo na tor - przerwanie, nigdy wejscie w sciane) i obrotem przodem do sciany. false = przerwany.
	static bool TravStep() {
		float x, y, z;
		zdcore::TravSample(*ts.def, ts.fit, ts.t, x, y, z);
		zVEC3 right(ts.dir.n[2], 0.0f, -ts.dir.n[0]);
		zVEC3 target(ts.start.n[0] + right.n[0] * x + ts.dir.n[0] * z, ts.startGround + ts.yOff + y,
			ts.start.n[2] + right.n[2] * x + ts.dir.n[2] * z);
		zVEC3 cur = player->GetPositionWorld();
		zVEC3 d = target - cur;
		if (TravLen(d) > 0.5f && MvTrace(cur, d, player, nullptr, TRAV_TRACE)) {
			TravFinish(true, "przeszkoda na torze");
			return false;
		}
		player->SetPositionWorld(target);
		ts.lastTarget = target;
		ts.hasTarget = true;
		if (ts.t <= TRAV_TURN_TIME + 0.05f && fabsf(zdcore::AngleDiff(ts.yaw0, ts.yaw1)) > 1e-3f) {
			float yaw = zdcore::LerpYaw(ts.yaw0, ts.yaw1, zdcore::TravSmooth01(ts.t / TRAV_TURN_TIME));
			player->SetHeadingAtWorld(zVEC3(sinf(yaw), 0.0f, cosf(yaw)));
		}
		return true;
	}

	// Stan bohatera do logu wcisniecia: pozycja, predkosc, stan ciala, klawisze, bron.
	static std::string TravHeroState() {
		if (!player)
			return "brak bohatera";
		char buf[200];
		zVEC3 p = player->GetPositionWorld();
		sprintf_s(buf, "poz (%.0f, %.0f, %.0f), predkosc %.0f, stan %d, W %d S %d, bron %d", p.n[0], p.n[1], p.n[2], heroSpeed,
			player->GetBodyState() & BS_ONLY_STATE, MvRawState(GAME_UP) ? 1 : 0, MvRawState(GAME_DOWN) ? 1 : 0, player->GetWeaponMode());
		return buf;
	}

	// ---------- Skok gry po odmowie: parametry silnika i animacji dla werdyktu (Core TravGameJumpVerdict) ----------
	static float travJumpLogged[3] = { -1.0f, -1.0f, -1.0f };   // zasieg, szczyt, krawedz z ostatniego logu parametrow
	static float travJumpAnimSpeed = 372.0f;                      // pozioma predkosc T_RUNL_2_JUMP (cm/s)
	static const zdcore::TravMomentumParams TRAV_MOMENTUM;
	static bool travLedgePending = false;                         // skok z miejsca gry w nastepnej klatce petli
	// Dobieg do sciany i skok z miejsca gry przy niej (werdykt LEDGE dla sciany dalej niz ledgeFace).
	struct TravLedgeRun {
		bool active = false;
		float t = 0.0f, yaw0 = 0.0f, face0 = 0.0f, face = 0.0f;
	};
	static TravLedgeRun tlr;
	static const float TRAV_LEDGE_REACH = 80.0f;   // dobieg: skok z miejsca, gdy sciana najwyzej tyle przed bohaterem
	static const float TRAV_LEDGE_RUN_MAX = 2.5f;  // s dobiegu najwyzej
	// Wspiecie / przeskok z miejsca (klipy idle): start w petli (nie w haku wejscia).
	struct TravIdleStartReq {
		bool active = false;
		const zdcore::TravDef* def = nullptr;
		zdcore::TravResult res;
		zVEC3 fwd;
	};
	static TravIdleStartReq tis;

	// Parametry skoku w biegu: zasieg z ruchu korzenia T_RUNL_2_JUMP modelu (z nakladkami, np. HUMANS_ACROBATIC),
	// promien silnika ze srodka postaci pod zMV_MAX_GROUND_ANGLE_WALK, krawedz skoku z miejsca jak DetectClimbUpLedge
	// (0,95 * GetJumpUpHeight + wzrost nad stopami). Log przy kazdej zmianie (nakladka akrobatyki, inny model).
	static zdcore::TravGameJumpParams TravJumpParams(float groundY) {
		zdcore::TravGameJumpParams p;
		zCModel* m = player ? player->GetModel() : nullptr;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		std::string src = "brak";
		if (m) {
			int id = m->GetAniIDFromAniName("T_RUNL_2_JUMP");
			zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			if (a) {
				src = MvOverlayOfAni(m, a);
				bool acro = zdcore::Upper(src).find("ACROBATIC") != std::string::npos;
				zVEC3 tr = a->GetAniTranslation();
				float reach = sqrtf(tr.n[0] * tr.n[0] + tr.n[2] * tr.n[2]);
				p.reach = reach >= 100.0f && reach <= 900.0f ? reach : (acro ? 470.0f : 290.0f);
				// predkosc pozioma animacji (ped skoku: dodatek ponad nia)
				float dur = a->numFrames > 1 && a->fpsRate > 0.0f ? (float)(a->numFrames - 1) / a->fpsRate : 0.0f;
				travJumpAnimSpeed = dur > 0.1f ? p.reach / dur : 372.0f;
				if (acro) {
					p.rise = 38.0f;
					p.apexAt = 0.78f;
				}
			}
		}
		float jh = 0.0f, body = 0.0f;
		if (ac) {
			float ry = ac->centerPos.n[1] - groundY;
			if (ry > 30.0f && ry < 200.0f)
				p.rayY = ry;
			float up = sinf(ac->config.zMV_MAX_GROUND_ANGLE_WALK);
			if (up > 0.1f && up < 1.0f)
				p.rayUp = up;
			jh = ac->GetJumpUpHeight();
			body = ac->headY - ac->feetY;
			if (jh > 0.0f && jh < 1000.0f && body > 100.0f && body < 260.0f)
				p.ledgeMax = 0.95f * jh + body + (ac->feetY - groundY);
		}
		// runda 12: chwyt w locie dokonczy skok gry (bez broni, TraversalAirGrab) - krawedz do wzrostu + szczytu lotu + 45 cm
		p.airGrab = zcfg.traversalAirGrab && player && player->GetWeaponMode() == NPC_WEAPON_NONE;
		p.grabTopMax = (body > 100.0f && body < 260.0f ? body : 180.0f) + p.rise + 45.0f;
		if (fabsf(p.reach - travJumpLogged[0]) > 1.0f || fabsf(p.rise - travJumpLogged[1]) > 1.0f ||
			fabsf(p.ledgeMax - travJumpLogged[2]) > 5.0f) {
			travJumpLogged[0] = p.reach;
			travJumpLogged[1] = p.rise;
			travJumpLogged[2] = p.ledgeMax;
			ZDebug("Skok gry: T_RUNL_2_JUMP z %s - zasieg %.0f cm (+%.0f opadanie), szczyt %.0f cm; promien silnika %.0f cm nad "
				"ziemia, %.2f w gore na cm, %.0f cm; skok z miejsca: GetJumpUpHeight %.0f, wzrost %.0f, krawedz do %.0f cm "
				"(YMAX_JUMPLOW %.0f, YMAX_JUMPMID %.0f)", src.c_str(), p.reach, p.landExtra, p.rise, p.rayY, p.rayUp, p.rayLen, jh,
				body, p.ledgeMax, ac ? ac->config.zMV_YMAX_JUMPLOW : 0.0f, ac ? ac->config.zMV_YMAX_JUMPMID : 0.0f);
			ZDebug("Skok gry: animacja %.0f cm/s, ped skoku %s (JumpKeepMomentum, do %.0f cm/s)", travJumpAnimSpeed,
				zcfg.jumpKeepMomentum ? "wlaczony" : "wylaczony", TRAV_MOMENTUM.maxSpeed);
		}
		// ped skoku (JumpKeepMomentum): skok z biegu / sprintu dalszy - luk i werdykt wg predkosci teraz
		if (zcfg.jumpKeepMomentum)
			p = zdcore::TravGameJumpWithSpeed(p, heroSpeed, travJumpAnimSpeed, TRAV_MOMENTUM);
		return p;
	}

	// ---------- Sledzenie po odmowie (log, Debug=1): pozycja wzgledem klatki wcisniecia po 0,1 / 0,3 / 0,6 / 1,0 s ----------
	struct TravTrack {
		bool active = false;
		float t = 0.0f;
		zVEC3 p0, fwd;
		float g0 = 0.0f;              // ziemia pod bohaterem przy wcisnieciu
		std::string what, out;
		int next = 0;
		float prevAlong = 0.0f, maxStep = 0.0f, maxStepT = 0.0f, minAlong = 0.0f, minAlongT = 0.0f;
		std::string stepAni;
		std::string ledge;            // krawedz znaleziona przez silnik (pierwsza)
	};
	static TravTrack tk;
	static const float TRAV_TRACK_T[] = { 0.1f, 0.3f, 0.6f, 1.0f };

	// Animacje warstwy 1 (~ = wygasa).
	static std::string TravLayerOne(zCModel* m) {
		std::string s;
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1)
				continue;
			s += (s.empty() ? "" : "+") + std::string(p->aniName.ToChar()) + (a->isFadingOut ? "~" : "");
		}
		return s.empty() ? "-" : s;
	}

	static void TravTrackStart(const std::string& what) {
		if (!zcfg.debug || !player)
			return;
		tk = TravTrack();
		tk.active = true;
		tk.p0 = player->GetPositionWorld();
		tk.fwd = TravHeading();
		tk.what = what;
		if (!TravGroundAt(tk.p0, tk.g0))
			tk.g0 = tk.p0.n[1];
		ZDebug("Skok gry [sledzenie] start: %s; %s, animacje %s", what.c_str(), TravHeroState().c_str(),
			TravLayerOne(player->GetModel()).c_str());
	}

	static void TravTrackUpdate(float dt) {
		if (!tk.active)
			return;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!ac || !player->GetModel()) {
			tk.active = false;
			return;
		}
		tk.t += dt;
		zVEC3 d = player->GetPositionWorld() - tk.p0;
		float along = d.n[0] * tk.fwd.n[0] + d.n[2] * tk.fwd.n[2];
		float side = d.n[0] * tk.fwd.n[2] - d.n[2] * tk.fwd.n[0];
		float step = along - tk.prevAlong;
		tk.prevAlong = along;
		if (step < tk.maxStep) {
			tk.maxStep = step;
			tk.maxStepT = tk.t;
			tk.stepAni = TravLayerOne(player->GetModel());
		}
		if (along < tk.minAlong) {
			tk.minAlong = along;
			tk.minAlongT = tk.t;
		}
		if (tk.ledge.empty() && ac->GetFoundLedge()) {
			char lb[96];
			sprintf_s(lb, "; krawedz silnika %+.0f cm nad ziemia startu w %.2f s", ac->GetLedgeInfo()->point.n[1] - tk.g0, tk.t);
			tk.ledge = lb;
		}
		while (tk.next < 4 && tk.t >= TRAV_TRACK_T[tk.next]) {
			zVEC3 v(0.0f, 0.0f, 0.0f);
			if (player->physicsEnabled)
				if (zCRigidBody* rb = player->GetRigidBody())
					v = rb->v;
			char buf[320];
			sprintf_s(buf, "; %.1f s: %+.0f cm (bok %+.0f, y %+.0f), stan %d, ruch %d, fizyka %d, v %+.0f/%+.0f, %s", TRAV_TRACK_T[tk.next],
				along, side, d.n[1], player->GetBodyState() & BS_ONLY_STATE, (int)ac->state, player->physicsEnabled ? 1 : 0,
				v.n[0] * tk.fwd.n[0] + v.n[2] * tk.fwd.n[2], v.n[1], TravLayerOne(player->GetModel()).c_str());
			tk.out += buf;
			tk.next++;
		}
		if (tk.next >= 4) {
			ZDebug("Skok gry [sledzenie] %s%s; najdalej w tyl %+.0f cm w %.2f s, najwiekszy krok w tyl %+.0f cm w %.2f s (%s)%s",
				tk.what.c_str(), tk.out.c_str(), tk.minAlong, tk.minAlongT, tk.maxStep, tk.maxStepT,
				tk.stepAni.empty() ? "-" : tk.stepAni.c_str(), tk.ledge.c_str());
			tk.active = false;
		}
	}

	static bool TravIdleDecide(const char* runWhy);

	// Skok z miejsca gry (zawis / wspiecie) zamiast skoku w biegu, ktory silnik anuluje albo ktory odbilby sie od
	// sciany - jak PC_JumpForward przy postawie (S_RUN): JumpForward (CanJumpLedge: JUMPUPLOW / JUMPUPMID / JUMPUP i
	// zawis), stan skoku, do_jump = 0. Silnik bez krawedzi (CanJumpLedge = 0, runda 8): nasze wspiecie / przeskok
	// z miejsca (klipy idle, bez broni), a gdy ich brak - i tak JumpForward (0x6B21E0 bez krawedzi: skok z miejsca
	// gry do przodu, przy przeszkodzie w gore) - nie "nic".
	static void TravLedgeJump(const char* src) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!ac || player->IsDead() || ts.def || !player->GetModel() || ac->state != zCAIPlayer::zMV_STATE_STAND ||
			player->physicsEnabled || player->IsMovLock()) {
			ZDebug("Przeskok [%s]: skok z miejsca gry pominiety (bohater zajety / w powietrzu)", src);
			return;
		}
		int kind = ac->CanJumpLedge();
		float h = ac->GetFoundLedge() ? ac->GetLedgeInfo()->point.n[1] - ac->feetY : 0.0f;
		if (kind <= 0) {
			ac->ClearFoundLedge();
			if (zcfg.traversal && player->GetWeaponMode() == NPC_WEAPON_NONE &&
				TravIdleDecide("silnik nie widzi krawedzi po dobiegu")) {
				ZDebug("Przeskok [%s]: silnik nie widzi krawedzi przed bohaterem - nasze wspiecie / przeskok z miejsca", src);
				return;
			}
			// runda 12 (log 138318.73: skok pozycji 219 cm przy JumpForward z naszej petli w biegu): skok gry przez klawisz
			// (silnik sam, w swoim AI - bieg: skok w biegu, postoj: skok z miejsca), nie JumpForward spoza AI
			TravTrackStart("skok gry (silnik bez krawedzi)");
			jumpInjectFrames = 3;
			jumpInjectToggle = true;
			ZDebug("Przeskok [%s]: silnik nie widzi krawedzi przed bohaterem - skok gry klawiszem (bez zawisu)", src);
			return;
		}
		const char* what = kind == 3 ? "zawis (JUMPUP)" : kind == 2 ? "wspiecie (JUMPUPMID)" : "wspiecie niskie (JUMPUPLOW)";
		TravTrackStart(std::string("skok z miejsca: ") + what);
		ac->JumpForward();
		player->SetBodyState(BS_JUMP);
		ac->do_jump = 0;
		ZDebug("Przeskok [%s]: skok z miejsca gry - %s, krawedz %.0f cm nad stopami", src, what, h);
	}

	// ---------- Debug wizualny (zdism trav show / hide, TraversalDebug=1) ----------
	struct TravDebugView {
		bool on = false;
		bool lines = true;            // linie przez zrenderer->DrawLine; false = kropki (napisy) wzdluz linii
		float probe = 0.0f;           // do nastepnego wykrywania (co TRAV_DBG_PROBE)
		float freeze = 0.0f;          // zamrozony obraz decyzji po Alt (s)
		zVEC3 o, fwd;                 // uklad elementow: ziemia pod bohaterem, kierunek
		std::vector<zdcore::TravDrawItem> items;
		std::string decision, jump;   // napisy w rogu: decyzja / co zrobi skok gry
		float range = 0.0f;
	};
	static TravDebugView travDbg;
	static int travDbgIni = -1;       // ostatnio zastosowane TraversalDebug z Gothic.ini
	static const float TRAV_DBG_PROBE = 0.1f, TRAV_DBG_FREEZE = 5.0f;

	static void TravDebugSet(bool on, const char* src) {
		if (travDbg.on == on)
			return;
		bool lines = travDbg.lines;
		travDbg = TravDebugView();
		travDbg.on = on;
		travDbg.lines = lines;
		ZLog("Przeskok debug: %s (%s)", on ? "wlaczony - wykrywanie co 0,1 s przed bohaterem, Alt zamraza obraz decyzji na 5 s" :
			"wylaczony", src);
	}

	static void TravDebugSetItems(const zdcore::TravResult& res, const zdcore::TravDef* def, const std::vector<zdcore::TravRayRec>* rays,
		const zVEC3& origin, const zVEC3& fwd, const std::string& decision, const zdcore::TravGameJumpParams* jp,
		const zdcore::TravGameJumpInfo* g) {
		zdcore::TravDebugInfo in;
		in.res = res;
		in.def = def;
		if (rays)
			in.rays = *rays;
		in.lim = TRAV_LIMITS;
		in.tm = TRAV_TIMING;
		in.decision = decision;
		if (jp && g) {
			in.showJump = true;
			in.jp = *jp;
			in.jump = *g;
			in.gameJump = zdcore::TravGameJumpLabel(*g);
		}
		travDbg.items = zdcore::TravDebugBuild(in);
		travDbg.o = origin;
		travDbg.fwd = fwd;
		travDbg.decision = decision;
		travDbg.jump = in.gameJump;
	}

	// Alt: obraz decyzji zamrozony na TRAV_DBG_FREEZE s ("Alt: <decyzja>") i ta sama decyzja w logu.
	static void TravDebugFreeze(const zdcore::TravResult& res, const zdcore::TravDef* def, const std::vector<zdcore::TravRayRec>* rays,
		const zVEC3& origin, const zVEC3& fwd, const std::string& decision, const zdcore::TravGameJumpParams* jp = nullptr,
		const zdcore::TravGameJumpInfo* g = nullptr) {
		if (!travDbg.on)
			return;
		std::string label = "Alt: " + decision;
		TravDebugSetItems(res, def, rays, origin, fwd, label, jp, g);
		travDbg.freeze = TRAV_DBG_FREEZE;
		ZLog("Przeskok debug: %s%s%s", label.c_str(), travDbg.jump.empty() ? "" : " | ", travDbg.jump.c_str());
	}

	// Odmowa przy wcisnieciu Alt: skok gry wg werdyktu - przepuszczony (false: klawisz dla gry), bez skoku albo skok
	// z miejsca w nastepnej klatce petli (true: klawisz przejety). label: decyzja (debug), detail: opis do logu.
	static bool TravRefuse(const zdcore::TravResult& res, const zdcore::TravDef* def, const std::string& detail,
		const std::string& label, const std::vector<zdcore::TravRayRec>* rays, const zVEC3& origin, const zVEC3& fwd) {
		if (player->GetWeaponMode() != NPC_WEAPON_NONE) {
			// z bronia w reku Alt to przewrot / atak z wyskoku / skok walki - jak dotad dla gry i unikow
			ZDebug("Przeskok [Alt]: %s - bron w reku: Alt dla gry / unikow", detail.c_str());
			TravDebugFreeze(res, def, rays, origin, fwd, label + " - bron w reku: Alt dla gry / unikow");
			return false;
		}
		zdcore::TravGameJumpParams jp = TravJumpParams(origin.n[1]);
		zdcore::TravGameJumpInfo g = zdcore::TravGameJumpVerdict(res, jp);
		// petla biegu nieaktywna (zatrzymywanie, przejscie) - silnik zrobi skok z miejsca (z chwytem krawedzi): dla gry
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* hm = player->GetModel();
		zCModelAni* loop = ac && hm && ac->_s_walkl >= 0 ? hm->GetAniFromAniID(ac->_s_walkl) : nullptr;
		if (!loop || !hm->IsAniActive(loop)) {
			g.verdict = zdcore::TRAVJUMP_PASS;
			g.why = "bez petli biegu - silnik zrobi skok z miejsca";
		}
		std::string jl = zdcore::TravGameJumpLabel(g);
		ZDebug("Przeskok [Alt]: %s - %s", detail.c_str(), jl.c_str());
		TravDebugFreeze(res, def, rays, origin, fwd, label, &jp, &g);
		if (g.verdict == zdcore::TRAVJUMP_PASS) {
			TravTrackStart(jl);
			return false;
		}
		if (g.verdict == zdcore::TRAVJUMP_LEDGE) {
			if (res.face <= jp.ledgeFace)
				travLedgePending = true;
			else {
				// dalej od sciany: bieg do niej trwa (W), skok z miejsca gry przy scianie (TravLedgeRunUpdate)
				tlr = TravLedgeRun();
				tlr.active = true;
				tlr.yaw0 = atan2f(fwd.n[0], fwd.n[2]);
				tlr.face0 = tlr.face = res.face;
			}
		}
		else
			TravTrackStart(jl);
		return true;
	}

	// Skok gry teraz (przerwany bufor wcisniecia): wstrzykniecie klawisza skoku jak po oknie Alt w Dodge.cpp (GetState
	// przez kilka klatek, GetToggled raz) - tylko na ziemi, bez blokady.
	static void TravInjectJump(const char* why) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		bool ok = player && !player->IsDead() && ac && ac->state == zCAIPlayer::zMV_STATE_STAND && !player->physicsEnabled &&
			!player->IsMovLock();
		if (ok) {
			jumpInjectFrames = 3;
			jumpInjectToggle = true;
		}
		ZDebug("Przeskok [bufor]: przerwany po %.2f s (%s) - %s", tp.time, why, ok ? "skok gry teraz" : "bez skoku (w powietrzu / zajety)");
	}

	// Bufor wcisniecia: bieg do sciany, co klatke wykrywanie od nowa (bohater moze skrecac, sciana sie zbliza), start
	// gdy sciana w oknie startu klipu (kGo * edge); przerwanie (TravBufferDecide) = skok gry teraz.
	static void TravPendingUpdate(float dt) {
		ZPROF_SCOPE("Traversal.Buffer");   // bieg do sciany po Alt: wykrywanie co klatke
		bool first = tp.first;
		tp.first = false;
		tp.time += dt;
		int wm = 0;
		const char* why = nullptr;
		zdcore::TravBufferInput in;
		in.time = tp.time;
		in.forwardKey = MvRawState(GAME_UP) && !MvRawState(GAME_DOWN);
		in.running = TravHeroCanStart(true, wm, why);
		zVEC3 fwd = TravHeading();
		in.turn = zdcore::AngleDiff(tp.yaw0, atan2f(fwd.n[0], fwd.n[2]));
		zdcore::TravResult res;
		const zdcore::TravDef* def = nullptr;
		if (in.running && in.forwardKey) {
			res = TravProbeBuffered(fwd, false);
			def = TravPickHere(res, fwd, false);
			// start tylko z pelnego szukania (teren rozbiegu od bohatera do sciany)
			if (def && !res.groundZ.empty() && res.groundZ.front() > 0.5f) {
				zdcore::TravTiming tm0 = TRAV_TIMING;
				tm0.maxWait = tm0.bufferMax - tp.time > 0.0f ? tm0.bufferMax - tp.time : 0.0f;
				if (zdcore::TravPlanStart(res.face, def->edge, heroSpeed, !first, tm0) == zdcore::TRAVSTART_NOW) {
					res = TravProbeBuffered(fwd, true);
					def = TravPickHere(res, fwd, false);
				}
			}
		}
		in.obstacleOk = def != nullptr;
		if (def) {
			zdcore::TravTiming tm = TRAV_TIMING;
			tm.maxWait = tm.bufferMax - tp.time > 0.0f ? tm.bufferMax - tp.time : 0.0f;
			in.plan = zdcore::TravPlanStart(res.face, def->edge, heroSpeed, !first, tm);
		}
		const char* bwhy = "";
		zdcore::TravBufferStep step = zdcore::TravBufferDecide(in, TRAV_TIMING, bwhy);
		if (step == zdcore::TRAVBUF_KEEP)
			return;
		if (step == zdcore::TRAVBUF_START) {
			ZDebug("Przeskok [bufor]: po %.2f s start %s - sciana %.0f cm (przy wcisnieciu %.0f), predkosc %.0f, szukan pelnych %d, "
				"lokalnych %d", tp.time, def->id.c_str(), res.face, tp.face0, heroSpeed, tp.fullProbes, tp.localProbes);
			if (!TravBegin(def, res, fwd)) {
				ZDebug("Przeskok %s: nie wystartowal", def->id.c_str());
				TravInjectJump("start nieudany");
			}
			tp = TravPending();
			return;
		}
		std::string detail;
		if (!in.running && why)
			detail = why;
		else if (in.running && in.forwardKey && !def)
			detail = res.kind == zdcore::TRAV_NONE ? zdcore::TravDescribe(res) : zdcore::TravDescribe(res) + " - brak klipu";
		else if (def)
			detail = def->id + ": " + TravStartName(in.plan) + ", sciana " + std::to_string((int)res.face) + " cm";
		else
			detail = TravHeroState();
		char full[400];
		sprintf_s(full, "%s; %s; skret %.0f st.", bwhy, detail.c_str(), in.turn * 57.2958f);
		// skok gry wg werdyktu (wysoka sciana w zasiegu lotu - bez skoku, przy scianie - skok z miejsca gry)
		zVEC3 origin = player ? player->GetPositionWorld() : zVEC3(0.0f, 0.0f, 0.0f);
		float g0;
		if (player && TravGroundAt(origin, g0))
			origin.n[1] = g0;
		zdcore::TravGameJumpParams jp = TravJumpParams(origin.n[1]);
		zdcore::TravGameJumpInfo gj = zdcore::TravGameJumpVerdict(res, jp);
		if (player && player->GetWeaponMode() != NPC_WEAPON_NONE)
			gj.verdict = zdcore::TRAVJUMP_PASS, gj.why = "bron w reku - skok walki gry";
		std::string jl = zdcore::TravGameJumpLabel(gj);
		TravDebugFreeze(res, def, nullptr, origin, fwd, std::string("bufor przerwany (") + bwhy + ")", &jp, &gj);
		if (gj.verdict == zdcore::TRAVJUMP_PASS) {
			TravInjectJump((std::string(full) + "; " + jl).c_str());
			TravTrackStart(jl);
		}
		else {
			ZDebug("Przeskok [bufor]: przerwany po %.2f s (%s) - %s", tp.time, full, jl.c_str());
			if (gj.verdict == zdcore::TRAVJUMP_LEDGE && res.face <= jp.ledgeFace)
				TravLedgeJump("bufor");
			else if (gj.verdict == zdcore::TRAVJUMP_LEDGE) {
				tlr = TravLedgeRun();
				tlr.active = true;
				tlr.yaw0 = atan2f(fwd.n[0], fwd.n[2]);
				tlr.face0 = tlr.face = res.face;
			}
			else
				TravTrackStart(jl);
		}
		tp = TravPending();
	}

	// Z miejsca / z wolnego chodu (bez broni): sciana do idleMaxFace i klip idle (najblizsza wysokosc, podparcie konca) -
	// wcisniecie nasze, start w petli; inaczej false (skok gry - zawis / wspiecie Gothica). runWhy: czemu nie z biegu.
	static bool TravIdleDecide(const char* runWhy) {
		zVEC3 fwd = TravHeading(), origin = player->GetPositionWorld();
		std::string log, endLog;
		std::vector<zdcore::TravRayRec> rays;
		zdcore::TravResult res = TravProbe(fwd, zcfg.debug ? &log : nullptr, TRAV_TIMING.idleLook, 0.0f, 0.0f,
			travDbg.on ? &rays : nullptr, &origin);
		ZDebug("Przeskok [Alt z miejsca]: %s (%s)", TravHeroState().c_str(), runWhy);
		ZDebug("Przeskok [Alt z miejsca]: %s", log.c_str());
		if (zcfg.debug && res.wallDist > 0.0f)
			ZDebug("Przeskok [Alt z miejsca]: sciana trafiona: %s", TravWallHitName(fwd, res).c_str());
		else if (zcfg.debug)
			ZDebug("Przeskok [Alt z miejsca]: bez sciany - promienie przed bohaterem: %s",
				TravRayAudit(fwd, TRAV_TIMING.idleLook).c_str());
		const zdcore::TravDef* def = TravPickHere(res, fwd, true, zcfg.debug ? &endLog : nullptr, travDbg.on ? &rays : nullptr);
		if (!endLog.empty())
			ZDebug("Przeskok [Alt z miejsca]: %s", endLog.c_str() + 2);
		if (!def) {
			ZDebug("Przeskok [Alt z miejsca]: %s - skok gry (zawis / wspiecie Gothica)", zdcore::TravDescribe(res).c_str());
			TravDebugFreeze(res, nullptr, &rays, origin, fwd, zdcore::TravDecisionLabel(res, nullptr) + " - z miejsca: skok gry");
			return false;
		}
		zdcore::TravStart st = zdcore::TravIdleStart(res.face, TRAV_TIMING);
		std::string label = zdcore::TravDecisionLabel(res, def) + " - " + TravStartName(st);
		if (st != zdcore::TRAVSTART_NOW) {
			ZDebug("Przeskok [Alt z miejsca]: %s, %s: sciana %.0f cm (z miejsca do %.0f) - skok gry", zdcore::TravDescribe(res).c_str(),
				def->id.c_str(), res.face, TRAV_TIMING.idleMaxFace);
			TravDebugFreeze(res, def, &rays, origin, fwd, label + " - skok gry");
			return false;
		}
		tis = TravIdleStartReq();
		tis.active = true;
		tis.def = def;
		tis.res = res;
		tis.fwd = fwd;
		ZDebug("Przeskok [Alt z miejsca]: klawisz przejety - %s, %s (sciana %.0f cm)", zdcore::TravDescribe(res).c_str(),
			def->id.c_str(), res.face);
		TravDebugFreeze(res, def, &rays, origin, fwd, label);
		return true;
	}

	// 23.10: wspiecie / przeskok z miejsca z bronia w reku (chowana od razu na starcie) - tylko z W bez S i bokow: Alt
	// z kierunkiem w walce to przewrot / unik, sam Alt - skok walki gry (Dodge.cpp).
	static bool TravArmedIdleOk(int wm) {
		if (!zcfg.traversalSheathe || wm < NPC_WEAPON_FIST || wm > NPC_WEAPON_2HS || !TravLeftHandBusy().empty())
			return false;
		return MvRawState(GAME_UP) && !MvRawState(GAME_DOWN) && !MvRawState(GAME_LEFT) && !MvRawState(GAME_RIGHT) &&
			!MvRawState(GAME_STRAFELEFT) && !MvRawState(GAME_STRAFERIGHT);
	}

	// Rozstrzygniecie wcisniecia klawisza skoku: przeszkoda przed biegnacym bohaterem (sciana w zasiegu wg predkosci)
	// pasuje do klipu i da sie zaczac od razu albo po biegu (bufor) - wcisniecie nasze, start w TraversalUpdate (nie
	// w haku wejscia, ktory silnik wola w trakcie AI bohatera). Inaczej skok gry od razu. Log (Debug=1): stan bohatera,
	// teren przed nim, sciana, profil nad podstawa, gora i decyzja.
	static bool TravDecide() {
		ZPROF_SCOPE("Traversal.Decide");   // rozstrzygniecie wcisniecia skoku (hak wejscia albo MovesUpdate)
		int wm = 0;
		const char* why = nullptr;
		if (!TravHeroCanStart(true, wm, why)) {
			// z miejsca / z wolnego chodu (bez broni): wspiecie / przeskok W3 z klipow idle; gdy nic - skok gry (zawis Gothica)
			int wmIdle = 0;
			const char* whyIdle = nullptr;
			if (why && zcfg.traversal && TravHeroCanStart(false, wmIdle, whyIdle) && (wmIdle == NPC_WEAPON_NONE ||
				TravArmedIdleOk(wmIdle)) && TravIdleDecide(why))
				return true;
			if (why && zcfg.traversal) {
				ZDebug("Przeskok [Alt]: %s - skok gry (%s)", why, TravHeroState().c_str());
				if (travDbg.on && player && player->GetModel() && ogame && ogame->GetGameWorld() && !ts.def) {
					zVEC3 fwd = TravHeading(), origin = player->GetPositionWorld();
					std::vector<zdcore::TravRayRec> rays;
					zdcore::TravResult res = TravProbe(fwd, nullptr, -1.0f, 0.0f, 0.0f, &rays, &origin);
					TravDebugFreeze(res, nullptr, &rays, origin, fwd, std::string(why) + " - skok gry");
				}
			}
			return false;
		}
		zVEC3 fwd = TravHeading();
		std::string log;
		std::vector<zdcore::TravRayRec> rays;
		zVEC3 origin = player->GetPositionWorld();
		zdcore::TravResult res = TravProbe(fwd, zcfg.debug ? &log : nullptr, -1.0f, 0.0f, 0.0f, travDbg.on ? &rays : nullptr, &origin);
		ZDebug("Przeskok [Alt]: %s, zasieg %.0f cm", TravHeroState().c_str(),
			zdcore::TravLookahead(heroSpeed, travMaxEdge, TRAV_TIMING));
		ZDebug("Przeskok [Alt]: %s", log.c_str());
		if (zcfg.debug && res.wallDist > 0.0f)
			ZDebug("Przeskok [Alt]: sciana trafiona: %s", TravWallHitName(fwd, res).c_str());
		else if (zcfg.debug)
			ZDebug("Przeskok [Alt]: bez sciany - promienie przed bohaterem: %s",
				TravRayAudit(fwd, zdcore::TravLookahead(heroSpeed, travMaxEdge, TRAV_TIMING)).c_str());
		if (res.kind == zdcore::TRAV_NONE)
			return TravRefuse(res, nullptr, zdcore::TravDescribe(res), zdcore::TravDecisionLabel(res, nullptr), &rays, origin, fwd);
		bool swingLeft = TravSwingLeftNow();
		std::string endLog;
		const zdcore::TravDef* def = TravPickHere(res, fwd, false, zcfg.debug ? &endLog : nullptr, travDbg.on ? &rays : nullptr);
		if (!endLog.empty())
			ZDebug("Przeskok [Alt]: %s", endLog.c_str() + 2);
		if (!def)
			return TravRefuse(res, nullptr, zdcore::TravDescribe(res) + (res.kind == zdcore::TRAV_NONE ? "" :
				std::string(" - brak klipu (noga ") + (swingLeft ? "L" : "R") + ")"), zdcore::TravDecisionLabel(res, nullptr), &rays, origin, fwd);
		zdcore::TravStart st = zdcore::TravPlanStart(res.face, def->edge, heroSpeed, false, TRAV_TIMING);
		if (st == zdcore::TRAVSTART_TOO_FAR || st == zdcore::TRAVSTART_TOO_CLOSE) {
			char detail[400];
			sprintf_s(detail, "%s, %s: %s (sciana %.0f cm, okno startu %.0f-%.0f cm, predkosc %.0f)", zdcore::TravDescribe(res).c_str(),
				def->id.c_str(), TravStartName(st), res.face, def->edge * TRAV_TIMING.kMin, def->edge * TRAV_TIMING.kMax, heroSpeed);
			return TravRefuse(res, def, detail, zdcore::TravDecisionLabel(res, def) + " - " + TravStartName(st), &rays, origin, fwd);
		}
		tp = TravPending();
		tp.active = true;
		tp.yaw0 = atan2f(fwd.n[0], fwd.n[2]);
		tp.face0 = res.face;
		TravRememberProbe(res, fwd);
		float wait = (res.face - def->edge * TRAV_TIMING.kGo) / (heroSpeed > TRAV_MIN_SPEED ? heroSpeed : TRAV_MIN_SPEED);
		ZDebug("Przeskok [Alt]: klawisz przejety - %s, %s (%s; start przy %.0f cm, ok. %.2f s biegu)", zdcore::TravDescribe(res).c_str(),
			def->id.c_str(), TravStartName(st), def->edge * TRAV_TIMING.kGo, wait > 0.0f ? wait : 0.0f);
		TravDebugFreeze(res, def, &rays, origin, fwd, zdcore::TravDecisionLabel(res, def) + " - " + TravStartName(st));
		return true;
	}

	// Podpowiedz (TraversalHint, 21.10 - tester nie wiedzial, jak wywolac przeskok): przy biegu na przeszkode, ktora Alt
	// teraz by przejal, napis z klawiszem skoku; pierwsze TraversalHint razy (licznik TraversalHintShown w Gothic.ini).
	// Badanie co TRAV_HINT_PROBE s (TravProbe - ok. 25-30 promieni), nie czesciej niz co TRAV_HINT_GAP s.
	static const float TRAV_HINT_PROBE = 0.3f;
	static const float TRAV_HINT_GAP = 20.0f;
	static float travHintProbeT = 0.0f;
	static zdcore::TravHint travHint;

	static void TravHintUpdate(float dt) {
		travHint.since += dt;
		travHint.left = zcfg.traversalHint - zcfg.traversalHintShown;
		if (travHint.left <= 0 || travHint.since < TRAV_HINT_GAP)
			return;
		travHintProbeT += dt;
		if (travHintProbeT < TRAV_HINT_PROBE)
			return;
		travHintProbeT = 0.0f;
		int wm = 0;
		const char* why = nullptr;
		if (!TravHeroCanStart(true, wm, why))
			return;
		ZPROF_SCOPE("Traversal.Hint");
		zVEC3 fwd = TravHeading();
		zdcore::TravResult res = TravProbe(fwd);
		const zdcore::TravDef* def = res.kind != zdcore::TRAV_NONE ? TravPickHere(res, fwd, false) : nullptr;
		zdcore::TravStart st = def ? zdcore::TravPlanStart(res.face, def->edge, heroSpeed, false, TRAV_TIMING) : zdcore::TRAVSTART_TOO_FAR;
		if (!zdcore::TravHintDue(travHint, true, def != nullptr, st, TRAV_HINT_GAP))
			return;
		// napis ekranowy w cp1250: e = \xEA
		std::string text = zdcore::DikLabel(TravJumpKey()) + (res.kind == zdcore::TRAV_CLIMB ? " - wespnij si\xEA" : " - przeskocz");
		if (ogame && ogame->GetTextView())
			ogame->GetTextView()->PrintTimedCXY(zSTRING(text.c_str()), 2500.0f, nullptr);
		travHint.since = 0.0f;
		zcfg.traversalHintShown++;
		zoptions->WriteInt("ZDISMEMBER", "TraversalHintShown", zcfg.traversalHintShown, FALSE);
		ZLog("Przeskok: podpowiedz %d/%d (%s, %s, sciana %.0f cm)", zcfg.traversalHintShown, zcfg.traversalHint, def->id.c_str(),
			TravStartName(st), res.face);
	}

	bool TraversalTakesJump() {
		if (ts.def || tp.active)
			return true;
		int key = TravJumpKey();
		bool down = zinput && zinput->KeyPressed(key) != 0;
		if (!down) {
			travLatch = 0;
			return false;
		}
		if (travLatch == 0)
		{
			travAltT = travClockNow();
			travLatch = TravDecide() ? 1 : 2;
		}
		return travLatch == 1;
	}

	// Sciana przed bohaterem (promienie 40 / 90 / 140 cm nad ziemia, do 400 cm) - dobieg do niej; -1 = brak.
	static float TravWallAhead(const zVEC3& fwd) {
		TravGameWorld w;
		if (!TravWorldHere(fwd, w))
			return -1.0f;
		float best = -1.0f;
		for (float h : { 40.0f, 90.0f, 140.0f }) {
			zdcore::TravRay r = w.Forward(0.0f, h, 0.0f, 400.0f);
			if (r.hit && (best < 0.0f || r.dist < best))
				best = r.dist;
		}
		return best;
	}

	// Dobieg: bieg do sciany trwa (gracz trzyma W), przy niej skok z miejsca gry (JumpForward - zawis / wspiecie).
	static void TravLedgeRunUpdate(float dt) {
		if (!tlr.active)
			return;
		tlr.t += dt;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zVEC3 fwd = player ? TravHeading() : zVEC3(0.0f, 0.0f, 1.0f);
		const char* why = nullptr;
		if (!ac || player->IsDead() || ts.def || !player->GetModel())
			why = "bohater zajety";
		else if (ac->state != zCAIPlayer::zMV_STATE_STAND || player->physicsEnabled)
			why = "w powietrzu";
		else if (fabsf(zdcore::AngleDiff(tlr.yaw0, atan2f(fwd.n[0], fwd.n[2]))) > TRAV_TIMING.maxTurn)
			why = "skret";
		else if (tlr.t > TRAV_LEDGE_RUN_MAX)
			why = "za dlugo";
		float face = why ? -1.0f : TravWallAhead(fwd);
		if (!why && face < 0.0f)
			why = "sciana zniknela";
		bool forward = MvRawState(GAME_UP) && !MvRawState(GAME_DOWN);
		if (!why && !forward && face > 120.0f)
			why = "puszczone W";
		if (why) {
			ZDebug("Przeskok [dobieg]: przerwany po %.2f s (%s, sciana przy wcisnieciu %.0f cm) - bez skoku", tlr.t, why, tlr.face0);
			tlr = TravLedgeRun();
			return;
		}
		tlr.face = face;
		// przy scianie albo zatrzymany przed nia (kolizja, puszczone W)
		if (face <= TRAV_LEDGE_REACH || (!forward && face <= 120.0f) || (heroSpeed < 60.0f && face <= 150.0f && tlr.t > 0.3f)) {
			ZDebug("Przeskok [dobieg]: po %.2f s sciana %.0f cm (przy wcisnieciu %.0f) - skok z miejsca", tlr.t, face, tlr.face0);
			tlr = TravLedgeRun();
			TravLedgeJump("dobieg");
		}
	}

	// ---------- 18.10 (log NB): nasze korekty pozycji bohatera a przestawienia z zewnatrz (Core HeroGuard) ----------
	// zTimeRewind w trakcie cofania co klatke stawia bohatera w pozycji z nagrania (SetPositionWorld z DIFF_02, model
	// zamrozony timeScale 0, klatki animacji ustawiane recznie - ruch korzenia przewijanych klatek ma metry), a nasz
	// straznik lotu / skoku korzenia w tej samej klatce cofal go z powrotem (ping-pong 130-240 cm co klatke). Teraz:
	// kazde ustawienie pozycji bohatera (haki SetPositionWorld / SetTrafoObjToWorld / Move) jest klasyfikowane po
	// wywolujacym (nasz DLL / obcy DLL / funkcja teleportu silnika: ResetPos, BeamTo, Enable, konsola, mob) - przez
	// TRAV_EXT_WINDOW po przestawieniu z zewnatrz i przy zamrozonym modelu korekty nie dzialaja (log: "Korekta pozycji
	// pominieta ..."). Wpisy "Przesuniecie bohatera" (Debug=1) podaja zrodlo: nasza korekta (powod) / zewnetrzne / silnik.
	static float travClock = 0.0f;                    // s petli pluginu (TraversalUpdate)
	static float travClockNow() {
		return travClock;
	}
	// 19.10: kolejny ruch tuz po koncu poprzedniego tylko z nowym Alt (po koncu) albo z Alt w oknie wyjscia - true = blokada
	static bool TravChainNow(const char* what) {
		bool fresh = travChainAlt || travAltT > travLastEndT;
		if (!zdcore::TravChainBlocked(travClock - travLastEndT, fresh, TRAV_CHAIN_MIN))
			return false;
		static float loggedFor = -1e9f;
		if (loggedFor != travLastEndT) {
			loggedFor = travLastEndT;
			ZDebug("Nastepny ruch zablokowany - brak nowego Alt (%s %.2f s po koncu poprzedniego, trzymane W)", what,
				travClock - travLastEndT);
		}
		return true;
	}
	static zdcore::HeroExtTracker travExt;             // ostatnie przestawienie bohatera z zewnatrz
	static const float TRAV_EXT_WINDOW = 0.35f;        // s po przestawieniu z zewnatrz bez naszych korekt
	static const float TRAV_EXT_MIN = 30.0f;           // cm - mniejsze ustawienia (dosuwanie, kroki) bez klasyfikacji
	static const zdcore::CollTeleportParams TRAV_COLL_TELEPORT;   // teleport kolizji (haki EndMovement, ped skoku)
	static int travGuardSkips = 0, travGuardSkipLogs = 0;   // korekty pominiete (zewnetrzne przestawienie / zamrozony model)
	static const char* travSuspendLast = nullptr;      // log przejsc wstrzymania
	static float travSuspendT0 = 0.0f;
	static int travSuspendSkips0 = 0;
	static int travExtLogs = 0;

	static HMODULE TravHgModule(const void* a) {
		HMODULE h = nullptr;
		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a,
			&h))
			return nullptr;
		return h;
	}

	static HMODULE TravHgOurs() {
		static HMODULE h = TravHgModule((const void*)&TravHgOurs);
		return h;
	}

	// Modul ramki: exe / nasz DLL / obcy DLL; biblioteki systemowe i Union (SHW32) - systemowe (nie ustawiaja pozycji).
	static int TravHgModKind(const void* a) {
		HMODULE h = TravHgModule(a);
		if (!h)
			return zdcore::HG_MOD_SYSTEM;
		if (h == GetModuleHandleA(nullptr))
			return zdcore::HG_MOD_EXE;
		if (h == TravHgOurs())
			return zdcore::HG_MOD_OURS;
		char path[MAX_PATH] = "";
		GetModuleFileNameA(h, path, MAX_PATH);
		const char* b = strrchr(path, '\\');
		std::string n = zdcore::Upper(b ? b + 1 : path);
		static const char* sys[] = { "NTDLL.DLL", "KERNEL32.DLL", "KERNELBASE.DLL", "MSVCR100.DLL", "MSVCP100.DLL",
			"UCRTBASE.DLL", "VCRUNTIME140.DLL", "SHW32.DLL" };
		for (const char* s : sys)
			if (n == s)
				return zdcore::HG_MOD_SYSTEM;
		return zdcore::HG_MOD_FOREIGN;
	}

	// Funkcja teleportu silnika (bezposredni wywolujacy w exe) - nazwa albo nullptr.
	static const char* TravHgExeTeleport(const void* a) {
		struct R { uintptr_t lo, hi; const char* name; };
		static const R r[] = { { 0x006824D0, 0x006827D0, "oCNpc::ResetPos" }, { 0x006827D0, 0x00682B40, "oCNpc::EmergencyResetPos" },
			{ 0x00736EE0, 0x00737350, "oCNpc::BeamTo" }, { 0x00745D40, 0x00746400, "oCNpc::Enable" },
			{ 0x006CC0A0, 0x006CFE50, "oCGame::ConsoleEvalFunc" }, { 0x0071E240, 0x0071E490, "oCMobInter::SetIdealPosition" } };
		uintptr_t x = (uintptr_t)a;
		for (const R& e : r)
			if (x >= e.lo && x < e.hi)
				return e.name;
		return nullptr;
	}

	static std::string TravAddrDesc(void* a);

	// Ustawienie pozycji bohatera (z haka, przed oryginalem): klasyfikacja wywolujacego, zapis przestawienia z zewnatrz.
	// kindOut: HG_SET_*; desc: kto (log).
	static int TravHgClassify(void* ret, std::string* desc) {
		void* fr[10];
		USHORT n = RtlCaptureStackBackTrace(0, 10, fr, nullptr);
		int at = -1;
		for (USHORT i = 0; i < n; i++)
			if (fr[i] == ret) {
				at = i;
				break;
			}
		int kinds[6];
		int k = 0;
		kinds[k++] = TravHgModKind(ret);
		for (int i = at >= 0 ? at + 1 : n; i < n && k < 6; i++)
			kinds[k++] = TravHgModKind(fr[i]);
		const char* tele = kinds[0] == zdcore::HG_MOD_EXE ? TravHgExeTeleport(ret) : nullptr;
		int kind = zdcore::ClassifyHeroSet(kinds, k, tele != nullptr);
		if (desc)
			*desc = TravAddrDesc(ret) + (tele ? std::string(" (") + tele + ")" : std::string());
		return kind;
	}

	static void TravHgNoteSet(const zVEC3& from, const zVEC3& to, void* ret) {
		float d = sqrtf((to.n[0] - from.n[0]) * (to.n[0] - from.n[0]) + (to.n[1] - from.n[1]) * (to.n[1] - from.n[1]) +
			(to.n[2] - from.n[2]) * (to.n[2] - from.n[2]));
		if (d < TRAV_EXT_MIN || travOwnWhy)
			return;
		std::string who;
		if (TravHgClassify(ret, &who) != zdcore::HG_SET_EXTERNAL)
			return;
		zdcore::HeroExtNote(travExt, travClock, who);
		if (zcfg.debug && travExtLogs < 40) {
			travExtLogs++;
			ZDebug("Przestawienie bohatera z zewnatrz %.0f cm: %s - nasze korekty pozycji wstrzymane na %.2f s (%d.)", d, who.c_str(),
				TRAV_EXT_WINDOW, travExt.count);
		}
	}

	// Powod wstrzymania naszych korekt w tej chwili (zamrozony model / przestawienie z zewnatrz) albo nullptr.
	static const char* TravGuardSuspended() {
		zCModel* m = player ? player->GetModel() : nullptr;
		bool frozen = m && zdcore::HeroModelFrozen(m->timeScale);
		return zdcore::HeroGuardSuspendWhy(frozen, zdcore::HeroExtRecent(travExt, travClock, TRAV_EXT_WINDOW));
	}

	// Log przejsc wstrzymania (raz na wejscie / wyjscie) - wolane co klatke z TraversalUpdate.
	static void TravGuardSuspendTrack() {
		const char* why = TravGuardSuspended();
		if (why == travSuspendLast)
			return;
		bool ext = why && strncmp(why, "zewn", 4) == 0;
		if (why && !travSuspendLast) {
			travSuspendT0 = travClock;
			travSuspendSkips0 = travGuardSkips;
			ZDebug("Korekty pozycji wstrzymane: %s (%s)", why, ext ? ("modul " + travExt.who).c_str() : "timeScale 0");
		}
		else if (!why)
			ZDebug("Korekty pozycji wznowione po %.2f s (pominiete poprawki: %d)", travClock - travSuspendT0,
				travGuardSkips - travSuspendSkips0);
		travSuspendLast = why;
	}

	// Korekta, ktora by zadzialala, pominieta (wstrzymanie) - licznik i log (pierwsze 40 na sesje).
	static void TravGuardSkip(const char* kind, float cm, const char* why) {
		travGuardSkips++;
		if (travGuardSkipLogs >= 40)
			return;
		travGuardSkipLogs++;
		bool ext = why && strncmp(why, "zewn", 4) == 0;
		ZDebug("Korekta pozycji pominieta - %s (%s): %s %.0f cm (%d.)", why ? why : "?",
			ext ? ("modul " + travExt.who).c_str() : "timeScale 0", kind, cm, travGuardSkips);
	}

	// 18.10: kolizja bohatera wlaczona w trakcie naszego ruchu (log NB 47297: CLIMB_IDLE_100 z kolizja 1/1 - kolizja
	// wypchnela bohatera 87 cm do tylu z toru, potem przerwanie "przeszkoda na torze"). Kto wlaczyl: haki SetCollDetStat /
	// SetCollDetDyn (pierwszy wywolujacy w ruchu); TraversalUpdate wylacza ja ponownie.
	static void TravCdOnNote(const char* fn, int on, zCVob* self, void* ret) {
		if (on && player && self == player && ts.def && ts.cdOff && !ts.runExit && !travOwnWhy && travCdOnBy.empty())
			travCdOnBy = std::string(fn) + " z " + TravAddrDesc(ret);
	}
	static void __fastcall TravHkSetCollDetStat(zCVob* self, int on);
	static CInvoke<void(__fastcall*)(zCVob*, int)> Ivk_zCVob_SetCollDetStat_Trav(0x0061CE50, &TravHkSetCollDetStat,
		IvkEnabled(ENGINE));
	static void __fastcall TravHkSetCollDetStat(zCVob* self, int on) {
		TravCdOnNote("SetCollDetStat", on, self, _ReturnAddress());
		Ivk_zCVob_SetCollDetStat_Trav(self, on);
	}
	static void __fastcall TravHkSetCollDetDyn(zCVob* self, int on);
	static CInvoke<void(__fastcall*)(zCVob*, int)> Ivk_zCVob_SetCollDetDyn_Trav(0x0061CF40, &TravHkSetCollDetDyn,
		IvkEnabled(ENGINE));
	static void __fastcall TravHkSetCollDetDyn(zCVob* self, int on) {
		TravCdOnNote("SetCollDetDyn", on, self, _ReturnAddress());
		Ivk_zCVob_SetCollDetDyn_Trav(self, on);
	}

	// Klawisze ruchu gry (ukrywane przed gra w trakcie animacji przeskoku - Core TravMaskMoveKey).
	static bool TravIsMoveKey(unsigned short k) {
		return k == GAME_UP || k == GAME_DOWN || k == GAME_LEFT || k == GAME_RIGHT || k == GAME_STRAFELEFT || k == GAME_STRAFERIGHT ||
			k == GAME_SMOVE || k == GAME_SMOVE2 || k == GAME_SLOW || k == GAME_SNEAK;
	}

	// ---------- Lot po skoku gry / spadanie: straznik cofniecia, odbicie fizyki, ped skoku (JumpKeepMomentum) ----------
	// Log 16.10: start skoku gry w biegu albo spadania potrafi w jednej klatce cofnac bohatera o 1,3-2 m (pozycja sprzed ok.
	// 0,27-0,4 s), a fizyka (zCRigidBody) po zderzeniu w locie odbija go do tylu (v -151 cm/s, 2,5 m wstecz). Poprawki:
	// krok w tyl > 40 cm w jednej klatce w pierwszych 0,5 s lotu - pozycja sprzed klatki + poprzedni krok; predkosc ciala
	// sztywnego w tyl (wzgledem kierunku biegu przy odbiciu) przycinana do 50 cm/s; dryf w tyl > 60 cm w 0,3 s - pozycja
	// najdalej z przodu. Ped: animacja T_RUNL_2_JUMP niesie ok. 370 cm/s - brakujaca czesc do predkosci odbicia dosuwana
	// (faza animacji: przesuniecie z kolizja promieniami; faza fizyki: predkosc ciala sztywnego, tylko brak ponad
	// predkosc nadana przez silnik / wtyczke JumpAcrobaticsNoFps - bez dublowania), po ladowaniu wygasa (0,2 s).
	static const zdcore::TravJumpGuardParams TRAV_JUMP_GUARD;
	static const zdcore::TravFlightClampParams TRAV_FLIGHT_CLAMP;   // krok w locie najwyzej limit pedu (runda 9)
	static void TravTeleportHero(const zVEC3& to, const char* why);

	// Czas klatki silnika bez przyciecia petli (LoopUpdate tnie dt do 0,1 s, przycinki maja 130+ ms) - limity ruchu
	// w klatce (krok lotu, skok korzenia) wg prawdziwego czasu.
	static float TravFrameTime(float dt) {
		float r = ztimer ? ztimer->frameTimeFloat / 1000.0f : dt;
		return r > dt ? r : dt;
	}
	static const float TRAV_BOUNCE_MAX_BACK = 50.0f;     // cm/s w tyl w locie najwyzej
	static const float TRAV_DRIFT_WINDOW = 0.3f;         // s
	static const float TRAV_DRIFT_MAX = 60.0f;           // cm w tyl w oknie - korekta
	static const float TRAV_BODY_RADIUS = 35.0f;         // ped: promien ciala przy sprawdzaniu drogi
	struct TravFlight {
		bool have = false;
		zVEC3 prev;                   // pozycja z poprzedniej klatki (po naszych poprawkach)
		float lastAlong = 0.0f;       // krok wzdluz biegu w poprzedniej klatce
		zVEC3 vel;                    // pozioma predkosc z ostatniej klatki (cm/s)
		float airT = -1.0f;           // s od startu lotu (-1 = na ziemi)
		zVEC3 fwd, origin;            // kierunek biegu i pozycja przy odbiciu
		std::string start;            // animacja przy starcie lotu
		float bestAlong = 0.0f, bestT = 0.0f;
		zVEC3 best;                   // najdalej do przodu w locie
		int fixes = 0, bounceLogs = 0;
		bool momentum = false;        // ped skoku dziala
		float v0 = 0.0f, landedT = -1.0f, added = 0.0f;
		// 18.10: poprawki tylko w skoku z biegu (start T_*RUN*_2_JUMP); poprawka cofnieta przez silnik w nastepnej
		// klatce (ping-pong) = koniec poprawek w tym locie
		bool runJump = false, gaveUp = false;
		bool fixPending = false;      // poprawka z poprzedniej klatki do sprawdzenia
		zVEC3 fixFrom, fixTo;         // pozycja silnika przed poprawka -> nasza
		bool animLogged = false, physLogged = false, fadeLogged = false;   // 24.10: log pedu raz na lot
	};
	static TravFlight tfl;
	static const zdcore::TravMomentumFadeParams TRAV_MOMENTUM_FADE;   // 24.10: ped tylko przez fizyke, wygaszany przy przeszkodzie

	// 24.10: straznik oscylacji (Core HeroOscDecide) - kazda nasza korekta pozycji bohatera (cofniecie teleportu kolizji,
	// skok korzenia / pozycji, poprawki lotu): najwyzej jedna na klatke (zdFrame), ta sama korekta tam i z powrotem w
	// kolejnych klatkach = oscylacja - korekty wstrzymane, zostaje pozycja silnika. Log kazdego przypadku.
	static zdcore::HeroOscState travOsc;
	static const zdcore::HeroOscParams TRAV_OSC;
	static int travOscLogs = 0;
	static bool TravCorrectOk(const zVEC3& from, const zVEC3& to, const char* why) {
		float a[3] = { from.n[0], from.n[1], from.n[2] }, b[3] = { to.n[0], to.n[1], to.n[2] };
		zdcore::HeroOscVerdict v = zdcore::HeroOscDecide(travOsc, zdFrame, a, b, TRAV_OSC);
		if (v == zdcore::HOSC_APPLY)
			return true;
		if (v == zdcore::HOSC_OSCILLATION) {
			ZLog("Straznik: oscylacja (%d.) - ta sama korekta %.0f cm w %d kolejnych klatkach (%s): poz silnika (%.0f, %.0f, %.0f), "
				"nasza (%.0f, %.0f, %.0f) - korekty zatrzymane na %u klatek, zostaje pozycja silnika (kl. %u)", travOsc.trips,
				TravLen(to - from), TRAV_OSC.maxRepeats + 1, why, from.n[0], from.n[1], from.n[2], to.n[0], to.n[1], to.n[2],
				TRAV_OSC.holdFrames, zdFrame);
		}
		else if (travOscLogs < 40) {
			travOscLogs++;
			ZDebug("Straznik: korekta %.0f cm (%s) %s (kl. %u)", TravLen(to - from), why, zdcore::HeroOscVerdictName(v), zdFrame);
		}
		return false;
	}

	static void TravFlightFixNote(const zVEC3& from, const zVEC3& to) {
		tfl.fixPending = true;
		tfl.fixFrom = from;
		tfl.fixTo = to;
	}
	// predkosc na ziemi z ostatnich 0,25 s (ped przy odbiciu - w klatce startu skoku predkosc juz spada)
	static float travSpeedHist[16], travSpeedAge[16];
	static int travSpeedPos = 0;

	// Skok gry z biegu: T_RUNL_2_JUMP / T_RUNR_2_JUMP, z bronia T_1HRUNL_2_JUMP itd. (zEBA).
	static bool TravIsRunJumpName(const char* n) {
		std::string u = zdcore::Upper(n);
		auto ends = [&](const char* s) {
			size_t l = strlen(s);
			return u.size() >= l && u.compare(u.size() - l, l, s) == 0;
		};
		return u.compare(0, 2, "T_") == 0 && (ends("RUNL_2_JUMP") || ends("RUNR_2_JUMP"));
	}

	// Lot po odbiciu: S_JUMP (z bronia S_1HJUMP itd.), spadanie.
	static bool TravIsFlightName(const char* n) {
		std::string u = zdcore::Upper(n);
		return (u.compare(0, 2, "S_") == 0 && u.size() >= 6 && u.compare(u.size() - 4, 4, "JUMP") == 0) || u == "S_FALL" ||
			u == "S_FALLDN" || u == "T_JUMP_2_FALLDN";
	}

	static const char* TravRunJumpAni(zCModel* m) {
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1 || a->isFadingOut)
				continue;
			const char* n = p->aniName.ToChar();
			if (TravIsRunJumpName(n))
				return n;
		}
		return nullptr;
	}

	// Animacja lotu warstwy 1 (nazwa) albo nullptr; hang = zawis / skok do gory silnika (lot obsluguje silnik).
	static const char* TravFlightAni(zCModel* m, bool& hang) {
		hang = false;
		const char* fly = nullptr;
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1)
				continue;
			const char* n = p->aniName.ToChar();
			std::string u = zdcore::Upper(n);
			if (u.find("HANG") != std::string::npos || u.find("JUMPUP") != std::string::npos)
				hang = true;
			if (!a->isFadingOut && (TravIsRunJumpName(n) || TravIsFlightName(n)))
				fly = n;
		}
		return fly;
	}

	// Droga do przodu wolna (promienie ze srodka postaci: biodra, pas, piersi) - ile z want cm mozna przesunac.
	static float TravClearAhead(const zVEC3& cur, const zVEC3& fwd, float want) {
		float allowed = want;
		for (float dy : { -45.0f, 0.0f, 50.0f }) {
			zVEC3 from(cur.n[0], cur.n[1] + dy, cur.n[2]);
			float len = want + TRAV_BODY_RADIUS;
			if (MvTrace(from, zVEC3(fwd.n[0] * len, 0.0f, fwd.n[2] * len), player, nullptr, MOVE_TRACE)) {
				float d = MvFlatDist(from, ogame->GetGameWorld()->traceRayReport.foundIntersection) - TRAV_BODY_RADIUS;
				allowed = d < allowed ? d : allowed;
			}
		}
		return allowed > 0.0f ? allowed : 0.0f;
	}

	static float TravRecentSpeed() {
		float best = 0.0f;
		for (int i = 0; i < 16; i++)
			if (travSpeedAge[i] <= 0.25f && travSpeedHist[i] > best)
				best = travSpeedHist[i];
		return best;
	}

	// Koniec pedu w trakcie lotu (lot trwa dalej - bez ponownego startu pedu w tym skoku).
	static void TravMomentumStop(const char* why) {
		if (tfl.momentum && tfl.added > 1.0f)
			ZDebug("Skok gry: ped - dodano ok. %.0f cm ponad gre (odbicie %.0f cm/s, %.2f s, %s)", tfl.added, tfl.v0, tfl.airT, why);
		else if (tfl.momentum)
			ZDebug("Skok gry: ped - koniec bez dodatku (odbicie %.0f cm/s, %.2f s, %s)", tfl.v0, tfl.airT, why);
		tfl.momentum = false;
		tfl.added = 0.0f;
	}

	// Koniec lotu (ladowanie, zawis, nasz ruch).
	static void TravFlightEnd(const char* why) {
		TravMomentumStop(why);
		tfl.airT = -1.0f;
		tfl.landedT = -1.0f;
	}

	static void TravSetRbVel(zCRigidBody* rb, const zVEC3& v) {
		// bezposrednio (jak zCRigidBody::SetVelocity: v i ped) - bez hakow innych wtyczek na SetVelocity
		rb->v = v;
		rb->PLinMom = zVEC3(v.n[0] * rb->mass, v.n[1] * rb->mass, v.n[2] * rb->mass);
	}

	static void TravFlightUpdate(float dt) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!ac || !m || player->IsDead() || dt <= 0.0f) {
			tfl = TravFlight();
			return;
		}
		for (int i = 0; i < 16; i++)
			travSpeedAge[i] += dt;
		zVEC3 cur = player->GetPositionWorld();
		bool hang = false;
		const char* flyAni = TravFlightAni(m, hang);
		const char* jumpAni = TravRunJumpAni(m);
		bool ours = ts.def || ms.def || DirectorActive();
		bool flying = !ours && !hang && flyAni && (jumpAni || ac->state == zCAIPlayer::zMV_STATE_FLY || player->physicsEnabled);
		if (!flying && !ours && ac->state == zCAIPlayer::zMV_STATE_STAND) {
			travSpeedHist[travSpeedPos] = heroSpeed;
			travSpeedAge[travSpeedPos] = 0.0f;
			travSpeedPos = (travSpeedPos + 1) % 16;
		}
		if (flying && tfl.airT < 0.0f) {
			// start lotu: kierunek biegu, ped z predkosci odbicia (tylko skok z biegu)
			tfl.airT = 0.0f;
			// kierunek lotu: ruch z ostatniej klatki (zejscie tylem z krawedzi - w tyl), na postoju - przod postaci
			float vl = sqrtf(tfl.vel.n[0] * tfl.vel.n[0] + tfl.vel.n[2] * tfl.vel.n[2]);
			tfl.fwd = tfl.have && vl > 100.0f ? zVEC3(tfl.vel.n[0] / vl, 0.0f, tfl.vel.n[2] / vl) : TravHeading();
			tfl.origin = tfl.have ? tfl.prev : cur;
			tfl.start = flyAni;
			tfl.bestAlong = 0.0f;
			tfl.bestT = 0.0f;
			tfl.best = tfl.origin;
			tfl.bounceLogs = 0;
			tfl.landedT = -1.0f;
			tfl.added = 0.0f;
			tfl.v0 = TravRecentSpeed();
			tfl.runJump = jumpAni != nullptr;
			tfl.gaveUp = false;
			tfl.fixPending = false;
			tfl.animLogged = tfl.physLogged = tfl.fadeLogged = false;
			tfl.momentum = zcfg.jumpKeepMomentum && jumpAni && !TravGuardSuspended() &&
				zdcore::TravMomentumTarget(tfl.v0, travJumpAnimSpeed, -1.0f, TRAV_MOMENTUM) > 0.0f;
			if (tfl.momentum)
				ZDebug("Skok gry: ped %.0f cm/s (animacja %.0f, %s, bron %d)", tfl.v0 < TRAV_MOMENTUM.maxSpeed ? tfl.v0 :
					TRAV_MOMENTUM.maxSpeed, travJumpAnimSpeed, jumpAni, player->GetWeaponMode());
		}
		else if (flying)
			tfl.airT += dt;
		else if (tfl.airT >= 0.0f) {
			// koniec lotu: zawis / nasz ruch - od razu; ladowanie - ped wygasa (landFade)
			if (hang || ours || !tfl.momentum)
				TravFlightEnd(hang ? "zawis" : ours ? "ruch" : "ladowanie");
			else {
				tfl.landedT = tfl.landedT < 0.0f ? 0.0f : tfl.landedT + dt;
				if (tfl.landedT >= TRAV_MOMENTUM.landFade)
					TravFlightEnd("ladowanie");
			}
		}
		const zVEC3 fwd = tfl.fwd;
		// 18.10: wstrzymanie (cofanie czasu, teleport z zewnatrz) - bez poprawek i pedu, punkt odniesienia od nowa
		const char* susp = TravGuardSuspended();
		if (tfl.fixPending) {
			// poprawka z poprzedniej klatki cofnieta (silnik / kolizja wrocily do swojej pozycji) - koniec poprawek w locie
			tfl.fixPending = false;
			float a[3] = { tfl.fixFrom.n[0], tfl.fixFrom.n[1], tfl.fixFrom.n[2] }, b[3] = { tfl.fixTo.n[0], tfl.fixTo.n[1], tfl.fixTo.n[2] },
				c[3] = { cur.n[0], cur.n[1], cur.n[2] };
			if (!susp && tfl.airT >= 0.0f && zdcore::TravFixUndone(a, b, c)) {
				tfl.gaveUp = true;
				if (tfl.momentum)
					TravMomentumStop("poprawka cofnieta");
				ZDebug("Skok gry: poprawka %.0f cm cofnieta w nastepnej klatce (silnik / kolizja wraca do swojej pozycji, %.2f s lotu, %s) "
					"- bez dalszych poprawek w tym skoku", TravLen(tfl.fixTo - tfl.fixFrom), tfl.airT, TravLayerOne(m).c_str());
			}
		}
		if (susp && tfl.momentum)
			TravMomentumStop("wstrzymane");
		if (tfl.have && tfl.airT >= 0.0f) {
			zVEC3 d = cur - tfl.prev;
			float along = d.n[0] * fwd.n[0] + d.n[2] * fwd.n[2];
			bool allowed = zdcore::TravFlightFixAllowed(tfl.runJump, susp != nullptr, tfl.gaveUp);
			// 1. cofniecie w jednej klatce na starcie lotu - pozycja z poprzedniej klatki + zwykly krok; krok do przodu
			// ponad limit pedu (powrot silnika po cofnieciu, poprawka i powrot = krok na kroku) - przyciety (runda 9)
			int fixed = 0;
			float maxStep = zdcore::TravFlightMaxStep(TRAV_MOMENTUM.maxSpeed, TravFrameTime(dt), TRAV_FLIGHT_CLAMP);
			float step = flying ? zdcore::TravFlightGuardStep(along, tfl.lastAlong, tfl.airT, maxStep, TRAV_JUMP_GUARD, fixed) : along;
			if (fixed && !allowed) {
				if (susp)
					TravGuardSkip(fixed == 1 ? "cofniecie w locie" : "krok w locie ponad limit", fabsf(step - along), susp);
				fixed = 0;
				step = along;
			}
			if (fixed && !TravCorrectOk(cur, zVEC3(cur.n[0] + fwd.n[0] * (step - along), cur.n[1], cur.n[2] + fwd.n[2] * (step - along)),
				fixed == 1 ? "cofniecie w locie" : "krok w locie ponad limit pedu")) {
				fixed = 0;
				step = along;
			}
			if (fixed) {
				// tylko skladowa wzdluz lotu (bok i wysokosc jak silnik)
				zVEC3 fix(cur.n[0] + fwd.n[0] * (step - along), cur.n[1], cur.n[2] + fwd.n[2] * (step - along));
				TravTeleportHero(fix, fixed == 1 ? "korekta: cofniecie w locie" : "korekta: krok w locie ponad limit pedu");
				TravFlightFixNote(cur, fix);
				tfl.fixes++;
				if (fixed == 1)
					ZDebug("Skok gry: cofniecie o %.0f cm w klatce (%.2f s po starcie %s, ok. %.2f s biegu, nad ziemia %.0f, fizyka %d) - "
						"poprawione (%d.)", -along, tfl.airT, tfl.start.c_str(), heroSpeed > 1.0f ? -along / heroSpeed : 0.0f,
						ac->aboveFloor, player->physicsEnabled ? 1 : 0, tfl.fixes);
				else
					ZDebug("Skok gry: krok %.0f cm w klatce ponad limit pedu (%.0f cm/s: %.0f cm, %.2f s lotu, %s) - przyciety (%d.)",
						along, TRAV_MOMENTUM.maxSpeed, maxStep, tfl.airT, TravLayerOne(m).c_str(), tfl.fixes);
				cur = fix;
				along = step;
				zCRigidBody* rbc = fixed == 2 && player->physicsEnabled ? player->GetRigidBody() : nullptr;
				if (rbc) {
					float va = rbc->v.n[0] * fwd.n[0] + rbc->v.n[2] * fwd.n[2];
					if (va > TRAV_MOMENTUM.maxSpeed) {
						float cut = va - TRAV_MOMENTUM.maxSpeed;
						TravSetRbVel(rbc, zVEC3(rbc->v.n[0] - fwd.n[0] * cut, rbc->v.n[1], rbc->v.n[2] - fwd.n[2] * cut));
					}
				}
			}
			// 2. odbicie fizyki: predkosc w tyl przycieta (boczna i pionowa zostaja) - kazdy lot bez wstrzymania
			zCRigidBody* rb = player->physicsEnabled ? player->GetRigidBody() : nullptr;
			if (flying && rb && !susp && !tfl.gaveUp) {
				float va = rb->v.n[0] * fwd.n[0] + rb->v.n[2] * fwd.n[2];
				if (va < -TRAV_BOUNCE_MAX_BACK) {
					float cut = -TRAV_BOUNCE_MAX_BACK - va;
					TravSetRbVel(rb, zVEC3(rb->v.n[0] + fwd.n[0] * cut, rb->v.n[1], rb->v.n[2] + fwd.n[2] * cut));
					if (tfl.bounceLogs++ < 2)
						ZDebug("Skok gry: odbicie fizyki - predkosc w tyl %.0f cm/s przycieta do %.0f (%.2f s lotu, %s)", -va,
							TRAV_BOUNCE_MAX_BACK, tfl.airT, TravLayerOne(m).c_str());
				}
			}
			// 3. dryf w tyl w locie (kilka klatek) - pozycja najdalej z przodu
			float cAlong = (cur.n[0] - tfl.origin.n[0]) * fwd.n[0] + (cur.n[2] - tfl.origin.n[2]) * fwd.n[2];
			bool drift = flying && cAlong < tfl.bestAlong - TRAV_DRIFT_MAX && tfl.airT - tfl.bestT <= TRAV_DRIFT_WINDOW;
			if (drift && !allowed) {
				if (susp)
					TravGuardSkip("dryf w tyl w locie", tfl.bestAlong - cAlong, susp);
				drift = false;
				// punkt odniesienia od biezacej pozycji (po przestawieniu / bez poprawek - bez wracania do starego "najdalej")
				tfl.bestAlong = cAlong;
				tfl.bestT = tfl.airT;
				tfl.best = cur;
			}
			if (drift && !TravCorrectOk(cur, zVEC3(tfl.best.n[0], cur.n[1], tfl.best.n[2]), "dryf w tyl w locie")) {
				drift = false;
				tfl.bestAlong = cAlong;
				tfl.bestT = tfl.airT;
				tfl.best = cur;
			}
			if (drift) {
				zVEC3 fix(tfl.best.n[0], cur.n[1], tfl.best.n[2]);
				TravTeleportHero(fix, "korekta: dryf w tyl w locie");
				TravFlightFixNote(cur, fix);
				if (rb) {
					float va = rb->v.n[0] * fwd.n[0] + rb->v.n[2] * fwd.n[2];
					if (va < 0.0f)
						TravSetRbVel(rb, zVEC3(rb->v.n[0] - fwd.n[0] * va, rb->v.n[1], rb->v.n[2] - fwd.n[2] * va));
				}
				tfl.fixes++;
				ZDebug("Skok gry: odbicie fizyki %.0f cm w tyl w %.2f s - poprawione (%.2f s lotu, %d.)", tfl.bestAlong - cAlong,
					tfl.airT - tfl.bestT, tfl.airT, tfl.fixes);
				cur = fix;
				cAlong = tfl.bestAlong;
			}
			if (cAlong >= tfl.bestAlong) {
				tfl.bestAlong = cAlong;
				tfl.bestT = tfl.airT;
				tfl.best = cur;
			}
			// 4. ped skoku - 24.10: tylko predkosc fizyki silnika (zadnego przestawiania pozycji w locie). Log NB 49252:
			// dosuwanie pozycji z kolizja w fazie animacji przy stopniu przed bohaterem - kolizja przenosila go o 351-364 cm
			// do tylu, cofniecie, koniec pedu zerowal lot i ped startowal od nowa w nastepnej klatce (ping-pong ok. 3,5 m).
			float target = tfl.momentum ? zdcore::TravMomentumTarget(tfl.v0, travJumpAnimSpeed, tfl.landedT, TRAV_MOMENTUM) : 0.0f;
			if (target > 0.0f) {
				if (tfl.airT > 0.15f && tfl.landedT < 0.0f && along < 0.2f * target * dt && tfl.added > 0.0f) {
					TravMomentumStop("droga zablokowana");   // sciana w locie - bez pchania w nia
				}
				else if (rb && flying) {
					float va = rb->v.n[0] * fwd.n[0] + rb->v.n[2] * fwd.n[2];
					float clear = TravClearAhead(cur, fwd, target * TRAV_MOMENTUM_FADE.lookTime);
					float fade = zdcore::TravMomentumFade(clear, target, TRAV_MOMENTUM_FADE);
					float nv = zdcore::TravMomentumPhysicsVel(target, travJumpAnimSpeed, va, fade);
					if (fabsf(nv - va) > 1.0f) {
						TravSetRbVel(rb, zVEC3(rb->v.n[0] + fwd.n[0] * (nv - va), rb->v.n[1], rb->v.n[2] + fwd.n[2] * (nv - va)));
						if (!tfl.physLogged || (fade < 1.0f && !tfl.fadeLogged)) {
							tfl.physLogged = true;
							tfl.fadeLogged = fade < 1.0f;
							ZDebug("Skok gry: ped przez fizyke %.0f -> %.0f cm/s (cel %.0f, gra %.0f, wolna droga %.0f cm, wygaszenie %.2f, "
								"%.2f s lotu, %s)", va, nv, target, travJumpAnimSpeed, clear, fade, tfl.airT, TravLayerOne(m).c_str());
						}
					}
					float over = nv - (travJumpAnimSpeed < target ? travJumpAnimSpeed : target);
					if (over > 0.0f)
						tfl.added += over * dt;
				}
				else if (flying && !tfl.animLogged) {
					// faza animacji (T_*RUN*_2_JUMP bez fizyki): ruch korzenia gry, ped dopiero od fazy fizyki
					tfl.animLogged = true;
					ZDebug("Skok gry: ped - faza animacji bez dosuwania pozycji (krok gry %.0f cm/s, cel %.0f cm/s od fazy fizyki, %s)",
						dt > 0.0f ? along / dt : 0.0f, target, TravLayerOne(m).c_str());
				}
			}
			tfl.lastAlong = along;
		}
		if (tfl.have)
			tfl.vel = zVEC3((cur.n[0] - tfl.prev.n[0]) / dt, 0.0f, (cur.n[2] - tfl.prev.n[2]) / dt);
		tfl.prev = cur;
		tfl.have = true;
	}

	// ---------- Runda 9: skok korzenia animacji w klatce (cofniecia 2-4 m, teleport po wspieciu). Silnik w kazdej
	// klatce (zCVob::DoFrameActivity 0x602C60: BeginMovement, UpdatePhysics, AdvanceAnis, MoveLocal(vobTrans), AI,
	// EndMovement) przesuwa voba o ruch korzenia animacji: zCModel::vobTrans (0x150) = suma wag * (thisPos - lastPos)
	// animacji (AdvanceAnis 0x57CA90 / CalcTransBlending 0x57F8E0). Gdy klatki korzenia animacji sa niespojne (start /
	// przerwanie petli biegu, zmiana nakladki), vobTrans w jednej klatce ma metry - bohater "teleportuje sie" o dlugosc
	// petli biegu (log 131330: 236-371 cm, tyle co 0,3-0,5 s biegu). Straznik: vobTrans ponad mozliwy ruch w klatce
	// (Core TravRootGlitch) i vob faktycznie przesuniety w te strone - przesuniecie cofniete dokladnie (bez kroku na
	// kroku), przeniesienie bez kolizji i ze stanem kolizji z nowej pozycji. Log: animacje i ich delty korzenia. ----------
	static const zdcore::TravRootGlitchParams TRAV_ROOT_GLITCH;
	static const float TRAV_POS_JUMP_LOG = 150.0f;   // cm w klatce bez naszego ruchu - wpis do logu (inna przyczyna)
	static bool TravAirGrabSmoothing();
	struct TravRootGuard {
		bool have = false;
		zVEC3 prev;
		int fixes = 0, jumpLogs = 0;
		// runda 12: historia pozycji (ok. 3 s) - czy skok trafia w dawna pozycje bohatera (i jak dawna)
		zVEC3 hist[192];
		float histAge[192] = {};
		int histPos = 0, histN = 0;
	};
	static TravRootGuard trg;

	// Animacje modelu z deltami korzenia w ostatniej klatce: nazwa (warstwa) |thisPos - lastPos| x waga, ~ = wygasa.
	static std::string TravRootAnis(zCModel* m) {
		std::string s;
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p)
				continue;
			zVEC3 d = a->thisPos - a->lastPos;
			char buf[120];
			sprintf_s(buf, "%s%s(w%d, kl. %.1f) %.0f x %.2f", s.empty() ? "" : ", ", p->aniName.ToChar(), p->layer, a->actFrame,
				TravLen(d), a->transWeight);
			s += buf;
			if (a->isFadingOut)
				s += "~";
		}
		return s.empty() ? "-" : s;
	}

	// Przeniesienie bohatera bez kolizji (jak teleport silnika) i stan kolizji z nowej pozycji.
	static void TravTeleportHero(const zVEC3& to, const char* why) {
		TravWhy w(why);
		bool cdS = player->collDetectionStatic != 0, cdD = player->collDetectionDynamic != 0;
		player->SetCollDetStat(FALSE);
		player->SetCollDetDyn(FALSE);
		player->SetPositionWorld(to);
		TravSyncCollision();
		player->SetCollDetStat(cdS ? TRUE : FALSE);
		player->SetCollDetDyn(cdD ? TRUE : FALSE);
	}

	// 24.10: przestawienie bez kolizji i BEZ zerowania stanu kolizji (podloga / sufit zostaja) - drobne poprawki wysokosci
	// w trakcie wyjscia w bieg; kolizja wraca od razu, silnik liczy ja w nastepnej klatce od nowej pozycji.
	static void TravSetHeroNoColl(const zVEC3& to, const char* why) {
		TravWhy w(why);
		bool cdS = player->collDetectionStatic != 0, cdD = player->collDetectionDynamic != 0;
		player->SetCollDetStat(FALSE);
		player->SetCollDetDyn(FALSE);
		player->SetPositionWorld(to);
		player->SetCollDetStat(cdS ? TRUE : FALSE);
		player->SetCollDetDyn(cdD ? TRUE : FALSE);
	}

	static void TravRootGuardUpdate(float dt) {
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m || player->IsDead() || dt <= 0.0f) {
			trg.have = false;
			return;
		}
		zVEC3 cur = player->GetPositionWorld();
		// nasz ruch ustawia pozycje sam (tor przeskoku, unik, finisher, dosuwanie chwytu); bieg po przeskoku - silnik
		bool ours = (ts.def && !ts.runExit) || ms.def || DirectorActive() || TravAirGrabSmoothing();
		// 18.10: cofanie czasu / przestawienie z zewnatrz - bez korekt (log tego, co by bylo poprawione), odniesienie od nowa
		const char* susp = ours || !trg.have ? nullptr : TravGuardSuspended();
		if (susp) {
			zVEC3 d0 = cur - trg.prev;
			const zVEC3& t0 = m->vobTrans;
			if (zdcore::TravRootGlitch(t0.n[0], t0.n[1], t0.n[2], TravFrameTime(dt), TRAV_ROOT_GLITCH) && TravLen(d0) > 1.0f)
				TravGuardSkip("skok korzenia animacji", TravLen(t0), susp);
			else if (TravLen(d0) > TRAV_POS_JUMP_LOG && travPostWin > 0.0f && !player->physicsEnabled)
				TravGuardSkip("skok pozycji po naszym ruchu", TravLen(d0), susp);
			if (travPostWin > 0.0f)
				travPostWin -= dt;
		}
		if (!trg.have || ours || susp) {
			for (int i = 0; i < trg.histN; i++)
				trg.histAge[i] += dt;
			trg.hist[trg.histPos] = cur;
			trg.histAge[trg.histPos] = 0.0f;
			trg.histPos = (trg.histPos + 1) % 192;
			trg.histN = trg.histN < 192 ? trg.histN + 1 : 192;
			trg.prev = cur;
			trg.have = true;
			return;
		}
		zVEC3 d = cur - trg.prev;
		const zVEC3& t = m->vobTrans;
		if (zdcore::TravRootGlitch(t.n[0], t.n[1], t.n[2], TravFrameTime(dt), TRAV_ROOT_GLITCH)) {
			zVEC3 r = player->GetRightVectorWorld(), u = player->GetUpVectorWorld(), a = player->GetAtVectorWorld();
			zVEC3 g(r.n[0] * t.n[0] + u.n[0] * t.n[1] + a.n[0] * t.n[2], r.n[1] * t.n[0] + u.n[1] * t.n[1] + a.n[1] * t.n[2],
				r.n[2] * t.n[0] + u.n[2] * t.n[1] + a.n[2] * t.n[2]);
			float gl2 = g.n[0] * g.n[0] + g.n[1] * g.n[1] + g.n[2] * g.n[2];
			float moved = d.n[0] * g.n[0] + d.n[1] * g.n[1] + d.n[2] * g.n[2];   // ruch w strone bledu (x |g|)
			zVEC3 toPre = cur - g;
			bool blockedPre = gl2 > 1.0f && moved > 0.5f * gl2 && MvTrace(cur, toPre - cur, player, nullptr, TRAV_TRACE);
			if (gl2 > 1.0f && moved > 0.5f * gl2 && TravCorrectOk(cur, blockedPre ? trg.prev : toPre, "skok korzenia animacji")) {
				zVEC3 to = blockedPre ? trg.prev : toPre;
				bool blocked = blockedPre;   // droga z powrotem zastawiona - pozycja z poprzedniej klatki
				oCAniCtrl_Human* ac = player->GetAnictrl();
				ZDebug("Skok korzenia animacji: %.0f cm w klatce (lokalnie %+.0f / %+.0f / %+.0f, %.3f s) - cofniety%s; animacje: %s; "
					"stan %d, ruch %d, fizyka %d, kolizja %d/%d, poz (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f)", sqrtf(gl2), t.n[0], t.n[1],
					t.n[2], dt, blocked ? " do poprzedniej klatki (droga zastawiona)" : "", TravRootAnis(m).c_str(),
					player->GetBodyState() & BS_ONLY_STATE, ac ? (int)ac->state : -1, player->physicsEnabled ? 1 : 0,
					player->collDetectionStatic ? 1 : 0, player->collDetectionDynamic ? 1 : 0, cur.n[0], cur.n[1], cur.n[2], to.n[0],
					to.n[1], to.n[2]);
				TravTeleportHero(to, "korekta: skok korzenia animacji");
				trg.fixes++;
				cur = to;
				d = cur - trg.prev;
			}
		}
		float dl = TravLen(d);
		if (travPostWin > 0.0f)
			travPostWin -= dt;
		if (dl > TRAV_POS_JUMP_LOG) {
			// najblizsza dawna pozycja z historii (czy silnik / mod wraca do pozycji sprzed X s)
			float bestD = 1e9f, bestAge = -1.0f;
			for (int i = 0; i < trg.histN; i++) {
				float e = TravLen(trg.hist[i] - cur);
				if (e < bestD) {
					bestD = e;
					bestAge = trg.histAge[i];
				}
			}
			// runda 12: tuz po naszym ruchu (okno 0,6 s) skok bez naszego ruchu i bez ruchu korzenia - przywrocenie pozycji
			// z poprzedniej klatki (przeniesienie bez kolizji, stan kolizji z niej)
			bool fix = travPostWin > 0.0f && !player->physicsEnabled && TravCorrectOk(cur, trg.prev, "skok pozycji po naszym ruchu");
			oCAniCtrl_Human* ac = player->GetAnictrl();
			if (trg.jumpLogs < 20) {
				trg.jumpLogs++;
				ZDebug("Skok pozycji %.0f cm w klatce (%.3f s) bez skoku korzenia (vobTrans %.0f cm): poz (%.0f, %.0f, %.0f) -> "
					"(%.0f, %.0f, %.0f), stan %d, ruch %d, fizyka %d, kolizja %d/%d; animacje: %s; cel najblizej pozycji sprzed %.2f s "
					"(%.0f cm od niej)%s", dl, dt, TravLen(t), trg.prev.n[0], trg.prev.n[1], trg.prev.n[2], cur.n[0], cur.n[1], cur.n[2],
					player->GetBodyState() & BS_ONLY_STATE, ac ? (int)ac->state : -1, player->physicsEnabled ? 1 : 0,
					player->collDetectionStatic ? 1 : 0, player->collDetectionDynamic ? 1 : 0, TravRootAnis(m).c_str(), bestAge, bestD,
					fix ? " - przywrocone (okno po naszym ruchu)" : "");
			}
			if (fix) {
				TravTeleportHero(trg.prev, "korekta: skok pozycji po naszym ruchu");
				trg.fixes++;
				cur = trg.prev;
			}
		}
		for (int i = 0; i < trg.histN; i++)
			trg.histAge[i] += dt;
		trg.hist[trg.histPos] = cur;
		trg.histAge[trg.histPos] = 0.0f;
		trg.histPos = (trg.histPos + 1) % 192;
		trg.histN = trg.histN < 192 ? trg.histN + 1 : 192;
		trg.prev = cur;
	}

	// ---------- Runda 12: kto przesuwa bohatera (Debug=1) - haki na ustawienie pozycji voba bohatera (SetPositionWorld
	// 0x61BB70, SetTrafoObjToWorld 0x61BC80, Move 0x61B2E0) i koniec ruchu voba (EndMovement 0x61E0D0: zadany trafo
	// i wynik po kolizji). Przesuniecie ponad TRAV_SRC_LOG_DIST w jednym wywolaniu - wpis z modulem wywolujacego
	// (exe: adres, DLL: nazwa+przesuniecie) i krotkim stosem. ----------
	static const float TRAV_SRC_LOG_DIST = 100.0f;

	static std::string TravAddrDesc(void* a) {
		HMODULE h = nullptr;
		char path[MAX_PATH] = "?";
		if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a,
			&h) && h)
			GetModuleFileNameA(h, path, MAX_PATH);
		const char* base = strrchr(path, '\\');
		base = base ? base + 1 : path;
		char buf[160];
		if (h && h == GetModuleHandleA(nullptr))
			sprintf_s(buf, "%s 0x%08X", base, (unsigned)(uintptr_t)a);
		else
			sprintf_s(buf, "%s+0x%X", base, (unsigned)((uintptr_t)a - (uintptr_t)h));
		return buf;
	}

	static std::string TravStackDesc() {
		void* fr[8];
		USHORT n = RtlCaptureStackBackTrace(2, 8, fr, nullptr);
		std::string s;
		for (USHORT i = 0; i < n; i++)
			s += (i ? " < " : "") + TravAddrDesc(fr[i]);
		return s.empty() ? "-" : s;
	}

	// Zrodlo ustawienia pozycji do logu: nasza korekta (powod) / nasz kod / zewnetrzne (kto) / silnik.
	static std::string TravSrcKind(void* ret) {
		if (travOwnWhy)
			return std::string("nasza korekta: ") + travOwnWhy;
		std::string who;
		int k = TravHgClassify(ret, &who);
		return k == zdcore::HG_SET_OURS ? std::string("nasz kod") : k == zdcore::HG_SET_EXTERNAL ? "zewnetrzne (" + who + ")" :
			std::string("silnik");
	}

	static void TravSrcLog(const char* fn, const zVEC3& from, const zVEC3& to, void* ret, const std::string& extra = "",
		const std::string& src = "") {
		if (travSrcLogs >= 40)
			return;
		travSrcLogs++;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		std::string kind = src.empty() ? TravSrcKind(ret) : src;
		ZDebug("Przesuniecie bohatera %.0f cm: %s z %s%s; zrodlo: %s; poz (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f); nasz ruch: %s%s, "
			"stan %d, ruch %d, fizyka %d, kolizja %d/%d, animacje %s; stos: %s", TravLen(to - from), fn, TravAddrDesc(ret).c_str(),
			extra.c_str(), kind.c_str(), from.n[0], from.n[1], from.n[2], to.n[0], to.n[1], to.n[2], ts.def ? ts.def->id.c_str() : "-",
			ts.runExit ? " (bieg)" : "", player->GetBodyState() & BS_ONLY_STATE, ac ? (int)ac->state : -1,
			player->physicsEnabled ? 1 : 0, player->collDetectionStatic ? 1 : 0, player->collDetectionDynamic ? 1 : 0,
			m ? TravLayerOne(m).c_str() : "-", TravStackDesc().c_str());
	}

	// Kazde ustawienie pozycji bohatera: przestawienie z zewnatrz zapisane (wstrzymanie korekt), Debug=1 - wpis do logu.
	static void TravHeroSet(const char* fn, const zVEC3& from, const zVEC3& to, void* ret) {
		TravHgNoteSet(from, to, ret);
		if (zcfg.debug && TravLen(to - from) > TRAV_SRC_LOG_DIST)
			TravSrcLog(fn, from, to, ret);
	}

	static void __fastcall TravHkSetPositionWorld(zCVob* self, void* edx, const zVEC3& p);
	static CInvoke<void(__fastcall*)(zCVob*, void*, const zVEC3&)> Ivk_zCVob_SetPositionWorld_Trav(0x0061BB70,
		&TravHkSetPositionWorld, IvkEnabled(ENGINE));
	static void __fastcall TravHkSetPositionWorld(zCVob* self, void* edx, const zVEC3& p) {
		if (player && self == player)
			TravHeroSet("zCVob::SetPositionWorld", self->GetPositionWorld(), p, _ReturnAddress());
		Ivk_zCVob_SetPositionWorld_Trav(self, edx, p);
	}

	static void __fastcall TravHkSetTrafoObjToWorld(zCVob* self, void* edx, const zMAT4& m);
	static CInvoke<void(__fastcall*)(zCVob*, void*, const zMAT4&)> Ivk_zCVob_SetTrafoObjToWorld_Trav(0x0061BC80,
		&TravHkSetTrafoObjToWorld, IvkEnabled(ENGINE));
	static void __fastcall TravHkSetTrafoObjToWorld(zCVob* self, void* edx, const zMAT4& m) {
		if (player && self == player)
			TravHeroSet("zCVob::SetTrafoObjToWorld", self->GetPositionWorld(), m.GetTranslation(), _ReturnAddress());
		Ivk_zCVob_SetTrafoObjToWorld_Trav(self, edx, m);
	}

	static void __fastcall TravHkMove(zCVob* self, void* edx, float x, float y, float z);
	static CInvoke<void(__fastcall*)(zCVob*, void*, float, float, float)> Ivk_zCVob_Move_Trav(0x0061B2E0, &TravHkMove,
		IvkEnabled(ENGINE));
	static void __fastcall TravHkMove(zCVob* self, void* edx, float x, float y, float z) {
		if (player && self == player && x * x + y * y + z * z > TRAV_EXT_MIN * TRAV_EXT_MIN) {
			zVEC3 from = self->GetPositionWorld();
			TravHeroSet("zCVob::Move", from, zVEC3(from.n[0] + x, from.n[1] + y, from.n[2] + z), _ReturnAddress());
		}
		Ivk_zCVob_Move_Trav(self, edx, x, y, z);
	}

	// ---------- 18.10: teleport kolizji (Core CollTeleportGlitch). Log NB i Steam: zCVob::EndMovement wolane z
	// zCVob::DoFrameActivity (0x602E95 - ruch voba w klatce) przy zwyklym ruchu w klatce ("zadane 55 cm") przenosi
	// bohatera o 1,5-4 m ("kolizja zmienila o 272 cm") - do pozycji sprzed ok. 0,3-0,6 s: start skoku w biegu przy
	// scianie (T_*RUN*_2_JUMP), start spadania (S_FALLDN), wlaczenie kolizji po przeskoku (wyjscie w bieg). Rozwiazanie
	// kolizji jest pod naszym hakiem (NB UNION_ABI ma hak nad nami - to oryginal silnika). Dawny straznik w petli pluginu
	// poprawial po renderze, a silnik w nastepnej klatce cofal znowu (ping-pong do ok. 0,1 s, potem skok o metry do
	// przodu). Teraz w samym haku, przed renderem: wynik kolizji cofniety do pozycji z poczatku klatki (tam bohater stal
	// w poprzedniej klatce - pozycja poprawna), cialo sztywne tez; najwyzej maxStreak klatek pod rzad. Nie w naszym
	// ruchu (tor przeskoku / unik / finisher), nie przy wstrzymaniu (cofanie czasu, przestawienie z zewnatrz), nie przy
	// mobie, tylko przy wlaczonej kolizji statycznej. TeleportGuard=0 - bez tego. ----------
	static int travCollStreak = 0, travCollFixes = 0, travCollLogs = 0;
	static bool travCollBusy = false;

	static void TravCollTeleportCheck(const zVEC3& from, const zVEC3& req, const zVEC3& to) {
		if (!zcfg.enabled || !zcfg.teleportGuard || travCollBusy || player->IsDead() || !player->collDetectionStatic) {
			travCollStreak = 0;
			return;
		}
		float a[3] = { from.n[0], from.n[1], from.n[2] }, b[3] = { req.n[0], req.n[1], req.n[2] }, c[3] = { to.n[0], to.n[1], to.n[2] };
		if (!zdcore::CollTeleportGlitch(a, b, c, TRAV_COLL_TELEPORT)) {
			travCollStreak = 0;
			return;
		}
		// 18.10 (58087/58119): ms.def, nie MovesActive() - ta obejmuje tez TraversalActive(), czyli wyjscie w bieg
		bool ours = (ts.def && !ts.runExit) || ms.def != nullptr || DirectorActive() || TravAirGrabSmoothing() ||
			player->GetInteractMob() != nullptr || HeroMounted();
		const char* susp = TravGuardSuspended();
		if (ours || susp) {
			if (susp)
				TravGuardSkip("teleport kolizji", TravLen(to - from), susp);
			travCollStreak = 0;
			return;
		}
		int streakBefore = travCollStreak;
		// 24.10: straznik oscylacji przed cofnieciem (jedna korekta na klatke, powtarzane cofanie = wynik silnika zostaje)
		if (streakBefore < TRAV_COLL_TELEPORT.maxStreak && !TravCorrectOk(to, from, "teleport kolizji cofniety do startu klatki")) {
			travCollStreak = TRAV_COLL_TELEPORT.maxStreak + 1;   // do konca serii bez cofania
			return;
		}
		if (!zdcore::CollTeleportDecide(true, travCollStreak, TRAV_COLL_TELEPORT)) {
			if (streakBefore == TRAV_COLL_TELEPORT.maxStreak && travCollLogs < 40) {
				travCollLogs++;
				ZDebug("Teleport kolizji: silnik upiera sie %d klatek - wynik przyjety (%.0f cm, %s)", streakBefore, TravLen(to - from),
					TravLayerOne(player->GetModel()).c_str());
			}
			travCollStreak = streakBefore + 1;   // do konca serii wynik przyjmowany (log raz)
			return;
		}
		travCollFixes++;
		if (travCollLogs < 40) {
			travCollLogs++;
			oCAniCtrl_Human* ac = player->GetAnictrl();
			ZDebug("Teleport kolizji: zadane w ruchu %.0f cm, kolizja przeniosla o %.0f cm (od zadanego %.0f) - cofniety do pozycji z "
				"poczatku klatki (%d. pod rzad, razem %d); poz (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f); stan %d, ruch %d, fizyka %d, "
				"animacje %s", TravLen(req - from), TravLen(to - from), TravLen(to - req), travCollStreak, travCollFixes, from.n[0],
				from.n[1], from.n[2], to.n[0], to.n[1], to.n[2], player->GetBodyState() & BS_ONLY_STATE, ac ? (int)ac->state : -1,
				player->physicsEnabled ? 1 : 0, TravLayerOne(player->GetModel()).c_str());
		}
		travCollBusy = true;
		TravTeleportHero(from, "teleport kolizji cofniety do startu klatki");
		if (player->physicsEnabled)
			if (zCRigidBody* rb = player->GetRigidBody())
				rb->xPos = from;
		travCollBusy = false;
	}

	static void __fastcall TravHkEndMovement(zCVob* self, void* edx, int hint);
	static CInvoke<void(__fastcall*)(zCVob*, void*, int)> Ivk_zCVob_EndMovement_Trav(0x0061E0D0, &TravHkEndMovement,
		IvkEnabled(ENGINE));
	static void __fastcall TravHkEndMovement(zCVob* self, void* edx, int hint) {
		ZHookHit(ZH_ENDMOVEMENT);
		if (!player || self != player) {
			Ivk_zCVob_EndMovement_Trav(self, edx, hint);
			return;
		}
		zVEC3 from = self->GetPositionWorld(), req = from;
		if (zCCollisionObject* co = self->m_poCollisionObject)
			req = co->m_oTrafoNew.GetTranslation();
		void* ret = _ReturnAddress();
		Ivk_zCVob_EndMovement_Trav(self, edx, hint);
		zVEC3 to = self->GetPositionWorld();
		bool frame = ret == (void*)0x00602E95;   // zCVob::DoFrameActivity - ruch voba w klatce
		if (zcfg.debug && (TravLen(to - from) > TRAV_SRC_LOG_DIST || TravLen(to - req) > TRAV_SRC_LOG_DIST)) {
			char b[160];
			sprintf_s(b, " (zadane w ruchu %.0f cm, kolizja zmienila o %.0f cm)", TravLen(req - from), TravLen(to - req));
			TravSrcLog("zCVob::EndMovement", from, to, ret, b, travOwnWhy ? std::string("nasza korekta: ") + travOwnWhy :
				frame ? std::string("silnik (ruch voba w klatce)") : std::string("silnik / koniec ustawienia pozycji"));
		}
		if (frame)
			TravCollTeleportCheck(from, req, to);
	}

	// Limity wpisow diagnostyki (Przesuniecie bohatera, Skok pozycji, Teleport kolizji, korekty pominiete) od nowa -
	// przy starcie kazdego przeskoku (wczesniej na sesje: przy wyjsciu w bieg w 58119 juz wyczerpane).
	static void TravDiagBudgetReset() {
		travSrcLogs = 0;
		trg.jumpLogs = 0;
		travCollLogs = 0;
		travGuardSkipLogs = 0;
		travOscLogs = 0;
	}

	// ---------- Runda 13: falszywe spadanie. Silnik: zCAIPlayer::CalcStateVars (0x50E440) liczy "nad podloga" (stopy -
	// podloga stanu kolizji postaci, promien ze srodka) i wola CheckPhysics (0x50DC40): nad podloga > zMV_STEP_HEIGHT
	// (config +0x2C) i podloga nie schodek -> fizyka wlaczona, animacja spadania. Szczelina miedzy skrzyniami / dziura
	// przy kamyku pod srodkiem wystarczy. Hak: przed CheckPhysics bohatera probki podlogi na obwodzie ciala (8 punktow,
	// 30 cm) i 30 cm przed nim (kierunek ruchu); Core FallForgiveDecide - wybaczone: "nad podloga" = krok (silnik zostaje
	// przy chodzie / biegu). Log (Debug=1): wybaczenie z probkami, start prawdziwego spadania z probkami, ladowanie (czas
	// lotu, wysokosc spadku, animacja). ----------
	static const float TRAV_FALL_RING = 30.0f;     // promien obwodu ciala (cm)
	struct TravFallState {
		float forgiven = 0.0f;         // s wybaczania pod rzad
		int logs = 0;
		bool tracking = false;         // trwa prawdziwe spadanie (log ladowania)
		float t = 0.0f, startFeet = 0.0f;
		std::string start;
	};
	static TravFallState tfall;

	// Spadek (stopy - podloga) pod punktem (x, z): promien od 60 cm nad stopami w dol do 200 cm pod nie; brak = 1e9.
	static float TravFallSample(float x, float z, float feetY) {
		zVEC3 from(x, feetY + 60.0f, z);
		if (!MvTrace(from, zVEC3(0.0f, -260.0f, 0.0f), player, nullptr, TRAV_TRACE))
			return 1e9f;
		return feetY - ogame->GetGameWorld()->traceRayReport.foundIntersection.n[1];
	}

	static std::string TravFallSamplesDesc(const float ring[8], float ahead) {
		std::string s;
		char b[24];
		for (int i = 0; i < 8; i++) {
			if (ring[i] > 1e8f)
				sprintf_s(b, "%s-", i ? " " : "");
			else
				sprintf_s(b, "%s%.0f", i ? " " : "", ring[i]);
			s += b;
		}
		if (ahead > 1e8f)
			sprintf_s(b, "; przod -");
		else
			sprintf_s(b, "; przod %.0f", ahead);
		return s + b;
	}

	static void __fastcall TravHkCheckPhysics(zCAIPlayer* self, void* edx);
	static CInvoke<void(__fastcall*)(zCAIPlayer*, void*)> Ivk_zCAIPlayer_CheckPhysics_Trav(0x0050DC40, &TravHkCheckPhysics,
		IvkEnabled(ENGINE));
	static void __fastcall TravHkCheckPhysics(zCAIPlayer* self, void* edx) {
		ZHookHit(ZH_CHECKPHYSICS);
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!ac || self != (zCAIPlayer*)ac || player->physicsEnabled || !ogame || !ogame->GetGameWorld()) {
			Ivk_zCAIPlayer_CheckPhysics_Trav(self, edx);
			return;
		}
		float step = self->config.zMV_STEP_HEIGHT;
		int bs = player->GetBodyState() & BS_ONLY_STATE;
		bool ours = TraversalActive() || MovesActive() || DirectorActive() || TravAirGrabSmoothing() || TravGuardSuspended();
		if (self->aboveFloor <= step || self->waterLevel > 0 || ours || bs == BS_JUMP || player->IsDead()) {
			if (self->aboveFloor <= step)
				tfall.forgiven = 0.0f;
			Ivk_zCAIPlayer_CheckPhysics_Trav(self, edx);
			return;
		}
		// probki podlogi: obwod ciala i przod (kierunek ruchu, bez ruchu - patrzenia)
		zVEC3 c = player->GetPositionWorld();
		float feet = self->feetY;
		float ring[8];
		for (int i = 0; i < 8; i++) {
			float a = i * 0.785398f;
			ring[i] = TravFallSample(c.n[0] + sinf(a) * TRAV_FALL_RING, c.n[2] + cosf(a) * TRAV_FALL_RING, feet);
		}
		zVEC3 v = self->velocity;
		float sp = sqrtf(v.n[0] * v.n[0] + v.n[2] * v.n[2]);
		zVEC3 f = sp > 50.0f ? zVEC3(v.n[0] / sp, 0.0f, v.n[2] / sp) : TravHeading();
		float spd = sp > heroSpeed ? sp : heroSpeed;
		float ahead = TravFallSample(c.n[0] + f.n[0] * TRAV_FALL_RING, c.n[2] + f.n[2] * TRAV_FALL_RING, feet);
		zdcore::FallForgiveParams prm;
		prm.maxDrop = (float)zcfg.fallForgiveDrop;
		zdcore::FallForgive dec = zdcore::FallForgiveDecide(self->aboveFloor, step, ring, ahead, spd, tfall.forgiven, prm);
		float dt = ztimer ? ztimer->frameTimeFloat / 1000.0f : 0.016f;
		if (zcfg.fallForgiveness && dec == zdcore::FALL_FORGIVE) {
			if (tfall.forgiven <= 0.0f && zcfg.debug && tfall.logs < 40) {
				tfall.logs++;
				ZDebug("Spadanie wybaczone: nad podloga %.0f (krok %.0f), predkosc %.0f, stan %d, animacje %s; obwod %s",
					self->aboveFloor, step, spd, bs, TravLayerOne(player->GetModel()).c_str(), TravFallSamplesDesc(ring, ahead).c_str());
			}
			tfall.forgiven += dt;
			self->aboveFloor = step;   // silnik: na podlodze (chod / bieg dalej)
			Ivk_zCAIPlayer_CheckPhysics_Trav(self, edx);
			return;
		}
		Ivk_zCAIPlayer_CheckPhysics_Trav(self, edx);
		if (player->physicsEnabled && !tfall.tracking) {
			// prawdziwe spadanie (albo wybaczanie wylaczone / za dlugo) - log startu, ladowanie w TravFallTrack
			tfall.tracking = true;
			tfall.t = 0.0f;
			tfall.startFeet = feet;
			char b[400];
			sprintf_s(b, "nad podloga %.0f (krok %.0f, podloga kolizji %+.0f), predkosc %.0f, stan %d, decyzja %s%s; obwod %s",
				self->aboveFloor, step, -self->aboveFloor, spd, bs, dec == zdcore::FALL_FORGIVE ? "wybaczyc (wylaczone)" :
				"spada", tfall.forgiven > 0.0f ? " po wybaczaniu" : "", TravFallSamplesDesc(ring, ahead).c_str());
			tfall.start = b;
			if (zcfg.debug && tfall.logs < 40) {
				tfall.logs++;
				ZDebug("Spadanie: start - %s, poz (%.0f, %.0f, %.0f)", b, c.n[0], c.n[1], c.n[2]);
			}
		}
		tfall.forgiven = 0.0f;
	}

	// Ladowanie po prawdziwym spadaniu (log): czas lotu, wysokosc spadku, animacja ladowania.
	static void TravFallTrack(float dt) {
		if (!tfall.tracking)
			return;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!ac || player->IsDead()) {
			tfall.tracking = false;
			return;
		}
		tfall.t += dt;
		if (player->physicsEnabled && tfall.t < 8.0f)
			return;
		tfall.tracking = false;
		if (zcfg.debug && tfall.logs < 40) {
			tfall.logs++;
			ZDebug("Spadanie: ladowanie po %.2f s, spadek %.0f cm, animacje %s, stan %d (start: %s)", tfall.t,
				tfall.startFeet - ac->feetY, TravLayerOne(player->GetModel()).c_str(), player->GetBodyState() & BS_ONLY_STATE,
				tfall.start.c_str());
		}
	}

	// Runda 10 (cofniecie modelu o ok. 1 m w trakcie wspiecia, log pozycji voba bez skoku): stan wizualny w klatce -
	// BIP01 wzgledem voba (w ukladzie fwd: przod / bok / y), animacje na wezle BIP01 (waga, klatka, ~ = wygasa), kamera
	// (tryb, pozycja wzgledem bohatera, cel sledzenia kamery sampledPlayerPos wzgledem voba).
	static std::string TravVisualDiag(const zVEC3& fwd) {
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m)
			return "-";
		std::string s;
		char buf[220];
		zVEC3 vp = player->GetPositionWorld();
		zVEC3 right(fwd.n[2], 0.0f, -fwd.n[0]);
		auto rel = [&](const zVEC3& p, float& a, float& sd, float& y) {
			zVEC3 d = p - vp;
			a = d.n[0] * fwd.n[0] + d.n[2] * fwd.n[2];
			sd = d.n[0] * right.n[0] + d.n[2] * right.n[2];
			y = d.n[1];
		};
		float a, sd, y;
		if (zCModelNodeInst* root = m->SearchNode(zSTRING("BIP01"))) {
			rel(m->GetNodePositionWorld(root), a, sd, y);
			sprintf_s(buf, "BIP01 wzgl. voba: przod %+.0f, bok %+.0f, y %+.0f; wezel BIP01:", a, sd, y);
			s += buf;
			int n = 0;
			for (int k = 0; k < root->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++) {
				const zCModelNodeInst::zTNodeAni& e = root->nodeAniList[k];
				zCModelAniActive* aa = e.modelAni;
				if (!aa || !aa->protoAni)
					continue;
				sprintf_s(buf, "%s %s w%.2f kl.%.1f%s", n++ ? "," : "", aa->protoAni->aniName.ToChar(), e.weight, aa->actFrame,
					aa->isFadingOut ? "~" : "");
				s += buf;
			}
			if (!n)
				s += " -";
		}
		zCAICamera* cam = zCAICamera::GetCurrent();
		if (cam && cam->camVob) {
			rel(cam->camVob->GetPositionWorld(), a, sd, y);
			zSTRING mode = cam->GetMode();
			sprintf_s(buf, "; kamera %s: przod %+.0f, bok %+.0f, y %+.0f", mode.ToChar(), a, sd, y);
			s += buf;
			if (zCMovementTracker* tr = cam->moveTracker) {
				rel(tr->sampledPlayerPos, a, sd, y);
				sprintf_s(buf, ", cel kamery wzgl. voba: przod %+.0f, bok %+.0f, y %+.0f", a, sd, y);
				s += buf;
			}
		}
		return s;
	}

	// Log co klatke w trakcie przeskoku (Debug=1, od TRAV_DIAG_TAIL s przed koncem klipu): pozycja voba w chwili
	// naszej klatki (po klatce silnika i renderze - Game_Loop po zCWorld::Render) wzgledem startu, przesuniecie przez
	// silnik od naszego ostatniego celu toru, stan wizualny. Wolane przed TravStep.
	static const float TRAV_DIAG_TAIL = 1.2f;
	static void TravMoveDiag() {
		if (!zcfg.debug || !ts.def || ts.t < ts.def->cancel - TRAV_DIAG_TAIL)
			return;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zVEC3 p = player->GetPositionWorld(), d = p - ts.start;
		zVEC3 right(ts.dir.n[2], 0.0f, -ts.dir.n[0]);
		float along = d.n[0] * ts.dir.n[0] + d.n[2] * ts.dir.n[2], side = d.n[0] * right.n[0] + d.n[2] * right.n[2];
		zVEC3 e = ts.hasTarget ? p - ts.lastTarget : zVEC3(0.0f, 0.0f, 0.0f);
		ZDebug("Przeskok %s [kl. %d, %.3f s]: vob przod %+.0f, bok %+.0f, y %+.0f (od startu); silnik od naszego celu %.0f cm "
			"(przod %+.0f, y %+.0f); %s; fizyka %d, kolizja %d/%d, stan %d, ruch %d%s", ts.def->id.c_str(), ts.diagFrame++, ts.t,
			along, side, p.n[1] - ts.startGround, TravLen(e), e.n[0] * ts.dir.n[0] + e.n[2] * ts.dir.n[2], e.n[1],
			TravVisualDiag(ts.dir).c_str(), player->physicsEnabled ? 1 : 0, player->collDetectionStatic ? 1 : 0,
			player->collDetectionDynamic ? 1 : 0, player->GetBodyState() & BS_ONLY_STATE, ac ? (int)ac->state : -1,
			ts.runExit ? ", bieg" : ts.ending >= 0.0f ? ", konczenie" : "");
	}

	// Log co klatke po koncu naszego ruchu (TravPostWatchStart).
	static void TravPostWatchUpdate(float dt) {
		if (tpw.t < 0.0f)
			return;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!ac || !m || !zcfg.debug) {
			tpw.t = -1.0f;
			return;
		}
		tpw.t += dt;
		zVEC3 p = player->GetPositionWorld(), d = p - tpw.prev;
		zCCollObjectCharacter* cc = TravCharColl();
		ZDebug("Po ruchu %s [kl. %d, %.3f s]: y %+.0f (od konca), krok %.0f cm (pion %+.0f), stan %d, ruch %d, fizyka %d, "
			"kolizja %d/%d, nad podloga %.0f, podloga kolizji %s, korzen %.0f cm; %s; %s", tpw.what.c_str(), tpw.frame++, tpw.t,
			p.n[1] - tpw.p0.n[1], sqrtf(d.n[0] * d.n[0] + d.n[2] * d.n[2]), d.n[1], player->GetBodyState() & BS_ONLY_STATE,
			(int)ac->state, player->physicsEnabled ? 1 : 0, player->collDetectionStatic ? 1 : 0,
			player->collDetectionDynamic ? 1 : 0, ac->aboveFloor, cc ? std::to_string((int)(cc->m_oSpatialState.m_fFloorY -
			tpw.p0.n[1])).c_str() : "?", TravLen(m->vobTrans), TravLayerOne(m).c_str(), TravVisualDiag(TravHeading()).c_str());
		tpw.prev = p;
		if (tpw.t >= TRAV_POST_WATCH_TIME)
			tpw.t = -1.0f;
	}

	// ---------- Chwyt krawedzi w locie (TraversalAirGrab): skok gry, spadanie, W trzymane (bez S) - krawedz silnika
	// (DetectClimbUpLedge) na wysokosci dloni -> natywny zawis Gothica jak przy wychodzeniu z wody (Swim_ClimbLedge
	// 0x6B2440): AlignModelToLedge (0x511070: kolizja statyczna i fizyka wylaczone, animacja, dlon na krawedz, kierunek do
	// niej) z S_HANG; dalej silnik (oCAIHuman::PC_CheckSpecialStates 0x699510: S_HANG bez S = od razu podciaganie
	// T_HANG_2_STAND, S bez skoku = puszczenie, S + skok = zawis). Chwyt dopiero, gdy dosuniecie dloni do krawedzi najwyzej
	// 70 cm (albo bohater sie nie zbliza).
	// Runda 8: dawniej T_JUMP_2_HANG - to skok z miejsca do zawisu (korzen 83 -> 47 -> 195 cm, start w pozie stojacej),
	// a AlignModelToLedge bierze dlon z pozy po jednej klatce (glownie jeszcze poza lotu, GetHandPositionWorld 0x511170 =
	// srodek dloni biezacej pozy) - vob ustawiony za wysoko, potem opadanie w polowie. Teraz S_HANG (staly korzen) i co
	// klatke dosuwania cel z dloni biezacej pozy (Core TravGrabLockAxis): dlonie na krawedzi od chwytu; w tym czasie zawis
	// trzymany (S + skok dla silnika, TraversalGameKeyOverride), podciaganie silnika po dosunieciu. Log co klatke (Debug=1):
	// "Chwyt [kl.]". ----------
	static const zdcore::TravAirGrabParams TRAV_AIR_GRAB;
	static const zdcore::TravAirGrabTiming TRAV_AIR_TIMING;
	static const zdcore::TravGrabHold TRAV_AIR_HOLD;
	static const float TRAV_AIR_TRACE_TIME = 2.0f;   // s logu co klatke po chwycie (Debug=1)
	struct TravAirGrabState {
		float air = 0.0f, cooldown = 0.0f;
		std::string info;             // ostatni chwyt (debug, kilka sekund)
		float infoT = 0.0f;
		bool smoothing = false;       // dosuwanie do pozycji zawisu
		bool hold = false;            // zawis trzymany (silnik widzi S + skok) do dosuniecia
		float st = 0.0f;
		zVEC3 offset;                 // dosuniecie z AlignModelToLedge (log, przyblizenie)
		zVEC3 before, ledge;          // pozycja sprzed chwytu, punkt krawedzi
		float yaw0 = 0.0f, yaw1 = 0.0f;
		int waits = 0;                // klatki czekania na mniejsze dosuniecie (log)
		float trace = 0.0f;           // log co klatke - pozostaly czas
		float traceT = 0.0f;
		int traceFrame = 0;
	};
	static TravAirGrabState tag;

	static bool TravAirGrabSmoothing() {
		return tag.smoothing;
	}

	static bool TravAniActiveByName(zCModel* m, const char* name) {
		int id = m ? m->GetAniIDFromAniName(name) : -1;
		zCModelAni* a = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		return a && m->IsAniActive(a);
	}

	// Klawisze gry w czasie dosuwania do zawisu: S i skok wcisniete dla silnika (zawis trzymany, bez podciagania).
	// 18.10: w trakcie animacji przeskoku / wspiecia klawisze ruchu sa dla gry puszczone (0) - blokada ruchu silnika
	// (SetMovLock) dziala tez w NB (log: z kolizja wylaczona silnik nie przesuwa voba z toru), to zabezpieczenie przed
	// hakami NB na _Forward / PC_JumpForward / PC_Turnings / PC_CheckSpecialStates i obrotem A/D. Wyjscie w bieg (silnik
	// prowadzi) - W dla gry; nasze okno wyjscia czyta W surowo (MvRawState - wywolujacy pomija podmiany).
	int TraversalGameKeyOverride(unsigned short logicalKey, bool toggled) {
		if (zcfg.enabled && zdcore::TravMaskMoveKey(TravIsMoveKey(logicalKey), ts.def != nullptr, ts.runExit, false))
			return 0;
		if (!tag.hold || toggled)
			return -1;
		return logicalKey == GAME_DOWN || logicalKey == GAME_SMOVE ? 1 : -1;
	}

	// Animacje warstwy 1 w locie, z ktorych wolno chwycic (skok w biegu / z miejsca, spadanie); zawis, skok do gory
	// (silnik sam chwyta), woda - nie.
	static bool TravAirGrabAnis(zCModel* m) {
		bool fly = false;
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* p = a ? a->protoAni : nullptr;
			if (!p || p->layer != 1)
				continue;
			std::string n = zdcore::Upper(p->aniName.ToChar());
			if (n.find("HANG") != std::string::npos || n.find("JUMPUP") != std::string::npos || n.find("SWIM") != std::string::npos ||
				n.find("DIVE") != std::string::npos)
				return false;
			if (TravIsRunJumpName(n.c_str()) || TravIsFlightName(n.c_str()) || n == "T_STAND_2_JUMP" || n == "S_FALLB")
				fly = true;
		}
		return fly;
	}

	// Dosuniecie do pozycji zawisu: obrot do krawedzi, potem vob tak, by dlonie biezacej pozy (lot -> S_HANG) byly na
	// krawedzi, dochodzenie z pozycji sprzed chwytu (ease-out); zawis trzymany do dosuniecia + chwila (poza w pelni).
	static void TravAirGrabSmoothUpdate(float dt) {
		if (!tag.smoothing)
			return;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!ac || !m || player->IsDead()) {
			tag.smoothing = false;
			tag.hold = false;
			return;
		}
		tag.st += dt;
		float e = zdcore::TravEaseOut(tag.st / TRAV_AIR_TIMING.smoothTime);
		float yaw = zdcore::LerpYaw(tag.yaw0, tag.yaw1, e);
		player->SetHeadingAtWorld(zVEC3(sinf(yaw), 0.0f, cosf(yaw)));
		bool hang = TravAniActiveByName(m, "S_HANG");
		bool climb = TravAniActiveByName(m, "T_HANG_2_STAND");
		bool down = MvRawState(GAME_DOWN);
		if (hang && !climb) {
			zVEC3 p = player->GetPositionWorld(), hand = ac->GetHandPositionWorld(), q;
			for (int i = 0; i < 3; i++)
				q.n[i] = zdcore::TravGrabLockAxis(tag.before.n[i], p.n[i], hand.n[i], tag.ledge.n[i], e);
			player->SetPositionWorld(q);
		}
		tag.hold = !climb && zdcore::TravGrabHolding(tag.st, TRAV_AIR_TIMING.smoothTime, hang, down, TRAV_AIR_HOLD);
		if (!tag.hold) {
			tag.smoothing = false;
			zVEC3 p = player->GetPositionWorld(), hand = ac->GetHandPositionWorld();
			ZDebug("Przeskok [chwyt w locie]: dosuniety w %.2f s (%s) - vob %+.0f / dlon %+.0f cm wzgledem krawedzi, dlon %.0f cm od "
				"niej poziomo, przesuniecie od chwytu %.0f cm (pion %+.0f); dalej silnik: %s", tag.st, down ? "S - puszczenie" :
				climb ? "podciaganie juz trwa" : !hang ? "bez S_HANG" : "zawis gotowy", p.n[1] - tag.ledge.n[1], hand.n[1] - tag.ledge.n[1],
				sqrtf((hand.n[0] - tag.ledge.n[0]) * (hand.n[0] - tag.ledge.n[0]) + (hand.n[2] - tag.ledge.n[2]) * (hand.n[2] - tag.ledge.n[2])),
				TravLen(p - tag.before), p.n[1] - tag.before.n[1], down ? "spadanie" : "T_HANG_2_STAND");
		}
	}

	// Log co klatke po chwycie (Debug=1): y voba i dloni wzgledem krawedzi, animacje warstwy 1, stan.
	static void TravAirGrabTrace(float dt) {
		if (tag.trace <= 0.0f)
			return;
		tag.trace -= dt;
		tag.traceT += dt;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!ac || !m || !zcfg.debug) {
			tag.trace = 0.0f;
			return;
		}
		zVEC3 p = player->GetPositionWorld(), hand = ac->GetHandPositionWorld();
		float hx = hand.n[0] - tag.ledge.n[0], hz = hand.n[2] - tag.ledge.n[2];
		ZDebug("Chwyt [kl. %d, %.3f s]: vob y %+.1f, dlon y %+.1f wzgl. krawedzi (poziomo %.0f cm), warstwa 1: %s, ruch %d, "
			"cialo %d, fizyka %d, trzymanie %d", tag.traceFrame++, tag.traceT, p.n[1] - tag.ledge.n[1], hand.n[1] - tag.ledge.n[1],
			sqrtf(hx * hx + hz * hz), TravLayerOne(m).c_str(), (int)ac->state, player->GetBodyState() & BS_ONLY_STATE,
			player->physicsEnabled ? 1 : 0, tag.hold ? 1 : 0);
	}

	static void TravAirGrabUpdate(float dt) {
		if (tag.cooldown > 0.0f)
			tag.cooldown -= dt;
		if (tag.infoT > 0.0f)
			tag.infoT -= dt;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		// chwyt tylko z W (ruch do przodu), bez S - inaczej spadanie jak w grze
		bool air = zcfg.enabled && zcfg.traversalAirGrab && ac && m && !player->IsDead() && !ts.def && !ms.def && !DirectorActive() &&
			!tag.smoothing && ac->state == zCAIPlayer::zMV_STATE_FLY && ac->waterLevel == 0 &&
			player->GetWeaponMode() == NPC_WEAPON_NONE && MvRawState(GAME_UP) && !MvRawState(GAME_DOWN) && TravAirGrabAnis(m);
		if (!air) {
			tag.air = 0.0f;
			tag.waits = 0;
			return;
		}
		tag.air += dt;
		if (tag.air < TRAV_AIR_GRAB.minAirTime || tag.cooldown > 0.0f)
			return;
		if (TravChainNow("chwyt w locie"))
			return;
		zVEC3 v(0.0f, 0.0f, 0.0f);
		if (!ac->DetectClimbUpLedge(v, 1) || !ac->GetFoundLedge())
			return;
		zVEC3 ledge = ac->GetLedgeInfo()->point;
		float ledgeY = ledge.n[1];
		if (!zdcore::TravAirGrabOk(ledgeY, ac->headY, ac->feetY, TRAV_AIR_GRAB)) {
			ac->ClearFoundLedge();
			return;
		}
		// dosuniecie dloni do krawedzi (poziomo) i predkosc w jej strone - chwyt blizej (mniejszy przeskok)
		zVEC3 hand = ac->GetHandPositionWorld();
		float sx = ledge.n[0] - hand.n[0], sz = ledge.n[2] - hand.n[2];
		float shift = sqrtf(sx * sx + sz * sz);
		float toward = shift > 1.0f ? (tfl.vel.n[0] * sx + tfl.vel.n[2] * sz) / shift : 0.0f;
		if (!zdcore::TravAirGrabNow(shift, ledgeY - ac->headY, toward, TRAV_AIR_GRAB, TRAV_AIR_TIMING)) {
			ac->ClearFoundLedge();
			tag.waits++;
			return;
		}
		char buf[200];
		sprintf_s(buf, "chwyt w locie: krawedz %.0f cm nad stopami (glowa %+.0f), dlon %.0f cm od niej", ledgeY - ac->feetY,
			ledgeY - ac->headY, shift);
		zVEC3 before = player->GetPositionWorld(), at0 = TravHeading();
		float handBefore = hand.n[1] - ledgeY;
		std::string anisBefore = TravLayerOne(m);
		TravTrackStart(buf);
		// zawis od razu w pozie S_HANG (staly korzen) - nie T_JUMP_2_HANG (skok z miejsca do zawisu, korzen 83 -> 47 -> 195)
		ac->AlignModelToLedge(zSTRING("S_HANG"));
		zVEC3 after = player->GetPositionWorld(), at1 = TravHeading();
		// wroc do pozycji sprzed chwytu; dosuwanie z dloni biezacej pozy co klatke (TravAirGrabSmoothUpdate)
		tag.offset = after - before;
		tag.before = before;
		tag.ledge = ledge;
		tag.yaw0 = atan2f(at0.n[0], at0.n[2]);
		tag.yaw1 = atan2f(at1.n[0], at1.n[2]);
		tag.st = 0.0f;
		tag.smoothing = true;
		tag.hold = true;   // bez podciagania silnika, zanim dlonie dojda do krawedzi
		tag.trace = zcfg.debug ? TRAV_AIR_TRACE_TIME : 0.0f;
		tag.traceT = 0.0f;
		tag.traceFrame = 0;
		player->SetPositionWorld(before);
		player->SetHeadingAtWorld(at0);
		// poza: S_HANG wchodzi z pozy lotu w czasie dosuwania
		int id = m->GetAniIDFromAniName("S_HANG");
		zCModelAni* ha = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		if (zCModelAniActive* in = ha ? m->GetActiveAni(ha) : nullptr)
			MvSetBlend(m, in, nullptr, TRAV_AIR_TIMING.smoothTime);
		float off = TravLen(tag.offset), offH = sqrtf(tag.offset.n[0] * tag.offset.n[0] + tag.offset.n[2] * tag.offset.n[2]);
		ZDebug("Przeskok [chwyt w locie]: %s; dlon %+.0f cm wzgl. krawedzi (animacje %s); AlignModelToLedge(S_HANG): %.0f cm "
			"(poziomo %.0f, pion %+.0f) - przyblizenie z pozy lotu, dosuwanie z dloni co klatke w %.2f s, obrot %.0f st.; czekal %d "
			"kl.; %s, %.2f s w powietrzu", buf, handBefore, anisBefore.c_str(), off, offH, tag.offset.n[1], TRAV_AIR_TIMING.smoothTime,
			zdcore::AngleDiff(tag.yaw0, tag.yaw1) * 57.2958f, tag.waits, TravHeroState().c_str(), tag.air);
		tag.cooldown = TRAV_AIR_GRAB.cooldown;
		tag.air = 0.0f;
		tag.waits = 0;
		tag.info = buf;
		tag.infoT = 3.0f;
	}

	// 24.10 (stopien 100 na torze: bohater utknal w szparze stosu skrzyn 2x2): po naszym ruchu W trzymane, bohater stoi
	// w miejscu (TRAV_STUCK.window s) na niepewnym gruncie - wypchniecie na najblizszy pewny grunt (8 kierunkow x 30 / 50 cm,
	// grunt pod obrysem i sonda kolizji), log.
	static const zdcore::TravStuckParams TRAV_STUCK;
	struct TravStuckState {
		bool have = false;
		zVEC3 ref;
		float still = 0.0f;
	};
	static TravStuckState tstk;
	static void TravStuckUpdate(float dt) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		float since = travClock - travLastEndT;
		if (ts.def || !ac || player->IsDead() || player->physicsEnabled || ac->state != zCAIPlayer::zMV_STATE_STAND || since < 0.0f ||
			since > TRAV_STUCK.after || !player->collDetectionStatic) {
			tstk.have = false;
			return;
		}
		zVEC3 p = player->GetPositionWorld();
		if (!tstk.have || TravLen(p - tstk.ref) > TRAV_STUCK.maxMove) {
			tstk.have = true;
			tstk.ref = p;
			tstk.still = 0.0f;
			return;
		}
		tstk.still += dt;
		bool w = MvRawState(GAME_UP) && !MvRawState(GAME_DOWN);
		float yOff = p.n[1] - ac->feetY;
		if (yOff < 30.0f || yOff > 200.0f)
			return;
		std::string foot;
		zVEC3 head = TravHeading();
		bool ok = TravFootingAt(p, yOff, head, &foot);
		if (!zdcore::TravStuck(since, tstk.still, w, ok, TRAV_STUCK))
			return;
		tstk.have = false;
		zVEC3 right(head.n[2], 0.0f, -head.n[0]);
		for (float r : { 30.0f, 50.0f })
			for (int a = 0; a < 8; a++) {
				float ang = a * 0.785398f, cx = cosf(ang), cz = sinf(ang);
				zVEC3 q(p.n[0] + (head.n[0] * cx + right.n[0] * cz) * r, p.n[1], p.n[2] + (head.n[2] * cx + right.n[2] * cz) * r);
				float g;
				if (TravGroundAt(q, g))
					q.n[1] = g + yOff;
				if (!TravFootingAt(q, yOff, head))
					continue;
				float shift = TravCollProbe(q);
				if (shift > TRAV_SETTLE_TOL)
					continue;
				if (!TravCorrectOk(p, q, "zaklinowanie - pewny grunt")) {
					TravTeleportHero(p, "zaklinowanie - powrot (straznik)");
					return;
				}
				TravTeleportHero(q, "zaklinowanie - pewny grunt");
				ZDebug("Zaklinowanie: %.2f s po ruchu bez ruchu %.2f s z W, grunt pod obrysem niepewny (%s) - przesuniety o %.0f cm na pewny grunt "
					"(kierunek %d x 45 st. od przodu, sonda kolizji %.0f cm); poz (%.0f, %.0f, %.0f) -> (%.0f, %.0f, %.0f)", since,
					TRAV_STUCK.window, foot.c_str(), TravLen(q - p), a, shift, p.n[0], p.n[1], p.n[2], q.n[0], q.n[1], q.n[2]);
				return;
			}
		TravTeleportHero(p, "zaklinowanie - powrot (bez pewnego gruntu)");
		ZDebug("Zaklinowanie: %.2f s po ruchu bez ruchu z W, grunt niepewny (%s) - brak pewnego gruntu w 50 cm, bez zmian", since,
			foot.c_str());
	}

	void TraversalUpdate(float dt) {
		travClock += dt;
		TravStuckUpdate(dt);
		if (travRedrawPending && !ts.def && !tp.active && !tis.active)
			TravRedrawStep(dt);
		TravGuardSuspendTrack();
		TravPostWatchUpdate(dt);
		TravRootGuardUpdate(dt);
		TravFallTrack(dt);
		TravTrackUpdate(dt);
		TravFlightUpdate(dt);
		TravAirGrabSmoothUpdate(dt);
		TravAirGrabUpdate(dt);
		TravAirGrabTrace(dt);
		TravLedgeRunUpdate(dt);
		if (tis.active) {
			// wspiecie / przeskok z miejsca (klawisz przejety w TravIdleDecide)
			tis.active = false;
			int wm = 0;
			const char* why = nullptr;
			if (!ts.def && TravHeroCanStart(false, wm, why) && tis.def) {
				if (!TravBegin(tis.def, tis.res, tis.fwd))
					ZDebug("Przeskok %s: nie wystartowal z miejsca", tis.def->id.c_str());
			}
			else
				ZDebug("Przeskok z miejsca: start pominiety (%s)", why ? why : "bohater zajety");
		}
		if (travLedgePending) {
			// skok z miejsca gry po odmowie (werdykt LEDGE) - poza hakiem wejscia, w ktorym silnik liczy AI bohatera
			travLedgePending = false;
			TravLedgeJump("Alt");
		}
		if (zinput && !zinput->KeyPressed(TravJumpKey()) && !ts.def && !tp.active)
			travLatch = 0;
		if (!ts.def) {
			if (tp.active)
				TravPendingUpdate(dt);
			else
				TravHintUpdate(dt);
			return;
		}
		if (!player || player->IsDead() || !player->GetModel() || !ogame || !ogame->GetGameWorld()) {
			TraversalFinishNow();
			return;
		}
		{
			// 19.10: nowe Alt w oknie wyjscia (od exit - 0,3 s) = swiadomy lancuch kolejnego ruchu
			bool jd = zinput && zinput->KeyPressed(TravJumpKey()) != 0;
			if (jd && !ts.jumpPrev && ts.t >= ts.def->exit - 0.3f && !ts.chainAlt) {
				ts.chainAlt = true;
				ZDebug("Przeskok %s: Alt w oknie wyjscia (%.2f s) - lancuch kolejnego ruchu dozwolony", ts.def->id.c_str(), ts.t);
			}
			ts.jumpPrev = jd;
		}
		if (ts.cdOff && !ts.runExit && (player->collDetectionStatic || player->collDetectionDynamic)) {
			// 18.10: ktos wlaczyl kolizje w trakcie naszego toru (kolizja wypycha bohatera z toru do tylu) - znowu wylaczona
			if (!ts.cdOnLogged) {
				ts.cdOnLogged = true;
				ZDebug("Przeskok %s: kolizja wlaczona w trakcie ruchu po %.2f s (%d/%d, %s) - wylaczona ponownie", ts.def->id.c_str(), ts.t,
					player->collDetectionStatic ? 1 : 0, player->collDetectionDynamic ? 1 : 0,
					travCdOnBy.empty() ? "bez SetCollDetStat/Dyn - zapis pola" : travCdOnBy.c_str());
			}
			TravWhy w("tor przeskoku: kolizja wylaczona ponownie");
			player->SetCollDetStat(FALSE);
			player->SetCollDetDyn(FALSE);
		}
		if (!ts.keysLogged && !ts.runExit && zcfg.debug) {
			bool f = MvRawState(GAME_UP), b = MvRawState(GAME_DOWN), l = MvRawState(GAME_LEFT) || MvRawState(GAME_STRAFELEFT),
				r = MvRawState(GAME_RIGHT) || MvRawState(GAME_STRAFERIGHT), j = MvRawState(GAME_SMOVE);
			if (f || b || l || r || j) {
				ts.keysLogged = true;
				ZDebug("Przeskok %s: klawisze ruchu w trakcie animacji po %.2f s (W %d S %d lewo %d prawo %d skok %d) - ukryte przed gra "
					"(blokada ruchu %d)%s", ts.def->id.c_str(), ts.t, f, b, l, r, j, player->IsMovLock() ? 1 : 0,
					f && ts.t < ts.def->exit ? "; W przed oknem wyjscia w bieg" : "");
			}
		}
		TravMoveDiag();
		ts.t += dt;
		if (ts.ending >= 0.0f) {
			if (ts.runExit) {
				// bieg gry prowadzi voba w poziomie, wysokosc dochodzi do ziemi pod nim
				ts.exitT += dt;
				zVEC3 p = player->GetPositionWorld();
				float g;
				if (TravGroundAt(p, g)) {
					float want = g + ts.yOff;
					float y = ts.exitY0 + (want - ts.exitY0) * zdcore::TravSmooth01(ts.exitT / TRAV_RUN_EXIT_BLEND);
					// 24.10: bez kolizji (log NB 49177: to ustawienie z kolizja co klatke dawalo "zadane 0 cm, kolizja zmienila
					// o 325 cm" - nowe wypchniecie do tylu po kazdym cofnieciu straznika, ping-pong 3 m przez 0,12 s)
					if (fabsf(want - p.n[1]) <= TRAV_GROUND_SNAP && !ts.settled && fabsf(y - p.n[1]) > 0.1f)
						TravSetHeroNoColl(zVEC3(p.n[0], y, p.n[2]), "wyjscie w bieg: wysokosc do ziemi");
				}
			}
			else if (!TravStep())
				return;
			ts.ending -= dt;
			if (ts.ending <= 0.0f)
				TravFinish(false, nullptr);
			return;
		}
		int g = TravGuard(player->GetModel());
		if (g < 0) {
			TravFinish(true, "animacje przerwala gra (trafienie / reakcja)");
			return;
		}
		if (!TravStep())
			return;
		if (g == 0) {
			TravFinish(false, nullptr);
			return;
		}
		if (ts.t >= ts.def->exit && MvRawState(GAME_UP) && !MvRawState(GAME_DOWN) && player->GetWeaponMode() == ts.weaponMode) {
			// ziemia pod nogami w miejscu wyjscia (wspiecie: gora, przeskok: za przeszkoda) - inaczej dogrywa do postawy.
			// Runda 12: tylko gdy tor juz na wysokosci konca (stopy nie w przeszkodzie - kolizja wypychala do tylu)
			bool ready = zdcore::TravRunExitReady(*ts.def, ts.fit, ts.t);
			zVEC3 p = player->GetPositionWorld();
			float gy;
			if (ready && !ts.runExitBlocked && TravGroundAt(p, gy) && fabsf(p.n[1] - ts.yOff - gy) <= TRAV_GROUND_SNAP &&
				TravBeginRunExit())
				return;
			if (!ready && !ts.exitWaitLogged) {
				ts.exitWaitLogged = true;
				float x, y, z;
				zdcore::TravSample(*ts.def, ts.fit, ts.t, x, y, z);
				ZDebug("Przeskok %s: W od %.2f s - wyjscie w bieg czeka, stopy %.0f cm od wysokosci konca", ts.def->id.c_str(), ts.t,
					y - zdcore::TravEndLevel(*ts.def, ts.fit));
			}
		}
		if (ts.t >= ts.def->cancel - TRAV_END_BLEND)
			TravBeginEnding();
	}

	bool TraversalActive() {
		return ts.def != nullptr;
	}

	// 24.10: ruch bohatera prowadzi przeskok (tor, wyjscie w bieg) albo trwa skok gry z biegu (lot z pedem) - lokomocja W3
	// (Styles: plan nakladek, tempo sprintu W3) nic wtedy nie zmienia.
	bool TraversalHoldsMotion() {
		return ts.def != nullptr || tis.active || (tfl.airT >= 0.0f && tfl.runJump);
	}

	void TraversalFinishNow() {
		travFinishSaveLoad = true;
		if (ts.def)
			TravFinish(true, "zapis / wczytanie / przeladowanie");
		travFinishSaveLoad = false;
		if (travRedrawPending)
			ZDebug("Przeskok: bron (tryb %d) zostaje schowana - zapis / wczytanie przed powrotem do reki", travRedrawPending);
		travRedrawPending = 0;
		tp = TravPending();
		travLatch = 0;
		travLedgePending = false;
		tk = TravTrack();
		tlr = TravLedgeRun();
		tis = TravIdleStartReq();
		tfl = TravFlight();
		tag = TravAirGrabState();
		trg = TravRootGuard();
		tpw = TravPostWatch();
		travPostWin = 0.0f;
		travSrcLogs = 0;
		travExtLogs = 0;
		travGuardSkipLogs = 0;
		travCollLogs = 0;
		travCollStreak = 0;
		travCdOnBy.clear();
		tfall = TravFallState();
		travDbg.items.clear();
		travDbg.freeze = 0.0f;
	}

	int TraversalCount() {
		return (int)travDefs.size();
	}

	const std::vector<zdcore::TravDef>& TraversalDefs() {
		return travDefs;
	}

	void TraversalLoad() {
		travDefs.clear();
		static const char* names[] = { "\\SYSTEM\\ZDISMEMBER\\TRAVERSAL_W3.INI", "SYSTEM\\ZDISMEMBER\\TRAVERSAL_W3.INI" };
		std::string text;
		if (!ZReadGameFile(names, 2, text)) {
			ZLog("Przeskoki: brak traversal_w3.ini - przeskoki i wspinanie w biegu wylaczone");
			return;
		}
		std::vector<std::string> errors;
		auto defs = zdcore::ParseTraversal(text, errors);
		for (auto& e : errors)
			ZLog("Przeskoki (traversal_w3.ini): %s", e.c_str());
		for (auto& d : defs) {
			if (!ZOverlayExists(d.overlay, "T_1HPARADEJUMPB") || !ZOverlayExists(d.overlay, "T_CANNOTTAKE")) {
				ZLog("Przeskoki: %s - brak nakladki %s, pominiety", d.id.c_str(), d.overlay.c_str());
				continue;
			}
			travDefs.push_back(d);
		}
		travMaxEdge = 0.0f;
		for (auto& d : travDefs)
			travMaxEdge = d.edge > travMaxEdge ? d.edge : travMaxEdge;
		int idle = 0;
		for (auto& d : travDefs)
			idle += d.idle ? 1 : 0;
		ZLog("Przeskoki: bron w reku - TraversalSheathe=%d (chowana od razu na starcie, ruch bez broni; z miejsca z W), "
			"TraversalRedraw=%d (po wyladowaniu od razu w rece); pochodnia / tarcza / para NB w lewej rece - ruch z bronia jak dotad",
			zcfg.traversalSheathe ? 1 : 0, zcfg.traversalRedraw ? 1 : 0);
		ZLog("Przeskoki: wczytano %d, z miejsca %d (Traversal=%d, TraversalCombat=%d, TraversalAirGrab=%d, TraversalDebug=%d, "
			"klawisz skoku %d, sciana klipow do %.0f cm); chwyt w locie: zawis S_HANG, dlonie na krawedzi od chwytu; za krawedzia "
			"dach / stok do %.0f st.; dobieg bez krawedzi silnika: wspiecie z miejsca albo skok z miejsca gry; runda 9: straznik "
			"skoku korzenia animacji, krok lotu do limitu pedu, stan kolizji postaci po ruchu; 18.10: TeleportGuard=%d, korekty "
			"wstrzymane przy cofaniu czasu / przestawieniu z zewnatrz, klawisze ruchu ukryte w animacji",
			(int)travDefs.size(), idle, zcfg.traversal, zcfg.traversalCombat, zcfg.traversalAirGrab, zcfg.traversalDebug, TravJumpKey(),
			travMaxEdge, acosf(zdcore::TravLimits().walkMinNy) * 57.2958f, zcfg.teleportGuard ? 1 : 0);
		travJumpLogged[0] = travJumpLogged[1] = travJumpLogged[2] = -1.0f;
	}

	// ---------- Debug wizualny: wykrywanie co 0,1 s, rysowanie co klatke ----------
	static bool TravSprinting(zCModel* m) {
		for (int i = 1; m && i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p)
				continue;
			std::string n = zdcore::Upper(p->modelProtoName.ToChar());
			size_t dot = n.find('.');
			if (zdcore::IsForeignSprintOverlay(dot == std::string::npos ? n : n.substr(0, dot)))
				return true;
		}
		return false;
	}

	// Co bylo by po Alt teraz (bez skutkow): klip i start albo odmowa ze skokiem gry wg werdyktu.
	static void TravDebugProbe() {
		zVEC3 fwd = TravHeading(), origin = player->GetPositionWorld();
		std::vector<zdcore::TravRayRec> rays;
		int wm = 0, wmIdle = 0;
		const char* why = nullptr;
		const char* whyIdle = nullptr;
		bool can = TravHeroCanStart(true, wm, why);
		// z miejsca / z wolnego chodu (bez broni): klipy idle, szukanie blisko
		bool idle = !can && why && TravHeroCanStart(false, wmIdle, whyIdle) && wmIdle == NPC_WEAPON_NONE;
		travDbg.range = idle ? TRAV_TIMING.idleLook : zdcore::TravLookahead(heroSpeed, travMaxEdge, TRAV_TIMING);
		zdcore::TravResult res = TravProbe(fwd, nullptr, idle ? TRAV_TIMING.idleLook : -1.0f, 0.0f, 0.0f, &rays, &origin);
		const zdcore::TravDef* def = TravPickHere(res, fwd, idle, nullptr, &rays);
		std::string decision = zdcore::TravDecisionLabel(res, def);
		bool refused = true;
		if (def) {
			zdcore::TravStart st = idle ? zdcore::TravIdleStart(res.face, TRAV_TIMING) :
				zdcore::TravPlanStart(res.face, def->edge, heroSpeed, false, TRAV_TIMING);
			decision += std::string(" - ") + TravStartName(st);
			refused = st == zdcore::TRAVSTART_TOO_FAR || st == zdcore::TRAVSTART_TOO_CLOSE;
		}
		if (idle && refused)
			decision += " | Alt teraz: z miejsca - skok gry (zawis / wspiecie Gothica)";
		else if (!can && !idle)
			decision += std::string(" | Alt teraz: ") + (why ? why : "niedostepne") + " - skok gry";
		if (can && refused && wm == NPC_WEAPON_NONE) {
			zdcore::TravGameJumpParams jp = TravJumpParams(origin.n[1]);
			zdcore::TravGameJumpInfo g = zdcore::TravGameJumpVerdict(res, jp);
			TravDebugSetItems(res, def, &rays, origin, fwd, decision, &jp, &g);
		}
		else
			TravDebugSetItems(res, def, &rays, origin, fwd, decision, nullptr, nullptr);
	}

	static zCOLOR TravColor(unsigned c) {
		return zCOLOR((unsigned char)((c >> 16) & 255), (unsigned char)((c >> 8) & 255), (unsigned char)(c & 255), 255);
	}

	// Punkt elementu (uklad bohatera z chwili wykrywania) w swiecie.
	static zVEC3 TravDbgWorld(const float* a) {
		const zVEC3& f = travDbg.fwd;
		return zVEC3(travDbg.o.n[0] + f.n[2] * a[0] + f.n[0] * a[2], travDbg.o.n[1] + a[1], travDbg.o.n[2] - f.n[0] * a[0] + f.n[2] * a[2]);
	}

	// Odcinek swiata -> piksele (przyciety do plaszczyzny tuz przed kamera); false = caly za kamera / poza zakresem.
	static bool TravDbgSegment(zCCamera* cam, const zVEC3& a, const zVEC3& b, float& x1, float& y1, float& x2, float& y2) {
		const float nearZ = 10.0f;
		zVEC3 ca = cam->camMatrix * a, cb = cam->camMatrix * b;
		if (ca.n[2] < nearZ && cb.n[2] < nearZ)
			return false;
		if (ca.n[2] < nearZ || cb.n[2] < nearZ) {
			float t = (nearZ - ca.n[2]) / (cb.n[2] - ca.n[2]);
			zVEC3 c(ca.n[0] + (cb.n[0] - ca.n[0]) * t, ca.n[1] + (cb.n[1] - ca.n[1]) * t, nearZ);
			if (ca.n[2] < nearZ)
				ca = c;
			else
				cb = c;
		}
		cam->Project(&ca, x1, y1);
		cam->Project(&cb, x2, y2);
		return fabsf(x1) < 20000.0f && fabsf(y1) < 20000.0f && fabsf(x2) < 20000.0f && fabsf(y2) < 20000.0f;
	}

	// Napis w pikselach (center: znacznik wysrodkowany na punkcie) - zCView (widoczny tez pod GD3D11).
	static void TravDbgPrint(float px, float py, const std::string& s, unsigned color, bool center) {
		zSTRING t(s.c_str());
		int x = screen->anx((int)px), y = screen->any((int)py);
		if (center) {
			x -= screen->FontSize(t) / 2;
			y -= screen->FontY() / 2;
		}
		else
			y -= screen->FontY();
		if (x < 0 || y < 0 || x > 8100 || y > 8100)
			return;
		screen->SetFontColor(TravColor(color));
		screen->Print(x, y, t);
	}

	static void TravDebugDraw() {
		zCCamera* cam = zCCamera::activeCam;
		if (!cam || !screen)
			return;
		cam->Activate();
		for (auto& it : travDbg.items) {
			if (it.kind == zdcore::TRAVDRAW_LINE) {
				float x1, y1, x2, y2;
				if (!TravDbgSegment(cam, TravDbgWorld(it.a), TravDbgWorld(it.b), x1, y1, x2, y2))
					continue;
				if (travDbg.lines && zrenderer)
					zrenderer->DrawLine(x1, y1, x2, y2, TravColor(it.color));
				else {
					float len = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
					int n = (int)(len / 14.0f);
					n = n < 2 ? 2 : (n > 14 ? 14 : n);
					for (int k = 0; k <= n; k++)
						TravDbgPrint(x1 + (x2 - x1) * k / n, y1 + (y2 - y1) * k / n, ".", it.color, true);
				}
				continue;
			}
			zVEC3 v = cam->camMatrix * TravDbgWorld(it.a);
			if (v.n[2] < 10.0f)
				continue;
			float px = 0.0f, py = 0.0f;
			cam->Project(&v, px, py);
			TravDbgPrint(px, py, it.text, it.color, it.kind == zdcore::TRAVDRAW_MARK);
		}
		// linia stanu i decyzja w rogu ekranu
		std::string buf = ts.def ? "ruch " + ts.def->id : tp.active ? "czeka" : "-";
		if (tp.active) {
			char b[32];
			sprintf_s(b, "czeka %.2f s", tp.time);
			buf = b;
		}
		else if (tlr.active && !ts.def) {
			char b[64];
			sprintf_s(b, "dobieg do krawedzi %.0f cm (%.2f s)", tlr.face, tlr.t);
			buf = b;
		}
		zCModel* m = player->GetModel();
		int y = 1100, dy = screen->FontY() + 20;
		screen->SetFontColor(TravColor(zdcore::TRAVCOL_TEXT));
		screen->Print(150, y, zSTRING(zdcore::TravStatusLine(heroSpeed, TravSprinting(m), travDbg.range, buf.c_str()).c_str()));
		char head[48] = "teraz";
		if (travDbg.freeze > 0.0f)
			sprintf_s(head, "zamrozone %.1f s", travDbg.freeze);
		screen->SetFontColor(TravColor(travDbg.freeze > 0.0f ? zdcore::TRAVCOL_TOP : zdcore::TRAVCOL_TEXT));
		screen->Print(150, y + dy, zSTRING((std::string(head) + ": " + travDbg.decision).c_str()));
		int line = 2;
		if (!travDbg.jump.empty()) {
			screen->SetFontColor(TravColor(zdcore::TRAVCOL_JUMP_NO));
			screen->Print(150, y + line++ * dy, zSTRING(travDbg.jump.c_str()));
		}
		if (tag.infoT > 0.0f && !tag.info.empty()) {
			screen->SetFontColor(TravColor(zdcore::TRAVCOL_JUMP_LEDGE));
			screen->Print(150, y + line++ * dy, zSTRING(tag.info.c_str()));
		}
		screen->SetFontColor(TravColor(zdcore::TRAVCOL_TEXT));
	}

	// ---------- Tor przeszkod do testow (zdism trav tor): przeszkody z modeli gry przed bohaterem (uklad: Core
	// CourseBuild, wysokosci z bboxow wizuali). Voby z kolizja dynamiczna (bohater, NPC i promienie wykrywania je widza),
	// dontWriteIntoArchive (zCWorld::ArcTraverseVobs 0x627553 pomija vob z bitem 0x10 w +0x114 - nie trafiaja do zapisu
	// ani do swiata zostawianego przy zmianie poziomu). Trzymamy wlasna referencje: przy wczytaniu / zmianie swiata
	// (LoadBegin) i "tor usun" - RemoveVob, gdy jeszcze w swiecie, i Release. Napisy przy przeszkodach co klatke. ----------
	struct TravCourseLabel {
		zVEC3 pos;
		std::string text;
	};
	struct TravCourse {
		std::vector<zCVob*> vobs;
		std::vector<TravCourseLabel> labels;
		zCWorld* world = nullptr;
	};
	static TravCourse tcourse;
	static const float TRAV_COURSE_LABEL_DIST = 3500.0f;   // napisy przeszkod do tylu cm od bohatera

	void TraversalCourseClear() {
		int n = 0;
		for (zCVob* v : tcourse.vobs) {
			if (!v)
				continue;
			if (v->homeWorld)
				v->homeWorld->RemoveVob(v);
			v->Release();
			n++;
		}
		if (n)
			ZLog("Tor przeszkod: usuniety (%d vobow)", n);
		tcourse = TravCourse();
	}

	// Bbox wizualu (vob probny - ten sam wzorzec co Gibs: SetVisual, GetVisual, Release); false = brak modelu.
	static bool TravCourseBBox(const std::string& visual, zdcore::CoursePiece& p) {
		zCVob* v = new zCVob();
		v->SetVisual(zSTRING(visual.c_str()));
		zCVisual* vis = v->GetVisual();
		bool ok = vis != nullptr;
		if (ok) {
			zTBBox3D bb = vis->GetBBox3D();
			p.visual = visual;
			p.minX = bb.mins.n[0];
			p.minY = bb.mins.n[1];
			p.minZ = bb.mins.n[2];
			p.maxX = bb.maxs.n[0];
			p.maxY = bb.maxs.n[1];
			p.maxZ = bb.maxs.n[2];
			ok = p.SizeX() > 1.0f && p.SizeY() > 1.0f && p.SizeZ() > 1.0f;
		}
		v->Release();
		return ok;
	}

	static std::string TravCourseBuild(float startOffset) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !player)
			return "brak swiata / bohatera";
		TraversalCourseClear();
		zVEC3 fwd = TravHeading(), right(fwd.n[2], 0.0f, -fwd.n[0]), hp = player->GetPositionWorld();
		zVEC3 o(hp.n[0] + fwd.n[0] * startOffset, hp.n[1], hp.n[2] + fwd.n[2] * startOffset);
		float g0;
		if (!TravGroundAt(o, g0))
			return "brak ziemi przed bohaterem";
		o.n[1] = g0;
		// modele i bboxy
		std::vector<zdcore::CoursePiece> cat;
		std::string missing;
		for (auto& v : zdcore::CourseVisuals()) {
			zdcore::CoursePiece p;
			if (TravCourseBBox(v, p))
				cat.push_back(p);
			else
				missing += (missing.empty() ? "" : ", ") + v;
		}
		zdcore::CoursePlan plan = zdcore::CourseBuild(cat);
		if (plan.obstacles.empty())
			return "brak modeli toru (" + missing + ")";
		auto at = [&](float x, float z, float y) {
			return zVEC3(o.n[0] + right.n[0] * x + fwd.n[0] * z, y, o.n[2] + right.n[2] * x + fwd.n[2] * z);
		};
		auto groundAt = [&](float x, float z, float& gy) {
			zVEC3 p = at(x, z, g0 + 600.0f);
			if (!MvTrace(p, zVEC3(0.0f, -1500.0f, 0.0f), player, nullptr, TRAV_TRACE))
				return false;
			gy = ogame->GetGameWorld()->traceRayReport.foundIntersection.n[1];
			return true;
		};
		// pas: plaski i pusty? (przed postawieniem przeszkod)
		float worstDev = 0.0f, worstStep = 0.0f;
		int noGround = 0;
		std::string blocked;
		int blockedN = 0;
		for (float x : { plan.minX, -500.0f, 0.0f, 500.0f, plan.maxX }) {
			float prev = g0;
			for (float z = 0.0f; z <= plan.length + 1.0f; z += 200.0f) {
				float gy;
				if (!groundAt(x, z, gy)) {
					noGround++;
					continue;
				}
				worstDev = std::fmax(worstDev, fabsf(gy - g0));
				worstStep = std::fmax(worstStep, fabsf(gy - prev));
				prev = gy;
				for (float h : { 50.0f, 150.0f }) {
					zVEC3 from = at(x, z, gy + h);
					if (MvTrace(from, zVEC3(fwd.n[0] * 200.0f, 0.0f, fwd.n[2] * 200.0f), player, nullptr,
						zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN)) {
						if (blockedN++ < 6) {
							char b[200];
							sprintf_s(b, "%s %.0f m / rzad %+.0f m: %s", blocked.empty() ? "" : ";", z / 100.0f, x / 100.0f,
								TravHitDesc(ogame->GetGameWorld()->traceRayReport).c_str());
							blocked += b;
						}
						break;
					}
				}
			}
		}
		// voby
		int made = 0, failed = 0;
		std::vector<std::string> measured;
		for (auto& ob : plan.obstacles) {
			// 24.10: ziemia przed przeszkoda (tam stoi bohater; wysokosci opisu sa od niej) - dawniej srodek obrysu (przy
			// chwycie w locie 7 m dalej, w przepasci / na innym terenie)
			float gy;
			if (!groundAt(ob.x, ob.z - 60.0f, gy) && !groundAt(ob.x, ob.z + ob.depth * 0.5f, gy))
				gy = g0;
			for (auto& part : ob.parts) {
				zCVob* v = new zCVob();
				v->SetVobName("ZDISM_TOR");
				v->SetVisual(zSTRING(part.visual.c_str()));
				if (!v->GetVisual()) {
					v->Release();
					failed++;
					continue;
				}
				float yaw = part.yawDeg * 3.14159265f / 180.0f, pitch = part.pitchDeg * 3.14159265f / 180.0f;
				zVEC3 f2(fwd.n[0] * cosf(yaw) + right.n[0] * sinf(yaw), 0.0f, fwd.n[2] * cosf(yaw) + right.n[2] * sinf(yaw));
				zVEC3 r2(f2.n[2], 0.0f, -f2.n[0]);
				float c = cosf(pitch), s = sinf(pitch);
				zVEC3 up(-f2.n[0] * s, c, -f2.n[2] * s), at2(f2.n[0] * c, s, f2.n[2] * c);   // lokalne y / z po pochyleniu
				zMAT4 m = Alg_Identity3D();
				m.SetRightVector(r2);
				m.SetUpVector(up);
				m.SetAtVector(at2);
				m.SetTranslation(at(part.x, part.z, gy + part.y));
				v->SetTrafoObjToWorld(m);
				v->dontWriteIntoArchive = TRUE;
				world->AddVob(v);
				v->SetCollDetStat(FALSE);
				v->SetCollDetDyn(TRUE);
				tcourse.vobs.push_back(v);   // nasza referencja (Release przy usuwaniu)
				made++;
			}
			// 24.10: pomiar po postawieniu (promienie w dol na osi rzedu): krawedz w miejscu testu nad ziemia przed przeszkoda
			// i profil gory co 50 cm (glebokosc podestu, nawis, kat dachu) - opis -> zmierzone w logu i na napisie
			float fh = ob.featureH >= 0.0f ? ob.featureH : ob.height;
			auto topAt = [&](float dz, float& h) {
				zVEC3 p = at(ob.x, ob.z + dz, gy + fh + 400.0f);
				if (!MvTrace(p, zVEC3(0.0f, -(fh + 400.0f + 50.0f), 0.0f), player, nullptr, TRAV_TRACE))
					return false;
				h = ogame->GetGameWorld()->traceRayReport.foundIntersection.n[1] - gy;
				return true;
			};
			float edge = -1.0f;
			bool edgeOk = topAt(ob.featureZ + 15.0f, edge);
			std::string prof;
			float prevH = 0.0f, maxSlope = 0.0f;
			for (int i = 0; i <= 8; i++) {
				float dz = ob.featureZ + 15.0f + 50.0f * i, h;
				char pb[32];
				if (topAt(dz, h)) {
					sprintf_s(pb, "%s%.0f:%.0f", prof.empty() ? "" : " ", dz - ob.featureZ, h);
					if (i > 0 && fabsf(h - prevH) < 60.0f)
						maxSlope = fmaxf(maxSlope, atanf(fabsf(h - prevH) / 50.0f) * 57.2958f);
					prevH = h;
				}
				else
					sprintf_s(pb, "%s%.0f:-", prof.empty() ? "" : " ", dz - ob.featureZ);
				prof += pb;
			}
			// szczelina nad gora (nawis): promien w gore z 5 cm nad gora przy krawedzi
			float headroom = -1.0f;
			if (edgeOk) {
				zVEC3 p = at(ob.x, ob.z + ob.featureZ + 15.0f, gy + edge + 5.0f);
				if (MvTrace(p, zVEC3(0.0f, 300.0f, 0.0f), player, nullptr, TRAV_TRACE))
					headroom = ogame->GetGameWorld()->traceRayReport.foundIntersection.n[1] - (gy + edge);
			}
			char mb[420];
			sprintf_s(mb, "Tor przeszkod [pomiar] %s: opis %.0f cm -> krawedz %s cm nad ziemia przed przeszkoda (%.0f m od przodu), "
				"miejsce nad gora %s, nachylenie do %.0f st.; profil gory (cm od krawedzi:wysokosc) %s", ob.name.c_str(), fh,
				edgeOk ? std::to_string((int)lroundf(edge)).c_str() : "brak", ob.featureZ / 100.0f, headroom < 0.0f ? "wolne do 3 m" :
				(std::to_string((int)lroundf(headroom)) + " cm").c_str(), maxSlope, prof.c_str());
			measured.push_back(mb);
			char lb[200];
			if (edgeOk && fabsf(edge - fh) > 10.0f)
				sprintf_s(lb, "%s (opis %.0f, zmierzone %.0f cm)", ob.name.c_str(), fh, edge);
			else
				sprintf_s(lb, "%s (%.0f cm)", ob.name.c_str(), edgeOk ? edge : fh);
			tcourse.labels.push_back({ at(ob.x, ob.z + ob.featureZ, gy + fh + 40.0f), lb });
		}
		tcourse.world = world;
		// log: lista przeszkod (odleglosc od startu toru, rzad, wysokosc z bboxow, modele, czego sie spodziewac)
		ZLog("Tor przeszkod: %d przeszkod, %d vobow (start %.0f m przed bohaterem, pas %.0f m x %.0f m, poz (%.0f, %.0f, %.0f), "
			"kierunek (%.2f, %.2f)); teren: odchylka do %.0f cm, skok miedzy probkami do %.0f cm%s%s%s", (int)plan.obstacles.size(),
			made, startOffset / 100.0f, plan.length / 100.0f, (plan.maxX - plan.minX) / 100.0f, o.n[0], o.n[1], o.n[2], fwd.n[0],
			fwd.n[2], worstDev, worstStep, noGround ? (", bez ziemi " + std::to_string(noGround) + " probek").c_str() : "",
			failed ? (", nie powstalo " + std::to_string(failed) + " vobow").c_str() : "", missing.empty() ? "" :
			(", brak modeli: " + missing).c_str());
		for (auto& ob : plan.obstacles)
			ZLog("Tor przeszkod: %s", zdcore::CourseDescribe(ob).c_str());
		for (auto& m : measured)
			ZLog("%s", m.c_str());
		for (auto& s : plan.skipped)
			ZLog("Tor przeszkod: pominiete - %s", s.c_str());
		if (blockedN)
			ZLog("Tor przeszkod: UWAGA pas nie jest pusty (%d trafien promieni na 50 / 150 cm):%s", blockedN, blocked.c_str());
		std::string warn;
		if (worstDev > 150.0f || worstStep > 60.0f)
			warn += " UWAGA: teren nierowny (odchylka " + std::to_string((int)worstDev) + " cm, skok " + std::to_string((int)worstStep) +
				" cm) - przeszkody stoja na ziemi przy kazdej z osobna";
		if (blockedN)
			warn += " UWAGA: w pasie cos stoi (" + std::to_string(blockedN) + " trafien, lista w logu)";
		if (noGround)
			warn += " UWAGA: brak ziemi w " + std::to_string(noGround) + " probkach";
		char sum[220];
		sprintf_s(sum, "tor: %d przeszkod w 4 rzedach od lewej (stopnie 30-150, stopnie 200-350 - gora 6 m; murek, plot, beczka, "
			"slup, nawis, stos skrzyn; dachy 30 / 45, przepasc, chwyt w locie), %.0f m; lista w zDismember.log",
			(int)plan.obstacles.size(), plan.length / 100.0f);
		return std::string(sum) + warn;
	}

	// Napisy przy przeszkodach toru (nazwa i wysokosc) - co klatke, do TRAV_COURSE_LABEL_DIST od bohatera.
	static void TravCourseDraw() {
		zCCamera* cam = zCCamera::activeCam;
		if (tcourse.labels.empty() || !cam || !screen || !player || !ogame || tcourse.world != ogame->GetGameWorld())
			return;
		zVEC3 hp = player->GetPositionWorld();
		cam->Activate();
		for (auto& l : tcourse.labels) {
			if (TravLen(l.pos - hp) > TRAV_COURSE_LABEL_DIST)
				continue;
			zVEC3 v = cam->camMatrix * l.pos;
			if (v.n[2] < 10.0f)
				continue;
			float px = 0.0f, py = 0.0f;
			cam->Project(&v, px, py);
			TravDbgPrint(px, py, l.text, zdcore::TRAVCOL_TOP, true);
		}
	}

	void TraversalDebugUpdate(float dt) {
		TravCourseDraw();
		if ((int)zcfg.traversalDebug != travDbgIni) {
			travDbgIni = (int)zcfg.traversalDebug;
			TravDebugSet(zcfg.traversalDebug, "TraversalDebug w Gothic.ini");
		}
		if (!travDbg.on || !player || !player->GetModel() || !ogame || !ogame->GetGameWorld())
			return;
		if (travDbg.freeze > 0.0f)
			travDbg.freeze -= dt;
		else if (!ts.def && (travDbg.probe -= dt) <= 0.0f) {
			travDbg.probe = TRAV_DBG_PROBE;
			TravDebugProbe();
		}
		TravDebugDraw();
	}

	std::string TraversalConsole(const std::string& argIn) {
		std::string arg = zdcore::Upper(argIn);
		if (arg == "SHOW") {
			TravDebugSet(true, "zdism trav show");
			return std::string("debug przeskokow wlaczony (") + (travDbg.lines ? "linie" : "kropki") +
				"; zdism trav kropki - linie z kropek, zdism trav hide - wylacz)";
		}
		if (arg == "HIDE") {
			TravDebugSet(false, "zdism trav hide");
			return "debug przeskokow wylaczony";
		}
		if (arg == "KROPKI" || arg == "DOTS" || arg == "LINIE" || arg == "LINES") {
			travDbg.lines = arg == "LINIE" || arg == "LINES";
			TravDebugSet(true, "zdism trav kropki / linie");
			ZLog("Przeskok debug: linie %s", travDbg.lines ? "przez zrenderer->DrawLine" : "z kropek (napisy zCView)");
			return std::string("debug przeskokow: linie ") + (travDbg.lines ? "(DrawLine)" : "z kropek");
		}
		if (arg == "TOR USUN" || arg == "TOR DEL" || arg == "TOR REMOVE") {
			bool had = !tcourse.vobs.empty();
			TraversalCourseClear();
			return had ? "tor usuniety" : "brak toru";
		}
		if (arg.compare(0, 3, "TOR") == 0) {
			// zdism trav tor [m] - start toru tyle metrow przed bohaterem (domyslnie 2)
			float m = 2.0f;
			if (arg.size() > 4)
				m = (float)atof(arg.c_str() + 4);
			if (m < 0.0f || m > 50.0f)
				m = 2.0f;
			if (ts.def)
				return "ruch trwa - najpierw koniec przeskoku";
			return TravCourseBuild(m * 100.0f);
		}
		if (arg == "LIST") {
			std::string s = "przeskoki:";
			for (auto& d : travDefs)
				s += " " + d.id;
			return travDefs.empty() ? "brak traversal_w3.ini (paczka W3)" : s;
		}
		if (!player || !ogame || !ogame->GetGameWorld())
			return "brak bohatera";
		if (ts.def)
			return "ruch trwa: " + ts.def->id;
		zVEC3 fwd = TravHeading();
		std::string diag;
		zdcore::TravResult res = TravProbe(fwd, &diag, TRAV_TIMING.lookMax);   // konsola: pelny zasieg (bohater stoi)
		ZLog("zdism trav: %s; %s", TravHeroState().c_str(), diag.c_str());
		std::string s = zdcore::TravDescribe(res);
		const zdcore::TravDef* def = nullptr;
		if (res.kind != zdcore::TRAV_NONE) {
			bool left = TravSwingLeftNow();
			// konsola: z biegu, a gdy brak - z miejsca (bohater zwykle stoi przy wpisywaniu)
			zdcore::TravResult runRes = res;
			def = TravPickHere(runRes, fwd, false);
			if (!def && res.face <= TRAV_TIMING.idleMaxFace) {
				def = TravPickHere(res, fwd, true);
				if (def)
					s += " (z miejsca)";
			}
			else
				res = runRes;
			if (def) {
				zdcore::TravStart st = def->idle ? zdcore::TravIdleStart(res.face, TRAV_TIMING) :
					zdcore::TravPlanStart(res.face, def->edge, heroSpeed, false, TRAV_TIMING);
				char buf[160];
				sprintf_s(buf, " -> %s (sciana w klipie %.0f cm, k %.2f): %s", def->id.c_str(), def->edge, res.face / def->edge,
					TravStartName(st));
				s += buf;
			}
			else
				s += std::string(" -> brak klipu (noga ") + (left ? "L" : "R") + ")";
		}
		int wm = 0;
		const char* why = nullptr;
		bool ok = TravHeroCanStart(true, wm, why);
		char state[96];
		sprintf_s(state, "; bieg: %s (predkosc %.0f)", ok ? "tak" : (why ? why : "nie"), heroSpeed);
		s += state;
		ZLog("zdism trav: %s", s.c_str());
		if (arg == "GO") {
			if (!def)
				return s + " - nie ma czego wykonac";
			if (!TravHeroCanStart(false, wm, why))
				return s + " - teraz nie mozna" + (why ? std::string(" (") + why + ")" : "");
			return TravBegin(def, res, fwd) ? "start " + def->id : "nie wystartowal (log)";
		}
		return s;
	}

	// ---------- Diagnostyka: W trzymane, a bohater nie rusza do przodu ----------
	// Zgloszenie 21.10: czasem po animacji przez kilka sekund nie dziala ruch do przodu (w boki tak). Po FWD_DIAG_AFTER s z W
	// (bez S) i predkoscia ponizej FWD_DIAG_SPEED - poza naszymi ruchami, finisherem, podnoszeniem i oknami - jeden wpis ze
	// stanem bohatera: animacje, nakladki, kolejka zdarzen, blokada ruchu, co gra widzi z W, miejsce i przeszkoda przed nim;
	// po odblokowaniu czas trwania. Najwyzej FWD_DIAG_MAX wpisow na sesje.
	static const float FWD_DIAG_AFTER = 0.6f, FWD_DIAG_SPEED = 40.0f;
	static const int FWD_DIAG_MAX = 30;
	static float fwdDiagT = 0.0f, fwdDiagSpeed = 0.0f;
	static bool fwdDiagLogged = false, fwdDiagHavePrev = false;
	static int fwdDiagCount = 0;
	static zVEC3 fwdDiagPrev;

	static std::string FwdDiagEm() {
		zCEventManager* em = player->GetEM(FALSE);
		if (!em)
			return "brak";
		int n = em->GetNumMessages();
		if (n <= 0)
			return "pusta";
		std::string s = std::to_string(n) + ":";
		for (int i = 0; i < n && i < 4; i++) {
			zCEventMessage* m = em->messageList[i];
			zCClassDef* cd = m ? m->_GetClassDef() : nullptr;
			char buf[96];
			sprintf_s(buf, " %s/%d", cd ? cd->className.ToChar() : "?", m ? (int)m->subType : -1);
			s += buf;
		}
		return s;
	}

	static std::string FwdDiagOverlays(zCModel* m) {
		std::string s;
		int n = m->modelProtoList.GetNum();
		for (int i = n - 1; i >= 0 && i >= n - 5; i--)
			if (zCModelPrototype* p = m->modelProtoList[i])
				s += (s.empty() ? "" : " ") + std::string(p->modelProtoName.ToChar());
		return s;
	}

	// Promien 100 cm przed bohatera na wysokosci h nad srodkiem voba (postacie tez) - opis trafienia albo "nic".
	static std::string FwdDiagRay(const zVEC3& p, const zVEC3& fwd, float h) {
		zVEC3 from(p.n[0], p.n[1] + h, p.n[2]);
		if (!MvTrace(from, zVEC3(fwd.n[0] * 100.0f, 0.0f, fwd.n[2] * 100.0f), player, nullptr, MOVE_TRACE))
			return "nic";
		const zTTraceRayReport& rep = ogame->GetGameWorld()->traceRayReport;
		zVEC3 d = rep.foundIntersection - from;
		char buf[32];
		sprintf_s(buf, "%.0f cm: ", d.Length());
		std::string who = rep.foundVob && zDYNAMIC_CAST<oCNpc>(rep.foundVob) ?
			std::string("NPC ") + static_cast<oCNpc*>(rep.foundVob)->GetInstanceName().ToChar() + (static_cast<oCNpc*>(rep.foundVob)->IsDead() ? " (martwy)" : "") :
			TravHitDesc(rep);
		return buf + who;
	}

	void HeroForwardDiagUpdate(float dt) {
		if (!player || !ogame || !ogame->GetGameWorld() || player->IsDead() || !player->GetModel() || !player->GetAnictrl() ||
			fwdDiagCount >= FWD_DIAG_MAX || dt <= 0.0f) {
			fwdDiagHavePrev = false;
			return;
		}
		zVEC3 p = player->GetPositionWorld();
		if (fwdDiagHavePrev) {
			float dx = p.n[0] - fwdDiagPrev.n[0], dz = p.n[2] - fwdDiagPrev.n[2];
			fwdDiagSpeed = fwdDiagSpeed * 0.7f + sqrtf(dx * dx + dz * dz) / dt * 0.3f;
		}
		fwdDiagPrev = p;
		fwdDiagHavePrev = true;
		bool w = MvRawState(GAME_UP) && !MvRawState(GAME_DOWN);
		// nasze ruchy (unik, przeskok) nie wykluczaja - zawieszony ruch tez moze trzymac bohatera (opis w logu)
		bool busy = DirectorPlaying() || LootActive() || (zcon && zcon->IsVisible()) || player->inventory2.IsOpen() ||
			!oCInformationManager::GetInformationManager().HasFinished() || player->IsUnconscious();
		if (!w || busy || fwdDiagSpeed >= FWD_DIAG_SPEED) {
			if (fwdDiagLogged)
				ZLog("W: bohater rusza po %.1f s (%s)", fwdDiagT, !w ? "W puszczone" : busy ? "finisher / podnoszenie / okno" : "jedzie");
			fwdDiagT = 0.0f;
			fwdDiagLogged = false;
			return;
		}
		fwdDiagT += dt;
		if (fwdDiagLogged || fwdDiagT < FWD_DIAG_AFTER)
			return;
		fwdDiagLogged = true;
		fwdDiagCount++;
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		zVEC3 fwd = TravHeading();
		int ours = MovesGameKeyOverride(GAME_UP, false);
		float seen = zinput ? zinput->GetState(GAME_UP) : -1.0f;
		ZLog("W bez ruchu %.1f s (%d/%d): stan %d (body 0x%X), ruch silnika %d, walkmode %d, tryb broni %d, blokada %d, "
			"predkosc %.0f cm/s; W: surowe 1, gra widzi %.0f, nasz hak %d; miejsce przod/lewo/prawo %d/%d/%d; kolejka %s",
			fwdDiagT, fwdDiagCount, FWD_DIAG_MAX, player->GetBodyState() & BS_ONLY_STATE, player->GetBodyState(), (int)ac->state,
			ac->walkmode, player->GetWeaponMode(), player->IsMovLock() ? 1 : 0, fwdDiagSpeed, seen, ours,
			ac->CheckEnoughSpaceMoveForward(TRUE), ac->CheckEnoughSpaceMoveLeft(TRUE), ac->CheckEnoughSpaceMoveRight(TRUE),
			FwdDiagEm().c_str());
		ZLog("W bez ruchu: animacje %s; nakladki (od gory) %s; przed bohaterem: pas %s | kolana %s; wrog %s; nasz ruch %s, "
			"przeskok %s (bufor %d), finisher %s",
			AnisDesc(m).c_str(), FwdDiagOverlays(m).c_str(), FwdDiagRay(p, fwd, 0.0f).c_str(), FwdDiagRay(p, fwd, -50.0f).c_str(),
			player->enemy ? player->enemy->GetInstanceName().ToChar() : "-", ms.def ? ms.def->id.c_str() : "-",
			ts.def ? ts.def->id.c_str() : "-", tp.active ? 1 : 0, DirectorActive() ? "Settle" : "-");
	}
}
