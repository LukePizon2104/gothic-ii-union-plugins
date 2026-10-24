// Testy czystej logiki zQuickSign (zQuickSignSelect.h) - bez gry i bez Windows.
// Kompilacja (vcvars32): cl /nologo /EHsc /W3 tests_zqs.cpp /Fo:<katalog>\ /Fe:<katalog>\tests_zqs.exe
// Kod wyjscia 0 = wszystkie testy przeszly.

#include <stdio.h>
#include <string.h>
#include "zQuickSignSelect.h"
#include "zQuickSignNb.h"
#include <stdlib.h>

static int failures, checks;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("BLAD %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { checks++; long long va_ = (long long)(a), vb_ = (long long)(b); if (va_ != vb_) { failures++; \
	printf("BLAD %s:%d: %s == %lld, oczekiwano %lld\n", __FILE__, __LINE__, #a, va_, vb_); } } while (0)
#define CHECK_STR(a, b) do { checks++; if (strcmp((a), (b)) != 0) { failures++; \
	printf("BLAD %s:%d: \"%s\", oczekiwano \"%s\"\n", __FILE__, __LINE__, (a), (b)); } } while (0)

static void TestDikToKeyNo() {
	// DIK_4 (0x05) .. DIK_9 (0x0A), DIK_0 (0x0B) -> keyNo ksiegi 1..7
	CHECK_EQ(qs::DikToKeyNo(0x05), 1);
	CHECK_EQ(qs::DikToKeyNo(0x06), 2);
	CHECK_EQ(qs::DikToKeyNo(0x0A), 6);
	CHECK_EQ(qs::DikToKeyNo(0x0B), 7);
	// 1..3 (bron, luk, ...), minus, litery, F, numeryczna - nie
	CHECK_EQ(qs::DikToKeyNo(0x02), 0);
	CHECK_EQ(qs::DikToKeyNo(0x04), 0);
	CHECK_EQ(qs::DikToKeyNo(0x0C), 0);
	CHECK_EQ(qs::DikToKeyNo(0x21), 0);
	CHECK_EQ(qs::DikToKeyNo(0x4B), 0);
	CHECK_EQ(qs::DikToKeyNo(0), 0);
	CHECK_EQ(qs::DikToKeyNo(-1), 0);
}

static void TestKeyNoToSlot() {
	// slot jak na kregu magii: 4..10 (klawisz 0 = 10)
	CHECK_EQ(qs::KeyNoToSlot(1), 4);
	CHECK_EQ(qs::KeyNoToSlot(6), 9);
	CHECK_EQ(qs::KeyNoToSlot(7), 10);
	CHECK_EQ(qs::KeyNoToSlot(0), 0);
	CHECK_EQ(qs::KeyNoToSlot(8), 0);
	CHECK_EQ(qs::KeyNoToSlot(-3), 0);
	// klawisz -> slot w jednym kroku
	CHECK_EQ(qs::KeyNoToSlot(qs::DikToKeyNo(0x05)), 4);
	CHECK_EQ(qs::KeyNoToSlot(qs::DikToKeyNo(0x0B)), 10);
}

static void TestInList() {
	int ids[] = { 90, 91, 92, 93, 94 };
	CHECK(qs::InList(90, ids, 5));
	CHECK(qs::InList(94, ids, 5));
	CHECK(!qs::InList(95, ids, 5));
	CHECK(!qs::InList(90, ids, 0));
	CHECK(!qs::InList(0, 0, 0));
}

static void TestWheelDir() {
	CHECK_EQ(qs::WheelDir(120.0f), -1);   // kolko w gore = poprzedni
	CHECK_EQ(qs::WheelDir(-120.0f), 1);   // w dol = nastepny
	CHECK_EQ(qs::WheelDir(0.0f), 0);
}

