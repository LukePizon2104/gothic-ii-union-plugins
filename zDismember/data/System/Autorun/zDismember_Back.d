META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

// zDismember - miecze na plecach (BackSwords.cpp, spec [dokument projektowy - niepublikowany]). Plik bez
// polskich znakow (ASCII). Stan w zapisie gry: silnik zapisuje tylko zwykle zmienne int (zCParser::SaveGlobalVars),
// wiec przedmiot drugiego miecza jest zapisany skrotem nazwy instancji. Plugin czyta zmienne przy wczytaniu, pisze
// przy zmianie i przed zapisem. Diagnoza: set var ZDF_BACK_SECOND 0 (bez drugiego miecza), zdism back info.

var int ZDF_BACK_SECOND;   // skrot nazwy instancji drugiego miecza (zdcore::BackNameHash; 0 = brak)
var int ZDF_BACK_FLAGS;    // 1 = zalozona bron wisi w miejscu B (srebro Geralta) - po zamianie z drugim mieczem
