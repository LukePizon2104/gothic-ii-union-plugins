// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// ---------- konwersje (nazwy z przedrostkiem Pry - jeden TU z reszta pluginu) ----------
	static zdcore::Vec3 PryCore(const zVEC3& v) {
		return zdcore::Vec3(v.n[0], v.n[1], v.n[2]);
	}

	static zVEC3 PryZ(const zdcore::Vec3& v) {
		return zVEC3(v.x, v.y, v.z);
	}

	static zdcore::Mat34 PryMat(const zMAT4& m) {
		zdcore::Mat34 r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = m.v[i].n[j];
		return r;
	}

	// Wiersz 3 (0 0 0 1) zostaje.
	static void PryStore(zMAT4& m, const zdcore::Mat34& c) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				m.v[i].n[j] = c.m[i][j];
	}

	static float PryNow() {
		return ztimer ? ztimer->totalTimeFloat / 1000.0f : 0.0f;
	}

	static float PryDist(const zVEC3& a, const zVEC3& b) {
		float dx = a.n[0] - b.n[0], dy = a.n[1] - b.n[1], dz = a.n[2] - b.n[2];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	// ---------- emitery czasteczek (budowane w kodzie, jak ParseParticleFXScript: new + pola skryptu + UpdateInternals) ----------
	enum { PRY_SPARKS, PRY_FLASH, PRY_SPLINTERS, PRY_EMITTER_COUNT };
	static zCParticleEmitter* pryEmitters[PRY_EMITTER_COUNT] = {};
	static float pryEmitterLife[PRY_EMITTER_COUNT] = {};
	static bool pryEmittersBuilt = false;

	static zCParticleEmitter* PryMakeEmitter(const zdcore::EmitterSpec& s) {
		zCParticleEmitter* e = new zCParticleEmitter();   // nigdy nie zwalniany (jak presety ze skryptow)
		e->ppsValue = s.ppsValue;
		e->ppsScaleKeys_S = s.ppsScaleKeys.c_str();
		e->ppsIsLooping = s.ppsIsLooping;
		e->ppsIsSmooth = s.ppsIsSmooth;
		e->ppsFPS = s.ppsFPS;
		e->ppsCreateEm_S = "";
		e->ppsCreateEmDelay = 0.0f;
		e->shpType_S = s.shpType.c_str();
		e->shpFOR_S = s.shpFOR.c_str();
		e->shpOffsetVec_S = s.shpOffsetVec.c_str();
		e->shpDistribType_S = s.shpDistribType.c_str();
		e->shpDistribWalkSpeed = 0.0f;
		e->shpIsVolume = s.shpIsVolume;
		e->shpDim_S = s.shpDim.c_str();
		e->shpMesh_S = "";
		e->shpMeshRender_B = 0;
		e->shpScaleKeys_S = "";
		e->shpScaleIsLooping = 0;
		e->shpScaleIsSmooth = 0;
		e->shpScaleFPS = 0.0f;
		e->dirMode_S = s.dirMode.c_str();
		e->dirFOR_S = s.dirFOR.c_str();
		e->dirModeTargetFOR_S = "";
		e->dirModeTargetPos_S = "0 0 0";
		e->dirAngleHead = s.dirAngleHead;
		e->dirAngleHeadVar = s.dirAngleHeadVar;
		e->dirAngleElev = s.dirAngleElev;
		e->dirAngleElevVar = s.dirAngleElevVar;
		e->velAvg = s.velAvg;
		e->velVar = s.velVar;
		e->lspPartAvg = s.lspPartAvg;
		e->lspPartVar = s.lspPartVar;
		e->flyGravity_S = s.flyGravity.c_str();
		e->flyCollDet_B = s.flyCollDet;
		e->visName_S = s.visName.c_str();
		e->visOrientation_S = s.visOrientation.c_str();
		e->visTexIsQuadPoly = s.visTexIsQuadPoly;
		e->visTexAniFPS = s.visTexAniFPS;
		e->visTexAniIsLooping = s.visTexAniIsLooping;
		e->visTexColorStart_S = s.visTexColorStart.c_str();
		e->visTexColorEnd_S = s.visTexColorEnd.c_str();
		e->visSizeStart_S = s.visSizeStart.c_str();
		e->visSizeEndScale = s.visSizeEndScale;
		e->visAlphaFunc_S = s.visAlphaFunc.c_str();
		e->visAlphaStart = s.visAlphaStart;
		e->visAlphaEnd = s.visAlphaEnd;
		e->trlFadeSpeed = 0.0f;
		e->trlTexture_S = "";
		e->trlWidth = 0.0f;
		e->mrkFadeSpeed = 0.0f;
		e->mrkTexture_S = "";
		e->mrkSize = 0.0f;
		e->m_flockMode_S = "";
		e->m_fFlockWeight = 0.0f;
		e->m_bSlowLocalFOR = 0;
		e->m_timeStartEnd_S = "";
		e->m_bIsAmbientPFX = 0;
		e->particleFXName = s.name.c_str();
		e->UpdateInternals();
		return e;
	}

	static zCParticleEmitter* PryEmitter(int which) {
		if (!pryEmittersBuilt) {
			pryEmittersBuilt = true;
			const zdcore::EmitterSpec specs[PRY_EMITTER_COUNT] = { zdcore::ParrySparksSpec(), zdcore::ParryFlashSpec(), zdcore::ParrySplintersSpec() };
			for (int i = 0; i < PRY_EMITTER_COUNT; i++) {
				pryEmitters[i] = PryMakeEmitter(specs[i]);
				pryEmitterLife[i] = zdcore::EmitterLifetime(specs[i]);
			}
			ZLog("Blok: emitery %s / %s / %s gotowe (iskry ~%.0f, drzazgi ~%.0f czasteczek)", specs[0].name.c_str(),
				specs[1].name.c_str(), specs[2].name.c_str(), zdcore::EmitterBurstCount(specs[0]), zdcore::EmitterBurstCount(specs[2]));
		}
		return which >= 0 && which < PRY_EMITTER_COUNT ? pryEmitters[which] : nullptr;
	}

	// ---------- efekty w swiecie ----------
	struct PryFxVob {
		zCVob* vob;   // nasz ref
		float life;
	};
	static std::vector<PryFxVob> pryFxVobs;
	static const int PRY_MAX_FX = 24;

	static void PryRemoveVob(zCVob* vob) {
		if (vob->homeWorld)
			vob->homeWorld->RemoveVob(vob);
		vob->Release();
	}

	// Jak oCAniCtrl_Human::DoSparks (0x6AFC50): vob bez kolizji z PFX, emiter startowany po AddVob. Vob usuwamy sami po
	// czasie zycia efektu (dontKillPFXWhenDone), bez zapisu do archiwum.
	static void PrySpawn(int which, const zdcore::Vec3& pos) {
		zCParticleEmitter* em = PryEmitter(which);
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!em || !world)
			return;
		if ((int)pryFxVobs.size() >= PRY_MAX_FX) {
			PryRemoveVob(pryFxVobs.front().vob);
			pryFxVobs.erase(pryFxVobs.begin());
		}
		zCParticleFX* pfx = new zCParticleFX();
		zCVob* vob = new zCVob();
		vob->SetVobName("ZDISM_PARRY");
		vob->SetCollDetStat(FALSE);
		vob->SetCollDetDyn(FALSE);
		vob->dontWriteIntoArchive = TRUE;
		vob->SetPositionWorld(PryZ(pos));
		vob->SetVisual(pfx);
		world->AddVob(vob);
		pfx->SetAndStartEmitter(em, FALSE);
		pfx->dontKillPFXWhenDone = TRUE;
		pfx->Release();   // trzyma go vob
		pryFxVobs.push_back({ vob, pryEmitterLife[which] });
	}

	// ---------- odrzut broni i rak ----------
	struct PryJoint {
		int node;
		zdcore::Vec3 axisModel;   // os obrotu w ukladzie modelu (z chwili bloku)
		float weight;
		zdcore::RecoilNodeState st;
	};

	struct PryRecoil {
		float t0 = 0.0f;
		float duration = 0.0f;
		float amp = 0.0f;         // stopnie w szczycie przy wadze 1
		bool done = false;
		std::vector<PryJoint> joints;
	};

	static std::unordered_map<zCModel*, PryRecoil> pryRecoils;   // model trzymany przez AddRef

	// Przywraca poze z animacji tam, gdzie w wezle zostala nasza macierz (silnik jej w tej klatce nie przeliczyl).
	static void PryRestoreJoints(zCModel* model, PryRecoil& r) {
		auto& nodes = model->nodeList;
		for (auto& j : r.joints) {
			if (j.node < 0 || j.node >= nodes.GetNum())
				continue;
			zdcore::Mat34 out;
			if (zdcore::RecoilRestore(j.st, PryMat(nodes[j.node]->trafo), out))
				PryStore(nodes[j.node]->trafo, out);
		}
	}

	void ParryRecoilApply(zCModel* model) {
		if (pryRecoils.empty())
			return;
		auto it = pryRecoils.find(model);
		if (it == pryRecoils.end() || it->second.done)
			return;
		ZPROF_SCOPE("Anis.Recoil");
		PryRecoil& r = it->second;
		float t = PryNow() - r.t0;
		if (t < 0.0f || t >= r.duration) {
			PryRestoreJoints(model, r);
			r.done = true;
			return;
		}
		float f = zdcore::RecoilCurve(t, r.duration);
		auto& nodes = model->nodeList;
		// rodzic przed dzieckiem (kolejnosc z RecoilJoints) - os liczona w juz obroconym ukladzie rodzica
		for (auto& j : r.joints) {
			if (j.node < 0 || j.node >= nodes.GetNum())
				continue;
			zCModelNodeInst* n = nodes[j.node];
			zdcore::Mat34 out = zdcore::RecoilBase(j.st, PryMat(n->trafo));
			zdcore::Mat34 parent = n->parentNode ? PryMat(model->GetTrafoNodeToModel(n->parentNode)) : zdcore::Mat34::Identity();
			zdcore::RotateBasis(out, zdcore::AxisInParent(parent, j.axisModel), r.amp * j.weight * f);
			PryStore(n->trafo, out);
			zdcore::RecoilWritten(j.st, PryMat(n->trafo));
		}
	}

	static int PryStartRecoil(oCNpc* npc, bool defender, const zdcore::Vec3& contact, const zdcore::Vec3& attToDef) {
		zCModel* m = npc ? npc->GetModel() : nullptr;
		if (!m || npc->IsDead() || zcfg.parryRecoilDeg <= 0.0f)
			return 0;
		int wm = npc->GetWeaponMode();
		bool twoHanded = (npc == player ? AniSwapAniWeaponMode(wm) : wm) == NPC_WEAPON_2HS;   // chwyt wg animacji (zamiana 1H <-> 2H)
		zdcore::Vec3 push = zdcore::RecoilPush(attToDef, defender);
		zdcore::Mat34 objToWorld = PryMat(npc->trafoObjToWorld);
		PryRecoil r;
		r.t0 = PryNow();
		r.duration = zcfg.parryRecoilTime * zdcore::RecoilRoleDuration(defender);
		r.amp = zcfg.parryRecoilDeg * zdcore::RecoilRoleScale(defender);
		for (auto& rj : zdcore::RecoilJoints(twoHanded)) {
			int idx = FindNode(m, rj.node);
			if (idx < 0)
				continue;
			zdcore::Vec3 joint = PryCore(npc->GetTrafoModelNodeToWorld(m->nodeList[idx]).GetTranslation());
			zdcore::Vec3 axisWorld = zdcore::RecoilAxis(joint, contact, push);
			if (zdcore::Length(axisWorld) < 0.5f)
				continue;
			PryJoint pj;
			pj.node = idx;
			pj.axisModel = zdcore::Rotate(zdcore::Transposed3(objToWorld), axisWorld);
			pj.weight = rj.weight;
			r.joints.push_back(pj);
		}
		if (r.joints.empty() || r.duration <= 0.0f)
			return 0;
		auto it = pryRecoils.find(m);
		if (it != pryRecoils.end()) {
			// kolejny blok w trakcie odrzutu: stan wezlow (baza, nasza macierz) przechodzi dalej, reszta wraca do animacji
			PryRecoil& old = it->second;
			for (auto& nj : r.joints)
				for (auto& oj : old.joints)
					if (oj.node == nj.node)
						nj.st = oj.st;
			for (auto& oj : old.joints) {
				bool kept = false;
				for (auto& nj : r.joints)
					kept = kept || nj.node == oj.node;
				if (!kept && oj.node < m->nodeList.GetNum()) {
					zdcore::Mat34 out;
					if (zdcore::RecoilRestore(oj.st, PryMat(m->nodeList[oj.node]->trafo), out))
						PryStore(m->nodeList[oj.node]->trafo, out);
				}
			}
			old = r;
		}
		else {
			m->AddRef();
			pryRecoils[m] = r;
		}
		return (int)r.joints.size();
	}

	// ---------- blok ----------
	// Animacja parady broniacego i nakladka (.MDS), z ktorej gra (najwyzsza, ktora ma ten sam zCModelAni).
	static bool PryParadeAni(oCNpc* npc, std::string& ani, std::string& overlay) {
		// bez homeWorld GetModel rozpakowuje model (UnShrink); w swiecie bez modelu/sterownika - nic do sprawdzenia
		oCAniCtrl_Human* ac = npc && npc->homeWorld ? npc->GetAnictrl() : nullptr;
		zCModel* m = ac ? npc->GetModel() : nullptr;
		zCModelAniActive* a = m ? ac->IsParadeRunning() : nullptr;
		if (!a || !a->protoAni)
			return false;
		ani = a->protoAni->aniName.ToChar();
		overlay = ProtoOfAniName(m, a->protoAni);   // nie SearchAni - crash NB na pustych wpisach nakladki
		return true;
	}

	static oCItem* PryMeleeWeapon(oCNpc* npc) {
		int wm = npc ? npc->GetWeaponMode() : 0;
		return wm >= NPC_WEAPON_DAG && wm <= NPC_WEAPON_2HS ? npc->GetWeapon() : nullptr;
	}

	// Glownia broni w rece: slot ZS_RIGHTHAND, dlugosc i zwrot z bboxa modelu broni (os X slotu), bez modelu - z zasiegu.
	static bool PryBlade(oCNpc* npc, oCItem* weapon, zdcore::Blade& b) {
		zCModel* m = npc ? npc->GetModel() : nullptr;
		int node = m ? FindNode(m, "ZS_RIGHTHAND") : -1;
		if (node < 0)
			return false;
		zMAT4 w = npc->GetTrafoModelNodeToWorld(m->nodeList[node]);
		float minX = 0.0f, maxX = 0.0f, len = 0.0f, sign = -1.0f;
		zCVisual* vis = weapon ? weapon->GetVisual() : nullptr;
		if (vis) {
			zTBBox3D bb = vis->GetBBox3D();
			minX = bb.mins.n[0];
			maxX = bb.maxs.n[0];
		}
		zdcore::BladeFromBox(minX, maxX, weapon ? weapon->range : 0, len, sign);
		b.hand = PryCore(w.GetTranslation());
		b.dir = zdcore::Normalize(zdcore::Vec3(w.v[0].n[0], w.v[1].n[0], w.v[2].n[0])) * sign;
		b.length = len;
		return true;
	}

	static std::string PryVisualName(oCItem* weapon) {
		if (!weapon)
			return "";
		if (weapon->file.Length() > 0)
			return weapon->file.ToChar();
		zCVisual* v = weapon->GetVisual();
		return v ? std::string(v->GetVisualName().ToChar()) : std::string();
	}

	static zdcore::ParryMat PryMatOf(oCItem* weapon) {
		if (!weapon)
			return zdcore::ParryMat::NoWeapon;
		return zdcore::ClassifyParryMat(true, (int)weapon->sndMat, weapon->GetInstanceName().ToChar(), PryVisualName(weapon),
			zcfg.parryWoodTokens);
	}

	// forced: test z konsoli (bez sprawdzania animacji parady).
	static std::string PryOnBlock(oCNpc* attacker, oCNpc* defender, bool forced) {
		if (!attacker || !defender || attacker == defender)
			return "brak napastnika albo broniacego";
		if (!attacker->homeWorld || !defender->homeWorld)   // poza swiatem (usuniety / w trakcie wczytania) - bez modelu
			return "postac poza swiatem";
		if (!zcfg.parryFx && !zcfg.parryRecoil)
			return "ParryFx=0 i ParryRecoil=0";
		std::string ani = "-", overlay = "-";
		if (!forced) {
			if (!PryParadeAni(defender, ani, overlay))
				return "brak animacji parady";
			if (!zdcore::IsBlockParade(ani, overlay)) {
				ZDebug("Blok: %s -> %s, %s [%s] to odskok/unik - bez efektu", attacker->GetInstanceName().ToChar(),
					defender->GetInstanceName().ToChar(), ani.c_str(), overlay.c_str());
				return "odskok/unik - bez efektu";
			}
		}
		zCVob* cam = ogame ? ogame->GetCameraVob() : nullptr;
		if (cam && PryDist(cam->GetPositionWorld(), defender->GetPositionWorld()) > 4000.0f)
			return "za daleko od kamery";
		oCItem* wa = PryMeleeWeapon(attacker);
		oCItem* wd = PryMeleeWeapon(defender);
		zdcore::Blade ba, bd;
		bool hasA = PryBlade(attacker, wa, ba), hasD = PryBlade(defender, wd, bd);
		zdcore::Vec3 contact;
		if (hasA && hasD)
			contact = zdcore::ParryContactPoint(ba, bd);
		else if (hasD)
			contact = bd.hand + bd.dir * (bd.length * 0.6f);
		else if (hasA)
			contact = ba.hand + ba.dir * (ba.length * 0.7f);
		else
			contact = (PryCore(attacker->GetPositionWorld()) + PryCore(defender->GetPositionWorld())) * 0.5f + zdcore::Vec3(0.0f, 60.0f, 0.0f);
		zdcore::Vec3 attToDef = PryCore(defender->GetPositionWorld()) - PryCore(attacker->GetPositionWorld());
		zdcore::ParryMat ma = PryMatOf(wa), md = PryMatOf(wd);
		uint32_t fx = zcfg.parryFx ? zdcore::ParryFx(ma, md) : zdcore::PARRY_FX_NONE;
		if (fx & zdcore::PARRY_FX_FLASH)
			PrySpawn(PRY_FLASH, contact);
		if (fx & zdcore::PARRY_FX_SPARKS)
			PrySpawn(PRY_SPARKS, contact);
		if (fx & zdcore::PARRY_FX_SPLINTERS)
			PrySpawn(PRY_SPLINTERS, contact);
		int nd = 0, na = 0;
		if (zcfg.parryRecoil) {
			nd = PryStartRecoil(defender, true, contact, attToDef);
			na = PryStartRecoil(attacker, false, contact, attToDef);
		}
		char buf[320];
		sprintf_s(buf, "%s -> %s: %s [%s], bron %s (%s) / %s (%s), efekt %u, punkt %.0f %.0f %.0f, odrzut wezlow %d/%d",
			attacker->GetInstanceName().ToChar(), defender->GetInstanceName().ToChar(), ani.c_str(), overlay.c_str(),
			wa ? wa->GetInstanceName().ToChar() : "-", zdcore::ParryMatName(ma), wd ? wd->GetInstanceName().ToChar() : "-",
			zdcore::ParryMatName(md), fx, contact.x, contact.y, contact.z, nd, na);
		ZDebug("Blok: %s", buf);
		return buf;
	}

	// oCAniCtrl_Human::StartParadeEffects(oCNpc* broniacy) - this = sterownik napastnika (wolane tylko z HitCombo, gdy cel
	// sparowal cios). Oryginal zostaje (dzwiek zderzenia, czasteczki silnika). Hak po adresie (w UserAPI brak deklaracji _Union).
	static void __fastcall PryStartParadeEffects(oCAniCtrl_Human* self, void* edx, oCNpc* defender);
	static CInvoke<void(__fastcall*)(oCAniCtrl_Human*, void*, oCNpc*)> Ivk_oCAniCtrl_Human_StartParadeEffects(
		ENGINE == Engine_G2A ? 0x006B16F0 : 0, &PryStartParadeEffects, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall PryStartParadeEffects(oCAniCtrl_Human* self, void* edx, oCNpc* defender) {
		ZHookHit(ZH_STARTPARADEFX);
		Ivk_oCAniCtrl_Human_StartParadeEffects(self, edx, defender);
		if (zcfg.enabled && self && self->npc && defender) {
			ZPROF_SCOPE("Parry.Block");   // iskry / drzazgi i odrzut po bloku (z HitCombo napastnika)
			PryOnBlock(self->npc, defender, false);
		}
	}

	void ParryFxFlash(const zVEC3& pos) {
		PrySpawn(PRY_FLASH, PryCore(pos));
		PrySpawn(PRY_SPARKS, PryCore(pos));
	}

	void ParryUpdate(float dt) {
		for (auto it = pryFxVobs.begin(); it != pryFxVobs.end();) {
			it->life -= dt;
			if (it->life <= 0.0f) {
				PryRemoveVob(it->vob);
				it = pryFxVobs.erase(it);
			}
			else
				++it;
		}
		if (pryRecoils.empty())
			return;
		float now = PryNow();
		for (auto it = pryRecoils.begin(); it != pryRecoils.end();) {
			PryRecoil& r = it->second;
			// koniec w AdvanceAnis albo model bez animacji (NPC poza widokiem, usuniety) / cofniety zegar (wczytanie)
			if (r.done || now - r.t0 > r.duration + 1.0f || now < r.t0 - 1.0f) {
				if (!r.done)
					PryRestoreJoints(it->first, r);
				it->first->Release();
				it = pryRecoils.erase(it);
			}
			else
				++it;
		}
	}

	void ParryClearAll() {
		for (auto& f : pryFxVobs)
			PryRemoveVob(f.vob);
		pryFxVobs.clear();
		for (auto& kv : pryRecoils) {
			PryRestoreJoints(kv.first, kv.second);
			kv.first->Release();
		}
		pryRecoils.clear();
	}

	std::string ParryTest(oCNpc* attacker, oCNpc* defender) {
		return PryOnBlock(attacker, defender, true);
	}
}
