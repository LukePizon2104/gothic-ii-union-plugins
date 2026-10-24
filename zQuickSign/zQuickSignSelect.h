// zQuickSignSelect.h - czysta logika wyboru runy pod F (Shift + 4..0, Shift + kolko myszy) - bez Windows i adresow gry,
// zeby dalo sie ja testowac poza gra (tests_zqs.cpp). Uzywa zQuickSign.cpp.
#pragma once

namespace qs {

enum { DIK_4 = 0x05, DIK_0 = 0x0B, KEYNO_MAX = 7 };

// Klawisze run w ksiedze czarow: DIK_4..DIK_9, DIK_0 -> oCSpell::keyNo 1..7 (4 = 1, ..., 9 = 6, 0 = 7), inne 0.
// Tak samo liczy gra w oCGame::HandleEvent (0x6FCFBD: znak klawisza - 0x33, '0' jako ':').
inline int DikToKeyNo(int dik) {
	return dik >= DIK_4 && dik <= DIK_0 ? dik - DIK_4 + 1 : 0;
}

// keyNo -> numer slotu jak na kregu magii (4..10; klawisz 0 = 10), inne 0.
inline int KeyNoToSlot(int keyNo) {
	return keyNo >= 1 && keyNo <= KEYNO_MAX ? keyNo + 3 : 0;
}

inline bool InList(int value, const int* list, int n) {
	for (int i = 0; i < n; i++)
		if (list[i] == value)
			return true;
	return false;
}

// Os z kolka myszy (zCInput::GetMousePos, z) -> krok wyboru: w gore = poprzedni (-1), w dol = nastepny (+1).
inline int WheelDir(float z) {
	return z > 0.0f ? -1 : z < 0.0f ? 1 : 0;
}

// Klucz kolejnosci czaru: slot (keyNo), przy rownych indeks w ksiedze; czary bez klawisza (keyNo <= 0) na koncu.
inline int WheelOrderKey(const int* keyNo, int i) {
	return (keyNo[i] > 0 ? keyNo[i] : 1000) * 1024 + i;
}

// Kolko: nastepny (dir > 0) albo poprzedni (dir < 0) znak W3 w kolejnosci slotow, z zawijaniem.
// keyNo[i] - slot czaru i w ksiedze (oCSpell::keyNo), isSign[i] - czy to znak W3, cur - indeks wybranego czaru
// (oCMag_Book::spellnr; poza zakresem = brak wybranego - pierwszy albo ostatni znak). Wybrana moze byc zwykla runa -
// wtedy najblizszy znak za (przed) jej slotem. -> indeks czaru albo -1 (brak znakow, dir 0). Jedyny znak = on sam.
inline int WheelPick(const int* keyNo, const bool* isSign, int n, int cur, int dir) {
	if (dir == 0 || n <= 0)
		return -1;
	bool hasCur = cur >= 0 && cur < n;
	int curKey = hasCur ? WheelOrderKey(keyNo, cur) : (dir > 0 ? -1 : 0x7FFFFFFF);
	int best = -1, bestKey = 0, wrap = -1, wrapKey = 0;
	for (int i = 0; i < n; i++) {
		if (!isSign[i])
			continue;
		int k = WheelOrderKey(keyNo, i);
		if (dir > 0) {
			if (k > curKey && (best < 0 || k < bestKey)) { best = i; bestKey = k; }
			if (wrap < 0 || k < wrapKey) { wrap = i; wrapKey = k; }
		}
		else {
			if (k < curKey && (best < 0 || k > bestKey)) { best = i; bestKey = k; }
			if (wrap < 0 || k > wrapKey) { wrap = i; wrapKey = k; }
		}
	}
	return best >= 0 ? best : wrap;
}

// Odczyt cyfr co klatke: now[1..7] - wcisniete klawisze keyNo 1..7, prev - stan z poprzedniej klatki (aktualizowany).
// -> keyNo cyfry wcisnietej w tej klatce przy trzymanym Shift (najnizszy, gdy kilka naraz), 0 = nic. Shift wcisniety
// przy juz trzymanej cyfrze nie wybiera (to nie nowe wcisniecie).
inline int DigitEdge(bool* prev, const bool* now, bool shift) {
	int hit = 0;
	for (int k = 1; k <= KEYNO_MAX; k++) {
		if (now[k] && !prev[k] && shift && !hit)
			hit = k;
		prev[k] = now[k];
	}
	return hit;
}

// Tekst komunikatu i etykiety: "Znak: <nazwa>"; nazwa, ktora juz jest "Znak ..." (runy znakow W3: "Znak Aard"), bez
// zmian. Pusta nazwa - "?". Obcina do size - 1 znakow.
inline void FormatSignText(char* out, int size, const char* name) {
	if (!out || size <= 0)
		return;
	if (!name || !*name)
		name = "?";
	bool hasPrefix = name[0] == 'Z' && name[1] == 'n' && name[2] == 'a' && name[3] == 'k' && (name[4] == ' ' || name[4] == 0);
	const char* parts[2] = { hasPrefix ? "" : "Znak: ", name };
	int n = 0;
	for (int p = 0; p < 2; p++)
		for (const char* s = parts[p]; *s && n < size - 1; s++)
			out[n++] = *s;
	out[n] = 0;
}

// Czy szybki rzut ma szukac celu fokusem magii (PickTarget): czar celujacy w fokus (targetCollectAlgo FOCUS = 2,
// FOCUS_FALLBACK_NONE = 4, FOCUS_FALLBACK_CASTER = 5) z katem zbierania > 0. Czary kierunkowe z katem 0 (Znaki W3
// Aard / Igni / Yrden: stozek i krag liczy zDismember z celownika) - nie: silnik i tak nie zbierze dla nich fokusu,
// a brak celu ich nie blokuje (rzut idzie dalej, zDismember bierze cele w stozku).
// dismSign = czar jest Znakiem zDismember (ZDISM_SignHoldState = 1): cel wybiera zDismember (Axii - fokus w stozku albo
// najblizszy NPC w stozku z celownika, zasieg AxiiRange, linia wzroku) - bez fokusu magii; brak celu nie blokuje rzutu.
inline bool WantsMagicFocus(int algo, int azi, bool dismSign = false) {
	return !dismSign && (algo == 2 || algo == 4 || algo == 5) && azi > 0;
}

// Klasa broni gestu trybu alternatywnego (make_signs.py ALT_WEAPONS, indeks w SIGN_ANIS_ALT): 0 bez broni (brak, piesci,
// luk, kusza, runa), 1 sztylet / 1H (gest W3 "rp" - prawa noga z przodu jak garda 1H), 2 2H (W3 "lp" jak garda 2H).
// fmode Gothica: 0 brak, 1 piesci, 2 sztylet, 3 1H, 4 2H, 5 luk, 6 kusza, 7 magia.
inline int SignWeaponClass(int fmode) {
	return fmode == 2 || fmode == 3 ? 1 : fmode == 4 ? 2 : 0;
}

// Czas biezacego gestu dla zDismember (eksport ZQS_SignGesture): od startu gestu (start, GetTickCount) do teraz (now) -
// *castLeft = ms do pchniecia dloni / dotkniecia ziemi (0, gdy minelo), *lenLeft = ms do konca gestu. false = gest juz
// sie skonczyl (albo dlugosc 0).
inline bool GestureTimeLeft(int castMs, int lengthMs, unsigned long start, unsigned long now, int* castLeft, int* lenLeft) {
	int el = (int)(now - start);
	if (el < 0)
		el = 0;
	int c = castMs - el, l = lengthMs - el;
	if (castLeft)
		*castLeft = c > 0 ? c : 0;
	if (lenLeft)
		*lenLeft = l > 0 ? l : 0;
	return l > 0;
}

// Klatka wariantu ruchu, od ktorej gra dalej gest przerwany ruszeniem bohatera: gest zwykly - ta sama klatka (oba warianty
// maja te same klatki); gest alternatywny (inna os czasu, bez wlasnego wariantu ruchu - bohatera w miejscu trzyma
// zDismember, wiec to tylko zapas bez zDismember / przy SignAltInPlace=0) - pchniecie dloni gestu zwyklego (castMs, 25 kl./s).
// Tryb zwykly Znaku W3 z koncem pchniecia dloni (SignCastSync, 23.10): ms od puszczenia F do rzutu - SignCastSyncMs Znaku,
// a bez niej (0) chwila pchniecia gestu (SignCastMs).
inline int SyncDelayMs(int syncMs, int gestureCastMs) {
	return syncMs > 0 ? syncMs : gestureCastMs > 0 ? gestureCastMs : 0;
}

// Czy odlozony rzut Znaku W3 jest przerwany (jak w W3: bez skutku i bez kosztu). lateMs = o ile pozniej niz w planie
// sprawdzamy (HookActionMove nie jest wolany przy blokadzie ruchu - unik, przeskok, finisher, dialog); -1 = jeszcze przed
// czasem rzutu. Kolejnosc: zmiana runy, trafienie, ekwipunek, spoznienie.
enum { QSYNC_OK = 0, QSYNC_SPELL, QSYNC_HIT, QSYNC_INVENTORY, QSYNC_LATE };
inline int SyncVerdict(bool sameSpell, bool hit, bool inventory, int lateMs, int lateLimitMs) {
	if (!sameSpell)
		return QSYNC_SPELL;
	if (hit)
		return QSYNC_HIT;
	if (inventory)
		return QSYNC_INVENTORY;
	if (lateMs > lateLimitMs)
		return QSYNC_LATE;
	return QSYNC_OK;
}

inline float MoveSwitchFrame(bool altGesture, float frame, int normalCastMs) {
	float f = altGesture ? (float)normalCastMs / 40.0f : frame;
	return f < 0.0f ? 0.0f : f;
}

// XBOW_SLOT_23_10 - Kolko z kusza (23.10, slot kuszy zDismember): za ostatnim Znakiem jest wpis wirtualny kuszy (gdy xbow = kusza dostepna -
// zalozona, paczka W3). cur = n oznacza wybrana kusze. -> indeks czaru w ksiedze, n = kusza, -1 = nic. Bez kuszy jak WheelPick
// (wybrana, ale niedostepna kusza = brak wybranego czaru). Ksiega obcieta do 64 czarow jak w zQuickSign.cpp.
inline int WheelPickXbow(const int* keyNo, const bool* isSign, int n, int cur, int dir, bool xbow) {
	if (n > 64)
		n = 64;
	if (n < 0)
		n = 0;
	if (!xbow)
		return WheelPick(keyNo, isSign, n, cur >= n ? -1 : cur, dir);
	int k2[65];
	bool s2[65];
	for (int i = 0; i < n; i++) {
		k2[i] = keyNo[i];
		s2[i] = isSign[i];
	}
	k2[n] = 2000;   // za czarami bez klawisza (WheelOrderKey: keyNo <= 0 -> 1000)
	s2[n] = true;
	return WheelPick(k2, s2, n + 1, cur, dir);
}

// Etykieta i komunikat slotu kuszy: "Kusza: <nazwa> (belty: N)" (cp1250 l z kreska), N >= 999 = kusza tworzaca belty.
inline void FormatXbowText(char* out, int size, const char* name, int bolts) {
	if (!out || size <= 0)
		return;
	if (!name || !*name)
		name = "?";
	char num[16];
	int k = 0;
	if (bolts >= 999) {
		const char* inf = "bez limitu";
		while (inf[k] && k < 15) {
			num[k] = inf[k];
			k++;
		}
	}
	else {
		int v = bolts < 0 ? 0 : bolts;
		char tmp[12];
		int t = 0;
		do {
			tmp[t++] = (char)('0' + v % 10);
			v /= 10;
		} while (v && t < 11);
		while (t)
			num[k++] = tmp[--t];
	}
	num[k] = 0;
	const char* parts[5] = { "Kusza: ", name, " (be\xB3ty: ", num, ")" };
	int n = 0;
	for (int p = 0; p < 5; p++)
		for (const char* s = parts[p]; *s && n < size - 1; s++)
			out[n++] = *s;
	out[n] = 0;
}

// F przy wybranej kuszy (zQuickSign -> zDismember ZDISM_XbowInput): czy po heldMs trzymania wyslac prog przytrzymania
// (faza 2 - celownik). Prog XbowHoldMs (W3 ThrowHoldTest 0,3 s).
inline bool XbowHoldReached(unsigned long pressTick, unsigned long now, int holdMs) {
	return (long)(now - pressTick) >= (long)(holdMs < 0 ? 0 : holdMs);
}

}  // namespace qs
