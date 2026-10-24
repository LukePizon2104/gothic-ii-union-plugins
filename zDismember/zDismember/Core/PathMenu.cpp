#include "PathMenu.h"
#include <algorithm>
#include <cstdio>

namespace zdcore {
	const char* const PM_V_VER = "ZDF_PATHS_VER";
	const char* const PM_V_OFF = "ZDF_PATHS_OFF";
	const char* const PM_V_KEEP = "ZDF_PATH_KEEP";
	const char* const PM_V_SKILL_OFF = "ZDF_SKILL_OFF";
	const char* const PM_V_SKILL_KEEP = "ZDF_SKILL_KEEP";
	const char* const PM_V_GUILD_LAST = "ZDF_GUILD_LAST";

	static const char* const V_AW_OUTOFCOMBAT = "RX_AW_SKILL_USEOUTOFCOMBAT";
	static const char* const V_VAMP_TYPE = "RX_VAMPIREWAYTYPE";
	static const char* const V_GUILD_HOOK = "VALUE_GUILD_HOOK";
	static const char* const V_TEMP_GUILD = "VALUE_TEMP_HERO_GUILD";

	bool PmAvailable(const PmVars& v, std::string* missing) {
		struct Need { const char* n; int count; };
		static const Need need[] = { { PM_V_OFF, 1 }, { PM_V_KEEP, PM_PATHS }, { PM_V_SKILL_OFF, PM_SKILL_SLOTS / 32 },
			{ PM_V_SKILL_KEEP, PM_SKILL_SLOTS }, { "RX_ISVAMPWAY", 1 } };
		std::string miss;
		for (const Need& n : need)
			if (v.Count(n.n) < n.count)
				miss += (miss.empty() ? "" : ", ") + std::string(n.n);
		if (missing)
			*missing = miss;
		return miss.empty();
	}

	// ---------- gildie ----------

	bool PmGuildSafe(int g) {
		return g >= 0 && g <= 19 && g != 9 && g != 10 && g != 11 && g != 13;
	}

	const char* PmGuildName(int g) {
		switch (g) {
		case 0: return "Bez gildii";
		case 1: return "Paladyn";
		case 2: return "Stra\xBF Miejska";
		case 3: return "Obywatel";
		case 4: return "Mag Ognia";
		case 5: return "Adept Ognia";
		case 6: return "Pogromca Smok\xF3w";
		case 7: return "Najemnik";
		case 8: return "Ch\xB3op";
		case 9: return "Bandyta";
		case 12: return "Bractwo (Sekciarz)";
		case 13: return "Pirat";
		case 14: return "Mag Wody";
		case 15: return "Guru";
		case 16: return "Stra\xBFnik \x8Cwi\xB9tynny";
		case 17: return "Adept Wody";
		case 18: return "Adept Mroku";
		case 19: return "Mag Mroku";
		default: return "inna gildia";
		}
	}

	bool PmIsWaterGuild(int g, bool awFlag) {
		return g == 14 || g == 17 || (g == 16 && awFlag);
	}

	struct GuildTrail {
		int guild;
		const char* trail;
		const char* who;   // cp1250
	};
	static const GuildTrail GUILD_TRAILS[] = {
		{ 1, "VALUE_GPAL", "Paladyn" }, { 2, "VALUE_GMIL", "Stra\xBF Miejska" }, { 4, "VALUE_GFMM", "Mag Ognia" },
		{ 5, "VALUE_GFMN", "Adept Ognia" }, { 6, "VALUE_GDGH", "Pogromca Smok\xF3w" }, { 7, "VALUE_GMRC", "Najemnik" },
		{ 12, "VALUE_GSEC", "Sekciarz" }, { 14, "VALUE_GWMM", "Mag Wody" }, { 15, "VALUE_GGUR", "Guru" },
		{ 16, "VALUE_GTPL", "Stra\xBFnik" }, { 16, "VALUE_GCVN", "Wampir" }, { 16, "VALUE_GSCT", "Zwiadowca" },
		{ 16, "VALUE_GDHN", "\xA3owca demon\xF3w" }, { 16, "VALUE_GDWN", "Mroczny rycerz" }, { 16, "VALUE_GWOA", "Wojownik Adanosa" },
		{ 17, "VALUE_GWMN", "Adept Wody" }, { 18, "VALUE_GDMN", "Adept Mroku" }, { 19, "VALUE_GDMM", "Czarnoksi\xEA\xBFnik" },
		{ 19, "VALUE_GDNS", "Przywo\xB3ywacz" }, { 19, "VALUE_GDDS", "Demonolog" },
	};
	// Nizszy stopien zdobyty razem z wyzszym (NDM bez sladu - druga rozmowa NB; MIL przed PAL, NOV przed KDF ...).
	struct GuildImplied {
		int higher, lower;
	};
	static const GuildImplied GUILD_IMPLIED[] = { { 1, 2 }, { 4, 5 }, { 14, 17 }, { 19, 18 }, { 15, 12 } };

	std::vector<PmGuildChoice> PmGuildChoices(const PmVars& v, int curGuild) {
		std::vector<std::string> via(20);
		auto add = [&](int g, const std::string& who) {
			if (g < 0 || g >= 20)
				return;
			if (via[g].find(who) == std::string::npos)
				via[g] += (via[g].empty() ? "" : ", ") + who;
		};
		for (const GuildTrail& t : GUILD_TRAILS)
			if (v.Get(t.trail) != 0)
				add(t.guild, t.who);
		// znaczniki sciezek bez sladu (NB bez fake guilds, zbroja)
		for (int p = 0; p < PM_PATHS; p++) {
			const PmPathDef& d = PmPath(p);
			if (d.scriptGuild && v.Get(d.flag) != 0 && v.Get(d.trail) == 0)
				add(d.scriptGuild, d.name);
		}
		for (const GuildImplied& i : GUILD_IMPLIED)
			if (!via[i.higher].empty() && via[i.lower].empty())
				add(i.lower, "razem z wy\xBFszym stopniem");
		add(0, "zawsze");
		if (PmGuildSafe(curGuild) && via[curGuild].empty())
			add(curGuild, "obecna");
		std::vector<PmGuildChoice> out;
		for (int g = 0; g < 20; g++)
			if (!via[g].empty() && PmGuildSafe(g)) {
				PmGuildChoice c;
				c.guild = g;
				c.via = via[g];
				out.push_back(c);
			}
		return out;
	}

	PmGuildPlan PmGuildApply(PmVars& v, int target) {
		PmGuildPlan p;
		if (v.Get(V_GUILD_HOOK) == PM_GUILD_HOOK_MAGIC) {
			p.hookUndone = true;
			p.hookTempGuild = v.Get(V_TEMP_GUILD);
			v.Set(V_GUILD_HOOK, 0);
		}
		if (v.Get(PM_V_VER) != PM_VERSION)
			v.Set(PM_V_VER, PM_VERSION);
		v.Set(PM_V_GUILD_LAST, target);
		return p;
	}

	std::string PmGuildPathsHint(const PmVars& v, int target) {
		auto list = [&](bool (*pick)(int)) {
			std::string s;
			for (int p = 0; p < PM_PATHS; p++)
				if (pick(p) && PmReadPath(v, p).state == PMS_ON)
					s += (s.empty() ? "" : ", ") + std::string(PmPath(p).name);
			return s;
		};
		if (target == 16) {
			std::string s = list([](int p) { return PmIsTplPath(p); });
			return s.empty() ? "Przy 16: czysty templariusz (bez \x9C" "cie\xBF" "ek, premie Stra\xBFnika)" : "Przy 16 dzia\xB3" "aj\xB9 w skryptach: " + s;
		}
		if (target == 19) {
			std::string s = list([](int p) { return p == PM_DARKMAGE || p == PM_SUMMONER; });
			return s.empty() ? "Przy 19: Demonolog (bez Czarnoksi\xEA\xBFnika i Przywo\xB3ywacza)" : "Przy 19 dzia\xB3" "aj\xB9 w skryptach: " + s;
		}
		return "\x8C" "cie\xBFki gildii 16 i 19 nieaktywne w skryptach; szaman, druid, truciciel i DLL \x9C" "cie\xBF" "ek KDM bez zmian";
	}

	// ---------- sciezki ----------