static void TestWheelPick() {
	// ksiega w kolejnosci dodania (indeksy), sloty nie po kolei: Igni w 6, ognista strzala w 4, Aard w 5, Quen w 9, sen w 7
	int  keyNo[]  = { 3, 1, 2, 6, 4 };
	bool isSign[] = { true, false, true, true, false };
	// kolejnosc znakow po slotach: Aard (idx 2, keyNo 2) -> Igni (idx 0, keyNo 3) -> Quen (idx 3, keyNo 6)
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 2, +1), 0);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 0, +1), 3);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 3, +1), 2);    // zawijanie
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 2, -1), 3);    // zawijanie wstecz
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 3, -1), 0);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 0, -1), 2);
	// wybrana zwykla runa (ognista strzala, keyNo 1): nastepny znak po jej slocie, poprzedni - zawijanie do ostatniego
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 1, +1), 2);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 1, -1), 3);
	// sen (keyNo 4) miedzy Igni (3) a Quen (6)
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 4, +1), 3);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 4, -1), 0);
	// brak wybranego czaru (-1 albo poza zakresem): pierwszy / ostatni znak
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, -1, +1), 2);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, -1, -1), 3);
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 99, +1), 2);
	// dir 0 = bez zmiany
	CHECK_EQ(qs::WheelPick(keyNo, isSign, 5, 0, 0), -1);
}

static void TestWheelPickEdge() {
	int  keyNo[]  = { 1, 2, 3 };
	bool none[]   = { false, false, false };
	bool one[]    = { false, true, false };
	CHECK_EQ(qs::WheelPick(keyNo, none, 3, 0, +1), -1);    // brak znakow w ksiedze
	CHECK_EQ(qs::WheelPick(keyNo, none, 0, -1, +1), -1);   // pusta ksiega
	CHECK_EQ(qs::WheelPick(keyNo, one, 3, 1, +1), 1);      // jedyny znak - zostaje on sam
	CHECK_EQ(qs::WheelPick(keyNo, one, 3, 1, -1), 1);
	CHECK_EQ(qs::WheelPick(keyNo, one, 3, 2, +1), 1);
	// czary bez klawisza (keyNo 0 - ponad 7 run) na koncu kolejnosci, miedzy soba po indeksie
	int  keyNo2[]  = { 0, 2, 0, 5 };
	bool isSign2[] = { true, true, true, true };
	CHECK_EQ(qs::WheelPick(keyNo2, isSign2, 4, 1, +1), 3);
	CHECK_EQ(qs::WheelPick(keyNo2, isSign2, 4, 3, +1), 0);
	CHECK_EQ(qs::WheelPick(keyNo2, isSign2, 4, 0, +1), 2);
	CHECK_EQ(qs::WheelPick(keyNo2, isSign2, 4, 2, +1), 1);  // zawijanie
	CHECK_EQ(qs::WheelPick(keyNo2, isSign2, 4, 1, -1), 2);
	// dwa czary w tym samym slocie (nie powinno sie zdarzyc) - kolejnosc po indeksie, bez petli w miejscu
	int  keyNo3[]  = { 4, 4 };
	bool isSign3[] = { true, true };
	CHECK_EQ(qs::WheelPick(keyNo3, isSign3, 2, 0, +1), 1);
	CHECK_EQ(qs::WheelPick(keyNo3, isSign3, 2, 1, +1), 0);
	CHECK_EQ(qs::WheelPick(keyNo3, isSign3, 2, 0, -1), 1);
}

static void TestDigitEdge() {
	bool prev[qs::KEYNO_MAX + 1] = {};
	bool now[qs::KEYNO_MAX + 1] = {};
	// bez Shift: wcisniecie zapamietane, nic nie wybiera
	now[2] = true;
	CHECK_EQ(qs::DigitEdge(prev, now, false), 0);
	CHECK(prev[2]);
	// Shift wcisniety pozniej przy trzymanej cyfrze - to nie nowe wcisniecie cyfry
	CHECK_EQ(qs::DigitEdge(prev, now, true), 0);
	// puszczenie i wcisniecie z Shift
	now[2] = false;
	CHECK_EQ(qs::DigitEdge(prev, now, true), 0);
	now[2] = true;
	CHECK_EQ(qs::DigitEdge(prev, now, true), 2);
	// trzymana dalej - tylko raz
	CHECK_EQ(qs::DigitEdge(prev, now, true), 0);
	// dwie nowe naraz - najnizsza, druga przepada (nie wybiera dwa razy w klatce)
	now[2] = false;
	qs::DigitEdge(prev, now, true);
	now[5] = true;
	now[7] = true;
	CHECK_EQ(qs::DigitEdge(prev, now, true), 5);
	CHECK(prev[5] && prev[7]);
	CHECK_EQ(qs::DigitEdge(prev, now, true), 0);
	// klawisz 0 (keyNo 7)
	now[5] = now[7] = false;
	qs::DigitEdge(prev, now, true);
	now[7] = true;
	CHECK_EQ(qs::DigitEdge(prev, now, true), 7);
}

