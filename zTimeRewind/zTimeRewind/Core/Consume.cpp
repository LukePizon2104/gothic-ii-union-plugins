#include "Consume.h"
#include <algorithm>
#include <map>

namespace trcore {
	ItemKind ClassifyItem(int mainflag, int flags) {
		if (mainflag & ITEM_KAT_RUNE)
			return (flags & ITEM_MULTI) ? ItemKind::Scroll : ItemKind::Other;
		if (mainflag & ITEM_KAT_POTIONS)
			return ItemKind::Potion;
		if (mainflag & ITEM_KAT_FOOD)
			return ItemKind::Food;
		if (mainflag & ITEM_KAT_MUN)
			return ItemKind::Ammo;
		return ItemKind::Other;
	}

	const char* ItemKindName(ItemKind k) {
		switch (k) {
		case ItemKind::Scroll: return "zwoj";
		case ItemKind::Potion: return "mikstura";
		case ItemKind::Food: return "jedzenie";
		case ItemKind::Ammo: return "amunicja";
		default: return "-";
		}
	}

	bool RestoreOptions::Allows(ItemKind k) const {
		switch (k) {
		case ItemKind::Scroll: return scrolls;
		case ItemKind::Potion: return potions;
		case ItemKind::Food: return food;
		case ItemKind::Ammo: return ammo;
		default: return false;
		}
	}

	int RestoreMenuIndex(const RestoreOptions& o) {
		if (!o.Any())
			return RESTORE_MENU_OFF;
		if (o.scrolls && o.potions && o.food && o.ammo)
			return RESTORE_MENU_ALL;
		if (o.scrolls && !o.potions && !o.food && !o.ammo)
			return RESTORE_MENU_SCROLLS;
		return RESTORE_MENU_CUSTOM;
	}

	bool RestoreFromMenu(int index, RestoreOptions& o) {
		if (index != RESTORE_MENU_OFF && index != RESTORE_MENU_SCROLLS && index != RESTORE_MENU_ALL)
			return false;
		o.scrolls = index != RESTORE_MENU_OFF;
		o.potions = o.food = o.ammo = index == RESTORE_MENU_ALL;
		return true;
	}

	void InvCounts::Add(int instance, int amount) {
		if (amount <= 0)
			return;
		auto it = std::lower_bound(items.begin(), items.end(), instance,
			[](const std::pair<int, int>& p, int inst) { return p.first < inst; });
		if (it != items.end() && it->first == instance)
			it->second += amount;
		else
			items.insert(it, { instance, amount });
	}

	int InvCounts::Get(int instance) const {
		auto it = std::lower_bound(items.begin(), items.end(), instance,
			[](const std::pair<int, int>& p, int inst) { return p.first < inst; });
		return it != items.end() && it->first == instance ? it->second : 0;
	}

	std::vector<std::pair<int, int>> CountDecreases(const InvCounts& before, const InvCounts& after) {
		std::vector<std::pair<int, int>> out;
		for (auto& p : before.items) {
			int d = p.second - after.Get(p.first);
			if (d > 0)
				out.push_back({ p.first, d });
		}
		return out;
	}

	void UseEffect::Add(const HeroStats& before, const HeroStats& after) {
		for (int i = 0; i < HS_COUNT; i++) {
			int d = after.v[i] - before.v[i];
			if (!d)
				continue;
			if (i == HS_HP)
				hpGain += d;
			else if (i == HS_MANA)
				manaGain += d;
			else
				lasting = true;
		}
	}

	void UseEffect::Merge(const UseEffect& o) {
		manaGain += o.manaGain;
		hpGain += o.hpGain;
		lasting = lasting || o.lasting;
	}

	const char* UseSkipName(UseSkip s) {
		switch (s) {
		case UseSkip::Rewinding: return "w trakcie cofania";
		case UseSkip::Cutscene: return "cutscenka";
		case UseSkip::Dialog: return "rozmowa (handel / dawanie / skrypt)";
		case UseSkip::Mob: return "interakcja z mobem (przerobiony)";
		case UseSkip::InWorld: return "lezy w swiecie (wyrzucony)";
		case UseSkip::Lasting: return "trwaly efekt (cofniecie go nie usuwa)";
		default: return "zuzyty";
		}
	}

	UseSkip JudgeUse(const UseContext& c, const UseEffect& e) {
		if (c.rewinding)
			return UseSkip::Rewinding;
		if (c.cutscene)
			return UseSkip::Cutscene;
		if (c.dialog)
			return UseSkip::Dialog;
		if (c.mob)
			return UseSkip::Mob;
		if (c.inWorld)
			return UseSkip::InWorld;
		if (e.lasting)
			return UseSkip::Lasting;
		return UseSkip::Keep;
	}

	void UseLedger::TrimBefore(float t, std::vector<UseRecord>* removed) {
		size_t n = 0;
		while (n < recs.size() && recs[n].t < t)
			n++;
		if (removed)
			removed->insert(removed->end(), recs.begin(), recs.begin() + n);
		recs.erase(recs.begin(), recs.begin() + n);
	}

	void UseLedger::CutAfter(float t, std::vector<UseRecord>* removed) {
		while (!recs.empty() && recs.back().t > t) {
			if (removed)
				removed->push_back(recs.back());
			recs.pop_back();
		}
	}

