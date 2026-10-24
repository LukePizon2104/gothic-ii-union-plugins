// Logika bez silnika: relacje i stan walki postaci (opis w Combat.h).
#include "Combat.h"
#include <cctype>

namespace trcore {
	bool CombatSnap::operator==(const CombatSnap& o) const {
		if (attitude != o.attitude || tmpAttitude != o.tmpAttitude || npcType != o.npcType || state != o.state ||
			enemy != o.enemy || enemyInst != o.enemyInst || nAiv != o.nAiv)
			return false;
		for (int i = 0; i < nAiv && i < COMBAT_AIV_MAX; i++)
			if (aiv[i] != o.aiv[i])
				return false;
		return true;
	}

	CombatPlan DecideCombat(const CombatSnap& target, const CombatSnap& now, bool targetHostile, bool nowHostile,
		const void* hero, const std::vector<unsigned char>& crimeMask, bool fullScope) {
		(void)targetHostile;   // stan walki wtedy nie do odtworzenia (AI sie nie cofa) - liczy sie tylko "teraz"
		CombatPlan p;
		bool stateChanged = target.state != now.state;
		bool enemyChanged = target.enemy != now.enemy;
		bool newHeroEnemy = hero && now.enemy == hero && target.enemy != hero;
		p.resetAI = (nowHostile && (stateChanged || enemyChanged)) || (newHeroEnemy && stateChanged);
		p.attitude = target.attitude != now.attitude || target.tmpAttitude != now.tmpAttitude;
		p.npcType = target.npcType != now.npcType;
		p.enemy = enemyChanged;
		int n = target.nAiv < now.nAiv ? target.nAiv : now.nAiv;
		for (int i = 0; i < n && i < COMBAT_AIV_MAX; i++) {
			bool crime = i < (int)crimeMask.size() && crimeMask[i] != 0;
			if (target.aiv[i] != now.aiv[i] && (fullScope || !crime))
				p.aivars.push_back(i);
		}
		return p;
	}

	CommitAction CombatCommitAction(CommitAction action, bool resetAI) {
		return resetAI && action == CommitAction::SoftLatest ? CommitAction::HardTarget : action;
	}

	std::vector<std::string> ParseNameList(const std::string& text) {
		std::vector<std::string> out;
		std::string cur;
		auto flush = [&]() {
			if (cur.empty())
				return;
			bool seen = false;
			for (const std::string& s : out)
				seen = seen || s == cur;
			if (!seen)
				out.push_back(cur);
			cur.clear();
		};
		for (char c : text) {
			if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n')
				flush();
			else
				cur += (char)std::toupper((unsigned char)c);
		}
		flush();
		return out;
	}

	std::vector<AivEntry> ResolveAivars(const std::vector<std::string>& combat, const std::vector<std::string>& crime,
		const std::function<bool(const std::string&, int&)>& lookup, std::vector<std::string>* unknown) {
		std::vector<AivEntry> out;
		auto add = [&](const std::vector<std::string>& names, bool isCrime) {
			for (const std::string& name : names) {
				int v = -1;
				if (!lookup(name, v) || v < 0 || v >= AIVAR_COUNT) {
					if (unknown)
						unknown->push_back(name);
					continue;
				}
				bool dup = false;
				for (const AivEntry& e : out)
					dup = dup || e.index == v;
				if (dup || (int)out.size() >= COMBAT_AIV_MAX)
					continue;
				AivEntry e;
				e.index = v;
				e.crime = isCrime;
				e.name = name;
				out.push_back(e);
			}
		};
		add(combat, false);
		add(crime, true);
		return out;
	}

	std::vector<int> NewsToRemove(const std::vector<NewsKey>& target, const std::vector<NewsKey>& now) {
		std::vector<int> out;
		for (int i = 0; i < (int)now.size(); i++) {
			bool known = false;
			for (const NewsKey& k : target)
				known = known || k == now[i];
			if (!known)
				out.push_back(i);
		}
		return out;
	}

	const char* AttitudeName(int att) {
		switch (att) {
		case 0: return "wrogi";
		case 1: return "zly";
		case 2: return "neutralny";
		case 3: return "przyjazny";
		default: return "?";
		}
	}
}
