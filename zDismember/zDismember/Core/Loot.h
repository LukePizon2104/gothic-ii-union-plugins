// Logika bez silnika: podnoszenie przedmiotow z dobyta bronia (jak w W3, Loot.cpp w pluginie).
// Silnik (G2A): atak gry = oCAIHuman::PC_ActionMove(GetState(GAME_ACTION)) 0x699F60 -> FightMelee 0x696EC0 (bron biala
// i piesci; useGothic1Controls=1: akcja + GetToggled(GAME_UP), =0: GetToggled(GAME_ACTION)). Podniesienie = komunikat
// oCMsgManipulate EV_TAKEVOB (jak oCAIHuman::StandActions 0x698EA0 bez broni) -> oCNpc::EV_TakeVob 0x7534E0: rusza, gdy
// gra postawa _s_walk (tez postawa broni S_1HRUN / S_2HRUN), T_STAND_2_IGET z wysokoscia (item.y - npc.y + 100) * 0,005,
// w S_IGET oCNpc::DoTakeVob 0x7449C0 (ze swiata do ekwipunku, skrypty, percepcja kradziezy), T_IGET_2_STAND -> _s_walk.
#pragma once
#include <vector>

namespace zdcore {
	// Powod odmowy podniesienia (log Debug=1). Kolejnosc = kolejnosc sprawdzania stanu bohatera.
	enum LootDeny {
		LOOT_OK = 0,
		LOOT_OFF,          // LootArmed=0 albo plugin wylaczony
		LOOT_NOT_HUMAN,    // bohater w innym modelu (przemiana)
		LOOT_NOT_MELEE,    // w rece nie bron biala / piesci
		LOOT_WINDOW,       // ekwipunek, konsola, dialog
		LOOT_MOVLOCK,      // blokada sterowania (skrypt, inna wtyczka)
		LOOT_PLUGIN_MOVE,  // unik, przewrot, przeskok, finisher pluginu
		LOOT_ATTACK,       // trwa cios
		LOOT_BUSY,         // kolejka komunikatow bohatera niepusta (atak, dobycie broni...)
		LOOT_BODY,         // nie stoi i nie idzie (skok, upadek, sprint, parada, plywanie...)
		LOOT_WATER,        // w wodzie
		LOOT_ENEMY,        // walczacy wrog w promieniu LootEnemyRadius
		LOOT_NO_ITEM,      // nic do wziecia / otwarcia / przeszukania przed bohaterem
		LOOT_DENY_COUNT
	};
	const char* LootDenyText(LootDeny d);

	struct LootHeroState {
		bool enabled = true;      // zDismember wlaczony i LootArmed=1
		bool human = true;
		bool melee = true;        // piesci, sztylet, bron 1H / 2H dobyte
		bool window = false;
		bool movLock = false;
		bool pluginMove = false;
		bool attacking = false;
		bool busy = false;
		bool bodyOk = true;       // stoi, idzie, biegnie albo skrada sie
		bool water = false;
	};
	// Pierwszy powod odmowy ze stanu bohatera (bez wroga i przedmiotu), LOOT_OK = mozna.
	LootDeny LootHeroDeny(const LootHeroState& s);
	// Odmowa bez znaczenia dla podnoszenia (wylaczone, przemiana, bez broni) - wcisniecie zostaje grze bez wpisu w logu.
	inline bool LootDenySilent(LootDeny d) {
		return d == LOOT_OFF || d == LOOT_NOT_HUMAN || d == LOOT_NOT_MELEE;
	}
	// Decyzja: stan bohatera, potem wrog, potem przedmiot.
	LootDeny LootDecide(LootDeny heroDeny, bool enemyNear, bool hasItem);
	// Odmowa do logu (Debug=1) bez spamu w walce: gdy byl przedmiot do wziecia albo walki nie bylo (sam brak przedmiotu).
	bool LootLogDeny(LootDeny d, bool hadItem, bool enemyNear);

	// NPC w poblizu: walczacy wrog blokuje podnoszenie (wtedy klawisz atakuje).
	struct LootFoe {
		float dist = 0.0f;          // cm od bohatera
		bool alive = true;
		bool conscious = true;
		bool targetsHero = false;   // npc->enemy == bohater
		bool heroTargets = false;   // bohater->enemy == npc
		bool hostile = false;       // nastawienie do bohatera wrogie albo zle (NPC_ATT_HOSTILE / ANGRY)
		bool fighting = false;      // w trybie walki (bron / piesci dobyte)
	};
	// Blokuje: zywy, przytomny, w promieniu i walczacy - celuje w bohatera (wrogi albo z bronia w reku) albo wrogi
	// i w walce / cel bohatera. Neutralny walczacy z kims innym i wrogi spokojny (nie zauwazyl bohatera) - nie.
	bool LootFoeBlocks(const LootFoe& f, float radius);

	struct LootParams {
		float range = 150.0f;      // w poziomie od srodka bohatera (cm)
		float coneDeg = 45.0f;     // +- od przodu bohatera
		float nearAny = 35.0f;     // blizej niz tyle (pod nogami) - kat bez znaczenia
		float minDy = -160.0f;     // wysokosc przedmiotu wzgledem srodka voba bohatera (ok. 90 cm nad ziemia)
		float maxDy = 80.0f;       //   animacja podnoszenia siega od -100 do +100 cm
		float anglePenalty = 60.0f;   // wynik: odleglosc + tyle cm za przedmiot na brzegu stozka
	};
	// Przedmiot w zasiegu: dx, dy, dz = przedmiot - srodek voba bohatera (cm), fwd = przod bohatera (dowolna dlugosc
	// w poziomie). score: mniejszy = lepszy (blizej i blizej osi).
	bool LootItemFits(const LootParams& p, float fwdX, float fwdZ, float dx, float dy, float dz, float& score);

	struct LootItem {
		float dx = 0.0f, dy = 0.0f, dz = 0.0f;
		bool takeable = true;      // ze swiata, nie leci, bez NFOCUS, nie plonaca pochodnia, nie w gniezdzie, uniesie
	};
	// Indeksy przedmiotow, ktore pasuja i da sie je wziac, od najlepszego - silnik sprawdza po kolei widocznosc
	// promieniem i bierze pierwszy widoczny.
	std::vector<int> LootRankItems(const LootParams& p, float fwdX, float fwdZ, const std::vector<LootItem>& items);

	// Klawisz ataku: jedno wcisniecie rozstrzygane raz (jak skok w Traversal) - pierwsza obserwacja wcisnietego klawisza
	// po puszczeniu (z haka wejscia albo z petli gry, kolejnosc dowolna); do puszczenia wynik trzyma: nasze =
	// silnik nie widzi klawisza (bez ciosu w kolejce), gry = bez zmian.
	struct LootKeyLatch {
		int state = 0;             // 0 puszczony / do rozstrzygniecia, 1 nasze, 2 gry
	};
	// true = trzeba teraz rozstrzygnac (potem LootLatchSet). Puszczenie zeruje zatrzask.
	bool LootLatchNeedsDecision(LootKeyLatch& l, bool down);
	void LootLatchSet(LootKeyLatch& l, bool ours);
	bool LootLatchSwallow(const LootKeyLatch& l, bool down);

	// Menu: pozycja MenuLoot (0 wyl., 1 wl., 2 wl. z podpowiedzia) <-> klucze LootArmed i LootHint.
	int LootMenuIndex(bool armed, bool hint);
	// Zla pozycja - bez zmian; 0 zostawia LootHint (wraca z ta sama podpowiedzia po wlaczeniu w ini).
	void LootMenuApply(int index, bool& armed, bool& hint);
}
