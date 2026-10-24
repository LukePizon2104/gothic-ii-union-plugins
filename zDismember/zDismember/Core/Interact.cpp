#include "Interact.h"
#include "Moves.h"
#include <algorithm>
#include <cmath>

namespace zdcore {
	const char* IaKindText(IaKind k) {
		switch (k) {
		case IA_NONE: return "brak";
		case IA_ITEM: return "przedmiot";
		case IA_DOOR: return "drzwi";
		case IA_CHEST: return "skrzynia";
		case IA_CORPSE: return "zwloki";
		default: return "?";
		}
	}

	bool IaMobCanOpen(const IaMobInfo& m) {
		if (!m.locked)
			return true;
		if (m.keyNeeded && m.hasKey)
			return true;
		return m.pickLock && m.hasLockpick && m.pickTalent;
	}

	std::string IaMobDesc(const IaMobInfo& m) {
		std::string s = m.door ? "drzwi" : m.chest ? "skrzynia" : "MOB";
		if (m.door)
			s += m.open ? " otwarte" : " zamkniete";
		if (m.locked) {
			s += ", zamek (";
			s += !m.keyNeeded ? "bez klucza" : m.hasKey ? "klucz: jest" : "klucz: brak";
			s += ", ";
			s += !m.pickLock ? "nie na wytrych" : m.hasLockpick ? "wytrych: jest" : "wytrych: brak";
			if (m.pickLock)
				s += m.pickTalent ? ", talent: tak" : ", talent: nie";
			s += ")";
		}
		else
			s += ", bez zamka";
		if (m.condition)
			s += ", warunek skryptu";
		if (m.stateFunc)
			s += ", skrypt stanu";
		if (m.useWithItem)
			s += ", wymaga przedmiotu";
		if (m.occupied)
			s += ", zajety";
		if (m.destroyed)
			s += ", zniszczony";
		return s;
	}

	const char* IaPlanText(IaPlan p) {
		switch (p) {
		case IAP_NONE: return "brak";
		case IAP_ARMED: return "z bronia w rece";
		case IAP_SHEATHE: return "schowanie broni, zwykle otwarcie, dobycie";
		case IAP_LOCKED_MSG: return "zamkniete bez klucza i wytrycha - komunikat gry";
		case IAP_CORPSE: return "przeszukanie (gra chowa bron przy oknie, potem dobycie)";
		case IAP_SKIP_OFF: return "wylaczone w ini";
		case IAP_SKIP_SCRIPT: return "zamek / skrypt przy LootArmedDoors=1";
		case IAP_SKIP_BUSY: return "uzywa go ktos inny albo zniszczony";
		case IAP_SKIP_EMPTY: return "nic do zabrania";
		case IAP_SKIP_ALIVE: return "zywy i przytomny";
		default: return "?";
		}
	}

	IaPlan IaPlanMob(const IaMobInfo& m, const IaSettings& s) {
		if (m.door) {
			if (s.doors <= IA_DOORS_OFF)
				return IAP_SKIP_OFF;
		}
		else if (m.chest) {
			if (!s.chests)
				return IAP_SKIP_OFF;
		}
		else
			return IAP_SKIP_OFF;
		if (m.destroyed || m.occupied)
			return IAP_SKIP_BUSY;
		// zamek bez klucza i wytrycha: gra i tak tylko powie "zamkniete" - bez chowania broni
		if (!IaMobCanOpen(m))
			return IAP_LOCKED_MSG;
		// kazde okno ekwipunku chowa bron (OpenInventory) - skrzynia zawsze zwykla sciezka gry bez broni
		if (m.chest)
			return IAP_SHEATHE;
		if (!m.locked && !m.condition && !m.stateFunc && !m.useWithItem)
			return IAP_ARMED;
		return s.doors >= IA_DOORS_ALL ? IAP_SHEATHE : IAP_SKIP_SCRIPT;
	}

