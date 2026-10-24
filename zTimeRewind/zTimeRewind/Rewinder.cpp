// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct Puppet {
		oCNpc* npc;
		float timeScale;   // przy starcie cofania
		float written;     // nasz wpis (0 = zamrozenie; PuppetAnims=2 = timeScale, bez wpisu)
		bool collStat, collDyn;
	};
	// Tempo bohatera wokol cofania (log rozstrzygajacy zgloszenie "po cofnieciu postac przyspieszona" 23.10).
	static float heroScaleBegin = -1.0f, heroWatchLeft = -1.0f, heroScaleCommit = -1.0f;

	static float HeroScaleNow() {
		zCModel* m = player && player->homeWorld ? zDYNAMIC_CAST<zCModel>(player->GetVisual()) : nullptr;
		return m ? m->timeScale : -1.0f;
	}

	enum RewindMode { MODE_RUNE = 0, MODE_CONSOLE = 1, MODE_AUTO = 2 };
	static bool active = false, requested = false, consoleMode = false;   // consoleMode: bez trzymania akcji (konsola, auto)
	static int requestedMode = MODE_RUNE;
	static float requestedSeconds = 0.0f;
	static trcore::RewindCursor cursor;
	static std::vector<Puppet> puppets;

	bool RewindActive() {
		return active;
	}

	void RewindRequest() {
		requested = true;
		requestedMode = MODE_RUNE;
	}

	void RewindRequestAuto(float seconds) {
		requested = true;
		requestedMode = MODE_AUTO;
		requestedSeconds = seconds;
	}

	bool RewindPending() {
		return requested || active;
	}

	bool RewindCanStart(std::string& why) {
		why.clear();
		if (!player || !ogame || active || requested)
			return false;
		if (player->IsDead() || player->IsUnconscious())
			return false;
		if (!oCInformationManager::GetInformationManager().HasFinished() || player->IsInGlobalCutscene()) {
			why = "Nie w trakcie rozmowy.";
			return false;
		}
		if (player->GetInteractMob()) {
			why = "Najpierw odejdź od tego miejsca.";
			return false;
		}
		int bs = player->GetBodyState();
		if (bs == BS_SWIM || bs == BS_DIVE || (player->GetAnictrl() && player->GetAnictrl()->GetWaterLevel() > 1)) {
			why = "Piasek nie przesypie się w wodzie.";
			return false;
		}
		if (player->guild > 16) {   // GIL_SEPERATOR_HUM - bohater przemieniony w zwierze
			why = "Nie w tej postaci.";
			return false;
		}
		if (RecorderSeconds() < 1.0f) {
			why = "Za mało czasu, by go cofnąć.";
			return false;
		}
		if (player->attribute[NPC_ATR_MANA] < trcfg.manaStart) {
			why = "Za mało many.";
			return false;
		}
		return true;
	}

	// Lecace strzaly i pociski czarow znikaja w chwili rzucenia runy.
	static void RemoveProjectiles() {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world)
			return;
		std::vector<zCVob*> arrows;
		std::vector<oCVisualFX*> fxs;
		for (int i = 0; i < world->activeVobList.GetNum(); i++) {
			zCVob* vob = world->activeVobList[i];
			if (!vob)
				continue;
			if (auto ai = vob->callback_ai ? zDYNAMIC_CAST<oCAIArrowBase>(vob->callback_ai) : nullptr) {
				if (!ai->collisionOccured && !ai->hasHit)   // tylko w locie - lezace zostaja do podniesienia
					arrows.push_back(vob);
			}
			else if (auto fx = zDYNAMIC_CAST<oCVisualFX>(vob))
				if (zDYNAMIC_CAST<oCNpc>(fx->origin) && _stricmp(fx->emTrjMode_S.ToChar(), "FIXED") != 0 && !fx->IsFinished())
					fxs.push_back(fx);
		}
		for (zCVob* v : arrows)
			if (v->homeWorld) {
				ConsumeArrowRemoved(v);   // strzala bohatera wystrzelona po celu wraca przy Commit
				v->homeWorld->RemoveVob(v);
			}
		for (oCVisualFX* fx : fxs)
			fx->Stop(TRUE);
		ZDebug("Rewind: usuniete strzaly %d, pociski %d", (int)arrows.size(), (int)fxs.size());
	}

	// mode: runa (trzymanie akcji), konsola (bez many), auto przy smierci (samo, z mana)
	static void Begin(int mode, float seconds) {
		ZPROF_SCOPE("Rewind.Begin");
		trcore::CursorParams p;
		p.speed = trcfg.rewindSpeed;
		p.manaPerSecond = mode == MODE_CONSOLE ? 0.0f : trcfg.manaPerSecond;
		p.minSeconds = mode == MODE_RUNE ? trcfg.minRewindSeconds : seconds;
		cursor.Begin(TrNow(), RecorderOldest(), p);
		consoleMode = mode != MODE_RUNE;
		if (consoleMode)
			cursor.Release();   // jedzie samo do minSeconds i staje
		ConsumeRewindBegin();
		RemoveProjectiles();
		puppets.clear();
		heroScaleBegin = -1.0f;
		heroWatchLeft = -1.0f;
		for (TrActor* a : RecorderActors()) {
			oCNpc* npc = a->npc;
			TrCutsBegin(*a);   // podglad odciec zDismember: stan na starcie (takze wyzerowanie poza swiatem)
			// najpierw swiat: GetModel postaci zdespawnowanej buduje jej zwolniony model od nowa (UnShrink, 10-20 ms)
			if (!npc->homeWorld)
				continue;
			zCModel* m = npc->GetModel();
			if (!m)
				continue;
			float written = trcfg.puppetAnims != 2 ? 0.0f : m->timeScale;   // 0 = zastygla poza, 1 = klatki przy zatrzymanym czasie
			puppets.push_back({ npc, m->timeScale, written, npc->collDetectionStatic != 0, npc->collDetectionDynamic != 0 });
			if (npc == player)
				heroScaleBegin = m->timeScale;
			m->timeScale = written;
			npc->SetCollDet(FALSE);
		}
		trFrozen = true;
		active = true;
		trPuppetAniSwitches = 0;
		FxBegin();
		ZLog("Rewind: start t=%.2f, bufor %.1f s, marionetki %d, %s, kamera %.0f cm, tempo bohatera x%.2f, factorMotion %.2f", TrNow(),
			RecorderSeconds(), (int)puppets.size(), mode == MODE_CONSOLE ? "konsola" : mode == MODE_AUTO ? "auto przy smierci" : "runa",
			TrCameraDistance(), heroScaleBegin, ztimer ? ztimer->factorMotion : 0.0f);
		ZDebug("Rewind: bohater na starcie | %s", TrDescribeNpc(player).c_str());
	}

	static void ApplyFrame(float t) {
		ZPROF_SCOPE("Rewind.Frame");   // pozy marionetek w klatce przewijania (Freeze.Pose w srodku)
		for (TrActor* a : RecorderActors()) {
			if (!a->npc->homeWorld || a->track.Empty())
				continue;
			trcore::Sample s;
			if (!a->track.At(t, s))
				s = a->track.First();   // pojawil sie pozniej - stoi tam, gdzie sie pojawil
			TrSetPuppetPose(a->npc, s.x, s.y, s.z, s.yaw, s.aniId, s.frame, trcfg.puppetAnims);
			if (a->cutView.watch)
				TrCutsPreview(*a, t, s);   // czesci ciala wracaja, gdy kursor minie chwile odciecia
		}
		float ft = 0.0f;
		if (RecorderClock().At(t, ft))
			if (oCWorldTimer* timer = ogame->GetWorldTimer())
				timer->SetFullTime(ft);
	}

	static void RestorePuppets() {
		// takze postacie zdespawnowane w trakcie: spawn manager wstawi ten sam obiekt pozniej (i zapisze go w save)
		int foreign = 0;
		for (Puppet& p : puppets) {
			if (zCModel* m = p.npc->GetModel()) {
				// tylko nasz czynnik: wartosc wpisana w trakcie przez inny modul zostaje (jego), nasze 0 -> tempo ze startu
				float cur = m->timeScale;
				float to = trcore::PuppetScaleAfter(p.timeScale, p.written, cur);
				bool theirs = std::fabs(cur - p.written) > 1e-6f;
				if (theirs)
					foreign++;
				else
					m->timeScale = to;
				if (p.npc == player) {
					heroScaleCommit = to;
					heroWatchLeft = 1.0f;   // RewindUpdate: wpis kontrolny sekunde po oddaniu
					ZLog("Rewind: tempo bohatera - start x%.2f, przy oddaniu x%.2f -> x%.2f (%s), factorMotion %.2f",
						p.timeScale, cur, to, theirs ? "wpis innego modulu w trakcie - zostaje" : "nasze zamrozenie zdjete",
						ztimer ? ztimer->factorMotion : 0.0f);
				}
			}
			if (p.npc->homeWorld) {
				p.npc->SetCollDetStat(p.collStat);
				p.npc->SetCollDetDyn(p.collDyn);
			}
			else {
				p.npc->collDetectionStatic = p.collStat;
				p.npc->collDetectionDynamic = p.collDyn;
			}
		}
		if (foreign)
			ZLog("Rewind: tempo %d marionetek wpisane w trakcie przez inny modul - zostaje ich wartosc", foreign);
		puppets.clear();
	}

	// Twarde ustawienie: stawia na nogi (takze martwego), czysci kolejke i animacje, AI od nowa.
	static void HardSet(oCNpc* npc, const trcore::Sample& s, bool isHero) {
		int mana = npc->attribute[NPC_ATR_MANA];
		zVEC3 pos(s.x, s.y, s.z);
		npc->ResetPos(pos);
		// ResetPos (0x682553) wpisuje HP = HPMAX i MANA = MANAMAX - HP z probki, mana bez zmian (spec)
		npc->attribute[NPC_ATR_HITPOINTS] = s.hp > 0 ? s.hp : 1;
		npc->attribute[NPC_ATR_MANA] = mana;
		npc->SetHeadingAtWorld(zVEC3(sinf(s.yaw), 0.0f, cosf(s.yaw)));
		npc->SetCollDetStat(TRUE);
		npc->SetCollDetDyn(TRUE);
		if ((int)s.weapon != npc->GetWeaponMode())
			npc->SetWeaponMode2(s.weapon);
		if (!isHero)
			npc->state.ClearAIState();   // DoAIState sam wznawia rutyne (ActivateRtnState) - potwierdzone w spike
	}

	static void CommitNpc(TrActor& a, float t) {
		oCNpc* npc = a.npc;
		bool isHero = npc == player;
		trcore::Sample target, latest;
		if (!a.track.At(t, target))
			target = a.track.First();
		a.track.At(a.track.LastTime(), latest);
		bool deadNow = npc->IsDead() != 0, knockedNow = npc->IsUnconscious() != 0;
		trcore::CommitAction action = trcore::DecideCommit(target, latest, deadNow, knockedNow, isHero, trcfg.hardResetDistance);
		// relacje i walka: postac walczaca teraz, a wtedy nie - AI od nowa (ResetPos w punkcie docelowym + rutyna)
		TrCombatCommit combat;
		bool hasCombat = !isHero && TrPlanCombat(a, t, combat);
		if (hasCombat)
			action = trcore::CombatCommitAction(action, combat.plan.resetAI);
		ZDebug("Commit: %s %s | cel pos(%.0f %.0f %.0f) hp %d zycie %d ani %s | przed pos(%.0f %.0f %.0f) hp %d zycie %d | teraz martwy %d nokaut %d",
			npc->GetInstanceName().ToChar(),
			action == trcore::CommitAction::HardTarget ? "TWARDO" : action == trcore::CommitAction::SoftLatest ? "miekko" : "bez zmian",
			target.x, target.y, target.z, target.hp, target.life, TrAniName(npc->GetModel(), target.aniId).c_str(),
			latest.x, latest.y, latest.z, latest.hp, latest.life, deadNow, knockedNow);
		// odciecia zDismember przed ustawieniem: zdjecie nakladek ofiary zatrzymuje animacje, a ResetPos startuje postawe
		TrCutsCommit(a, t, target, action, deadNow, knockedNow);
		switch (action) {
		case trcore::CommitAction::HardTarget:
			HardSet(npc, target, isHero);
			break;
		case trcore::CommitAction::SoftLatest:
			// AI sie nie cofa - poza wraca do tej sprzed cofniecia, zeby pasowala do stanu AI (np. siedzi na lawce)
			TrSetPuppetPose(npc, latest.x, latest.y, latest.z, latest.yaw, latest.aniId, latest.frame, trcfg.puppetAnims);
			break;
		case trcore::CommitAction::Keep:
			break;   // martwy / nieprzytomny wtedy i teraz - animacja upadku leci dalej od klatki marionetki
		}
		if (hasCombat)
			TrApplyCombat(a, t, combat);   // postawa, wrog, aivary, newsy z chwili docelowej
	}

	static void Commit() {
		ZPROF_SCOPE("Rewind.Commit");   // ustawienie swiata w punkcie cofniecia (StateSnap.Apply w srodku)
		float t = cursor.Time();
		int stateIdx = -1;
		if (trcfg.rewindScope == 1) {
			stateIdx = RecorderStateAtOrBefore(t);
			if (stateIdx >= 0)
				t = RecorderStateTime(stateIdx);   // pozycje i stan z tej samej chwili
			else
				ZLog("Rewind: brak migawki stanu <= %.2f - tylko swiat fizyczny", t);
		}
		RestorePuppets();
		RelationsCommitBegin();
		TrCutsCommitBegin();
		for (TrActor* a : RecorderActors())
			if (a->npc->homeWorld && !a->track.Empty())
				CommitNpc(*a, t);
		RelationsCommitEnd();
		TrCutsCommitEnd();
		float ft = 0.0f;
		if (RecorderClock().At(t, ft))
			if (oCWorldTimer* timer = ogame->GetWorldTimer())
				timer->SetFullTime(ft);
		MobsRevertAfter(t);
		if (stateIdx >= 0)
			TrApplyState(RecorderState(stateIdx));
		ConsumeRestoreAfter(t);   // zwoje / mikstury / jedzenie / strzaly zuzyte po t wracaja (przed RecorderCutAfter)
		float rewound = TrNow() - t;
		RecorderCutAfter(t);
		trFrozen = false;
		active = false;
		consoleMode = false;
		FxEnd();
		ZLog("Rewind: commit t=%.2f, cofnieto %.2f s, przelaczen animacji %d, powod %d (1 puszczenie, 2 koniec bufora, 3 mana), stan %d",
			t, rewound, trPuppetAniSwitches, (int)cursor.Reason(), stateIdx);
		ZDebug("Rewind: bohater po commit | %s", TrDescribeNpc(player).c_str());
	}

	void RewindUpdate(float realDt) {
		if (heroWatchLeft > 0.0f && !active) {
			heroWatchLeft -= realDt;
			if (heroWatchLeft <= 0.0f) {
				float now = HeroScaleNow();
				ZLog("Rewind: tempo bohatera 1 s po oddaniu x%.2f (oddane x%.2f, przed cofaniem x%.2f), factorMotion %.2f%s",
					now, heroScaleCommit, heroScaleBegin, ztimer ? ztimer->factorMotion : 0.0f,
					now > 0.0f && std::fabs(now - heroScaleCommit) > 0.02f ? " - zmienione przez inny modul (NB co klatke / zDismember)" : "");
			}
		}
		if (requested && !active) {
			requested = false;
			Begin(requestedMode, requestedSeconds);
		}
		if (!active)
			return;
		if (!consoleMode && zinput->GetState(GAME_ACTION) <= 0.0f)
			cursor.Release();
		int mana = player ? player->attribute[NPC_ATR_MANA] : 0;
		int pay = cursor.Step(realDt, mana);
		if (player && pay > 0)
			player->attribute[NPC_ATR_MANA] = mana - pay;
		ApplyFrame(cursor.Time());
		FxUpdate(cursor.Rewound(), RecorderSeconds());
		static float traceAcc = 0.0f;
		if (trcfg.debug && TrTraceDue(traceAcc, realDt, 0.5f))
			ZDebug("Rewind: t %.2f cofnieto %.2f mana %d przelaczen %d kamera %.0f cm | %s", cursor.Time(), cursor.Rewound(),
				player ? player->attribute[NPC_ATR_MANA] : -1, trPuppetAniSwitches, TrCameraDistance(), TrDescribeNpc(player).c_str());
		if (cursor.Stopped())
			Commit();
	}

	void RewindFinishNow() {
		requested = false;
		if (active)
			Commit();
	}

	void RewindAbort() {
		requested = false;
		if (!active)
			return;
		RestorePuppets();   // bohater przechodzi do nowego swiata - nie moze zostac z zatrzymanym modelem
		trFrozen = false;
		active = false;
		consoleMode = false;
		FxEnd();
		ZLog("Rewind: przerwane (wczytanie / zmiana swiata)");
	}

	void RewindConsole(float seconds) {
		if (!active)
			Begin(MODE_CONSOLE, seconds);
	}
}
