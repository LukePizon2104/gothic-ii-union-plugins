// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// TYMCZASOWE (Task 4) - usuwane w Task 10.
	struct SpikeFrame { float x, y, z, yaw; int ani; float frame; };
	static oCNpc* spikeNpc = nullptr;
	static std::vector<SpikeFrame> spikeFrames;
	static float spikeRecLeft = 0.0f, spikePlayPos = -1.0f, spikeSavedScale = 1.0f;
	static int spikeMode = 1;

	static void SpikeSetTarget(oCNpc* npc) {
		if (spikeNpc)
			spikeNpc->Release();
		spikeNpc = npc;
		if (spikeNpc)
			spikeNpc->AddRef();
	}

	void SpikeUpdate(float dt) {
		if (!spikeNpc)
			return;
		zCModel* m = spikeNpc->GetModel();
		if (!m)
			return;
		if (spikeRecLeft > 0.0f) {
			zVEC3 p = spikeNpc->GetPositionWorld();
			zVEC3 at = spikeNpc->GetAtVectorWorld();
			SpikeFrame f;
			f.x = p.n[0]; f.y = p.n[1]; f.z = p.n[2];
			f.yaw = atan2f(at.n[0], at.n[2]);
			f.ani = TrMainAni(m, f.frame);
			spikeFrames.push_back(f);
			spikeRecLeft -= dt;
			if (spikeRecLeft <= 0.0f) {
				// podsumowanie: kolejne animacje (nazwa x klatek) i przebyta droga
				std::string seq;
				int run = 0;
				for (size_t j = 0; j < spikeFrames.size(); j++) {
					run++;
					if (j + 1 == spikeFrames.size() || spikeFrames[j + 1].ani != spikeFrames[j].ani) {
						seq += " " + TrAniName(m, spikeFrames[j].ani) + "x" + std::to_string(run);
						run = 0;
					}
				}
				const SpikeFrame& a = spikeFrames.front();
				const SpikeFrame& b = spikeFrames.back();
				float dist = sqrtf((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) + (b.z - a.z) * (b.z - a.z));
				ZLog("Spike: nagrane %d klatek, droga %.0f cm, animacje:%s", (int)spikeFrames.size(), dist, seq.c_str());
			}
			return;
		}
		if (spikePlayPos >= 0.0f && !spikeFrames.empty()) {
			static float traceAcc = 0.0f;
			// odtwarzanie wstecz w 5 s czasu rzeczywistego
			bool wasPlaying = spikePlayPos < 1.0f;
			spikePlayPos += dt / 5.0f;
			float k = spikePlayPos > 1.0f ? 1.0f : spikePlayPos;
			int i = (int)((1.0f - k) * (spikeFrames.size() - 1));
			const SpikeFrame& f = spikeFrames[i];
			TrSetPuppetPose(spikeNpc, f.x, f.y, f.z, f.yaw, f.ani, f.frame, spikeMode);
			if (wasPlaying && TrTraceDue(traceAcc, dt, 0.25f))
				ZLog("Spike: k %.2f cel %s f%.1f pos(%.0f %.0f %.0f)%s | %s", k, TrAniName(m, f.ani).c_str(), f.frame,
					f.x, f.y, f.z, spikeNpc == player ? (" kamera " + std::to_string((int)TrCameraDistance()) + " cm").c_str() : "",
					TrDescribeNpc(spikeNpc).c_str());
			if (spikePlayPos >= 1.0f)
				spikePlayPos = 1.0f;   // stoi na pierwszej klatce do "tr spike end"
		}
	}

	void SpikeRegister() {
		TrConsoleRegister("SPIKE", "tr spike rec [hero] | play 0/1/2 | end | kill | revive", [](const std::vector<std::string>& a) {
			std::string cmd = a.size() > 1 ? a[1] : "";
			if (cmd == "REC") {
				oCNpc* t = (a.size() > 2 && a[2] == "HERO") ? player : TrConsoleTarget();
				if (!t)
					return zSTRING("spike: brak celu");
				SpikeSetTarget(t);
				spikeFrames.clear();
				spikeRecLeft = 5.0f;
				spikePlayPos = -1.0f;
				return zSTRING("spike: nagrywam 5 s ") + t->GetInstanceName();
			}
			if (cmd == "PLAY" && spikeNpc && !spikeFrames.empty()) {
				spikeMode = a.size() > 2 ? atoi(a[2].c_str()) : 1;
				zCModel* m = spikeNpc->GetModel();
				// drugie "play" bez "end" nie moze zapamietac juz wyzerowanego czasu modelu (postac zostawala zamrozona)
				if (spikePlayPos < 0.0f)
					spikeSavedScale = m && m->timeScale > 0.0f ? m->timeScale : 1.0f;
				if (m)
					m->timeScale = spikeMode != 2 ? 0.0f : spikeSavedScale;
				ZLog("Spike: przed play | %s", TrDescribeNpc(spikeNpc).c_str());
				spikeNpc->SetCollDet(FALSE);
				trFrozen = true;
				spikePlayPos = 0.0f;
				trPuppetAniSwitches = 0;
				ZLog("Spike: play tryb %d, %s, klatek %d", spikeMode, spikeNpc->GetInstanceName().ToChar(), (int)spikeFrames.size());
				return zSTRING("spike: zamrozone, odtwarzam wstecz, tryb ") + zSTRING(spikeMode);
			}
			if (cmd == "END") {
				ZLog("Spike: end, przelaczen animacji %d", trPuppetAniSwitches);
				trFrozen = false;
				spikePlayPos = -1.0f;
				if (spikeNpc) {
					if (zCModel* m = spikeNpc->GetModel())
						m->timeScale = spikeSavedScale;
					zVEC3 p = spikeNpc->GetPositionWorld();
					spikeNpc->ResetPos(p);
					if (spikeNpc != player)
						spikeNpc->state.ClearAIState();
					ZLog("Spike: po end | %s", TrDescribeNpc(spikeNpc).c_str());
				}
				return zSTRING("spike: odmrozone");
			}
			if (cmd == "KILL") {
				oCNpc* t = TrConsoleTarget();
				if (!t || t->IsDead())
					return zSTRING("spike: brak zywego celu");
				SpikeSetTarget(t);
				t->attribute[NPC_ATR_HITPOINTS] = 0;
				t->DoDie(nullptr);
				return zSTRING("spike: zabity ") + t->GetInstanceName();
			}
			if (cmd == "REVIVE") {
				// cel z fokusu / najblizszy (takze martwy); wczesniej brany byl NPC z "rec" - nie ten zabity
				oCNpc* t = TrConsoleTarget();
				if (!t)
					t = spikeNpc;
				if (!t)
					return zSTRING("spike: brak celu");
				bool wasDead = t->IsDead() != 0;
				int bsBefore = t->GetBodyState();
				t->attribute[NPC_ATR_HITPOINTS] = t->attribute[NPC_ATR_HITPOINTSMAX];
				zVEC3 p = t->GetPositionWorld();
				t->ResetPos(p);
				t->SetCollDetStat(TRUE);
				t->SetCollDetDyn(TRUE);
				t->state.ClearAIState();
				ZLog("Spike: revive %s, martwy przed %d, stan ciala przed %d po %d, IsDead po %d, HP %d",
					t->GetInstanceName().ToChar(), wasDead, bsBefore, t->GetBodyState(), t->IsDead(),
					t->attribute[NPC_ATR_HITPOINTS]);
				return zSTRING("spike: wskrzeszony ") + t->GetInstanceName()
					+ zSTRING(t->IsDead() ? " (nadal IsDead!)" : " (zyje)");
			}
			return zSTRING("spike: rec [hero] | play 0/1/2 | end | kill | revive");
		});
	}
}
