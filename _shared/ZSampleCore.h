// Profiler probkujacy (sampling) dla pluginow Union - logika bez Windows i bez silnika. Czesc z watkiem probkujacym,
// wstrzymywaniem glownego watku i mapa modulow procesu: ZSampler.h. Testy: zDismember\tests\test_core.cpp.
//
// Model: watek probkujacy co ok. 1 ms wstrzymuje glowny watek gry, czyta EIP i kopie stosu, wznawia go i dopiero na
// kopii szuka adresow powrotu (lancuch EBP + skan slow). Probki (znacznik QPC) ida do pierscienia (jeden pisarz, jeden
// czytelnik). Glowny watek na koncu klatki (zprof::FrameMark) zbiera probki z okna tej klatki, rozpoznaje modul i
// funkcje (exe: tabela z Union SDK, inne moduly: najblizszy eksport) i liczy udzialy - dla przycinki linia w logu,
// do podsumowania osobno przycinki i klatki normalne (porownanie = co jest specyficzne dla przycinek).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>
#include <unordered_map>

namespace zsamp {
	const int MAX_RET = 8;                // adresow powrotu w probce
	const int SCAN_WORDS = 1024;          // skan stosu (slowa po 4 B) poza lancuchem EBP
	const int TOP_MODULES = 5;            // modulow w linii przycinki
	const int TOP_FUNCS = 3;              // funkcji w nawiasie modulu
	const int TOP_CALLERS = 2;            // "z <funkcja exe>" w nawiasie modulu spoza exe
	const int SUMMARY_TOP = 15;           // wierszy w tabelach funkcji / modulow podsumowania
	const int SUMMARY_TOP_CTX = 10;       // wierszy w tabelach "wywolane z" / "przez"
	const uint32_t EXPORT_NEAR = 0x800;   // eksport dalej niz tyle przed adresem = funkcja nieznana (to juz nie ta)

	struct Sample {
		uint64_t t = 0;              // QPC chwili probki
		uint32_t eip = 0;
		uint32_t ret[MAX_RET] = {};  // adresy powrotu od najglebszego
		uint32_t hostRet = 0;        // pierwszy adres powrotu w exe na calej kopii stosu (takze dalej niz ret[])
		uint8_t nret = 0;
	};

	// ---------- Pierscien probek: jeden pisarz (watek probkujacy), jeden czytelnik (glowny watek) ----------
	// Komorka ma numer sekwencyjny (seqlock): 0 w trakcie zapisu, potem indeks+1. Czytelnik sprawdza numer przed i po
	// kopii - nadpisana w miedzyczasie = LOST. Bez blokad i alokacji (pisarz nie moze czekac na glowny watek).
	template <int CAP>
	class Ring {
	public:
		enum Got { GOT, NOT_YET, LOST };
		void Push(const Sample& s) {
			uint64_t n = written_.load(std::memory_order_relaxed);
			Cell& c = cells_[n % CAP];
			c.seq.store(0, std::memory_order_relaxed);
			std::atomic_thread_fence(std::memory_order_release);
			c.s = s;
			c.seq.store(n + 1, std::memory_order_release);
			written_.store(n + 1, std::memory_order_release);
		}
		uint64_t Written() const {
			return written_.load(std::memory_order_acquire);
		}
		// Najstarszy indeks, ktory moze jeszcze byc w pierscieniu.
		uint64_t Oldest() const {
			uint64_t w = Written();
			return w > (uint64_t)CAP ? w - CAP : 0;
		}
		Got Get(uint64_t idx, Sample& out) const {
			if (idx >= Written())
				return NOT_YET;
			const Cell& c = cells_[idx % CAP];
			if (c.seq.load(std::memory_order_acquire) != idx + 1)
				return LOST;
			out = c.s;
			std::atomic_thread_fence(std::memory_order_acquire);
			return c.seq.load(std::memory_order_relaxed) == idx + 1 ? GOT : LOST;
		}
	private:
		struct Cell {
			std::atomic<uint64_t> seq{ 0 };
			Sample s;
		};
		Cell cells_[CAP];
		std::atomic<uint64_t> written_{ 0 };
	};

