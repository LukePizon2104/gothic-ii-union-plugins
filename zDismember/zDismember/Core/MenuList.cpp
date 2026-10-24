#include "MenuList.h"
#include <algorithm>

namespace zdcore {
	MenuKey MenuKeyFromDik(int dik) {
		switch (dik) {
		case 0xC8: case 0x11: case 0x48: return MKEY_UP;      // strzalka, W, num 8
		case 0xD0: case 0x1F: case 0x50: return MKEY_DOWN;    // strzalka, S, num 2
		case 0xCB: case 0x1E: case 0x4B: return MKEY_LEFT;    // strzalka, A, num 4
		case 0xCD: case 0x20: case 0x4D: return MKEY_RIGHT;   // strzalka, D, num 6
		case 0x1C: case 0x9C: return MKEY_ENTER;              // Enter, Enter numeryczny
		case 0x01: case 0x0E: return MKEY_ESC;                // Esc, Backspace
		case 0xC7: return MKEY_HOME;
		case 0xCF: return MKEY_END;
		case 0xC9: return MKEY_PGUP;
		case 0xD1: return MKEY_PGDN;
		case 0x10: return MKEY_TAB_PREV;                      // Q
		case 0x12: case 0x0F: return MKEY_TAB_NEXT;           // E, Tab
		default: return MKEY_NONE;
		}
	}

	const char* MenuKeyName(MenuKey k) {
		switch (k) {
		case MKEY_UP: return "gora";
		case MKEY_DOWN: return "dol";
		case MKEY_LEFT: return "lewo";
		case MKEY_RIGHT: return "prawo";
		case MKEY_ENTER: return "Enter";
		case MKEY_ESC: return "Esc";
		case MKEY_HOME: return "Home";
		case MKEY_END: return "End";
		case MKEY_PGUP: return "PgUp";
		case MKEY_PGDN: return "PgDn";
		case MKEY_TAB_PREV: return "zakladka w lewo";
		case MKEY_TAB_NEXT: return "zakladka w prawo";
		default: return "-";
		}
	}

	bool MenuRowSelectable(const MenuRow& r) {
		return r.visible && r.enabled && r.kind != MROW_TEXT;
	}

	int MenuFindRow(const MenuList& m, int id) {
		for (size_t i = 0; i < m.rows.size(); i++)
			if (m.rows[i].id == id)
				return (int)i;
		return -1;
	}

	MenuRow* MenuRowById(MenuList& m, int id) {
		int i = MenuFindRow(m, id);
		return i >= 0 ? &m.rows[i] : nullptr;
	}

	void MenuFixCursor(MenuList& m) {
		int n = (int)m.rows.size();
		if (n == 0) {
			m.cursor = -1;
			return;
		}
		int c = std::clamp(m.cursor, 0, n - 1);
		if (MenuRowSelectable(m.rows[c])) {
			m.cursor = c;
			return;
		}
		for (int i = c + 1; i < n; i++)
			if (MenuRowSelectable(m.rows[i])) {
				m.cursor = i;
				return;
			}
		for (int i = c - 1; i >= 0; i--)
			if (MenuRowSelectable(m.rows[i])) {
				m.cursor = i;
				return;
			}
		m.cursor = -1;
	}

	bool MenuSelectRow(MenuList& m, int id) {
		int i = MenuFindRow(m, id);
		if (i < 0 || !MenuRowSelectable(m.rows[i]))
			return false;
		m.cursor = i;
		return true;
	}

	// Nastepny wybieralny w kierunku dir (w kolko); -1 gdy zaden.
	static int StepCursor(const MenuList& m, int from, int dir) {
		int n = (int)m.rows.size();
		if (n == 0)
			return -1;
		int i = from;
		for (int k = 0; k < n; k++) {
			i = (i + dir + n) % n;
			if (MenuRowSelectable(m.rows[i]))
				return i;
		}
		return from >= 0 && from < n && MenuRowSelectable(m.rows[from]) ? from : -1;
	}

	// Pierwszy / ostatni wybieralny.
	static int EdgeCursor(const MenuList& m, bool last) {
		int n = (int)m.rows.size();
		for (int k = 0; k < n; k++) {
			int i = last ? n - 1 - k : k;
			if (MenuRowSelectable(m.rows[i]))
				return i;
		}
		return -1;
	}

