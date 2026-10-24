// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct MobEvent {
		float t;
		oCMobInter* mob;   // AddRef
		int from, to;
	};
	static std::vector<MobEvent> mobEvents;

	HOOK Ivk_oCMobInter_StartStateChange AS(&oCMobInter::StartStateChange, &oCMobInter::StartStateChange_Union);
	void oCMobInter::StartStateChange_Union(oCNpc* npc, int from, int to) {
		THISCALL(Ivk_oCMobInter_StartStateChange)(npc, from, to);
		ZPROF_SCOPE("Mobs.Hook");
		if (RewindActive() || from == to)
			return;
		if (!zDYNAMIC_CAST<oCMobDoor>(this) && !zDYNAMIC_CAST<oCMobContainer>(this))
			return;
		AddRef();
		mobEvents.push_back({ TrNow(), this, from, to });
		ZDebug("Mobs: %s %d -> %d", GetObjectName().ToChar(), from, to);
	}

	void MobsRevertAfter(float t) {
		ZPROF_SCOPE("Mobs.Revert");
		while (!mobEvents.empty() && mobEvents.back().t > t) {
			MobEvent e = mobEvents.back();
			mobEvents.pop_back();
			ZDebug("Mobs: cofam %s %d -> %d (swiat %d)", e.mob->GetObjectName().ToChar(), e.to, e.from, e.mob->homeWorld ? 1 : 0);
			if (e.mob->homeWorld) {
				e.mob->state = e.from;
				e.mob->state_target = e.from;
				if (zCModel* m = zDYNAMIC_CAST<zCModel>(e.mob->GetVisual())) {
					m->StopAnisLayerRange(0, 999);
					m->StartAni(zSTRING("S_S") + zSTRING(e.from), 0);
				}
			}
			e.mob->Release();
		}
	}

	void MobsTrim(float oldest) {
		size_t drop = 0;
		while (drop < mobEvents.size() && mobEvents[drop].t < oldest) {
			mobEvents[drop].mob->Release();
			drop++;
		}
		if (drop)
			mobEvents.erase(mobEvents.begin(), mobEvents.begin() + drop);
	}

	void MobsReset() {
		for (auto& e : mobEvents)
			e.mob->Release();
		mobEvents.clear();
	}
}
