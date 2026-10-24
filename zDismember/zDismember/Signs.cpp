// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// ---------- Znaki Wiedzmina ----------
	// Rzut (sprawdzone w deasemblacji Gothic2.exe 2.6):
	//   - oCSpell::Invest (0x4850D0) przy pierwszym wywolaniu (manaInvested == 0) od razu wola
	//     oCSpell::CallScriptInvestedMana (0x485D30) - tam skrypt Spell_ProcessMana, ktory naszych slotow nie zna.
	//     Pozostale czary (np. runa zTimeRewind: Steam 99, New Balance 201) ida do oryginalu - hak lancuchowy, kazdy plugin bierze swoje sloty.
	//   - oCSpell::Cast (0x485360) jest wolany WYLACZNIE z oCMag_Book::Spell_Cast (0x4767A0, 0x476AAF), a ten z
	//     oCAIHuman::MagicCheckSpellStates (tryb magii, po SENDCAST, 0x472A35 / 0x472C33) i bezposrednio z zQuickSign
	//     (szybki Znak z mieczem: Spell_Setup -> Spell_Invest -> Spell_Cast, takze po opoznieniu gestu).
	//     Hak po oryginale: rzut udany (Cast zwraca 1 tylko przy manaInvested > 0) = jeden skutek na rzut w obu sciezkach.
	//     Skutek idzie do kolejki i wykonuje sie w SignsUpdate (ta sama klatka gry albo nastepna) - poza ksiega czarow
	//     i AI bohatera (obrazenia moga zabic, odpalic finisher, percepcje).
	//   - oCSpell::CastSpecificSpell (0x486960) obsluguje tylko przemiany (id 0x2F-0x3A) - sloty znakow (SignSpellIds: Steam 90-94, New Balance 196-200) nic tam nie robia.
	// Obrazenia: oCNpc::OnDamage(zCVob*, zCVob*, float, int, zVEC3) 0x67B860 (jak repel w Dodge.cpp), napastnik = bohater,
	// na czas wywolania pole fmode (+0x250) = 7 (tryb magii): OnDamage_Hit liczy cios jak bronia biala (talent, obrazenia
	// broni), gdy napastnik ma tryb broni <= 4 i pVisualFX == 0 (0x6670A9..0x6670D3) - przy szybkim Znaku miecz jest w rece.
	//
	// Oszolomienie Axii i reakcja palenia Igni (runda 2): stany AI Gothica w zDismember_Signs.d (ZS_ZDF_Stun*,
	// ZS_ZDF_Burn), startowane skryptem ZDF_Sign_StartState (self = cel, other = bohater) - jak B_AssessMagic przy
	// Lodowym bloku. Plugin pilnuje czasu (oCNpc_States::curState.stateTime, ms) i egzekucji kleczacego.
	// oCNpc_States::DoAIState (0x76D1A0) liczy czas stanu (+0x4C, ms) i odpala petle skryptu tylko przy pustej kolejce EM.
	//
	// Runda 3: moc z maks. many (zdcore::SignPower) i tryby alternatywne przez przytrzymanie rzutu.
	//   - Pierwszy Invest: zamiast SENDCAST status 8 (SPL_STATUS_CANINVEST_NO_MANADEC - Invest 0x48529C: bez -1 many
	//     za takt, manaInvested++), czar czeka. Przy time_per_mana = 0 oCSpell::Invest wola nasz hak co klatke, dopoki
	//     klawisz jest wcisniety (tryb magii: oCAIHuman::MagicCheckSpellStates, zQuickSign: Spell_Invest co klatke F).
	//   - Puszczenie: oCSpell::SetReleaseStatus (0x486670; wolany z MagicCheckSpellStates 0x472EAF, z Invest 0x48532A
	//     gdy many zabraklo, i z zQuickSign) - hak: krotko = SENDCAST (tryb zwykly), w podtrzymaniu = SENDSTOP.
	//     Skrypt Spell_ProcessMana_Release naszych slotow nie zna (zwrocilby SENDSTOP) - nie jest wolany.
	//   - Dluzej niz SignAltHold: jednorazowy tryb alternatywny od razu SENDCAST (Cast z flaga alt), podtrzymywany
	//     (Igni strumien, Quen aktywna tarcza) trwa, dopoki Invest wola hak (zabraklo taktow > 0,3 s = koniec).
	//   - 23.10: Aard / Yrden w trybie alternatywnym spod F - zQuickSign gra gest W3 "w ziemie" calym cialem; skutek czeka
	//     w kolejce do dotkniecia ziemi (SgPending.delay), bohater stoi w miejscu do konca gestu (sgStill, SignAltInPlace).

	static const int SG_SPL_SENDCAST = 2;   // Constants.d: SPL_SENDCAST
	static const int SG_SPL_SENDSTOP = 3;   // SPL_SENDSTOP
	static const int SG_SPL_CANINVEST_NO_MANADEC = 8;   // SPL_STATUS_CANINVEST_NO_MANADEC (Piesc Wiatru, poziom 1)
	static const unsigned long SG_DESC_OVERLAY = 0x1000;   // oEDamageDescFlag_OverlayActivate - takt palenia (EV_DamagePerFrame)
	static const float SG_MAX_DY = 250.0f;  // roznica wysokosci celu stozka (pietro wyzej / nizej - poza)
	static const float SG_PUSH_TIME = 0.35f;   // odsuniecie Aard / Quen (cm w tyle s, z kolizja)
	static const float SG_HIT_STATE_MS = 2000.0f * 1000.0f;   // ZDF_SIGN_STATE_HIT_SEC w skrypcie (trafienie)
	static const float SG_QUEN_FX_MAX_DIST = 150.0f;   // efekt tarczy dalej od bohatera = oderwany (odtwarzamy)
	static const float SG_SUSTAIN_TIMEOUT = 0.3f;      // tyle s bez Invest = klawisz puszczony / czar przerwany
	static const float SG_CONSOLE_SUSTAIN = 3.0f;      // zdism sign igni|quen alt: podtrzymanie na tyle s

	// NPC w stanie znaku (oszolomienie Axii / reakcja palenia Igni).
	struct SgHold {
		oCNpc* npc;              // AddRef
		int variant;             // zdcore::StunVariant
		float left;              // do konca (plugin konczy stan)
		float age;
		float seconds;           // pelny czas (prog petli skryptu)
		bool active;             // NPC juz w stanie ZS_ZDF_* (start idzie przez kolejke EM - z opoznieniem)
		bool ended;              // plugin ustawil czas stanu za progiem
		bool finisher;           // egzekucja kleczacego trwa (Director) - stan trzymany
		float lastStateMs;       // czas stanu w ostatniej klatce (rozpoznanie trafienia po wyjsciu ze stanu)
		oCVisualFX* fx;          // SPELLFX_AXII_TARGET (nasz ref)
	};

	// Odsuniecie po Aard / peknieciu Quen (krokami z kolizja jak repel w Dodge.cpp).
	struct SgPush {
		oCNpc* npc;              // AddRef
		zVEC3 dir;
		float t, dist, done, yOff, dur;
	};

	// Lot Aard zlecony silnikowi - sprawdzany po AardFlyCheck s (AardFlyVerdict).
	struct SgFly {
		oCNpc* npc;              // AddRef
		zVEC3 start, dir;
		float t;
		float push;              // odsuniecie pluginu, gdy nie polecial (z moca)
	};

	// Wrog spowolniony w strefie Yrden albo w glifie pulapki.
	struct SgSlowed {
		oCNpc* npc;              // AddRef
		zCModel* model;          // AddRef - tempo cofamy na tym samym modelu
		zdcore::YrdenMod mod;
		bool applied;            // false = cofniete na czas zapisu
		oCVisualFX* fx;          // SPELLFX_YRDEN_SLOW
		float slow, armor;       // parametry strefy, ktora go spowolnila (z mocy)
	};

	// Celowanie (runda 5): Gothic Free Aim w trybie magii albo celownik zQuickSign (od wcisniecia F). Punkt i kierunek z chwili rzutu -
	// promien z kamery przez srodek ekranu (celownik), jak GFA_AimRay.
	struct SgAim {
		bool valid = false;
		zVEC3 point;             // trafienie promienia celownika (albo koniec promienia)
		float dirX = 0.0f, dirZ = 1.0f;   // kierunek w poziomie od bohatera do punktu
		oCNpc* npc = nullptr;    // AddRef: NPC trafiony promieniem (Axii)
		const char* source = "";
	};

	struct SgPending {
		int sign;
		oCNpc* target;           // AddRef: cel czaru z fokusu (Axii)
		bool console;
		bool alt;                // tryb alternatywny (przytrzymanie)
		SgAim aim;
		float delay = 0.0f;      // s do skutku (gest "w ziemie" zQuickSign - dotkniecie ziemi; 0 = od razu)
	};

	// Biezacy rzut bohatera (przytrzymanie): czar w ksiedze, faza decyzji.
	struct SgCastState {
		oCSpell* spell = nullptr;    // tylko porownanie wskaznika (czar zyje w ksiedze bohatera)
		int sign = -1;
		zdcore::SignHold hold;
		DWORD last = 0;
		bool alt = false;            // nastepny Cast tego czaru = tryb alternatywny
	};

	// Tryb podtrzymywany (Igni strumien, Quen aktywna tarcza).
	struct SgSustain {
		int sign = -1;
		bool active = false;
		bool console = false;
		float consoleLeft = 0.0f;
		float age = 0.0f, sinceInvest = 0.0f, tick = 0.0f;
		float power = 1.0f;
		zdcore::SignDamagePower dmg;   // moc obrazen (strumien Igni, odrzut aktywnej tarczy)
		zdcore::SustainMana mana;
		oCVisualFX* fx = nullptr;
		int absorbed = 0;
		float absorbedDmg = 0.0f;
		bool slowed = false;         // tempo modelu bohatera (SignChannelMoveScale)
		oCNpc* hero = nullptr;          // AddRef (bohater z chwili spowolnienia)
		zCModel* heroModel = nullptr;   // AddRef
		zdcore::YrdenMod heroMod;
		bool magicPath = false;      // runa w rece: klawisz akcji czyta hak MagicMode (ruch w trakcie podtrzymania)
		int poseId = -1;             // gest W3 na warstwie 21 (SgChannelPoseUpdate)
		bool poseSword = false;      // wariant gestu z mieczem (ten sam dla klipu konca - SgChannelStopName)
		bool poseTried = false;
		float poseHold = 0.0f, poseWait = 0.0f;
		zCModel* poseModel = nullptr;   // AddRef
	};

	// Kula pulapki - jedna na ladunek (SPELLFX_YRDEN_TRAP_ORB rysuje sie dokladnie w podanym srodku).
	struct SgTrapOrb {
		zVEC3 pos;
		bool used;
		oCVisualFX* fx;              // petla (nasz ref)
	};

	// Pulapka Yrdena (glif z ladunkami).
	struct SgTrap {
		bool active = false;
		zdcore::YrdenZone zone;
		zdcore::YrdenTrapState st;
		oCVisualFX* fx = nullptr;    // sam glif (SPELLFX_YRDEN_TRAP, bez kul)
		std::vector<SgTrapOrb> orbs;
		zVEC3 ground;
		float clock = 0.0f, damage = 0.0f, slow = 0.65f, armor = 30.0f;
	};

	// Pocisk pulapki w locie: obrazenia po czasie dolotu efektu SPELLFX_YRDEN_BOLT (ok. 26 m/s).
	struct SgBolt {
		oCNpc* npc;                  // AddRef
		float delay, damage;
	};
	static const float SG_BOLT_SPEED = 2600.0f;   // cm/s
	static const float SG_ORB_RADIUS = 110.0f, SG_ORB_HEIGHT = 90.0f;   // kule wokol srodka glifu (cm)
	static const int SG_MAX_ORBS = 8;

	// Marionetka Axii (jedna naraz).
	struct SgPuppet {
		oCNpc* npc = nullptr;        // AddRef
		float left = 0.0f, scan = 0.0f;
		int origAtt = 0, origTmp = 0, origParty = 0;
		bool applied = false;        // nasze wartosci wpisane (do zapisu cofniete)
		oCVisualFX* fx = nullptr;
		bool noTargetLogged = false;
	};

	static std::vector<SgHold> sgHolds;
	static std::vector<SgPush> sgPushes;
	static std::vector<SgFly> sgFlies;
	static std::vector<SgSlowed> sgSlowed;
	static std::vector<SgPending> sgPending;
	static std::vector<SgBolt> sgBolts;
	static zdcore::YrdenZone sgZone;
	static oCVisualFX* sgZoneFx = nullptr;
	static zVEC3 sgZoneGround;   // punkt glifu na ziemi (strefa liczona od srodka bohatera z chwili rzutu)
	static float sgZoneSlow = 0.65f, sgZoneArmor = 30.0f;
	static zdcore::QuenState sgQuen;
	static float sgQuenPower = 1.0f;
	static zdcore::SignDamagePower sgQuenDmg;   // moc obrazen pekniecia tarczy (z chwili rzutu)
	static oCVisualFX* sgQuenFx = nullptr;
	static zdcore::SignCooldowns sgCooldown;
	static float sgYrdenScan = 0.0f;
	static DWORD sgLastTell = 0;
	static std::vector<std::string> sgMissingFx;
	static int sgLastStun = zdcore::STUN_NONE;
	static int sgStateFunc = -2;   // indeks ZDF_SIGN_STARTSTATE w parserze (-1 = stare skrypty bez stanow)
	static float sgPower = 1.0f;   // moc biezacego skutku (SgDoCast) - czasy, odrzut, Yrden
	static zdcore::SignDamagePower sgDmg;   // moc obrazen biezacego skutku (SignPowerSource)
	static SgCastState sgCast;
	static SgSustain sgSustain;
	static SgTrap sgTrap;
	static SgPuppet sgPuppet;

	// ---------- pomocnicze ----------

	static void SgTell(const char* text) {
		DWORD now = GetTickCount();
		if (now - sgLastTell < 1200 || !ogame || !ogame->GetTextView())
			return;
		sgLastTell = now;
		ogame->GetTextView()->PrintTimedCXY(zSTRING(text), 2000.0f, nullptr);
	}

	static int SgParserInt(const char* name, int def) {
		zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(name)) : nullptr;
		int v = def;
		if (s)
			s->GetValue(v, 0);
		return v;
	}

	static void SgParserSetInt(const char* name, int v) {
		if (zCPar_Symbol* s = parser ? parser->GetSymbol(zSTRING(name)) : nullptr)
			s->SetValue(v, 0);
	}

	// Funkcja skryptu bez parametrow z self / other (przywracane). false = brak funkcji.
	static bool SgCallScript(const char* func, oCNpc* self, oCNpc* other) {
		int idx = parser ? parser->GetIndex(zSTRING(func)) : -1;
		zCPar_Symbol* sSelf = parser ? parser->GetSymbol(zSTRING("SELF")) : nullptr;
		zCPar_Symbol* sOther = parser ? parser->GetSymbol(zSTRING("OTHER")) : nullptr;
		if (idx < 0 || !sSelf || !sOther)
			return false;
		void* oldSelf = sSelf->GetInstanceAdr();
		void* oldOther = sOther->GetInstanceAdr();
		parser->SetInstance(zSTRING("SELF"), self);
		parser->SetInstance(zSTRING("OTHER"), other);
		parser->CallFunc(idx);
		parser->SetInstance(zSTRING("SELF"), oldSelf);
		parser->SetInstance(zSTRING("OTHER"), oldOther);
		return true;
	}

	// Nazwa do logu: GetInstanceName zwraca zSTRING przez wartosc - kopia w buforze (8 naraz w jednej linii logu).
	static const char* SgNpcName(oCNpc* npc) {
		static char buf[8][64];
		static int next = 0;
		if (!npc)
			return "-";
		char* b = buf[next];
		next = (next + 1) & 7;
		strncpy_s(b, 64, npc->GetInstanceName().ToChar(), _TRUNCATE);
		return b;
	}

	static bool SgAlive(oCNpc* npc) {
		return npc && npc != player && !npc->IsDead() && !npc->IsUnconscious() && npc->homeWorld && npc->GetModel();   // homeWorld najpierw: GetModel na zdespawnowanym = UnShrink (10-20 ms)
	}

	static float SgFlatDist(const zVEC3& a, const zVEC3& b) {
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2];
		return sqrtf(dx * dx + dz * dz);
	}

	// Moc z maksymalnej many bohatera (projekt, runda 3).
	static float SgPowerNow() {
		int maxMana = player ? player->attribute[NPC_ATR_MANAMAX] : 0;
		return zdcore::SignPower(maxMana, zcfg.signPowerPerMana, zcfg.signPowerMax);
	}

	static bool SgSustainedAlt(int sign) {
		return sign == zdcore::SIGN_IGNI || sign == zdcore::SIGN_QUEN;
	}

	static int SgAltCost(int sign) {
		switch (sign) {
		case zdcore::SIGN_AARD: return zcfg.aardAltCost;
		case zdcore::SIGN_YRDEN: return zcfg.yrdenAltCost;
		case zdcore::SIGN_AXII: return zcfg.axiiAltCost;
		}
		return 0;   // podtrzymywane: mana na sekunde
	}

	// Efekt wizualny po nazwie (instancja VisualFX ze skryptow). Brak instancji: CreateAndPlay zwraca 0 (0x48E7D2) - raz
	// w logu na nazwe. Zwracany efekt ma ref dla wolajacego (skrypt Wld_PlayEffect zwalnia go od razu); keep = trzymamy
	// (petle zatrzymywane przez plugin), inaczej zwalniamy. Trzymane nie trafiaja do zapisu (dontWriteIntoArchive).
	static oCVisualFX* SgFxDone(oCVisualFX* fx, const char* name, bool keep) {
		if (!fx) {
			if (std::find(sgMissingFx.begin(), sgMissingFx.end(), name) == sgMissingFx.end()) {
				sgMissingFx.push_back(name);
				ZLog("Znaki: brak efektu %s (VisualFX) - bez efektu", name);
			}
			return nullptr;
		}
		fx->dontWriteIntoArchive = TRUE;
		if (keep)
			return fx;
		fx->Release();
		return nullptr;
	}

	static oCVisualFX* SgFx(const char* name, zCVob* origin, zCVob* target, bool keep = false) {
		if (!origin || !origin->homeWorld)
			return nullptr;
		oCVisualFX* fx = oCVisualFX::CreateAndPlay(zSTRING(name), (const zCVob*)origin, (const zCVob*)target, 1, 0.0f, 0, FALSE);
		return SgFxDone(fx, name, keep);
	}

	static oCVisualFX* SgFxAt(const char* name, const zVEC3& pos, bool keep = false, zCVob* target = nullptr) {
		if (!ogame || !ogame->GetGameWorld())
			return nullptr;
		oCVisualFX* fx = oCVisualFX::CreateAndPlay(zSTRING(name), pos, (const zCVob*)target, 1, 0.0f, 0, FALSE);
		return SgFxDone(fx, name, keep);
	}

	// Zatrzymanie petli razem z dziecmi (emFXCreate_S z emFXCreatedOwnTrj = 1 zyja jako osobne efekty w fxList /
	// childList korzenia). oCVisualFX::Stop (0x493BE0) wraca od razu, gdy efekt jest juz skonczony (stan +0x3B8 >= 5) -
	// nie schodzi wtedy na dzieci; stad osierocone banki Quen. Dzieci zatrzymujemy same.
	static void SgFxStopTree(oCVisualFX* fx, int depth) {
		if (!fx || depth > 6)
			return;
		for (int i = 0; i < fx->fxList.GetNum(); i++)
			if (fx->fxList[i] && fx->fxList[i] != fx)
				SgFxStopTree(fx->fxList[i], depth + 1);
		for (int i = 0; i < fx->childList.GetNum(); i++)
			if (fx->childList[i] && fx->childList[i] != fx)
				SgFxStopTree(fx->childList[i], depth + 1);
		fx->Stop(TRUE);
	}

	static void SgFxStop(oCVisualFX*& fx) {
		if (!fx)
			return;
		SgFxStopTree(fx, 0);
		fx->Release();
		fx = nullptr;
	}

	// Bezpiecznik petli w skrypcie (emFXLifeSpan) liczy oCVisualFX::CheckDeletion (0x496E9C): lifeSpanTimer += frameTime,
	// po przekroczeniu emFXLifeSpan efekt konczy sie sam. Petle trzymane przez plugin (gaszone zawsze przez plugin) maja
	// licznik zerowany co klatke - razem z dziecmi - wiec trwaja tyle, ile znak (moc x2, zapas many), bez tworzenia od
	// nowa (nowy efekt gra od nowa swoj dzwiek startu). Bezpiecznik dziala dalej, gdyby plugin przestal je prowadzic.
	static void SgFxRefresh(oCVisualFX* fx, int depth = 0) {
		if (!fx || depth > 6)
			return;
		fx->lifeSpanTimer = 0.0f;
		for (int i = 0; i < fx->fxList.GetNum(); i++)
			if (fx->fxList[i] && fx->fxList[i] != fx)
				SgFxRefresh(fx->fxList[i], depth + 1);
		for (int i = 0; i < fx->childList.GetNum(); i++)
			if (fx->childList[i] && fx->childList[i] != fx)
				SgFxRefresh(fx->childList[i], depth + 1);
	}

	// Awaryjnie: petla mimo to skonczona (np. przerwana przez silnik) - od nowa (tylko gdy byla utworzona). Nie dla efektow
	// z dzwiekiem startu, ktorego powtorka razi (marionetka Axii - tylko SgFxRefresh).
	static void SgFxKeep(oCVisualFX*& fx, const char* name, zCVob* origin, zCVob* target) {
		if (fx && fx->IsFinished()) {
			SgFxStop(fx);
			fx = SgFx(name, origin, target, true);
		}
	}

	static void SgFxKeepAt(oCVisualFX*& fx, const char* name, const zVEC3& pos) {
		if (fx && fx->IsFinished()) {
			SgFxStop(fx);
			fx = SgFxAt(name, pos, true);
		}
	}

	// Tryb magii na czas obrazen od znaku (opis na gorze pliku).
	struct SgMagicMode {
		int saved;
		SgMagicMode() : saved(player->fmode) { player->fmode = NPC_WEAPON_MAG; }
		~SgMagicMode() { player->fmode = saved; }
	};

	static int sgOwnDamage = 0;   // > 0: trwa OnDamage od znaku (SgBeforeHit nie blokuje go jak ciosu bohatera)

	// Inteligencja bohatera ze skryptu: New Balance trzyma ja w zmiennej ATR_INTELLECT (RX_ChangeIntellect, talent
	// NPC_TALENT_INTELLECT); Steam G2 jej nie ma - -1.
	static int SgIntellect() {
		return SgParserInt("ATR_INTELLECT", -1);
	}

	// Moc obrazen znakow (SignPowerSource, zdcore::SignDamagePowerOf): mana / INT (SignPowerInt, SignPowerIntMax) / iloczyn.
	// Wszystkie obrazenia znakow ida przez ten jeden mnoznik (wolajacy mnoza obrazenia bazowe przed SgDamage, SgFixBurn
	// mnozy palenie); czasy efektow - z many (sgPower). Steam G2 bez ATR_INTELLECT: INT i oba = mana (fellBack, wpis w logu).
	static zdcore::SignDamagePower SgDamagePowerNow(float manaPower) {
		return zdcore::SignDamagePowerOf(zcfg.signPowerSource, manaPower, SgIntellect(), zcfg.signPowerInt, zcfg.signPowerIntMax);
	}

	static std::string SgDamagePowerText(const zdcore::SignDamagePower& d) {
		char buf[200];
		if (d.fellBack)
			sprintf_s(buf, "moc obrazen x%.2f (zrodlo: %s - SignPowerSource=%d, a skrypt bez ATR_INTELLECT, np. Steam G2)",
				d.mul, zdcore::SignPowerSourceName(d.source), zcfg.signPowerSource);
		else
			sprintf_s(buf, "moc obrazen x%.2f (zrodlo: %s)", d.mul, zdcore::SignPowerSourceName(d.source));
		return buf;
	}

	// New Balance: UNION_ABI.DLL podmienia oCNpc::OnDamage_Hit (wlasna funkcja obrazen, w DLL z 18.10 0x10358770). Gdy
	// napastnik jest postacia, bierze obrazenia z deskryptora aryDamage[typ] minus ochrona, a nie z fDamageTotal; ponizej
	// NPC_MINIMAL_DAMAGE daje 5 (0x1035A091..0x1035A239). oCNpc::OnDamage(zCVob*, zCVob*, float, int, zVEC3) 0x67B860
	// wypelnia tylko fDamageTotal (tablica zerowa, dwFieldsValid 0) - w NB kazdy znak bil po 5 niezaleznie od obrazen i
	// ochrony (log 20.10: Aard 17,8 obrazen w BDT_JINA_02 z ochrona lotu 0 -> HP -5). Steam G2 rozdziela fDamageTotal
	// sam (Aard 15 = -15 HP) - tam zostaje stara droga.
	static bool SgNewBalance() {
		static int nb = -1;
		if (nb < 0)
			nb = GetModuleHandleA("UNION_ABI.DLL") != nullptr ? 1 : 0;
		return nb == 1;
	}

	static bool SgDamageArray() {
		return zdcore::SignDamageArrayWanted(zcfg.signDamageArray, SgNewBalance());
	}

	// Deskryptor jak w 0x67B860 (napastnik i zrodlo = bohater, bez broni i efektu, czar -1, tryb broni magia, mnoznik 1,
	// miejsce i kierunek = punkt trafienia), do tego aryDamage[typ] = obrazenia; wirtualny OnDamage(oSDamageDescriptor&).
	static void SgDamageDesc(oCNpc* npc, float damage, int type, const zVEC3& at) {
		alignas(oCNpc::oSDamageDescriptor) unsigned char buf[sizeof(oCNpc::oSDamageDescriptor)];
		memset(buf, 0, sizeof(buf));
		oCNpc::oSDamageDescriptor& d = *reinterpret_cast<oCNpc::oSDamageDescriptor*>(buf);
		::new (static_cast<void*>(&d.strVisualFX)) zSTRING();   // globalny placement new - zSTRING ma wlasny operator new
		d.pVobAttacker = player;
		d.pNpcAttacker = player;
		d.pVobHit = player;
		d.nSpellID = d.nSpellCat = d.nSpellLevel = 0xFFFFFFFFul;
		d.enuModeDamage = (unsigned long)type;
		d.enuModeWeapon = 8;   // oETypeWeapon_Magic (0x67BACB przy fmode 7)
		d.fDamageTotal = damage;
		d.fDamageMultiplier = 1.0f;
		d.vecLocationHit = at;
		d.vecDirectionFly = at;
		zdcore::SignSplitDamage(d.enuModeDamage, damage, d.aryDamage);
		npc->OnDamage(d);
		d.strVisualFX.~zSTRING();
	}

	static void SgDamage(oCNpc* npc, float damage, int type) {
		if (damage <= 0.0f || !npc)
			return;
		zVEC3 b = npc->GetPositionWorld();
		zVEC3 at(b.n[0], b.n[1] + 40.0f, b.n[2]);
		SgMagicMode mm;
		sgOwnDamage++;
		if (SgDamageArray())
			SgDamageDesc(npc, damage, type, at);
		else
			npc->OnDamage(player, player, damage, type, at);
		sgOwnDamage--;
	}

	static int SgAivarPartyMember() {
		static int idx = -2;
		if (idx == -2)
			idx = SgParserInt("AIV_PARTYMEMBER", 15);
		return idx;
	}

	static zdcore::SignTargetInfo SgInfo(oCNpc* npc) {
		zdcore::SignTargetInfo t;
		t.isPlayer = npc == player;
		t.dead = npc->IsDead() != 0;
		t.unconscious = npc->IsUnconscious() != 0;
		int pm = SgAivarPartyMember();
		t.partyMember = pm >= 0 && pm < 100 && npc->aiscriptvars[pm] != 0;
		t.targetsHero = npc->enemy == player;
		t.weaponDrawn = npc->GetWeaponMode() != NPC_WEAPON_NONE;
		t.human = IsHumanGuild(npc);
		t.attitude = npc->GetAttitude(player);
		t.immortal = npc->HasFlag(NPC_FLAG_IMMORTAL) != 0;
		return t;
	}

	// Widocznosc celu: promienie z piersi bohatera (3/4 jego bboxa) do kilku punktow celu (srodek i 3/4 bboxa, pozycja
	// voba + 20 cm) - wystarczy jeden wolny. Niskie stwory (wilk) i nierowny teren nie chowaja sie za grzbietem terenu.
	// Postacie, przezroczyste poly i pociski nie zaslaniaja. why: opis pierwszej przeszkody (log).
	static bool SgVisibleFrom(const zVEC3& eye, oCNpc* npc, std::string* why) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world)
			return false;
		zVEC3 q = npc->GetPositionWorld();
		std::vector<float> ys = zdcore::LosTargetHeights(q.n[1], npc->bbox3D.mins.n[1], npc->bbox3D.maxs.n[1]);
		static zCArray<zCVob*> ignore;
		ignore.EmptyList();
		ignore.Insert(player);
		ignore.Insert(npc);
		int flags = zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP | zTRACERAY_VOB_IGNORE_NO_CD_DYN |
			zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_VOB_IGNORE_PROJECTILES;
		std::string first;
		for (float y : ys) {
			zVEC3 to(q.n[0], y, q.n[2]);
			if (!(world->TraceRayNearestHit(eye, to - eye, &ignore, flags) && world->traceRayReport.foundHit))
				return true;
			if (why && first.empty()) {
				const zVEC3& p = world->traceRayReport.foundIntersection;
				zCVob* v = world->traceRayReport.foundVob;
				char buf[200];
				sprintf_s(buf, "promien z (%.0f %.0f %.0f) do y=%.0f trafia %s w (%.0f %.0f %.0f)", eye.n[0], eye.n[1], eye.n[2], y,
					v ? v->GetObjectName().ToChar() : "poly swiata", p.n[0], p.n[1], p.n[2]);
				first = buf;
			}
		}
		if (why)
			*why = first + (ys.size() > 1 ? " (wszystkie punkty zasloniete)" : "");
		return false;
	}

	static zVEC3 SgHeroEye() {
		zVEC3 hp = player->GetPositionWorld();
		zVEC3 eye(hp.n[0], zdcore::LosEyeHeight(player->bbox3D.mins.n[1], player->bbox3D.maxs.n[1]), hp.n[2]);
		if (!(player->bbox3D.maxs.n[1] - player->bbox3D.mins.n[1] > 50.0f))
			eye.n[1] = hp.n[1] + 50.0f;
		return eye;
	}

	static bool SgVisible(oCNpc* npc, std::string* why) {
		return SgVisibleFrom(SgHeroEye(), npc, why);
	}

	// ---------- celowanie (runda 5) ----------
	// Gothic Free Aim (Ikarus/LeGo, Warsztat Steam 2786958841): w trybie magii stala GFA_ACTIVE ma bit GFA_ACT_FREEAIM (2),
	// gdy wybrany czar celuje (GFA_IsSpellEligible: canTurnDuringInvest, canChangeTargetDuringInvest i targetCollectAlgo
	// TARGET_COLLECT_FOCUS_FALLBACK_NONE - tak sa ustawione Spell_Aard / Igni / Axii / Yrden w zDismember_Signs.d).
	// zQuickSign (szybki Znak spod F) eksportuje ZQS_AimActive(): 1 = celownik widoczny (F trzymane, czar z kierunkiem; rzut
	// przy puszczeniu F albo tryb alternatywny po SignAltHold). Odwrotnie zQuickSign czyta ZDISM_SignHoldState (pasek ladowania).
	typedef int(__cdecl* SgZqsAimActive_t)();
	static SgZqsAimActive_t SgZqsAimFn() {
		static SgZqsAimActive_t fn = nullptr;
		static DWORD next = 0;
		if (fn)
			return fn;
		DWORD now = GetTickCount();
		if ((int)(now - next) < 0)
			return nullptr;
		next = now + 5000;   // pluginy laduja sie w roznej kolejnosci - ponowna proba co 5 s
		HMODULE m = GetModuleHandleA("zQuickSign.dll");
		if (!m)
			m = GetModuleHandleA("NB_UNION_PLUGIN_ALLOWED_01.DLL");
		if (m)
			fn = (SgZqsAimActive_t)GetProcAddress(m, "ZQS_AimActive");
		return fn;
	}

	static bool SgQsAiming() {
		SgZqsAimActive_t fn = SgZqsAimFn();
		return fn && fn() != 0;
	}

	// zQuickSign (23.10) eksportuje ZQS_SignGesture(int* castMs, int* lengthMs, int* alt): 1 = gest Znaku gra (ms do
	// pchniecia dloni / dotkniecia ziemi i do konca gestu, alt = gest trybu alternatywnego "w ziemie" calym cialem).
	// Pytany w haku oCSpell::Cast (zQuickSign startuje gest tuz przed Spell_Cast). -1 = brak eksportu (stara wersja).
	typedef int(__cdecl* SgZqsSignGesture_t)(int*, int*, int*);
	static int SgQsGesture(int* castMs, int* lengthMs, int* alt) {
		static SgZqsSignGesture_t fn = nullptr;
		static DWORD next = 0;
		*castMs = *lengthMs = *alt = 0;
		if (!fn) {
			DWORD now = GetTickCount();
			if ((int)(now - next) < 0)
				return -1;
			next = now + 5000;
			HMODULE m = GetModuleHandleA("zQuickSign.dll");
			if (!m)
				m = GetModuleHandleA("NB_UNION_PLUGIN_ALLOWED_01.DLL");
			if (m)
				fn = (SgZqsSignGesture_t)GetProcAddress(m, "ZQS_SignGesture");
			if (!fn)
				return -1;
		}
		return fn(castMs, lengthMs, alt);
	}

	static bool SgGfaAiming() {
		return player && player->fmode == NPC_WEAPON_MAG && (SgParserInt("GFA_ACTIVE", 0) & 2) != 0;
	}

	static void SgAimRelease(SgAim& a) {
		if (a.npc)
			a.npc->Release();
		a = SgAim();
	}

	// Promien jak GFA_AimRay: z kamery (przesuniety o odleglosc kamera - bohater, zeby nie trafic w nic za bohaterem) wzdluz
	// osi kamery, flagi GFA (bez przezroczystych poly, z woda, bez pociskow), bez bohatera. Kierunek stozkow = od bohatera
	// do punktu trafienia (paralaksa kamery za plecami), przy punkcie tuz obok bohatera - os kamery.
	static bool SgCameraAim(SgAim& a, const char* source) {
		SgAimRelease(a);
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		zCVob* cam = ogame ? ogame->camVob : nullptr;
		if (!world || !cam || !player)
			return false;
		zVEC3 cp = cam->GetPositionWorld(), out = cam->GetAtVectorWorld();
		zVEC3 hp = player->GetPositionWorld();
		float d = (hp - cp).Length();
		zVEC3 from = cp + out * d, dir = out * 3000.0f;
		int flags = zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_POLY_NORMAL | zTRACERAY_POLY_IGNORE_TRANSP |
			zTRACERAY_POLY_TEST_WATER | zTRACERAY_VOB_IGNORE_PROJECTILES;
		a.point = from + dir;
		if (world->TraceRayNearestHit(from, dir, (zCVob*)player, flags) && world->traceRayReport.foundHit) {
			a.point = world->traceRayReport.foundIntersection;
			if (oCNpc* npc = zDYNAMIC_CAST<oCNpc>(world->traceRayReport.foundVob)) {
				npc->AddRef();
				a.npc = npc;
			}
		}
		float dx = a.point.n[0] - hp.n[0], dz = a.point.n[2] - hp.n[2], l = sqrtf(dx * dx + dz * dz);
		if (l < 60.0f) {
			dx = out.n[0];
			dz = out.n[2];
			l = sqrtf(dx * dx + dz * dz);
		}
		if (l < 0.001f)
			return false;
		a.dirX = dx / l;
		a.dirZ = dz / l;
		a.valid = true;
		a.source = source;
		return true;
	}

	// Celowanie w chwili rzutu: GFA (runa w rece) albo celownik zQuickSign; inaczej brak (przod bohatera, jak dotad).
	static void SgCaptureAim(SgAim& a) {
		SgAimRelease(a);
		if (SgQsAiming())
			SgCameraAim(a, "celownik zQuickSign (F)");
		else if (SgGfaAiming())
			SgCameraAim(a, "Gothic Free Aim");
	}

	static SgAim sgAimNow;   // celowanie biezacego skutku (SgDoCast) - stozki, krag Yrden, cel Axii

	// Kierunek stozkow znakow: celownik albo przod bohatera.
	static zVEC3 SgFacing() {
		if (sgAimNow.valid)
			return zVEC3(sgAimNow.dirX, 0.0f, sgAimNow.dirZ);
		return player->GetAtVectorWorld();
	}

	struct SgConeTarget {
		oCNpc* npc;
		zdcore::ConeHit hit;
	};

	// Cele w stozku przed bohaterem (filtr: SignAffects - jak czar obszarowy gry, bez wymogu ataku). Debug=1: NPC do
	// zasiegu + 3 m z powodem odrzucenia - najblizszych 12, a przy braku celow najblizszych 5 z naglowkiem (kierunek,
	// zrodlo celowania). quiet = bez logu pominietych (takty strumienia).
	static std::vector<SgConeTarget> SgCone(const char* label, float range, float halfDeg, bool affectedOnly, bool quiet = false) {
		std::vector<SgConeTarget> out;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return out;
		zVEC3 p = player->GetPositionWorld(), at = SgFacing();
		struct Diag {
			float dist;
			std::string line;
		};
		std::vector<Diag> diag;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 q = npc->GetPositionWorld();
			zdcore::ConeHit h = zdcore::ConeTest(p.n[0], p.n[1], p.n[2], at.n[0], at.n[2], q.n[0], q.n[1], q.n[2],
				range, halfDeg, SG_MAX_DY);
			bool log = !quiet && zcfg.debug && h.dist <= range + 300.0f;
			char buf[320];
			if (!h.inside) {
				if (log) {
					sprintf_s(buf, "%s %.0f cm, kat %.0f st., dy %.0f: %s", SgNpcName(npc), h.dist, h.angleDeg, q.n[1] - p.n[1],
						zdcore::ConeReasonName(h.reason));
					diag.push_back({ h.dist, buf });
				}
				continue;
			}
			zdcore::SignTargetInfo info = SgInfo(npc);
			const char* reject = affectedOnly ? zdcore::SignRejectReason(info, zcfg.signHitNeutral)
				: info.dead ? "martwy" : info.unconscious ? "nieprzytomny" : info.partyMember ? "druzyna (AIV_PARTYMEMBER)" : nullptr;
			if (reject) {
				if (log) {
					sprintf_s(buf, "%s %.0f cm: %s (postawa %d, czlowiek %d, walczy z bohaterem %d, druzyna %d, niesmiertelny %d)",
						SgNpcName(npc), h.dist, reject, info.attitude, info.human, info.targetsHero, info.partyMember, info.immortal);
					diag.push_back({ h.dist, buf });
				}
				continue;
			}
			std::string why;
			if (!SgVisible(npc, log ? &why : nullptr)) {
				if (log)
					diag.push_back({ h.dist, std::string(SgNpcName(npc)) + ": zasloniety - " + why });
				continue;
			}
			out.push_back({ npc, h });
		}
		if (!diag.empty()) {
			std::sort(diag.begin(), diag.end(), [](const Diag& a, const Diag& b) { return a.dist < b.dist; });
			size_t limit = out.empty() ? 5 : 12;
			if (out.empty())
				ZDebug("%s: brak celow w %.0f cm / +-%.0f st. - najblizsi kandydaci (kierunek %.2f %.2f z %s):", label, range, halfDeg,
					at.n[0], at.n[2], sgAimNow.valid ? sgAimNow.source : "przodu bohatera");
			for (size_t i = 0; i < diag.size() && i < limit; i++)
				ZDebug("%s: pominiety %s", label, diag[i].line.c_str());
		}
		return out;
	}

	// ---------- ogien: Aard gasi, Igni podpala (runda 5) ----------
	// oCMobFire (ogniska, piece, paleniska, pochodnie w uchwytach - mob z fireSlot i fireVobtreeName): gra zapala go przez
	// oCMobFire::OnTrigger (0x722980: stan 0 -> 1, wczytuje drzewo vobow ognia fireVobtreeName - PFX, swiatlo, dzwiek -
	// w wezle fireSlot) i gasi przez OnUntrigger (0x722B90: drzewo ognia usuniete, stan 0). Tak samo zapala go trafienie
	// ogniem (oCMobFire::OnDamage 0x7228A0: obrazenia > 0 z bitem DAM_FIRE -> OnTrigger przez kolejke EM) - plugin wola te
	// same funkcje. Zapis: PreSave (0x722C00) zdejmuje drzewo ognia, gdy stan = 1, PostSave (0x722D40) zapala z powrotem
	// tylko palace sie - zgaszone zostaje zgaszone przez zapis; stanu nie ma w pliku zapisu (oCMobInter::Archive zapisuje
	// tylko stateNum ze swiata), wiec po wczytaniu ogien wraca do stanu ze swiata - jak przy kazdym przelaczeniu skryptem.
	// Pochodnie w rece NPC (przedmioty) - pomijane.
	//
	// Ognie z efektu (runda 6): wiekszosc ogni swiatow G2 to nie oCMobFire, tylko drzewa vobow z ZEN-a (NEWWORLD: 47 vobow
	// "FIRE", 197 FIRE_MEDIUM.PFX, 213 decali FIRE_COMPLETE_A0 - opis w Core\Signs.h): plomien = zCVob z wizualem
	// zCParticleFX (FIRE*.PFX, TORCH.PFX) albo animowanym decalem, obok dym / iskry / poswiata, zCVobLight (dynamiczne
	// z migotaniem albo statyczne - lightmapa) i zCVobSound (FIRE_MEDIUM, TORCH_BURN). Grupa plomienia: uchwyt (rodzic w drzewie
	// swiata, np. "LIGHT" ogniska albo mesh pochodni) i jego dzieci + luzne voby (pod kompozycja poziomu) w SG_FIRE_GROUP_R;
	// dzieci innego uchwytu (sasiednia pochodnia) i luzne swiatlo bedace uchwytem innego plomienia - nie.
	// Gaszenie: wizual ukryty (showVisual = 0 - silnik go nie rysuje ani nie liczy czastek, jak vob poza widokiem; bez
	// zawieszonego dymu), swiatlo dynamiczne zCVobLight::OnUntrigger (0x608120: zasieg 0, flaga wlaczenia zdjeta; statyczne -
	// silnik nic nie robi, pomijane), dzwiek zCVobSound::OnUntrigger (0x63E370: soundAllowedToRun zdjete, dzwiek zatrzymany).
	// Podpalenie: oryginaly z listy sgFiresOff (showVisual, zCVobLight::OnTrigger 0x608050 - zasieg z rangeBackup,
	// zCVobSound::OnTrigger 0x63E350 - dozwolony + StartSound). Zapis: showVisual, flaga swiatla (zasieg idzie z rangeBackup,
	// zCVobLightData::Archive 0x609AD1) i dzwiek trafiaja do pliku - zgaszony zostaje zgaszony po wczytaniu, a lista pluginu
	// jest czyszczona przy wczytaniu / zmianie swiata (SignsClearAll). Igni zapala wtedy ukryty plomien bez pamieci: pokazuje
	// go z ukrytymi dodatkami grupy, wlacza wylaczone swiatla dynamiczne i dzwieki ognia grupy.
	static const float SG_FIRE_GROUP_R = 200.0f;   // luzne swiatla / dzwieki / dym przy plomieniu (cm; drzewo uchwytu - 2x)
	static const float SG_FIRE_DY = 400.0f;        // roznica wysokosci (pochodnie wysoko na scianie)

	struct SgFireOff {
		zCVob* vob;          // AddRef
		zCVob* flame;        // plomien grupy (tylko porownanie wskaznika)
		int type;            // zdcore::FireMemberType
		bool on;             // oryginal: widoczny / swiatlo wlaczone / dzwiek dozwolony
		float range;         // swiatlo: zasieg (rangeBackup) - log
	};
	static std::vector<SgFireOff> sgFiresOff;

	static void SgFiresForget() {
		if (!sgFiresOff.empty())
			ZDebug("Ognie: lista zgaszonych wyczyszczona (%d vobow)", (int)sgFiresOff.size());
		for (auto& f : sgFiresOff)
			f.vob->Release();
		sgFiresOff.clear();
	}

	static std::vector<SgFireOff>::iterator SgFireOffFind(zCVob* v) {
		return std::find_if(sgFiresOff.begin(), sgFiresOff.end(), [v](const SgFireOff& f) { return f.vob == v; });
	}

	static zCVob* SgVobParent(zCVob* v) {
		zCTree<zCVob>* n = v ? v->globalVobTreeNode : nullptr;
		zCTree<zCVob>* p = n ? n->GetParent() : nullptr;
		return p ? p->GetData() : nullptr;
	}

	// Uchwyt plomienia: rodzic w drzewie swiata, o ile nie jest kompozycja poziomu (luzne voby pod LEVEL-VOB).
	static zCVob* SgFireHolder(zCVob* v) {
		zCVob* p = SgVobParent(v);
		return p && !zDYNAMIC_CAST<zCVobLevelCompo>(p) ? p : nullptr;
	}

	// Postac / przedmiot w drzewie (pochodnia w rece, przedmiot z wizualem .ZEN) albo drzewo ognia oCMobFire (korzen drzewa =
	// oCMobFire::fireVobtree; MergeVobSubtree z rodzicem 0 - pod korzeniem swiata, nie pod mobem).
	static void SgFireAncestry(zCVob* v, const std::vector<zCVob*>& mobTrees, bool& owned, bool& mobTree) {
		owned = mobTree = false;
		for (int d = 0; v && d < 12; d++, v = SgVobParent(v)) {
			if (zDYNAMIC_CAST<oCNpc>(v) || zDYNAMIC_CAST<oCItem>(v))
				owned = true;
			if (zDYNAMIC_CAST<oCMobFire>(v) || std::find(mobTrees.begin(), mobTrees.end(), v) != mobTrees.end())
				mobTree = true;
		}
	}

	// Nazwa wizuala czastek / decala ("" = inny wizual albo brak). Bez emitera / materialu GetVisualName silnika
	// (0x5ADD30 / 0x556B80) siega w null - wtedy "?".
	static std::string SgFxVisualName(zCVob* v) {
		zCVisual* vis = v ? v->visual : nullptr;
		if (!vis)
			return std::string();
		if (zCParticleFX* pfx = zDYNAMIC_CAST<zCParticleFX>(vis))
			return pfx->emitter ? std::string(pfx->GetVisualName().ToChar()) : std::string("?.PFX");
		if (zCDecal* d = zDYNAMIC_CAST<zCDecal>(vis))
			return d->decalMaterial ? std::string(d->GetVisualName().ToChar()) : std::string("?.TGA");
		return std::string();
	}

	static bool SgVobRuntime(zCVob* v) {
		return v->dontWriteIntoArchive || zDYNAMIC_CAST<oCVisualFX>(v);
	}

	static zdcore::FireFlame SgFlameInfo(zCVob* v, const std::string& visual, const std::vector<zCVob*>& mobTrees) {
		zdcore::FireFlame f;
		f.kind = zdcore::FireVisualKind(visual, v->GetObjectName().ToChar());
		f.runtime = SgVobRuntime(v);
		SgFireAncestry(v, mobTrees, f.owned, f.mobTree);
		f.shown = v->showVisual != 0;
		return f;
	}

	// Za sciana (promien z oczu bohatera; przeszkoda tuz przy ogniu - kamienie paleniska, uchwyt pochodni - sie nie liczy).
	static bool SgFireBlocked(oCWorld* world, const zVEC3& eye, const zVEC3& to, zCVob* a, zCVob* b) {
		static zCArray<zCVob*> ignore;
		ignore.EmptyList();
		ignore.Insert(player);
		if (a)
			ignore.Insert(a);
		if (b)
			ignore.Insert(b);
		return world->TraceRayNearestHit(eye, to - eye, &ignore, zTRACERAY_STAT_POLY | zTRACERAY_POLY_IGNORE_TRANSP |
			zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_VOB_IGNORE_PROJECTILES) &&
			world->traceRayReport.foundHit && (world->traceRayReport.foundIntersection - to).Length() > 80.0f;
	}

	struct SgFireMemberRef {
		zCVob* vob;
		int type;    // zdcore::FireMemberType
		int skip;    // zdcore::FireMemberSkip
		float dist;
	};

	// Bezposrednie dzieci voba w drzewie swiata (uchwyt pochodni / swiatlo ogniska -> plomien, dym, poswiata, swiatlo, dzwiek).
	static void SgVobChildren(zCVob* v, std::vector<zCVob*>& out) {
		zCTree<zCVob>* node = v ? v->globalVobTreeNode : nullptr;
		int n = 0;
		for (zCTree<zCVob>* c = node ? node->GetFirstChild() : nullptr; c && n < 32; c = c->GetNextChild(), n++)
			if (c->GetData())
				out.push_back(c->GetData());
	}

	// Luzne swiatlo / dzwiek, ktore samo jest uchwytem plomienia (zCVobLight "LIGHT" sasiedniego ogniska) - nie nasze.
	static bool SgHoldsFlame(zCVob* v) {
		std::vector<zCVob*> ch;
		SgVobChildren(v, ch);
		for (zCVob* c : ch)
			if (zdcore::FireVisualKind(SgFxVisualName(c), c->GetObjectName().ToChar()) == zdcore::FIREVIS_FLAME)
				return true;
		return false;
	}

	// Czlonkowie grupy plomienia: uchwyt i jego dzieci, dzieci plomienia + voby z listy CollectVobsInBBox3D do
	// 2x SG_FIRE_GROUP_R (bez GetModel - postacie odpadaja na rzutowaniu typu). ignite = stan dla Igni bez pamieci.
	static std::vector<SgFireMemberRef> SgFireMembers(zCVob* flame, const zCArray<zCVob*>& found,
		const std::vector<zCVob*>& mobTrees, bool ignite) {
		std::vector<SgFireMemberRef> out;
		zCVob* holder = SgFireHolder(flame);
		std::vector<zCVob*> cand;
		if (holder) {
			cand.push_back(holder);
			SgVobChildren(holder, cand);
		}
		SgVobChildren(flame, cand);
		for (int i = 0; i < found.GetNum(); i++)
			cand.push_back(found[i]);
		zVEC3 fp = flame->GetPositionWorld();
		std::vector<zCVob*> seen;
		for (zCVob* v : cand) {
			if (!v || v == flame || !v->homeWorld || std::find(seen.begin(), seen.end(), v) != seen.end())
				continue;
			seen.push_back(v);
			zdcore::FireMember m;
			m.dist = (v->GetPositionWorld() - fp).Length();
			if (m.dist > 2.0f * SG_FIRE_GROUP_R)
				continue;
			zCVobLight* light = zDYNAMIC_CAST<zCVobLight>(v);
			zCVobSound* sound = light ? nullptr : zDYNAMIC_CAST<zCVobSound>(v);
			if (light) {
				m.type = zdcore::FMEM_LIGHT;
				m.staticLight = light->lightData.isStatic != 0;
				m.active = light->lightData.isTurnedOn != 0;
			}
			else if (sound) {
				m.type = zdcore::FMEM_SOUND;
				m.fireSound = zdcore::FireSoundName(sound->soundName.ToChar());
				m.active = sound->soundAllowedToRun != 0;
			}
			else {
				std::string vis = SgFxVisualName(v);
				if (vis.empty())
					continue;   // mesh uchwytu, przedmioty, postacie
				m.type = zdcore::FMEM_VISUAL;
				m.visKind = zdcore::FireVisualKind(vis, v->GetObjectName().ToChar());
				m.active = v->showVisual != 0;
			}
			zCVob* parent = SgVobParent(v);
			m.sameTree = (holder && (v == holder || parent == holder)) || parent == flame;
			m.loose = !m.sameTree && !SgFireHolder(v) && (m.type == zdcore::FMEM_VISUAL || !SgHoldsFlame(v));
			bool mobTree;
			SgFireAncestry(v, mobTrees, m.owned, mobTree);
			m.owned = m.owned || mobTree;
			m.runtime = SgVobRuntime(v);
			out.push_back({ v, m.type, zdcore::FireMemberCheck(m, SG_FIRE_GROUP_R, ignite), m.dist });
		}
		return out;
	}

	// Gaszenie voba grupy; oryginal na liste (raz - vob juz na liscie zostaje z pierwszym oryginalem).
	static void SgFireTurnOff(zCVob* v, zCVob* flame, int type) {
		SgFireOff e = { v, flame, type, true, 0.0f };
		if (type == zdcore::FMEM_LIGHT) {
			zCVobLight* l = static_cast<zCVobLight*>(v);
			e.on = l->lightData.isTurnedOn != 0;
			e.range = l->lightData.rangeBackup;
			l->OnUntrigger(player, player);
		}
		else if (type == zdcore::FMEM_SOUND) {
			zCVobSound* s = static_cast<zCVobSound*>(v);
			e.on = s->soundAllowedToRun != 0;
			s->OnUntrigger(player, player);
		}
		else {
			e.on = v->showVisual != 0;
			v->showVisual = FALSE;
		}
		if (SgFireOffFind(v) == sgFiresOff.end()) {
			v->AddRef();
			sgFiresOff.push_back(e);
		}
	}

	static void SgFireTurnOn(zCVob* v, int type) {
		if (type == zdcore::FMEM_LIGHT) {
			zCVobLight* l = static_cast<zCVobLight*>(v);
			if (!l->lightData.isTurnedOn)
				l->OnTrigger(player, player);
		}
		else if (type == zdcore::FMEM_SOUND) {
			zCVobSound* s = static_cast<zCVobSound*>(v);
			if (!s->soundAllowedToRun)
				s->OnTrigger(player, player);
		}
		else
			v->showVisual = TRUE;
	}

	static void SgFireCountLine(const int cnt[3], int staticLights, char* buf, size_t size) {
		sprintf_s(buf, size, "wizuale %d, swiatla %d, dzwieki %d%s", cnt[zdcore::FMEM_VISUAL], cnt[zdcore::FMEM_LIGHT],
			cnt[zdcore::FMEM_SOUND], staticLights ? ", swiatlo statyczne bez zmian (lightmapa)" : "");
	}

	// Aard: plomien z grupa. Zwraca opis do logu.
	static std::string SgFireExtinguish(zCVob* flame, const zCArray<zCVob*>& found, const std::vector<zCVob*>& mobTrees) {
		int cnt[3] = { 1, 0, 0 }, statics = 0;
		SgFireTurnOff(flame, flame, zdcore::FMEM_VISUAL);
		for (auto& m : SgFireMembers(flame, found, mobTrees, false)) {
			if (m.skip == zdcore::FMSKIP_STATIC_LIGHT)
				statics++;
			if (m.skip != zdcore::FMSKIP_OK)
				continue;
			SgFireTurnOff(m.vob, flame, m.type);
			cnt[m.type]++;
		}
		char buf[160];
		SgFireCountLine(cnt, statics, buf, sizeof(buf));
		return buf;
	}

	// Igni: grupa z listy (oryginaly; plomien bedacy dodatkiem innej grupy zapala cala tamta grupe), inaczej - bez pamieci
	// (po wczytaniu): plomien + nieaktywni czlonkowie grupy.
	static std::string SgFireIgnite(zCVob* flame, const zCArray<zCVob*>& found, const std::vector<zCVob*>& mobTrees) {
		int cnt[3] = { 0, 0, 0 }, statics = 0;
		auto own = SgFireOffFind(flame);
		zCVob* leader = own != sgFiresOff.end() ? own->flame : nullptr;   // ("group" to makro Union)
		char buf[200];
		if (leader) {
			for (auto it = sgFiresOff.begin(); it != sgFiresOff.end();) {
				if (it->flame != leader) {
					++it;
					continue;
				}
				if (it->on && it->vob->homeWorld) {
					SgFireTurnOn(it->vob, it->type);
					cnt[it->type]++;
				}
				it->vob->Release();
				it = sgFiresOff.erase(it);
			}
			flame->showVisual = TRUE;
			SgFireCountLine(cnt, 0, buf, sizeof(buf));
			return std::string("z listy: ") + buf;
		}
		flame->showVisual = TRUE;
		cnt[zdcore::FMEM_VISUAL] = 1;
		for (auto& m : SgFireMembers(flame, found, mobTrees, true)) {
			if (m.skip == zdcore::FMSKIP_STATIC_LIGHT)
				statics++;
			if (m.skip != zdcore::FMSKIP_OK)
				continue;
			SgFireTurnOn(m.vob, m.type);
			cnt[m.type]++;
		}
		SgFireCountLine(cnt, statics, buf, sizeof(buf));
		return std::string("bez pamieci (stan ze swiata, np. po wczytaniu): ") + buf;
	}

	struct SgFireDiag {
		float dist;
		std::string line;
	};

	static void SgFireDiagAdd(std::vector<SgFireDiag>& diag, zCVob* v, const std::string& vis, float dist, const char* why) {
		char buf[320];
		sprintf_s(buf, "%s '%s' %s %.0f cm: %s", v->_GetClassDef()->className.ToChar(), v->GetObjectName().ToChar(),
			vis.c_str(), dist, why);
		diag.push_back({ dist, buf });
	}

	// Voby wokol bohatera dla znaku o zasiegu range: zasieg + promien grupy plomienia (luzne swiatla / dzwieki przy plomieniu
	// na brzegu stozka), +-SG_FIRE_DY w pionie. Jeden CollectVobsInBBox3D na rzut (ognie i zamki Aarda z tej samej listy).
	static bool SgCollectNear(float range, zCArray<zCVob*>& found) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !world || !world->bspTree.bspRoot)
			return false;
		zVEC3 hp = player->GetPositionWorld();
		float reach = range + SG_FIRE_GROUP_R;
		zTBBox3D box;
		box.mins = zVEC3(hp.n[0] - reach, hp.n[1] - SG_FIRE_DY, hp.n[2] - reach);
		box.maxs = zVEC3(hp.n[0] + reach, hp.n[1] + SG_FIRE_DY, hp.n[2] + reach);
		world->bspTree.bspRoot->CollectVobsInBBox3D(found, box);
		return true;
	}

	// quiet = bez diagnostyki pominietych (takty strumienia Igni co ok. 1 s). pre = lista z SgCollectNear (ten sam rzut);
	// UWAGA: po SgFires lista moze miec zwolnione voby (oCMobFire::OnUntrigger) - kto jeszcze jej uzywa, idzie PRZED.
	static void SgFires(bool ignite, float range, float halfDeg, const char* label, bool quiet = false,
		const zCArray<zCVob*>* pre = nullptr) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!zcfg.signFires || !player || !world || !world->bspTree.bspRoot)
			return;
		zVEC3 hp = player->GetPositionWorld(), at = SgFacing();
		zCArray<zCVob*> local;
		if (!pre)
			SgCollectNear(range, local);
		const zCArray<zCVob*>& found = pre ? *pre : local;
		zVEC3 eye = SgHeroEye();
		bool diagOn = zcfg.debug && !quiet;
		std::vector<SgFireDiag> diag;
		// Drzewa ognia mobow - przed jakimkolwiek gaszeniem: oCMobFire::OnUntrigger zwalnia voby drzewa (wskazniki w found).
		std::vector<oCMobFire*> mobs;
		std::vector<zCVob*> mobTrees;
		for (int i = 0; i < found.GetNum(); i++)
			if (oCMobFire* fire = zDYNAMIC_CAST<oCMobFire>(found[i]))
				if (fire->homeWorld) {
					mobs.push_back(fire);
					if (fire->fireVobtree)
						mobTrees.push_back(fire->fireVobtree);
				}
		// 1) ognie z efektu (najpierw - voby drzew mobow sa jeszcze zywe)
		std::vector<std::pair<zCVob*, float>> flames;
		for (int i = 0; i < found.GetNum(); i++) {
			zCVob* v = found[i];
			if (!v || !v->homeWorld || zDYNAMIC_CAST<oCMobFire>(v))
				continue;
			std::string vis = SgFxVisualName(v);
			if (vis.empty())
				continue;
			zVEC3 q = v->GetPositionWorld();
			zdcore::ConeHit h = zdcore::ConeTest(hp.n[0], hp.n[1], hp.n[2], at.n[0], at.n[2], q.n[0], q.n[1], q.n[2], range,
				halfDeg, SG_FIRE_DY);
			if (h.reason == zdcore::CONE_RANGE && !diagOn)
				continue;
			zdcore::FireFlame f = SgFlameInfo(v, vis, mobTrees);
			int skip = zdcore::FireFlameCheck(f, ignite);
			char why[160];
			if (skip == zdcore::FIRESKIP_OK && !h.inside) {
				skip = zdcore::FIRESKIP_CONE;
				sprintf_s(why, "%s (%s, kat %.0f st., dy %.0f)", zdcore::FireSkipName(skip), zdcore::ConeReasonName(h.reason),
					h.angleDeg, q.n[1] - hp.n[1]);
			}
			else if (skip == zdcore::FIRESKIP_OK && SgFireBlocked(world, eye, zVEC3(q.n[0], q.n[1] + 20.0f, q.n[2]), v,
				SgFireHolder(v)))
				skip = zdcore::FIRESKIP_BLOCKED;
			if (skip == zdcore::FIRESKIP_OK) {
				flames.push_back({ v, h.dist });
				continue;
			}
			if (diagOn && h.dist <= range) {
				if (skip != zdcore::FIRESKIP_CONE)
					sprintf_s(why, "%s (%s, %s)", zdcore::FireSkipName(skip), zdcore::FireVisKindName(f.kind),
						f.shown ? "widoczny" : "ukryty");
				SgFireDiagAdd(diag, v, vis, h.dist, why);
			}
		}
		int changed = 0;
		for (auto& fl : flames) {
			zCVob* v = fl.first;
			if ((v->showVisual != 0) == ignite)
				continue;   // grupa wczesniejszego plomienia juz go zmienila
			std::string vis = SgFxVisualName(v), how = ignite ? SgFireIgnite(v, found, mobTrees) :
				SgFireExtinguish(v, found, mobTrees);
			changed++;
			ZDebug("%s: %s ogien z efektu %s '%s' (%s, %.0f cm) - %s", label, ignite ? "zapalony" : "zgaszony",
				v->_GetClassDef()->className.ToChar(), v->GetObjectName().ToChar(), vis.c_str(), fl.second, how.c_str());
		}
		// 2) oCMobFire (OnUntrigger zwalnia drzewo ognia - po petli po found)
		for (oCMobFire* fire : mobs) {
			zVEC3 q = fire->GetPositionWorld();
			zdcore::ConeHit h = zdcore::ConeTest(hp.n[0], hp.n[1], hp.n[2], at.n[0], at.n[2], q.n[0], q.n[1], q.n[2], range,
				halfDeg, SG_FIRE_DY);
			bool burning = fire->state != 0 || fire->fireVobtree;
			if (!h.inside || burning == ignite) {
				if (diagOn && h.dist <= range)
					SgFireDiagAdd(diag, fire, fire->fireVobtreeName.ToChar(), h.dist, !h.inside ?
						zdcore::ConeReasonName(h.reason) : burning ? "oCMobFire juz plonie" : "oCMobFire juz zgaszony");
				continue;
			}
			if (SgFireBlocked(world, eye, zVEC3(q.n[0], q.n[1] + 30.0f, q.n[2]), fire, nullptr)) {
				ZDebug("%s: ogien %s (%.0f cm) zasloniety - bez zmian", label, fire->GetObjectName().ToChar(), h.dist);
				continue;
			}
			if (ignite)
				fire->OnTrigger(player, player);
			else
				fire->OnUntrigger(player, player);
			changed++;
			ZDebug("%s: %s ogien %s (%s, %.0f cm), stan %d", label, ignite ? "zapalony" : "zgaszony",
				fire->GetObjectName().ToChar(), fire->fireVobtreeName.ToChar(), h.dist, fire->state);
		}
		if (changed) {
			ZDebug("%s: ognie %s: %d (lista zgaszonych: %d vobow)", label, ignite ? "zapalone" : "zgaszone", changed,
				(int)sgFiresOff.size());
			return;
		}
		if (!diagOn)
			return;
		std::vector<float> d;
		for (auto& x : diag)
			d.push_back(x.dist);
		std::vector<int> idx = zdcore::NearestIndices(d, 5);
		ZDebug("%s: ogien - bez zmian; w zasiegu %.0f cm vobow PFX / decali / oCMobFire: %d%s", label, range, (int)diag.size(),
			idx.empty() ? " (zadnego - ogien dalej albo wyzej / nizej niz 4 m)" : ", najblizsze:");
		for (int i : idx)
			ZDebug("%s:   %s", label, diag[i].line.c_str());
	}

	// ---------- Aard otwiera zamki (runda 6, SignAardUnlock) ----------
	// Zasady jak runa otwierania NB (UNION_ABI Mob_SetUnLocked_Rune 0x1016A170, mob z fokusu): oCMobContainer / oCMobDoor
	// zamkniety i (keyInstance == "" albo pickLockStr niepusty) -> vtable+0x120 = oCMobLockable::SetLocked(0) (0x719620),
	// potem skrypt UNLOCKDONE ("Otwieranie zakonczone!" + $ITEMREADY); klucz bez wytrycha -> CANTUNLOCK. Runa nie zglasza
	// przestepstwa (brak percepcji / B_Attack) - Aard tez nie; kradziez zawartosci zglasza dopiero gra jak zwykle.
	// Steam (bez runy otwierania w skryptach): tylko zamki do wytrycha (pickLockStr niepusty), bez kluczowych.
	// Komunikat i dzwiek raz na rzut (Aard w stozku moze otworzyc kilka zamkow naraz).
	static void SgUnlocks(const zCArray<zCVob*>& found, float range, float halfDeg, const char* label) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!zcfg.signAardUnlock || !player || !world)
			return;
		int rules = zdcore::LockRulesFor(parser && parser->GetIndex(zSTRING("MOB_SETUNLOCKED_RUNE")) >= 0);
		zVEC3 hp = player->GetPositionWorld(), at = SgFacing(), eye = SgHeroEye();
		int opened = 0, keyOnly = 0;
		for (int i = 0; i < found.GetNum(); i++) {
			oCMobLockable* mob = zDYNAMIC_CAST<oCMobLockable>(found[i]);
			if (!mob || !mob->homeWorld)
				continue;
			zVEC3 c = (mob->bbox3D.mins + mob->bbox3D.maxs) * 0.5f;
			zdcore::ConeHit h = zdcore::ConeTest(hp.n[0], hp.n[1], hp.n[2], at.n[0], at.n[2], c.n[0], c.n[1], c.n[2], range,
				halfDeg, SG_FIRE_DY);
			if (!h.inside)
				continue;
			zdcore::LockInfo li;
			li.locked = mob->locked != 0;
			li.hasKey = mob->keyInstance.Length() > 0;
			li.hasPickLock = mob->pickLockStr.Length() > 0;
			int v = zdcore::LockDecide(li, rules);
			if (v == zdcore::LOCK_SKIP_UNLOCKED)
				continue;
			const char* cls = mob->_GetClassDef()->className.ToChar();
			if (!zdcore::LockVerdictOpens(v)) {
				if (v == zdcore::LOCK_SKIP_KEY_ONLY)
					keyOnly++;
				ZDebug("%s: pominiety zamek %s '%s' %s (%s, %.0f cm; klucz '%s', wytrych '%s', zasady: %s)", label, cls,
					mob->GetObjectName().ToChar(), mob->name.ToChar(), zdcore::LockVerdictName(v), h.dist,
					mob->keyInstance.ToChar(), mob->pickLockStr.ToChar(), zdcore::LockRulesName(rules));
				continue;
			}
			if (SgFireBlocked(world, eye, c, mob, nullptr)) {
				ZDebug("%s: zamek %s '%s' (%.0f cm) zasloniety - bez zmian", label, cls, mob->GetObjectName().ToChar(), h.dist);
				continue;
			}
			mob->SetLocked(FALSE);
			opened++;
			if (zsound)
				zsound->PlaySound3D(zSTRING("PICKLOCK_SUCCESS"), mob, zSND_SLOT_NONE, nullptr);
			ZDebug("%s: otwarty zamek %s '%s' %s (%s, %.0f cm; klucz '%s', wytrych '%s', zasady: %s)", label, cls,
				mob->GetObjectName().ToChar(), mob->name.ToChar(), zdcore::LockVerdictName(v), h.dist,
				mob->keyInstance.ToChar(), mob->pickLockStr.ToChar(), zdcore::LockRulesName(rules));
		}
		if (opened) {
			// NB: komunikat i glos jak po runie otwierania; Steam - napis na ekranie
			if (!SgCallScript("UNLOCKDONE", player, player)) {
				char buf[64];
				sprintf_s(buf, opened == 1 ? "Aard: zamek otwarty" : "Aard: otwarte zamki: %d", opened);
				SgTell(buf);
			}
			ZDebug("%s: zamki otwarte: %d, tylko na klucz: %d", label, opened, keyOnly);
		}
	}

	// Aard: zamki (przed ogniami - OnUntrigger ogni zwalnia voby z listy) i ognie z jednej listy vobow na rzut.
	static void SgAardWorld(float range, float halfDeg, const char* label) {
		if (!zcfg.signAardUnlock && !zcfg.signFires)
			return;
		zCArray<zCVob*> found;
		if (!SgCollectNear(range, found))
			return;
		SgUnlocks(found, range, halfDeg, label);
		SgFires(false, range, halfDeg, label, false, &found);
	}

	// ---------- odsuniecie ----------

	static void SgPushStop(std::vector<SgPush>::iterator it) {
		it->npc->Release();
		sgPushes.erase(it);
	}

	static void SgPushStart(oCNpc* npc, float dist, float dur = SG_PUSH_TIME) {
		if (dist <= 0.0f || !npc)
			return;
		for (auto it = sgPushes.begin(); it != sgPushes.end(); ++it)
			if (it->npc == npc) {
				SgPushStop(it);
				break;
			}
		zVEC3 a = player->GetPositionWorld(), b = npc->GetPositionWorld();
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2], l = sqrtf(dx * dx + dz * dz);
		float g;
		if (!MoveGroundAt(b, g))
			return;
		zVEC3 dir;
		if (l > 1.0f)
			dir = zVEC3(dx / l, 0.0f, dz / l);
		else {
			zVEC3 at = player->GetAtVectorWorld();
			float al = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
			dir = al > 0.01f ? zVEC3(at.n[0] / al, 0.0f, at.n[2] / al) : zVEC3(0.0f, 0.0f, 1.0f);
		}
		npc->AddRef();
		sgPushes.push_back({ npc, dir, 0.0f, dist, 0.0f, b.n[1] - g, dur > 0.05f ? dur : SG_PUSH_TIME });
	}

	static void SgPushUpdate(float dt) {
		static const zdcore::TerrainLimits lim = zdcore::TerrainLimits();
		for (auto it = sgPushes.begin(); it != sgPushes.end();) {
			SgPush& p = *it;
			if (!FoeReady(p.npc) || p.t >= p.dur) {
				p.npc->Release();
				it = sgPushes.erase(it);
				continue;
			}
			p.t += dt;
			float want = zdcore::PushOffset(p.t, p.dist, p.dur) - p.done;
			if (want < 0.05f) {
				++it;
				continue;
			}
			zVEC3 cur = p.npc->GetPositionWorld(), n;
			zVEC3 d(p.dir.n[0] * want, 0.0f, p.dir.n[2] * want);
			float g;
			if (MvTryStep(p.npc, player, cur, cur.n[1] - p.yOff, d, lim, g, n) != MVSTEP_OK) {
				p.npc->Release();
				it = sgPushes.erase(it);   // sciana, urwisko - koniec odsuwania
				continue;
			}
			p.npc->SetPositionWorld(zVEC3(cur.n[0] + d.n[0], g + p.yOff, cur.n[2] + d.n[2]));
			p.done += want;
			++it;
		}
	}

	// Zatoczenie (RepelStagger z Dodge.cpp: ClearEM, Interrupt, BS_STUMBLE, EV_PLAYANI_NOOVERLAY T_STUMBLE(B)) i odsuniecie.
	static const char* SgStaggerPush(oCNpc* npc, float dist, float dur = SG_PUSH_TIME) {
		int r = RepelStagger(npc);
		if (r > 0) {
			SgPushStart(npc, dist, dur);
			return "potkniecie + odsuniecie";
		}
		return r < 0 ? "bez T_STUMBLE" : "stan bez przerywania";
	}

	// ---------- Aard: sprawdzenie lotu ----------
	// Silnik odmawia lotu (oCNpc::OnDamage_Anim 0x675BD0): skuteczne obrazenia lotu + bariery = 0 (0x67682A..0x676846),
	// woda (oCAniCtrl_Human::GetWaterLevel != 0, 0x678F0A), stan lezenia BS_LIE (0x678F1E), sterownik nie oCAIHuman /
	// brak modelu (wczesne wyjscie 0x675F41..0x675F57) oraz caly OnDamage pominiety (oCNpc::IsConditionValid 0x66648E,
	// dialog: komunikat EV_PROCESSINFOS w kolejce 0x6664FB). StartFlyDamage (0x69D940) zawsze startuje S_FALL(B), wlacza
	// fizyke i daje impuls - ale AI celu (atak w toku, kolejka EM, StandUp) potrafi go zaraz zgasic. Dlatego plugin
	// sprawdza po AardFlyCheck s, czy NPC naprawde polecial (przesuniecie od bohatera albo nadal w powietrzu).

	static bool SgAirborne(oCNpc* npc) {
		zCModel* m = npc->GetModel();
		return npc->physicsEnabled || (m && (m->IsAnimationActive(zSTRING("S_FALLB")) || m->IsAnimationActive(zSTRING("S_FALL"))));
	}

	static void SgFlyWatch(oCNpc* npc, float push) {
		zVEC3 a = player->GetPositionWorld(), b = npc->GetPositionWorld();
		float dx = b.n[0] - a.n[0], dz = b.n[2] - a.n[2], l = sqrtf(dx * dx + dz * dz);
		zVEC3 dir = l > 1.0f ? zVEC3(dx / l, 0.0f, dz / l) : zVEC3(0.0f, 0.0f, 1.0f);
		npc->AddRef();
		sgFlies.push_back({ npc, b, dir, 0.0f, push });
	}

	static void SgFlyUpdate(float dt) {
		for (auto it = sgFlies.begin(); it != sgFlies.end();) {
			SgFly& f = *it;
			f.t += dt;
			if (!FoeReady(f.npc) || DirectorIsVictim(f.npc)) {
				f.npc->Release();
				it = sgFlies.erase(it);
				continue;
			}
			zVEC3 q = f.npc->GetPositionWorld();
			float moved = (q.n[0] - f.start.n[0]) * f.dir.n[0] + (q.n[2] - f.start.n[2]) * f.dir.n[2];
			bool air = SgAirborne(f.npc);
			zdcore::FlyVerdict v = zdcore::AardFlyVerdict(f.t, moved, air, zcfg.aardFlyCheck, zcfg.aardFlyMinMove);
			if (v == zdcore::FLY_WAIT) {
				++it;
				continue;
			}
			if (v == zdcore::FLY_OK)
				ZDebug("Aard: %s polecial (%.0f cm w %.2f s, w powietrzu %d)", SgNpcName(f.npc), moved, f.t, air);
			else {
				const char* r = SgStaggerPush(f.npc, f.push, 0.55f);
				ZDebug("Aard: %s NIE polecial (%.0f cm w %.2f s, stan ciala %d, fizyka %d) - odrzut pluginu %.0f cm: %s",
					SgNpcName(f.npc), moved, f.t, f.npc->GetBodyState() & BS_ONLY_STATE, f.npc->physicsEnabled != 0, f.push, r);
			}
			f.npc->Release();
			it = sgFlies.erase(it);
		}
	}

	// ---------- stany znakow (Axii, Igni) ----------

	static SgHold* SgFindHold(oCNpc* npc) {
		for (auto& h : sgHolds)
			if (h.npc == npc)
				return &h;
		return nullptr;
	}

	static bool SgModelHas(zCModel* m, const char* ani) {
		return m && m->GetAniIDFromAniName(zSTRING(ani)) >= 0;
	}

	static bool SgStateIs(oCNpc* npc, const char* name) {
		const TNpcAIState& s = npc->state.curState;
		return s.valid && _stricmp(s.name.ToChar(), name) == 0;
	}

	// Stany oszolomienia i palenia (ZS_ZDF_Stun*, ZS_ZDF_Burn) - bez stanu marionetki ZS_ZDF_Puppet.
	static bool SgInSignState(oCNpc* npc) {
		const TNpcAIState& s = npc->state.curState;
		return s.valid && (_strnicmp(s.name.ToChar(), "ZS_ZDF_STUN", 11) == 0 || _stricmp(s.name.ToChar(), "ZS_ZDF_BURN") == 0);
	}

	// Stan mozna zaczac tylko z ruchu / walki na ziemi (nie w wodzie, w powietrzu, lezac, przy interakcji).
	// trance: trans Axii poza walka - tez z siedzenia / interakcji z mobem (stan zaczyna AI_StandUp), bez wody i lezenia.
	static bool SgCanTakeState(oCNpc* npc, bool trance = false) {
		int bs = npc->GetBodyState() & BS_ONLY_STATE;
		bool ok = bs == BS_STAND || bs == BS_WALK || bs == BS_SNEAK || bs == BS_RUN || bs == BS_SPRINT ||
			bs == BS_AIMNEAR || bs == BS_AIMFAR || bs == BS_HIT || bs == BS_PARADE || bs == BS_STUMBLE ||
			(trance && (bs == BS_SIT || bs == BS_MOBINTERACT || bs == BS_MOBINTERACT_INTERRUPT || bs == BS_ITEMINTERACT));
		oCAniCtrl_Human* ac = npc->GetAnictrl();
		return ok && ac && ac->GetWaterLevel() == 0 && !npc->bodyStateInterruptableOverride;
	}

	// Wejscia wariantow (musza byc w modelu - inaczej wariant wypada z losowania).
	static const char* SgStunEntryAni(int v) {
		switch (v) {
		case zdcore::STUN_PRAY: return "T_STAND_2_PRAY";
		case zdcore::STUN_FEAR1: return "T_STAND_2_FEAR_VICTIM1";
		case zdcore::STUN_FEAR2: return "T_STAND_2_FEAR_VICTIM2";
		case zdcore::STUN_FEAR3: return "T_STAND_2_FEAR_VICTIM3";
		case zdcore::STUN_WOUNDED: return "T_STAND_2_WOUNDED";
		case zdcore::STUN_WOUNDEDB: return "T_STAND_2_WOUNDEDB";
		case zdcore::STUN_MONSTER: return "T_STUMBLEB";
		case zdcore::STUN_BURN: return "S_FIRE_VICTIM";
		}
		return "";
	}

	static const char* SgFearLoopAni(int v) {
		return v == zdcore::STUN_FEAR1 ? "S_FEAR_VICTIM1" : v == zdcore::STUN_FEAR2 ? "S_FEAR_VICTIM2" :
			v == zdcore::STUN_FEAR3 ? "S_FEAR_VICTIM3" : nullptr;
	}

	// S_FEAR_VICTIMx w HUMANS.MSB nie jest petla (next = T_FEAR_VICTIM_2_STAND po 3,5-5,8 s): na czas oszolomienia
	// aktywna animacja dostaje next = ona sama (zCModelAniActive::nextAni, +0x04), na koniec wraca wstawanie.
	static void SgFearLoop(SgHold& h, bool loop) {
		const char* name = SgFearLoopAni(h.variant);
		zCModel* m = h.npc->GetModel();
		if (!name || !m)
			return;
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (!a || !a->protoAni || _stricmp(a->protoAni->aniName.ToChar(), name) != 0)
				continue;
			if (loop)
				a->nextAni = a->protoAni;
			else {
				int id = m->GetAniIDFromAniName(zSTRING("T_FEAR_VICTIM_2_STAND"));
				a->nextAni = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			}
		}
	}

	// Start stanu skryptem ZDF_Sign_StartState (self = cel, other = bohater; self/other przywracane).
	static bool SgStartState(oCNpc* npc, int variant, float seconds) {
		if (sgStateFunc == -2)
			sgStateFunc = parser ? parser->GetIndex(zSTRING("ZDF_SIGN_STARTSTATE")) : -1;
		if (sgStateFunc < 0) {
			static bool warned = false;
			if (!warned)
				ZLog("Znaki: brak ZDF_Sign_StartState w skryptach (stary zDismember_Signs.d) - oszolomienie tylko potknieciem");
			warned = true;
			return false;
		}
		SgParserSetInt("ZDF_SIGN_STATE", variant);
		// limit petli skryptu jest wspolny dla wszystkich NPC (zmienna globalna) - najdluzszy z trwajacych stanow tego
		// rodzaju, zeby krotszy nowy nie konczyl dluzszego wczesniej (koniec i tak ustawia plugin czasem stanu);
		// trans ma wlasny bezpiecznik w skrypcie (ZDF_SIGN_TRANCE_LIMIT_SEC) - bez zmiennej
		if (variant != zdcore::STUN_TRANCE) {
			bool burn = variant == zdcore::STUN_BURN;
			float longest = seconds;
			for (auto& h : sgHolds)
				if ((h.variant == zdcore::STUN_BURN) == burn && h.variant != zdcore::STUN_TRANCE && !h.ended && h.seconds > longest)
					longest = h.seconds;
			SgParserSetInt(burn ? "ZDF_SIGN_BURNSEC" : "ZDF_SIGN_STUNSEC", zdcore::StunScriptSeconds(longest));
		}
		return SgCallScript("ZDF_SIGN_STARTSTATE", npc, player);
	}

	static void SgHoldDrop(size_t i, const char* why) {
		SgHold& h = sgHolds[i];
		SgFxStop(h.fx);
		if (h.npc->GetModel())
			SgFearLoop(h, false);
		ZDebug("Znaki: %s %s (%s) - koniec (%s, %.1f s)", h.variant == zdcore::STUN_BURN ? "palenie" : "oszolomienie",
			SgNpcName(h.npc), zdcore::StunName(h.variant), why, h.age);
		h.npc->Release();
		sgHolds.erase(sgHolds.begin() + i);
	}

	// Oszolomienie / reakcja palenia stanem AI. false = stanu nie da sie zaczac (wywolujacy daje potkniecie).
	static bool SgHoldStart(oCNpc* npc, int variant, float seconds) {
		if (SgHold* old = SgFindHold(npc)) {
			if (old->variant == variant || (variant != zdcore::STUN_BURN && old->variant != zdcore::STUN_BURN)) {
				// ponowny znak tego rodzaju: dluzej w tym samym stanie (bez nowego wejscia)
				old->left = seconds > old->left ? seconds : old->left;
				old->seconds = old->seconds > seconds ? old->seconds : seconds;
				old->ended = false;
				if (old->active)
					npc->state.curState.stateTime = 0.0f;
				return true;
			}
			SgHoldDrop(old - sgHolds.data(), "inny znak");
		}
		if (!SgCanTakeState(npc, variant == zdcore::STUN_TRANCE) || !SgStartState(npc, variant, seconds))
			return false;
		SgHold h = { npc, variant, seconds, 0.0f, seconds, false, false, false, 0.0f, nullptr };
		if (variant != zdcore::STUN_BURN)
			h.fx = SgFx("SPELLFX_AXII_TARGET", npc, npc, true);
		npc->AddRef();
		sgHolds.push_back(h);
		return true;
	}

	static void SgHoldsUpdate(float dt) {
		for (size_t i = 0; i < sgHolds.size();) {
			SgHold& h = sgHolds[i];
			h.age += dt;
			if (!SgAlive(h.npc)) {
				SgHoldDrop(i, "martwy / powalony / poza swiatem");
				continue;
			}
			bool inState = SgInSignState(h.npc);
			if (!inState) {
				if (!h.active && h.age < 1.0f) {   // AI_StartState idzie przez kolejke EM - czekamy na start
					i++;
					continue;
				}
				SgHoldDrop(i, !h.active ? "stan nie wystartowal" : h.lastStateMs >= SG_HIT_STATE_MS ? "trafienie" :
					h.ended ? "czas" : "inny stan AI");
				continue;
			}
			if (!h.active)
				ZDebug("Znaki: %s w stanie %s", SgNpcName(h.npc), h.npc->state.curState.name.ToChar());
			h.active = true;
			float& st = h.npc->state.curState.stateTime;
			if (h.finisher && DirectorIsVictim(h.npc))
				st = 0.0f;   // egzekucja - stan trwa do smierci z finishera
			else if (!h.ended) {
				SgFearLoop(h, true);
				h.left -= dt;
				if (h.left <= 0.0f) {
					// trans: za bezpiecznikiem skryptu (600 s) - wspolny limit oszolomienia (ZDF_Sign_StunSec) zmienia kazdy rzut
					float end = h.variant == zdcore::STUN_TRANCE ? zdcore::TranceStateEndMs() : zdcore::StunStateEndMs(h.seconds);
					if (h.variant != zdcore::STUN_TRANCE) {
						// wspolny limit petli (najdluzszy trwajacy stan) moze byc dluzszy niz ten stan - czas za limitem
						int lim = SgParserInt(h.variant == zdcore::STUN_BURN ? "ZDF_SIGN_BURNSEC" : "ZDF_SIGN_STUNSEC", 0);
						float limEnd = (float)lim * 1000.0f + 1.0f;
						if (limEnd > end)
							end = limEnd;
					}
					if (st < end)
						st = end;   // petla skryptu konczy stan w najblizszej klatce (z animacja wstawania)
					h.ended = true;
					SgFearLoop(h, false);
				}
			}
			h.lastStateMs = st;
			i++;
		}
	}

	// ---------- Yrden (strefa i pulapka) ----------

	static zdcore::NpcCombatStats SgReadStats(oCNpc* npc, zCModel* m) {
		zdcore::NpcCombatStats s;
		s.timeScale = m ? m->timeScale : 1.0f;
		for (int i = 0; i < zdcore::SIGN_PROT_COUNT; i++)
			s.prot[i] = npc->protection[i];
		return s;
	}

	static void SgWriteStats(oCNpc* npc, zCModel* m, const zdcore::NpcCombatStats& s) {
		if (m)
			m->timeScale = s.timeScale;
		for (int i = 0; i < zdcore::SIGN_PROT_COUNT; i++)
			npc->protection[i] = s.prot[i];
	}

	// ---------- powrot tempa odlozony (model zamrozony z zewnatrz) ----------
	// zTimeRewind na czas cofania wpisuje marionetkom (bohater tez) timeScale 0, a po cofnieciu oddaje tempo ze startu.
	// Powrot Yrdenu / podtrzymania znaku w tym czasie liczylby sie od 0 (0 x oryginal / nasze = 0), a zTimeRewind
	// oddalby potem nasze spowolnione tempo na stale. Ochrony wracaja od razu, tempo - gdy model znow ruszy.
	struct SgTempoPending {
		zCModel* model;
		zdcore::YrdenMod mod;
		std::string who;
		float age;
	};
	static std::vector<SgTempoPending> sgTempoPending;

	static void SgRestoreStats(oCNpc* npc, zCModel* m, const zdcore::YrdenMod& mod, const char* who) {
		zdcore::NpcCombatStats cur = SgReadStats(npc, m);
		zdcore::NpcCombatStats back = zdcore::YrdenRestore(mod, cur);
		if (m && zdcore::YrdenTempoDeferred(mod, cur.timeScale)) {
			back.timeScale = cur.timeScale;   // tempo bez zmian - po odmrozeniu
			m->AddRef();
			sgTempoPending.push_back({ m, mod, who, 0.0f });
			ZLog("Znaki: %s - model zamrozony (cofanie czasu), powrot tempa (nasze x%.2f -> x%.2f) po odmrozeniu", who,
				mod.applied.timeScale, mod.orig.timeScale);
		}
		SgWriteStats(npc, m, back);
	}

	static void SgTempoPendingUpdate(float dt) {
		for (size_t i = 0; i < sgTempoPending.size();) {
			SgTempoPending& p = sgTempoPending[i];
			p.age += dt;
			float cur = p.model->timeScale;
			if (cur < 0.001f && p.age < 120.0f) {
				i++;
				continue;
			}
			if (cur >= 0.001f) {
				float to = zdcore::YrdenRestoreTempo(p.mod, cur);
				p.model->timeScale = to;
				ZLog("Znaki: %s - model odmrozony, tempo x%.2f -> x%.2f (%s)", p.who.c_str(), cur, to,
					cur == p.mod.applied.timeScale ? "nasze - oryginal" : "zmienione z zewnatrz - cofniete tylko nasze");
			}
			else
				ZLog("Znaki: %s - model nadal zamrozony po 120 s, powrot tempa porzucony", p.who.c_str());
			p.model->Release();
			sgTempoPending.erase(sgTempoPending.begin() + i);
		}
	}

	static void SgSlowApply(SgSlowed& s) {
		s.mod = zdcore::YrdenApply(SgReadStats(s.npc, s.model), s.slow, s.armor, zdcore::YrdenArmorMask());
		SgWriteStats(s.npc, s.model, s.mod.applied);
		s.applied = true;
		if (!s.fx)
			s.fx = SgFx("SPELLFX_YRDEN_SLOW", s.npc, s.npc, true);
	}

	// Dokladny powrot (Core: zmiany obce w trakcie strefy zostaja). Model zmieniony w miedzyczasie (przemiana) - tempo na
	// starym modelu i tak wracamy, bo trzymamy do niego ref.
	static void SgSlowRestore(SgSlowed& s) {
		SgFxStop(s.fx);
		if (!s.applied)
			return;
		SgRestoreStats(s.npc, s.model, s.mod, "Yrden");
		s.applied = false;
	}

	static void SgSlowRelease(SgSlowed& s) {
		SgSlowRestore(s);
		if (s.model)
			s.model->Release();
		s.npc->Release();
	}

	static void SgSlowReleaseAll() {
		for (auto& s : sgSlowed)
			SgSlowRelease(s);
		sgSlowed.clear();
	}

	// Koniec strefy zwyklej: spowolnieni wychodza w najblizszym skanie (albo od razu, gdy nie ma juz zadnej strefy).
	static void SgZoneEnd(const char* why) {
		if (sgZone.active || sgZoneFx)
			ZDebug("Yrden: strefa koniec (%s)", why);
		SgFxStop(sgZoneFx);
		sgZone.Stop();
		if (!sgTrap.active)
			SgSlowReleaseAll();
	}

	// Kule niezuzytych ladunkow (petle w swoich srodkach) - przy rzucie i po zapisie gry.
	static void SgTrapOrbsSpawn() {
		for (auto& o : sgTrap.orbs)
			if (!o.used && !o.fx)
				o.fx = SgFxAt("SPELLFX_YRDEN_TRAP_ORB", o.pos, true);
	}

	static void SgTrapOrbsStop() {
		for (auto& o : sgTrap.orbs)
			SgFxStop(o.fx);
	}

	static void SgTrapEnd(const char* why) {
		if (sgTrap.active || sgTrap.fx)
			ZDebug("Yrden: pulapka koniec (%s), ladunki %d", why, sgTrap.st.charges);
		SgFxStop(sgTrap.fx);
		SgTrapOrbsStop();
		sgTrap.orbs.clear();
		sgTrap.active = false;
		sgTrap.zone.Stop();
		if (!sgZone.active)
			SgSlowReleaseAll();
	}

	// Pocisk pulapki: kula najblizsza wroga gasnie, z jej srodka leci SPELLFX_YRDEN_BOLT (bez kul: ze srodka glifu);
	// obrazenia magiczne (OnDamage jak inne znaki) po czasie dolotu (odleglosc / 26 m/s), ladunek zuzyty od razu.
	static void SgTrapFire(oCNpc* npc) {
		zVEC3 q = npc->GetPositionWorld();
		zVEC3 from(sgTrap.ground.n[0], sgTrap.ground.n[1] + SG_ORB_HEIGHT, sgTrap.ground.n[2]);
		SgTrapOrb* orb = nullptr;
		for (auto& o : sgTrap.orbs)
			if (!o.used && (!orb || (o.pos - q).Length() < (orb->pos - q).Length()))
				orb = &o;
		if (orb) {
			from = orb->pos;
			orb->used = true;
			SgFxStop(orb->fx);
		}
		SgFxAt("SPELLFX_YRDEN_BOLT", from, false, npc);
		float dist = (q - from).Length();
		npc->AddRef();
		sgBolts.push_back({ npc, dist / SG_BOLT_SPEED, sgTrap.damage });
		sgTrap.st.Fire((int)(uintptr_t)npc, sgTrap.clock);
		ZDebug("Yrden pulapka: pocisk w %s (%.0f cm, dolot %.2f s), ladunki %d", SgNpcName(npc), dist, dist / SG_BOLT_SPEED,
			sgTrap.st.charges);
	}

	static void SgBoltsUpdate(float dt) {
		for (size_t i = 0; i < sgBolts.size();) {
			SgBolt& b = sgBolts[i];
			b.delay -= dt;
			if (b.delay > 0.0f) {
				i++;
				continue;
			}
			oCNpc* npc = b.npc;
			float damage = b.damage;
			sgBolts.erase(sgBolts.begin() + i);
			if (SgAlive(npc)) {
				int hp0 = npc->attribute[NPC_ATR_HITPOINTS];
				SgDamage(npc, damage, oEDamageType_Magic);
				ZDebug("Yrden pulapka: trafienie %s, HP %d -> %d", SgNpcName(npc), hp0, npc->attribute[NPC_ATR_HITPOINTS]);
			}
			npc->Release();
		}
	}

	static void SgBoltsClear() {
		for (auto& b : sgBolts)
			b.npc->Release();
		sgBolts.clear();
	}

	static void SgYrdenUpdate(float dt) {
		if (!sgZone.active && !sgTrap.active) {
			if (!sgSlowed.empty())
				SgSlowReleaseAll();
			return;
		}
		if (sgZone.active && sgZone.Update(dt))
			SgZoneEnd("czas");
		if (sgTrap.active) {
			sgTrap.clock += dt;
			if (sgTrap.zone.Update(dt))
				SgTrapEnd("czas");
			else {
				SgFxKeepAt(sgTrap.fx, "SPELLFX_YRDEN_TRAP", sgTrap.ground);
				for (auto& o : sgTrap.orbs)
					if (!o.used)
						SgFxKeepAt(o.fx, "SPELLFX_YRDEN_TRAP_ORB", o.pos);
			}
		}
		sgYrdenScan -= dt;
		if (sgYrdenScan > 0.0f)
			return;
		sgYrdenScan = 0.1f;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return;
		std::vector<int> inside, applied;
		std::vector<oCNpc*> inTrap;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 q = npc->GetPositionWorld();
			bool inZone = sgZone.Contains(q.n[0], q.n[1], q.n[2]);
			bool inGlyph = sgTrap.active && sgTrap.zone.Contains(q.n[0], q.n[1], q.n[2]);
			if (!inZone && !inGlyph)
				continue;
			zdcore::SignTargetInfo info = SgInfo(npc);
			if (!(zdcore::SignIsEnemy(info) && zdcore::SignAffects(info, false)))   // tylko wrogowie (bez SignHitNeutral)
				continue;
			inside.push_back((int)(uintptr_t)npc);
			if (inGlyph)
				inTrap.push_back(npc);
		}
		for (auto& s : sgSlowed)
			applied.push_back((int)(uintptr_t)s.npc);
		zdcore::YrdenPlan plan = zdcore::YrdenStep(applied, inside);
		for (int id : plan.leave)
			for (auto it = sgSlowed.begin(); it != sgSlowed.end(); ++it)
				if ((int)(uintptr_t)it->npc == id) {
					ZDebug("Yrden: %s wychodzi - tempo %.2f, ochrona przywrocona", SgNpcName(it->npc), it->mod.orig.timeScale);
					SgSlowRelease(*it);
					sgSlowed.erase(it);
					break;
				}
		for (int id : plan.enter) {
			oCNpc* npc = (oCNpc*)(uintptr_t)(unsigned)id;
			bool glyphOnly = std::find(inTrap.begin(), inTrap.end(), npc) != inTrap.end() && !sgZone.active;
			SgSlowed s = { npc, npc->GetModel(), {}, false, nullptr, glyphOnly ? sgTrap.slow : sgZoneSlow,
				glyphOnly ? sgTrap.armor : sgZoneArmor };
			if (sgZone.active) {
				zVEC3 q = npc->GetPositionWorld();
				if (!sgZone.Contains(q.n[0], q.n[1], q.n[2])) {   // tylko w glifie pulapki
					s.slow = sgTrap.slow;
					s.armor = sgTrap.armor;
				}
			}
			npc->AddRef();
			s.model->AddRef();
			SgSlowApply(s);
			ZDebug("Yrden: %s w strefie - tempo %.2f -> %.2f, ostrze %d -> %d, ogien %d -> %d", SgNpcName(npc),
				s.mod.orig.timeScale, s.mod.applied.timeScale, s.mod.orig.prot[2], s.mod.applied.prot[2],
				s.mod.orig.prot[3], s.mod.applied.prot[3]);
			sgSlowed.push_back(s);
		}
		// pulapka: pocisk w kazdego wroga w glifie (co najmniej YrdenTrapInterval s miedzy pociskami w tego samego)
		if (sgTrap.active)
			for (oCNpc* npc : inTrap) {
				if (!sgTrap.st.CanFire((int)(uintptr_t)npc, sgTrap.clock) || !SgAlive(npc))
					continue;
				SgTrapFire(npc);
				if (sgTrap.st.Spent()) {
					SgTrapEnd("ladunki zuzyte");
					break;
				}
			}
	}

	// ---------- Quen ----------

	// Efekt tarczy co klatke (QuenFxGuard): tarcza nieaktywna, a efekt jest - Stop + Release (z dziecmi); aktywna -
	// efekt istnieje, nie skonczyl sie, ma bohatera za zrodlo i stoi przy nim (inaczej odtworzenie).
	static void SgQuenFxGuard() {
		bool exists = sgQuenFx != nullptr;
		bool finished = exists && (sgQuenFx->IsFinished() || sgQuenFx->origin != player);
		float dist = exists && player ? SgFlatDist(sgQuenFx->GetPositionWorld(), player->GetPositionWorld()) : 0.0f;
		zdcore::QuenFxAction a = zdcore::QuenFxGuard(sgQuen.active, exists, finished, dist, SG_QUEN_FX_MAX_DIST);
		if (a == zdcore::QFX_NONE)
			return;
		if (a == zdcore::QFX_STOP || a == zdcore::QFX_RESPAWN) {
			ZDebug("Quen: efekt tarczy %s (skonczony %d, zrodlo bohater %d, odleglosc %.0f cm)",
				a == zdcore::QFX_STOP ? "bez tarczy - zatrzymany" : "oderwany - odtworzony", finished,
				sgQuenFx->origin == player, dist);
			SgFxStop(sgQuenFx);
		}
		if ((a == zdcore::QFX_CREATE || a == zdcore::QFX_RESPAWN) && player && player->homeWorld)
			sgQuenFx = SgFx("SPELLFX_QUEN_SHIELD", player, player, true);
	}

	// Pekniecie tarczy: wrogowie do QuenRadius dostaja obrazenia (magia), potkniecie i odsuniecie.
	static void SgQuenBreakAoE(const char* label, float damage, float push) {
		SgFx("SPELLFX_QUEN_BREAK", player, player);
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return;
		zVEC3 hp = player->GetPositionWorld();
		std::vector<oCNpc*> hit;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 q = npc->GetPositionWorld();
			float dx = q.n[0] - hp.n[0], dz = q.n[2] - hp.n[2];
			if (dx * dx + dz * dz > zcfg.quenRadius * zcfg.quenRadius || fabsf(q.n[1] - hp.n[1]) > SG_MAX_DY)
				continue;
			if (zdcore::SignAffects(SgInfo(npc), zcfg.signHitNeutral))
				hit.push_back(npc);
		}
		for (oCNpc* npc : hit) {
			int hp0 = npc->attribute[NPC_ATR_HITPOINTS];
			SgDamage(npc, damage, oEDamageType_Magic);
			const char* r = FoeReady(npc) && !DirectorIsVictim(npc) ? SgStaggerPush(npc, push) : "zabity / powalony";
			ZDebug("%s: %s HP %d -> %d, %s", label, SgNpcName(npc), hp0, npc->attribute[NPC_ATR_HITPOINTS], r);
		}
		ZDebug("%s: odrzuceni %d (obrazenia %.0f)", label, (int)hit.size(), damage);
	}

	static void SgQuenUpdate(float dt) {
		if (sgQuen.Update(dt))
			ZDebug("Quen: tarcza wygasla");
		bool broke = sgQuen.TakeBreak();
		SgQuenFxGuard();   // takze zatrzymanie po wygasnieciu / peknieciu
		if (broke)
			SgQuenBreakAoE("Quen pekniecie", zcfg.quenDamage * sgQuenDmg.mul, zcfg.quenPush * sgQuenPower);
	}

	// Obrazenia pochloniete (do leczenia aktywnej tarczy): deskryptor (upadek, czary, pociski) albo szacunek z napastnika -
	// cios wrecz ma fDamageTotal liczone dopiero w OnDamage_Hit (w logu rundy 3 "ok. 0 obrazen" od wargow). Bron w rece:
	// obrazenia broni + sila, potwor (bez broni): tablica obrazen + sila (w G2 potwory bija sila); minus ochrona bohatera
	// przed przewazajacym typem, co najmniej 5 (NPC_MINIMAL_DAMAGE).
	static float SgEstimateDamage(const oCNpc::oSDamageDescriptor& desc) {
		if (desc.fDamageTotal > 0.0f)
			return desc.fDamageTotal;
		oCNpc* a = desc.pNpcAttacker;
		if (!a || !player)
			return 0.0f;
		int wm = a->GetWeaponMode();
		oCItem* w = wm >= NPC_WEAPON_DAG && wm <= NPC_WEAPON_2HS ? a->GetWeapon() : nullptr;
		int base = 0, idx;
		if (w) {
			base = w->damageTotal;
			idx = zdcore::DamageMainIndex(w->damage, (unsigned)w->damageTypes);
		}
		else {
			for (int i = 0; i < 8; i++)
				base += a->damage[i] > 0 ? a->damage[i] : 0;
			idx = zdcore::DamageMainIndex(a->damage, (unsigned)a->damagetype);
		}
		return (float)zdcore::EstimateMeleeDamage(base, a->attribute[NPC_ATR_STRENGTH], player->protection[idx], 5);
	}

	// ---------- tryby podtrzymywane (Igni strumien, Quen aktywna tarcza) ----------

	// Tempo bohatera w podtrzymaniu (SignChannelMoveScale): tempo modelu - ruch (animacje z przesunieciem korzenia) i
	// animacje wolniejsze; maska ochron 0 - ochrony bez zmian; powrot dokladny jak przy Yrdenie.
	static void SgSustainHeroSlow(bool on) {
		if (on && !sgSustain.slowed && player && player->GetModel() && zcfg.signChannelMoveScale < 0.999f) {
			sgSustain.hero = player;
			sgSustain.hero->AddRef();
			sgSustain.heroModel = player->GetModel();
			sgSustain.heroModel->AddRef();
			sgSustain.heroMod = zdcore::YrdenApply(SgReadStats(player, sgSustain.heroModel), zcfg.signChannelMoveScale, 0.0f, 0u);
			SgWriteStats(player, sgSustain.heroModel, sgSustain.heroMod.applied);
			sgSustain.slowed = true;
		}
		else if (!on && sgSustain.slowed) {
			oCNpc* h = sgSustain.hero;
			SgRestoreStats(h, sgSustain.heroModel, sgSustain.heroMod, "podtrzymanie znaku (tempo bohatera)");
			sgSustain.heroModel->Release();
			sgSustain.heroModel = nullptr;
			h->Release();
			sgSustain.hero = nullptr;
			sgSustain.slowed = false;
		}
	}

	// Poza podtrzymania (runda 4; w rundzie 3 cale cialo T_MAGRUN_2_FBTSHOOT / HEASHOOT trzymalo bohatera w miejscu, a
	// HEASHOOT to obie rece w gore): gest znaku W3 z nakladki HUMANS_ZDF_SIGNS (zQuickSign, make_signs.py) w wariancie
	// ruchu - tylko tulow i rece (kosci od BIP01 SPINE), warstwa 21 (nie gasi jej ruch ani zakrety, tylko przerwania
	// postaci), nogi z normalnego ruchu. Gest staje na klatce SignChannelFrame (wyciagnieta dlon; pchniecie dloni na
	// kl. 8 z 17) do konca podtrzymania, potem dogrywa sie i wygasza (blend 0,3 s). Igni: R_CHAIR_RANDOM_3 (z mieczem
	// _4), Quen: T_BAUMSAEGE_RANDOM_1 (z mieczem T_REPAIR_RANDOM_1) - gesty W3 Igni / Quen. Nakladke zaklada zQuickSign
	// na postoju; bez niej podtrzymanie bez pozy (log).
	static const int SG_SIGN_LAYER = 21;

	static const char* SgChannelAniName(int sign, bool sword) {
		if (sign == zdcore::SIGN_IGNI)
			return sword ? "R_CHAIR_RANDOM_4" : "R_CHAIR_RANDOM_3";
		return sword ? "T_REPAIR_RANDOM_1" : "T_BAUMSAEGE_RANDOM_1";
	}

	// Koniec podtrzymania (23.10): klip W3 man_geralt_ignii_stop / quen_stop (z mieczem *_stop_rp_sword) - wyjscie z pozy
	// trzymania, tulow i rece (nakladka HUMANS_ZDF_SIGNS, make_signs.py SIGN_ANIS_STOP - te same nazwy), zamiast dogrywania
	// gestu od klatki trzymania. Startowany w SignsUpdate (SgChannelStopUpdate - poza hakami Invest / SetReleaseStatus),
	// tylko gdy poza trzymania nadal gra; StartAni na tej samej warstwie wygasza poze. Bez klipu w nakladce (stara paczka
	// zDismember_W3.vdf) - jak dawniej.
	static const char* SgChannelStopName(int sign, bool sword) {
		if (sign == zdcore::SIGN_IGNI)
			return sword ? "T_LGUARD_CHANGELEG" : "T_LGUARD_STRETCH";
		return sword ? "T_DANCE_09" : "T_LGUARD_SCRATCH";
	}

	struct SgStopReq {
		zCModel* model = nullptr;   // AddRef (model z poza trzymania)
		int sign = -1;
		int poseId = -1;
		bool sword = false;
	};
	static SgStopReq sgStopReq;

	static void SgStopReqClear() {
		if (sgStopReq.model)
			sgStopReq.model->Release();
		sgStopReq = SgStopReq();
	}

	static zCModelAniActive* SgChannelActive(zCModel* m, int id) {
		for (int i = 0; i < m->numActiveAnis && i < zMDL_MAX_ANIS_PARALLEL; i++) {
			zCModelAniActive* a = m->aniChannels[i];
			if (a && a->protoAni && a->protoAni->aniID == id && a->protoAni->layer == SG_SIGN_LAYER && !a->isFadingOut)
				return a;
		}
		return nullptr;
	}

	static void SgChannelPoseUpdate(float dt) {
		SgSustain& s = sgSustain;
		zCModel* m = player ? player->GetModel() : nullptr;
		if (!m)
			return;
		if (s.poseId < 0) {
			if (s.poseTried)
				return;
			s.poseTried = true;
			int fm = player->fmode;   // poza hakiem Invest - prawdziwy tryb broni (zQuickSign podstawia 7 tylko na czas wywolan)
			s.poseSword = fm >= NPC_WEAPON_DAG && fm <= NPC_WEAPON_2HS;
			const char* name = SgChannelAniName(s.sign, s.poseSword);
			int id = m->GetAniIDFromAniName(zSTRING(name));
			zCModelAni* ani = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			if (!ani || ani->layer != SG_SIGN_LAYER) {
				ZDebug("Znak %s: poza podtrzymania - gest %s nie z nakladki HUMANS_ZDF_SIGNS (warstwa %d zamiast %d) - bez pozy",
					zdcore::SignName(s.sign), name, ani ? ani->layer : -1, SG_SIGN_LAYER);
				return;
			}
			s.poseId = id;
			s.poseModel = m;
			m->AddRef();
			s.poseHold = zdcore::ChannelHoldFrame(ani->numFrames, zcfg.signChannelFrame);
			s.poseWait = 0.0f;
			m->StartAni(id, 0);
			ZDebug("Znak %s: poza podtrzymania %s (warstwa %d, tryb broni %d), klatka %.0f z %d", zdcore::SignName(s.sign), name,
				SG_SIGN_LAYER, fm, s.poseHold, (int)ani->numFrames);
			return;
		}
		if (m != s.poseModel)
			return;   // model zmieniony (przemiana) - bez pozy
		zCModelAniActive* a = SgChannelActive(m, s.poseId);
		zdcore::ChannelPoseAction act = zdcore::ChannelPoseStep(a != nullptr, a ? a->actFrame : 0.0f, s.poseHold);
		if (act == zdcore::CPOSE_FREEZE)
			a->SetActFrame(s.poseHold);
		else if (act == zdcore::CPOSE_RESTART) {
			s.poseWait += dt;
			if (s.poseWait < 0.25f)
				return;   // przerwanie (trafienie, potkniecie) - chwila na jego animacje
			s.poseWait = 0.0f;
			m->StartAni(s.poseId, 0);
			if (zCModelAniActive* b = SgChannelActive(m, s.poseId))
				b->SetActFrame(s.poseHold);
		}
		else
			s.poseWait = 0.0f;
	}

	// Koniec pozy: gest nie jest juz zatrzymywany. stopClip (zwykly koniec - puszczenie, mana, przerwanie czaru): klip W3
	// konca w najblizszym SignsUpdate (SgChannelStopUpdate), bez niego (zapis, czyszczenie) gest dogrywa sie od klatki
	// trzymania do konca i wygasza.
	static void SgChannelPoseEnd(bool stopClip) {
		if (stopClip && sgSustain.poseModel && sgSustain.poseId >= 0) {
			SgStopReqClear();
			sgStopReq.model = sgSustain.poseModel;   // ref przechodzi na zadanie
			sgStopReq.sign = sgSustain.sign;
			sgStopReq.poseId = sgSustain.poseId;
			sgStopReq.sword = sgSustain.poseSword;
		}
		else if (sgSustain.poseModel)
			sgSustain.poseModel->Release();
		sgSustain.poseModel = nullptr;
		sgSustain.poseId = -1;
	}

	static void SgChannelStopUpdate() {
		if (!sgStopReq.model)
			return;
		zCModel* m = sgStopReq.model;
		const char* name = SgChannelStopName(sgStopReq.sign, sgStopReq.sword);
		const char* why = nullptr;
		if (!player || player->GetModel() != m)
			why = "inny model bohatera";
		else if (!SgChannelActive(m, sgStopReq.poseId))
			why = "poza trzymania juz nie gra (przerwanie)";
		else {
			int id = m->GetAniIDFromAniName(zSTRING(name));
			zCModelAni* ani = id >= 0 ? m->GetAniFromAniID(id) : nullptr;
			if (!ani || ani->layer != SG_SIGN_LAYER)
				why = "klipu nie ma w nakladce HUMANS_ZDF_SIGNS (stara paczka) - gest dogrywa sie jak dawniej";
			else {
				m->StartAni(id, 0);
				ZDebug("Znak %s: koniec podtrzymania - klip W3 %s (%s)", zdcore::SignName(sgStopReq.sign), name,
					sgStopReq.sword ? "z mieczem" : "bez broni");
			}
		}
		if (why)
			ZDebug("Znak %s: koniec podtrzymania bez klipu %s - %s", zdcore::SignName(sgStopReq.sign), name, why);
		SgStopReqClear();
	}

	// effects = false: zapis / czyszczenie - bez odrzutu Quen (obrazenia i odsuniecia w trakcie zapisu)
	static void SgSustainEnd(const char* why, bool effects = true) {
		if (!sgSustain.active)
			return;
		int sign = sgSustain.sign;
		SgFxStop(sgSustain.fx);
		SgSustainHeroSlow(false);
		SgChannelPoseEnd(effects);   // zwykly koniec - klip W3 konca (Igni / Quen stop)
		if (sign == zdcore::SIGN_QUEN) {
			if (effects && sgSustain.absorbed > 0)
				SgQuenBreakAoE("Quen aktywna - odrzut", zcfg.quenDamage * sgSustain.dmg.mul, zcfg.quenPush * sgSustain.power);
		}
		ZDebug("Znak %s (tryb alternatywny): koniec (%s) po %.1f s%s", zdcore::SignName(sign), why, sgSustain.age,
			sign == zdcore::SIGN_QUEN ? (sgSustain.absorbed > 0 ? ", pochloniete trafienia - odrzut" : ", bez trafien") : "");
		sgSustain.active = false;
		sgSustain.sign = -1;
		sgCooldown.Start(sign, zcfg.signCooldown);
		sgCast.hold.RequestStop();   // nastepny Invest zwroci SENDSTOP
	}

	static bool SgSustainStart(int sign, bool console) {
		if (sgSustain.active)
			SgSustainEnd("nowy");
		if (!player || !player->homeWorld)
			return false;
		sgSustain = SgSustain();
		sgSustain.sign = sign;
		sgSustain.active = true;
		sgSustain.console = console;
		sgSustain.consoleLeft = SG_CONSOLE_SUSTAIN;
		sgSustain.power = SgPowerNow();
		sgSustain.dmg = SgDamagePowerNow(sgSustain.power);
		if (sign == zdcore::SIGN_IGNI)
			sgSustain.fx = SgFx("SPELLFX_IGNI_STREAM", player, nullptr, true);
		else
			sgSustain.fx = SgFx("SPELLFX_QUEN_ACTIVE", player, player, true);
		SgSustainHeroSlow(true);   // poza (gest W3 na gornej polowie ciala) startuje w SgSustainUpdate - poza hakiem Invest
		ZDebug("Znak %s: tryb alternatywny %s (%s), moc x%.2f (mana max %d), %s, mana %d, podtrzymanie %.0f many/s, tempo bohatera x%.2f",
			zdcore::SignName(sign), sign == zdcore::SIGN_IGNI ? "strumien ognia" : "aktywna tarcza",
			console ? "konsola" : "przytrzymanie", sgSustain.power, player->attribute[NPC_ATR_MANAMAX],
			SgDamagePowerText(sgSustain.dmg).c_str(), player->attribute[NPC_ATR_MANA],
			sign == zdcore::SIGN_IGNI ? zcfg.igniStreamManaPerSec : zcfg.quenActiveManaPerSec, zcfg.signChannelMoveScale);
		return true;
	}

	// Palenie silnika po obrazeniach od ognia: komunikat EV_DAMAGE_PER_FRAME (opis w Core/Signs.h, BurnOverride).
	// burnDps = obrazenia bazowe na sekunde (IgniBurnDps) - mnoznik mocy obrazen D tutaj, jak przy trafieniu.
	static zdcore::SignDamagePower sgBurnDmg;   // moc obrazen ostatniego podpalenia (log taktow)
	static void SgFixBurn(oCNpc* npc, const std::vector<zCEventMessage*>& before, float burnTime, float burnDps,
		const zdcore::SignDamagePower& D, bool quiet) {
		zCEventManager* em = npc->GetEM(FALSE);
		if (!em)
			return;
		for (int i = 0; i < em->GetNumMessages(); i++) {
			oCMsgDamage* msg = zDYNAMIC_CAST<oCMsgDamage>(em->GetEventMessage(i));
			if (!msg || msg->subType != oCMsgDamage::EV_DAMAGE_PER_FRAME)
				continue;
			oCNpc::oSDamageDescriptor& d = msg->descDamage;
			if (d.pNpcAttacker != player || !(d.enuModeDamage & oEDamageType_Fire))
				continue;
			bool isNew = std::find(before.begin(), before.end(), (zCEventMessage*)msg) == before.end();
			bool activated = (d.dwFieldsValid & SG_DESC_OVERLAY) != 0;
			int prot = npc->protection[oEDamageIndex_Fire];
			zdcore::BurnTiming b = zdcore::BurnOverride(d.fTimeDuration, isNew, activated, burnTime, burnDps * D.mul, prot);
			sgBurnDmg = D;
			float was = d.fTimeDuration;
			d.fTimeDuration = b.durationMs;
			if (b.setRate) {
				d.fTimeInterval = b.intervalMs;
				d.fDamagePerInterval = b.damagePerInterval;
				if (b.setTotal) {
					d.fDamageTotal = b.damagePerInterval;
					if (d.fTimeCurrent > b.intervalMs)
						d.fTimeCurrent = b.intervalMs;
				}
				// NB: tablica z ciosu Igni (cale obrazenia) zostalaby w kazdym takcie - takt = obrazenia taktu
				if (SgDamageArray()) {
					memset(d.aryDamage, 0, sizeof(d.aryDamage));
					zdcore::SignSplitDamage(d.enuModeDamage, b.damagePerInterval, d.aryDamage);
				}
			}
			if (!quiet)
				ZDebug("Igni: palenie %s %s - %.0f -> %.0f ms, %.0f na %.0f ms (ochrona ognia %d, skutecznie %d na takt), %s",
					SgNpcName(npc), isNew ? "nowe" : "trwa", was, d.fTimeDuration, d.fDamagePerInterval, d.fTimeInterval, prot,
					zdcore::BurnTickEffective(d.fDamagePerInterval, prot), SgDamagePowerText(D).c_str());
		}
	}

	static std::vector<zCEventMessage*> SgBurnMessages(oCNpc* npc) {
		std::vector<zCEventMessage*> out;
		if (zCEventManager* em = npc->GetEM(FALSE))
			for (int i = 0; i < em->GetNumMessages(); i++)
				if (oCMsgDamage* msg = zDYNAMIC_CAST<oCMsgDamage>(em->GetEventMessage(i)))
					out.push_back(msg);
		return out;
	}

	// Takt strumienia Igni: obrazenia ogniem w waskim stozku, podpalenie silnika (czas z moca), reakcja palenia ludzi.
	static void SgIgniStreamTick() {
		float P = sgSustain.power;
		const zdcore::SignDamagePower& D = sgSustain.dmg;
		auto targets = SgCone("Igni strumien", zcfg.igniStreamRange, zcfg.igniStreamCone, true, true);
		if (((int)(sgSustain.age * 4.0f)) % 4 == 0)   // ogniska co ok. 1 s (co czwarty takt)
			SgFires(true, zcfg.igniStreamRange, zcfg.igniStreamCone, "Igni strumien", true);
		for (auto& t : targets) {
			oCNpc* npc = t.npc;
			int hp0 = npc->attribute[NPC_ATR_HITPOINTS];
			std::vector<zCEventMessage*> before = SgBurnMessages(npc);
			SgDamage(npc, zcfg.igniStreamDps * zcfg.igniStreamTick * D.mul, oEDamageType_Fire);
			SgFixBurn(npc, before, zcfg.igniBurnTime * P, zcfg.igniBurnDps, D, true);
			if (FoeReady(npc) && !DirectorIsVictim(npc) && IsHumanGuild(npc) && SgModelHas(npc->GetModel(), "S_FIRE_VICTIM") &&
				zcfg.igniReactTime > 0.0f)
				SgHoldStart(npc, zdcore::STUN_BURN, zcfg.igniReactTime);
			ZDebug("Igni strumien: %s (%.0f cm) HP %d -> %d", SgNpcName(npc), t.hit.dist, hp0, npc->attribute[NPC_ATR_HITPOINTS]);
		}
	}

	static void SgSustainUpdate(float dt) {
		if (!sgSustain.active)
			return;
		if (!player || !player->homeWorld || player->IsDead()) {
			SgSustainEnd("bohater");
			return;
		}
		sgSustain.age += dt;
		sgSustain.sinceInvest += dt;
		if (sgSustain.console) {
			sgSustain.consoleLeft -= dt;
			if (sgSustain.consoleLeft <= 0.0f) {
				SgSustainEnd("konsola - czas");
				return;
			}
		}
		else if (sgSustain.sinceInvest > SG_SUSTAIN_TIMEOUT) {
			SgSustainEnd("czar przerwany / klawisz puszczony");
			return;
		}
		bool igni = sgSustain.sign == zdcore::SIGN_IGNI;
		SgFxRefresh(sgSustain.fx);
		SgFxKeep(sgSustain.fx, igni ? "SPELLFX_IGNI_STREAM" : "SPELLFX_QUEN_ACTIVE", player, igni ? nullptr : player);
		SgChannelPoseUpdate(dt);
		// strumien Igni z runa w rece przy celowaniu GFA: MagicMode (a z nim obrot GFA) pominiety - bohater obraca sie za
		// celownikiem sam (do 360 st./s), wiec strumien i jego stozek ida tam, gdzie celownik
		if (igni && sgSustain.magicPath && SgGfaAiming()) {
			SgAim aim;
			if (SgCameraAim(aim, "Gothic Free Aim")) {
				zVEC3 at = player->GetAtVectorWorld();
				float al = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
				if (al > 0.001f) {
					float fx = at.n[0] / al, fz = at.n[2] / al;
					float ang = atan2f(fx * aim.dirZ - fz * aim.dirX, fx * aim.dirX + fz * aim.dirZ);
					float step = 6.2831853f * dt;
					ang = ang > step ? step : ang < -step ? -step : ang;
					float c = cosf(ang), s = sinf(ang);
					zVEC3 hp = player->GetPositionWorld();
					player->SetHeadingYWorld(zVEC3(hp.n[0] + (fx * c - fz * s) * 100.0f, hp.n[1], hp.n[2] + (fx * s + fz * c) * 100.0f));
				}
			}
			SgAimRelease(aim);
		}
		if (!sgSustain.console) {
			float perSec = sgSustain.sign == zdcore::SIGN_IGNI ? zcfg.igniStreamManaPerSec : zcfg.quenActiveManaPerSec;
			int n = sgSustain.mana.Take(dt, perSec);
			if (n > 0) {
				int& mana = player->attribute[NPC_ATR_MANA];
				if (mana < n) {
					mana = 0;
					SgSustainEnd("brak many");
					return;
				}
				mana -= n;
			}
		}
		if (sgSustain.sign == zdcore::SIGN_IGNI) {
			sgSustain.tick -= dt;
			if (sgSustain.tick <= 0.0f) {
				sgSustain.tick += zcfg.igniStreamTick;
				if (sgSustain.tick < 0.0f)
					sgSustain.tick = 0.0f;
				SgIgniStreamTick();
			}
		}
	}

	// ---------- palenie: takty (runda 5) ----------
	// Blad rundy 4 ("palenie z Igni zadaje 0"): silnik przy kazdym takcie (oCNpc::EV_DamagePerFrame 0x6649A0 -> wirtualny
	// OnDamage, vtable +0xB8 @0x666097) odejmuje ochrone przed ogniem od obrazen taktu - przy 5-7 na takt i ochronie 35
	// (scierwojad, wilk) zostawalo 0, bandyta bez ochrony tracil zycie. Poprawka w BurnOverride (takt + ochrona). Hak tutaj:
	// takt od bohatera liczony w trybie magii (fmode 7 - jak SgDamage: z mieczem w rece OnDamage_Hit liczylby cios bronia)
	// i log kazdego taktu (Debug=1): obrazenia, ochrona, HP przed / po.
	static oCNpc* sgBurnFrame = nullptr;
	static bool sgBurnTickSeen = false;
	static float sgBurnTickDmg = 0.0f;

	static int __fastcall SgEvDamagePerFrame(oCNpc* self, void* edx, oCMsgDamage* msg);
	static CInvoke<int(__fastcall*)(oCNpc*, void*, oCMsgDamage*)> Ivk_oCNpc_EV_DamagePerFrame(
		ENGINE == Engine_G2A ? 0x006649A0 : 0, &SgEvDamagePerFrame, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SgEvDamagePerFrame(oCNpc* self, void* edx, oCMsgDamage* msg) {
		ZHookHit(ZH_EV_DAMAGEPERFRAME);
		if (!zcfg.signs || !player || !self || self == player || !msg || msg->descDamage.pNpcAttacker != player ||
			!(msg->descDamage.enuModeDamage & oEDamageType_Fire) || sgBurnFrame)
			return Ivk_oCNpc_EV_DamagePerFrame(self, edx, msg);
		int hp0 = self->attribute[NPC_ATR_HITPOINTS];
		int prot = self->protection[oEDamageIndex_Fire];
		float perTick = msg->descDamage.fDamagePerInterval;
		if (SgDamageArray() && perTick > 0.0f) {   // NB liczy takt z aryDamage (opis przy SgNewBalance)
			memset(msg->descDamage.aryDamage, 0, sizeof(msg->descDamage.aryDamage));
			zdcore::SignSplitDamage(msg->descDamage.enuModeDamage, perTick, msg->descDamage.aryDamage);
		}
		sgBurnFrame = self;
		sgBurnTickSeen = false;
		int r;
		{
			SgMagicMode mm;
			r = Ivk_oCNpc_EV_DamagePerFrame(self, edx, msg);
		}
		sgBurnFrame = nullptr;
		if (sgBurnTickSeen || self->attribute[NPC_ATR_HITPOINTS] != hp0)
			ZDebug("Igni: takt palenia %s - %.0f obrazen ognia (na takt %.0f, ochrona %d, skutecznie %d), HP %d -> %d, %s - z podpalenia",
				SgNpcName(self), sgBurnTickDmg, perTick, prot, zdcore::BurnTickEffective(perTick, prot), hp0,
				self->attribute[NPC_ATR_HITPOINTS], SgDamagePowerText(sgBurnDmg).c_str());
		return r;
	}

	// ---------- trafienia (lancuch zdOnBeforeHit) ----------

	// zdOnBeforeHit (lancuch po Director): true = trafienie polkniete.
	// Upadek bohatera: oCAniCtrl_Human::CheckFallStates (0x6B60CE) -> oCNpc::CreateFallDamage (0x681DA0): deskryptor
	// DAM_FALL (enuModeDamage 0x80, pola 0x280), bez napastnika, komunikat oCMsgDamage EV_DAMAGE_ONCE do wlasnej kolejki
	// -> oCNpc::OnDamage -> nasz hak (CombatHooks). Quen pochlania tez to (QuenShouldAbsorb).
	static bool SgBeforeHit(oCNpc* victim, oCNpc::oSDamageDescriptor& desc) {
		if (!zcfg.signs || !victim)
			return false;
		bool tick = (desc.dwFieldsValid & SG_DESC_OVERLAY) != 0;   // takt palenia / trucizny - to nie cios
		if (tick && victim == sgBurnFrame) {
			sgBurnTickSeen = true;
			sgBurnTickDmg = desc.fDamageTotal;
		}
		if (victim == player) {
			bool attacker = (desc.pNpcAttacker && desc.pNpcAttacker != player) || (desc.pVobAttacker && desc.pVobAttacker != player);
			bool fall = (desc.enuModeDamage & oEDamageType_Fall) != 0;
			// aktywna tarcza (podtrzymywana) - pierwsza: wszystko i upadek, czesc obrazen leczy
			if (sgSustain.active && sgSustain.sign == zdcore::SIGN_QUEN && zdcore::QuenShouldAbsorb(true, tick, attacker, fall)) {
				float dmg = SgEstimateDamage(desc);
				int hp0 = player->attribute[NPC_ATR_HITPOINTS];
				player->attribute[NPC_ATR_HITPOINTS] = zdcore::QuenActiveHealTo(hp0, player->attribute[NPC_ATR_HITPOINTSMAX], dmg,
					zcfg.quenActiveHeal);
				sgSustain.absorbed++;
				sgSustain.absorbedDmg += dmg;
				SgFx("SPELLFX_QUEN_ABSORB", player, player);
				ZDebug("Quen aktywna: pochloniete %s (ok. %.0f obrazen), HP %d -> %d", fall ? "upadek" :
					desc.pNpcAttacker ? SgNpcName(desc.pNpcAttacker) : "trafienie", dmg, hp0, player->attribute[NPC_ATR_HITPOINTS]);
				return true;
			}
			if (!zdcore::QuenShouldAbsorb(sgQuen.active, tick, attacker, fall))
				return false;
			sgQuen.Absorb();
			if (sgQuen.active)
				SgFx("SPELLFX_QUEN_ABSORB", player, player);   // tarcza na kilka trafien - jeszcze trwa
			ZDebug("Quen: pochloniete %s %s (obrazenia %.0f, typ %lu) - %s", attacker ? "trafienie od" : "obrazenia",
				attacker ? (desc.pNpcAttacker ? SgNpcName(desc.pNpcAttacker) : "?") : (fall ? "upadku" : "?"), desc.fDamageTotal,
				desc.enuModeDamage, sgQuen.active ? "tarcza trwa" : "tarcza peka");
			return true;
		}
		// aktywna tarcza: bohater nie atakuje (cios bohatera bez skutku; obrazenia od znakow przechodza)
		if (sgSustain.active && sgSustain.sign == zdcore::SIGN_QUEN && desc.pNpcAttacker == player && !sgOwnDamage && !tick)
			return true;
		SgHold* h = SgFindHold(victim);
		if (!h || h->variant == zdcore::STUN_BURN || tick || h->finisher)
			return false;
		// cios bohatera w kleczacego oszolomionego czlowieka = egzekucja jak przy Daze (finishery POSE_DAZED z S_PRAY);
		// inne trafienia konczy stan skryptu (percepcja obrazen) - plugin nic nie wymusza
		oCNpc* att = desc.pNpcAttacker;
		int wm = att == player ? player->GetWeaponMode() : NPC_WEAPON_NONE;
		if (zcfg.axiiFinisher && h->variant == zdcore::STUN_PRAY && h->active && (wm == NPC_WEAPON_1HS || wm == NPC_WEAPON_2HS) &&
			!desc.pFXHit && DirectorTryStart(victim, zdcore::POSE_DAZED)) {
			h->finisher = true;
			SgFxStop(h->fx);
			ZDebug("Axii: egzekucja kleczacego %s", SgNpcName(victim));
			return true;
		}
		return false;
	}

	// ---------- Axii: marionetka ----------
	// Wrog w walce (czlowiek, ork albo potwor) walczy po stronie bohatera jak przywolaniec G2 (ZS_MM_Rtn_Summoned:
	// B_SetAttitude(ATT_FRIENDLY) + AIV_PARTYMEMBER): postawa do bohatera (pola attitude / tmp_attitude) PRZYJAZNA,
	// AIV_PARTYMEMBER = TRUE (ZS_Attack i ZS_MM_Attack ignoruja wtedy ciosy bohatera - B_AssessDamage / B_MM_AssessDamage).
	// Co 0,5 s plugin decyduje (zdcore::PuppetDecide): walczy z wrogiem bohatera - bez zmian; jest wrog w AxiiPuppetRange -
	// ZDF_Sign_PuppetAttack (ludzie ZS_Attack, orkowie i potwory ZS_MM_Attack; bez B_SetAttitude i bez przestepstw);
	// brak wrogow - ZDF_Sign_PuppetFollow (stan ZS_ZDF_Puppet: idzie za bohaterem, stoi przy nim, nie atakuje go).
	// Stare wartosci wracaja dokladnie: koniec czasu (potem krotkie oszolomienie AxiiPuppetEndStun i powrot do walki),
	// smierc / powalenie, zapis (stan ZS_ZDF_Puppet konczony, po zapisie znow marionetka), wczytanie / zmiana swiata.
	static int SgPickStun(oCNpc* npc);

	static void SgPuppetApply() {
		SgPuppet& p = sgPuppet;
		if (!p.npc || p.applied)
			return;
		int pm = SgAivarPartyMember();
		p.origAtt = p.npc->attitude;
		p.origTmp = p.npc->tmp_attitude;
		p.origParty = pm >= 0 && pm < 100 ? p.npc->aiscriptvars[pm] : 0;
		p.npc->attitude = zdcore::ATTITUDE_FRIENDLY;
		p.npc->tmp_attitude = zdcore::ATTITUDE_FRIENDLY;
		if (pm >= 0 && pm < 100)
			p.npc->aiscriptvars[pm] = 1;
		p.applied = true;
	}

	static void SgPuppetRestore() {
		SgPuppet& p = sgPuppet;
		if (!p.npc || !p.applied)
			return;
		int pm = SgAivarPartyMember();
		p.npc->attitude = p.origAtt;
		p.npc->tmp_attitude = p.origTmp;
		if (pm >= 0 && pm < 100)
			p.npc->aiscriptvars[pm] = p.origParty;
		p.applied = false;
	}

	// Walczy z zywym wrogiem bohatera (ludzie ZS_Attack, orkowie i potwory ZS_MM_Attack).
	static bool SgPuppetEngaged() {
		oCNpc* m = sgPuppet.npc;
		oCNpc* e = m ? m->enemy : nullptr;
		if (!e || e == player || !SgAlive(e))
			return false;
		return SgStateIs(m, "ZS_ATTACK") || SgStateIs(m, "ZS_MM_ATTACK");
	}

	// Najblizszy zywy wrog bohatera w AxiiPuppetRange od marionetki (nullptr = brak).
	static oCNpc* SgPuppetTarget(float* dist) {
		oCNpc* m = sgPuppet.npc;
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!m || !world || !world->voblist_npcs)
			return nullptr;
		zVEC3 mp = m->GetPositionWorld();
		std::vector<zdcore::PuppetCandidate> cands;
		std::vector<oCNpc*> ptrs;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || npc == m || !npc->homeWorld || !npc->GetModel())
				continue;
			zdcore::SignTargetInfo info = SgInfo(npc);
			ptrs.push_back(npc);
			cands.push_back({ (int)ptrs.size() - 1, SgFlatDist(mp, npc->GetPositionWorld()),
				zdcore::SignIsEnemy(info) && !info.partyMember, SgAlive(npc) });
		}
		int pick = zdcore::PickPuppetTarget(cands, zcfg.axiiPuppetRange);
		if (pick < 0)
			return nullptr;
		if (dist)
			*dist = cands[pick].dist;
		return ptrs[pick];
	}

	static void SgPuppetThink() {
		oCNpc* m = sgPuppet.npc;
		if (!m)
			return;
		float dist = 0.0f;
		bool engaged = SgPuppetEngaged();
		oCNpc* target = engaged ? nullptr : SgPuppetTarget(&dist);
		bool following = SgStateIs(m, "ZS_ZDF_PUPPET");
		switch (zdcore::PuppetDecide(engaged, target != nullptr, following)) {
		case zdcore::PUPPET_ATTACK: {
			bool ok = SgCallScript("ZDF_SIGN_PUPPETATTACK", m, target);
			sgPuppet.noTargetLogged = false;
			ZDebug("Axii marionetka: %s atakuje %s (%.0f cm, %s)%s", SgNpcName(m), SgNpcName(target), dist,
				zdcore::PuppetHumanAi(m->guild) ? "ZS_Attack" : "ZS_MM_Attack", ok ? "" : " - BRAK ZDF_Sign_PuppetAttack w skryptach");
			break;
		}
		case zdcore::PUPPET_FOLLOW: {
			bool ok = SgCallScript("ZDF_SIGN_PUPPETFOLLOW", m, player);
			if (!sgPuppet.noTargetLogged)
				ZDebug("Axii marionetka: %s - brak wrogow w %.0f cm, idzie do bohatera%s", SgNpcName(m), zcfg.axiiPuppetRange,
					ok ? "" : " - BRAK ZDF_Sign_PuppetFollow w skryptach");
			sgPuppet.noTargetLogged = true;
			break;
		}
		default:
			break;
		}
	}

	// Stan ZS_ZDF_Puppet konczony od razu (oCNpc_States::EndCurrentState jak przy oszolomieniu) - przed zapisem i na
	// koniec marionetki (nazwa stanu nie trafia do zapisu, NPC nie zostaje przy bohaterze).
	static void SgPuppetLeaveFollow(oCNpc* npc) {
		if (SgAlive(npc) && SgStateIs(npc, "ZS_ZDF_PUPPET")) {
			npc->ClearEM();
			npc->state.EndCurrentState();
		}
	}

	// how: 0 = bez skryptu (smierc, czyszczenie), 1 = czas (krotkie oszolomienie albo ZDF_Sign_PuppetEnd),
	// 2 = ZDF_Sign_PuppetEnd (nowa marionetka)
	static void SgPuppetEnd(const char* why, int how) {
		SgPuppet& p = sgPuppet;
		if (!p.npc)
			return;
		oCNpc* npc = p.npc;
		SgPuppetRestore();
		SgFxStop(p.fx);
		const char* after = "-";
		SgPuppetLeaveFollow(npc);   // takze przy wylaczeniu znakow w menu - nie zostaje przy bohaterze
		if (how && SgAlive(npc)) {
			if (how == 1 && zcfg.axiiPuppetEndStun > 0.0f && SgHoldStart(npc, SgPickStun(npc), zcfg.axiiPuppetEndStun))
				after = "krotkie oszolomienie, potem walka";
			else {
				SgCallScript("ZDF_SIGN_PUPPETEND", npc, player);
				after = "ZDF_Sign_PuppetEnd";
			}
		}
		ZDebug("Axii marionetka: %s - koniec (%s), postawa %d / %d przywrocona, dalej: %s", SgNpcName(npc), why, npc->attitude,
			npc->tmp_attitude, after);
		npc->Release();
		sgPuppet = SgPuppet();
	}

	static void SgPuppetStart(oCNpc* npc, float seconds) {
		SgPuppetEnd("nowa marionetka", 2);
		npc->AddRef();
		sgPuppet.npc = npc;
		sgPuppet.left = seconds;
		sgPuppet.scan = 0.5f;
		SgPuppetApply();
		sgPuppet.fx = SgFx("SPELLFX_AXII_PUPPET", npc, npc, true);
		SgPuppetThink();
		char buf[160];
		sprintf_s(buf, "Marionetka: %s (%.0f s)", npc->name[0].ToChar(), seconds);
		SgTell(buf);
	}

	static void SgPuppetUpdate(float dt) {
		if (!sgPuppet.npc)
			return;
		if (!SgAlive(sgPuppet.npc)) {
			SgPuppetEnd("martwa / powalona / poza swiatem", 0);
			return;
		}
		sgPuppet.left -= dt;
		if (sgPuppet.left <= 0.0f) {
			SgPuppetEnd("czas", 1);
			return;
		}
		SgFxRefresh(sgPuppet.fx);   // bez tworzenia od nowa - nowy efekt gralby od nowa dzwiek przejecia
		sgPuppet.scan -= dt;
		if (sgPuppet.scan > 0.0f)
			return;
		sgPuppet.scan = 0.5f;
		if (!sgPuppet.applied)
			SgPuppetApply();
		SgPuppetThink();
	}

	// ---------- skutki znakow ----------

	static void SgAardHit(const char* label, const std::vector<SgConeTarget>& targets, float damage) {
		float P = sgPower;
		int maxProt = zcfg.aardFlyMaxProt < 0 ? -1 : (int)(zcfg.aardFlyMaxProt * P);   // prog ciezkich rosnie z moca
		for (auto& t : targets) {
			oCNpc* npc = t.npc;
			int hp0 = npc->attribute[NPC_ATR_HITPOINTS];
			zCModel* m = npc->GetModel();
			int protFly = npc->protection[oEDamageIndex_Fly];
			float height = m->bbox3D.maxs.n[1] - m->bbox3D.mins.n[1];
			bool light = zdcore::AardShouldFly(protFly, height, maxProt, zcfg.aardFlyMaxHeight);
			// lekki: ochrona lotu 0 tylko na czas trafienia (lot i obrazenia jak bez ochrony), od razu z powrotem
			if (light)
				npc->protection[oEDamageIndex_Fly] = 0;
			SgDamage(npc, damage, oEDamageType_Fly);
			if (light && npc->protection[oEDamageIndex_Fly] == 0)
				npc->protection[oEDamageIndex_Fly] = protFly;
			SgFx("SPELLFX_AARD_HIT", npc, npc);
			const char* r;
			// lot silnika: oCNpc::OnDamage_Anim -> oCAIHuman::StartFlyDamage (0x69D940, wywolanie 0x679737) przy DAM_FLY
			// i skutecznych obrazeniach lotu > 0, poza woda i lezeniem; czy naprawde polecial - SgFlyUpdate
			if (!FoeReady(npc) || DirectorIsVictim(npc))
				r = "zabity / powalony";
			else if (light) {
				SgFlyWatch(npc, zcfg.aardFallbackPush * P);
				r = SgAirborne(npc) ? "lot zlecony (w powietrzu)" : "lot zlecony (jeszcze na ziemi)";
			}
			else
				r = SgStaggerPush(npc, zcfg.aardPush * P);
			ZDebug("%s: %s (%.0f cm, %.0f st.) HP %d -> %d, ochrona lotu %d (prog %d), wys. %.0f cm, stan ciala %d, %s, %s", label,
				SgNpcName(npc), t.hit.dist, t.hit.angleDeg, hp0, npc->attribute[NPC_ATR_HITPOINTS], protFly, maxProt, height,
				npc->GetBodyState() & BS_ONLY_STATE, light ? "lekki" : "ciezki", r);
		}
	}

	static void SgAard() {
		SgFx("SPELLFX_AARD_WAVE", player, nullptr);
		auto targets = SgCone("Aard", zcfg.aardRange, zcfg.aardCone, true);
		ZDebug("Aard: cele %d (stozek +-%.0f st., %.0f cm)%s", (int)targets.size(), zcfg.aardCone, zcfg.aardRange,
			sgAimNow.valid ? " - kierunek z celownika" : "");
		SgAardHit("Aard", targets, zcfg.aardDamage * sgDmg.mul);
		SgAardWorld(zcfg.aardRange, zcfg.aardCone, "Aard");
	}

	// Tryb alternatywny: fala 360 st. wokol bohatera (polkat 180 st.), obrazenia AardDamage x AardAltDamage.
	static void SgAardSweep() {
		SgFx("SPELLFX_AARD_SWEEP", player, nullptr);
		auto targets = SgCone("Aard fala", zcfg.aardAltRadius, 180.0f, true);
		ZDebug("Aard fala: cele %d (wokol, %.0f cm)", (int)targets.size(), zcfg.aardAltRadius);
		SgAardHit("Aard fala", targets, zcfg.aardDamage * zcfg.aardAltDamage * sgDmg.mul);
		SgAardWorld(zcfg.aardAltRadius, 180.0f, "Aard fala");
	}

	static void SgIgni() {
		float P = sgPower;
		SgFx("SPELLFX_IGNI_CONE", player, nullptr);
		auto targets = SgCone("Igni", zcfg.igniRange, zcfg.igniCone, true);
		ZDebug("Igni: cele %d (stozek +-%.0f st., %.0f cm)%s", (int)targets.size(), zcfg.igniCone, zcfg.igniRange,
			sgAimNow.valid ? " - kierunek z celownika" : "");
		SgFires(true, zcfg.igniRange, zcfg.igniCone, "Igni");
		for (auto& t : targets) {
			oCNpc* npc = t.npc;
			int hp0 = npc->attribute[NPC_ATR_HITPOINTS];
			std::vector<zCEventMessage*> before = SgBurnMessages(npc);
			SgDamage(npc, zcfg.igniDamage * sgDmg.mul, oEDamageType_Fire);
			SgFx("SPELLFX_IGNI_HIT", npc, npc);
			SgFixBurn(npc, before, zcfg.igniBurnTime * P, zcfg.igniBurnDps, sgDmg, false);
			const char* r;
			zCModel* m = npc->GetModel();
			// reakcja palenia stanem ZS_ZDF_Burn tylko u ludzi: u orkow S_FIRE_VICTIM to alias S_FALLDN (upadek, ORC.MDS)
			if (!FoeReady(npc) || DirectorIsVictim(npc))
				r = "zabity / powalony";
			else if (IsHumanGuild(npc) && SgModelHas(m, "S_FIRE_VICTIM") && zcfg.igniReactTime > 0.0f &&
				SgHoldStart(npc, zdcore::STUN_BURN, zcfg.igniReactTime))
				r = "reakcja palenia (stan ZS_ZDF_Burn, S_FIRE_VICTIM)";
			else
				r = SgStaggerPush(npc, 0.0f);
			ZDebug("Igni: %s (%.0f cm) HP %d -> %d, ochrona ognia %d, plonie %d, %s", SgNpcName(npc), t.hit.dist, hp0,
				npc->attribute[NPC_ATR_HITPOINTS], npc->protection[oEDamageIndex_Fire], npc->HasBodyStateModifier(BS_MOD_BURNING) != 0, r);
		}
	}

	// Niepamiec jak Spell_Charm G2: skrypt Spell_Logic_Charm z self = bohater, other = cel (B_DeletePetzCrime,
	// AIV_NpcSawPlayerCommit, AIV_LastFightAgainstPlayer, tymczasowa wrogosc). Na czas wywolania: mana bohatera na zapas
	// (warunek SPL_Cost_Charm, skrypt jej nie zdejmuje), Charm_Test (zadanie Ignaza) i self/other przywracane.
	// Bez skryptu (mody) - te same kroki w C++ bez licznikow PETZCOUNTER.
	// logicRet: wynik Spell_Logic_Charm (zCParser::CallFunc 0x7929F0 zwraca adres statycznego wyniku 0xAB6298), -1 = brak.
	static const char* SgForget(oCNpc* npc, int* logicRet = nullptr) {
		if (logicRet)
			*logicRet = -1;
		int idx = parser ? parser->GetIndex(zSTRING("SPELL_LOGIC_CHARM")) : -1;
		zCPar_Symbol* sSelf = parser ? parser->GetSymbol(zSTRING("SELF")) : nullptr;
		zCPar_Symbol* sOther = parser ? parser->GetSymbol(zSTRING("OTHER")) : nullptr;
		if (idx >= 0 && sSelf && sOther) {
			void* oldSelf = sSelf->GetInstanceAdr();
			void* oldOther = sOther->GetInstanceAdr();
			zCPar_Symbol* sTest = parser->GetSymbol(zSTRING("CHARM_TEST"));
			int oldTest = 0;
			if (sTest)
				sTest->GetValue(oldTest, 0);
			int mana = player->attribute[NPC_ATR_MANA];
			player->attribute[NPC_ATR_MANA] = mana > 100000 ? mana : 100000;
			parser->SetInstance(zSTRING("SELF"), player);
			parser->SetInstance(zSTRING("OTHER"), npc);
			void* ret = parser->CallFunc(idx, 0);
			if (logicRet && ret)
				*logicRet = *(int*)ret;
			parser->SetInstance(zSTRING("SELF"), oldSelf);
			parser->SetInstance(zSTRING("OTHER"), oldOther);
			player->attribute[NPC_ATR_MANA] = mana;
			if (sTest)
				sTest->SetValue(oldTest, 0);
			return "Spell_Logic_Charm";
		}
		int saw = SgParserInt("AIV_NPCSAWPLAYERCOMMIT", 1), fight = SgParserInt("AIV_LASTFIGHTAGAINSTPLAYER", 0);
		if (saw >= 0 && saw < 100)
			npc->aiscriptvars[saw] = SgParserInt("CRIME_NONE", 0);
		if (fight >= 0 && fight < 100)
			npc->aiscriptvars[fight] = SgParserInt("FIGHT_NONE", 0);
		int guildAtt = npc->GetGuildAttitude(player->guild);
		if (guildAtt != zdcore::ATTITUDE_HOSTILE && npc->GetAttitude(player) == zdcore::ATTITUDE_HOSTILE)
			npc->SetTmpAttitude(guildAtt);
		return "C++ (brak Spell_Logic_Charm)";
	}

	// Urok (runda 6, SignAxiiCharm): skryptowa Spell_Cast_Charm z self = bohater, other = cel - w NB tam jest cala
	// niepamiec zwoju ITSC_Charm: B_DeletePetzCrime, AIV_NpcSawPlayerCommit / AIV_LastFightAgainstPlayer = 0, postawa,
	// RX_SetNpcVar(other, VAR_CAUGHT_THIEF, FALSE) (przylapanie na kradziezy bez konfrontacji - warunek RX_CanStealNpcAst
	// i "Juz cie przylapano..." w DIA_Addon_Steal_DoIt) oraz reakcje questow (Ramirez, Larius, Reinar, Falk, Sagitta,
	// Orun, Urshak, test Ignaza). Skutki uboczne cofane: koszt many (SPL_Cost_Charm / SPL_Cost_Scroll2 - mane Axii zdjal
	// juz znak), AIV_SelectSpell bohatera i wpis questa runy (RX_RuneRoutine: TESTRUNEME na czas wywolania = TRUE, bo znak
	// to nie runa), AI_Wait(self, 1.0) - komunikat EV_WAIT bohatera usuwany z kolejki (blokowalby gest / ruch po znaku).
	static const char* SgCharmCast(oCNpc* npc, int* waits) {
		*waits = 0;
		if (!parser || parser->GetIndex(zSTRING("SPELL_CAST_CHARM")) < 0)
			return "brak Spell_Cast_Charm";
		std::vector<zCEventMessage*> before;
		zCEventManager* em = player->GetEM(FALSE);
		if (em)
			for (int i = 0; i < em->GetNumMessages(); i++)
				before.push_back(em->GetEventMessage(i));
		int mana = player->attribute[NPC_ATR_MANA];
		int sel = SgParserInt("AIV_SELECTSPELL", -1);
		int selOld = sel >= 0 && sel < 100 ? player->aiscriptvars[sel] : 0;
		zCPar_Symbol* sRune = parser->GetSymbol(zSTRING("TESTRUNEME"));
		int runeOld = 0;
		if (sRune) {
			sRune->GetValue(runeOld, 0);
			sRune->SetValue(1, 0);
		}
		SgCallScript("SPELL_CAST_CHARM", player, npc);
		if (sRune)
			sRune->SetValue(runeOld, 0);
		if (sel >= 0 && sel < 100)
			player->aiscriptvars[sel] = selOld;
		player->attribute[NPC_ATR_MANA] = mana;
		em = player->GetEM(FALSE);
		if (em)
			for (int i = 0; i < em->GetNumMessages(); i++) {
				zCEventMessage* m = em->GetEventMessage(i);
				oCMsgState* st = zDYNAMIC_CAST<oCMsgState>(m);
				if (st && st->subType == oCMsgState::EV_WAIT && std::find(before.begin(), before.end(), m) == before.end()) {
					m->Delete();
					(*waits)++;
				}
			}
		return "Spell_Cast_Charm";
	}

	// ---------- premia kieszonkowa Axii (runda 6) ----------
	// Szansa kradziezy NB = RX_GetThieftChance(thief, target) (procent, DIA_Addon_Steal i kradziez od tylu KEY_DOSTEAL,
	// opis opcji w B_Addon_Steal). Paczka NB podmienia obie funkcje (data_nb\...\zDismember_NB_Thief.d): premia z tablic
	// ZDF_AxiiThiefId[8] / ZDF_AxiiThiefPct[8] wedlug Hlp_GetInstanceID(target), opis "(..: N%, Axii +M%)". Plugin tylko
	// wypelnia tablice; przed zapisem gry zeruje (zmienne skryptu ida do zapisu), po zapisie / wczytaniu wpisuje od nowa.
	// Steam: wanilia nie ma szansy w procentach (C_Beklauen / B_Beklauen = zrecznosc >= TheftDex) - bez skryptu premia nic nie
	// robi (log raz).
	static const int SG_THIEF_SLOTS = 8;
	static zdcore::ThiefBonusList sgThief;
	static float sgThiefClock = 0.0f;
	static bool sgThiefDirty = true;
	static bool sgThiefNoScriptLogged = false;

	static bool SgThiefWrite(const int* ids, const int* pcts) {
		zCPar_Symbol* sId = parser ? parser->GetSymbol(zSTRING("ZDF_AXIITHIEFID")) : nullptr;
		zCPar_Symbol* sPct = parser ? parser->GetSymbol(zSTRING("ZDF_AXIITHIEFPCT")) : nullptr;
		if (!sId || !sPct || (int)sId->ele < SG_THIEF_SLOTS || (int)sPct->ele < SG_THIEF_SLOTS)
			return false;
		for (int i = 0; i < SG_THIEF_SLOTS; i++) {
			sId->SetValue(ids[i], i);
			sPct->SetValue(pcts[i], i);
		}
		return true;
	}

	static void SgThiefSync() {
		int ids[SG_THIEF_SLOTS], pcts[SG_THIEF_SLOTS];
		sgThief.Export(ids, pcts, SG_THIEF_SLOTS, sgThiefClock);
		SgThiefWrite(ids, pcts);
		sgThiefDirty = false;
	}

	static void SgThiefZero() {
		int z[SG_THIEF_SLOTS] = {};
		SgThiefWrite(z, z);
	}

	static void SgThiefUpdate(float dt) {
		sgThiefClock += dt;
		size_t n = sgThief.Items().size();
		for (auto& b : sgThief.Items())
			if (sgThiefClock >= b.until) {
				zCPar_Symbol* s = parser ? parser->GetSymbol(b.instance) : nullptr;
				ZDebug("Axii: premia kieszonkowa +%d%% dla %s wygasla", b.pct, s ? s->name.ToChar() : "?");
			}
		if (sgThief.Prune(sgThiefClock) || n != sgThief.Items().size())
			sgThiefDirty = true;
		if (sgThiefDirty)
			SgThiefSync();
	}

	static void SgThiefGrant(oCNpc* npc, float tranceSeconds) {
		if (zcfg.axiiPickpocketBonus <= 0)
			return;
		float secs = zdcore::ThiefBonusSeconds(zcfg.axiiPickpocketTime, tranceSeconds);
		if (secs <= 0.0f)
			return;
		if (!parser || !parser->GetSymbol(zSTRING("ZDF_AXIITHIEFID"))) {
			if (!sgThiefNoScriptLogged)
				ZDebug("Axii: premia kieszonkowa %d%% bez skryptu zDismember_NB_Thief.d (tylko paczka New Balance) - pominieta",
					zcfg.axiiPickpocketBonus);
			sgThiefNoScriptLogged = true;
			return;
		}
		float until = sgThiefClock + secs;
		sgThief.Add(npc, npc->GetInstance(), zcfg.axiiPickpocketBonus, until);
		SgThiefSync();
		ZDebug("Axii: premia kieszonkowa +%d%% dla %s do %.0f s zegara znakow (za %.0f s)", zcfg.axiiPickpocketBonus,
			SgNpcName(npc), until, secs);
	}

	static bool SgHumanModel(oCNpc* npc) {
		zCModel* m = npc->GetModel();
		return m && IsHumanGuild(npc) && _strnicmp(m->GetVisualName().ToChar(), "HUMANS", 6) == 0;
	}

	// Wariant oszolomienia: ludzie (model HUMANS) - losowo z AxiiStunAnis bez powtorki pod rzad, tylko te, ktorych wejscie
	// jest w modelu; potwory i orkowie - STUN_MONSTER.
	static int SgPickStun(oCNpc* npc) {
		if (!SgHumanModel(npc))
			return zdcore::STUN_MONSTER;
		zCModel* m = npc->GetModel();
		uint32_t mask = 0;
		for (int v = zdcore::STUN_PRAY; v <= zdcore::STUN_WOUNDEDB; v++)
			if ((zcfg.axiiStunMask & zdcore::StunBit(v)) && SgModelHas(m, SgStunEntryAni(v)))
				mask |= zdcore::StunBit(v);
		int v = zdcore::PickStunVariant(mask, sgLastStun, ZRand32());
		return v == zdcore::STUN_NONE ? zdcore::STUN_MONSTER : v;
	}

	// Cel Axii (runda 7, zgloszenie "Axii - nadal musze byc blisko"): plugin wybiera cel sam - fokus silnika / NPC pod
	// celownikiem, jesli jest dobrym celem (w waskim stozku AxiiCone i zasiegu AxiiRange, z linia wzroku), inaczej najblizszy
	// dobry NPC w stozku wzdluz kierunku celownika zQuickSign / GFA (bez celowania - przod bohatera). Filtr celu
	// zdcore::AxiiRejectReason: poza walka tez neutralni i przyjazni (Urok / trans to nie atak), w walce wrogowie; odpornych
	// rozstrzyga lista Uroku po wyborze. Fokus silnika ma zasieg Focus_Normal / Focus_Magic i targetCollectRange czaru -
	// nie jest potrzebny (zQuickSign przy Znakach zDismember nie szuka juz fokusu magii).
	static oCNpc* SgAxiiPick(oCNpc* focus, const char** how) {
		*how = "stozek";
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!world || !world->voblist_npcs)
			return nullptr;
		zVEC3 p = player->GetPositionWorld(), at = SgFacing();
		std::vector<zdcore::AxiiCandidate> cands;
		std::vector<oCNpc*> npcs;
		std::vector<std::string> whys;
		for (auto it = world->voblist_npcs->next; it; it = it->next) {
			oCNpc* npc = it->data;
			if (!npc || npc == player || !npc->homeWorld || !npc->GetModel())
				continue;
			zVEC3 q = npc->GetPositionWorld();
			zdcore::ConeHit h = zdcore::ConeTest(p.n[0], p.n[1], p.n[2], at.n[0], at.n[2], q.n[0], q.n[1], q.n[2],
				zcfg.axiiRange, zcfg.axiiCone, SG_MAX_DY);
			if (h.dist > zcfg.axiiRange + 1500.0f && npc != focus)
				continue;   // daleko - nawet do logu
			zdcore::AxiiCandidate c;
			c.id = (int)npcs.size();
			c.dist = h.dist;
			c.angleDeg = h.angleDeg;
			c.cone = h.reason;
			c.reject = zdcore::AxiiRejectReason(SgInfo(npc));
			c.focus = npc == focus;
			std::string why;
			if (!c.reject && c.cone == zdcore::CONE_OK)
				c.visible = SgVisible(npc, zcfg.debug ? &why : nullptr);
			cands.push_back(c);
			npcs.push_back(npc);
			whys.push_back(why);
		}
		int pick = zdcore::PickAxiiTarget(cands);
		const char* src = sgAimNow.valid ? sgAimNow.source : "przod bohatera";
		if (pick >= 0) {
			const zdcore::AxiiCandidate& c = cands[pick];
			*how = !c.focus ? "stozek" : sgAimNow.valid && sgAimNow.npc == npcs[pick] ? "pod celownikiem" : "fokus";
			ZDebug("Axii: cel %s (%.0f cm, stozek %.0f st. z +-%.0f, zasieg %.0f cm, %s, kierunek z: %s)", SgNpcName(npcs[pick]),
				c.dist, c.angleDeg, zcfg.axiiCone, zcfg.axiiRange, *how, src);
			return npcs[pick];
		}
		ZDebug("Axii: brak celu w %.0f cm / +-%.0f st. (kierunek %.2f %.2f z: %s, fokus %s) - najblizsi kandydaci:", zcfg.axiiRange,
			zcfg.axiiCone, at.n[0], at.n[2], src, SgNpcName(focus));
		std::vector<int> order(cands.size());
		for (size_t i = 0; i < order.size(); i++)
			order[i] = (int)i;
		std::sort(order.begin(), order.end(), [&](int x, int y) { return cands[x].dist < cands[y].dist; });
		for (size_t k = 0; k < order.size() && k < 5; k++) {
			const zdcore::AxiiCandidate& c = cands[order[k]];
			const char* r = zdcore::AxiiCandidateReason(c);
			ZDebug("Axii: pominiety %s%s %.0f cm, kat %.0f st., dy %.0f: %s%s%s", SgNpcName(npcs[order[k]]), c.focus ? " (fokus)" : "",
				c.dist, c.angleDeg, npcs[order[k]]->GetPositionWorld().n[1] - p.n[1], r ? r : "?",
				whys[order[k]].empty() ? "" : " - ", whys[order[k]].c_str());
		}
		if (order.empty())
			ZDebug("Axii: pominiety - brak NPC w %.0f cm", zcfg.axiiRange + 1500.0f);
		return nullptr;
	}

	static void SgAxii(oCNpc* focus, bool alt) {
		if (sgAimNow.valid && sgAimNow.npc) {
			// celowanie: NPC pod celownikiem (promien GFA) ma pierwszenstwo przed fokusem silnika
			ZDebug("Axii: celowanie (%s) - pod celownikiem %s, fokus silnika %s", sgAimNow.source, SgNpcName(sgAimNow.npc),
				SgNpcName(focus));
			focus = sgAimNow.npc;
		}
		const char* how;
		oCNpc* npc = SgAxiiPick(focus, &how);
		if (!npc) {
			SgTell("Axii: brak celu");
			return;
		}
		zdcore::SignTargetInfo info = SgInfo(npc);
		if (zdcore::AxiiInCombat(info)) {
			if (alt) {
				// ludzie, orkowie i potwory (runda 4; w rundzie 3 tylko ludzie - potwory dostawaly zwykle oszolomienie)
				if (SgHold* old = SgFindHold(npc))
					SgHoldDrop(old - sgHolds.data(), "marionetka");
				ZDebug("Axii: %s (%s, gildia %d, %s) - MARIONETKA na %.1f s", SgNpcName(npc), how, npc->guild,
					zdcore::PuppetHumanAi(npc->guild) ? "AI ludzi" : "AI potworow / orkow", zcfg.axiiPuppetTime * sgPower);
				SgPuppetStart(npc, zcfg.axiiPuppetTime * sgPower);
				return;
			}
			int v = SgPickStun(npc);
			float t = zcfg.axiiStunTime * sgPower;
			if (SgHoldStart(npc, v, t)) {
				if (v != zdcore::STUN_MONSTER)
					sgLastStun = v;
				ZDebug("Axii: %s (%s) w walce - oszolomienie %.1f s, wariant %s (stan AI)", SgNpcName(npc), how, t,
					zdcore::StunName(v));
			}
			else
				ZDebug("Axii: %s (%s) w walce - stan niemozliwy (stan ciala %d), tylko potkniecie: %s", SgNpcName(npc), how,
					npc->GetBodyState() & BS_ONLY_STATE, SgStaggerPush(npc, 0.0f));
			return;
		}
		int logicRet = -1;
		const char* f = SgForget(npc, &logicRet);
		bool castFound = parser && parser->GetIndex(zSTRING("SPELL_CAST_CHARM")) >= 0;
		zdcore::AxiiCharmPlan plan = zdcore::PlanAxiiCharm(logicRet >= 0, logicRet, SgParserInt("SPL_SENDSTOP", 3), castFound,
			zcfg.signAxiiCharm, zdcore::AxiiTranceApplies(SgInfo(npc)));
		if (plan.immune) {
			// lista odpornych z Spell_Logic_Charm (komunikat "To sie nie uda..." wypisal skrypt): bez Uroku, transu i premii
			ZDebug("Axii: %s (%s) poza walka - odporny na Urok (Spell_Logic_Charm = SPL_SENDSTOP mimo many na zapas)",
				SgNpcName(npc), how);
			return;
		}
		const char* c = "Urok wylaczony (SignAxiiCharm=0)";
		int waits = 0;
		if (plan.callCast)
			c = SgCharmCast(npc, &waits);
		SgFx("SPELLFX_AXII_FORGET", npc, npc);
		ZDebug("Axii: %s (%s) poza walka - niepamiec (%s, logika %d), %s%s, postawa %d -> %d", SgNpcName(npc), how, f, logicRet, c,
			waits ? " - AI_Wait bohatera usuniete" : "", info.attitude, npc->GetAttitude(player));
		if (IsHumanGuild(npc) && !info.partyMember)
			SgThiefGrant(npc, zcfg.axiiTranceTime > 0.0f && plan.trance ? zcfg.axiiTranceTime * sgPower : 0.0f);
		// runda 5: niepamiec + trans - czlowiek nie zagada, nie zatrzyma (straz bramy, port) i nie zauwazy bohatera
		if (zcfg.axiiTranceTime > 0.0f && plan.trance) {
			float t = zcfg.axiiTranceTime * sgPower;
			if (SgHoldStart(npc, zdcore::STUN_TRANCE, t)) {
				ZDebug("Axii: %s - trans %.1f s (bez percepcji bohatera, bez rozmowy; trafienie przerywa)", SgNpcName(npc), t);
				char buf[160];
				sprintf_s(buf, "Trans: %s (%.0f s)", npc->name[0].ToChar(), t);
				SgTell(buf);
			}
			else
				ZDebug("Axii: %s - trans niemozliwy (stan ciala %d)", SgNpcName(npc), npc->GetBodyState() & BS_ONLY_STATE);
		}
	}

	// Srodek kregu / pulapki Yrden: miejsce bohatera albo (celowanie) punkt celownika, najdalej SignAimRange w poziomie;
	// p = wysokosc srodka bohatera nad ziemia w tym punkcie (strefa liczona jak od bohatera), ground = ziemia (efekt glifu).
	static void SgYrdenCenter(zVEC3& p, zVEC3& ground) {
		zVEC3 hp = player->GetPositionWorld();
		float g, hg;
		float heroUp = MoveGroundAt(hp, hg) ? hp.n[1] - hg : 90.0f;
		p = hp;
		if (sgAimNow.valid) {
			float x, z;
			zdcore::AimPointInRange(hp.n[0], hp.n[2], sgAimNow.point.n[0], sgAimNow.point.n[2], zcfg.signAimRange, x, z);
			// wysokosc w skroconym punkcie: wzdluz prostej bohater - punkt celownika (trafienie lezy na powierzchni)
			float full = SgFlatDist(hp, sgAimNow.point), part = SgFlatDist(hp, zVEC3(x, hp.n[1], z));
			float feet = hp.n[1] - heroUp;
			float y = full > 1.0f ? feet + (sgAimNow.point.n[1] - feet) * (part / full) : sgAimNow.point.n[1];
			zVEC3 probe(x, y + 20.0f, z);
			if (MoveGroundAt(probe, g)) {
				p = zVEC3(x, g + heroUp, z);
				ground = zVEC3(x, g, z);
				ZDebug("Yrden: srodek z celownika (%s) %.0f cm od bohatera", sgAimNow.source, SgFlatDist(hp, p));
				return;
			}
			ZDebug("Yrden: punkt celownika bez ziemi - krag przy bohaterze");
		}
		ground = MoveGroundAt(p, g) ? zVEC3(p.n[0], g, p.n[2]) : p;
	}

	static void SgYrden() {
		float P = sgPower;
		SgFxStop(sgZoneFx);
		sgZone.Stop();
		zVEC3 p, ground;
		SgYrdenCenter(p, ground);
		sgZoneSlow = zdcore::YrdenSlowAt(P, zcfg.yrdenSlow, zcfg.yrdenSlowMin);
		sgZoneArmor = zdcore::YrdenArmorAt(P, zcfg.yrdenArmor, zcfg.yrdenArmorMax);
		sgZone.Start(p.n[0], p.n[1], p.n[2], zcfg.yrdenRadius, zcfg.yrdenTime * P);
		sgZoneGround = ground;
		sgZoneFx = SgFxAt("SPELLFX_YRDEN_ZONE", ground, true);
		sgYrdenScan = 0.0f;
		ZDebug("Yrden: strefa %.0f cm na %.1f s w (%.0f, %.0f, %.0f), tempo x%.2f, ochrona -%.0f%%", zcfg.yrdenRadius,
			zcfg.yrdenTime * P, p.n[0], p.n[1], p.n[2], sgZoneSlow, sgZoneArmor);
	}

	// Tryb alternatywny: pulapka-mina (glif z ladunkami) w miejscu rzutu; ponowny rzut przenosi pulapke.
	static void SgYrdenTrap() {
		float P = sgPower;
		SgTrapEnd("nowa pulapka");
		zVEC3 p, ground;
		SgYrdenCenter(p, ground);
		sgTrap.active = true;
		sgTrap.zone.Start(p.n[0], p.n[1], p.n[2], zcfg.yrdenTrapRadius, zcfg.yrdenTrapTime * P);
		sgTrap.st.Arm(zcfg.yrdenTrapCharges, zcfg.yrdenTrapInterval);
		sgTrap.ground = ground;
		sgTrap.clock = 0.0f;
		sgTrap.damage = zcfg.yrdenTrapDamage * sgDmg.mul;
		sgTrap.slow = zdcore::YrdenSlowAt(P, zcfg.yrdenSlow, zcfg.yrdenSlowMin);
		sgTrap.armor = zdcore::YrdenArmorAt(P, zcfg.yrdenArmor, zcfg.yrdenArmorMax);
		sgTrap.fx = SgFxAt("SPELLFX_YRDEN_TRAP", ground, true);
		// kule ladunkow na okregu 110 cm, 90 cm nad glifem, pierwsza przed bohaterem (3 kule = trojkat jak w efekcie)
		zVEC3 at = player->GetAtVectorWorld();
		float al = sqrtf(at.n[0] * at.n[0] + at.n[2] * at.n[2]);
		float fx = al > 0.01f ? at.n[0] / al : 0.0f, fz = al > 0.01f ? at.n[2] / al : 1.0f;
		int orbs = sgTrap.st.charges < SG_MAX_ORBS ? sgTrap.st.charges : SG_MAX_ORBS;
		for (int k = 0; k < orbs; k++) {
			float a = 6.2831853f * (float)k / (float)orbs;
			float side = SG_ORB_RADIUS * sinf(a), fwd = SG_ORB_RADIUS * cosf(a);   // prawo / przod bohatera
			zVEC3 pos(ground.n[0] + fz * side + fx * fwd, ground.n[1] + SG_ORB_HEIGHT, ground.n[2] - fx * side + fz * fwd);
			sgTrap.orbs.push_back({ pos, false, nullptr });
		}
		SgTrapOrbsSpawn();
		sgYrdenScan = 0.0f;
		ZDebug("Yrden pulapka: glif %.0f cm na %.1f s, ladunki %d, obrazenia %.0f, tempo x%.2f, ochrona -%.0f%%",
			zcfg.yrdenTrapRadius, zcfg.yrdenTrapTime * P, zcfg.yrdenTrapCharges, sgTrap.damage, sgTrap.slow, sgTrap.armor);
	}

	static void SgQuen() {
		float P = sgPower;
		bool renew = sgQuen.active;
		int hits = zdcore::QuenHitsAt(P);
		sgQuen.Cast(zcfg.quenTime * P, hits);
		sgQuenPower = P;
		sgQuenDmg = sgDmg;
		SgQuenFxGuard();   // nowa tarcza: efekt przy bohaterze (stary oderwany - od nowa)
		ZDebug("Quen: tarcza %.1f s, trafien %d (%s)", zcfg.quenTime * P, hits, renew ? "odnowiona" : "nowa");
	}

	// aim: celowanie z chwili rzutu (nullptr / nieaktywne = przod bohatera); na czas skutku w sgAimNow.
	static void SgDoCast(int sign, oCNpc* focus, bool console, bool alt, const SgAim* aim = nullptr) {
		if (!player || !player->homeWorld)
			return;
		ZPROF_SCOPE("Signs.Effect");
		sgPower = SgPowerNow();
		sgDmg = SgDamagePowerNow(sgPower);
		SgAimRelease(sgAimNow);
		if (aim && aim->valid) {
			sgAimNow = *aim;
			if (sgAimNow.npc)
				sgAimNow.npc->AddRef();
		}
		ZDebug("Znak %s: rzut (%s%s), moc x%.2f (mana max %d), %s (INT %d%s), mana %d%s%s", zdcore::SignName(sign),
			console ? "konsola" : "runa", alt ? ", tryb alternatywny" : "", sgPower, player->attribute[NPC_ATR_MANAMAX],
			SgDamagePowerText(sgDmg).c_str(), SgIntellect(), SgDamageArray() ? ", obrazenia w aryDamage - NB" : "",
			player->attribute[NPC_ATR_MANA],
			sgAimNow.valid ? ", celowanie: " : "", sgAimNow.valid ? sgAimNow.source : "");
		switch (sign) {
		case zdcore::SIGN_AARD:
			alt ? SgAardSweep() : SgAard();
			break;
		case zdcore::SIGN_IGNI:
			if (alt)
				SgSustainStart(sign, console);
			else
				SgIgni();
			break;
		case zdcore::SIGN_AXII:
			SgAxii(focus, alt);
			break;
		case zdcore::SIGN_YRDEN:
			alt ? SgYrdenTrap() : SgYrden();
			break;
		case zdcore::SIGN_QUEN:
			if (alt)
				SgSustainStart(sign, console);
			else
				SgQuen();
			break;
		}
		SgAimRelease(sgAimNow);
	}

	// ---------- haki rzutu ----------

	static int SgSignOf(oCSpell* spell) {
		if (!spell || !player || spell->spellCasterNpc != player)
			return -1;
		return zdcore::SignFromSpellId(zcfg.signSpellIds, spell->spellID);
	}

	static void SgRefuse(oCSpell* self, int sign, zdcore::SignCastResult r, int cost, bool alt) {
		self->spellStatus = SG_SPL_SENDSTOP;
		char buf[160];
		// komunikaty w cp1250 (escape - niezaleznie od kodowania zrodla): l = \xB3, e = \xEA, a = \xB9, z = \x9F
		if (r == zdcore::SIGNCAST_NO_MANA)
			sprintf_s(buf, alt ? "Za ma\xB3o many: Znak %s, tryb alternatywny (%d)" : "Za ma\xB3o many: Znak %s (%d)",
				zdcore::SignName(sign), cost);
		else if (r == zdcore::SIGNCAST_COOLDOWN)
			sprintf_s(buf, "Znak %s jeszcze si\xEA odnawia", zdcore::SignName(sign));
		else
			sprintf_s(buf, "Znaki Wied\x9Fmina wy\xB3\xB9" "czone (menu zDismember)");
		SgTell(buf);
		static unsigned int lastLogFrame = 0;
		if (zdFrame - lastLogFrame > 30) {
			lastLogFrame = zdFrame;
			ZDebug("Znak %s: odmowa (%s%s), mana %d, koszt %d, odnowienie %.2f s", zdcore::SignName(sign),
				r == zdcore::SIGNCAST_NO_MANA ? "mana" : r == zdcore::SIGNCAST_COOLDOWN ? "odnowienie" : "wylaczone",
				alt ? ", tryb alternatywny" : "", player->attribute[NPC_ATR_MANA], cost, sgCooldown.Left(sign));
		}
	}

	// CallScriptInvestedMana: pierwszy Invest = start przytrzymania, kolejne co klatke = decyzja / podtrzymanie.
	static void __fastcall SgCallScriptInvestedMana(oCSpell* self, void* edx);
	static CInvoke<void(__fastcall*)(oCSpell*, void*)> Ivk_oCSpell_CallScriptInvestedMana(
		ENGINE == Engine_G2A ? 0x00485D30 : 0, &SgCallScriptInvestedMana, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SgCallScriptInvestedMana(oCSpell* self, void* edx) {
		ZHookHit(ZH_SPELL_INVESTEDMANA);
		int sign = SgSignOf(self);
		if (sign < 0) {
			Ivk_oCSpell_CallScriptInvestedMana(self, edx);
			return;
		}
		ZPROF_SCOPE("Signs.Invest");
		DWORD now = GetTickCount();
		bool first = self->manaInvested == 0 || sgCast.spell != self || sgCast.hold.phase == zdcore::HOLD_NONE;
		if (first) {
			int cost = zcfg.signManaCost[sign];
			zdcore::SignCastResult r = zdcore::SignCheckCast(zcfg.enabled && zcfg.signs, player->attribute[NPC_ATR_MANA], cost,
				sgCooldown.Left(sign));
			if (r != zdcore::SIGNCAST_OK) {
				sgCast.hold.Reset();
				SgRefuse(self, sign, r, cost, false);
				return;
			}
			sgCast.spell = self;
			sgCast.sign = sign;
			sgCast.last = now;
			sgCast.alt = false;
			zdcore::HoldAction a = sgCast.hold.Begin(zcfg.signAltHold);
			self->spellStatus = a == zdcore::HOLD_CAST_NORMAL ? SG_SPL_SENDCAST : SG_SPL_CANINVEST_NO_MANADEC;
			return;
		}
		float dt = (float)(now - sgCast.last) / 1000.0f;
		sgCast.last = now;
		// Celowanie zQuickSign trwa od wcisniecia F (nie blokuje trybu alternatywnego): krotko = znak zwykly w punkt
		// celownika, dluzej niz SignAltHold = tryb alternatywny; zQuickSign rysuje ladowanie z ZDISM_SignHoldState.
		zdcore::HoldAction a = sgCast.hold.Tick(dt, zcfg.signAltHold, SgSustainedAlt(sign));
		switch (a) {
		case zdcore::HOLD_WAIT:
			self->spellStatus = SG_SPL_CANINVEST_NO_MANADEC;
			break;
		case zdcore::HOLD_CAST_ALT: {
			int cost = SgAltCost(sign);
			zdcore::SignCastResult r = zdcore::SignCheckCast(true, player->attribute[NPC_ATR_MANA], cost, 0.0f);
			if (r != zdcore::SIGNCAST_OK) {
				sgCast.hold.Reset();
				SgRefuse(self, sign, r, cost, true);
				break;
			}
			sgCast.alt = true;
			self->spellStatus = SG_SPL_SENDCAST;
			ZDebug("Znak %s: przytrzymanie %.2f s (prog %.2f s) - tryb alternatywny%s", zdcore::SignName(sign), sgCast.hold.held,
				zcfg.signAltHold, SgQsAiming() ? (zdcore::SignAimsAt(sign, true) ? ", celownik zQuickSign zostaje" :
				", celownik zQuickSign znika (bez kierunku)") : "");
			break;
		}
		case zdcore::HOLD_START_SUSTAIN:
			if (player->attribute[NPC_ATR_MANA] <= 0 || !SgSustainStart(sign, false)) {
				sgCast.hold.Reset();
				SgRefuse(self, sign, zdcore::SIGNCAST_NO_MANA, 1, true);
				break;
			}
			sgSustain.sinceInvest = 0.0f;
			self->spellStatus = SG_SPL_CANINVEST_NO_MANADEC;
			ZDebug("Znak %s: przytrzymanie %.2f s (prog %.2f s) - podtrzymanie%s", zdcore::SignName(sign), sgCast.hold.held,
				zcfg.signAltHold, SgQsAiming() ? (zdcore::SignAimsAt(sign, true) ? ", celownik zQuickSign zostaje" :
				", celownik zQuickSign znika (bez kierunku)") : "");
			break;
		case zdcore::HOLD_SUSTAIN:
			sgSustain.sinceInvest = 0.0f;
			self->spellStatus = sgSustain.active ? SG_SPL_CANINVEST_NO_MANADEC : SG_SPL_SENDSTOP;
			break;
		default:   // HOLD_STOP (koniec many, zapis) albo stan nieznany
			SgSustainEnd("stop");
			sgCast.hold.Reset();
			self->spellStatus = SG_SPL_SENDSTOP;
			break;
		}
	}

	// SetReleaseStatus (puszczenie klawisza, koniec many w Invest): krotkie wcisniecie = tryb zwykly (SENDCAST), koniec
	// podtrzymania / po trybie alternatywnym = SENDSTOP. Skrypt Spell_ProcessMana_Release nie jest wolany dla znakow.
	static void __fastcall SgSetReleaseStatus(oCSpell* self, void* edx);
	static CInvoke<void(__fastcall*)(oCSpell*, void*)> Ivk_oCSpell_SetReleaseStatus(
		ENGINE == Engine_G2A ? 0x00486670 : 0, &SgSetReleaseStatus, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall SgSetReleaseStatus(oCSpell* self, void* edx) {
		ZHookHit(ZH_SPELL_RELEASE);
		int sign = SgSignOf(self);
		if (sign < 0) {
			Ivk_oCSpell_SetReleaseStatus(self, edx);
			return;
		}
		ZPROF_SCOPE("Signs.Release");
		if (sgCast.spell != self) {
			self->spellStatus = SG_SPL_SENDSTOP;
			return;
		}
		float held = sgCast.hold.held;
		zdcore::HoldAction a = sgCast.hold.Release();
		if (a == zdcore::HOLD_CAST_NORMAL && zcfg.enabled && zcfg.signs) {
			sgCast.alt = false;
			self->spellStatus = SG_SPL_SENDCAST;
			ZDebug("Znak %s: puszczony po %.2f s - tryb zwykly", zdcore::SignName(sign), held);
			return;
		}
		if (sgSustain.active && sgSustain.sign == sign)
			SgSustainEnd("puszczenie");
		self->spellStatus = SG_SPL_SENDSTOP;
	}

	// Ruch w trakcie podtrzymania z runa w rece (runda 4). oCAIHuman::_WalkCycle (0x69AE51) wola co klatke
	// PC_ActionMove(GetState(GAME_ACTION)) (0x699F60), a ten w trybie magii oCAIHuman::MagicMode (0x472FD0, wywolanie
	// 0x69A0CA): przy wcisnietej akcji MagicInvestSpell (0x472160 - Invest co klatke) i zwrot 1, wiec _WalkCycle pomija
	// caly ruch (0x69AE62). W podtrzymaniu przy trzymanej akcji MagicMode nie jest wolany (zwrot 0 - jak zQuickSign przy
	// ladowaniu spod F): ruch dziala, czar czeka w statusie 8, klawisz czyta plugin (zamiast taktow Invest). Puszczenie =
	// zwykly MagicMode: MagicCheckSpellStates (0x4733BF) wykrywa puszczenie -> SetReleaseStatus -> koniec (SENDSTOP).
	// Sciezka zQuickSign (miecz pod F) przez MagicMode nie przechodzi - tam ruch dziala zawsze.
	// Runda 5: hak na WYWOLANIU 0x69A0CA (jak zQuickSign), nie na poczatku funkcji - Gothic Free Aim zaklada na poczatek
	// MagicMode (0x472FD0, 7 bajtow) wlasny hak LeGo HookEngineF; dwa haki w tych samych bajtach (Union + LeGo) moglyby
	// popsuc gre. Cel wywolania czytamy z rel32 (moze juz prowadzic do zQuickSign) i wolamy dalej - lancuch.
	typedef int(__fastcall* SgMagicMode_t)(oCAIHuman*, void*);
	static SgMagicMode_t sgMagicModeOrig = nullptr;
	static const DWORD SG_CALL_MAGICMODE = 0x0069A0CA;   // oCAIHuman::PC_ActionMove: call oCAIHuman::MagicMode

	static int __fastcall SgMagicModeHook(oCAIHuman* self, void* edx);

	static void SgPatchMagicModeCall() {
		if (sgMagicModeOrig || ENGINE != Engine_G2A)
			return;
		BYTE* p = (BYTE*)SG_CALL_MAGICMODE;
		static const BYTE ctx[] = { 0x7F, 0x0F, 0x8B, 0x44, 0x24, 0x0C };   // 0x69A0B9 (jak zQuickSign IsSupportedExe)
		if (p[0] != 0xE8 || memcmp((void*)0x0069A0B9, ctx, sizeof(ctx)) != 0) {
			ZLog("Znaki: nieznane bajty wywolania MagicMode (0x%08X) - bez ruchu w podtrzymaniu z runa w rece", SG_CALL_MAGICMODE);
			return;
		}
		DWORD oldProtect;
		if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
			return;
		sgMagicModeOrig = (SgMagicMode_t)(SG_CALL_MAGICMODE + 5 + *(int*)(p + 1));
		*(int*)(p + 1) = (int)((DWORD)&SgMagicModeHook - (SG_CALL_MAGICMODE + 5));
		VirtualProtect(p, 5, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), p, 5);
		ZLog("Znaki: hak wywolania MagicMode 0x%08X (dalej 0x%08X%s)", SG_CALL_MAGICMODE, (DWORD)sgMagicModeOrig,
			(DWORD)sgMagicModeOrig == 0x00472FD0 ? " - MagicMode" : " - inny plugin, np. zQuickSign");
	}

	static int __fastcall SgMagicModeHook(oCAIHuman* self, void* edx) {
		if (sgSustain.active && !sgSustain.console && player && self && self->npc == player && zinput) {
			bool held = zinput->GetState(GAME_ACTION) != 0.0f;
			if (zdcore::ChannelSkipMagicMode(true, true, held)) {
				if (!sgSustain.magicPath)
					ZDebug("Znak %s: podtrzymanie z runa w rece - ruch wlaczony (MagicMode pominiety do puszczenia akcji)",
						zdcore::SignName(sgSustain.sign));
				sgSustain.magicPath = true;
				sgSustain.sinceInvest = 0.0f;
				return 0;
			}
		}
		return sgMagicModeOrig(self, edx);
	}

	// Bohater w miejscu na czas gestu "w ziemie" (23.10, tryb alternatywny Aard / Yrden spod F, SignAltInPlace): zQuickSign gra
	// gest calym cialem, a blokada ruchu oCNpc::SetMovLock (oCAIHuman::Moving 0x69BA09 wola wtedy tylko _Stand, bieg wygasa,
	// ruch z animacji nie przesuwa voba; uniki i przeskoki nie startuja - sprawdzaja IsMovLock) trzyma go do konca gestu.
	// Logika w zdcore::StillHold: zakladamy tylko wolna blokade, zdejmujemy tylko wlasna (dialog, medytacja, finisher - nie
	// ruszamy). Blokada na konkretnym NPC (AddRef) - przemiana bohatera w trakcie nie zostawi zablokowanego czlowieka.
	// Uwaga: przy blokadzie silnik nie wola oCAIHuman::_WalkCycle, wiec zQuickSign (hak w PC_ActionMove) stoi do jej konca -
	// stad skutek liczy zDismember (SgPending.delay), a nie odlozony rzut zQuickSign.
	static zdcore::StillHold sgStill;
	static oCNpc* sgStillNpc = nullptr;   // AddRef
	static int sgStillSign = -1;

	static void SgStillApply(zdcore::StillAction a, const char* why) {
		if (a == zdcore::STILL_LOCK && sgStillNpc)
			sgStillNpc->SetMovLock(TRUE);
		else if (a == zdcore::STILL_UNLOCK && sgStillNpc)
			sgStillNpc->SetMovLock(FALSE);
		if (!sgStill.active && sgStillNpc) {
			ZDebug("Znak %s: bohater w miejscu - koniec (%s)%s", sgStillSign >= 0 ? zdcore::SignName(sgStillSign) : "?", why,
				a == zdcore::STILL_UNLOCK ? ", blokada ruchu zdjeta" : ", blokady nie ruszamy (nie nasza / juz zdjeta)");
			sgStillNpc->Release();
			sgStillNpc = nullptr;
			sgStillSign = -1;
		}
	}

	static void SgStillStart(float seconds, int sign) {
		if (!player || !player->homeWorld || seconds <= 0.0f)
			return;
		if (sgStill.active && sgStillNpc != player)
			SgStillApply(sgStill.End(sgStillNpc && sgStillNpc->IsMovLock(), DirectorActive()), "inny bohater");
		if (!sgStillNpc) {
			sgStillNpc = player;
			sgStillNpc->AddRef();
		}
		sgStillSign = sign;
		bool locked = player->IsMovLock() != 0;
		zdcore::StillAction a = sgStill.Start(seconds, locked);
		SgStillApply(a, "start");
		ZDebug("Znak %s: bohater w miejscu %.2f s%s", zdcore::SignName(sign), sgStill.left,
			a == zdcore::STILL_LOCK ? " (blokada ruchu)" : locked ? " - blokada ruchu juz trzymana przez kogos innego, nie ruszamy" :
			" (wydluzone)");
	}

	static void SgStillUpdate(float dt) {
		if (!sgStill.active)
			return;
		oCNpc* n = sgStillNpc;
		if (!n || n != player || !n->homeWorld || n->attribute[NPC_ATR_HITPOINTS] <= 0) {
			SgStillApply(sgStill.End(n && n->IsMovLock(), DirectorActive()), n != player ? "inny bohater" : "smierc / brak swiata");
			return;
		}
		SgStillApply(sgStill.Step(dt, n->IsMovLock() != 0, DirectorActive()), "koniec gestu");
	}

	static void SgStillEnd(const char* why) {
		if (sgStill.active)
			SgStillApply(sgStill.End(sgStillNpc && sgStillNpc->IsMovLock(), DirectorActive()), why);
	}

	// Efekt dloni czaru (SPELLFX_<ZNAK>, KEY_CAST z dzwiekiem sfxIsAmbient = 1 - w petli az do konca efektu): Spell_Cast
	// oddaje go swiatu (oCSpell::effect = 0). Efekt FIXED z celem (Axii - TARGET_COLLECT_FOCUS) sam sie nie konczy, wiec
	// dzwiek rzutu moglby grac bez konca. Plugin bierze ref przed rzutem i po SG_HAND_FX_TIME gasi go razem z dziecmi.
	struct SgHandFx {
		oCVisualFX* fx;   // AddRef
		float left;
	};
	static std::vector<SgHandFx> sgHandFx;
	static const float SG_HAND_FX_TIME = 3.0f;   // dzwieki rzutu w SPELLFX_<ZNAK>_FLASH maja do 2,4 s (agent VFX)

	static void SgHandFxUpdate(float dt) {
		for (size_t i = 0; i < sgHandFx.size();) {
			SgHandFx& h = sgHandFx[i];
			h.left -= dt;
			if (h.left > 0.0f) {
				i++;
				continue;
			}
			oCVisualFX* fx = h.fx;
			sgHandFx.erase(sgHandFx.begin() + i);
			if (!fx->IsFinished())
				ZDebug("Znaki: efekt dloni po rzucie nadal trwa po %.1f s - zatrzymany (z dzwiekiem)", SG_HAND_FX_TIME);
			SgFxStop(fx);
		}
	}

	static void SgHandFxClear() {
		while (!sgHandFx.empty()) {
			oCVisualFX* fx = sgHandFx.back().fx;
			sgHandFx.pop_back();
			SgFxStop(fx);
		}
	}

	static int __fastcall SgSpellCast(oCSpell* self, void* edx);
	static CInvoke<int(__fastcall*)(oCSpell*, void*)> Ivk_oCSpell_Cast(
		ENGINE == Engine_G2A ? 0x00485360 : 0, &SgSpellCast, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static int __fastcall SgSpellCast(oCSpell* self, void* edx) {
		ZHookHit(ZH_SPELL_CAST);
		int sign = SgSignOf(self);
		oCNpc* target = sign >= 0 ? self->spellTargetNpc : nullptr;   // przed oryginalem (cel z fokusu, Axii)
		oCVisualFX* hand = sign >= 0 ? self->effect : nullptr;        // przed oryginalem (Cast oddaje go swiatu)
		if (hand)
			hand->AddRef();
		int r = Ivk_oCSpell_Cast(self, edx);
		if (hand) {
			if (r)
				sgHandFx.push_back({ hand, SG_HAND_FX_TIME });
			else
				hand->Release();
		}
		if (sign < 0 || !r || !zcfg.enabled || !zcfg.signs)
			return r;
		ZPROF_SCOPE("Signs.Cast");
		bool alt = sgCast.spell == self && sgCast.alt;
		sgCast.alt = false;
		int cost = alt ? SgAltCost(sign) : zcfg.signManaCost[sign];
		int mana = player->attribute[NPC_ATR_MANA];
		player->attribute[NPC_ATR_MANA] = mana > cost ? mana - cost : 0;
		sgCooldown.Start(sign, zcfg.signCooldown);
		if (target)
			target->AddRef();
		SgPending pend = { sign, target, false, alt };
		SgCaptureAim(pend.aim);   // celowanie z chwili rzutu (GFA w trybie magii / celownik F w zQuickSign)
		if (alt) {
			// gest "w ziemie" zQuickSign (Aard / Yrden spod F): skutek w chwili dotkniecia ziemi, bohater w miejscu do konca
			int qc, ql, qa;
			int qr = SgQsGesture(&qc, &ql, &qa);
			zdcore::AltInPlacePlan plan = zdcore::PlanAltInPlace(zcfg.signAltInPlace, true, qr, qa, qc, ql);
			if (plan.use) {
				pend.delay = plan.delay;
				SgStillStart(plan.hold, sign);
			}
			ZDebug("Znak %s: tryb alternatywny - gest zQuickSign %s%s", zdcore::SignName(sign),
				qr < 0 ? "nieznany (brak eksportu ZQS_SignGesture)" : qr == 0 ? "brak (zwykla animacja rzutu)" :
				qa ? "\"w ziemie\" calym cialem" : "trybu zwyklego",
				plan.use ? "" : zcfg.signAltInPlace ? " - skutek od razu" : " - skutek od razu (SignAltInPlace=0)");
			if (plan.use)
				ZDebug("Znak %s: tryb alternatywny w miejscu - skutek za %.2f s (dotkniecie ziemi), bohater w miejscu %.2f s",
					zdcore::SignName(sign), plan.delay, plan.hold);
		}
		sgPending.push_back(pend);
		ZDebug("Znak %s: Cast%s (tryb broni %d), mana %d -> %d, cel czaru %s", zdcore::SignName(sign), alt ? " ALT" : "",
			player->fmode, mana, player->attribute[NPC_ATR_MANA], SgNpcName(target));
		return r;
	}

	// zQuickSign (celownik od wcisniecia F) co klatke celowania pyta o stan trzymania Znaku - zrodlem prawdy o czasie
	// trzymania i progu SignAltHold jest SignHold tutaj. spell = wybrany czar z ksiegi bohatera (oCSpell*). Wynik 0 = to nie
	// Znak bohatera (albo znaki wylaczone) - bez paska, celownik wedlug zQuickSign. 1 = Znak: frac = ladowanie trybu
	// alternatywnego 0..1, alt = juz tryb alternatywny, wantsAim = celownik ma sens (zdcore::SignAimsAt), showBar = pasek
	// ladowania (zdcore::SignHoldHud). Tylko odczyt, z petli gry (zQuickSign wola z oCAIHuman::_WalkCycle bohatera).
	// Jeden eksport w DLL - definicja tylko w przebiegu G2A (Sources.h jest dolaczany raz na silnik).
#if ENGINE == Engine_G2A
	extern "C" __declspec(dllexport) int __cdecl ZDISM_SignHoldState(void* spell, float* frac, int* alt, int* wantsAim, int* showBar) {
		oCSpell* s = (oCSpell*)spell;
		int sign = s && zcfg.enabled && zcfg.signs ? SgSignOf(s) : -1;
		if (sign < 0)
			return 0;
		bool mine = sgCast.spell == s;
		zdcore::SignAimHud h = zdcore::SignHoldHud(mine ? sgCast.hold.phase : (int)zdcore::HOLD_NONE, mine ? sgCast.hold.held : 0.0f,
			zcfg.signAltHold, sign);
		if (frac)
			*frac = h.frac;
		if (alt)
			*alt = h.alt ? 1 : 0;
		if (wantsAim)
			*wantsAim = h.wantsAim ? 1 : 0;
		if (showBar)
			*showBar = h.showBar ? 1 : 0;
		return 1;
	}
#endif

	// ---------- interfejs ----------

	void SignsPatchSlots() {
		if (!parser)
			return;
		static const char* const NAMES[zdcore::SIGN_COUNT] = { "AARD", "IGNI", "AXII", "YRDEN", "QUEN" };
		// Wspolny helper slotow (_shared\ZSpellSlots.h, jak zTimeRewind): ID ponad rozmiar tablic skryptow (New Balance:
		// MAX_SPELL = 196, znaki 196..200) wydluza SPELLFXINSTANCENAMES / SPELLFXANILETTERS / TXT_SPELLS w pamieci.
		// Steam (100 slotow, znaki 90..94): nic nie jest przydzielane, wpis jak dawniej (SetValue w slocie).
		int done = 0;
		for (int i = 0; i < zdcore::SIGN_COUNT; i++) {
			int id = zcfg.signSpellIds[i];
			std::string label = std::string("Znak ") + zdcore::SignName(i);
			std::vector<std::string> log;
			bool ok = zslots::SetSpellSlot(id, NAMES[i], zcfg.signAniLetters[i].c_str(), label.c_str(), log);
			for (auto& l : log)
				if (!ok || l.find("UWAGA") != std::string::npos || l.find("->") != std::string::npos)
					ZLog("Znaki: %s", l.c_str());   // wydluzenie tablic, nadpisany cudzy slot, odmowa - reszta tylko w debug
				else
					ZDebug("Znaki: %s", l.c_str());
			if (ok)
				done++;
			else
				ZLog("Znaki: slot %d (%s) niedostepny w tablicach parsera - znak nie zadziala", id, zdcore::SignName(i));
		}
		sgStateFunc = parser->GetIndex(zSTRING("ZDF_SIGN_STARTSTATE"));
		ZLog("Znaki: sloty %d,%d,%d,%d,%d wpisane (%d/5), litery %s/%s/%s/%s/%s, wlaczone %d, stany AI w skryptach %s, "
			"marionetka w skryptach %s, przytrzymanie %.2f s",
			zcfg.signSpellIds[0], zcfg.signSpellIds[1], zcfg.signSpellIds[2], zcfg.signSpellIds[3], zcfg.signSpellIds[4], done,
			zcfg.signAniLetters[0].c_str(), zcfg.signAniLetters[1].c_str(), zcfg.signAniLetters[2].c_str(),
			zcfg.signAniLetters[3].c_str(), zcfg.signAniLetters[4].c_str(), zcfg.signs, sgStateFunc >= 0 ? "tak" : "NIE",
			parser->GetIndex(zSTRING("ZDF_SIGN_PUPPETATTACK")) >= 0 ? "tak" : "NIE", zcfg.signAltHold);
	}

	void SignsInit() {
		auto prev = zdOnBeforeHit;
		zdOnBeforeHit = [prev](oCNpc* victim, oCNpc::oSDamageDescriptor& desc) -> bool {
			if (prev && prev(victim, desc))
				return true;   // unik / finisher - Quen zostaje cala
			ZPROF_SCOPE("Signs.Hit");
			return SgBeforeHit(victim, desc);
		};
		SgPatchMagicModeCall();
	}

	void SignsUpdate(float dt) {
		if (!sgTempoPending.empty())
			SgTempoPendingUpdate(dt);   // takze przy wylaczonych znakach - tempo odlozone przy cofaniu czasu
		if (!zcfg.signs) {
			if (sgZone.active || sgTrap.active || sgQuen.active || sgQuenFx || !sgHolds.empty() || !sgPushes.empty() ||
				!sgPending.empty() || !sgFlies.empty() || sgSustain.active || sgPuppet.npc || !sgBolts.empty() || !sgHandFx.empty() ||
				sgThief.Size() || sgStill.active)
				SignsClearAll();
			if (sgThiefDirty)
				SgThiefSync();   // tablice premii kieszonkowej w skrypcie wyzerowane
			return;
		}
		sgCooldown.Update(dt);
		SgThiefUpdate(dt);
		if (!player)
			return;
		if (!sgPending.empty()) {
			std::vector<SgPending> todo, wait;
			todo.swap(sgPending);
			for (auto& p : todo) {
				if (!zdcore::PendingStep(p.delay, dt)) {
					wait.push_back(p);   // gest "w ziemie" - dlon jeszcze nie w ziemi (referencje przechodza z kopia)
					continue;
				}
				SgDoCast(p.sign, p.target, p.console, p.alt, &p.aim);
				if (p.target)
					p.target->Release();
				SgAimRelease(p.aim);
			}
			for (auto& p : sgPending)
				wait.push_back(p);   // dodane w trakcie skutkow - za odlozonymi
			sgPending.swap(wait);
		}
		SgStillUpdate(dt);
		SgSustainUpdate(dt);
		SgChannelStopUpdate();   // klip konca podtrzymania (koniec w hakach Invest / SetReleaseStatus albo wyzej)
		SgHoldsUpdate(dt);
		SgFlyUpdate(dt);
		SgPushUpdate(dt);
		SgYrdenUpdate(dt);
		SgBoltsUpdate(dt);
		SgQuenUpdate(dt);
		SgPuppetUpdate(dt);
		SgHandFxUpdate(dt);
		// petle trzymane przez plugin: bezpiecznik skryptu (emFXLifeSpan) liczy od zera - gasi je tylko plugin (SgFxRefresh)
		SgFxRefresh(sgZoneFx);
		SgFxRefresh(sgTrap.fx);
		for (auto& o : sgTrap.orbs)
			SgFxRefresh(o.fx);
		SgFxRefresh(sgQuenFx);
		for (auto& h : sgHolds)
			SgFxRefresh(h.fx);
		for (auto& s : sgSlowed)
			SgFxRefresh(s.fx);
	}

	// Stan znaku w zapisie: oszolomienie / palenie konczy sie od razu (oCNpc_States::EndCurrentState 0x76D880: faza konca
	// + DoAIState, przy pustej kolejce) - nazwa ZS_ZDF_* nie trafia do pliku (bez zDismember stan bylby nieznany).
	static void SgHoldsEndForSave() {
		while (!sgHolds.empty()) {
			SgHold& h = sgHolds.back();
			if (SgAlive(h.npc) && SgInSignState(h.npc)) {
				h.npc->ClearEM();
				h.npc->state.EndCurrentState();
			}
			SgHoldDrop(sgHolds.size() - 1, "zapis gry");
		}
	}

	void SignsBeforeSave() {
		SgStillEnd("zapis gry");            // blokada ruchu z gestu "w ziemie" nie trafia do zapisu
		SgSustainEnd("zapis gry", false);   // tempo bohatera wraca, efekt zdjety
		SgStopReqClear();
		for (auto& s : sgSlowed)
			SgSlowRestore(s);   // tempo i ochrona do pliku oryginalne
		SgFxStop(sgZoneFx);
		SgFxStop(sgTrap.fx);
		SgTrapOrbsStop();   // ladunki zostaja (kule wracaja po zapisie)
		SgFxStop(sgQuenFx);
		SgHoldsEndForSave();
		SgThiefZero();   // premia kieszonkowa Axii nie trafia do zapisu (po zapisie / wczytaniu wpisywana od nowa)
		// marionetka: postawa i AIV_PARTYMEMBER do pliku oryginalne, stan ZS_ZDF_Puppet zakonczony (po zapisie znow
		// marionetka - plan w najblizszym skanie); efekt zostaje (dontWriteIntoArchive - nie trafia do zapisu), bez
		// tworzenia od nowa, ktore gralo od nowa dzwiek przejecia
		SgPuppetRestore();
		if (sgPuppet.npc)
			SgPuppetLeaveFollow(sgPuppet.npc);
		for (auto& p : sgPushes)
			p.npc->Release();
		sgPushes.clear();
		for (auto& f : sgFlies)
			f.npc->Release();
		sgFlies.clear();
		if (!sgSlowed.empty() || sgZone.active || sgTrap.active || sgQuen.active || sgPuppet.npc)
			ZDebug("Znaki: zapis - cofnieto Yrden (%d NPC), marionetke, efekty strefy / pulapki / tarczy, stany znakow zakonczone",
				(int)sgSlowed.size());
	}

	void SignsAfterSave() {
		sgThiefDirty = true;
		if (!player || !player->homeWorld)
			return;
		for (auto& s : sgSlowed)
			if (SgAlive(s.npc) && s.npc->GetModel() == s.model)
				SgSlowApply(s);   // strefa trwa - nastepny skan wypusci tych, co wyszli
		if (sgZone.active && !sgZoneFx)
			sgZoneFx = SgFxAt("SPELLFX_YRDEN_ZONE", sgZoneGround, true);   // ten sam punkt na ziemi co przy rzucie
		if (sgTrap.active && !sgTrap.fx)
			sgTrap.fx = SgFxAt("SPELLFX_YRDEN_TRAP", sgTrap.ground, true);
		if (sgTrap.active)
			SgTrapOrbsSpawn();
		SgQuenFxGuard();   // tarcza trwa - efekt od nowa przy bohaterze
		if (sgPuppet.npc && SgAlive(sgPuppet.npc)) {
			SgPuppetApply();
			sgPuppet.scan = 0.0f;   // atak / do bohatera od razu w najblizszej klatce
		}
	}

	void SignsClearAll() {
		for (auto& p : sgPending) {
			if (p.target)
				p.target->Release();
			SgAimRelease(p.aim);
		}
		sgPending.clear();
		SgAimRelease(sgAimNow);
		SgStillEnd("czyszczenie");
		SgSustainEnd("czyszczenie", false);
		SgStopReqClear();
		sgCast = SgCastState();
		while (!sgHolds.empty())
			SgHoldDrop(sgHolds.size() - 1, "czyszczenie");
		for (auto& p : sgPushes)
			p.npc->Release();
		sgPushes.clear();
		for (auto& f : sgFlies)
			f.npc->Release();
		sgFlies.clear();
		SgBoltsClear();
		SgTrapEnd("czyszczenie");
		SgZoneEnd("czyszczenie");
		SgSlowReleaseAll();
		sgQuen.Clear();
		SgFxStop(sgQuenFx);
		SgPuppetEnd("czyszczenie", 0);
		SgHandFxClear();
		sgCooldown.Clear();
		SgFiresForget();   // oryginaly zgaszonych ogni z efektu (zgaszone zostaja; Igni zapali je bez pamieci)
		sgThief.Clear();   // premie kieszonkowe Axii (wskazniki NPC z poprzedniego swiata) - tablice skryptu w nastepnej klatce
		sgThiefDirty = true;
	}

	// zdism fires [m]: ognie w promieniu (domyslnie 10 m) - klasa, nazwa, wizual, stan, grupa, czy Aard / Igni zadziala.
	std::string SignsFiresConsole(const std::string& arg) {
		oCWorld* world = ogame ? ogame->GetGameWorld() : nullptr;
		if (!player || !player->homeWorld || !world || !world->bspTree.bspRoot)
			return "brak swiata / bohatera";
		float m = (float)atof(arg.c_str());
		float r = m > 0.5f && m <= 50.0f ? m * 100.0f : 1000.0f;
		zVEC3 hp = player->GetPositionWorld();
		zTBBox3D box;
		box.mins = zVEC3(hp.n[0] - r, hp.n[1] - SG_FIRE_DY, hp.n[2] - r);
		box.maxs = zVEC3(hp.n[0] + r, hp.n[1] + SG_FIRE_DY, hp.n[2] + r);
		zCArray<zCVob*> found;
		world->bspTree.bspRoot->CollectVobsInBBox3D(found, box);
		std::vector<zCVob*> mobTrees;
		for (int i = 0; i < found.GetNum(); i++)
			if (oCMobFire* fire = zDYNAMIC_CAST<oCMobFire>(found[i]))
				if (fire->fireVobtree)
					mobTrees.push_back(fire->fireVobtree);
		struct Row {
			float dist;
			std::string line, brief;
		};
		std::vector<Row> rows;
		int extras = 0, others = 0;
		for (int i = 0; i < found.GetNum(); i++) {
			zCVob* v = found[i];
			if (!v || !v->homeWorld)
				continue;
			float dist = (v->GetPositionWorld() - hp).Length();
			if (dist > r)
				continue;
			char buf[512], brief[96];
			if (oCMobFire* fire = zDYNAMIC_CAST<oCMobFire>(v)) {
				sprintf_s(buf, "oCMobFire '%s' drzewo %s, %.0f cm: stan %d, %s", fire->GetObjectName().ToChar(),
					fire->fireVobtreeName.ToChar(), dist, fire->state, fire->fireVobtree ? "plonie" : "zgaszony");
				sprintf_s(brief, "mob %s %.0fcm %s", fire->fireVobtreeName.ToChar(), dist, fire->fireVobtree ? "plonie" : "zgaszony");
				rows.push_back({ dist, buf, brief });
				continue;
			}
			std::string vis = SgFxVisualName(v);
			if (vis.empty())
				continue;
			zdcore::FireFlame f = SgFlameInfo(v, vis, mobTrees);
			if (f.kind == zdcore::FIREVIS_EXTRA) {
				extras++;
				continue;
			}
			if (f.kind != zdcore::FIREVIS_FLAME) {
				others++;
				if (f.kind == zdcore::FIREVIS_NONE)
					continue;
			}
			int lightsOn = 0, lightsOff = 0, lightsStatic = 0, sounds = 0, soundsOff = 0, members = 0;
			if (f.kind == zdcore::FIREVIS_FLAME)
				for (auto& mm : SgFireMembers(v, found, mobTrees, false)) {
					if (mm.type == zdcore::FMEM_LIGHT) {
						zCVobLight* l = static_cast<zCVobLight*>(mm.vob);
						if (mm.skip == zdcore::FMSKIP_STATIC_LIGHT)
							lightsStatic++;
						else if (mm.skip == zdcore::FMSKIP_OK || mm.skip == zdcore::FMSKIP_STATE)
							(l->lightData.isTurnedOn ? lightsOn : lightsOff)++;
					}
					else if (mm.type == zdcore::FMEM_SOUND && (mm.skip == zdcore::FMSKIP_OK || mm.skip == zdcore::FMSKIP_STATE))
						(static_cast<zCVobSound*>(mm.vob)->soundAllowedToRun ? sounds : soundsOff)++;
					else if (mm.type == zdcore::FMEM_VISUAL && (mm.skip == zdcore::FMSKIP_OK || mm.skip == zdcore::FMSKIP_STATE))
						members++;
				}
			zCVob* holder = SgFireHolder(v);
			bool listed = SgFireOffFind(v) != sgFiresOff.end();
			sprintf_s(buf, "%s '%s' %s (%s), %.0f cm, dy %.0f: %s%s, uchwyt %s; grupa: dodatki %d, swiatla dyn. %d wl./%d wyl., "
				"statyczne %d, dzwieki %d wl./%d wyl.; Aard: %s, Igni: %s", v->_GetClassDef()->className.ToChar(),
				v->GetObjectName().ToChar(), vis.c_str(), zdcore::FireVisKindName(f.kind), dist, v->GetPositionWorld().n[1] - hp.n[1],
				f.shown ? "widoczny" : "ukryty", listed ? " (na liscie zgaszonych)" : "",
				holder ? holder->_GetClassDef()->className.ToChar() : "-", members, lightsOn, lightsOff, lightsStatic, sounds,
				soundsOff, zdcore::FireSkipName(zdcore::FireFlameCheck(f, false)), zdcore::FireSkipName(zdcore::FireFlameCheck(f, true)));
			sprintf_s(brief, "%s %.0fcm %s", vis.c_str(), dist, f.kind != zdcore::FIREVIS_FLAME ? "efekt" : f.shown ? "pali" : "zgaszony");
			rows.push_back({ dist, buf, brief });
		}
		std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.dist < b.dist; });
		ZLog("Ognie (zdism fires): promien %.0f cm, ognie %d, dym/iskry/poswiata %d, inne PFX %d, lista zgaszonych %d vobow, "
			"SignFires=%d", r, (int)rows.size(), extras, others, (int)sgFiresOff.size(), zcfg.signFires);
		for (auto& row : rows)
			ZLog("  %s", row.line.c_str());
		char head[160];
		sprintf_s(head, "ognie w %.0f m: %d (dodatki %d, zgaszone na liscie %d)", r / 100.0f, (int)rows.size(), extras,
			(int)sgFiresOff.size());
		std::string out = head;
		for (size_t i = 0; i < rows.size() && i < 6; i++)
			out += " | " + rows[i].brief;
		if (rows.size() > 6)
			out += " | ... (zDismember.log)";
		return out;
	}

	std::string SignsConsole(const std::string& arg) {
		std::vector<std::string> args = zdcore::SplitArgs(arg);
		std::string a = args.empty() ? std::string() : args[0];
		bool alt = args.size() > 1 && args[1] == "ALT";
		if (a.empty() || a == "INFO") {
			char buf[640];
			std::string held;
			for (auto& h : sgHolds) {
				char line[120];
				sprintf_s(line, " %s:%s %.1fs%s", SgNpcName(h.npc), zdcore::StunName(h.variant), h.left,
					h.active ? "" : " (start)");
				held += line;
			}
			sprintf_s(buf, "znaki %s, moc x%.2f | Quen %s %.0fs traf. %d efekt %d | Yrden %s %.0fs, pulapka %s lad. %d, "
				"spowolnieni %d | stany %d%s | podtrzymanie %s | marionetka %s %.0fs | odnowienie A%.1f I%.1f X%.1f Y%.1f Q%.1f"
				" | w miejscu %.1fs%s, skutki w kolejce %d",
				zcfg.signs ? "wl." : "wyl.", player ? SgPowerNow() : 1.0f, sgQuen.active ? "tak" : "nie", sgQuen.left, sgQuen.hitsLeft,
				sgQuenFx != nullptr, sgZone.active ? "tak" : "nie", sgZone.left, sgTrap.active ? "tak" : "nie", sgTrap.st.charges,
				(int)sgSlowed.size(), (int)sgHolds.size(), held.c_str(),
				sgSustain.active ? zdcore::SignName(sgSustain.sign) : "-", SgNpcName(sgPuppet.npc), sgPuppet.left,
				sgCooldown.Left(0), sgCooldown.Left(1), sgCooldown.Left(2), sgCooldown.Left(3), sgCooldown.Left(4),
				sgStill.active ? sgStill.left : 0.0f, sgStill.owned ? " (blokada nasza)" : "", (int)sgPending.size());
			ZLog("%s", buf);
			return buf;
		}
		int sign = zdcore::SignFromName(a);
		if (sign < 0)
			return "zdism sign <aard|igni|axii|yrden|quen> [alt] | zdism sign info";
		if (!zcfg.enabled || !zcfg.signs)
			return "znaki wylaczone (Signs=0)";
		if (!player || !player->homeWorld)
			return "brak bohatera";
		oCNpc* focus = zDYNAMIC_CAST<oCNpc>(player->GetFocusVob());
		SgDoCast(sign, focus, true, alt);   // bez many i odnowienia, bez efektu dloni; podtrzymywane na 3 s
		return std::string("Znak ") + zdcore::SignName(sign) + (alt ? " - tryb alternatywny" : "") + " (test, log Debug=1)";
	}
}
