// Logika bez silnika: diagnostyka hakow w lancuchu (port New Balance - UNION_ABI.DLL haczy te same funkcje).
// Silnik (liczniki, odczyt wejscia funkcji, log): zDismember\HookDiag.cpp. Testy: tests\test_core.cpp.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>

namespace zdcore {
	enum HookJumpKind { HOOKJMP_NONE = 0, HOOKJMP_REL, HOOKJMP_INDIRECT };

	// Skok na wejsciu funkcji (hak Union / detour): E9 rel32, EB rel8, 68 imm32 C3 (push/ret) -> target;
	// FF 25 [abs32] -> slot (adres komorki z celem, odczyt robi wywolujacy). b: co najmniej 6 bajtow spod addr.
	inline int DecodeEntryJump(const uint8_t* b, uint32_t addr, uint32_t& target, uint32_t& slot) {
		if (b[0] == 0xE9) {
			int32_t rel;
			memcpy(&rel, b + 1, 4);
			target = addr + 5 + (uint32_t)rel;
			return HOOKJMP_REL;
		}
		if (b[0] == 0xEB) {
			target = addr + 2 + (uint32_t)(int32_t)(int8_t)b[1];
			return HOOKJMP_REL;
		}
		if (b[0] == 0xFF && b[1] == 0x25) {
			memcpy(&slot, b + 2, 4);
			return HOOKJMP_INDIRECT;
		}
		if (b[0] == 0x68 && b[5] == 0xC3) {
			memcpy(&target, b + 1, 4);
			return HOOKJMP_REL;
		}
		return HOOKJMP_NONE;
	}

	// Miejsce wywolania podmienione przez haki "na wywolaniu" (np. 0x69A0CA call oCAIHuman::MagicMode): E8 rel32 -> cel.
	inline bool DecodeCallSite(const uint8_t* b, uint32_t addr, uint32_t& target) {
		if (b[0] != 0xE8)
			return false;
		int32_t rel;
		memcpy(&rel, b + 1, 4);
		target = addr + 5 + (uint32_t)rel;
		return true;
	}

