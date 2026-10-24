// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static const float SQUASH_SCALE = 0.01f;

	struct SquashEntry {
		std::vector<int> nodes;        // wezly ze skala SQUASH_SCALE
		bool branch = false;           // klon odcietej czesci
		int keep = -1;
		std::vector<zMAT4> pose;
		std::vector<int> ancestors, sideRoots;
	};

	static std::unordered_map<zCModel*, SquashEntry> squashMap;   // model trzymany przez AddRef

	static float BasisLength(const zMAT4& m) {
		return sqrtf(m.v[0].n[0] * m.v[0].n[0] + m.v[1].n[0] * m.v[1].n[0] + m.v[2].n[0] * m.v[2].n[0]);
	}

	static void ScaleBasis(zMAT4& m, float f) {
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				m.v[r].n[c] *= f;
	}

	// Bezwzgledna skala - idempotentne, bo AdvanceAnis nie zawsze przelicza macierz od nowa.
	static void SetBasisScale(zMAT4& m, float s) {
		float len = BasisLength(m);
		if (len > 1e-20f)
			ScaleBasis(m, s / len);
	}

	static void SetIdentity(zMAT4& m, float s) {
		for (int r = 0; r < 4; r++)
			for (int c = 0; c < 4; c++)
				m.v[r].n[c] = r == c ? (r < 3 ? s : 1.0f) : 0.0f;
	}

	static void Apply(zCModel* model, SquashEntry& e) {
		auto& nodes = model->nodeList;
		int n = nodes.GetNum();
		if (e.branch && e.keep >= 0 && e.keep < n) {
			for (int i = 0; i < n && i < (int)e.pose.size(); i++)
				nodes[i]->trafo = e.pose[i];
			for (int i : e.ancestors)
				SetIdentity(nodes[i]->trafo, SQUASH_SCALE);
			for (int i : e.sideRoots)
				ScaleBasis(nodes[i]->trafo, SQUASH_SCALE);
			SetIdentity(nodes[e.keep]->trafo, powf(SQUASH_SCALE, -(float)e.ancestors.size()));
		}
		for (int i : e.nodes)
			if (i >= 0 && i < n)
				SetBasisScale(nodes[i]->trafo, SQUASH_SCALE);
	}

	// Wolany dla kazdego animowanego modelu w kazdej klatce - sekcja Anis.Hook liczy tylko czesc pluginu (bez animacji
	// silnika), wywolan/klatke = liczba modeli; Anis.Ragdoll / Anis.Recoil / Anis.Squash tylko, gdy model ma prace.
	HOOK Ivk_zCModel_AdvanceAnis AS(&zCModel::AdvanceAnis, &zCModel::AdvanceAnis_Union);
	void zCModel::AdvanceAnis_Union() {
		ZHookHit(ZH_ADVANCEANIS);
		THISCALL(Ivk_zCModel_AdvanceAnis)();
		ZPROF_SCOPE("Anis.Hook");
		CorpseApply(this);   // ragdoll zwlok - przed sciskaniem, zeby odciete czesci zostaly scisniete
		ParryRecoilApply(this);   // odrzut broni i rak po udanym bloku - na pozie z animacji
		BackSwordsApply(this);    // miecze na plecach: wezly slotow broni wzgledem kregoslupa (po odrzucie - za tulowiem)
		CrossbowApplyNodes(this); // szybki strzal z kuszy: skala / przesuniecie kuszy w lewej dloni (XbowHandScale / Adjust)
		if (squashMap.empty())
			return;
		auto it = squashMap.find(this);
		if (it != squashMap.end()) {
			ZPROF_SCOPE("Anis.Squash");
			Apply(this, it->second);
		}
	}

	static SquashEntry& Entry(zCModel* model) {
		auto it = squashMap.find(model);
		if (it != squashMap.end())
			return it->second;
		model->AddRef();
		return squashMap[model];
	}

	void SquashNode(zCModel* model, int nodeIndex) {
		if (!model || nodeIndex < 0 || nodeIndex >= model->nodeList.GetNum())
			return;
		SquashEntry& e = Entry(model);
		if (std::find(e.nodes.begin(), e.nodes.end(), nodeIndex) == e.nodes.end())
			e.nodes.push_back(nodeIndex);
		Apply(model, e);
	}

	bool SquashHasNode(zCModel* model, int nodeIndex) {
		auto it = squashMap.find(model);
		return it != squashMap.end() && std::find(it->second.nodes.begin(), it->second.nodes.end(), nodeIndex) != it->second.nodes.end();
	}

	bool SquashUnnode(zCModel* model, int nodeIndex) {
		auto it = model ? squashMap.find(model) : squashMap.end();
		if (it == squashMap.end())
			return false;
		std::vector<int>& nodes = it->second.nodes;
		auto f = std::find(nodes.begin(), nodes.end(), nodeIndex);
		if (f == nodes.end())
			return false;
		nodes.erase(f);
		// skala 1 od razu: AdvanceAnis nie zawsze przelicza macierz wezla (marionetka przy timeScale 0, wezel bez animacji)
		if (nodeIndex >= 0 && nodeIndex < model->nodeList.GetNum())
			SetBasisScale(model->nodeList[nodeIndex]->trafo, 1.0f);
		if (nodes.empty() && !it->second.branch) {
			squashMap.erase(it);
			model->Release();
		}
		return true;
	}

	void SquashClear(zCModel* model) {
		auto it = squashMap.find(model);
		if (it == squashMap.end())
			return;
		squashMap.erase(it);
		model->Release();
	}

	void SquashClearAll() {
		for (auto& kv : squashMap)
			kv.first->Release();
		squashMap.clear();
	}

	void SquashCollect() {
		for (auto it = squashMap.begin(); it != squashMap.end();) {
			if (it->first->refCtr <= 1) {
				zCModel* m = it->first;
				it = squashMap.erase(it);
				m->Release();
			}
			else
				++it;
		}
	}

	int NodeIndex(zCModel* model, zCModelNodeInst* node) {
		auto& nodes = model->nodeList;
		for (int i = 0; i < nodes.GetNum(); i++)
			if (nodes[i] == node)
				return i;
		return -1;
	}

	bool NodeInSubtree(zCModelNodeInst* node, zCModelNodeInst* root) {
		for (auto n = node; n; n = n->parentNode)
			if (n == root)
				return true;
		return false;
	}

	bool ProtoHasAni(zCModelPrototype* proto, zCModelAni* ani) {
		if (!proto || !ani)
			return false;
		const void* const* anis = (const void* const*)proto->protoAnis.GetArray();
		int n = proto->protoAnis.GetNum();
		static std::vector<zCModelPrototype*> checked;   // tylko diagnostyka (prototypy zyja w cache silnika)
		if (zcfg.debug && std::find(checked.begin(), checked.end(), proto) == checked.end()) {
			checked.push_back(proto);
			int nulls = zdcore::ProtoNullCount(anis, n);
			if (nulls || !anis)
				ZDebug("Model: prototyp %s - %d pustych wpisow protoAnis z %d (nakladka wg aniID; SearchAni by sie wywrocil)",
					proto->modelProtoName.ToChar(), nulls, n);
		}
		return zdcore::ProtoAniIndex(anis, n, ani->aniID, ani) >= 0;
	}

	std::string ProtoOfAniName(zCModel* model, zCModelAni* ani) {
		if (!model || !ani)
			return "";
		for (int i = model->modelProtoList.GetNum() - 1; i >= 0; i--) {
			zCModelPrototype* p = model->modelProtoList[i];
			if (ProtoHasAni(p, ani))
				return p->modelProtoName.ToChar();
		}
		return "";
	}

	int FindNode(zCModel* model, const char* exact, const char* contains) {
		if (!model)
			return -1;
		auto& nodes = model->nodeList;
		for (int i = 0; i < nodes.GetNum(); i++)
			if (_stricmp(nodes[i]->protoNode->nodeName.ToChar(), exact) == 0)
				return i;
		if (contains)
			for (int i = 0; i < nodes.GetNum(); i++)
				if (strstr(nodes[i]->protoNode->nodeName.ToChar(), contains))
					return i;
		return -1;
	}

	void SquashSetBranch(zCModel* clone, int keepNode, const std::vector<zMAT4>& pose) {
		if (!clone || keepNode < 0 || keepNode >= clone->nodeList.GetNum())
			return;
		SquashEntry& e = Entry(clone);
		e.branch = true;
		e.keep = keepNode;
		e.pose = pose;
		e.ancestors.clear();
		e.sideRoots.clear();
		int n = clone->nodeList.GetNum();
		std::vector<bool> isAncestor(n, false);
		for (auto p = clone->nodeList[keepNode]->parentNode; p; p = p->parentNode) {
			int pi = NodeIndex(clone, p);
			if (pi < 0)
				break;
			isAncestor[pi] = true;
			e.ancestors.push_back(pi);
		}
		for (int i = 0; i < n; i++) {
			auto parent = clone->nodeList[i]->parentNode;
			if (i == keepNode || isAncestor[i] || !parent)
				continue;
			int pi = NodeIndex(clone, parent);
			if (pi >= 0 && isAncestor[pi])
				e.sideRoots.push_back(i);
		}
		Apply(clone, e);
	}
}