static void TestFormatSignText() {
	char buf[64];
	qs::FormatSignText(buf, sizeof(buf), "Ognista strza\xB3" "a");
	CHECK_STR(buf, "Znak: Ognista strza\xB3" "a");
	// runy znakow W3 maja juz "Znak ..." w opisie - bez podwojenia
	qs::FormatSignText(buf, sizeof(buf), "Znak Aard");
	CHECK_STR(buf, "Znak Aard");
	qs::FormatSignText(buf, sizeof(buf), "Znak");
	CHECK_STR(buf, "Znak");
	// "Znakowanie" to nie "Znak ..."
	qs::FormatSignText(buf, sizeof(buf), "Znakowanie");
	CHECK_STR(buf, "Znak: Znakowanie");
	qs::FormatSignText(buf, sizeof(buf), "");
	CHECK_STR(buf, "Znak: ?");
	qs::FormatSignText(buf, sizeof(buf), 0);
	CHECK_STR(buf, "Znak: ?");
	// obciecie do rozmiaru bufora (z zerem na koncu)
	char small[10];
	qs::FormatSignText(small, sizeof(small), "Lodowa strza\xB3" "a");
	CHECK_STR(small, "Znak: Lod");
	char tiny[1] = { 'x' };
	qs::FormatSignText(tiny, sizeof(tiny), "Aard");
	CHECK_EQ(tiny[0], 0);
}

// --- sygnatury (nbSigScan.h, zQuickSignNb.h) ---

static void TestWantsMagicFocus() {
	CHECK(qs::WantsMagicFocus(4, 30));     // Axii (FOCUS_FALLBACK_NONE, kat 30) - fokus magii
	CHECK(qs::WantsMagicFocus(2, 45));     // zwykla runa celujaca
	CHECK(qs::WantsMagicFocus(5, 10));
	CHECK(!qs::WantsMagicFocus(4, 0));     // Aard / Igni / Yrden - kierunek z celownika, cele liczy zDismember
	CHECK(!qs::WantsMagicFocus(1, 30));    // TARGET_COLLECT_CASTER (Quen, leczenie)
	CHECK(!qs::WantsMagicFocus(0, 30));
	// Znak zDismember (ZDISM_SignHoldState = 1): cel Axii wybiera zDismember (stozek z celownika, zasieg AxiiRange,
	// linia wzroku) - bez szukania fokusu magii; brak fokusu nie blokuje rzutu
	CHECK(!qs::WantsMagicFocus(4, 20, true));
	CHECK(qs::WantsMagicFocus(4, 20, false));    // bez zDismember - jak dotad
	CHECK(qs::WantsMagicFocus(2, 45, false));
}

static void TestSigParse() {
	sigscan::Pattern p;
	CHECK(sigscan::Parse("55 8B ?? EC", &p));
	CHECK_EQ(p.len, 4);
	CHECK_EQ(p.bytes[1], 0x8B);
	CHECK_EQ(p.mask[2], 0);
	CHECK_EQ(p.mask[3], 1);
	CHECK(sigscan::Parse("  e9 ? ff  ", &p));     // male litery, pojedynczy "?", nadmiarowe spacje
	CHECK_EQ(p.len, 3);
	CHECK_EQ(p.bytes[2], 0xFF);
	CHECK(!sigscan::Parse("?? 55", &p));          // pierwszy bajt dowolny - odrzucone
	CHECK(!sigscan::Parse("5", &p));
	CHECK(!sigscan::Parse("55 8G", &p));
	CHECK(!sigscan::Parse("558B", &p));           // bez spacji
	CHECK(!sigscan::Parse("", &p));
	CHECK(!sigscan::Parse(0, &p));
	CHECK(sigscan::Parse(qs::NB_SIG_STREAM_FN, &p));
	CHECK(sigscan::Parse(qs::NB_SIG_FIRESTREAM_FN, &p));
}