	// Hak Ikarusa/LeGo (HookEngineI) zalozony na wejsciu funkcji PO naszym haku Union: Union (Detours) pisze na wejsciu
	// E9 rel32 + CC do granicy instrukcji, a HookEngine kopiuje OLDINSTR bajtow wejscia SUROWO do swojego stubu i stawia za
	// nimi 68 <funkcja+OLDINSTR> C3 (push/ret). Skopiowany E9 skacze wtedy wzgledem nowego miejsca - w przypadkowy kod
	// (crash przy pierwszym wywolaniu). Kroniki Myrtany: OnDamage i EV_AttackFinish (INIT_FEATURES_GAMESTART, 7 bajtow).
	// Plan poprawki: stub = bajty spod stubAddr (n), expected = cel skoku na wejsciu sprzed haka Ikarusa (nasz / Union).
	enum IkarusStubKind { IKSTUB_NOT_FOUND = 0, IKSTUB_NOT_JUMP, IKSTUB_ALREADY_OK, IKSTUB_PATCH, IKSTUB_FOREIGN };
	struct IkarusStubPlan {
		int kind = IKSTUB_NOT_FOUND;
		uint32_t copyOffset = 0;   // offset skopiowanych bajtow wejscia w stubie
		uint32_t oldInstr = 0;     // ile bajtow skopiowal HookEngine
		uint32_t jumpsTo = 0;      // dokad skacze skopiowany E9 teraz
		int32_t newRel = 0;        // IKSTUB_PATCH: rel32 do wpisania pod stubAddr + copyOffset + 1
	};
	inline IkarusStubPlan PlanIkarusStubFix(const uint8_t* stub, uint32_t n, uint32_t stubAddr, uint32_t funcAddr,
		uint32_t expected) {
		IkarusStubPlan p;
		for (uint32_t i = 0; i + 6 <= n; i++) {
			if (stub[i] != 0x68 || stub[i + 5] != 0xC3)
				continue;
			uint32_t imm;
			memcpy(&imm, stub + i + 1, 4);
			if (imm < funcAddr + 5 || imm > funcAddr + 16)
				continue;
			uint32_t k = imm - funcAddr;
			if (i < k)
				continue;
			p.copyOffset = i - k;
			p.oldInstr = k;
			const uint8_t* c = stub + p.copyOffset;
			if (c[0] != 0xE9) {
				p.kind = IKSTUB_NOT_JUMP;   // skopiowany zwykly prolog - Ikarus byl pierwszy, nic do poprawy
				return p;
			}
			int32_t rel;
			memcpy(&rel, c + 1, 4);
			uint32_t at = stubAddr + p.copyOffset;
			p.jumpsTo = at + 5 + (uint32_t)rel;
			if (p.jumpsTo == expected) {
				p.kind = IKSTUB_ALREADY_OK;   // HookEngine przeliczyl skok sam (nowsze LeGo) albo juz poprawione
				return p;
			}
			if (funcAddr + 5 + (uint32_t)rel == expected) {
				p.kind = IKSTUB_PATCH;
				p.newRel = (int32_t)(expected - (at + 5));
				return p;
			}
			p.kind = IKSTUB_FOREIGN;   // skok gdzie indziej - nie ruszamy
			return p;
		}
		return p;
	}
	inline const char* IkarusStubKindName(int k) {
		switch (k) {
		case IKSTUB_NOT_JUMP: return "stub Ikarusa ze zwyklym prologiem (bez zmian)";
		case IKSTUB_ALREADY_OK: return "stub Ikarusa skacze poprawnie (bez zmian)";
		case IKSTUB_PATCH: return "stub Ikarusa ze skopiowanym skokiem - poprawiony";
		case IKSTUB_FOREIGN: return "stub ze skokiem w inne miejsce (bez zmian)";
		default: return "inny hak na wejsciu, nie stub Ikarusa (bez zmian)";
		}
	}

	// Kroniki Myrtany (ONDAMAGE_HOOK / ONDAMAGESCRIPT_HOOK): przed oCNpc::OnDamage skrypt zdejmuje ofierze ochrone o wage
	// broni bohatera (ONDAMAGEARMORPENETRATION), a oddaje ja w OnDamage_Script. Polkniete trafienie (finisher, Quen) nie
	// dochodzi do OnDamage_Script - ochrone oddajemy sami. Indeks ochrony jak w skrypcie (porownanie ==, nie maska):
	// DAM_BLUNT 2 -> 1, DAM_EDGE 4 -> 2, DAM_MAGIC 32 -> 5, DAM_POINT 64 -> 6, reszta -1.
	inline int ScriptPenetrationProtIndex(uint32_t damageType) {
		switch (damageType) {
		case 2: return 1;
		case 4: return 2;
		case 32: return 5;
		case 64: return 6;
		default: return -1;
		}
	}

	// "OnDamage 12, DoDie 0, HitCombo 3 | bez wywolan: DoDie"
	inline std::string HookCountsLine(const char* const* names, const unsigned* counts, int n) {
		std::string s, zero;
		for (int i = 0; i < n; i++) {
			s += (i ? ", " : "") + std::string(names[i]) + " " + std::to_string(counts[i]);
			if (!counts[i])
				zero += (zero.empty() ? "" : ", ") + std::string(names[i]);
		}
		return s + " | bez wywolan: " + (zero.empty() ? "-" : zero);
	}

	// Raporty w chwilach marks[] (s od startu, rosnaco): true raz na przekroczony znacznik; po dlugiej przerwie
	// (menu, wczytanie) jeden raport za wszystkie przekroczone. done = ile znacznikow juz minelo.
	inline bool HookReportDue(float elapsed, int& done, const float* marks, int n) {
		int k = done;
		while (k < n && elapsed >= marks[k])
			k++;
		if (k == done)
			return false;
		done = k;
		return true;
	}
}
