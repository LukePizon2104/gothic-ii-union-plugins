#include "AniSwap.h"
#include "Text.h"
#include "Overlays.h"
#include "Preload.h"
#include <cstring>
#include <algorithm>

namespace zdcore {
	int SwapTypeOfWeaponMode(int wm) {
		return wm == SWAP_WM_DAG || wm == SWAP_WM_1H ? SWAP_TYPE_1H : wm == SWAP_WM_2H ? SWAP_TYPE_2H : SWAP_TYPE_NONE;
	}

	int SwapTypeOfItemFlags(int flags) {
		if (flags & ((1 << 16) | (1 << 17)))                // ITM_FLAG_2HD_SWD, ITM_FLAG_2HD_AXE
			return SWAP_TYPE_2H;
		if (flags & ((1 << 13) | (1 << 14) | (1 << 15)))    // ITM_FLAG_DAG, ITM_FLAG_SWD, ITM_FLAG_AXE
			return SWAP_TYPE_1H;
		return SWAP_TYPE_NONE;
	}

	int SwapOtherType(int type) {
		return type == SWAP_TYPE_1H ? SWAP_TYPE_2H : type == SWAP_TYPE_2H ? SWAP_TYPE_1H : SWAP_TYPE_NONE;
	}

	bool SwapModeCovers(int mode, int type) {
		if (type == SWAP_TYPE_1H)
			return mode == SWAP_1H_AS_2H || mode == SWAP_BOTH;
		if (type == SWAP_TYPE_2H)
			return mode == SWAP_2H_AS_1H || mode == SWAP_BOTH;
		return false;
	}

	const char* SwapModeLabel(int mode) {
		switch (mode) {
		case SWAP_1H_AS_2H: return "1H jak 2H";
		case SWAP_2H_AS_1H: return "2H jak 1H";
		case SWAP_BOTH: return "obie";
		default: return "wyl.";
		}
	}

	static bool ListHas(const std::vector<std::string>& list, const std::string& upper) {
		return std::find(list.begin(), list.end(), upper) != list.end();
	}

	bool SwapActiveFor(const SwapSettings& s, int type, const std::string& instance) {
		if (!SwapModeCovers(s.mode, type))
			return false;
		return !s.perWeapon || (!instance.empty() && ListHas(s.weapons, Upper(instance)));
	}

	SwapTable SwapIdentity() {
		SwapTable t;
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			t.map[i] = i;
		return t;
	}

	bool SwapIsIdentity(const SwapTable& t) {
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			if (t.map[i] != i)
				return false;
		return true;
	}

	bool SwapSame(const SwapTable& a, const SwapTable& b) {
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			if (a.map[i] != b.map[i])
				return false;
		return true;
	}

	// Tryby rodzaju broni -> tryb drugiego rodzaju (sztylet gra jak 1H).
	static void MapType(SwapTable& t, int type) {
		if (type == SWAP_TYPE_1H)
			t.map[SWAP_WM_DAG] = t.map[SWAP_WM_1H] = SWAP_WM_2H;
		else if (type == SWAP_TYPE_2H)
			t.map[SWAP_WM_2H] = SWAP_WM_1H;
	}

	SwapTable SwapBuildTable(const SwapSettings& s, int curType, const std::string& curInstance) {
		SwapTable t = SwapIdentity();
		if (s.perWeapon) {
			if (SwapActiveFor(s, curType, curInstance))
				MapType(t, curType);
			return t;
		}
		for (int type : { SWAP_TYPE_1H, SWAP_TYPE_2H })
			if (SwapModeCovers(s.mode, type))
				MapType(t, type);
		return t;
	}

	int SwapMapWeaponMode(const SwapTable& t, int wm) {
		return wm >= 0 && wm < SWAP_WM_COUNT ? t.map[wm] : wm;
	}

	const std::vector<SwapArrayRange>& SwapModeArrayRanges() {
		// s_run 0x2D8 .. t_stand_2_jumpuplow 0xA08 (46 tablic), s_sneak 0xA20 .. t_stumble 0xFC0 (36); s_weapon pominiete
		static const std::vector<SwapArrayRange> r = { { 0x2D8, 0xA08 }, { 0xA20, 0xFC0 } };
		return r;
	}

	bool SwapRemapsOffset(int ofs) {
		for (auto& r : SwapModeArrayRanges())
			if (ofs >= r.begin && ofs < r.end)
				return true;
		return false;
	}

	int SwapStanceRedirect(const int native[3], const int current[3], int nextId) {
		if (nextId < 0)
			return -1;
		for (int k = 0; k < 3; k++)
			if (nextId == native[k] && current[k] >= 0 && current[k] != native[k])
				return current[k];
		return -1;
	}