	MenuEvent MenuHandleKey(MenuList& m, MenuKey k) {
		MenuEvent e;
		if (k == MKEY_ESC) {
			e.type = MEV_CLOSE;
			return e;
		}
		if (k == MKEY_TAB_PREV || k == MKEY_TAB_NEXT) {
			int n = (int)m.tabs.size();
			if (n < 2)
				return e;
			m.tab = ((std::clamp(m.tab, 0, n - 1) + (k == MKEY_TAB_NEXT ? 1 : -1)) % n + n) % n;
			e.type = MEV_TAB;
			e.value = m.tab;
			return e;
		}
		MenuFixCursor(m);
		if (m.cursor < 0)
			return e;
		int before = m.cursor;
		switch (k) {
		case MKEY_UP:
			m.cursor = StepCursor(m, m.cursor, -1);
			break;
		case MKEY_DOWN:
			m.cursor = StepCursor(m, m.cursor, +1);
			break;
		case MKEY_HOME:
		case MKEY_PGUP:
			m.cursor = EdgeCursor(m, false);
			break;
		case MKEY_END:
		case MKEY_PGDN:
			m.cursor = EdgeCursor(m, true);
			break;
		case MKEY_LEFT:
		case MKEY_RIGHT: {
			MenuRow& r = m.rows[m.cursor];
			int n = (int)r.values.size();
			if (r.kind != MROW_CHOICE || n < 2)
				return e;
			int v = r.value + (k == MKEY_RIGHT ? 1 : -1);
			if (r.wrap)
				v = (v + n) % n;
			else
				v = std::clamp(v, 0, n - 1);
			if (v == r.value)
				return e;
			r.value = v;
			e.type = MEV_CHANGED;
			e.rowId = r.id;
			e.value = v;
			return e;
		}
		case MKEY_ENTER: {
			MenuRow& r = m.rows[m.cursor];
			if (r.kind == MROW_ACTION || (r.kind == MROW_CHOICE && r.activates)) {
				e.type = MEV_ACTIVATE;
				e.rowId = r.id;
				e.value = r.value;
			}
			return e;
		}
		default:
			return e;
		}
		if (m.cursor < 0)
			m.cursor = before;
		if (m.cursor != before) {
			e.type = MEV_MOVED;
			e.rowId = m.rows[m.cursor].id;
			e.value = m.rows[m.cursor].value;
		}
		return e;
	}

	std::string MenuValueText(const MenuRow& r) {
		if (r.kind != MROW_CHOICE || r.values.empty())
			return "";
		int n = (int)r.values.size();
		int v = std::clamp(r.value, 0, n - 1);
		bool left = n > 1 && (r.wrap || v > 0), right = n > 1 && (r.wrap || v < n - 1);
		return std::string(left ? "< " : "  ") + r.values[v] + (right ? " >" : "  ");
	}

	std::vector<int> MenuVisibleRows(MenuList& m, int maxRows) {
		std::vector<int> vis;
		for (size_t i = 0; i < m.rows.size(); i++)
			if (m.rows[i].visible)
				vis.push_back((int)i);
		std::vector<int> out;
		if (vis.empty() || maxRows <= 0)
			return out;
		// pozycja kursora na liscie widocznych
		int cur = -1;
		for (size_t i = 0; i < vis.size(); i++)
			if (vis[i] == m.cursor)
				cur = (int)i;
		int count = (int)vis.size();
		int first = std::clamp(m.first, 0, std::max(0, count - maxRows));
		if (cur >= 0) {
			if (cur < first)
				first = cur;
			if (cur >= first + maxRows)
				first = cur - maxRows + 1;
		}
		m.first = first;
		for (int i = first; i < count && (int)out.size() < maxRows; i++)
			out.push_back(vis[i]);
		return out;
	}

	int MenuHitRowAt(const MenuHitMap& h, int x, int y) {
		if (!h.valid || x < h.x0 || x > h.x1)
			return -1;
		for (size_t i = 0; i < h.rows.size(); i++)
			if (y >= h.rows[i].y0 && y < h.rows[i].y1)
				return (int)i;
		return -1;
	}

