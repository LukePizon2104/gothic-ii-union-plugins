#include "Text.h"
#include <cctype>
#include <cstdlib>

namespace zdcore {
	std::string Trim(const std::string& s) {
		size_t a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos)
			return "";
		size_t b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	std::string Upper(std::string s) {
		for (auto& c : s)
			c = (char)toupper((unsigned char)c);
		return s;
	}

	std::vector<std::string> SplitArgs(const std::string& line) {
		std::vector<std::string> out;
		std::string cur;
		for (char c : line) {
			if (c == ' ' || c == '\t') {
				if (!cur.empty())
					out.push_back(Upper(cur));
				cur.clear();
			}
			else
				cur += c;
		}
		if (!cur.empty())
			out.push_back(Upper(cur));
		return out;
	}

	template <typename T, typename Conv>
	static bool ParseList(const std::string& s, T* out, int n, Conv conv) {
		std::vector<T> tmp;
		size_t pos = 0;
		while (pos <= s.size()) {
			size_t comma = s.find(',', pos);
			std::string item = Trim(s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
			if (item.empty())
				return false;
			char* end = nullptr;
			T v = conv(item.c_str(), &end);
			if (!end || *end != '\0')
				return false;
			tmp.push_back(v);
			if (comma == std::string::npos)
				break;
			pos = comma + 1;
		}
		if ((int)tmp.size() != n)
			return false;
		for (int i = 0; i < n; i++)
			out[i] = tmp[i];
		return true;
	}

	bool ParseIntList(const std::string& s, int* out, int n) {
		return ParseList<int>(s, out, n, [](const char* p, char** e) { return (int)strtol(p, e, 10); });
	}

	bool ParseFloatList(const std::string& s, float* out, int n) {
		return ParseList<float>(s, out, n, [](const char* p, char** e) { return strtof(p, e); });
	}

	std::vector<std::string> SplitTokens(const std::string& s, char sep) {
		std::vector<std::string> out;
		size_t from = 0;
		while (from <= s.size()) {
			size_t to = s.find(sep, from);
			if (to == std::string::npos)
				to = s.size();
			std::string t = Upper(Trim(s.substr(from, to - from)));
			if (!t.empty())
				out.push_back(t);
			from = to + 1;
		}
		return out;
	}

	bool NameHasToken(const std::string& name, const std::vector<std::string>& upperTokens) {
		std::string u = Upper(name);
		for (auto& t : upperTokens)
			if (!t.empty() && u.find(t) != std::string::npos)
				return true;
		return false;
	}

	int MenuIndexForValue(const std::vector<int>& values, int value, bool nearest) {
		int best = -1;
		for (int i = 0; i < (int)values.size(); i++) {
			if (values[i] == value)
				return i;
			if (nearest && (best < 0 || std::abs(values[i] - value) < std::abs(values[best] - value)))
				best = i;
		}
		return best;
	}

	bool MenuChangedValue(const std::vector<int>& values, int lastIndex, int menuIndex, int& value) {
		if (menuIndex == lastIndex || menuIndex < 0 || menuIndex >= (int)values.size())
			return false;
		value = values[menuIndex];
		return true;
	}
}
