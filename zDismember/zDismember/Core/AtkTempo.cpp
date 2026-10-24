#include "AtkTempo.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace zdcore {
	static const char* SLOT_ANI[ATK_SLOT_COUNT] = { "T_1HATTACKL", "T_1HATTACKR", "S_1HATTACK", "T_2HATTACKL", "T_2HATTACKR", "S_2HATTACK" };
	static const char* SLOT_INI[ATK_SLOT_COUNT] = { "W3AtkSpeed1HLeft", "W3AtkSpeed1HRight", "W3AtkSpeed1HCombo",
		"W3AtkSpeed2HLeft", "W3AtkSpeed2HRight", "W3AtkSpeed2HCombo" };
	static const char* SLOT_MENU[ATK_SLOT_COUNT] = { "MenuW3AtkSpeed1HLeft", "MenuW3AtkSpeed1HRight", "MenuW3AtkSpeed1HCombo",
		"MenuW3AtkSpeed2HLeft", "MenuW3AtkSpeed2HRight", "MenuW3AtkSpeed2HCombo" };

	const char* AtkSlotAni(int slot) {
		return slot >= 0 && slot < ATK_SLOT_COUNT ? SLOT_ANI[slot] : "";
	}

	const char* AtkSlotIniKey(int slot) {
		return slot >= 0 && slot < ATK_SLOT_COUNT ? SLOT_INI[slot] : "";
	}

	const char* AtkSlotMenuKey(int slot) {
		return slot >= 0 && slot < ATK_SLOT_COUNT ? SLOT_MENU[slot] : "";
	}

	int AtkSlotOfAni(const std::string& aniName) {
		for (int i = 0; i < ATK_SLOT_COUNT; i++)
			if (_stricmp(aniName.c_str(), SLOT_ANI[i]) == 0)
				return i;
		return -1;
	}

	bool AtkSlotIs2H(int slot) {
		return slot >= ATK_2H_LEFT && slot < ATK_SLOT_COUNT;
	}

	bool AtkSlotIsCombo(int slot) {
		return slot == ATK_1H_COMBO || slot == ATK_2H_COMBO;
	}

	int ClampAtkSpeed(int percent) {
		if (percent == ATK_SPEED_NATIVE)
			return ATK_SPEED_NATIVE;
		return percent < ATK_SPEED_MIN ? ATK_SPEED_MIN : percent > ATK_SPEED_MAX ? ATK_SPEED_MAX : percent;
	}

	const std::vector<int>& AtkSpeedMenuValues() {
		static const std::vector<int> v = { ATK_SPEED_NATIVE, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 175, 200 };
		return v;
	}

	std::vector<int> ParseEventInts(const std::string& s) {
		std::vector<int> out;
		size_t i = 0;
		while (i < s.size()) {
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == ',' || s[i] == '\r' || s[i] == '\n'))
				i++;
			size_t j = i;
			while (j < s.size() && !(s[j] == ' ' || s[j] == '\t' || s[j] == ',' || s[j] == '\r' || s[j] == '\n'))
				j++;
			if (j > i) {
				std::string tok = s.substr(i, j - i);
				bool num = true;
				for (size_t k = 0; k < tok.size(); k++)
					if (!(isdigit((unsigned char)tok[k]) || (k == 0 && tok[k] == '-' && tok.size() > 1)))
						num = false;
				if (num)
					out.push_back(atoi(tok.c_str()));
			}
			i = j;
		}
		return out;
	}

	void AtkEventsAddTag(AtkEvents& e, const std::string& tag, const std::string& values) {
		if (_stricmp(tag.c_str(), "DEF_OPT_FRAME") == 0)
			e.opt = ParseEventInts(values);
		else if (_stricmp(tag.c_str(), "DEF_HIT_END") == 0)
			e.end = ParseEventInts(values);
		else if (_stricmp(tag.c_str(), "DEF_WINDOW") == 0)
			e.win = ParseEventInts(values);
	}

	static float Fps(const AtkEvents& e) {
		return e.fps > 0.01f ? e.fps : 25.0f;
	}

	float AtkFirstHitSeconds(const AtkEvents& e) {
		return e.opt.empty() ? -1.0f : (float)e.opt[0] / Fps(e);
	}

	std::vector<int> AtkComboGaps(const AtkEvents& e) {
		std::vector<int> g;
		for (size_t i = 0; i + 1 < e.opt.size() && 2 * i + 1 < e.win.size(); i++)
			g.push_back((e.win[2 * i] - e.opt[i]) + (e.opt[i + 1] - (e.win[2 * i + 1] - 1)));
		return g;
	}

	float AtkPaceSeconds(const AtkEvents& e, bool combo) {
		if (combo) {
			std::vector<int> g = AtkComboGaps(e);
			if (!g.empty()) {
				double sum = 0.0;
				for (int x : g)
					sum += x;
				float avg = (float)(sum / g.size());
				return avg > 0.0f ? avg / Fps(e) : -1.0f;
			}
		}
		if (!e.win.empty() && e.win[0] > 0)
			return (float)e.win[0] / Fps(e);
		if (!e.end.empty() && e.end[0] > 0)
			return (float)e.end[0] / Fps(e);
		return e.frames > 0 ? (float)e.frames / Fps(e) : -1.0f;
	}

	float AtkTempoScale(int percent, float oursPace, float nativePace) {
		percent = ClampAtkSpeed(percent);
		if (percent != ATK_SPEED_NATIVE)
			return percent / 100.0f;
		if (!(oursPace > 0.0f) || !(nativePace > 0.0f))
			return 1.0f;
		float s = oursPace / nativePace;
		return s < ATK_MATCH_MIN ? ATK_MATCH_MIN : s > ATK_MATCH_MAX ? ATK_MATCH_MAX : s;
	}

	bool AtkFpsTarget(float base, float scale, float current, float& target) {
		if (!(base > 0.0f) || !(scale > 0.0f)) {
			target = current;
			return false;
		}
		target = base * scale;
		return fabsf(current - target) > 1e-4f;
	}

	int ClampSide2H(int v) {
		return v < 0 || v >= SIDE2H_COUNT ? SIDE2H_W3 : v;
	}

	int Side2HEffective(int wanted, bool fastAvailable) {
		wanted = ClampSide2H(wanted);
		return wanted == SIDE2H_FAST && !fastAvailable ? SIDE2H_W3 : wanted;
	}

	const char* Side2HLabel(int v) {
		switch (ClampSide2H(v)) {
		case SIDE2H_FAST: return "W3 szybkie (miecz dwureczny NPC)";
		case SIDE2H_NATIVE: return "natywne (talent / zEBA)";
		default: return "W3 (mocne ciecia Geralta)";
		}
	}

	bool SideEntryObserve(SideEntry& s, const void* cur) {
		if (s.known && (cur == s.orig || (s.changed && cur == s.placed)))
			return false;
		s.orig = cur;
		s.placed = cur;
		s.known = true;
		s.changed = false;
		return true;
	}

	const void* SideEntryWant(const SideEntry& s, int side2h, const void* fast) {
		switch (ClampSide2H(side2h)) {
		case SIDE2H_FAST: return fast ? fast : s.orig;
		case SIDE2H_NATIVE: return nullptr;
		default: return s.orig;
		}
	}

	void SideEntryWrote(SideEntry& s, const void* want) {
		s.placed = want;
		s.changed = want != s.orig;
	}
}