	IaPlan IaPlanCorpse(const IaCorpseInfo& c, const IaSettings& s) {
		if (!c.dead && !c.unconscious)
			return IAP_SKIP_ALIVE;
		if (!s.corpses)
			return IAP_SKIP_OFF;
		if (!c.hasLoot)
			return IAP_SKIP_EMPTY;
		return IAP_CORPSE;
	}

	static float IaClampf(float v, float a, float b) {
		float lo = std::min(a, b), hi = std::max(a, b);
		return v < lo ? lo : v > hi ? hi : v;
	}

	bool IaBoxFits(const LootParams& p, float fwdX, float fwdZ, const IaBox& b, float& score, float& dist) {
		float y0 = std::min(b.minY, b.maxY), y1 = std::max(b.minY, b.maxY);
		if (y1 < p.minDy || y0 > p.maxDy)
			return false;
		// najblizszy punkt obrysu w poziomie (bohater w srodku ukladu)
		float nx = IaClampf(0.0f, b.minX, b.maxX), nz = IaClampf(0.0f, b.minZ, b.maxZ);
		float d = std::sqrt(nx * nx + nz * nz);
		if (d > p.range)
			return false;
		float cone = std::max(p.coneDeg, 1.0f) * 3.14159265f / 180.0f;
		float ang = 0.0f;
		if (d >= p.nearAny) {
			float tx = nx, tz = nz;
			if (d < 0.01f) {
				// bohater w obrysie - kierunek do srodka (otwarte drzwi obok w przejsciu sie nie licza)
				tx = (b.minX + b.maxX) * 0.5f;
				tz = (b.minZ + b.maxZ) * 0.5f;
			}
			if (tx * tx + tz * tz >= 1.0f) {
				if (fwdX * fwdX + fwdZ * fwdZ < 1e-8f)
					return false;
				ang = std::fabs(AngleDiff(std::atan2(fwdX, fwdZ), std::atan2(tx, tz)));
				if (ang > cone)
					return false;
			}
		}
		score = d + p.anglePenalty * std::min(ang / cone, 1.0f);
		dist = d;
		return true;
	}

	bool IaMobReachable(const std::vector<IaPos>& pos, float reach) {
		if (pos.empty())
			return true;
		for (const IaPos& q : pos)
			if (q.free && q.dx * q.dx + q.dy * q.dy + q.dz * q.dz <= reach * reach)
				return true;
		return false;
	}

	bool IaTargetBeatsItem(bool hasItem, float itemScore, bool hasTarget, float targetScore) {
		if (!hasTarget)
			return false;
		if (!hasItem)
			return true;
		return targetScore < itemScore;
	}

	const char* IaPhaseText(IaPhase p) {
		switch (p) {
		case IAPH_IDLE: return "brak";
		case IAPH_STAND: return "postawa";
		case IAPH_SHEATHE: return "chowanie broni";
		case IAPH_MOB: return "interakcja";
		case IAPH_WINDOW: return "okno zwlok";
		case IAPH_SETTLE: return "postawa broni";
		case IAPH_REDRAW_WAIT: return "czekanie na dobycie";
		case IAPH_REDRAW: return "dobycie broni";
		default: return "?";
		}
	}

	const char* IaActionText(IaAction a) {
		switch (a) {
		case IAA_NONE: return "brak";
		case IAA_STAND: return "zatrzymanie";
		case IAA_REMOVE_WEAPON: return "schowanie broni";
		case IAA_START_MOB: return "start interakcji";
		case IAA_OPEN_CORPSE: return "przeszukanie";
		case IAA_FIX_STANCE: return "postawa broni";
		case IAA_DRAW_WEAPON: return "dobycie broni";
		case IAA_END: return "koniec";
		default: return "?";
		}
	}

	static void IaEnter(IaRun& r, IaPhase ph) {
		r.phase = ph;
		r.phaseT = 0.0f;
	}

	static IaAction IaFinish(IaRun& r, const std::string& why) {
		r.endReason = why;
		r.phase = IAPH_IDLE;
		r.phaseT = 0.0f;
		return IAA_END;
	}

