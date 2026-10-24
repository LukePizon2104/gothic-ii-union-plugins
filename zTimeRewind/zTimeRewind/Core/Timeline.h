// Logika bez silnika: tor probek jednego aktora (NPC / bohater) i zegar swiata.
#pragma once
#include <cstddef>
#include <vector>

namespace trcore {
	enum Life : unsigned char { LIFE_ALIVE = 0, LIFE_UNCONSCIOUS = 1, LIFE_DEAD = 2 };

	struct Sample {
		float t = 0.0f;              // czas pluginu (s), rosnacy
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float yaw = 0.0f;            // atan2(at.x, at.z), radiany
		int aniId = -1;              // glowna animacja (najnizsza warstwa), -1 = brak
		float frame = 0.0f;          // klatka tej animacji
		int hp = 0;
		unsigned char life = LIFE_ALIVE;
		unsigned char weapon = 0;    // oCNpc::GetWeaponMode()
	};

	// Najkrotsza roznica katow b - a, wynik w (-pi, pi].
	float AngleDelta(float a, float b);
	// Obrot z a w strone b najkrotsza droga, wynik w (-pi, pi].
	float LerpAngle(float a, float b, float alpha);
	// Ten sam stan (bez czasu): pozycja do 0.5 cm, kat do 0.001 rad, klatka do 0.01, reszta rowna.
	bool SameState(const Sample& a, const Sample& b);
	// Stan w chwili t (a.t <= t <= b.t): pozycja i kat liniowo; klatka liniowo tylko przy tej samej animacji
	// i rosnacej klatce (inaczej skok do blizszej probki); hp/life/weapon z wczesniejszej probki.
	Sample Interpolate(const Sample& a, const Sample& b, float t);
	// Postac trzeba ustawic twardo (ResetPos + AI od nowa): przesuniecie > distance albo inne hp/life/weapon.
	bool NeedsHardReset(const Sample& target, const Sample& latest, float distance);

	// Co zrobic z postacia przy Commit (reguly przywracania ze specu):
	// Keep - zostaje, jak jest (martwa / nieprzytomna wtedy i teraz);
	// SoftLatest - poza sprzed cofniecia, AI nietkniete (AI sie nie cofa, wiec poza musi do niego pasowac);
	// HardTarget - ResetPos w punkcie docelowym + AI od nowa (takze wskrzeszenie i wstanie z nokautu).
	enum class CommitAction { Keep, SoftLatest, HardTarget };
	// target = stan w punkcie cofniecia, latest = stan sprzed cofniecia. Bohater nieprzytomny w punkcie
	// docelowym wstaje tam (swiat sie cofnal, wiec on tez); NPC nie jest nokautowany na nowo.
	CommitAction DecideCommit(const Sample& target, const Sample& latest, bool deadNow, bool knockedNow, bool isHero, float distance);

	// Co nagrywanie robi z postacia z listy NPC swiata (voblist_npcs) w ticku probkowania.
	// Zdespawnowana postac (oCNpc::Disable -> oCWorld::DisableVob) zostaje na tej liscie, ale nie jest w swiecie
	// (homeWorld 0): silnik po chwili zwalnia jej model (Shrink), a odczyt przez oCNpc::GetModel buduje go od nowa
	// (UnShrink, 10-20 ms na postac). Takiej sie nie probkuje: nowa - pomijana, znana - stan bez zmian (Extend),
	// zeby po powrocie do swiata nie "plynela" w nagraniu przez cala przerwe.
	enum class SampleAction { Skip, Extend, Sample };
	SampleAction DecideSample(bool inWorld, bool known);

	class Track {
	public:
		// Dopisuje probke (s.t rosnace). Probka rowna ostatniej nie jest zapisywana, tylko zapamietana jako
		// trzymana; przy zmianie stanu trzymana trafia do toru przed nowa (stanie w miejscu nie rozmywa sie w ruch).
		void Push(const Sample& s);
		// Stan bez zmian do chwili t (postac poza swiatem): jak Push kopii ostatniego stanu, bez nowej probki
		// w torze. Nic, gdy tor pusty albo t nie jest pozniejsze niz ostatnia znana chwila.
		void Extend(float t);
		// Stan w chwili t. false, gdy tor pusty albo t przed pierwsza probka (aktor jeszcze nie istnial).
		bool At(float t, Sample& out) const;
		// Pierwsza probka (miejsce pojawienia sie). Wymaga !Empty().
		const Sample& First() const { return samples.front(); }
		// Usuwa probki starsze niz t, zostawiajac ostatnia <= t jako kotwice interpolacji.
		void TrimBefore(float t);
		// Kasuje wszystko po t (przyszlosc po cofnieciu); w chwili t zostaje stan z At(t).
		void CutAfter(float t);
		bool Empty() const { return samples.empty(); }
		size_t Size() const { return samples.size(); }
		// Czas ostatniej znanej probki (wliczajac trzymana); 0 gdy pusty.
		float LastTime() const;
		size_t Bytes() const { return samples.capacity() * sizeof(Sample); }

	private:
		std::vector<Sample> samples;
		Sample held;
		bool hasHeld = false;
	};

	struct ClockSample {
		float t = 0.0f;
		float fullTime = 0.0f;       // oCWorldTimer::GetFullTime()
	};

	// Zegar swiata: probka przy kazdym ticku nagrywania.
	class ClockTrack {
	public:
		void Push(float t, float fullTime);
		bool At(float t, float& fullTime) const;
		void TrimBefore(float t);
		void CutAfter(float t);
		bool Empty() const { return samples.empty(); }
		float FirstTime() const { return samples.empty() ? 0.0f : samples.front().t; }
		float LastTime() const { return samples.empty() ? 0.0f : samples.back().t; }

	private:
		std::vector<ClockSample> samples;
	};
}
