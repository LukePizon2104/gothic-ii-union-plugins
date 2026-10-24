// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Ustawienia z Gothic.ini [ZTIMEREWIND]; brakujace klucze sa dopisywane z wartosciami domyslnymi.
	struct TrConfig {
		bool enabled = true;
		float bufferSeconds = 30.0f;      // dlugosc bufora (s czasu gry)
		float sampleInterval = 0.1f;      // odstep probek (s czasu gry)
		float rewindSpeed = 3.0f;         // sekundy cofniete na sekunde rzeczywista
		int manaStart = 10;               // koszt rzucenia runy
		float manaPerSecond = 5.0f;       // koszt za sekunde cofnietego czasu
		float minRewindSeconds = 3.0f;    // krotkie klikniecie (albo zQuickSign) cofa co najmniej tyle
		int spellId = 99;                 // slot czaru 0..255 (Steam 99, NB 201) - przy niezgodnosci wygrywa SPL_TimeRewind
		int puppetAnims = 1;              // 0 zastygla poza, 1 klatki przy zatrzymanym czasie modelu, 2 klatki przy biegnacym
		std::string castFx = "SPELLFX_TELEPORT";   // efekt dloni z runa (SPELLFX_TIMEREWIND nie istnieje)
		float hardResetDistance = 50.0f;  // przesuniecie (cm), od ktorego postac dostaje ResetPos + AI od nowa
		int rewindScope = 1;              // 1 = pelne cofniecie (etap 2), 0 = tylko swiat fizyczny (etap 1)
		float stateInterval = 1.0f;       // odstep migawek stanu (s czasu gry)
		// KeepGlobals: zmienne skryptow poza cofaniem (migawka ich nie przywraca) - stan, ktory trzyma takze silnik / DLL
		// moda poza skryptami (NB: tempo bohatera ANIACCELERATION... dodawane i odejmowane przez UNION_ABI, sprint
		// z nakladka Humans_MageSprint, mikstura szybkosci z AIVRX). Cofniecie samej zmiennej rozjezdza pary +=/-=.
		std::string keepGlobals;
		int keepGlobalsGen = 0;           // rosnie przy kazdym Load - StateSnap przelicza znaczniki symboli
		bool autoRewindOnDeath = true;    // smierc bohatera z runa w plecaku i mana = samo cofniecie zamiast smierci
		float autoRewindSeconds = 10.0f;  // o ile cofa auto-cofniecie
		// Relacje i walka (Relations.cpp): attitude / enemy / stan AI / aivary walki zawsze, przestepstwa przy RewindScope=1
		bool restoreCombat = true;        // RestoreCombat
		std::string combatStates;         // CombatStates: stany AI walki/reakcji (ZS_*) - AI od nowa, gdy wtedy ich nie bylo
		std::string combatAivars;         // CombatAivars: aivary walki (nazwy stalych AIV_* ze skryptow)
		std::string crimeAivars;          // CrimeAivars: aivary przestepstw / skutkow walki (tylko RewindScope=1)
		// Zwrot przedmiotow zuzytych po chwili docelowej (Consume.cpp): RestoreScrolls / RestorePotions / RestoreFood / RestoreAmmo
		trcore::RestoreOptions restore;
		// RestoreMana: mana wydana na czary po chwili docelowej wraca (bez runy cofania, do max many)
		bool restoreMana = true;
		bool debug = false;
		bool profiler = true;             // Profiler: lekki pomiar sekcji + przycinki do logu (konsola: tr prof)
		float profSpikeMs = 25.0f;        // ProfSpikeMs: przycinka = klatka dluzsza niz max(tyle, 2 x mediana 120 klatek)
		float profSummarySeconds = 0.0f;  // ProfSummarySeconds: podsumowanie do logu co tyle s (0 = tylko na zadanie)

		// fromMenu: po wyjsciu z menu opcji - pozycje przestawione w menu Union (klucze Menu*) wygrywaja z ini.
		void Load(bool fromMenu = false);
	};

	extern TrConfig trcfg;
}
