#include "Moves.h"
#include "Catalog.h"
#include "Text.h"
#include <cstdlib>

namespace zdcore {
	static const char* const kDirSuffix[] = { "B", "F", "L", "R" };

	std::vector<MoveDef> ParseMoves(const std::string& text, std::vector<std::string>& errors) {
		std::vector<MoveDef> out;
		MoveDef cur;
		bool inSection = false, bad = false;
		std::string badReason;

		auto close = [&]() {
			if (!inSection)
				return;
			if (!bad && (cur.overlay.empty() || cur.traj.empty())) {
				bad = true;
				badReason = "brak overlay albo traj";
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
				cur = MoveDef();
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
			if (key == "OVERLAY")
				ok = !(cur.overlay = val).empty();
			else if (key == "OVERLAYW3")
				ok = !(cur.overlayW3 = val).empty();
			else if (key == "TRAJFPS") {
				char* e = nullptr;
				cur.trajFps = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.trajFps > 0.0f;
			}
			else if (key == "TRAJ")
				ok = ParseTraj(val, cur.traj);
			else if (key == "IFRAMES") {
				float w[2];
				ok = ParseFloatList(val, w, 2) && w[0] >= 0.0f && w[1] >= w[0];
				if (ok) {
					cur.iframeFrom = w[0];
					cur.iframeTo = w[1];
				}
			}
			else if (key == "HIT") {
				char* e = nullptr;
				cur.hitTime = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.hitTime >= 0.0f;
			}
			else if (key == "CANCEL") {
				char* e = nullptr;
				cur.cancel = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.cancel > 0.0f;
			}
			else if (key == "REACH") {
				char* e = nullptr;
				cur.reach = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.reach >= 0.0f;
			}
			else if (key == "EXIT") {
				char* e = nullptr;
				cur.exitTime = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.exitTime >= 0.0f;
			}
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

	const MoveDef* FindMove(const std::vector<MoveDef>& defs, const std::string& kind, MoveDir dir) {
		std::string id = Upper(kind) + "_" + kDirSuffix[dir];
		for (auto& d : defs)
			if (d.id == id)
				return &d;
		return nullptr;
	}

	std::vector<int> ParseKeyBinding(const std::string& hex) {
		std::vector<int> out;
		for (size_t i = 0; i + 4 <= hex.size(); i += 4) {
			char lo[3] = { hex[i], hex[i + 1], 0 }, hi[3] = { hex[i + 2], hex[i + 3], 0 };
			char* e1 = nullptr;
			char* e2 = nullptr;
			long l = strtol(lo, &e1, 16), h = strtol(hi, &e2, 16);
			if (*e1 || *e2)
				return {};
			int code = (int)(l | (h << 8));
			if (code)
				out.push_back(code);
		}
		return out;
	}

	int CycleStyle(int current, int count, int step) {
		if (count <= 0 || current >= count)
			return -1;
		int n = count + 1;                    // pozycje: 0 = AUTO, 1..count = style
		int pos = (current + 1 + (step >= 0 ? 1 : n - 1)) % n;
		return pos - 1;
	}

	std::string StyleLabel(const std::string& overlay) {
		std::string o = Upper(overlay);
		if (o == "HUMANS_ZDF_W3_1H")
			return "Wiedzmin: szybkie ciecia";
		if (o == "HUMANS_ZDF_W3_2H")
			return "Wiedzmin: mocne ciecia";
		if (o.compare(0, 15, "HUMANS_ZDF_NAT_") == 0)
			return "Gothic: " + o.substr(15);
		if (o.compare(0, 7, "HUMANS_") == 0)
			return "zEBA: " + o.substr(7);
		return o;
	}

	int RollDirFromKeys(bool sharesJump, bool left, bool right, bool forward, bool back) {
		if (!sharesJump)
			return MoveDirFromKeys(left, right, forward, back);
		if (left != right)
			return left ? MOVE_LEFT : MOVE_RIGHT;
		return back ? MOVE_BACK : -1;
	}

	int DoubleTapStep(DoubleTap& s, const bool down[4], float dt, float window, bool armed) {
		int edges = 0, edgeDir = -1;
		for (int i = 0; i < 4; i++) {
			if (down[i] && !s.prev[i]) {
				edges++;
				edgeDir = i;
			}
			s.prev[i] = down[i];
		}
		if (s.lastDir >= 0)
			s.age += dt;
		if (!armed || edges > 1) {
			s.lastDir = -1;
			return -1;
		}
		if (s.lastDir >= 0 && s.age > window)
			s.lastDir = -1;
		if (edges == 0)
			return -1;
		if (edgeDir == s.lastDir) {
			s.lastDir = -1;
			return edgeDir;
		}
		s.lastDir = edgeDir;
		s.age = 0.0f;
		return -1;
	}

	bool AttackHoldIdle(const std::vector<std::string>& activeAnis) {
		bool stance = false;
		for (const std::string& raw : activeAnis) {
			std::string n = raw;
			for (char& c : n)
				c = (char)toupper((unsigned char)c);
			if (n.find("ATTACK") == std::string::npos)
				continue;
			if (n.compare(0, 2, "S_") == 0)
				stance = true;
			else if (n.compare(0, 2, "T_") == 0 && n.find("_2_") == std::string::npos)
				return false;   // cios w toku
		}
		return stance;
	}

	RollTapResult JumpTapStep(RollTap& s, const JumpTapInput& in, float dt, const JumpTapWindows& w) {
		if (!s.waiting) {
			if (!in.press)
				return ROLLTAP_NONE;
			bool fwd = in.forwardOnly && (in.rollArmed || in.attackArmed);
			bool plain = in.noDir && in.attackArmed;
			if (!fwd && !plain)
				return ROLLTAP_NONE;
			s = RollTap();
			if (in.attack && in.attackArmed)
				return ROLLTAP_ATTACK;   // Alt i atak w tej samej klatce (akord)
			s.waiting = true;
			s.left = fwd ? w.doubleTap : w.attack;
			s.forward = fwd;
			s.held = in.down ? -1.0f : 0.0f;
			return ROLLTAP_WAIT;
		}
		if (in.attack && in.attackArmed) {
			s = RollTap();
			return ROLLTAP_ATTACK;
		}
		if (in.press) {
			// przod przy pierwszym albo drugim Alt (Alt i W prawie naraz - pierwsze Alt bywa zlapane przed W)
			bool roll = (s.forward || in.forwardOnly) && in.rollArmed;
			s = RollTap();
			return roll ? ROLLTAP_ROLL : ROLLTAP_JUMP;
		}
		if (!s.forward && in.forwardOnly && in.rollArmed) {
			// W wcisniete chwile po Alt: dalej moze przyjsc drugie Alt - okno jak przy Alt+W od pierwszego wcisniecia
			s.forward = true;
			float rest = w.doubleTap - s.age;
			if (rest > s.left)
				s.left = rest;
		}
		if (s.held < 0.0f && !in.down)
			s.held = s.age;
		s.left -= dt;
		s.age += dt;
		if (s.left > 0.0f)
			return ROLLTAP_WAIT;
		s = RollTap();
		return ROLLTAP_JUMP;
	}

	RollTapResult JumpTapStep(RollTap& s, const JumpTapInput& in, float dt, float window) {
		JumpTapWindows w;
		w.doubleTap = w.attack = window;
		return JumpTapStep(s, in, dt, w);
	}

	bool MoveTowardCamera(float dirX, float dirZ, float trajX, float trajZ, float toCamX, float toCamZ, float minDist) {
		// swiat: prawo voba = (dirZ, -dirX) (jak w Dodge.cpp MoveAlong), przod = dir
		float mx = dirZ * trajX + dirX * trajZ, mz = -dirX * trajX + dirZ * trajZ;
		float ml = std::sqrt(mx * mx + mz * mz), cl = std::sqrt(toCamX * toCamX + toCamZ * toCamZ);
		if (ml < minDist || ml < 1e-3f || cl < 1e-3f)
			return false;
		return (mx * toCamX + mz * toCamZ) / (ml * cl) > 0.5f;
	}

	RollTapResult RollTapStep(RollTap& s, bool press, bool forwardOnly, float dt, float window) {
		JumpTapInput in;
		in.press = press;
		in.forwardOnly = forwardOnly;
		in.rollArmed = true;
		return JumpTapStep(s, in, dt, window);
	}

	JumpNowResult JumpTapNowStep(RollTap& s, const JumpTapInput& in, float dt, const JumpTapWindows& w) {
		if (!s.waiting) {
			if (!in.press)
				return JUMPNOW_NONE;
			bool fwd = in.forwardOnly && (in.rollArmed || in.attackArmed);
			bool plain = in.noDir && in.attackArmed;
			if (!fwd && !plain)
				return JUMPNOW_NONE;
			s = RollTap();
			if (in.attack && in.attackArmed)
				return JUMPNOW_ATTACK;   // Alt i atak w tej samej klatce (akord)
			s.waiting = true;
			s.left = fwd ? w.doubleTap : w.attack;
			s.forward = fwd;
			s.held = in.down ? -1.0f : 0.0f;
			return JUMPNOW_JUMP;
		}
		if (in.attack && in.attackArmed) {
			s = RollTap();
			return JUMPNOW_ATTACK;
		}
		if (in.press) {
			// przod przy pierwszym albo drugim Alt; drugie Alt bez przodu - pominiete (skok juz trwa)
			bool roll = (s.forward || in.forwardOnly) && in.rollArmed;
			s = RollTap();
			return roll ? JUMPNOW_ROLL : JUMPNOW_NONE;
		}
		if (!s.forward && in.forwardOnly && in.rollArmed) {
			s.forward = true;   // W chwile po Alt - okno jak przy Alt+W od pierwszego wcisniecia
			float rest = w.doubleTap - s.age;
			if (rest > s.left)
				s.left = rest;
		}
		if (s.held < 0.0f && !in.down)
			s.held = s.age;
		s.left -= dt;
		s.age += dt;
		if (s.left > 0.0f)
			return JUMPNOW_WAIT;
		s = RollTap();
		return JUMPNOW_CLOSED;
	}

	bool IsJumpTakeoffAni(const std::string& name) {
		std::string u = Upper(name);
		if (u.compare(0, 2, "T_") != 0 || u.find("PARADE") != std::string::npos || u.find("JUMPUP") != std::string::npos)
			return false;
		return u.find("_2_JUMP") != std::string::npos || (u.size() >= 5 && u.compare(u.size() - 5, 5, "JUMPB") == 0);
	}

	bool IsJumpFlightAni(const std::string& name) {
		std::string u = Upper(name);
		if (u == "S_FALL" || u == "S_FALLDN" || u == "S_FALLB" || u == "T_JUMP_2_FALLDN")
			return true;
		return u.compare(0, 2, "S_") == 0 && u.size() >= 6 && u.compare(u.size() - 4, 4, "JUMP") == 0;
	}

	JumpCancel JumpCancelDecide(const JumpCancelInput& in, const JumpCancelLimits& lim) {
		if (!in.started)
			return JUMPCANCEL_BEFORE;
		if (in.physics || in.flight)
			return JUMPCANCEL_LATE;    // lot S_JUMP / spadanie (CheckJump silnika laduje po swojemu) - za pozno
		if (in.takeoff)
			return in.height <= lim.takeoffMaxHeight ? JUMPCANCEL_TAKEOFF : JUMPCANCEL_LATE;
		return in.height <= lim.maxHeight ? JUMPCANCEL_TAKEOFF : JUMPCANCEL_LATE;
	}

	float MoveDropLeft(float drop, float t, float dropTime) {
		if (drop <= 0.0f || dropTime <= 0.0f || t >= dropTime)
			return 0.0f;
		float u = t / dropTime;
		return drop * (1.0f - u * u * (3.0f - 2.0f * u));
	}

	bool JumpKeyTakenOver(bool left, bool right, bool forward, bool back, bool rollArmed, bool attackArmed) {
		if (RollDirFromKeys(true, left, right, forward, back) >= 0)
			return true;   // bok / tyl - przewrot
		bool fwd = forward && left == right && !back;
		bool noDir = !forward && !back && left == right;
		return (fwd && (rollArmed || attackArmed)) || (noDir && attackArmed);
	}

	float StrikeDamage(int weaponDamage, int strength, float share) {
		float d = (float)(weaponDamage + strength) * share;
		return d < 1.0f ? 1.0f : d;
	}

	bool IsLocomotionAni(const std::string& name) {
		std::string u = Upper(name);
		if (u.size() < 3 || (u.compare(0, 2, "S_") != 0 && u.compare(0, 2, "T_") != 0))
			return false;
		static const char* const bad[] = { "JUMP", "FALL", "HIT", "STUMBLE", "ATTACK", "PARADE", "DEAD", "DIE", "WOUND",
			"SWIM", "DIVE", "AIM", "SHOOT", "CAST", "DRAW", "REMOVE" };
		for (const char* b : bad)
			if (u.find(b) != std::string::npos)
				return false;
		return u.find("RUN") != std::string::npos || u.find("WALK") != std::string::npos ||
			u.find("SNEAK") != std::string::npos;
	}

	bool MoveResumable(const std::vector<std::string>& layerOne, bool hpDropped, bool overlayOn, int resumes, int maxResumes) {
		if (!overlayOn || hpDropped || resumes >= maxResumes)
			return false;
		for (auto& n : layerOne)
			if (!IsLocomotionAni(n))
				return false;
		return true;
	}

	bool IsMoveVariant(const std::string& id, const std::string& base) {
		if (base.empty() || id.size() < base.size() || id.compare(0, base.size(), base) != 0)
			return false;
		for (size_t i = base.size(); i < id.size(); i++)
			if (id[i] < '0' || id[i] > '9')
				return false;
		return true;
	}

	std::vector<const MoveDef*> WeaponMoveVariants(const std::vector<MoveDef>& defs, const std::string& base1H,
		const std::string& base2H, bool twoHanded) {
		std::vector<const MoveDef*> out;
		if (twoHanded)
			for (auto& d : defs)
				if (IsMoveVariant(d.id, base2H))
					out.push_back(&d);
		if (out.empty())
			for (auto& d : defs)
				if (IsMoveVariant(d.id, base1H))
					out.push_back(&d);
		return out;
	}

	float LungeNeedScale(float need, float pos, float neg) {
		return pos < 1.0f ? 1.0f : (need - neg) / pos;
	}

	const MoveDef* PickMoveVariant(const std::vector<const MoveDef*>& vars, const std::vector<float>& scales, unsigned rnd,
		const std::string& lastId) {
		if (vars.empty())
			return nullptr;
		bool target = scales.size() == vars.size();
		std::vector<const MoveDef*> cand;
		for (size_t i = 0; i < vars.size(); i++)
			if (!target || LungeFitsWell(scales[i]))
				cand.push_back(vars[i]);
		if (cand.empty()) {
			// zaden nie pasuje: najmniejszy blad proporcjonalny |ln skali| (cel za blisko - skala ujemna - jak 0,01)
			size_t best = 0;
			float bestErr = 0.0f;
			for (size_t i = 0; i < vars.size(); i++) {
				float err = std::fabs(std::log(scales[i] < 0.01f ? 0.01f : scales[i]));
				if (i == 0 || err < bestErr) {
					best = i;
					bestErr = err;
				}
			}
			return vars[best];
		}
		if (cand.size() > 1) {
			std::vector<const MoveDef*> fresh;
			for (const MoveDef* d : cand)
				if (d->id != lastId)
					fresh.push_back(d);
			if (!fresh.empty())
				cand.swap(fresh);
		}
		return cand[rnd % cand.size()];
	}

	std::vector<const MoveDef*> JumpAttackVariants(const std::vector<MoveDef>& defs, bool twoHanded) {
		return WeaponMoveVariants(defs, "JUMPATK_F", "JUMPATK2H_F", twoHanded);
	}

	const MoveDef* FindJumpAttack(const std::vector<MoveDef>& defs, bool twoHanded) {
		auto v = JumpAttackVariants(defs, twoHanded);
		return v.empty() ? nullptr : v[0];
	}

	std::vector<const MoveDef*> RunAttackVariants(const std::vector<MoveDef>& defs, bool twoHanded) {
		return WeaponMoveVariants(defs, "RUNATK_F", "RUNATK2H_F", twoHanded);
	}

	const MoveDef* FindRunAttack(const std::vector<MoveDef>& defs, bool twoHanded) {
		auto v = RunAttackVariants(defs, twoHanded);
		return v.empty() ? nullptr : v[0];
	}

	StepVerdict ClassifyStep(float rise, float horiz, float normalY, const TerrainLimits& lim) {
		if (rise < -lim.maxDrop)
			return STEP_DROP;
		if (rise > lim.stepUp + lim.maxSlope * horiz)
			return STEP_WALL;   // murek, skrzynia, polka skalna
		// podloze celu bardziej strome niz chod gry i krok wyraznie pod gore (wzdluz zbocza i w dol wolno)
		if (normalY < lim.minNormalY && rise > 0.25f * horiz + 2.0f)
			return STEP_WALL;
		return STEP_OK;
	}

	static const float BLOCK_LOST_LIMIT = 40.0f;   // cm niewykonanej drogi = utkniecie
	static const float BLOCK_PROGRESS = 0.6f;      // wykonana czesc kroku, od ktorej strata wygasa
	static const float BLOCK_MIN_LEFT = 0.25f;     // krocej do konca (przejscie do postawy) - dogrywa w miejscu

	bool MoveBlockStep(MoveBlock& b, float planned, float done, float timeLeft) {
		if (planned < 0.5f)
			return false;   // tor stoi - nic do stracenia
		b.lost += planned - (done < 0.0f ? 0.0f : done);
		if (done >= BLOCK_PROGRESS * planned)
			b.lost *= 0.5f;
		return b.lost >= BLOCK_LOST_LIMIT && timeLeft > BLOCK_MIN_LEFT;
	}

	void TrajForward(const std::vector<float>& xz, float fps, float t0, float t1, float& pos, float& neg) {
		pos = neg = 0.0f;
		size_t n = xz.size() / 2;
		if (n < 2 || fps <= 0.0f || t1 <= t0)
			return;
		float f0 = t0 * fps, f1 = t1 * fps, last = (float)(n - 1);
		f0 = f0 < 0.0f ? 0.0f : (f0 > last ? last : f0);
		f1 = f1 < 0.0f ? 0.0f : (f1 > last ? last : f1);
		auto zAt = [&](float f) {
			size_t i = (size_t)f;
			if (i >= n - 1)
				return xz[2 * (n - 1) + 1];
			float u = f - (float)i;
			return xz[2 * i + 1] + (xz[2 * i + 3] - xz[2 * i + 1]) * u;
		};
		float a = f0;
		while (a < f1) {
			float b = std::floor(a) + 1.0f;
			if (b > f1)
				b = f1;
			float d = zAt(b) - zAt(a);
			if (d > 0.0f)
				pos += d;
			else
				neg += d;
			a = b;
		}
	}

	float LungeScale(float need, float pos, float neg, float maxScale) {
		if (pos < 1.0f)
			return 1.0f;
		float k = (need - neg) / pos;
		return k < 0.0f ? 0.0f : (k > maxScale ? maxScale : k);
	}

	float TurnToward(float yaw, float target, float maxStep) {
		const float pi = 3.14159265f;
		float d = std::fmod(target - yaw, 2.0f * pi);
		if (d > pi)
			d -= 2.0f * pi;
		else if (d < -pi)
			d += 2.0f * pi;
		if (d > maxStep)
			d = maxStep;
		else if (d < -maxStep)
			d = -maxStep;
		return yaw + d;
	}

	int MoveDirOfId(const std::string& id) {
		size_t us = id.rfind('_');
		if (us == std::string::npos || us + 2 != id.size())
			return -1;
		for (int d = 0; d < 4; d++)
			if (id[us + 1] == kDirSuffix[d][0])
				return d;
		return -1;
	}

	float AngleDiff(float a, float b) {
		const float pi = 3.14159265f;
		float d = std::fmod(b - a, 2.0f * pi);
		if (d > pi)
			d -= 2.0f * pi;
		else if (d < -pi)
			d += 2.0f * pi;
		return d;
	}

	bool TrackStartAllowed(float yaw, float hx, float hz, float ex, float ez, const TrackParams& p) {
		float dx = ex - hx, dz = ez - hz, l2 = dx * dx + dz * dz;
		if (l2 > p.maxDist * p.maxDist)
			return false;
		if (l2 < p.minDist * p.minDist)
			return true;   // tuz obok - sledzenie zacznie sie po odejsciu
		return std::fabs(AngleDiff(yaw, YawTo(hx, hz, ex, ez))) <= p.startAngle;
	}

	float TrackYawStep(float yaw, float hx, float hz, float ex, float ez, float offset, float dt, const TrackParams& p,
		bool& released) {
		if (released)
			return yaw;
		float dx = ex - hx, dz = ez - hz;
		if (dx * dx + dz * dz < p.minDist * p.minDist)
			return yaw;
		float to = YawTo(hx, hz, ex, ez);
		if (std::fabs(AngleDiff(yaw, to)) > p.releaseAngle) {
			released = true;
			return yaw;
		}
		return TurnToward(yaw, to + offset, p.turnRate * dt);
	}

	float GameTurnRad(int keyDir, bool fight, float mouseX, float rotScale, float frameMs) {
		float arg;
		if (keyDir != 0)
			arg = (float)(keyDir > 0 ? 1 : -1) * (fight ? 2.0f : 1.0f);
		else {
			float m = mouseX * rotScale;
			m = m > 100.0f ? 100.0f : (m < -100.0f ? -100.0f : m);
			arg = m * 0.1f;
		}
		return 0.1f * frameMs * arg * (3.14159265f / 180.0f);
	}

	void SteerGateStart(SteerKeyGate& g, bool left, bool right) {
		g.heldL = left;
		g.heldR = right;
	}

	int SteerGateDir(SteerKeyGate& g, bool left, bool right) {
		if (!left)
			g.heldL = false;
		if (!right)
			g.heldR = false;
		bool l = left && !g.heldL, r = right && !g.heldR;
		return l == r ? 0 : (l ? -1 : 1);
	}

	void DeflectAroundCircle(float px, float pz, float& dx, float& dz, float cx, float cz, float r, float sideX, float sideZ) {
		float qx = px + dx, qz = pz + dz;
		float vqx = qx - cx, vqz = qz - cz, dq = std::sqrt(vqx * vqx + vqz * vqz);
		if (dq >= r || r <= 0.0f)
			return;
		float vpx = px - cx, vpz = pz - cz, dp = std::sqrt(vpx * vpx + vpz * vpz);
		if (dp < r - 0.5f) {
			// juz w kole: bez skladowej w glab (wyjscie i ruch wzdluz wolne)
			if (dp < 1e-3f)
				return;
			float nx = vpx / dp, nz = vpz / dp, dn = dx * nx + dz * nz;
			if (dn < 0.0f) {
				dx -= nx * dn;
				dz -= nz * dn;
			}
			return;
		}
		float len = std::sqrt(dx * dx + dz * dz);
		if (len < 1e-4f)
			return;
		// rzut konca na brzeg kola
		float mx, mz;
		if (dq > 1e-3f) {
			mx = vqx / dq;
			mz = vqz / dq;
		}
		else {
			mx = vpx / dp;
			mz = vpz / dp;
		}
		float ex = cx + mx * r, ez = cz + mz * r;
		float lost = len - std::sqrt((ex - px) * (ex - px) + (ez - pz) * (ez - pz));
		if (lost > 0.0f) {
			// reszta drogi lukiem po brzegu w strone ruchu
			float t1x = -mz, t1z = mx;   // jedna z dwoch stycznych
			float along = t1x * dx + t1z * dz;
			float sgn;
			if (std::fabs(along) > 0.05f * len)
				sgn = along > 0.0f ? 1.0f : -1.0f;
			else {
				// czolowo: strona przeciwna do srodka kola wzgledem toru, a przy dokladnie czolowym - podpowiedz
				float rx = dz, rz = -dx;                         // prawo kierunku ruchu
				float cSide = (cx - px) * rx + (cz - pz) * rz;   // > 0: srodek kola po prawej
				float wantX, wantZ;
				if (std::fabs(cSide) > 1e-3f * len) {
					wantX = cSide > 0.0f ? -rx : rx;
					wantZ = cSide > 0.0f ? -rz : rz;
				}
				else {
					wantX = sideX;
					wantZ = sideZ;
				}
				sgn = (t1x * wantX + t1z * wantZ) >= 0.0f ? 1.0f : -1.0f;
			}
			float th = lost / r;
			if (th > 0.5f)
				th = 0.5f;
			float c = std::cos(th), s = std::sin(th) * sgn;
			float nx = mx * c + t1x * s, nz = mz * c + t1z * s;
			ex = cx + nx * r;
			ez = cz + nz * r;
		}
		dx = ex - px;
		dz = ez - pz;
	}

	bool CircleSeparation(float px, float pz, float cx, float cz, float r, float fbX, float fbZ, float& ox, float& oz) {
		ox = oz = 0.0f;
		float vx = px - cx, vz = pz - cz, d = std::sqrt(vx * vx + vz * vz);
		if (d >= r)
			return false;
		float ux, uz;
		if (d > 1.0f) {
			ux = vx / d;
			uz = vz / d;
		}
		else {
			float fl = std::sqrt(fbX * fbX + fbZ * fbZ);
			ux = fl > 1e-4f ? fbX / fl : 1.0f;
			uz = fl > 1e-4f ? fbZ / fl : 0.0f;
		}
		ox = ux * (r - d);
		oz = uz * (r - d);
		return true;
	}

	float TrajTimeAtShare(const std::vector<float>& xz, float fps, float share) {
		size_t n = xz.size() / 2;
		if (n < 2 || fps <= 0.0f)
			return 0.0f;
		std::vector<float> cum(n, 0.0f);
		for (size_t i = 1; i < n; i++) {
			float ax = xz[2 * i] - xz[2 * i - 2], az = xz[2 * i + 1] - xz[2 * i - 1];
			cum[i] = cum[i - 1] + std::sqrt(ax * ax + az * az);
		}
		float total = cum[n - 1];
		if (total < 1.0f || share <= 0.0f)
			return 0.0f;
		float want = share * total;
		for (size_t i = 1; i < n; i++)
			if (cum[i] >= want - 1e-4f) {
				float seg = cum[i] - cum[i - 1];
				float u = seg > 1e-6f ? (want - cum[i - 1]) / seg : 1.0f;
				return ((float)(i - 1) + u) / fps;
			}
		return (float)(n - 1) / fps;
	}

	float ParadeCancelFrom(const MoveDef& d, float afterHit, float pathShare) {
		float t;
		if (d.hitTime >= 0.0f)
			t = d.hitTime + afterHit;
		else {
			t = TrajTimeAtShare(d.traj, d.trajFps, pathShare);
			if (t < d.iframeTo)
				t = d.iframeTo;
		}
		return t > d.cancel ? d.cancel : t;
	}

	bool ParadeCancelStep(ParadeBuffer& b, bool press, bool held, bool windowOpen, float dt, float buffer) {
		if (press)
			b.left = buffer;
		else if (!held && b.left > 0.0f) {
			b.left -= dt;
			if (b.left < 0.0f)
				b.left = 0.0f;
		}
		if (windowOpen && b.left > 0.0f) {
			b.left = 0.0f;
			return true;
		}
		return false;
	}

	float RunExitTime(const MoveDef& d, float afterHit, float blend) {
		float last = d.cancel - blend;
		float t = d.hitTime >= 0.0f ? d.hitTime + afterHit : last;
		t = t > last ? last : t;
		return t < 0.0f ? 0.0f : t;
	}

	float TrajSpeedAt(const std::vector<float>& xz, float fps, float t, float span) {
		if (span <= 0.0f)
			return 0.0f;
		float x0, z0, x1, z1;
		SampleTraj(xz, fps, t, x0, z0);
		SampleTraj(xz, fps, t + span, x1, z1);
		return std::sqrt((x1 - x0) * (x1 - x0) + (z1 - z0) * (z1 - z0)) / span;
	}

	float EntrySpeedRatio(float entrySpeed, float clipSpeed, float maxRatio) {
		if (clipSpeed < 50.0f)
			return 1.0f;
		float r = entrySpeed / clipSpeed;
		return r < 1.0f ? 1.0f : (r > maxRatio ? maxRatio : r);
	}

	MoveDir MoveDirFromKeys(bool left, bool right, bool forward, bool back) {
		if (left != right)
			return left ? MOVE_LEFT : MOVE_RIGHT;
		if (back)
			return MOVE_BACK;
		if (forward)
			return MOVE_FORWARD;
		return MOVE_BACK;
	}
}