	// howTo: rozmowa przyjecia z dekompilacji NB 18.10 (funkcja ustawiajaca znacznik -> C_INFO -> NPC, skrypt
	// [notatki lokalne]) i blokady nauki RX_CANLEARN* (mapa B0).
	static const PmPathDef PATHS[PM_PATHS] = {
		{ PM_VAMP, "Wampir (Pot\xEApiony)", "RX_ISVAMPWAY", 7345012, "VALUE_GCVN", 16,
			"Gildia 16: premie i kary wampira (s\xB3o\xF1" "ce, pragnienie, ogie\xF1) w skryptach i DLL",
			"Zdobycie: Remus - wypicie jego krwi (rozmowa przy zamku). Nauka zablokowana przy: mroczny rycerz, \xB3owca "
			"demon\xF3w, Wojownik Adanosa, zwiadowca (przy gildii 16), truciciel, przywo\xB3ywacz; szaman, druid i truciciel "
			"zamykaj\xB9 rozmow\xEA z Remusem" },
		{ PM_SCOUT, "Zwiadowca", "RX_ISSCOUTWAY", 9980312, "VALUE_GSCT", 16,
			"Skrypty przy 16, DLL sam znacznik. Wy\xB3\xB9" "czona: \xB3up ze szkieletu przy sferze przepada",
			"Zdobycie: Allen - \"(Zosta\xF1 zwiadowc\xB9)\", rozdzia\xB3 1, bez gildii. Nauka zablokowana przy: szaman" },
		{ PM_DH, "\xA3owca demon\xF3w", "RX_ISDHWAY", 8891273, "VALUE_GDHN", 16,
			"Gildia 16: skrypty i DLL. Wy\xB3\xB9" "czona: \xB3up Ituselda i Korgaloma przepada",
			"Zdobycie: Angel (\xB3owcy demon\xF3w) - \"Zrobi\xB3" "em wszystko, o co mnie prosi\xB3" "e\x9C.\", rozdzia\xB3 1. Przyj\xEA" "cie zeruje "
			"mrocznego rycerza i Czarnoksi\xEA\xBFnika. Nauka zablokowana przy: szaman" },
		{ PM_DK, "Mroczny rycerz", "RX_ISDARKWAYWARRIOR", 98044518, "VALUE_GDWN", 16,
			"Gildia 16: skrypty i DLL. Wyklucza szamana (p\xEAtla NB zeruje DK przy szamanie)",
			"Zdobycie: Mroczny rycerz (DMT_DARKTEACHER) - \"(Zosta\xF1 mrocznym rycerzem)\", bez gildii. Przyj\xEA" "cie zeruje "
			"\xB3owc\xEA demon\xF3w i Czarnoksi\xEA\xBFnika. Nauka zablokowana przy: szaman, truciciel; przy szamanie p\xEAtla NB zeruje DK" },
		{ PM_AW, "Wojownik Adanosa", "RX_ISADANOSWARRIORWAY", 987412, "VALUE_GWOA", 16,
			"Skrypty przy 16, DLL sam znacznik. Wy\xB3\xB9" "czony: tabliczki oddane Nefariusowi przepadaj\xB9",
			"Zdobycie: Nefarius - \"Chc\xEA zosta\xE6 twoim uczniem.\", bez gildii. Nauka zablokowana przy: mroczny rycerz, "
			"wampir, \xB3owca demon\xF3w, zwiadowca (przy gildii 16), truciciel, przywo\xB3ywacz, druid, szaman" },
		{ PM_DARKMAGE, "Czarnoksi\xEA\xBFnik", "RX_ISNTWOWAY", 17889123, "VALUE_GDMM", 19,
			"Skrypty przy 19; DLL w ka\xBF" "dej gildii (odporno\x9C\xE6 na kl\xB9twy, mroczna gildia)",
			"Zdobycie: Xardas - \"Chc\xEA pod\xB9\xBF" "a\xE6 \x9C" "cie\xBFk\xB9 Czarnoksi\xEA\xBFnika!\" (Adept Mroku, zadanie na Maga Mroku w toku). "
			"Przyj\xEA" "cie do \xB3owc\xF3w demon\xF3w lub mrocznych rycerzy zeruje t\xEA \x9C" "cie\xBFk\xEA" },
		{ PM_SUMMONER, "Mroczny przywo\xB3ywacz", "RX_NECROSUMMONWAY", 179935, "VALUE_GDNS", 19,
			"DLL w ka\xBF" "dej gildii: kara x0,25 obra\xBF" "e\xF1 run, limit 4 przywo\xB3" "a\xF1" "c\xF3w. Wy\xB3\xB9" "czenie zdejmuje kar\xEA",
			"Zdobycie: Xardas - \"Chc\xEA w pe\xB3ni odda\xE6 si\xEA mrocznym sztukom! (Mroczny przywo\xB3ywacz)\". Nauka zablokowana "
			"przy: truciciel, szaman" },
		{ PM_SHAMAN, "Szaman", "RX_ISSHAMANWAY", 19871233, "VALUE_GSHM", 0,
			"Bez gildii: runy x0,25, bez przywo\xB3" "a\xF1, totemy, orkowie przyja\x9Fni. Wyklucza DK",
			"Zdobycie: Ur-Azog (szaman ork\xF3w), rozdzia\xB3 1. Nauka zablokowana przy: mroczny rycerz, wampir, \xB3owca "
			"demon\xF3w, Wojownik Adanosa, zwiadowca (przy gildii 16), truciciel, przywo\xB3ywacz" },
		{ PM_DRUID, "Druid", "RX_ISDRUIDWAY", 7789023, "VALUE_GDRD", 0,
			"Bez gildii: przemiany, czary x0,7, limit HP 25 000",
			"Zdobycie: Kormak (druid). Z fake guilds bez blokad nauki" },
		{ PM_POISON, "Truciciel", "RX_POISONMAIN", 1, "VALUE_GAPP", 0,
			"Bez gildii: trucizny na broni; bro\xF1 bia\xB3" "a i dystansowa x0,5",
			"Zdobycie: Mutagen Trucizny od Fregyala (do rozdzia\xB3u 2) albo Szata Odporno\x9C" "ci. Z fake guilds bez blokad; "
			"sam blokuje nauk\xEA wampira, szamana, Wojownika Adanosa, mrocznego rycerza i przywo\xB3ywacza" },
	};

	const PmPathDef& PmPath(int id) {
		return PATHS[std::clamp(id, 0, PM_PATHS - 1)];
	}

	// Nauczyciele i warunki rozmow z dekompilacji NB 18.10 ([notatki lokalne]).
	static const PmGuildClassDef CLASSES[PM_GROUPS - PM_PATHS] = {
		{ PM_GC_KDF, "Mag Ognia", 4, -1, "VALUE_GFMM", nullptr, false,
			"Spalanie z magii ognia (DLL): trzy pierwsze w ka\xBF" "dej gildii, przed\xB3u\xBF" "enie i dodatkowe obra\xBF" "enia - \xB3" "atka patch_guildskills",
			"Parlan - \"(Umiej\xEAtno\x9C" "ci magii Ognia)\": gildia Mag Ognia, kr\xB9g magii co najmniej 1, po rozmowie wst\xEApnej" },
		{ PM_GC_KDW, "Mag Wody", 14, -1, "VALUE_GWMM", nullptr, false,
			"Odnowa many (DLL) - \xB3" "atka: ka\xBF" "da gildia; regeneracja many i eliksiry - skrypt, tylko gildia Mag Wody",
			"Nefarius (Jarkendar) - \"(Umiej\xEAtno\x9C" "ci maga wody)\": gildia Mag Wody, po nauce run u Nefariusa" },
		{ PM_GC_PAL, "Paladyn", 1, -1, "VALUE_GPAL", nullptr, false,
			"Szybkie leczenie (\xB3" "atka) i Magia bitewna - ka\xBF" "da gildia; obra\xBF" "enia przeciw orkom - skrypt, tylko gildia Paladyn",
			"Albrecht - \"(Nauka umiej\xEAtno\x9C" "ci specjalnych)\": gildia Paladyn, po nauce many u Albrechta" },
		{ PM_GC_GUR, "Guru", 15, -1, "VALUE_GGUR", nullptr, false,
			"Umys\xB3 Guru (\xB3" "atka) i Spok\xF3j Guru (\xB3" "atka + guildskills_isway.d) - ka\xBF" "da gildia",
			"Baal Namib - \"(Nauka specjalnych umiej\xEAtno\x9C" "ci)\": gildia Guru, po nauce run" },
		{ PM_GC_TPL, "Stra\xBFnik \x8Cwi\xB9tynny", 16, -1, "VALUE_GTPL", nullptr, true,
			"DLL; \xB3" "atka patch_tplskills: w ka\xBF" "dej gildii i przy ka\xBF" "dej \x9C" "cie\xBF" "ce",
			"Gor Na Toth - \"(Nauka umiej\xEAtno\x9C" "ci specjalnych)\": gildia 16 i czysty templariusz (opcjonalna nak\xB3" "adka tplteachers_isway.d "
			"zdejmuje wym\xF3g \x9C" "cie\xBF" "ek)" },
		{ PM_GC_SLD, "Najemnik / \xA3owca smok\xF3w", 7, 6, "VALUE_GMRC", "VALUE_GDGH", false,
			"G\xB3\xEA" "bokie rany i Techniki obronne (\xB3" "atka) - ka\xBF" "da gildia; Szcz\xEA\x9C" "cie najemnika i Skuteczne mikstury - skrypt, tylko Najemnik / \xA3owca smok\xF3w",
			"Torlof - \"Chc\xEA zwi\xEAkszy\xE6 moje umiej\xEAtno\x9C" "ci.\": gildia Najemnik albo \xA3owca smok\xF3w (te\xBF na statku)" },
	};

	bool PmIsGuildClass(int id) {
		return id >= PM_PATHS && id < PM_GROUPS;
	}

	const PmGuildClassDef& PmGuildClass(int id) {
		return CLASSES[std::clamp(id - PM_PATHS, 0, PM_GROUPS - PM_PATHS - 1)];
	}

	const char* PmGroupName(int id) {
		return PmIsGuildClass(id) ? PmGuildClass(id).name : PmPath(id).name;
	}

	const char* PmGroupLogName(int id) {
		static const char* const gil[PM_GROUPS - PM_PATHS] = { "GIL_KDF", "GIL_KDW", "GIL_PAL", "GIL_GUR", "GIL_TPL", "GIL_SLD/DJG" };
		return PmIsGuildClass(id) ? gil[id - PM_PATHS] : PmPath(id).flag;
	}

	bool PmGuildClassActive(int id, int curGuild) {
		const PmGuildClassDef& c = PmGuildClass(id);
		return c.anyGuild || curGuild == c.guild1 || (c.guild2 >= 0 && curGuild == c.guild2);
	}

	bool PmGuildClassOwn(int id, int curGuild) {
		if (!PmIsGuildClass(id))
			return false;
		const PmGuildClassDef& c = PmGuildClass(id);
		return curGuild == c.guild1 || (c.guild2 >= 0 && curGuild == c.guild2);
	}

	// Sprawdzone w DECOMPILED.d i fix.dll 18.10 ([notatki lokalne], abi\dis.py).
	int PmSkillGuildGate(int idx) {
		switch (idx) {
		case 233: case 234: case 235:          // DOT1 / DOT2 / DOTPLUS1: 'cmp eax,4' to GetEngineVersion, nie gildia
		case 240:                              // Magia bitewna: przeliczanie SPL_DAMAGE_* bez gildii
			return PGG_ANY;
		case 236: case 237:                    // DOTREPEAT / DOTNEWDAMAGE (0x1014A4F8)
		case 244: case 241:                    // odnowa many Maga Wody / Guru (0x1024B7D9)
		case 239:                              // szybkie leczenie (0x10242202)
		case 247: case 248:                    // rany / obrona najemnika (0x1004FEB2, 0x1035FBA7)
		case 243:                              // Spokoj Guru: DLL 0x100EBBA5 z lata; skryptowe RX_APPLYCURSE (GIL_GUR)
			return PGG_PATCH;                  // nadpisuje guildskills_isway.d (NB 22.10) - dawniej PGG_PART
		case 42: case 43:
			return PGG_TPL;
		case 238: case 249:                    // RX_WEAPONDAMAGE
		case 245:                              // RX_REGENCOMMONFUNC
		case 246: case 41:                     // HERO_POISONED
			return PGG_SCRIPT;
		}
		return PGG_NONE;
	}

