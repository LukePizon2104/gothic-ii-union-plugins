// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Stan niefizyczny (etap 2 / F1): zmienne skryptow, flagi odbytych dialogow, dziennik.
	struct TrStateSnap {
		std::vector<int> globals;             // wartosci zmiennych zapisu (filtr jak SaveGlobalVars), w kolejnosci symboli
		std::vector<unsigned char> log;       // oCLogManager::Archive (BINARY_SAFE)
		std::vector<int> told;                // oCInfo::told w kolejnosci infoList
		std::vector<unsigned char> guilds;    // oCGuilds::attitude (size x size) - Wld_SetGuildAttitude
	};

	// Migawki w nagraniu (definicje w Recorder.cpp; deklaracje tutaj, bo Recorder.h jest przed tym naglowkiem).
	int RecorderStateAtOrBefore(float t);
	float RecorderStateTime(int i);
	const TrStateSnap& RecorderState(int i);

	// Przygotowanie migawki: lista zmiennych zapisu (po wczytaniu gry przeglad ~89 tys. symboli parsera) budowana
	// najwyzej relistBudget symboli na wywolanie (0 = do konca). true = mozna robic migawke.
	bool TrStatePrepare(int relistBudget);
	// Wypelnia out (takze obiekt z puli - nadpisuje wszystkie pola, bufory wykorzystuje ponownie).
	bool TrCaptureState(TrStateSnap& out);
	bool TrApplyState(const TrStateSnap& s);
	// "tr state save|load|info" - test w grze bez cofania
	void StateRegisterCommands();
}
