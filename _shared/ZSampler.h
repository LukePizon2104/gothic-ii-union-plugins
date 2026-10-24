// Profiler probkujacy (sampling) glownego watku gry - czesc z Windows. Logika (pierscien, stos, symbole, agregacja,
// linie logu): ZSampleCore.h, testy w zDismember\tests\test_core.cpp. Uzywa zprof (ZProf.h): okna klatek, wykrywania
// przycinek i ujscia linii do logu pluginu.
//
// Watek probkujacy (TIME_CRITICAL, timer wysokiej rozdzielczosci; bez niego timeBeginPeriod(1) + Sleep) co ProfSampleMs:
//   SuspendThread(glowny) -> GetThreadContext -> kopia stosu [esp, StackBase) do 32 KB -> ResumeThread,
// dopiero potem (watek gry juz biegnie) przejscie stosu na kopii i zapis probki do pierscienia. W oknie wstrzymania
// tylko wywolania systemowe i memcpy - zadnej alokacji, logu, blokad (sterta, CRT, loader): watek gry moze je trzymac
// i czekanie na nie = zakleszczenie. Granice stosu z TEB glownego watku (wskaznik pobrany w nim samym przy starcie,
// czytany przy kazdej probce). Straz: wstrzymanie dluzsze niz WATCHDOG_MS = wznowienie watku gry i wylaczenie probkowania.
//
// Glowny watek (zprof::FrameMark -> OnFrame): probki z okna klatki [poczatek, koniec) wg QPC -> modul / funkcja ->
// linia po linii przycinki i statystyki (przycinki osobno od klatek normalnych). Mapa modulow (EnumProcessModules,
// eksporty) budowana tylko tu, przy starcie i gdy probka trafi w nieznany adres.
//
// Uzycie (po zprof::Init, w tym samym watku):
//   Game_Init:     zsamp::Configure(on, intervalMs, tekstFunkcjiExe);   // pierwsze wlaczenie = start watku + kalibracja
//   Game_Pause / Game_Unpause:  zsamp::SetPaused(true / false);
//   Game_Exit:     zsamp::Stop();      // czeka na watek (nie w DllMain - loader lock)
//   DllMain DETACH: zsamp::RequestStop();   // sama flaga
//   konsola:       zsamp::Summary(), zsamp::Reset()
#pragma once
#include <windows.h>
#include <psapi.h>
#include <mmsystem.h>
#include "ZProf.h"
#include "ZSampleCore.h"
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "psapi.lib")

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace zsamp {
	const int RING = 8192;                    // probek w pierscieniu (ok. 8 s przy 1 ms)
	const int COPY_BYTES = 32 * 1024;         // kopia stosu przy probce
	const DWORD CODE_REFRESH_MS = 3000;       // watek probkujacy: lista modulow (zakresy kodu) co tyle
	const ULONGLONG MAP_REFRESH_MS = 5000;    // glowny watek: przebudowa mapy przy nieznanym adresie najczesciej co tyle
	const int CALIBRATION = 200;              // probek kalibracji przy starcie (koszt probki do logu)
	const double SLOW_SAMPLE_US = 100.0;      // wstrzymanie dluzsze - probkowanie rzadsze
	const int SLOW_INTERVAL_MS = 5;
	const double WATCHDOG_MS = 250.0;         // watek gry wstrzymany dluzej = awaria: wznowienie i koniec probkowania
	const int MAX_PROBES = 8;                 // nieznanych regionow pamieci badanych w jednej klatce

	typedef HANDLE(WINAPI* CreateTimerExFn)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);

	struct Shared {   // watek probkujacy <-> glowny
		std::atomic<bool> stop{ false }, want{ false }, paused{ false }, broken{ false }, calibrating{ false };
		std::atomic<int> intervalMs{ 1 };
		std::atomic<uint64_t> windowStart{ 0 };   // QPC poczatku wstrzymania (0 = watek gry biegnie) - dla strazy
		std::atomic<uint64_t> taken{ 0 }, failed{ 0 }, suspTicks{ 0 }, totalTicks{ 0 }, maxSusp{ 0 };
	};
	struct Worker {   // tylko watek probkujacy
		CodeRanges codes;
		uint32_t stack[COPY_BYTES / 4];
		HMODULE mods[1024];
		DWORD modsBytes = 0;
		ULONGLONG lastRefresh = 0;
	};
	struct Main {     // tylko glowny watek
		bool started = false, startFailed = false, brokenLogged = false;
		HANDLE thread = nullptr, watchdog = nullptr, mainThread = nullptr, timer = nullptr;
		const NT_TIB* mainTib = nullptr;
		HMODULE exe = nullptr;
		double tpms = 1.0;   // tykniecia QPC na ms
		std::string winDir;
		std::shared_ptr<SymTable> hostSyms;
		std::unordered_map<uint32_t, std::shared_ptr<SymTable>> exports;   // po bazie modulu
		std::vector<HMODULE> mapMods;
		std::vector<Module> pseudo;                // regiony spoza listy modulow (VirtualQuery)
		std::vector<uint32_t> probed;              // zbadane regiony 64 KB (bez ponownego VirtualQuery)
		ULONGLONG lastMap = 0;
		ModuleMap map;
		Names names;
		uint64_t cursor = 0;
		std::vector<Sample> window;
		FrameAgg frame;
		Stats total, period;
	};
	inline Shared g_sh;
	inline Worker g_w;
	inline Main g_m;
	inline Ring<RING> g_ring;
	inline CreateTimerExFn g_createTimerEx = nullptr;

	inline uint64_t Qpc() {
		LARGE_INTEGER c;
		QueryPerformanceCounter(&c);
		return (uint64_t)c.QuadPart;
	}

	inline void AtomicMax(std::atomic<uint64_t>& a, uint64_t v) {
		uint64_t cur = a.load(std::memory_order_relaxed);
		while (v > cur && !a.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {
		}
	}

	// ---------- Watek probkujacy ----------
	// Bajty przed adresem powrotu (kod modulu; wyjatek tylko gdy modul zwolniono od ostatniego odswiezenia listy).
	inline bool CallBefore(uint32_t v) {
		uint8_t b[7];
		if (v < 0x10007)
			return false;
		__try {
			std::memcpy(b, (const void*)(uintptr_t)(v - 7), 7);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
		return IsCallBefore(b);
	}
	inline bool IsRetAddr(uint32_t v) {
		return g_w.codes.Contains(v) && CallBefore(v);
	}
	inline bool IsHostRet(uint32_t v) {
		return g_w.codes.InHost(v) && CallBefore(v);
	}

	// Sekcje wykonywalne modulu do g_w.codes (naglowki PE z pamieci).
	inline void AddModuleCode(HMODULE h, bool host) {
		__try {
			uintptr_t base = (uintptr_t)h;
			const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
			if (dos->e_magic != IMAGE_DOS_SIGNATURE)
				return;
			const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE)
				return;
			const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
			for (int i = 0; i < nt->FileHeader.NumberOfSections && i < 96; i++) {
				if (!(sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE))
					continue;
				DWORD size = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
				uint32_t lo = (uint32_t)(base + sec[i].VirtualAddress), hi = lo + size;
				g_w.codes.Add(lo, hi);
				if (host && !g_w.codes.hostLo)
					g_w.codes.SetHost(lo, hi);   // .text exe
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
		}
	}

	// Lista modulow (EnumProcessModules czyta PEB jak dla obcego procesu - bez blokady loadera); poza wstrzymaniem.
	inline void RefreshCodes(bool force) {
		ULONGLONG now = GetTickCount64();
		if (!force && now - g_w.lastRefresh < CODE_REFRESH_MS)
			return;
		g_w.lastRefresh = now;
		HMODULE mods[1024];
		DWORD need = 0;
		if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need))
			return;
		if (need > sizeof(mods))
			need = sizeof(mods);
		if (!force && need == g_w.modsBytes && std::memcmp(mods, g_w.mods, need) == 0)
			return;
		std::memcpy(g_w.mods, mods, need);
		g_w.modsBytes = need;
		g_w.codes.Clear();
		for (DWORD i = 0; i < need / sizeof(HMODULE); i++)
			AddModuleCode(mods[i], mods[i] == g_m.exe);
		g_w.codes.Finish();
	}

	inline bool CopyStack(void* dst, const void* src, size_t n) {
		__try {
			std::memcpy(dst, src, n);
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
		return true;
	}

	inline void TakeSample() {
		CONTEXT ctx;
		std::memset(&ctx, 0, sizeof(ctx));
		ctx.ContextFlags = CONTEXT_CONTROL;
		size_t got = 0;
		uint64_t t0 = Qpc();
		g_sh.windowStart.store(t0);
		// ---- okno wstrzymania: tylko wywolania systemowe i memcpy ----
		if (SuspendThread(g_m.mainThread) == (DWORD)-1) {
			g_sh.windowStart.store(0);
			g_sh.failed++;
			return;
		}
		BOOL ok = GetThreadContext(g_m.mainThread, &ctx);   // czeka, az wstrzymanie naprawde nastapi
		if (ok) {
			uint32_t esp = ctx.Esp;
			uint32_t base = (uint32_t)(uintptr_t)g_m.mainTib->StackBase, limit = (uint32_t)(uintptr_t)g_m.mainTib->StackLimit;
			if (esp >= limit && esp < base) {   // [esp, StackBase) zawsze zatwierdzone; inny stos (fiber) - bez kopii
				size_t n = (size_t)(base - esp);
				if (n > (size_t)COPY_BYTES)
					n = COPY_BYTES;
				n &= ~(size_t)3;
				if (CopyStack(g_w.stack, (const void*)(uintptr_t)esp, n))
					got = n;
			}
		}
		uint64_t t1 = Qpc();
		ResumeThread(g_m.mainThread);
		// ---- koniec okna ----
		uint64_t t2 = Qpc(), expect = t0;
		g_sh.windowStart.compare_exchange_strong(expect, 0);
		if (!ok) {
			g_sh.failed++;
			return;
		}
		Sample s;
		s.t = t1;
		s.eip = ctx.Eip;
		int words = (int)(got / 4);
		s.nret = (uint8_t)WalkStack(g_w.stack, words, ctx.Esp, ctx.Ebp, IsRetAddr, s.ret, MAX_RET);
		for (int i = 0; i < s.nret && !s.hostRet; i++)
			if (g_w.codes.InHost(s.ret[i]))
				s.hostRet = s.ret[i];
		if (!s.hostRet)
			s.hostRet = FindFirst(g_w.stack, words, IsHostRet);
		g_ring.Push(s);
		g_sh.taken++;
		g_sh.suspTicks += t2 - t0;
		g_sh.totalTicks += Qpc() - t0;
		AtomicMax(g_sh.maxSusp, t2 - t0);
	}

	inline bool ProfOn() {
		return *(volatile bool*)&zprof::g_rt.on;
	}

	// Aktywne probkowanie: timer okresowy (rytm niezalezny od kosztu probki); bezczynnosc: Sleep(50) bez timera.
	inline void Wait(bool active, int ms, int& armed) {
		if (active && g_m.timer) {
			if (armed != ms) {
				LARGE_INTEGER due;
				due.QuadPart = -(LONGLONG)ms * 10000;
				armed = SetWaitableTimer(g_m.timer, &due, ms, nullptr, nullptr, FALSE) ? ms : 0;
			}
			if (armed) {
				WaitForSingleObject(g_m.timer, (DWORD)ms + 50);
				return;
			}
		}
		if (armed && g_m.timer)
			CancelWaitableTimer(g_m.timer);
		armed = 0;
		Sleep(active ? (DWORD)ms : 50);
	}

	inline DWORD WINAPI SamplerProc(void*) {
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		bool period = !g_m.timer;
		if (period)
			timeBeginPeriod(1);
		RefreshCodes(true);
		if (g_sh.calibrating.load()) {   // kilkaset probek od razu - koszt do logu przy starcie
			for (int i = 0; i < CALIBRATION && !g_sh.stop.load(); i++)
				TakeSample();
			g_sh.calibrating.store(false);
		}
		int armed = 0;
		while (!g_sh.stop.load()) {
			bool active = g_sh.want.load() && !g_sh.paused.load() && !g_sh.broken.load() && ProfOn();
			Wait(active, g_sh.intervalMs.load(), armed);
			if (g_sh.stop.load())
				break;
			if (!active)
				continue;
			RefreshCodes(false);
			TakeSample();
		}
		if (period)
			timeEndPeriod(1);
		return 0;
	}

	// Straz: watek gry wstrzymany za dlugo (watek probkujacy utknal) - wznow i wylacz probkowanie na dobre.
	inline DWORD WINAPI WatchdogProc(void*) {
		uint64_t limit = (uint64_t)(WATCHDOG_MS * g_m.tpms);
		while (!g_sh.stop.load()) {
			Sleep(50);
			uint64_t s = g_sh.windowStart.load();
			if (s && Qpc() - s > limit && g_sh.windowStart.compare_exchange_strong(s, 0)) {
				g_sh.broken.store(true);
				ResumeThread(g_m.mainThread);
			}
		}
		return 0;
	}

	// ---------- Glowny watek: mapa modulow ----------
	typedef void (*ExportSink)(void* ctx, uint32_t addr, const char* name);
	inline void AddExport(void* ctx, uint32_t addr, const char* name) {
		((SymTable*)ctx)->Add(addr, name);
	}
	inline int WalkExports(uintptr_t base, ExportSink sink, void* ctx) {
		int n = 0;
		__try {
			const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
			if (dos->e_magic != IMAGE_DOS_SIGNATURE)
				return 0;
			const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
				return 0;
			const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
			if (!dd.VirtualAddress || !dd.Size)
				return 0;
			const IMAGE_EXPORT_DIRECTORY* ed = (const IMAGE_EXPORT_DIRECTORY*)(base + dd.VirtualAddress);
			const DWORD* funcs = (const DWORD*)(base + ed->AddressOfFunctions);
			const DWORD* names = (const DWORD*)(base + ed->AddressOfNames);
			const WORD* ords = (const WORD*)(base + ed->AddressOfNameOrdinals);
			for (DWORD i = 0; i < ed->NumberOfNames && i < 65536; i++) {
				WORD o = ords[i];
				if (o >= ed->NumberOfFunctions)
					continue;
				DWORD rva = funcs[o];
				if (!rva || (rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size))   // przekierowanie do innej DLL
					continue;
				sink(ctx, (uint32_t)(base + rva), (const char*)(base + names[i]));
				n++;
			}
		} __except (EXCEPTION_EXECUTE_HANDLER) {
		}
		return n;
	}

	inline std::shared_ptr<SymTable> ExportsOf(uint32_t base, const std::string& name) {
		auto it = g_m.exports.find(base);
		if (it != g_m.exports.end())
			return it->second;
		auto t = std::make_shared<SymTable>();
		WalkExports(base, AddExport, t.get());
		t->Finish();
		std::shared_ptr<SymTable> r = t->Size() ? t : nullptr;
		g_m.exports[base] = r;
		(void)name;
		return r;
	}

	inline std::vector<HMODULE> ListModules() {
		std::vector<HMODULE> v(1024);
		DWORD need = 0;
		if (!EnumProcessModules(GetCurrentProcess(), v.data(), (DWORD)(v.size() * sizeof(HMODULE)), &need))
			return {};
		size_t n = need / sizeof(HMODULE);
		v.resize(n < v.size() ? n : v.size());
		return v;
	}

	inline void BuildMap(const std::vector<HMODULE>& mods) {
		g_m.map.Clear();
		g_m.mapMods = mods;
		for (HMODULE h : mods) {
			MODULEINFO mi;
			char path[MAX_PATH] = {};
			if (!GetModuleInformation(GetCurrentProcess(), h, &mi, sizeof(mi)) || !GetModuleFileNameA(h, path, MAX_PATH))
				continue;
			Module m;
			m.base = (uint32_t)(uintptr_t)mi.lpBaseOfDll;
			m.end = m.base + mi.SizeOfImage;
			bool exe = h == g_m.exe;
			m.kind = ClassifyPath(path, g_m.winDir, exe);
			m.name = BaseName(path);
			m.transit = IsTransit(m.name);
			if (exe) {
				m.syms = g_m.hostSyms;
				m.exact = true;
			} else
				m.syms = ExportsOf(m.base, m.name);
			g_m.map.Add(m);
		}
		for (const Module& p : g_m.pseudo) {
			bool covered = false;
			for (int i = 0; i < g_m.map.Count() && !covered; i++)
				covered = p.base < g_m.map.At(i).end && g_m.map.At(i).base < p.end;
			if (!covered)
				g_m.map.Add(p);
		}
		g_m.map.Finish();
		g_m.lastMap = GetTickCount64();
	}

	// Adres spoza modulow z listy: region pamieci (obraz 64-bitowy WOW64, modul zaladowany po ostatniej mapie,
	// trampolina hooka, kod generowany) jako modul zastepczy.
	inline void Probe(uint32_t addr) {
		uint32_t region = addr >> 16;
		if (std::find(g_m.probed.begin(), g_m.probed.end(), region) != g_m.probed.end() || g_m.probed.size() >= 1024)
			return;
		g_m.probed.push_back(region);
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery((const void*)(uintptr_t)addr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT)
			return;
		Module m;
		m.base = (uint32_t)(uintptr_t)mbi.AllocationBase;
		m.end = (uint32_t)((uintptr_t)mbi.BaseAddress + mbi.RegionSize);
		for (int i = 0; i < 256; i++) {   // koniec alokacji: kolejne regiony z ta sama baza
			MEMORY_BASIC_INFORMATION nx;
			if (!VirtualQuery((const void*)(uintptr_t)m.end, &nx, sizeof(nx)) || nx.AllocationBase != mbi.AllocationBase)
				break;
			m.end = (uint32_t)((uintptr_t)nx.BaseAddress + nx.RegionSize);
		}
		char buf[MAX_PATH] = {};
		if (mbi.Type == MEM_IMAGE && GetMappedFileNameA(GetCurrentProcess(), (void*)(uintptr_t)addr, buf, MAX_PATH)) {
			m.name = BaseName(buf);
			m.kind = ClassifyPath(buf, g_m.winDir, false);
			m.transit = IsTransit(m.name);
			m.syms = ExportsOf(m.base, m.name);
		} else {
			std::snprintf(buf, sizeof(buf), "%s %08X", mbi.Type == MEM_IMAGE ? "obraz" : "pamiec", m.base);
			m.name = buf;
			m.kind = mbi.Type == MEM_IMAGE ? MOD_OTHER : MOD_ANON;
		}
		g_m.pseudo.push_back(m);
		g_m.map.Add(m);
		g_m.map.Finish();
	}

	// Przed rozpoznaniem okna: nieznane EIP / pierwsze ramki -> nowa mapa (gdy zmienila sie lista modulow) albo region.
	inline void EnsureKnown(const std::vector<Sample>& v) {
		uint32_t unknown[MAX_PROBES];
		int n = 0;
		for (const Sample& s : v) {
			uint32_t a[2] = { s.eip, s.nret ? s.ret[0] : 0 };
			for (uint32_t x : a) {
				if (!x || g_m.map.Find(x) >= 0 || n >= MAX_PROBES)
					continue;
				if (std::find(g_m.probed.begin(), g_m.probed.end(), x >> 16) != g_m.probed.end())
					continue;
				bool dup = false;
				for (int i = 0; i < n; i++)
					dup = dup || (unknown[i] >> 16) == (x >> 16);
				if (!dup)
					unknown[n++] = x;
			}
		}
		if (!n)
			return;
		if (GetTickCount64() - g_m.lastMap >= MAP_REFRESH_MS) {
			std::vector<HMODULE> mods = ListModules();
			if (mods != g_m.mapMods)
				BuildMap(mods);
			g_m.lastMap = GetTickCount64();
		}
		for (int i = 0; i < n; i++)
			if (g_m.map.Find(unknown[i]) < 0)
				Probe(unknown[i]);
	}

	inline void Emit(const char* line) {
		zprof::g_rt.lines.push_back(line);
		zprof::Emit();
	}

	inline double AvgUs(uint64_t ticks, uint64_t n) {
		return n ? (double)ticks / (double)n / g_m.tpms * 1000.0 : 0.0;
	}

	// Koniec klatki (z zprof::FrameMark): probki okna [start, end) -> linia przycinki (po linii zprof) i statystyki.
	inline void OnFrame(uint64_t start, uint64_t end, zprof::FrameKind k, bool periodic, std::vector<std::string>& lines, size_t at) {
		if (!g_m.started)
			return;
		ZPROF_SCOPE("Prof.Probki");
		g_m.window.clear();
		uint64_t oldest = g_ring.Oldest();
		if (g_m.cursor < oldest)
			g_m.cursor = oldest;
		Sample s;
		for (;;) {
			auto r = g_ring.Get(g_m.cursor, s);
			if (r == Ring<RING>::NOT_YET)
				break;
			if (r == Ring<RING>::LOST) {
				g_m.cursor++;
				continue;
			}
			if (s.t >= end)
				break;   // nastepna klatka
			g_m.cursor++;
			if (start && s.t >= start)
				g_m.window.push_back(s);
		}
		bool use = start && (k == zprof::FRAME_SPIKE || k == zprof::FRAME_OK) && g_sh.want.load();
		g_m.frame.Clear();
		if (use && !g_m.window.empty()) {
			EnsureKnown(g_m.window);
			for (const Sample& x : g_m.window)
				g_m.frame.Add(Resolve(x, g_m.map), g_m.map, g_m.names);
		}
		if (use) {
			float ms = zprof::g_rt.prof.LastFrame().frameMs;
			if (k == zprof::FRAME_SPIKE) {
				std::string line = SpikeLine(g_m.frame, ms, g_m.names);
				lines.insert(lines.begin() + (at <= lines.size() ? at : lines.size()), line);
			}
			g_m.total.Add(g_m.frame, k == zprof::FRAME_SPIKE, ms);
			g_m.period.Add(g_m.frame, k == zprof::FRAME_SPIKE, ms);
		}
		if (periodic && g_sh.want.load()) {
			g_m.period.Lines(g_m.names, "z ostatniego okresu", lines);
			g_m.period.Clear();
		}
		if (g_sh.broken.load() && !g_m.brokenLogged) {
			g_m.brokenLogged = true;
			char buf[200];
			std::snprintf(buf, sizeof(buf), "Prof: probkowanie WYLACZONE - watek gry byl wstrzymany dluzej niz %.0f ms (watek "
				"probkujacy utknal?), wznowiony przez straz; do restartu gry", WATCHDOG_MS);
			lines.push_back(buf);
		}
	}

	// ---------- Sterowanie (glowny watek) ----------
	inline bool Start(int intervalMs, const std::string& hostText) {
		if (g_m.started || g_m.startFailed)
			return g_m.started;
		if (GetCurrentThreadId() != zprof::g_rt.mainThread || !zprof::g_rt.inited) {
			g_m.startFailed = true;
			Emit("Prof: probkowanie - start poza watkiem gry albo przed zprof::Init, wylaczone");
			return false;
		}
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		g_m.tpms = (double)f.QuadPart / 1000.0;
		g_m.exe = GetModuleHandleA(nullptr);
		g_m.mainTib = (const NT_TIB*)NtCurrentTeb();
		char win[MAX_PATH] = {};
		GetWindowsDirectoryA(win, MAX_PATH);
		g_m.winDir = win;
		g_m.mainThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
		if (!g_m.mainThread) {
			g_m.startFailed = true;
			Emit("Prof: probkowanie - OpenThread(watek gry) nieudane, wylaczone");
			return false;
		}
		g_m.hostSyms = std::make_shared<SymTable>();
		int funcs = hostText.empty() ? 0 : g_m.hostSyms->Parse(hostText);
		BuildMap(ListModules());
		g_createTimerEx = (CreateTimerExFn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateWaitableTimerExW");
		if (g_createTimerEx)
			g_m.timer = g_createTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		g_sh.intervalMs.store(intervalMs < 1 ? 1 : intervalMs);
		g_sh.calibrating.store(true);
		g_m.thread = CreateThread(nullptr, 64 * 1024, SamplerProc, nullptr, 0, nullptr);
		if (!g_m.thread) {
			g_m.startFailed = true;
			Emit("Prof: probkowanie - CreateThread nieudane, wylaczone");
			return false;
		}
		g_m.watchdog = CreateThread(nullptr, 16 * 1024, WatchdogProc, nullptr, 0, nullptr);
		// kalibracja: watek probkujacy wstrzymuje nas teraz kilkaset razy (my czekamy) - do 2 s
		ULONGLONG until = GetTickCount64() + 2000;
		while (g_sh.calibrating.load() && GetTickCount64() < until)
			Sleep(1);
		uint64_t n = g_sh.taken.load();
		double susp = AvgUs(g_sh.suspTicks.load(), n), total = AvgUs(g_sh.totalTicks.load(), n);
		double maxUs = (double)g_sh.maxSusp.load() / g_m.tpms * 1000.0;
		int iv = g_sh.intervalMs.load();
		bool slow = susp > SLOW_SAMPLE_US && iv < SLOW_INTERVAL_MS;
		if (slow)
			g_sh.intervalMs.store(SLOW_INTERVAL_MS);
		g_m.cursor = g_ring.Written();   // kalibracja nie jest klatka gry
		g_m.started = true;
		zprof::g_rt.frameHook = OnFrame;
		char buf[400];
		std::snprintf(buf, sizeof(buf), "Prof: probkowanie watku gry co %d ms (%s): probka sr %.1f us, watek gry wstrzymany sr %.1f us "
			"(max %.0f us; kalibracja %llu probek, nieudanych %llu)%s; funkcji exe %d%s, modulow %d", g_sh.intervalMs.load(),
			g_m.timer ? "timer wysokiej rozdzielczosci" : "timeBeginPeriod(1)", total, susp, maxUs, (unsigned long long)n,
			(unsigned long long)g_sh.failed.load(), slow ? " - za drogo, probkowanie rzadsze" : "", funcs,
			funcs ? "" : " (brak g2a_funcs.txt - tylko moduly)", g_m.map.Count());
		Emit(buf);
		return true;
	}

	// Ustawienia z ini; pierwsze wlaczenie startuje watek.
	inline void Configure(bool on, int intervalMs, const std::string& hostText) {
		int iv = intervalMs < 1 ? 1 : intervalMs > 100 ? 100 : intervalMs;
		if (on && !g_m.started && !Start(iv, hostText))
			return;
		if (g_m.started) {
			uint64_t n = g_sh.taken.load();
			double susp = AvgUs(g_sh.suspTicks.load(), n);
			g_sh.intervalMs.store(susp > SLOW_SAMPLE_US && iv < SLOW_INTERVAL_MS ? SLOW_INTERVAL_MS : iv);
		}
		g_sh.want.store(on);
	}

	inline bool IsStarted() {
		return g_m.started;
	}

	inline bool IsActive() {
		return g_m.started && g_sh.want.load() && !g_sh.broken.load();
	}

	inline void SetPaused(bool p) {
		g_sh.paused.store(p);
	}

	inline void RequestStop() {
		g_sh.stop.store(true);
	}

	inline void Stop() {
		g_sh.stop.store(true);
		if (zprof::g_rt.frameHook == OnFrame)
			zprof::g_rt.frameHook = nullptr;
		HANDLE hs[2] = { g_m.thread, g_m.watchdog };
		for (HANDLE h : hs)
			if (h) {
				WaitForSingleObject(h, 500);
				CloseHandle(h);
			}
		g_m.thread = g_m.watchdog = nullptr;
		if (g_m.timer)
			CloseHandle(g_m.timer);
		g_m.timer = nullptr;
		if (g_m.mainThread)
			CloseHandle(g_m.mainThread);
		g_m.mainThread = nullptr;
		g_m.started = false;
	}

	inline void Reset() {
		g_m.total.Clear();
		g_m.period.Clear();
	}

	// Tabele od resetu do logu, jedna linia do konsoli.
	inline std::string Summary() {
		if (!g_m.started)
			return "probkowanie nie dziala (ProfSampler=1, Profiler=1)";
		uint64_t n = g_sh.taken.load();
		char buf[300];
		std::snprintf(buf, sizeof(buf), "Prof: probkowanie: %llu probek, co %d ms, probka sr %.1f us (wstrzymanie sr %.1f, max %.0f us), "
			"nieudanych %llu%s", (unsigned long long)n, g_sh.intervalMs.load(), AvgUs(g_sh.totalTicks.load(), n),
			AvgUs(g_sh.suspTicks.load(), n), (double)g_sh.maxSusp.load() / g_m.tpms * 1000.0, (unsigned long long)g_sh.failed.load(),
			g_sh.broken.load() ? ", WYLACZONE przez straz" : "");
		zprof::g_rt.lines.push_back(buf);
		g_m.total.Lines(g_m.names, "od resetu", zprof::g_rt.lines);
		zprof::Emit();
		return g_m.total.Short(g_m.names);
	}
}