	// ---------- Przejscie stosu (na kopii, po wznowieniu watku gry) ----------
	// Dlugosc instrukcji FF /2 (call r/m32) od bajtu modrm (sib - kolejny bajt, gdy rm == 4).
	inline int CallFFLen(uint8_t modrm, uint8_t sib) {
		int mod = modrm >> 6, rm = modrm & 7, len = 2;
		if (mod == 3)
			return len;
		if (rm == 4) {
			len += 1;
			if (mod == 0 && (sib & 7) == 5)
				len += 4;
		} else if (mod == 0 && rm == 5)
			len += 4;
		if (mod == 1)
			len += 1;
		else if (mod == 2)
			len += 4;
		return len;
	}
	// b[0..6] = bajty pod adresami ret-7..ret-1. Prawda, gdy tuz przed adresem konczy sie call (E8 rel32 albo FF /2) -
	// odsiewa liczby / wskazniki na stosie, ktore tylko przypadkiem wpadaja w sekcje kodu.
	inline bool IsCallBefore(const uint8_t* b) {
		if (b[2] == 0xE8)
			return true;
		for (int k = 2; k <= 7; k++) {
			const uint8_t* p = b + 7 - k;
			if (p[0] != 0xFF || ((p[1] >> 3) & 7) != 2)
				continue;
			if (CallFFLen(p[1], k >= 3 ? p[2] : 0) == k)
				return true;
		}
		return false;
	}

	// words[0] lezy pod adresem esp, count slow. isRet(v): v to adres powrotu (kod modulu + call przed nim).
	// 1) skan od esp do ebp (funkcje bez ramki nad miejscem probki, np. stub ntdll), 2) lancuch EBP ([ebp] = poprzedni
	// ebp, [ebp+4] = powrot), 3) skan od konca lancucha. Razem do SCAN_WORDS slow skanu i maxOut adresow.
	template <class IsRet>
	int WalkStack(const uint32_t* words, int count, uint32_t esp, uint32_t ebp, IsRet isRet, uint32_t* out, int maxOut) {
		int n = 0;
		if (count <= 0 || maxOut <= 0)
			return 0;
		uint64_t end = (uint64_t)esp + (uint64_t)count * 4;
		bool chain = ebp >= esp && (uint64_t)ebp + 8 <= end && (ebp & 3) == 0;
		int scanned = 0;
		auto scan = [&](int from, int to) {
			for (int i = from; i < to && i < count && n < maxOut && scanned < SCAN_WORDS; i++, scanned++) {
				uint32_t v = words[i];
				if (isRet(v) && (n == 0 || out[n - 1] != v))
					out[n++] = v;
			}
		};
		int resume = 0;
		if (chain) {
			scan(0, (int)((ebp - esp) / 4));
			uint32_t frame = ebp;
			while (n < maxOut && frame >= esp && (uint64_t)frame + 8 <= end && (frame & 3) == 0) {
				int i = (int)((frame - esp) / 4);
				uint32_t next = words[i], ret = words[i + 1];
				if (!isRet(ret))
					break;
				out[n++] = ret;
				resume = i + 2;
				if (next <= frame)
					break;
				frame = next;
			}
			if (!resume)
				resume = (int)((ebp - esp) / 4);
		}
		scan(resume, count);
		return n;
	}

	// Pierwsze slowo spelniajace pred (np. pierwszy adres powrotu w exe na calej kopii stosu); 0 = brak.
	template <class Pred>
	uint32_t FindFirst(const uint32_t* words, int count, Pred pred) {
		for (int i = 0; i < count; i++)
			if (pred(words[i]))
				return words[i];
		return 0;
	}

