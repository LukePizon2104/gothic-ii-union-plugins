// nbSigScan.h - wyszukiwanie miejsc w kodzie po sygnaturach bajtowych (z "??" jako dowolny bajt) i odwolan rel32
// (call/jmp) do funkcji - zamiast sztywnych adresow, ktore przesuwaja sie przy kazdej aktualizacji UNION_ABI.DLL
// New Balance. Czysta logika na buforze (bez Windows) - testy: zQuickSign\tests_zqs.cpp (tez na prawdziwych DLL NB).
// Ta sama kopia w zQuickSign i zNBTweaks (kazdy plugin buduje sie sam, bez wspolnych plikow) - zmieniac obie.
#pragma once

#include <stddef.h>
#include <string.h>

namespace sigscan {

typedef unsigned char  u8;
typedef unsigned int   u32;

enum { MAX_PATTERN = 256 };

struct Pattern {
	u8  bytes[MAX_PATTERN];
	u8  mask[MAX_PATTERN];   // 1 = bajt musi sie zgadzac, 0 = dowolny ("??")
	int len;                 // 0 = blad skladni (albo pusta)
};

inline int HexDigit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// "55 8B EC ?? 6A" -> Pattern. Tokeny oddzielone spacjami: dwie cyfry szesnastkowe albo "??" (tez "?").
// Pierwszy bajt nie moze byc dowolny (przyspiesza szukanie i chroni przed pomylka). Blad -> len 0.
inline bool Parse(const char* text, Pattern* p) {
	p->len = 0;
	if (!text)
		return false;
	const char* s = text;
	while (*s) {
		while (*s == ' ')
			s++;
		if (!*s)
			break;
		if (p->len >= MAX_PATTERN) {
			p->len = 0;
			return false;
		}
		if (s[0] == '?') {
			s += s[1] == '?' ? 2 : 1;
			p->bytes[p->len] = 0;
			p->mask[p->len] = 0;
		}
		else {
			int hi = HexDigit(s[0]), lo = hi >= 0 ? HexDigit(s[1]) : -1;
			if (hi < 0 || lo < 0) {
				p->len = 0;
				return false;
			}
			s += 2;
			p->bytes[p->len] = (u8)(hi * 16 + lo);
			p->mask[p->len] = 1;
		}
		if (*s && *s != ' ') {
			p->len = 0;
			return false;
		}
		p->len++;
	}
	if (p->len && !p->mask[0])
		p->len = 0;
	return p->len > 0;
}

// Wszystkie wystapienia w data[0..size). Do out (max maxOut) offsety; zwraca liczbe WSZYSTKICH (takze ponad maxOut),
// -1 = bledna sygnatura.
inline int FindAll(const u8* data, size_t size, const Pattern& p, size_t* out, int maxOut) {
	if (p.len <= 0)
		return -1;
	if (!data || size < (size_t)p.len)
		return 0;
	int count = 0;
	u8 first = p.bytes[0];
	size_t last = size - (size_t)p.len;
	for (size_t i = 0; i <= last; i++) {
		if (data[i] != first)
			continue;
		int k = 1;
		while (k < p.len && (!p.mask[k] || data[i + k] == p.bytes[k]))
			k++;
		if (k == p.len) {
			if (count < maxOut && out)
				out[count] = i;
			count++;
		}
	}
	return count;
}

inline int FindAll(const u8* data, size_t size, const char* pattern, size_t* out, int maxOut) {
	Pattern p;
	if (!Parse(pattern, &p))
		return -1;
	return FindAll(data, size, p, out, maxOut);
}

// Instrukcje opcode rel32 (0xE8 call, 0xE9 jmp) w data, ktorych cel == target. base = adres data[0] w pamieci.
// Zwraca liczbe wszystkich, offsety do out (max maxOut).
inline int FindRel32Refs(const u8* data, size_t size, u32 base, u32 target, u8 opcode, size_t* out, int maxOut) {
	if (!data || size < 5)
		return 0;
	int count = 0;
	for (size_t i = 0; i + 5 <= size; i++) {
		if (data[i] != opcode)
			continue;
		int rel;
		memcpy(&rel, data + i + 1, 4);
		if ((u32)(base + (u32)i + 5u + (u32)rel) == target) {
			if (count < maxOut && out)
				out[count] = i;
			count++;
		}
	}
	return count;
}

inline u32 ReadU32(const u8* p) {
	u32 v;
	memcpy(&v, p, 4);
	return v;
}

}  // namespace sigscan

