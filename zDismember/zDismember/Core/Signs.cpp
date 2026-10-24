// Logika bez silnika: Znaki Wiedzmina (opis w Signs.h).
#include "Signs.h"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace zdcore {
	static const char* const SIGN_NAMES[SIGN_COUNT] = { "Aard", "Igni", "Axii", "Yrden", "Quen" };

	const char* SignName(int sign) {
		return sign >= 0 && sign < SIGN_COUNT ? SIGN_NAMES[sign] : "?";
	}

	static std::string UpperCopy(const std::string& s) {
		std::string u = s;
		for (auto& c : u)
			c = (char)std::toupper((unsigned char)c);
		return u;
	}

	int SignFromName(const std::string& name) {
		std::string u = UpperCopy(name);
		for (int i = 0; i < SIGN_COUNT; i++)
			if (u == UpperCopy(SIGN_NAMES[i]))
				return i;
		return -1;
	}

	int SignFromSpellId(const int ids[SIGN_COUNT], int spellId) {
		if (spellId < 0)
			return -1;
		for (int i = 0; i < SIGN_COUNT; i++)
			if (ids[i] == spellId)
				return i;
		return -1;
	}

	int ResolveSignIds(const int ini[SIGN_COUNT], const int script[SIGN_COUNT], int out[SIGN_COUNT]) {
		const int MAX_ID = 4094;   // zCPar_Symbol::ele ma 12 bitow (tablice czarow do 4095 elementow)
		int merged[SIGN_COUNT], fromScript = 0;
		for (int i = 0; i < SIGN_COUNT; i++) {
			bool ok = script[i] >= 0 && script[i] <= MAX_ID;
			merged[i] = ok ? script[i] : ini[i];
			if (ok && script[i] != ini[i])
				fromScript++;
		}
		for (int i = 0; i < SIGN_COUNT; i++)
			for (int j = i + 1; j < SIGN_COUNT; j++)
				if (merged[i] == merged[j]) {
					for (int k = 0; k < SIGN_COUNT; k++)
						out[k] = ini[k];
					return -1;
				}
		for (int i = 0; i < SIGN_COUNT; i++)
			out[i] = merged[i];
		return fromScript;
	}

	std::string SignIdsLabel(const int ids[SIGN_COUNT]) {
		std::string s;
		for (int i = 0; i < SIGN_COUNT; i++)
			s += (i ? "," : "") + std::to_string(ids[i]);
		return s;
	}

	bool ParseSignLetters(const std::string& s, std::string out[SIGN_COUNT]) {
		std::string tmp[SIGN_COUNT];
		int n = 0;
		size_t pos = 0;
		while (pos <= s.size()) {
			size_t comma = s.find(',', pos);
			std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
			size_t a = tok.find_first_not_of(" \t"), b = tok.find_last_not_of(" \t");
			tok = a == std::string::npos ? std::string() : tok.substr(a, b - a + 1);
			if (tok.empty() || tok.size() > 4 || n >= SIGN_COUNT)
				return false;
			for (char c : tok)
				if (!std::isalpha((unsigned char)c))
					return false;
			tmp[n++] = UpperCopy(tok);
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
		}
		if (n != SIGN_COUNT)
			return false;
		for (int i = 0; i < SIGN_COUNT; i++)
			out[i] = tmp[i];
		return true;
	}

	// ---------- mana i odnowienie ----------
	SignCastResult SignCheckCast(bool enabled, int mana, int cost, float cooldownLeft) {
		if (!enabled)
			return SIGNCAST_DISABLED;
		if (cooldownLeft > 0.0f)
			return SIGNCAST_COOLDOWN;
		if (mana < cost)
			return SIGNCAST_NO_MANA;
		return SIGNCAST_OK;
	}

	void SignCooldowns::Start(int sign, float seconds) {
		if (sign >= 0 && sign < SIGN_COUNT)
			left[sign] = seconds > 0.0f ? seconds : 0.0f;
	}

	void SignCooldowns::Update(float dt) {
		for (auto& l : left)
			l = l > dt ? l - dt : 0.0f;
	}

	float SignCooldowns::Left(int sign) const {
		return sign >= 0 && sign < SIGN_COUNT ? left[sign] : 0.0f;
	}

	void SignCooldowns::Clear() {
		for (auto& l : left)
			l = 0.0f;
	}

	// ---------- stozek ----------
	ConeHit ConeTest(float ox, float oy, float oz, float fx, float fz, float tx, float ty, float tz,
		float range, float halfAngleDeg, float maxDy, float nearRadius) {
		ConeHit h;
		float dx = tx - ox, dz = tz - oz, dy = ty - oy;
		h.dist = std::sqrt(dx * dx + dz * dz);
		float fl = std::sqrt(fx * fx + fz * fz);
		float c = 1.0f;
		if (fl >= 1e-6f && h.dist >= 1e-3f) {
			// kat tez dla odrzuconych (zasieg / wysokosc) - do logu diagnostycznego
			c = (fx * dx + fz * dz) / (fl * h.dist);
			c = c > 1.0f ? 1.0f : c < -1.0f ? -1.0f : c;
			h.angleDeg = std::acos(c) * 180.0f / 3.14159265f;
		}
		if (h.dist > range) {
			h.reason = CONE_RANGE;
			return h;
		}
		if (std::fabs(dy) > maxDy) {
			h.reason = CONE_HEIGHT;
			return h;
		}
		if (fl < 1e-6f) {
			h.reason = CONE_NODIR;
			return h;
		}
		if (h.dist < 1e-3f) {
			h.inside = true;   // w srodku bohatera
			h.reason = CONE_OK;
			return h;
		}
		if (h.dist < nearRadius) {
			h.inside = c > -0.2f;   // tuz przy bohaterze: wszystko poza plecami
			h.reason = h.inside ? CONE_OK : CONE_BEHIND;
		}
		else {
			h.inside = h.angleDeg <= halfAngleDeg;
			h.reason = h.inside ? CONE_OK : CONE_ANGLE;
		}
		return h;
	}

	const char* ConeReasonName(int reason) {
		switch (reason) {
		case CONE_OK: return "ok";
		case CONE_RANGE: return "zasieg";
		case CONE_HEIGHT: return "wysokosc";
		case CONE_ANGLE: return "kat";
		case CONE_BEHIND: return "za plecami";
		case CONE_NODIR: return "kierunek";
		}
		return "?";
	}

	// ---------- cele ----------
	bool SignIsEnemy(const SignTargetInfo& t) {
		return t.attitude == ATTITUDE_HOSTILE || t.targetsHero;
	}

	const char* SignRejectReason(const SignTargetInfo& t, bool hitNeutral) {
		if (t.isPlayer)
			return "bohater";
		if (t.dead)
			return "martwy";
		if (t.unconscious)
			return "nieprzytomny";
		if (t.partyMember)
			return "druzyna (AIV_PARTYMEMBER)";
		if (t.immortal)
			return "niesmiertelny (NPC_FLAG_IMMORTAL)";
		if (SignIsEnemy(t))
			return nullptr;
		if (t.attitude == ATTITUDE_FRIENDLY)
			return "przyjaciel";
		if (t.human && !hitNeutral)
			return "neutralny czlowiek (SignHitNeutral=0)";
		return nullptr;
	}

	bool SignAffects(const SignTargetInfo& t, bool hitNeutral) {
		return SignRejectReason(t, hitNeutral) == nullptr;
	}

	bool AxiiInCombat(const SignTargetInfo& t) {
		if (t.attitude == ATTITUDE_HOSTILE)
			return true;
		return t.targetsHero && (t.weaponDrawn || !t.human);
	}

	bool AxiiTranceApplies(const SignTargetInfo& t) {
		return t.human && !t.dead && !t.unconscious && !t.partyMember && !t.isPlayer && !AxiiInCombat(t);
	}

	float TranceStateEndMs() {
		return 600.0f * 1000.0f + 1.0f;
	}

	const char* AxiiRejectReason(const SignTargetInfo& t) {
		if (t.isPlayer)
			return "bohater";
		if (t.dead)
			return "martwy";
		if (t.unconscious)
			return "nieprzytomny";
		if (t.partyMember)
			return "druzyna (AIV_PARTYMEMBER)";
		return nullptr;
	}

	const char* AxiiCandidateReason(const AxiiCandidate& c) {
		if (c.reject)
			return c.reject;
		if (c.cone != CONE_OK)
			return ConeReasonName(c.cone);
		if (!c.visible)
			return "zasloniety";
		return nullptr;
	}

	int PickAxiiTarget(const std::vector<AxiiCandidate>& cands) {
		const AxiiCandidate* best = nullptr;
		for (auto& c : cands) {
			if (AxiiCandidateReason(c))
				continue;
			if (c.focus)
				return c.id;
			if (!best || c.dist < best->dist || (c.dist == best->dist && c.angleDeg < best->angleDeg))
				best = &c;
		}
		return best ? best->id : -1;
	}

	// ---------- Quen ----------
	void QuenState::Cast(float seconds, int hits) {
		active = true;
		left = seconds;
		hitsLeft = hits < 1 ? 1 : hits;
	}

	bool QuenState::Absorb() {
		if (!active)
			return false;
		if (--hitsLeft > 0)
			return true;   // tarcza trwa (nastepne trafienia)
		active = false;
		left = 0.0f;
		hitsLeft = 0;
		breakPending = true;
		return true;
	}

	bool QuenState::Update(float dt) {
		if (!active)
			return false;
		left -= dt;
		if (left > 0.0f)
			return false;
		active = false;
		left = 0.0f;
		hitsLeft = 0;
		return true;
	}

	bool QuenState::TakeBreak() {
		bool b = breakPending;
		breakPending = false;
		return b;
	}

	void QuenState::Clear() {
		active = false;
		breakPending = false;
		left = 0.0f;
		hitsLeft = 0;
	}

	// ---------- runda 3: moc ----------
	float SignPower(int maxMana, float perMana, float maxBonus) {
		float bonus = maxMana > 0 && perMana > 0.0f ? (float)maxMana * perMana : 0.0f;
		if (maxBonus < 0.0f)
			maxBonus = 0.0f;
		return 1.0f + (bonus < maxBonus ? bonus : maxBonus);
	}

	float SignIntPower(int intellect, float per10Pct, float maxMul) {
		if (intellect <= 0 || !(per10Pct > 0.0f) || !(maxMul > 1.0f))
			return 1.0f;
		float m = 1.0f + (float)intellect / 10.0f * per10Pct / 100.0f;
		return m < maxMul ? m : maxMul;
	}

	SignDamagePower SignDamagePowerOf(int setting, float manaPower, int intellect, float per10Pct, float maxMul) {
		SignDamagePower r;
		if (setting != SIGN_POWER_MANA && setting != SIGN_POWER_INT)
			setting = SIGN_POWER_BOTH;
		if (setting != SIGN_POWER_MANA && intellect < 0) {
			r.fellBack = true;
			setting = SIGN_POWER_MANA;
		}
		float i = SignIntPower(intellect, per10Pct, maxMul);
		r.source = setting;
		r.mul = setting == SIGN_POWER_MANA ? manaPower : setting == SIGN_POWER_INT ? i : manaPower * i;
		return r;
	}

	const char* SignPowerSourceName(int source) {
		switch (source) {
		case SIGN_POWER_MANA: return "mana";
		case SIGN_POWER_INT: return "INT";
		case SIGN_POWER_BOTH: return "oba";
		}
		return "?";
	}

	float MigrateDefault(float value, float oldDef, float newDef) {
		return std::fabs(value - oldDef) < 0.5f ? newDef : value;
	}

	bool SignSplitDamage(unsigned long mode, float total, unsigned long ary[8]) {
		if (!(total > 0.0f))
			return false;
		int n = 0;
		for (int i = 0; i < 8; i++) {
			if (ary[i])
				return false;
			if (mode & (1ul << i))
				n++;
		}
		if (!n)
			return false;
		unsigned long per = (unsigned long)std::floor(total / (float)n + 0.5f);
		for (int i = 0; i < 8; i++)
			if (mode & (1ul << i))
				ary[i] = per;
		return true;
	}

	bool SignDamageArrayWanted(int setting, bool newBalance) {
		return setting < 0 ? newBalance : setting != 0;
	}

	int QuenHitsAt(float power) {
		return power >= 2.0f - 1e-5f ? 3 : power >= 1.5f - 1e-5f ? 2 : 1;
	}

	static float PowerLerp(float power, float atOne, float atTwo) {
		float t = power - 1.0f;
		t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
		return atOne + (atTwo - atOne) * t;
	}

	float YrdenSlowAt(float power, float slowAtOne, float slowMin) {
		return PowerLerp(power, slowAtOne, slowMin);
	}

	float YrdenArmorAt(float power, float armorAtOne, float armorMax) {
		return PowerLerp(power, armorAtOne, armorMax);
	}

	// ---------- runda 3: przytrzymanie ----------
	HoldAction SignHold::Begin(float altHold) {
		Reset();
		if (altHold <= 0.0f)
			return HOLD_CAST_NORMAL;
		phase = HOLD_PENDING;
		return HOLD_WAIT;
	}

	HoldAction SignHold::Tick(float dt, float altHold, bool sustainedAlt) {
		if (phase == HOLD_NONE || phase == HOLD_FIRED)
			return HOLD_STOP;
		held += dt > 0.0f ? dt : 0.0f;
		if (phase == HOLD_SUSTAINING) {
			if (stopRequested) {
				Reset();
				return HOLD_STOP;
			}
			return HOLD_SUSTAIN;
		}
		if (held < altHold)
			return HOLD_WAIT;
		if (sustainedAlt) {
			phase = HOLD_SUSTAINING;
			return HOLD_START_SUSTAIN;
		}
		phase = HOLD_FIRED;
		return HOLD_CAST_ALT;
	}

	HoldAction SignHold::Release() {
		int p = phase;
		Reset();
		return p == HOLD_PENDING ? HOLD_CAST_NORMAL : HOLD_STOP;
	}

	bool SignAimsAt(int sign, bool alt) {
		switch (sign) {
		case SIGN_AARD: return !alt;
		case SIGN_IGNI: case SIGN_AXII: case SIGN_YRDEN: return true;
		}
		return false;   // Quen i zly indeks
	}

	SignAimHud SignHoldHud(int phase, float held, float altHold, int sign) {
		SignAimHud h;
		h.held = phase != HOLD_NONE;
		h.alt = phase == HOLD_SUSTAINING || phase == HOLD_FIRED;
		h.wantsAim = SignAimsAt(sign, h.alt);
		if (!h.held || altHold <= 0.0f)
			return h;
		float f = h.alt ? 1.0f : held / altHold;
		h.frac = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
		h.showBar = !h.alt || h.wantsAim;
		return h;
	}

	int SustainMana::Take(float dt, float perSecond) {
		if (dt > 0.0f && perSecond > 0.0f)
			acc += dt * perSecond;
		int n = (int)std::floor(acc + 1e-4f);
		if (n > 0)
			acc -= (float)n;
		if (acc < 0.0f)
			acc = 0.0f;
		return n;
	}

	void YrdenTrapState::Arm(int n, float minInterval) {
		charges = n;
		interval = minInterval;
		lastShot.clear();
	}

	bool YrdenTrapState::CanFire(int id, float now) const {
		if (charges <= 0)
			return false;
		for (auto& s : lastShot)
			if (s.first == id)
				return now - s.second >= interval - 1e-4f;
		return true;
	}

	void YrdenTrapState::Fire(int id, float now) {
		if (charges > 0)
			charges--;
		for (auto& s : lastShot)
			if (s.first == id) {
				s.second = now;
				return;
			}
		lastShot.push_back({ id, now });
	}

	int QuenActiveHealTo(int hp, int hpMax, float absorbed, float ratio) {
		int heal = absorbed > 0.0f && ratio > 0.0f ? (int)std::floor(absorbed * ratio + 0.5f) : 0;
		int r = hp + heal;
		return r > hpMax ? (hp > hpMax ? hp : hpMax) : r;
	}

	int PickPuppetTarget(const std::vector<PuppetCandidate>& cands, float range) {
		int best = -1;
		float bestDist = range;
		for (auto& c : cands)
			if (c.enemyOfHero && c.alive && c.dist <= bestDist) {
				bestDist = c.dist;
				best = c.id;
			}
		return best;
	}

	// ---------- runda 4 ----------
	bool ChannelSkipMagicMode(bool sustaining, bool magicPath, bool actionHeld) {
		return sustaining && magicPath && actionHeld;
	}

	float ChannelHoldFrame(int numFrames, int wanted) {
		int last = numFrames - 2;   // przedostatnia - ostatnia konczy animacje
		if (last < 0)
			last = 0;
		int f = wanted < 0 ? 0 : wanted > last ? last : wanted;
		return (float)f;
	}

	ChannelPoseAction ChannelPoseStep(bool active, float actFrame, float holdFrame) {
		if (!active)
			return CPOSE_RESTART;
		return actFrame > holdFrame + 0.01f ? CPOSE_FREEZE : CPOSE_NONE;
	}

	AltInPlacePlan PlanAltInPlace(bool enabled, bool altCast, int qsResult, int qsAlt, int castMsLeft, int lengthMsLeft) {
		AltInPlacePlan p;
		if (!enabled || !altCast || qsResult != 1 || !qsAlt || lengthMsLeft <= 0)
			return p;
		p.use = true;
		p.delay = (float)(castMsLeft > 0 ? castMsLeft : 0) / 1000.0f;
		p.hold = (float)lengthMsLeft / 1000.0f;
		if (p.delay > p.hold)
			p.delay = p.hold;   // skutek najpozniej z koncem gestu
		return p;
	}

	bool PendingStep(float& delay, float dt) {
		if (delay <= 0.0f)
			return true;
		delay -= dt > 0.0f ? dt : 0.0f;
		return false;
	}

	StillAction StillHold::Start(float seconds, bool lockedNow) {
		if (seconds <= 0.0f)
			return STILL_NONE;
		if (active) {
			if (seconds > left)
				left = seconds;
			return STILL_NONE;
		}
		active = true;
		left = seconds;
		owned = !lockedNow;
		return owned ? STILL_LOCK : STILL_NONE;
	}

	StillAction StillHold::Step(float dt, bool lockedNow, bool othersBusy) {
		if (!active)
			return STILL_NONE;
		if (owned && (!lockedNow || othersBusy))
			owned = false;   // zdjal ja ktos inny albo przejal finisher - juz nie nasza
		left -= dt > 0.0f ? dt : 0.0f;
		return left <= 0.0f ? End(lockedNow, othersBusy) : STILL_NONE;
	}

	StillAction StillHold::End(bool lockedNow, bool othersBusy) {
		if (!active)
			return STILL_NONE;
		bool mine = owned && lockedNow && !othersBusy;
		active = false;
		owned = false;
		left = 0.0f;
		return mine ? STILL_UNLOCK : STILL_NONE;
	}

	PuppetOrder PuppetDecide(bool engaged, bool hasTarget, bool following) {
		if (engaged)
			return PUPPET_KEEP;
		if (hasTarget)
			return PUPPET_ATTACK;
		return following ? PUPPET_KEEP : PUPPET_FOLLOW;
	}

	bool PuppetHumanAi(int guild) {
		return guild < 16;   // GIL_SEPERATOR_HUM
	}

	int DamageMainIndex(const int damage[8], unsigned damageTypeMask) {
		int best = -1, bestVal = 0;
		for (int i = 0; i < 8; i++)
			if (damage[i] > bestVal) {
				bestVal = damage[i];
				best = i;
			}
		if (best >= 0)
			return best;
		for (int i = 0; i < 8; i++)
			if (damageTypeMask & (1u << i))
				return i;
		return 2;   // ostrze
	}

	int EstimateMeleeDamage(int baseDamage, int strength, int protection, int minDamage) {
		if (protection < 0)
			return 0;   // IMMUNE
		int d = baseDamage + strength - protection;
		return d < minDamage ? minDamage : d;
	}

	// ---------- Yrden ----------
	uint32_t YrdenArmorMask() {
		return (1u << 1) | (1u << 2) | (1u << 3) | (1u << 5) | (1u << 6);
	}

	YrdenMod YrdenApply(const NpcCombatStats& cur, float slow, float armorCutPercent, uint32_t mask) {
		YrdenMod m;
		m.orig = cur;
		m.applied = cur;
		m.applied.timeScale = cur.timeScale * slow;
		// procenty calkowite, zaokraglenie polowek w gore (25 x 70% = 17,5 -> 18) - bez bledu float (0,7f * 25 = 17,49)
		long keepPct = 100 - std::lround(armorCutPercent);
		keepPct = keepPct < 0 ? 0 : keepPct > 100 ? 100 : keepPct;
		for (int i = 0; i < SIGN_PROT_COUNT; i++) {
			if (!(mask & (1u << i)) || cur.prot[i] < 0)
				continue;   // poza maska albo IMMUNE
			m.applied.prot[i] = (int)(((long long)cur.prot[i] * keepPct + 50) / 100);
		}
		return m;
	}

	float YrdenRestoreTempo(const YrdenMod& m, float cur) {
		if (cur == m.applied.timeScale)
			return m.orig.timeScale;
		if (m.applied.timeScale > 1e-6f)
			return cur * (m.orig.timeScale / m.applied.timeScale);
		return cur;
	}

	bool YrdenTempoDeferred(const YrdenMod& m, float cur) {
		return cur < 0.001f && m.applied.timeScale != m.orig.timeScale && m.applied.timeScale >= 0.001f;
	}

	NpcCombatStats YrdenRestore(const YrdenMod& m, const NpcCombatStats& cur) {
		NpcCombatStats r = cur;
		r.timeScale = YrdenRestoreTempo(m, cur.timeScale);
		for (int i = 0; i < SIGN_PROT_COUNT; i++) {
			if (m.orig.prot[i] == m.applied.prot[i])
				continue;   // nie ruszalismy
			if (cur.prot[i] == m.applied.prot[i])
				r.prot[i] = m.orig.prot[i];
			else
				r.prot[i] = cur.prot[i] + (m.orig.prot[i] - m.applied.prot[i]);
		}
		return r;
	}

	void YrdenZone::Start(float px, float py, float pz, float r, float seconds) {
		active = true;
		x = px;
		y = py;
		z = pz;
		radius = r;
		left = seconds;
	}

	bool YrdenZone::Contains(float px, float py, float pz, float maxDy) const {
		if (!active)
			return false;
		float dx = px - x, dz = pz - z;
		return dx * dx + dz * dz <= radius * radius && std::fabs(py - y) <= maxDy;
	}

	bool YrdenZone::Update(float dt) {
		if (!active)
			return false;
		left -= dt;
		if (left > 0.0f)
			return false;
		Stop();
		return true;
	}

	void YrdenZone::Stop() {
		active = false;
		left = 0.0f;
	}

	YrdenPlan YrdenStep(const std::vector<int>& applied, const std::vector<int>& insideEnemies) {
		YrdenPlan p;
		for (int id : insideEnemies)
			if (std::find(applied.begin(), applied.end(), id) == applied.end())
				p.enter.push_back(id);
		for (int id : applied)
			if (std::find(insideEnemies.begin(), insideEnemies.end(), id) == insideEnemies.end())
				p.leave.push_back(id);
		return p;
	}

	// ---------- Aard ----------
	bool AardShouldFly(int protFly, float modelHeight, int maxProt, float maxHeight) {
		return protFly >= 0 && protFly <= maxProt && modelHeight <= maxHeight;
	}

	FlyVerdict AardFlyVerdict(float elapsed, float movedAlong, bool airborne, float checkTime, float minMove) {
		if (movedAlong >= minMove)
			return FLY_OK;
		if (elapsed < checkTime)
			return FLY_WAIT;
		return airborne ? FLY_OK : FLY_FAILED;
	}

	// ---------- widocznosc ----------
	std::vector<float> LosTargetHeights(float posY, float bboxMinY, float bboxMaxY) {
		std::vector<float> out;
		float h = bboxMaxY - bboxMinY;
		if (!(h > 1.0f) || h > 5000.0f) {
			out.push_back(posY);
			return out;
		}
		out.push_back(bboxMinY + 0.5f * h);
		out.push_back(bboxMinY + 0.75f * h);
		float p = posY + 20.0f;
		out.push_back(p > bboxMaxY ? bboxMaxY : p);
		return out;
	}

	float LosEyeHeight(float bboxMinY, float bboxMaxY) {
		return bboxMinY + 0.75f * (bboxMaxY - bboxMinY);
	}

	// ---------- warianty oszolomienia ----------
	static const char* const STUN_NAMES[STUN_COUNT] = { "NONE", "PRAY", "FEAR1", "FEAR2", "FEAR3", "WOUNDED", "WOUNDEDB",
		"MONSTER", "BURN", "TRANCE" };

	const char* StunName(int v) {
		return v >= 0 && v < STUN_COUNT ? STUN_NAMES[v] : "?";
	}

	uint32_t ParseStunAnis(const std::string& s, bool* ok) {
		uint32_t mask = 0;
		bool good = true;
		size_t pos = 0;
		while (pos <= s.size()) {
			size_t comma = s.find(',', pos);
			std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
			size_t a = tok.find_first_not_of(" \t"), b = tok.find_last_not_of(" \t");
			tok = a == std::string::npos ? std::string() : UpperCopy(tok.substr(a, b - a + 1));
			if (!tok.empty()) {
				int found = STUN_NONE;
				for (int v = STUN_PRAY; v <= STUN_WOUNDEDB; v++)   // z ini tylko warianty ludzi
					if (tok == STUN_NAMES[v])
						found = v;
				if (found != STUN_NONE)
					mask |= StunBit(found);
				else
					good = false;
			}
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
		}
		if (ok)
			*ok = good;
		return mask;
	}

	int PickStunVariant(uint32_t mask, int last, uint32_t rnd) {
		int cands[STUN_COUNT];
		int n = 0, nAll = 0;
		for (int v = STUN_PRAY; v < STUN_COUNT; v++)
			if (mask & StunBit(v)) {
				nAll++;
				if (v != last)
					cands[n++] = v;
			}
		if (nAll == 0)
			return STUN_NONE;
		if (n == 0)
			return last;   // jedyny dozwolony
		return cands[rnd % (uint32_t)n];
	}

	int StunScriptSeconds(float seconds) {
		int s = (int)std::ceil(seconds - 1e-4f);
		return s < 1 ? 1 : s;
	}

	float StunStateEndMs(float seconds) {
		return (float)StunScriptSeconds(seconds) * 1000.0f + 1.0f;
	}

	// ---------- Quen ----------
	bool QuenShouldAbsorb(bool shieldActive, bool isTick, bool hasAttacker, bool isFall) {
		return shieldActive && !isTick && (hasAttacker || isFall);
	}

	QuenFxAction QuenFxGuard(bool shieldActive, bool fxExists, bool fxFinished, float fxDistToHero, float maxDist) {
		if (!shieldActive)
			return fxExists ? QFX_STOP : QFX_NONE;
		if (!fxExists)
			return QFX_CREATE;
		if (fxFinished || fxDistToHero > maxDist)
			return QFX_RESPAWN;
		return QFX_NONE;
	}

	// ---------- Igni ----------
	int MenuIndexForReal(const std::vector<float>& values, float v) {
		int best = -1;
		float bestD = 0.0f;
		for (size_t i = 0; i < values.size(); i++) {
			float d = std::fabs(values[i] - v);
			if (best < 0 || d < bestD - 1e-7f) {
				best = (int)i;
				bestD = d;
			}
		}
		return best;
	}

	void AimPointInRange(float hx, float hz, float px, float pz, float maxRange, float& outX, float& outZ) {
		float dx = px - hx, dz = pz - hz, l = std::sqrt(dx * dx + dz * dz);
		if (l <= maxRange || l < 1e-3f) {
			outX = px;
			outZ = pz;
			return;
		}
		outX = hx + dx / l * maxRange;
		outZ = hz + dz / l * maxRange;
	}

	int BurnTickEffective(float damagePerInterval, int fireProtection) {
		if (fireProtection < 0)
			return 0;
		int d = (int)damagePerInterval - fireProtection;
		return d > 0 ? d : 0;
	}

	BurnTiming BurnOverride(float remainingMs, bool isNew, bool activated, float burnTimeS, float damagePerSecond,
		int fireProtection) {
		BurnTiming b;
		float want = burnTimeS * 1000.0f;
		if (isNew) {
			b.durationMs = want;
			b.intervalMs = 1000.0f;
			b.damagePerInterval = std::floor(damagePerSecond + 0.5f) + (fireProtection > 0 ? (float)fireProtection : 0.0f);
			b.setRate = true;
			b.setTotal = activated;
		}
		else
			b.durationMs = remainingMs > want ? remainingMs : want;
		return b;
	}

	// ---------- ognie z efektu ----------

	// Nazwa bez rozszerzenia, wielkimi literami ("fire_medium.pfx" -> "FIRE_MEDIUM").
	static std::string FireBaseName(const std::string& s) {
		std::string u = UpperCopy(s);
		size_t dot = u.rfind('.');
		if (dot != std::string::npos)
			u.erase(dot);
		return u;
	}

	static bool HasAny(const std::string& u, const char* const* tokens) {
		for (; *tokens; tokens++)
			if (u.find(*tokens) != std::string::npos)
				return true;
		return false;
	}

	// Efekty czarow (MFX_*, SPELLFX_*, VOB_BURN*), bron i pociski ognia, palaca sie postac / golem / smok, lawa, menu.
	static const char* const FIRE_EFFECT_TOKENS[] = { "MFX_", "SPELLFX", "VOB_BURN", "VOB_MAGICBURN", "FIREBALL", "FIREBOLT",
		"FIRERAIN", "FIRESTORM", "FIRESPELL", "MAGICFIRE", "HUMANBURN", "FIREARROW", "FIRE_ARROW", "FIREBOW", "FIRE_BOW",
		"FIRESWORD", "FIRE_SWORD", "FIREARMOR", "FLAMETHROWER", "DRAGON", "GOLEM", "DEMON", "FIRE_MODEL", "FIRE_MENU",
		"FIRECLOUD", "LAVA", nullptr };
	static const char* const FIRE_EXTRA_TOKENS[] = { "SMOKE", "SPARK", "FLARE", nullptr };
	static const char* const FIRE_FLAME_TOKENS[] = { "FIRE", "FLAME", "TORCH", "CANDLE", "EMBER", "COALGLOW", nullptr };

	FireVisKind FireVisualKind(const std::string& visualName, const std::string& vobName) {
		std::string v = FireBaseName(visualName), n = UpperCopy(vobName);
		if (HasAny(v, FIRE_EFFECT_TOKENS))
			return FIREVIS_EFFECT;
		if (HasAny(v, FIRE_EXTRA_TOKENS))
			return FIREVIS_EXTRA;
		if (HasAny(v, FIRE_FLAME_TOKENS))
			return FIREVIS_FLAME;
		if (v.empty())
			return FIREVIS_NONE;   // bez wizuala nie ma czego gasic
		if (HasAny(n, FIRE_EFFECT_TOKENS))
			return FIREVIS_EFFECT;
		if (HasAny(n, FIRE_EXTRA_TOKENS))
			return FIREVIS_EXTRA;
		if (n.find("FIRE") != std::string::npos || n.find("FLAME") != std::string::npos)
			return FIREVIS_FLAME;
		return FIREVIS_NONE;
	}

	const char* FireVisKindName(int kind) {
		static const char* const N[] = { "-", "plomien", "dodatek", "efekt" };
		return kind >= 0 && kind <= FIREVIS_EFFECT ? N[kind] : "?";
	}

	bool FireSoundName(const std::string& soundName) {
		static const char* const T[] = { "FIRE", "TORCH", "FLAME", "BURN", nullptr };
		std::string u = FireBaseName(soundName);
		return u.compare(0, 4, "MFX_") != 0 && HasAny(u, T);
	}

	const char* FireSkipName(int reason) {
		static const char* const N[FIRESKIP_COUNT] = { "ok", "nie ogien", "dym/iskry/poswiata (tylko w grupie plomienia)",
			"efekt czaru / broni / lawy", "vob tymczasowy", "postac / przedmiot", "drzewo oCMobFire", "stan bez zmian",
			"poza stozkiem", "zasloniety" };
		return reason >= 0 && reason < FIRESKIP_COUNT ? N[reason] : "?";
	}

	int FireFlameCheck(const FireFlame& f, bool ignite) {
		if (f.kind == FIREVIS_EFFECT)
			return FIRESKIP_EFFECT;
		if (f.kind == FIREVIS_EXTRA)
			return FIRESKIP_EXTRA;
		if (f.kind != FIREVIS_FLAME)
			return FIRESKIP_NOT_FIRE;
		if (f.runtime)
			return FIRESKIP_RUNTIME;
		if (f.owned)
			return FIRESKIP_OWNER;
		if (f.mobTree)
			return FIRESKIP_MOBTREE;
		if (f.shown == ignite)
			return FIRESKIP_ALREADY;
		return FIRESKIP_OK;
	}

	const char* FireMemberSkipName(int reason) {
		static const char* const N[FMSKIP_COUNT] = { "ok", "za daleko", "inny uchwyt", "nie ogien",
			"swiatlo statyczne (lightmapa)", "stan bez zmian", "postac / przedmiot / tymczasowy" };
		return reason >= 0 && reason < FMSKIP_COUNT ? N[reason] : "?";
	}

	int FireMemberCheck(const FireMember& m, float radius, bool ignite) {
		if (m.owned || m.runtime)
			return FMSKIP_OWNER;
		if (!m.sameTree && !m.loose)
			return FMSKIP_OTHER_HOLDER;
		if (m.dist > (m.sameTree ? 2.0f * radius : radius))
			return FMSKIP_FAR;
		if (m.type == FMEM_VISUAL && m.visKind != FIREVIS_FLAME && m.visKind != FIREVIS_EXTRA)
			return FMSKIP_NOT_FIRE;
		if (m.type == FMEM_SOUND && !m.sameTree && !m.fireSound)
			return FMSKIP_NOT_FIRE;
		if (m.type == FMEM_LIGHT && m.staticLight)
			return FMSKIP_STATIC_LIGHT;
		if (m.active == ignite)
			return FMSKIP_STATE;
		return FMSKIP_OK;
	}

	std::vector<int> NearestIndices(const std::vector<float>& dists, size_t n) {
		std::vector<int> idx(dists.size());
		for (size_t i = 0; i < idx.size(); i++)
			idx[i] = (int)i;
		std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return dists[a] < dists[b]; });
		if (idx.size() > n)
			idx.resize(n);
		return idx;
	}

	// ---------- runda 6: zamki, Urok, premia kieszonkowa ----------
	int LockRulesFor(bool nbUnlockExternal) {
		return nbUnlockExternal ? LOCKRULES_NB : LOCKRULES_PICKLOCK;
	}

	const char* LockRulesName(int rules) {
		return rules == LOCKRULES_NB ? "runa NB" : "wytrych";
	}

	int LockDecide(const LockInfo& l, int rules) {
		if (!l.locked)
			return LOCK_SKIP_UNLOCKED;
		if (l.hasPickLock)
			return LOCK_OPEN_PICK;
		if (l.hasKey)
			return LOCK_SKIP_KEY_ONLY;
		return rules == LOCKRULES_NB ? LOCK_OPEN_BARE : LOCK_SKIP_NO_PICKLOCK;
	}

	bool LockVerdictOpens(int verdict) {
		return verdict == LOCK_OPEN_PICK || verdict == LOCK_OPEN_BARE;
	}

	const char* LockVerdictName(int verdict) {
		static const char* const names[LOCK_VERDICT_COUNT] = { "zamek do wytrycha", "bez klucza i wytrycha (jak runa NB)",
			"juz otwarty", "tylko na klucz", "bez wytrycha" };
		return verdict >= 0 && verdict < LOCK_VERDICT_COUNT ? names[verdict] : "?";
	}

	AxiiCharmPlan PlanAxiiCharm(bool logicFound, int logicRet, int sendStop, bool castFound, bool charmOn, bool tranceApplies) {
		AxiiCharmPlan p;
		p.immune = charmOn && logicFound && logicRet == sendStop;
		p.callCast = charmOn && castFound && !p.immune;
		p.trance = tranceApplies && !p.immune;
		return p;
	}

	float ThiefBonusSeconds(float optionSeconds, float tranceSeconds) {
		if (optionSeconds > 0.0f)
			return optionSeconds;
		return tranceSeconds > 0.0f ? tranceSeconds : 0.0f;
	}

	int ThiefBonusList::Add(const void* key, int instance, int pct, float until) {
		auto it = std::find_if(items.begin(), items.end(), [key](const ThiefBonus& b) { return b.key == key; });
		if (pct <= 0) {
			if (it != items.end())
				items.erase(it);
			return (int)items.size();
		}
		if (it != items.end())
			*it = { key, instance, pct, until };
		else
			items.push_back({ key, instance, pct, until });
		return (int)items.size();
	}

	int ThiefBonusList::PctFor(const void* key, float now) const {
		for (auto& b : items)
			if (b.key == key && now < b.until)
				return b.pct;
		return 0;
	}

	int ThiefBonusList::Prune(float now) {
		size_t before = items.size();
		items.erase(std::remove_if(items.begin(), items.end(), [now](const ThiefBonus& b) { return now >= b.until; }), items.end());
		return (int)(before - items.size());
	}

	int ThiefBonusList::Export(int* ids, int* pcts, int n, float now) const {
		std::vector<ThiefBonus> live;
		for (auto& b : items)
			if (now < b.until)
				live.push_back(b);
		std::stable_sort(live.begin(), live.end(), [](const ThiefBonus& a, const ThiefBonus& b) { return a.until > b.until; });
		int used = 0;
		for (int i = 0; i < n; i++)
			ids[i] = pcts[i] = 0;
		for (auto& b : live) {
			int slot = -1;
			for (int i = 0; i < used; i++)
				if (ids[i] == b.instance)
					slot = i;
			if (slot >= 0) {
				if (b.pct > pcts[slot])
					pcts[slot] = b.pct;
				continue;
			}
			if (used >= n)
				continue;
			ids[used] = b.instance;
			pcts[used] = b.pct;
			used++;
		}
		return used;
	}
}
