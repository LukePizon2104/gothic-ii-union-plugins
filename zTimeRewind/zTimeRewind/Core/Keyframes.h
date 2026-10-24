// Logika bez silnika: migawki stanu w czasie (rosnace t), wyszukiwanie najpozniejszej <= t.
#pragma once
#include <cstddef>
#include <utility>
#include <vector>

namespace trcore {
	// Migawki usuniete z nagrania (TrimBefore, CutAfter) trafiaja do puli razem z buforami: nastepna migawka
	// bierze je przez Acquire, wiec w stalym rytmie nagrywania (migawka co StateInterval, bufor BufferSeconds)
	// nie ma alokacji - tylko w pierwszych sekundach, zanim bufor sie zapelni.
	template <class T>
	class Keyframes {
	public:
		void Push(float t, T v) { items.emplace_back(t, std::move(v)); }
		// Obiekt do wypelnienia i Push: z puli (stara zawartosc - wypelniajacy nadpisuje wszystkie pola) albo nowy.
		T Acquire() {
			if (pool.empty())
				return T();
			T v = std::move(pool.back());
			pool.pop_back();
			return v;
		}
		// Niewykorzystany obiekt z Acquire (nieudana migawka) wraca do puli.
		void Release(T v) { pool.push_back(std::move(v)); }
		// Indeks najpozniejszej migawki z czasem <= t; -1 gdy brak.
		int FindAtOrBefore(float t) const {
			for (int i = (int)items.size() - 1; i >= 0; i--)
				if (items[i].first <= t)
					return i;
			return -1;
		}
		float TimeAt(int i) const { return items[i].first; }
		const T& At(int i) const { return items[i].second; }
		// Usuwa migawki starsze niz t, zostawiajac ostatnia <= t (kotwica).
		void TrimBefore(float t) {
			size_t keep = 0;
			while (keep + 1 < items.size() && items[keep + 1].first <= t)
				keep++;
			if (keep == 0)
				return;
			// najpierw bufory do puli: przesuwanie przy erase przenosi wtedy obiekty w puste miejsca (bez zwalniania)
			for (size_t i = 0; i < keep; i++)
				pool.push_back(std::move(items[i].second));
			items.erase(items.begin(), items.begin() + keep);
		}
		void CutAfter(float t) {
			while (!items.empty() && items.back().first > t) {
				pool.push_back(std::move(items.back().second));
				items.pop_back();
			}
		}
		size_t Size() const { return items.size(); }
		size_t PoolSize() const { return pool.size(); }
		// Wczytanie gry / zmiana swiata: wszystko, takze pula (pamiec zwolniona).
		void Clear() {
			items.clear();
			pool.clear();
		}

	private:
		std::vector<std::pair<float, T>> items;
		std::vector<T> pool;
	};
}
