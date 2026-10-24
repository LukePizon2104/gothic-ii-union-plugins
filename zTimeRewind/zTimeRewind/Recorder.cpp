// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static std::unordered_map<oCNpc*, std::unique_ptr<TrActor>> actors;
	static trcore::ClockTrack worldClock;
	static float now = 0.0f, tickAcc = 0.0f;
	static trcore::Keyframes<TrStateSnap> states;   // etap 2: migawki stanu skryptow (bufory z puli, bez alokacji)
	static float stateAcc = 0.0f;
	static const int STATE_RELIST_CHUNK = 16384;    // symboli parsera na tick przy budowie listy zmiennych (~0,7 ms)

	// Ostatni tick probkowania postaci - tr stats i wpisy o wolnym ticku w logu.
	struct SnapStats {
		int listed = 0;     // postacie na voblist_npcs
		int inWorld = 0;
		int outside = 0;    // zdespawnowane (homeWorld 0) - bez probki i bez odczytu modelu
		int noModel = 0;    // w swiecie, ale bez modelu (sama pozycja)
		int added = 0;      // nowe tory
		double ms = 0.0;
	};
	static SnapStats lastSnap;
	static bool snapLogged = false;                 // pierwszy tick po wczytaniu -> linia w logu
	static int slowSnaps = 0, snapsSinceSlowLog = 1000;
	static const double SLOW_SNAP_MS = 2.0;         // wolniejszy tick probkowania -> wpis (najwyzej co 50 tickow)

	float TrNow() {
		return now;
	}

	static void Forget(TrActor& a) {
		if (a.npc)
			a.npc->Release();
		a.npc = nullptr;
	}

	void RecorderReset() {
		for (auto& kv : actors)
			Forget(*kv.second);
		actors.clear();
		worldClock = trcore::ClockTrack();
		tickAcc = 0.0f;
		states.Clear();
		stateAcc = 0.0f;
		lastSnap = SnapStats();
		snapLogged = false;
		MobsReset();
		ConsumeReset();
	}

	// Model postaci bez skutkow ubocznych. oCNpc::GetModel (0x738720) dla postaci bez modelu (silnik zwalnia go
	// zdespawnowanym - oCNpc::Shrink z zCCacheAsk::PurgeCache 0x760750) wola zCCache<oCNpc*, oCShrinkHelper>[npc]
	// (0x7608E0) -> oCNpc::UnShrink (0x72CBA0), czyli buduje caly model od nowa: 10-20 ms na postac. To byly
	// przycinki Recorder.Snap (co kilkanascie s na kazda zdespawnowana postac) i 1,6 s w pierwszym ticku po wczytaniu.
	static zCModel* LoadedModel(oCNpc* npc) {
		return zDYNAMIC_CAST<zCModel>(npc->GetVisual());
	}

	bool TrSampleNpc(oCNpc* npc, float t, trcore::Sample& s, bool* hasModel) {
		zVEC3 p = npc->GetPositionWorld();
		// postac wstawiona przez skrypt (Wld_InsertNpc) przez chwile stoi w (0,0,0), zanim trafi na miejsce -
		// taka probka jako "miejsce pojawienia sie" wysylala ja przy cofnieciu do srodka mapy
		if (p.n[0] == 0.0f && p.n[1] == 0.0f && p.n[2] == 0.0f)
			return false;
		zCModel* m = LoadedModel(npc);
		if (hasModel)
			*hasModel = m != nullptr;
		zVEC3 at = npc->GetAtVectorWorld();
		s.t = t;
		s.x = p.n[0];
		s.y = p.n[1];
		s.z = p.n[2];
		s.yaw = atan2f(at.n[0], at.n[2]);
		s.frame = 0.0f;
		s.aniId = m ? TrMainAni(m, s.frame) : -1;   // bez modelu sama pozycja (marionetka bez klatki animacji)
		s.hp = npc->attribute[NPC_ATR_HITPOINTS];
		s.life = npc->IsDead() ? trcore::LIFE_DEAD : npc->IsUnconscious() ? trcore::LIFE_UNCONSCIOUS : trcore::LIFE_ALIVE;
		s.weapon = (unsigned char)npc->GetWeaponMode();
		return true;
	}

	static std::string DescribeSnap(const SnapStats& st) {
		char buf[200];
		sprintf_s(buf, "%.2f ms, lista NPC %d: w swiecie %d (bez modelu %d), poza swiatem %d, nowe tory %d",
			st.ms, st.listed, st.inWorld, st.noModel, st.outside, st.added);
		return buf;
	}

	// Probka postaci z listy NPC swiata. Zdespawnowane (oCWorld::DisableVob zostawia je na voblist_npcs) bez
	// probkowania i bez odczytu modelu - trcore::DecideSample; nowe tory tylko dla postaci w swiecie.
	static void SampleNpcs(oCWorld* world) {
		static LARGE_INTEGER freq = {};
		if (!freq.QuadPart)
			QueryPerformanceFrequency(&freq);
		LARGE_INTEGER a, b;
		QueryPerformanceCounter(&a);
		SnapStats st;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc)
				continue;
			st.listed++;
			bool inWorld = npc->homeWorld != nullptr;
			if (inWorld)
				st.inWorld++;
			else
				st.outside++;
			auto found = actors.find(npc);
			TrActor* actor = found != actors.end() ? found->second.get() : nullptr;
			trcore::SampleAction action = trcore::DecideSample(inWorld, actor != nullptr);
			if (action == trcore::SampleAction::Skip)
				continue;
			if (action == trcore::SampleAction::Extend) {
				actor->track.Extend(now);
				continue;
			}
			trcore::Sample s;
			bool hasModel = false;
			if (!TrSampleNpc(npc, now, s, &hasModel))
				continue;
			if (!hasModel)
				st.noModel++;
			if (!actor) {
				std::unique_ptr<TrActor>& slot = actors[npc];
				slot = std::make_unique<TrActor>();
				slot->npc = npc;
				npc->AddRef();
				actor = slot.get();
				st.added++;
			}
			actor->track.Push(s);
			TrRecordCombat(*actor, now);
			TrRecordCuts(*actor, now);   // maska odciec zDismember (bez zDismember nic)
		}
		QueryPerformanceCounter(&b);
		st.ms = (b.QuadPart - a.QuadPart) * 1000.0 / freq.QuadPart;
		lastSnap = st;
		if (!snapLogged) {
			snapLogged = true;
			ZLog("Recorder: pierwsza probka - %s", DescribeSnap(st).c_str());
		}
		snapsSinceSlowLog++;
		if (st.ms > SLOW_SNAP_MS) {
			slowSnaps++;
			if (snapsSinceSlowLog >= 50) {
				ZLog("Recorder: wolna probka (%d od ostatniego wpisu) - %s", slowSnaps, DescribeSnap(st).c_str());
				slowSnaps = 0;
				snapsSinceSlowLog = 0;
			}
		}
	}

	// Migawka stanu skryptow co StateInterval - bez dialogu / cutscenki (stan nieciagly). Lista zmiennych zapisu
	// (po wczytaniu gry przeglad ~89 tys. symboli) budowana porcjami w kolejnych tickach; do tego czasu migawka
	// czeka. Bufory migawki z puli (Keyframes::Acquire) - bez alokacji w stalym rytmie.
	static void StateTick() {
		stateAcc += trcfg.sampleInterval;
		if (stateAcc < trcfg.stateInterval)
			return;
		if (!oCInformationManager::GetInformationManager().HasFinished() || player->IsInGlobalCutscene())
			return;
		if (!TrStatePrepare(STATE_RELIST_CHUNK))
			return;   // lista jeszcze niegotowa - nastepna porcja w kolejnym ticku
		stateAcc = 0.0f;
		TrStateSnap snap = states.Acquire();
		if (TrCaptureState(snap))
			states.Push(now, std::move(snap));
		else
			states.Release(std::move(snap));
	}

	void RecorderUpdate(float dt) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs || !player)
			return;
		now += dt;
		static float statsAcc = 0.0f;
		if (trcfg.debug && TrTraceDue(statsAcc, dt, 10.0f))
			ZDebug("%s", RecorderStats().c_str());
		tickAcc += dt;
		if (tickAcc < trcfg.sampleInterval)
			return;
		tickAcc = 0.0f;
		{
			ZPROF_SCOPE("Recorder.Snap");   // probka postaci w swiecie i zegara swiata co SampleInterval
			SampleNpcs(world);
			ConsumeSample(now);   // liczniki ekwipunku bohatera (limit zwrotu zuzytych przedmiotow)
			if (oCWorldTimer* timer = ogame->GetWorldTimer())
				worldClock.Push(now, timer->GetFullTime());
		}
		if (trcfg.rewindScope == 1)
			StateTick();   // sekcje StateSnap.* (czas reszty - w sekcji Recorder)
		ZPROF_SCOPE("Recorder.Trim");   // obcinanie bufora do BufferSeconds
		float oldest = now - trcfg.bufferSeconds;
		worldClock.TrimBefore(oldest);
		states.TrimBefore(oldest);
		MobsTrim(oldest);
		ConsumeTrim(oldest);
		for (auto it = actors.begin(); it != actors.end();) {
			TrActor& a = *it->second;
			a.track.TrimBefore(oldest);
			a.combat.TrimBefore(oldest);
			a.news.TrimBefore(oldest);
			a.cuts.TrimBefore(oldest);
			if (a.track.Empty() || a.track.LastTime() < oldest) {
				Forget(a);
				it = actors.erase(it);
			}
			else
				++it;
		}
	}

	float RecorderOldest() {
		if (worldClock.Empty())
			return now;
		float lo = now - trcfg.bufferSeconds;
		return worldClock.FirstTime() > lo ? worldClock.FirstTime() : lo;
	}

	float RecorderSeconds() {
		return now - RecorderOldest();
	}

	std::vector<TrActor*> RecorderActors() {
		std::vector<TrActor*> out;
		out.reserve(actors.size());
		for (auto& kv : actors)
			out.push_back(kv.second.get());
		return out;
	}

	trcore::ClockTrack& RecorderClock() {
		return worldClock;
	}

	void RecorderCutAfter(float t) {
		for (auto it = actors.begin(); it != actors.end();) {
			it->second->track.CutAfter(t);
			it->second->combat.CutAfter(t);
			it->second->news.CutAfter(t);
			it->second->cuts.CutAfter(t);
			if (it->second->track.Empty()) {
				Forget(*it->second);
				it = actors.erase(it);
			}
			else
				++it;
		}
		worldClock.CutAfter(t);
		states.CutAfter(t);
		ConsumeCutAfter(t);
		now = t;
		tickAcc = 0.0f;
	}

	int RecorderStateAtOrBefore(float t) {
		return states.FindAtOrBefore(t);
	}

	float RecorderStateTime(int i) {
		return states.TimeAt(i);
	}

	const TrStateSnap& RecorderState(int i) {
		return states.At(i);
	}

	std::string RecorderStats() {
		size_t samples = 0, bytes = 0;
		for (auto& kv : actors) {
			samples += kv.second->track.Size();
			bytes += kv.second->track.Bytes();
		}
		char buf[200];
		sprintf_s(buf, "zTimeRewind: aktorzy %d, probki %d, pamiec %.1f KB, bufor %.1f s, migawki %d (pula %d) | probka ",
			(int)actors.size(), (int)samples, bytes / 1024.0, RecorderSeconds(), (int)states.Size(), (int)states.PoolSize());
		return buf + DescribeSnap(lastSnap);
	}
}
