// Logika bez silnika: ktore symbole parsera sa zmiennymi globalnymi zapisu gry.
#pragma once
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace trcore {
	const int PAR_TYPE_INT = 2;   // zPAR_TYPE_INT

	// Jak filtr zCParser::SaveGlobalVars (0x796C40): typ INT, zadnych flag (const, pole klasy, extern, merged),
	// co najmniej jeden element. Tylko to silnik zapisuje w save - floaty i stringi nie.
	inline bool IsSavedGlobal(int type, unsigned flags, unsigned ele) {
		return type == PAR_TYPE_INT && flags == 0 && ele > 0;
	}

	// Przeglad listy 0..count-1 w porcjach - lista zmiennych zapisu to przeglad ~89 tys. symboli parsera (~4 ms
	// naraz po wczytaniu gry). Kluczem jest wlasciciel (parser) i liczba elementow: zmiana = przeglad od zera.
	class ChunkScan {
	public:
		// true, gdy klucz inny niz ostatnio: przeglad od poczatku (wywolujacy czysci zebrane wyniki).
		bool Restart(const void* owner, int count) {
			if (started_ && owner == owner_ && count == count_)
				return false;
			started_ = true;
			owner_ = owner;
			count_ = count < 0 ? 0 : count;
			next_ = 0;
			return true;
		}
		// Nastepna porcja [begin, end), najwyzej budget elementow (budget <= 0 = reszta naraz); false, gdy koniec.
		bool Next(int budget, int& begin, int& end) {
			if (!started_ || next_ >= count_)
				return false;
			begin = next_;
			end = budget <= 0 || count_ - next_ <= budget ? count_ : next_ + budget;
			next_ = end;
			return true;
		}
		// Przeglad skonczony (wyniki kompletne). Przed pierwszym Restart - nie.
		bool Done() const { return started_ && next_ >= count_; }

	private:
		bool started_ = false;
		const void* owner_ = nullptr;
		int count_ = 0, next_ = 0;
	};

	// Lista nazw z ini (KeepGlobals): rozdzielona przecinkami / spacjami / srednikami, wielkosc liter bez znaczenia,
	// '*' na koncu = przedrostek (RX_SPEED* pasuje do RX_SPEEDPOTION). Puste pozycje pomijane.
	class NameList {
	public:
		void Parse(const std::string& csv) {
			items_.clear();
			std::string cur;
			auto flush = [&]() {
				if (!cur.empty())
					items_.push_back(cur);
				cur.clear();
			};
			for (char c : csv) {
				if (c == ',' || c == ';' || c == ' ' || c == '\t')
					flush();
				else
					cur += (char)std::toupper((unsigned char)c);
			}
			flush();
		}
		bool Match(const std::string& name) const {
			std::string up;
			up.reserve(name.size());
			for (char c : name)
				up += (char)std::toupper((unsigned char)c);
			for (const std::string& it : items_) {
				if (!it.empty() && it.back() == '*') {
					if (up.compare(0, it.size() - 1, it, 0, it.size() - 1) == 0)
						return true;
				}
				else if (up == it)
					return true;
			}
			return false;
		}
		const std::vector<std::string>& Items() const { return items_; }
		bool Empty() const { return items_.empty(); }

	private:
		std::vector<std::string> items_;
	};

	// Tempo modelu (zCModel::timeScale) marionetki przy oddaniu sterowania. Przy starcie cofania zapamietane 'saved',
	// wpisane przez nas 'written' (0 = zamrozenie; PuppetAnims=2 nic nie wpisuje: written = saved). Wartosc inna niz
	// nasza w chwili oddania wpisal w trakcie inny modul (NB co klatke, zDismember, skrypt) - jest jego i zostaje
	// (zwracane 'current'). Inaczej wraca 'saved' (0 = pozostalosc po zamrozeniu, nie stan gry -> 1).
	inline float PuppetScaleAfter(float saved, float written, float current) {
		if (std::fabs(current - written) > 1e-6f)
			return current;
		return saved > 0.0f ? saved : 1.0f;
	}
}
