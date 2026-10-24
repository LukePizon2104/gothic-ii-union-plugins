// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Medytacja W3 (opis w Meditate.h, logika bez silnika w Core\Meditate.*).
	// Napisy ekranowe w cp1250 (jak w Traversal.cpp): \xB3 = l z kreska, \xF3 = o z kreska, \xBF = z z kropka,
	// \xE6 = c z kreska, \xEA = e z ogonkiem, \xB9 = a z ogonkiem, \xF1 = n z kreska, \xAF = Z z kropka.
	static const char* MED_OVERLAY_DEF = "HUMANS_ZDF_W3_MEDITATE";
	// Nazwy animacji musza istniec w bazowym HUMANS (nakladka z nowa nazwa = "ani in overlay not present in base",
	// silnik odrzuca cala nakladke) - jak Znaki (R_CHAIR_RANDOM_*). Nakladka jest zalozona tylko na czas medytacji,
	// wiec modlitwa przy kapliczce i sen na ziemi poza nia zostaja natywne. T_PRAY_2_STAND to w HUMANS alias.
	static const char* MED_ANI_START_DEF = "T_STAND_2_PRAY";
	static const char* MED_ANI_LOOP_DEF = "S_PRAY";
	static const char* MED_ANI_STOP_DEF = "T_SLEEPGROUND_2_STAND";
	static const float MED_ENEMY_RADIUS = 1500.0f;     // walczacy wrog blizej = bez medytacji / wstanie (cm)
	static const float MED_ENEMY_RECHECK = 0.2f;       // s miedzy przegladami wrogow (w czekaniu co klatke)
	static const DWORD MED_HOOK_STALE_MS = 250;        // hak klawisza bez petli gry dluzej (menu, pauza) - bez decyzji
	static const float MED_CONSOLE_WAIT = 15.0f;       // zdism meditate: start po zamknieciu konsoli, najwyzej tyle s
	static const double MED_REAL_MS_MAX = 100.0;       // dluzsza klatka (przycinka, wczytanie) liczona jako tyle ms
	static const double MED_ENGINE_CAP_MS = 100.0;     // zCTimer::ResetTimer tnie klatke gry do 100 ms
	static const DWORD MED_TELL_GAP_MS = 1500;         // napis odmowy najwyzej co tyle

	enum MedRowId { MED_ROW_WAIT = 1, MED_ROW_UNTIL, MED_ROW_SPEED, MED_ROW_REGEN, MED_ROW_GOD, MED_ROW_RISE };

	struct MedNames {
		std::string overlay, start, loop, stop;
	};
	static MedNames mdN;

	struct MedAnis {
		zCModelAni* start = nullptr;
		zCModelAni* loop = nullptr;
		zCModelAni* stop = nullptr;
	};

	static zdcore::MedMachine md;
	static zdcore::MenuList mdMenu;
	static int mdLastRow = MED_ROW_WAIT;
	static int mdTab = 0;                             // 0 medytacja; New Balance: 1 gildia, 2 sciezki (PathMenu.cpp)
	static int mdWaitHours = 3;                       // pamietane w sesji
	static int mdUntil = zdcore::MED_UNTIL_MORNING;
	static int mdSpeed = 20;
	static int mdRegenMode = zdcore::MED_REGEN_GRADUAL;
	static std::vector<int> mdGods;                   // wybory daru aktywnego (puste = bez wiersza)
	static bool mdSettingsChanged = false;            // tempo / regeneracja zmienione w oknie - menu Union do odswiezenia
	static bool mdMovLock = false;                    // blokade sterowania zalozyl plugin
	static bool mdOverlayOn = false;
	static bool mdFactorSet = false;
	static float mdFactorPrev = 1.0f;
	static int mdFactorResets = 0;
	static bool mdScaleSet = false;
	static float mdScalePrev = 1.0f;
	static int mdHpPrev = -1;
	static zdcore::MedRegenAcc mdAcc;
	static int mdRegenHp = 0, mdRegenMana = 0;
	static double mdLastNow = 0.0;
	static double mdClockPrev = 0.0;
	static double mdPhaseMs = 0.0;                    // ZClockMs startu medytacji
	static double mdTopUpMs = 0.0, mdEngineMs = 0.0, mdRealMs = 0.0;
	static int mdWaitFrames = 0;
	static DWORD mdLoopTick = 0;
	static bool mdPending = false;
	static std::string mdPendingSrc;
	static float mdConsoleWait = 0.0f;
	static float mdEnemyAge = 1e9f;
	static bool mdEnemyNear = false;
	static std::string mdEnemyDesc;
	static DWORD mdLastTell = 0;

	// ---------- pomocnicze ----------

	static std::string MdIniName(const char* key, const char* def) {
		if (zoptions && zoptions->EntryExists("ZDISMEMBER", key)) {
			std::string v = zdcore::Upper(zdcore::Trim(zoptions->ReadString("ZDISMEMBER", key, def).ToChar()));
			if (!v.empty())
				return v;
		}
		return def;
	}

	// Nazwy nakladki i animacji: domyslne albo z Gothic.ini (klucze opcjonalne, nie dopisywane).
	static void MdReadNames() {
		mdN.overlay = MdIniName("MeditateOverlay", MED_OVERLAY_DEF);
		size_t dot = mdN.overlay.find('.');
		if (dot != std::string::npos)
			mdN.overlay = mdN.overlay.substr(0, dot);
		mdN.start = MdIniName("MeditateAniStart", MED_ANI_START_DEF);
		mdN.loop = MdIniName("MeditateAniLoop", MED_ANI_LOOP_DEF);
		mdN.stop = MdIniName("MeditateAniStop", MED_ANI_STOP_DEF);
	}

	// Pliki nakladki w paczkach (MSB + 3 x MAN). missing = czego brak (do logu).
	static bool MdFilesPresent(std::string& missing) {
		missing.clear();
		if (!ZOverlayExists(mdN.overlay, ""))
			missing = mdN.overlay + ".MSB";
		for (const std::string* a : { &mdN.start, &mdN.loop, &mdN.stop })
			if (!ZOverlayExists(mdN.overlay, *a))
				missing += (missing.empty() ? "" : ", ") + mdN.overlay + "-" + *a + ".MAN";
		return missing.empty();
	}

	static oCWorldTimer* MdTimer() {
		return ogame ? ogame->GetWorldTimer() : nullptr;
	}

	static double MdNow() {
		oCWorldTimer* t = MdTimer();
		return t ? zdcore::MedGameMinutes(t->day, t->worldTime) : 0.0;
	}

	// Doba w ms gry: zmienna, z ktora oCWorldTimer::Timer porownuje worldTime (0xAB371C w G2A), inaczej 24 x 250000.
	static double MdTicksPerDay() {
		if (ENGINE != Engine_G2A)
			return zdcore::MED_TICKS_PER_DAY;
		float v = *(float*)0x00AB371C;
		return v > 1e5f && v < 1e9f ? (double)v : zdcore::MED_TICKS_PER_DAY;
	}

	static int MdAttr(int i) {
		return player ? player->attribute[i] : 0;
	}

	// Do logu: dzien jak Wld_GetDay (od 0; na ekranie okna dzien + 1) i godzina.
	static std::string MdClockDay(double now) {
		int d, h, m;
		zdcore::MedSplitMinutes(now, d, h, m);
		char buf[48];
		sprintf_s(buf, "dzien %d %s", d, zdcore::MedClockText(now).c_str());
		return buf;
	}

	static zCModelPrototype* MdProto(zCModel* m) {
		if (!m)
			return nullptr;
		for (int i = 1; i < m->modelProtoList.GetNum(); i++) {
			zCModelPrototype* p = m->modelProtoList[i];
			if (!p)
				continue;
			std::string n = zdcore::Upper(p->modelProtoName.ToChar());
			size_t d = n.find('.');
			if ((d == std::string::npos ? n : n.substr(0, d)) == mdN.overlay)
				return p;
		}
		return nullptr;
	}

	// Animacja z NASZEJ nakladki (prototyp trzyma animacje wg ID z modelu bazowego - jak GetAniFromAniID 0x472F50).
	static zCModelAni* MdAni(zCModel* m, zCModelPrototype* proto, const std::string& name) {
		if (!m || !proto)
			return nullptr;
		int id = m->GetAniIDFromAniName(zSTRING(name.c_str()));
		if (id < 0 || id >= proto->protoAnis.GetNum())
			return nullptr;
		return proto->protoAnis[id];
	}

	static MedAnis MdGetAnis(zCModel* m) {
		MedAnis a;
		zCModelPrototype* p = MdProto(m);
		if (!p)
			return a;
		a.start = MdAni(m, p, mdN.start);
		a.loop = MdAni(m, p, mdN.loop);
		a.stop = MdAni(m, p, mdN.stop);
		return a;
	}

	// Gra nasza animacja - albo ta sama nazwa (ID) z innego prototypu, nie wygasajaca: next z MDS albo StartAni(id) moga
	// wziac wersje z innej nakladki. aniChannels to zwarta lista dlugosci numActiveAnis (dalej wpisy nieaktualne).
	static bool MdPlaying(zCModel* m, zCModelAni* a) {
		if (!m || !a)
			return false;
		if (m->IsAniActive(a))
			return true;
		int n = std::min(m->numActiveAnis, (int)zMDL_MAX_ANIS_PARALLEL);
		for (int i = 0; i < n; i++) {
			zCModelAniActive* ch = m->aniChannels[i];
			if (ch && ch->protoAni && ch->protoAni->aniID == a->aniID && !ch->isFadingOut)
				return true;
		}
		return false;
	}

	static bool MdWeaponMsg() {
		zCEventManager* em = player ? player->GetEM(FALSE) : nullptr;
		if (!em)
			return false;
		for (int i = 0; i < em->GetNumMessages(); i++)
			if (zDYNAMIC_CAST<oCMsgWeapon>(em->GetEventMessage(i)))
				return true;
		return false;
	}

	// Kolejka bohatera niepusta czyms innym niz bron (chowanie / dobycie czeka plugin w MED_SHEATHE).
	static bool MdOtherMsg() {
		zCEventManager* em = player ? player->GetEM(FALSE) : nullptr;
		if (!em)
			return false;
		for (int i = 0; i < em->GetNumMessages(); i++) {
			zCEventMessage* msg = em->GetEventMessage(i);
			if (msg && !zDYNAMIC_CAST<oCMsgWeapon>(msg))
				return true;
		}
		return false;
	}

	// Aktywna animacja ciosu (hitAniID) - jak LootHeroAttacking.
	static bool MdAttacking() {
		oCAniCtrl_Human* ac = player->GetAnictrl();
		zCModel* m = player->GetModel();
		if (!ac || !m || ac->hitAniID < 0)
			return false;
		zCModelAniActive* a = m->GetActiveAni(ac->hitAniID);
		return a && !a->isFadingOut;
	}

	static bool MdModsHeld(int mods) {
		if (!zinput)
			return false;
		auto down = [](int a, int b) { return zinput->KeyPressed(a) || zinput->KeyPressed(b); };
		if ((mods & 1) && !down(KEY_LCONTROL, KEY_RCONTROL))
			return false;
		if ((mods & 2) && !down(KEY_LSHIFT, KEY_RSHIFT))
			return false;
		if ((mods & 4) && !down(KEY_LMENU, KEY_RMENU))
			return false;
		return true;
	}

	static void MdTell(const char* text) {
		DWORD now = GetTickCount();
		if (!text || now - mdLastTell < MED_TELL_GAP_MS || !ogame || !ogame->GetTextView())
			return;
		mdLastTell = now;
		ogame->GetTextView()->PrintTimedCXY(zSTRING(text), 2500.0f, nullptr);
	}

	static std::string MdFoeDesc(oCNpc* npc, float dist) {
		if (!npc)
			return "";
		char buf[200];
		sprintf_s(buf, "%s %.0f cm (enemy=bohater %d, nastawienie %d, tryb broni %d)", npc->GetInstanceName().ToChar(), dist,
			npc->enemy == player, npc->GetAttitude(player), npc->GetWeaponMode());
		return buf;
	}

	// Otwarte okno gry (nazwa do logu) albo nullptr. Dokument / mapa: oCDocumentManager::HandleEvent 0x65F450 przy
	// zamknieciu zwalnia wpis listy (ListDocuments->next = 0) i zdejmuje blokade sterowania.
	static const char* MdWindow(bool ignoreConsole) {
		if (!ignoreConsole && zcon && zcon->IsVisible())
			return "konsola";
		if (player->inventory2.IsOpen())
			return "ekwipunek";
		if (!oCInformationManager::GetInformationManager().HasFinished())
			return "dialog";
		oCDocumentManager& dm = oCDocumentManager::GetDocumentManager();
		if (dm.ListDocuments && dm.ListDocuments->next)
			return "dokument / mapa";
		if (ogame && ogame->singleStep)
			return "pauza";
		return nullptr;
	}

	// Czy mozna zaczac: odmowa do logu (okno / wylaczone / blokada sterowania - tylko Debug=1, bo Enter w dialogach
	// i oknach wtyczek to zwykle zatwierdzenie) i na ekran (tell).
	static bool MdCanStart(const char* src, bool tell, bool ignoreConsole) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !world)
			return false;
		MdReadNames();
		zdcore::MedHeroState s;
		s.enabled = zcfg.enabled && zcfg.meditate;
		std::string missing;
		s.anis = MdFilesPresent(missing);
		s.human = HeroIsHuman();
		zCModel* m = player->GetModel();
		oCAniCtrl_Human* ac = player->GetAnictrl();
		s.down = player->IsDead() || player->IsUnconscious() || !m || !ac;
		const char* win = s.down ? nullptr : MdWindow(ignoreConsole);
		s.window = win != nullptr;
		s.mounted = HeroMounted();
		s.pluginMove = MovesActive() || TraversalActive() || DirectorActive() || LootActive();
		s.movLock = player->IsMovLock() != 0;
		if (!s.down) {
			s.attacking = MdAttacking();
			s.busy = MdOtherMsg();
			int bs = player->GetBodyState() & BS_ONLY_STATE;
			s.bodyOk = (bs == BS_STAND || bs == BS_WALK || bs == BS_RUN || bs == BS_SNEAK || bs == BS_SPRINT) && !player->physicsEnabled;
			s.water = ac->GetWaterLevel() > 0;
		}
		float foeDist = 0.0f;
		oCNpc* foe = FindFightingFoe(MED_ENEMY_RADIUS, nullptr, foeDist);
		s.enemy = foe != nullptr;
		zdcore::MedDeny d = zdcore::MedStartDeny(s);
		if (d == zdcore::MDENY_OK)
			return true;
		std::string extra;
		if (d == zdcore::MDENY_WINDOW && win)
			extra = std::string(": ") + win;
		else if (d == zdcore::MDENY_NO_ANIS)
			extra = ": brak " + missing + " (paczka z animacjami medytacji)";
		else if (d == zdcore::MDENY_ENEMY)
			extra = ": " + MdFoeDesc(foe, foeDist);
		else if (d == zdcore::MDENY_BODY)
			extra = std::string(": stan ") + player->GetBodyStateName().ToChar() + (player->physicsEnabled ? ", fizyka (lot / upadek)" : "");
		if (d == zdcore::MDENY_OFF || d == zdcore::MDENY_WINDOW || d == zdcore::MDENY_MOVLOCK)
			ZDebug("Medytacja: odmowa (%s) - %s%s", src, zdcore::MedDenyText(d), extra.c_str());
		else
			ZLog("Medytacja: odmowa (%s) - %s%s", src, zdcore::MedDenyText(d), extra.c_str());
		if (tell)
			MdTell(zdcore::MedDenyScreen(d));
		return false;
	}

	// ---------- dar aktywny (New Balance) ----------

	static zCPar_Symbol* MdKarmaSym() {
		zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING("KARMA_SKILL_LEARNED")) : nullptr;
		if (!s || s->type != zPAR_TYPE_INT || (int)s->ele <= zdcore::MED_GOD_ADANOS_SLOT)
			return nullptr;
		return s;
	}

	static int MdKarma(zCPar_Symbol* s, int i) {
		int v = 0;
		s->GetValue(v, i);
		return v;
	}

	static std::vector<int> MdGodChoicesNow(int& slot) {
		slot = 0;
		zCPar_Symbol* s = MdKarmaSym();
		if (!s)
			return {};
		slot = MdKarma(s, zdcore::MED_GOD_SLOT);
		return zdcore::MedGodChoices(MdKarma(s, zdcore::MED_GOD_INNOS_SLOT), MdKarma(s, zdcore::MED_GOD_BELIAR_SLOT),
			MdKarma(s, zdcore::MED_GOD_ADANOS_SLOT));
	}

	static void MdSetGod(int god) {
		zCPar_Symbol* s = MdKarmaSym();
		if (!s)
			return;
		int old = MdKarma(s, zdcore::MED_GOD_SLOT);
		s->SetValue(god, zdcore::MED_GOD_SLOT);
		ZLog("Medytacja: dar aktywny - %s (karma_skill_learned[%d] %d -> %d)", zdcore::MedGodLabel(god), zdcore::MED_GOD_SLOT, old, god);
	}

	// ---------- okno ----------

	static std::string MdRegenHint() {
		char buf[160];
		if (mdRegenMode == zdcore::MED_REGEN_GRADUAL)
			sprintf_s(buf, "\xAFycie i mana rosn\xB9 w czasie czekania - pe\xB3" "ne po %d godz.", zcfg.meditateRegenHours);
		else if (mdRegenMode == zdcore::MED_REGEN_SLEEP)
			sprintf_s(buf, "Pe\xB3" "ne \xBFycie i mana po co najmniej %d godz. czekania (kr\xF3" "cej - nic)", zcfg.meditateSleepHours);
		else
			sprintf_s(buf, "Czekanie bez regeneracji");
		return buf;
	}

	static void MdMenuHeader() {
		if (mdTab > 0) {
			PathMenuHeader(mdMenu, mdTab - 1);
			return;
		}
		double now = MdNow();
		int d, h, mi;
		zdcore::MedSplitMinutes(now, d, h, mi);
		char a[96], b[96];
		sprintf_s(a, "Dzie\xF1 %d, godzina %s", d + 1, zdcore::MedClockText(now).c_str());
		sprintf_s(b, "\xAFycie %d/%d    Mana %d/%d", MdAttr(NPC_ATR_HITPOINTS), MdAttr(NPC_ATR_HITPOINTSMAX), MdAttr(NPC_ATR_MANA),
			MdAttr(NPC_ATR_MANAMAX));
		mdMenu.header = { a, b };
		if (zdcore::MenuRow* r = zdcore::MenuRowById(mdMenu, MED_ROW_REGEN))
			r->hint = MdRegenHint();
	}

	// Zakladki Gildia / Sciezki tylko w New Balance (symbole NB i paczki pluginu) - Steam i Kroniki bez zmian.
	static std::vector<std::string> MdTabs() {
		if (!PathMenuAvailable())
			return {};
		return { "Medytacja", "Gildia", "\x8C" "cie\xBFki" };
	}

	static void MdMenuBuild() {
		zdcore::MenuList m;
		m.title = "Medytacja";
		m.tabs = MdTabs();
		if (m.tabs.empty())
			mdTab = 0;
		m.tab = mdTab;
		if (mdTab > 0) {
			mdMenu = m;
			PathMenuBuild(mdMenu, mdTab - 1);
			return;
		}
		zdcore::MenuRow r;
		r.id = MED_ROW_WAIT;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Czekaj";
		for (int h = 1; h <= 24; h++)
			r.values.push_back(std::to_string(h) + " godz.");
		r.value = std::clamp(mdWaitHours, 1, 24) - 1;
		r.activates = true;
		r.hint = "Enter - czekaj tyle godzin (strza\xB3ki w lewo / w prawo zmieniaj\xB9 liczb\xEA)";
		m.rows.push_back(r);

		r = zdcore::MenuRow();
		r.id = MED_ROW_UNTIL;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Czekaj do";
		for (int u = 0; u < zdcore::MED_UNTIL_COUNT; u++)
			r.values.push_back(zdcore::MedUntilLabel(u));
		r.value = std::clamp(mdUntil, 0, zdcore::MED_UNTIL_COUNT - 1);
		r.wrap = true;
		r.activates = true;
		r.hint = "Enter - czekaj do wybranej pory dnia";
		m.rows.push_back(r);

		r = zdcore::MenuRow();
		r.id = MED_ROW_SPEED;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Tempo";
		for (int s : zdcore::MedSpeedValues())
			r.values.push_back("x" + std::to_string(s));
		r.value = zdcore::MedSpeedIndex(mdSpeed);
		r.hint = "Ile razy szybciej p\xB3ynie czas w czasie czekania";
		m.rows.push_back(r);

		r = zdcore::MenuRow();
		r.id = MED_ROW_REGEN;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Regeneracja";
		for (int g = 0; g < zdcore::MED_REGEN_COUNT; g++)
			r.values.push_back(zdcore::MedRegenLabel(g));
		r.value = std::clamp(mdRegenMode, 0, zdcore::MED_REGEN_COUNT - 1);
		r.wrap = true;
		m.rows.push_back(r);

		int slot = 0;
		mdGods = MdGodChoicesNow(slot);
		r = zdcore::MenuRow();
		r.id = MED_ROW_GOD;
		r.kind = zdcore::MROW_CHOICE;
		r.label = "Dar aktywny";
		for (int g : mdGods)
			r.values.push_back(zdcore::MedGodLabel(g));
		r.value = zdcore::MedGodChoiceIndex(mdGods, slot);
		r.wrap = true;
		r.visible = !mdGods.empty();
		r.hint = "Wersja daru aktywnego (polipatronat); auto = Innos, Adanos, Beliar";
		m.rows.push_back(r);

		r = zdcore::MenuRow();
		r.id = MED_ROW_RISE;
		r.kind = zdcore::MROW_ACTION;
		r.label = "Wsta\xF1";
		r.hint = "Koniec medytacji";
		m.rows.push_back(r);

		m.footer = { m.tabs.empty() ? "Strza\xB3ki - wyb\xF3r i zmiana, Enter - wykonaj, Esc - wsta\xF1" :
			"Strza\xB3ki - wyb\xF3r i zmiana, Enter - wykonaj, Q / E - zak\xB3" "adki, Esc - wsta\xF1" };
		if (zcfg.menuMouse)
			m.footer.push_back("Mysz: LPM - wybierz, k\xF3\xB3ko - zmie\xF1, PPM + ruch - obr\xF3t kamery");
		mdMenu = m;
		if (!zdcore::MenuSelectRow(mdMenu, mdLastRow))
			zdcore::MenuFixCursor(mdMenu);
		MdMenuHeader();
	}

	static ZMenuLayout MdMenuLayout() {
		ZMenuLayout l;
		l.x = 4700;
		l.y = 1300;
		l.w = 3100;
		l.h = 4600;
		if (!mdMenu.tabs.empty()) {   // New Balance: zakladki Gildia / Sciezki - dluzsze listy i nazwy
			l.x = 4500;
			l.y = 1000;
			l.w = 3400;
			l.h = 5600;
		}
		return l;
	}

	static ZMenuLayout MdStatusLayout() {
		ZMenuLayout l;
		l.w = 3400;
		l.h = 1100;
		l.x = (8192 - l.w) / 2;
		l.y = 450;
		return l;
	}

	// ---------- czas ----------

	static void MdRestoreTime() {
		if (mdFactorSet && ztimer)
			ztimer->factorMotion = mdFactorPrev;
		mdFactorSet = false;
		if (mdScaleSet) {
			zCModel* m = player ? player->GetModel() : nullptr;
			if (m)
				m->timeScale = mdScalePrev;
		}
		mdScaleSet = false;
	}

	static void MdWaitStart(int rowId) {
		double now = MdNow();
		double target = rowId == MED_ROW_WAIT ? zdcore::MedTargetAfterHours(now, mdWaitHours) :
			zdcore::MedTargetUntilHour(now, zdcore::MedUntilHour(mdUntil));
		if (!ztimer || !MdTimer() || !zdcore::MedStartWait(md, now, target))
			return;
		mdFactorPrev = ztimer->factorMotion;
		mdFactorSet = true;
		mdFactorResets = 0;
		ztimer->factorMotion = (float)mdSpeed;
		if (zCModel* m = player->GetModel()) {
			mdScalePrev = m->timeScale;
			mdScaleSet = true;
		}
		mdAcc = zdcore::MedRegenAcc();
		mdRegenHp = mdRegenMana = 0;
		mdLastNow = now;
		mdTopUpMs = mdEngineMs = mdRealMs = 0.0;
		mdWaitFrames = 0;
		mdEnemyAge = 1e9f;
		ZLog("Medytacja: czekanie %s -> %s (%s, %.2f godz.), tempo x%d, regeneracja %s, factorMotion %.2f -> %d, doba %.0f ms, "
			"HP %d/%d, mana %d/%d", zdcore::MedClockText(now).c_str(), zdcore::MedClockText(target).c_str(),
			rowId == MED_ROW_WAIT ? "czekaj" : zdcore::MedUntilLabel(mdUntil), (target - now) / 60.0, mdSpeed,
			zdcore::MedRegenLabel(mdRegenMode), mdFactorPrev, mdSpeed, MdTicksPerDay(), MdAttr(NPC_ATR_HITPOINTS),
			MdAttr(NPC_ATR_HITPOINTSMAX), MdAttr(NPC_ATR_MANA), MdAttr(NPC_ATR_MANAMAX));
	}

	// Klatka czekania: tempo co klatke (NB / zUtilities moga je przestawiac), dolozenie reszty tempa do zegara, regeneracja
	// stopniowa wg przyrostu czasu gry, petla kleczenia w normalnym tempie.
	static void MdWaitFrame(double realMs) {
		oCWorldTimer* wt = MdTimer();
		if (!wt || !ztimer || !player)
			return;
		float want = (float)mdSpeed;
		if (fabsf(ztimer->factorMotion - want) > 0.001f && mdWaitFrames > 0) {
			if (mdFactorResets++ == 0)
				ZLog("Medytacja: factorMotion przestawiony z zewnatrz na %.2f (inna wtyczka / skrypt) - ustawiam x%d co klatke",
					ztimer->factorMotion, mdSpeed);
		}
		double engineMs = ztimer->frameTimeFloat;
		double now = MdNow();
		double toTarget = (md.waitTo - now) * zdcore::MED_TICKS_PER_HOUR / 60.0;
		double extra = zdcore::MedClockTopUp(want, realMs, engineMs, wt->worldTime, MdTicksPerDay(), toTarget);
		if (extra > 0.0)
			wt->worldTime += (float)extra;
		mdTopUpMs += extra;
		mdEngineMs += engineMs;
		mdRealMs += realMs;
		mdWaitFrames++;
		ztimer->factorMotion = want;
		double now2 = MdNow();
		double dH = std::clamp((now2 - mdLastNow) / 60.0, 0.0, 1.0);
		mdLastNow = now2;
		if (!player->IsDead()) {
			int ah = 0, am = 0;
			zdcore::MedRegenStep(mdRegenMode, dH, (double)zcfg.meditateRegenHours, MdAttr(NPC_ATR_HITPOINTS),
				MdAttr(NPC_ATR_HITPOINTSMAX), MdAttr(NPC_ATR_MANA), MdAttr(NPC_ATR_MANAMAX), mdAcc, ah, am);
			player->attribute[NPC_ATR_HITPOINTS] += ah;
			player->attribute[NPC_ATR_MANA] += am;
			mdRegenHp += ah;
			mdRegenMana += am;
		}
		// nastepna klatka silnika: min(czas x tempo, 100 ms) - petla kleczenia w czasie rzeczywistym; gdy ktos z zewnatrz
		// przestawia factorMotion, przewidywanie nie trzyma - wtedy wg faktycznej klatki silnika
		double nextEngineMs = mdFactorResets > 0 ? engineMs : std::min(realMs * want, MED_ENGINE_CAP_MS);
		if (zCModel* m = player->GetModel())
			if (mdScaleSet)
				m->timeScale = zdcore::MedHeroTimeScale(realMs, nextEngineMs);
	}

	static void MdWaitEnd(zdcore::MedStop why) {
		double now = MdNow();
		double waited = (now - md.waitFrom) / 60.0;
		MdRestoreTime();
		bool sleep = zdcore::MedSleepRestores(mdRegenMode, waited, (double)zcfg.meditateSleepHours);
		std::string sleepLog;
		if (sleep && player && !player->IsDead()) {
			int hp = MdAttr(NPC_ATR_HITPOINTS), mana = MdAttr(NPC_ATR_MANA);
			player->attribute[NPC_ATR_HITPOINTS] = std::max(hp, MdAttr(NPC_ATR_HITPOINTSMAX));
			player->attribute[NPC_ATR_MANA] = std::max(mana, MdAttr(NPC_ATR_MANAMAX));
			mdRegenHp += MdAttr(NPC_ATR_HITPOINTS) - hp;
			mdRegenMana += MdAttr(NPC_ATR_MANA) - mana;
			sleepLog = " (jak sen: pelne)";
		}
		else if (mdRegenMode == zdcore::MED_REGEN_SLEEP)
			sleepLog = " (jak sen: za krotko)";
		double secs = mdRealMs / 1000.0;
		ZLog("Medytacja: koniec czekania (%s) - %s -> %s, przeczekano %.2f godz. z %.2f; %d klatek, %.1f s, silnik %.0f ms gry "
			"(x%.1f), plugin dolozyl %.0f ms (razem x%.1f); factorMotion przestawiany z zewnatrz %d razy; regeneracja %s: "
			"+%d HP, +%d many%s; HP %d/%d, mana %d/%d", zdcore::MedStopText(why), zdcore::MedClockText(md.waitFrom).c_str(),
			zdcore::MedClockText(now).c_str(), waited, (md.waitTo - md.waitFrom) / 60.0, mdWaitFrames, secs, mdEngineMs,
			mdRealMs > 0 ? mdEngineMs / mdRealMs : 0.0, mdTopUpMs, mdRealMs > 0 ? (mdEngineMs + mdTopUpMs) / mdRealMs : 0.0,
			mdFactorResets, zdcore::MedRegenLabel(mdRegenMode), mdRegenHp, mdRegenMana, sleepLog.c_str(), MdAttr(NPC_ATR_HITPOINTS),
			MdAttr(NPC_ATR_HITPOINTSMAX), MdAttr(NPC_ATR_MANA), MdAttr(NPC_ATR_MANAMAX));
	}

	// ---------- kamera orbitalna ----------
	// Opis silnika w Core\Meditate.h (sekcja kamery). Faza ACTIVE od klekania (blokada sterowania - mysz X juz nie obraca
	// bohatera) do konca medytacji (kleczenie, okno, czekanie, wstawanie), potem RETURN: azymut wraca krotsza droga za
	// bohatera (bestRotY trybu z poczatku), elewacja zostaje silnikowi. Krok raz na klatke w pierwszym CheckKeys (DoAI
	// woła go dwa razy - drugi zapis z tej samej klatki). Odleglosc, punkt celu, kolizja i doganianie - silnik (AI_Normal).
	static zdcore::MedCam mdCam;
	static unsigned mdCamStamp = ~0u;     // zdFrame klatki ostatniego kroku
	static double mdCamClock = 0.0;       // ZClockMs ostatniego kroku
	static float mdCamDt = 0.0f;          // czas rzeczywisty klatki kamery (s)
	static bool mdCamWrite = false;       // wynik kroku tej klatki: zapis bestRotY (w ACTIVE tez bestRotX)
	static std::string mdCamMode;         // tryb kamery z ostatniego kroku
	static bool mdCamSkipLogged = false;
	static double mdCamStartMs = 0.0;
	static float mdCamStartAzi = 0.0f;
	static int mdCamFrames = 0, mdCamDtFixes = 0;
	static float mdCamDtMinEngine = 0.0f;

	static std::string MdCamModeName(zCAICamera* cam) {
		zSTRING m = cam->GetMode();
		return zdcore::Upper(m.ToChar());
	}

	static const char* MdCamPhaseName(int p) {
		return p == zdcore::MCAM_ACTIVE ? "orbita" : p == zdcore::MCAM_RETURN ? "powrot za bohatera" : "wyl.";
	}

	static void MdCamLogEnd(const char* why) {
		ZLog("Medytacja: kamera orbitalna - koniec (%s) po %.1f s: %d klatek, obrot mysza %.0f st., azymut %.1f -> %.1f, elewacja "
			"%.1f; frameTime kamery podniesiony w %d klatkach (najmniejszy silnikowy %.4f s)", why, (ZClockMs() - mdCamStartMs) / 1000.0,
			mdCamFrames, mdCam.turned, mdCamStartAzi, zdcore::MedCamAzi(mdCam), mdCam.elev, mdCamDtFixes, mdCamDtMinEngine);
	}

	// Kleczenie zaczete (blokada sterowania): orbita z biezacych bestRotY / bestRotX - bez skoku kamery.
	static void MdCamBegin() {
		if (ENGINE != Engine_G2A)
			return;
		if (!zcfg.meditateCam) {
			ZLog("Medytacja: kamera orbitalna wylaczona (MeditateCam=0)");
			return;
		}
		zCAICamera* cam = zCAICamera::GetCurrent();
		if (!cam || !cam->moveTracker || !cam->camVob) {
			ZLog("Medytacja: kamera orbitalna - brak kamery gry (zCAICamera::GetCurrent)");
			return;
		}
		bool again = mdCam.phase == zdcore::MCAM_RETURN;
		zdcore::MedCamBegin(mdCam, cam->bestRotY, cam->bestRotX, again ? mdCam.baseAzi : cam->bestRotY);
		mdCamMode = MdCamModeName(cam);
		mdCamStamp = ~0u;
		mdCamClock = 0.0;
		mdCamWrite = false;
		mdCamSkipLogged = false;
		mdCamStartMs = ZClockMs();
		mdCamStartAzi = zdcore::MedCamAzi(mdCam);
		mdCamFrames = mdCamDtFixes = 0;
		mdCamDtMinEngine = 0.0f;
		float sx = 0.0f, sy = 0.0f;
		int fx = 0, fy = 0;
		if (zinput) {
			zinput->GetMouseSensitivity(sx, sy);
			zinput->GetMouseFlipXY(fx, fy);
		}
		zVEC3 d = cam->camVob->GetPositionWorld() - cam->moveTracker->playerPos;
		ZLog("Medytacja: kamera orbitalna - start (tryb %s%s%s): azymut %.1f, elewacja %.1f (zakres %.0f..%.0f), odleglosc %.0f cm "
			"(bestRange; kamera teraz %.0f cm od celu), %.3f st. na jednostke myszy (MeditateCamSens %.2f), mysz z opcji gry x%.2f / "
			"x%.2f, odwrocenie osi %d/%d, gora/dol %d", mdCamMode.c_str(), cam->firstPerson ? ", pierwsza osoba - orbita nieczynna" : "",
			again ? ", w trakcie powrotu poprzedniej" : "", zdcore::MedCamAzi(mdCam), mdCam.elev, mdCam.elevMin, mdCam.elevMax,
			cam->GetBestRange() * 100.0f, d.Length(), zdcore::MED_CAM_DEG_PER_UNIT * zcfg.meditateCamSens, zcfg.meditateCamSens, sx, sy,
			fx, fy, (int)zCAICamera::s_iLookAroundSgn);
	}

	// Koniec medytacji. smooth: powrot za bohatera (RETURN); inaczej od razu (wczytanie, zmiana swiata, wylaczenie -
	// bestRotY trybu wpisany raz, silnik dojezdza sam). Wolane tez poza medytacja (konczy trwajacy powrot).
	static void MdCamEnd(bool smooth, const char* why) {
		if (mdCam.phase == zdcore::MCAM_OFF)
			return;
		if (smooth && mdCam.phase == zdcore::MCAM_ACTIVE) {
			zdcore::MedCamRelease(mdCam);
			ZLog("Medytacja: kamera orbitalna - powrot za bohatera (%s): azymut %.1f -> %.1f, elewacja %.1f zostaje", why,
				zdcore::MedCamAzi(mdCam), mdCam.baseAzi, mdCam.elev);
			return;
		}
		if (smooth)
			return;   // powrot juz trwa
		std::string w = std::string(why) + ", od razu";
		MdCamLogEnd(w.c_str());
		zCAICamera* cam = ENGINE == Engine_G2A ? zCAICamera::GetCurrent() : nullptr;
		if (cam)
			cam->bestRotY = mdCam.baseAzi;
		mdCam.phase = zdcore::MCAM_OFF;
		mdCamWrite = false;
	}

	// Krok kamery raz na klatke: tryb, mysz, wygladzanie (pierwszy CheckKeys w DoAI).
	static void MdCamFrame(zCAICamera* cam) {
		double now = ZClockMs();
		mdCamDt = mdCamClock > 0.0 ? (float)((now - mdCamClock) / 1000.0) : 0.0f;
		mdCamClock = now;
		std::string mode = MdCamModeName(cam);
		if (mode != mdCamMode) {
			bool wasReturn = mdCam.phase == zdcore::MCAM_RETURN;
			ZLog("Medytacja: kamera - tryb %s -> %s (%s, bestRotY trybu %.1f)", mdCamMode.c_str(), mode.c_str(),
				MdCamPhaseName(mdCam.phase), cam->bestRotY);
			zdcore::MedCamModeChanged(mdCam, cam->bestRotY);
			mdCamMode = mode;
			if (wasReturn)
				MdCamLogEnd("zmiana trybu kamery - silnik przejal");
		}
		mdCamWrite = false;
		if (mdCam.phase == zdcore::MCAM_OFF)
			return;
		if (!zdcore::MedCamModeOrbits(mode, cam->firstPerson != 0)) {
			if (!mdCamSkipLogged) {
				mdCamSkipLogged = true;
				ZLog("Medytacja: kamera orbitalna nieczynna w trybie %s%s (DoAI bez AI_Normal)", mode.c_str(),
					cam->firstPerson ? ", pierwsza osoba" : "");
			}
			if (mdCam.phase == zdcore::MCAM_RETURN) {
				MdCamLogEnd("tryb bez orbity");
				mdCam.phase = zdcore::MCAM_OFF;
			}
			return;
		}
		if (mdCam.phase == zdcore::MCAM_ACTIVE && zinput && zinput->GetDeviceEnabled(zINPUT_MOUSE)) {
			float x = 0.0f, y = 0.0f, z = 0.0f;
			zinput->GetMousePos(x, y, z);
			zdcore::MedCamMouse(mdCam, x, y, zdcore::MED_CAM_DEG_PER_UNIT * zcfg.meditateCamSens, zCAICamera::s_iLookAroundSgn);
		}
		bool wasReturn = mdCam.phase == zdcore::MCAM_RETURN;
		mdCamWrite = zdcore::MedCamStep(mdCam, mdCamDt);
		mdCamFrames++;
		if (wasReturn && mdCam.phase == zdcore::MCAM_OFF)
			MdCamLogEnd(mdCamDt > zdcore::MED_CAM_GAP ? "przerwa kamery (dialog / menu) - od razu za bohatera" : "kamera za bohaterem");
	}

	// Po kazdym CheckKeys: nasz azymut / elewacja zamiast przycietych przez silnik (-80..90 w NORMAL).
	static void MdCamApply(zCAICamera* cam) {
		if (ENGINE != Engine_G2A || !cam || cam != zCAICamera::GetCurrent())
			return;
		if (mdCamStamp != zdFrame) {
			mdCamStamp = zdFrame;
			MdCamFrame(cam);
		}
		if (!mdCamWrite)
			return;
		cam->bestRotY = zdcore::MedCamAzi(mdCam);
		if (mdCam.phase == zdcore::MCAM_ACTIVE)
			cam->bestRotX = mdCam.elev;
	}

	// zCAICamera::CheckKeys 0x4A45C0 (wolany tylko z DoAI, 0x4A506F i 0x4A5165): mysz i klawisze kamery. Po oryginale
	// w medytacji (i w powrocie) bestRotY / bestRotX pluginu - DoAI liczy z nich punkt idealny kamery (camSysFirstPos).
	static void __fastcall MdCamCheckKeys(zCAICamera* self, void* edx);
	static CInvoke<void(__fastcall*)(zCAICamera*, void*)> Ivk_zCAICamera_CheckKeys(ENGINE == Engine_G2A ? 0x004A45C0 : 0,
		&MdCamCheckKeys, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall MdCamCheckKeys(zCAICamera* self, void* edx) {
		ZHookHit(ZH_CAM_CHECKKEYS);
		Ivk_zCAICamera_CheckKeys(self, edx);
		if (mdCam.phase != zdcore::MCAM_OFF || mdCamWrite)
			MdCamApply(self);
	}

	// zCAICamera::AI_Normal 0x4A4370 (z DoAI 0x4A522C): kolizja i InterpolateTo. W orbicie frameTime trackera = czas
	// rzeczywisty, gdy silnikowy jest wyraznie mniejszy (czekanie: klatka przycieta do 100 ms / tempo) - inaczej kamera
	// doganialaby punkt idealny 3-17 razy wolniej. Po oryginale wartosc silnika wraca.
	static void __fastcall MdCamAINormal(zCAICamera* self, void* edx);
	static CInvoke<void(__fastcall*)(zCAICamera*, void*)> Ivk_zCAICamera_AI_Normal(ENGINE == Engine_G2A ? 0x004A4370 : 0,
		&MdCamAINormal, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall MdCamAINormal(zCAICamera* self, void* edx) {
		ZHookHit(ZH_CAM_AINORMAL);
		zCMovementTracker* t = self ? self->moveTracker : nullptr;
		float keep = 0.0f;
		bool fix = false;
		if (t && mdCam.phase == zdcore::MCAM_ACTIVE && mdCamWrite && self == zCAICamera::GetCurrent()) {
			keep = t->frameTime;
			float want = zdcore::MedCamTrackerDt(keep, mdCamDt);
			if (want != keep) {
				fix = true;
				if (mdCamDtFixes++ == 0 || keep < mdCamDtMinEngine)
					mdCamDtMinEngine = keep;
				t->frameTime = want;
			}
		}
		Ivk_zCAICamera_AI_Normal(self, edx);
		if (fix)
			t->frameTime = keep;
	}

	static std::string MdCamInfo() {
		char buf[200];
		sprintf_s(buf, "kamera orbitalna %s: %s, azymut %.1f (powrot do %.1f), elewacja %.1f, tryb %s, MeditateCamSens %.2f",
			zcfg.meditateCam ? "wl." : "wyl.", MdCamPhaseName(mdCam.phase), zdcore::MedCamAzi(mdCam), mdCam.baseAzi, mdCam.elev,
			mdCamMode.empty() ? "-" : mdCamMode.c_str(), zcfg.meditateCamSens);
		return buf;
	}

	// ---------- animacje i sprzatanie ----------

	static void MdReact(const zdcore::MedStep& st);

	static bool MdKneel() {
		zCModel* m = player ? player->GetModel() : nullptr;
		oCAniCtrl_Human* ac = player ? player->GetAnictrl() : nullptr;
		if (!m || !ac)
			return false;
		std::string mds = mdN.overlay + ".MDS";
		if (!MdProto(m))
			ZApplyOverlayKeepAnis(m, mds);
		if (!MdProto(m)) {
			ZLog("Medytacja: silnik nie zalozyl nakladki %s - w zSpy: \"ani in overlay not present in base\" (animacje nakladki "
				"musza miec nazwy z modelu bazowego HUMANS) albo \"overlay- and model base-.mds do not match\"", mds.c_str());
			return false;
		}
		mdOverlayOn = true;
		MedAnis a = MdGetAnis(m);
		if (!a.start || !a.loop || !a.stop) {
			ZLog("Medytacja: w nakladce %s brak animacji%s%s%s (nazwa spoza modelu bazowego albo bez MAN)", mds.c_str(),
				a.start ? "" : (" " + mdN.start).c_str(), a.loop ? "" : (" " + mdN.loop).c_str(), a.stop ? "" : (" " + mdN.stop).c_str());
			return false;
		}
		ac->StopTurnAnis();
		m->StartAni(a.start, 0);
		if (!player->IsMovLock()) {
			player->SetMovLock(TRUE);
			mdMovLock = true;
		}
		ZLog("Medytacja: klekanie %s (warstwa %d, next %s), %s", mdN.start.c_str(), a.start->layer,
			a.start->nextAni ? a.start->nextAni->aniName.ToChar() : "-", MdClockDay(MdNow()).c_str());
		MdCamBegin();
		return true;
	}

	static void MdRiseStart(zdcore::MedStop why) {
		zCModel* m = player ? player->GetModel() : nullptr;
		MedAnis a = MdGetAnis(m);
		if (!a.stop) {
			MdReact(zdcore::MedAbort(md, why));
			return;
		}
		m->StartAni(a.stop, 0);
		ZLog("Medytacja: wstawanie (%s)%s", zdcore::MedStopText(why), why == zdcore::MSTOP_ENEMY ? (" - " + mdEnemyDesc).c_str() : "");
		if (why == zdcore::MSTOP_ENEMY)
			MdTell("Wrogowie w pobli\xBFu!");
	}

	// Okno medytacji: tempo i regeneracja zmienione w grze trafiaja tez do pozycji menu Union.
	static void MdSaveSettings() {
		if (!mdSettingsChanged)
			return;
		mdSettingsChanged = false;
		zcfg.SyncMenu();
	}

	static void MdCleanup(bool quick, zdcore::MedStop why) {
		MdRestoreTime();
		zCModel* m = player ? player->GetModel() : nullptr;
		bool stood = false;
		if (m && mdOverlayOn) {
			MedAnis a = MdGetAnis(m);
			bool ours = MdPlaying(m, a.start) || MdPlaying(m, a.loop) || MdPlaying(m, a.stop);
			oCAniCtrl_Human* ac = player->GetAnictrl();
			if (ours) {
				// najpierw lista (StopAni zmienia kanaly), potem zatrzymanie - nasze i te same ID z innych prototypow
				std::vector<zCModelAni*> stop;
				int n = std::min(m->numActiveAnis, (int)zMDL_MAX_ANIS_PARALLEL);
				for (int i = 0; i < n; i++) {
					zCModelAniActive* ch = m->aniChannels[i];
					if (!ch || !ch->protoAni)
						continue;
					for (zCModelAni* x : { a.start, a.loop, a.stop })
						if (x && ch->protoAni->aniID == x->aniID)
							stop.push_back(ch->protoAni);
				}
				for (zCModelAni* x : stop)
					m->StopAni(x);
				if (ac && !player->IsDead() && ac->_s_walk >= 0) {
					m->StartAni(ac->_s_walk, 0);   // postawa z najwyzszej nakladki, ktora ja ma (ruch W3 / baza)
					stood = true;
				}
			}
			ZRemoveOverlayKeepAnis(m, mdN.overlay + ".MDS");
		}
		mdOverlayOn = false;
		if (mdMovLock && player)
			player->SetMovLock(FALSE);
		mdMovLock = false;
		// kamera za bohatera plynnie; wczytanie / zmiana swiata / wylaczenie (MeditateFinishNow) - od razu
		MdCamEnd(!(quick && why == zdcore::MSTOP_EXTERNAL), zdcore::MedStopText(why));
		ZMenuCapture(false, "medytacja");
		MdSaveSettings();
		double secs = (ZClockMs() - mdPhaseMs) / 1000.0;
		ZLog("Medytacja: koniec %s (%s) po %.1f s - %s, HP %d/%d, mana %d/%d%s", quick ? "od razu" : "po wstaniu",
			zdcore::MedStopText(why), secs, MdClockDay(MdNow()).c_str(), MdAttr(NPC_ATR_HITPOINTS), MdAttr(NPC_ATR_HITPOINTSMAX),
			MdAttr(NPC_ATR_MANA), MdAttr(NPC_ATR_MANAMAX), stood ? ", postawa przywrocona" : "");
	}

	static void MdReact(const zdcore::MedStep& st) {
		if (st.act == zdcore::MACT_NONE)
			return;
		if (st.from == zdcore::MED_WAIT)
			MdWaitEnd(st.why);
		switch (st.act) {
		case zdcore::MACT_KNEEL:
			if (!MdKneel())
				MdReact(zdcore::MedAbort(md, zdcore::MSTOP_LOST));
			break;
		case zdcore::MACT_OPEN_MENU: {
			MdMenuBuild();
			const zdcore::MenuRow* god = zdcore::MenuRowById(mdMenu, MED_ROW_GOD);
			int gi = god ? god->value : 0;
			ZLog("Medytacja: okno (po %.1f s od startu), %s, dar aktywny %s, zakladki Gildia / Sciezki: %s", (ZClockMs() - mdPhaseMs) / 1000.0,
				MdClockDay(MdNow()).c_str(), mdGods.empty() ? "- (bez wiersza)" :
				zdcore::MedGodLabel(mdGods[std::clamp(gi, 0, (int)mdGods.size() - 1)]), mdMenu.tabs.empty() ? "nie (brak NB)" : "tak");
			break;
		}
		case zdcore::MACT_WAIT_END:
			MdMenuBuild();
			break;
		case zdcore::MACT_RISE:
			MdRiseStart(st.why);
			break;
		case zdcore::MACT_FINISH:
			MdCleanup(false, st.why);
			break;
		case zdcore::MACT_ABORT:
			MdCleanup(true, st.why);
			break;
		default:
			break;
		}
	}

	static void MdStart(const char* src) {
		bool weaponOut = player->GetWeaponMode() != NPC_WEAPON_NONE || MdWeaponMsg();
		mdSpeed = zdcore::MedClampSpeed(zcfg.meditateSpeed);
		mdRegenMode = std::clamp(zcfg.meditateRegen, 0, zdcore::MED_REGEN_COUNT - 1);
		mdHpPrev = MdAttr(NPC_ATR_HITPOINTS);
		mdEnemyAge = 1e9f;
		mdEnemyNear = false;
		mdPhaseMs = ZClockMs();
		mdTab = 0;
		ZMenuCapture(true, "medytacja");
		zdcore::MedStep st = zdcore::MedBegin(md, weaponOut);
		if (weaponOut && !MdWeaponMsg())
			if (zCEventManager* em = player->GetEM(FALSE))
				em->OnMessage(new oCMsgWeapon(oCMsgWeapon::EV_REMOVEWEAPON, 0, 0), player);
		ZLog("Medytacja: start (%s) - %s, HP %d/%d, mana %d/%d, %s, tempo x%d, regeneracja %s, nakladka %s (%s / %s / %s)", src,
			MdClockDay(MdNow()).c_str(), MdAttr(NPC_ATR_HITPOINTS), MdAttr(NPC_ATR_HITPOINTSMAX), MdAttr(NPC_ATR_MANA),
			MdAttr(NPC_ATR_MANAMAX), weaponOut ? "bron w reku - chowanie" : "bez broni", mdSpeed, zdcore::MedRegenLabel(mdRegenMode),
			mdN.overlay.c_str(), mdN.start.c_str(), mdN.loop.c_str(), mdN.stop.c_str());
		MdReact(st);
	}

	// ---------- klatka ----------

	static zdcore::MedSense MdSense(bool cancel, float realDt) {
		zdcore::MedSense s;
		s.now = MdNow();
		zCModel* m = player->GetModel();
		s.down = player->IsDead() || player->IsUnconscious() || !m || !HeroIsHuman();
		s.weaponOut = player->GetWeaponMode() != NPC_WEAPON_NONE || MdWeaponMsg();
		if (m && mdOverlayOn) {
			MedAnis a = MdGetAnis(m);
			s.startPlaying = MdPlaying(m, a.start);
			s.loopPlaying = MdPlaying(m, a.loop);
			s.stopPlaying = MdPlaying(m, a.stop);
		}
		s.hit = mdHpPrev >= 0 && MdAttr(NPC_ATR_HITPOINTS) < mdHpPrev;
		s.dialog = !oCInformationManager::GetInformationManager().HasFinished();
		mdEnemyAge += realDt;
		if (md.state == zdcore::MED_WAIT || mdEnemyAge >= MED_ENEMY_RECHECK) {
			mdEnemyAge = 0.0f;
			float d = 0.0f;
			oCNpc* foe = FindFightingFoe(MED_ENEMY_RADIUS, nullptr, d);
			mdEnemyNear = foe != nullptr;
			if (foe)
				mdEnemyDesc = MdFoeDesc(foe, d);
		}
		s.enemy = mdEnemyNear;
		s.cancelKey = cancel;
		if (s.hit)
			ZLog("Medytacja: HP %d -> %d (%s)", mdHpPrev, MdAttr(NPC_ATR_HITPOINTS), zdcore::MedStateName(md.state));
		return s;
	}

	// Zdarzenie okna (klawisz albo mysz): true = zmiana stanu (reszta kolejki z tej klatki odrzucona).
	static bool MdMenuEvent(zdcore::MenuEvent e) {
		if (e.type == zdcore::MEV_TAB) {
			mdTab = e.value;
			PathMenuEnterPage(mdTab - 1);
			MdMenuBuild();
			ZDebug("Medytacja: zakladka %d (%s)", mdTab, mdTab >= 0 && mdTab < (int)mdMenu.tabs.size() ? mdMenu.tabs[mdTab].c_str() : "-");
			return false;
		}
		if (mdTab > 0) {
			// Gildia / Sciezki (PathMenu.cpp); Esc na gorze strony = powrot do zakladki medytacji (nie wstanie)
			if (PathMenuEvent(mdMenu, mdTab - 1, e)) {
				mdTab = 0;
				MdMenuBuild();
			}
			return false;
		}
		if (mdMenu.cursor >= 0 && mdMenu.cursor < (int)mdMenu.rows.size())
			mdLastRow = mdMenu.rows[mdMenu.cursor].id;
		switch (e.type) {
		case zdcore::MEV_CLOSE:
			MdReact(zdcore::MedRise(md, zdcore::MSTOP_MENU));
			return true;
		case zdcore::MEV_ACTIVATE:
			if (e.rowId == MED_ROW_RISE) {
				MdReact(zdcore::MedRise(md, zdcore::MSTOP_MENU));
				return true;
			}
			if (e.rowId == MED_ROW_WAIT || e.rowId == MED_ROW_UNTIL) {
				MdWaitStart(e.rowId);
				return true;
			}
			return false;
		case zdcore::MEV_CHANGED:
			switch (e.rowId) {
			case MED_ROW_WAIT:
				mdWaitHours = e.value + 1;
				break;
			case MED_ROW_UNTIL:
				mdUntil = e.value;
				break;
			case MED_ROW_SPEED:
				mdSpeed = zdcore::MedSpeedValues()[std::clamp(e.value, 0, (int)zdcore::MedSpeedValues().size() - 1)];
				zcfg.meditateSpeed = mdSpeed;
				zoptions->WriteInt("ZDISMEMBER", "MeditateSpeed", mdSpeed, FALSE);
				mdSettingsChanged = true;
				ZLog("Medytacja: tempo x%d (MeditateSpeed)", mdSpeed);
				break;
			case MED_ROW_REGEN:
				mdRegenMode = std::clamp(e.value, 0, zdcore::MED_REGEN_COUNT - 1);
				zcfg.meditateRegen = mdRegenMode;
				zoptions->WriteInt("ZDISMEMBER", "MeditateRegen", mdRegenMode, FALSE);
				mdSettingsChanged = true;
				MdMenuHeader();
				ZLog("Medytacja: regeneracja %s (MeditateRegen=%d)", zdcore::MedRegenLabel(mdRegenMode), mdRegenMode);
				break;
			case MED_ROW_GOD:
				if (e.value >= 0 && e.value < (int)mdGods.size())
					MdSetGod(mdGods[e.value]);
				break;
			default:
				break;
			}
			return false;
		default:
			return false;
		}
	}

	static bool MdMenuKey(zdcore::MenuKey k) {
		return MdMenuEvent(zdcore::MenuHandleKey(mdMenu, k));
	}

	// Mysz w oknie (ZMenu.cpp): najechanie, LPM, kolko jak klawisze; PPM = wstecz tylko w zakladkach Gildia / Sciezki
	// (na zakladce medytacji nic - wstaje Esc albo "Wstan"); PPM z ruchem = obrot kamery orbitalnej.
	static bool MdMenuMouse(zdcore::MenuMouseKind kind, int wx, int wy) {
		zdcore::MenuMouseResult r = zdcore::MenuMouse(mdMenu, ZMenuHitMap(), kind, wx, wy);
		if (r.back) {
			if (mdTab <= 0)
				return false;
			zdcore::MenuEvent esc;
			esc.type = zdcore::MEV_CLOSE;
			return MdMenuEvent(esc);
		}
		if (r.ev.type == zdcore::MEV_NONE)
			return false;
		return MdMenuEvent(r.ev);
	}

	static void MdIdleUpdate(float realDt) {
		if (mdConsoleWait > 0.0f) {
			mdConsoleWait -= realDt;
			if (!(zcon && zcon->IsVisible())) {
				mdConsoleWait = 0.0f;
				if (MdCanStart("konsola", true, false))
					MdStart("konsola");
			}
			else if (mdConsoleWait <= 0.0f)
				ZLog("Medytacja: konsola otwarta dluzej niz %.0f s - start odwolany", MED_CONSOLE_WAIT);
		}
		if (mdPending) {
			mdPending = false;
			if (MdCanStart((mdPendingSrc + ", petla").c_str(), true, false))
				MdStart(mdPendingSrc.c_str());
		}
	}

	void MeditateUpdate() {
		mdLoopTick = GetTickCount();
		double clock = ZClockMs();
		double realMs = mdClockPrev > 0.0 ? std::clamp(clock - mdClockPrev, 0.0, MED_REAL_MS_MAX) : 0.0;
		mdClockPrev = clock;
		float realDt = (float)(realMs / 1000.0);
		ZMenuInputUpdate(realDt);
		if (!zdcore::MedActive(md)) {
			MdIdleUpdate(realDt);
			return;
		}
		if (!zcfg.enabled || !zcfg.meditate || !player || !ogame || !ogame->GetGameWorld()) {
			MeditateFinishNow(!zcfg.enabled || !zcfg.meditate ? "wylaczona" : "brak bohatera / swiata");
			return;
		}
		bool cancel = false;
		zdcore::MenuKey k;
		int dik = 0;
		while (ZMenuNextKey(k, dik)) {
			if (md.state == zdcore::MED_MENU) {
				if (MdMenuKey(k)) {
					ZMenuClearKeys();
					break;
				}
			}
			else if (k == zdcore::MKEY_ESC || (k == zdcore::MKEY_ENTER && md.state == zdcore::MED_WAIT))
				cancel = true;
		}
		zdcore::MenuMouseKind mk;
		int wx = 0, wy = 0;
		while (ZMenuNextMouse(mk, wx, wy)) {
			if (md.state == zdcore::MED_MENU && zdcore::MedActive(md) && MdMenuMouse(mk, wx, wy))
				break;
		}
		// kursor tylko przy oknie z lista; w czekaniu, klekaniu i wstawaniu mysz obraca kamere jak dotad
		ZMenuMouse(zdcore::MedActive(md) && md.state == zdcore::MED_MENU, true);
		if (!zdcore::MedActive(md))
			return;
		zdcore::MedStep st = zdcore::MedUpdate(md, MdSense(cancel, realDt), realDt);
		MdReact(st);
		if (md.state == zdcore::MED_WAIT)
			MdWaitFrame(realMs);
		else if (md.state == zdcore::MED_MENU)
			MdMenuHeader();
		mdHpPrev = player ? MdAttr(NPC_ATR_HITPOINTS) : -1;
	}

	void MeditateDraw() {
		if (!zdcore::MedActive(md) || !player || !ogame)
			return;
		if (md.state == zdcore::MED_MENU)
			ZMenuDraw(mdMenu, MdMenuLayout());
		else if (md.state == zdcore::MED_WAIT) {
			double now = MdNow();
			double p = zdcore::MedWaitProgress(md, now);
			int filled = (int)(p * 20.0 + 0.5);
			std::string bar = "[" + std::string(filled, '=') + std::string(20 - filled, '-') + "]";
			char a[96], b[64];
			sprintf_s(a, "Medytacja: %s -> %s   (x%d)", zdcore::MedClockText(now).c_str(), zdcore::MedClockText(md.waitTo).c_str(), mdSpeed);
			sprintf_s(b, "%s %d%%", bar.c_str(), (int)(p * 100.0 + 0.5));
			ZMenuDrawLines({ a, b, "Esc / Enter - przerwij" }, MdStatusLayout());
		}
	}

	bool MeditateKeyEvent(unsigned short dik, int state) {
		if (ZMenuCapturing())
			return ZMenuKeyEvent(dik, state);
		if (!state || !zcfg.enabled || !zcfg.meditate || !zinput)
			return false;
		bool alt = zcfg.meditateKeyAlt > 0 && dik == zcfg.meditateKeyAlt && MdModsHeld(zcfg.meditateKeyAltMods);
		bool main = !alt && zcfg.meditateKey > 0 && dik == zcfg.meditateKey;
		if (!alt && !main)
			return false;
		// hak wolany tez z menu / pauzy (petla gry stoi) - wtedy klawisz zostaje grze; konsola - pisanie
		bool inGame = mdLoopTick && GetTickCount() - mdLoopTick <= MED_HOOK_STALE_MS && player && ogame && ogame->GetGameWorld();
		if (!inGame || (zcon && zcon->IsVisible()))
			return false;
		mdPendingSrc = alt ? "drugi klawisz" : "klawisz";
		bool ok = mdPending || MdCanStart(mdPendingSrc.c_str(), true, false);
		if (ok)
			mdPending = true;
		// kombinacja z modyfikatorem zawsze nasza (przecinek = LocoKey NB / pochodnia i widok FPP Steam), sam klawisz
		// tylko gdy zaczyna medytacje (Enter w dialogach, dokumentach, ekwipunku zostaje grze)
		return ok || (alt && zcfg.meditateKeyAltMods != 0);
	}

	bool MeditateActive() {
		return zdcore::MedActive(md);
	}

	bool MeditateComboHeld(int key) {
		return zcfg.enabled && zcfg.meditate && zcfg.meditateKeyAlt > 0 && key == zcfg.meditateKeyAlt && zcfg.meditateKeyAltMods != 0 &&
			MdModsHeld(zcfg.meditateKeyAltMods);
	}

	void MeditateFinishNow(const char* why) {
		mdPending = false;
		mdConsoleWait = 0.0f;
		if (!zdcore::MedActive(md)) {
			MdRestoreTime();
			MdCamEnd(false, why);   // powrot kamery po medytacji jeszcze trwa (zapis, wczytanie zaraz po wstaniu)
			return;
		}
		ZLog("Medytacja: przerwana z zewnatrz - %s (stan %s)", why, zdcore::MedStateName(md.state));
		MdReact(zdcore::MedAbort(md, zdcore::MSTOP_EXTERNAL));
	}

	static std::string MdInfo() {
		MdReadNames();
		std::string missing;
		bool files = MdFilesPresent(missing);
		zCModel* m = player ? player->GetModel() : nullptr;
		int slot = 0;
		std::vector<int> gods = MdGodChoicesNow(slot);
		char buf[700];
		sprintf_s(buf, "medytacja %s, stan %s, %s; klawisze %d / %d (mods %d), tempo x%d, regeneracja %s (%d / %d godz.); "
			"nakladka %s %s%s; factorMotion %.2f, timeScale bohatera %.2f; dar aktywny: %s",
			zcfg.meditate ? "wl." : "wyl.", zdcore::MedStateName(md.state), MdClockDay(MdNow()).c_str(), zcfg.meditateKey,
			zcfg.meditateKeyAlt, zcfg.meditateKeyAltMods, zcfg.meditateSpeed, zdcore::MedRegenLabel(zcfg.meditateRegen),
			zcfg.meditateRegenHours, zcfg.meditateSleepHours, mdN.overlay.c_str(), files ? "w paczce" : "BRAK: ",
			files ? "" : missing.c_str(), ztimer ? ztimer->factorMotion : 0.0f, m ? m->timeScale : 0.0f,
			gods.empty() ? (MdKarmaSym() ? "mniej niz 2 patronaty - bez wiersza" : "brak KARMA_SKILL_LEARNED - bez wiersza") :
			(std::string(zdcore::MedGodLabel(slot)) + " (slot 90 = " + std::to_string(slot) + ")").c_str());
		std::string out = std::string(buf) + "; " + MdCamInfo();
		if (md.state == zdcore::MED_WAIT)
			out += "; czekanie do " + zdcore::MedClockText(md.waitTo) + " (" +
				std::to_string((int)(zdcore::MedWaitProgress(md, MdNow()) * 100.0)) + "%)";
		return out;
	}

	std::string MeditateConsole(const std::string& arg) {
		std::string a = zdcore::Upper(arg);
		if (!player || !ogame || !ogame->GetGameWorld())
			return "brak bohatera";
		if (a == "STOP") {
			if (!zdcore::MedActive(md))
				return "medytacja nie trwa";
			zdcore::MedStep st = zdcore::MedRise(md, zdcore::MSTOP_EXTERNAL);
			if (st.act == zdcore::MACT_NONE)
				MeditateFinishNow("zdism meditate stop");
			else
				MdReact(st);
			return "wstaje";
		}
		std::string info = MdInfo();
		ZLog("Medytacja (konsola): %s", info.c_str());
		if (a == "INFO" || zdcore::MedActive(md))
			return info;
		if (!MdCanStart("konsola", false, true))
			return "nie mozna teraz (powod w zDismember.log); " + info;
		mdConsoleWait = MED_CONSOLE_WAIT;
		return "medytacja zacznie sie po zamknieciu konsoli; " + info;
	}
}