	int MenuHitTabAt(const MenuHitMap& h, int x, int y) {
		if (!h.valid)
			return -1;
		for (const MenuHitTab& t : h.tabs)
			if (x >= t.x0 && x < t.x1 && y >= t.y0 && y < t.y1)
				return t.tab;
		return -1;
	}

	MenuMouseResult MenuMouse(MenuList& m, const MenuHitMap& h, MenuMouseKind k, int x, int y) {
		MenuMouseResult r;
		r.inside = x >= 0 && x <= 8192 && y >= 0 && y <= 8192;
		if (k == MMOUSE_RIGHT) {
			r.back = true;
			return r;
		}
		int hi = MenuHitRowAt(h, x, y);
		int row = hi >= 0 ? h.rows[hi].row : -1;
		bool sel = row >= 0 && row < (int)m.rows.size() && MenuRowSelectable(m.rows[row]);
		if (k == MMOUSE_MOVE) {
			if (sel && row != m.cursor) {
				m.cursor = row;
				r.ev.type = MEV_MOVED;
				r.ev.rowId = m.rows[row].id;
				r.ev.value = m.rows[row].value;
			}
			return r;
		}
		if (k == MMOUSE_LEFT) {
			int tab = MenuHitTabAt(h, x, y);
			if (tab >= 0 && tab < (int)m.tabs.size()) {
				if (tab != m.tab) {
					m.tab = tab;
					r.ev.type = MEV_TAB;
					r.ev.value = tab;
				}
				return r;
			}
			if (!sel)
				return r;
			m.cursor = row;
			const MenuRow& mr = m.rows[row];
			const MenuHitRow& hr = h.rows[hi];
			bool hasVal = mr.kind == MROW_CHOICE && hr.valX1 > hr.valX0;
			if (hasVal && x >= hr.valX0 && x < hr.valX0 + hr.arrowW)
				r.ev = MenuHandleKey(m, MKEY_LEFT);
			else if (hasVal && x > hr.valX1 - hr.arrowW && x <= hr.valX1)
				r.ev = MenuHandleKey(m, MKEY_RIGHT);
			else if (mr.kind == MROW_ACTION || (mr.kind == MROW_CHOICE && mr.activates))
				r.ev = MenuHandleKey(m, MKEY_ENTER);
			else if (mr.kind == MROW_CHOICE)
				r.ev = MenuHandleKey(m, MKEY_RIGHT);
			return r;
		}
		// kolko
		bool up = k == MMOUSE_WHEEL_UP;
		if (sel && m.rows[row].kind == MROW_CHOICE && h.rows[hi].valX1 > h.rows[hi].valX0 && x >= h.rows[hi].valX0 - h.rows[hi].arrowW) {
			m.cursor = row;
			r.ev = MenuHandleKey(m, up ? MKEY_RIGHT : MKEY_LEFT);
			return r;
		}
		r.ev = MenuHandleKey(m, up ? MKEY_UP : MKEY_DOWN);
		return r;
	}

	bool MenuKeyRepeats(MenuKey k) {
		return k == MKEY_UP || k == MKEY_DOWN || k == MKEY_LEFT || k == MKEY_RIGHT || k == MKEY_PGUP || k == MKEY_PGDN;
	}

	void MenuRepeatPress(MenuRepeat& r, MenuKey k) {
		if (!MenuKeyRepeats(k))
			return;
		r.key = k;
		r.held = 0.0f;
		r.next = 0.0f;
	}

	void MenuRepeatRelease(MenuRepeat& r, MenuKey k) {
		if (r.key == k)
			r = MenuRepeat();
	}

	int MenuRepeatStep(MenuRepeat& r, float dt, float delay, float rate) {
		if (r.key == MKEY_NONE || dt <= 0.0f)
			return 0;
		r.held += dt;
		if (r.held < delay)
			return 0;
		if (r.next <= 0.0f)
			r.next = delay;
		int n = 0;
		float step = rate > 0.01f ? rate : 0.01f;
		while (r.held >= r.next && n < 8) {
			r.next += step;
			n++;
		}
		if (r.held >= r.next)
			r.next = r.held + step;   // dluga przycinka - bez serii powtorzen
		return n;
	}
}