static void TestSigFind() {
	const unsigned char d[] = { 0x55, 0x8B, 0xEC, 0x90, 0x55, 0x8B, 0x00, 0x90, 0x55, 0x8B };
	size_t offs[4];
	CHECK_EQ(sigscan::FindAll(d, sizeof(d), "55 8B ?? 90", offs, 4), 2);
	CHECK_EQ(offs[0], 0);
	CHECK_EQ(offs[1], 4);
	CHECK_EQ(sigscan::FindAll(d, sizeof(d), "55 8B EC", offs, 4), 1);
	CHECK_EQ(sigscan::FindAll(d, sizeof(d), "55 8B", offs, 1), 3);    // liczy wszystkie, zapisuje max 1
	CHECK_EQ(offs[0], 0);
	CHECK_EQ(sigscan::FindAll(d, sizeof(d), "8B 00 90 55 8B ??", offs, 4), 0);   // pod 5 brakuje ostatniego bajtu - nie wychodzi poza bufor
	CHECK_EQ(sigscan::FindAll(d, 3, "55 8B EC 90", offs, 4), 0);
	CHECK_EQ(sigscan::FindAll(d, sizeof(d), "?? 8B", offs, 4), -1);
}

static void TestRel32Refs() {
	unsigned char d[32] = {};
	// pod 0x1000 (offset 0) call do 0x1010, pod 0x1008 jmp do 0x1010, pod 0x1010 jmp wstecz do 0x1000
	int rel;
	d[0] = 0xE8; rel = 0x1010 - 0x1005; memcpy(d + 1, &rel, 4);
	d[8] = 0xE9; rel = 0x1010 - 0x100D; memcpy(d + 9, &rel, 4);
	d[16] = 0xE9; rel = 0x1000 - 0x1015; memcpy(d + 17, &rel, 4);
	size_t offs[4];
	CHECK_EQ(sigscan::FindRel32Refs(d, sizeof(d), 0x1000, 0x1010, 0xE8, offs, 4), 1);
	CHECK_EQ(offs[0], 0);
	CHECK_EQ(sigscan::FindRel32Refs(d, sizeof(d), 0x1000, 0x1010, 0xE9, offs, 4), 1);
	CHECK_EQ(offs[0], 8);
	CHECK_EQ(sigscan::FindRel32Refs(d, sizeof(d), 0x1000, 0x1000, 0xE9, offs, 4), 1);
	CHECK_EQ(offs[0], 16);
	CHECK_EQ(sigscan::FindRel32Refs(d, sizeof(d), 0x1000, 0x1234, 0xE8, offs, 4), 0);
	CHECK_EQ(sigscan::FindRel32Refs(d, 4, 0x1000, 0x1010, 0xE8, offs, 4), 0);      // ucieta instrukcja
}

// Minimalny obraz PE w pamieci: naglowek + sekcje pod VirtualAddress.
static void PutU32(unsigned char* p, unsigned v) { memcpy(p, &v, 4); }
static void PutSection(unsigned char* img, int i, const char* name, unsigned rva, unsigned vsize, unsigned flags) {
	unsigned char* sec = img + 0x80 + 24 + 0xE0 + i * 40;
	memcpy(sec, name, strlen(name));
	PutU32(sec + 8, vsize);
	PutU32(sec + 12, rva);
	PutU32(sec + 36, flags);
}
static void MakeImage(unsigned char* img, int nSec) {
	img[0] = 'M'; img[1] = 'Z';
	PutU32(img + 0x3C, 0x80);
	memcpy(img + 0x80, "PE\0\0", 4);
	img[0x80 + 6] = (unsigned char)nSec;
	img[0x80 + 20] = 0xE0;
}

