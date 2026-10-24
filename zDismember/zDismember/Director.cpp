// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Align: ustawienie; Play: animacja i ciecie; Settle: sterowanie juz u gracza, postawa walki wchodzi przejsciem
	// z pozy finishera, a nakladka finishera schodzi dopiero, gdy jego animacja wygasnie.
	enum class DirPhase { Idle, Align, Play, Settle };

	struct DirSequence {
		DirPhase phase = DirPhase::Idle;
		oCNpc* victim = nullptr;                     // AddRef (do oddania sterowania)
		const zdcore::FinisherDef* def = nullptr;
		uint32_t pose = 0;
		std::string overlay;                         // nakladka bohatera z .MDS (zwykla albo w miejscu)
		zVEC3 dir, from, to;
		float alignT = 0.0f, t = 0.0f;
		int heroAni = -1;
		zCModelAni* heroProto = nullptr;             // animacja bohatera z nakladki finishera
		float releaseTime = 0.0f;                    // czas animacji bohatera, w ktorym sterowanie wraca do gracza
		float settleT = 0.0f;
		int weaponMode = 0;
		bool cut = false, slow = false;
		float slowFactor = 1.0f;                     // tempo gry w oknie slowmo katalogu (FinisherSlowMo; 1 = bez spowolnienia)
		zVEC3 heroStart;                             // finisher parowany: poczatek torow
		float heroYOff = 0.0f, victimYOff = 0.0f;    // wysokosc voba nad ziemia
		float heroYaw0 = 0.0f, victimYaw0 = 0.0f;    // kierunki na starcie - obrot rozlozony na faze ustawiania
	};

	// Para W3 po oddaniu sterowania: ofiara pada dalej po swoim torze (bohater juz stoi, ofiara jeszcze 0,5-1,2 s leci).
	struct VictimTail {
		oCNpc* npc = nullptr;                        // AddRef
		const zdcore::FinisherDef* def = nullptr;
		zVEC3 origin, dir;
		float yOff = 0.0f, t = 0.0f;
	};

	static DirSequence seq;
	static VictimTail tail;
	static oCNpc* frozenVictim = nullptr;
	static std::vector<oCNpc*> staggered;   // AddRef; walczacy w poblizu, zamrozeni do konca finishera

	static bool IsFrozen(oCNpc* npc) {
		if (frozenVictim && npc == frozenVictim)
			return true;
		return std::find(staggered.begin(), staggered.end(), npc) != staggered.end();
	}
	static const float ALIGN_SECONDS = 0.25f;
	static const float SETTLE_MAX_SECONDS = 1.0f;   // awaryjnie: nakladka schodzi, nawet gdy animacja finishera nie wygasla
	static const int PLACE_TRACE = zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
		zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_VOB_IGNORE_PROJECTILES;

	// ---------- Nakladki bez zatrzymywania animacji ----------
	// zCModel::ApplyModelProtoOverlay (nowa nakladka, 0x578933) i RemoveModelProtoOverlay (0x578DB5) zatrzymuja WSZYSTKIE
	// animacje modelu (warstwy 0..99999), a animacja startowana na pustym wezle wchodzi od razu z waga 1 (AddNodeAni
	// 0x57EB37) - postac przeskakuje albo zamiera, dopoki cos nie wystartuje. Migawka kanalow (klatka, zdarzenia, ruch
	// korzenia, przejscia) i wag na wezlach przed zmiana nakladek i wznowienie tych samych zCModelAni po niej.
	struct AniChanSnap {
		zCModelAni* ani;
		bool fromRemoved;                            // animacja ze zdejmowanej nakladki - nie wraca
		int layer;
		zCModelAni* nextAni;
		int nextId;                                  // gdy nextAni ze zdejmowanej nakladki - wersja z wierzchu
		zCModelAni* nextAniOverride;
		int nextOverrideId;
		int advanceDir;
		float actFrame;
		int actAniEvent;
		float combAniX, combAniY;
		int isFadingOut, isFirstTime;
		float blendInOverride, blendOutOverride;
		zVEC3 lastPos, thisPos;
		zCQuat lastRotQuat, thisRotQuat, freezeRotQuat;
		int rotFirstTime;
		float transWeight, randAniTimer;
		zCModelAniActive* old;
	};
	struct AniNodeSnap {
		int node, chan;
		float weight, weightSpeed;
		int blendState;
		bool master;
	};
	struct AniSnapshot {
		std::vector<AniChanSnap> chans;
		std::vector<AniNodeSnap> nodes;
		std::vector<float> masterSpeed;              // zCModelNodeInst::masterAniSpeed wg wezla
	};

	static zCModelPrototype* FindOverlayProto(zCModel* m, const std::string& mds) {
		std::string name = zdcore::Upper(mds);
		size_t dot = name.find('.');
		if (dot != std::string::npos)
			name = name.substr(0, dot);
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p)
				continue;
			std::string n = zdcore::Upper(p->modelProtoName.ToChar());
			size_t d = n.find('.');
			if ((d == std::string::npos ? n : n.substr(0, d)) == name)
				return p;
		}
		return nullptr;
	}

	// Animacja pochodzi z tej nakladki (prototyp trzyma swoje animacje wg ID - jak GetAniFromAniID 0x472F50).
	static bool AniOfProto(zCModelPrototype* proto, zCModelAni* ani) {
		return proto && ani && ani->aniID >= 0 && ani->aniID < proto->protoAnis.GetNum() && proto->protoAnis[ani->aniID] == ani;
	}

	static void TakeAniSnapshot(zCModel* m, zCModelPrototype* removing, AniSnapshot& s) {
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni)
				continue;
			AniChanSnap c;
			c.ani = a->protoAni;
			c.fromRemoved = AniOfProto(removing, a->protoAni);
			c.layer = a->protoAni->layer;
			c.nextAni = a->nextAni;
			c.nextId = a->nextAni ? a->nextAni->aniID : -1;
			c.nextAniOverride = a->nextAniOverride;
			c.nextOverrideId = a->nextAniOverride ? a->nextAniOverride->aniID : -1;
			if (AniOfProto(removing, c.nextAni))
				c.nextAni = nullptr;
			if (AniOfProto(removing, c.nextAniOverride))
				c.nextAniOverride = nullptr;
			c.advanceDir = a->advanceDir;
			c.actFrame = a->actFrame;
			c.actAniEvent = a->actAniEvent;
			c.combAniX = a->combAniX;
			c.combAniY = a->combAniY;
			c.isFadingOut = a->isFadingOut;
			c.isFirstTime = a->isFirstTime;
			c.blendInOverride = a->blendInOverride;
			c.blendOutOverride = a->blendOutOverride;
			c.lastPos = a->lastPos;
			c.thisPos = a->thisPos;
			c.lastRotQuat = a->lastRotQuat;
			c.thisRotQuat = a->thisRotQuat;
			c.freezeRotQuat = a->freezeRotQuat;
			c.rotFirstTime = a->rotFirstTime;
			c.transWeight = a->transWeight;
			c.randAniTimer = a->randAniTimer;
			c.old = a;
			s.chans.push_back(c);
		}
		s.masterSpeed.assign(m->nodeList.GetNum(), 0.0f);
		for (int n = 0; n < m->nodeList.GetNum(); n++) {
			zCModelNodeInst* node = m->nodeList[n];
			if (!node)
				continue;
			s.masterSpeed[n] = node->masterAniSpeed;
			for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++) {
				auto& e = node->nodeAniList[k];
				for (int c = 0; c < (int)s.chans.size(); c++)
					if (s.chans[c].old == e.modelAni) {
						s.nodes.push_back({ n, c, e.weight, e.weightSpeed, e.blendState, node->masterAni == k });
						break;
					}
			}
		}
	}

	static void RestoreAniSnapshot(zCModel* m, const AniSnapshot& s) {
		// rosnaco po warstwach (inaczej nizsza warstwa dostaje na wezlach wage 0), w warstwie najpierw wygasajace
		std::vector<int> order;
		for (int c = 0; c < (int)s.chans.size(); c++)
			if (!s.chans[c].fromRemoved)
				order.push_back(c);
		std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
			if (s.chans[a].layer != s.chans[b].layer)
				return s.chans[a].layer < s.chans[b].layer;
			return s.chans[a].isFadingOut > s.chans[b].isFadingOut;
		});
		for (int c : order) {
			// animacja z flaga END_SYNC przy innej w tej samej warstwie bylaby tylko zakolejkowana (StartAni 0x57B371)
			zCModelAni* ani = s.chans[c].ani;
			byte endSync = ani->aniFlags.flagEndSync;
			ani->aniFlags.flagEndSync = 0;
			m->StartAni(ani, 0);
			ani->aniFlags.flagEndSync = endSync;
			// wygasajaca musi byc oznaczona przed startem nastepnej w tej warstwie (FadeOutAni jej nie ruszy)
			if (s.chans[c].isFadingOut)
				if (zCModelAniActive* a = m->GetActiveAni(s.chans[c].ani))
					a->isFadingOut = TRUE;
		}
		// stan kanalow - po wszystkich startach (start kolejnej w warstwie zmienia blendOutOverride poprzedniej)
		std::vector<zCModelAniActive*> now(s.chans.size(), nullptr);
		for (int c : order) {
			const AniChanSnap& sc = s.chans[c];
			zCModelAniActive* a = m->GetActiveAni(sc.ani);
			if (!a)
				continue;
			now[c] = a;
			a->nextAni = sc.nextAni ? sc.nextAni : (sc.nextId >= 0 ? m->GetAniFromAniID(sc.nextId) : nullptr);
			a->nextAniOverride = sc.nextAniOverride ? sc.nextAniOverride : (sc.nextOverrideId >= 0 ? m->GetAniFromAniID(sc.nextOverrideId) : nullptr);
			a->advanceDir = sc.advanceDir;
			a->actFrame = sc.actFrame;
			a->actAniEvent = sc.actAniEvent;
			a->combAniX = sc.combAniX;
			a->combAniY = sc.combAniY;
			a->isFadingOut = sc.isFadingOut;
			a->isFirstTime = sc.isFirstTime;
			a->blendInOverride = sc.blendInOverride;
			a->blendOutOverride = sc.blendOutOverride;
			a->lastPos = sc.lastPos;
			a->thisPos = sc.thisPos;
			a->lastRotQuat = sc.lastRotQuat;
			a->thisRotQuat = sc.thisRotQuat;
			a->freezeRotQuat = sc.freezeRotQuat;
			a->rotFirstTime = sc.rotFirstTime;
			a->transWeight = sc.transWeight;
			a->randAniTimer = sc.randAniTimer;
		}
		// wagi przejsc na wezlach
		for (auto& w : s.nodes) {
			zCModelAniActive* a = now[w.chan];
			if (!a || w.node >= m->nodeList.GetNum())
				continue;
			zCModelNodeInst* node = m->nodeList[w.node];
			if (!node)
				continue;
			for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++) {
				auto& e = node->nodeAniList[k];
				if (e.modelAni != a)
					continue;
				e.weight = w.weight;
				e.weightSpeed = w.weightSpeed;
				e.blendState = w.blendState;
				if (w.master) {
					node->masterAni = k;
					node->masterAniSpeed = s.masterSpeed[w.node];
				}
				break;
			}
		}
	}

	bool ZApplyOverlayKeepAnis(zCModel* m, const std::string& mds) {
		if (!m)
			return false;
		AniSnapshot s;
		TakeAniSnapshot(m, nullptr, s);
		int ok = ZApplyOverlayTimed(m, mds, m == (player ? player->GetModel() : nullptr) ? "ruch bohatera" : "postac");
		// juz zalozona albo blad - silnik nic nie zatrzymal
		if (m->numActiveAnis == 0 && !s.chans.empty())
			RestoreAniSnapshot(m, s);
		return ok != 0;
	}

	void ZRemoveOverlayKeepAnis(zCModel* m, const std::string& mds) {
		if (!m)
			return;
		zCModelPrototype* proto = FindOverlayProto(m, mds);
		if (!proto) {
			ZRemoveOverlayTimed(m, mds);   // nazwa nie do poznania - jak dotad (silnik szuka po swojemu)
			return;
		}
		AniSnapshot s;
		TakeAniSnapshot(m, proto, s);
		ZRemoveOverlayTimed(m, mds);   // zwalnia prototyp (gdy nikt inny go nie trzyma) - po tym tylko migawka
		if (m->numActiveAnis == 0 && !s.chans.empty())
			RestoreAniSnapshot(m, s);
	}

	// Przejscie silnika z animacji 'out' (wygasa) w 'in' (wchodzi) trwa blendOut / blendIn z MDS (0,1 s) - ustawiamy je
	// na wezlach (CalcWeights 0x57F470: waga += predkosc * czas, przy wygasaniu predkosc ujemna).
	static void SetTransitionSeconds(zCModel* m, zCModelAniActive* in, zCModelAniActive* out, float sec) {
		if (!m || sec <= 0.0f)
			return;
		float speed = 1.0f / sec;
		if (in)
			in->blendInOverride = speed;
		if (out)
			out->blendOutOverride = -speed;
		for (int i = 0; i < m->nodeList.GetNum(); i++) {
			zCModelNodeInst* node = m->nodeList[i];
			if (!node)
				continue;
			for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++) {
				auto& e = node->nodeAniList[k];
				if (in && e.modelAni == in && e.blendState == zCModelNodeInst::zMDL_BLEND_STATE_FADEIN && e.weightSpeed > 0.0f)
					e.weightSpeed = speed;
				else if (out && e.modelAni == out && e.blendState == zCModelNodeInst::zMDL_BLEND_STATE_FADEOUT && e.weightSpeed < 0.0f)
					e.weightSpeed = -speed;
			}
		}
	}

	// Zamrozenie AI ofiary (skryptowy stan); animacje i ruch z animacji dzialaja dalej.
	HOOK Ivk_oCNpc_States_DoAIState AS(&oCNpc_States::DoAIState, &oCNpc_States::DoAIState_Union);
	int oCNpc_States::DoAIState_Union() {
		ZHookHit(ZH_DOAISTATE);
		if (npc && IsFrozen(npc))
			return 0;
		return THISCALL(Ivk_oCNpc_States_DoAIState)();
	}

	// Rezyser trzyma model bohatera (takze w Settle - nakladka finishera jeszcze na wierzchu).
	bool DirectorActive() {
		return seq.phase != DirPhase::Idle;
	}

	static bool DirectorPlaying() {
		return seq.phase == DirPhase::Align || seq.phase == DirPhase::Play;
	}

	bool DirectorIsVictim(oCNpc* npc) {
		return DirectorPlaying() && npc && npc == seq.victim;
	}

	static float YawOf(const zVEC3& v) {
		return atan2f(v.n[0], v.n[2]);
	}

	static void SetYaw(oCNpc* npc, float yaw) {
		npc->SetHeadingAtWorld(zVEC3(sinf(yaw), 0.0f, cosf(yaw)));
	}

	static bool GroundAt(const zVEC3& p, float& y) {
		oCWorld* world = ogame->GetGameWorld();
		zVEC3 from(p.n[0], p.n[1] + 150.0f, p.n[2]);
		if (!world->TraceRayNearestHit(from, zVEC3(0.0f, -400.0f, 0.0f), (zCVob*)nullptr, PLACE_TRACE) || !world->traceRayReport.foundHit)
			return false;
		y = world->traceRayReport.foundIntersection.n[1];
		return true;
	}

	static bool PathClear(const zVEC3& a, const zVEC3& b) {
		oCWorld* world = ogame->GetGameWorld();
		zVEC3 from(a.n[0], a.n[1] + 40.0f, a.n[2]);
		zVEC3 ray(b.n[0] - a.n[0], b.n[1] - a.n[1], b.n[2] - a.n[2]);
		return !world->TraceRayNearestHit(from, ray, (zCVob*)nullptr, PLACE_TRACE) || !world->traceRayReport.foundHit;
	}

	// Miejsce ofiary: punkt z definicji w ukladzie bohatera (x w prawo, z do przodu), na ziemi. Bohater obraca sie tak,
	// zeby ten punkt wypadl na linii do ofiary - ofiara przesuwa sie tylko o roznice odleglosci (bez skoku w bok).
	static bool Place(const zdcore::FinisherDef* def, bool inplace, oCNpc* victim, zVEC3& outTo, zVEC3& outDir) {
		zVEC3 hp = player->GetPositionWorld(), vp = victim->GetPositionWorld();
		const float* off = inplace ? def->inplaceOffset : def->offset;
		float dx = vp.n[0] - hp.n[0], dz = vp.n[2] - hp.n[2], l = sqrtf(dx * dx + dz * dz);
		float angV;
		if (l > 1.0f)
			angV = atan2f(dx, dz);
		else {
			zVEC3 at = player->GetAtVectorWorld();
			angV = atan2f(at.n[0], at.n[2]);
		}
		float angO = (fabsf(off[0]) + fabsf(off[2]) > 1.0f) ? atan2f(off[0], off[2]) : 0.0f;
		float heading = angV - angO;
		zVEC3 dir(sinf(heading), 0.0f, cosf(heading));
		zVEC3 right(dir.n[2], 0.0f, -dir.n[0]);
		zVEC3 to(hp.n[0] + right.n[0] * off[0] + dir.n[0] * off[2], vp.n[1], hp.n[2] + right.n[2] * off[0] + dir.n[2] * off[2]);
		float gHero, gVictim, gTarget;
		if (!GroundAt(hp, gHero) || !GroundAt(vp, gVictim) || !GroundAt(to, gTarget))
			return false;
		if (fabsf(gTarget - gHero) > 60.0f)
			return false;
		to.n[1] = gTarget + (vp.n[1] - gVictim);
		if (!PathClear(hp, to))
			return false;
		outTo = to;
		outDir = dir;
		return true;
	}

	// Czas animacji bohatera z aktywnego kanalu (klatka / fps) - zgodny ze zwolnionym tempem.
	static bool HeroAniTime(zCModel* model, const std::string& name, float& time) {
		for (int i = 0; i < model->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = model->aniChannels[i];
			if (!a || !a->protoAni || _stricmp(a->protoAni->aniName.ToChar(), name.c_str()) != 0)
				continue;
			float fps = a->protoAni->fpsRate > 0.0f ? a->protoAni->fpsRate : 25.0f;
			time = a->actFrame / fps;
			return true;
		}
		return false;
	}

	// Kiedy oddac sterowanie: poczatek koncowego bezruchu bohatera (finishery W3 stoja 1-2 s, az ofiara upadnie),
	// bez bezruchu - tuz przed koncem, tak zeby przejscie w postawe zmiescilo sie w animacji. Ruch liczony z probek
	// animacji (najwiekszy obrot wezla miedzy klatkami).
	static float ComputeReleaseTime(zCModelAni* ani, const zdcore::FinisherDef* d) {
		zdcore::ReleaseParams p;
		p.minTime = d->cutTime + (d->slowmo < 1.0f ? std::max(0.0f, d->slowTo) : 0.0f) + 0.1f;
		p.blendSec = zcfg.finisherBlend + 0.05f;   // zapas: finisher nie dochodzi do konca w trakcie przejscia
		if (!zcfg.finisherEarlyRelease)
			p.stillDegPerSec = 0.0f;               // bez szukania bezruchu - tuz przed koncem animacji
		int frames = ani ? (int)ani->numFrames : 0, nodes = ani ? (int)ani->numNodes : 0;
		if (frames < 2 || nodes < 1)
			return p.minTime;
		p.fps = ani->fpsRate > 0.0f ? ani->fpsRate : 25.0f;
		std::vector<float> motion(frames - 1, 0.0f);
		std::vector<zCQuat> prev(nodes), cur(nodes);
		for (int n = 0; n < nodes; n++)
			prev[n] = ani->GetQuat(0, n);
		for (int f = 1; f < frames; f++) {
			float mx = 0.0f;
			for (int n = 0; n < nodes; n++) {
				cur[n] = ani->GetQuat(f, n);
				mx = std::max(mx, zdcore::QuatAngleDeg(prev[n].q, cur[n].q));
			}
			motion[f - 1] = mx;
			prev.swap(cur);
		}
		return zdcore::ReleaseTime(motion, p);
	}

	// Pozycje do diagnozy ustawiania i ruchu z animacji (Debug=1).
	static void LogPositions(const char* when) {
		if (!zcfg.debug || !player || !seq.victim)
			return;
		zVEC3 h = player->GetPositionWorld(), v = seq.victim->GetPositionWorld();
		float dx = v.n[0] - h.n[0], dz = v.n[2] - h.n[2];
		ZDebug("Finisher %s [%s]: bohater (%.0f %.0f %.0f) ofiara (%.0f %.0f %.0f) cel (%.0f %.0f %.0f) odleglosc %.0f",
			seq.def ? seq.def->id.c_str() : "?", when, h.n[0], h.n[1], h.n[2], v.n[0], v.n[1], v.n[2],
			seq.to.n[0], seq.to.n[1], seq.to.n[2], sqrtf(dx * dx + dz * dz));
	}

	// Para W3: vob po torze z animacji (animacje sa w miejscu; ruch z animacji gasi blokada). Tor w ukladzie bohatera
	// na starcie: origin = jego pozycja, dir = jego przod.
	static void SetOnTrajectory(oCNpc* npc, const std::vector<float>& traj, float fps, float t, const zVEC3& origin,
		const zVEC3& dir, float yOff) {
		float x, z;
		zdcore::SampleTraj(traj, fps, t, x, z);
		zVEC3 right(dir.n[2], 0.0f, -dir.n[0]);
		zVEC3 p(origin.n[0] + right.n[0] * x + dir.n[0] * z, npc->GetPositionWorld().n[1],
			origin.n[2] + right.n[2] * x + dir.n[2] * z);
		float g;
		if (GroundAt(p, g))
			p.n[1] = g + yOff;
		npc->SetPositionWorld(p);
	}

	static void FollowTrajectories(oCNpc* victim, float t) {
		SetOnTrajectory(player, seq.def->heroTraj, seq.def->trajFps, t, seq.heroStart, seq.dir, seq.heroYOff);
		SetOnTrajectory(victim, seq.def->victimTraj, seq.def->trajFps, t, seq.heroStart, seq.dir, seq.victimYOff);
	}

	static void EndVictimTail() {
		if (tail.npc)
			tail.npc->Release();
		tail = VictimTail();
	}

	static void VictimTailUpdate(float dt) {
		if (!tail.npc)
			return;
		tail.t += dt;
		float duration = tail.def->victimTraj.size() >= 2 ? (tail.def->victimTraj.size() / 2 - 1) / tail.def->trajFps : 0.0f;
		if (!tail.npc->homeWorld || !tail.npc->GetModel() || tail.t > duration + 0.1f) {
			EndVictimTail();
			return;
		}
		SetOnTrajectory(tail.npc, tail.def->victimTraj, tail.def->trajFps, tail.t, tail.origin, tail.dir, tail.yOff);
	}

	// Walczacy w poblizu (ludzie z dobyta bronia, potwory) zataczaja sie i czekaja, az finisher sie skonczy.
	static void StaggerNearby() {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (zcfg.staggerRadius <= 0.0f || !world || !world->voblist_npcs)
			return;
		zVEC3 hp = player->GetPositionWorld();
		float r2 = zcfg.staggerRadius * zcfg.staggerRadius;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || npc == seq.victim || npc->IsDead() || npc->IsUnconscious() || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 p = npc->GetPositionWorld();
			float dx = p.n[0] - hp.n[0], dz = p.n[2] - hp.n[2];
			if (dx * dx + dz * dz > r2 || (IsHumanGuild(npc) && npc->GetWeaponMode() == NPC_WEAPON_NONE))
				continue;
			npc->AddRef();
			staggered.push_back(npc);
			if (zCEventManager* em = npc->GetEM(FALSE))
				em->Clear();
			zCModel* m = npc->GetModel();
			const char* ani = (rand() & 1) ? "T_STUMBLE" : "T_STUMBLEB";
			if (m->GetAniIDFromAniName(ani) >= 0)
				m->StartAni(ani, 0);
		}
		ZDebug("Finisher: zataczaja sie %d NPC", (int)staggered.size());
	}

	// Spowolnienie tego finishera (FinisherSlowMo, FinisherSlowMoLastEnemy): inny walczacy wrog (ofiara pominieta) szukany
	// w promieniu SLOWMO_FOE_RADIUS przed zataczaniem.
	static const float SLOWMO_FOE_RADIUS = 2000.0f;
	static float FinisherSlowNow() {
		oCNpc* foe = nullptr;
		float dist = 0.0f;
		if (zcfg.finisherSlowMoLastEnemy && zcfg.finisherSlowMo != zdcore::SLOWMO_OFF && seq.def->slowmo < 1.0f)
			foe = FindFightingFoe(SLOWMO_FOE_RADIUS, seq.victim, dist);
		float f = zdcore::FinisherSlowFactor(seq.def->slowmo, zcfg.finisherSlowMo, zcfg.finisherSlowMoLastEnemy, foe != nullptr);
		if (foe)
			ZDebug("Finisher %s: bez spowolnienia - walczy jeszcze %s (%.0f cm)", seq.def->id.c_str(), foe->GetName(0).ToChar(), dist);
		else if (seq.def->slowmo < 1.0f)
			ZDebug("Finisher %s: spowolnienie x%.2f (FinisherSlowMo=%d, katalog x%.2f)", seq.def->id.c_str(), f,
				zcfg.finisherSlowMo, seq.def->slowmo);
		return f;
	}

	static void ReleaseStaggered() {
		for (auto npc : staggered)
			npc->Release();
		staggered.clear();
	}

	// Smierc odroczona do ciecia jeszcze nie przeszla (ciala nie ma w stanie smierci). Nie IsDead(): od NB 22.10 ofiara
	// ma HP 0 juz przed cieciem (polkniete DoDie z HP 0), a IsDead() patrzy na HP - DoDie(bohater) bylo pomijane i
	// stan AI smierci (ZS_Dead: zadania, PD, B_CheckDeadMissionNPCs) nie ruszal (zgloszenie 23.10: Juan, siepacze).
	static bool DeathPending(oCNpc* v) {
		return !zdcore::IsRepeatDeath(v->GetBodyState());
	}

	static void DoCut() {
		seq.cut = true;
		LogPositions("ciecie");
		oCNpc* v = seq.victim;
		if (seq.def->cut != zdcore::Part::NoPart)
			CutPart(v, seq.def->cut, GibVelocity(seq.dir, (rand() & 1) ? 1 : -1));
		else if (zCModel* vm = v->GetModel()) {
			// pchniecie: rana bez odciecia
			int node = FindNode(vm, seq.def->wound.c_str());
			if (node >= 0) {
				// krew wychodzi z rany w kierunku pchniecia (od bohatera), nigdy w kamere
				BloodBurst(v->GetTrafoModelNodeToWorld(vm->nodeList[node]).GetTranslation(), 3.0f, &seq.dir);
				BloodStump(v, node, 2.5f);
			}
			BloodPool(v);
		}
		frozenVictim = nullptr;
		if (DeathPending(v)) {
			ZLog("Finisher %s: smierc %s przy cieciu - DoDie(bohater), HP przed %d", seq.def->id.c_str(),
				v->GetInstanceName().ToChar(), v->attribute[NPC_ATR_HITPOINTS]);
			// jak oryginalne dobicie (EV_AttackFinish 0x7521DA): HP=0, CheckModelOverlays, DoDie(napastnik)
			v->attribute[NPC_ATR_HITPOINTS] = 0;
			v->CheckModelOverlays();
			zdDeathPassThrough = v;
			v->DoDie(player);
			zdDeathPassThrough = nullptr;
			// Animacja smierci w klatce ciecia. DoDie (0x736760) jej nie startuje (tylko stan AI smierci), a pierwsza
			// polowa animacji ofiary (T_STAND_2_WOUNDED z nakladki, next '') konczy sie wlasnie w klatce ciecia - ofiara
			// stala sztywno w ostatniej pozie, az po ~1,5 s awaryjne DoDie(0) z oCAniCtrl_Human::IsDead (martwy stoi bez
			// T_DEAD/T_DEADB) przewracalo ja ragdollem. Para W3: T_DEAD z nakladki ofiary = druga polowa jej animacji.
			const char* deathAni = zdcore::FinisherDeathAni(*seq.def, seq.pose);
			zCModel* vm = v->GetModel();
			if (*deathAni && vm && vm->GetAniIDFromAniName(deathAni) >= 0) {
				vm->StartAni(deathAni, 0);
				ZDebug("Finisher %s: ofiara %s", seq.def->id.c_str(), deathAni);
			}
		}
		if (seq.pose == zdcore::POSE_DAZED)
			DazeRelease(v);
		ZDebug("Finisher %s: ciecie %s", seq.def->id.c_str(), zdcore::PartName(seq.def->cut));
	}

	// oCAniCtrl_Human::_Stand (0x6B7490 - przy blokadzie ruchu wolany co klatke) widzac bieg, przejscie w bieg, bieg
	// do tylu albo celowanie startuje postawe, ktora wygasilaby finisher - wznowione z migawki zatrzymujemy od razu.
	static void StopStandTriggers(zCModel* hm) {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!ac || !hm)
			return;
		int ids[] = { ac->_s_walkl, ac->_t_walk_2_walkl, ac->_s_walkbl, ac->_s_aim };
		for (int id : ids) {
			zCModelAni* a = id >= 0 ? hm->GetAniFromAniID(id) : nullptr;
			if (a && hm->IsAniActive(a))
				hm->StopAni(a);
		}
	}

	// Postawa walki tak, jak ja wybiera sterowanie (_s_walk: S_1HRUN / S_1HWALK ... dla broni i trybu chodu) - z nia
	// oCAniCtrl_Human::IsStateAniActive uznaje bohatera za stojacego i od razu przyjmuje ruch.
	static zCModelAni* HeroStanceAni(zCModel* hm) {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		if (ac && ac->_s_walk >= 0)
			if (zCModelAni* a = hm->GetAniFromAniID(ac->_s_walk))
				return a;
		int wm = AniSwapAniWeaponMode(player->GetWeaponMode());
		int id = hm->GetAniIDFromAniName(wm == NPC_WEAPON_2HS ? "S_2HRUN" : wm == NPC_WEAPON_1HS ? "S_1HRUN" : "S_RUN");
		return id >= 0 ? hm->GetAniFromAniID(id) : nullptr;
	}

	// Oddanie sterowania w tej klatce: postawa walki startuje, gdy animacja finishera jeszcze gra, wiec silnik przechodzi
	// z jego pozy plynnie (FinisherRecoverBlend), a blokada ruchu schodzi od razu. Nakladka finishera zostaje do Settle -
	// jej zdjecie teraz zatrzymaloby wszystkie animacje (to bylo zamrozenie po finiszerze). aniTime < 0: bez ogona ofiary.
	static void Release(float aniTime) {
		LogPositions("powrot");
		zCModel* hm = player ? player->GetModel() : nullptr;
		if (hm) {
			zCModelAniActive* fin = seq.heroProto ? hm->GetActiveAni(seq.heroProto) : nullptr;
			if (fin) {
				fin->nextAni = nullptr;   // koniec finishera nie startuje juz swojej nastepnej (postawa idzie od nas)
				fin->nextAniOverride = nullptr;
			}
			if (zCModelAni* stance = HeroStanceAni(hm)) {
				hm->StartAni(stance, 0);
				// postawa juz grala (StartAni nic nie zrobil) - finisher i tak ma wygasnac
				fin = seq.heroProto ? hm->GetActiveAni(seq.heroProto) : nullptr;
				if (fin && !fin->isFadingOut)
					hm->FadeOutAni(fin);
				if (fin)
					SetTransitionSeconds(hm, hm->GetActiveAni(stance), fin, zcfg.finisherBlend);
			}
		}
		if (player)
			player->SetMovLock(FALSE);
		if (seq.slow && ztimer)
			ztimer->SetMotionFactor(1.0f);
		seq.slow = false;
		frozenVictim = nullptr;
		ReleaseStaggered();
		if (seq.victim) {
			EndVictimTail();
			if (aniTime >= 0.0f && seq.def && seq.def->paired() && seq.victim->homeWorld) {
				tail.npc = seq.victim;   // referencja przechodzi na ogon
				tail.def = seq.def;
				tail.origin = seq.heroStart;
				tail.dir = seq.dir;
				tail.yOff = seq.victimYOff;
				tail.t = aniTime;
			}
			else
				seq.victim->Release();
			seq.victim = nullptr;
		}
		ZDebug("Finisher %s: sterowanie wraca (%.2f s animacji)", seq.def ? seq.def->id.c_str() : "?", aniTime);
		seq.phase = DirPhase::Settle;
		seq.settleT = 0.0f;
	}

	// Aktywne animacje modelu (nazwa:warstwa) - do logu przerwania.
	static std::string AnisDesc(zCModel* m) {
		std::string s;
		for (int i = 0; m && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni)
				continue;
			char buf[96];
			sprintf_s(buf, "%s%s:%d", s.empty() ? "" : " ", a->protoAni->aniName.ToChar(), a->protoAni->layer);
			s += buf;
		}
		return s.empty() ? "brak" : s;
	}

	// Animacje bohatera przerwala gra przed cieciem (21.10: skok gry z Alt, cios kombinacji - klawisze sa juz ukryte, to
	// zabezpieczenie na inne przyczyny): finisher anulowany - ofiara bez dalszej animacji pary i bez odciecia. Stojaca
	// (smierc odroczona, HP=1) umiera zwyklym zgonem gry (T_DEAD, bez ragdolla i odciecia), oszolomiona zostaje kleczaca.
	static void CancelBeforeCut(zCModel* hm) {
		oCNpc* v = seq.victim;
		ZLog("Finisher %s: anulowany przed cieciem po %.2f s - animacja bohatera przerwana (bohater: %s) - bez animacji ofiary "
			"i bez odciecia", seq.def->id.c_str(), seq.t, AnisDesc(hm).c_str());
		frozenVictim = nullptr;
		zCModel* vm = v->GetModel();
		if (vm && !seq.def->victimOverlay.empty())
			vm->RemoveModelProtoOverlay((seq.def->victimOverlay + ".MDS").c_str());   // zatrzymuje animacje ofiary - nowa nizej
		if ((seq.pose == zdcore::POSE_STANDING || v->attribute[NPC_ATR_HITPOINTS] <= 0) && DeathPending(v)) {
			v->attribute[NPC_ATR_HITPOINTS] = 0;
			v->CheckModelOverlays();
			zdDeathPassThrough = v;
			v->DoDie(player);
			zdDeathPassThrough = nullptr;
			if (vm && vm->GetAniIDFromAniName("T_DEAD") >= 0)
				vm->StartAni("T_DEAD", 0);
		}
		else if (vm && !v->IsDead()) {
			// oszolomiony: kleczenie z nakladki oszolomienia (S_WOUNDED) albo Axii (S_PRAY)
			const char* kneel = v->IsUnconscious() ? "S_WOUNDED" : "S_PRAY";
			if (vm->GetAniIDFromAniName(kneel) >= 0)
				vm->StartAni(kneel, 0);
		}
		seq.cut = true;   // bez ciecia w DirectorFinishNow
		Release(-1.0f);
	}

	// Animacja finishera wygasla: zdejmujemy jego nakladke bez zatrzymywania animacji (postawa / ruch gracza graja dalej).
	static void Settle() {
		zCModel* hm = player ? player->GetModel() : nullptr;
		if (hm && !seq.overlay.empty())
			ZRemoveOverlayKeepAnis(hm, seq.overlay);
		if (player)
			ResetWeaponGrip(player);
		seq = DirSequence();
	}

	bool DirectorTryStart(oCNpc* victim, uint32_t pose, const zdcore::FinisherDef* forced) {
		ZPROF_SCOPE("Director.Start");   // wybor i start finishera (z hakow smierci / dobicia / powalenia)
		if (seq.phase != DirPhase::Idle || MovesActive() || !zcfg.enabled || !player || !victim || victim == player || !ogame || !ogame->GetGameWorld())
			return false;
		zCModel* hm = player->GetModel();
		zCModel* vm = victim->GetModel();
		if (!hm || !vm || player->IsDead())
			return false;
		// finisher wg animacji broni (zamiana 1H <-> 2H: miecz 1H w stylu 2H - finishery 2H); ciecie z prawdziwej broni
		int wm = AniSwapAniWeaponMode(player->GetWeaponMode());
		int weapon = wm == NPC_WEAPON_1HS ? zdcore::WEAPON_1H : wm == NPC_WEAPON_2HS ? zdcore::WEAPON_2H : zdcore::WEAPON_NONE;
		// obuch: bez finisherow; rapier: tylko pchniecie (cut=NONE); konsola (forced) bez ograniczen
		zdcore::WeaponCut wcut = NpcWeaponCut(player);
		if (!forced) {
			if (weapon == zdcore::WEAPON_NONE || !zdcore::WeaponCanFinish(wcut) || !oCInformationManager::GetInformationManager().HasFinished())
				return false;
			if (vm->bbox3D.maxs.n[1] - vm->bbox3D.mins.n[1] > zcfg.maxVictimHeight)
				return false;
		}
		uint32_t vgroup = VictimGroup(victim);
		if (!forced && !vgroup)
			return false;

		// losowanie; finisher, ktory nie miesci sie w terenie, ustepuje kolejnemu (do 4 prob)
		const zdcore::FinisherDef* def = nullptr;
		std::vector<const zdcore::FinisherDef*> tried;
		if (!forced && wcut == zdcore::WeaponCut::ThrustOnly)
			for (auto& f : FinisherDefs())
				if (!zdcore::IsThrustFinisher(f))
					tried.push_back(&f);   // rapierem tylko pchniecie przez klatke
		zVEC3 to, dir;
		bool inplace = false;
		for (int attempt = 0; attempt < (forced ? 1 : 4); attempt++) {
			const zdcore::FinisherDef* d = forced ? forced : zdcore::PickFinisher(FinisherDefs(), weapon, vgroup, pose, ZRand32(), &tried);
			if (!d)
				break;
			if (Place(d, false, victim, to, dir)) {
				def = d;
				inplace = false;
				break;
			}
			if (!d->inplace.empty() && Place(d, true, victim, to, dir)) {
				def = d;
				inplace = true;
				break;
			}
			ZDebug("Finisher %s: brak miejsca", d->id.c_str());
			tried.push_back(d);
		}
		if (!def) {
			// np. rapier przy oszolomionym bez pchniec dla POSE_DAZED (brak W3_05_D/W3_06_D) - zostaje dobicie z gry
			ZDebug("Finisher: zaden nie pasuje (%s, bron %s, poza %s)", victim->GetInstanceName().ToChar(), zdcore::WeaponCutName(wcut),
				pose == zdcore::POSE_DAZED ? "DAZED" : pose == zdcore::POSE_LYING ? "LYING" : "STANDING");
			return false;
		}

		seq = DirSequence();
		seq.phase = DirPhase::Align;
		seq.victim = victim;
		victim->AddRef();
		seq.def = def;
		seq.pose = pose;
		seq.overlay = (inplace ? def->inplace : def->overlay) + ".MDS";
		seq.dir = dir;
		seq.from = victim->GetPositionWorld();
		seq.to = to;
		seq.weaponMode = wm;
		frozenVictim = victim;
		if (pose == zdcore::POSE_STANDING && victim->attribute[NPC_ATR_HITPOINTS] < 1)
			victim->attribute[NPC_ATR_HITPOINTS] = 1;   // smierc odroczona do klatki ciecia
		if (zCEventManager* em = victim->GetEM(FALSE))
			em->Clear();
		// obrot w fazie ustawiania (natychmiastowy szarpal kamera)
		seq.heroYaw0 = YawOf(player->GetAtVectorWorld());
		seq.victimYaw0 = YawOf(victim->GetAtVectorWorld());
		player->SetMovLock(TRUE);
		seq.heroStart = player->GetPositionWorld();
		float g;
		seq.heroYOff = GroundAt(seq.heroStart, g) ? seq.heroStart.n[1] - g : 0.0f;
		zVEC3 vp0 = victim->GetPositionWorld();
		seq.victimYOff = GroundAt(vp0, g) ? vp0.n[1] - g : 0.0f;
		ZLog("Finisher %s%s na %s (bron: %s)", def->id.c_str(), inplace ? " (w miejscu)" : "", victim->GetInstanceName().ToChar(),
			zdcore::WeaponCutName(wcut));
		LogPositions("start");
		return true;
	}

	void DirectorUpdate(float dt) {
		VictimTailUpdate(dt);
		if (seq.phase == DirPhase::Idle)
			return;
		if (seq.phase == DirPhase::Settle) {
			seq.settleT += dt;
			zCModel* hm = player ? player->GetModel() : nullptr;
			if (!hm || !seq.heroProto || !hm->IsAniActive(seq.heroProto) || seq.settleT > SETTLE_MAX_SECONDS)
				Settle();
			return;
		}
		oCNpc* v = seq.victim;
		zCModel* hm = player ? player->GetModel() : nullptr;
		if (!hm || !v->homeWorld || !v->GetModel()) {
			DirectorFinishNow();
			return;
		}
		if (!seq.cut) {
			if (zCEventManager* em = v->GetEM(FALSE))
				em->Clear();
			if (zinput)
				zinput->ClearKeyBuffer();
		}
		for (auto npc : staggered)
			if (zCEventManager* em = npc->GetEM(FALSE))
				em->Clear();

		if (seq.phase == DirPhase::Align) {
			seq.alignT += dt;
			float f = seq.alignT / ALIGN_SECONDS;
			if (f > 1.0f)
				f = 1.0f;
			zVEC3 p(seq.from.n[0] + (seq.to.n[0] - seq.from.n[0]) * f, seq.from.n[1] + (seq.to.n[1] - seq.from.n[1]) * f,
				seq.from.n[2] + (seq.to.n[2] - seq.from.n[2]) * f);
			v->SetPositionWorld(p);
			// ofiara twarza do bohatera (animacje par W3 tez sa w ukladzie jej voba)
			float heroYaw = YawOf(seq.dir), victimYaw = YawOf(zVEC3(-seq.dir.n[0], 0.0f, -seq.dir.n[2]));
			SetYaw(player, zdcore::LerpYaw(seq.heroYaw0, heroYaw, f));
			SetYaw(v, zdcore::LerpYaw(seq.victimYaw0, victimYaw, f));
			if (f < 1.0f)
				return;
			LogPositions("ustawione");
			// zalozenie nakladki zatrzymuje wszystkie animacje - wznawiamy je i finisher wchodzi przejsciem z biezacej pozy
			if (!ZApplyOverlayKeepAnis(hm, seq.overlay)) {
				ZLog("Finisher %s: nie podpieto nakladki %s", seq.def->id.c_str(), seq.overlay.c_str());
				DirectorFinishNow();
				return;
			}
			seq.heroAni = hm->GetAniIDFromAniName(seq.def->heroAni.c_str());
			seq.heroProto = seq.heroAni >= 0 ? hm->GetAniFromAniID(seq.heroAni) : nullptr;
			if (!seq.heroProto) {
				ZLog("Finisher %s: brak animacji %s", seq.def->id.c_str(), seq.def->heroAni.c_str());
				DirectorFinishNow();
				return;
			}
			StopStandTriggers(hm);
			hm->FadeOutAnisLayerRange(0, 99999);
			hm->StartAni(seq.heroProto, 0);
			seq.releaseTime = ComputeReleaseTime(seq.heroProto, seq.def);
			seq.slowFactor = FinisherSlowNow();
			StaggerNearby();
			if (zCModel* vm = v->GetModel()) {
				// ofiara z animacja pary: jak dotad twarde przejscie w jej pierwsza klatke (zalozenie nakladki zatrzymuje
				// animacje, a ta startuje w tej samej klatce); bez animacji - wznawiamy, zeby nie zamarla
				bool victimAni = !seq.def->victimAni.empty();
				if (!seq.def->victimOverlay.empty()) {
					if (victimAni)
						vm->ApplyModelProtoOverlay((seq.def->victimOverlay + ".MDS").c_str());
					else
						ZApplyOverlayKeepAnis(vm, seq.def->victimOverlay + ".MDS");
				}
				if (victimAni)
					vm->StartAni(seq.def->victimAni.c_str(), 0);
			}
			ZDebug("Finisher %s: sterowanie wroci po %.2f s animacji (%d kl.)", seq.def->id.c_str(), seq.releaseTime,
				(int)seq.heroProto->numFrames);
			seq.phase = DirPhase::Play;
			seq.t = 0.0f;
			return;
		}

		seq.t += dt;
		float aniTime = 0.0f;
		bool active = HeroAniTime(hm, seq.def->heroAni, aniTime);
		float tt = active ? aniTime : seq.t;
		const zdcore::FinisherDef* d = seq.def;
		if (d->paired())
			FollowTrajectories(v, tt);
		bool slow = seq.slowFactor < 1.0f && tt >= d->cutTime + d->slowFrom && tt <= d->cutTime + d->slowTo;
		if (slow != seq.slow && ztimer) {
			ztimer->SetMotionFactor(slow ? seq.slowFactor : 1.0f);
			seq.slow = slow;
		}
		if (!seq.cut && !active && seq.t > 0.1f) {
			CancelBeforeCut(hm);
			return;
		}
		if (!seq.cut && tt >= d->cutTime)
			DoCut();
		if (seq.cut && (tt >= seq.releaseTime || (!active && seq.t > 0.1f)))
			Release(tt);   // sterowanie wraca w tej klatce, nakladka zejdzie w Settle
		else if (seq.t > 8.0f)
			DirectorFinishNow();   // zawieszona animacja: ofiara nie moze zostac z HP=1 i polknieta smiercia
	}

	void DirectorFinishNow() {
		if (DirectorPlaying()) {
			if (!seq.cut && seq.victim && seq.victim->homeWorld && seq.victim->GetModel())
				DoCut();
			Release(-1.0f);
		}
		if (seq.phase == DirPhase::Settle)
			Settle();
		EndVictimTail();
	}

	bool DirectorForgetVictim(oCNpc* npc) {
		if (!npc || tail.npc != npc)
			return false;
		EndVictimTail();
		return true;
	}

	void DirectorInit() {
		zdOnBeforeHit = [](oCNpc* victim, oCNpc::oSDamageDescriptor& desc) -> bool {
			if (victim == player && MovesHeroInvulnerable())
				return true;   // unik / przewrot w oknie nietykalnosci
			if (!DirectorPlaying())
				return false;   // po oddaniu sterowania (Settle) bohater jest juz zwyczajnie trafialny
			if (victim == seq.victim && !seq.cut)
				return true;
			return victim == player && zcfg.heroInvulnerable;
		};
		zdOnBeforeDeath = [](oCNpc* victim, oCNpc* killer) -> bool {
			// Ponowne DoDie ofiary trwajacego finishera przed cieciem (od NB 22.10 - kolejne DoDie z HP 0, killer
			// rozny / pusty): odkladamy jak pierwsze - inaczej silnik zabijal ofiare w pol finishera i lezala w ragdollu,
			// a ciecie trafialo w lezace cialo (zgloszenie 22.10). Smierc i tak przychodzi przy cieciu (zdDeathPassThrough).
			if (victim && victim == seq.victim && !seq.cut && DirectorPlaying()) {
				static int lateDeaths = 0;
				if (lateDeaths++ < 20)
					ZLog("Finisher: ponowne DoDie ofiary %s przed cieciem (zabojca %s) - odlozone do ciecia",
						victim->GetInstanceName().ToChar(), killer ? killer->GetInstanceName().ToChar() : "brak");
				return true;
			}
			if (!player || killer != player || zcfg.finisherChance <= 0 || DirectorIsVictim(victim))
				return false;
			const LastHit* h = GetLastHit(victim);
			if (!IsFreshEdgeHit(h) || h->attacker != player)
				return false;   // takze obuch (h->cut == NoCut)
			if (rand() % 100 >= zcfg.finisherChance)
				return false;
			return DirectorTryStart(victim, zdcore::POSE_STANDING);
		};
		// Czlowiek po smiertelnym ciosie gracza tylko pada nieprzytomny - wtedy tez losujemy finisher (KnockoutFinisher).
		zdOnBeforeKnockout = [](oCNpc* victim, oCNpc* attacker) -> bool {
			if (!player || attacker != player || !victim)
				return false;
			// domyslnie tylko trwale wrodzy: arena, bojki i questowe powalenia zostaja powaleniami
			if (!zdcore::KnockoutFinisherAllowed(zcfg.knockoutFinisher, victim->GetPermAttitude(player)))
				return false;
			return zdOnBeforeDeath(victim, attacker);
		};
	}
}
