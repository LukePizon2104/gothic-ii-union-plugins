// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Nakladki ofiary zostaja na zwlokach (smierc gra z nich - Director, Daze). Postac wstajaca po cofnieciu czasu musi
	// je stracic, inaczej nastepne powalenie / smierc gralyby animacje ofiary W3. Zdjecie nakladki zatrzymuje wszystkie
	// animacje modelu (0x578C40) - dlatego zTimeRewind wola nas przed ResetPos, ktory startuje postawe od nowa.
	static int RemoveVictimOverlays(zCModel* model, std::string& names) {
		if (!model)
			return 0;
		std::vector<std::string> victim;
		for (const zdcore::FinisherDef& d : FinisherDefs())
			if (!d.victimOverlay.empty())
				victim.push_back(d.victimOverlay);
		std::vector<std::string> list = zdcore::VictimOverlayList(victim, DAZE_OVERLAY);
		std::vector<std::string> found;
		for (int i = 1; i < model->modelProtoList.GetNum(); i++) {   // 0 = baza
			zCModelPrototype* p = model->modelProtoList[i];
			if (p && zdcore::IsListedOverlay(p->modelProtoName.ToChar(), list))
				found.push_back(zdcore::OverlayBaseName(p->modelProtoName.ToChar()));
		}
		for (const std::string& n : found) {
			model->RemoveModelProtoOverlay(zSTRING((n + ".MDS").c_str()));
			names += (names.empty() ? "" : " ") + n;
		}
		return (int)found.size();
	}

	uint32_t RewindRestoreCuts(oCNpc* npc, uint32_t mask, uint32_t flags, const char* who) {
		if (!npc)
			return 0u;
		ZPROF_SCOPE("Rewind.Cuts");   // wolane z petli zTimeRewind (Commit / podglad przewijania)
		uint32_t before = CutMaskOf(npc);
		zdcore::CutRestorePlan plan = zdcore::PlanCutRestore(before, mask);
		zCModel* model = npc->homeWorld ? npc->GetModel() : nullptr;   // poza swiatem bez GetModel (UnShrink)
		uint32_t done = 0, restored = 0, squashed = 0;
		int gibs = 0, stumps = 0;
		if (plan.Any()) {
			if (!model)
				done |= zcut::DONE_NOMODEL;
			for (int i = 0; i < (int)zdcore::Part::NoPart; i++) {
				zdcore::Part p = (zdcore::Part)i;
				int node = model ? PartNode(model, p) : -1;
				if (zdcore::MaskHas(plan.restore, p)) {
					if (node >= 0 && SquashUnnode(model, node))
						restored = zdcore::MaskSet(restored, p);
					if (node >= 0) {
						gibs += GibsRemoveFor(npc, node);
						stumps += BloodStumpsRemove(npc, node);
					}
				}
				else if (zdcore::MaskHas(plan.apply, p) && node >= 0) {
					SquashNode(model, node);
					squashed = zdcore::MaskSet(squashed, p);
				}
			}
			if (!model && !plan.result) {
				gibs += GibsRemoveFor(npc, -1);   // bez modelu nie znamy wezlow - przywrocenie calej postaci
				stumps += BloodStumpsRemove(npc, -1);
			}
			PersistSet(npc, plan.result);   // aivar (AivarIndex) - gdy pamiec kikutow wlaczona
			CutMaskRemember(npc, plan.result);
			if (restored || squashed)
				done |= zcut::DONE_PARTS;
		}
		if (gibs)
			done |= zcut::DONE_GIBS;
		if (stumps)
			done |= zcut::DONE_STUMPS;
		if ((flags & zcut::FLAG_ALIVE) && CorpseForget(npc))
			done |= zcut::DONE_RAGDOLL;
		std::string overlays;
		if (flags & zcut::FLAG_REVIVE) {
			if (RemoveVictimOverlays(model, overlays))
				done |= zcut::DONE_OVERLAYS;
			if (DirectorForgetVictim(npc))
				done |= zcut::DONE_TAIL;
			if (IsDazed(npc)) {
				DazeRelease(npc);
				done |= zcut::DONE_DAZE;
			}
		}
		if (!plan.Any() && !(done & ~zcut::DONE_NOMODEL)) {
			ZDebug("Cofniecie czasu (%s): %s maska %03X bez zmian, flagi %u - nic do zrobienia", who ? who : "?",
				npc->GetInstanceName().ToChar(), before, flags);
			return done;
		}
		ZLog("Cofniecie czasu (%s): %s maska %03X -> %03X | przywrocone: %s | scisniete: %s | odlamki -%d, kikuty -%d%s%s%s%s%s%s",
			who ? who : "?", npc->GetInstanceName().ToChar(), before, plan.result, zdcore::MaskPartNames(restored).c_str(),
			zdcore::MaskPartNames(squashed).c_str(), gibs, stumps,
			(done & zcut::DONE_RAGDOLL) ? " | ragdoll porzucony" : "",
			overlays.empty() ? "" : " | nakladki zdjete: ", overlays.c_str(),
			(done & zcut::DONE_TAIL) ? " | ogon finishera zakonczony" : "",
			(done & zcut::DONE_DAZE) ? " | oszolomienie zakonczone" : "",
			(done & zcut::DONE_NOMODEL) ? " | postac bez modelu - tylko maska" : "");
		return done;
	}

	static uint32_t __cdecl LinkGetMask(void* npc) {
		return CutMaskOf((oCNpc*)npc);
	}

	static uint32_t __cdecl LinkSetMask(void* npc, uint32_t mask, uint32_t flags, const char* who) {
		return RewindRestoreCuts((oCNpc*)npc, mask, flags, who);
	}

	void RewindLinkInit() {
		zcut::Table* t = zcut::Open();
		bool ok = zcut::Publish(t, "zDismember", &LinkGetMask, &LinkSetMask);
		ZLog("Cofniecie czasu: kanal masek odciec (zCutLink v%u) %s%s%s", zcut::LINK_VERSION,
			ok ? "opublikowany" : t ? "NIE opublikowany - inny uklad tablicy (stara wersja zTimeRewind?)" : "NIE - brak pamieci nazwanej",
			ok && t->consumers ? ", zTimeRewind juz dolaczony: " : "", ok && t->consumers ? t->consumer : "");
	}
}
