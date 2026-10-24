// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Przyczyna (pkt 25, 22.10): Commit przywracal pozycje, HP i stan, a odciecia zDismember (sciskanie kosci, odlamki,
	// kikuty, ragdoll) zostawaly - wskrzeszony NPC chodzil bez glowy. Maska byla tylko w pamieci zDismember (w NB i
	// Kronikach AivarIndex=-1), a na Steamie w aivarze, ktorego zTimeRewind nie przywraca.
	struct CutsCommitStats {
		int calls = 0, restored = 0, revived = 0;
	};
	static CutsCommitStats cutsCommit;

	static const char* NpcName(oCNpc* npc) {
		return npc ? npc->GetInstanceName().ToChar() : "?";
	}

	void TrCutsInit() {
		bool joined = zcut::Join(zcut::Open(), "zTimeRewind");
		const zcut::Table* t = zcut::Linked();
		ZLog("Odciecia: kanal zCutLink v%u %s; zDismember %s", zcut::LINK_VERSION,
			joined ? "otwarty" : "NIE (inny uklad tablicy albo brak pamieci nazwanej - bez masek odciec)",
			t ? "polaczony (maski odciec w migawkach postaci)" : "jeszcze nie (sprawdzane przy kazdym uzyciu)");
	}

	void TrCutsLogState() {
		static int last = -1;
		int now = zcut::Linked() ? 1 : 0;
		if (now == last)
			return;
		last = now;
		const zcut::Table* t = zcut::Linked();
		if (t)
			ZLog("Odciecia: polaczono z %s - maski odciec w migawkach, po cofnieciu czesci ciala wracaja", t->provider);
		else
			ZLog("Odciecia: brak zDismember - bez masek odciec (cofanie jak dotad)");
	}

	void TrRecordCuts(TrActor& a, float t) {
		const zcut::Table* link = zcut::Linked();
		if (!link || !a.npc)
			return;
		uint32_t m = link->getMask(a.npc) & trcore::CUT_MASK_BITS;
		bool first = a.cuts.Empty();
		uint32_t prev = first ? 0u : a.cuts.Last();
		a.cuts.Push(t, m);   // rowna ostatniej - bez wpisu
		if (first ? m != 0 : m != prev)
			ZLog("Odciecia: migawka %s maska %03X -> %03X (t=%.2f%s)", NpcName(a.npc), prev, m, t,
				first ? ", pierwsza probka postaci" : "");
	}

	void TrCutsBegin(TrActor& a) {
		a.cutView = trcore::CutView();
		const zcut::Table* link = zcut::Linked();
		if (!link || !a.npc || !a.npc->homeWorld || a.track.Empty())
			return;
		uint32_t m = link->getMask(a.npc);
		a.cutView = trcore::CutViewBegin(m, a.npc->IsDead() != 0, a.npc->IsUnconscious() != 0, trcore::CutsTrackHasCut(a.cuts));
	}

	// Maska z toru w chwili t: przed pierwszym wpisem (postac pojawila sie pozniej) - pierwszy wpis, jak tor pozycji.
	static uint32_t CutsAt(const TrActor& a, float t, uint32_t fallback) {
		if (a.cuts.Empty())
			return fallback;
		uint32_t m = 0;
		if (!a.cuts.At(t, m))
			m = a.cuts.First();
		return m & trcore::CUT_MASK_BITS;
	}

	void TrCutsPreview(TrActor& a, float t, const trcore::Sample& s) {
		if (!a.cutView.watch || !a.npc->homeWorld)
			return;
		const zcut::Table* link = zcut::Linked();
		if (!link)
			return;
		trcore::Sample latest;
		if (!a.track.At(a.track.LastTime(), latest))
			return;
		trcore::CommitAction act = trcore::DecideCommit(s, latest, a.cutView.dead, a.cutView.knocked, a.npc == player,
			trcfg.hardResetDistance);
		uint32_t before = a.cutView.shown;
		bool wasAlive = a.cutView.alive;
		trcore::CutCall c = trcore::CutPreviewStep(a.cutView, act, s.life, CutsAt(a, t, a.cutView.start));
		if (!c.send)
			return;
		uint32_t done = link->setMask(a.npc, c.mask, c.flags, "zTimeRewind podglad");
		char what[96];
		ZLog("Odciecia: podglad %s maska %03X -> %03X%s (t=%.2f, zycie wtedy %d) - zDismember: %s", NpcName(a.npc), before, c.mask,
			a.cutView.alive && !wasAlive ? ", kursor przed smiercia" : "", t, (int)s.life, zcut::DoneText(done, what, sizeof(what)));
	}

	void TrCutsCommit(TrActor& a, float t, const trcore::Sample& target, trcore::CommitAction action, bool deadNow, bool knockedNow) {
		const zcut::Table* link = zcut::Linked();
		if (!link || !a.npc)
			return;
		uint32_t live = link->getMask(a.npc) & trcore::CUT_MASK_BITS;
		uint32_t start = a.cutView.watch ? a.cutView.start : live;   // maska sprzed przewijania (podglad mogl ja zmienic)
		uint32_t tm = CutsAt(a, t, start);
		trcore::CutCall c = trcore::CutCommitCall(action, target.life, tm, start, live, deadNow, knockedNow);
		if (!c.send)
			return;
		uint32_t done = link->setMask(a.npc, c.mask, c.flags, "zTimeRewind commit");
		cutsCommit.calls++;
		if (c.mask != live)
			cutsCommit.restored++;
		if (c.flags & trcore::CUT_FLAG_REVIVE)
			cutsCommit.revived++;
		char what[96];
		if (c.mask != live || (done & ~zcut::DONE_NOMODEL))
			ZLog("Odciecia: przywrocenie %s maska %03X -> %03X (t=%.2f, %s, zycie wtedy %d, teraz %s) - zDismember: %s", NpcName(a.npc),
				live, c.mask, t, action == trcore::CommitAction::HardTarget ? "TWARDO" : action == trcore::CommitAction::Keep ? "bez zmian" : "miekko",
				(int)target.life, deadNow ? "martwy" : knockedNow ? "nieprzytomny" : "zywy", zcut::DoneText(done, what, sizeof(what)));
		else
			ZDebug("Odciecia: %s wstaje - maska %03X bez zmian, zDismember: %s", NpcName(a.npc), live, zcut::DoneText(done, what, sizeof(what)));
	}

	void TrCutsCommitBegin() {
		cutsCommit = CutsCommitStats();
	}

	void TrCutsCommitEnd() {
		if (cutsCommit.calls)
			ZLog("Odciecia: commit - wywolan zDismember %d (maska zmieniona %d, wstajacych %d)", cutsCommit.calls,
				cutsCommit.restored, cutsCommit.revived);
	}
}