	// Po etapie, ktory mogl schowac bron (my albo okno gry): czekanie na dobycie; bez broni przed - koniec.
	static IaAction IaAfter(IaRun& r, const std::string& why) {
		r.note = why;
		if (r.weaponBefore == 0)
			return IaFinish(r, why);
		IaEnter(r, IAPH_REDRAW_WAIT);
		return IAA_NONE;
	}

	// Koniec interakcji z MOB-em: z bronia w rece - pilnowanie postawy, po schowaniu - dobycie.
	static IaAction IaMobOver(IaRun& r, const std::string& why) {
		if (r.plan == IAP_ARMED) {
			r.note = why;
			IaEnter(r, IAPH_SETTLE);
			return IAA_NONE;
		}
		return IaAfter(r, why);
	}

	// Postawa jest: pierwsza akcja planu.
	static IaAction IaStandNext(IaRun& r, const IaObs& o) {
		switch (r.plan) {
		case IAP_ARMED:
			IaEnter(r, IAPH_MOB);
			return IAA_START_MOB;
		case IAP_CORPSE:
			IaEnter(r, IAPH_WINDOW);
			return IAA_OPEN_CORPSE;
		case IAP_SHEATHE:
			if (o.weaponMode != 0) {
				r.sheathedByUs = true;
				IaEnter(r, IAPH_SHEATHE);
				return IAA_REMOVE_WEAPON;
			}
			IaEnter(r, IAPH_MOB);
			return IAA_START_MOB;
		default:
			return IaFinish(r, "plan bez przebiegu");
		}
	}

	bool IaBegin(IaRun& r, IaKind kind, IaPlan plan, const IaObs& o, IaAction& a) {
		r = IaRun();
		a = IAA_NONE;
		if (plan != IAP_ARMED && plan != IAP_SHEATHE && plan != IAP_CORPSE)
			return false;
		r.kind = kind;
		r.plan = plan;
		r.weaponBefore = o.weaponMode;
		IaEnter(r, IAPH_STAND);
		a = o.standing ? IaStandNext(r, o) : IAA_STAND;
		return true;
	}