	int SwapWatchStep(SwapWeaponWatch& w, const void* msg, unsigned nowMs, unsigned limitMs) {
		if (!msg) {
			if (!w.msg)
				return SWAP_WATCH_NONE;
			w = SwapWeaponWatch();
			return SWAP_WATCH_END;
		}
		if (msg != w.msg) {
			w.msg = msg;
			w.startMs = nowMs;
			w.stuck = false;
			return SWAP_WATCH_START;
		}
		if (!w.stuck && nowMs - w.startMs >= limitMs) {
			w.stuck = true;
			return SWAP_WATCH_STUCK;
		}
		return SWAP_WATCH_NONE;
	}

	std::string SwapStyleCounterpart(const std::string& overlay, int toType) {
		std::string n = OverlayKey(overlay);
		const char* to = toType == SWAP_TYPE_1H ? "_1H" : toType == SWAP_TYPE_2H ? "_2H" : nullptr;
		const char* from = toType == SWAP_TYPE_1H ? "_2H" : toType == SWAP_TYPE_2H ? "_1H" : nullptr;
		if (!to || n.empty())
			return "";
		if (n.find(to) != std::string::npos)
			return n;   // juz tego typu
		size_t p = n.find(from);
		if (p == std::string::npos)
			return "";
		n.replace(p, 3, to);
		return n;
	}

	int SwapSlotDriver(const SwapTable& t, int curType, int slotType) {
		if (curType == SWAP_TYPE_NONE)
			return slotType;
		int wm = curType == SWAP_TYPE_1H ? SWAP_WM_1H : SWAP_WM_2H;
		return SwapTypeOfWeaponMode(SwapMapWeaponMode(t, wm)) == slotType ? curType : slotType;
	}

	bool SwapRestartStep(SwapRestartWatch& w, int aniId, unsigned frame, int threshold, unsigned window) {
		if (aniId != w.aniId || frame - w.first > window) {   // inna animacja albo poza oknem - nowa seria
			w.aniId = aniId;
			w.first = frame;
			w.last = frame;
			w.count = 1;
			w.flagged = threshold <= 1;
			return w.flagged;
		}
		if (frame == w.last)
			return false;   // kolejny start w tej samej klatce (skrypt / migawka nakladek) - nie drganie
		w.last = frame;
		w.count++;
		if (!w.flagged && w.count >= threshold) {
			w.flagged = true;
			return true;
		}
		return false;
	}

	int DualState(const DualInput& in) {
		if (in.weaponType == SWAP_TYPE_NONE)
			return DUAL_NONE;   // bez broni bialej (zmienne NB zeruje RX_UNEQUIPWEAPON, ale nie ufamy samym zmiennym)
		if (in.rhVar >= 0) {
			if (in.rhVar == 0)
				return DUAL_NONE;
			return in.lhVar > 0 ? DUAL_PAIR : DUAL_RIGHT;
		}
		bool pair = false, right = false;
		for (auto& o : in.overlays) {
			std::string k = OverlayKey(o);
			pair = pair || k == "HUMANS_2X2ST3";
			right = right || k == "HUMANS_1X2ST3";
		}
		if (pair)
			return DUAL_PAIR;
		std::string w = Upper(in.weapon);
		bool rightName = w.size() > 6 && w.compare(w.size() - 6, 6, "_RIGHT") == 0;
		return right && rightName ? DUAL_RIGHT : DUAL_NONE;
	}

	const char* DualLabel(int state) {
		return state == DUAL_PAIR ? "para (2x2)" : state == DUAL_RIGHT ? "prawa (1x2)" : "-";
	}

	std::vector<int> SwapCollidingModes(const SwapTable& t, int wm) {
		std::vector<int> out;
		if (wm <= 0 || wm >= SWAP_WM_COUNT)
			return out;
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			if (i != wm && t.map[i] == t.map[wm])
				out.push_back(i);
		return out;
	}

	void SwapRemapModeArray(int* arr, const SwapTable& t) {
		int src[SWAP_WM_COUNT];
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			src[i] = arr[i];
		for (int i = 0; i < SWAP_WM_COUNT; i++)
			arr[i] = src[t.map[i] >= 0 && t.map[i] < SWAP_WM_COUNT ? t.map[i] : i];
	}

	std::string SwapTableLabel(const SwapTable& t) {
		std::string s;
		if (t.map[SWAP_WM_1H] == SWAP_WM_2H)
			s += "1H->2H";
		if (t.map[SWAP_WM_2H] == SWAP_WM_1H)
			s += std::string(s.empty() ? "" : " ") + "2H->1H";
		return s.empty() ? "brak" : s;
	}

	static int ValidLast(int last) {
		return last > SWAP_OFF && last < SWAP_MODE_COUNT ? last : SWAP_BOTH;
	}

