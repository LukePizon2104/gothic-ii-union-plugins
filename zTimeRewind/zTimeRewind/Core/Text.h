// Logika bez silnika: proste operacje na tekscie (konsola).
#pragma once
#include <string>
#include <vector>

namespace trcore {
	std::string Upper(std::string s);
	// Dzieli linie po spacjach i tabulatorach, zamienia na wielkie litery.
	std::vector<std::string> SplitArgs(const std::string& line);

	// Menu Union (choicebox zapisuje NUMER pozycji): pozycja dla wartosci z ini - dokladna albo (nearest) najblizsza;
	// -1 gdy nie ma i nearest=false.
	int MenuIndexForValue(const std::vector<int>& values, int value, bool nearest = true);
	// Wartosc do ini tylko, gdy gracz przestawil pozycje w menu (inna niz przy wejsciu i na liscie).
	bool MenuChangedValue(const std::vector<int>& values, int lastIndex, int menuIndex, int& value);
}