static void TestExecSections() {
	static unsigned char img[0x4000];
	memset(img, 0, sizeof(img));
	MakeImage(img, 3);
	PutSection(img, 0, ".text", 0x1000, 0x800, 0x60000020);
	PutSection(img, 1, ".rdata", 0x2000, 0x100, 0x40000040);
	PutSection(img, 2, ".text2", 0x3000, 0x200, 0x60000020);
	sigscan::CodeRange r[4];
	CHECK_EQ(sigscan::ExecSections(img, 0x10000000, r, 4), 2);
	CHECK(r[0].data == img + 0x1000);
	CHECK_EQ(r[0].size, 0x800);
	CHECK_EQ(r[0].va, 0x10001000);
	CHECK_EQ(r[1].va, 0x10003000);
	CHECK_EQ(sigscan::ExecSections(img, 0x10000000, r, 1), 1);
	img[0] = 'X';
	CHECK_EQ(sigscan::ExecSections(img, 0x10000000, r, 4), 0);
	CHECK_EQ(sigscan::ExecSections(0, 0, r, 4), 0);
}

// Wpisuje bajty sygnatury (dowolne jako 0xCC) pod p, -> dlugosc.
static int PutPattern(unsigned char* p, const char* text) {
	sigscan::Pattern pat;
	if (!sigscan::Parse(text, &pat))
		return 0;
	for (int i = 0; i < pat.len; i++)
		p[i] = pat.mask[i] ? pat.bytes[i] : 0xCC;
	return pat.len;
}
static void PutRel(unsigned char* code, unsigned va, int at, unsigned char op, unsigned target) {
	code[at] = op;
	int rel = (int)(target - (va + at + 5));
	memcpy(code + at + 1, &rel, 4);
}

static void TestNbStreamSitesSynthetic() {
	static unsigned char code[0x2000];
	const unsigned va = 0x20001000;
	memset(code, 0x90, sizeof(code));
	PutPattern(code + 0x400, qs::NB_SIG_STREAM_FN);
	PutPattern(code + 0x800, qs::NB_SIG_FIRESTREAM_FN);
	PutRel(code, va, 0x100, 0xE9, va + 0x400);
	PutRel(code, va, 0x200, 0xE8, va + 0x800);
	sigscan::CodeRange r = { code, sizeof(code), va };
	qs::NbStreamSites s;
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_OK);
	CHECK_EQ(s.streamFn, va + 0x400);
	CHECK_EQ(s.streamSite, va + 0x100);
	CHECK_EQ(s.fireFn, va + 0x800);
	CHECK_EQ(s.fireSite, va + 0x200);
	// drugie wejscie do funkcji strumieni (call) - opakowanie by go nie objelo -> odmowa
	PutRel(code, va, 0x300, 0xE8, va + 0x400);
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_STREAM_REFS);
	CHECK_EQ(s.streamCall, 1);
	memset(code + 0x300, 0x90, 5);
	// funkcja ognia wolana jmp zamiast call -> odmowa
	PutRel(code, va, 0x200, 0xE9, va + 0x800);
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_FIRE_REFS);
	PutRel(code, va, 0x200, 0xE8, va + 0x800);
	// druga kopia funkcji strumieni -> niejednoznaczne
	PutPattern(code + 0xC00, qs::NB_SIG_STREAM_FN);
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_STREAM_FN);
	CHECK_EQ(s.streamFnHits, 2);
	memset(code + 0xC00, 0x90, 0x100);
	// brak funkcji ognia
	code[0x800] = 0x90;
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_FIRE_FN);
	code[0x800] = 0x55;
	CHECK_EQ(qs::FindNbStreamSites(&r, 1, &s), qs::NBF_OK);
	// pusto (np. DLL innego moda pod ta nazwa)
	CHECK_EQ(qs::FindNbStreamSites(&r, 0, &s), qs::NBF_STREAM_FN);
}

