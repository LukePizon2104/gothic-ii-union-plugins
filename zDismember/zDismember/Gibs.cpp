// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct Gib {
		zCVob* vob;                    // nasz ref (z new)
		zCModel* model;                // klucz w BoneSquash
		zCVob* blood;                  // wytrysk lecacy z czescia (wlasnosc Blood)
		zdcore::BallisticState st;
		float life;
		bool inWorld;
		oCNpc* srcNpc;                 // postac, z ktorej odcieto (tylko porownanie - bez referencji)
		int srcNode;                   // wezel ciecia w jej modelu (PartNode)
	};

	static std::vector<Gib> gibs;

	static const int GIB_TRACE_FLAGS = zTRACERAY_STAT_POLY | zTRACERAY_POLY_NORMAL | zTRACERAY_POLY_IGNORE_TRANSP |
		zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_VOB_IGNORE_PROJECTILES;

	static zdcore::Vec3 ToV(const zVEC3& v) { return zdcore::Vec3(v.n[0], v.n[1], v.n[2]); }
	static zVEC3 ToZ(const zdcore::Vec3& v) { return zVEC3(v.x, v.y, v.z); }

	static zdcore::Mat34 ToMat34(const zMAT4& m) {
		zdcore::Mat34 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = m.v[i].n[j];
		return r;
	}

	static zMAT4 ToMat4(const zdcore::Mat34& m) {
		zMAT4 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.v[i].n[j] = m.m[i][j];
		r.v[3].n[0] = r.v[3].n[1] = r.v[3].n[2] = 0.0f;
		r.v[3].n[3] = 1.0f;
		return r;
	}

	static void NormalizeBasis(zMAT4& m) {
		for (int c = 0; c < 3; c++) {
			float l = sqrtf(m.v[0].n[c] * m.v[0].n[c] + m.v[1].n[c] * m.v[1].n[c] + m.v[2].n[c] * m.v[2].n[c]);
			if (l > 1e-6f)
				for (int r = 0; r < 3; r++)
					m.v[r].n[c] /= l;
		}
	}

	static void DestroyGib(Gib& g) {
		if (g.inWorld && g.vob->homeWorld)
			g.vob->homeWorld->RemoveVob(g.vob);
		SquashClear(g.model);
		g.vob->Release();
	}

	static float Rand01() {
		return (float)rand() / (float)RAND_MAX;
	}

	zCVob* SpawnGib(oCNpc* npc, int keepNode, const zVEC3& velocity) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zCModel* src = npc ? npc->GetModel() : nullptr;
		if (!world || !src || keepNode < 0 || keepNode >= src->nodeList.GetNum() || zcfg.maxGibs <= 0)
			return nullptr;
		zCModelNodeInst* srcKeep = src->nodeList[keepNode];
		if (!srcKeep->parentNode)
			return nullptr;

		zMAT4 trafo = npc->GetTrafoModelNodeToWorld(srcKeep);
		NormalizeBasis(trafo);

		zCVob* vob = new zCVob();
		vob->SetVobName("ZDISM_GIB");
		vob->SetVisual(src->GetVisualName());
		zCModel* model = zDYNAMIC_CAST<zCModel>(vob->GetVisual());
		if (!model || model->nodeList.GetNum() != src->nodeList.GetNum()) {
			ZLog("Gibs: nie zbudowano klona %s", src->GetVisualName().ToChar());
			vob->Release();
			return nullptr;
		}

		for (int i = 0; i < src->meshLibList.GetNum(); i++) {
			zCModelMeshLib* lib = src->meshLibList[i]->meshLib;
			if (lib && lib->GetObjectName().Length())
				model->ApplyMeshLib(lib->GetObjectName());
		}
		// warianty tekstur (V/C) - same numery klatek, bez wskaznikow na tekstury
		for (int j = 0; j < model->meshLibList.GetNum(); j++)
			for (int i = 0; i < src->meshLibList.GetNum(); i++)
				if (src->meshLibList[i]->meshLib == model->meshLibList[j]->meshLib) {
					memcpy(model->meshLibList[j]->texAniState.actAniFrames, src->meshLibList[i]->texAniState.actAniFrames,
						sizeof(model->meshLibList[j]->texAniState.actAniFrames));
					break;
				}
		model->SetModelScale(src->modelScale);
		model->fatness = src->fatness;

		// osobne modele na wezlach galezi (glowa ludzi/orkow) jada razem z czescia
		int n = src->nodeList.GetNum();
		for (int i = 0; i < n; i++)
			if (src->nodeList[i]->nodeVisual && NodeInSubtree(src->nodeList[i], srcKeep) && zDYNAMIC_CAST<zCMorphMesh>(src->nodeList[i]->nodeVisual))
				model->SetNodeVisual(model->nodeList[i], src->nodeList[i]->nodeVisual, FALSE);

		std::vector<zMAT4> pose(n);
		for (int i = 0; i < n; i++)
			pose[i] = src->nodeList[i]->trafo;
		SquashSetBranch(model, keepNode, pose);

		// ksztalt do kolizji: wezly galezi w ukladzie czesci + prostopadloscian modelu na wezle
		std::vector<zdcore::Vec3> points;
		std::vector<zMAT4> obj(n);
		std::vector<char> done(n, 0);
		obj[keepNode] = Alg_Identity3D();
		done[keepNode] = 1;
		for (bool progress = true; progress;) {
			progress = false;
			for (int i = 0; i < n; i++) {
				auto parent = src->nodeList[i]->parentNode;
				int p = parent ? NodeIndex(src, parent) : -1;
				if (!done[i] && p >= 0 && done[p]) {
					obj[i] = obj[p] * pose[i];
					done[i] = 1;
					progress = true;
				}
			}
		}
		for (int i = 0; i < n; i++) {
			if (!done[i])
				continue;
			points.push_back(ToV(obj[i].GetTranslation()));
			if (zCVisual* nv = src->nodeList[i]->nodeVisual) {
				zTBBox3D bb = nv->GetBBox3D();
				for (int c = 0; c < 8; c++) {
					zVEC3 corner((c & 1) ? bb.maxs.n[0] : bb.mins.n[0], (c & 2) ? bb.maxs.n[1] : bb.mins.n[1], (c & 4) ? bb.maxs.n[2] : bb.mins.n[2]);
					zMAT4 m = obj[i];
					points.push_back(ToV(m * corner));
				}
			}
		}

		vob->SetTrafoObjToWorld(trafo);
		world->AddVob(vob);

		Gib g;
		g.vob = vob;
		g.model = model;
		g.life = 0.0f;
		g.inWorld = true;
		g.srcNpc = npc;
		g.srcNode = keepNode;
		g.st.trafo = ToMat34(trafo);
		zdcore::BallisticSetShape(g.st, points);
		g.st.vel = ToV(velocity);
		g.st.axis = zdcore::Normalize(zdcore::Vec3(Rand01() - 0.5f, Rand01() - 0.5f, Rand01() - 0.5f));
		g.st.degPerSec = 400.0f + 400.0f * Rand01();
		g.blood = BloodBurst(ToZ(zdcore::Transform(g.st.trafo, g.st.center)), 2.5f, &velocity);
		gibs.push_back(g);

		while ((int)gibs.size() > zcfg.maxGibs) {
			DestroyGib(gibs.front());
			gibs.erase(gibs.begin());
		}
		ZDebug("Gib %s wezel %s, punktow %d", npc->GetInstanceName().ToChar(), srcKeep->protoNode->nodeName.ToChar(), (int)points.size());
		return vob;
	}

	void GibsUpdate(float dt) {
		if (gibs.empty() || dt <= 0.0f)
			return;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world)
			return;
		zVEC3 pp = player ? player->GetPositionWorld() : zVEC3(0, 0, 0);
		for (auto it = gibs.begin(); it != gibs.end();) {
			Gib& g = *it;
			g.life += dt;
			zVEC3 gp = g.vob->GetPositionWorld();
			float dx = gp.n[0] - pp.n[0], dz = gp.n[2] - pp.n[2];
			if (g.life > zcfg.gibLifetime || (player && dx * dx + dz * dz > zcfg.gibDespawnDist * zcfg.gibDespawnDist)) {
				DestroyGib(g);
				it = gibs.erase(it);
				continue;
			}
			if (g.inWorld && !g.st.resting) {
				zCVob* self = g.vob;
				zdcore::TraceFn trace = [world, self](const zdcore::Vec3& from, const zdcore::Vec3& ray, zdcore::Vec3& hit, zdcore::Vec3& normal) {
					if (!world->TraceRayNearestHit(ToZ(from), ToZ(ray), self, GIB_TRACE_FLAGS) || !world->traceRayReport.foundHit)
						return false;
					hit = ToV(world->traceRayReport.foundIntersection);
					normal = ToV(world->traceRayReport.foundPolyNormal);
					return true;
				};
				zdcore::BallisticStep(g.st, dt, trace);
				g.vob->SetTrafoObjToWorld(ToMat4(g.st.trafo));
				if (g.life < 2.5f && g.blood && g.blood->homeWorld)
					g.blood->SetPositionWorld(ToZ(zdcore::Transform(g.st.trafo, g.st.center)));
			}
			if (g.life >= 2.5f)
				g.blood = nullptr;   // vob krwi i tak juz zniknal (Blood)
			++it;
		}
	}

	int GibsCount() {
		return (int)gibs.size();
	}

	int GibsRemoveFor(oCNpc* npc, int node) {
		int removed = 0;
		for (auto it = gibs.begin(); it != gibs.end();) {
			if (npc && it->srcNpc == npc && (node < 0 || it->srcNode == node)) {
				DestroyGib(*it);
				it = gibs.erase(it);
				removed++;
			}
			else
				++it;
		}
		return removed;
	}

	void GibsBeforeSave() {
		for (auto& g : gibs) {
			g.blood = nullptr;   // BloodClearAll przed zapisem usuwa voby krwi
			if (g.inWorld && g.vob->homeWorld) {
				g.vob->homeWorld->RemoveVob(g.vob);
				g.inWorld = false;
			}
		}
	}

	void GibsAfterSave() {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world)
			return;
		for (auto& g : gibs)
			if (!g.inWorld) {
				g.vob->SetTrafoObjToWorld(ToMat4(g.st.trafo));
				world->AddVob(g.vob);
				g.inWorld = true;
			}
	}

	void GibsClearAll() {
		for (auto& g : gibs)
			DestroyGib(g);
		gibs.clear();
	}
}
