// Profiler sekcji dla pluginow Union (Gothic II NotR): QueryPerformanceCounter, sekcje RAII, wspolna tablica pluginow
// w pamieci nazwanej procesu ("Local\zProfShared.v<wersja>.<pid>" - kto pierwszy, ten tworzy; kolejnosc ladowania DLL
// bez znaczenia). Logika (mediana, przycinki, podsumowania): ZProfCore.h, testy w zDismember\tests\test_core.cpp.
//
// Dlaczego plugins\_shared: naglowek jest wspolny dla kilku pluginow (rodzenstwo w plugins\), kazdy wkompilowuje
// wlasna kopie (osobny stan w kazdej DLL), a pluginy widza sie nawzajem tylko przez pamiec nazwana. Dolaczany z
// Headers.h w nawiasach ostrych (<../../_shared/ZProf.h> wzgledem $(ProjectDir)) - beforebuild.exe z Union SDK
// przetwarza naglowki w cudzyslowie jako pliki interfejsu (zakaz #pragma once).
//
// Uzycie:
//   Game_Init:   zprof::Init("zDismember", Sink, on, spikeMs, summarySec);   // Sink(const char* linia) -> log pluginu
//   Game_Loop:   zprof::FrameMark();       // na samym poczatku: klatka = odstep miedzy kolejnymi Game_Loop
//   kod:         ZPROF_SCOPE("Traversal.Probe");   // do konca bloku; grube sekcje, nazwa do 31 znakow
//   wczytanie / zapis / menu: zprof::Break();      // odstep nie jest klatka
// Narzut: dwa odczyty QPC na sekcje (~20-40 ns na typowym PC, mierzony przy starcie), bez alokacji w sekcjach;
// sekcje dzialaja tylko w watku, ktory wywolal Init (glowny watek gry).
#pragma once
#include <windows.h>
#include "ZProfCore.h"

namespace zprof {
	typedef void (*LineSink)(const char* line);
	// Po zamknieciu klatki (FrameMark): okno klatki [start, end) w tyknieciach QPC (start 0 = brak poprzedniego Mark),
	// rodzaj klatki, czy bylo podsumowanie okresowe; linie dopisane tu ida do logu razem z liniami profilera
	// (insertAt = miejsce zaraz po linii przycinki). Uzywa profiler probkujacy (ZSampler.h).
	typedef void (*FrameHook)(uint64_t start, uint64_t end, FrameKind k, bool periodic, std::vector<std::string>& lines, size_t insertAt);

	struct Runtime {
		Profiler prof;
		bool on = false;
		bool inited = false;
		DWORD mainThread = 0;
		LineSink sink = nullptr;
		double clockNs = 0.0;
		bool shared = false;
		std::vector<std::string> lines;
		FrameHook frameHook = nullptr;
		uint64_t markPrev = 0;   // QPC poprzedniego FrameMark
	};
	inline Runtime g_rt;

	inline uint64_t Now() {
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		return (uint64_t)c.QuadPart;
	}

	inline int Section(const char* name) {
		return g_rt.prof.Section(name);
	}

	class Scope {
	public:
		explicit Scope(int idx) : active_(g_rt.on && GetCurrentThreadId() == g_rt.mainThread) {
			if (active_)
				g_rt.prof.Begin(idx, Now());
		}
		~Scope() {
			if (active_)
				g_rt.prof.End(Now());
		}
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;
	private:
		bool active_;
	};

	inline void Emit() {
		for (auto& l : g_rt.lines)
			if (g_rt.sink)
				g_rt.sink(l.c_str());
		g_rt.lines.clear();
	}

