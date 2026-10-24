// Kanal masek odciec zDismember <-> zTimeRewind: pamiec nazwana procesu "Local\zCutLink.v<wersja>.<pid>".
// Uklad i logika: ZCutLinkCore.h. Dolaczany z Headers.h w nawiasach ostrych (<../../_shared/ZCutLink.h>) - jak ZProf.h.
//
// Uzycie:
//   zDismember (Game_Init):  zcut::Publish(zcut::Open(), "zDismember", GetMask, SetMask);
//   zTimeRewind (Game_Init): zcut::Join(zcut::Open(), "zTimeRewind");   potem przy kazdym uzyciu zcut::Linked()
#pragma once
#include <windows.h>
#include <cstdio>
#include "ZCutLinkCore.h"

namespace zcut {
	// Widok tablicy (uchwyt i widok zostaja do konca procesu - czyta je drugi plugin). nullptr przy bledzie systemu.
	inline Table* Open() {
		static Table* view = nullptr;
		static bool tried = false;
		if (tried)
			return view;
		tried = true;
		char name[64];
		std::snprintf(name, sizeof(name), "Local\\zCutLink.v%u.%lu", LINK_VERSION, (unsigned long)GetCurrentProcessId());
		HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)sizeof(Table), name);
		if (!h)
			return nullptr;
		void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Table));
		if (!p) {
			CloseHandle(h);
			return nullptr;
		}
		view = (Table*)p;
		return view;
	}

	// Tablica z funkcjami dostawcy albo nullptr (zDismember nie zaladowany / jeszcze nie opublikowal).
	inline const Table* Linked() {
		const Table* t = Open();
		return Ready(t) ? t : nullptr;
	}
}
