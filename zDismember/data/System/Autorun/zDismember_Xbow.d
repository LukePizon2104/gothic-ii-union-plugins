META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

// zDismember - szybki strzal z kuszy (Crossbow.cpp, spec [dokument projektowy - niepublikowany]). Plik bez
// polskich znakow (ASCII). Silnik zapisuje tylko zwykle zmienne int (zCParser::SaveGlobalVars), wiec wybor slotu kuszy
// pod F (zQuickSign: Shift + kolko za ostatnim Znakiem) siedzi tutaj. Plugin czyta przy wczytaniu, pisze przy zmianie
// i przed zapisem. Diagnoza: zdism xbow info; set var ZDF_XBOW_SLOT 0 (kusza odznaczona).

var int ZDF_XBOW_SLOT;   // bit 0 = kusza wybrana pod F (zamiast Znaku)
