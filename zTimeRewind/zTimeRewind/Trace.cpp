// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static float RootWeight(zCModel* m, zCModelAniActive* a) {
		if (m->nodeList.GetNum() <= 0 || !m->nodeList[0])
			return -1.0f;
		zCModelNodeInst* node = m->nodeList[0];
		for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++)
			if (node->nodeAniList[k].modelAni == a)
				return node->nodeAniList[k].weight;
		return -1.0f;
	}

	std::string TrDescribeAnis(zCModel* m) {
		if (!m)
			return "brak modelu";
		std::string out;
		char buf[160];
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni)
				continue;
			sprintf_s(buf, "%s%s[l%d f%.1f/%d w%.2f%s]", out.empty() ? "" : " ", a->protoAni->aniName.ToChar(),
				a->protoAni->layer, a->actFrame, a->protoAni->numFrames, RootWeight(m, a), a->isFadingOut ? " F" : "");
			out += buf;
		}
		return out.empty() ? "brak animacji" : out;
	}

	std::string TrAniName(zCModel* m, int aniId) {
		if (!m || aniId < 0)
			return "-";
		zCModelAni* ani = m->GetAniFromAniID(aniId);
		return ani ? ani->aniName.ToChar() : "?";
	}

	std::string TrDescribeNpc(oCNpc* npc) {
		if (!npc)
			return "null";
		zVEC3 p = npc->GetPositionWorld();
		zVEC3 at = npc->GetAtVectorWorld();
		zCModel* m = npc->GetModel();
		char buf[256];
		sprintf_s(buf, "%s pos(%.0f %.0f %.0f) yaw %.2f hp %d%s%s bs %d ws %d ts %.2f swiat %d sen %d | ",
			npc->GetInstanceName().ToChar(), p.n[0], p.n[1], p.n[2], atan2f(at.n[0], at.n[2]),
			npc->attribute[NPC_ATR_HITPOINTS], npc->IsDead() ? " MARTWY" : "", npc->IsUnconscious() ? " NOKAUT" : "",
			npc->GetBodyState(), npc->GetWeaponMode(), m ? m->timeScale : -1.0f, npc->homeWorld ? 1 : 0,
			(int)npc->sleepingMode);
		return std::string(buf) + TrDescribeAnis(m);
	}

	float TrCameraDistance() {
		zCVob* cam = ogame ? ogame->GetCameraVob() : nullptr;
		if (!cam || !player)
			return -1.0f;
		zVEC3 a = cam->GetPositionWorld(), b = player->GetPositionWorld();
		float dx = a.n[0] - b.n[0], dy = a.n[1] - b.n[1], dz = a.n[2] - b.n[2];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	bool TrTraceDue(float& acc, float dt, float period) {
		acc += dt;
		if (acc < period)
			return false;
		acc = 0.0f;
		return true;
	}
}
