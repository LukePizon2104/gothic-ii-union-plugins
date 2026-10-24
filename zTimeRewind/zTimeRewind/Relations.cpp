// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Listy z ini przeliczone na symbole biezacego parsera.
	struct RelDefs {
		const void* parser = nullptr;
		bool dirty = true;
		std::vector<trcore::AivEntry> aivs;
		std::vector<unsigned char> crimeMask;   // crimeMask[i] = aivs[i].crime
		std::vector<int> hostileStates;         // indeksy symboli ZS_* z CombatStates
		std::string layout;                     // uklad migawek (indeksy aivarow) - zmiana czysci tory walki
	};
	static RelDefs relDefs;

	struct RelCounters {
		int npcs = 0, resetAI = 0, attitude = 0, enemy = 0, npcType = 0, aivars = 0, news = 0, newsShared = 0;
	};
	static RelCounters relCount;

	void RelationsInvalidate() {
		relDefs.dirty = true;
	}

	static bool ConstInt(const std::string& name, int& v) {
		zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(name.c_str())) : nullptr;
		if (!s || s->type != zPAR_TYPE_INT || !(s->flags & zPAR_FLAG_CONST) || s->ele < 1)
			return false;
		s->GetValue(v, 0);
		return true;
	}

	static void ResetCombatTracks() {
		for (TrActor* a : RecorderActors()) {
			a->combat.Clear();
			a->news.Clear();
		}
	}

	static const RelDefs& Defs() {
		if (!parser)
			return relDefs;
		if (!relDefs.dirty && relDefs.parser == parser)
			return relDefs;
		RelDefs& d = relDefs;
		d.parser = parser;
		d.dirty = false;
		std::vector<std::string> unknown;
		d.aivs = trcore::ResolveAivars(trcore::ParseNameList(trcfg.combatAivars), trcore::ParseNameList(trcfg.crimeAivars),
			ConstInt, &unknown);
		d.crimeMask.clear();
		std::string layout;
		for (const trcore::AivEntry& e : d.aivs) {
			d.crimeMask.push_back(e.crime ? 1 : 0);
			layout += std::to_string(e.index) + (e.crime ? "c," : ",");
		}
		d.hostileStates.clear();
		std::vector<std::string> states = trcore::ParseNameList(trcfg.combatStates);
		for (const std::string& name : states) {
			int idx = parser->GetIndex(zSTRING(name.c_str()));
			zCPar_Symbol* s = idx >= 0 ? parser->GetSymbol(idx) : nullptr;
			if (s && s->type == zPAR_TYPE_FUNC)
				d.hostileStates.push_back(idx);
			else
				unknown.push_back(name);
		}
		if (layout != d.layout) {
			ResetCombatTracks();   // inny uklad aiv[] - stare migawki nieporownywalne
			d.layout = layout;
		}
		std::string missing;
		for (const std::string& n : unknown)
			missing += (missing.empty() ? "" : ", ") + n;
		ZLog("Relacje: aivary %d (przestepstwa %d), stany walki %d z %d%s%s", (int)d.aivs.size(),
			(int)std::count(d.crimeMask.begin(), d.crimeMask.end(), (unsigned char)1), (int)d.hostileStates.size(),
			(int)states.size(), missing.empty() ? "" : " - brak w skryptach: ", missing.c_str());
		return d;
	}

	static bool IsHostileState(const RelDefs& d, int idx) {
		if (idx < 0)
			return false;
		return std::find(d.hostileStates.begin(), d.hostileStates.end(), idx) != d.hostileStates.end();
	}

	static void CaptureCombat(oCNpc* npc, const RelDefs& d, trcore::CombatSnap& c) {
		c.attitude = npc->attitude;
		c.tmpAttitude = npc->tmp_attitude;
		c.attTimer = npc->attTimer;
		c.npcType = npc->npcType;
		c.state = npc->state.curState.index;
		c.enemy = npc->enemy;
		c.enemyInst = npc->enemy ? npc->enemy->instanz : -1;
		c.nAiv = (int)d.aivs.size();
		for (int i = 0; i < c.nAiv; i++)
			c.aiv[i] = npc->aiscriptvars[d.aivs[i].index];
	}

	// News w pamieci postaci. Skrypty G2A i NB nie wywoluja Npc_MemoryEntry(Guild) (jedyne miejsca
	// oCNewsManager::CreateNews w silniku), wiec lista jest zwykle pusta - wtedy bez wpisu w torze.
	static void CaptureNews(oCNpc* npc, std::vector<trcore::NewsKey>& out) {
		out.clear();
		for (auto it = npc->knownNews.iknow.next; it; it = it->next)
			if (oCNews* n = it->data)
				out.push_back({ n, n->news_id, n->mNpcOffender, n->mNpcVictim });
	}

	void TrRecordCombat(TrActor& a, float t) {
		if (!trcfg.restoreCombat || !a.npc || a.npc == player)
			return;
		const RelDefs& d = Defs();
		if (!d.parser)
			return;
		trcore::CombatSnap c;
		CaptureCombat(a.npc, d, c);
		a.combat.Push(t, c);
		static std::vector<trcore::NewsKey> news;
		CaptureNews(a.npc, news);
		if (!news.empty() || !a.news.Empty())
			a.news.Push(t, news);
	}

	// Wrog z migawki tylko, gdy wskaznik nadal wskazuje te sama postac: bohater albo aktor nagrania (rejestr trzyma
	// referencje, wiec adres jest zywy) z ta sama instancja. Inaczej brak wroga.
	static oCNpc* ValidEnemy(const trcore::CombatSnap& s) {
		if (!s.enemy)
			return nullptr;
		if (player && s.enemy == player)
			return player->instanz == s.enemyInst ? player : nullptr;
		for (TrActor* a : RecorderActors())
			if (a->npc == s.enemy)
				return a->npc->instanz == s.enemyInst && a->npc->homeWorld ? a->npc : nullptr;
		return nullptr;
	}

	static std::string StateName(int idx) {
		if (idx < 0)
			return idx == -1 ? "-" : "PRG" + std::to_string(idx);
		zCPar_Symbol* s = parser ? parser->GetSymbol(idx) : nullptr;
		return s ? std::string(s->name.ToChar()) : "#" + std::to_string(idx);
	}

	// Opis wroga do logu. Wskaznik zywy (oCNpc::enemy trzyma referencje) albo z migawki - ten tylko przez rejestr.
	static std::string EnemyName(oCNpc* live) {
		if (!live)
			return "-";
		if (live == player)
			return "BOHATER";
		return live->GetInstanceName().ToChar();
	}

	bool TrPlanCombat(TrActor& a, float t, TrCombatCommit& out) {
		if (!trcfg.restoreCombat || !a.npc || a.npc == player || a.combat.Empty())
			return false;
		const RelDefs& d = Defs();
		if (!d.parser)
			return false;
		if (!a.combat.At(t, out.target))
			out.target = a.combat.First();   // pojawil sie pozniej - stan z pierwszej probki
		CaptureCombat(a.npc, d, out.now);
		const TNpcAIState& next = a.npc->state.nextState;
		out.nowHostile = IsHostileState(d, out.now.state) || (next.valid && IsHostileState(d, next.index));
		out.plan = trcore::DecideCombat(out.target, out.now, IsHostileState(d, out.target.state), out.nowHostile, player,
			d.crimeMask, trcfg.rewindScope == 1);
		return true;
	}

	// Newsy nowsze niz chwila docelowa (RewindScope=1). oCNewsMemory::Remove (0x729790) odpina i usuwa obiekt -
	// news wspoldzielony z pamiecia innej postaci zostaje (bez podwojnego zwolnienia).
	static int RevertNews(TrActor& a, float t, int& shared) {
		std::vector<trcore::NewsKey> now, target;
		CaptureNews(a.npc, now);
		if (now.empty())
			return 0;
		if (!a.news.At(t, target)) {
			trcore::Sample s;
			if (!a.track.At(t, s))
				return 0;   // postaci jeszcze nie bylo - nie wiadomo, co wtedy wiedziala
			target.clear();
		}
		std::vector<int> drop = trcore::NewsToRemove(target, now);
		int removed = 0;
		for (int i : drop) {
			oCNews* n = (oCNews*)now[i].news;
			int owners = 0;
			for (TrActor* o : RecorderActors())
				for (auto it = o->npc->knownNews.iknow.next; it; it = it->next)
					owners += it->data == n;
			if (owners > 1) {
				shared++;
				continue;
			}
			a.npc->knownNews.Remove(n);
			removed++;
		}
		return removed;
	}

	void TrApplyCombat(TrActor& a, float t, const TrCombatCommit& c) {
		ZPROF_SCOPE("Relations.Apply");
		oCNpc* npc = a.npc;
		const RelDefs& d = Defs();
		const trcore::CombatPlan& p = c.plan;
		if (p.attitude) {
			npc->SetAttitude(c.target.attitude);       // jak Npc_SetAttitude (takze bit stanu w 0x75C)
			npc->SetTmpAttitude(c.target.tmpAttitude);
			npc->attTimer = c.target.attTimer;
		}
		if (p.npcType)
			npc->npcType = c.target.npcType;
		for (int i : p.aivars)
			if (i < (int)d.aivs.size())
				npc->aiscriptvars[d.aivs[i].index] = c.target.aiv[i];
		std::string enemyBefore = trcfg.debug ? EnemyName(npc->enemy) : "";   // przed SetEnemy (Release starego)
		if (p.enemy || p.resetAI)
			npc->SetEnemy(ValidEnemy(c.target));   // SetEnemy (0x734BC0): Release starego, AddRef nowego
		int newsShared = 0;
		int news = trcfg.rewindScope == 1 ? RevertNews(a, t, newsShared) : 0;
		if (!p.Any() && !news)
			return;
		relCount.npcs++;
		relCount.resetAI += p.resetAI;
		relCount.attitude += p.attitude;
		relCount.enemy += p.enemy;
		relCount.npcType += p.npcType;
		relCount.aivars += (int)p.aivars.size();
		relCount.news += news;
		relCount.newsShared += newsShared;
		if (!trcfg.debug)
			return;
		std::string aivs;
		for (int i : p.aivars)
			if (i < (int)d.aivs.size())
				aivs += " " + (d.aivs[i].name.compare(0, 4, "AIV_") == 0 ? d.aivs[i].name.substr(4) : d.aivs[i].name) + " " +
					std::to_string(c.now.aiv[i]) + "->" + std::to_string(c.target.aiv[i]);
		ZDebug("Relacje: %s (%s)%s | postawa %s/%s -> %s/%s | wrog %s -> %s | ZS %s%s -> %s | typ %d -> %d | aivary %d:%s | newsy usuniete %d%s",
			npc->GetInstanceName().ToChar(), npc->GetName(0).ToChar(), p.resetAI ? " AI OD NOWA" : "",
			trcore::AttitudeName(c.now.attitude), trcore::AttitudeName(c.now.tmpAttitude),
			trcore::AttitudeName(npc->attitude), trcore::AttitudeName(npc->tmp_attitude),
			enemyBefore.c_str(), EnemyName(npc->enemy).c_str(),
			StateName(c.now.state).c_str(), c.nowHostile ? " (walka)" : "", StateName(c.target.state).c_str(),
			c.now.npcType, npc->npcType, (int)p.aivars.size(), aivs.c_str(), news,
			newsShared ? (" (wspoldzielone pominiete " + std::to_string(newsShared) + ")").c_str() : "");
	}

	void RelationsCommitBegin() {
		relCount = RelCounters();
	}

	void RelationsCommitEnd() {
		if (!trcfg.restoreCombat)
			return;
		const RelCounters& c = relCount;
		ZLog("Relacje: przywrocone postacie %d (AI od nowa %d, postawa %d, wrog %d, typ %d, aivary %d, newsy %d%s)",
			c.npcs, c.resetAI, c.attitude, c.enemy, c.npcType, c.aivars, c.news,
			c.newsShared ? (", wspoldzielone pominiete " + std::to_string(c.newsShared)).c_str() : "");
	}

	void TrCaptureGuilds(std::vector<unsigned char>& out) {
		out.clear();
		oCGuilds* g = ogame ? ogame->GetGuilds() : nullptr;
		if (!g || !g->attitude || g->size <= 0 || g->size > 256)
			return;
		out.assign(g->attitude, g->attitude + g->size * g->size);
	}

	int TrApplyGuilds(const std::vector<unsigned char>& in) {
		oCGuilds* g = ogame ? ogame->GetGuilds() : nullptr;
		if (!g || !g->attitude || in.empty() || (size_t)g->size * g->size != in.size())
			return 0;
		int changed = 0;
		for (size_t i = 0; i < in.size(); i++)
			if (g->attitude[i] != in[i]) {
				g->attitude[i] = in[i];
				changed++;
			}
		return changed;
	}

	void RelationsRegisterCommands() {
		TrConsoleRegister("COMBAT", "tr combat - relacje i stan walki postaci z fokusu (zapis cofania)", [](const std::vector<std::string>&) {
			oCNpc* t = TrConsoleTarget();
			if (!t)
				return zSTRING("combat: brak celu");
			const RelDefs& d = Defs();
			trcore::CombatSnap c;
			CaptureCombat(t, d, c);
			std::string aivs;
			for (int i = 0; i < c.nAiv; i++)
				if (c.aiv[i])
					aivs += " " + d.aivs[i].name + "=" + std::to_string(c.aiv[i]);
			int news = 0;
			for (auto it = t->knownNews.iknow.next; it; it = it->next)
				news += it->data != nullptr;
			size_t entries = 0;
			for (TrActor* a : RecorderActors())
				if (a->npc == t)
					entries = a->combat.Size();
			char buf[256];
			sprintf_s(buf, "combat: %s postawa %s/%s wrog %s ZS %s%s typ %d newsy %d wpisy %u |", t->GetInstanceName().ToChar(),
				trcore::AttitudeName(c.attitude), trcore::AttitudeName(c.tmpAttitude),
				EnemyName(t->enemy).c_str(), StateName(c.state).c_str(), IsHostileState(d, c.state) ? " (walka)" : "",
				c.npcType, news, (unsigned)entries);
			ZLog("%s%s", buf, aivs.c_str());
			return zSTRING(buf) + zSTRING(aivs.c_str());
		});
	}
}