	// Zakresy kodu (sekcje wykonywalne modulow) dla watku probkujacego: stala tablica, bez alokacji.
	struct CodeRanges {
		static const int MAX = 1024;
		uint32_t lo[MAX], hi[MAX];
		int n = 0;
		uint32_t hostLo = 0, hostHi = 0;
		void Clear() {
			n = 0;
			hostLo = hostHi = 0;
		}
		bool Add(uint32_t a, uint32_t b) {
			if (n >= MAX || b <= a)
				return false;
			lo[n] = a;
			hi[n] = b;
			n++;
			return true;
		}
		void SetHost(uint32_t a, uint32_t b) {
			hostLo = a;
			hostHi = b;
		}
		// sortowanie przez wstawianie - bez alokacji, kilkaset zakresow raz na kilka sekund
		void Finish() {
			for (int i = 1; i < n; i++) {
				uint32_t a = lo[i], b = hi[i];
				int j = i;
				for (; j > 0 && lo[j - 1] > a; j--) {
					lo[j] = lo[j - 1];
					hi[j] = hi[j - 1];
				}
				lo[j] = a;
				hi[j] = b;
			}
		}
		int Count() const {
			return n;
		}
		bool Contains(uint32_t v) const {
			int a = 0, b = n - 1;
			while (a <= b) {
				int m = (a + b) / 2;
				if (v < lo[m])
					b = m - 1;
				else if (v >= hi[m])
					a = m + 1;
				else
					return true;
			}
			return false;
		}
		bool InHost(uint32_t v) const {
			return v >= hostLo && v < hostHi;
		}
	};

