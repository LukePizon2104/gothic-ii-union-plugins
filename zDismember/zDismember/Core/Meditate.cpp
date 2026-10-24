#include "Meditate.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zdcore {
	// ---------- czas gry ----------
	double MedGameMinutes(int day, double worldTicks) {
		return (double)day * MED_MIN_PER_DAY + worldTicks / MED_TICKS_PER_HOUR * 60.0;
	}

	void MedSplitMinutes(double minutes, int& day, int& hour, int& minute) {
		if (minutes < 0.0)
			minutes = 0.0;
		double whole = std::floor(minutes + 1e-6);
		day = (int)std::floor(whole / MED_MIN_PER_DAY);
		int inDay = (int)(whole - (double)day * MED_MIN_PER_DAY);
		hour = inDay / 60;
		minute = inDay % 60;
	}

	std::string MedClockText(double minutes) {
		int d, h, m;
		MedSplitMinutes(minutes, d, h, m);
		char buf[16];
		snprintf(buf, sizeof(buf), "%d:%02d", h, m);
		return buf;
	}

	// ---------- cel czekania ----------
	int MedUntilHour(int until) {
		switch (until) {
		case MED_UNTIL_MORNING: return 6;
		case MED_UNTIL_NOON: return 12;
		case MED_UNTIL_EVENING: return 18;
		default: return 0;
		}
	}

	const char* MedUntilLabel(int until) {
		switch (until) {
		case MED_UNTIL_MORNING: return "rano (6:00)";
		case MED_UNTIL_NOON: return "po\xB3" "udnie (12:00)";
		case MED_UNTIL_EVENING: return "wiecz\xF3r (18:00)";
		default: return "p\xF3\xB3noc (0:00)";
		}
	}

	double MedTargetAfterHours(double now, int hours) {
		return now + (double)std::max(hours, 0) * 60.0;
	}

	double MedTargetUntilHour(double now, int hour) {
		double day = std::floor(now / MED_MIN_PER_DAY);
		double t = day * MED_MIN_PER_DAY + (double)(((hour % 24) + 24) % 24) * 60.0;
		while (t < now + 1.0)
			t += MED_MIN_PER_DAY;
		return t;
	}

	// ---------- tempo ----------
	int MedClampSpeed(int speed) {
		return std::clamp(speed, MED_SPEED_MIN, MED_SPEED_MAX);
	}

	const std::vector<int>& MedSpeedValues() {
		static const std::vector<int> v = { 5, 10, 15, 20, 25, 30, 40, 50, 60, 75, 100 };
		return v;
	}

	int MedSpeedIndex(int speed) {
		const std::vector<int>& v = MedSpeedValues();
		int best = 0;
		for (int i = 1; i < (int)v.size(); i++)
			if (std::abs(v[i] - speed) < std::abs(v[best] - speed))
				best = i;
		return best;
	}

	double MedClockTopUp(double tempo, double realMs, double engineMs, double worldTicks, double ticksPerDay, double ticksToTarget) {
		if (tempo <= 0.0 || realMs <= 0.0)
			return 0.0;
		double extra = tempo * realMs - std::max(engineMs, 0.0);
		double toMidnight = ticksPerDay - 1.0 - worldTicks;
		extra = std::min(extra, toMidnight);
		extra = std::min(extra, ticksToTarget);
		return extra > 0.0 ? extra : 0.0;
	}

	float MedHeroTimeScale(double realMs, double engineMs) {
		if (engineMs <= 0.0 || realMs <= 0.0)
			return 1.0f;
		return (float)std::clamp(realMs / engineMs, 0.02, 1.0);
	}

	// ---------- regeneracja ----------
	const char* MedRegenLabel(int mode) {
		switch (mode) {
		case MED_REGEN_NONE: return "brak";
		case MED_REGEN_GRADUAL: return "stopniowa";
		case MED_REGEN_SLEEP: return "jak sen";
		default: return "?";
		}
	}

	// Jeden zasob: przyrost max * dHours / regenHours, czesc calkowita teraz, ulamek w acc; pelny = acc zerowany.
	static int RegenOne(double dHours, double regenHours, int cur, int max, double& acc) {
		if (max <= 0 || cur >= max) {
			acc = 0.0;
			return 0;
		}
		acc += (double)max * dHours / regenHours;
		int add = (int)std::floor(acc);
		if (add <= 0)
			return 0;
		acc -= (double)add;
		if (cur + add >= max) {
			add = max - cur;
			acc = 0.0;
		}
		return add;
	}

	void MedRegenStep(int mode, double dHours, double regenHours, int hp, int hpMax, int mana, int manaMax,
		MedRegenAcc& acc, int& addHp, int& addMana) {
		addHp = addMana = 0;
		if (mode != MED_REGEN_GRADUAL || dHours <= 0.0 || regenHours <= 0.0)
			return;
		addHp = RegenOne(dHours, regenHours, hp, hpMax, acc.hp);
		addMana = RegenOne(dHours, regenHours, mana, manaMax, acc.mana);
	}

	bool MedSleepRestores(int mode, double waitedHours, double sleepHours) {
		return mode == MED_REGEN_SLEEP && waitedHours + 1e-6 >= sleepHours;
	}

	// ---------- start ----------
	MedDeny MedStartDeny(const MedHeroState& s) {
		if (!s.enabled)
			return MDENY_OFF;
		if (!s.anis)
			return MDENY_NO_ANIS;
		if (!s.human)
			return MDENY_NOT_HUMAN;
		if (s.down)
			return MDENY_DOWN;
		if (s.window)
			return MDENY_WINDOW;
		if (s.mounted)
			return MDENY_MOUNTED;
		if (s.pluginMove)
			return MDENY_PLUGIN_MOVE;
		if (s.movLock)
			return MDENY_MOVLOCK;
		if (s.attacking)
			return MDENY_ATTACK;
		if (s.busy)
			return MDENY_BUSY;
		if (s.water)
			return MDENY_WATER;
		if (!s.bodyOk)
			return MDENY_BODY;
		if (s.enemy)
			return MDENY_ENEMY;
		return MDENY_OK;
	}

	const char* MedDenyText(MedDeny d) {
		switch (d) {
		case MDENY_OK: return "mozna";
		case MDENY_OFF: return "wylaczona (Meditate=0)";
		case MDENY_NO_ANIS: return "brak nakladki / animacji medytacji";
		case MDENY_NOT_HUMAN: return "bohater nie w modelu czlowieka";
		case MDENY_DOWN: return "bohater martwy / nieprzytomny";
		case MDENY_WINDOW: return "otwarte okno (ekwipunek, dialog, konsola, inne okno gry)";
		case MDENY_MOUNTED: return "na wierzchowcu";
		case MDENY_PLUGIN_MOVE: return "trwa ruch pluginu (unik, przeskok, finisher, podnoszenie)";
		case MDENY_MOVLOCK: return "blokada sterowania (skrypt / inna wtyczka)";
		case MDENY_BUSY: return "kolejka komunikatow bohatera niepusta";
		case MDENY_ATTACK: return "trwa cios";
		case MDENY_BODY: return "bohater nie stoi na ziemi (skok, upadek, wspinanie, siedzenie...)";
		case MDENY_WATER: return "w wodzie";
		case MDENY_ENEMY: return "walczacy wrog w poblizu";
		default: return "?";
		}
	}

	const char* MedDenyScreen(MedDeny d) {
		switch (d) {
		case MDENY_NO_ANIS: return "Medytacja: brak animacji (log zDismember)";
		case MDENY_MOUNTED: return "Nie mo\xBF" "esz medytowa\xE6 na wierzchowcu";
		case MDENY_BODY: return "Nie mo\xBF" "esz teraz medytowa\xE6";
		case MDENY_WATER: return "Nie mo\xBF" "esz medytowa\xE6 w wodzie";
		case MDENY_ENEMY: return "Nie mo\xBF" "esz medytowa\xE6, gdy w pobli\xBF" "u s\xB9 wrogowie";
		case MDENY_ATTACK:
		case MDENY_BUSY:
		case MDENY_PLUGIN_MOVE: return "Nie mo\xBF" "esz teraz medytowa\xE6";
		default: return nullptr;   // MDENY_MOVLOCK tez bez napisu: okna innych wtyczek (zTransmog, NB) blokuja sterowanie,
		                           // a Enter jest w nich zatwierdzeniem
		}
	}

	// ---------- maszyna stanow ----------
	const char* MedStateName(int s) {
		switch (s) {
		case MED_IDLE: return "brak";
		case MED_SHEATHE: return "chowanie broni";
		case MED_KNEEL: return "klekanie";
		case MED_MENU: return "menu";
		case MED_WAIT: return "czekanie";
		case MED_RISE: return "wstawanie";
		default: return "?";
		}
	}

	const char* MedStopText(int s) {
		switch (s) {
		case MSTOP_NONE: return "-";
		case MSTOP_TARGET: return "osiagnieta godzina";
		case MSTOP_KEY: return "klawisz (Esc / Enter)";
		case MSTOP_HIT: return "bohater trafiony (spadek HP)";
		case MSTOP_ENEMY: return "walczacy wrog w poblizu";
		case MSTOP_DIALOG: return "dialog";
		case MSTOP_DOWN: return "bohater martwy / nieprzytomny / inny model";
		case MSTOP_LOST: return "animacja kleczenia przerwana przez gre";
		case MSTOP_SHEATHE: return "bron sie nie schowala";
		case MSTOP_MENU: return "wstan z menu";
		case MSTOP_EXTERNAL: return "z zewnatrz (zapis / wczytanie / wylaczenie / konsola)";
		default: return "?";
		}
	}

	static MedStep Go(MedMachine& m, MedState to, MedAct act, MedStop why) {
		MedStep st;
		st.act = act;
		st.why = why;
		st.from = m.state;
		m.state = to;
		m.t = 0.0f;
		if (why != MSTOP_NONE)
			m.lastStop = why;
		return st;
	}

	// Zagrozenia wspolne dla klekania, menu i czekania: twarde = koniec od razu, wrog = wstawanie.
	static bool HardStop(const MedSense& s, MedStop& why) {
		if (s.down)
			why = MSTOP_DOWN;
		else if (s.dialog)
			why = MSTOP_DIALOG;
		else if (s.hit)
			why = MSTOP_HIT;
		else
			return false;
		return true;
	}

	MedStep MedBegin(MedMachine& m, bool weaponOut) {
		m.waitFrom = m.waitTo = 0.0;
		m.lastStop = MSTOP_NONE;
		if (weaponOut)
			return Go(m, MED_SHEATHE, MACT_NONE, MSTOP_NONE);
		return Go(m, MED_KNEEL, MACT_KNEEL, MSTOP_NONE);
	}

	MedStep MedUpdate(MedMachine& m, const MedSense& s, float dt) {
		MedStep none;
		none.from = m.state;
		if (m.state == MED_IDLE)
			return none;
		m.t += dt > 0.0f ? dt : 0.0f;
		MedStop why = MSTOP_NONE;
		switch (m.state) {
		case MED_SHEATHE:
			if (HardStop(s, why))
				return Go(m, MED_IDLE, MACT_ABORT, why);
			if (s.enemy)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_ENEMY);
			if (s.cancelKey)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_KEY);
			if (!s.weaponOut)
				return Go(m, MED_KNEEL, MACT_KNEEL, MSTOP_NONE);
			if (m.t > MED_SHEATHE_MAX)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_SHEATHE);
			return none;
		case MED_KNEEL:
			if (HardStop(s, why))
				return Go(m, MED_IDLE, MACT_ABORT, why);
			if (s.enemy)
				return Go(m, MED_RISE, MACT_RISE, MSTOP_ENEMY);
			if (s.cancelKey)
				return Go(m, MED_RISE, MACT_RISE, MSTOP_KEY);
			if (s.loopPlaying || m.t > MED_KNEEL_MAX)
				return Go(m, MED_MENU, MACT_OPEN_MENU, MSTOP_NONE);
			if (!s.startPlaying && m.t > MED_ANI_GRACE)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_LOST);
			return none;
		case MED_MENU:
			if (HardStop(s, why))
				return Go(m, MED_IDLE, MACT_ABORT, why);
			if (s.enemy)
				return Go(m, MED_RISE, MACT_RISE, MSTOP_ENEMY);
			if (!s.loopPlaying && !s.startPlaying && m.t > MED_ANI_GRACE)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_LOST);
			return none;
		case MED_WAIT:
			if (HardStop(s, why))
				return Go(m, MED_IDLE, MACT_ABORT, why);
			if (s.enemy)
				return Go(m, MED_RISE, MACT_RISE, MSTOP_ENEMY);
			if (!s.loopPlaying && !s.startPlaying && m.t > MED_ANI_GRACE)
				return Go(m, MED_IDLE, MACT_ABORT, MSTOP_LOST);
			if (s.now + 1e-6 >= m.waitTo)
				return Go(m, MED_MENU, MACT_WAIT_END, MSTOP_TARGET);
			if (s.cancelKey)
				return Go(m, MED_MENU, MACT_WAIT_END, MSTOP_KEY);
			return none;
		case MED_RISE:
			if (s.down || s.dialog)
				return Go(m, MED_IDLE, MACT_ABORT, s.down ? MSTOP_DOWN : MSTOP_DIALOG);
			if ((!s.stopPlaying && m.t > MED_ANI_GRACE) || m.t > MED_RISE_MAX)
				return Go(m, MED_IDLE, MACT_FINISH, m.lastStop);
			return none;
		default:
			return none;
		}
	}

	bool MedStartWait(MedMachine& m, double now, double target) {
		if (m.state != MED_MENU || target <= now)
			return false;
		m.waitFrom = now;
		m.waitTo = target;
		Go(m, MED_WAIT, MACT_NONE, MSTOP_NONE);
		return true;
	}

	MedStep MedRise(MedMachine& m, MedStop why) {
		if (m.state != MED_MENU && m.state != MED_KNEEL && m.state != MED_WAIT) {
			MedStep none;
			none.from = m.state;
			return none;
		}
		return Go(m, MED_RISE, MACT_RISE, why);
	}

	MedStep MedAbort(MedMachine& m, MedStop why) {
		if (m.state == MED_IDLE) {
			MedStep none;
			return none;
		}
		return Go(m, MED_IDLE, MACT_ABORT, why);
	}

	bool MedActive(const MedMachine& m) {
		return m.state != MED_IDLE;
	}

	double MedWaitProgress(const MedMachine& m, double now) {
		double span = m.waitTo - m.waitFrom;
		if (span <= 0.0)
			return 1.0;
		return std::clamp((now - m.waitFrom) / span, 0.0, 1.0);
	}

	// ---------- dar aktywny ----------
	std::vector<int> MedGodChoices(int innos, int beliar, int adanos) {
		std::vector<int> out;
		int n = (innos >= 1) + (beliar >= 1) + (adanos >= 1);
		if (n < 2)
			return out;
		out.push_back(MGOD_AUTO);
		if (innos >= 1)
			out.push_back(MGOD_INNOS);
		if (adanos >= 1)
			out.push_back(MGOD_ADANOS);
		if (beliar >= 1)
			out.push_back(MGOD_BELIAR);
		return out;
	}

	int MedGodChoiceIndex(const std::vector<int>& choices, int slot90) {
		for (size_t i = 0; i < choices.size(); i++)
			if (choices[i] == slot90)
				return (int)i;
		return 0;
	}

	const char* MedGodLabel(int god) {
		switch (god) {
		case MGOD_INNOS: return "Innos";
		case MGOD_ADANOS: return "Adanos";
		case MGOD_BELIAR: return "Beliar";
		default: return "auto";
		}
	}

	// ---------- kamera orbitalna ----------
	float MedCamWrap(float deg) {
		float d = std::fmod(deg, 360.0f);
		if (d > 180.0f)
			d -= 360.0f;
		else if (d <= -180.0f)
			d += 360.0f;
		return d;
	}

	float MedCamDelta(float from, float to) {
		return MedCamWrap(to - from);
	}

	float MedCamApproach(float cur, float goal, float dt, float tau, float minSpeed) {
		float d = goal - cur;
		if (dt <= 0.0f || d == 0.0f)
			return cur;
		float step = tau > 0.0f ? std::fabs(d) * (1.0f - std::exp(-dt / tau)) : std::fabs(d);
		step = std::max(step, minSpeed * dt);
		if (step >= std::fabs(d))
			return goal;
		return cur + (d > 0.0f ? step : -step);
	}

	float MedCamAzi(const MedCam& c) {
		return MedCamWrap(c.azi);
	}

	// Biezacy i cel przesuwane razem o pelny obrot - liczby nie rosna, roznica (droga obrotu) bez zmian.
	static void CamNormalize(MedCam& c) {
		while (c.azi > 180.0f) {
			c.azi -= 360.0f;
			c.aziGoal -= 360.0f;
		}
		while (c.azi <= -180.0f) {
			c.azi += 360.0f;
			c.aziGoal += 360.0f;
		}
	}

	void MedCamBegin(MedCam& c, float azi, float elev, float base) {
		c = MedCam();
		c.phase = MCAM_ACTIVE;
		c.azi = c.aziGoal = MedCamWrap(azi);
		c.elev = c.elevGoal = elev;
		c.elevMin = std::min(MED_CAM_ELEV_MIN, elev);
		c.elevMax = std::max(MED_CAM_ELEV_MAX, elev);
		c.baseAzi = MedCamWrap(base);
	}

	void MedCamMouse(MedCam& c, float dx, float dy, float degPerUnit, int elevSign) {
		if (c.phase != MCAM_ACTIVE)
			return;
		float da = -dx * degPerUnit;
		c.aziGoal += da;
		c.turned += std::fabs(da);
		c.elevGoal = std::clamp(c.elevGoal + dy * degPerUnit * (elevSign < 0 ? -1.0f : 1.0f), c.elevMin, c.elevMax);
	}

	bool MedCamStep(MedCam& c, float dt) {
		if (c.phase == MCAM_OFF)
			return false;
		if (c.phase == MCAM_RETURN && dt > MED_CAM_GAP) {
			// kamera stala (dialog, menu gry) - powrot bez animacji
			c.azi = c.aziGoal;
			CamNormalize(c);
			c.phase = MCAM_OFF;
			return true;
		}
		float s = std::clamp(dt, 0.0f, MED_CAM_DT_MAX);
		c.t += s;
		if (c.phase == MCAM_ACTIVE) {
			c.azi = MedCamApproach(c.azi, c.aziGoal, s, MED_CAM_TAU, 1.0f);
			c.elev = MedCamApproach(c.elev, c.elevGoal, s, MED_CAM_TAU, 1.0f);
			CamNormalize(c);
			return true;
		}
		c.azi = MedCamApproach(c.azi, c.aziGoal, s, MED_CAM_RETURN_TAU, MED_CAM_RETURN_MIN_SPEED);
		if (std::fabs(c.aziGoal - c.azi) <= MED_CAM_RETURN_DONE || c.t >= MED_CAM_RETURN_MAX) {
			c.azi = c.aziGoal;
			c.phase = MCAM_OFF;
		}
		CamNormalize(c);
		return true;
	}

	void MedCamRelease(MedCam& c) {
		if (c.phase != MCAM_ACTIVE)
			return;
		c.phase = MCAM_RETURN;
		c.t = 0.0f;
		c.aziGoal = c.azi + MedCamDelta(MedCamWrap(c.azi), c.baseAzi);
		c.elevGoal = c.elev;
	}

	void MedCamModeChanged(MedCam& c, float newBase) {
		if (c.phase == MCAM_ACTIVE)
			c.baseAzi = MedCamWrap(newBase);
		else if (c.phase == MCAM_RETURN)
			c.phase = MCAM_OFF;
	}

	bool MedCamModeOrbits(const std::string& modeUpper, bool firstPerson) {
		if (firstPerson)
			return false;
		return modeUpper != "CAMMODFIRSTPERSON" && modeUpper != "CAMMODLOOK" && modeUpper != "CAMMODDIALOG";
	}

	float MedCamTrackerDt(float engineDt, float realDt) {
		float r = std::clamp(realDt, 0.0f, MED_CAM_DT_MAX);
		return r > engineDt * MED_CAM_DT_FIX ? r : engineDt;
	}
}
