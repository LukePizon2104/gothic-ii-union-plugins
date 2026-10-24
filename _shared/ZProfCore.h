// Profiler sekcji dla pluginow Union (zDismember, zTimeRewind) - logika bez silnika i bez Windows.
// Czesc z zegarem (QueryPerformanceCounter), pamiecia wspolna i makrem ZPROF_SCOPE: ZProf.h. Testy: zDismember\tests.
//
// Model: sekcje maja liczniki narastajace (tykniecia czasu wlasnego i calkowitego, wywolania) w slocie pluginu.
// Slot lezy we wspolnej tablicy procesu (kazdy plugin swoj slot), wiec plugin widzi tez liczniki innych pluginow.
// Mark() na poczatku Game_Loop zamyka klatke: delty licznikow od poprzedniego Mark = czas sekcji w tej klatce.
// Czas wlasny = bez sekcji zagniezdzonych - sumy czasow wlasnych daja czas pluginu bez liczenia czegokolwiek dwa razy.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace zprof {
	const int MAX_SECTIONS = 64;     // sekcji na plugin; ostatnia to "(inne)" - nadmiar nazw
	const int MAX_SLOTS = 8;         // pluginow we wspolnej tablicy
	const int NAME_LEN = 32;         // nazwa sekcji / pluginu (z zerem)
	const int WINDOW = 120;          // klatek do mediany
	const int MIN_WINDOW = 15;       // mniej klatek w oknie - prog przycinki = sam ProfSpikeMs
	const int MAX_DEPTH = 32;        // zagniezdzenie sekcji
	const int TOP_SECTIONS = 5;      // najdrozszych sekcji w linii przycinki
	const float GAP_MS = 1000.0f;    // tak dluga klatka, w ktorej plugin zajal < polowy = przerwa (alt-tab, okno systemowe)
	const float TOP_MIN_MS = 0.05f;  // krotsze sekcje nie trafiaja do linii przycinki
	const int HIST_FINE = 1000;      // histogram czasu klatki: co 0,1 ms do 100 ms
	const int HIST_COARSE = 1000;    // potem co 1 ms do 1100 ms, dalej jeden kubel
	const int HIST_BINS = HIST_FINE + HIST_COARSE + 1;
	const uint32_t TABLE_MAGIC = 0x464F5250u;   // "PROF"
	const uint32_t TABLE_VERSION = 1;           // zmiana ukladu Table/Slot = nowa wersja (inna nazwa pamieci w ZProf.h)

	// ---------- Wspolna tablica (pamiec nazwana procesu; tu bez Windows) ----------
	struct SlotSection {
		char name[NAME_LEN];
		uint64_t self;    // tykniecia czasu wlasnego, narastajaco
		uint64_t incl;    // tykniecia czasu calkowitego (z zagniezdzonymi)
		uint64_t calls;
	};
	struct Slot {
		int32_t used;
		int32_t sectionCount;
		char plugin[NAME_LEN];
		uint64_t pluginTicks;   // narastajaco: czas sekcji najwyzszego poziomu = czas pluginu
		uint64_t frames;        // wywolan Mark (klatek pluginu)
		SlotSection sec[MAX_SECTIONS];
	};
	struct Table {
		uint32_t magic, version, size, slotCount;
		Slot slots[MAX_SLOTS];
	};

	inline void CopyName(char* dst, const char* src) {
		size_t n = src ? std::strlen(src) : 0;
		if (n > (size_t)NAME_LEN - 1)
			n = NAME_LEN - 1;
		if (n)
			std::memcpy(dst, src, n);
		dst[n] = '\0';
	}

	inline bool SameName(const char* a, const char* b) {
		return std::strncmp(a, b, NAME_LEN - 1) == 0;
	}

	// Swieza tablica (pamiec wyzerowana) dostaje naglowek; istniejaca musi miec ten sam uklad.
	inline bool TableAttach(Table& t) {
		if (t.magic == 0 && t.version == 0 && t.size == 0 && t.slotCount == 0) {
			t.magic = TABLE_MAGIC;
			t.version = TABLE_VERSION;
			t.size = (uint32_t)sizeof(Table);
			t.slotCount = MAX_SLOTS;
			return true;
		}
		return t.magic == TABLE_MAGIC && t.version == TABLE_VERSION && t.size == (uint32_t)sizeof(Table) &&
			t.slotCount == (uint32_t)MAX_SLOTS;
	}

	// Slot pluginu: ten sam plugin dostaje swoj slot z powrotem, nowy - pierwszy wolny; -1 = tablica pelna.
	inline int TableClaim(Table& t, const char* plugin) {
		for (int i = 0; i < MAX_SLOTS; i++)
			if (t.slots[i].used && SameName(t.slots[i].plugin, plugin))
				return i;
		for (int i = 0; i < MAX_SLOTS; i++)
			if (!t.slots[i].used) {
				CopyName(t.slots[i].plugin, plugin);
				t.slots[i].used = 1;
				return i;
			}
		return -1;
	}

	// ---------- Mediana ostatnich WINDOW klatek ----------
	class Median {
	public:
		void Push(float v) {
			buf_[pos_] = v;
			pos_ = (pos_ + 1) % WINDOW;
			if (count_ < WINDOW)
				count_++;
		}
		int Count() const {
			return count_;
		}
		float Value() const {
			if (count_ == 0)
				return 0.0f;
			float tmp[WINDOW];
			std::memcpy(tmp, buf_, sizeof(float) * count_);   // przed zapelnieniem okna dane leza od 0
			std::sort(tmp, tmp + count_);
			int h = count_ / 2;
			return count_ % 2 ? tmp[h] : 0.5f * (tmp[h - 1] + tmp[h]);
		}
		void Clear() {
			count_ = pos_ = 0;
		}
	private:
		float buf_[WINDOW] = {};
		int count_ = 0, pos_ = 0;
	};

	// Prog przycinki: max(ProfSpikeMs, 2 x mediana); przy malo klatkach w oknie sam ProfSpikeMs.
	inline float SpikeThreshold(float spikeMs, const Median& m) {
		if (m.Count() < MIN_WINDOW)
			return spikeMs;
		float t = 2.0f * m.Value();
		return t > spikeMs ? t : spikeMs;
	}

	enum FrameKind {
		FRAME_NONE,    // pierwszy Mark po starcie / Break - tylko start pomiaru
		FRAME_OK,
		FRAME_SPIKE,   // przycinka
		FRAME_GAP      // przerwa (bardzo dluga klatka, nie przez plugin) - poza statystykami klatek
	};

	inline FrameKind Classify(float frameMs, float pluginMs, float threshold) {
		if (frameMs >= GAP_MS && pluginMs < 0.5f * frameMs)
			return FRAME_GAP;
		return frameMs > threshold ? FRAME_SPIKE : FRAME_OK;
	}

	// ---------- Werdykt przycinki: kto winien ----------
	const float CAUSE_MIN_MS = 2.0f;   // plugin z mniejszym czasem w klatce nigdy nie jest przyczyna (nawet czesciowa)
	enum CauseKind {
		CAUSE_OUTSIDE,   // poza pluginami
		CAUSE_PARTIAL,   // czesciowo plugin 'who'
		CAUSE_PLUGIN     // przyczyna: plugin 'who'
	};
	struct Cause {
		CauseKind kind = CAUSE_OUTSIDE;
		int who = -1;   // -1 = ten plugin, >= 0 = indeks w othersMs
	};
	// Nadwyzka = klatka - mediana; bez wiarygodnej mediany (baseMs < 0: start, po wczytaniu) cala klatka - wtedy
	// mediana z kilku klatek po ~50 ms dawala nadwyzke ~0 i 0,1 ms pluginu "wystarczalo" na werdykt (log 16.10).
	// Przyczyna: plugin z najwiekszym czasem >= polowy nadwyzki; poza pluginami: suma pluginow < 1/4 nadwyzki;
	// pomiedzy - czesciowo. Najwiekszy czas < CAUSE_MIN_MS = zawsze poza pluginami.
	inline Cause Attribute(float frameMs, float baseMs, float ownMs, const float* othersMs, int otherCount) {
		float excess = frameMs - (baseMs >= 0.0f ? baseMs : 0.0f);
		if (excess < 0.001f)
			excess = 0.001f;
		Cause c;
		float best = ownMs, sum = ownMs;
		for (int i = 0; i < otherCount; i++) {
			sum += othersMs[i];
			if (othersMs[i] > best) {
				best = othersMs[i];
				c.who = i;
			}
		}
		if (best < CAUSE_MIN_MS || sum < 0.25f * excess)
			c.kind = CAUSE_OUTSIDE;
		else
			c.kind = best >= 0.5f * excess ? CAUSE_PLUGIN : CAUSE_PARTIAL;
		if (c.kind == CAUSE_OUTSIDE)
			c.who = -1;
		return c;
	}

	// ---------- Jedna klatka ----------
	struct SecFrame {
		float selfMs = 0.0f, inclMs = 0.0f;
		uint32_t calls = 0;
	};
	struct OtherShare {          // inny plugin z tablicy w tej klatce
		const char* plugin = "";
		float ms = 0.0f;
		const char* top = "";    // jego najdrozsza sekcja (czas wlasny)
		float topMs = 0.0f;
	};
	struct FrameView {
		uint64_t frameNo = 0;
		float frameMs = 0.0f, pluginMs = 0.0f;
		float medianMs = 0.0f, thresholdMs = 0.0f;   // liczone tylko dla klatek dluzszych niz ProfSpikeMs
		bool medianValid = false;                    // okno mediany ma co najmniej MIN_WINDOW klatek
		int sectionCount = 0;
		SecFrame sec[MAX_SECTIONS];
		int otherCount = 0;
		OtherShare others[MAX_SLOTS];
	};

	// ---------- Statystyki (od resetu / okresowe) ----------
	struct Stats {
		uint64_t frames = 0, spikes = 0, gaps = 0;
		double sumFrameMs = 0.0, maxFrameMs = 0.0, sumPluginMs = 0.0, maxPluginMs = 0.0;
		uint32_t hist[HIST_BINS] = {};
		struct Sec {
			double sumSelf = 0.0, sumIncl = 0.0, maxSelf = 0.0;   // ms
			uint64_t calls = 0;
		} sec[MAX_SECTIONS];

		void Clear() {
			frames = spikes = gaps = 0;
			sumFrameMs = maxFrameMs = sumPluginMs = maxPluginMs = 0.0;
			std::memset(hist, 0, sizeof(hist));
			for (auto& s : sec)
				s = Sec();
		}
		static int Bin(float ms) {
			if (!(ms > 0.0f))
				return 0;
			if (ms < 100.0f) {
				int b = (int)(ms * 10.0f);
				return b < HIST_FINE ? b : HIST_FINE - 1;
			}
			if (ms < 100.0f + HIST_COARSE)
				return HIST_FINE + (int)(ms - 100.0f);
			return HIST_BINS - 1;
		}
		double BinUpper(int b) const {
			if (b < HIST_FINE)
				return (b + 1) * 0.1;
			if (b < HIST_FINE + HIST_COARSE)
				return 100.0 + (b - HIST_FINE + 1);
			return maxFrameMs;
		}
		void Add(const FrameView& f, FrameKind k) {
			if (k == FRAME_NONE)
				return;
			if (k == FRAME_GAP) {
				gaps++;
				return;
			}
			frames++;
			if (k == FRAME_SPIKE)
				spikes++;
			sumFrameMs += f.frameMs;
			if (f.frameMs > maxFrameMs)
				maxFrameMs = f.frameMs;
			hist[Bin(f.frameMs)]++;
			sumPluginMs += f.pluginMs;
			if (f.pluginMs > maxPluginMs)
				maxPluginMs = f.pluginMs;
			for (int i = 0; i < f.sectionCount && i < MAX_SECTIONS; i++) {
				const SecFrame& s = f.sec[i];
				if (!s.calls && s.selfMs == 0.0f)
					continue;
				sec[i].sumSelf += s.selfMs;
				sec[i].sumIncl += s.inclMs;
				sec[i].calls += s.calls;
				if (s.selfMs > sec[i].maxSelf)
					sec[i].maxSelf = s.selfMs;
			}
		}
		double AvgFrameMs() const {
			return frames ? sumFrameMs / frames : 0.0;
		}
		double Seconds() const {
			return sumFrameMs / 1000.0;
		}
		// Gorna granica kubla histogramu z p-tym percentylem (nie wiecej niz max).
		float Percentile(float p) const {
			if (!frames)
				return 0.0f;
			uint64_t rank = (uint64_t)std::ceil((double)p * (double)frames - 1e-9);
			if (rank < 1)
				rank = 1;
			uint64_t cum = 0;
			for (int b = 0; b < HIST_BINS; b++) {
				cum += hist[b];
				if (cum >= rank) {
					double v = BinUpper(b);
					return (float)(v < maxFrameMs ? v : maxFrameMs);
				}
			}
			return (float)maxFrameMs;
		}
	};

	// ---------- Profiler jednego pluginu (zegar podawany z zewnatrz: now = tykniecia) ----------
	class Profiler {
	public:
		Profiler() {
			std::memset(&local_, 0, sizeof(local_));
			local_.used = 1;
			CopyName(local_.plugin, "plugin");
			CopyName(name_, "plugin");
		}

		void SetName(const char* name) {
			CopyName(name_, name);
			CopyName(slot_->plugin, name);
		}
		const char* Name() const {
			return name_;
		}
		void SetTicksPerMs(double t) {
			tpm_ = t > 0.0 ? t : 1.0;
		}
		// spikeMs: dolny prog przycinki; summarySec > 0: podsumowanie okresowe co tyle sekund klatek
		void Configure(float spikeMs, float summarySec) {
			spikeMs_ = spikeMs > 1.0f ? spikeMs : 1.0f;
			summarySec_ = summarySec > 0.0f ? summarySec : 0.0f;
		}
		// koszt jednego odczytu zegara (do szacunku narzutu w podsumowaniu)
		void SetMeasureCostNs(double ns) {
			measureNs_ = ns;
		}

		// Wspolna tablica: slot dla pluginu, nazwy i liczniki sekcji zarejestrowanych wczesniej przechodza do niego.
		// false = tablica niezgodna albo pelna - profiler dziala dalej na slocie lokalnym (bez udzialu innych pluginow).
		bool Attach(Table* t) {
			if (!t || !TableAttach(*t))
				return false;
			int i = TableClaim(*t, name_);
			if (i < 0)
				return false;
			Slot& s = t->slots[i];
			s = *slot_;
			s.used = 1;
			CopyName(s.plugin, name_);
			table_ = t;
			slotIndex_ = i;
			slot_ = &s;
			for (int k = 0; k < MAX_SLOTS; k++)
				otherKnown_[k] = false;
			haveMark_ = false;
			return true;
		}
		int SlotIndex() const {
			return slotIndex_;
		}

		// Indeks sekcji o nazwie (ta sama nazwa = ta sama sekcja); po zapelnieniu "(inne)".
		int Section(const char* name) {
			int n = slot_->sectionCount;
			for (int i = 0; i < n; i++)
				if (SameName(slot_->sec[i].name, name))
					return i;
			if (n >= MAX_SECTIONS - 1) {
				if (n == MAX_SECTIONS - 1) {
					CopyName(slot_->sec[MAX_SECTIONS - 1].name, "(inne)");
					slot_->sectionCount = MAX_SECTIONS;
				}
				return MAX_SECTIONS - 1;
			}
			CopyName(slot_->sec[n].name, name);
			slot_->sectionCount = n + 1;
			return n;
		}

		void Begin(int idx, uint64_t now) {
			if (depth_ >= MAX_DEPTH || idx < 0 || idx >= MAX_SECTIONS) {
				overflow_++;
				return;
			}
			Open& o = stack_[depth_++];
			o.start = now;
			o.child = 0;
			o.idx = idx;
		}
		void End(uint64_t now) {
			if (overflow_ > 0) {
				overflow_--;
				return;
			}
			if (depth_ <= 0)
				return;
			Open& o = stack_[--depth_];
			uint64_t el = now > o.start ? now - o.start : 0;
			SlotSection& s = slot_->sec[o.idx];
			s.incl += el;
			s.self += el > o.child ? el - o.child : 0;
			s.calls++;
			if (depth_ > 0)
				stack_[depth_ - 1].child += el;
			else
				slot_->pluginTicks += el;
		}
		int Depth() const {
			return depth_;
		}

		// Koniec klatki (poczatek Game_Loop). out: linie do logu (przycinka, przerwa, podsumowanie okresowe).
		FrameKind Mark(uint64_t now, std::vector<std::string>* out) {
			slot_->frames++;
			if (!haveMark_) {
				Snapshot();
				lastMark_ = now;
				haveMark_ = true;
				return FRAME_NONE;
			}
			uint64_t ticks = now > lastMark_ ? now - lastMark_ : 0;
			lastMark_ = now;
			Collect(ticks);
			float thr = spikeMs_;
			last_.medianMs = 0.0f;
			last_.medianValid = median_.Count() >= MIN_WINDOW;
			if (last_.frameMs > spikeMs_) {
				last_.medianMs = median_.Value();
				thr = SpikeThreshold(spikeMs_, median_);
			}
			last_.thresholdMs = thr;
			FrameKind k = Classify(last_.frameMs, last_.pluginMs, thr);
			if (k != FRAME_GAP)
				median_.Push(last_.frameMs);
			total_.Add(last_, k);
			period_.Add(last_, k);
			if (out && k == FRAME_SPIKE)
				out->push_back(SpikeLine());
			else if (out && k == FRAME_GAP)
				out->push_back(GapLine());
			if (summarySec_ > 0.0f && period_.sumFrameMs >= summarySec_ * 1000.0) {
				if (out) {
					char label[48];
					std::snprintf(label, sizeof(label), "z ostatnich %.0f s", summarySec_);
					FormatSummary(period_, label, *out);
				}
				period_.Clear();
				periods_++;
			}
			return k;
		}
		// Liczba podsumowan okresowych od startu - zmiana po Mark = w tej klatce bylo podsumowanie (profiler probkujacy
		// dopisuje wtedy swoje, ZSampler.h).
		uint64_t PeriodCount() const {
			return periods_;
		}

		// Nastepny Mark zaczyna pomiar od nowa (menu, wczytanie, zapis - odstep miedzy klatkami nie jest klatka).
		void Break() {
			haveMark_ = false;
		}
		// Statystyki od nowa. Liczniki narastajace w slocie zostaja (czytaja je inne pluginy).
		void Reset() {
			total_.Clear();
			period_.Clear();
		}

		const FrameView& LastFrame() const {
			return last_;
		}
		const Stats& Total() const {
			return total_;
		}
		const Slot& Own() const {
			return *slot_;
		}

		void SummaryLines(std::vector<std::string>& out) const {
			FormatSummary(total_, "od resetu", out);
		}

		// Jedna linia do konsoli.
		std::string ShortSummary() const {
			const Stats& st = total_;
			char buf[320];
			std::snprintf(buf, sizeof(buf), "%s: %llu kl. (%.0f s), klatka sr %.1f / p99 %.1f / max %.1f ms, przycinek %llu, przerw %llu; "
				"plugin sr %.2f ms (max %.1f)", name_, (unsigned long long)st.frames, st.Seconds(), st.AvgFrameMs(),
				st.Percentile(0.99f), st.maxFrameMs, (unsigned long long)st.spikes, (unsigned long long)st.gaps,
				st.frames ? st.sumPluginMs / st.frames : 0.0, st.maxPluginMs);
			std::string s = buf;
			int best = -1;
			for (int i = 0; i < slot_->sectionCount && i < MAX_SECTIONS; i++)
				if (st.sec[i].calls && (best < 0 || st.sec[i].sumSelf > st.sec[best].sumSelf))
					best = i;
			if (best >= 0 && st.frames) {
				std::snprintf(buf, sizeof(buf), "; najdrozsza: %s %.2f ms/kl", slot_->sec[best].name, st.sec[best].sumSelf / st.frames);
				s += buf;
			}
			return s + " (tabela w logu)";
		}

	private:
		struct Open {
			uint64_t start, child;
			int idx;
		};

		double Ms(uint64_t ticks) const {
			return (double)ticks / tpm_;
		}
		static uint64_t Delta(uint64_t cur, uint64_t prev) {
			return cur > prev ? cur - prev : 0;
		}

		void SnapshotOther(int si) {
			const Slot& o = table_->slots[si];
			int n = o.sectionCount < MAX_SECTIONS ? o.sectionCount : MAX_SECTIONS;
			otherPrevPlugin_[si] = o.pluginTicks;
			for (int k = 0; k < MAX_SECTIONS; k++)
				otherPrevSelf_[si][k] = k < n ? o.sec[k].self : 0;
			otherKnown_[si] = true;
		}

		void Snapshot() {
			prevPlugin_ = slot_->pluginTicks;
			for (int i = 0; i < MAX_SECTIONS; i++) {
				prevSelf_[i] = slot_->sec[i].self;
				prevIncl_[i] = slot_->sec[i].incl;
				prevCalls_[i] = slot_->sec[i].calls;
			}
			if (table_)
				for (int si = 0; si < MAX_SLOTS; si++)
					if (si != slotIndex_ && table_->slots[si].used)
						SnapshotOther(si);
		}

		void Collect(uint64_t ticks) {
			last_.frameNo = slot_->frames;
			last_.frameMs = (float)Ms(ticks);
			uint64_t pt = slot_->pluginTicks;
			last_.pluginMs = (float)Ms(Delta(pt, prevPlugin_));
			prevPlugin_ = pt;
			int n = slot_->sectionCount < MAX_SECTIONS ? slot_->sectionCount : MAX_SECTIONS;
			last_.sectionCount = n;
			for (int i = 0; i < n; i++) {
				const SlotSection& s = slot_->sec[i];
				SecFrame& f = last_.sec[i];
				f.selfMs = (float)Ms(Delta(s.self, prevSelf_[i]));
				f.inclMs = (float)Ms(Delta(s.incl, prevIncl_[i]));
				f.calls = (uint32_t)Delta(s.calls, prevCalls_[i]);
				prevSelf_[i] = s.self;
				prevIncl_[i] = s.incl;
				prevCalls_[i] = s.calls;
			}
			last_.otherCount = 0;
			if (!table_)
				return;
			for (int si = 0; si < MAX_SLOTS; si++) {
				if (si == slotIndex_)
					continue;
				const Slot& o = table_->slots[si];
				if (!o.used) {
					otherKnown_[si] = false;
					continue;
				}
				if (!otherKnown_[si]) {   // plugin doszedl w tej klatce - jego udzial od nastepnej
					SnapshotOther(si);
					continue;
				}
				OtherShare& sh = last_.others[last_.otherCount++];
				sh.plugin = o.plugin;
				sh.ms = (float)Ms(Delta(o.pluginTicks, otherPrevPlugin_[si]));
				otherPrevPlugin_[si] = o.pluginTicks;
				int on = o.sectionCount < MAX_SECTIONS ? o.sectionCount : MAX_SECTIONS;
				int best = -1;
				uint64_t bestT = 0;
				for (int k = 0; k < on; k++) {
					uint64_t d = Delta(o.sec[k].self, otherPrevSelf_[si][k]);
					otherPrevSelf_[si][k] = o.sec[k].self;
					if (d > bestT) {
						bestT = d;
						best = k;
					}
				}
				sh.top = best >= 0 ? o.sec[best].name : "";
				sh.topMs = (float)Ms(bestT);
			}
		}

		struct TopEntry {
			int idx;
			float ms;
		};
		int TopSections(TopEntry* top, int maxN) const {
			int n = 0;
			for (int i = 0; i < last_.sectionCount; i++) {
				const SecFrame& s = last_.sec[i];
				if (!s.calls || s.selfMs < TOP_MIN_MS)
					continue;
				TopEntry e = { i, s.selfMs };
				int pos = n < maxN ? n++ : maxN;   // wstawianie z obcieciem do maxN
				while (pos > 0 && top[pos - 1].ms < e.ms) {
					if (pos < maxN)
						top[pos] = top[pos - 1];
					pos--;
				}
				if (pos < maxN)
					top[pos] = e;
			}
			return n;
		}

		std::string Verdict() const {
			float others[MAX_SLOTS];
			for (int i = 0; i < last_.otherCount; i++)
				others[i] = last_.others[i].ms;
			Cause c = Attribute(last_.frameMs, last_.medianValid ? last_.medianMs : -1.0f, last_.pluginMs, others, last_.otherCount);
			if (c.kind == CAUSE_OUTSIDE)
				return last_.otherCount ? "poza pluginami" : "poza pluginem";
			const char* best = c.who < 0 ? name_ : last_.others[c.who].plugin;
			if (c.kind == CAUSE_PARTIAL)
				return std::string("czesciowo ") + best;
			return c.who < 0 ? std::string("przyczyna: ") + name_ : std::string("przyczyna: ") + best + " (poza " + name_ + ")";
		}

		std::string SpikeLine() const {
			char buf[160], med[24];
			if (last_.medianValid)
				std::snprintf(med, sizeof(med), "%.1f", last_.medianMs);
			else
				std::snprintf(med, sizeof(med), "-");   // za malo klatek (start, po wczytaniu)
			std::snprintf(buf, sizeof(buf), "Prof: przycinka %.1f ms (kl. %llu, mediana %s, prog %.1f) | %s %.1f ms", last_.frameMs,
				(unsigned long long)last_.frameNo, med, last_.thresholdMs, name_, last_.pluginMs);
			std::string s = buf;
			TopEntry top[TOP_SECTIONS];
			int n = TopSections(top, TOP_SECTIONS);
			for (int i = 0; i < n; i++) {
				std::snprintf(buf, sizeof(buf), "%s%s %.1f ms (%ux)", i ? ", " : ": ", slot_->sec[top[i].idx].name, top[i].ms,
					last_.sec[top[i].idx].calls);
				s += buf;
			}
			float rest = last_.frameMs - last_.pluginMs;
			for (int i = 0; i < last_.otherCount; i++) {
				const OtherShare& o = last_.others[i];
				if (o.topMs >= TOP_MIN_MS && o.top[0])
					std::snprintf(buf, sizeof(buf), " | %s %.1f ms (%s)", o.plugin, o.ms, o.top);
				else
					std::snprintf(buf, sizeof(buf), " | %s %.1f ms", o.plugin, o.ms);
				s += buf;
				rest -= o.ms;
			}
			std::snprintf(buf, sizeof(buf), " | reszta gry %.1f ms - ", rest > 0.0f ? rest : 0.0f);
			return s + buf + Verdict();
		}

		std::string GapLine() const {
			char buf[200];
			std::snprintf(buf, sizeof(buf), "Prof: przerwa %.0f ms (kl. %llu, %s %.1f ms) - poza pluginem (alt-tab, okno systemowe, "
				"wczytywanie?), bez statystyk", last_.frameMs, (unsigned long long)last_.frameNo, name_, last_.pluginMs);
			return buf;
		}

		void FormatSummary(const Stats& st, const char* label, std::vector<std::string>& out) const {
			char buf[256];
			std::snprintf(buf, sizeof(buf), "Prof: podsumowanie %s %s: %llu klatek (%.1f s), klatka sr %.2f / p99 %.1f / max %.1f ms, "
				"przycinek %llu, przerw %llu", name_, label, (unsigned long long)st.frames, st.Seconds(), st.AvgFrameMs(),
				st.Percentile(0.99f), st.maxFrameMs, (unsigned long long)st.spikes, (unsigned long long)st.gaps);
			out.push_back(buf);
			if (!st.frames)
				return;
			double fr = (double)st.frames, plugin = st.sumPluginMs / fr, calls = 0.0;
			for (int i = 0; i < MAX_SECTIONS; i++)
				calls += (double)st.sec[i].calls;
			std::snprintf(buf, sizeof(buf), "Prof:   plugin sr %.3f ms/kl (%.1f%% klatki), max %.3f ms", plugin,
				st.AvgFrameMs() > 0.0 ? 100.0 * plugin / st.AvgFrameMs() : 0.0, st.maxPluginMs);
			std::string s = buf;
			if (measureNs_ > 0.0) {
				std::snprintf(buf, sizeof(buf), ", narzut pomiaru ok. %.3f ms/kl (%.0f sekcji/kl, zegar %.0f ns)",
					calls / fr * 2.0 * measureNs_ / 1e6, calls / fr, measureNs_);
				s += buf;
			}
			out.push_back(s);
			std::snprintf(buf, sizeof(buf), "Prof:   %-26s %9s %9s %8s %9s", "sekcja", "sr ms/kl", "max ms", "wyw/kl", "wl.sr ms");
			out.push_back(buf);
			std::vector<int> order;
			for (int i = 0; i < slot_->sectionCount && i < MAX_SECTIONS; i++)
				if (st.sec[i].calls)
					order.push_back(i);
			std::stable_sort(order.begin(), order.end(), [&st](int a, int b) { return st.sec[a].sumSelf > st.sec[b].sumSelf; });
			for (int i : order) {
				std::snprintf(buf, sizeof(buf), "Prof:   %-26s %9.3f %9.3f %8.2f %9.3f", slot_->sec[i].name, st.sec[i].sumSelf / fr,
					st.sec[i].maxSelf, (double)st.sec[i].calls / fr, st.sec[i].sumIncl / fr);
				out.push_back(buf);
			}
		}

		char name_[NAME_LEN];
		Slot local_;
		Slot* slot_ = &local_;
		Table* table_ = nullptr;
		int slotIndex_ = -1;
		double tpm_ = 1.0;
		float spikeMs_ = 25.0f, summarySec_ = 0.0f;
		double measureNs_ = 0.0;
		Open stack_[MAX_DEPTH] = {};
		int depth_ = 0, overflow_ = 0;
		bool haveMark_ = false;
		uint64_t lastMark_ = 0, periods_ = 0;
		uint64_t prevPlugin_ = 0, prevSelf_[MAX_SECTIONS] = {}, prevIncl_[MAX_SECTIONS] = {}, prevCalls_[MAX_SECTIONS] = {};
		bool otherKnown_[MAX_SLOTS] = {};
		uint64_t otherPrevPlugin_[MAX_SLOTS] = {}, otherPrevSelf_[MAX_SLOTS][MAX_SECTIONS] = {};
		Median median_;
		Stats total_, period_;
		FrameView last_;
	};
}
