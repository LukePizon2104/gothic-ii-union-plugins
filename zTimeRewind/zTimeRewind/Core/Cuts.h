// Logika bez silnika: maska odciec zDismember w nagraniu postaci (kanal _shared\ZCutLink.h) - co pokazac w podgladzie
// przewijania i co ustawic przy Commit. Silnik: Cuts.cpp.
//
// zDismember odcina czesci w chwili smierci (DoDie, finisher), wiec maska i smierc zmieniaja sie w tej samej probce.
// Regula jest jedna dla podgladu i Commit - "maska po ustawieniu postaci w chwili t" wg akcji Commit (DecideCommit):
// postac wstaje (HardTarget) = maska z chwili t; martwa wtedy i teraz (Keep) = maska z chwili t (zwloki jak wtedy);
// nieprzytomna wtedy, martwa teraz (Keep) = smierc zostaje, wiec i odciecia; SoftLatest (zywa wtedy i teraz) = maska
// z chwili t, a martwa / nieprzytomna wtedy i zywa teraz = bez zmian.
#pragma once
#include <cstdint>
#include "Timeline.h"
#include "Combat.h"   // StepTrack

namespace trcore {
	// Flagi i wynik jak w _shared\ZCutLinkCore.h (zcut::FLAG_*) - tu bez zaleznosci od naglowka wspolnego.
	const uint32_t CUT_FLAG_ALIVE = 1u;
	const uint32_t CUT_FLAG_REVIVE = 2u;
	const uint32_t CUT_FLAG_PREVIEW = 4u;
	const uint32_t CUT_MASK_BITS = 0x1FFu;

	inline uint32_t CutMaskAfter(CommitAction action, unsigned char targetLife, uint32_t targetMask, uint32_t nowMask) {
		targetMask &= CUT_MASK_BITS;
		nowMask &= CUT_MASK_BITS;
		switch (action) {
		case CommitAction::HardTarget:
			// wstaje: maska zywej postaci z chwili docelowej; "martwy wtedy" (tylko bohater / AI od nowa) - bez zmian
			return targetLife == LIFE_DEAD ? nowMask : targetMask;
		case CommitAction::Keep:
			return targetLife == LIFE_DEAD ? targetMask : nowMask;
		case CommitAction::SoftLatest:
		default:
			return targetLife == LIFE_ALIVE ? targetMask : nowMask;
		}
	}

	// Flagi wywolania zDismember przy Commit: postac wstaje (martwa albo nieprzytomna teraz, HardTarget) =
	// ALIVE (bez ragdolla zwlok) + REVIVE (nakladki ofiary / oszolomienia, ogon finishera) - przed ResetPos.
	inline uint32_t CutCommitFlags(CommitAction action, bool deadNow, bool knockedNow) {
		return action == CommitAction::HardTarget && (deadNow || knockedNow) ? CUT_FLAG_ALIVE | CUT_FLAG_REVIVE : 0u;
	}

	// Co wyslac do zDismember przy Commit: wywolanie, gdy maska inna niz biezaca albo postac wstaje.
	struct CutCall {
		bool send = false;
		uint32_t mask = 0, flags = 0;
	};

	inline CutCall CutCommitCall(CommitAction action, unsigned char targetLife, uint32_t targetMask, uint32_t startMask,
		uint32_t liveMask, bool deadNow, bool knockedNow) {
		CutCall c;
		c.mask = CutMaskAfter(action, targetLife, targetMask, startMask);
		c.flags = CutCommitFlags(action, deadNow, knockedNow);
		c.send = c.mask != (liveMask & CUT_MASK_BITS) || (c.flags & CUT_FLAG_REVIVE) != 0;
		return c;
	}

	// Stan podgladu postaci w jednym przewijaniu (Begin -> Commit).
	struct CutView {
		bool watch = false;     // martwa / nieprzytomna albo z odcieciami w nagraniu - liczona w kazdej klatce
		bool dead = false, knocked = false;   // na starcie przewijania (swiat zamrozony - bez zmian do Commit)
		uint32_t start = 0;     // maska na starcie przewijania
		uint32_t shown = 0;     // maska pokazana (ostatnio wyslana)
		bool alive = false;     // ragdoll zwlok juz porzucony (kursor w chwili, gdy postac zyla)
	};

	inline CutView CutViewBegin(uint32_t mask, bool dead, bool knocked, bool cutsInTrack) {
		CutView v;
		v.start = v.shown = mask & CUT_MASK_BITS;
		v.dead = dead;
		v.knocked = knocked;
		v.watch = dead || knocked || v.start != 0 || cutsInTrack;
		return v;
	}

	// Klatka przewijania: action / targetLife / targetMask = jak przy Commit w chwili kursora. Kursor idzie tylko wstecz,
	// a Commit jest w tej chwili albo wczesniej (migawka stanu) - podglad nie musi niczego cofac.
	inline CutCall CutPreviewStep(CutView& v, CommitAction action, unsigned char targetLife, uint32_t targetMask) {
		CutCall c;
		if (!v.watch)
			return c;
		c.mask = CutMaskAfter(action, targetLife, targetMask, v.start);
		bool alive = action == CommitAction::HardTarget && v.dead;
		c.flags = CUT_FLAG_PREVIEW | (alive || v.alive ? CUT_FLAG_ALIVE : 0u);
		c.send = c.mask != v.shown || (alive && !v.alive);
		if (c.send) {
			v.shown = c.mask;
			v.alive = v.alive || alive;
		}
		return c;
	}

	// Czy w torze masek jest odciecie (wpis niezerowy; tor zapisuje tylko zmiany) - postac do podgladu, choc teraz cala.
	inline bool CutsTrackHasCut(const StepTrack<uint32_t>& tr) {
		return !tr.Empty() && (tr.Size() > 1 || (tr.First() & CUT_MASK_BITS) != 0);
	}
}
