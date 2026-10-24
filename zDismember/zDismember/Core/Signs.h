// Logika bez silnika: Znaki Wiedzmina (Aard, Igni, Axii, Yrden, Quen) - nazwy, koszty i odnowienie, stozek,
// wybor celu, decyzja walka / poza walka, tarcza Quen, strefa Yrden z dokladnym przywracaniem wartosci NPC.
// Silnik (haki rzutu, obrazenia, animacje, efekty): zDismember\Signs.cpp. Projekt:
// [dokument projektowy - niepublikowany].
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zdcore {
	// Kolejnosc jak w ini (SignSpellIds, SignManaCost, SignAniLetters): Aard, Igni, Axii, Yrden, Quen.
	enum SignKind { SIGN_AARD = 0, SIGN_IGNI, SIGN_AXII, SIGN_YRDEN, SIGN_QUEN, SIGN_COUNT };

	const char* SignName(int sign);                      // "Aard" ... ("?" dla zlego indeksu)
	int SignFromName(const std::string& name);           // "aard" / "AARD" -> SIGN_AARD, inaczej -1
	int SignFromSpellId(const int ids[SIGN_COUNT], int spellId);   // slot czaru -> znak, -1 = nie nasz
	// Sloty znakow: SignSpellIds z ini vs stale SPL_Aard..SPL_Quen ze skryptu run (script[i] = -1: brak symbolu).
	// Zrodlem prawdy dla przedmiotu jest skrypt (C_Item.spell), wiec poprawna stala (0..4094) wygrywa z ini - np. paczka
	// New Balance (196..200) przy starym SignSpellIds=90..94. Wynik: liczba znakow wzietych ze skryptu; -1 = po scaleniu
	// dwa znaki w jednym slocie - wtedy out = ini (do logu). Steam: skrypt = ini, wynik 0.
	int ResolveSignIds(const int ini[SIGN_COUNT], const int script[SIGN_COUNT], int out[SIGN_COUNT]);
	std::string SignIdsLabel(const int ids[SIGN_COUNT]);   // "196,197,198,199,200"
	// "WND,FBT,SLE,FEA,HEA" -> 5 liter animacji (1-4 litery A-Z); false (out nietkniete) przy bledzie.
	bool ParseSignLetters(const std::string& s, std::string out[SIGN_COUNT]);

	// ---------- mana i odnowienie ----------
	enum SignCastResult { SIGNCAST_OK, SIGNCAST_NO_MANA, SIGNCAST_COOLDOWN, SIGNCAST_DISABLED };
	// Kolejnosc: wylaczone, odnowienie (komunikat z czasem), mana.
	SignCastResult SignCheckCast(bool enabled, int mana, int cost, float cooldownLeft);

	struct SignCooldowns {
		float left[SIGN_COUNT] = {};
		void Start(int sign, float seconds);
		void Update(float dt);
		float Left(int sign) const;
		void Clear();
	};

	// ---------- stozek ----------
	enum ConeReason { CONE_OK = 0, CONE_RANGE, CONE_HEIGHT, CONE_ANGLE, CONE_BEHIND, CONE_NODIR };
	const char* ConeReasonName(int reason);   // do logu: "ok", "zasieg", "wysokosc", "kat", "za plecami", "kierunek"
	struct ConeHit {
		bool inside = false;
		float dist = 0.0f;       // w poziomie (cm)
		float angleDeg = 0.0f;   // od osi patrzenia
		int reason = CONE_RANGE; // ConeReason
	};
	// Poczatek (ox,oy,oz), kierunek patrzenia w poziomie (fx,fz - dowolna dlugosc), cel (tx,ty,tz).
	// Cel blizej niz nearRadius liczy sie bez wzgledu na kat (wtulony w bok bohatera), byle nie za plecami.
	ConeHit ConeTest(float ox, float oy, float oz, float fx, float fz, float tx, float ty, float tz,
		float range, float halfAngleDeg, float maxDy, float nearRadius = 60.0f);

	// ---------- cele ----------
	// Wartosci ATT_* z Constants.d (= enum NPC_ATT_* silnika): wrogi 0, zly 1, neutralny 2, przyjazny 3.
	const int ATTITUDE_HOSTILE = 0, ATTITUDE_ANGRY = 1, ATTITUDE_NEUTRAL = 2, ATTITUDE_FRIENDLY = 3;
	struct SignTargetInfo {
		bool isPlayer = false;
		bool dead = false;
		bool unconscious = false;
		bool partyMember = false;    // aivar[AIV_PARTYMEMBER]
		bool targetsHero = false;    // npc->enemy == bohater
		bool weaponDrawn = false;    // tryb broni != brak
		bool human = false;          // gildia ludzi
		bool immortal = false;       // NPC_FLAG_IMMORTAL (chroniony skryptem - czary NB go pomijaja, B_MagicHurtNpc)
		int attitude = ATTITUDE_NEUTRAL;   // oCNpc::GetAttitude(bohater)
	};
	// Wrog: wrogo nastawiony albo walczy z bohaterem.
	bool SignIsEnemy(const SignTargetInfo& t);
	// Znaki obszarowe (Aard, Igni, pekniecie Quen) - jak czar obszarowy gry, bez wymogu wczesniejszego ataku: wrogowie,
	// potwory niebedace przyjaciolmi (neutralne i zle tez), ludzie neutralni / zli tylko z hitNeutral (SignHitNeutral -
	// trafienie = przestepstwo jak zwyklym czarem). Nigdy: bohater, martwi, nieprzytomni, druzyna, niesmiertelni, przyjaciele.
	bool SignAffects(const SignTargetInfo& t, bool hitNeutral);
	// Powod odrzucenia do logu (nullptr = cel znaku).
	const char* SignRejectReason(const SignTargetInfo& t, bool hitNeutral);
	// Axii: walka = wrogo nastawiony albo atakuje bohatera (czlowiek z dobyta bronia, potwor zawsze). Inaczej niepamiec.
	bool AxiiInCombat(const SignTargetInfo& t);
	// Axii poza walka na czlowieka (nie druzyna, zywy): niepamiec + trans (nie zagada, nie zatrzyma, nie zauwazy bohatera).
	bool AxiiTranceApplies(const SignTargetInfo& t);
	// Czas stanu, ktory konczy trans: za bezpiecznikiem skryptu ZDF_SIGN_TRANCE_LIMIT_SEC (600 s), ponizej trafienia (2000 s).
	float TranceStateEndMs();

	// Axii: filtr celu (nullptr = cel). Poza walka Urok / trans to nie atak - neutralni i przyjazni tez sa celem, w walce
	// wrogowie; nigdy bohater, martwi, nieprzytomni, druzyna. Odpornych rozstrzyga lista Uroku (Spell_Logic_Charm) po wyborze.
	const char* AxiiRejectReason(const SignTargetInfo& t);
	struct AxiiCandidate {
		int id = -1;
		float dist = 0.0f;              // w poziomie (cm)
		float angleDeg = 0.0f;          // od osi celownika / przodu bohatera
		int cone = CONE_OK;             // ConeTest(zasieg AxiiRange, polkat AxiiCone).reason
		const char* reject = nullptr;    // AxiiRejectReason
		bool visible = true;            // linia wzroku (bez celow za sciana)
		bool focus = false;             // fokus silnika / NPC pod celownikiem
	};
	// Powod pominiecia do logu (nullptr = dobry cel): filtr, potem geometria stozka, potem linia wzroku.
	const char* AxiiCandidateReason(const AxiiCandidate& c);
	// Fokus, jesli jest dobrym celem (w stozku i zasiegu, widoczny); inaczej najblizszy dobry (rowna odleglosc - mniejszy
	// kat). -1 = brak.
	int PickAxiiTarget(const std::vector<AxiiCandidate>& cands);

	// ---------- Quen ----------
	struct QuenState {
		bool active = false;
		bool breakPending = false;   // pekniecie do obsluzenia w petli gry (odrzut wrogow, efekt)
		float left = 0.0f;
		int hitsLeft = 0;            // ile trafien jeszcze pochlonie (QuenHitsAt z mocy)
		void Cast(float seconds, int hits = 1);   // nowa tarcza albo odnowienie (pelna liczba trafien, min. 1)
		bool Absorb();               // true = trafienie pochloniete; ostatnie trafienie - tarcza peka (breakPending)
		bool Update(float dt);       // true = wygasla w tej klatce (bez pekniecia)
		bool TakeBreak();            // zwraca i kasuje breakPending
		void Clear();
	};

	// ---------- Yrden ----------
	const int SIGN_PROT_COUNT = 8;   // oEDamageIndex: barrier, blunt, edge, fire, fly, magic, point, fall
	const int SIGN_PROT_IMMUNE = -1; // IMMUNE z Constants.d - nie ruszamy
	struct NpcCombatStats {
		float timeScale = 1.0f;      // zCModel::timeScale
		int prot[SIGN_PROT_COUNT] = {};
	};
	struct YrdenMod {
		NpcCombatStats orig;         // przed strefa
		NpcCombatStats applied;      // nasze wartosci (do rozpoznania cudzych zmian przy przywracaniu)
	};
	// Ochrona oslabiana przez Yrden: obuch, ostrze, ogien, magia, przebicie (bity indeksow).
	uint32_t YrdenArmorMask();
	// slow: mnoznik tempa modelu (0,65), armorCutPercent: o ile procent mniej ochrony (30).
	YrdenMod YrdenApply(const NpcCombatStats& cur, float slow, float armorCutPercent, uint32_t mask);
	// Dokladny powrot: wartosc nadal nasza -> oryginal; zmieniona w strefie przez kogos innego -> cofniecie tylko naszej
	// czesci (ochrona: + roznica, tempo: x oryginal / nasze).
	NpcCombatStats YrdenRestore(const YrdenMod& m, const NpcCombatStats& cur);
	// Samo tempo z YrdenRestore (ten sam wzor) - dla powrotu odlozonego.
	float YrdenRestoreTempo(const YrdenMod& m, float curTimeScale);
	// Model zamrozony z zewnatrz (timeScale ~0 - marionetka zTimeRewind w trakcie cofania): powrotu tempa nie da sie
	// policzyc teraz (0 x oryginal / nasze = 0, a zTimeRewind oddalby potem nasze applied na stale - postac zostawala
	// spowolniona / przyspieszona). true = tempo odlozyc do odmrozenia (ochrony wracaja od razu).
	bool YrdenTempoDeferred(const YrdenMod& m, float curTimeScale);

	struct YrdenZone {
		bool active = false;
		float x = 0.0f, y = 0.0f, z = 0.0f, radius = 0.0f, left = 0.0f;
		void Start(float px, float py, float pz, float r, float seconds);   // ponowny rzut przenosi strefe
		bool Contains(float px, float py, float pz, float maxDy = 300.0f) const;
		bool Update(float dt);       // true = wygasla w tej klatce
		void Stop();
	};
	struct YrdenPlan {
		std::vector<int> enter, leave;
	};
	// applied: NPC z nalozonymi zmianami, insideEnemies: wrogowie w strefie teraz.
	YrdenPlan YrdenStep(const std::vector<int>& applied, const std::vector<int>& insideEnemies);

	// ---------- Aard: kto leci ----------
	// Silnik rzuca NPC (oCAIHuman::StartFlyDamage), gdy skuteczne obrazenia lotu > 0, czyli DAM_FLY > PROT_FLY. Ludzie maja
	// 0, orkowie 130-160, wilki 10-75, trolle / golemy kamienne IMMUNE. Aard ma lekkie obrazenia (15), wiec lekkich
	// (ochrona <= maxProt, model nie wyzszy niz maxHeight) plugin rzuca z ochrona lotu 0 na czas trafienia, ciezkich nie.
	bool AardShouldFly(int protFly, float modelHeight, int maxProt, float maxHeight);

	// Lot zlecony silnikowi sprawdzany po checkTime s: przesuniecie wzdluz pchniecia (cm, ujemne = w strone bohatera)
	// albo NPC w powietrzu (fizyka voba / S_FALL / S_FALLB). FLY_FAILED = nasz odrzut (potkniecie + odsuniecie).
	enum FlyVerdict { FLY_WAIT, FLY_OK, FLY_FAILED };
	FlyVerdict AardFlyVerdict(float elapsed, float movedAlong, bool airborne, float checkTime, float minMove);

	// ---------- widocznosc celu ----------
	// Wysokosci punktow celu (swiat): srodek bboxa, 3/4 bboxa i pozycja voba + 20 cm (nie wyzej niz gora bboxa).
	// Niskie stwory (wilk: vob przy ziemi) i nierowny teren nie moga byc zasloniete jednym promieniem tuz nad ziemia.
	std::vector<float> LosTargetHeights(float posY, float bboxMinY, float bboxMaxY);
	// Skad patrzy bohater: piersi (3/4 wysokosci bboxa).
	float LosEyeHeight(float bboxMinY, float bboxMaxY);

	// ---------- warianty oszolomienia Axii i reakcja palenia Igni (stany AI w zDismember_Signs.d) ----------
	// Numery = stale ZDF_SIGN_STATE_* w skrypcie (ZDF_Sign_StartState).
	enum StunVariant {
		STUN_NONE = 0,
		STUN_PRAY,       // kleczenie T_STAND_2_PRAY / S_PRAY (jak Daze - jedyny wariant z egzekucja / finisherem)
		STUN_FEAR1,      // skulenie ze strachu T_STAND_2_FEAR_VICTIM1..3 / S_FEAR_VICTIMx / T_FEAR_VICTIM_2_STAND
		STUN_FEAR2,
		STUN_FEAR3,
		STUN_WOUNDED,    // powalenie T_STAND_2_WOUNDED (4,9 s) / T_WOUNDED_2_STAND (8,5 s) - tylko z ini
		STUN_WOUNDEDB,   // T_STAND_2_WOUNDEDB (3,4 s) / T_WOUNDEDB_2_STAND (10 s) - tylko z ini
		STUN_MONSTER,    // potwory i orkowie: T_STUMBLEB, potem stoja bez AI walki
		STUN_BURN,       // Igni: S_FIRE_VICTIM (ludzie)
		STUN_TRANCE,     // Axii poza walka (runda 5): trans - ludzie stoja z rekami skrzyzowanymi, bez percepcji bohatera
		STUN_COUNT
	};
	inline uint32_t StunBit(int v) { return v > STUN_NONE && v < STUN_COUNT ? (1u << v) : 0u; }
	const char* StunName(int v);
	// "PRAY,FEAR1,FEAR2,FEAR3" -> maska; nieznane nazwy pominiete (*ok = false).
	uint32_t ParseStunAnis(const std::string& s, bool* ok);
	// Losowo z maski, bez powtorki poprzedniego (chyba ze jedyny). STUN_NONE dla pustej maski.
	int PickStunVariant(uint32_t mask, int last, uint32_t rnd);
	// Petla stanu w skrypcie konczy sie, gdy Npc_GetStateTime >= ceil(sekundy) (min. 1); plugin konczy dokladnie
	// ustawiajac czas stanu na StunStateEndMs. Trafienie: czas stanu 2000 s (koniec bez animacji wstawania).
	int StunScriptSeconds(float seconds);
	float StunStateEndMs(float seconds);

	// ---------- Quen: co pochlania, pilnowanie efektu ----------
	// hasAttacker: napastnik inny niz bohater; isFall: DAM_FALL (oCNpc::CreateFallDamage, bez napastnika);
	// isTick: takt obrazen w czasie (palenie, OverlayActivate) - nigdy.
	bool QuenShouldAbsorb(bool shieldActive, bool isTick, bool hasAttacker, bool isFall);
	enum QuenFxAction { QFX_NONE, QFX_STOP, QFX_CREATE, QFX_RESPAWN };
	QuenFxAction QuenFxGuard(bool shieldActive, bool fxExists, bool fxFinished, float fxDistToHero, float maxDist);

	// ---------- runda 3: moc z maksymalnej many ----------
	// P = 1 + min(maxBonus, maxMana x perMana); ujemna mana = 1.
	float SignPower(int maxMana, float perMana, float maxBonus);
	// Mnoznik obrazen znakow z inteligencji (New Balance: zmienna skryptu ATR_INTELLECT; brak = -1): 1 + INT / 10 x
	// per10Pct %, najwyzej maxMul; ujemna INT, per10Pct <= 0 albo maxMul <= 1 = 1. Tylko obrazenia (czasy z many jak dotad).
	float SignIntPower(int intellect, float per10Pct, float maxMul);
	// Skalowanie obrazen znakow (SignPowerSource): wszystkie obrazenia (trafienia Aard / Igni, fala Aard, palenie - takty,
	// strumien Igni, pekniecie Quen, pulapka Yrden) z jednego mnoznika; czasy efektow zawsze z many (SignPower).
	// 0 = moc z maks. many, 1 = mnoznik z INT (SignIntPower), 2 = iloczyn obu. Bez INT (Steam G2, intellect < 0) 1 i 2
	// licza sie jak 0 (fellBack - wpis w logu). Zla wartosc ustawienia = 2.
	enum SignPowerSource { SIGN_POWER_MANA = 0, SIGN_POWER_INT = 1, SIGN_POWER_BOTH = 2 };
	struct SignDamagePower {
		float mul = 1.0f;
		int source = SIGN_POWER_MANA;   // zrodlo faktycznie uzyte
		bool fellBack = false;          // INT / oba bez INT - liczone z many
	};
	SignDamagePower SignDamagePowerOf(int setting, float manaPower, int intellect, float per10Pct, float maxMul);
	const char* SignPowerSourceName(int source);   // "mana", "INT", "oba"
	// Zmiana wartosci domyslnej w ini (SignDefaultsRev): stara domyslna -> nowa, inna (ustawiona recznie / w menu) zostaje.
	float MigrateDefault(float value, float oldDef, float newDef);
	// Obrazenia w tablicy aryDamage deskryptora (indeks = bit typu DAM_*): gdy tablica pusta, total rozdzielony po rowno
	// na typy z mode (zaokraglone). false = bez zmian (tablica juz ma wartosci, brak typu albo total <= 0).
	bool SignSplitDamage(unsigned long mode, float total, unsigned long ary[8]);
	// SignDamageArray: -1 = samo (tylko przy New Balance - UNION_ABI liczy obrazenia z tablicy), 0 = nigdy, 1 = zawsze.
	bool SignDamageArrayWanted(int setting, bool newBalance);
	// Quen: 1 trafienie przy P < 1,5, 2 przy P >= 1,5, 3 przy P >= 2.
	int QuenHitsAt(float power);
	// Yrden: od wartosci przy P = 1 liniowo do wartosci granicznej przy P = 2 (dalej bez zmian).
	float YrdenSlowAt(float power, float slowAtOne, float slowMin);
	float YrdenArmorAt(float power, float armorAtOne, float armorMax);

	// ---------- runda 3: tryb alternatywny przez przytrzymanie rzutu ----------
	// Pierwszy Invest = Begin (czar czeka: SPL_STATUS_CANINVEST_NO_MANADEC), kolejne Invest co klatke = Tick,
	// puszczenie (oCSpell::SetReleaseStatus) = Release. Krotko = tryb zwykly przy puszczeniu; dluzej niz altHold =
	// tryb alternatywny: jednorazowy od razu (SENDCAST), podtrzymywany do puszczenia albo RequestStop (mana, zapis).
	enum HoldPhase { HOLD_NONE, HOLD_PENDING, HOLD_SUSTAINING, HOLD_FIRED };
	enum HoldAction { HOLD_WAIT, HOLD_CAST_NORMAL, HOLD_CAST_ALT, HOLD_START_SUSTAIN, HOLD_SUSTAIN, HOLD_STOP };
	struct SignHold {
		int phase = HOLD_NONE;
		float held = 0.0f;
		bool stopRequested = false;
		HoldAction Begin(float altHold);   // altHold <= 0: bez trybow alternatywnych (rzut zwykly od razu)
		HoldAction Tick(float dt, float altHold, bool sustainedAlt);
		HoldAction Release();
		void RequestStop() { stopRequested = true; }
		void Reset() { phase = HOLD_NONE; held = 0.0f; stopRequested = false; }
	};
	// Celownik zQuickSign (celowanie od wcisniecia F, eksport ZDISM_SignHoldState): czy znak w danym trybie ma kierunek.
	// Bez kierunku: Quen (tarcza na bohaterze) i fala Aard wokol bohatera - tam celownik znika.
	bool SignAimsAt(int sign, bool alt);
	// Co pokazac przy celowniku w trakcie trzymania rzutu: held = rzut trzymany (faza != HOLD_NONE), frac = ladowanie
	// trybu alternatywnego 0..1 (held / altHold), alt = juz tryb alternatywny, wantsAim = SignAimsAt, showBar = pasek
	// ladowania (tylko przy altHold > 0; po przejsciu w alt bez kierunku znika razem z celownikiem).
	struct SignAimHud {
		bool held = false;
		float frac = 0.0f;
		bool alt = false;
		bool wantsAim = false;
		bool showBar = false;
	};
	SignAimHud SignHoldHud(int phase, float held, float altHold, int sign);
	// Mana na sekunde w trybach podtrzymywanych: ulamki przechodza na kolejne klatki.
	struct SustainMana {
		float acc = 0.0f;
		int Take(float dt, float perSecond);
		void Reset() { acc = 0.0f; }
	};
	// Pulapka Yrdena: ladunki i odstep miedzy pociskami w tego samego wroga (id).
	struct YrdenTrapState {
		int charges = 0;
		float interval = 0.8f;
		std::vector<std::pair<int, float>> lastShot;
		void Arm(int n, float minInterval);
		bool CanFire(int id, float now) const;
		void Fire(int id, float now);
		bool Spent() const { return charges <= 0; }
	};
	// Aktywny Quen: leczenie czescia pochlonietych obrazen (zaokraglenie od polowy w gore, nie ponad maksimum).
	int QuenActiveHealTo(int hp, int hpMax, float absorbed, float ratio);
	// Marionetka Axii: najblizszy zywy wrog bohatera (dist od marionetki) w zasiegu; -1 = brak.
	struct PuppetCandidate {
		int id;
		float dist;
		bool enemyOfHero;
		bool alive;          // zywy i przytomny
	};
	int PickPuppetTarget(const std::vector<PuppetCandidate>& cands, float range);

	// ---------- runda 4 ----------
	// Podtrzymanie z runa w rece: klawisz akcji trzymany = oCAIHuman::MagicMode pominiety (ruch dziala, czar czeka),
	// puszczony = zwykly MagicMode (MagicCheckSpellStates wykrywa puszczenie). Sciezka zQuickSign nie przechodzi przez MagicMode.
	bool ChannelSkipMagicMode(bool sustaining, bool magicPath, bool actionHeld);
	// Poza podtrzymania: gest znaku (gorna polowa ciala) zatrzymany na klatce z wyciagnieta dlonia.
	float ChannelHoldFrame(int numFrames, int wanted);
	enum ChannelPoseAction { CPOSE_NONE, CPOSE_FREEZE, CPOSE_RESTART };
	ChannelPoseAction ChannelPoseStep(bool active, float actFrame, float holdFrame);

	// ---------- tryb alternatywny w miejscu (23.10) ----------
	// zQuickSign gra w trybie alternatywnym Aard / Yrden gest "w ziemie" calym cialem (eksport ZQS_SignGesture: wynik 1,
	// alt 1, ms do dotkniecia ziemi i do konca gestu, pytany w haku oCSpell::Cast). Wtedy skutek (fala, pulapka) czeka na
	// dotkniecie ziemi, a bohater stoi w miejscu do konca gestu. use = false: jak dotad - skutek od razu, bez blokady
	// (inny Znak, tryb magii bez zQuickSign, stara wersja zQuickSign, SignAltInPlace=0).
	struct AltInPlacePlan {
		bool use = false;
		float delay = 0.0f;   // s do skutku
		float hold = 0.0f;    // s blokady ruchu
	};
	AltInPlacePlan PlanAltInPlace(bool enabled, bool altCast, int qsResult, int qsAlt, int castMsLeft, int lengthMsLeft);
	// Odliczanie odlozonego skutku z kolejki: true = wykonac teraz (delay juz <= 0); inaczej delay -= dt.
	bool PendingStep(float& delay, float dt);
	// Blokada ruchu bohatera na czas gestu (oCNpc::SetMovLock - oCAIHuman::Moving wola wtedy tylko _Stand): zakladamy tylko
	// wolna (dialog, medytacja, finisher - nie ruszamy), zdejmujemy tylko wlasna i tylko, gdy nadal stoi; gdy blokade zdejmie
	// ktos inny albo przejmie ja finisher (othersBusy), juz jej nie dotykamy. Nowy gest w trakcie - wydluza do dluzszego.
	enum StillAction { STILL_NONE = 0, STILL_LOCK, STILL_UNLOCK };
	struct StillHold {
		bool active = false;
		bool owned = false;      // blokade zalozylismy my
		float left = 0.0f;       // s do konca
		StillAction Start(float seconds, bool lockedNow);
		StillAction Step(float dt, bool lockedNow, bool othersBusy);
		StillAction End(bool lockedNow, bool othersBusy = false);
	};
	// Marionetka: co robi w tym skanie (walka z wrogiem bohatera / atak / do bohatera).
	enum PuppetOrder { PUPPET_KEEP, PUPPET_ATTACK, PUPPET_FOLLOW };
	PuppetOrder PuppetDecide(bool engaged, bool hasTarget, bool following);
	bool PuppetHumanAi(int guild);   // gildie ludzi - ZS_Attack, reszta (orkowie, potwory) - ZS_MM_Attack
	// Szacunek ciosu bronia / potwora (pochloniete przez aktywna tarcze Quen - leczenie): indeks typu z tablicy obrazen
	// (najwieksza czesc) albo z maski typu (bit DAM_x -> indeks), obrazenia = baza + sila - ochrona, co najmniej minimum.
	int DamageMainIndex(const int damage[8], unsigned damageTypeMask);
	int EstimateMeleeDamage(int baseDamage, int strength, int protection, int minDamage);

	// ---------- runda 5 ----------
	// Menu strojenia znakow: pozycja listy najblizsza wartosci z ini (-1 = pusta lista).
	int MenuIndexForReal(const std::vector<float>& values, float v);
	// Punkt celowania (GFA) w poziomie najwyzej maxRange od bohatera - dalej skracany w tym samym kierunku.
	void AimPointInRange(float hx, float hz, float px, float pz, float maxRange, float& outX, float& outZ);

	// ---------- Igni: palenie silnika ----------
	// Silnik (oCNpc::OnDamage_Effects_Start, galaz VOB_BURN) daje palenie na NPC_BURN_TICKS_PER_DAMAGE_POINT (1000 ms)
	// razy skuteczne obrazenia od ognia, po NPC_BURN_DAMAGE_POINTS_PER_INTERVALL na takt - przy Igni 40 pkt byloby 40 s.
	// Komunikat oCMsgDamage (EV_DAMAGE_PER_FRAME) poprawiamy po obrazeniach Igni.
	struct BurnTiming {
		float durationMs = 0.0f;
		float intervalMs = 1000.0f;
		float damagePerInterval = 0.0f;
		bool setRate = false;        // takt i obrazenia na takt z ini (tylko nowe palenie od Igni)
		bool setTotal = false;       // komunikat juz przetworzony (fDamageTotal skopiowane) - poprawic tez je
	};
	// fireProtection: ochrona celu przed ogniem - silnik odejmuje ja od kazdego taktu, wiec takt = obrazenia na sekunde
	// (zaokraglone) + ochrona (runda 5: przy 5-7 na takt i ochronie 35 palenie nie zadawalo nic). -1 = IMMUNE (bez dodatku).
	BurnTiming BurnOverride(float remainingMs, bool isNew, bool activated, float burnTimeS, float damagePerSecond,
		int fireProtection = 0);
	// Ile zycia zabierze takt po odjeciu ochrony (log; IMMUNE = 0).
	int BurnTickEffective(float damagePerInterval, int fireProtection);

	// ---------- Aard / Igni: ognie z efektu (bez oCMobFire) ----------
	// Wiekszosc ogni w swiatach G2 to drzewa vobow z ZEN-a, nie oCMobFire: ognisko = zCVobLight "LIGHT" (dynamiczne,
	// migotanie) z dziecmi "FIRE" (FIRE.PFX / FIRE_MEDIUM.PFX), "SMOKE" (FIRE_SMOKE.PFX) i zCVobSound "SOUND" (FIRE_MEDIUM);
	// pochodnia scienna = mesh OW_MISC_WALL_TORCH_01 z dziecmi FIRE_MEDIUM.PFX, FIRE_SPARKS.PFX, ZFLARE6.TGA (poswiata),
	// zCVobLight, zCVobSound TORCH_BURN; kominek NW_MISC_FIREPLACE_01 z FIRE_HOT.PFX; lampy / swiece - decal
	// FIRE_COMPLETE_A0.TGA; luzne FIRE.PFX pod kompozycja poziomu (obok swiatla czesto statyczne - lightmapa).
	// FLAME - plomien (cel znaku), EXTRA - dym / iskry / poswiata (gaszone tylko w grupie plomienia), EFFECT - czar,
	// bron, palaca sie postac, lawa, menu (nigdy). Rozszerzenie nazwy bez znaczenia; nazwa voba ("FIRE", "SMOKE") pomocniczo.
	enum FireVisKind { FIREVIS_NONE = 0, FIREVIS_FLAME, FIREVIS_EXTRA, FIREVIS_EFFECT };
	FireVisKind FireVisualKind(const std::string& visualName, const std::string& vobName);
	const char* FireVisKindName(int kind);   // "-", "plomien", "dodatek", "efekt" ("?" poza zakresem)
	bool FireSoundName(const std::string& soundName);   // FIRE_MEDIUM, TORCH_BURN, CAMPFIRE_BURN, FIRE_SMALL...

	// Plomien w zasiegu znaku: Aard gasi widoczny, Igni zapala ukryty (takze ukryty przed wczytaniem - plugin go nie pamieta).
	enum FireSkip { FIRESKIP_OK = 0, FIRESKIP_NOT_FIRE, FIRESKIP_EXTRA, FIRESKIP_EFFECT, FIRESKIP_RUNTIME, FIRESKIP_OWNER,
		FIRESKIP_MOBTREE, FIRESKIP_ALREADY, FIRESKIP_CONE, FIRESKIP_BLOCKED, FIRESKIP_COUNT };
	const char* FireSkipName(int reason);
	struct FireFlame {
		int kind = FIREVIS_NONE;  // FireVisKind wizuala
		bool runtime = false;     // dontWriteIntoArchive - vob tymczasowy (efekt czaru, VFX)
		bool owned = false;       // w drzewie postaci / przedmiotu (pochodnia w rece albo na ziemi)
		bool mobTree = false;     // drzewo ognia oCMobFire (gasi i zapala mob)
		bool shown = true;        // showVisual
	};
	int FireFlameCheck(const FireFlame& f, bool ignite);   // FireSkip

	// Czlonek grupy plomienia (dym, poswiata, swiatlo, dzwiek). sameTree = potomek tego samego uchwytu co plomien (rodzic
	// plomienia nie jest kompozycja poziomu) - do 2x promienia; loose = bez uchwytu (rodzic = kompozycja poziomu / korzen
	// swiata) - w promieniu; inny uchwyt (sasiednia pochodnia) - nigdy. Aard bierze aktywne (widoczne / wlaczone /
	// dozwolone), Igni bez pamieci pluginu (po wczytaniu) - nieaktywne.
	enum FireMemberType { FMEM_VISUAL = 0, FMEM_LIGHT, FMEM_SOUND };
	enum FireMemberSkip { FMSKIP_OK = 0, FMSKIP_FAR, FMSKIP_OTHER_HOLDER, FMSKIP_NOT_FIRE, FMSKIP_STATIC_LIGHT, FMSKIP_STATE,
		FMSKIP_OWNER, FMSKIP_COUNT };
	const char* FireMemberSkipName(int reason);
	struct FireMember {
		int type = FMEM_VISUAL;
		float dist = 0.0f;           // od plomienia (3D, cm)
		bool sameTree = false;
		bool loose = false;
		int visKind = FIREVIS_NONE;  // FMEM_VISUAL: FireVisKind
		bool fireSound = false;      // FMEM_SOUND: FireSoundName
		bool staticLight = false;    // FMEM_LIGHT: statyczne (wypalone w lightmapie)
		bool active = true;          // widoczny / swiatlo wlaczone / dzwiek dozwolony
		bool owned = false;          // w drzewie postaci / przedmiotu
		bool runtime = false;        // dontWriteIntoArchive
	};
	int FireMemberCheck(const FireMember& m, float radius, bool ignite);   // FireMemberSkip
	// Indeksy n najmniejszych odleglosci rosnaco (rowne - w kolejnosci wejscia); diagnostyka "co bylo w zasiegu".
	std::vector<int> NearestIndices(const std::vector<float>& dists, size_t n);

	// ---------- runda 6: Aard otwiera zamki (SignAardUnlock) ----------
	// Zasady: NB = jak runa otwierania New Balance (UNION_ABI Mob_SetUnLocked_Rune 0x1016A170: zamkniety i (bez klucza albo
	// z wytrychem) -> SetLocked(0), klucz bez wytrycha -> CantUnlock); Steam (bez runy otwierania) = tylko zamki do wytrycha.
	enum LockRules { LOCKRULES_PICKLOCK = 0, LOCKRULES_NB };
	int LockRulesFor(bool nbUnlockExternal);   // MOB_SETUNLOCKED_RUNE w parserze
	const char* LockRulesName(int rules);      // "wytrych" / "runa NB"
	struct LockInfo {
		bool locked = false;
		bool hasKey = false;        // keyInstance niepusty
		bool hasPickLock = false;   // pickLockStr niepusty
	};
	enum LockVerdict { LOCK_OPEN_PICK = 0, LOCK_OPEN_BARE, LOCK_SKIP_UNLOCKED, LOCK_SKIP_KEY_ONLY, LOCK_SKIP_NO_PICKLOCK,
		LOCK_VERDICT_COUNT };
	int LockDecide(const LockInfo& l, int rules);   // LockVerdict
	bool LockVerdictOpens(int verdict);
	const char* LockVerdictName(int verdict);       // do logu: "zamek do wytrycha", "tylko na klucz", ...

	// ---------- runda 6: Axii poza walka jak Urok (SignAxiiCharm) ----------
	// Spell_Logic_Charm wolana zawsze (mana na zapas, wiec SENDSTOP = odpornosc z listy skryptu - NB: Pyrokar, Xardas,
	// Hagen...; komunikat "To sie nie uda..." daje sam skrypt). Steam: niepamiec jest w Logic, Cast tylko odejmuje mane.
	// NB: niepamiec (przestepstwa, przylapanie na kradziezy VAR_CAUGHT_THIEF, questy) jest w Spell_Cast_Charm.
	struct AxiiCharmPlan {
		bool immune = false;     // odporny na Urok - bez Spell_Cast_Charm, transu i premii kieszonkowej
		bool callCast = false;   // wolac Spell_Cast_Charm (bez kosztu many i bez AI_Wait bohatera)
		bool trance = false;     // trans (runda 5) - gdy AxiiTranceApplies
	};
	// charmOn = SignAxiiCharm; przy wylaczonym zachowanie sprzed rundy 6 (bez sprawdzania odpornosci).
	AxiiCharmPlan PlanAxiiCharm(bool logicFound, int logicRet, int sendStop, bool castFound, bool charmOn, bool tranceApplies);

	// ---------- runda 6: premia kieszonkowa Axii (SignAxiiPickpocketBonus, SignAxiiPickpocketTime) ----------
	// Czas premii: opcja (s) albo przy 0 tyle co trans Axii.
	float ThiefBonusSeconds(float optionSeconds, float tranceSeconds);
	// Premia na NPC (klucz = wskaznik - tylko porownanie, bez wyluskania), eksport do tablic skryptu NB po instancji.
	struct ThiefBonus {
		const void* key;
		int instance;
		int pct;
		float until;
	};
	class ThiefBonusList {
	public:
		int Add(const void* key, int instance, int pct, float until);   // pct <= 0 = usun; wynik: liczba wpisow
		int PctFor(const void* key, float now) const;                   // 0 = brak / wygasla
		int Prune(float now);                                           // usuniete wygasle
		// Sloty skryptu: najdluzej trwajace pierwsze, jedna instancja raz (wieksza premia); reszta zer. Wynik: zajete.
		int Export(int* ids, int* pcts, int n, float now) const;
		void Clear() { items.clear(); }
		int Size() const { return (int)items.size(); }
		const std::vector<ThiefBonus>& Items() const { return items; }
	private:
		std::vector<ThiefBonus> items;
	};
}