// Plik PE -> obraz jak po zaladowaniu (sekcje pod VirtualAddress), baza z naglowka (bez relokacji).
static unsigned char* MapPeFile(const char* path, unsigned* imageBase) {
	FILE* f = 0;
	if (fopen_s(&f, path, "rb") || !f)
		return 0;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char* file = (unsigned char*)malloc(size);
	size_t got = fread(file, 1, size, f);
	fclose(f);
	if ((long)got != size || size < 0x400 || file[0] != 'M') {
		free(file);
		return 0;
	}
	unsigned nt = sigscan::ReadU32(file + 0x3C);
	unsigned optSize = file[nt + 20] | (file[nt + 21] << 8);
	unsigned nSec = file[nt + 6] | (file[nt + 7] << 8);
	*imageBase = sigscan::ReadU32(file + nt + 24 + 28);
	unsigned sizeOfImage = sigscan::ReadU32(file + nt + 24 + 56);
	unsigned sizeOfHeaders = sigscan::ReadU32(file + nt + 24 + 60);
	unsigned char* img = (unsigned char*)calloc(1, sizeOfImage);
	memcpy(img, file, sizeOfHeaders);
	const unsigned char* sec = file + nt + 24 + optSize;
	for (unsigned i = 0; i < nSec; i++, sec += 40) {
		unsigned vsize = sigscan::ReadU32(sec + 8), rva = sigscan::ReadU32(sec + 12);
		unsigned raw = sigscan::ReadU32(sec + 16), ptr = sigscan::ReadU32(sec + 20);
		unsigned n = raw < vsize ? raw : vsize;
		if (rva + n <= sizeOfImage && ptr + n <= (unsigned)size)
			memcpy(img + rva, file + ptr, n);
	}
	free(file);
	return img;
}

static void TestNbStreamSitesDll(const char* path, unsigned site, unsigned fn, unsigned fireSite, unsigned fireFn) {
	unsigned base = 0;
	unsigned char* img = MapPeFile(path, &base);
	CHECK(img != 0);
	if (!img) {
		printf("brak pliku %s\n", path);
		return;
	}
	sigscan::CodeRange r[8];
	int n = sigscan::ExecSections(img, base, r, 8);
	CHECK(n >= 1);
	qs::NbStreamSites s;
	int res = qs::FindNbStreamSites(r, n, &s);
	printf("%s: %s, jmp %08X -> %08X, call %08X -> %08X\n", path, qs::NbFindResultText(res), s.streamSite, s.streamFn,
		s.fireSite, s.fireFn);
	CHECK_EQ(res, qs::NBF_OK);
	CHECK_EQ(s.streamSite, site);
	CHECK_EQ(s.streamFn, fn);
	CHECK_EQ(s.fireSite, fireSite);
	CHECK_EQ(s.fireFn, fireFn);
	// w prawdziwym DLL pod znalezionymi adresami musza stac jmp / call
	CHECK_EQ(img[s.streamSite - base], 0xE9);
	CHECK_EQ(img[s.fireSite - base], 0xE8);
	free(img);
}

static void TestSignWeaponClass() {
	// gest alternatywny wg postawy: bez broni / piesci / luk / kusza / runa = 0, sztylet i 1H = 1 (W3 rp), 2H = 2 (W3 lp)
	CHECK_EQ(qs::SignWeaponClass(0), 0);
	CHECK_EQ(qs::SignWeaponClass(1), 0);
	CHECK_EQ(qs::SignWeaponClass(2), 1);
	CHECK_EQ(qs::SignWeaponClass(3), 1);
	CHECK_EQ(qs::SignWeaponClass(4), 2);
	CHECK_EQ(qs::SignWeaponClass(5), 0);
	CHECK_EQ(qs::SignWeaponClass(6), 0);
	CHECK_EQ(qs::SignWeaponClass(7), 0);
	CHECK_EQ(qs::SignWeaponClass(-1), 0);
}