	bool PmGuildSkillWorks(const PmSkillDef& d, int curGuild, bool guildPatch) {
		if (!PmIsGuildClass(d.path))
			return true;
		if (PmGuildClassOwn(d.path, curGuild))
			return true;
		switch (PmSkillGuildGate(d.idx)) {
		case PGG_ANY:
		case PGG_TPL:
			return true;
		case PGG_PATCH:
		case PGG_PART:
			return guildPatch;
		}
		return false;
	}

	int PmGuildClassReach(int id, int curGuild, bool guildPatch) {
		if (!PmIsGuildClass(id))
			return 0;
		if (PmGuildClassOwn(id, curGuild))
			return 2;
		int all = 0, works = 0, part = 0;
		for (const PmSkillDef& d : PmSkills()) {
			if (d.path != id || d.kind == PSK_GROUP || d.kind == PSK_LOCKED)
				continue;
			all++;
			if (PmGuildSkillWorks(d, curGuild, guildPatch)) {
				works++;
				if (PmSkillGuildGate(d.idx) == PGG_PART)
					part++;
			}
		}
		if (!works)
			return 0;
		return works == all && !part ? 2 : 1;
	}

	bool PmIsTplPath(int id) {
		return id == PM_VAMP || id == PM_SCOUT || id == PM_DH || id == PM_DK || id == PM_AW;
	}

	static bool PathBit(const PmVars& v, int id) {
		return ((unsigned)v.Get(PM_V_OFF) >> id) & 1u;
	}

	PmPathView PmReadPath(const PmVars& v, int id) {
		const PmPathDef& d = PmPath(id);
		PmPathView p;
		p.id = id;
		p.flag = v.Get(d.flag);
		p.trail = v.Get(d.trail);
		p.keep = v.Get(PM_V_KEEP, id);
		p.offBit = PathBit(v, id);
		if (!PmHas(v, d.flag))
			p.state = PMS_NONE;
		else if (p.offBit)
			p.state = PMS_OFF_MENU;
		else if (p.flag != 0)
			p.state = PMS_ON;
		else if (p.trail != 0 || p.keep != 0)
			p.state = PMS_OFF_GAME;
		else
			p.state = PMS_NONE;
		return p;
	}

	const char* PmPathStateText(PmPathState s) {
		switch (s) {
		case PMS_ON: return "W\xB3\xB9" "czona";
		case PMS_OFF_MENU: return "Wy\xB3\xB9" "czona";
		case PMS_OFF_GAME: return "Wy\xB3. przez gr\xEA";
		default: return "nie zdobyta";
		}
	}

	const char* PmPathStateLog(PmPathState s) {
		switch (s) {
		case PMS_ON: return "wlaczona";
		case PMS_OFF_MENU: return "wylaczona w menu";
		case PMS_OFF_GAME: return "wylaczona przez gre";
		default: return "nie zdobyta";
		}
	}

	// ---------- blokady ----------

	const char* PmDenyText(PmDeny d) {
		switch (d) {
		case PMD_OK: return "ok";
		case PMD_TRANS: return "przemiana (HEROTRANS / HEROTRANSRUNE / HEROTRANSOPEN albo model nie-czlowiek)";
		case PMD_DIALOG: return "rozmowa / przerywnik";
		case PMD_JOINT: return "joint (JOINTBONUSTYPE != 0 - ochrona templariusza)";
		case PMD_HOLYLIGHT: return "Swiete Swiatlo (PALLIGHTISON) - dryf ochrony przed magia";
		case PMD_BLOODGIFT: return "dar krwi Beliara (RX_VAMP_BLOODDAR_CURRENTBLOODTYPE != 0) - +-20 SIL/ZR na stale";
		case PMD_ISLAND_GUILD: return "Wyspa Starozytnych - gildia bez statusu wody (RX_PUNISHGAME)";
		case PMD_ISLAND_AW: return "Wyspa Starozytnych - Wojownik Adanosa daje status wody (RX_PUNISHGAME)";
		case PMD_GUILD_FORBIDDEN: return "gildia zakazana (9, 10, 11, 13, >= 20)";
		case PMD_NOT_EARNED: return "niezdobyta";
		case PMD_SHAMAN_DK: return "znacznik szamana ustawiony - petla NB zeruje DK co tick";
		case PMD_DK_ON: return "sciezka DK wlaczona - umiejetnosci DK tylko przy zgaszonych runach";
		case PMD_FAMILIAR: return "chowaniec przywolany";
		case PMD_LOCKED: return "trwala premia z nauki";
		case PMD_NO_VAR: return "brak zmiennej w skryptach";
		case PMD_SAME: return "bez zmian";
		default: return "?";
		}
	}

	const char* PmDenyScreen(PmDeny d) {
		switch (d) {
		case PMD_TRANS: return "Nie w trakcie przemiany";
		case PMD_DIALOG: return "Nie w trakcie rozmowy ani przerywnika";
		case PMD_JOINT: return "Dzia\xB3" "a joint - poczekaj, a\xBF minie (ochrona templariusza)";
		case PMD_HOLYLIGHT: return "Dzia\xB3" "a \x8Cwi\xEAte \x8Cwiat\xB3o - prze\xB3\xB9" "czenie szamana zmieni\xB3oby ochron\xEA przed magi\xB9 na sta\xB3" "e";
		case PMD_BLOODGIFT: return "Trwa dar krwi Beliara - zmiana typu da\xB3" "aby si\xB3\xEA / zr\xEA" "czno\x9C\xE6 na sta\xB3" "e";
		case PMD_ISLAND_GUILD: return "Wyspa Staro\xBFytnych: tylko gildia wody (Mag / Adept Wody, Stra\xBFnik z Wojownikiem Adanosa)";
		case PMD_ISLAND_AW: return "Wyspa Staro\xBFytnych: bez Wojownika Adanosa gra si\xEA zako\xF1" "czy";
		case PMD_GUILD_FORBIDDEN: return "Tej gildii nie mo\xBFna ustawi\xE6";
		case PMD_NOT_EARNED: return "Nie zdobyto";
		case PMD_SHAMAN_DK: return "Najpierw wy\xB3\xB9" "cz szamana (p\xEAtla NB zeruje mrocznego rycerza)";
		case PMD_DK_ON: return "Najpierw wy\xB3\xB9" "cz \x9C" "cie\xBFk\xEA Mrocznego rycerza (runy DK gasn\xB9)";
		case PMD_FAMILIAR: return "Najpierw odwo\xB3" "aj chowa\xF1" "ca";
		case PMD_LOCKED: return "Trwa\xB3" "a premia z nauki - bez prze\xB3\xB9" "czania";
		case PMD_NO_VAR: return "Brak zmiennej w skryptach";
		default: return nullptr;
		}
	}

	PmDeny PmGuildDeny(const PmWorld& w, const PmVars& v, int target, bool earned) {
		if (w.trans)
			return PMD_TRANS;
		if (w.dialog)
			return PMD_DIALOG;
		if (!PmGuildSafe(target))
			return PMD_GUILD_FORBIDDEN;
		if (target == w.guild)
			return PMD_SAME;
		if (!earned)
			return PMD_NOT_EARNED;
		if (w.joint && (w.guild == 16 || target == 16))
			return PMD_JOINT;
		bool aw = v.Get(PmPath(PM_AW).flag) == PmPath(PM_AW).magic;
		if (w.island && !PmIsWaterGuild(target, aw))
			return PMD_ISLAND_GUILD;
		return PMD_OK;
	}

	PmDeny PmPathDeny(const PmWorld& w, const PmVars& v, int id, bool on) {
		const PmPathDef& d = PmPath(id);
		if (!PmHas(v, d.flag))
			return PMD_NO_VAR;
		PmPathView p = PmReadPath(v, id);
		if (p.state == PMS_NONE)
			return PMD_NOT_EARNED;
		if (on ? p.state == PMS_ON : p.offBit)
			return PMD_SAME;
		if (w.trans)
			return PMD_TRANS;
		if (w.dialog)
			return PMD_DIALOG;
		bool dkFlag = v.Get(PmPath(PM_DK).flag) != 0;
		if (w.joint && (PmIsTplPath(id) || (on && id == PM_SHAMAN && dkFlag)))
			return PMD_JOINT;
		if (w.holyLight && id == PM_SHAMAN)
			return PMD_HOLYLIGHT;
		if (w.island && id == PM_AW && !on && w.guild != 14 && w.guild != 17)
			return PMD_ISLAND_AW;
		if (on && id == PM_DK && v.Get(PmPath(PM_SHAMAN).flag) != 0)
			return PMD_SHAMAN_DK;
		return PMD_OK;
	}

	// ---------- zapis z logiem ----------

	static void Write(PmVars& v, PmResult& r, const std::string& name, int val, int i = 0) {
		int from = v.Get(name, i);
		if (from == val)
			return;
		v.Set(name, val, i);
		PmChange c;
		c.var = name;
		c.idx = i;
		c.from = from;
		c.to = v.Get(name, i);
		r.changes.push_back(c);
	}

	static void EnsureVer(PmVars& v, PmResult& r) {
		Write(v, r, PM_V_VER, PM_VERSION);
	}

	std::string PmChangesText(const PmResult& r) {
		std::string s;
		char buf[160];
		for (const PmChange& c : r.changes) {
			if (c.var == PM_V_VER)
				continue;
			if (c.var == PM_V_KEEP || c.var == PM_V_SKILL_OFF || c.var == PM_V_SKILL_KEEP)
				sprintf_s(buf, "%s[%d] %d->%d", c.var.c_str(), c.idx, c.from, c.to);
			else if (c.var == PM_V_OFF)
				sprintf_s(buf, "%s 0x%X->0x%X", c.var.c_str(), (unsigned)c.from, (unsigned)c.to);
			else
				sprintf_s(buf, "%s %d->%d", c.var.c_str(), c.from, c.to);
			s += (s.empty() ? "" : ", ") + std::string(buf);
		}
		return s.empty() ? "bez zmian zmiennych" : s;
	}

