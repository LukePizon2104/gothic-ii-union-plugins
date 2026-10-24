#include "Text.h"
#include <cctype>
#include <cstdlib>

namespace trcore {
	std::string Upper(std::string s) {
		for (char& c : s)
			c = (char)std::toupper((unsigned char)c);
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
