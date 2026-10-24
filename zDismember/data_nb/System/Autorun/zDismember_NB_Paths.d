META
{
	Engine = G2A;
	Parser = Game;
	MergeMode = 1;
};

// zDismember - TYLKO paczka New Balance (build_vdf.py --nb dokleja katalog data_nb). Plik bez polskich znakow (ASCII).
// Stan menu gildii i sciezek klas NB (okno medytacji: zakladki Gildia / Sciezki, klawisz PathMenuKey). Zmienne
// zwykle - silnik zapisuje je z gra (SAVEDAT), wczytanie starego zapisu przywraca stary stan menu. Uklad tablic
// opisuje Core\PathMenu.h pluginu (indeksy umiejetnosci stale, tylko dopisywane).
// Nakladki *_isway.d czytaja bity przez Hlp_GetIntArrayValue("ZDF_PATHS_OFF", 0) (8 mroczny rycerz,
// 32 czarnoksieznik, 64 przywolywacz) - bez tego pliku funkcja zwraca 0 i nakladki dzialaja jak dawniej.
// Diagnoza: set var ZDF_PATHS_OFF 0 (wszystkie sciezki znow "wlaczone" w menu, znaczniki wracaja po wlaczeniu
// w menu albo zbroja), savevars.py SAVEDAT.SAV ZDF_PATHS_OFF ZDF_PATH_KEEP ZDF_SKILL_OFF ZDF_SKILL_KEEP.

var int ZDF_PATHS_VER;          // 1 - wersja ukladu
var int ZDF_PATHS_OFF;          // bity sciezek wylaczonych w menu: 0 wampir, 1 zwiadowca, 2 lowca demonow,
                                // 3 mroczny rycerz, 4 wojownik Adanosa, 5 czarnoksieznik, 6 przywolywacz,
                                // 7 szaman, 8 druid, 9 truciciel
var int ZDF_PATH_KEEP[10];      // znacznik sprzed wylaczenia (0 = przy wlaczeniu stala magiczna)
var int ZDF_SKILL_OFF[8];       // 256 bitow: umiejetnosci wylaczone w menu
var int ZDF_SKILL_KEEP[256];    // zapamietane poziomy wylaczonych (250..255: druga zmienna par)
var int ZDF_GUILD_LAST;         // ostatnia gildia ustawiona z menu (diagnostyka)