namespace sigscan {

// Kawalek kodu: data = bajty, va = adres data[0] w pamieci procesu (albo w obrazie przy bazie z naglowka).
struct CodeRange {
	const u8* data;
	size_t    size;
	u32       va;
};

// Sekcje wykonywalne obrazu PE zaladowanego w pamieci (uklad jak po LoadLibrary: sekcje pod VirtualAddress).
// image = poczatek obrazu (HMODULE), va = adres, pod ktorym obraz lezy (zwykle == image). -> liczba sekcji, 0 = zly PE.
inline int ExecSections(const u8* image, u32 va, CodeRange* out, int maxOut) {
	if (!image || image[0] != 'M' || image[1] != 'Z')
		return 0;
	u32 nt = ReadU32(image + 0x3C);
	if (nt > 0x1000 || memcmp(image + nt, "PE\0\0", 4) != 0)
		return 0;
	unsigned nSec = image[nt + 6] | (image[nt + 7] << 8);
	unsigned optSize = image[nt + 20] | (image[nt + 21] << 8);
	const u8* sec = image + nt + 24 + optSize;
	int n = 0;
	for (unsigned i = 0; i < nSec && i < 96; i++, sec += 40) {
		u32 vsize = ReadU32(sec + 8), rva = ReadU32(sec + 12), flags = ReadU32(sec + 36);
		if (!(flags & 0x20000000u) || !vsize)   // IMAGE_SCN_MEM_EXECUTE
			continue;
		if (n < maxOut && out) {
			out[n].data = image + rva;
			out[n].size = vsize;
			out[n].va = va + rva;
		}
		n++;
	}
	return n < maxOut ? n : maxOut;
}

// Wystapienia sygnatury we wszystkich zakresach; adresy (va) do out, zwraca liczbe wszystkich (-1 = zla sygnatura).
inline int FindAllVa(const CodeRange* r, int n, const char* pattern, u32* out, int maxOut) {
	Pattern p;
	if (!Parse(pattern, &p))
		return -1;
	int count = 0;
	for (int i = 0; i < n; i++) {
		size_t offs[16];
		int c = FindAll(r[i].data, r[i].size, p, offs, 16);
		for (int k = 0; k < c && k < 16; k++)
			if (count + k < maxOut && out)
				out[count + k] = r[i].va + (u32)offs[k];
		count += c;
	}
	return count;
}

// Odwolania call/jmp rel32 do target we wszystkich zakresach.
inline int FindRefsVa(const CodeRange* r, int n, u32 target, u8 opcode, u32* out, int maxOut) {
	int count = 0;
	for (int i = 0; i < n; i++) {
		size_t offs[16];
		int c = FindRel32Refs(r[i].data, r[i].size, r[i].va, target, opcode, offs, 16);
		for (int k = 0; k < c && k < 16; k++)
			if (count + k < maxOut && out)
				out[count + k] = r[i].va + (u32)offs[k];
		count += c;
	}
	return count;
}

// Dokladnie jedno wystapienie -> jego adres w *va; inaczej false (liczba w *count, gdy podany).
inline bool FindUniqueVa(const CodeRange* r, int n, const char* pattern, u32* va, int* count = 0) {
	u32 hit = 0;
	int c = FindAllVa(r, n, pattern, &hit, 1);
	if (count)
		*count = c;
	if (c != 1)
		return false;
	*va = hit;
	return true;
}

}  // namespace sigscan

namespace sigscan {

// Caly obraz (do odczytu stalych spod adresow z operandow instrukcji).
struct Image {
	const u8* data;
	u32       va;
	u32       size;   // SizeOfImage
};

// SizeOfImage z naglowka PE (0 = zly PE).
inline u32 ImageSize(const u8* image) {
	if (!image || image[0] != 'M' || image[1] != 'Z')
		return 0;
	u32 nt = ReadU32(image + 0x3C);
	if (nt > 0x1000 || memcmp(image + nt, "PE\0\0", 4) != 0)
		return 0;
	return ReadU32(image + nt + 24 + 56);
}

// Wskaznik na len bajtow spod adresu va w obrazie, 0 = poza obrazem.
inline const u8* At(const Image& img, u32 va, u32 len) {
	if (!img.data || va < img.va || va - img.va > img.size || img.size - (va - img.va) < len)
		return 0;
	return img.data + (va - img.va);
}

}  // namespace sigscan
