#include "Wheel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace zdcore {
	// ---------- klawisz ----------
	bool WheelKeyPress(WheelKey& k, double nowMs) {
		if (k.state != WK_IDLE)
			return false;
		k.state = WK_ARMED;
		k.pressMs = nowMs;
		return true;
	}

	WheelKeyAct WheelKeyStep(WheelKey& k, double nowMs, bool down, bool focus, int holdMs) {
		switch (k.state) {
		case WK_ARMED:
			if (!focus) {
				k.state = WK_IDLE;
				return WKA_LOST;
			}
			if (!down) {
				k.state = WK_IDLE;
				return WKA_TAP;
			}
			if (nowMs - k.pressMs >= (double)std::max(0, holdMs)) {
				k.state = WK_OPEN;
				return WKA_OPEN;
			}
			return WKA_NONE;
		case WK_OPEN:
			if (!focus) {
				k.state = WK_IDLE;
				return WKA_LOST;
			}
			if (!down) {
				k.state = WK_IDLE;
				return WKA_CONFIRM;
			}
			return WKA_NONE;
		case WK_WAIT_RELEASE:
			if (!down || !focus)
				k.state = WK_IDLE;
			return WKA_NONE;
		default:
			return WKA_NONE;
		}
	}

	void WheelKeyClosed(WheelKey& k, bool down) {
		k.state = down ? WK_WAIT_RELEASE : WK_IDLE;
	}

	const char* WheelKeyStateName(int s) {
		static const char* const N[] = { "IDLE", "ARMED", "OPEN", "WAIT_RELEASE" };
		return s >= 0 && s <= WK_WAIT_RELEASE ? N[s] : "?";
	}

	// ---------- uklad ----------
	std::vector<int> WheelSpellOrder(const std::vector<int>& keyNo) {
		std::vector<int> idx(keyNo.size());
		for (size_t i = 0; i < idx.size(); i++)
			idx[i] = (int)i;
		auto key = [&](int i) { return (keyNo[i] > 0 ? keyNo[i] : 1000) * 1024 + i; };
		std::stable_sort(idx.begin(), idx.end(), [&](int a, int b) { return key(a) < key(b); });
		return idx;
	}

	int WheelSpellsPerPage(bool xbow) {
		return xbow ? WHEEL_SLOTS - 1 : WHEEL_SLOTS;
	}

	int WheelPageCount(int nSpells, bool xbow) {
		int per = WheelSpellsPerPage(xbow);
		return std::max(1, (std::max(0, nSpells) + per - 1) / per);
	}

	WheelEntry WheelSlotEntry(const std::vector<int>& order, bool xbow, int page, int slot) {
		WheelEntry e;
		if (slot < 0 || slot >= WHEEL_SLOTS || page < 0)
			return e;
		if (xbow && slot == WHEEL_XBOW_SLOT) {
			e.kind = WE_XBOW;
			return e;
		}
		int j = xbow && slot > WHEEL_XBOW_SLOT ? slot - 1 : slot;
		int at = page * WheelSpellsPerPage(xbow) + j;
		if (at < (int)order.size()) {
			e.kind = WE_SPELL;
			e.spell = order[at];
		}
		return e;
	}

	bool WheelFindActive(const std::vector<int>& order, bool xbow, bool xbowSelected, int activeSpell, int& page, int& slot) {
		page = 0;
		slot = -1;
		if (xbow && xbowSelected) {
			slot = WHEEL_XBOW_SLOT;
			return true;
		}
		for (size_t p = 0; p < order.size(); p++) {
			if (order[p] != activeSpell)
				continue;
			int per = WheelSpellsPerPage(xbow);
			page = (int)p / per;
			int j = (int)p % per;
			slot = xbow && j >= WHEEL_XBOW_SLOT ? j + 1 : j;
			return true;
		}
		return false;
	}

	void WheelSlotCenter(int slot, float r, float& x, float& y) {
		float a = (float)slot * 0.785398163f;   // 45 st.
		x = r * sinf(a);
		y = -r * cosf(a);
	}

	// ---------- mysz ----------
	int WheelSlotFromVec(float x, float y, float deadZone) {
		if (x * x + y * y <= deadZone * deadZone)
			return -1;
		float a = atan2f(x, -y);   // 0 = gora, rosnie zgodnie z zegarem
		if (a < 0.0f)
			a += 6.28318531f;
		int s = (int)floorf(a / 0.785398163f + 0.5f);
		return s % WHEEL_SLOTS;
	}

	bool WheelMouseMove(WheelMouse& m, float dx, float dy, float clampR, float deadZone) {
		m.x += dx;
		m.y += dy;
		float len = sqrtf(m.x * m.x + m.y * m.y);
		if (clampR > 0.0f && len > clampR) {
			m.x *= clampR / len;
			m.y *= clampR / len;
		}
		int s = WheelSlotFromVec(m.x, m.y, deadZone);
		if (s < 0 || s == m.slot)
			return false;
		m.slot = s;
		return true;
	}

	// ---------- czas ----------
	WheelTimeOut WheelTimeStep(WheelTime& t, unsigned realMs, float factor, float capMs) {
		WheelTimeOut o;
		double game = (double)realMs * (double)std::max(0.0f, factor);
		if (capMs > 0.0f && game > capMs)
			game = capMs;
		o.frameMs = (float)game;
		double acc = t.carry + game;
		double whole = std::floor(acc + 1e-9);
		if (whole < 0.0)
			whole = 0.0;
		o.frameInt = (unsigned)whole;
		t.carry = acc - whole;
		if (t.carry < 0.0)
			t.carry = 0.0;
		return o;
	}

	float WheelTempoApproach(float cur, float target, double realMs, double rampMs) {
		if (rampMs <= 0.0)
			return target;
		float step = (float)(std::max(0.0, realMs) / rampMs);
		if (cur < target)
			return std::min(target, cur + step);
		return std::max(target, cur - step);
	}

	float WheelTempoTarget(bool wheelOpen, int wheelPct, bool xbowAim, int xbowAimPct) {
		float t = 1.0f;
		if (wheelOpen)
			t = std::min(t, (float)wheelPct / 100.0f);
		if (xbowAim && xbowAimPct > 0 && xbowAimPct < 100)
			t = std::min(t, (float)xbowAimPct / 100.0f);
		return std::clamp(t, 0.01f, 1.0f);
	}

	// ---------- odmowa ----------
	WheelDeny WheelDenyCheck(const WheelSense& s) {
		if (!s.enabled)
			return WDENY_OFF;
		if (!s.inGame)
			return WDENY_NOT_IN_GAME;
		if (s.down)
			return WDENY_DOWN;
		if (!s.human)
			return WDENY_NOT_HUMAN;
		if (s.window)
			return WDENY_WINDOW;
		if (s.pluginWindow)
			return WDENY_PLUGIN_WINDOW;
		if (s.foreignWindow)
			return WDENY_FOREIGN_WINDOW;
		if (s.cutscene)
			return WDENY_CUTSCENE;
		if (s.mob)
			return WDENY_MOB;
		if (s.pluginMove)
			return WDENY_PLUGIN_MOVE;
		if (s.quickCast)
			return WDENY_QUICKCAST;
		if (s.xbowBusy)
			return WDENY_XBOW;
		if (s.rewind)
			return WDENY_REWIND;
		if (s.empty)
			return WDENY_EMPTY;
		return WDENY_OK;
	}

	const char* WheelDenyText(int d) {
		static const char* const T[WDENY_COUNT] = {
			"ok", "wylaczone (Wheel=0 / plugin)", "poza gra (menu, wczytywanie, brak bohatera)", "bohater nie jest czlowiekiem",
			"smierc / nieprzytomnosc", "okno gry", "okno zDismember (medytacja / sciezki)", "okno innego pluginu (nowy callback wejscia)",
			"przerywnik", "mob", "finisher / przeskok / unik / podnoszenie", "szybki rzut zQuickSign w toku", "strzal z kuszy w toku",
			"cofanie czasu / czar trzymany", "brak czarow i kuszy",
		};
		return d >= 0 && d < WDENY_COUNT ? T[d] : "?";
	}

	// ---------- opis ----------
	std::string WheelKindText(int kind, int amount) {
		if (kind == WSK_SIGN)
			return "Znak Wied\x9Fmina";
		if (kind == WSK_SCROLL) {
			char b[48];
			std::snprintf(b, sizeof(b), "Zw\xF3j x%d", std::max(0, amount));
			return b;
		}
		return "Runa";
	}

	std::string WheelCooldownText(int ms) {
		if (ms <= 0)
			return "";
		char b[48];
		int tenths = (ms + 99) / 100;
		std::snprintf(b, sizeof(b), "Odnowienie: %d,%d s", tenths / 10, tenths % 10);
		return b;
	}

	std::string WheelItemLine(const std::string& text, int count) {
		if (text.empty())
			return "";
		if (count == 0)
			return text;
		return text + "  " + std::to_string(count);
	}

	std::string WheelPageText(int page, int pages) {
		if (pages <= 1)
			return "";
		char b[48];
		std::snprintf(b, sizeof(b), "Strona %d/%d", page + 1, pages);
		return b;
	}

	bool WheelIsMouseCode(int code) {
		return code >= WHEEL_MOUSE_LEFT && code <= WHEEL_MOUSE_LAST;
	}

	int WheelMouseVk(int code) {
		switch (code) {
		case WHEEL_MOUSE_LEFT: return 0x01;    // VK_LBUTTON
		case WHEEL_MOUSE_RIGHT: return 0x02;   // VK_RBUTTON
		case WHEEL_MOUSE_MID: return 0x04;     // VK_MBUTTON
		case WHEEL_MOUSE_X1: return 0x05;      // VK_XBUTTON1
		case WHEEL_MOUSE_X2: return 0x06;      // VK_XBUTTON2
		default: return 0;
		}
	}

	int WheelMouseCodeFromVk(int vk) {
		for (int c = WHEEL_MOUSE_LEFT; c <= WHEEL_MOUSE_X2; c++)
			if (WheelMouseVk(c) == vk)
				return c;
		return 0;
	}

	bool WheelKeyBindable(int code, const char** why) {
		const char* w = nullptr;
		if (code <= 0 || code > WHEEL_KEY_MAX || (code > 0xFF && !WheelIsMouseCode(code)))
			w = "nieznany klawisz";
		else if (code == 0x01)
			w = "Esc anuluje";
		else if (code == WHEEL_MOUSE_LEFT)
			w = "LPM zatwierdza w kole";
		else if (code == WHEEL_MOUSE_RIGHT)
			w = "PPM anuluje w kole";
		if (why)
			*why = w;
		return w == nullptr;
	}

	std::string WheelMouseName(int code) {
		switch (code) {
		case WHEEL_MOUSE_LEFT: return "Mysz (lewy)";
		case WHEEL_MOUSE_RIGHT: return "Mysz (prawy)";
		case WHEEL_MOUSE_MID: return "Mysz (\x9C" "rodek)";
		default: break;
		}
		if (code >= WHEEL_MOUSE_X1 && code <= WHEEL_MOUSE_LAST)
			return "Mysz " + std::to_string(code - WHEEL_MOUSE_X1 + 4);
		return "";
	}

	std::string WheelShortName(const std::string& name, int maxChars) {
		if (maxChars < 4 || (int)name.size() <= maxChars)
			return name;
		std::string s = name.substr(0, (size_t)(maxChars - 3));
		while (!s.empty() && s.back() == ' ')
			s.pop_back();
		return s + "...";
	}
}