	// Wspolna tablica pluginow; uchwyt i widok zostaja do konca procesu (czytaja je inne pluginy).
	inline Table* OpenSharedTable() {
		char name[64];
		std::snprintf(name, sizeof(name), "Local\\zProfShared.v%u.%lu", TABLE_VERSION, (unsigned long)GetCurrentProcessId());
		HANDLE h = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, (DWORD)sizeof(Table), name);
		if (!h)
			return nullptr;
		void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Table));
		if (!p) {
			CloseHandle(h);
			return nullptr;
		}
		return (Table*)p;
	}

	inline void SetOn(bool on) {
		if (on && !g_rt.on)
			g_rt.prof.Break();   // pomiar klatek od nowa
		g_rt.on = on && g_rt.inited;
	}

	inline bool IsOn() {
		return g_rt.on;
	}

	// Ustawienia z ini (takze po przeladowaniu).
	inline void Configure(bool on, float spikeMs, float summarySec) {
		g_rt.prof.Configure(spikeMs, summarySec);
		SetOn(on);
	}

	// Pierwsze wywolanie: zegar, koszt pomiaru, slot we wspolnej tablicy, linia w logu. Kolejne = Configure.
	inline void Init(const char* plugin, LineSink sink, bool on, float spikeMs, float summarySec) {
		Runtime& r = g_rt;
		r.sink = sink;
		if (!r.inited) {
			r.inited = true;
			r.mainThread = GetCurrentThreadId();
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			r.prof.SetName(plugin);
			r.prof.SetTicksPerMs((double)f.QuadPart / 1000.0);
			const int n = 2000;
			uint64_t a = Now();
			for (int i = 0; i < n; i++)
				Now();
			uint64_t b = Now();
			r.clockNs = (double)(b - a) * 1e9 / (double)f.QuadPart / (n + 1);
			r.prof.SetMeasureCostNs(r.clockNs);
			r.shared = r.prof.Attach(OpenSharedTable());
			Configure(on, spikeMs, summarySec);
			char buf[320];
			std::snprintf(buf, sizeof(buf), "Prof: %s %s - przycinka = klatka > max(%.0f ms, 2 x mediana %d klatek), "
				"podsumowanie %s; zegar %.0f ns/odczyt; wspolna tablica pluginow: %s", plugin, on ? "wlaczony" : "wylaczony",
				spikeMs, WINDOW, summarySec > 0.0f ? "okresowe" : "na zadanie", r.clockNs,
				r.shared ? "tak" : "NIE (bez udzialu innych pluginow)");
			r.lines.push_back(buf);
			Emit();
			return;
		}
		Configure(on, spikeMs, summarySec);
	}

	// Poczatek Game_Loop: zamyka poprzednia klatke (przycinka / przerwa / podsumowanie okresowe -> log).
	inline void FrameMark() {
		if (!g_rt.on)
			return;
		uint64_t now = Now(), periods = g_rt.prof.PeriodCount();
		size_t at = g_rt.lines.size();
		FrameKind k = g_rt.prof.Mark(now, &g_rt.lines);
		if (g_rt.frameHook)
			g_rt.frameHook(k == FRAME_NONE ? 0 : g_rt.markPrev, now, k, g_rt.prof.PeriodCount() != periods, g_rt.lines,
				k == FRAME_SPIKE ? at + 1 : at);
		g_rt.markPrev = now;
		if (!g_rt.lines.empty())
			Emit();
	}

	inline void Break() {
		g_rt.prof.Break();
	}

	inline void Reset() {
		g_rt.prof.Reset();
	}

	// Pelne podsumowanie od resetu do logu, krotkie do konsoli.
	inline std::string Summary() {
		g_rt.prof.SummaryLines(g_rt.lines);
		Emit();
		return g_rt.prof.ShortSummary();
	}
}

#define ZPROF_CAT_(a, b) a##b
#define ZPROF_CAT(a, b) ZPROF_CAT_(a, b)
// Sekcja do konca bloku. Indeks nazwy liczony raz (statyczna zmienna w miejscu uzycia).
#define ZPROF_SCOPE(name) \
	static const int ZPROF_CAT(zprofSec_, __LINE__) = ::zprof::Section(name); \
	::zprof::Scope ZPROF_CAT(zprofScope_, __LINE__)(ZPROF_CAT(zprofSec_, __LINE__))
