// Logika bez silnika: proste operacje na tekscie (konsola, ini).
#pragma once
#include <string>
#include <vector>

namespace zdcore {
	std::string Trim(const std::string& s);
	std::string Upper(std::string s);
	// Dzieli linie po spacjach, zamienia na wielkie litery.
	std::vector<std::string> SplitArgs(const std::string& line);
	// "50,35,15" -> out[0..n-1]; false (out nietkniete), gdy liczba elementow != n albo smieci.
	bool ParseIntList(const std::string& s, int* out, int n);
	bool ParseFloatList(const std::string& s, float* out, int n);

	// "a, b ,c" -> {"A","B","C"} (bez pustych, wielkie litery).
	std::vector<std::string> SplitTokens(const std::string& s, char sep);
	// Nazwa zawiera ktorys z fragmentow (bez wielkosci liter), np. instancja broni ITMW_RAPIER i fragment RAPIER.
	bool NameHasToken(const std::string& name, const std::vector<std::string>& upperTokens);

	// Choicebox menu (Gothic.ini) zapisuje numer pozycji. Pozycja dla wartosci: najblizsza (skala, np. procenty)
	// albo dokladna (lista klawiszy); -1 = brak.
	int MenuIndexForValue(const std::vector<int>& values, int value, bool nearest = true);
	// Gracz przestawil pozycje w menu (inna niz zapisana przy ostatniej synchronizacji) -> nowa wartosc.
	bool MenuChangedValue(const std::vector<int>& values, int lastIndex, int menuIndex, int& value);
}
