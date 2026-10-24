// zQuickSignNb.h - wyszukanie w UNION_ABI.DLL (New Balance) wejsc do logiki strumieni, ktore zQuickSign opakowuje
// (InstallNbStreamHooks). Sygnatury zamiast adresow: NB 18.10.2026 przesunal kod (stary DLL: jmp 0x103B3B43 ->
// 0x103B3ED0, call 0x1023A6DD -> 0x103B9190; nowy: 0x103C4143 -> 0x103C44E0, 0x1024986D -> 0x103C9830).
// Czysta logika (bez Windows) - testy w tests_zqs.cpp, takze na obu prawdziwych DLL.
#pragma once

#include "nbSigScan.h"

namespace qs {

// Funkcja strumieni NB (S_FSCCAST, 0xBD...): prolog SEH z ramka 0x100 i pierwsze pytanie o zinput->GetState(5)
// (cmp [0x8B21C0],0 / mov ecx,[zinput 0x8D1650] / ... push 5 / call [eax+4]). Adresy z Gothic2.exe sa stale,
// adresy wewnatrz DLL (handler SEH, cookie) - dowolne.
static const char* NB_SIG_STREAM_FN =
	"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC 00 01 00 00 A1 ?? ?? ?? ?? 33 C5 89 45 F0 56 57 50 "
	"8D 45 F4 64 A3 00 00 00 00 83 3D C0 21 8B 00 00 8B 0D 50 16 8D 00 8B 01 74 ?? 8B 40 04 6A 05 FF D0";
// Funkcja SPL_FIRE_STREAM: prolog SEH z ramka 0x60, call, player->fmode (+0x250) == 7, aktywny czar, id == 0xB4 (180).
static const char* NB_SIG_FIRESTREAM_FN =
	"55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 83 EC 60 A1 ?? ?? ?? ?? 33 C5 89 45 F0 56 50 8D 45 F4 "
	"64 A3 00 00 00 00 E8 ?? ?? ?? ?? 8B 0D 84 26 AB 00 83 B9 50 02 00 00 07 0F 85 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
	"85 C0 0F 84 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 81 78 54 B4 00 00 00";

struct NbStreamSites {
	sigscan::u32 streamSite, streamFn;   // jmp rel32 (wywolanie koncowe z dyspozytora) -> funkcja strumieni
	sigscan::u32 fireSite, fireFn;       // call rel32 (z Game_Loop NB) -> funkcja SPL_FIRE_STREAM
	// diagnostyka (do logu): liczba trafien sygnatur i odwolan jmp/call do kazdej funkcji
	int streamFnHits, fireFnHits, streamJmp, streamCall, fireJmp, fireCall;
};

enum NbFindResult { NBF_OK = 0, NBF_STREAM_FN, NBF_FIRE_FN, NBF_STREAM_REFS, NBF_FIRE_REFS };

inline const char* NbFindResultText(int r) {
	switch (r) {
	case NBF_OK:          return "ok";
	case NBF_STREAM_FN:   return "funkcja strumieni nie jest jednoznaczna";
	case NBF_FIRE_FN:     return "funkcja SPL_FIRE_STREAM nie jest jednoznaczna";
	case NBF_STREAM_REFS: return "do funkcji strumieni nie prowadzi dokladnie jeden jmp";
	case NBF_FIRE_REFS:   return "do funkcji SPL_FIRE_STREAM nie prowadzi dokladnie jeden call";
	}
	return "?";
}

// Obie funkcje musza wystapic dokladnie raz, a kazda miec dokladnie jedno wejscie rel32 w oczekiwanej postaci
// (strumienie: jeden jmp i zaden call, ogien: jeden call i zaden jmp) - opakowanie musi objac kazde wejscie, inaczej
// lepiej nie zakladac hakow wcale. -> NBF_OK albo powod.
inline int FindNbStreamSites(const sigscan::CodeRange* r, int n, NbStreamSites* s) {
	memset(s, 0, sizeof(*s));
	if (!sigscan::FindUniqueVa(r, n, NB_SIG_STREAM_FN, &s->streamFn, &s->streamFnHits))
		return NBF_STREAM_FN;
	if (!sigscan::FindUniqueVa(r, n, NB_SIG_FIRESTREAM_FN, &s->fireFn, &s->fireFnHits))
		return NBF_FIRE_FN;
	s->streamJmp = sigscan::FindRefsVa(r, n, s->streamFn, 0xE9, &s->streamSite, 1);
	s->streamCall = sigscan::FindRefsVa(r, n, s->streamFn, 0xE8, 0, 0);
	if (s->streamJmp != 1 || s->streamCall != 0)
		return NBF_STREAM_REFS;
	s->fireCall = sigscan::FindRefsVa(r, n, s->fireFn, 0xE8, &s->fireSite, 1);
	s->fireJmp = sigscan::FindRefsVa(r, n, s->fireFn, 0xE9, 0, 0);
	if (s->fireCall != 1 || s->fireJmp != 0)
		return NBF_FIRE_REFS;
	return NBF_OK;
}

}  // namespace qs
