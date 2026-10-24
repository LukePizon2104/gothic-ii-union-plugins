// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	struct StyleSlot {
		const char* testAni;                 // S_1HATTACK / S_2HATTACK - styl ma kombinacje dla tej broni
		const char* w3Overlay;               // styl Wiedzmina dla tej broni
		int talent;                          // oCNpcTalent::NPC_TAL_1H / _2H
		const char* runAni;                  // S_1HRUNL / S_2HRUNL
		const char* locoOverlay;             // bieg i chod Geralta z mieczem (tylko przy stylu W3)
		const char* sprintOverlay;           // sprint Geralta z mieczem (W3Sprint, tylko przy stylu W3)
		std::vector<std::string> styles;     // dostepne nakladki (natywne, zEBA, W3)
		bool w3Available = false;
		bool locoAvailable = false, sprintAvailable = false;
	};
	static StyleSlot styleSlots[2] = {
		{ "S_1HATTACK", "HUMANS_ZDF_W3_1H", oCNpcTalent::NPC_TAL_1H, "S_1HRUNL", "HUMANS_ZDF_W3_LOCO_1H", "HUMANS_ZDF_W3_SPRINT_1H" },
		{ "S_2HATTACK", "HUMANS_ZDF_W3_2H", oCNpcTalent::NPC_TAL_2H, "S_2HRUNL", "HUMANS_ZDF_W3_LOCO_2H", "HUMANS_ZDF_W3_SPRINT_2H" },
	};
	// Ruch Geralta bez broni (W3Locomotion) i jego sprint zamiast animacji sprintu gry / zEBA (W3Sprint).
	static const char* LOCO_OVERLAY = "HUMANS_ZDF_W3_LOCO";
	static const char* SPRINT_OVERLAY = "HUMANS_ZDF_W3_SPRINT";
	static bool locoAvailable = false, sprintAvailable = false;
	// nakladki zalozone przez plugin; 'mine' = nie bylo jej na modelu wczesniej (wolno ja zdjac)
	struct AppliedStyle {
		std::string name;
		bool mine;
	};
	static std::vector<AppliedStyle> appliedStyles;
	// Wybor cyklu U per bron w reku (0 = 1H, 1 = 2H; nazwa nakladki, "" = AUTO). Zestaw animacji, ktorym ta bron gra
	// (przy zamianie 1H <-> 2H - drugi), dostaje wybor albo jego odpowiednik drugiego typu (zdcore::SwapStyleCounterpart:
	// W3 1H -> W3 2H, zEBA 1HST1_REMASTER -> 2HST1_REMASTER); bez odpowiednika - AUTO.
	static std::string forcedStyle[2];
	static bool prevStyleKey = false, prevLocoKey = false;

	// Napis ze stanem ruchu co LOCO_WATCH_MS (zdism loco watch).
	static bool locoWatch = false;
	static DWORD locoWatchNext = 0;
	static const DWORD LOCO_WATCH_MS = 800;
	// Tempo sprintu W3: fpsRate z pliku per "NAKLADKA|id animacji" (klucz po nazwie - prototyp nakladki bywa trzymany przez
	// silnik po zdjeciu, wiec wskaznik moglby byc nieaktualny) i skala nalozona w tej klatce per nasza nakladka sprintu.
	static std::map<std::string, float> sprintBaseFps;
	static std::map<std::string, float> sprintScaleNow;
	static std::map<std::string, float> sprintScaleLogged;
	// Kolizje sprintu zalogowane w tej sesji (dwie obce nakladki sprintu naraz, tempo modelu).
	static std::map<std::string, bool> sprintClashLogged;

	// Diagnostyka nakladek bohatera (zDismember.log): zmiana listy nakladek, zmiana nakladki petli ruchu, wykonany plan.
	// Z limitem linii (odnawiany przy starcie i zdism reload), zeby uniki i walka nie zasypaly logu.
	static const int DIAG_LIMIT = 400;
	static int diagLeft = 0;
	static std::string diagOverlays, diagLoop;
	// Zdjecie / zalozenie nakladki odlozone przez trwajaca animacje (dobycie broni, picie...) - od tej klatki.
	static unsigned int deferSince = 0;
	static const unsigned int DEFER_MAX_FRAMES = 180;

	static void StyleDiag(const char* fmt, ...) {
		if (diagLeft <= 0)
			return;
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);
		ZLog("%s", buf);
		if (--diagLeft == 0)
			ZLog("Ruch: limit diagnostyki nakladek (%d linii) wyczerpany - dalej bez wpisow (zdism reload odnawia)", DIAG_LIMIT);
	}

	// kandydaci w kolejnosci cyklu: Gothic, zExtendedBattleAnimations, Wiedzmin
	static const char* STYLE_CANDIDATES[] = {
		"HUMANS_ZDF_NAT_1HST1", "HUMANS_ZDF_NAT_1HST2", "HUMANS_ZDF_NAT_2HST1", "HUMANS_ZDF_NAT_2HST2",
		"HUMANS_1HST1", "HUMANS_1HST1_REMASTER", "HUMANS_1HST2", "HUMANS_1HST2_REMASTER", "HUMANS_1HST3",
		"HUMANS_1HDEXST2", "HUMANS_1HDEXST3", "HUMANS_RAPIER_ST1", "HUMANS_RAPIER_ST2", "HUMANS_RAPIER_ST3",
		"HUMANS_2HST1", "HUMANS_2HST1_REMASTER", "HUMANS_2HST2", "HUMANS_2HST2B", "HUMANS_2HST2_LHIV", "HUMANS_2HST3",
		"HUMANS_2HST3B", "HUMANS_2HTEMP_LHIV", "HUMANS_1X2ST3", "HUMANS_AXEST2", "HUMANS_AXEBST1", "HUMANS_AXEBST2",
		"HUMANS_SPST2", "HUMANS_ZDF_W3_1H", "HUMANS_ZDF_W3_2H",
	};

	static std::vector<int> IniKeys(const char* key) {
		if (!zoptions->EntryExists("KEYS", key))
			return {};
		return zdcore::ParseKeyBinding(zoptions->ReadString("KEYS", key, "").ToChar());
	}

	static int IniInt(const char* section, const char* key, int def) {
		return zoptions->EntryExists(section, key) ? zoptions->ReadInt(section, key, def) : def;
	}

	// Mody sprintu w tej instalacji (paczka w grze + ustawienia z Gothic.ini) i kolizje na jednym klawiszu. Steam G2 +
	// launcher warsztatu: zEBA (HUMANS_SPRINT_REMASTER, LShift), Ninja Sprint (Sprint.vdf, przedmiot 2787109635, klawisz
	// Ninja_Sprint_UseKey; Perma=0 = sprint przy kazdym biegu), [NATURALSPRINT] zostaje w ini po Nostalgic Edition.
	static void LogSprintMods() {
		std::vector<zdcore::SprintSource> src;
		std::string info;
		const char* ZEBA = "ZEXTENDEDBATTLEANIMATIONS";
		bool zebaPack = ZOverlayExists("HUMANS_SPRINT_REMASTER", "S_RUNL");
		if (zebaPack) {
			bool on = IniInt(ZEBA, "EnableSprint", 1) != 0;
			int key = zdcore::ZebaSprintKey(IniInt(ZEBA, "SprintKeyChoice", 0), zoptions->ReadString(ZEBA, "SprintKeyName", "").ToChar());
			bool remaster = IniInt(ZEBA, "RemasterSprint", 1) != 0;
			src.push_back({ "zEBA", { key }, false, on });
			info += std::string("zEBA ") + (on ? "wl." : "wyl. (EnableSprint=0)") + ", klawisz " + zdcore::KeyLabel(key) +
				(IniInt(ZEBA, "SprintMode", 0) ? " (przelacznik)" : " (trzymanie)") + ", nakladka " +
				(remaster ? "HUMANS_SPRINT_REMASTER" : zoptions->ReadString(ZEBA, "SprintAniSet", "HUMANS_SPRINT.MDS").ToChar());
		}
		struct NinjaPack { const char* name; const char* overlays[3]; const char* keyEntry; const char* section; };
		static const NinjaPack ninjas[] = {
			{ "Ninja Sprint", { "NINJA_SPRINT_DEFAULT", "NINJA_SPRINT_ALTERNATIVE", "NINJA_SPRINT_ALTERNATIVE_FAST" }, "Ninja_Sprint_UseKey", "NINJA_SPRINT" },
			{ "Ninja Sprint L'Hiver", { "NINJA_SPRINT_AL", "NINJA_SPRINT_ST", nullptr }, "Ninja_SprintLhiverEng_1_6_UseKey", "NINJA_SPRINTLHIVERENG_1_6" },
		};
		for (auto& n : ninjas) {
			bool pack = false;
			for (const char* o : n.overlays)
				if (o && ZOverlayExists(o, "S_RUNL"))
					pack = true;
			if (!pack)
				continue;
			std::vector<int> keys = IniKeys(n.keyEntry);
			bool perma = IniInt(n.section, "Perma", 0) != 0;   // Ninja: brak wpisu = 0 = sprint przy kazdym biegu
			src.push_back({ n.name, keys, !perma, true });
			std::string k;
			for (int c : keys)
				k += (k.empty() ? "" : "/") + zdcore::KeyLabel(c);
			info += std::string(info.empty() ? "" : " | ") + n.name + ": paczka jest, " +
				(!perma ? "sprint przy kazdym biegu (Perma=0)" : keys.empty() ? "bez klawisza (" + std::string(n.keyEntry) + "=0000) - nie sprintuje" : "klawisz " + k);
		}
		if (zoptions->SectionExists("NATURALSPRINT"))
			info += std::string(info.empty() ? "" : " | ") + "[NATURALSPRINT] NS_ActivationKey=" +
				zoptions->ReadString("NATURALSPRINT", "NS_ActivationKey", "").ToChar() +
				" (wtyczki nie wykrywam - sekcja moze zostac po Nostalgic Edition; w grze widac ja po tempie modelu)";
		ZLog("Sprint w grze: %s", info.empty() ? "zadnego znanego moda (sprint tylko z mikstury szybkosci)" : info.c_str());
		for (auto& c : zdcore::SprintKeyClashes(src))
			ZLog("Sprint: UWAGA kolizja %s - wylacz jeden: zEBA [ZEXTENDEDBATTLEANIMATIONS] EnableSprint=0 (albo inny SprintKeyChoice), "
				"Ninja Sprint [KEYS] Ninja_Sprint_UseKey=0000 i [NINJA_SPRINT] Perma=1 (albo odznacz Sprint w launcherze warsztatu)", c.c_str());
	}

	// Klawisz przelacznika ruchu zajety w grze ([KEYS]) albo przez inny klawisz pluginu.
	static void WarnLocoKey() {
		if (zcfg.locoKey <= 0)
			return;
		static const char* keys[] = { "keyEnd", "keyHeal", "keyPotion", "keyLockTarget", "keyParade", "keyActionRight",
			"keyActionLeft", "keyUp", "keyDown", "keyLeft", "keyRight", "keyStrafeLeft", "keyStrafeRight", "keyAction",
			"keySlow", "keySMove", "keyWeapon", "keySneak", "keyLook", "keyLookFP", "keyInventory", "keyShowStatus",
			"keyShowLog", "keyShowMap", "keyTorch", "keyIntSprint", "Ninja_Sprint_UseKey" };
		for (const char* k : keys)
			for (int code : IniKeys(k))
				if (code == zcfg.locoKey)
					ZLog("Ruch: UWAGA klawisz %s (LocoKey=%d) jest tez w [KEYS] %s - zmien LocoKey w [ZDISMEMBER]",
						zdcore::KeyLabel(zcfg.locoKey).c_str(), zcfg.locoKey, k);
		if (zcfg.locoKey == zcfg.styleKey || zcfg.locoKey == zcfg.dodgeKey || zcfg.locoKey == zcfg.rollKey)
			ZLog("Ruch: UWAGA LocoKey=%d to tez StyleKey / DodgeKey / RollKey", zcfg.locoKey);
	}

	void StylesLoad() {
		forcedStyle[0].clear();
		forcedStyle[1].clear();
		for (auto& slot : styleSlots) {
			slot.styles.clear();
			for (const char* name : STYLE_CANDIDATES)
				if (ZOverlayExists(name, slot.testAni))
					slot.styles.push_back(name);
			slot.w3Available = ZOverlayExists(slot.w3Overlay, slot.testAni);
			slot.locoAvailable = ZOverlayExists(slot.locoOverlay, slot.runAni);
			slot.sprintAvailable = ZOverlayExists(slot.sprintOverlay, slot.runAni);
		}
		locoAvailable = ZOverlayExists(LOCO_OVERLAY, "S_RUNL");
		sprintAvailable = ZOverlayExists(SPRINT_OVERLAY, "S_RUNL");
		// tryb magii (czar / runa / Znak w reku): petle S_MAGRUNL / S_MAGWALKL w tych samych nakladkach (make_loco.py od
		// 21.10) - starsza paczka W3 ich nie ma, wtedy z czarem w reku gra bieg natywny
		bool locoMag = locoAvailable && ZOverlayExists(LOCO_OVERLAY, "S_MAGRUNL");
		bool sprintMag = sprintAvailable && ZOverlayExists(SPRINT_OVERLAY, "S_MAGRUNL");
		diagLeft = DIAG_LIMIT;
		diagOverlays.clear();
		diagLoop.clear();
		deferSince = 0;
		ZLog("Style: 1H %d (W3 %s), 2H %d (W3 %s); W3 sam od %d%% talentu%s, klawisz cyklu %d",
			(int)styleSlots[0].styles.size(), styleSlots[0].w3Available ? "tak" : "nie",
			(int)styleSlots[1].styles.size(), styleSlots[1].w3Available ? "tak" : "nie",
			zcfg.w3StyleMinTalent, zcfg.w3Style ? "" : " (W3Style=0 - wylaczone)", zcfg.styleKey);
		ZLog("Ruch W3: bieg/chod %s (W3Locomotion=%d), sprint %s / 1H %s / 2H %s (W3Sprint=%d); z czarem w reku: bieg/chod %s, "
			"sprint %s",
			locoAvailable ? "tak" : "brak nakladki", (int)zcfg.w3Locomotion, sprintAvailable ? "tak" : "brak nakladki",
			styleSlots[0].sprintAvailable ? "tak" : "brak", styleSlots[1].sprintAvailable ? "tak" : "brak", (int)zcfg.w3Sprint,
			locoMag ? "tak" : "brak S_MAGRUNL (stara paczka W3)", sprintMag ? "tak" : "brak S_MAGRUNL");
		ZLog("Ruch: zestaw %s (LocoMode=%d%s), klawisz cyklu %s (LocoKey=%d, z Shiftem wstecz), konsola zdism loco",
			zdcore::LocoModeLabel(zcfg.locoMode), zcfg.locoMode, zcfg.locoMode ? " - wymuszony, AUTO = 0" : "",
			zdcore::KeyLabel(zcfg.locoKey).c_str(), zcfg.locoKey);
		WarnLocoKey();
		LogSprintMods();
		sprintScaleLogged.clear();
	}

	// Nakladki, ktore plan / cykl stylu moze zalozyc bohaterowi (tylko dostepne - po StylesLoad).
	std::vector<std::string> StylesOverlayNames() {
		std::vector<std::string> out;
		for (auto& slot : styleSlots) {
			out.insert(out.end(), slot.styles.begin(), slot.styles.end());
			if (slot.w3Available)
				out.push_back(slot.w3Overlay);
			if (slot.locoAvailable)
				out.push_back(slot.locoOverlay);
			if (slot.sprintAvailable)
				out.push_back(slot.sprintOverlay);
		}
		if (locoAvailable)
			out.push_back(LOCO_OVERLAY);
		if (sprintAvailable)
			out.push_back(SPRINT_OVERLAY);
		for (auto& o : BackSwordsOverlayNames())   // dobycie zza plecow (BackSwords.cpp)
			out.push_back(o);
		return out;
	}

	// Nazwa nakladki jak w ModelOverlays: wielkie litery, bez .MDS.
	static std::string ProtoKey(zCModelPrototype* p) {
		std::string n = zdcore::Upper(p->modelProtoName.ToChar());
		size_t dot = n.find('.');
		return dot == std::string::npos ? n : n.substr(0, dot);
	}

	static int ModelOverlayIndex(zCModel* m, const std::string& name) {
		for (int i = 0; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			const char* n = p ? p->modelProtoName.ToChar() : nullptr;
			if (n && _strnicmp(n, name.c_str(), name.size()) == 0 && (n[name.size()] == 0 || n[name.size()] == '.'))
				return i;
		}
		return -1;
	}

	// AUTO: W3 od progu talentu; przy zamianie animacji 1H <-> 2H zestaw drugiego typu idzie za talentem broni w rece
	// (miecz 1H gra animacjami 2H -> HUMANS_ZDF_W3_2H wg talentu 1H).
	static int SlotType(const StyleSlot& slot) {
		return &slot == &styleSlots[0] ? zdcore::SWAP_TYPE_1H : zdcore::SWAP_TYPE_2H;
	}

	static int StyleIndex(const StyleSlot& slot, const std::string& name) {
		for (size_t i = 0; i < slot.styles.size(); i++)
			if (slot.styles[i] == name)
				return (int)i;
		return -1;
	}

	// Wymuszony styl zestawu (indeks w slot.styles, -1 = AUTO): wybor broni, ktora tym zestawem gra (AniSwapSlotDriver).
	static int ForcedIndex(const StyleSlot& slot) {
		int type = SlotType(slot);
		const std::string& f = forcedStyle[AniSwapSlotDriver(type) == zdcore::SWAP_TYPE_2H ? 1 : 0];
		if (f.empty())
			return -1;
		int i = StyleIndex(slot, f);
		return i >= 0 ? i : StyleIndex(slot, zdcore::SwapStyleCounterpart(f, type));
	}

	static std::string WantedStyle(StyleSlot& slot) {
		// bron parowa NB (23.10): zestaw 2H gra natywnymi animacjami HUMANS_1X2ST3 / 2X2ST3 - bez stylu (W3 / zEBA / natywne
		// kopie) nad nimi; wymuszony wybor 2H zostaje w pamieci i wraca po odlozeniu pary
		if (&slot == &styleSlots[1] && HeroDualState() != zdcore::DUAL_NONE)
			return "";
		int forced = ForcedIndex(slot);
		if (forced >= 0)
			return slot.styles[forced];
		if (zdcore::AutoW3Style(player->GetTalentValue(AniSwapSlotTalent(slot.talent)), zcfg.w3StyleMinTalent, zcfg.w3Style,
			slot.w3Available))
			return slot.w3Overlay;
		return "";
	}

	// Nakladki na modelu od dolu, bez bazowego modelu, nazwy wielkimi literami bez .MDS.
	static std::vector<std::string> ModelOverlays(zCModel* m) {
		std::vector<std::string> out;
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p)
				out.push_back(ProtoKey(p));
		}
		return out;
	}

	bool HeroMounted() {
		// raz na klatke (wolane tez z hakow GetState / GetToggled - wiele razy w klatce)
		static unsigned frame = 0;
		static zCModel* model = nullptr;
		static bool mounted = false;
		zCModel* m = player && player->homeWorld ? player->GetModel() : nullptr;
		if (frame != zdFrame || model != m || !frame) {
			frame = zdFrame;
			model = m;
			mounted = m && zdcore::HasExclusiveForeignOverlay(ModelOverlays(m));
		}
		return mounted;
	}

	// Nakladka, z ktorej pochodzi animacja (najwyzsza z tym wskaznikiem w protoAnis), "model" = bazowy.
	static std::string OverlayOfAni(zCModel* m, zCModelAni* ani) {
		int id = ani ? ani->aniID : -1;
		for (int i = m->modelProtoList.GetNum() - 1; i >= 0 && id >= 0; i--) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && id < p->protoAnis.GetNum() && p->protoAnis[id] == ani)
				return i == 0 ? std::string("model") : ProtoKey(p);
		}
		return "?";
	}

	// Animacje ruchu (stany S_* i przejscia biegu) aktywne przed zmiana nakladek - po id, z faza.
	struct LocoSnap {
		int id;
		float progress;
		std::string name;
	};
	static std::vector<LocoSnap> SnapLocoAnis(zCModel* m) {
		std::vector<LocoSnap> out;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* ani = a ? a->protoAni : nullptr;
			if (!ani || a->isFadingOut || ani->aniID < 0)
				continue;
			std::string name = ani->aniName.ToChar();
			if (zdcore::IsRestorableAni(name))
				out.push_back({ ani->aniID, a->GetProgressPercent(), name });
		}
		return out;
	}

	// Po zmianie nakladek animacje ruchu ze zrzutu graja w wersji z wierzchu listy (GetAniFromAniID - tak samo wybiera
	// silnik), z zachowana faza. Po samym przelozeniu stara wersja jeszcze gra - StartAni przechodzi z niej plynnie;
	// po zdjeciu / zalozeniu (stop wszystkiego) startuje od nowa. Bez tego oCAniCtrl_Human::IsRunning (0x6AE200,
	// porownuje wskazniki z wersja z wierzchu) zwraca 0 i zEBA w tej samej klatce konczy sprint (0x1000982A).
	static std::string SyncLocoAnis(zCModel* m, const std::vector<LocoSnap>& snap) {
		std::string done;
		for (auto& s : snap) {
			zCModelAni* top = m->GetAniFromAniID(s.id);
			if (!top)
				continue;
			zCModelAniActive* act = m->GetActiveAni(top);
			if (act && !act->isFadingOut)
				continue;   // juz gra wersja z wierzchu
			m->StartAni(top, 0);
			act = m->GetActiveAni(top);
			if (act)
				act->SetProgressPercent(s.progress);
			char buf[192];
			sprintf_s(buf, " %s<-%s(%.2f)", s.name.c_str(), OverlayOfAni(m, top).c_str(), s.progress);
			done += buf;
		}
		return done;
	}

	// Trwajaca animacja, ktorej nie przerywamy zdjeciem / zalozeniem nakladki (stop wszystkich animacji): dobycie
	// broni, uzycie przedmiotu, trafienie - a takze bieg (zalozenie z wyprzedzeniem, gdy bohater stoi).
	static std::string BlockingAni(zCModel* m) {
		std::string loop;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* ani = a ? a->protoAni : nullptr;
			if (!ani || a->isFadingOut)
				continue;
			if (zdcore::IsBlockingAni(ani->aniName.ToChar(), ani->layer))
				return ani->aniName.ToChar();
			if (loop.empty() && zdcore::IsMoveLoopAni(ani->aniName.ToChar()))
				loop = ani->aniName.ToChar();
		}
		return loop;
	}

	// Ustawia nakladki modelu (bez bazowego) w kolejnosci 'order' samym przestawieniem wskaznikow w modelProtoList -
	// bez zdejmowania, wiec bez zatrzymania animacji. Silnik szuka animacji od gory listy (StartAni 0x57B070,
	// GetAniFromAniID 0x472F50), a wpis 0 (bazowy model) zostaje na miejscu. Nakladki spoza 'order' laduja na wierzchu.
	static bool ReorderOverlays(zCModel* m, const std::vector<std::string>& order) {
		int n = m->modelProtoList.GetNum();
		if (n <= 2)
			return false;
		std::vector<zCModelPrototype*> cur, out;
		for (int i = 1; i < n; i++)
			cur.push_back(m->modelProtoList[i]);
		std::vector<bool> used(cur.size(), false);
		for (auto& name : order)
			for (size_t i = 0; i < cur.size(); i++)
				if (!used[i] && cur[i] && ProtoKey(cur[i]) == name) {
					used[i] = true;
					out.push_back(cur[i]);
					break;
				}
		for (size_t i = 0; i < cur.size(); i++)
			if (!used[i])
				out.push_back(cur[i]);
		bool changed = false;
		for (size_t i = 0; i < out.size(); i++)
			if (m->modelProtoList[(int)i + 1] != out[i]) {
				m->modelProtoList[(int)i + 1] = out[i];
				changed = true;
			}
		return changed;
	}

	static std::string Join(const std::vector<std::string>& v) {
		std::string s;
		for (auto& x : v)
			s += (s.empty() ? "" : " ") + x;
		return s.empty() ? "-" : s;
	}

	static int AniId(zCModel* m, const char* name) {
		return m->GetAniIDFromAniName(zSTRING(name));
	}

	static zCModelAni* ProtoAni(zCModelPrototype* p, int id) {
		return p && id >= 0 && id < p->protoAnis.GetNum() ? p->protoAnis[id] : nullptr;
	}

	// Czy ktoras obca nakladka sprintu na modelu ma te animacje (sprint gry ma petle dla tej broni).
	static bool ForeignSprintHasAni(zCModel* m, int id) {
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (p && ProtoAni(p, id) && zdcore::IsForeignSprintOverlay(ProtoKey(p)))
				return true;
		}
		return false;
	}

	// Nasza nakladka sprintu -> jej petle (S_RUNL + S_MAGRUNL / S_1HRUNL / S_2HRUNL), inna -> {}.
	static std::vector<std::string> SprintLoopsOf(const std::string& overlay) {
		return zdcore::OurSprintLoops(overlay);
	}

	// Wersja animacji, ktora gralaby bez naszych nakladek sprintu (od gory listy; model bazowy na koncu).
	static zCModelPrototype* ProtoWithoutOurSprint(zCModel* m, int id) {
		for (int i = m->modelProtoList.GetNum() - 1; i >= 0; i--) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!ProtoAni(p, id) || (i > 0 && zdcore::IsOurSprintOverlay(ProtoKey(p))))
				continue;
			return p;
		}
		return nullptr;
	}

	// Tempo sprintu W3 = tempo sprintu gry: gdy nasza petla sprintu gra zamiast petli z obcej nakladki sprintu, jej
	// fpsRate (i przejsc) x skala = ta sama predkosc korzenia co petla gry (zCModelAni::GetAniVelocity, cm/ms). zEBA
	// przyspiesza wylacznie nakladka (brak zmian timeScale / fps w zeba.dll), wiec tak nie ma podwojnego przyspieszenia.
	// W kazdym innym wypadku (podglad, nakladka na spodzie, restoreOnly) - fpsRate z pliku. Kazda petla nakladki osobno
	// (HUMANS_ZDF_W3_SPRINT: S_RUNL i S_MAGRUNL - sprint gry z czarem w reku ma wlasna petle i tempo).
	static void SyncSprintTempo(zCModel* m, bool restoreOnly) {
		sprintScaleNow.clear();
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p)
				continue;
			std::string ovl = ProtoKey(p);
			std::vector<std::string> loops = SprintLoopsOf(ovl);
			for (size_t li = 0; li < loops.size(); li++) {
				const char* loop = loops[li].c_str();
				// klucz tempa / logu: nakladka dla pierwszej petli (jak dotad), "nakladka/petla" dla kolejnych
				std::string name = li == 0 ? ovl : ovl + "/" + loops[li];
				int loopId = AniId(m, loop);
				zCModelAni* our = ProtoAni(p, loopId);
				if (!our)
					continue;
				auto baseOf = [&](zCModelAni* a) -> float {
					std::string key = ovl + "|" + std::to_string(a->aniID);
					auto it = sprintBaseFps.find(key);
					if (it == sprintBaseFps.end())
						it = sprintBaseFps.emplace(key, a->fpsRate).first;
					return it->second;
				};
				float scale = 1.0f;
				std::string refName;
				float vRef = 0.0f, vOur = 0.0f;
				if (!restoreOnly && m->GetAniFromAniID(loopId) == our) {
					zCModelPrototype* ref = ProtoWithoutOurSprint(m, loopId);
					zCModelAni* refAni = ProtoAni(ref, loopId);
					std::string refKey = ref ? ProtoKey(ref) : "";
					if (refAni && zdcore::IsForeignSprintOverlay(refKey) && our->fpsRate > 0.0f) {
						refName = refKey;
						vRef = refAni->GetAniVelocity();
						vOur = our->GetAniVelocity() * baseOf(our) / our->fpsRate;   // predkosc przy fps z pliku
						// z bronia (S_1HRUNL / S_2HRUNL) wlasne tempo W3SprintArmedSpeed (600 cm/s, petla z make_loco tez 600)
						// zamiast sprintu zEBA z bronia (474); bez broni i z czarem w reku (S_RUNL / S_MAGRUNL - ta sama petla
						// Geralta) W3SprintUnarmedSpeed (0 = jak sprint gry - w NB sprint maga 451 zwalnialby Geralta ponizej
						// sprintu z mieczem); cm/s -> cm/ms jak GetAniVelocity
						bool armed = zdcore::SprintLoopArmed(loop);
						float own = armed ? zcfg.w3SprintArmedSpeed : zcfg.w3SprintUnarmedSpeed;
						float want = own > 0.0f ? own / 1000.0f : 0.0f;
						scale = zdcore::SprintTargetScale(want, vRef, vOur);
						if (want > 0.0f) {
							refName += armed ? " (z bronia W3SprintArmedSpeed)" : " (bez broni W3SprintUnarmedSpeed)";
							vRef = want;
						}
					}
				}
				for (auto& aniName : zdcore::SprintAniNames(loop)) {
					zCModelAni* a = ProtoAni(p, AniId(m, aniName.c_str()));
					if (!a)
						continue;
					float target = baseOf(a) * scale;
					if (fabsf(a->fpsRate - target) > 1e-4f)
						a->fpsRate = target;
				}
				if (!refName.empty())
					sprintScaleNow[name] = scale;
				auto logged = sprintScaleLogged.find(name);
				if (!refName.empty() && (logged == sprintScaleLogged.end() || fabsf(logged->second - scale) > 0.01f)) {
					sprintScaleLogged[name] = scale;
					StyleDiag("Sprint W3 [kl. %u]: %s tempo x%.2f wg %s (%.0f / %.0f cm/s)%s", zdFrame, loop, scale, refName.c_str(),
						vRef * 1000.0f, vOur * 1000.0f, scale == 1.0f && vRef > 0.0f && fabsf(vRef - vOur) > 0.02f * vOur ?
						" - stosunek poza 0,5..2, zostaje tempo wlasne" : "");
				}
			}
		}
	}

	void StylesRemove() {
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m || !HeroIsHuman())
			return;   // po przemianie nasze nakladki zostaja na ludzkim modelu razem z zapisem, ktore sa nasze
		SyncSprintTempo(m, true);   // prototyp zostaje po zdjeciu (nasza referencja PreloadOverlays / inne modele) - bez naszej skali
		std::vector<LocoSnap> snap = SnapLocoAnis(m);
		bool removed = false;
		for (auto& a : appliedStyles)
			if (a.mine && ModelOverlayIndex(m, a.name) >= 0) {
				ZRemoveOverlayTimed(m, a.name + ".MDS");
				removed = true;
			}
		appliedStyles.clear();
		if (removed)
			SyncLocoAnis(m, snap);
	}

	void StylesForget() {
		appliedStyles.clear();   // nowy swiat / zapis - stary model nie istnieje
		deferSince = 0;
	}

	// W trakcie ciosu nie przekladamy nakladek (zdjecie nakladki przerywa jej aktywna animacje). Cios = aktywna animacja
	// ciosu (oCAniCtrl_Human::hitAniID, -1 od konstruktora). NIE oCAniCtrl_Human::IsInPostHit (0x6B13A0): zwraca 1 takze
	// wtedy, gdy zadnej animacji ciosu nie ma (GetActiveAni == 0 -> mov eax,1) - przez to plan nakladek nie ruszal
	// nigdy poza klawiszem stylu (U): trucht W3 dopiero po U, sprint W3 nigdy nie wchodzil nad sprint zEBA.
	static bool HeroAttacking() {
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!ac || !m || ac->hitAniID < 0)
			return false;
		zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
		return a && !a->isFadingOut;
	}

	// Zestaw ruchu: AUTO wg ini (W3Locomotion, W3Sprint) albo wymuszony przelacznikiem (LocoKey, zdism loco, LocoMode).
	static zdcore::LocoFlags CurrentLoco() {
		return zdcore::ResolveLocoMode(zcfg.locoMode, zcfg.w3Locomotion, zcfg.w3Sprint);
	}

	// Co bohater ma miec: style broni, ruch W3 (z bronia tylko przy stylu W3), sprint W3. Sprint W3 lezy na stale na
	// spodzie listy, a gdy gra sprintuje (albo podglad sprintu) - nad jej sprintem (zdcore::WantedMotion).
	static zdcore::MotionSetup CurrentSetup(const zdcore::LocoFlags& f) {
		zdcore::MotionSetup s;
		bool sprint = f.sprint && sprintAvailable;
		for (auto& slot : styleSlots) {
			std::string w = WantedStyle(slot);
			bool w3 = w == slot.w3Overlay;
			if (!w.empty())
				s.styles.push_back(w);
			if (w3 && f.loco && slot.locoAvailable)
				s.weaponLoco.push_back(slot.locoOverlay);
			if (w3 && sprint && slot.sprintAvailable)
				s.weaponSprint.push_back(slot.sprintOverlay);
		}
		if (f.loco && locoAvailable)
			s.loco = LOCO_OVERLAY;
		if (sprint)
			s.sprint = SPRINT_OVERLAY;
		// dobycie / chowanie zza plecow W3 (BackSwords.cpp): nad nakladkami talentu i zEBA (maja wlasne dobycie)
		for (auto& o : BackSwordsOverlays())
			s.styles.push_back(o);
		return s;
	}

	// Nasze nakladki sprintu, ktorych petli nie ma obcy sprint na modelu (np. HUMANS_SPRINT mikstury szybkosci - tylko
	// S_RUNL): zostaja na spodzie, zeby z bronia nie biec sprintem W3, gdy gra biegnie zwyczajnie.
	static std::vector<std::string> SprintsWithoutGameLoop(zCModel* m, const zdcore::MotionSetup& s) {
		std::vector<std::string> out;
		if (!s.sprint.empty() && !ForeignSprintHasAni(m, AniId(m, "S_RUNL")))
			out.push_back(s.sprint);
		for (auto& slot : styleSlots)
			if (std::find(s.weaponSprint.begin(), s.weaponSprint.end(), slot.sprintOverlay) != s.weaponSprint.end() &&
				!ForeignSprintHasAni(m, AniId(m, slot.runAni)))
				out.push_back(slot.sprintOverlay);
		return out;
	}

	// Nakladka stylu albo ruchu z naszej paczki (natywne kopie, W3) - nasza, nawet gdy nie ma jej w appliedStyles.
	static bool IsOurStyleOverlay(const std::string& name) {
		if (name.rfind("HUMANS_ZDF_NAT_", 0) == 0 || name == LOCO_OVERLAY || name == SPRINT_OVERLAY || BackSwordsIsOurOverlay(name))
			return true;
		for (auto& slot : styleSlots)
			if (name == slot.w3Overlay || name == slot.locoOverlay || name == slot.sprintOverlay)
				return true;
		return false;
	}

	static void DisableOverlaySource(const std::string& name) {
		for (auto& f : forcedStyle)
			if (!f.empty() && (f == name || zdcore::SwapStyleCounterpart(f, zdcore::SWAP_TYPE_1H) == name ||
				zdcore::SwapStyleCounterpart(f, zdcore::SWAP_TYPE_2H) == name))
				f.clear();
		for (auto& slot : styleSlots) {
			if (name == slot.w3Overlay)
				slot.w3Available = false;
			if (name == slot.locoOverlay)
				slot.locoAvailable = false;
			if (name == slot.sprintOverlay)
				slot.sprintAvailable = false;
		}
		if (name == LOCO_OVERLAY)
			locoAvailable = false;
		if (name == SPRINT_OVERLAY)
			sprintAvailable = false;
		BackSwordsOverlayFailed(name);
	}

	// Diagnostyka (przed planem, wiec widac tez zmiany zEBA z poprzedniej klatki): lista nakladek bohatera i nakladka,
	// z ktorej gra petla ruchu (S_RUNL, S_1HRUNL...). Petla logowana tylko, gdy inna niz ostatnio zalogowana.
	static void DiagHero(zCModel* m, const std::vector<std::string>& model) {
		if (diagLeft <= 0)
			return;
		std::vector<std::string> shown;
		for (auto& o : model)
			if (!zdcore::IsTransientOverlay(o))
				shown.push_back(o);
		std::string list = Join(shown);
		if (list != diagOverlays) {
			diagOverlays = list;
			StyleDiag("Nakladki bohatera [kl. %u]: %s", zdFrame, list.c_str());
		}
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* ani = a ? a->protoAni : nullptr;
			if (!ani || a->isFadingOut || !zdcore::IsMoveLoopAni(ani->aniName.ToChar()))
				continue;
			std::string loop = std::string(ani->aniName.ToChar()) + " z " + OverlayOfAni(m, ani);
			if (loop != diagLoop) {
				diagLoop = loop;
				std::string fs = zdcore::ForeignSprintOverlay(model);
				StyleDiag("Petla ruchu [kl. %u]: %s (sprint gry: %s, W3Sprint=%d, LocoMode=%d)", zdFrame, loop.c_str(),
					fs.empty() ? "nie" : fs.c_str(), (int)zcfg.w3Sprint, zcfg.locoMode);
			}
			break;
		}
	}

	// Chciane nakladki ponad nakladkami talentu / zEBA (plan w zdcore::PlanOverlays, sprint gry moze lezec nad nami).
	// Obecne na modelu tylko przekladamy (bez zatrzymania animacji); zdejmujemy i zakladamy tylko to, czego brakuje
	// albo co juz niechciane - wtedy animacje ruchu wracaja z faza (SyncLocoAnis). Nakladek, ktore byly na modelu przed
	// nami (mine=false - np. styl zEBA wymuszony w cyklu), nigdy nie zdejmujemy - przesuwamy je tylko, dopoki chciane.
	static void ApplyStyles(bool force) {
		ZPROF_SCOPE("Styles.Apply");   // plan nakladek bohatera co klatke
		// w medytacji bez zmian nakladek: zalozenie / zdjecie brakujacej zatrzymaloby kleczenie (Meditate.cpp)
		if (!player || !HeroIsHuman() || DirectorActive() || MovesActive() || MeditateActive() ||
			TraversalHoldsMotion() ||   // 24.10: w trakcie przeskoku / skoku z biegu bez zmian nakladek ruchu W3
			 (!force && HeroAttacking()))
			return;
		zCModel* m = player->GetModel();
		if (!m)
			return;
		std::vector<std::string> model = ModelOverlays(m);
		DiagHero(m, model);
		std::string foreignSprint = zdcore::ForeignSprintOverlay(model);
		zdcore::LocoFlags loco = CurrentLoco();
		zdcore::MotionSetup setup = CurrentSetup(loco);
		// wierzchowiec (NB HUMANS_MOUNT): nic naszego nad nakladka jazdy - plan zdejmuje nasze nakladki ruchu i stylow
		static bool mountedLogged = false;
		bool mounted = zdcore::HasExclusiveForeignOverlay(model);
		if (mounted)
			setup = zdcore::MotionSetup();
		if (mounted != mountedLogged) {
			mountedLogged = mounted;
			ZLog("Ruch: %s", mounted ? "bohater na wierzchowcu (nakladka jazdy na modelu) - nakladki ruchu / stylow W3 zdjete, "
				"uniki i przeskoki wylaczone" : "koniec jazdy - nakladki ruchu / stylow W3 wracaja");
		}
		std::vector<std::string> keepParked;
		if (!foreignSprint.empty() && !loco.sprintOnTop)
			keepParked = SprintsWithoutGameLoop(m, setup);
		zdcore::WantedSet wanted = zdcore::WantedMotion(setup, !foreignSprint.empty() || loco.sprintOnTop, keepParked);
		// gra sprintuje bez sprintu W3 na wierzchu: nic naszego nie laduje nad jej sprintem
		bool ourSprintUp = false;
		for (auto& w : wanted.top)
			ourSprintUp = ourSprintUp || zdcore::IsOurSprintOverlay(w);
		bool sprintGameOnTop = !foreignSprint.empty() && !ourSprintUp;
		std::vector<std::string> mine;
		for (auto& a : appliedStyles)
			if (a.mine)
				mine.push_back(a.name);
		for (auto& o : model)
			if (IsOurStyleOverlay(o) && std::find(mine.begin(), mine.end(), o) == mine.end())
				mine.push_back(o);
		zdcore::OverlayPlan plan = zdcore::PlanOverlays(model, wanted.top, wanted.parked, mine);
		if (force)
			plan.apply = wanted.top;
		if (plan.remove.empty() && plan.park.empty() && plan.apply.empty()) {
			deferSince = 0;
			return;
		}
		bool stops = zdcore::PlanStopsAnis(model, plan);
		if (stops && !force) {
			std::string blocking = BlockingAni(m);
			if (!blocking.empty()) {
				if (!deferSince) {
					deferSince = zdFrame ? zdFrame : 1;
					StyleDiag("Nakladki: zmiana odlozona - trwa %s (zdjecie / zalozenie zatrzymaloby ja)", blocking.c_str());
				}
				if (zdFrame - deferSince < DEFER_MAX_FRAMES)
					return;
				StyleDiag("Nakladki: %s trwa za dlugo - zmieniam mimo to", blocking.c_str());
			}
		}
		deferSince = 0;
		// czy nakladka jest nasza: z poprzedniego zapisu, a dla nowej - czy jej nie bylo na modelu
		auto wasMine = [&](const std::string& name) {
			if (IsOurStyleOverlay(name))
				return true;
			for (auto& a : appliedStyles)
				if (a.name == name)
					return a.mine;
			return ModelOverlayIndex(m, name) < 0;
		};
		std::vector<AppliedStyle> next;
		for (auto& w : wanted.top)
			next.push_back({ w, wasMine(w) });
		for (auto& w : wanted.parked)
			next.push_back({ w, wasMine(w) });
		std::vector<std::string> target = zdcore::ApplyPlan(model, plan, sprintGameOnTop);
		std::vector<LocoSnap> snap = SnapLocoAnis(m);
		int engineStops = 0;
		for (auto& r : plan.remove)
			if (zdcore::IsOurSprintOverlay(r)) {
				SyncSprintTempo(m, true);   // zdejmowany sprint W3 wraca do tempa z pliku (prototyp zostaje w silniku)
				break;
			}
		// niechciane: zdejmij tylko nasze (cudze zostaja tam, gdzie sa)
		for (auto& r : plan.remove)
			if (ModelOverlayIndex(m, r) >= 0) {
				ZRemoveOverlayTimed(m, r + ".MDS");
				engineStops++;
			}
		// brakujace: zaloz (silnik dopisuje na wierzch), miejsce ustawia ReorderOverlays; prototyp z pamieci, gdy
		// wczytany zawczasu (PreloadOverlays) - inaczej MSB i MAN-y z dysku w tej klatce
		for (auto& name : target) {
			if (ModelOverlayIndex(m, name) >= 0)
				continue;
			engineStops++;
			if (!ZApplyOverlayTimed(m, name + ".MDS", "styl")) {
				// nie da sie zalozyc - wylacz zrodlo (bez ponawiania w kolejnych klatkach)
				ZLog("Style: nie podpieto %s - pomijam", name.c_str());
				DisableOverlaySource(name);
			}
		}
		bool moved = ReorderOverlays(m, target);
		appliedStyles.clear();
		for (auto& a : next)
			if (ModelOverlayIndex(m, a.name) >= 0)
				appliedStyles.push_back(a);
		std::string synced = SyncLocoAnis(m, snap);
		StyleDiag("Nakladki [kl. %u]: zdejmij %s | spod %s | wierzch %s | stop animacji %d, przelozenie %s | ruch:%s",
			zdFrame, Join(plan.remove).c_str(), Join(plan.park).c_str(), Join(plan.apply).c_str(), engineStops,
			moved ? "tak" : "nie", synced.empty() ? " bez zmian" : synced.c_str());
	}

	// Zestaw stylow dla animacji broni w reku (przy zamianie animacji 1H <-> 2H - drugiego typu).
	static StyleSlot* SlotForWeapon() {
		int wm = AniSwapAniWeaponMode(player ? player->GetWeaponMode() : 0);
		return wm == NPC_WEAPON_1HS ? &styleSlots[0] : wm == NPC_WEAPON_2HS ? &styleSlots[1] : nullptr;
	}

	static std::string Describe(StyleSlot& slot) {
		const char* kind = &slot == &styleSlots[0] ? "1H" : "2H";
		char buf[256];
		int forced = ForcedIndex(slot);
		if (forced < 0) {
			std::string w = WantedStyle(slot);
			sprintf_s(buf, "Styl %s: AUTO (%s)", kind, w.empty() ? "gra / zEBA" : zdcore::StyleLabel(w).c_str());
		}
		else
			sprintf_s(buf, "Styl %s: %s [%d/%d]", kind, zdcore::StyleLabel(slot.styles[forced]).c_str(),
				forced + 1, (int)slot.styles.size());
		return buf;
	}

	std::string StyleCycle(int step) {
		int dual = HeroDualState();
		if (dual != zdcore::DUAL_NONE && player && player->GetWeaponMode() == NPC_WEAPON_2HS) {
			// para NB: zawsze natywne animacje NB (W3 nie ma animacji dwoch broni) - klawisz tylko informuje
			const char* text = dual == zdcore::DUAL_PAIR ? "Dwie bronie (NB): animacje New Balance" : "Bro\xF1 parowa (NB): animacje New Balance";
			if (screen)
				screen->PrintTimedCXY(text, 2500.0f, nullptr);
			ZLog("Styl: bron parowa NB %s - natywne animacje NB (HUMANS_1X2ST3 / 2X2ST3), klawisz stylu bez zmian; wybor 1H / 2H "
				"wraca po odlozeniu pary", zdcore::DualLabel(dual));
			return std::string("bron parowa NB ") + zdcore::DualLabel(dual) + " - natywne animacje NB (styl bez zmian)";
		}
		StyleSlot* slot = SlotForWeapon();
		if (!slot)
			return "dobadz broni 1H albo 2H";
		// zestaw animacji broni w rece (przy zamianie drugiego typu); wybor zapamietany dla rodzaju broni w rece
		int next = step == 0 ? -1 : zdcore::CycleStyle(ForcedIndex(*slot), (int)slot->styles.size(), step);
		forcedStyle[player->GetWeaponMode() == NPC_WEAPON_2HS ? 1 : 0] = next >= 0 ? slot->styles[next] : "";
		ApplyStyles(true);
		std::string d = Describe(*slot);
		if (screen)
			screen->PrintTimedCXY(d.c_str(), 2500.0f, nullptr);
		ZLog("%s", d.c_str());
		return d;
	}

	std::string StyleList() {
		std::string s;
		for (auto& slot : styleSlots) {
			s += &slot == &styleSlots[0] ? "1H:" : " | 2H:";
			int forced = ForcedIndex(slot);
			for (size_t i = 0; i < slot.styles.size(); i++)
				s += ((int)i == forced ? " *" : " ") + slot.styles[i].substr(7);
		}
		return s;
	}

	// ---------------------------------------------------------------- przelacznik ruchu (LocoKey, zdism loco)

	// Petla biegu dla broni w reku (bez broni / piesci itd. - jej wlasna; przy zamianie animacji - drugiego typu),
	// "" = tylko S_RUNL.
	static const char* WeaponRunLoop() {
		switch (AniSwapAniWeaponMode(player ? player->GetWeaponMode() : 0)) {
		case NPC_WEAPON_FIST: return "S_FISTRUNL";
		case NPC_WEAPON_DAG:
		case NPC_WEAPON_1HS: return "S_1HRUNL";
		case NPC_WEAPON_2HS: return "S_2HRUNL";
		case NPC_WEAPON_BOW: return "S_BOWRUNL";
		case NPC_WEAPON_CBOW: return "S_CBOWRUNL";
		case NPC_WEAPON_MAG: return "S_MAGRUNL";
		default: return "";
		}
	}

	// Skad gra petla: wersja z wierzchu listy (GetAniFromAniID - tak wybiera StartAni) -> nakladka.
	static std::string LoopSource(zCModel* m, const char* aniName) {
		int id = AniId(m, aniName);
		zCModelAni* ani = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
		if (!ani)
			return "-";
		std::string o = OverlayOfAni(m, ani);
		std::string l = zdcore::MotionSourceLabel(o);
		return l == o ? o : l + " (" + o + ")";
	}

	// Stan ruchu bohatera w kilku liniach: tryb, skad petle biegu, sprint gry i tempo, petla grana teraz.
	static std::vector<std::string> LocoStatusLines() {
		std::vector<std::string> lines;
		char buf[512];
		zdcore::LocoFlags f = CurrentLoco();
		sprintf_s(buf, "Ruch [%s]: %s (%d/%d)%s", zdcore::KeyLabel(zcfg.locoKey).c_str(), zdcore::LocoModeLabel(zcfg.locoMode),
			zcfg.locoMode + 1, (int)zdcore::LOCO_MODE_COUNT, zcfg.locoMode == zdcore::LOCO_AUTO ?
			(std::string(" - trucht W3 ") + (f.loco ? "wl." : "wyl.") + ", sprint W3 " + (f.sprint ? "wl." : "wyl.")).c_str() : "");
		lines.push_back(buf);
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m || !HeroIsHuman()) {
			lines.push_back("bohater nie jest czlowiekiem - nakladki ruchu nieaktywne");
			return lines;
		}
		std::string loops = "S_RUNL: " + LoopSource(m, "S_RUNL");
		const char* wl = WeaponRunLoop();
		if (*wl)
			loops += std::string(" | ") + wl + ": " + LoopSource(m, wl);
		lines.push_back(loops);
		std::vector<std::string> fs = zdcore::ForeignSprintOverlays(ModelOverlays(m));
		std::string sprint = "Sprint gry: ";
		if (fs.empty())
			sprint += "nie";
		for (size_t i = 0; i < fs.size(); i++)
			sprint += (i ? " + " : "") + fs[i] + " (" + zdcore::SprintModLabel(fs[i]) + ")";
		if (fs.size() > 1)
			sprint += " - DWA SPRINTY NARAZ";
		for (auto& s : sprintScaleNow) {
			sprintf_s(buf, " | tempo %s x%.2f", s.first.c_str() + 14, s.second);   // bez "HUMANS_ZDF_W3_"
			sprint += buf;
		}
		if (fabsf(m->timeScale - 1.0f) > 0.02f) {
			sprintf_s(buf, " | tempo modelu x%.2f", m->timeScale);
			sprint += buf;
		}
		lines.push_back(sprint);
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			zCModelAni* ani = a ? a->protoAni : nullptr;
			if (!ani || a->isFadingOut || !zdcore::IsMoveLoopAni(ani->aniName.ToChar()))
				continue;
			lines.push_back(std::string("Gra teraz: ") + ani->aniName.ToChar() + " z " + OverlayOfAni(m, ani));
			break;
		}
		return lines;
	}

	static std::string ShowLocoStatus(bool toScreen, float ms) {
		std::vector<std::string> lines = LocoStatusLines();
		std::string all;
		for (size_t i = 0; i < lines.size(); i++) {
			all += (i ? " | " : "") + lines[i];
			if (toScreen && screen)
				screen->PrintTimedCX(1500 + (int)i * 280, zSTRING(lines[i].c_str()), ms, nullptr);
		}
		return all;
	}

	std::string LocoSetMode(int mode) {
		if (mode < 0 || mode >= zdcore::LOCO_MODE_COUNT)
			mode = zdcore::LOCO_AUTO;
		zcfg.locoMode = mode;
		zoptions->WriteInt("ZDISMEMBER", "LocoMode", mode, FALSE);   // menu i nastepne uruchomienie
		ApplyStyles(true);
		std::string s = ShowLocoStatus(true, 3000.0f);
		ZLog("%s", s.c_str());
		return s;
	}

	std::string LocoCycle(int step) {
		return LocoSetMode(step == 0 ? (int)zdcore::LOCO_AUTO : zdcore::CycleLocoMode(zcfg.locoMode, step));
	}

	std::string LocoList() {
		std::string s = "zestawy ruchu:";
		for (int i = 0; i < zdcore::LOCO_MODE_COUNT; i++)
			s += std::string(i == zcfg.locoMode ? " *" : " ") + std::to_string(i) + "=" + zdcore::LocoModeLabel(i);
		return s + " (klawisz " + zdcore::KeyLabel(zcfg.locoKey) + ", Shift = wstecz)";
	}

	std::string LocoInfo() {
		return ShowLocoStatus(true, 4000.0f);
	}

	std::string LocoWatchToggle() {
		locoWatch = !locoWatch;
		locoWatchNext = 0;
		return locoWatch ? "stan ruchu na ekranie co 0,8 s (zdism loco watch - wylacz)" : "stan ruchu na ekranie wylaczony";
	}

	// Dwa mody sprintu naraz (dwie obce nakladki sprintu na modelu) albo sprint przez tempo modelu (timeScale) - raz na
	// sesje dla kazdego zestawu. Tempo modelu razem z nakladka sprintu = podwojne przyspieszenie.
	static void WatchSprintMods(zCModel* m, const std::vector<std::string>& model) {
		std::vector<std::string> fs = zdcore::ForeignSprintOverlays(model);
		if (fs.size() > 1) {
			std::string key = Join(fs);
			if (!sprintClashLogged[key]) {
				sprintClashLogged[key] = true;
				std::string mods;
				for (auto& f : fs)
					mods += (mods.empty() ? "" : " + ") + f + " (" + zdcore::SprintModLabel(f) + ")";
				ZLog("Sprint: UWAGA dwa sprinty naraz: %s - gra animacja z najwyzszej nakladki, kazdy mod konczy sprint po "
					"swojemu; wylacz jeden (zEBA: EnableSprint=0, Ninja Sprint: Ninja_Sprint_UseKey=0000 i Perma=1)", mods.c_str());
			}
		}
		if (fabsf(m->timeScale - 1.0f) > 0.02f) {
			char key[64];
			sprintf_s(key, "timeScale %.2f%s", m->timeScale, fs.empty() ? "" : " +nakladka");
			if (!sprintClashLogged[key]) {
				sprintClashLogged[key] = true;
				ZLog("Sprint: tempo modelu bohatera x%.2f - inny mod przyspiesza animacje bohatera (np. NaturalSprint bez animacji)%s",
					m->timeScale, fs.empty() ? "" : " RAZEM z nakladka sprintu = podwojne przyspieszenie, wylacz jeden z modow");
			}
		}
	}

	// Co klatke po planie: tempo sprintu W3, kontrola kolizji sprintu, napis ze stanem (zdism loco watch).
	static void HeroMotionUpdate() {
		ZPROF_SCOPE("Styles.Motion");   // sprint / tempo ruchu W3
		zCModel* m = player && HeroIsHuman() ? player->GetModel() : nullptr;
		if (!m)
			return;
		WatchSprintMods(m, ModelOverlays(m));
		// 24.10: tempo sprintu W3 zamrozone na czas przeskoku / wyjscia w bieg / skoku z biegu (petla, z ktorej wszedl ruch,
		// ma te sama predkosc do konca ruchu)
		if (!TraversalHoldsMotion())
			SyncSprintTempo(m, false);
		if (locoWatch) {
			DWORD now = GetTickCount();
			if (now >= locoWatchNext) {
				locoWatchNext = now + LOCO_WATCH_MS;
				ShowLocoStatus(true, (float)LOCO_WATCH_MS);
			}
		}
	}

	static bool StyleKeyEdge(int key, bool& prev) {
		bool down = key > 0 && zinput && zinput->KeyPressed(key) != 0;
		bool pressed = down && !prev;
		prev = down;
		return pressed;
	}

	void StylesUpdate() {
		bool stylePressed = StyleKeyEdge(zcfg.styleKey, prevStyleKey);
		bool locoPressed = StyleKeyEdge(zcfg.locoKey, prevLocoKey);
		// Ctrl+Shift+U (klawisz zamiany animacji na tym samym klawiszu) - tylko zamiana, bez cyklu stylu
		if (stylePressed && AniSwapComboHeld(zcfg.styleKey))
			stylePressed = false;
		if (locoPressed && AniSwapComboHeld(zcfg.locoKey))
			locoPressed = false;
		// Ctrl + przecinek = medytacja (MeditateKeyAlt, New Balance: przecinek to LocoKey) - bez cyklu ruchu; w trakcie
		// medytacji klawisze cyklu nie dzialaja (okno ma klawiature)
		if (locoPressed && (MeditateComboHeld(zcfg.locoKey) || MeditateActive()))
			locoPressed = false;
		if (stylePressed && (MeditateComboHeld(zcfg.styleKey) || MeditateActive()))
			stylePressed = false;
		// Wymuszony zestaw ruchu (LocoMode != AUTO) zostaje w Gothic.ini - raz na sesje napis, zeby nie zostal niezauwazony
		// (16.10: LocoMode=4 = podglad sprintu jako zwykly bieg wygladal jak "sprint wlaczony domyslnie").
		static bool locoForcedShown = false;
		if (!locoForcedShown && player && zcfg.locoMode != zdcore::LOCO_AUTO && ogame && ogame->GetTextView()) {
			locoForcedShown = true;
			ZLog("Ruch: UWAGA wymuszony zestaw %s (LocoMode=%d z Gothic.ini) - AUTO: klawisz cyklu albo zdism loco auto",
				zdcore::LocoModeLabel(zcfg.locoMode), zcfg.locoMode);
			ShowLocoStatus(true, 6000.0f);
		}
		// Przy otwartym ekwipunku klawisze cyklu nie dzialaja: O (LocoKey=24 = DIK_O) to tez sortowanie zInvSort.
		if ((stylePressed || locoPressed) && player && !(zcon && zcon->IsVisible()) && !player->inventory2.IsOpen() &&
			oCInformationManager::GetInformationManager().HasFinished()) {
			// Shift + klawisz = wstecz
			bool back = zinput->KeyPressed(KEY_LSHIFT) || zinput->KeyPressed(KEY_RSHIFT);
			if (stylePressed)
				StyleCycle(back ? -1 : +1);
			else
				LocoCycle(back ? -1 : +1);
			HeroMotionUpdate();
			return;
		}
		// Plan co klatke: zEBA zaklada / zdejmuje swoj sprint po naszym Game_Loop (hak za zCWorld::Render, 0x6C87EB),
		// a petle biegu silnik startuje juz w nastepnej klatce (zCVob::DoFrameActivity przed Game_Loop).
		ApplyStyles(false);
		HeroMotionUpdate();
	}
}