	void UseLedger::Clear(std::vector<UseRecord>* removed) {
		if (removed)
			removed->insert(removed->end(), recs.begin(), recs.end());
		recs.clear();
	}

	std::vector<Refund> PlanRefunds(const std::vector<UseRecord>& recs, float t, const RestoreOptions& opt,
		const InvCounts& atTarget, const InvCounts& now, const std::function<bool(std::uintptr_t)>& returnable,
		std::vector<RefundSkip>* skipped) {
		std::vector<Refund> out;
		std::map<int, int> budget;   // instancja -> ile jeszcze mozna zwrocic (ubytek netto)
		auto skip = [&](const UseRecord& r, int amount, const char* why) {
			if (skipped && amount > 0)
				skipped->push_back({ r.instance, amount, r.kind, r.t - t, why });
		};
		for (const UseRecord& r : recs) {
			if (r.t <= t || r.amount <= 0)
				continue;
			if (r.skip != UseSkip::Keep) {
				skip(r, r.amount, UseSkipName(r.skip));
				continue;
			}
			if (!opt.Allows(r.kind)) {
				skip(r, r.amount, "kategoria wylaczona w ini");
				continue;
			}
			if (r.kind == ItemKind::Ammo && (!r.token || !returnable || !returnable(r.token))) {
				skip(r, r.amount, "strzala podniesiona / wbita / zniszczona");
				continue;
			}
			auto b = budget.find(r.instance);
			if (b == budget.end()) {
				int net = atTarget.Get(r.instance) - now.Get(r.instance);
				b = budget.emplace(r.instance, net > 0 ? net : 0).first;
			}
			int give = std::min(r.amount, b->second);
			if (give < r.amount)
				skip(r, r.amount - give, "ponad ubytek netto od chwili docelowej");
			if (give <= 0)
				continue;
			b->second -= give;
			Refund* f = nullptr;
			for (Refund& x : out)
				if (x.instance == r.instance)
					f = &x;
			if (!f) {
				out.push_back(Refund());
				f = &out.back();
				f->instance = r.instance;
				f->kind = r.kind;
				f->firstDt = r.t - t;
			}
			f->amount += give;
			if (r.manaGain > 0)
				f->manaRemove += give == r.amount ? r.manaGain : r.manaGain * give / r.amount;
			if (r.token)
				f->tokens.push_back(r.token);
		}
		return out;
	}

	float UseRecordTime(float now, float lastCountSample) {
		return lastCountSample == now ? now + 0.0005f : now;
	}

	std::vector<std::pair<int, int>> MergeDirectUse(std::vector<std::pair<int, int>> decreases, int instance, int used) {
		if (used <= 0 || instance < 0)
			return decreases;
		auto it = std::lower_bound(decreases.begin(), decreases.end(), instance,
			[](const std::pair<int, int>& p, int inst) { return p.first < inst; });
		if (it != decreases.end() && it->first == instance)
			it->second = std::max(it->second, used);
		else
			decreases.insert(it, { instance, used });
		return decreases;
	}

	std::vector<std::pair<std::string, int>> ParsePackString(const std::string& s) {
		std::vector<std::pair<std::string, int>> out;
		size_t i = 0;
		while (i < s.size()) {
			size_t end = s.find(',', i);
			if (end == std::string::npos)
				end = s.size();
			size_t colon = s.find(':', i);
			if (colon != std::string::npos && colon < end && colon > i) {
				std::string name = s.substr(i, colon - i);
				int amount = 0;
				size_t k = colon + 1;
				while (k < end && s[k] >= '0' && s[k] <= '9')
					amount = amount * 10 + (s[k++] - '0');
				size_t a = name.find_first_not_of(' '), b = name.find_last_not_of(' ');
				if (a != std::string::npos && amount > 0)
					out.push_back({ name.substr(a, b - a + 1), amount });
			}
			i = end + 1;
		}
		return out;
	}

	void ManaLedger::TrimBefore(float t) {
		size_t n = 0;
		while (n < recs.size() && recs[n].t < t)
			n++;
		recs.erase(recs.begin(), recs.begin() + n);
	}

	void ManaLedger::CutAfter(float t) {
		while (!recs.empty() && recs.back().t > t)
			recs.pop_back();
	}

	int ManaLedger::SpentAfter(float t, int* count) const {
		int sum = 0, n = 0;
		for (const ManaSpend& r : recs)
			if (r.t > t) {
				sum += r.amount;
				n++;
			}
		if (count)
			*count = n;
		return sum;
	}

	ManaRefund PlanManaRefund(int mana, int manaMax, int spentAfter, int potionRemove) {
		ManaRefund r;
		int give = spentAfter > 0 ? spentAfter : 0;
		int remove = potionRemove > 0 ? potionRemove : 0;
		int want = mana + give - remove;
		int top = manaMax > mana ? manaMax : mana;   // max nizsze niz mana (np. mikstura ponad max) - nie ucinamy
		r.mana = want < 0 ? 0 : want > top ? top : want;
		r.clipped = want > top ? std::min(give, want - top) : 0;
		r.give = give - r.clipped;
		return r;
	}
}