	SwapToggleResult SwapToggle(const SwapSettings& s, int curType, const std::string& curInstance) {
		SwapToggleResult r;
		r.settings = s;
		SwapSettings& n = r.settings;
		n.last = ValidLast(n.last);
		if (!n.perWeapon || curType == SWAP_TYPE_NONE || curInstance.empty()) {
			if (n.mode != SWAP_OFF) {
				n.last = ValidLast(n.mode);
				n.mode = SWAP_OFF;
			}
			else
				n.mode = n.last;
			r.active = SwapActiveFor(n, curType, curInstance);
			return r;
		}
		std::string inst = Upper(curInstance);
		auto it = std::find(n.weapons.begin(), n.weapons.end(), inst);
		r.perWeaponChange = true;
		if (SwapActiveFor(n, curType, inst)) {
			n.weapons.erase(it);
			r.active = false;
			return r;
		}
		if (it == n.weapons.end())
			n.weapons.push_back(inst);
		if (!SwapModeCovers(n.mode, curType)) {
			n.mode = SwapModeCovers(n.last, curType) ? n.last : SWAP_BOTH;
			n.last = n.mode;
		}
		r.active = SwapActiveFor(n, curType, inst);
		return r;
	}

	std::string SwapJoinWeapons(const std::vector<std::string>& weapons) {
		std::string s;
		for (auto& w : weapons)
			s += (s.empty() ? "" : ",") + w;
		return s;
	}

	int SwapKeyMenuIndex(int key, int mods) {
		if (key <= 0)
			return 1;
		return key == SWAP_KEY_DEFAULT && mods == SWAP_MODS_DEFAULT ? 0 : 2;
	}

	void SwapKeyMenuApply(int index, int& key, int& mods) {
		if (index == 0) {
			key = SWAP_KEY_DEFAULT;
			mods = SWAP_MODS_DEFAULT;
		}
		else if (index == 1)
			key = 0;
	}

	std::string SwapKeyLabel(int key, int mods) {
		if (key <= 0)
			return "-";
		std::string s;
		if (mods & SWAP_MOD_CTRL)
			s += "Ctrl+";
		if (mods & SWAP_MOD_SHIFT)
			s += "Shift+";
		if (mods & SWAP_MOD_ALT)
			s += "Alt+";
		return s + KeyLabel(key);
	}

	// Dlugosc przedrostka broni na poczatku czlonu (FIST, 1H, 2H, BOW, CBOW, MAG; 1HS / 2HS / DAG z GetWeaponHitString).
	static size_t WeaponPrefix(const std::string& seg) {
		static const char* tokens[] = { "FIST", "CBOW", "BOW", "MAG", "1H", "2H" };
		for (const char* t : tokens) {
			size_t n = strlen(t);
			if (seg.compare(0, n, t) == 0)
				return n;
		}
		return 0;
	}

	bool SwapIsDrawAni(const std::string& aniName) {
		std::string n = Upper(aniName);
		if (n.compare(0, 2, "T_") != 0)
			return false;
		size_t mid = n.find("_2_", 2);
		if (mid == std::string::npos)
			return false;
		std::string l = n.substr(2, mid - 2), r = n.substr(mid + 3);
		size_t wl = WeaponPrefix(l), wr = WeaponPrefix(r);
		if ((wl == 0) != (wr == 0))
			return true;   // miedzy stanem bez broni i z bronia
		return wl && wr && (l.size() == wl || r.size() == wr);   // sama poza broni (T_1H_2_1HRUN, T_2H_2_1H)
	}

	std::string SwapAniName(const std::string& aniName, int fromType, int toType) {
		const char* from = fromType == SWAP_TYPE_1H ? "1H" : fromType == SWAP_TYPE_2H ? "2H" : nullptr;
		const char* to = toType == SWAP_TYPE_1H ? "1H" : toType == SWAP_TYPE_2H ? "2H" : nullptr;
		std::string n = Upper(aniName);
		if (!from || !to || (n.compare(0, 2, "S_") != 0 && n.compare(0, 2, "T_") != 0))
			return "";
		bool changed = false;
		auto at = [&](size_t pos) {
			if (n.compare(pos, 2, from) == 0) {
				n.replace(pos, 2, to);
				changed = true;
			}
		};
		at(2);
		for (size_t p = n.find("_2_", 2); p != std::string::npos; p = n.find("_2_", p + 3))
			at(p + 3);
		return changed ? n : "";
	}

	std::string SwapTalentOverlay(int type, int level) {
		if (level <= 0 || (type != SWAP_TYPE_1H && type != SWAP_TYPE_2H))
			return "";
		return std::string("HUMANS_") + (type == SWAP_TYPE_1H ? "1H" : "2H") + "ST" + std::to_string(level);
	}

