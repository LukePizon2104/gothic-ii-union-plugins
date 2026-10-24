// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Nagrany aktor. Rejestr trzyma referencje (AddRef) - usuniety ze swiata obiekt nie zostawia wiszacego wskaznika.
	struct TrActor {
		oCNpc* npc = nullptr;
		trcore::Track track;
		trcore::StepTrack<trcore::CombatSnap> combat;            // relacje i walka (Relations.cpp) - tylko zmiany
		trcore::StepTrack<std::vector<trcore::NewsKey>> news;    // pamiec newsow (zwykle pusta - bez wpisow)
		trcore::StepTrack<uint32_t> cuts;                        // maska odciec zDismember (Cuts.cpp) - tylko zmiany;
		                                                         // pusty bez zDismember
		trcore::CutView cutView;                                 // podglad odciec w jednym przewijaniu (Cuts.cpp)
	};

	// Zegar pluginu (s): rosnie tylko przy nagrywaniu; po cofnieciu = punkt cofniecia.
	float TrNow();
	// Czysci bufor (wczytanie gry, zmiana swiata, nowa gra).
	void RecorderReset();
	// Co SampleInterval czasu gry: probki postaci w swiecie (z bohaterem; zdespawnowane bez probki) i zegara swiata.
	void RecorderUpdate(float dt);
	// Najstarszy dostepny punkt czasu i ile sekund wstecz jest w buforze.
	float RecorderOldest();
	float RecorderSeconds();
	std::vector<TrActor*> RecorderActors();
	trcore::ClockTrack& RecorderClock();
	// Po Commit: kasuje przyszlosc wszystkich torow, zegar pluginu = t.
	void RecorderCutAfter(float t);
	std::string RecorderStats();
	// Probka postaci bez oCNpc::GetModel (ten buduje od nowa model zwolniony przez silnik - 10-20 ms).
	// Bez modelu: sama pozycja (aniId -1), hasModel = false.
	bool TrSampleNpc(oCNpc* npc, float t, trcore::Sample& out, bool* hasModel = nullptr);
}