	static void PathOff(PmVars& v, PmResult& r, int id) {
		const PmPathDef& d = PmPath(id);
		int f = v.Get(d.flag);
		if (f != 0)
			Write(v, r, PM_V_KEEP, f, id);
		Write(v, r, d.flag, 0);
		Write(v, r, PM_V_OFF, (int)((unsigned)v.Get(PM_V_OFF) | (1u << id)));
	}

	PmResult PmSetPath(PmVars& v, const PmWorld& w, int id, bool on) {
		PmResult r;
		r.deny = PmPathDeny(w, v, id, on);
		if (r.deny != PMD_OK)
			return r;
		EnsureVer(v, r);
		const PmPathDef& d = PmPath(id);
		if (!on) {
			PathOff(v, r, id);
			return r;
		}
		if (id == PM_SHAMAN && v.Get(PmPath(PM_DK).flag) != 0) {
			PathOff(v, r, PM_DK);
			r.notes.push_back("mroczny rycerz wylaczony (szaman zeruje go co tick w petli NB)");
		}
		int keep = v.Get(PM_V_KEEP, id);
		Write(v, r, d.flag, keep != 0 ? keep : d.magic);
		Write(v, r, PM_V_OFF, (int)((unsigned)v.Get(PM_V_OFF) & ~(1u << id)));
		if (id == PM_AW && PmHas(v, V_AW_OUTOFCOMBAT) && v.Get(V_AW_OUTOFCOMBAT) == 0) {
			Write(v, r, V_AW_OUTOFCOMBAT, 1);
			r.notes.push_back("RX_AW_SKILL_USEOUTOFCOMBAT 0 -> 1 (umiejetnosci AW takze bez broni w reku)");
		}
		return r;
	}

	// ---------- typ wampira ----------

	std::vector<int> PmVampTypes(const PmVars& v) {
		static const char* const trails[3] = { "VALUE_GCVM", "VALUE_GCVF", "VALUE_GCVW" };
		std::vector<int> out;
		for (int t = 1; t <= 3; t++)
			if (v.Get(trails[t - 1]) != 0 || v.Get(V_VAMP_TYPE) == t)
				out.push_back(t);
		return out;
	}

	const char* PmVampTypeName(int t) {
		switch (t) {
		case 1: return "Mag krwi";
		case 2: return "Pi\xEA\x9C" "ciarz";
		case 3: return "Wojownik or\xEA\xBF" "em";
		default: return "brak typu";
		}
	}

	PmResult PmSetVampType(PmVars& v, const PmWorld& w, int t) {
		PmResult r;
		std::vector<int> types = PmVampTypes(v);
		if (!PmHas(v, V_VAMP_TYPE))
			r.deny = PMD_NO_VAR;
		else if (std::find(types.begin(), types.end(), t) == types.end())
			r.deny = PMD_NOT_EARNED;
		else if (v.Get(V_VAMP_TYPE) == t)
			r.deny = PMD_SAME;
		else if (w.trans)
			r.deny = PMD_TRANS;
		else if (w.dialog)
			r.deny = PMD_DIALOG;
		else if (w.bloodGift)
			r.deny = PMD_BLOODGIFT;
		if (r.deny != PMD_OK)
			return r;
		EnsureVer(v, r);
		Write(v, r, V_VAMP_TYPE, t);
		return r;
	}

	// ---------- umiejetnosci ----------

	static PmSkillDef Grp(int path, const char* name) {
		return { -1, path, PSK_GROUP, nullptr, name, nullptr, PV2_NONE, -1, PG_NONE, 0, nullptr };
	}
	static PmSkillDef Tg(int idx, int path, const char* var, const char* name) {
		return { idx, path, PSK_TOGGLE, var, name, nullptr, PV2_NONE, -1, PG_NONE, 0, nullptr };
	}
	static PmSkillDef Lk(int idx, int path, const char* var, const char* name) {
		return { idx, path, PSK_LOCKED, var, name, nullptr, PV2_NONE, -1, PG_NONE, 0, nullptr };
	}
	static PmSkillDef With2(PmSkillDef d, const char* var2, int mode, int keep2 = -1) {
		d.var2 = var2;
		d.var2Mode = mode;
		d.keep2 = keep2;
		return d;
	}
	static PmSkillDef Guard(PmSkillDef d, int g) {
		d.guard = g;
		return d;
	}
	static PmSkillDef Flags(PmSkillDef d, int f, const char* teacher = nullptr) {
		d.flags = f;
		d.teacher = teacher;
		return d;
	}

