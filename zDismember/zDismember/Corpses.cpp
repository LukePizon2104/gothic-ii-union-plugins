// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	using zdcore::Mat34;
	using zdcore::Vec3;

	// wezly szkieletu Gothica dla punktow ragdolla (kolejnosc jak zdcore::RagNode)
	static const char* RAG_NODE_NAMES[zdcore::RAG_COUNT] = {
		"BIP01", "BIP01 SPINE2", "BIP01 HEAD",
		"BIP01 L UPPERARM", "BIP01 L FOREARM", "BIP01 L HAND", "BIP01 R UPPERARM", "BIP01 R FOREARM", "BIP01 R HAND",
		"BIP01 L THIGH", "BIP01 L CALF", "BIP01 L FOOT", "BIP01 R THIGH", "BIP01 R CALF", "BIP01 R FOOT",
	};
	// kosc obracana wzdluz odcinka miedzy punktami (od, do)
	struct LimbMap {
		const char* node;
		int from, to;
	};
	static const LimbMap LIMBS[] = {
		{ "BIP01 L UPPERARM", zdcore::RAG_L_SHOULDER, zdcore::RAG_L_ELBOW }, { "BIP01 L FOREARM", zdcore::RAG_L_ELBOW, zdcore::RAG_L_HAND },
		{ "BIP01 R UPPERARM", zdcore::RAG_R_SHOULDER, zdcore::RAG_R_ELBOW }, { "BIP01 R FOREARM", zdcore::RAG_R_ELBOW, zdcore::RAG_R_HAND },
		{ "BIP01 L THIGH", zdcore::RAG_L_HIP, zdcore::RAG_L_KNEE }, { "BIP01 L CALF", zdcore::RAG_L_KNEE, zdcore::RAG_L_FOOT },
		{ "BIP01 R THIGH", zdcore::RAG_R_HIP, zdcore::RAG_R_KNEE }, { "BIP01 R CALF", zdcore::RAG_R_KNEE, zdcore::RAG_R_FOOT },
		{ "BIP01 NECK", zdcore::RAG_CHEST, zdcore::RAG_HEAD }, { "BIP01 HEAD", zdcore::RAG_CHEST, zdcore::RAG_HEAD },
	};
	struct Corpse {
		oCNpc* npc = nullptr;                 // AddRef
		zCModel* model = nullptr;
		zdcore::RagdollState sim;
		zdcore::RagPoseRig rig;
		Mat34 objToVob;                       // uklad modelu -> uklad voba (swiat = vob * objToVob, nadaza za vobem)
		bool frozen = false;
		std::vector<Mat34> frozenLocals;      // bufor pozy; po spoczynku liczona raz dla danego polozenia voba
		zVEC3 frozenVobPos;
	};

	static std::vector<Corpse> corpses;
	static std::unordered_map<zCModel*, size_t> corpseByModel;
	// ziemia tylko ze statycznego swiata (bez vobow) - kilkanascie promieni na zwloki na krok
	static const int RAG_TRACE = zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE;
	static const size_t MAX_CORPSES = 32;

	static Mat34 ToM34(const zMAT4& m) {
		Mat34 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = m.v[i].n[j];
		return r;
	}

	static zMAT4 ToM4(const Mat34& m) {
		zMAT4 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.v[i].n[j] = m.m[i][j];
		r.v[3].n[0] = r.v[3].n[1] = r.v[3].n[2] = 0.0f;
		r.v[3].n[3] = 1.0f;
		return r;
	}

	// Bez skali w osiach (scisniete wezly odcietych czesci maja 0,01) - odwracanie zaklada czysty obrot.
	static Mat34 Orthonormal(const Mat34& m) {
		Mat34 r = m;
		for (int c = 0; c < 3; c++) {
			float l = sqrtf(m.m[0][c] * m.m[0][c] + m.m[1][c] * m.m[1][c] + m.m[2][c] * m.m[2][c]);
			if (l > 1e-12f)
				for (int k = 0; k < 3; k++)
					r.m[k][c] = m.m[k][c] / l;
		}
		return r;
	}

	static Mat34 VobTrafo(oCNpc* npc) {
		return Orthonormal(ToM34(npc->trafoObjToWorld));
	}

	static std::vector<Vec3> Particles(const Corpse& c) {
		std::vector<Vec3> p(zdcore::RAG_COUNT);
		for (int i = 0; i < zdcore::RAG_COUNT; i++)
			p[i] = c.sim.p[i].pos;
		return p;
	}

	int CorpsesCount() {
		return (int)corpses.size();
	}

	static void RebuildIndex() {
		corpseByModel.clear();
		for (size_t i = 0; i < corpses.size(); i++)
			corpseByModel[corpses[i].model] = i;
	}

	static void Drop(size_t i) {
		if (corpses[i].npc)
			corpses[i].npc->Release();
		corpses.erase(corpses.begin() + i);
		RebuildIndex();
	}

	bool CorpseForget(oCNpc* npc) {
		for (size_t i = 0; npc && i < corpses.size(); i++)
			if (corpses[i].npc == npc) {
				Drop(i);
				return true;
			}
		return false;
	}

	void CorpsesClearAll() {
		for (auto& c : corpses)
			if (c.npc)
				c.npc->Release();
		corpses.clear();
		corpseByModel.clear();
	}

	void CorpsesOnDeath(oCNpc* npc, oCNpc* killer) {
		if (!zcfg.enabled || !zcfg.ragdoll || !npc || npc == player || !npc->homeWorld || rand() % 100 >= zcfg.ragdollChance)
			return;
		zCModel* m = npc->GetModel();
		if (!m || corpseByModel.count(m))
			return;
		int n = m->nodeList.GetNum();
		Corpse c;
		c.npc = npc;
		c.model = m;
		// punkty i wezly - tylko szkielety ludzkie (ludzie, orkowie, szkielety, zombie)
		std::vector<Vec3> pos(zdcore::RAG_COUNT);
		std::vector<int> ragNode(zdcore::RAG_COUNT);
		for (int i = 0; i < zdcore::RAG_COUNT; i++) {
			ragNode[i] = FindNode(m, RAG_NODE_NAMES[i]);
			if (ragNode[i] < 0)
				return;
			zVEC3 w = npc->GetTrafoModelNodeToWorld(m->nodeList[ragNode[i]]).GetTranslation();
			pos[i] = Vec3(w.n[0], w.n[1], w.n[2]);
		}
		// hierarchia, poza startowa i role wezlow (Core\Ragdoll: korzen/miednica/klatka = tulow, konczyny = odcinki)
		std::vector<int> parent(n, -1), role(n, zdcore::RAG_ROLE_RIGID), from(n, -1), to(n, -1);
		std::vector<Mat34> local(n);
		for (int i = 0; i < n; i++) {
			auto pn = m->nodeList[i]->parentNode;
			parent[i] = pn ? NodeIndex(m, pn) : -1;
			local[i] = Orthonormal(ToM34(m->nodeList[i]->trafo));
		}
		role[ragNode[zdcore::RAG_PELVIS]] = zdcore::RAG_ROLE_ROOT;
		for (const char* t : { "BIP01 PELVIS", "BIP01 SPINE" }) {
			int k = FindNode(m, t);
			if (k >= 0)
				role[k] = zdcore::RAG_ROLE_PELVIS;
		}
		for (const char* t : { "BIP01 SPINE1", "BIP01 SPINE2" }) {
			int k = FindNode(m, t);
			if (k >= 0)
				role[k] = zdcore::RAG_ROLE_CHEST;
		}
		for (auto& l : LIMBS) {
			int k = FindNode(m, l.node);
			if (k >= 0) {
				role[k] = zdcore::RAG_ROLE_LIMB;
				from[k] = l.from;
				to[k] = l.to;
			}
		}
		// uklad modelu -> swiat z korzenia: swiat korzenia * odwrotnosc jego ukladu w modelu
		Mat34 rootObj = local[ragNode[zdcore::RAG_PELVIS]];
		for (int k = parent[ragNode[zdcore::RAG_PELVIS]]; k >= 0; k = parent[k])
			rootObj = zdcore::Mul(local[k], rootObj);
		Mat34 rootWorld = Orthonormal(ToM34(npc->GetTrafoModelNodeToWorld(m->nodeList[ragNode[zdcore::RAG_PELVIS]])));
		Mat34 objToWorld = zdcore::Mul(rootWorld, zdcore::InverseRigid(rootObj));
		c.objToVob = zdcore::Mul(zdcore::InverseRigid(VobTrafo(npc)), objToWorld);
		c.rig = zdcore::BuildRagPoseRig(parent, local, role, from, to, pos, objToWorld);
		if ((int)c.rig.order.size() != n)
			return;
		// pchniecie w strone ciosu (albo do tylu, gdy nieznany)
		Vec3 dir;
		const LastHit* h = GetLastHit(npc);
		if (h && h->attacker && zdFrame - h->frame <= 2)
			dir = Vec3(h->dir.n[0], 0.0f, h->dir.n[2]);
		else {
			zVEC3 at = npc->GetAtVectorWorld();
			dir = Vec3(-at.n[0], 0.0f, -at.n[2]);
		}
		zdcore::RagdollInit(c.sim, pos, zdcore::Normalize(dir) * zcfg.ragdollImpulse);
		npc->AddRef();
		// limit symulowanych naraz: najstarsze w ruchu odpadaja (dalej zwykla animacja smierci - nie zamarzaja w pol
		// upadku); limit wszystkich: najstarsze odpadaja
		int active = 0;
		for (size_t k = corpses.size(); k-- > 0;) {
			if (corpses[k].frozen || ++active < zcfg.ragdollMaxActive)
				continue;
			if (corpses[k].npc)
				corpses[k].npc->Release();
			corpses.erase(corpses.begin() + k);
		}
		corpses.push_back(c);
		while (corpses.size() > MAX_CORPSES)
			Drop(0);
		RebuildIndex();
		ZDebug("Ragdoll %s", npc->GetInstanceName().ToChar());
	}

	// Zamrozone zwloki: vob pod miednice (fokus i lup tam, gdzie lezy cialo; po wczytaniu gry tam lezy).
	// Punkty sa w swiecie, a uklad modelu liczony z voba co klatke - przesuniecie voba nie rusza ciala.
	static void Freeze(Corpse& c) {
		c.frozen = true;
		oCNpc* npc = c.npc;                       // SetPositionWorld moze odpalic skrypty (nowe zwloki = realokacja)
		zVEC3 v = npc->GetPositionWorld();
		Vec3 pel = c.sim.p[zdcore::RAG_PELVIS].pos;
		float dx = pel.x - v.n[0], dz = pel.z - v.n[2];
		if (dx * dx + dz * dz >= 30.0f * 30.0f)
			npc->SetPositionWorld(zVEC3(v.n[0] + dx, v.n[1], v.n[2] + dz));
	}

	void CorpsesUpdate(float dt) {
		if (corpses.empty())
			return;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zVEC3 hp = player ? player->GetPositionWorld() : zVEC3(0, 0, 0);
		for (size_t i = 0; i < corpses.size();) {
			Corpse& c = corpses[i];
			zVEC3 v = c.npc->GetPositionWorld();
			float dx = v.n[0] - hp.n[0], dz = v.n[2] - hp.n[2];
			if (!world || !c.npc->homeWorld || !c.npc->IsDead() || c.npc->GetModel() != c.model || dx * dx + dz * dz > 8000.0f * 8000.0f) {
				Drop(i);
				continue;
			}
			if (!c.frozen) {
				oCNpc* self = c.npc;
				zdcore::TraceFn trace = [world, self](const Vec3& from, const Vec3& ray, Vec3& hit, Vec3& normal) -> bool {
					if (!world->TraceRayNearestHit(zVEC3(from.x, from.y, from.z), zVEC3(ray.x, ray.y, ray.z), self, RAG_TRACE) ||
						!world->traceRayReport.foundHit)
						return false;
					auto& r = world->traceRayReport;
					hit = Vec3(r.foundIntersection.n[0], r.foundIntersection.n[1], r.foundIntersection.n[2]);
					normal = Vec3(r.foundPolyNormal.n[0], r.foundPolyNormal.n[1], r.foundPolyNormal.n[2]);
					return true;
				};
				// kroki do 1/30 s, najwyzej 3 na klatke (przy niskim FPS cialo zwalnia zamiast dlawic gre promieniami)
				float t = std::min(dt, 0.1f);
				for (int k = 0; k < 3 && t > 1e-4f && !c.sim.resting; k++) {
					float h = std::min(t, 1.0f / 30.0f);
					zdcore::RagdollStep(c.sim, h, trace);
					t -= h;
				}
				if (c.sim.resting) {
					Freeze(c);
					continue;   // Freeze mogl zmienic wektor - ten sam indeks sprawdzony w nastepnej klatce
				}
			}
			i++;
		}
	}

	void CorpseApply(zCModel* model) {
		auto it = corpseByModel.find(model);
		if (it == corpseByModel.end())
			return;
		Corpse& c = corpses[it->second];
		auto& nodes = model->nodeList;
		if (!zcfg.enabled || !zcfg.ragdoll || !c.npc || c.npc->GetModel() != model || nodes.GetNum() != (int)c.rig.parent.size())
			return;
		ZPROF_SCOPE("Anis.Ragdoll");
		zVEC3 vp = c.npc->GetPositionWorld();
		bool cached = c.frozen && !c.frozenLocals.empty() && vp.n[0] == c.frozenVobPos.n[0] && vp.n[1] == c.frozenVobPos.n[1] &&
			vp.n[2] == c.frozenVobPos.n[2];
		if (!cached) {
			Mat34 objToWorld = zdcore::Mul(VobTrafo(c.npc), c.objToVob);
			zdcore::RagdollPose(c.rig, Particles(c), objToWorld, c.frozenLocals);
			c.frozenVobPos = vp;
		}
		for (int i = 0; i < nodes.GetNum(); i++)
			nodes[i]->trafo = ToM4(c.frozenLocals[i]);
	}
}
