#include "Overlays.h"
#include "Text.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zdcore {
	static bool Ours(const std::string& upper) {
		return upper.rfind("HUMANS_ZDF_", 0) == 0;
	}

	bool IsForeignSprintOverlay(const std::string& name) {
		std::string u = Upper(name);
		return u.find("SPRINT") != std::string::npos && !Ours(u);
	}

	std::string ForeignSprintOverlay(const std::vector<std::string>& model) {
		for (size_t i = model.size(); i > 0; i--)
			if (IsForeignSprintOverlay(model[i - 1]))
				return model[i - 1];
		return "";
	}

	bool IsExclusiveForeignOverlay(const std::string& name) {
		std::string u = Upper(name);
		return u.find("MOUNT") != std::string::npos && !Ours(u);
	}

	bool HasExclusiveForeignOverlay(const std::vector<std::string>& model) {
		for (auto& m : model)
			if (IsExclusiveForeignOverlay(m))
				return true;
		return false;
	}

	static bool IsOurSprint(const std::string& name) {
		std::string u = Upper(name);
		return u.find("SPRINT") != std::string::npos && Ours(u);
	}

	// Nakladki cudzych mechanik: gesty Znakow zQuickSign (HUMANS_ZDF_SIGNS, nie nachodza na nasze nazwy) i ruchy
	// (unik / przewrot, na chwile) - zdejmuje je ten, kto je zalozyl.
	bool IsTransientOverlay(const std::string& name) {
		std::string u = Upper(name);
		return u.rfind("HUMANS_ZDF_SIGN", 0) == 0 || u.rfind("HUMANS_ZDF_MV_", 0) == 0;
	}

	static int IndexOf(const std::vector<std::string>& v, const std::string& s) {
		auto it = std::find(v.begin(), v.end(), s);
		return it == v.end() ? -1 : (int)(it - v.begin());
	}

	WantedSet WantedMotion(const MotionSetup& s, bool foreignSprint) {
		return WantedMotion(s, foreignSprint, {});
	}

	WantedSet WantedMotion(const MotionSetup& s, bool foreignSprint, const std::vector<std::string>& keepParked) {
		WantedSet w;
		for (auto& st : s.styles)
			if (!st.empty() && IndexOf(w.top, st) < 0)
				w.top.push_back(st);
		for (auto& l : s.weaponLoco)
			if (!l.empty())
				w.top.push_back(l);
		if (!s.loco.empty())
			w.top.push_back(s.loco);
		std::vector<std::string> sprint;
		if (!s.sprint.empty())
			sprint.push_back(s.sprint);
		for (auto& sp : s.weaponSprint)
			if (!sp.empty())
				sprint.push_back(sp);
		for (auto& sp : sprint) {
			bool up = foreignSprint && IndexOf(keepParked, sp) < 0;
			(up ? w.top : w.parked).push_back(sp);
		}
		return w;
	}

	int CycleLocoMode(int mode, int step) {
		if (mode < 0 || mode >= LOCO_MODE_COUNT)
			return LOCO_AUTO;
		int n = LOCO_MODE_COUNT;
		return ((mode + (step < 0 ? -1 : step > 0 ? 1 : 0)) % n + n) % n;
	}

	LocoFlags ResolveLocoMode(int mode, bool iniLoco, bool iniSprint) {
		LocoFlags f;
		switch (mode) {
		case LOCO_GAME:
			break;
		case LOCO_W3_RUN:
			f.loco = true;
			break;
		case LOCO_W3_FULL:
			f.loco = f.sprint = true;
			break;
		case LOCO_SPRINT_PREVIEW:
			f.loco = f.sprint = f.sprintOnTop = true;
			break;
		default:
			f.loco = iniLoco;
			f.sprint = iniSprint;
			break;
		}
		return f;
	}

	const char* LocoModeLabel(int mode) {
		switch (mode) {
		case LOCO_GAME: return "Gothic / zEBA (bez ruchu W3)";
		case LOCO_W3_RUN: return "W3 trucht i chod, sprint gry";
		case LOCO_W3_FULL: return "W3 trucht, chod i sprint";
		case LOCO_SPRINT_PREVIEW: return "W3 sprint jako zwykly bieg (podglad)";
		default: return "AUTO (wg ini)";
		}
	}

	bool IsOurSprintOverlay(const std::string& name) {
		return IsOurSprint(name);
	}

	std::vector<std::string> ForeignSprintOverlays(const std::vector<std::string>& model) {
		std::vector<std::string> out;
		for (auto& m : model)
			if (IsForeignSprintOverlay(m))
				out.push_back(m);
		return out;
	}

	std::string SprintModLabel(const std::string& overlay) {
		std::string u = Upper(overlay);
		if (IsOurSprint(u))
			return "W3 (zDismember)";
		if (u.rfind("HUMANS_SPRINT_REMASTER", 0) == 0)
			return "zEBA";
		if (u.rfind("NINJA_SPRINT", 0) == 0)
			return "Ninja Sprint";
		if (u == "HUMANS_SPRINT" || u == "HUMANS_SPRINT.MDS")
			return "mikstura szybkosci / zEBA";
		return "inny mod";
	}

	std::string MotionSourceLabel(const std::string& overlay) {
		std::string u = Upper(overlay);
		if (u.empty())
			return "-";
		if (u == "MODEL")
			return "Gothic";
		if (IsOurSprint(u))
			return "W3 sprint";
		if (u.rfind("HUMANS_ZDF_W3_LOCO", 0) == 0)
			return "W3 trucht";
		if (IsForeignSprintOverlay(u))
			return "sprint " + SprintModLabel(u);
		return overlay;
	}

	float SprintTempoScale(float refVelocity, float ourVelocity) {
		if (!std::isfinite(refVelocity) || !std::isfinite(ourVelocity) || refVelocity <= 0.0f || ourVelocity <= 0.0f)
			return 1.0f;
		float r = refVelocity / ourVelocity;
		return r >= 0.5f - 1e-6f && r <= 2.0f + 1e-6f ? r : 1.0f;
	}

	float SprintTargetScale(float targetVelocity, float refVelocity, float ourVelocity) {
		return SprintTempoScale(targetVelocity > 0.0f ? targetVelocity : refVelocity, ourVelocity);
	}

	std::vector<std::string> SprintAniNames(const std::string& loop) {
		std::string u = Upper(loop);
		// S_<bron>RUNL
		if (u.size() < 6 || u.rfind("S_", 0) != 0 || u.compare(u.size() - 4, 4, "RUNL") != 0)
			return {};
		std::string w = u.substr(2, u.size() - 6);
		return { u, "T_" + w + "RUN_2_" + w + "RUNL", "T_" + w + "RUNL_2_" + w + "RUN" };
	}

	std::vector<std::string> OurSprintLoops(const std::string& overlay) {
		std::string u = Upper(overlay);
		if (u == "HUMANS_ZDF_W3_SPRINT")
			return { "S_RUNL", "S_MAGRUNL" };
		if (u == "HUMANS_ZDF_W3_SPRINT_1H")
			return { "S_1HRUNL" };
		if (u == "HUMANS_ZDF_W3_SPRINT_2H")
			return { "S_2HRUNL" };
		return {};
	}

	bool SprintLoopArmed(const std::string& loop) {
		std::string u = Upper(loop);
		return u.rfind("S_1H", 0) == 0 || u.rfind("S_2H", 0) == 0;
	}

	struct KeyName {
		int dik;
		const char* names[4];   // pierwsza = etykieta
	};
	static const KeyName KEY_NAMES[] = {
		{ 0x2A, { "LSHIFT", "LEWY SHIFT", "LEFT SHIFT", "LINKE UMSCHALTTASTE" } },
		{ 0x36, { "RSHIFT", "PRAWY SHIFT", "RIGHT SHIFT", "RECHTE UMSCHALTTASTE" } },
		{ 0x1D, { "LCONTROL", "LCTRL", "LEWY CTRL", "LEFT CTRL" } },
		{ 0x9D, { "RCONTROL", "RCTRL", "PRAWY CTRL", "RIGHT CTRL" } },
		{ 0x38, { "LMENU", "LALT", "LEWY ALT", "LEFT ALT" } },
		{ 0xB8, { "RMENU", "RALT", "PRAWY ALT", "RIGHT ALT" } },
		{ 0x39, { "SPACE", "SPACJA", nullptr, nullptr } },
		{ 0x0F, { "TAB", nullptr, nullptr, nullptr } },
		{ 0x3A, { "CAPITAL", "CAPSLOCK", "CAPS LOCK", nullptr } },
		{ 0x1C, { "RETURN", "ENTER", nullptr, nullptr } },
		{ 0x01, { "ESCAPE", "ESC", nullptr, nullptr } },
		{ 0x0E, { "BACK", "BACKSPACE", nullptr, nullptr } },
		{ 0x29, { "GRAVE", nullptr, nullptr, nullptr } },
		{ 0x0C, { "MINUS", nullptr, nullptr, nullptr } },
		{ 0x0D, { "EQUALS", nullptr, nullptr, nullptr } },
		{ 0x1A, { "LBRACKET", nullptr, nullptr, nullptr } },
		{ 0x1B, { "RBRACKET", nullptr, nullptr, nullptr } },
		{ 0x27, { "SEMICOLON", nullptr, nullptr, nullptr } },
		{ 0x28, { "APOSTROPHE", nullptr, nullptr, nullptr } },
		{ 0x2B, { "BACKSLASH", nullptr, nullptr, nullptr } },
		{ 0x33, { "COMMA", nullptr, nullptr, nullptr } },
		{ 0x34, { "PERIOD", nullptr, nullptr, nullptr } },
		{ 0x35, { "SLASH", nullptr, nullptr, nullptr } },
		{ 0xC8, { "UP", nullptr, nullptr, nullptr } },
		{ 0xD0, { "DOWN", nullptr, nullptr, nullptr } },
		{ 0xCB, { "LEFT", nullptr, nullptr, nullptr } },
		{ 0xCD, { "RIGHT", nullptr, nullptr, nullptr } },
		{ 0xC7, { "HOME", nullptr, nullptr, nullptr } },
		{ 0xCF, { "END", nullptr, nullptr, nullptr } },
		{ 0xC9, { "PRIOR", "PGUP", nullptr, nullptr } },
		{ 0xD1, { "NEXT", "PGDN", nullptr, nullptr } },
		{ 0xD2, { "INSERT", nullptr, nullptr, nullptr } },
		{ 0xD3, { "DELETE", nullptr, nullptr, nullptr } },
	};
	// Litery i cyfry w kolejnosci kodow DIK.
	static const char* DIK_ROW_Q = "QWERTYUIOP";   // 0x10..0x19
	static const char* DIK_ROW_A = "ASDFGHJKL";    // 0x1E..0x26
	static const char* DIK_ROW_Z = "ZXCVBNM";      // 0x2C..0x32

	int DikFromKeyName(const std::string& name) {
		std::string u = Upper(name);
		while (!u.empty() && u.back() == ' ')
			u.pop_back();
		size_t lead = u.find_first_not_of(' ');
		u = lead == std::string::npos ? "" : u.substr(lead);
		if (u.rfind("KEY_", 0) == 0)
			u = u.substr(4);
		if (u.empty())
			return 0;
		if (u.size() == 1) {
			char c = u[0];
			const char* p;
			if ((p = strchr(DIK_ROW_Q, c)) != nullptr)
				return 0x10 + (int)(p - DIK_ROW_Q);
			if ((p = strchr(DIK_ROW_A, c)) != nullptr)
				return 0x1E + (int)(p - DIK_ROW_A);
			if ((p = strchr(DIK_ROW_Z, c)) != nullptr)
				return 0x2C + (int)(p - DIK_ROW_Z);
			if (c >= '1' && c <= '9')
				return 0x02 + (c - '1');
			if (c == '0')
				return 0x0B;
			return 0;
		}
		if (u[0] == 'F' && u.size() <= 3 && isdigit((unsigned char)u[1])) {
			int f = atoi(u.c_str() + 1);
			if (f >= 1 && f <= 10)
				return 0x3B + f - 1;
			if (f == 11 || f == 12)
				return 0x57 + f - 11;
			return 0;
		}
		for (auto& k : KEY_NAMES)
			for (const char* n : k.names)
				if (n && u == n)
					return k.dik;
		return 0;
	}

	std::string KeyLabel(int dik) {
		if (dik <= 0)
			return "-";
		if (dik >= 0x10 && dik < 0x10 + 10)
			return std::string(1, DIK_ROW_Q[dik - 0x10]);
		if (dik >= 0x1E && dik < 0x1E + 9)
			return std::string(1, DIK_ROW_A[dik - 0x1E]);
		if (dik >= 0x2C && dik < 0x2C + 7)
			return std::string(1, DIK_ROW_Z[dik - 0x2C]);
		if (dik >= 0x02 && dik <= 0x0A)
			return std::string(1, (char)('1' + dik - 0x02));
		if (dik == 0x0B)
			return "0";
		if (dik >= 0x3B && dik <= 0x44)
			return "F" + std::to_string(dik - 0x3B + 1);
		if (dik == 0x57 || dik == 0x58)
			return "F" + std::to_string(dik - 0x57 + 11);
		for (auto& k : KEY_NAMES)
			if (k.dik == dik)
				return k.names[0];
		char buf[16];
		snprintf(buf, sizeof(buf), "0x%X", dik);
		return buf;
	}

	std::string DikLabel(int dik) {
		switch (dik) {
		case 0x38: return "Alt";
		case 0xB8: return "prawy Alt";
		case 0x2A: case 0x36: return "Shift";
		case 0x1D: case 0x9D: return "Ctrl";
		case 0x39: return "Spacja";
		case 0x3A: return "CapsLock";
		default: return KeyLabel(dik);
		}
	}

	int ZebaSprintKey(int choice, const std::string& keyName) {
		static const int codes[] = { 0x2A, 0x36, 0x1D, 0x9D };
		if (choice >= 0 && choice < 4)
			return codes[choice];
		return choice == 4 ? DikFromKeyName(keyName) : 0;
	}

	std::vector<std::string> SprintKeyClashes(const std::vector<SprintSource>& sources) {
		std::vector<std::string> out;
		auto fires = [](const SprintSource& s) { return s.active && (s.onForward || !s.keys.empty()); };
		for (size_t i = 0; i < sources.size(); i++)
			for (size_t j = i + 1; j < sources.size(); j++) {
				const SprintSource& a = sources[i];
				const SprintSource& b = sources[j];
				if (!fires(a) || !fires(b))
					continue;
				std::string why;
				if (a.onForward || b.onForward)
					why = (a.onForward ? a.name : b.name) + " sprintuje przy kazdym biegu do przodu";
				else
					for (int k : a.keys)
						if (k > 0 && std::find(b.keys.begin(), b.keys.end(), k) != b.keys.end()) {
							why = "ten sam klawisz " + KeyLabel(k);
							break;
						}
				if (!why.empty())
					out.push_back(a.name + " + " + b.name + ": " + why);
			}
		return out;
	}

	// want[0..len) moze zostac na miejscu: jest w kolejnosci, a nad nim leza tylko nakladki przekladane i sprint gry
	// (ponizej kazdego naszego sprintu z tej czesci).
	static bool PrefixStays(const std::vector<std::string>& model, const std::vector<std::string>& want, size_t len) {
		if (len == 0)
			return true;
		int prev = -1;
		for (size_t j = 0; j < len; j++) {
			int pos = IndexOf(model, want[j]);
			if (pos <= prev)
				return false;
			prev = pos;
		}
		int first = IndexOf(model, want[0]);
		for (size_t i = first + 1; i < model.size(); i++) {
			if (IndexOf(want, model[i]) >= 0)
				continue;   // nasza: z tej czesci albo i tak przekladana na wierzch
			if (IsTransientOverlay(model[i]))
				continue;
			if (!IsForeignSprintOverlay(model[i]))
				return false;
			for (size_t j = 0; j < len; j++)
				if (IsOurSprint(want[j]) && IndexOf(model, want[j]) < (int)i)
					return false;
		}
		return true;
	}

	size_t TopInsertIndex(const std::vector<std::string>& model, bool sprintGameOnTop) {
		size_t i = model.size();
		while (i > 0 && (IsTransientOverlay(model[i - 1]) || (sprintGameOnTop && IsForeignSprintOverlay(model[i - 1]))))
			i--;
		return i;
	}

	std::vector<std::string> ApplyPlan(const std::vector<std::string>& model, const OverlayPlan& plan, bool sprintGameOnTop) {
		std::vector<std::string> list;
		for (auto& m : model)
			if (IndexOf(plan.remove, m) < 0 && IndexOf(plan.park, m) < 0 && IndexOf(plan.apply, m) < 0)
				list.push_back(m);
		list.insert(list.begin(), plan.park.begin(), plan.park.end());
		size_t at = TopInsertIndex(list, sprintGameOnTop);
		list.insert(list.begin() + at, plan.apply.begin(), plan.apply.end());
		return list;
	}

	bool PlanStopsAnis(const std::vector<std::string>& model, const OverlayPlan& plan) {
		for (auto& r : plan.remove)
			if (IndexOf(model, r) >= 0)
				return true;
		for (auto& p : plan.park)
			if (IndexOf(model, p) < 0)
				return true;
		for (auto& a : plan.apply)
			if (IndexOf(model, a) < 0)
				return true;
		return false;
	}

	OverlayPlan PlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& want,
		const std::vector<std::string>& parked, const std::vector<std::string>& mine) {
		OverlayPlan plan;
		std::vector<std::string> rest;
		for (auto& m : model) {
			if (IndexOf(mine, m) >= 0 && IndexOf(want, m) < 0 && IndexOf(parked, m) < 0)
				plan.remove.push_back(m);
			else
				rest.push_back(m);
		}
		// odstawione: na samym spodzie, w kolejnosci
		bool parkedOk = rest.size() >= parked.size() && std::equal(parked.begin(), parked.end(), rest.begin());
		if (!parkedOk) {
			plan.park = parked;
			OverlayPlan onlyPark;
			onlyPark.park = parked;
			rest = ApplyPlan(rest, onlyPark, false);
		}
		size_t keep = want.size();
		while (keep > 0 && !PrefixStays(rest, want, keep))
			keep--;
		plan.apply.assign(want.begin() + keep, want.end());
		return plan;
	}

	OverlayPlan PlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& want,
		const std::vector<std::string>& mine) {
		return PlanOverlays(model, want, {}, mine);
	}

	// Stan ruchu: [bron](RUN|WALK|SNEAK)[L|R|BL|BR|WL], np. RUN, RUNL, 1HRUNL, MAGWALKBL, WALKWL.
	static bool ParseMoveState(const std::string& s, std::string* suffix) {
		static const char* weapons[] = { "FIST", "MAG", "1H", "2H", "BOW", "CBOW" };
		static const char* cores[] = { "RUN", "WALK", "SNEAK" };
		static const char* suffixes[] = { "", "L", "R", "BL", "BR", "WL" };
		std::string rest = s;
		for (const char* w : weapons) {
			size_t n = strlen(w);
			if (rest.compare(0, n, w) == 0) {
				rest = rest.substr(n);
				break;
			}
		}
		for (const char* c : cores) {
			size_t n = strlen(c);
			if (rest.compare(0, n, c) != 0)
				continue;
			std::string suf = rest.substr(n);
			for (const char* x : suffixes)
				if (suf == x) {
					if (suffix)
						*suffix = suf;
					return true;
				}
		}
		return false;
	}

	bool IsRestorableAni(const std::string& aniName) {
		std::string u = Upper(aniName);
		if (u.rfind("S_", 0) == 0)
			return true;
		if (u.rfind("T_", 0) != 0)
			return false;
		std::string body = u.substr(2);
		size_t at = body.find("_2_");
		return at != std::string::npos && ParseMoveState(body.substr(0, at), nullptr) &&
			ParseMoveState(body.substr(at + 3), nullptr);
	}

	bool IsMoveLoopAni(const std::string& aniName) {
		std::string u = Upper(aniName);
		std::string suffix;
		return u.rfind("S_", 0) == 0 && ParseMoveState(u.substr(2), &suffix) && !suffix.empty();
	}

	bool IsBlockingAni(const std::string& aniName, int layer) {
		std::string u = Upper(aniName);
		return layer >= 1 && layer <= 2 && u.rfind("T_", 0) == 0 && !IsRestorableAni(u);
	}
}
