// Kanal masek odciec zDismember <-> zTimeRewind - uklad tablicy i logika bez Windows (testy: oba pluginy).
// Czesc z pamiecia nazwana procesu: ZCutLink.h.
//
// Po co: zTimeRewind cofa czas (pozycje, HP, stan), a odciete czesci ciala zDismember (sciskanie kosci, odlamki,
// kikuty, ragdoll zwlok) siedza w pamieci zDismember - po cofnieciu NPC wstawal bez glowy. Pluginy sa osobnymi DLL
// (w New Balance pod nazwami z bialej listy UNION_ABI, ladowane przez nia), wiec nie szukamy eksportow po nazwie
// modulu: zDismember wpisuje do tablicy w pamieci nazwanej procesu wskazniki swoich funkcji, zTimeRewind je wola.
// Obie strony otwieraja te sama pamiec (kto pierwszy, ten tworzy - kolejnosc ladowania DLL bez znaczenia).
// Bez zDismember wskazniki zostaja puste: zTimeRewind nic nie nagrywa i dziala jak dotad. Bez zTimeRewind tablica
// nikomu nie przeszkadza.
#pragma once
#include <cstdint>
#include <cstring>

namespace zcut {
	const uint32_t LINK_MAGIC = 0x5455435Au;     // "ZCUT"
	const uint32_t LINK_VERSION = 1;             // zmiana ukladu Table = nowa wersja (inna nazwa pamieci w ZCutLink.h)
	const int NAME_LEN = 32;

	// Flagi SetMask (od zTimeRewind).
	const uint32_t FLAG_ALIVE = 1u;      // postac zyje w pokazywanej chwili: ragdoll zwlok porzucony (poza z animacji)
	const uint32_t FLAG_REVIVE = 2u;     // Commit: postac wstaje (wskrzeszenie / koniec nokautu) - przed ResetPos:
	                                     // nakladki ofiary finishera i oszolomienia zdjete, ogon finishera i wpis
	                                     // oszolomienia zakonczone
	const uint32_t FLAG_PREVIEW = 4u;    // podglad w trakcie przewijania (marionetki), nie Commit

	// Wynik SetMask (bity - co zDismember zrobil).
	const uint32_t DONE_PARTS = 1u;      // czesci przywrocone (zdjete sciskanie) albo scisniete na nowo
	const uint32_t DONE_GIBS = 2u;       // usuniete odlamki (odlatujace / lezace czesci)
	const uint32_t DONE_STUMPS = 4u;     // zatrzymana krew z kikutow
	const uint32_t DONE_RAGDOLL = 8u;    // porzucony ragdoll zwlok
	const uint32_t DONE_OVERLAYS = 16u;  // zdjete nakladki ofiary / oszolomienia
	const uint32_t DONE_TAIL = 32u;      // zakonczony ogon animacji ofiary finishera
	const uint32_t DONE_DAZE = 64u;      // zakonczony wpis oszolomienia
	const uint32_t DONE_NOMODEL = 128u;  // postac bez modelu w swiecie - tylko maska (pamiec / aivar)

	// Maska: bit = czesc ciala zDismember (Core\PartMask.h): 0 glowa, 1-2 przedramie L/P, 3-4 ramie L/P,
	// 5-6 lydka L/P, 7-8 udo L/P. 0 = cala postac.
	const uint32_t MASK_BITS = 0x1FFu;

	typedef uint32_t(__cdecl* GetMaskFn)(void* npc);
	// mask = maska docelowa (zdjete bity = przywrocone czesci, nowe bity = sciskanie bez odlamkow i krwi);
	// who = opis wolajacego do logu zDismember (np. "zTimeRewind commit"). Zwraca DONE_*.
	typedef uint32_t(__cdecl* SetMaskFn)(void* npc, uint32_t mask, uint32_t flags, const char* who);

	struct Table {
		uint32_t magic, version, size;
		int32_t ready;                   // 1 = dostawca (zDismember) wpisal funkcje
		char provider[NAME_LEN];
		GetMaskFn getMask;
		SetMaskFn setMask;
		int32_t consumers;               // ile razy klient (zTimeRewind) sie dolaczyl - diagnostyka
		char consumer[NAME_LEN];
		uint32_t reserved[8];
	};

	inline void CopyName(char* dst, const char* src) {
		size_t n = src ? std::strlen(src) : 0;
		if (n > (size_t)NAME_LEN - 1)
			n = NAME_LEN - 1;
		if (n)
			std::memcpy(dst, src, n);
		dst[n] = '\0';
	}

	// Swieza tablica (pamiec wyzerowana) dostaje naglowek; istniejaca musi miec ten sam uklad.
	inline bool Attach(Table& t) {
		if (t.magic == 0 && t.version == 0 && t.size == 0) {
			t.magic = LINK_MAGIC;
			t.version = LINK_VERSION;
			t.size = (uint32_t)sizeof(Table);
			return true;
		}
		return t.magic == LINK_MAGIC && t.version == LINK_VERSION && t.size == (uint32_t)sizeof(Table);
	}

	// Dostawca: funkcje do tablicy (gotowosc na koncu - klient sprawdza 'ready' przed wywolaniem).
	inline bool Publish(Table* t, const char* provider, GetMaskFn get, SetMaskFn set) {
		if (!t || !get || !set || !Attach(*t))
			return false;
		t->ready = 0;
		CopyName(t->provider, provider);
		t->getMask = get;
		t->setMask = set;
		t->ready = 1;
		return true;
	}

	// Klient: zapis, kto korzysta (log dostawcy / diagnostyka).
	inline bool Join(Table* t, const char* consumer) {
		if (!t || !Attach(*t))
			return false;
		CopyName(t->consumer, consumer);
		t->consumers++;
		return true;
	}

	// Tablica gotowa do wywolan (dostawca zaladowany i opublikowal funkcje).
	inline bool Ready(const Table* t) {
		return t && t->magic == LINK_MAGIC && t->version == LINK_VERSION && t->size == (uint32_t)sizeof(Table) &&
			t->ready == 1 && t->getMask && t->setMask;
	}

	// Opis wyniku SetMask do logu klienta: "czesci, odlamki, ragdoll" ("nic" przy 0).
	inline const char* DoneText(uint32_t done, char* buf, size_t size) {
		static const char* names[] = { "czesci", "odlamki", "kikuty", "ragdoll", "nakladki", "ogon finishera",
			"oszolomienie", "bez modelu" };
		size_t len = 0;
		if (size)
			buf[0] = '\0';
		for (int i = 0; i < 8; i++) {
			if (!(done & (1u << i)))
				continue;
			const char* n = names[i];
			size_t need = std::strlen(n) + (len ? 2 : 0);
			if (len + need + 1 > size)
				break;
			if (len) {
				buf[len++] = ',';
				buf[len++] = ' ';
			}
			std::memcpy(buf + len, n, std::strlen(n));
			len += std::strlen(n);
			buf[len] = '\0';
		}
		if (!len && size > 4)
			std::memcpy(buf, "nic", 4);
		return buf;
	}
}