static void TestGestureTimeLeft() {
	// gest alternatywny 440 ms do ziemi, 1120 ms calosci: tuz po starcie (zDismember pyta w haku Cast) - pelne czasy
	int c = -1, l = -1;
	CHECK(qs::GestureTimeLeft(440, 1120, 1000, 1000, &c, &l));
	CHECK_EQ(c, 440);
	CHECK_EQ(l, 1120);
	CHECK(qs::GestureTimeLeft(440, 1120, 1000, 1100, &c, &l));
	CHECK_EQ(c, 340);
	CHECK_EQ(l, 1020);
	// po dotknieciu ziemi - 0 do skutku, gest trwa
	CHECK(qs::GestureTimeLeft(440, 1120, 1000, 1500, &c, &l));
	CHECK_EQ(c, 0);
	CHECK_EQ(l, 620);
	// koniec gestu
	CHECK(!qs::GestureTimeLeft(440, 1120, 1000, 2120, &c, &l));
	CHECK_EQ(l, 0);
	CHECK(!qs::GestureTimeLeft(320, 0, 1000, 1000, &c, &l));
	// przekrecenie licznika GetTickCount
	CHECK(qs::GestureTimeLeft(440, 1120, 0xFFFFFF00u, 0x00000010u, &c, &l));
	CHECK_EQ(c, 440 - 0x110);
	// zegar "przed" startem (ta sama klatka, inny odczyt) - jak start
	CHECK(qs::GestureTimeLeft(440, 1120, 1000, 990, &c, &l));
	CHECK_EQ(c, 440);
	CHECK(qs::GestureTimeLeft(440, 1120, 1000, 1000, 0, 0));
}

static void TestMoveSwitchFrame() {
	// gest zwykly: ta sama klatka (warianty postoju i ruchu maja te same klatki)
	CHECK(qs::MoveSwitchFrame(false, 5.5f, 320) == 5.5f);
	// gest alternatywny bez blokady ruchu: wariant ruchu gestu zwyklego od jego pchniecia (320 ms = kl. 8)
	CHECK(qs::MoveSwitchFrame(true, 20.0f, 320) == 8.0f);
	CHECK(qs::MoveSwitchFrame(true, 2.0f, 200) == 5.0f);
	CHECK(qs::MoveSwitchFrame(false, -1.0f, 320) == 0.0f);
}

static void TestSyncDelayAndVerdict() {
	// skutek z koncem pchniecia dloni (SignCastSyncMs z animacji), bez wartosci - chwila pchniecia gestu
	CHECK_EQ(qs::SyncDelayMs(360, 320), 360);
	CHECK_EQ(qs::SyncDelayMs(0, 320), 320);
	CHECK_EQ(qs::SyncDelayMs(0, -1), 0);
	// przerwanie jak w W3: zmiana runy, trafienie, ekwipunek, spoznienie (bohater zablokowany: unik, finisher)
	CHECK_EQ(qs::SyncVerdict(true, false, false, -1, 300), qs::QSYNC_OK);
	CHECK_EQ(qs::SyncVerdict(true, false, false, 16, 300), qs::QSYNC_OK);      // zwykla klatka po czasie
	CHECK_EQ(qs::SyncVerdict(true, false, false, 300, 300), qs::QSYNC_OK);     // na granicy - jeszcze rzut
	CHECK_EQ(qs::SyncVerdict(true, false, false, 301, 300), qs::QSYNC_LATE);
	CHECK_EQ(qs::SyncVerdict(true, false, false, 900, 300), qs::QSYNC_LATE);   // unik 0,88 s w trakcie gestu
	CHECK_EQ(qs::SyncVerdict(true, true, false, -1, 300), qs::QSYNC_HIT);
	CHECK_EQ(qs::SyncVerdict(true, false, true, -1, 300), qs::QSYNC_INVENTORY);
	CHECK_EQ(qs::SyncVerdict(false, true, true, 900, 300), qs::QSYNC_SPELL);   // zmiana runy pierwsza
	CHECK_EQ(qs::SyncVerdict(true, true, true, 900, 300), qs::QSYNC_HIT);
}