	bool SwapParseTalentOverlay(const std::string& name, int& type, int& level) {
		std::string n = OverlayKey(name);
		if (n.size() != 12 || n.compare(0, 7, "HUMANS_") != 0 || n.compare(9, 2, "ST") != 0)
			return false;
		int t = n.compare(7, 2, "1H") == 0 ? SWAP_TYPE_1H : n.compare(7, 2, "2H") == 0 ? SWAP_TYPE_2H : SWAP_TYPE_NONE;
		int l = n[11] - '0';
		if (t == SWAP_TYPE_NONE || l < 1 || l > 3)
			return false;
		type = t;
		level = l;
		return true;
	}

	static bool TalentOfType(const std::string& name, int type) {
		int t = 0, l = 0;
		return SwapParseTalentOverlay(name, t, l) && t == type;
	}

	static bool IsTalent(const std::string& name) {
		int t = 0, l = 0;
		return SwapParseTalentOverlay(name, t, l);
	}

	std::vector<std::string> SwapWantedOverlays(const std::vector<std::string>& native, bool active, int levelOption,
		int curType, int skill1H, int skill2H) {
		std::vector<std::string> out;
		for (auto& n : native)
			out.push_back(OverlayKey(n));
		int other = SwapOtherType(curType);
		if (!active || levelOption != 0 || other == SWAP_TYPE_NONE)
			return out;
		std::string want = SwapTalentOverlay(other, curType == SWAP_TYPE_1H ? skill1H : skill2H);
		std::vector<std::string> res;
		bool placed = false;
		for (auto& n : out) {
			if (!TalentOfType(n, other)) {
				res.push_back(n);
				continue;
			}
			if (!placed && !want.empty())
				res.push_back(want);   // w miejscu pierwszej natywnej tego rodzaju
			placed = true;
		}
		if (!placed && !want.empty())
			res.push_back(want);
		return res;
	}

	static int IndexOf(const std::vector<std::string>& v, const std::string& s) {
		auto it = std::find(v.begin(), v.end(), s);
		return it == v.end() ? -1 : (int)(it - v.begin());
	}

	SwapOverlayPlan SwapPlanOverlays(const std::vector<std::string>& model, const std::vector<std::string>& native,
		const std::vector<std::string>& wanted) {
		SwapOverlayPlan p;
		std::vector<std::string> nat, want;
		for (auto& n : native)
			nat.push_back(OverlayKey(n));
		for (auto& w : wanted)
			want.push_back(OverlayKey(w));
		for (auto& m : model)
			if (IsTalent(m) && IndexOf(want, m) < 0)
				p.remove.push_back(m);
		for (auto& w : want)
			if (IsTalent(w) && IndexOf(model, w) < 0 && IndexOf(p.add, w) < 0)
				p.add.push_back(w);
		std::vector<std::string> pending = p.add;
		for (auto& m : model) {
			if (IndexOf(p.remove, m) < 0) {
				p.order.push_back(m);
				continue;
			}
			int type = 0, level = 0;
			SwapParseTalentOverlay(m, type, level);
			for (size_t i = 0; i < pending.size(); i++)
				if (TalentOfType(pending[i], type)) {
					p.order.push_back(pending[i]);   // w miejscu zdjetej tego rodzaju
					pending.erase(pending.begin() + i);
					break;
				}
		}
		for (auto& a : pending) {
			size_t pos = 0;
			int ni = IndexOf(nat, a);
			for (int j = ni - 1; j >= 0; j--) {
				int at = IndexOf(p.order, nat[j]);
				if (at >= 0) {
					pos = (size_t)at + 1;
					break;
				}
			}
			p.order.insert(p.order.begin() + pos, a);
		}
		return p;
	}

	std::string SwapTalentSignature(const std::vector<std::string>& overlays) {
		std::vector<std::string> t;
		for (auto& o : overlays)
			if (IsTalent(o))
				t.push_back(OverlayKey(o));
		std::sort(t.begin(), t.end());
		std::string s;
		for (auto& x : t)
			s += (s.empty() ? "" : ",") + x;
		return s;
	}

	const char* SwapDeferReason(const SwapBusy& b, unsigned framesWaited) {
		if (b.director)
			return "finisher";
		if (b.moves)
			return "unik / przewrot";
		if (b.traversal)
			return "przeskok";
		if (b.attacking)
			return "cios";
		if (b.parade)
			return "parada";
		if (b.weaponMsg)
			return "dobycie / schowanie broni";
		if (b.body)
			return "stan ciala (skok, upadek, woda, interakcja)";
		if (b.soft && framesWaited < SWAP_SOFT_WAIT_FRAMES)
			return "ruch / animacja w toku";
		return nullptr;
	}
}
