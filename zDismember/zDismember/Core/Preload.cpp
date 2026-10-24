#include "Preload.h"
#include "Text.h"
#include <algorithm>

namespace zdcore {
	std::string OverlayKey(const std::string& name) {
		std::string k = Upper(Trim(name));
		size_t n = k.size();
		if (n >= 4 && (k.compare(n - 4, 4, ".MDS") == 0 || k.compare(n - 4, 4, ".MSB") == 0))
			k = Trim(k.substr(0, n - 4));
		return k;
	}

	static void AddHero(std::vector<std::string>& out, const std::string& name) {
		std::string k = OverlayKey(name);
		if (k.empty() || k.rfind("HUMANS", 0) != 0)
			return;
		if (std::find(out.begin(), out.end(), k) == out.end())
			out.push_back(k);
	}

	std::vector<std::string> HeroOverlaysToPreload(const std::vector<std::string>& styles, const std::vector<std::string>& extra,
		const std::vector<MoveDef>& moves, const std::vector<TravDef>& trav, const std::vector<FinisherDef>& finishers) {
		std::vector<std::string> out;
		for (auto& s : styles)
			AddHero(out, s);
		for (auto& e : extra)
			AddHero(out, e);
		for (auto& m : moves) {
			AddHero(out, m.overlay);
			AddHero(out, m.overlayW3);
		}
		for (auto& t : trav)
			AddHero(out, t.overlay);
		for (auto& f : finishers) {
			AddHero(out, f.overlay);
			AddHero(out, f.inplace);
		}
		return out;
	}

	std::vector<std::string> PreloadMissing(const std::vector<std::string>& wanted, const std::vector<std::string>& held) {
		std::vector<std::string> heldKeys, out;
		for (auto& h : held)
			heldKeys.push_back(OverlayKey(h));
		for (auto& w : wanted) {
			std::string k = OverlayKey(w);
			if (!k.empty() && std::find(heldKeys.begin(), heldKeys.end(), k) == heldKeys.end() &&
				std::find(out.begin(), out.end(), k) == out.end())
				out.push_back(k);
		}
		return out;
	}
}