	IaAction IaStep(IaRun& r, const IaObs& o, const IaSettings& s) {
		if (r.phase == IAPH_IDLE)
			return IAA_NONE;
		r.t += o.dt;
		r.phaseT += o.dt;
		if (o.dialog)
			r.sawDialog = true;
		if (o.invOpen)
			r.sawWindow = true;
		if (!o.alive)
			return IaFinish(r, "bohater martwy albo nieprzytomny");
		switch (r.phase) {
		case IAPH_STAND:
			if (o.standing)
				return IaStandNext(r, o);
			if (r.phaseT > IA_STAND_MAX)
				return IaFinish(r, "bohater nie stanal w 1 s (ruch trwa)");
			return IAA_NONE;
		case IAPH_SHEATHE:
			if (o.weaponMode == 0) {
				if (o.emEmpty && o.standing) {
					IaEnter(r, IAPH_MOB);
					return IAA_START_MOB;
				}
				if (r.phaseT > IA_SHEATHE_MAX)
					return IaAfter(r, "bron schowana, ale bohater nie stanal - bez interakcji");
				return IAA_NONE;
			}
			if (o.emEmpty && r.phaseT > IA_SHEATHE_LOST)
				return IaFinish(r, "schowanie broni przerwane (bron dalej w rece)");
			if (r.phaseT > IA_SHEATHE_MAX)
				return IaFinish(r, "schowanie broni trwa za dlugo");
			return IAA_NONE;
		case IAPH_MOB:
			if (!r.started) {
				if (o.mobActive) {
					r.started = true;
					r.phaseT = 0.0f;
					return IAA_NONE;
				}
				if (r.phaseT > IA_MOB_START_MAX)
					return IaMobOver(r, "interakcja nie ruszyla w 1,5 s");
				return IAA_NONE;
			}
			if (o.invOpen) {
				r.phaseT = 0.0f;   // okno skrzyni - bezpiecznik liczy od zamkniecia
				return IAA_NONE;
			}
			if (!o.anyMob)
				return IaMobOver(r, r.sawWindow ? "okno zamkniete, interakcja skonczona" : "interakcja skonczona");
			if (r.phaseT > IA_MOB_MAX)
				return IaFinish(r, "bezpiecznik 300 s - interakcja trwa dalej bez sledzenia");
			return IAA_NONE;
		case IAPH_WINDOW:
			if (!r.started) {
				if (o.invOpen) {
					r.started = true;
					r.phaseT = 0.0f;
					return IAA_NONE;
				}
				if (r.phaseT > IA_WINDOW_OPEN_MAX)
					return IaAfter(r, "okno zwlok sie nie otworzylo (gra odmowila albo inny plugin zabral lup)");
				return IAA_NONE;
			}
			if (o.invOpen)
				return IAA_NONE;
			return IaAfter(r, "okno zwlok zamkniete");
		case IAPH_SETTLE:
			if (o.weaponMode == 0)
				return IaFinish(r, r.note + "; bron schowana przez gracza");
			if (o.stanceWrong && !r.fixedStance && o.emEmpty && !o.anyMob) {
				r.fixedStance = true;
				return IAA_FIX_STANCE;
			}
			if (r.phaseT >= IA_SETTLE_TIME)
				return IaFinish(r, r.note + (r.fixedStance ? "; postawa broni przywrocona" : ""));
			return IAA_NONE;
		case IAPH_REDRAW_WAIT:
			if (o.weaponMode != 0 && o.emEmpty)
				return IaFinish(r, r.note + "; bron w rece - bez dobycia");
			if (o.invOpen || o.anyMob || o.dialog || !o.emEmpty || !o.bodyFree) {
				if (r.phaseT > IA_REDRAW_WAIT_MAX)
					return IaFinish(r, r.note + "; dobycie pominiete (nie doczekano w 20 s)");
				return IAA_NONE;
			}
			if (o.weaponMode != 0)
				return IaFinish(r, r.note + "; bron w rece - bez dobycia");
			if (r.sawDialog)
				return IaFinish(r, r.note + "; rozmowa w trakcie - bez dobycia broni");
			if (!s.redraw)
				return IaFinish(r, r.note + "; bez dobycia (LootArmedRedraw=0)");
			IaEnter(r, IAPH_REDRAW);
			return IAA_DRAW_WEAPON;
		case IAPH_REDRAW:
			if (o.weaponMode != 0)
				return IaFinish(r, r.note + "; bron dobyta");
			if (r.phaseT > IA_REDRAW_MAX)
				return IaFinish(r, r.note + "; dobycie nie ruszylo w 3 s");
			return IAA_NONE;
		default:
			return IaFinish(r, "nieznana faza");
		}
	}

	IaAction IaActionFailed(IaRun& r, IaAction a, const IaSettings& s, const char* why) {
		(void)s;
		std::string w = why ? why : "?";
		switch (a) {
		case IAA_START_MOB:
			if (r.plan == IAP_ARMED)
				return IaFinish(r, "silnik odmowil interakcji: " + w);
			return IaAfter(r, "silnik odmowil interakcji: " + w);
		case IAA_DRAW_WEAPON:
			return IaFinish(r, r.note + "; dobycie: " + w);
		default:
			return IaFinish(r, std::string(IaActionText(a)) + ": " + w);
		}
	}

	void IaAbort(IaRun& r, const char* why) {
		r.endReason = why ? why : "przerwane";
		r.phase = IAPH_IDLE;
		r.phaseT = 0.0f;
	}

	int IaClampDoors(int v) {
		return v < IA_DOORS_OFF ? IA_DOORS_OFF : v >= IA_DOORS_COUNT ? IA_DOORS_COUNT - 1 : v;
	}
}