	// Indeksy STALE (zapisane w grze w ZDF_SKILL_OFF / ZDF_SKILL_KEEP) - nowe tylko na koncu wolnych zakresow.
	// Zakresy: wampir 0-49, zwiadowca 50-79, DH 80-89, DK 90-119, AW 120-159, czarnoksieznik 160-164, przywolywacz
	// 165-184, szaman 185-199, druid 200-219, truciciel 220-239, drugie zmienne par 250-255;
	// klasy gildii (E): 233-249 i 41-45.
	static std::vector<PmSkillDef> BuildSkills() {
		std::vector<PmSkillDef> s;
		// C1 wampir
		s.push_back(Grp(PM_VAMP, "Og\xF3lne"));
		s.push_back(Tg(0, PM_VAMP, "RX_VAMP_SKILL_REGEN", "Regeneracja"));
		s.push_back(Tg(1, PM_VAMP, "RX_VAMP_SKILL_EVADE", "Unik"));
		s.push_back(Tg(2, PM_VAMP, "RX_VAMP_SKILL_MAGDEF", "Przekl\xEAty umys\xB3"));
		s.push_back(Tg(3, PM_VAMP, "RX_VAMP_SKILL_STRAFESPEED", "Zwinno\x9C\xE6 bestii"));
		s.push_back(Tg(4, PM_VAMP, "RX_VAMP_SKILL_SPRINT", "Wytrzyma\xB3o\x9C\xE6 bestii (sprint)"));
		s.push_back(Tg(5, PM_VAMP, "RX_VAMP_SKILL_SHADOWSTRAFE", "Cienisty przewr\xF3t"));
		s.push_back(Tg(6, PM_VAMP, "RX_VAMP_SKILL_NOFALLDAMAGE", "Pi\xF3rkospadanie"));
		s.push_back(Lk(7, PM_VAMP, "RX_VAMP_SKILL_CHARM", "Oczarowanie"));
		s.push_back(Lk(8, PM_VAMP, "RX_VAMP_SKILL_PHYSDEF", "Przekl\xEAte cia\xB3o"));
		s.push_back(Lk(9, PM_VAMP, "RX_VAMP_SKILL_SPIRITMANA", "Duch Pot\xEApionych"));
		s.push_back(Lk(10, PM_VAMP, "RX_VAMP_SKILL_ACROBAT", "Zr\xEA" "czno\x9C\xE6 Pot\xEApionych"));
		s.push_back(Grp(PM_VAMP, "Mag krwi"));
		s.push_back(Lk(11, PM_VAMP, "RX_VAMP_SKILLMAGIC_BLOODCIRCLE", "Kr\xB9g magii krwi"));
		s.push_back(Tg(12, PM_VAMP, "RX_VAMP_SKILLMAGIC_FASTSTREAM", "Szybki przep\xB3yw"));
		s.push_back(Tg(13, PM_VAMP, "RX_VAMP_SKILLMAGIC_SUCKMORE", "Przep\xB3yw krwi"));
		s.push_back(Tg(14, PM_VAMP, "RX_VAMP_SKILLMAGIC_SLOWONSUCK", "Spowolnienie \xBFycia"));
		s.push_back(Tg(15, PM_VAMP, "RX_VAMP_SKILLMAGIC_MORERANDDAMAGE", "Strumie\xF1 terroru"));
		s.push_back(Tg(16, PM_VAMP, "RX_VAMP_SKILLMAGIC_TENTACLESMORE", "Wzmocnienie Macek ciemno\x9C" "ci"));
		s.push_back(Tg(17, PM_VAMP, "RX_VAMP_SKILLMAGIC_EVADEONSUCK", "Moc ciemno\x9C" "ci"));
		s.push_back(Tg(18, PM_VAMP, "RX_VAMP_SKILLMAGIC_BLOODSHIELD", "Tarcza krwi"));
		s.push_back(Tg(19, PM_VAMP, "RX_VAMP_SKILLMAGIC_MAGICCLOUD", "Esencja Krwi"));
		s.push_back(Tg(20, PM_VAMP, "RX_VAMP_SKILLMAGIC_MAGICCLOUD_MOREENEMIES", "Esencja Krwi: wi\xEA" "cej wrog\xF3w"));
		s.push_back(Tg(21, PM_VAMP, "RX_VAMP_SKILLMAGIC_MAGICCLOUD_ATTACKSPEED", "Esencja Krwi: szybko\x9C\xE6 ataku"));
		s.push_back(Tg(22, PM_VAMP, "RX_VAMP_SKILLMAGIC_MAGICCLOUD_RADIUS", "Esencja Krwi: zasi\xEAg"));
		s.push_back(Grp(PM_VAMP, "Pi\xEA\x9C" "ciarz"));
		s.push_back(Flags(Tg(23, PM_VAMP, "RX_VAMP_SKILLFIST_NEWOVERLAY", "Styl walki Pot\xEApionych"), PSF_FISTANIM));
		s.push_back(Tg(24, PM_VAMP, "RX_VAMP_SKILLFIST_ANIMALSBLOCK", "Pi\xEA\x9C" "ci \xB3owcy"));
		s.push_back(Tg(25, PM_VAMP, "RX_VAMP_SKILLFIST_FISTSTRIKE", "Cios prosty"));
		s.push_back(Tg(26, PM_VAMP, "RX_VAMP_SKILLFIST_LEGSTRIKE", "Kopni\xEA" "cie"));
		s.push_back(Tg(27, PM_VAMP, "RX_VAMP_SKILLFIST_LEGROUNDAOE", "Kopniak z p\xF3\xB3obrotu"));
		s.push_back(Tg(28, PM_VAMP, "RX_VAMP_SKILLFIST_BLEED", "Krwawe szpony"));
		s.push_back(Tg(29, PM_VAMP, "RX_VAMP_SKILLFIST_BLUNTRESISTATTACKBONUS", "Wiedza o czu\xB3ych punktach"));
		s.push_back(Tg(30, PM_VAMP, "RX_VAMP_SKILLFIST_STAMINARESTORE", "Wytrzyma\xB3o\x9C\xE6 Pot\xEApionych"));
		s.push_back(Tg(31, PM_VAMP, "RX_VAMP_SKILLFIST_STRSCALEBONUS", "Przekl\xEAta si\xB3" "a"));
		s.push_back(Grp(PM_VAMP, "Wojownik or\xEA\xBF" "em"));
		s.push_back(Tg(32, PM_VAMP, "RX_VAMP_SKILLWARRIOR_STEALHEALTH", "Dar krwi"));
		s.push_back(Tg(33, PM_VAMP, "RX_VAMP_SKILLWARRIOR_ATTACKSPEED", "Reakcja bestii"));
		s.push_back(Tg(34, PM_VAMP, "RX_VAMP_SKILLWARRIOR_DARKSWORD", "Mroczne ostrze"));
		s.push_back(Guard(Tg(35, PM_VAMP, "RX_VAMP_SKILLWARRIOR_FAMILIAR", "Chowaniec"), PG_FAMILIAR));
		s.push_back(Guard(Tg(36, PM_VAMP, "RX_VAMP_SKILLWARRIOR_FAMILIARBUFF", "Dar chowa\xF1" "ca"), PG_FAMILIAR));
		s.push_back(Guard(Tg(37, PM_VAMP, "RX_VAMP_SKILLWARRIOR_FAMILIARSAVIOR", "Ostatnia szansa"), PG_FAMILIAR));
		s.push_back(Tg(38, PM_VAMP, "RX_VAMP_SKILLWARRIOR_BETTERHEALTHSTEAL", "Moc krwi"));
		s.push_back(Tg(39, PM_VAMP, "RX_VAMP_SKILLWARRIOR_MANAEXCHANGE", "Emanacja ciemno\x9C" "ci"));
		s.push_back(Tg(40, PM_VAMP, "RX_VAMP_SKILLWARRIOR_ARMORIGNORE", "Przebicie pancerza"));
		// C2 zwiadowca
		s.push_back(Grp(PM_SCOUT, "Umiej\xEAtno\x9C" "ci"));
		s.push_back(Tg(50, PM_SCOUT, "RX_SCOUT_SKILL_HITPLACE", "Znajomo\x9C\xE6 s\xB3" "abych punkt\xF3w"));
		s.push_back(Tg(51, PM_SCOUT, "RX_SCOUT_SKILL_WEAKNESSPROTECT", "Odporno\x9C\xE6 na os\xB3" "abienie"));
		s.push_back(Tg(52, PM_SCOUT, "RX_SCOUT_SKILL_EVADEACTIVE", "Szybka reakcja"));
		s.push_back(Tg(53, PM_SCOUT, "RX_SCOUT_SKILL_SPRINT", "Bieg zwiadowcy"));
		s.push_back(Tg(54, PM_SCOUT, "RX_SCOUT_SKILL_EVADE", "Unik"));
		s.push_back(Tg(55, PM_SCOUT, "RX_SCOUT_SKILL_SNEAKINGBETTER", "Mistrzowskie skradanie si\xEA"));
		s.push_back(Tg(56, PM_SCOUT, "RX_SCOUT_SKILL_SNEAKINGDAMAGE", "Nag\xB3" "e uderzenie"));
		s.push_back(Tg(57, PM_SCOUT, "RX_SCOUT_SKILL_SNEAKINVIS", "Znikni\xEA" "cie"));
		s.push_back(Lk(58, PM_SCOUT, "RX_SCOUT_SKILL_BOWNODIVIATION", "Precyzyjny strza\xB3"));
		s.push_back(Lk(59, PM_SCOUT, "RX_SCOUT_SKILL_SNEAKING", "Skradanie"));
		s.push_back(Lk(60, PM_SCOUT, "RX_SCOUT_SKILL_ACROBAT", "Akrobatyka"));
		s.push_back(Tg(75, PM_SCOUT, "RX_SCOUT_DAMAGEHEALPOTION", "Odwar zwiadowcy"));
		s.push_back(Grp(PM_SCOUT, "Proszki"));
		s.push_back(Tg(61, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_MASTERYLEVEL", "Tworzenie proszk\xF3w"));
		s.push_back(Tg(62, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_FIRE", "Wa\xB3 ognia"));
		s.push_back(Tg(63, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_SMOKE", "Zas\xB3ona dymna"));
		s.push_back(Tg(64, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_ACID", "Truj\xB9" "cy rozpad"));
		s.push_back(Tg(65, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_EXPLOSION", "Eksplozja"));
		s.push_back(Tg(66, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_MAGIC", "Magiczny huk"));
		s.push_back(Tg(67, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TYPE_UNDEAD", "Wyp\xEA" "dzenie z\xB3" "a"));
		s.push_back(Tg(68, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_FASTTHROW", "Szybki rzut"));
		s.push_back(Tg(69, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_RANGE", "Zasi\xEAg rzutu"));
		s.push_back(Tg(70, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TIMELEVEL", "Czas dzia\xB3" "ania"));
		s.push_back(Tg(71, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_RADIUSSKILL", "Promie\xF1"));
		s.push_back(Tg(72, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_BASEDAMAGEUP", "Bazowe obra\xBF" "enia"));
		s.push_back(Tg(73, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_DAMAGELEVEL", "Obra\xBF" "enia"));
		s.push_back(Tg(74, PM_SCOUT, "RX_SCOUT_SKILL_POWDER_TICKSUP", "Obra\xBF" "enia w czasie"));
		// C3 lowca demonow
		s.push_back(Grp(PM_DH, "Umiej\xEAtno\x9C" "ci"));
		s.push_back(With2(Tg(80, PM_DH, "RX_DH_SKILLSHIELDLEVEL", "Duchowa tarcza"), "RX_DH_SKILLSHIELDVALUE", PV2_ZERO));
		s.push_back(Tg(81, PM_DH, "RX_DH_DOUBLESHOTLEVEL", "Podw\xF3jny strza\xB3"));
		s.push_back(Tg(82, PM_DH, "RX_DH_SKILLMELEEDAMAGE", "\x8Cwi\xEAty m\x9C" "ciciel"));
		s.push_back(Tg(83, PM_DH, "RX_DH_SKILLMELEEDOT", "\x8Cwi\xEAty kat"));
		s.push_back(Tg(84, PM_DH, "RX_DH_BETTERKNOWCROSSBOW", "Zaawansowane kusznictwo"));
		s.push_back(Tg(85, PM_DH, "RX_DH_KNOWRESISTSKILL", "Wiedza o \xBFywio\xB3" "ach"));
		s.push_back(Tg(86, PM_DH, "RX_DH_KNOWUNDEADSKILL", "Anatomia nieumar\xB3ych"));
		// C4 mroczny rycerz (wszystko tylko przy zgaszonych runach DK)
		s.push_back(Grp(PM_DK, "Runy i umiej\xEAtno\x9C" "ci"));
		static const char* const dk[9] = { "Przyzwanie mrocznego s\xB3ugi", "Wampiryzm", "Ochrona cia\xB3" "a", "Zaraza", "Ofiara krwi",
			"Mroczny pakt", "Paranoja", "Obj\xEA" "cia mroku", "Ochrona ducha" };
		static const char* const dkVar[9] = { "DARKSKILL0", "DARKSKILL1", "DARKSKILL2", "DARKSKILL3", "DARKSKILL4", "DARKSKILL5",
			"DARKSKILL6", "DARKSKILL7", "DARKSKILL8" };
		for (int i = 0; i < 9; i++)
			s.push_back(Guard(Tg(90 + i, PM_DK, dkVar[i], dk[i]), PG_DK_OFF));
		s.push_back(Grp(PM_DK, "S\xB3uga"));
		static const char* const pet[9] = { "Krzyk ciemno\x9C" "ci", "Fala pustki", "Pr\xEA" "dko\x9C\xE6 s\xB3ugi +10%", "Odnowa \xBFycia przy zabiciu",
			"Ochrona s\xB3ugi +50", "Leczenie bohatera", "Odnowienie s\xB3ugi", "Ochrona s\xB3ugi +50 (druga)", "Regeneracja s\xB3ugi" };
		static const char* const petVar[9] = { "DARKPETSKILL0", "DARKPETSKILL1", "DARKPETSKILL2", "DARKPETSKILL3", "DARKPETSKILL4",
			"DARKPETSKILL5", "DARKPETSKILL6", "DARKPETSKILL7", "DARKPETSKILL8" };
		for (int i = 0; i < 9; i++)
			s.push_back(Guard(Tg(99 + i, PM_DK, petVar[i], pet[i]), PG_DK_OFF));
		s.push_back(Grp(PM_DK, "W\xB3" "asne"));
		s.push_back(Guard(With2(Tg(108, PM_DK, "DARKSKILLOWN0", "Limit aktywnych run +1"), "SKILLCOUNTCURM", PV2_COUNT), PG_DK_OFF));
		s.push_back(Guard(Tg(109, PM_DK, "DARKSKILLOWN1", "Wytrzyma\xB3o\x9C\xE6 przy uderzeniu"), PG_DK_OFF));
		s.push_back(Lk(110, PM_DK, "DARKSKILLOWNPASSIVEPLAGUE", "Zaraza pasywna"));
		s.push_back(Guard(Tg(111, PM_DK, "DARKSKILLOWNINTBONUSPLAGUE", "Zaraza: skalowanie z man\xB9 i INT"), PG_DK_OFF));
		// C5 wojownik Adanosa
		s.push_back(Grp(PM_AW, "Moce"));
		s.push_back(Tg(120, PM_AW, "RX_AW_SKILL_SOULHIT", "Nawa\xB3nica"));
		s.push_back(Tg(121, PM_AW, "RX_AW_SKILL_LIGHTNING", "Uderzenie b\xB3yskawicy"));
		s.push_back(Tg(122, PM_AW, "RX_AW_SKILL_LIGHTNINGTIMEIMPROVE", "Koncentracja magii"));
		s.push_back(Tg(123, PM_AW, "RX_AW_SKILL_LIGHTNINGINSTANT", "Furia \xBFywio\xB3\xF3w"));
		s.push_back(Tg(124, PM_AW, "RX_AW_SKILL_SHIELDS", "Tarcza Staro\xBFytnych"));
		s.push_back(Tg(125, PM_AW, "RX_AW_SKILL_SHIELDSTARGETSMORE", "Tarcza: wi\xEA" "cej cel\xF3w"));
		s.push_back(Tg(126, PM_AW, "RX_AW_SKILL_SHIELDSDAMAGEUP", "Tarcza: obra\xBF" "enia"));
		s.push_back(Tg(127, PM_AW, "RX_AW_SKILL_SHIELDSSPEED", "Tarcza: pr\xEA" "dko\x9C\xE6"));
		s.push_back(Tg(128, PM_AW, "RX_AW_SKILL_SHIELDSTIMEIMPROVE", "Tarcza: odnowienie"));
		s.push_back(Tg(129, PM_AW, "RX_AW_SKILL_WRATH", "Gniew Staro\xBFytnych"));
		s.push_back(Tg(130, PM_AW, "RX_AW_SKILL_WRATH_TARGETS", "Gniew: wi\xEA" "cej cel\xF3w"));
		s.push_back(Tg(131, PM_AW, "RX_AW_SKILL_WRATH_SOULSCOUNT", "Nowe dusze"));
		s.push_back(Tg(132, PM_AW, "RX_AW_SKILL_WRATH_DAMAGECOUNT", "Furia Staro\xBFytnych"));
		s.push_back(Grp(PM_AW, "Zaklinanie broni"));
		s.push_back(Tg(133, PM_AW, "RX_AW_SKILL_ENCHANT", "Czar ognia"));
		s.push_back(Tg(134, PM_AW, "RX_AW_SKILL_ENCHANTCOLD", "Czar lodu"));
		s.push_back(Tg(135, PM_AW, "RX_AW_SKILL_ENCHANTELECTRO", "Czar b\xB3yskawicy"));
		s.push_back(Tg(136, PM_AW, "RX_AW_SKILL_ENCHANTFIREBURST", "Ogie\xF1: wybuch"));
		s.push_back(Tg(137, PM_AW, "RX_AW_SKILL_ENCHANTFIREINSIDE", "Ogie\xF1: \xBF" "ar wewn\xB9trz"));
		s.push_back(Tg(138, PM_AW, "RX_AW_SKILL_ENCHANTFIREALWAYS", "Ogie\xF1: zawsze"));
		s.push_back(Tg(139, PM_AW, "RX_AW_SKILL_ENCHANTFIREFAST", "Ogie\xF1: szybciej"));
		s.push_back(Tg(140, PM_AW, "RX_AW_SKILL_ENCHANTCOLDFREEZE", "L\xF3" "d: zamro\xBF" "enie"));
		s.push_back(Tg(141, PM_AW, "RX_AW_SKILL_ENCHANTCOLDDAMAGE", "L\xF3" "d: obra\xBF" "enia"));
		s.push_back(Tg(142, PM_AW, "RX_AW_SKILL_ENCHANTCOLDSLOW", "L\xF3" "d: spowolnienie"));
		s.push_back(Tg(143, PM_AW, "RX_AW_SKILL_ENCHANTELECTROMOREDEF", "B\xB3yskawica: ochrona"));
		s.push_back(Tg(144, PM_AW, "RX_AW_SKILL_ENCHANTELECTRODAMAGEPROC", "B\xB3yskawica: obra\xBF" "enia"));
		s.push_back(Grp(PM_AW, "Inne"));
		s.push_back(Tg(145, PM_AW, "RX_AW_SKILL_CLEANRUNE", "Rytua\xB3 oczyszczenia"));
		s.push_back(Lk(146, PM_AW, "RX_AW_SKILL_MANAUP", "Zew ducha"));
		s.push_back(Tg(147, PM_AW, "RX_AW_SKILL_MANAREGEN", "Odnowa energii"));
		s.push_back(Tg(148, PM_AW, "RX_AW_SKILL_RANGEUP", "Rozw\xF3j duszy"));
		s.push_back(Tg(149, PM_AW, "RX_AW_SKILL_HEALSURGE", "Przyp\xB3yw si\xB3"));
		s.push_back(Tg(150, PM_AW, "RX_AW_SKILL_MANANEEDNERF", "Bieg\xB3o\x9C\xE6 w magii"));
		s.push_back(Tg(151, PM_AW, "RX_AW_SKILL_MANAEATER", "Poch\xB3" "anianie many"));
		s.push_back(Tg(152, PM_AW, "RX_AW_SKILL_ADANOSLUCK", "B\xB3ogos\xB3" "awie\xF1stwo Adanosa"));
		s.push_back(Tg(153, PM_AW, "RX_AW_SKILL_COMMONCOOLDOWN", "Przep\xB3yw magii"));
		s.push_back(Tg(154, PM_AW, "RX_AW_SKILL_STONEGUARDLOOT", "Wiedza o kamiennych stra\xBFnikach"));
		s.push_back(Tg(155, PM_AW, "RX_SKILL_ARCHEOLOGY", "Archeologia (runa poszukiwania)"));
		// C6 czarnoksieznik
		s.push_back(Grp(PM_DARKMAGE, "Umiej\xEAtno\x9C" "ci"));
		s.push_back(Flags(Tg(160, PM_DARKMAGE, "RX_DARKMAGEDIALOGSKILL", "Czarna magia (nowe dialogi)"), PSF_DIALOG_TEACHER,
			"NONE_100_XARDAS"));
		s.push_back(Tg(161, PM_DARKMAGE, "RX_DARKMAGECURSERESIST", "Odporno\x9C\xE6 na kl\xB9twy"));
		// C7 przywolywacz
		s.push_back(Grp(PM_SUMMONER, "Aury"));
		s.push_back(With2(Tg(165, PM_SUMMONER, "RX_NECRORUNEAURARUNELEARNED", "Aura w\xB3" "adcy nieumar\xB3ych"), "RX_NECRORUNEAURA_1", PV2_PAIR, 250));
		s.push_back(Tg(166, PM_SUMMONER, "RX_NECRORUNEAURA_ARMOR", "Aura mistrza ko\x9C" "ci"));
		s.push_back(Tg(167, PM_SUMMONER, "RX_NECRORUNEAURA_2", "Aura szybkich ko\x9C" "ci"));
		s.push_back(Tg(168, PM_SUMMONER, "RX_NECRORUNEAURA_3", "Aura dusz\xB9" "cej ciemno\x9C" "ci"));
		s.push_back(Tg(169, PM_SUMMONER, "RX_NECRORUNEAURA_4", "Aura magicznej broni"));
		s.push_back(Grp(PM_SUMMONER, "Przywo\xB3" "a\xF1" "cy"));
		s.push_back(Tg(170, PM_SUMMONER, "RX_NECROREGENPERC", "Regeneracja nieumar\xB3ych"));
		s.push_back(Lk(171, PM_SUMMONER, "RX_NECROSUMMONMANABOOST", "Mana +15"));
		s.push_back(Tg(172, PM_SUMMONER, "RX_NECROSUMMONBOOSTDAMAGELEARNED", "Przebicie pancerza przywo\xB3" "a\xF1" "c\xF3w"));
		s.push_back(Tg(173, PM_SUMMONER, "RX_NECROSUMMONREDUCEDAMAGE", "Nieumarli: -5% obra\xBF" "e\xF1"));
		s.push_back(Tg(174, PM_SUMMONER, "RX_NECRORUNEGHOSTDAMAGE", "Obra\xBF" "enia przeciwko duchom"));
		s.push_back(Tg(175, PM_SUMMONER, "RX_NECROSUMMONADDSKILLLEARNED", "Limit przywo\xB3" "a\xF1" "c\xF3w +1"));
		s.push_back(Tg(176, PM_SUMMONER, "RX_NECROSUMMONMANAREGEN", "Regeneracja many +1"));
		s.push_back(Tg(177, PM_SUMMONER, "RX_NECROSUMMONMANAREDUCEAURA", "Mniejszy koszt many aury"));
		s.push_back(Tg(178, PM_SUMMONER, "RX_NECROSUMMONMANAREDUCEGLOBALAURA", "Mniejszy koszt utrzymania"));
		s.push_back(Tg(179, PM_SUMMONER, "RX_NECROSUMMONREDUCESELFDAMAGE", "Przekierowanie obra\xBF" "e\xF1"));
		s.push_back(Tg(180, PM_SUMMONER, "RX_NECROSUMMONERUPSID", "Sid zajmuje 1 miejsce"));
		s.push_back(Tg(181, PM_SUMMONER, "RX_NECROSUMMONERSKELETONLEARNED", "Osobisty szkielet"));
		// C9 szaman
		s.push_back(Grp(PM_SHAMAN, "Totemy"));
		s.push_back(Tg(185, PM_SHAMAN, "RX_SHAMANTOTEMUSELEARNED", "U\xBFywanie totem\xF3w"));
		s.push_back(Tg(186, PM_SHAMAN, "RX_SHAMANTOTEMFIRELEARNED", "Totem magii"));
		s.push_back(Tg(187, PM_SHAMAN, "RX_SHAMANTOTEMLIFELEARNED", "Totem \xBFycia"));
		s.push_back(Tg(188, PM_SHAMAN, "RX_SHAMANTOTEMPOWERLEARNED", "Totem mocy"));
		s.push_back(Tg(189, PM_SHAMAN, "RX_SHAMANTOTEMAOELEARNED", "Totem \xBF" "aru"));
		s.push_back(Tg(190, PM_SHAMAN, "RX_SHAMANTOTEMSLOWLEARNED", "Totem ziemi"));
		s.push_back(Tg(191, PM_SHAMAN, "RX_SHAMANTOTEMSTUNLEARNED", "Totem ska\xB3y"));
		s.push_back(Tg(192, PM_SHAMAN, "RX_SHAMANTOTEMAGROLEARNED", "Totem ochrony"));
		s.push_back(Tg(193, PM_SHAMAN, "RX_SHAMANTOTEMWOLFLEARNED", "Totem lasu"));
		s.push_back(Tg(194, PM_SHAMAN, "RX_SHAMANTOTEMELECTROLEARNED", "Totem burzy"));
		s.push_back(Tg(195, PM_SHAMAN, "RX_SHAMANMORETOTEMSLEVEL", "Limit aktywnych totem\xF3w"));
		s.push_back(Tg(196, PM_SHAMAN, "RX_SHAMANWINDLEVEL", "Furia wichru"));
		// C10 druid
		s.push_back(Grp(PM_DRUID, "Umiej\xEAtno\x9C" "ci"));
		s.push_back(Tg(200, PM_DRUID, "RX_DRUID_SKILL_0", "Regeneracja zdrowia w przemianie"));
		s.push_back(Tg(201, PM_DRUID, "RX_DRUID_SKILL_1", "Regeneracja many w przemianie"));
		s.push_back(Tg(202, PM_DRUID, "RX_DRUID_SKILL_2", "Obrona fizyczna w przemianie"));
		s.push_back(Tg(203, PM_DRUID, "RX_DRUID_SKILL_3", "Obrona magiczna w przemianie"));
		s.push_back(Tg(204, PM_DRUID, "RX_DRUID_SKILL_5", "Mniejszy koszt many przemiany"));
		s.push_back(Tg(205, PM_DRUID, "RX_DRUID_SKILL_6", "Mniejszy koszt umiej\xEAtno\x9C" "ci druida"));
		s.push_back(Tg(206, PM_DRUID, "RX_DRUID_SKILL_7", "Zbieranie przedmiot\xF3w w przemianie"));
		s.push_back(Tg(207, PM_DRUID, "RX_DRUID_SKILL_8", "Zdrowie bohatera w przemianie"));
		s.push_back(Lk(217, PM_DRUID, "RX_DRUIDLANGLEARNED", "J\xEAzyk druid\xF3w"));
		s.push_back(Grp(PM_DRUID, "Przemiany"));
		static const char* const trf[9] = { "kr\xF3lika", "dzika", "smoczego z\xEA" "bacza", "ognistego jaszczura", "pe\xB3zacza",
			"goblina szamana", "golema", "kolcogrzbieta", "jaszczurocz\xB3" "eka" };
		static const char* const trfVar[9] = { "RX_DRUID_TRF_1", "RX_DRUID_TRF_2", "RX_DRUID_TRF_3", "RX_DRUID_TRF_4", "RX_DRUID_TRF_5",
			"RX_DRUID_TRF_6", "RX_DRUID_TRF_7", "RX_DRUID_TRF_8", "RX_DRUID_TRF_9" };
		static std::string trfName[9];
		for (int i = 0; i < 9; i++) {
			trfName[i] = std::string("Przemiana w ") + trf[i];
			s.push_back(Tg(208 + i, PM_DRUID, trfVar[i], trfName[i].c_str()));
		}
		// C11 truciciel
		s.push_back(Grp(PM_POISON, "Trucizny"));
		s.push_back(Tg(220, PM_POISON, "RX_POISONKNOWLEVEL", "Tworzenie trucizn"));
		s.push_back(Tg(221, PM_POISON, "RX_POISONMOREHITS", "Liczba \xB3" "adunk\xF3w na broni"));
		s.push_back(Tg(222, PM_POISON, "RX_POISONCHANCE", "Szansa na zatrucie"));
		s.push_back(Tg(223, PM_POISON, "RX_POISONMORETICKS", "Ilo\x9C\xE6 tick\xF3w trucizny"));
		s.push_back(Tg(224, PM_POISON, "RX_POISONMORESPEEDTICK", "Pr\xEA" "dko\x9C\xE6 tick\xF3w trucizny"));
		s.push_back(Tg(225, PM_POISON, "RX_POISONNEWRECEPIES", "Specjalne trucizny"));
		s.push_back(Tg(226, PM_POISON, "RX_POISONMULTIDOTLEVEL", "Dodatkowe zatrucie"));
		s.push_back(Tg(227, PM_POISON, "RX_POISONPARALIZE", "Parali\xBF"));
		s.push_back(Tg(228, PM_POISON, "RX_POISONSLOW", "Spowolnienie"));
		s.push_back(Tg(229, PM_POISON, "RX_POISONACID", "Zatruwanie trolli i golem\xF3w"));
		s.push_back(Tg(230, PM_POISON, "RX_POISONINSTANTDAMAGE", "Natychmiastowe obra\xBF" "enia"));
		s.push_back(Tg(231, PM_POISON, "RX_POISONUNIVERSAL", "Uniwersalna trucizna"));
		s.push_back(Tg(232, PM_POISON, "RX_POISONFINALTARGET", "\x8Cmiertelna trucizna"));
		// E: klasy gildii (indeksy z wolnych miejsc: 233-249 i 41-45; stale w zapisie)
		s.push_back(Grp(PM_GC_KDF, "Magia ognia"));
		s.push_back(Tg(233, PM_GC_KDF, "RX_SKILL_DOT1", "Szybko\x9C\xE6 spalania +10%"));
		s.push_back(Tg(234, PM_GC_KDF, "RX_SKILL_DOT2", "Szybko\x9C\xE6 spalania +20%"));
		s.push_back(Tg(235, PM_GC_KDF, "RX_SKILL_DOTPLUS1", "Dodatkowy tick spalania"));
		s.push_back(Tg(236, PM_GC_KDF, "RX_SKILL_DOTREPEAT", "Przed\xB3u\xBF" "enie spalania"));
		s.push_back(Tg(237, PM_GC_KDF, "RX_SKILL_DOTNEWDAMAGE", "Dodatkowe obra\xBF" "enia spalania"));
		s.push_back(Grp(PM_GC_KDW, "Magia wody"));
		s.push_back(Tg(244, PM_GC_KDW, "RX_WATERMAGEMANALEARNED", "Odnowa many w potrzebie"));
		s.push_back(Tg(245, PM_GC_KDW, "RX_WATERMAGEREGENCOMMON", "Regeneracja many"));
		s.push_back(Tg(246, PM_GC_KDW, "RX_WATERMAGEPOTIONSBETTER", "Skuteczniejsze eliksiry many"));
		s.push_back(Grp(PM_GC_PAL, "Paladyn"));
		s.push_back(Tg(238, PM_GC_PAL, "RX_PALADINSKILL_ORCSDAMAGE", "Obra\xBF" "enia przeciw orkom i jaszczurocz\xB3" "ekom"));
		s.push_back(Tg(239, PM_GC_PAL, "RX_PALADINSKILL_HEALSPEED", "Szybkie leczenie"));
		s.push_back(Tg(240, PM_GC_PAL, "RX_PALADINSKILL_MAGICDAMAGE", "Magia bitewna"));
		s.push_back(Grp(PM_GC_GUR, "Guru"));
		s.push_back(Tg(241, PM_GC_GUR, "RX_GURUMANAADDLEARNED", "Umys\xB3 Guru"));
		s.push_back(Lk(242, PM_GC_GUR, "RX_GURUPOWERLEARNED", "Pot\xEAga Guru"));
		s.push_back(Tg(243, PM_GC_GUR, "RX_GURURESISTLEARNED", "Spok\xF3j Guru"));
		s.push_back(Grp(PM_GC_TPL, "Stra\xBFnik"));
		s.push_back(Tg(42, PM_GC_TPL, "RX_TPLSKILLCRIT", "Mistrzostwo walki wr\xEA" "cz"));
		s.push_back(Tg(43, PM_GC_TPL, "RX_TPL_SKILL_MAGICSTR", "Mistrzostwo magii stra\xBFnika"));
		s.push_back(Lk(44, PM_GC_TPL, "PSIWAYCOMBAT", "Magia Stra\xBFnik\xF3w: runy bojowe"));
		s.push_back(Lk(45, PM_GC_TPL, "PSIWAYHEAL", "Magia Stra\xBFnik\xF3w: runy lecznicze"));
		s.push_back(Grp(PM_GC_SLD, "Najemnik / \xA3owca smok\xF3w"));
		s.push_back(Tg(247, PM_GC_SLD, "RX_SKILLMERCBLEED", "G\xB3\xEA" "bokie rany"));
		s.push_back(Tg(248, PM_GC_SLD, "RX_SKILLMERCPHYSRESIST", "Techniki obronne"));
		s.push_back(Tg(249, PM_GC_SLD, "RX_SKILLMERCNEWDAMAGE", "Szcz\xEA\x9C" "cie najemnika"));
		s.push_back(Tg(41, PM_GC_SLD, "RX_SKILLBETTERREGEN", "Skuteczne mikstury"));
		return s;
	}

	const std::vector<PmSkillDef>& PmSkills() {
		static const std::vector<PmSkillDef> s = BuildSkills();
		return s;
	}

	const PmSkillDef* PmSkillByIdx(int idx) {
		for (const PmSkillDef& d : PmSkills())
			if (d.idx == idx && d.kind != PSK_GROUP)
				return &d;
		return nullptr;
	}

	static bool SkillBit(const PmVars& v, int idx) {
		if (idx < 0 || idx >= PM_SKILL_SLOTS)
			return false;
		return ((unsigned)v.Get(PM_V_SKILL_OFF, idx / 32) >> (idx % 32)) & 1u;
	}

	static void SkillBitSet(PmVars& v, PmResult& r, int idx, bool on) {
		unsigned w = (unsigned)v.Get(PM_V_SKILL_OFF, idx / 32);
		unsigned b = 1u << (idx % 32);
		Write(v, r, PM_V_SKILL_OFF, (int)(on ? (w | b) : (w & ~b)), idx / 32);
	}

	PmSkillView PmReadSkill(const PmVars& v, const PmSkillDef& d) {
		PmSkillView s;
		s.def = &d;
		if (d.kind == PSK_GROUP || !d.var)
			return s;
		s.value = v.Get(d.var);
		s.keep = d.idx >= 0 ? v.Get(PM_V_SKILL_KEEP, d.idx) : 0;
		s.off = d.kind == PSK_TOGGLE && SkillBit(v, d.idx);
		s.learned = s.value > 0 || (d.kind == PSK_TOGGLE && (s.keep > 0 || s.off));
		return s;
	}

	bool PmGuildClassEarned(const PmVars& v, int id, int curGuild) {
		if (!PmIsGuildClass(id))
			return false;
		const PmGuildClassDef& c = PmGuildClass(id);
		if ((c.trail1 && v.Get(c.trail1) != 0) || (c.trail2 && v.Get(c.trail2) != 0))
			return true;
		if (curGuild == c.guild1 || (c.guild2 >= 0 && curGuild == c.guild2))
			return true;
		for (const PmSkillDef& d : PmSkills())
			if (d.path == id && d.kind != PSK_GROUP && PmReadSkill(v, d).learned)
				return true;
		return false;
	}

	PmDeny PmSkillDeny(const PmWorld& w, const PmVars& v, const PmSkillDef& d, bool on) {
		if (d.kind == PSK_LOCKED)
			return PMD_LOCKED;
		if (d.kind != PSK_TOGGLE || !d.var || !PmHas(v, d.var))
			return PMD_NO_VAR;
		PmSkillView s = PmReadSkill(v, d);
		if (!s.learned)
			return PMD_NOT_EARNED;
		if (on ? !s.off : s.off)
			return PMD_SAME;
		if (w.trans)
			return PMD_TRANS;
		if (w.dialog)
			return PMD_DIALOG;
		if (d.guard == PG_DK_OFF && v.Get(PmPath(PM_DK).flag) != 0)
			return PMD_DK_ON;
		if (d.guard == PG_FAMILIAR && w.familiar)
			return PMD_FAMILIAR;
		return PMD_OK;
	}

	PmResult PmSetSkill(PmVars& v, const PmWorld& w, const PmSkillDef& d, bool on) {
		PmResult r;
		r.deny = PmSkillDeny(w, v, d, on);
		if (r.deny != PMD_OK)
			return r;
		EnsureVer(v, r);
		int cur = v.Get(d.var), keep = v.Get(PM_V_SKILL_KEEP, d.idx);
		if (!on) {
			int k = std::max(cur, keep);
			Write(v, r, PM_V_SKILL_KEEP, k, d.idx);
			Write(v, r, d.var, 0);
			if (d.var2 && PmHas(v, d.var2)) {
				if (d.var2Mode == PV2_ZERO)
					Write(v, r, d.var2, 0);
				else if (d.var2Mode == PV2_PAIR && d.keep2 >= 0) {
					Write(v, r, PM_V_SKILL_KEEP, std::max(v.Get(d.var2), v.Get(PM_V_SKILL_KEEP, d.keep2)), d.keep2);
					Write(v, r, d.var2, 0);
				}
				else if (d.var2Mode == PV2_COUNT && k > 0)
					Write(v, r, d.var2, std::max(0, v.Get(d.var2) - 1));
			}
			SkillBitSet(v, r, d.idx, true);
			return r;
		}
		int val = std::max(cur, keep);
		Write(v, r, d.var, val);
		Write(v, r, PM_V_SKILL_KEEP, 0, d.idx);
		if (d.var2 && PmHas(v, d.var2)) {
			if (d.var2Mode == PV2_PAIR && d.keep2 >= 0) {
				Write(v, r, d.var2, std::max(v.Get(d.var2), v.Get(PM_V_SKILL_KEEP, d.keep2)));
				Write(v, r, PM_V_SKILL_KEEP, 0, d.keep2);
			}
			else if (d.var2Mode == PV2_COUNT && val > 0)
				Write(v, r, d.var2, v.Get(d.var2) + 1);
		}
		SkillBitSet(v, r, d.idx, false);
		return r;
	}

	// ---------- pilnowanie i ochrona rozmow ----------

	static void Event(std::vector<PmEvent>& out, int kind, int id, int from, int to, const std::string& var) {
		PmEvent e;
		e.kind = kind;
		e.id = id;
		e.from = from;
		e.to = to;
		e.var = var;
		out.push_back(e);
	}

	void PmWatch(PmVars& v, PmWatchMem& mem, bool dialogRestored, std::vector<PmEvent>& out) {
		for (int p = 0; p < PM_PATHS; p++) {
			const PmPathDef& d = PmPath(p);
			if (!PmHas(v, d.flag))
				continue;
			int f = v.Get(d.flag);
			bool off = PathBit(v, p);
			if (off && f != 0) {
				v.Set(PM_V_KEEP, f, p);
				v.Set(d.flag, 0);
				Event(out, PME_PATH_FORCED_OFF, p, f, 0, d.flag);
				f = 0;
			}
			else if (mem.init && !off && mem.lastFlag[p] != f) {
				if (f == 0)
					Event(out, PME_PATH_GAME_OFF, p, mem.lastFlag[p], 0, d.flag);
				else if (mem.lastFlag[p] == 0)
					Event(out, PME_PATH_GAME_ON, p, 0, f, d.flag);
			}
			mem.lastFlag[p] = f;
		}
		mem.init = true;
		if (dialogRestored)
			return;
		for (const PmSkillDef& d : PmSkills()) {
			if (d.kind != PSK_TOGGLE || !SkillBit(v, d.idx))
				continue;
			int cur = v.Get(d.var);
			if (cur > 0) {
				int keep = v.Get(PM_V_SKILL_KEEP, d.idx);
				v.Set(PM_V_SKILL_KEEP, std::max(cur, keep), d.idx);
				v.Set(d.var, 0);
				Event(out, PME_SKILL_REZERO, d.idx, cur, 0, d.var);
			}
			if (d.var2 && d.var2Mode == PV2_PAIR && d.keep2 >= 0) {
				int c2 = v.Get(d.var2);
				if (c2 > 0) {
					v.Set(PM_V_SKILL_KEEP, std::max(c2, v.Get(PM_V_SKILL_KEEP, d.keep2)), d.keep2);
					v.Set(d.var2, 0);
					Event(out, PME_SKILL_REZERO, d.idx, c2, 0, d.var2);
				}
			}
		}
	}

	int PmDialogRestore(PmVars& v, const std::string& npc, std::vector<PmEvent>& out) {
		int n = 0;
		for (const PmSkillDef& d : PmSkills()) {
			if (d.kind != PSK_TOGGLE || !SkillBit(v, d.idx))
				continue;
			if ((d.flags & PSF_DIALOG_TEACHER) && (!d.teacher || npc != d.teacher))
				continue;
			int cur = v.Get(d.var), keep = v.Get(PM_V_SKILL_KEEP, d.idx);
			if (keep > cur) {
				v.Set(d.var, keep);
				Event(out, PME_SKILL_RESTORED, d.idx, cur, keep, d.var);
				n++;
			}
			if (d.var2 && d.var2Mode == PV2_PAIR && d.keep2 >= 0) {
				int c2 = v.Get(d.var2), k2 = v.Get(PM_V_SKILL_KEEP, d.keep2);
				if (k2 > c2) {
					v.Set(d.var2, k2);
					Event(out, PME_SKILL_RESTORED, d.idx, c2, k2, d.var2);
				}
			}
		}
		return n;
	}

	int PmDialogReapply(PmVars& v, std::vector<PmEvent>& out) {
		int n = 0;
		for (const PmSkillDef& d : PmSkills()) {
			if (d.kind != PSK_TOGGLE || !SkillBit(v, d.idx))
				continue;
			int cur = v.Get(d.var), keep = v.Get(PM_V_SKILL_KEEP, d.idx);
			if (cur > keep)
				Event(out, PME_SKILL_BOUGHT, d.idx, keep, cur, d.var);
			if (cur > 0) {
				v.Set(PM_V_SKILL_KEEP, std::max(cur, keep), d.idx);
				v.Set(d.var, 0);
				n++;
			}
			if (d.var2 && d.var2Mode == PV2_PAIR && d.keep2 >= 0) {
				int c2 = v.Get(d.var2), k2 = v.Get(PM_V_SKILL_KEEP, d.keep2);
				if (c2 > 0) {
					v.Set(PM_V_SKILL_KEEP, std::max(c2, k2), d.keep2);
					v.Set(d.var2, 0);
				}
			}
		}
		return n;
	}

	static std::string SeriesText(const std::vector<int>& v, const char* unit) {
		bool any = false;
		for (int x : v)
			any = any || x != 0;
		if (!any)
			return "";
		bool same = true;
		for (int x : v)
			same = same && x == v[0];
		std::string s;
		if (same)
			s = std::to_string(v[0]);
		else
			for (size_t i = 0; i < v.size(); i++)
				s += (i ? " / " : "") + std::to_string(v[i]);
		return s + " " + unit;
	}

	std::string PmCostText(const std::vector<int>& lp, const std::vector<int>& gold) {
		std::string a = SeriesText(lp, "PN"), b = SeriesText(gold, "z\xB3");
		if (a.empty())
			return b;
		return b.empty() ? a : a + ", " + b;
	}

	bool PmFistOverlayWanted(const PmVars& v, int guild) {
		return v.Get(PmPath(PM_VAMP).flag) == PmPath(PM_VAMP).magic && guild == 16 && v.Get(V_VAMP_TYPE) == 2 &&
			v.Get("RX_VAMP_SKILLFIST_NEWOVERLAY") != 0;
	}

	std::string PmSummary(const PmVars& v, int guild) {
		std::string s;
		char buf[200];
		sprintf_s(buf, "gildia %d, ZDF_PATHS_OFF 0x%X, ZDF_GUILD_LAST %d; sciezki:", guild, (unsigned)v.Get(PM_V_OFF),
			v.Get(PM_V_GUILD_LAST));
		s = buf;
		for (int p = 0; p < PM_PATHS; p++) {
			PmPathView pv = PmReadPath(v, p);
			if (pv.state == PMS_NONE)
				continue;
			sprintf_s(buf, " %s %s (znacznik %d, slad %d, keep %d);", PmPath(p).flag, PmPathStateLog(pv.state), pv.flag, pv.trail,
				pv.keep);
			s += buf;
		}
		int off = 0;
		std::string offList;
		for (const PmSkillDef& d : PmSkills()) {
			if (d.kind != PSK_TOGGLE || !SkillBit(v, d.idx))
				continue;
			off++;
			sprintf_s(buf, " %s(keep %d)", d.var, v.Get(PM_V_SKILL_KEEP, d.idx));
			offList += buf;
		}
		sprintf_s(buf, " wylaczone umiejetnosci: %d", off);
		s += buf;
		return s + offList;
	}
}
