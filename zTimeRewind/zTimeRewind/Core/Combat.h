// Logika bez silnika: relacje i stan walki postaci - "wrogosc po ciosie" po cofnieciu czasu.
// Co odpowiada za wrogosc w G2A / NB (skrypty B_Attack, B_AssessDamage, ZS_Attack*, ZS_ReactToDamage,
// B_MemorizePlayerCrime): oCNpc::attitude / tmp_attitude (B_SetAttitude, Npc_SetTempAttitude; NB B_Attack
// ustawia ATT_ANGRY na stale), enemy (Npc_SetTarget), aktualny stan AI (ZS_Attack, ZS_ReactToDamage...),
// npcType (NB ZS_ReactToDamage: NPCTYPE_FRIEND -> MAIN) i aivary walki (AIV_ATTACKREASON, AIV_LASTTARGET...)
// oraz przestepstw (AIV_NPCSAWPLAYERCOMMIT...). Lista aivarow z nazw symboli parsera - indeksy z tego samego
// zrodla na Steam i w NB.
#pragma once
#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Timeline.h"

namespace trcore {
	const int COMBAT_AIV_MAX = 32;   // najwyzej tyle aivarow w migawce walki
	const int AIVAR_COUNT = 100;     // C_NPC.aivar[100]

	// Stan relacji i walki jednej postaci w chwili probki.
	struct CombatSnap {
		int attitude = 2, tmpAttitude = 2;   // ATT_NEUTRAL
		float attTimer = 0.0f;               // licznik powrotu tmp -> attitude (CheckAngryTime) - poza porownaniem
		int npcType = 0;
		int state = -1;                      // oCNpc_States::curState.index (symbol ZS_*), -1 = brak
		const void* enemy = nullptr;         // oCNpc::enemy - tylko do porownan i weryfikacji z rejestrem
		int enemyInst = -1;                  // instancja wroga (weryfikacja wskaznika przy przywracaniu)
		int nAiv = 0;
		int aiv[COMBAT_AIV_MAX] = {};        // wartosci aivarow w kolejnosci listy (ResolveAivars)
		// Bez attTimer (silnik zmienia go co klatke, gdy tmp != attitude).
		bool operator==(const CombatSnap& o) const;
	};

	// Stan schodkowy w czasie: zapis tylko zmian (Push rownego ostatniemu nic nie dopisuje).
	template <class T>
	class StepTrack {
	public:
		void Push(float t, const T& v) {
			if (!items.empty() && items.back().second == v)
				return;
			items.emplace_back(t, v);
		}
		// Stan w chwili t (ostatni wpis <= t); false, gdy t przed pierwszym wpisem albo tor pusty.
		bool At(float t, T& out) const {
			for (int i = (int)items.size() - 1; i >= 0; i--)
				if (items[i].first <= t) {
					out = items[i].second;
					return true;
				}
			return false;
		}
		const T& First() const { return items.front().second; }
		const T& Last() const { return items.back().second; }
		// Usuwa wpisy starsze niz t, zostawiajac ostatni <= t jako kotwice.
		void TrimBefore(float t) {
			size_t keep = 0;
			while (keep + 1 < items.size() && items[keep + 1].first <= t)
				keep++;
			if (keep)
				items.erase(items.begin(), items.begin() + keep);
		}
		void CutAfter(float t) {
			while (!items.empty() && items.back().first > t)
				items.pop_back();
		}
		bool Empty() const { return items.empty(); }
		size_t Size() const { return items.size(); }
		void Clear() { items.clear(); }

	private:
		std::vector<std::pair<float, T>> items;
	};

	// Co przywrocic postaci przy Commit.
	struct CombatPlan {
		bool resetAI = false;      // AI od nowa (rutyna): ClearEM + ClearAIState, bron schowana jak w probce
		bool attitude = false;     // attitude / tmp_attitude z migawki
		bool npcType = false;
		bool enemy = false;        // enemy z migawki (SetEnemy - z liczeniem referencji)
		std::vector<int> aivars;   // indeksy wpisow listy aivarow do przywrocenia
		bool Any() const { return resetAI || attitude || npcType || enemy || !aivars.empty(); }
	};

	// target = migawka z chwili docelowej, now = stan przed Commit. *Hostile = stan AI z listy stanow walki
	// (CombatStates). crimeMask[i] != 0: wpis i to przestepstwo - przywracany tylko przy fullScope (RewindScope=1).
	// AI od nowa, gdy postac jest teraz w stanie walki i wtedy byla w innym stanie / z innym wrogiem, albo gdy
	// teraz celuje w bohatera, a wtedy nie (stan spoza listy, np. wlasny ZS moda). Zmiana rutyny bez wrogosci
	// nie rusza AI (AI sie nie cofa - poza sprzed cofniecia).
	CombatPlan DecideCombat(const CombatSnap& target, const CombatSnap& now, bool targetHostile, bool nowHostile,
		const void* hero, const std::vector<unsigned char>& crimeMask, bool fullScope);

	// AI od nowa wymaga ResetPos w punkcie docelowym (ClearEM, animacje, postawa) - SoftLatest staje sie
	// HardTarget. Keep (martwy / nieprzytomny wtedy i teraz) zostaje.
	CommitAction CombatCommitAction(CommitAction action, bool resetAI);

	// Lista nazw z ini: separatory przecinek, srednik, spacja, tabulator; wielkie litery, bez powtorzen.
	std::vector<std::string> ParseNameList(const std::string& text);

	struct AivEntry {
		int index = 0;        // indeks w C_NPC.aivar
		bool crime = false;   // grupa przestepstw (tylko RewindScope=1)
		std::string name;
	};
	// Aivary z nazw stalych skryptu (lookup: nazwa -> wartosc stalej). Najpierw walka, potem przestepstwa;
	// indeks spoza 0..AIVAR_COUNT-1 albo brak symbolu -> unknown; powtorzony indeks pomijany; najwyzej
	// COMBAT_AIV_MAX wpisow.
	std::vector<AivEntry> ResolveAivars(const std::vector<std::string>& combat, const std::vector<std::string>& crime,
		const std::function<bool(const std::string&, int&)>& lookup, std::vector<std::string>* unknown);

	// News w pamieci postaci (oCNewsMemory). oCNews nie ma czasu powstania - nowszy niz chwila docelowa =
	// nieobecny w migawce z tej chwili (adres + news_id + sprawca + ofiara).
	struct NewsKey {
		const void* news = nullptr;
		int id = 0;
		const void* offender = nullptr;
		const void* victim = nullptr;
		bool operator==(const NewsKey& o) const {
			return news == o.news && id == o.id && offender == o.offender && victim == o.victim;
		}
	};
	// Indeksy (w now) newsow do usuniecia.
	std::vector<int> NewsToRemove(const std::vector<NewsKey>& target, const std::vector<NewsKey>& now);

	// ATT_HOSTILE 0, ATT_ANGRY 1, ATT_NEUTRAL 2, ATT_FRIENDLY 3 - do logu.
	const char* AttitudeName(int att);
}
