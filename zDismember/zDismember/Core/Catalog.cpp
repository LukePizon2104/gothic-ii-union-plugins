#include "Catalog.h"
#include <algorithm>
#include <cstdlib>

namespace zdcore {
	static bool ParseFlags(const std::string& value, const char* const* names, const uint32_t* bits, int count, uint32_t& out) {
		out = 0;
		size_t pos = 0;
		while (pos <= value.size()) {
			size_t comma = value.find(',', pos);
			std::string item = Upper(Trim(value.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos)));
			bool found = false;
			for (int i = 0; i < count; i++)
				if (item == names[i]) {
					out |= bits[i];
					found = true;
				}
			if (!found)
				return false;
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
		}
		return out != 0;
	}

	// "x,z x,z ..." (pary rozdzielone spacja - srednik w ini to komentarz) -> plaska lista; false przy zlej parze.
	bool ParseTraj(const std::string& value, std::vector<float>& out) {
		out.clear();
		size_t pos = 0;
		while (pos < value.size()) {
			size_t start = value.find_first_not_of(" 	", pos);
			if (start == std::string::npos)
				break;
			size_t end = value.find_first_of(" 	", start);
			std::string item = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
			float xz[2];
			if (!ParseFloatList(item, xz, 2))
				return false;
			out.push_back(xz[0]);
			out.push_back(xz[1]);
			pos = end == std::string::npos ? value.size() : end;
		}
		return !out.empty();
	}

	void SampleTraj(const std::vector<float>& xz, float fps, float t, float& x, float& z) {
		size_t n = xz.size() / 2;
		x = z = 0.0f;
		if (n == 0)
			return;
		float f = t * fps;
		if (f <= 0.0f) {
			x = xz[0];
			z = xz[1];
			return;
		}
		if (f >= (float)(n - 1)) {
			x = xz[2 * (n - 1)];
			z = xz[2 * (n - 1) + 1];
			return;
		}
		size_t i = (size_t)f;
		float u = f - (float)i;
		x = xz[2 * i] + (xz[2 * i + 2] - xz[2 * i]) * u;
		z = xz[2 * i + 1] + (xz[2 * i + 3] - xz[2 * i + 1]) * u;
	}

	static bool Finish(FinisherDef& d, const std::vector<std::string>& seen, std::string& error) {
		auto has = [&](const char* k) {
			for (auto& s : seen)
				if (s == k)
					return true;
			return false;
		};
		for (const char* req : { "WEAPON", "VICTIM", "POSE", "OVERLAY", "HEROANI", "CUT" })
			if (!has(req)) {
				error = d.id + ": brak " + req;
				return false;
			}
		return true;
	}

	std::vector<FinisherDef> ParseCatalog(const std::string& text, std::vector<std::string>& errors) {
		static const char* groupNames[] = { "HUMAN", "ORC", "ZOMBIE", "MONSTER" };
		static const uint32_t groupBits[] = { GROUP_HUMAN, GROUP_ORC, GROUP_ZOMBIE, GROUP_MONSTER };
		static const char* poseNames[] = { "STANDING", "DAZED", "LYING" };
		static const uint32_t poseBits[] = { POSE_STANDING, POSE_DAZED, POSE_LYING };

		std::vector<FinisherDef> out;
		FinisherDef cur;
		std::vector<std::string> seen;
		bool inSection = false, bad = false;
		std::string badReason;

		auto close = [&]() {
			if (!inSection)
				return;
			std::string err;
			if (bad)
				errors.push_back(cur.id + ": " + badReason);
			else if (!Finish(cur, seen, err))
				errors.push_back(err);
			else
				out.push_back(cur);
		};

		size_t pos = text.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;   // BOM z Notatnika
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
				cur = FinisherDef();
				cur.id = Trim(line.substr(1, line.size() - 2));
				seen.clear();
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
			seen.push_back(key);

			bool ok = true;
			if (key == "WEAPON") {
				std::string v = Upper(val);
				cur.weapon = v == "1H" ? WEAPON_1H : v == "2H" ? WEAPON_2H : WEAPON_NONE;
				ok = cur.weapon != WEAPON_NONE;
			}
			else if (key == "VICTIM")
				ok = ParseFlags(val, groupNames, groupBits, 4, cur.victimGroups);
			else if (key == "POSE")
				ok = ParseFlags(val, poseNames, poseBits, 3, cur.poses);
			else if (key == "OVERLAY")
				ok = !(cur.overlay = val).empty();
			else if (key == "HEROANI")
				ok = !(cur.heroAni = Upper(val)).empty();
			else if (key == "VICTIMOVERLAY")
				cur.victimOverlay = val;
			else if (key == "VICTIMANI")
				cur.victimAni = Upper(val);
			else if (key == "INPLACE")
				cur.inplace = val;
			else if (key == "CUT")
				ok = (cur.cut = PartFromName(val)) != Part::NoPart || Upper(val) == "NONE";
			else if (key == "WOUND")
				ok = !(cur.wound = Upper(val)).empty();
			else if (key == "CUTTIME") {
				char* e = nullptr;
				cur.cutTime = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.cutTime >= 0.0f;
			}
			else if (key == "VICTIMOFFSET")
				ok = ParseFloatList(val, cur.offset, 3);
			else if (key == "INPLACEOFFSET")
				ok = ParseFloatList(val, cur.inplaceOffset, 3);
			else if (key == "SLOWMO") {
				float s[3];
				ok = ParseFloatList(val, s, 3) && s[0] > 0.0f && s[0] <= 1.0f;
				if (ok) {
					cur.slowmo = s[0];
					cur.slowFrom = s[1];
					cur.slowTo = s[2];
				}
			}
			else if (key == "TRAJFPS") {
				char* e = nullptr;
				cur.trajFps = strtof(val.c_str(), &e);
				ok = e && *e == '\0' && cur.trajFps > 0.0f;
			}
			else if (key == "HEROTRAJ")
				ok = ParseTraj(val, cur.heroTraj);
			else if (key == "VICTIMTRAJ")
				ok = ParseTraj(val, cur.victimTraj);
			else if (key == "WEIGHT") {
				char* e = nullptr;
				cur.weight = (int)strtol(val.c_str(), &e, 10);
				ok = e && *e == '\0' && cur.weight >= 0;
			}
			else {
				ok = false;
			}
			if (!ok) {
				bad = true;
				badReason = "zla wartosc " + key + "=" + val;
			}
		}
		close();
		return out;
	}

	const FinisherDef* PickFinisher(const std::vector<FinisherDef>& defs, int weapon, uint32_t victimGroup, uint32_t pose, uint32_t rnd,
		const std::vector<const FinisherDef*>* skip) {
		auto fits = [&](const FinisherDef& d) {
			if (!(d.weapon == weapon && (d.victimGroups & victimGroup) && (d.poses & pose) && d.weight > 0))
				return false;
			return !skip || std::find(skip->begin(), skip->end(), &d) == skip->end();
		};
		int total = 0;
		for (auto& d : defs)
			if (fits(d))
				total += d.weight;
		if (total <= 0)
			return nullptr;
		int r = (int)(rnd % (uint32_t)total);
		for (auto& d : defs) {
			if (!fits(d))
				continue;
			if (r < d.weight)
				return &d;
			r -= d.weight;
		}
		return nullptr;
	}

	float FinisherSlowFactor(float slowmo, int mode, bool lastFoeOnly, bool othersFighting) {
		if (slowmo >= 1.0f || mode == SLOWMO_OFF || (lastFoeOnly && othersFighting))
			return 1.0f;
		if (mode == SLOWMO_LIGHT)
			return 1.0f - (1.0f - slowmo) * 0.5f;
		return slowmo;
	}
}
