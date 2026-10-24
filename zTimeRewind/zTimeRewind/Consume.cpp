// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Dlaczego haki sciezek zuzycia, a nie sama roznica ekwipunku: ubytek w plecaku nie mowi, dokad przedmiot
	// poszedl (sprzedaz, dawanie, wyrzucenie, kradziez, Npc_RemoveInvItems w skryptach). Kazda luka w filtrach
	// roznicy = dublowanie. Haki biora tylko ubytki w wywolaniach, ktore z definicji zuzywaja przedmiot, wiec
	// nieznana sciezka konczy sie najwyzej brakiem zwrotu, nigdy kopia. Pomiar przed / po wywolaniu (zamiast
	// odtwarzania logiki silnika) dziala tez, gdy silnik zmniejsza stos wprost (Spell_Cast: amount-- bez
	// RemoveFromInv) albo skrypt w tym samym wywolaniu oddaje przedmiot.
	static trcore::UseLedger useLedger;
	static trcore::StepTrack<trcore::InvCounts> heroCounts;   // liczniki w tickach nagrywania (tylko zmiany)
	static float lastCountSample = -1.0f;
	static std::vector<oCItem*> arrowsGoneAtBegin;            // tylko do porownan (referencje trzyma rejestr)
	static int probeDepth = 0;                                 // zagniezdzone haki (Destroy -> Equip -> UseItem)
	static trcore::UseEffect pendingUse;                       // efekt uzycia z EV_UseItemToState do zniszczenia
	static const void* pendingMsg = nullptr;
	static std::map<int, trcore::ItemKind> kindCache;          // instancja -> kategoria (dla przedmiotow ze spakowanego plecaka)
	static trcore::ManaLedger manaLedger;                      // mana wydana na czary (zwrot przy Commit, RestoreMana)
	static int manaDepth = 0;                                  // zagniezdzone pomiary many (Invest -> SetReleaseStatus)
	static bool spellCastSeen = false;                         // jednorazowy wpis: hak Spell_Cast dziala (test NB)

	static const unsigned MASK_SCROLL = trcore::KindBit(trcore::ItemKind::Scroll);
	static const unsigned MASK_USE = trcore::KindBit(trcore::ItemKind::Potion) | trcore::KindBit(trcore::ItemKind::Food);
	static const unsigned MASK_AMMO = trcore::KindBit(trcore::ItemKind::Ammo);

	static std::string InstName(int inst) {
		zCPar_Symbol* s = parser ? parser->GetSymbol(inst) : nullptr;
		return s ? s->name.ToChar() : ("#" + std::to_string(inst));
	}

	static void ReleaseTokens(const std::vector<trcore::UseRecord>& recs) {
		for (const trcore::UseRecord& r : recs)
			if (r.token)
				((oCItem*)r.token)->Release();
	}

	struct HeroItem {
		oCItem* item;
		int instance;
		trcore::ItemKind kind;
	};

	// Przedmioty sledzonych kategorii: plecak, przedmiot w rece (interactItem), sloty (strzala w dloni) i ksiega
	// czarow (zalozone zwoje) - bez powtorzen (zalozony przedmiot jest i w plecaku, i w slocie / ksiedze).
	// Wkladanie mikstury do reki nie jest ubytkiem.
	static void CollectHeroItems(oCNpc* h, std::vector<HeroItem>& out) {
		out.clear();
		auto add = [&](oCItem* it) {
			if (!it)
				return;
			trcore::ItemKind k = trcore::ClassifyItem(it->mainflag, it->flags);
			if (k == trcore::ItemKind::Other)
				return;
			for (const HeroItem& x : out)
				if (x.item == it)
					return;
			int inst = it->GetInstance();
			kindCache[inst] = k;
			out.push_back({ it, inst, k });
		};
		for (auto* n = h->inventory2.inventory.next; n; n = n->next)
			add(n->data);
		add(h->interactItem);
		for (int i = 0; i < h->invSlot.GetNum(); i++)
			if (TNpcSlot* s = h->invSlot[i])
				if (s->vob)
					add(zDYNAMIC_CAST<oCItem>(s->vob));
		if (oCMag_Book* b = h->mag_book)
			for (int i = 0; i < b->spellitems.GetNum(); i++)
				add(b->spellitems[i]);
	}

	// Spakowany plecak (packString): ilosci instancji, ktorych kategorie juz znamy (przedmiot byl kiedys obiektem -
	// zuzyc mozna tylko obiekt, wiec nieznane instancje ze spakowanego tekstu i tak nie sa sledzone). Silnik
	// (GetAmount, otwarcie plecaka) rozpakowuje je w obiekty - bez tego suma "przed / po" skakalaby przy rozpakowaniu.
	static void AddPackedCounts(oCNpc* h, trcore::InvCounts& out, int* packedPieces = nullptr) {
		if (!h || !parser || !h->inventory2.packString.Length())
			return;
		for (auto& p : trcore::ParsePackString(h->inventory2.packString.ToChar())) {
			int inst = parser->GetIndex(zSTRING(p.first.c_str()));
			auto k = kindCache.find(inst);
			if (inst < 0 || k == kindCache.end())
				continue;
			out.Add(inst, p.second);
			if (packedPieces)
				*packedPieces += p.second;
		}
	}

	static void CountItems(oCNpc* h, const std::vector<HeroItem>& items, trcore::InvCounts& out) {
		out.items.clear();
		for (const HeroItem& x : items)
			out.Add(x.instance, x.item->amount);
		AddPackedCounts(h, out);
	}

	static int CountNpcs() {
		oCWorld* w = ogame ? ogame->GetGameWorld() : nullptr;
		int n = 0;
		if (w && w->voblist_npcs)
			for (auto* it = w->voblist_npcs->next; it; it = it->next)
				n++;
		return n;
	}

	static void ReadStats(oCNpc* h, trcore::HeroStats& s) {
		for (int i = 0; i < NPC_ATR_MAX; i++)
			s.v[i] = h->attribute[i];
		for (int i = 0; i < oEDamageIndex_MAX; i++)
			s.v[trcore::HS_PROT + i] = h->protection[i];
		s.v[trcore::HS_LEVEL] = h->level;
		s.v[trcore::HS_EXP] = (int)h->experience_points;
		s.v[trcore::HS_LP] = (int)h->learn_points;
		s.v[trcore::HS_TIMED_OVERLAYS] = h->timedOverlays.GetNumInList();
		s.v[trcore::HS_OVERLAYS] = h->activeOverlays.GetNum();
		s.v[trcore::HS_NPCS] = CountNpcs();
	}

	struct Probe {
		bool on = false;
		unsigned mask = 0;
		std::vector<HeroItem> items;
		trcore::InvCounts counts;
		trcore::HeroStats stats;
	};

	// Tylko bohater, tylko najbardziej zewnetrzny hak. Przedmioty z maski dostaja referencje na czas wywolania
	// (zniszczony w nim przedmiot - np. mikstura w rece - musi dac sie sprawdzic po powrocie).
	static bool ProbeBegin(oCNpc* npc, unsigned mask, Probe& p) {
		if (!trcfg.enabled || !player || npc != player || probeDepth > 0 || !ogame)
			return false;
		p.on = true;
		p.mask = mask;
		CollectHeroItems(player, p.items);
		CountItems(player, p.items, p.counts);
		ReadStats(player, p.stats);
		for (const HeroItem& h : p.items)
			if (mask & trcore::KindBit(h.kind))
				h.item->AddRef();
		return true;
	}

	// direct*: bezposredni pomiar przedmiotu sciezki (Spell_Cast: zwoj z ksiegi, amount przed - po) - liczy sie
	// takze wtedy, gdy liczniki ekwipunku go nie widza (zwoj spoza listy plecaka).
	static void ProbeEnd(Probe& p, const char* where, bool usePath, int directInst = -1, int directUsed = 0,
		trcore::ItemKind directKind = trcore::ItemKind::Other) {
		if (!p.on)
			return;
		ZPROF_SCOPE("Consume.Probe");
		std::vector<HeroItem> nowItems;
		trcore::InvCounts nowCounts;
		trcore::HeroStats nowStats;
		CollectHeroItems(player, nowItems);
		CountItems(player, nowItems, nowCounts);
		ReadStats(player, nowStats);
		trcore::UseEffect e;
		e.Add(p.stats, nowStats);
		if (usePath)
			pendingUse.Merge(e);
		auto decreases = trcore::CountDecreases(p.counts, nowCounts);
		int countedDirect = 0;
		for (auto& d : decreases)
			if (d.first == directInst)
				countedDirect = d.second;
		if (directUsed > 0 && countedDirect < directUsed)
			ZLog("Ekwipunek: %s - zwoj %s x%d zmierzony wprost na przedmiocie ksiegi (liczniki plecaka: x%d)", where,
				InstName(directInst).c_str(), directUsed, countedDirect);
		decreases = trcore::MergeDirectUse(decreases, directInst, directUsed);
		for (auto& d : decreases) {
			const HeroItem* before = nullptr;
			for (const HeroItem& h : p.items)
				if (h.instance == d.first) {
					before = &h;
					break;
				}
			trcore::ItemKind kind = before ? before->kind : d.first == directInst ? directKind : trcore::ItemKind::Other;
			if (kind == trcore::ItemKind::Other || !(p.mask & trcore::KindBit(kind)))
				continue;   // inna kategoria niz ta sciezka zuzywa (np. skrypt czaru zabral cos innego)
			oCItem* inWorld = nullptr;
			for (const HeroItem& h : p.items)
				if (h.instance == d.first && h.item->homeWorld)
					inWorld = h.item;
			trcore::UseContext c;
			c.rewinding = RewindActive();
			c.cutscene = player->IsInGlobalCutscene() != 0;
			c.dialog = !oCInformationManager::GetInformationManager().HasFinished();
			c.mob = player->GetInteractMob() != nullptr;
			c.inWorld = kind != trcore::ItemKind::Ammo && inWorld;   // strzala w swiecie = wystrzelona
			const trcore::UseEffect& eff = usePath ? pendingUse : e;
			trcore::UseRecord r;
			r.t = trcore::UseRecordTime(TrNow(), lastCountSample);
			r.instance = d.first;
			r.amount = d.second;
			r.kind = kind;
			r.skip = trcore::JudgeUse(c, eff);
			r.manaGain = eff.manaGain;
			if (kind == trcore::ItemKind::Ammo && inWorld && r.skip == trcore::UseSkip::Keep) {
				inWorld->AddRef();
				r.token = (std::uintptr_t)inWorld;
			}
			useLedger.Add(r);
			// zwoje zawsze do logu (rzadkie; diagnostyka NB), reszta tylko z Debug=1 (strzaly w walce)
			(kind == trcore::ItemKind::Scroll ? ZLog : ZDebug)("Ekwipunek: zuzycie %s x%d (%s, %s) t=%.2f hp %+d mana %+d%s%s",
				InstName(r.instance).c_str(), r.amount, trcore::ItemKindName(kind), where, r.t, eff.hpGain, eff.manaGain,
				r.skip == trcore::UseSkip::Keep ? "" : " - bez zwrotu: ", r.skip == trcore::UseSkip::Keep ? "" : trcore::UseSkipName(r.skip));
			if (usePath)
				pendingUse = trcore::UseEffect();
		}
		for (const HeroItem& h : p.items)
			if (p.mask & trcore::KindBit(h.kind))
				h.item->Release();
		p.on = false;
	}

	// ---------------------------------------------------------------- haki (po adresach G2A, jak SetByScript)

	// ---- mana wydana na czary: pomiar many bohatera wokol wywolan ksiegi (Spell_Invest - koszt czaru ladowanego
	// i SENDCAST w Spell_ProcessMana, SetReleaseStatus - puszczenie, Spell_Cast - skrypt Spell_Cast_* odejmuje
	// koszt). New Balance podmienia oCSpell::Invest (0x1031F720 w UNION_ABI.DLL: mana na chwile 1000 / 0 wokol
	// CallScriptInvestedMana i przywrocenie) - pomiar na poziomie ksiegi widzi tylko wynik netto. Bez runy cofania
	// (jej koszt ManaStart / ManaPerSecond zostaje wydany) i bez strumieni NB (mana zabierana w petli DLL NB).
	struct ManaProbe {
		bool on = false;
		int before = 0;
	};

	static bool ManaBegin(zCVob* caster, int spellId, ManaProbe& m) {
		if (!trcfg.enabled || !player || caster != player || manaDepth > 0 || spellId == trcfg.spellId || RewindActive())
			return false;
		m.on = true;
		m.before = player->attribute[NPC_ATR_MANA];
		return true;
	}

	static void ManaEnd(ManaProbe& m, const char* where, int spellId) {
		if (!m.on || !player)
			return;
		int spent = m.before - player->attribute[NPC_ATR_MANA];
		if (spent <= 0)
			return;
		float t = trcore::UseRecordTime(TrNow(), lastCountSample);
		manaLedger.Add(t, spent);
		ZDebug("Mana: czar %d (%s) -%d, mana %d -> %d, t=%.2f", spellId, where, spent, m.before, player->attribute[NPC_ATR_MANA], t);
	}

	static int BookSpellId(oCMag_Book* b) {
		oCSpell* s = b ? b->GetSelectedSpell() : nullptr;
		return s ? s->spellID : -1;
	}

	// Zwoj rzucany z ksiegi: Spell_Cast zmniejsza amount wlasnie tego przedmiotu (spellitems[spellnr], vanilla
	// 0x476C98; przy 0 RemoveFromInv 0x476CBC). W NB sciezka ta sama (UNION_ABI nie rusza ilosci zwojow; EV_CastSpell
	// 0x10307010 i MagicMode 0x103123F0 NB koncza w MagicCheckSpellStates -> Spell_Cast 0x472A35).
	static oCItem* BookScroll(oCMag_Book* b) {
		if (!b || b->spellnr < 0 || b->spellnr >= b->spellitems.GetNum())
			return nullptr;
		oCItem* it = b->spellitems[b->spellnr];
		return it && trcore::ClassifyItem(it->mainflag, it->flags) == trcore::ItemKind::Scroll ? it : nullptr;
	}

	static void __fastcall Hk_oCMag_Book_Spell_Cast(oCMag_Book* self, void* edx);
	static CInvoke<void(__fastcall*)(oCMag_Book*, void*)> Ivk_oCMag_Book_Spell_Cast(0x004767A0,
		&Hk_oCMag_Book_Spell_Cast, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCMag_Book_Spell_Cast(oCMag_Book* self, void* edx) {
		bool hero = self && player && self->owner == player;
		Probe p;
		ProbeBegin(hero ? player : nullptr, MASK_SCROLL, p);
		ManaProbe m;
		int spellId = hero ? BookSpellId(self) : -1;
		ManaBegin(hero ? self->owner : nullptr, spellId, m);
		oCItem* scroll = p.on ? BookScroll(self) : nullptr;
		int scrollInst = -1, scrollAmount = 0;
		if (scroll) {
			scroll->AddRef();
			scrollInst = scroll->GetInstance();
			scrollAmount = scroll->amount;
		}
		if (hero && !spellCastSeen) {
			spellCastSeen = true;
			ZLog("Ekwipunek: pierwszy rzut bohatera z ksiegi (hak Spell_Cast 0x4767A0 dziala), czar %d, zwoj %s x%d", spellId,
				scroll ? InstName(scrollInst).c_str() : "-", scrollAmount);
		}
		probeDepth++;
		manaDepth++;
		Ivk_oCMag_Book_Spell_Cast(self, edx);
		manaDepth--;
		probeDepth--;
		int used = scroll && scroll->amount < scrollAmount ? scrollAmount - scroll->amount : 0;
		ProbeEnd(p, "rzut czaru", false, scrollInst, used, trcore::ItemKind::Scroll);
		ManaEnd(m, "rzut", spellId);
		if (scroll)
			scroll->Release();
	}

	static void __fastcall Hk_oCMag_Book_Spell_Invest(oCMag_Book* self, void* edx);
	static CInvoke<void(__fastcall*)(oCMag_Book*, void*)> Ivk_oCMag_Book_Spell_Invest(0x00476530,
		&Hk_oCMag_Book_Spell_Invest, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCMag_Book_Spell_Invest(oCMag_Book* self, void* edx) {
		bool hero = self && player && self->owner == player;
		ManaProbe m;
		int spellId = hero ? BookSpellId(self) : -1;
		ManaBegin(hero ? self->owner : nullptr, spellId, m);
		manaDepth++;
		Ivk_oCMag_Book_Spell_Invest(self, edx);
		manaDepth--;
		ManaEnd(m, "ladowanie", spellId);
	}

	static void __fastcall Hk_oCSpell_SetReleaseStatus(oCSpell* self, void* edx);
	static CInvoke<void(__fastcall*)(oCSpell*, void*)> Ivk_oCSpell_SetReleaseStatus(0x00486670,
		&Hk_oCSpell_SetReleaseStatus, IvkEnabled(ENGINE));
	static void __fastcall Hk_oCSpell_SetReleaseStatus(oCSpell* self, void* edx) {
		ManaProbe m;
		ManaBegin(self ? self->spellCasterNpc : nullptr, self ? self->spellID : -1, m);
		manaDepth++;
		Ivk_oCSpell_SetReleaseStatus(self, edx);
		manaDepth--;
		ManaEnd(m, "puszczenie", self ? self->spellID : -1);
	}

	static int __fastcall Hk_oCNpc_EV_UseItemToState(oCNpc* self, void* edx, oCMsgManipulate* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgManipulate*)> Ivk_oCNpc_EV_UseItemToState(0x007558F0,
		&Hk_oCNpc_EV_UseItemToState, IvkEnabled(ENGINE));
	static int __fastcall Hk_oCNpc_EV_UseItemToState(oCNpc* self, void* edx, oCMsgManipulate* msg) {
		Probe p;
		if (ProbeBegin(self, MASK_USE, p) && msg != pendingMsg) {
			pendingUse = trcore::UseEffect();   // nowe uzycie - efekt poprzedniego (przerwanego) przepada
			pendingMsg = msg;
		}
		probeDepth++;
		int r = Ivk_oCNpc_EV_UseItemToState(self, edx, msg);
		probeDepth--;
		ProbeEnd(p, "uzycie", true);
		return r;
	}

	static int __fastcall Hk_oCNpc_EV_DestroyInteractItem(oCNpc* self, void* edx, oCMsgManipulate* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgManipulate*)> Ivk_oCNpc_EV_DestroyInteractItem(0x00754B40,
		&Hk_oCNpc_EV_DestroyInteractItem, IvkEnabled(ENGINE));
	static int __fastcall Hk_oCNpc_EV_DestroyInteractItem(oCNpc* self, void* edx, oCMsgManipulate* msg) {
		Probe p;
		ProbeBegin(self, MASK_USE, p);
		probeDepth++;
		int r = Ivk_oCNpc_EV_DestroyInteractItem(self, edx, msg);
		probeDepth--;
		ProbeEnd(p, "zniszczony w rece", true);
		return r;
	}

	static int __fastcall Hk_oCNpc_UseItem(oCNpc* self, void* edx, oCItem* item);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCItem*)> Ivk_oCNpc_UseItem(0x0073BC10, &Hk_oCNpc_UseItem,
		IvkEnabled(ENGINE));
	static int __fastcall Hk_oCNpc_UseItem(oCNpc* self, void* edx, oCItem* item) {
		Probe p;
		if (ProbeBegin(self, MASK_USE, p))
			pendingUse = trcore::UseEffect();   // natychmiastowe uzycie: efekt i ubytek w tym samym wywolaniu
		probeDepth++;
		int r = Ivk_oCNpc_UseItem(self, edx, item);
		probeDepth--;
		ProbeEnd(p, "natychmiast", true);
		return r;
	}

	static int __fastcall Hk_oCNpc_DoShootArrow(oCNpc* self, void* edx, int aimed);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, int)> Ivk_oCNpc_DoShootArrow(0x007446B0, &Hk_oCNpc_DoShootArrow,
		IvkEnabled(ENGINE));
	static int __fastcall Hk_oCNpc_DoShootArrow(oCNpc* self, void* edx, int aimed) {
		Probe p;
		ProbeBegin(self, MASK_AMMO, p);
		probeDepth++;
		int r = Ivk_oCNpc_DoShootArrow(self, edx, aimed);
		probeDepth--;
		ProbeEnd(p, "strzal", false);
		return r;
	}

	// ---------------------------------------------------------------- nagranie i Commit

	void ConsumeReset() {
		std::vector<trcore::UseRecord> gone;
		useLedger.Clear(&gone);
		ReleaseTokens(gone);
		heroCounts.Clear();
		lastCountSample = -1.0f;
		arrowsGoneAtBegin.clear();
		pendingUse = trcore::UseEffect();
		pendingMsg = nullptr;
		kindCache.clear();
		manaLedger.Clear();
		spellCastSeen = false;
	}

	void ConsumeSample(float t) {
		if (!player)
			return;
		std::vector<HeroItem> items;
		trcore::InvCounts c;
		CollectHeroItems(player, items);
		CountItems(player, items, c);
		heroCounts.Push(t, c);
		lastCountSample = t;
	}

	void ConsumeTrim(float oldest) {
		std::vector<trcore::UseRecord> gone;
		useLedger.TrimBefore(oldest, &gone);
		ReleaseTokens(gone);
		heroCounts.TrimBefore(oldest);
		manaLedger.TrimBefore(oldest);
	}

	void ConsumeCutAfter(float t) {
		std::vector<trcore::UseRecord> gone;
		useLedger.CutAfter(t, &gone);
		ReleaseTokens(gone);
		heroCounts.CutAfter(t);
		manaLedger.CutAfter(t);
		arrowsGoneAtBegin.clear();
		pendingUse = trcore::UseEffect();
		pendingMsg = nullptr;
	}

	void ConsumeRewindBegin() {
		arrowsGoneAtBegin.clear();
	}

	void ConsumeArrowRemoved(zCVob* vob) {
		if (oCItem* it = zDYNAMIC_CAST<oCItem>(vob))
			arrowsGoneAtBegin.push_back(it);
	}

	// Mana wydana na czary po t wraca (RestoreMana), przyrost many z oddanych mikstur odchodzi - jedno ustawienie.
	static void RestoreManaAfter(float t, int potionRemove) {
		int count = 0;
		int spent = manaLedger.SpentAfter(t, &count);
		if (!trcfg.restoreMana) {
			if (spent > 0)
				ZDebug("Mana: wydana na czary po celu %d (%d wpisow) - bez zwrotu (RestoreMana=0)", spent, count);
			spent = 0;
		}
		if (spent <= 0 && potionRemove <= 0)
			return;
		int mana = player->attribute[NPC_ATR_MANA];
		trcore::ManaRefund r = trcore::PlanManaRefund(mana, player->attribute[NPC_ATR_MANAMAX], spent, potionRemove);
		player->attribute[NPC_ATR_MANA] = r.mana;
		if (spent > 0)
			ZLog("Mana: zwrot wydanej na czary +%d (%d wpisow po celu%s), mikstury -%d, mana %d -> %d (max %d)", r.give, count,
				r.clipped ? (", limit max many -" + std::to_string(r.clipped)).c_str() : "", potionRemove, mana, r.mana,
				player->attribute[NPC_ATR_MANAMAX]);
		else
			ZDebug("Ekwipunek: mana z oddanych mikstur %d -> %d (-%d)", mana, r.mana, potionRemove);
	}

	void ConsumeRestoreAfter(float t) {
		if (!player)
			return;
		ZPROF_SCOPE("Consume.Restore");
		int manaRemove = 0;
		trcore::InvCounts atT, now;
		if (!heroCounts.At(t, atT)) {
			if (!useLedger.Records().empty())
				ZLog("Ekwipunek: brak licznikow ekwipunku <= %.2f - bez zwrotu", t);
			RestoreManaAfter(t, 0);
			return;
		}
		std::vector<HeroItem> items;
		CollectHeroItems(player, items);
		CountItems(player, items, now);
		// strzala wraca, gdy lezy / tkwi w swiecie (zabierana) albo zniknela w locie na starcie cofania;
		// podniesiona, wbita w postac albo zniszczona przez silnik - nie (zwrot moglby ja podwoic)
		auto returnable = [](std::uintptr_t tok) {
			oCItem* it = (oCItem*)tok;
			return it->homeWorld != nullptr
				|| std::find(arrowsGoneAtBegin.begin(), arrowsGoneAtBegin.end(), it) != arrowsGoneAtBegin.end();
		};
		std::vector<trcore::RefundSkip> skipped;
		std::vector<trcore::Refund> refunds = trcore::PlanRefunds(useLedger.Records(), t, trcfg.restore, atT, now,
			returnable, &skipped);
		int pieces = 0;
		for (const trcore::Refund& f : refunds) {
			for (std::uintptr_t tok : f.tokens) {
				oCItem* it = (oCItem*)tok;
				if (it->homeWorld)
					it->homeWorld->RemoveVob(it);   // ta sama strzala nie zostaje do podniesienia
			}
			player->CreateItems(f.instance, f.amount);
			manaRemove += f.manaRemove;
			pieces += f.amount;
			(f.kind == trcore::ItemKind::Scroll ? ZLog : ZDebug)("Ekwipunek: oddano %s x%d (zuzyty %.1f s po celu, %s)",
				InstName(f.instance).c_str(), f.amount, f.firstDt, trcore::ItemKindName(f.kind));
		}
		for (const trcore::RefundSkip& s : skipped)
			(s.kind == trcore::ItemKind::Scroll ? ZLog : ZDebug)("Ekwipunek: bez zwrotu %s x%d (%s, %.1f s po celu)",
				InstName(s.instance).c_str(), s.amount, s.why, s.dt);
		RestoreManaAfter(t, manaRemove);
		if (!refunds.empty() || !skipped.empty())
			ZLog("Ekwipunek: oddano %d szt. (%d pozycji), bez zwrotu %d wpisow, mana -%d", pieces, (int)refunds.size(),
				(int)skipped.size(), manaRemove);
		else if (!useLedger.Records().empty())
			ZLog("Ekwipunek: w rejestrze %d zuzyc, zadne po chwili docelowej (najpozniejsze %.1f s przed nia) - bez zwrotu",
				(int)useLedger.Records().size(), t - useLedger.Records().back().t);
		arrowsGoneAtBegin.clear();
	}

	void ConsumeRegisterCommands() {
		TrConsoleRegister("INV", "tr inv - zuzyte przedmioty w buforze cofania (zwrot przy cofnieciu)", [](const std::vector<std::string>&) {
			std::string s = "inv: zwoje " + std::to_string((int)trcfg.restore.scrolls) + " mikstury " +
				std::to_string((int)trcfg.restore.potions) + " jedzenie " + std::to_string((int)trcfg.restore.food) +
				" amunicja " + std::to_string((int)trcfg.restore.ammo) + " | wpisy " + std::to_string((int)useLedger.Records().size());
			if (player) {
				std::vector<HeroItem> items;
				CollectHeroItems(player, items);
				trcore::InvCounts c;
				CountItems(player, items, c);
				int pieces = 0;
				for (auto& x : c.items)
					pieces += x.second;
				int packed = 0;
				trcore::InvCounts tmp;
				AddPackedCounts(player, tmp, &packed);
				s += " | bohater: " + std::to_string((int)c.items.size()) + " rodzajow, " + std::to_string(pieces) + " szt. (spakowane " +
					std::to_string(packed) + "), probki " + std::to_string((int)heroCounts.Size()) +
					(player->inventory2.packString.Length() ? ", PLECAK SPAKOWANY" : "");
				if (oCMag_Book* b = player->mag_book)
					if (oCItem* sc = BookScroll(b))
						s += " | zwoj w ksiedze: " + InstName(sc->GetInstance()) + " x" + std::to_string(sc->amount);
			}
			int manaCount = 0;
			int manaSpent = manaLedger.SpentAfter(-1.0f, &manaCount);
			s += " | mana na czary w buforze " + std::to_string(manaSpent) + " (" + std::to_string(manaCount) + " wpisow), zwrot " +
				std::to_string((int)trcfg.restoreMana);
			const auto& recs = useLedger.Records();
			for (size_t i = recs.size() > 6 ? recs.size() - 6 : 0; i < recs.size(); i++) {
				char buf[160];
				sprintf_s(buf, " | %s x%d %.1fs temu%s", InstName(recs[i].instance).c_str(), recs[i].amount, TrNow() - recs[i].t,
					recs[i].skip == trcore::UseSkip::Keep ? "" : " (bez zwrotu)");
				s += buf;
			}
			ZLog("%s", s.c_str());
			return zSTRING(s.c_str());
		});
	}
}
