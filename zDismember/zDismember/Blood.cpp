// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct TimedVob {
		zCVob* vob;    // nasz ref
		float life;
	};

	struct Stump {
		oCNpc* npc;    // AddRef
		int node;
		float left, timer;
	};

	static std::vector<TimedVob> bloodVobs;
	static std::vector<Stump> bloodStumps;

	bool BloodEnabled() {
		return oCNpc::modeBlood != oCNpc::oEBloodMode_None;
	}

	static void RemoveVob(zCVob* vob) {
		if (vob->homeWorld)
			vob->homeWorld->RemoveVob(vob);
		vob->Release();
	}

	static zdcore::Vec3 ToCore(const zVEC3& v) {
		return zdcore::Vec3(v.n[0], v.n[1], v.n[2]);
	}

	// Kierunek wytrysku: preferowany, ale nigdy w kamere (emiter PFX_BLOOD strzela wzdluz osi voba z predkoscia do
	// 6 m/s - nowy vob ma obrot jednostkowy, wiec krew zawsze leciala w -X swiata i przy kamerze po tej stronie
	// czasteczki 4x8 cm zaslanialy pol ekranu).
	static zdcore::Mat34 SprayTrafo(const zVEC3& pos, const zVEC3* preferred) {
		zdcore::Vec3 p = ToCore(pos), camToHit;
		zCVob* cam = ogame ? ogame->GetCameraVob() : nullptr;
		if (cam)
			camToHit = p - ToCore(cam->GetPositionWorld());
		zdcore::Vec3 dir = zdcore::SprayDirection(preferred ? ToCore(*preferred) : zdcore::Vec3(), camToHit);
		return zdcore::SprayBasis(dir, p);
	}

	static zMAT4 ToZMat(const zdcore::Mat34& m) {
		zMAT4 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.v[i].n[j] = m.m[i][j];
		r.v[3].n[0] = r.v[3].n[1] = r.v[3].n[2] = 0.0f;
		r.v[3].n[3] = 1.0f;
		return r;
	}

	zCVob* BloodBurst(const zVEC3& pos, float lifetime, const zVEC3* dir) {
		if (!BloodEnabled() || !ogame || !ogame->GetGameWorld())
			return nullptr;
		zCVob* vob = new zCVob();
		vob->SetVobName("ZDISM_BLOOD");
		vob->SetVisual("PFX_BLOOD.PFX");
		vob->SetTrafoObjToWorld(ToZMat(SprayTrafo(pos, dir)));   // przed AddVob - pierwsze czasteczki juz w dobra strone
		ogame->GetGameWorld()->AddVob(vob);
		bloodVobs.push_back({ vob, lifetime });
		return vob;
	}

	void BloodPool(oCNpc* npc) {
		if (!BloodEnabled() || !npc || !npc->human_ai)
			return;
		zVEC3 pos = npc->GetPositionWorld();
		zVEC3 down(0.0f, -1000.0f, 0.0f);
		npc->human_ai->AddBlood(pos, down, 100.0f, TRUE, &npc->bloodTexture);
	}

	void BloodStump(oCNpc* npc, int nodeIndex, float seconds) {
		if (!BloodEnabled() || !npc || nodeIndex < 0)
			return;
		npc->AddRef();
		bloodStumps.push_back({ npc, nodeIndex, seconds, 0.0f });
	}

	void BloodUpdate(float dt) {
		for (auto it = bloodVobs.begin(); it != bloodVobs.end();) {
			it->life -= dt;
			if (it->life <= 0.0f) {
				RemoveVob(it->vob);
				it = bloodVobs.erase(it);
			}
			else
				++it;
		}
		for (auto it = bloodStumps.begin(); it != bloodStumps.end();) {
			Stump& s = *it;
			s.left -= dt;
			s.timer -= dt;
			zCModel* model = s.npc->GetModel();
			bool alive = s.left > 0.0f && s.npc->homeWorld && model && s.node < model->nodeList.GetNum();
			if (alive && s.timer <= 0.0f) {
				s.timer = 0.35f;
				BloodBurst(s.npc->GetTrafoModelNodeToWorld(model->nodeList[s.node]).GetTranslation(), 2.0f);
			}
			if (!alive) {
				s.npc->Release();
				it = bloodStumps.erase(it);
			}
			else
				++it;
		}
	}

	int BloodStumpsRemove(oCNpc* npc, int nodeIndex) {
		int removed = 0;
		for (auto it = bloodStumps.begin(); it != bloodStumps.end();) {
			if (npc && it->npc == npc && (nodeIndex < 0 || it->node == nodeIndex)) {
				it->npc->Release();
				it = bloodStumps.erase(it);
				removed++;
			}
			else
				++it;
		}
		return removed;
	}

	void BloodClearAll() {
		for (auto& t : bloodVobs)
			RemoveVob(t.vob);
		bloodVobs.clear();
		for (auto& s : bloodStumps)
			s.npc->Release();
		bloodStumps.clear();
	}

	// ---------- Krew silnika przy trafieniu ----------
	// oCNpc::OnDamage_Effects_Start (0x66EE40) stawia czasteczki krwi (bloodEmitter, zwykle PFX_BLOOD) w punkcie
	// trafienia (desc.vecLocationHit, 0x66FFC8) nowym vobem bez obrotu, potem SetAndStartEmitter (0x670005). Gdy
	// trafiony jest bohater, kamera stoi 2-3 m za nim - krew lecaca w -X swiata wpadala w nia (wielkie plamy na
	// ekranie przy wymianie ciosow, takze przy kontrach na niesmiertelnym, bo krwawi wtedy bohater, nie cel:
	// niesmiertelny (NPC_FLAG_IMMORTAL) w ogole nie krwawi - warunek 0x66F871). Na czas Effects_Start pamietamy
	// ofiare i kierunek ciosu, a wystartowany wtedy emiter krwi obracamy: od napastnika, nigdy w kamere.
	// WYLACZONE (15.10): plamy przed kamera u gracza robi zewnetrzny submod (efekt krwi na ekranie przy obrazeniach
	// bohatera), nie krew silnika - haki zostaja w kodzie, ale bez instalacji (true = obracanie krwi silnika od kamery).
	static const bool ENGINE_BLOOD_REDIRECT = false;
	static oCNpc* engineBloodVictim = nullptr;
	static zVEC3 engineBloodDir;
	static bool engineBloodHasDir = false;

	static void __fastcall Hk_OnDamage_Effects_Start(oCNpc* self, void* edx, oCNpc::oSDamageDescriptor& desc);
	static CInvoke<void(__fastcall*)(oCNpc*, void*, oCNpc::oSDamageDescriptor&)> Ivk_oCNpc_OnDamage_Effects_Start(
		ENGINE == Engine_G2A ? 0x0066EE40 : ENGINE == Engine_G2 ? 0x00785B80 : ENGINE == Engine_G1A ? 0x007791F0 : 0x00739A20,
		&Hk_OnDamage_Effects_Start, ENGINE_BLOOD_REDIRECT ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall Hk_OnDamage_Effects_Start(oCNpc* self, void* edx, oCNpc::oSDamageDescriptor& desc) {
		oCNpc* prevVictim = engineBloodVictim;
		zVEC3 prevDir = engineBloodDir;
		bool prevHasDir = engineBloodHasDir;
		engineBloodVictim = zcfg.enabled ? self : nullptr;
		engineBloodHasDir = false;
		zCVob* att = desc.pNpcAttacker ? (zCVob*)desc.pNpcAttacker : desc.pVobAttacker;
		if (self && att && att != self) {
			zVEC3 d = self->GetPositionWorld() - att->GetPositionWorld();
			d.n[1] = 0.0f;
			if (d.n[0] * d.n[0] + d.n[2] * d.n[2] > 1.0f) {
				engineBloodDir = d;
				engineBloodHasDir = true;
			}
		}
		Ivk_oCNpc_OnDamage_Effects_Start(self, edx, desc);
		engineBloodVictim = prevVictim;
		engineBloodDir = prevDir;
		engineBloodHasDir = prevHasDir;
	}

	static int __fastcall Hk_zCParticleFX_SetAndStartEmitter(zCParticleFX* self, void* edx, const zSTRING& name, int arg);
	static CInvoke<int(__fastcall*)(zCParticleFX*, void*, const zSTRING&, int)> Ivk_zCParticleFX_SetAndStartEmitter(
		ENGINE == Engine_G2A ? 0x005AED40 : ENGINE == Engine_G2 ? 0x005A9660 : ENGINE == Engine_G1A ? 0x005AA400 : 0x0058ED00,
		&Hk_zCParticleFX_SetAndStartEmitter, ENGINE_BLOOD_REDIRECT ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall Hk_zCParticleFX_SetAndStartEmitter(zCParticleFX* self, void* edx, const zSTRING& name, int arg) {
		int r = Ivk_zCParticleFX_SetAndStartEmitter(self, edx, name, arg);
		oCNpc* victim = engineBloodVictim;
		if (victim && self && self->connectedVob && _stricmp(name.ToChar(), victim->bloodEmitter.ToChar()) == 0) {
			zCVob* vob = self->connectedVob;
			zVEC3 pos = vob->GetPositionWorld();
			vob->SetTrafoObjToWorld(ToZMat(SprayTrafo(pos, engineBloodHasDir ? &engineBloodDir : nullptr)));
			if (victim == player)
				ZDebug("Krew silnika na bohaterze (%s): wytrysk obrocony od kamery", name.ToChar());
		}
		return r;
	}

	// Plama krwi silnika (zCAIPlayer::AddBlood z Effects_Start, 0x670350): promien z pozycji ofiary w kierunku od
	// napastnika x bloodDistance (10 m), rozmiar do ~110. U bohatera promien leci prosto w kamere i na pochylym
	// terenie plama ladowala na ziemi tuz przed nia (pol ekranu czerwone). Skladowa pozioma w strone kamery odbijamy.
	static void __fastcall Hk_zCAIPlayer_AddBlood(zCAIPlayer* self, void* edx, const zVEC3& pos, const zVEC3& dir, float size, int flow, zSTRING* tex);
	static CInvoke<void(__fastcall*)(zCAIPlayer*, void*, const zVEC3&, const zVEC3&, float, int, zSTRING*)> Ivk_zCAIPlayer_AddBlood(
		ENGINE == Engine_G2A ? 0x0050F8C0 : ENGINE == Engine_G2 ? 0x0050CB10 : ENGINE == Engine_G1A ? 0x00513400 : 0x004FFE20,
		&Hk_zCAIPlayer_AddBlood, ENGINE_BLOOD_REDIRECT ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall Hk_zCAIPlayer_AddBlood(zCAIPlayer* self, void* edx, const zVEC3& pos, const zVEC3& dir, float size, int flow, zSTRING* tex) {
		zCVob* cam = engineBloodVictim && ogame ? ogame->GetCameraVob() : nullptr;
		float h = sqrtf(dir.n[0] * dir.n[0] + dir.n[2] * dir.n[2]);
		if (!cam || h < 1.0f) {
			Ivk_zCAIPlayer_AddBlood(self, edx, pos, dir, size, flow, tex);
			return;
		}
		zdcore::Vec3 d = zdcore::SprayDirection(ToCore(dir), ToCore(pos) - ToCore(cam->GetPositionWorld()), 0.0f);
		zVEC3 fixed(d.x * h, dir.n[1], d.z * h);
		Ivk_zCAIPlayer_AddBlood(self, edx, pos, fixed, size, flow, tex);
	}

	// ---------- Ogien na zwlokach ----------
	// Palenie (VOB_BURN z obrazen od ognia, VOB_MAGICBURN z trafienia ognistym pociskiem, ich dzieci przypiete do kosci)
	// gasimy przy smierci: zwloki z ragdollem / finisherem leza tam, gdzie bohater dalej walczy (atak W3 z wypadem
	// wchodzi w cialo) i plomienie wygladaly jak plonacy bohater. Pociski i czary w locie (origin = rzucajacy) zostaja.
	int BurnEffectsStop(oCNpc* npc) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !npc)
			return 0;
		zCArray<zCVob*> list;
		world->SearchVobListByBaseClass(oCVisualFX::classDef, list, nullptr);
		int stopped = 0;
		for (int i = 0; i < list.GetNum(); i++) {
			oCVisualFX* fx = zDYNAMIC_CAST<oCVisualFX>(list[i]);
			if (!fx || fx->GetOrigin() != npc || !zdcore::IsBurnEffectName(fx->GetName().ToChar()))
				continue;
			fx->Stop(TRUE);
			stopped++;
		}
		if (stopped)
			ZDebug("Ogien: zgaszono %d efekt(ow) na %s", stopped, npc->GetInstanceName().ToChar());
		return stopped;
	}
}
