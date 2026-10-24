// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Relacje i stan walki postaci w nagraniu (wrogosc po ciosie). Logika decyzji w trcore (Core\Combat.h).
	// Zawsze: attitude / tmp_attitude, npcType, enemy, stan AI (AI od nowa przy walce, ktorej wtedy nie bylo) i aivary
	// walki (CombatAivars). Przy RewindScope=1 takze aivary przestepstw (CrimeAivars), newsy (oCNewsMemory) i
	// postawy gildii (oCGuilds - w migawce stanu, StateSnap).

	// Listy z ini (CombatStates / CombatAivars / CrimeAivars) do ponownego przeliczenia z symboli parsera
	// (tr cfg, wczytanie gry). Zmiana listy aivarow czysci zapisane tory walki (inny uklad migawek).
	void RelationsInvalidate();
	// Probka w ticku nagrywania (SampleNpcs) - bez GetModel, kilka odczytow pol postaci.
	void TrRecordCombat(TrActor& a, float t);

	// Commit jednej postaci: plan przed ustawieniem polozenia (czy AI od nowa), przywrocenie po nim.
	struct TrCombatCommit {
		trcore::CombatSnap target, now;
		trcore::CombatPlan plan;
		bool nowHostile = false;
	};
	bool TrPlanCombat(TrActor& a, float t, TrCombatCommit& out);
	void TrApplyCombat(TrActor& a, float t, const TrCombatCommit& c);
	// Licznik i podsumowanie (linia w logu) jednego Commit.
	void RelationsCommitBegin();
	void RelationsCommitEnd();

	// Postawy gildii (RewindScope=1, migawka stanu): kopia tablicy oCGuilds / przywrocenie (liczba zmienionych).
	void TrCaptureGuilds(std::vector<unsigned char>& out);
	int TrApplyGuilds(const std::vector<unsigned char>& in);

	// "tr combat" - stan relacji i walki postaci z fokusu (test w grze)
	void RelationsRegisterCommands();
}