static void TestWheelPickXbow() {
	// ksiega: Aard (slot 4), runa ognia (5), Igni (6) + kusza za ostatnim znakiem
	int keyNo[] = { 1, 2, 3 };
	bool isSign[] = { true, false, true };
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 0, +1, true), 2);   // Aard -> Igni
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 2, +1, true), 3);   // Igni -> kusza
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 3, +1, true), 0);   // kusza -> Aard (zawijanie)
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 3, -1, true), 2);   // kusza -> Igni
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 0, -1, true), 3);   // Aard -> kusza (wstecz)
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 1, +1, true), 2);   // zwykla runa -> nastepny znak
	// bez kuszy jak dotad; wybrana, ale niedostepna kusza = brak wybranego czaru
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 2, +1, false), 0);
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 3, +1, false), 0);
	// ksiega bez znakow: tylko kusza
	bool none[] = { false, false, false };
	CHECK_EQ(qs::WheelPickXbow(keyNo, none, 3, 1, +1, true), 3);
	CHECK_EQ(qs::WheelPickXbow(keyNo, none, 3, 3, -1, true), 3);
	CHECK_EQ(qs::WheelPickXbow(keyNo, none, 3, 1, +1, false), -1);
	// pusta ksiega i czary bez klawisza (keyNo 0): kusza dalej na koncu
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 0, -1, +1, true), 0);
	int k0[] = { 0, 1 };
	bool s0[] = { true, true };
	CHECK_EQ(qs::WheelPickXbow(k0, s0, 2, 0, +1, true), 2);          // znak bez klawisza -> kusza
	CHECK_EQ(qs::WheelPickXbow(k0, s0, 2, 2, +1, true), 1);          // kusza -> znak w slocie 4
	CHECK_EQ(qs::WheelPickXbow(keyNo, isSign, 3, 0, 0, true), -1);
}

static void TestFormatXbowText() {
	char buf[64];
	qs::FormatXbowText(buf, sizeof(buf), "Lekka kusza", 12);
	CHECK_STR(buf, "Kusza: Lekka kusza (be\xB3ty: 12)");
	qs::FormatXbowText(buf, sizeof(buf), "", 0);
	CHECK_STR(buf, "Kusza: ? (be\xB3ty: 0)");
	qs::FormatXbowText(buf, sizeof(buf), "Magiczna", 999);
	CHECK_STR(buf, "Kusza: Magiczna (be\xB3ty: bez limitu)");
	qs::FormatXbowText(buf, 10, "Lekka kusza", 5);
	CHECK_STR(buf, "Kusza: Le");
	CHECK(qs::XbowHoldReached(1000, 1300, 300));
	CHECK(!qs::XbowHoldReached(1000, 1299, 300));
	CHECK(qs::XbowHoldReached(0xFFFFFF00u, 0x0000003Cu, 300));   // przejscie licznika GetTickCount
}

int main(int argc, char** argv) {
	TestWheelPickXbow();
	TestFormatXbowText();
	TestSyncDelayAndVerdict();
	TestSignWeaponClass();
	TestGestureTimeLeft();
	TestMoveSwitchFrame();
	TestDikToKeyNo();
	TestKeyNoToSlot();
	TestInList();
	TestWheelDir();
	TestWheelPick();
	TestWheelPickEdge();
	TestDigitEdge();
	TestFormatSignText();
	TestWantsMagicFocus();
	TestSigParse();
	TestSigFind();
	TestRel32Refs();
	TestExecSections();
	TestNbStreamSitesSynthetic();
	// prawdziwe UNION_ABI.DLL NB: tests_zqs.exe <stary.dll> <nowy.dll> [nowy_latany.dll] - adresy przy bazie z naglowka
	if (argc > 1)
		TestNbStreamSitesDll(argv[1], 0x103B3B43, 0x103B3ED0, 0x1023A6DD, 0x103B9190);
	for (int i = 2; i < argc; i++)
		TestNbStreamSitesDll(argv[i], 0x103C4143, 0x103C44E0, 0x1024986D, 0x103C9830);
	printf("%d sprawdzen, %d bledow\n", checks, failures);
	return failures ? 1 : 0;
}
