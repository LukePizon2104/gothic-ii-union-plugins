// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static const char* ATK_W3_1H = "HUMANS_ZDF_W3_1H";
	static const char* ATK_W3_2H = "HUMANS_ZDF_W3_2H";
	static const char* ATK_W3_2HS = "HUMANS_ZDF_W3_2HS";
	static const char* ATK_SIDE_ANI[2] = { "T_2HATTACKL", "T_2HATTACKR" };

	// fpsRate z pliku per "ZRODLO|ANIMACJA" (klucz po nazwie jak w sprincie - bez dryfu przy wielokrotnym skalowaniu)
	static std::map<std::string, float> atkBaseFps;
	// prototyp szybkich ciec bocznych 2H: nasza referencja z zCModelPrototype::Load (baza = HUMANS bohatera)
	static zCModelPrototype* atkFastProto = nullptr;
	static zCModelPrototype* atkFastBase = nullptr;
	static bool atkFastTried = false;
	struct AtkSideState {
		zCModelPrototype* proto = nullptr;   // HUMANS_ZDF_W3_2H, w ktorym podmieniamy wpisy
		bool held = false;                   // nasza referencja na nim (gdy wpis rozni sie od pliku)
		zdcore::SideEntry e[2];
	};
	static AtkSideState atkSide;
	static int atkSideLogged = -1;
	struct AtkSlotState {
		const void* ours = nullptr;
		const void* native = nullptr;
		int percent = -999;
		float scale = -1.0f;
		std::string nativeFrom;
		float oursPace = -1.0f, nativePace = -1.0f, oursHit = -1.0f, nativeHit = -1.0f;
	};
	static AtkSlotState atkSlot[zdcore::ATK_SLOT_COUNT];
	static bool atkRestoredOff = false;

	static zCModelPrototype* AtkModelProto(zCModel* m, const char* name) {
		for (int i = 1; m && i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && ProtoKey(p) == name)
				return p;
		}
		return nullptr;
	}

	// Zdarzenia walki animacji przy podanym fps (tag = pierwszy wyraz tagString, wartosci = reszta + napisy zdarzenia).
	static zdcore::AtkEvents AtkEventsOf(zCModelAni* a, float fps) {
		zdcore::AtkEvents e;
		if (!a)
			return e;
		e.frames = a->numFrames;
		e.fps = fps > 0.01f ? fps : 25.0f;
		for (int i = 0; i < a->numAniEvents && a->aniEvents; i++) {
			zCModelAniEvent& ev = a->aniEvents[i];
			if (ev.aniEventType != zMDL_EVENT_TAG)
				continue;
			std::string tag = ev.tagString.ToChar(), rest;
			size_t sp = tag.find_first_of(" \t");
			if (sp != std::string::npos) {
				rest = tag.substr(sp + 1);
				tag = tag.substr(0, sp);
			}
			for (int k = 0; k < zMDL_ANIEVENT_MAXSTRING; k++)
				rest += std::string(" ") + ev.string[k].ToChar();
			zdcore::AtkEventsAddTag(e, tag, rest);
		}
		return e;
	}

	// Animacja, ktora gralaby bez naszych nakladek stylu W3 (od gory listy; HUMANS na koncu).
	static zCModelAni* AtkNativeAni(zCModel* m, int id, std::string& from) {
		for (int i = m->modelProtoList.GetNum() - 1; i >= 0; i--) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p || (i > 0 && ProtoKey(p).rfind("HUMANS_ZDF_W3_", 0) == 0))
				continue;
			zCModelAni* a = ProtoAni(p, id);
			if (a) {
				from = i == 0 ? std::string("HUMANS") : ProtoKey(p);
				return a;
			}
		}
		from.clear();
		return nullptr;
	}

	static bool AtkAniPlaying(zCModel* m, const void* a) {
		for (int i = 0; a && i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++)
			if (m->aniChannels[i] && m->aniChannels[i]->protoAni == a)
				return true;
		return false;
	}

	static float AtkBase(const std::string& key, zCModelAni* a) {
		auto it = atkBaseFps.find(key);
		if (it == atkBaseFps.end())
			it = atkBaseFps.emplace(key, a->fpsRate > 0.0f ? a->fpsRate : 25.0f).first;
		return it->second;
	}

	// Zrodlo animacji do klucza bazy: szybkie ciecie z HUMANS_ZDF_W3_2HS albo nakladka, w ktorej jest wpis.
	static std::string AtkSource(zCModelPrototype* p, zCModelAni* a, int id) {
		if (atkFastProto && a && ProtoAni(atkFastProto, id) == a)
			return ATK_W3_2HS;
		return p ? ProtoKey(p) : std::string("?");
	}

	static void AtkFpsToBase(const std::string& src, zCModelAni* a) {
		if (!a)
			return;
		auto it = atkBaseFps.find(src + "|" + a->aniName.ToChar());
		if (it != atkBaseFps.end() && fabsf(a->fpsRate - it->second) > 1e-4f)
			a->fpsRate = it->second;
	}

	// ---------------------------------------------------------------- ciecia boczne 2H

	static zCModelPrototype* AtkFast(zCModel* m) {
		zCModelPrototype* base = m->modelProtoList.GetNum() > 0 ? m->modelProtoList[0] : nullptr;
		if (atkFastProto && atkFastBase == base)
			return atkFastProto;
		if (atkFastProto)
			return nullptr;   // inna baza bohatera (przemiana) - szybkie ciecia tylko z baza, pod ktora wczytane
		if (atkFastTried || !base)
			return nullptr;
		atkFastTried = true;
		if (!ZOverlayExists(ATK_W3_2HS, "T_2HATTACKL")) {
			ZLog("Tempo ciosow: brak %s (prywatna paczka zDismember_W3.vdf bez szybkich ciec 2H) - ciecia boczne 2H zostaja W3", ATK_W3_2HS);
			return nullptr;
		}
		zCModelPrototype* p = zCModelPrototype::Load(zSTRING((std::string(ATK_W3_2HS) + ".MDS").c_str()), base);
		if (!p) {
			ZLog("Tempo ciosow: silnik nie wczytal %s - ciecia boczne 2H zostaja W3", ATK_W3_2HS);
			return nullptr;
		}
		if (p->baseModelProto != base) {
			ZLog("Tempo ciosow: %s pod inna baza niz model bohatera - pomijam", ATK_W3_2HS);
			p->Release();
			return nullptr;
		}
		atkFastProto = p;
		atkFastBase = base;
		ZLog("Tempo ciosow: wczytano %s (szybkie ciecia boczne 2H)", ATK_W3_2HS);
		return p;
	}

	// Wpis k (T_2HATTACKL / R) w prototypie -> want. Referencje: wstawiona obca animacja +1 (oddawana przy zdjeciu),
	// oryginal z pliku po wyjeciu trzymamy my (jego referencja z tablicy) i oddajemy tablicy przy powrocie.
	static void AtkSideWrite(zCModelPrototype* p, int id, int k, zCModelAni* want) {
		zdcore::SideEntry& e = atkSide.e[k];
		zCModelAni* cur = p->protoAnis.GetArray()[id];
		if (cur && cur != e.orig) {
			AtkFpsToBase(ATK_W3_2HS, cur);
			cur->Release();
		}
		else if (cur)
			AtkFpsToBase(ProtoKey(p), cur);
		if (want && want != e.orig)
			want->AddRef();
		p->protoAnis.GetArray()[id] = want;
		zdcore::SideEntryWrote(e, want);
	}

	static void AtkSideHold() {
		bool changed = atkSide.e[0].changed || atkSide.e[1].changed;
		if (changed && !atkSide.held && atkSide.proto) {
			atkSide.proto->refCtr++;   // jak Load znalezionego prototypu - nie zniknie z podmienionym wpisem
			atkSide.held = true;
		}
		else if (!changed && atkSide.held && atkSide.proto) {
			atkSide.held = false;
			atkSide.proto->Release();   // wpisy jak w pliku (na modelu ma i tak referencje modelu)
		}
	}

	// Wpisy prototypu z powrotem jak w pliku (ignoruje to, co gra - wolane, gdy prototyp znika z modelu / plugin wyl.).
	static void AtkSideRestoreAll(zCModel* m, const char* why) {
		zCModelPrototype* p = atkSide.proto;
		if (!p || !(atkSide.e[0].changed || atkSide.e[1].changed)) {
			atkSide = AtkSideState();
			return;
		}
		for (int k = 0; k < 2; k++) {
			zdcore::SideEntry& e = atkSide.e[k];
			if (!e.changed || !e.known)
				continue;
			// indeks = aniID oryginalu (wpisy nakladki wyrownane wg aniID); bez oryginalu - po nazwie z modelu
			int id = e.orig ? ((zCModelAni*)e.orig)->aniID : m ? AniId(m, ATK_SIDE_ANI[k]) : -1;
			if (id >= 0 && id < p->protoAnis.GetNum() && p->protoAnis.GetArray()[id] == e.placed)
				AtkSideWrite(p, id, k, (zCModelAni*)e.orig);
		}
		ZLog("Tempo ciosow: ciecia boczne 2H %s z pliku (%s)", ATK_W3_2H, why);
		AtkSideHold();
		atkSide = AtkSideState();
	}

	static void AtkSideUpdate(zCModel* m) {
		zCModelPrototype* p2 = AtkModelProto(m, ATK_W3_2H);
		if (!p2)
			return;   // styl W3 2H nie na modelu - nic nie gra z naszej nakladki
		if (atkSide.proto && atkSide.proto != p2)
			AtkSideRestoreAll(m, "inny prototyp nakladki na modelu");
		atkSide.proto = p2;
		int wanted = zcfg.w3Side2H;
		zCModelPrototype* fp = wanted == zdcore::SIDE2H_FAST ? AtkFast(m) : nullptr;
		int eff = zdcore::Side2HEffective(wanted, fp != nullptr);
		if (eff != atkSideLogged) {
			atkSideLogged = eff;
			ZLog("Tempo ciosow: ciecia boczne 2H stylu W3 = %s%s", zdcore::Side2HLabel(eff),
				eff != wanted ? " (szybkich brak - zostaje W3)" : "");
		}
		for (int k = 0; k < 2; k++) {
			int id = AniId(m, ATK_SIDE_ANI[k]);
			if (id < 0 || id >= p2->protoAnis.GetNum())
				continue;
			zCModelAni* cur = p2->protoAnis.GetArray()[id];
			zdcore::SideEntryObserve(atkSide.e[k], cur);
			zCModelAni* want = (zCModelAni*)zdcore::SideEntryWant(atkSide.e[k], eff, ProtoAni(fp, id));
			if (want == cur)
				continue;
			if (AtkAniPlaying(m, cur) || AtkAniPlaying(m, want))
				continue;   // po ciosie (aktywna animacja trzyma wskaznik z tablicy - GetActiveAni(id) by jej nie znalazl)
			AtkSideWrite(p2, id, k, want);
		}
		AtkSideHold();
	}

	// ---------------------------------------------------------------- tempo

	static void AtkScaleSlot(zCModel* m, int slot) {
		AtkSlotState& st = atkSlot[slot];
		const char* aniName = zdcore::AtkSlotAni(slot);
		zCModelPrototype* p = AtkModelProto(m, zdcore::AtkSlotIs2H(slot) ? ATK_W3_2H : ATK_W3_1H);
		int id = p ? AniId(m, aniName) : -1;
		zCModelAni* a = ProtoAni(p, id);
		if (!a) {
			st.ours = nullptr;
			return;
		}
		std::string src = AtkSource(p, a, id);
		float base = AtkBase(src + "|" + aniName, a);
		int percent = zcfg.w3AtkSpeed[slot];
		bool combo = zdcore::AtkSlotIsCombo(slot);
		std::string from;
		zCModelAni* nat = AtkNativeAni(m, id, from);
		if (st.ours != a || st.native != nat || st.percent != percent || st.scale < 0.0f) {
			zdcore::AtkEvents ours = AtkEventsOf(a, base), native = AtkEventsOf(nat, nat ? nat->fpsRate : 25.0f);
			st.oursPace = zdcore::AtkPaceSeconds(ours, combo);
			st.oursHit = zdcore::AtkFirstHitSeconds(ours);
			st.nativePace = nat ? zdcore::AtkPaceSeconds(native, combo) : -1.0f;
			st.nativeHit = nat ? zdcore::AtkFirstHitSeconds(native) : -1.0f;
			float scale = zdcore::AtkTempoScale(percent, st.oursPace, st.nativePace);
			bool show = st.scale < 0.0f || fabsf(scale - st.scale) > 0.005f || st.ours != a;
			st.ours = a;
			st.native = nat;
			st.percent = percent;
			st.scale = scale;
			st.nativeFrom = from;
			if (show)
				ZLog("Tempo ciosow: %s z %s x%.2f (%s) - %s %.2f -> %.2f s, cios %.2f -> %.2f s; natywne %s: %s %.2f s, cios %.2f s; fps %.1f -> %.1f",
					aniName, src.c_str(), scale, percent == zdcore::ATK_SPEED_NATIVE ? "jak natywne" : (std::to_string(percent) + "%").c_str(),
					combo ? "odstep trafien" : "kolejny cios", st.oursPace, st.oursPace / scale, st.oursHit, st.oursHit / scale,
					from.empty() ? "-" : from.c_str(), combo ? "odstep" : "kolejny", st.nativePace, st.nativeHit, base, base * scale);
		}
		float target;
		if (zdcore::AtkFpsTarget(base, st.scale, a->fpsRate, target))
			a->fpsRate = target;
	}

	void AtkTempoUpdate() {
		ZPROF_SCOPE("AtkTempo");
		atkRestoredOff = false;
		zCModel* m = player && player->homeWorld && HeroIsHuman() ? player->GetModel() : nullptr;
		if (!m)
			return;
		AtkSideUpdate(m);
		for (int s = 0; s < zdcore::ATK_SLOT_COUNT; s++)
			AtkScaleSlot(m, s);
	}

	void AtkTempoRestore(const char* why) {
		if (atkRestoredOff)
			return;
		atkRestoredOff = true;
		zCModel* m = player && player->homeWorld && HeroIsHuman() ? player->GetModel() : nullptr;
		AtkSideRestoreAll(m, why);
		int n = 0;
		for (const char* ov : { ATK_W3_1H, ATK_W3_2H }) {
			zCModelPrototype* p = AtkModelProto(m, ov);
			for (int s = 0; p && s < zdcore::ATK_SLOT_COUNT; s++) {
				zCModelAni* a = ProtoAni(p, AniId(m, zdcore::AtkSlotAni(s)));
				if (a) {
					AtkFpsToBase(ov, a);
					n++;
				}
			}
		}
		for (int k = 0; atkFastProto && m && k < 2; k++)
			AtkFpsToBase(ATK_W3_2HS, ProtoAni(atkFastProto, AniId(m, ATK_SIDE_ANI[k])));
		for (auto& s : atkSlot)
			s = AtkSlotState();
		atkSideLogged = -1;
		if (n)
			ZLog("Tempo ciosow: animacje W3 w tempie z pliku (%s)", why);
	}

	void AtkTempoForget() {
		for (auto& s : atkSlot)
			s = AtkSlotState();
		atkSideLogged = -1;
	}

	std::string AtkTempoConsole(const std::string& arg) {
		zCModel* m = player && player->homeWorld && HeroIsHuman() ? player->GetModel() : nullptr;
		if (!m)
			return "bohater bez modelu czlowieka";
		std::string out;
		char buf[400];
		for (int s = 0; s < zdcore::ATK_SLOT_COUNT; s++) {
			const char* aniName = zdcore::AtkSlotAni(s);
			zCModelPrototype* p = AtkModelProto(m, zdcore::AtkSlotIs2H(s) ? ATK_W3_2H : ATK_W3_1H);
			int id = AniId(m, aniName);
			zCModelAni* a = ProtoAni(p, id);
			std::string from;
			zCModelAni* nat = AtkNativeAni(m, id, from);
			bool combo = zdcore::AtkSlotIsCombo(s);
			int pc = zcfg.w3AtkSpeed[s];
			if (!a) {
				sprintf_s(buf, "%s: %s | ", aniName, !p ? "styl W3 nie na modelu" : "pusty wpis (natywne ciecie)");
				out += buf;
				continue;
			}
			zdcore::AtkEvents ours = AtkEventsOf(a, a->fpsRate), native = AtkEventsOf(nat, nat ? nat->fpsRate : 25.0f);
			sprintf_s(buf, "%s [%s, %s] fps %.1f: cios %.2f s, %s %.2f s; natywne %s: cios %.2f s, %s %.2f s | ", aniName,
				AtkSource(p, a, id).c_str() + 14, pc == 0 ? "jak natywne" : (std::to_string(pc) + "%").c_str(), a->fpsRate,
				zdcore::AtkFirstHitSeconds(ours), combo ? "odstep" : "kolejny", zdcore::AtkPaceSeconds(ours, combo),
				from.empty() ? "-" : from.c_str(), nat ? zdcore::AtkFirstHitSeconds(native) : -1.0f, combo ? "odstep" : "kolejny",
				nat ? zdcore::AtkPaceSeconds(native, combo) : -1.0f);
			out += buf;
		}
		sprintf_s(buf, "ciecia boczne 2H: %s (W3Side2H=%d)", zdcore::Side2HLabel(zdcore::Side2HEffective(zcfg.w3Side2H,
			zcfg.w3Side2H != zdcore::SIDE2H_FAST || atkFastProto != nullptr)), zcfg.w3Side2H);
		out += buf;
		ZLog("zdism atk: %s", out.c_str());
		return out;
	}
}
