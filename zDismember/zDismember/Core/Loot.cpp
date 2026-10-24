#include "Loot.h"
#include "Moves.h"
#include <algorithm>
#include <cmath>

namespace zdcore {
	const char* LootDenyText(LootDeny d) {
		switch (d) {
		case LOOT_OK: return "mozna";
		case LOOT_OFF: return "wylaczone (LootArmed=0)";
		case LOOT_NOT_HUMAN: return "bohater nie w modelu czlowieka";
		case LOOT_NOT_MELEE: return "bez broni bialej / piesci w rece";
		case LOOT_WINDOW: return "otwarte okno (ekwipunek, konsola, dialog)";
		case LOOT_MOVLOCK: return "blokada sterowania";
		case LOOT_PLUGIN_MOVE: return "trwa ruch pluginu (unik, przewrot, przeskok, finisher)";
		case LOOT_ATTACK: return "trwa cios";
		case LOOT_BUSY: return "kolejka komunikatow bohatera niepusta";
		case LOOT_BODY: return "bohater nie stoi i nie idzie (skok, upadek, sprint, parada)";
		case LOOT_WATER: return "w wodzie";
		case LOOT_ENEMY: return "walczacy wrog w poblizu";
		case LOOT_NO_ITEM: return "brak celu przed bohaterem (przedmiot, drzwi, skrzynia, zwloki)";
		default: return "?";
		}
	}

	LootDeny LootHeroDeny(const LootHeroState& s) {
		if (!s.enabled)
			return LOOT_OFF;
		if (!s.human)
			return LOOT_NOT_HUMAN;
		if (!s.melee)
			return LOOT_NOT_MELEE;
		if (s.window)
			return LOOT_WINDOW;
		if (s.movLock)
			return LOOT_MOVLOCK;
		if (s.pluginMove)
			return LOOT_PLUGIN_MOVE;
		if (s.attacking)
			return LOOT_ATTACK;
		if (s.busy)
			return LOOT_BUSY;
		if (!s.bodyOk)
			return LOOT_BODY;
		if (s.water)
			return LOOT_WATER;
		return LOOT_OK;
	}

	LootDeny LootDecide(LootDeny heroDeny, bool enemyNear, bool hasItem) {
		if (heroDeny != LOOT_OK)
			return heroDeny;
		if (enemyNear)
			return LOOT_ENEMY;
		return hasItem ? LOOT_OK : LOOT_NO_ITEM;
	}

	bool LootLogDeny(LootDeny d, bool hadItem, bool enemyNear) {
		if (d == LOOT_OK || LootDenySilent(d))
			return false;
		if (hadItem)
			return true;
		return d == LOOT_NO_ITEM && !enemyNear;
	}

	bool LootFoeBlocks(const LootFoe& f, float radius) {
		if (radius <= 0.0f || !f.alive || !f.conscious || f.dist > radius)
			return false;
		if (f.targetsHero && (f.hostile || f.fighting))
			return true;
		return f.hostile && (f.fighting || f.heroTargets);
	}

	bool LootItemFits(const LootParams& p, float fwdX, float fwdZ, float dx, float dy, float dz, float& score) {
		if (dy < p.minDy || dy > p.maxDy)
			return false;
		float dist = std::sqrt(dx * dx + dz * dz);
		if (dist > p.range)
			return false;
		float ang = 0.0f;   // rad od osi przodu
		if (dist >= p.nearAny) {
			if (fwdX * fwdX + fwdZ * fwdZ < 1e-8f)
				return false;
			ang = std::fabs(AngleDiff(std::atan2(fwdX, fwdZ), std::atan2(dx, dz)));
			if (ang > p.coneDeg * 3.14159265f / 180.0f)
				return false;
		}
		float cone = std::max(p.coneDeg, 1.0f) * 3.14159265f / 180.0f;
		score = dist + p.anglePenalty * std::min(ang / cone, 1.0f);
		return true;
	}

	std::vector<int> LootRankItems(const LootParams& p, float fwdX, float fwdZ, const std::vector<LootItem>& items) {
		std::vector<std::pair<float, int>> ok;
		for (int i = 0; i < (int)items.size(); i++) {
			const LootItem& it = items[i];
			float s = 0.0f;
			if (it.takeable && LootItemFits(p, fwdX, fwdZ, it.dx, it.dy, it.dz, s))
				ok.push_back(std::make_pair(s, i));
		}
		std::stable_sort(ok.begin(), ok.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
			return a.first < b.first;
		});
		std::vector<int> r;
		for (auto& e : ok)
			r.push_back(e.second);
		return r;
	}

	bool LootLatchNeedsDecision(LootKeyLatch& l, bool down) {
		if (!down) {
			l.state = 0;
			return false;
		}
		return l.state == 0;
	}

	void LootLatchSet(LootKeyLatch& l, bool ours) {
		l.state = ours ? 1 : 2;
	}

	bool LootLatchSwallow(const LootKeyLatch& l, bool down) {
		return down && l.state == 1;
	}

	int LootMenuIndex(bool armed, bool hint) {
		return !armed ? 0 : hint ? 2 : 1;
	}

	void LootMenuApply(int index, bool& armed, bool& hint) {
		if (index < 0 || index > 2)
			return;
		armed = index > 0;
		if (index > 0)
			hint = index == 2;
	}
}