	// ---------- Symbole: adres -> nazwa (najblizszy <= adres) ----------
	class SymTable {
	public:
		void Add(uint32_t addr, const char* name) {
			entries_.push_back({ addr, (uint32_t)names_.size() });
			names_.append(name ? name : "");
			names_.push_back('\0');
		}
		// Sortuje po adresie; przy tym samym adresie zostaje pierwsza nazwa alfabetycznie (NtX przed ZwX).
		void Finish() {
			const std::string& nm = names_;
			std::stable_sort(entries_.begin(), entries_.end(), [&nm](const Entry& a, const Entry& b) {
				if (a.addr != b.addr)
					return a.addr < b.addr;
				return std::strcmp(nm.c_str() + a.name, nm.c_str() + b.name) < 0;
			});
			entries_.erase(std::unique(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.addr == b.addr; }),
				entries_.end());
		}
		// "XXXXXXXX nazwa" na linie (hex), '#' = komentarz, zle linie pomijane. Przy powtorzonym adresie - pierwsza
		// linia z pliku. Zwraca liczbe wpisow po Finish.
		int Parse(const std::string& text) {
			size_t pos = 0;
			while (pos < text.size()) {
				size_t eol = text.find('\n', pos);
				if (eol == std::string::npos)
					eol = text.size();
				size_t e = eol;
				while (e > pos && (text[e - 1] == '\r' || text[e - 1] == ' '))
					e--;
				uint32_t addr = 0;
				size_t i = pos;
				int digits = 0;
				for (; i < e && std::isxdigit((unsigned char)text[i]); i++, digits++)
					addr = addr * 16 + (uint32_t)(std::isdigit((unsigned char)text[i]) ? text[i] - '0' : (std::toupper((unsigned char)text[i]) - 'A' + 10));
				if (digits > 0 && digits <= 8 && i < e && text[i] == ' ' && text[pos] != '#')
					Add(addr, text.substr(i + 1, e - i - 1).c_str());
				pos = eol + 1;
			}
			// stabilnie po samym adresie - przy powtorzonym adresie zostaje pierwsza linia pliku
			std::stable_sort(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.addr < b.addr; });
			entries_.erase(std::unique(entries_.begin(), entries_.end(), [](const Entry& a, const Entry& b) { return a.addr == b.addr; }),
				entries_.end());
			return Size();
		}
		int Find(uint32_t addr) const {
			auto it = std::upper_bound(entries_.begin(), entries_.end(), addr, [](uint32_t v, const Entry& e) { return v < e.addr; });
			if (it == entries_.begin())
				return -1;
			return (int)(it - entries_.begin()) - 1;
		}
		uint32_t Addr(int i) const {
			return entries_[i].addr;
		}
		const char* Name(int i) const {
			return names_.c_str() + entries_[i].name;
		}
		int Size() const {
			return (int)entries_.size();
		}
	private:
		struct Entry {
			uint32_t addr, name;
		};
		std::vector<Entry> entries_;
		std::string names_;
	};

	// ---------- Moduly ----------
	enum ModKind : uint8_t {
		MOD_HOST,     // exe gry
		MOD_SYSTEM,   // z katalogu Windows (ntdll, kernelbase, d3d11, sterownik GPU z DriverStore)
		MOD_OTHER,    // reszta: pluginy Union, renderer (ddraw.dll GD3D11), biblioteki gry, nakladka Steam
		MOD_ANON      // pamiec bez modulu (trampoliny hookow, kod generowany)
	};

	inline std::string Lower(std::string s) {
		for (auto& c : s)
			c = (char)std::tolower((unsigned char)c);
		return s;
	}
	inline std::string BaseName(const std::string& path) {
		size_t k = path.find_last_of("\\/");
		return k == std::string::npos ? path : path.substr(k + 1);
	}
	// winDir np. "C:\Windows"; sciezki urzadzen (GetMappedFileName: "\Device\HarddiskVolume3\Windows\...") tez.
	inline ModKind ClassifyPath(const std::string& path, const std::string& winDir, bool isExe) {
		if (isExe)
			return MOD_HOST;
		std::string p = Lower(path), w = Lower(winDir);
		while (!w.empty() && (w.back() == '\\' || w.back() == '/'))
			w.pop_back();
		if (!w.empty() && p.compare(0, w.size() + 1, w + "\\") == 0)
			return MOD_SYSTEM;
		if (p.compare(0, 8, "\\device\\") == 0) {
			size_t colon = w.find(':');
			std::string tail = colon == std::string::npos ? w : w.substr(colon + 1);   // "\windows"
			size_t vol = p.find('\\', 8);   // za "\device\harddiskvolumeN"
			if (!tail.empty() && vol != std::string::npos && p.compare(vol, tail.size() + 1, tail + "\\") == 0)
				return MOD_SYSTEM;
		}
		return MOD_OTHER;
	}
	// Przejscie WOW64 (32-bitowy kod -> 64-bitowe wywolanie systemowe): miejsce probki liczone od pierwszej ramki.
	inline bool IsTransit(const std::string& name) {
		std::string n = Lower(name);
		return n == "wow64cpu.dll" || n == "wow64.dll" || n == "wow64win.dll";
	}

	struct Module {
		uint32_t base = 0, end = 0;
		ModKind kind = MOD_OTHER;
		std::string name;
		std::shared_ptr<const SymTable> syms;   // exe: funkcje (g2a_funcs.txt), inne: eksporty
		bool exact = false;                     // syms to poczatki funkcji (exe) - bez limitu EXPORT_NEAR
		bool transit = false;                   // IsTransit
	};

	class ModuleMap {
	public:
		void Clear() {
			mods_.clear();
			host_ = -1;
		}
		void Add(const Module& m) {
			mods_.push_back(m);
		}
		void Finish() {
			std::sort(mods_.begin(), mods_.end(), [](const Module& a, const Module& b) { return a.base < b.base; });
			host_ = -1;
			for (int i = 0; i < (int)mods_.size(); i++)
				if (mods_[i].kind == MOD_HOST)
					host_ = i;
		}
		int Find(uint32_t addr) const {
			auto it = std::upper_bound(mods_.begin(), mods_.end(), addr, [](uint32_t v, const Module& m) { return v < m.base; });
			if (it == mods_.begin())
				return -1;
			--it;
			return addr < it->end ? (int)(it - mods_.begin()) : -1;
		}
		const Module& At(int i) const {
			return mods_[i];
		}
		int Count() const {
			return (int)mods_.size();
		}
		int Host() const {
			return host_;
		}
	private:
		std::vector<Module> mods_;
		int host_ = -1;
	};

	inline int SymAt(const Module& m, uint32_t addr) {
		if (!m.syms)
			return -1;
		int i = m.syms->Find(addr);
		if (i < 0 || addr >= m.end)
			return -1;
		if (!m.exact && addr - m.syms->Addr(i) > EXPORT_NEAR)
			return -1;
		return i;
	}

	// ---------- Rozpoznanie probki ----------
	struct Hit {
		int mod = -1;          // modul miejsca probki; -1 = poza modulami
		uint32_t loc = 0;      // adres miejsca (EIP albo pierwsza ramka)
		int sym = -1;          // funkcja w module (indeks w syms)
		int caller = -1;       // miejsce spoza exe: pierwsza funkcja exe na stosie (indeks w syms exe)
		int via = -1;          // pierwszy modul MOD_OTHER na stosie, inny niz modul miejsca
		bool moved = false;    // EIP w pamieci bez modulu / przejsciu WOW64 - miejsce = pierwsza ramka
	};

	inline Hit Resolve(const Sample& s, const ModuleMap& map) {
		Hit h;
		int m = map.Find(s.eip), first = 0;
		h.loc = s.eip;
		if ((m < 0 || map.At(m).kind == MOD_ANON || map.At(m).transit) && s.nret > 0) {
			int m2 = map.Find(s.ret[0]);
			if (m2 >= 0 && map.At(m2).kind != MOD_ANON) {
				m = m2;
				h.loc = s.ret[0];
				h.moved = true;
				first = 1;
			}
		}
		h.mod = m;
		if (m >= 0)
			h.sym = SymAt(map.At(m), h.loc);
		int host = map.Host();
		if (host >= 0 && m != host) {
			uint32_t c = 0;
			for (int i = first; i < s.nret && !c; i++)
				if (map.Find(s.ret[i]) == host)
					c = s.ret[i];
			if (!c && s.hostRet && map.Find(s.hostRet) == host)
				c = s.hostRet;
			if (c)
				h.caller = SymAt(map.At(host), c);
		}
		for (int i = first; i < s.nret; i++) {
			int k = map.Find(s.ret[i]);
			if (k >= 0 && k != m && map.At(k).kind == MOD_OTHER) {
				h.via = k;
				break;
			}
		}
		return h;
	}

	// ---------- Nazwy kluczy (adresy: baza modulu / poczatek funkcji) - trwale miedzy odswiezeniami mapy ----------
	struct Names {
		std::unordered_map<uint32_t, std::string> mod, func;
		std::unordered_map<uint32_t, uint8_t> kind;   // rodzaj modulu po bazie
		std::string host;                             // nazwa exe (kolumna modulu w tabeli "wywolane z")
		const char* Mod(uint32_t k) const {
			auto it = mod.find(k);
			return it != mod.end() ? it->second.c_str() : "(poza modulami)";
		}
		const char* Func(uint32_t k) const {
			auto it = func.find(k);
			return it != func.end() ? it->second.c_str() : "(nieznana)";
		}
		ModKind Kind(uint32_t k) const {
			auto it = kind.find(k);
			return it != kind.end() ? (ModKind)it->second : MOD_ANON;
		}
	};

	struct Tally {
		uint32_t key = 0, n = 0;
	};
	inline void Bump(std::vector<Tally>& v, uint32_t key) {
		for (auto& t : v)
			if (t.key == key) {
				t.n++;
				return;
			}
		v.push_back({ key, 1 });
	}
	// Malejaco po n, przy rownych - kolejnosc pojawienia sie.
	inline void SortTallies(std::vector<Tally>& v) {
		std::stable_sort(v.begin(), v.end(), [](const Tally& a, const Tally& b) { return a.n > b.n; });
	}

	// ---------- Agregacja okna jednej klatki ----------
	struct ModAgg {
		uint32_t key = 0, n = 0;
		std::vector<Tally> funcs, callers, vias;
	};
	class FrameAgg {
	public:
		int samples = 0;
		std::vector<ModAgg> mods;
		void Clear() {
			samples = 0;
			mods.clear();
		}
		void Add(const Hit& h, const ModuleMap& map, Names& names) {
			samples++;
			uint32_t mk = 0;
			if (h.mod >= 0) {
				const Module& m = map.At(h.mod);
				mk = m.base;
				if (!names.mod.count(mk)) {
					names.mod[mk] = m.name;
					names.kind[mk] = (uint8_t)m.kind;
				}
			}
			ModAgg* a = nullptr;
			for (auto& x : mods)
				if (x.key == mk) {
					a = &x;
					break;
				}
			if (!a) {
				mods.push_back(ModAgg());
				a = &mods.back();
				a->key = mk;
			}
			a->n++;
			uint32_t fk = 0;
			if (h.mod >= 0 && h.sym >= 0) {
				const SymTable& st = *map.At(h.mod).syms;
				fk = st.Addr(h.sym);
				if (!names.func.count(fk))
					names.func[fk] = st.Name(h.sym);
			}
			Bump(a->funcs, fk);
			int host = map.Host();
			if (h.caller >= 0 && host >= 0) {
				if (names.host.empty())
					names.host = map.At(host).name;
				const SymTable& st = *map.At(host).syms;
				uint32_t ck = st.Addr(h.caller);
				if (!names.func.count(ck))
					names.func[ck] = st.Name(h.caller);
				Bump(a->callers, ck);
			}
			if (h.via >= 0) {
				const Module& v = map.At(h.via);
				if (!names.mod.count(v.base)) {
					names.mod[v.base] = v.name;
					names.kind[v.base] = (uint8_t)v.kind;
				}
				Bump(a->vias, v.base);
			}
		}
	};

	inline int Pct(uint32_t n, int total) {
		return total > 0 ? (int)(100.0 * n / total + 0.5) : 0;
	}

	// "Prof: probki 73.2 ms (68 probek): Gothic2.exe 55% [f1 30%, f2 10%], ntdll.dll 15% [NtWait... 12%; z oCGame::Render
	// 10%; przez ddraw.dll 8%], ..., inne 4%" - top TOP_MODULES; procenty z liczby probek okna.
	inline std::string SpikeLine(const FrameAgg& a, float frameMs, const Names& names) {
		char buf[256];
		if (a.samples <= 0) {
			std::snprintf(buf, sizeof(buf), "Prof: probki %.1f ms - brak probek (probkowanie wstrzymane albo nie nadazylo)", frameMs);
			return buf;
		}
		std::snprintf(buf, sizeof(buf), "Prof: probki %.1f ms (%d probek): ", frameMs, a.samples);
		std::string s = buf;
		std::vector<int> order;
		for (int i = 0; i < (int)a.mods.size(); i++)
			order.push_back(i);
		std::stable_sort(order.begin(), order.end(), [&a](int x, int y) { return a.mods[x].n > a.mods[y].n; });
		uint32_t rest = 0;
		for (int r = 0; r < (int)order.size(); r++) {
			const ModAgg& m = a.mods[order[r]];
			if (r >= TOP_MODULES) {
				rest += m.n;
				continue;
			}
			std::snprintf(buf, sizeof(buf), "%s%s %d%%", r ? ", " : "", names.Mod(m.key), Pct(m.n, a.samples));
			s += buf;
			bool host = m.key && names.Kind(m.key) == MOD_HOST;
			std::vector<std::string> groups;
			std::vector<Tally> t = m.funcs;
			SortTallies(t);
			std::string g;
			int shown = 0;
			for (auto& f : t) {
				if (!f.key || shown >= TOP_FUNCS)
					continue;
				std::snprintf(buf, sizeof(buf), "%s%s %d%%", shown ? ", " : "", names.Func(f.key), Pct(f.n, a.samples));
				g += buf;
				shown++;
			}
			if (!g.empty())
				groups.push_back(g);
			if (!host) {
				t = m.callers;
				SortTallies(t);
				g.clear();
				for (int i = 0; i < (int)t.size() && i < TOP_CALLERS; i++) {
					std::snprintf(buf, sizeof(buf), "%sz %s %d%%", i ? ", " : "", names.Func(t[i].key), Pct(t[i].n, a.samples));
					g += buf;
				}
				if (!g.empty())
					groups.push_back(g);
				t = m.vias;
				SortTallies(t);
				if (!t.empty()) {
					std::snprintf(buf, sizeof(buf), "przez %s %d%%", names.Mod(t[0].key), Pct(t[0].n, a.samples));
					groups.push_back(buf);
				}
			}
			if (!groups.empty()) {
				s += " [";
				for (size_t i = 0; i < groups.size(); i++)
					s += (i ? "; " : "") + groups[i];
				s += "]";
			}
		}
		if (rest) {
			std::snprintf(buf, sizeof(buf), ", inne %d%%", Pct(rest, a.samples));
			s += buf;
		}
		return s;
	}

	// ---------- Podsumowanie: przycinki osobno od klatek normalnych ----------
	struct SetStats {
		uint64_t frames = 0, samples = 0;
		double ms = 0.0;   // suma czasu klatek
		std::unordered_map<uint32_t, uint64_t> mods;
		std::unordered_map<uint64_t, uint64_t> funcs;   // (baza modulu << 32) | poczatek funkcji (0 = nieznana)
		std::unordered_map<uint32_t, uint64_t> callers, vias;
		void Clear() {
			frames = samples = 0;
			ms = 0.0;
			mods.clear();
			funcs.clear();
			callers.clear();
			vias.clear();
		}
		void Add(const FrameAgg& a, float frameMs) {
			frames++;
			samples += (uint64_t)a.samples;
			ms += frameMs;
			for (auto& m : a.mods) {
				mods[m.key] += m.n;
				for (auto& f : m.funcs)
					funcs[((uint64_t)m.key << 32) | f.key] += f.n;
				for (auto& c : m.callers)
					callers[c.key] += c.n;
				for (auto& v : m.vias)
					vias[v.key] += v.n;
			}
		}
		// Probki -> ms na klatke tego zbioru (czas klatek rozlozony rowno na probki).
		double MsPerFrame(uint64_t count) const {
			return frames && samples ? (double)count * (ms / (double)samples) / (double)frames : 0.0;
		}
	};

	class Stats {
	public:
		SetStats spike, normal;
		void Clear() {
			spike.Clear();
			normal.Clear();
		}
		// Klatka bez probek (probkowanie wstrzymane) nie wchodzi - zanizalaby ms na klatke.
		void Add(const FrameAgg& a, bool isSpike, float frameMs) {
			if (a.samples <= 0)
				return;
			(isSpike ? spike : normal).Add(a, frameMs);
		}

		void Lines(const Names& names, const char* label, std::vector<std::string>& out) const {
			char buf[320];
			std::snprintf(buf, sizeof(buf), "Prof: probki %s: przycinki %llu kl. (%.1f s, %llu probek, sr %.1f ms), normalne %llu kl. "
				"(%.1f s, %llu probek, sr %.1f ms)", label, (unsigned long long)spike.frames, spike.ms / 1000.0,
				(unsigned long long)spike.samples, spike.frames ? spike.ms / spike.frames : 0.0, (unsigned long long)normal.frames,
				normal.ms / 1000.0, (unsigned long long)normal.samples, normal.frames ? normal.ms / normal.frames : 0.0);
			out.push_back(buf);
			if (!spike.frames && !normal.frames)
				return;
			std::vector<Row> rows;
			for (auto& m : spike.mods)
				rows.push_back({ (uint64_t)m.first << 32, spike.MsPerFrame(m.second), 0.0 });
			for (auto& m : normal.mods)
				Merge(rows, (uint64_t)m.first << 32, normal.MsPerFrame(m.second));
			Table(rows, true, SUMMARY_TOP, "moduly, wg nadwyzki", false, names, out);
			rows.clear();
			for (auto& f : spike.funcs)
				rows.push_back({ f.first, spike.MsPerFrame(f.second), 0.0 });
			for (auto& f : normal.funcs)
				Merge(rows, f.first, normal.MsPerFrame(f.second));
			Table(rows, true, SUMMARY_TOP, "funkcje - przycinki, wg nadwyzki", true, names, out);
			Table(rows, false, SUMMARY_TOP, "funkcje - klatki normalne, wg czasu", true, names, out);
			rows.clear();
			for (auto& c : spike.callers)
				rows.push_back({ CallerKey(c.first), spike.MsPerFrame(c.second), 0.0 });
			for (auto& c : normal.callers)
				Merge(rows, CallerKey(c.first), normal.MsPerFrame(c.second));
			Table(rows, true, SUMMARY_TOP_CTX, "wywolane z (funkcja exe pod probka spoza exe), wg nadwyzki", true, names, out);
			rows.clear();
			for (auto& v : spike.vias)
				rows.push_back({ (uint64_t)v.first << 32, spike.MsPerFrame(v.second), 0.0 });
			for (auto& v : normal.vias)
				Merge(rows, (uint64_t)v.first << 32, normal.MsPerFrame(v.second));
			Table(rows, true, SUMMARY_TOP_CTX, "obcy modul na stosie (przez), wg nadwyzki", false, names, out);
		}

		// Jedna linia do konsoli: dwie najwieksze nadwyzki funkcji w przycinkach.
		std::string Short(const Names& names) const {
			char buf[320];
			if (!spike.frames) {
				std::snprintf(buf, sizeof(buf), "probki: brak przycinek (klatek normalnych %llu) - tabela w logu",
					(unsigned long long)normal.frames);
				return buf;
			}
			std::vector<Row> rows;
			for (auto& f : spike.funcs)
				rows.push_back({ f.first, spike.MsPerFrame(f.second), 0.0 });
			for (auto& f : normal.funcs)
				Merge(rows, f.first, normal.MsPerFrame(f.second));
			std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.Excess() > b.Excess(); });
			std::snprintf(buf, sizeof(buf), "probki: przycinek %llu (sr %.0f ms), najwieksza nadwyzka:", (unsigned long long)spike.frames,
				spike.ms / spike.frames);
			std::string s = buf;
			for (size_t i = 0; i < rows.size() && i < 2; i++) {
				std::snprintf(buf, sizeof(buf), "%s %s +%.1f ms", i ? "," : "", FuncLabel(rows[i].key, names).c_str(), rows[i].Excess());
				s += buf;
			}
			return s + " na przycinke (tabela w logu)";
		}

	private:
		struct Row {
			uint64_t key;
			double spikeMs, normalMs;
			double Excess() const {
				return spikeMs - normalMs;
			}
		};
		static const uint64_t CALLER_TAG = 0xFFFFFFFFull << 32;   // klucz "wywolane z": funkcja exe bez modulu w kluczu
		static uint64_t CallerKey(uint32_t f) {
			return CALLER_TAG | f;
		}
		static void Merge(std::vector<Row>& rows, uint64_t key, double normalMs) {
			for (auto& r : rows)
				if (r.key == key) {
					r.normalMs = normalMs;
					return;
				}
			rows.push_back({ key, 0.0, normalMs });
		}
		static std::string FuncLabel(uint64_t key, const Names& names) {
			uint32_t mk = (uint32_t)(key >> 32), fk = (uint32_t)key;
			if ((key & CALLER_TAG) == CALLER_TAG)
				return names.Func(fk);
			if (!fk)
				return std::string(names.Mod(mk)) + " (nieznana)";
			return names.Func(fk);
		}
		static void Table(std::vector<Row> rows, bool byExcess, int top, const char* title, bool funcs, const Names& names,
			std::vector<std::string>& out) {
			if (byExcess)
				std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.Excess() > b.Excess(); });
			else
				std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.normalMs > b.normalMs; });
			char buf[320];
			std::snprintf(buf, sizeof(buf), "Prof:   [%s; ms na klatke]", title);
			out.push_back(buf);
			std::snprintf(buf, sizeof(buf), "Prof:   %-48s %-18s %9s %9s %9s", funcs ? "funkcja" : "modul", funcs ? "modul" : "",
				"przyc.", "norm.", "nadwyzka");
			out.push_back(buf);
			int n = 0;
			for (auto& r : rows) {
				if (n >= top)
					break;
				if (r.spikeMs < 0.005 && r.normalMs < 0.005)
					continue;
				uint32_t mk = (uint32_t)(r.key >> 32);
				bool caller = (r.key & CALLER_TAG) == CALLER_TAG;
				std::string name = funcs ? FuncLabel(r.key, names) : names.Mod(mk);
				const char* mod = funcs && !caller ? names.Mod(mk) : (caller ? (names.host.empty() ? "exe" : names.host.c_str()) : "");
				std::snprintf(buf, sizeof(buf), "Prof:   %-48.48s %-18.18s %9.2f %9.2f %+9.2f", name.c_str(), mod, r.spikeMs, r.normalMs,
					r.Excess());
				out.push_back(buf);
				n++;
			}
		}
	};
}
