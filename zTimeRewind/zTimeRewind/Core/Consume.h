// Logika bez silnika: zwrot przedmiotow zuzytych przez bohatera po chwili docelowej cofniecia (zwoje, mikstury,
// jedzenie, amunicja). Silnik (Consume.cpp) mierzy ekwipunek bohatera (licznik per instancja, razem z
// przedmiotem w rece) przed i po wywolaniu konkretnych sciezek zuzycia - rzut czaru z ksiegi (zwoj), zniszczenie
// przedmiotu w rece po uzyciu (mikstura, jedzenie), natychmiastowe uzycie, strzal z luku / kuszy. Ubytek w takim
// wywolaniu = zuzycie (UseRecord). Sprzedaz, dawanie, wyrzucenie, kradziez i skrypty nie przechodza przez te
// sciezki, wiec nie trafiaja do rejestru wcale. Dodatkowo: wykluczenia kontekstu (JudgeUse), efekt, ktorego
// cofniecie nie usuwa (UseEffect::lasting), i limit ubytku netto od chwili docelowej (PlanRefunds).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace trcore {
	// Constants.d (G2A i NB): C_Item.mainflag / flags
	const int ITEM_KAT_MUN = 1 << 3;
	const int ITEM_KAT_FOOD = 1 << 5;
	const int ITEM_KAT_POTIONS = 1 << 7;
	const int ITEM_KAT_RUNE = 1 << 9;
	const int ITEM_MULTI = 1 << 21;

	enum class ItemKind : unsigned char { Other = 0, Scroll, Potion, Food, Ammo };
	// Kategoria z mainflag; zwoj = ITEM_KAT_RUNE z ITEM_MULTI (runa nie ma MULTI i sie nie zuzywa).
	ItemKind ClassifyItem(int mainflag, int flags);
	const char* ItemKindName(ItemKind k);
	inline unsigned KindBit(ItemKind k) { return 1u << (unsigned)k; }

	// Gothic.ini [ZTIMEREWIND] RestoreScrolls / RestorePotions / RestoreFood / RestoreAmmo
	struct RestoreOptions {
		bool scrolls = true, potions = true, food = true, ammo = true;
		bool Allows(ItemKind k) const;
		bool Any() const { return scrolls || potions || food || ammo; }
	};
	// Pozycja menu Union "Zwrot zuzytych rzeczy" (kolejnosc jak w zTimeRewind_Menu.d).
	enum { RESTORE_MENU_OFF = 0, RESTORE_MENU_SCROLLS = 1, RESTORE_MENU_ALL = 2, RESTORE_MENU_CUSTOM = 3 };
	int RestoreMenuIndex(const RestoreOptions& o);
	// Gotowy zestaw z pozycji menu; "z ini" (CUSTOM) i spoza listy - false, opcje bez zmian.
	bool RestoreFromMenu(int index, RestoreOptions& o);

	// Liczniki przedmiotow sledzonych kategorii: (instancja, ilosc), posortowane po instancji, tylko ilosc > 0.
	struct InvCounts {
		std::vector<std::pair<int, int>> items;
		void Add(int instance, int amount);
		int Get(int instance) const;
		bool operator==(const InvCounts& o) const { return items == o.items; }
	};
	// Ubytki before -> after: (instancja, ile ubylo), po instancji.
	std::vector<std::pair<int, int>> CountDecreases(const InvCounts& before, const InvCounts& after);

	// Stan bohatera, ktory moze zmienic zuzycie. HP i mana (biezace) cofa Commit (HP z probki, mana - zwrot);
	// reszta (atrybuty max, sila, zrecznosc, ochrony, poziom, doswiadczenie, punkty nauki, nakladki animacji
	// - np. mikstura szybkosci, liczba postaci w swiecie - przyzwanie) zostaje po cofnieciu = efekt trwaly.
	enum {
		HS_HP = 0, HS_MANA = 2,           // attribute[NPC_ATR_*] na 0..7
		HS_PROT = 8,                      // protection[8] na 8..15
		HS_LEVEL = 16, HS_EXP = 17, HS_LP = 18, HS_TIMED_OVERLAYS = 19, HS_OVERLAYS = 20, HS_NPCS = 21,
		HS_COUNT = 22
	};
	struct HeroStats {
		int v[HS_COUNT] = {};
	};
	struct UseEffect {
		int manaGain = 0;    // przyrost many (mikstura many: + ; zwoj placony mana: -)
		int hpGain = 0;
		bool lasting = false;
		void Add(const HeroStats& before, const HeroStats& after);
		void Merge(const UseEffect& o);
	};

	enum class UseSkip : unsigned char { Keep = 0, Rewinding, Cutscene, Dialog, Mob, InWorld, Lasting };
	const char* UseSkipName(UseSkip s);
	struct UseContext {
		bool rewinding = false;   // przewijanie trwa
		bool cutscene = false;
		bool dialog = false;      // rozmowa: handel, dawanie, skrypty dialogow (alchemia, gotowanie przez dialog)
		bool mob = false;         // interakcja z mobem: przedmiot w rece przerabiany (patelnia, kowadlo)
		bool inWorld = false;     // przedmiot tej instancji wyladowal w swiecie - wyrzucony / upuszczony, nie zuzyty
	};
	UseSkip JudgeUse(const UseContext& c, const UseEffect& e);

	struct UseRecord {
		float t = 0.0f;               // zegar pluginu (TrNow) - ten sam co probki
		int instance = -1;
		int amount = 0;
		ItemKind kind = ItemKind::Other;
		UseSkip skip = UseSkip::Keep; // != Keep: zapisane tylko do logu, bez zwrotu
		int manaGain = 0;
		std::uintptr_t token = 0;     // amunicja: wystrzelony przedmiot (silnik trzyma referencje), 0 = brak
	};

	class UseLedger {
	public:
		void Add(const UseRecord& r) { recs.push_back(r); }
		// Usuwa wpisy starsze niz t (removed: do zwolnienia referencji).
		void TrimBefore(float t, std::vector<UseRecord>* removed = nullptr);
		// Usuwa wpisy pozniejsze niz t (przyszlosc po cofnieciu).
		void CutAfter(float t, std::vector<UseRecord>* removed = nullptr);
		void Clear(std::vector<UseRecord>* removed = nullptr);
		const std::vector<UseRecord>& Records() const { return recs; }

	private:
		std::vector<UseRecord> recs;
	};

	struct Refund {
		int instance = -1;
		int amount = 0;
		ItemKind kind = ItemKind::Other;
		float firstDt = 0.0f;                 // ile s po chwili docelowej zuzyto pierwszy zwracany
		int manaRemove = 0;                   // suma dodatnich przyrostow many zwracanych mikstur
		std::vector<std::uintptr_t> tokens;   // amunicja: strzaly do zabrania ze swiata
	};
	struct RefundSkip {
		int instance = -1;
		int amount = 0;
		ItemKind kind = ItemKind::Other;
		float dt = 0.0f;
		const char* why = "";
	};
	// Zwroty dla wpisow po chwili docelowej (r.t > t), bez skip, z wlaczona kategoria; amunicja tylko z
	// returnable(token) (strzala lezy w swiecie albo zniknela w locie na starcie cofania). Limit na instancje:
	// ubytek netto max(0, atTarget - now) - przedmiot zdobyty po chwili docelowej, podniesiony z powrotem
	// albo oddany innym sposobem nie wraca podwojnie. Kolejnosc chronologiczna (limit ucina najpozniejsze).
	std::vector<Refund> PlanRefunds(const std::vector<UseRecord>& recs, float t, const RestoreOptions& opt,
		const InvCounts& atTarget, const InvCounts& now, const std::function<bool(std::uintptr_t)>& returnable,
		std::vector<RefundSkip>* skipped);

	// Czas wpisu: zuzycie w klatce, w ktorej ekwipunek juz policzono do probki (lastCountSample == now),
	// dostaje czas tuz po niej - probka w tej chwili ma jeszcze przedmiot, wiec cofniecie do niej go zwraca.
	float UseRecordTime(float now, float lastCountSample);

	// Bezposredni pomiar przedmiotu ksiegi czarow w Spell_Cast (zwoj: amount-- na tym obiekcie): ubytek
	// liczony z listy ekwipunku nie widzi zwoju spoza listy (spakowany plecak, przedmiot tylko w ksiedze).
	// Wynik: ubytki z licznikow + bezposredni (dla tej instancji wiekszy z obu - jeden rzut, nie dwa).
	std::vector<std::pair<int, int>> MergeDirectUse(std::vector<std::pair<int, int>> decreases, int instance, int used);

	// Spakowany plecak (oCNpcInventory::packString, "NAZWA:ilosc[:...],NAZWA:ilosc,"): przedmioty bez obiektu na
	// liscie ekwipunku. Pary (nazwa, ilosc) z iloscia > 0, nazwy jak w tekscie (symbole parsera, wielkie litery).
	std::vector<std::pair<std::string, int>> ParsePackString(const std::string& s);

	// Mana wydana przez bohatera na czary (rzut z ksiegi: Spell_Invest / SetReleaseStatus / Spell_Cast; bez runy
	// cofania). Commit zwraca sume wydatkow po chwili docelowej - mana nie jest probkowana (poza czasem), wiec
	// wraca tylko to, co poszlo na czary; regeneracja, mikstury i koszt runy zostaja.
	struct ManaSpend {
		float t = 0.0f;
		int amount = 0;
	};
	class ManaLedger {
	public:
		void Add(float t, int amount) {
			if (amount > 0)
				recs.push_back({ t, amount });
		}
		void TrimBefore(float t);
		void CutAfter(float t);
		void Clear() { recs.clear(); }
		// Suma i liczba wydatkow po t (r.t > t).
		int SpentAfter(float t, int* count = nullptr) const;
		const std::vector<ManaSpend>& Records() const { return recs; }

	private:
		std::vector<ManaSpend> recs;
	};
	struct ManaRefund {
		int mana = 0;      // nowa mana
		int give = 0;      // zwrot wydanej na czary (po limicie max many)
		int clipped = 0;   // ile zwrotu ucial limit max many
	};
	// mana teraz + wydana po celu - przyrost many z oddanych mikstur (Refund::manaRemove), w granicach [0, manaMax].
	// Zabranie many mikstury liczone przed limitem: mikstura (+50) i czar (-30) po celu = mana sprzed obu.
	ManaRefund PlanManaRefund(int mana, int manaMax, int spentAfter, int potionRemove);
}
