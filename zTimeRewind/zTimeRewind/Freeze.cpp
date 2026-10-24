// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	bool trFrozen = false;
	int trPuppetAniSwitches = 0;

	// Skryptowy stan AI (ZS_*_Loop, rutyny). Ten sam hook ma zDismember - detoury AS lacza sie w lancuch.
	HOOK Ivk_oCNpc_States_DoAIState AS(&oCNpc_States::DoAIState, &oCNpc_States::DoAIState_Union);
	int oCNpc_States::DoAIState_Union() {
		if (trFrozen)
			return 0;
		return THISCALL(Ivk_oCNpc_States_DoAIState)();
	}

	// Percepcje, regeneracja, stany czasowe NPC.
	HOOK Ivk_oCNpc_ProcessNpc AS(&oCNpc::ProcessNpc, &oCNpc::ProcessNpc_Union);
	void oCNpc::ProcessNpc_Union() {
		if (trFrozen)
			return;
		THISCALL(Ivk_oCNpc_ProcessNpc)();
	}

	// Kolejki rozkazow (AI_Goto, AI_PlayAni, rzucanie czarow...).
	HOOK Ivk_zCEventManager_ProcessMessageList AS(&zCEventManager::ProcessMessageList, &zCEventManager::ProcessMessageList_Union);
	void zCEventManager::ProcessMessageList_Union() {
		if (trFrozen)
			return;
		THISCALL(Ivk_zCEventManager_ProcessMessageList)();
	}

	// Ruch postaci (sterowanie gracza, chodzenie NPC, fizyka postaci).
	HOOK Ivk_oCAIHuman_DoAI AS(&oCAIHuman::DoAI, &oCAIHuman::DoAI_Union);
	void oCAIHuman::DoAI_Union(zCVob* vob, int& vobDeleted) {
		if (trFrozen)
			return;
		THISCALL(Ivk_oCAIHuman_DoAI)(vob, vobDeleted);
	}

	int TrMainAni(zCModel* m, float& frame) {
		zCModelAniActive* best = nullptr;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni || a->isFadingOut)
				continue;
			if (!best || a->protoAni->layer < best->protoAni->layer)
				best = a;
		}
		if (!best)
			return -1;
		frame = best->actFrame;
		return best->protoAni->aniID;
	}

	void TrSetPuppetPose(oCNpc* npc, float x, float y, float z, float yaw, int aniId, float frame, int mode) {
		ZPROF_SCOPE("Freeze.Pose");   // jedna marionetka (wywolan/klatke = postacie w buforze)
		// uspione martwe cialo (poza lista aktywnych vobow) nie przelicza klatek; tylko martwe - bohater
		// raportuje sen 0 co klatke i nie wymaga budzenia (log 16.10)
		if (npc->sleepingMode == zVOB_SLEEPING && npc->IsDead()) {
			ZDebug("Puppet: budze uspione cialo %s", npc->GetInstanceName().ToChar());
			npc->SetSleeping(FALSE);
		}
		npc->SetPositionWorld(zVEC3(x, y, z));
		npc->SetHeadingAtWorld(zVEC3(sinf(yaw), 0.0f, cosf(yaw)));
		if (mode == 0 || aniId < 0)
			return;
		zCModel* m = npc->GetModel();
		if (!m)
			return;
		zCModelAni* ani = m->GetAniFromAniID(aniId);
		if (!ani)
			return;
		zCModelAniActive* a = m->GetActiveAni(ani);
		if (!a || a->isFadingOut) {
			// Przy zatrzymanym czasie modelu (timeScale 0) wagi mieszania nie rosna (CalcWeights 0x57F470:
			// krok = timeScale * frameTime), wiec nowa animacja zostalaby niewidoczna, a stara zamrozona.
			// Warstwa od nowa i pelna waga od razu.
			m->StopAnisLayerRange(ani->layer, ani->layer);
			m->StartAni(ani, 0);
			a = m->GetActiveAni(ani);
			if (a)
				for (int n = 0; n < m->nodeList.GetNum(); n++) {
					zCModelNodeInst* node = m->nodeList[n];
					if (!node)
						continue;
					for (int k = 0; k < node->numNodeAnis && k < zMDL_MAX_ANIS_PARALLEL; k++)
						if (node->nodeAniList[k].modelAni == a) {
							node->nodeAniList[k].weight = 1.0f;
							node->nodeAniList[k].weightSpeed = 0.0f;
							node->nodeAniList[k].blendState = zCModelNodeInst::zMDL_BLEND_STATE_CONST;
						}
				}
			trPuppetAniSwitches++;
		}
		if (a)
			a->SetActFrame(frame);
	}
}
