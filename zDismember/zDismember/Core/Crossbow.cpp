// Logika bez silnika: szybki strzal z kuszy (Crossbow.h).
#include "Crossbow.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace zdcore {
	const char* XbowRefuseLabel(int r) {
		static const char* const L[XR_COUNT] = { "ok", "wylaczone (Crossbow=0)", "brak nakladki HUMANS_ZDF_XBOW (paczka W3)",
			"bohater nie w modelu czlowieka", "bohater nieprzytomny / martwy", "dialog / przerywnik / blokada ruchu",
			"otwarty ekwipunek", "inna akcja w toku (unik, przeskok, finisher, gest Znaku)", "woda", "skok / upadek",
			"mob / drabina", "tryb broni 5 / 6 / 7", "brak zalozonej kuszy", "zalozony luk (tylko kusza)",
			"pochodnia w rece (XbowWithTorch=0)", "tarcza (XbowWithShield=0)", "bron parowa (XbowWithDual=0)", "brak beltow",
			"przeladowanie", "strzal juz trwa" };
		return r >= 0 && r < XR_COUNT ? L[r] : "?";
	}

	// cp1250: \xB3 l z kreska, \xEA e z ogonkiem, \xB9 a z ogonkiem, \xF3 o z kreska, \xF1 n z kreska, \x9C s z kreska
	const char* XbowRefuseMessage(int r) {
		switch (r) {
		case XR_NOXBOW: return "Brak za\xB3o\xBFonej kuszy";
		case XR_BOW: return "Szybki strza\xB3 tylko z kuszy";
		case XR_TORCH: return "Najpierw schowaj pochodni\xEA";
		case XR_SHIELD: return "Z tarcz\xB9 nie strzelisz z kuszy";
		case XR_DUAL: return "Z dwiema bro\xF1mi nie strzelisz z kuszy";
		case XR_NOBOLTS: return "Brak be\xB3t\xF3w";
		case XR_MODE: return "Strzelaj zwykle (kusza / \xB3uk / czar w r\xEA" "ce)";
		case XR_WATER: return "Nie w wodzie";
		case XR_NOPACK: return "Kusza: brak animacji (paczka zDismember_W3.vdf)";
		default: return "";
		}
	}

	int XbowGateCheck(const XbowGate& g) {
		if (!g.enabled)
			return XR_OFF;
		if (!g.pack)
			return XR_NOPACK;
		if (!g.human)
			return XR_MODEL;
		if (!g.alive)
			return XR_DEAD;
		if (g.active)
			return XR_ACTIVE;
		if (g.dialog)
			return XR_DIALOG;
		if (g.inventory)
			return XR_INVENTORY;
		if (g.busy)
			return XR_BUSY;
		if (g.water)
			return XR_WATER;
		if (g.airborne)
			return XR_AIR;
		if (g.mob)
			return XR_MOB;
		if (g.fmode == 5 || g.fmode == 6 || g.fmode == 7)
			return XR_MODE;
		if (!g.hasCrossbow)
			return g.isBow ? XR_BOW : XR_NOXBOW;
		if (g.torch && !g.allowTorch)
			return XR_TORCH;
		if (g.shield && !g.allowShield)
			return XR_SHIELD;
		if (g.dual && !g.allowDual)
			return XR_DUAL;
		if (!g.hasBolts && !g.createAmmo)
			return XR_NOBOLTS;
		if (g.cooldownLeftMs > 0)
			return XR_COOLDOWN;
		return XR_OK;
	}

	static bool IniValue(const std::string& text, const char* key, float& out) {
		std::istringstream in(text);
		std::string line;
		size_t kl = strlen(key);
		while (std::getline(in, line)) {
			size_t p = line.find_first_not_of(" \t");
			if (p == std::string::npos || line[p] == ';' || line[p] == '#' || line[p] == '[')
				continue;
			if (line.compare(p, kl, key) != 0)
				continue;
			size_t eq = line.find('=', p + kl);
			if (eq == std::string::npos || line.find_first_not_of(" \t", p + kl) != eq)
				continue;
			char* end = nullptr;
			const char* s = line.c_str() + eq + 1;
			double v = strtod(s, &end);
			if (end == s)
				return false;
			out = (float)v;
			return true;
		}
		return false;
	}

	bool ParseXbowIni(const std::string& text, XbowTimes& t, std::string& err) {
		XbowTimes r = t;
		struct K { const char* key; float* dst; bool required; };
		K keys[] = { { "InMs", &r.inLen, true }, { "LoopMs", &r.loopLen, true }, { "ShootMs", &r.shootLen, true },
			{ "GrabMs", &r.grab, false }, { "FireMs", &r.fire, false }, { "ReleaseMs", &r.release, false },
			{ "TurnMs", &r.turn, false }, { "ShootOutMs", &r.outStart, false } };
		for (auto& k : keys) {
			float ms = 0.0f;
			if (!IniValue(text, k.key, ms)) {
				if (k.required) {
					err = std::string("brak ") + k.key;
					return false;
				}
				continue;
			}
			if (!(ms >= 0.0f && ms < 10000.0f)) {
				err = std::string("zla wartosc ") + k.key;
				return false;
			}
			*k.dst = ms / 1000.0f;
		}
		float grip = 0.0f;
		if (IniValue(text, "GripCm", grip)) {
			if (!(grip > -200.0f && grip < 200.0f)) {
				err = "zla wartosc GripCm";
				return false;
			}
			r.gripCm = grip;
		}
		if (r.inLen < 0.05f || r.loopLen < 0.05f || r.shootLen < 0.05f) {
			err = "za krotkie klipy";
			return false;
		}
		if (r.grab > r.inLen)
			r.grab = r.inLen;
		if (r.fire > r.shootLen)
			r.fire = r.shootLen;
		if (r.release < r.fire)
			r.release = r.fire;
		if (r.release > r.shootLen)
			r.release = r.shootLen;
		if (r.outStart > r.release)
			r.outStart = r.release;
		t = r;
		return true;
	}

	float XbowTimeScale(int speedPct) {
		if (speedPct < 25)
			speedPct = 25;
		if (speedPct > 400)
			speedPct = 400;
		return 100.0f / (float)speedPct;
	}

	const char* XbowPhaseName(int p) {
		static const char* const N[XP_COUNT] = { "spoczynek", "wejscie", "celowanie", "strzal" };
		return p >= 0 && p < XP_COUNT ? N[p] : "?";
	}

	void XbowStart(XbowRun& r) {
		r = XbowRun();
		r.phase = XP_IN;
	}

	bool XbowApplyInput(XbowRun& r, int in) {
		if (!r.Active())
			return false;
		switch (in) {
		case XIN_HOLD:
			if (r.phase == XP_IN && r.intent == XI_UNDECIDED) {
				r.intent = XI_HOLD;
				return true;
			}
			return false;
		case XIN_RELEASE:
			if (r.intent == XI_UNDECIDED && r.phase == XP_IN) {
				r.intent = XI_TAP;
				return true;
			}
			if (r.intent == XI_HOLD && (r.phase == XP_IN || r.phase == XP_LOOP)) {
				r.intent = XI_RELEASED;
				return true;
			}
			return false;
		case XIN_CANCEL:
			if (!r.fired && r.phase != XP_SHOOT) {
				r.intent = XI_CANCEL;
				return true;
			}
			return false;
		default:
			return false;
		}
	}

	XbowEvents XbowStep(XbowRun& r, const XbowTimes& tm, float scale, float dt) {
		XbowEvents e;
		if (!r.Active())
			return e;
		if (dt < 0.0f)
			dt = 0.0f;
		r.t += dt;
		r.total += dt;
		if (r.phase == XP_IN) {
			if (!r.grabbed && r.t >= tm.grab * scale) {
				r.grabbed = true;
				e.grab = true;
			}
			if (r.t >= tm.inLen * scale) {
				r.t -= tm.inLen * scale;
				if (r.intent == XI_HOLD) {
					r.phase = XP_LOOP;
					r.aimed = true;
					e.startLoop = true;
				}
				else {
					r.phase = XP_SHOOT;   // stukniecie, puszczenie po progu przed koncem wejscia, brak decyzji
					e.startShoot = true;
				}
			}
		}
		else if (r.phase == XP_LOOP) {
			if (r.intent == XI_RELEASED) {
				r.phase = XP_SHOOT;
				r.t = 0.0f;
				e.startShoot = true;
			}
		}
		if (r.phase == XP_SHOOT) {
			if (!r.fired && r.t >= tm.fire * scale) {
				r.fired = true;
				e.fire = true;
			}
			if (!r.released && r.t >= tm.release * scale) {
				r.released = true;
				e.release = true;
			}
			if (r.t >= tm.shootLen * scale) {
				e.end = true;
				if (!r.released) {
					r.released = true;
					e.release = true;
				}
				r.phase = XP_IDLE;
			}
		}
		return e;
	}

	bool XbowCancelOut(XbowRun& r, const XbowTimes& tm, float scale) {
		if (!r.Active() || !r.grabbed || r.fired || r.released)
			return false;
		r.phase = XP_SHOOT;
		r.intent = XI_CANCEL;
		r.t = tm.outStart * scale;
		r.fired = true;   // bez zdarzenia fire: bez beltu i przeladowania
		return true;
	}

	float XbowTapFireAt(const XbowTimes& tm, float scale) {
		return (tm.inLen + tm.fire) * scale;
	}

	int XbowCutVerdict(const XbowRun& r, int cut) {
		if (!r.Active())
			return XCA_IGNORE;
		if (!r.fired)
			return XCA_ABORT_FREE;
		if (cut == XC_HIT || cut == XC_CANCEL)
			return XCA_IGNORE;   // po belcie: trafienie nie przerywa (W3), PPM nic nie znaczy
		return XCA_ABORT_AFTER;
	}

	const char* XbowCutLabel(int cut) {
		static const char* const L[XC_COUNT] = { "trafienie", "unik / przewrot / przeskok / finisher", "ekwipunek",
			"zmiana broni / kuszy", "dialog / blokada", "anulowanie (PPM)", "smierc / omdlenie", "reset (wczytanie, zapis, model)" };
		return cut >= 0 && cut < XC_COUNT ? L[cut] : "?";
	}

	float XbowAngleDeg(float fx, float fz, float dx, float dz) {
		float a = sqrtf(fx * fx + fz * fz), b = sqrtf(dx * dx + dz * dz);
		if (a < 1e-6f || b < 1e-6f)
			return 180.0f;
		float c = (fx * dx + fz * dz) / (a * b);
		c = c < -1.0f ? -1.0f : c > 1.0f ? 1.0f : c;
		return acosf(c) * 57.29578f;
	}

	float XbowScore(const XbowCand& c, float camX, float camZ, const XbowAimRules& rules) {
		if (!c.alive || !c.visible || (!c.hostile && !rules.neutral))
			return -1.0f;
		float dist = sqrtf(c.dx * c.dx + c.dz * c.dz + c.dy * c.dy);
		if (dist > rules.range || fabsf(c.dy) > rules.maxRise || rules.range <= 0.0f)
			return -1.0f;
		float ang = XbowAngleDeg(camX, camZ, c.dx, c.dz);
		if (dist > 80.0f && ang > rules.coneDeg)
			return -1.0f;
		float cone = rules.coneDeg > 1.0f ? rules.coneDeg : 1.0f;
		return ang / cone + dist / rules.range;
	}

	int XbowPickTarget(const std::vector<XbowCand>& c, float camX, float camZ, float heroX, float heroZ, bool moving,
		const XbowAimRules& rules) {
		auto moveOk = [&](const XbowCand& x) {
			return !moving || XbowAngleDeg(heroX, heroZ, x.dx, x.dz) <= rules.moveConeDeg;
		};
		for (size_t i = 0; i < c.size(); i++) {
			const XbowCand& x = c[i];
			if (!x.focus || !x.alive || !x.visible || (!x.hostile && !rules.neutral))
				continue;
			float dist = sqrtf(x.dx * x.dx + x.dz * x.dz + x.dy * x.dy);
			if (dist <= rules.range && fabsf(x.dy) <= rules.maxRise && XbowAngleDeg(heroX, heroZ, x.dx, x.dz) <= 90.0f &&
				moveOk(x))
				return (int)i;
		}
		int best = -1;
		float bestScore = 0.0f;
		for (size_t i = 0; i < c.size(); i++) {
			float s = XbowScore(c[i], camX, camZ, rules);
			if (s < 0.0f || !moveOk(c[i]))
				continue;
			if (best < 0 || s < bestScore) {
				best = (int)i;
				bestScore = s;
			}
		}
		return best;
	}

	int XbowCooldownMs(const int ms[3], int talent) {
		if (talent < 0)
			talent = 0;
		if (talent > 2)
			talent = 2;
		int v = ms[talent];
		return v < 0 ? 0 : v;
	}

	int XbowBorrowSlot(const std::vector<XbowProtoInfo>& protos, int* why) {
		int w = XB_OK, out = -1;
		int top = -1;
		for (int i = (int)protos.size() - 1; i >= 0; i--)
			if (protos[i].hasEntry) {
				top = i;
				break;
			}
		if (top > 0 && protos[top].ours)
			out = top;
		else if (top > 0)
			w = XB_FOREIGN_ABOVE;
		else {
			for (int i = (int)protos.size() - 1; i > 0; i--)
				if (protos[i].ours) {
					out = i;
					break;
				}
			if (out < 0)
				w = protos.empty() ? XB_EMPTY : XB_NO_OURS;
		}
		if (why)
			*why = w;
		return out;
	}

	const char* XbowBorrowWhyLabel(int why) {
		switch (why) {
		case XB_OK: return "ok";
		case XB_FOREIGN_ABOVE: return "obca nakladka z ta animacja nad naszymi";
		case XB_NO_OURS: return "brak naszej nakladki na modelu";
		case XB_EMPTY: return "model bez prototypow";
		default: return "?";
		}
	}

	const char* XbowClipAni(int clip, bool standing) {
		static const char* const STAND[XCLIP_COUNT] = { "T_FOOD_RANDOM_1", "T_FOOD_RANDOM_2", "T_MEAT_RANDOM_1" };
		static const char* const MOVE[XCLIP_COUNT] = { "R_CHAIR_RANDOM_1", "R_CHAIR_RANDOM_2", "R_CHAIR_RANDOM_3" };
		if (clip < 0 || clip >= XCLIP_COUNT)
			return "";
		return standing ? STAND[clip] : MOVE[clip];
	}

	const char* XbowClipName(int clip) {
		static const char* const N[XCLIP_COUNT] = { "XBOW_IN", "XBOW_LOOP", "XBOW_SHOOT" };
		return clip >= 0 && clip < XCLIP_COUNT ? N[clip] : "?";
	}
}
