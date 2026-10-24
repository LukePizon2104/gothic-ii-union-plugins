// Logika bez silnika: przelaczanie gildii i sciezek klas New Balance (PathMenu.cpp w pluginie, mapa zmiennych
// i wartosci: [dokument projektowy - niepublikowany], czesci A-D). Zasada: menu zmienia WYLACZNIE zmienne skryptu (znaczniki
// sciezek, zmienne umiejetnosci, stan menu) i pole gildii bohatera - zadnych nadpisan funkcji Daedalusa (regula 21.10:
// funkcje wolane w warunkach rozmow podmienionych przez fake guilds nie moga byc nadpisywane, rozmowy znikaja).
//
// Stan menu trwale w zmiennych skryptu z paczki NB (data_nb\System\Autorun\zDismember_NB_Paths.d) - zapisywane z gra
// w SAVEDAT, wczytanie starego zapisu przywraca stary stan:
//   ZDF_PATHS_VER        wersja ukladu (1)
//   ZDF_PATHS_OFF        bity sciezek wylaczonych w menu (bit = PmPathId: 0 wampir ... 9 truciciel; nakladki
//                        *_isway.d czytaja go przez Hlp_GetIntArrayValue("ZDF_PATHS_OFF", 0): 8 DK, 32 czarnoksieznik,
//                        64 przywolywacz)
//   ZDF_PATH_KEEP[10]    znacznik sprzed wylaczenia (przywracany przy wlaczeniu; 0 = stala magiczna)
//   ZDF_SKILL_OFF[8]     256 bitow umiejetnosci wylaczonych (indeks = PmSkillDef::idx, tylko dopisywane)
//   ZDF_SKILL_KEEP[256]  zapamietane poziomy (i druga zmienna pary pod keep2)
//   ZDF_GUILD_LAST       ostatnia gildia ustawiona z menu (diagnostyka)
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	// ---------- dostep do zmiennych skryptu (plugin: parser; testy: mapa) ----------
	struct PmVars {
		virtual ~PmVars() {}
		virtual int Count(const std::string& name) const = 0;   // liczba elementow zmiennej int, 0 = brak symbolu
		virtual int Get(const std::string& name, int i = 0) const = 0;   // brak = 0
		virtual void Set(const std::string& name, int v, int i = 0) = 0;   // brak = nic
	};
	inline bool PmHas(const PmVars& v, const std::string& n) { return v.Count(n) > 0; }

	extern const char* const PM_V_VER;
	extern const char* const PM_V_OFF;
	extern const char* const PM_V_KEEP;
	extern const char* const PM_V_SKILL_OFF;
	extern const char* const PM_V_SKILL_KEEP;
	extern const char* const PM_V_GUILD_LAST;
	const int PM_VERSION = 1;
	const int PM_SKILL_SLOTS = 256;

	// Symbole wymagane przez menu (NB + paczka zDismember NB). Brak = menu niedostepne (Steam, Kroniki, czysty NB bez
	// pliku .d pluginu) - wtedy brak zakladek i klawisz nic nie robi.
	bool PmAvailable(const PmVars& v, std::string* missing = nullptr);

	// ---------- gildie ----------
	// Gildie, ktore wolno ustawic: 0..8, 12, 14..19 (nigdy 9 BDT, 10 OUT, 11 DMT, 13 PIR, 20 PUBLIC, >= 21 stworzenia).
	bool PmGuildSafe(int g);
	const char* PmGuildName(int g);        // cp1250
	bool PmIsWaterGuild(int g, bool awFlag);   // RX_ISWATERGUILD: 14, 17 albo 16 ze znacznikiem Wojownika Adanosa
	struct PmGuildChoice {
		int guild = 0;
		std::string via;                   // cp1250: tozsamosci ze sladem ("Straznik, Wampir"), "zawsze", "obecna"
	};
	// Zdobyte gildie (slady VALUE_G* != 0, A1 mapy) + NONE + obecna (gdy dozwolona). Kolejnosc wg numeru.
	std::vector<PmGuildChoice> PmGuildChoices(const PmVars& v, int curGuild);
	// Hak bandytow fake guilds (VALUE_GUILD_HOOK == 7012759): zdjety przed zmiana gildii.
	const int PM_GUILD_HOOK_MAGIC = 7012759;
	struct PmGuildPlan {
		bool hookUndone = false;
		int hookTempGuild = 0;             // VALUE_TEMP_HERO_GUILD (do logu)
	};
	// Zapis zmiennych przy zmianie gildii (samo pole gildii bohatera ustawia plugin): hak bandytow zdjety,
	// ZDF_GUILD_LAST = target. VALUE_HERO_GUILD NIE jest ruszane - gildia trwa do nastepnej zmiany zbroi [G].
	PmGuildPlan PmGuildApply(PmVars& v, int target);
	// Ktore sciezki beda dzialac w skryptach przy tej gildii (podpowiedz wiersza gildii, cp1250).
	std::string PmGuildPathsHint(const PmVars& v, int target);

	// ---------- sciezki ----------
	enum PmPathId { PM_VAMP = 0, PM_SCOUT, PM_DH, PM_DK, PM_AW, PM_DARKMAGE, PM_SUMMONER, PM_SHAMAN, PM_DRUID, PM_POISON, PM_PATHS };
	struct PmPathDef {
		int id;
		const char* name;      // cp1250
		const char* flag;      // znacznik NB
		int magic;             // wartosc znacznika "sciezka wlaczona"
		const char* trail;     // slad fake guilds (zdobyta droga, tylko odczyt)
		int scriptGuild;       // gildia wymagana w skryptach: 16 / 19 / 0 (bez gildii)
		const char* hint;      // cp1250 - podpowiedz wiersza
		const char* howTo;     // cp1250 - jak / u kogo zdobyc (rozmowa przyjecia z dekompilacji NB) i wykluczenia nauki
	};
	const PmPathDef& PmPath(int id);
	bool PmIsTplPath(int id);  // wampir, zwiadowca, DH, DK, AW (gildia 16, joint)

	// ---------- klasy gildii (umiejetnosci gildii glownych, mapa B12 / E) ----------
	// Bez znacznika: umiejetnosci dzialaja przy swojej gildii (skrypt / DLL sprawdzaja gildie bohatera; TPL - lata
	// patch_tplskills zdejmuje wymog). Zmiana gildii w zakladce Gildia wlacza / wylacza cala klase; tu - pojedyncze
	// umiejetnosci (te same zasady co w sciezkach: KEEP, bit, ochrona rozmow). Identyfikatory za sciezkami.
	enum PmGuildClassId { PM_GC_KDF = PM_PATHS, PM_GC_KDW, PM_GC_PAL, PM_GC_GUR, PM_GC_TPL, PM_GC_SLD, PM_GROUPS };
	struct PmGuildClassDef {
		int id;
		const char* name;      // cp1250
		int guild1, guild2;    // gildie klasy (guild2 = -1 brak)
		const char* trail1;    // slady zdobycia gildii (fake guilds)
		const char* trail2;
		bool anyGuild;         // dziala w kazdej gildii (lata DLL autora)
		const char* hint;      // cp1250 - gdzie dziala
		const char* howTo;     // cp1250 - nauczyciel, warunek rozmowy
	};
	bool PmIsGuildClass(int id);
	const PmGuildClassDef& PmGuildClass(int id);
	const char* PmGroupName(int id);     // sciezka albo klasa gildii (cp1250)
	const char* PmGroupLogName(int id);  // ASCII do logu: znacznik sciezki albo GIL_*
	// Zdobyta: slad gildii, obecna gildia klasy albo cos wyuczonego / wylaczonego w menu.
	bool PmGuildClassEarned(const PmVars& v, int id, int curGuild);
	bool PmGuildClassActive(int id, int curGuild);
	// Gdzie dziala umiejetnosc klasy gildii poza swoja gildia (mapa E1 / E4, lata patch_guildskills z 22.10):
	// ANY - DLL bez sprawdzenia gildii; PATCH - DLL, gildie zdejmuje patch_guildskills (wykrywana w pamieci UNION_ABI);
	// TPL - patch_tplskills (zainstalowana od 13.10); PART - czesc DLL z lata, czesc skryptowa tylko przy gildii;
	// SCRIPT - skrypt NB sprawdza gildie (funkcje walki / co tick - nie nadpisujemy).
	enum PmGuildGate { PGG_NONE = 0, PGG_ANY, PGG_PATCH, PGG_TPL, PGG_PART, PGG_SCRIPT };
	struct PmSkillDef;
	int PmSkillGuildGate(int idx);
	bool PmGuildClassOwn(int id, int curGuild);     // obecna gildia jest gildia klasy
	// Efekt dziala teraz (PART - tylko czesc DLL): wlasna gildia albo brama wg laty.
	bool PmGuildSkillWorks(const PmSkillDef& d, int curGuild, bool guildPatch);
	// Zasieg klasy (bez zablokowanych): 0 - nic nie dziala, 1 - czesc, 2 - wszystko.
	int PmGuildClassReach(int id, int curGuild, bool guildPatch);
	enum PmPathState { PMS_NONE = 0, PMS_ON, PMS_OFF_MENU, PMS_OFF_GAME };
	struct PmPathView {
		int id = 0;
		PmPathState state = PMS_NONE;
		int flag = 0, trail = 0, keep = 0;
		bool offBit = false;
	};
	// Zdobyta = slad, znacznik, zapamietany znacznik albo bit wylaczenia. Stan: ON (znacznik), OFF_MENU (bit),
	// OFF_GAME (bez bitu, znacznik 0: przepaska Bractwa, przyjecie DH / DK, petla szamana), NONE (niezdobyta).
	PmPathView PmReadPath(const PmVars& v, int id);
	const char* PmPathStateText(PmPathState s);   // cp1250
	const char* PmPathStateLog(PmPathState s);    // ASCII

	// ---------- blokady ----------
	struct PmWorld {
		bool trans = false;        // przemiana (HEROTRANS / HEROTRANSRUNE / HEROTRANSOPEN, model nie-czlowiek)
		bool dialog = false;       // rozmowa, przerywnik (RX_ISCUTSCENEACTIVE)
		bool joint = false;        // JOINTBONUSTYPE != 0 (ochrona templariusza naliczona warunkiem gildia 16 + czysty TPL)
		bool holyLight = false;    // PALLIGHTISON (Swiete Swiatlo - przelaczenie szamana = dryf ochrony przed magia o 50)
		bool bloodGift = false;    // RX_VAMP_BLOODDAR_CURRENTBLOODTYPE != 0 (zmiana typu wampira = +-20 SIL / ZR na stale)
		bool island = false;       // CURRENTLEVEL == ANCIENT_ISLAND_ZEN (47): bez statusu wody RX_PUNISHGAME konczy gre
		bool familiar = false;     // RX_VAMP_SKILLFAMILIAR_ACTIVE (chowaniec przywolany)
		int guild = 0;             // obecna gildia bohatera
	};
	enum PmDeny {
		PMD_OK = 0,
		PMD_TRANS, PMD_DIALOG, PMD_JOINT, PMD_HOLYLIGHT, PMD_BLOODGIFT, PMD_ISLAND_GUILD, PMD_ISLAND_AW,
		PMD_GUILD_FORBIDDEN, PMD_NOT_EARNED, PMD_SHAMAN_DK, PMD_DK_ON, PMD_FAMILIAR, PMD_LOCKED, PMD_NO_VAR, PMD_SAME,
		PMD_COUNT
	};
	const char* PmDenyText(PmDeny d);      // log
	const char* PmDenyScreen(PmDeny d);    // cp1250
	// Zmiana gildii cur -> target. earned = na liscie PmGuildChoices.
	PmDeny PmGuildDeny(const PmWorld& w, const PmVars& v, int target, bool earned);
	PmDeny PmPathDeny(const PmWorld& w, const PmVars& v, int id, bool on);

	// ---------- wynik operacji (do logu) ----------
	struct PmChange {
		std::string var;
		int idx = 0, from = 0, to = 0;
	};
	struct PmResult {
		PmDeny deny = PMD_OK;
		std::vector<PmChange> changes;
		std::vector<std::string> notes;    // ASCII (log); dla gracza osobno
	};
	std::string PmChangesText(const PmResult& r);   // "RX_ISVAMPWAY 7345012->0, ZDF_PATHS_OFF 0->1"

	// Wylaczenie: KEEP = znacznik (gdy != 0), znacznik 0, bit. Wlaczenie: znacznik = KEEP albo stala magiczna, bit
	// zgaszony; Wojownik Adanosa: RX_AW_SKILL_USEOUTOFCOMBAT 0 -> 1 (bez niego umiejetnosci tylko z bronia w reku);
	// szaman: najpierw wylacza mrocznego rycerza (petla NB i tak zeruje go co tick przy szamanie). Typ wampira zostaje
	// (wszystkie sprawdzenia typu w skryptach i DLL wymagaja znacznika). Blokady jak PmPathDeny.
	PmResult PmSetPath(PmVars& v, const PmWorld& w, int id, bool on);

	// ---------- typ wampira ----------
	// Zdobyte typy (slady VALUE_GCVM / GCVF / GCVW) + biezacy RX_VAMPIREWAYTYPE (gdy 1..3). 1 mag krwi, 2 piesciarz,
	// 3 wojownik orezem.
	std::vector<int> PmVampTypes(const PmVars& v);
	const char* PmVampTypeName(int t);     // cp1250
	PmResult PmSetVampType(PmVars& v, const PmWorld& w, int t);

	// ---------- umiejetnosci ----------
	enum PmSkillKind { PSK_TOGGLE = 0, PSK_LOCKED, PSK_GROUP };
	// Druga zmienna: ZERO - zerowana razem (nie przywracana: Duchowa tarcza DH - wartosc liczy tick DLL z poziomu);
	// PAIR - zapamietana pod keep2 i przywracana razem (Aura wladcy nieumarlych: runa + aura 1);
	// COUNT - -1 przy wylaczeniu, +1 przy wlaczeniu (DARKSKILLOWN0 = SKILLCOUNTCURM +1 przy nauce).
	enum PmVar2Mode { PV2_NONE = 0, PV2_ZERO, PV2_PAIR, PV2_COUNT };
	// Blokady umiejetnosci: DK_OFF - tylko przy wylaczonej sciezce DK (tick DLL gasi wtedy runy DK); FAMILIAR - tylko
	// przy odwolanym chowancu.
	enum PmGuard { PG_NONE = 0, PG_DK_OFF, PG_FAMILIAR };
	enum PmSkillFlags {
		PSF_FISTANIM = 1,          // po zmianie: nakladka HUMANS_FISTBLOOD wg warunku NB (RX_APPLYANIM_WEAPONS)
		PSF_DIALOG_TEACHER = 2     // dziala w rozmowach: w ochronie rozmow przywracana tylko u nauczyciela (teacher)
	};
	struct PmSkillDef {
		int idx;                   // -1 dla naglowka grupy
		int path;
		int kind;
		const char* var;
		const char* name;          // cp1250
		const char* var2;
		int var2Mode;
		int keep2;                 // PV2_PAIR: indeks w ZDF_SKILL_KEEP na druga zmienna
		int guard;
		int flags;
		const char* teacher;       // PSF_DIALOG_TEACHER: instancja nauczyciela (wielkie litery)
	};
	const std::vector<PmSkillDef>& PmSkills();
	const PmSkillDef* PmSkillByIdx(int idx);

	// Opis umiejetnosci do podgladu (Core\PathSkillInfo.cpp - WYGENEROWANE skryptem extract.py z katalogu
	// [notatki lokalne] z dekompilacji NB 18.10): efekt, koszt bazowy (PN / zloto / przedmioty), wymagania,
	// nauczyciel. Koszt w grze zalezy od trybu (RX_FMODE, Arena Koszmaru x0,7, FullSNC 0) - plugin wola wtedy te same
	// funkcje kosztu NB: lpFunc(stala lpConst, poziom) (lpLevelArg -1 = poziom nauki, inaczej ta liczba).
	struct PmSkillInfo {
		int idx;
		int levels;                // liczba poziomow nauki
		const char* effect;        // cp1250
		const char* cost;          // cp1250, koszt bazowy
		const char* req;           // cp1250, wymagania ("" = brak znanych)
		const char* teacher;       // cp1250
		const char* lpFunc;        // nullptr = koszt staly (z cost)
		const char* lpConst;
		int lpLevelArg;
		const char* goldFunc;
		const char* goldConst;
		int goldLevelArg;
	};
	const PmSkillInfo* PmSkillInfoFor(int idx);
	int PmSkillInfoCount();
	// Tekst kosztu z wartosci na poziomy (kolejne poziomy od 'from'): "3 / 6 PN, 1000 / 2000 zl" (cp1250).
	std::string PmCostText(const std::vector<int>& lp, const std::vector<int>& gold);
	struct PmSkillView {
		const PmSkillDef* def = nullptr;
		int value = 0, keep = 0;
		bool off = false;
		bool learned = false;      // wartosc > 0, zapamietana albo wylaczona
	};
	PmSkillView PmReadSkill(const PmVars& v, const PmSkillDef& d);
	PmDeny PmSkillDeny(const PmWorld& w, const PmVars& v, const PmSkillDef& d, bool on);
	// Wylaczenie: KEEP = max(KEEP, wartosc), zmienna 0, bit (+ druga zmienna wg trybu). Wlaczenie: zmienna =
	// max(biezaca, KEEP) (gracz mogl kupic ponownie), bit zgaszony.
	PmResult PmSetSkill(PmVars& v, const PmWorld& w, const PmSkillDef& d, bool on);

	// ---------- pilnowanie i ochrona rozmow ----------
	enum PmEventKind {
		PME_PATH_FORCED_OFF = 0,   // sciezka wylaczona w menu, a znacznik != 0 (zbroja / szata fake guilds, rozmowa) - zerowany
		PME_PATH_GAME_OFF,         // sciezka wlaczona, znacznik spadl do 0 (przepaska, przyjecie DH / DK, petla szamana)
		PME_PATH_GAME_ON,          // znacznik wrocil bez menu (zbroja)
		PME_SKILL_REZERO,          // wylaczona umiejetnosc > 0 poza rozmowa - kupiona / ustawiona ponownie, zerowana
		PME_SKILL_RESTORED,        // ochrona rozmow: przywrocona na czas rozmowy
		PME_SKILL_BOUGHT,          // po rozmowie wartosc wyzsza niz zapamietana (nauka w rozmowie)
	};
	struct PmEvent {
		int kind = 0;
		int id = 0;                // sciezka albo idx umiejetnosci
		int from = 0, to = 0;
		std::string var;
	};
	struct PmWatchMem {
		bool init = false;
		int lastFlag[PM_PATHS] = {};
	};
	// Co 250 ms (poza rozmowa chroniona - dialogRestored): sciezki z bitem i znacznikiem != 0 -> KEEP, 0; zmiany
	// znacznikow bez menu do logu; wylaczone umiejetnosci > 0 -> KEEP = max, 0.
	void PmWatch(PmVars& v, PmWatchMem& mem, bool dialogRestored, std::vector<PmEvent>& out);
	// Ochrona rozmow: na czas rozmowy wylaczone umiejetnosci dostaja zapamietane wartosci (nauczyciel nie zaproponuje
	// ponownego zakupu). Znaczniki sciezek zostaja (wylaczona sciezka nadal odblokowuje nauke innych sciezek).
	// npc = instancja rozmowcy (wielkie litery) - umiejetnosci PSF_DIALOG_TEACHER tylko u swojego nauczyciela.
	int PmDialogRestore(PmVars& v, const std::string& npc, std::vector<PmEvent>& out);
	// Koniec rozmowy: wylaczone umiejetnosci znow 0, KEEP = max (nauka w rozmowie podnosi zapamietany poziom).
	int PmDialogReapply(PmVars& v, std::vector<PmEvent>& out);

	// Nakladka piesciarza NB (RX_APPLYANIM_WEAPONS: RX_ISVAMPIRE() && typ 2 && RX_VAMP_SKILLFIST_NEWOVERLAY).
	bool PmFistOverlayWanted(const PmVars& v, int guild);

	// Stan do konsoli / logu (ASCII).
	std::string PmSummary(const PmVars& v, int guild);
}
