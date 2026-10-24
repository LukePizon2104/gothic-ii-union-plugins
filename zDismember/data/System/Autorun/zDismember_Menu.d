META
{
  Parser = Menu;
  After = zUnionMenu.d;
};

// Menu zDismember w "Union & Pluginy" (zParserExtender). Kodowanie: UTF-8 bez BOM.
// Wartości trafiają do Gothic.ini [ZDISMEMBER] przez choicebox (zapisuje NUMER pozycji). Klucze Menu* plugin
// przelicza na wartości (procenty, kody klawiszy) przy wyjściu z menu - kolejność pozycji MUSI zgadzać się
// z tablicą menuChoices w Config.cpp. Pozostałe klucze to 0/1 (wył/wł) albo numer trybu.

const int ZDISM_MENU_TEXT_X       = 1000;
const int ZDISM_MENU_TEXT_DX      = 8000;
const int ZDISM_MENU_TEXT_DY      = 750;
const int ZDISM_MENU_CHOICE_X     = 5300;
const int ZDISM_MENU_CHOICE_DX    = 2400;
const int ZDISM_MENU_CHOICE_DY    = 350;
const int ZDISM_MENU_CHOICE_YPLUS = 120;
const int ZDISM_MENU_TITLE_Y      = 450;
const int ZDISM_MENU_START_Y      = 1400;
const int ZDISM_MENU_DY           = 380;   // 13 wierszy strony W3 (1-13) nad "Wstecz" (posy 6892): ostatni wybór 6460-6810

const string ZDISM_MENU_ITEM_BACK_PIC   = "";
const string ZDISM_MENU_BACK_PIC        = "MENU_INGAME.TGA";
const string ZDISM_MENU_CHOICE_BACK_PIC = "MENU_CHOICE_BACK.TGA";


// ---------------------------------------------------------------- wpis na liście "Union & Pluginy"

instance MENUITEM_UNION_AUTO_ZDISMEMBER(C_MENU_ITEM_UNION_DEF)
{
  text[0] = Str_GetLocalizedString(
    "zDismember - dismemberment, finishers, Witcher combat",
    "zDismember - dismemberment, finishers, Witcher combat",
    "zDismember - dismemberment, finishers, Witcher combat",
    "zDismember - odcinanie, finishery, walka Wiedźmina"
  );
  text[1] = Str_GetLocalizedString("Dismemberment, finishers, Witcher combat & moves", "Dismemberment, finishers, Witcher combat & moves",
    "Dismemberment, finishers, Witcher combat & moves", "Odcinanie, finishery, walka i ruch Wiedźmina");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_HUB";
};


// ---------------------------------------------------------------- wspólne

instance C_ZDISM_MENU_ITEM_TEXT_BASE(C_MENU_ITEM_DEF)
{
  fontname       = MENU_FONT_SMALL;   // etykiety opcji - duza czcionka nie miesci sie przy 800x600
  backpic        = ZDISM_MENU_ITEM_BACK_PIC;
  posx           = ZDISM_MENU_TEXT_X;
  posy           = ZDISM_MENU_START_Y;
  dimx           = ZDISM_MENU_TEXT_DX;
  dimy           = ZDISM_MENU_TEXT_DY;
  flags          = flags | IT_EFFECTS_NEXT;
  onselaction[0] = SEL_ACTION_UNDEF;
};

instance C_ZDISM_MENUITEM_CHOICE_BASE(C_MENU_ITEM_DEF)
{
  backpic               = ZDISM_MENU_CHOICE_BACK_PIC;
  type                  = MENU_ITEM_CHOICEBOX;
  fontname              = MENU_FONT_SMALL;
  posx                  = ZDISM_MENU_CHOICE_X;
  posy                  = ZDISM_MENU_START_Y + ZDISM_MENU_CHOICE_YPLUS;
  dimx                  = ZDISM_MENU_CHOICE_DX;
  dimy                  = ZDISM_MENU_CHOICE_DY;
  onchgsetoptionsection = "ZDISMEMBER";
  flags                 = flags & ~IT_SELECTABLE;
  flags                 = flags | IT_TXT_CENTER;
};

instance C_ZDISM_MENU_HEADLINE(C_MENU_ITEM_DEF)
{
  type  = MENU_ITEM_TEXT;
  posx  = 0;
  posy  = ZDISM_MENU_TITLE_Y;
  dimx  = 8100;
  flags = flags & ~IT_SELECTABLE;
  flags = flags | IT_TXT_CENTER;
};

instance C_ZDISM_MENU_NOTE(C_MENU_ITEM_DEF)
{
  type     = MENU_ITEM_TEXT;
  posx     = 0;
  dimx     = 8100;
  fontname = MENU_FONT_SMALL;
  flags    = flags & ~IT_SELECTABLE;
  flags    = flags | IT_TXT_CENTER;
  text[0]  = Str_GetLocalizedString(
    "Changes apply on menu exit",
    "Changes apply on menu exit",
    "Changes apply on menu exit",
    "Zmiany działają po wyjściu z menu"
  );
};

const string ZDISM_OFF_ON_EN = "off|on";
const string ZDISM_OFF_ON_PL = "wył.|wł.";
const string ZDISM_PERCENT   = "0%|10%|20%|30%|40%|50%|60%|70%|80%|90%|100%";


// ---------------------------------------------------------------- strona główna

instance ZDISM_MENU_HUB(C_MENU_DEF)
{
  backPic  = ZDISM_MENU_BACK_PIC;
  items[0] = "ZDISM_HUB_HEADLINE";
  items[1] = "ZDISM_HUB_GORE";
  items[2] = "ZDISM_HUB_W3";
  items[3] = "ZDISM_HUB_SIGNS1";
  items[4] = "ZDISM_HUB_SIGNS2";
  items[5] = "ZDISM_HUB_SIGNS3";
  items[6] = "ZDISM_HUB_ANIM";
  items[7] = "ZDISM_HUB_ATK";
  items[8] = "ZDISM_HUB_MEDITATE";
  items[9] = "ZDISM_HUB_LOOT";
  items[10] = "ZDISM_HUB_BACKSWORDS";
  items[11] = "ZDISM_HUB_XBOW";
  items[12] = "ZDISM_HUB_WHEEL";
  items[13] = "UNION_MENUITEM_BACK";
  flags    = flags | MENU_SHOW_INFO;
};

instance ZDISM_HUB_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = "zDismember";
};

instance ZDISM_HUB_GORE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 1;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Dismemberment and finishers", "Dismemberment and finishers",
    "Dismemberment and finishers", "Odcinanie i finishery");
  text[1] = Str_GetLocalizedString("Dismemberment, finishers, ragdolls, blocking", "Dismemberment, finishers, ragdolls, blocking",
    "Dismemberment, finishers, ragdolls, blocking", "Odcinanie, finishery, ragdolle, efekty bloku");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_GORE";
};

instance ZDISM_HUB_W3(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 2;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Witcher combat and movement", "Witcher combat and movement",
    "Witcher combat and movement", "Walka i ruch Wiedźmina");
  text[1] = Str_GetLocalizedString("Style, dodges, counters, walk, run, sprint", "Style, dodges, counters, walk, run, sprint",
    "Style, dodges, counters, walk, run, sprint", "Styl, uniki, kontry, chód, bieg i sprint");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_W3";
};

// Strojenie Znaków Wiedźmina (runda 5) - dwie strony na końcu pliku (ZDISM_MENU_SIGNS1 / 2, gen_sign_menu.py).
instance ZDISM_HUB_SIGNS1(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 3;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Signs: range and strength", "Signs: range and strength",
    "Signs: range and strength", "Znaki: zasięg i siła");
  text[1] = Str_GetLocalizedString("Aard, Igni, Axii, Yrden: range, damage, effects", "Aard, Igni, Axii, Yrden: range, damage, effects",
    "Aard, Igni, Axii, Yrden: range, damage, effects", "Aard, Igni, Axii, Yrden: zasięg, siła, efekty");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_SIGNS1";
};

instance ZDISM_HUB_SIGNS2(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 4;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Signs: power, time and costs", "Signs: power, time and costs",
    "Signs: power, time and costs", "Znaki: moc, czas i koszty");
  text[1] = Str_GetLocalizedString("Quen, power from mana, alt mode, mana costs", "Quen, power from mana, alt mode, mana costs",
    "Quen, power from mana, alt mode, mana costs", "Quen, moc z many, tryb alternatywny, koszty");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_SIGNS2";
};

// Znaki poza walką (runda 6) - strona ZDISM_MENU_SIGNS3 na końcu pliku (ręcznie, nie z gen_sign_menu.py).
instance ZDISM_HUB_SIGNS3(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 5;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Signs: out of combat, power, upkeep", "Signs: out of combat, power, upkeep",
    "Signs: out of combat, power, upkeep", "Znaki: poza walką, moc, koszty");
  text[1] = Str_GetLocalizedString("Charm, trance, fires, power, Axii range, upkeep", "Charm, trance, fires, power, Axii range, upkeep",
    "Charm, trance, fires, power, Axii range, upkeep", "Urok, trans, ogień, moc, zasięg Axii, mana/s");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_SIGNS3";
};

// Zamiana animacji 1H <-> 2H bohatera (AniSwap.cpp) - strona ZDISM_MENU_ANIM niżej.
instance ZDISM_HUB_ANIM(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 6;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Weapon animations", "Weapon animations", "Weapon animations", "Animacje broni");
  text[1] = Str_GetLocalizedString("Fight with 1H weapons as 2H and vice versa", "Fight with 1H weapons as 2H and vice versa",
    "Fight with 1H weapons as 2H and vice versa", "Bronią 1H jak 2H i odwrotnie");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_ANIM";
};

// Tempo ciosów stylu Wiedźmina (AtkTempo.cpp) - strona ZDISM_MENU_ATK niżej.
instance ZDISM_HUB_ATK(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 7;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Witcher attack speed", "Witcher attack speed", "Witcher attack speed", "Tempo ciosów Wiedźmina");
  text[1] = Str_GetLocalizedString("Left, right and combo speed; 2H side attacks", "Left, right and combo speed; 2H side attacks",
    "Left, right and combo speed; 2H side attacks", "Tempo ciosów w lewo, w prawo, kombo; boczne 2H");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_ATK";
};

// Medytacja Wiedźmina (Meditate.cpp) - strona ZDISM_MENU_MEDITATE na końcu pliku.
instance ZDISM_HUB_MEDITATE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 8;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Meditation", "Meditation", "Meditation", "Medytacja");
  text[1] = Str_GetLocalizedString("Kneel, wait with faster time, regeneration", "Kneel, wait with faster time, regeneration",
    "Kneel, wait with faster time, regeneration", "Klęczenie, czekanie z szybszym czasem, regeneracja");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_MEDITATE";
};

// Podnoszenie, drzwi, skrzynie i ciała z dobytą bronią (Loot.cpp, LootInteract.cpp) - strona ZDISM_MENU_LOOT na końcu pliku.
instance ZDISM_HUB_LOOT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 9;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Actions with weapon drawn", "Actions with weapon drawn",
    "Actions with weapon drawn", "Interakcje z bronią w ręku");
  text[1] = Str_GetLocalizedString("Attack picks up, opens doors, chests, bodies", "Attack picks up, opens doors, chests, bodies", "Attack picks up, opens doors, chests, bodies", "Atak podnosi, otwiera drzwi, skrzynie, ciała");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_LOOT";
};


// Miecze na plecach (BackSwords.cpp) - strona ZDISM_MENU_BACK na końcu pliku.
instance ZDISM_HUB_BACKSWORDS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 10;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Swords on the back", "Swords on the back", "Swords on the back", "Miecze na plecach");
  text[1] = Str_GetLocalizedString("Draw from the back, 1H on the back, second sword", "Draw from the back, 1H on the back, second sword",
    "Draw from the back, 1H on the back, second sword", "Dobywanie zza pleców, 1H na plecach, drugi miecz");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_BACK";
};

// Szybki strzał z kuszy (Crossbow.cpp) - strona ZDISM_MENU_XBOW na końcu pliku.
instance ZDISM_HUB_XBOW(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 11;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Quick crossbow shot", "Quick crossbow shot", "Quick crossbow shot", "Szybki strzał z kuszy");
  text[1] = Str_GetLocalizedString("F with sword in hand, like Geralt", "F with sword in hand, like Geralt",
    "F with sword in hand, like Geralt", "F z mieczem w ręku, jak u Geralta");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_XBOW";
};

// Menu kołowe szybkiego wyboru (Wheel.cpp) - strona ZDISM_MENU_WHEEL na końcu pliku.
instance ZDISM_HUB_WHEEL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  fontname = "FONT_OLD_20_WHITE.TGA";   // jak domyslna czcionka menu
  posy += ZDISM_MENU_DY * 12;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Quick selection wheel", "Quick selection wheel", "Quick selection wheel", "Koło wyboru");
  text[1] = Str_GetLocalizedString("Hold Caps Lock: spells and crossbow, time slows down", "Hold Caps Lock: spells and crossbow, time slows down",
    "Hold Caps Lock: spells and crossbow, time slows down", "Przytrzymaj Caps Lock: czary i kusza, czas zwalnia");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_WHEEL";
};

// ---------------------------------------------------------------- odcinanie i finishery

instance ZDISM_MENU_GORE(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_GORE_HEADLINE";
  items[1]  = "ZDISM_GORE_ENABLED";
  items[2]  = "ZDISM_GORE_ENABLED_CHOICE";
  items[3]  = "ZDISM_GORE_CUT";
  items[4]  = "ZDISM_GORE_CUT_CHOICE";
  items[5]  = "ZDISM_GORE_FIN";
  items[6]  = "ZDISM_GORE_FIN_CHOICE";
  items[7]  = "ZDISM_GORE_SLOWMO";
  items[8]  = "ZDISM_GORE_SLOWMO_CHOICE";
  items[9]  = "ZDISM_GORE_SLOWLAST";
  items[10] = "ZDISM_GORE_SLOWLAST_CHOICE";
  items[11] = "ZDISM_GORE_KO";
  items[12] = "ZDISM_GORE_KO_CHOICE";
  items[13] = "ZDISM_GORE_DAZE";
  items[14] = "ZDISM_GORE_DAZE_CHOICE";
  items[15] = "ZDISM_GORE_RAGDOLL";
  items[16] = "ZDISM_GORE_RAGDOLL_CHOICE";
  items[17] = "ZDISM_GORE_INVUL";
  items[18] = "ZDISM_GORE_INVUL_CHOICE";
  items[19] = "ZDISM_GORE_PARRYFX";
  items[20] = "ZDISM_GORE_PARRYFX_CHOICE";
  items[21] = "ZDISM_GORE_RECOIL";
  items[22] = "ZDISM_GORE_RECOIL_CHOICE";
  items[23] = "ZDISM_GORE_NOTE";
  items[24] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_GORE_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Dismemberment and finishers", "Dismemberment and finishers",
    "Dismemberment and finishers", "Odcinanie i finishery");
};

instance ZDISM_GORE_ENABLED(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Plugin", "Plugin", "Plugin", "Plugin");
  text[1] = Str_GetLocalizedString("Whole zDismember on / off", "Whole zDismember on / off",
    "Whole zDismember on / off", "Cały zDismember włączony / wyłączony");
};
instance ZDISM_GORE_ENABLED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "Enabled";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_CUT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Dismemberment chance", "Dismemberment chance", "Dismemberment chance", "Odcinanie kończyn");
  text[1] = Str_GetLocalizedString("Chance a killing blade blow severs a limb", "Chance a killing blade blow severs a limb",
    "Chance a killing blade blow severs a limb", "Szansa, że śmiertelny cios ostrzem coś odetnie");
};
instance ZDISM_GORE_CUT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuDismemberChance";
  text[0] = ZDISM_PERCENT;
};

instance ZDISM_GORE_FIN(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Finisher chance", "Finisher chance", "Finisher chance", "Szansa finishera");
  text[1] = Str_GetLocalizedString("Chance of a finisher animation on a killing blow", "Chance of a finisher animation on a killing blow",
    "Chance of a finisher animation on a killing blow", "Szansa na dobicie przy śmiertelnym ciosie");
};
instance ZDISM_GORE_FIN_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuFinisherChance";
  text[0] = ZDISM_PERCENT;
};

// Spowolnienie czasu w finisherze (Director.cpp, zdcore::FinisherSlowFactor): klucze FinisherSlowMo (0 wył., 1 lekkie,
// 2 pełne = jak w finishers.ini) i FinisherSlowMoLastEnemy (0/1) - numer pozycji = wartość.
instance ZDISM_GORE_SLOWMO(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Finisher slow motion", "Finisher slow motion", "Finisher slow motion", "Spowolnienie w finisherze");
  text[1] = Str_GetLocalizedString("Slow motion around the killing blow", "Slow motion around the killing blow",
    "Slow motion around the killing blow", "Zwolnione tempo wokół ciosu dobijającego");
};
instance ZDISM_GORE_SLOWMO_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "FinisherSlowMo";
  text[0] = Str_GetLocalizedString("off|light|full", "off|light|full", "off|light|full", "wył.|lekkie|pełne");
};

instance ZDISM_GORE_SLOWLAST(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Slow motion: last enemy only", "Slow motion: last enemy only",
    "Slow motion: last enemy only", "Spowolnienie: tylko ostatni wróg");
  text[1] = Str_GetLocalizedString("No slow motion while other enemies still fight", "No slow motion while other enemies still fight",
    "No slow motion while other enemies still fight", "Bez spowolnienia, gdy walczy jeszcze inny wróg");
};
instance ZDISM_GORE_SLOWLAST_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "FinisherSlowMoLastEnemy";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_KO(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Finisher instead of knockout", "Finisher instead of knockout",
    "Finisher instead of knockout", "Finisher zamiast powalenia");
  text[1] = Str_GetLocalizedString("Kill humans instead of knockout; arena unchanged", "Kill humans instead of knockout; arena unchanged",
    "Kill humans instead of knockout; arena unchanged", "Dobija powalanych ludzi; arena i bójki bez zmian");
};
instance ZDISM_GORE_KO_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "KnockoutFinisher";
  text[0] = Str_GetLocalizedString("never|enemies|all but friends", "never|enemies|all but friends",
    "never|enemies|all but friends", "nigdy|wrogowie|poza przyjaciółmi");
};

instance ZDISM_GORE_DAZE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Dazed enemies", "Dazed enemies", "Dazed enemies", "Oszołomienie");
  text[1] = Str_GetLocalizedString("Downed foe kneels briefly; attack to execute", "Downed foe kneels briefly; attack to execute",
    "Downed foe kneels briefly; attack to execute", "Powalony wróg klęczy - atak dobija egzekucją");
};
instance ZDISM_GORE_DAZE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "DazeEnabled";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_RAGDOLL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Ragdolls", "Ragdolls", "Ragdolls", "Ragdolle");
  text[1] = Str_GetLocalizedString("Bodies fall physically (except finishers)", "Bodies fall physically (except finishers)",
    "Bodies fall physically (except finishers)", "Ciała padają fizycznie (poza finisherami)");
};
instance ZDISM_GORE_RAGDOLL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "Ragdoll";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_INVUL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Invulnerable during finisher", "Invulnerable during finisher",
    "Invulnerable during finisher", "Nietykalny w finisherze");
  text[1] = Str_GetLocalizedString("Nobody can hit you while a finisher plays", "Nobody can hit you while a finisher plays",
    "Nobody can hit you while a finisher plays", "Nikt cię nie trafi w trakcie animacji dobicia");
};
instance ZDISM_GORE_INVUL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 9;
  onchgsetoption = "HeroInvulnerable";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_PARRYFX(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Block sparks", "Block sparks", "Block sparks", "Iskry przy bloku");
  text[1] = Str_GetLocalizedString("Sparks on metal, splinters on wooden weapons", "Sparks on metal, splinters on wooden weapons",
    "Sparks on metal, splinters on wooden weapons", "Iskry (metal) albo drzazgi (drewno) przy bloku");
};
instance ZDISM_GORE_PARRYFX_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 10;
  onchgsetoption = "ParryFx";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_RECOIL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Block recoil", "Block recoil", "Block recoil", "Odrzut przy bloku");
  text[1] = Str_GetLocalizedString("Both fighters' weapons bounce back on a block", "Both fighters' weapons bounce back on a block",
    "Both fighters' weapons bounce back on a block", "Broń obu walczących odskakuje przy bloku");
};
instance ZDISM_GORE_RECOIL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 11;
  onchgsetoption = "ParryRecoil";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_GORE_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 12;
};


// ---------------------------------------------------------------- walka i ruch Wiedźmina

instance ZDISM_MENU_W3(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_W3_HEADLINE";
  items[1]  = "ZDISM_W3_ASSIST";
  items[2]  = "ZDISM_W3_ASSIST_CHOICE";
  items[3]  = "ZDISM_W3_STYLE";
  items[4]  = "ZDISM_W3_STYLE_CHOICE";
  items[5]  = "ZDISM_W3_TALENT";
  items[6]  = "ZDISM_W3_TALENT_CHOICE";
  items[7]  = "ZDISM_W3_LOCO";
  items[8]  = "ZDISM_W3_LOCO_CHOICE";
  items[9]  = "ZDISM_W3_SPRINT";
  items[10] = "ZDISM_W3_SPRINT_CHOICE";
  items[11] = "ZDISM_W3_MOVES";
  items[12] = "ZDISM_W3_MOVES_CHOICE";
  items[13] = "ZDISM_W3_DODGE";
  items[14] = "ZDISM_W3_DODGE_CHOICE";
  items[15] = "ZDISM_W3_ROLL";
  items[16] = "ZDISM_W3_ROLL_CHOICE";
  items[17] = "ZDISM_W3_COUNTER";
  items[18] = "ZDISM_W3_LOCOMODE";
  items[19] = "ZDISM_W3_LOCOMODE_CHOICE";
  items[20] = "ZDISM_W3_TRAV";
  items[21] = "ZDISM_W3_TRAV_CHOICE";
  items[22] = "ZDISM_W3_SIGNS";
  items[23] = "ZDISM_W3_SIGNS_CHOICE";
  items[24] = "ZDISM_W3_LOOT";
  items[25] = "ZDISM_W3_LOOT_CHOICE";
  items[26] = "ZDISM_W3_NOTE";
  items[27] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_W3_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Witcher combat and movement", "Witcher combat and movement",
    "Witcher combat and movement", "Walka i ruch Wiedźmina");
};

// Wiersz 0 (posy = ZDISM_MENU_START_Y, między notką a wierszem 1); kontry mają osobną stronę (wiersz 8).
// Klucz W3AttackAssist 0/1 (Reach.cpp); premia zasięgu W3AttackReach tylko w Gothic.ini.
instance ZDISM_W3_ASSIST(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  text[0] = Str_GetLocalizedString("Lunge to target", "Lunge to target", "Lunge to target", "Doskok do celu");
  text[1] = Str_GetLocalizedString("Turn and lunge at the enemy ahead (max 1.3 m)", "Turn and lunge at the enemy ahead (max 1.3 m)",
    "Turn and lunge at the enemy ahead (max 1.3 m)", "Obrót i doskok do wroga przed tobą (do 1,3 m)");
};
instance ZDISM_W3_ASSIST_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  onchgsetoption = "W3AttackAssist";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_W3_STYLE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Witcher style", "Witcher style", "Witcher style", "Styl Wiedźmina");
  text[1] = Str_GetLocalizedString("Geralt's attacks, combo and guard (U switches)", "Geralt's attacks, combo and guard (U switches)",
    "Geralt's attacks, combo and guard (U switches)", "Ciosy, kombo i garda Geralta (U zmienia styl)");
};
instance ZDISM_W3_STYLE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "W3Style";
  text[0] = Str_GetLocalizedString("off|from threshold", "off|from threshold", "off|from threshold", "wył.|od progu");
};

instance ZDISM_W3_TALENT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Talent threshold", "Talent threshold", "Talent threshold", "Próg talentu");
  text[1] = Str_GetLocalizedString("1H/2H talent needed for the Witcher style", "1H/2H talent needed for the Witcher style",
    "1H/2H talent needed for the Witcher style", "Talent broni 1H/2H potrzebny do stylu Wiedźmina");
};
instance ZDISM_W3_TALENT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuW3StyleMinTalent";
  text[0] = ZDISM_PERCENT;
};

instance ZDISM_W3_LOCO(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Geralt's walk and run", "Geralt's walk and run", "Geralt's walk and run", "Chód i bieg Geralta");
  text[1] = Str_GetLocalizedString("Unarmed always, with sword in Witcher style", "Unarmed always, with sword in Witcher style",
    "Unarmed always, with sword in Witcher style", "Bez broni zawsze, z mieczem w stylu Wiedźmina");
};
instance ZDISM_W3_LOCO_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "W3Locomotion";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_W3_SPRINT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Sprint animation", "Sprint animation", "Sprint animation", "Animacja sprintu");
  text[1] = Str_GetLocalizedString("Sprint key, stamina and speed stay from game", "Sprint key, stamina and speed stay from game",
    "Sprint key, stamina and speed stay from game", "Klawisz, kondycja i tempo sprintu z gry / zEBA");
};
instance ZDISM_W3_SPRINT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "W3Sprint";
  text[0] = Str_GetLocalizedString("game / zEBA|Geralt", "game / zEBA|Geralt", "game / zEBA|Geralt", "gra / zEBA|Geralt");
};

instance ZDISM_W3_MOVES(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Dodges and rolls", "Dodges and rolls", "Dodges and rolls", "Uniki i przewroty");
  text[1] = Str_GetLocalizedString("In combat: dodge key / roll key + direction", "In combat: dodge key / roll key + direction",
    "In combat: dodge key / roll key + direction", "W walce: klawisz uniku / przewrotu + kierunek");
};
instance ZDISM_W3_MOVES_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MovesEnabled";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_W3_DODGE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Dodge key", "Dodge key", "Dodge key", "Klawisz uniku");
  text[1] = Str_GetLocalizedString("LEFT / RIGHT arrows - previous / next key", "LEFT / RIGHT arrows - previous / next key",
    "LEFT / RIGHT arrows - previous / next key", "Strzałki w lewo / w prawo zmieniają klawisz");
};
instance ZDISM_W3_DODGE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuDodgeKey";
  text[0] = Str_GetLocalizedString("V|C|X|Z|L-ALT|L-CTRL|CAPS LOCK|off|custom (Gothic.ini)",
    "V|C|X|Z|L-ALT|L-CTRL|CAPS LOCK|off|custom (Gothic.ini)", "V|C|X|Z|L-ALT|L-CTRL|CAPS LOCK|off|custom (Gothic.ini)",
    "V|C|X|Z|lewy ALT|lewy CTRL|CAPS LOCK|wył.|własny (Gothic.ini)");
};

instance ZDISM_W3_ROLL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Roll key", "Roll key", "Roll key", "Klawisz przewrotu");
  text[1] = Str_GetLocalizedString("On ALT: jump alone, roll with side/back key", "On ALT: jump alone, roll with side/back key",
    "On ALT: jump alone, roll with side/back key", "Na ALT: sam = skok, z bokiem/tyłem = przewrót");
};
instance ZDISM_W3_ROLL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuRollKey";
  text[0] = Str_GetLocalizedString("L-ALT|Z|X|C|V|L-CTRL|off|custom (Gothic.ini)", "L-ALT|Z|X|C|V|L-CTRL|off|custom (Gothic.ini)",
    "L-ALT|Z|X|C|V|L-CTRL|off|custom (Gothic.ini)", "lewy ALT|Z|X|C|V|lewy CTRL|wył.|własny (Gothic.ini)");
};

// Kontry (Dodge.cpp) - osobna strona ZDISM_MENU_COUNTER niżej: wł./wył., kontra z odskoku, okno, bark / kopniak.
instance ZDISM_W3_COUNTER(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  flags = flags & ~IT_EFFECTS_NEXT;
  text[0] = Str_GetLocalizedString("Counters and dodges ...", "Counters and dodges ...", "Counters and dodges ...",
    "Kontry i uniki ...");
  text[1] = Str_GetLocalizedString("Counters, double-tap dodge, jump attack window", "Counters, double-tap dodge, jump attack window",
    "Counters, double-tap dodge, jump attack window", "Kontry, unik dwuklikiem, okno ataku z wyskoku");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZDISM_MENU_COUNTER";
};

// Zestaw ruchu do testów: numer pozycji = wartość LocoMode (0..4, kolejność jak zdcore::LocoMode), bez klucza Menu*.
instance ZDISM_W3_LOCOMODE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Movement set (test)", "Movement set (test)", "Movement set (test)", "Zestaw ruchu (test)");
  text[1] = Str_GetLocalizedString("Auto = options above; O cycles in game", "Auto = options above; O cycles in game",
    "Auto = options above; O cycles in game", "Auto = wg opcji wyżej; klawisz O przełącza");
};
instance ZDISM_W3_LOCOMODE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 9;
  onchgsetoption = "LocoMode";
  text[0] = Str_GetLocalizedString("auto|Gothic / zEBA|W3 run|W3 run + sprint|sprint preview",
    "auto|Gothic / zEBA|W3 run|W3 run + sprint|sprint preview", "auto|Gothic / zEBA|W3 run|W3 run + sprint|sprint preview",
    "auto|Gothic / zEBA|W3 trucht|W3 trucht + sprint|podgląd sprintu");
};

// Przeskoki i wspinanie w biegu: numer pozycji (0 wył., 1 poza walką, 2 też w walce) plugin przelicza na klucze
// Traversal i TraversalCombat (Config.cpp, MenuTraversal).
instance ZDISM_W3_TRAV(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Vault and climb", "Vault and climb", "Vault and climb", "Przeskok i wspinanie");
  text[1] = Str_GetLocalizedString("Run (W) + jump (ALT) at a 30-250 cm obstacle", "Run (W) + jump (ALT) at a 30-250 cm obstacle",
    "Run (W) + jump (ALT) at a 30-250 cm obstacle", "Bieg (W) + ALT przed przeszkodą 30-250 cm");
};
instance ZDISM_W3_TRAV_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 10;
  onchgsetoption = "MenuTraversal";
  text[0] = Str_GetLocalizedString("off|outside combat|also in combat", "off|outside combat|also in combat",
    "off|outside combat|also in combat", "wył.|poza walką|też w walce");
};

// Znaki Wiedźmina (runy Aard, Igni, Axii, Yrden, Quen - Signs.cpp): klucz Signs 0/1.
instance ZDISM_W3_SIGNS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Witcher Signs", "Witcher Signs", "Witcher Signs", "Znaki Wiedźmina");
  text[1] = Str_GetLocalizedString("Aard, Igni, Axii, Yrden, Quen runes (console: insert ItRu_Aard ...); off = sign runes do nothing",
    "Aard, Igni, Axii, Yrden, Quen runes (console: insert ItRu_Aard ...); off = sign runes do nothing",
    "Aard, Igni, Axii, Yrden, Quen runes (console: insert ItRu_Aard ...); off = sign runes do nothing",
    "Runy Aard, Igni, Axii, Yrden, Quen (konsola: insert ItRu_Aard ...); wył. = runy znaków nic nie robią");
};
instance ZDISM_W3_SIGNS_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 11;
  onchgsetoption = "Signs";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

// Podnoszenie z dobytą bronią (Loot.cpp): numer pozycji (0 wył., 1 wł., 2 wł. z podpowiedzią) plugin przelicza na
// klucze LootArmed i LootHint (Config.cpp, MenuLoot). Zasięg i promień wrogów tylko w Gothic.ini.
instance ZDISM_W3_LOOT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 12;
  text[0] = Str_GetLocalizedString("Pick up with weapon drawn", "Pick up with weapon drawn", "Pick up with weapon drawn",
    "Podnoszenie z bronią");
  text[1] = Str_GetLocalizedString("Attack picks up items when nobody is fighting", "Attack picks up items when nobody is fighting",
    "Attack picks up items when nobody is fighting", "Atak podnosi przedmiot, gdy nikt obok nie walczy");
};
instance ZDISM_W3_LOOT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 12;
  onchgsetoption = "MenuLoot";
  text[0] = Str_GetLocalizedString("off|on|on + hint", "off|on|on + hint", "off|on|on + hint", "wył.|wł.|wł. + podpowiedź");
};

// 12 wierszy (kontry na osobnej stronie; 13. wiersz przy ZDISM_MENU_DY 380: ostatni na 6340, wybór do 6810; przy 410
// ostatni wybór wchodziłby na "Wstecz" z zUnionMenu.d, posy 6892) - notka pod tytułem. To samo DY zagęszcza stronę odcinania (10 wierszy) i główną.
// 14. wiersz wymaga ZDISM_MENU_DY <= 355 (wybór 350 + odstęp 120 - wiersze zaczną się stykać) albo drugiej strony.
instance ZDISM_W3_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_TITLE_Y + 550;
};


// ---------------------------------------------------------------- kontry (z wiersza 8 strony W3)
// Dodge.cpp. Klucze Counters i CounterOnBackstep 0/1 (numer pozycji = wartość). Okno: pozycja MenuCounterWindow
// plugin przelicza na CounterWindow w s (0 = cały zamach; Config.cpp, realMenu - kolejność jak tutaj),
// bark / kopniak: MenuCounterRepelChance (menuChoices).

instance ZDISM_MENU_COUNTER(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_CNT_HEADLINE";
  items[1]  = "ZDISM_CNT_ON";
  items[2]  = "ZDISM_CNT_ON_CHOICE";
  items[3]  = "ZDISM_CNT_BACKSTEP";
  items[4]  = "ZDISM_CNT_BACKSTEP_CHOICE";
  items[5]  = "ZDISM_CNT_WINDOW";
  items[6]  = "ZDISM_CNT_WINDOW_CHOICE";
  items[7]  = "ZDISM_CNT_REPEL";
  items[8]  = "ZDISM_CNT_REPEL_CHOICE";
  items[9]  = "ZDISM_CNT_DBLTAP";
  items[10] = "ZDISM_CNT_DBLTAP_CHOICE";
  items[11] = "ZDISM_CNT_DBLMOVE";
  items[12] = "ZDISM_CNT_DBLMOVE_CHOICE";
  items[13] = "ZDISM_CNT_HOLD";
  items[14] = "ZDISM_CNT_HOLD_CHOICE";
  items[15] = "ZDISM_CNT_JUMPATK";
  items[16] = "ZDISM_CNT_JUMPATK_CHOICE";
  items[17] = "ZDISM_CNT_NOTE";
  items[18] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_CNT_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Witcher counters and dodges", "Witcher counters and dodges", "Witcher counters and dodges",
    "Kontry i uniki Wiedźmina");
};

instance ZDISM_CNT_ON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Counter on block", "Counter on block", "Counter on block", "Kontra przy bloku");
  text[1] = Str_GetLocalizedString("Block while the enemy swings = parry and riposte", "Block while the enemy swings = parry and riposte",
    "Block while the enemy swings = parry and riposte", "Blok podczas zamachu wroga = odbicie i riposta");
};
instance ZDISM_CNT_ON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "Counters";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_CNT_BACKSTEP(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Counter from backstep", "Counter from backstep", "Counter from backstep", "Kontra z odskoku");
  text[1] = Str_GetLocalizedString("Off: jumping back is a plain backstep", "Off: jumping back is a plain backstep",
    "Off: jumping back is a plain backstep", "Wył.: odskok w tył to zwykły odskok");
};
instance ZDISM_CNT_BACKSTEP_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "CounterOnBackstep";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_CNT_WINDOW(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Counter window", "Counter window", "Counter window", "Okno kontry");
  text[1] = Str_GetLocalizedString("Block at most this long before the enemy hit", "Block at most this long before the enemy hit",
    "Block at most this long before the enemy hit", "Blok najwyżej tyle przed trafieniem wroga");
};
instance ZDISM_CNT_WINDOW_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuCounterWindow";
  text[0] = Str_GetLocalizedString("whole swing|0.2 s|0.3 s|0.4 s|0.5 s|0.7 s", "whole swing|0.2 s|0.3 s|0.4 s|0.5 s|0.7 s",
    "whole swing|0.2 s|0.3 s|0.4 s|0.5 s|0.7 s", "cały zamach|0,2 s|0,3 s|0,4 s|0,5 s|0,7 s");
};

instance ZDISM_CNT_REPEL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Shoulder bash / kick", "Shoulder bash / kick", "Shoulder bash / kick", "Kontra barkiem / kopniakiem");
  text[1] = Str_GetLocalizedString("Share of counters that shove or kick instead", "Share of counters that shove or kick instead",
    "Share of counters that shove or kick instead", "Ile kontr to pchnięcie barkiem / kopniak");
};
instance ZDISM_CNT_REPEL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuCounterRepelChance";
  text[0] = ZDISM_PERCENT;
};

// Alternatywne wywolanie uniku (Dodge.cpp): klucze DoubleTapDodge (0 wył., 1 z bronią, 2 z bronią i Shiftem),
// DoubleTapMove (0 unik, 1 przewrót) i DodgeWithAction (0/1) - numer pozycji = wartość.
instance ZDISM_CNT_DBLTAP(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Double tap dodge", "Double tap dodge", "Double tap dodge", "Unik dwuklikiem");
  text[1] = Str_GetLocalizedString("Tap W/S/A/D (Q/E) twice quickly = move that way", "Tap W/S/A/D (Q/E) twice quickly = move that way",
    "Tap W/S/A/D (Q/E) twice quickly = move that way", "Dwa szybkie W/S/A/D (Q/E) = ruch w tę stronę");
};
instance ZDISM_CNT_DBLTAP_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "DoubleTapDodge";
  text[0] = Str_GetLocalizedString("off|weapon drawn|weapon drawn + Shift", "off|weapon drawn|weapon drawn + Shift",
    "off|weapon drawn|weapon drawn + Shift", "wył.|z dobytą bronią|z bronią + Shift");
};

instance ZDISM_CNT_DBLMOVE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Double tap does", "Double tap does", "Double tap does", "Dwuklik robi");
  text[1] = Str_GetLocalizedString("Short dodge or long roll", "Short dodge or long roll", "Short dodge or long roll",
    "Krótki unik albo długi przewrót");
};
instance ZDISM_CNT_DBLMOVE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "DoubleTapMove";
  text[0] = Str_GetLocalizedString("dodge|roll", "dodge|roll", "dodge|roll", "unik|przewrót");
};

instance ZDISM_CNT_HOLD(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Dodge with attack held", "Dodge with attack held", "Dodge with attack held",
    "Unik z trzymanym atakiem");
  text[1] = Str_GetLocalizedString("Dodge/roll key works while holding attack (LMB)", "Dodge/roll key works while holding attack (LMB)",
    "Dodge/roll key works while holding attack (LMB)", "Klawisz uniku/przewrotu działa przy trzymanym ataku (LPM)");
};
instance ZDISM_CNT_HOLD_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "DodgeWithAction";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

// Okno ataku z wyskoku (Dodge.cpp, zdcore::JumpTapWindowsFor): pozycję MenuJumpAttackWindow plugin przelicza na
// JumpAttackWindow w s (Config.cpp, realMenu - kolejność jak tutaj).
instance ZDISM_CNT_JUMPATK(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Jump attack window", "Jump attack window", "Jump attack window", "Okno ataku z wyskoku");
  text[1] = Str_GetLocalizedString("Attack this soon after jump = leap attack", "Attack this soon after jump = leap attack",
    "Attack this soon after jump = leap attack", "Atak tyle po skoku (Alt) = atak z wyskoku");
};
instance ZDISM_CNT_JUMPATK_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "MenuJumpAttackWindow";
  text[0] = Str_GetLocalizedString("0.2 s|0.3 s|0.4 s|0.5 s|0.6 s", "0.2 s|0.3 s|0.4 s|0.5 s|0.6 s", "0.2 s|0.3 s|0.4 s|0.5 s|0.6 s",
    "0,2 s|0,3 s|0,4 s|0,5 s|0,6 s");
};

instance ZDISM_CNT_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 9;
};


// ---------------------------------------------------------------- animacje broni (zamiana 1H <-> 2H)
// AniSwap.cpp. Klucze WeaponSwap (0 wył., 1 1H jak 2H, 2 2H jak 1H, 3 obie), WeaponSwapLevel (0/1)
// i WeaponSwapPerWeapon (0/1) - numer pozycji = wartość, bez klucza Menu*. Klawisz: pozycja MenuWeaponSwapKey
// (0 Ctrl+Shift+U, 1 wył., 2 własny) plugin przelicza na WeaponSwapKey i WeaponSwapMods (Config.cpp,
// zdcore::SwapKeyMenuApply) - poza tablicą menuChoices, jak MenuLoot.

instance ZDISM_MENU_ANIM(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_ANIM_HEADLINE";
  items[1]  = "ZDISM_ANIM_SWAP";
  items[2]  = "ZDISM_ANIM_SWAP_CHOICE";
  items[3]  = "ZDISM_ANIM_LEVEL";
  items[4]  = "ZDISM_ANIM_LEVEL_CHOICE";
  items[5]  = "ZDISM_ANIM_PERWEAPON";
  items[6]  = "ZDISM_ANIM_PERWEAPON_CHOICE";
  items[7]  = "ZDISM_ANIM_KEY";
  items[8]  = "ZDISM_ANIM_KEY_CHOICE";
  items[9]  = "ZDISM_ANIM_NOTE";
  items[10] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_ANIM_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Weapon animations", "Weapon animations", "Weapon animations", "Animacje broni");
};

instance ZDISM_ANIM_SWAP(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Animation swap", "Animation swap", "Animation swap", "Zamiana animacji");
  text[1] = Str_GetLocalizedString("1H weapon fights with 2H anims or the reverse", "1H weapon fights with 2H anims or the reverse",
    "1H weapon fights with 2H anims or the reverse", "Broń 1H walczy animacjami 2H lub odwrotnie");
};
instance ZDISM_ANIM_SWAP_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "WeaponSwap";
  text[0] = Str_GetLocalizedString("off|1H as 2H|2H as 1H|both", "off|1H as 2H|2H as 1H|both", "off|1H as 2H|2H as 1H|both",
    "wył.|1H jak 2H|2H jak 1H|obie");
};

instance ZDISM_ANIM_LEVEL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Animation level", "Animation level", "Animation level", "Poziom animacji");
  text[1] = Str_GetLocalizedString("Talent of the weapon in hand or the other type", "Talent of the weapon in hand or the other type",
    "Talent of the weapon in hand or the other type", "Talent broni w ręce albo drugiego typu broni");
};
instance ZDISM_ANIM_LEVEL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "WeaponSwapLevel";
  text[0] = Str_GetLocalizedString("weapon in hand|other type", "weapon in hand|other type",
    "weapon in hand|other type", "broń w ręce|drugi typ");
};

instance ZDISM_ANIM_PERWEAPON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Remember per weapon", "Remember per weapon", "Remember per weapon", "Pamiętaj dla broni");
  text[1] = Str_GetLocalizedString("On: swap state saved for each weapon", "On: swap state saved for each weapon",
    "On: swap state saved for each weapon", "Wł.: zamiana zapamiętana osobno dla broni");
};
instance ZDISM_ANIM_PERWEAPON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "WeaponSwapPerWeapon";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_ANIM_KEY(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Swap key", "Swap key", "Swap key", "Klawisz zamiany");
  text[1] = Str_GetLocalizedString("Turns the swap on / off in game", "Turns the swap on / off in game",
    "Turns the swap on / off in game", "Włącza / wyłącza zamianę w grze");
};
instance ZDISM_ANIM_KEY_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuWeaponSwapKey";
  text[0] = Str_GetLocalizedString("Ctrl+Shift+U|off|custom (Gothic.ini)", "Ctrl+Shift+U|off|custom (Gothic.ini)",
    "Ctrl+Shift+U|off|custom (Gothic.ini)", "Ctrl+Shift+U|wył.|własny (Gothic.ini)");
};

instance ZDISM_ANIM_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 5;
};


// ---------------------------------------------------------------- tempo ciosów Wiedźmina
// Ciosy boczne 2H: W3Side2H wprost (0 W3, 1 W3 szybkie, 2 natywne). Tempo: klucze MenuW3AtkSpeed* = numer pozycji,
// plugin przelicza na W3AtkSpeed* (0 = natywne, procenty) - kolejność pozycji MUSI zgadzać się z
// zdcore::AtkSpeedMenuValues (Core\AtkTempo.cpp) i menuChoices w Config.cpp.

instance ZDISM_MENU_ATK(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_ATK_HEADLINE";
  items[1]  = "ZDISM_ATK_SIDE2H";
  items[2]  = "ZDISM_ATK_SIDE2H_CHOICE";
  items[3]  = "ZDISM_ATK_S1HL";
  items[4]  = "ZDISM_ATK_S1HL_CHOICE";
  items[5]  = "ZDISM_ATK_S1HR";
  items[6]  = "ZDISM_ATK_S1HR_CHOICE";
  items[7]  = "ZDISM_ATK_S1HC";
  items[8]  = "ZDISM_ATK_S1HC_CHOICE";
  items[9]  = "ZDISM_ATK_S2HL";
  items[10] = "ZDISM_ATK_S2HL_CHOICE";
  items[11] = "ZDISM_ATK_S2HR";
  items[12] = "ZDISM_ATK_S2HR_CHOICE";
  items[13] = "ZDISM_ATK_S2HC";
  items[14] = "ZDISM_ATK_S2HC_CHOICE";
  items[15] = "ZDISM_ATK_NOTE";
  items[16] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_ATK_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Witcher attack speed", "Witcher attack speed", "Witcher attack speed", "Tempo ciosów Wiedźmina");
};

instance ZDISM_ATK_SIDE2H(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("2H side attacks", "2H side attacks", "2H side attacks", "Ciosy boczne 2H");
  text[1] = Str_GetLocalizedString("Geralt's heavy cuts, fast two-handed, game's", "Geralt's heavy cuts, fast two-handed, game's",
    "Geralt's heavy cuts, fast two-handed, game's", "Mocne cięcia Geralta, szybkie dwuręczne, z gry");
};
instance ZDISM_ATK_SIDE2H_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "W3Side2H";
  text[0] = Str_GetLocalizedString("W3|W3 fast|Gothic (native)", "W3|W3 fast|Gothic (native)",
    "W3|W3 fast|Gothic (native)", "W3|W3 szybkie|natywne Gothic");
};

instance ZDISM_ATK_S1HL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("1H: attack left", "1H: attack left", "1H: attack left", "1H: cios w lewo");
  text[1] = Str_GetLocalizedString("Animation speed; native = like the game style", "Animation speed; native = like the game style",
    "Animation speed; native = like the game style", "Tempo animacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S1HL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuW3AtkSpeed1HLeft";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_S1HR(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("1H: attack right", "1H: attack right", "1H: attack right", "1H: cios w prawo");
  text[1] = Str_GetLocalizedString("Animation speed; native = like the game style", "Animation speed; native = like the game style",
    "Animation speed; native = like the game style", "Tempo animacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S1HR_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuW3AtkSpeed1HRight";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_S1HC(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("1H: combo", "1H: combo", "1H: combo", "1H: kombinacja");
  text[1] = Str_GetLocalizedString("Combo speed; native = like the game style", "Combo speed; native = like the game style",
    "Combo speed; native = like the game style", "Tempo kombinacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S1HC_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuW3AtkSpeed1HCombo";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_S2HL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("2H: attack left", "2H: attack left", "2H: attack left", "2H: cios w lewo");
  text[1] = Str_GetLocalizedString("Animation speed; native = like the game style", "Animation speed; native = like the game style",
    "Animation speed; native = like the game style", "Tempo animacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S2HL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MenuW3AtkSpeed2HLeft";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_S2HR(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("2H: attack right", "2H: attack right", "2H: attack right", "2H: cios w prawo");
  text[1] = Str_GetLocalizedString("Animation speed; native = like the game style", "Animation speed; native = like the game style",
    "Animation speed; native = like the game style", "Tempo animacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S2HR_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuW3AtkSpeed2HRight";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_S2HC(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("2H: combo", "2H: combo", "2H: combo", "2H: kombinacja");
  text[1] = Str_GetLocalizedString("Combo speed; native = like the game style", "Combo speed; native = like the game style",
    "Combo speed; native = like the game style", "Tempo kombinacji; natywne = jak styl gry");
};
instance ZDISM_ATK_S2HC_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuW3AtkSpeed2HCombo";
  text[0] = Str_GetLocalizedString("native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%",
    "native|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%", "natywne|70%|80%|90%|100%|110%|120%|130%|140%|150%|160%|175%|200%");
};

instance ZDISM_ATK_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 8;
};

// ---------------------------------------------------------------- Znaki - strojenie (runda 5)
// Wygenerowane: [notatki lokalne] - kolejność pozycji MUSI zgadzać się z tablicą
// signMenu w Config.cpp (ten sam generator). Choicebox zapisuje numer pozycji do klucza MenuSg*, plugin przelicza
// go na wartość klucza znaku przy wyjściu z menu (wartość spoza listy z Gothic.ini zostaje, dopóki jej nie
// przestawisz). 13 wierszy na stronę - jak strona W3 (ZDISM_MENU_DY 380, ostatni wybór nad "Wstecz").

instance ZDISM_MENU_SIGNS1(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_SG1_HEADLINE";
  items[1]  = "ZDISM_SG1_1";
  items[2]  = "ZDISM_SG1_1_CHOICE";
  items[3]  = "ZDISM_SG1_2";
  items[4]  = "ZDISM_SG1_2_CHOICE";
  items[5]  = "ZDISM_SG1_3";
  items[6]  = "ZDISM_SG1_3_CHOICE";
  items[7]  = "ZDISM_SG1_4";
  items[8]  = "ZDISM_SG1_4_CHOICE";
  items[9]  = "ZDISM_SG1_5";
  items[10] = "ZDISM_SG1_5_CHOICE";
  items[11] = "ZDISM_SG1_6";
  items[12] = "ZDISM_SG1_6_CHOICE";
  items[13] = "ZDISM_SG1_7";
  items[14] = "ZDISM_SG1_7_CHOICE";
  items[15] = "ZDISM_SG1_8";
  items[16] = "ZDISM_SG1_8_CHOICE";
  items[17] = "ZDISM_SG1_9";
  items[18] = "ZDISM_SG1_9_CHOICE";
  items[19] = "ZDISM_SG1_10";
  items[20] = "ZDISM_SG1_10_CHOICE";
  items[21] = "ZDISM_SG1_11";
  items[22] = "ZDISM_SG1_11_CHOICE";
  items[23] = "ZDISM_SG1_12";
  items[24] = "ZDISM_SG1_12_CHOICE";
  items[25] = "ZDISM_SG1_13";
  items[26] = "ZDISM_SG1_13_CHOICE";
  items[27] = "ZDISM_SG1_NOTE";
  items[28] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_SG1_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Signs: range and strength", "Signs: range and strength", "Signs: range and strength", "Znaki: zasięg i siła");
};

instance ZDISM_SG1_1(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Aard - range", "Aard - range", "Aard - range", "Aard - zasięg");
  text[1] = Str_GetLocalizedString("Aard cone length", "Aard cone length", "Aard cone length", "Długość stożka Aard");
};
instance ZDISM_SG1_1_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "MenuSgAardRange";
  text[0] = Str_GetLocalizedString("4 m|5 m|6 m|7 m|8 m|10 m|12 m|15 m", "4 m|5 m|6 m|7 m|8 m|10 m|12 m|15 m", "4 m|5 m|6 m|7 m|8 m|10 m|12 m|15 m", "4 m|5 m|6 m|7 m|8 m|10 m|12 m|15 m");
};

instance ZDISM_SG1_2(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Aard - damage", "Aard - damage", "Aard - damage", "Aard - obrażenia");
  text[1] = Str_GetLocalizedString("Fly damage at power x1", "Fly damage at power x1",
    "Fly damage at power x1", "Obrażenia od lotu przy mocy x1 (fala: x0,8)");
};
instance ZDISM_SG1_2_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuSgAardDamage";
  text[0] = Str_GetLocalizedString("5|10|15|20|30|40|60|80|100|150|200", "5|10|15|20|30|40|60|80|100|150|200", "5|10|15|20|30|40|60|80|100|150|200", "5|10|15|20|30|40|60|80|100|150|200");
};

instance ZDISM_SG1_3(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Igni - range", "Igni - range", "Igni - range", "Igni - zasięg");
  text[1] = Str_GetLocalizedString("Fire cone length", "Fire cone length", "Fire cone length", "Długość stożka ognia");
};
instance ZDISM_SG1_3_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuSgIgniRange";
  text[0] = Str_GetLocalizedString("4.5 m|5.5 m|6.5 m|7.5 m|8.5 m|10 m|12 m|15 m", "4.5 m|5.5 m|6.5 m|7.5 m|8.5 m|10 m|12 m|15 m", "4.5 m|5.5 m|6.5 m|7.5 m|8.5 m|10 m|12 m|15 m", "4,5 m|5,5 m|6,5 m|7,5 m|8,5 m|10 m|12 m|15 m");
};

instance ZDISM_SG1_4(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Igni - stream range", "Igni - stream range", "Igni - stream range", "Igni - zasięg strumienia");
  text[1] = Str_GetLocalizedString("Fire stream (hold)", "Fire stream (hold)", "Fire stream (hold)", "Strumień ognia (przytrzymanie)");
};
instance ZDISM_SG1_4_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuSgIgniStreamRange";
  text[0] = Str_GetLocalizedString("5 m|6 m|7 m|8 m|9 m|10 m|12 m|15 m", "5 m|6 m|7 m|8 m|9 m|10 m|12 m|15 m", "5 m|6 m|7 m|8 m|9 m|10 m|12 m|15 m", "5 m|6 m|7 m|8 m|9 m|10 m|12 m|15 m");
};

instance ZDISM_SG1_5(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Igni - damage", "Igni - damage", "Igni - damage", "Igni - obrażenia");
  text[1] = Str_GetLocalizedString("Fire damage at power x1", "Fire damage at power x1", "Fire damage at power x1", "Obrażenia od ognia przy mocy x1");
};
instance ZDISM_SG1_5_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MenuSgIgniDamage";
  text[0] = Str_GetLocalizedString("20|30|40|50|60|80|100|150|200|250|300", "20|30|40|50|60|80|100|150|200|250|300", "20|30|40|50|60|80|100|150|200|250|300", "20|30|40|50|60|80|100|150|200|250|300");
};

instance ZDISM_SG1_6(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Igni - burn per second", "Igni - burn per second", "Igni - burn per second", "Igni - palenie na sekundę");
  text[1] = Str_GetLocalizedString("HP lost per second of burning (power x1)", "HP lost per second of burning (power x1)",
    "HP lost per second of burning (power x1)", "Obrażenia co sekundę płonięcia (moc x1)");
};
instance ZDISM_SG1_6_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuSgIgniBurnDps";
  text[0] = Str_GetLocalizedString("2 / s|3 / s|5 / s|8 / s|10 / s|15 / s|20 / s|30 / s|50 / s|75 / s|100 / s", "2 / s|3 / s|5 / s|8 / s|10 / s|15 / s|20 / s|30 / s|50 / s|75 / s|100 / s", "2 / s|3 / s|5 / s|8 / s|10 / s|15 / s|20 / s|30 / s|50 / s|75 / s|100 / s", "2 / s|3 / s|5 / s|8 / s|10 / s|15 / s|20 / s|30 / s|50 / s|75 / s|100 / s");
};

instance ZDISM_SG1_7(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Igni - burn time", "Igni - burn time", "Igni - burn time", "Igni - czas palenia");
  text[1] = Str_GetLocalizedString("Burn duration (power x1)", "Burn duration (power x1)", "Burn duration (power x1)", "Jak długo cel płonie (moc x1)");
};
instance ZDISM_SG1_7_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuSgIgniBurnTime";
  text[0] = Str_GetLocalizedString("1 s|2 s|3 s|4 s|5 s|6 s|8 s|10 s", "1 s|2 s|3 s|4 s|5 s|6 s|8 s|10 s", "1 s|2 s|3 s|4 s|5 s|6 s|8 s|10 s", "1 s|2 s|3 s|4 s|5 s|6 s|8 s|10 s");
};

instance ZDISM_SG1_8(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Axii - stun", "Axii - stun", "Axii - stun", "Axii - oszołomienie");
  text[1] = Str_GetLocalizedString("Stun time in combat (power x1)", "Stun time in combat (power x1)", "Stun time in combat (power x1)", "Czas oszołomienia wroga w walce (moc x1)");
};
instance ZDISM_SG1_8_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "MenuSgAxiiStunTime";
  text[0] = Str_GetLocalizedString("2 s|3 s|4 s|5 s|6 s|8 s|10 s", "2 s|3 s|4 s|5 s|6 s|8 s|10 s", "2 s|3 s|4 s|5 s|6 s|8 s|10 s", "2 s|3 s|4 s|5 s|6 s|8 s|10 s");
};

instance ZDISM_SG1_9(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Axii - puppet", "Axii - puppet", "Axii - puppet", "Axii - marionetka");
  text[1] = Str_GetLocalizedString("Puppet time (hold, power x1)", "Puppet time (hold, power x1)",
    "Puppet time (hold, power x1)", "Czas walki wroga po twojej stronie (moc x1)");
};
instance ZDISM_SG1_9_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 9;
  onchgsetoption = "MenuSgAxiiPuppetTime";
  text[0] = Str_GetLocalizedString("5 s|10 s|15 s|20 s|30 s", "5 s|10 s|15 s|20 s|30 s", "5 s|10 s|15 s|20 s|30 s", "5 s|10 s|15 s|20 s|30 s");
};

instance ZDISM_SG1_10(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Yrden - radius", "Yrden - radius", "Yrden - radius", "Yrden - promień");
  text[1] = Str_GetLocalizedString("Circle radius", "Circle radius", "Circle radius", "Promień kręgu");
};
instance ZDISM_SG1_10_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 10;
  onchgsetoption = "MenuSgYrdenRadius";
  text[0] = Str_GetLocalizedString("3 m|4 m|5 m|6 m|8 m", "3 m|4 m|5 m|6 m|8 m", "3 m|4 m|5 m|6 m|8 m", "3 m|4 m|5 m|6 m|8 m");
};

instance ZDISM_SG1_11(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Yrden - duration", "Yrden - duration", "Yrden - duration", "Yrden - czas");
  text[1] = Str_GetLocalizedString("Circle duration (power x1)", "Circle duration (power x1)", "Circle duration (power x1)", "Czas kręgu (moc x1)");
};
instance ZDISM_SG1_11_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 11;
  onchgsetoption = "MenuSgYrdenTime";
  text[0] = Str_GetLocalizedString("8 s|10 s|15 s|20 s|30 s|45 s", "8 s|10 s|15 s|20 s|30 s|45 s", "8 s|10 s|15 s|20 s|30 s|45 s", "8 s|10 s|15 s|20 s|30 s|45 s");
};

instance ZDISM_SG1_12(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 12;
  text[0] = Str_GetLocalizedString("Yrden - slow", "Yrden - slow", "Yrden - slow", "Yrden - spowolnienie");
  text[1] = Str_GetLocalizedString("Enemy tempo in the circle at power x1", "Enemy tempo in the circle at power x1",
    "Enemy tempo in the circle at power x1", "Tempo wrogów w kręgu przy mocy x1");
};
instance ZDISM_SG1_12_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 12;
  onchgsetoption = "MenuSgYrdenSlow";
  text[0] = Str_GetLocalizedString("x0.40|x0.50|x0.55|x0.60|x0.65|x0.70|x0.80", "x0.40|x0.50|x0.55|x0.60|x0.65|x0.70|x0.80", "x0.40|x0.50|x0.55|x0.60|x0.65|x0.70|x0.80", "x0,40|x0,50|x0,55|x0,60|x0,65|x0,70|x0,80");
};

instance ZDISM_SG1_13(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 13;
  text[0] = Str_GetLocalizedString("Yrden - weaken", "Yrden - weaken", "Yrden - weaken", "Yrden - osłabienie");
  text[1] = Str_GetLocalizedString("Armor cut in the circle (power x1)", "Armor cut in the circle (power x1)",
    "Armor cut in the circle (power x1)", "Mniej ochrony wrogów w kręgu (moc x1)");
};
instance ZDISM_SG1_13_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 13;
  onchgsetoption = "MenuSgYrdenArmor";
  text[0] = Str_GetLocalizedString("-10%|-20%|-30%|-40%|-50%|-60%", "-10%|-20%|-30%|-40%|-50%|-60%", "-10%|-20%|-30%|-40%|-50%|-60%", "-10%|-20%|-30%|-40%|-50%|-60%");
};

instance ZDISM_SG1_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_TITLE_Y + 550;
};

instance ZDISM_MENU_SIGNS2(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_SG2_HEADLINE";
  items[1]  = "ZDISM_SG2_1";
  items[2]  = "ZDISM_SG2_1_CHOICE";
  items[3]  = "ZDISM_SG2_2";
  items[4]  = "ZDISM_SG2_2_CHOICE";
  items[5]  = "ZDISM_SG2_3";
  items[6]  = "ZDISM_SG2_3_CHOICE";
  items[7]  = "ZDISM_SG2_4";
  items[8]  = "ZDISM_SG2_4_CHOICE";
  items[9]  = "ZDISM_SG2_5";
  items[10] = "ZDISM_SG2_5_CHOICE";
  items[11] = "ZDISM_SG2_6";
  items[12] = "ZDISM_SG2_6_CHOICE";
  items[13] = "ZDISM_SG2_7";
  items[14] = "ZDISM_SG2_7_CHOICE";
  items[15] = "ZDISM_SG2_8";
  items[16] = "ZDISM_SG2_8_CHOICE";
  items[17] = "ZDISM_SG2_9";
  items[18] = "ZDISM_SG2_9_CHOICE";
  items[19] = "ZDISM_SG2_10";
  items[20] = "ZDISM_SG2_10_CHOICE";
  items[21] = "ZDISM_SG2_11";
  items[22] = "ZDISM_SG2_11_CHOICE";
  items[23] = "ZDISM_SG2_12";
  items[24] = "ZDISM_SG2_12_CHOICE";
  items[25] = "ZDISM_SG2_13";
  items[26] = "ZDISM_SG2_13_CHOICE";
  items[27] = "ZDISM_SG2_NOTE";
  items[28] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_SG2_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Signs: power, time and costs", "Signs: power, time and costs", "Signs: power, time and costs", "Znaki: moc, czas i koszty");
};

instance ZDISM_SG2_1(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Quen - shield time", "Quen - shield time", "Quen - shield time", "Quen - czas tarczy");
  text[1] = Str_GetLocalizedString("At power x1; hits: 1, from x1.5 - 2, from x2 - 3", "At power x1; hits: 1, from x1.5 - 2, from x2 - 3",
    "At power x1; hits: 1, from x1.5 - 2, from x2 - 3", "Moc x1; trafienia: 1, od x1,5 - 2, od x2 - 3");
};
instance ZDISM_SG2_1_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "MenuSgQuenTime";
  text[0] = Str_GetLocalizedString("10 s|20 s|30 s|45 s|60 s", "10 s|20 s|30 s|45 s|60 s", "10 s|20 s|30 s|45 s|60 s", "10 s|20 s|30 s|45 s|60 s");
};

instance ZDISM_SG2_2(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Quen - burst", "Quen - burst", "Quen - burst", "Quen - pęknięcie");
  text[1] = Str_GetLocalizedString("Burst damage (power x1)", "Burst damage (power x1)", "Burst damage (power x1)", "Obrażenia przy pęknięciu tarczy (moc x1)");
};
instance ZDISM_SG2_2_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuSgQuenDamage";
  text[0] = Str_GetLocalizedString("5|10|15|20|30|50|75|100", "5|10|15|20|30|50|75|100", "5|10|15|20|30|50|75|100", "5|10|15|20|30|50|75|100");
};

instance ZDISM_SG2_3(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Power from max mana", "Power from max mana", "Power from max mana", "Moc z maks. many");
  text[1] = Str_GetLocalizedString("Power from max mana (scales damage, times, push)", "Power from max mana (scales damage, times, push)",
    "Power from max mana (scales damage, times, push)", "Moc z maks. many: obrażenia, czasy, odrzut, Quen");
};
instance ZDISM_SG2_3_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuSgPowerPerMana";
  text[0] = Str_GetLocalizedString("off|+0.05 / 100 mana|+0.1 / 100 mana|+0.2 / 100 mana", "off|+0.05 / 100 mana|+0.1 / 100 mana|+0.2 / 100 mana", "off|+0.05 / 100 mana|+0.1 / 100 mana|+0.2 / 100 mana", "wył.|+0,05 / 100 many|+0,1 / 100 many|+0,2 / 100 many");
};

instance ZDISM_SG2_4(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Power - cap", "Power - cap", "Power - cap", "Moc - limit");
  text[1] = Str_GetLocalizedString("Highest sign power", "Highest sign power", "Highest sign power", "Największa moc znaków");
};
instance ZDISM_SG2_4_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuSgPowerMax";
  text[0] = Str_GetLocalizedString("max x1.5|max x2|max x2.5|max x3", "max x1.5|max x2|max x2.5|max x3", "max x1.5|max x2|max x2.5|max x3", "maks. x1,5|maks. x2|maks. x2,5|maks. x3");
};

instance ZDISM_SG2_5(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Alternate mode", "Alternate mode", "Alternate mode", "Tryb alternatywny");
  text[1] = Str_GetLocalizedString("Hold cast longer = alt mode (off = cast at once)", "Hold cast longer = alt mode (off = cast at once)",
    "Hold cast longer = alt mode (off = cast at once)", "Dłuższe trzymanie = tryb alt. (wył. = od razu)");
};
instance ZDISM_SG2_5_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MenuSgAltHold";
  text[0] = Str_GetLocalizedString("off|0.25 s|0.35 s|0.5 s|0.75 s", "off|0.25 s|0.35 s|0.5 s|0.75 s", "off|0.25 s|0.35 s|0.5 s|0.75 s", "wył.|0,25 s|0,35 s|0,5 s|0,75 s");
};

instance ZDISM_SG2_6(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Mana cost - Aard", "Mana cost - Aard", "Mana cost - Aard", "Koszt many - Aard");
  text[1] = Str_GetLocalizedString("Normal sign cost", "Normal sign cost", "Normal sign cost", "Koszt zwykłego znaku");
};
instance ZDISM_SG2_6_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuSgCostAard";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_7(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Mana cost - Igni", "Mana cost - Igni", "Mana cost - Igni", "Koszt many - Igni");
  text[1] = Str_GetLocalizedString("Normal sign cost", "Normal sign cost", "Normal sign cost", "Koszt zwykłego znaku");
};
instance ZDISM_SG2_7_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuSgCostIgni";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_8(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Mana cost - Axii", "Mana cost - Axii", "Mana cost - Axii", "Koszt many - Axii");
  text[1] = Str_GetLocalizedString("Normal sign cost", "Normal sign cost", "Normal sign cost", "Koszt zwykłego znaku");
};
instance ZDISM_SG2_8_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "MenuSgCostAxii";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_9(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Mana cost - Yrden", "Mana cost - Yrden", "Mana cost - Yrden", "Koszt many - Yrden");
  text[1] = Str_GetLocalizedString("Normal sign cost", "Normal sign cost", "Normal sign cost", "Koszt zwykłego znaku");
};
instance ZDISM_SG2_9_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 9;
  onchgsetoption = "MenuSgCostYrden";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_10(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Mana cost - Quen", "Mana cost - Quen", "Mana cost - Quen", "Koszt many - Quen");
  text[1] = Str_GetLocalizedString("Normal sign cost", "Normal sign cost", "Normal sign cost", "Koszt zwykłego znaku");
};
instance ZDISM_SG2_10_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 10;
  onchgsetoption = "MenuSgCostQuen";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_11(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Alt cost - Aard", "Alt cost - Aard", "Alt cost - Aard", "Koszt alternatywny - Aard");
  text[1] = Str_GetLocalizedString("Alternate mode cost", "Alternate mode cost", "Alternate mode cost", "Fala wokół (przytrzymanie)");
};
instance ZDISM_SG2_11_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 11;
  onchgsetoption = "MenuSgAardAltCost";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_12(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 12;
  text[0] = Str_GetLocalizedString("Alt cost - Yrden", "Alt cost - Yrden", "Alt cost - Yrden", "Koszt alternatywny - Yrden");
  text[1] = Str_GetLocalizedString("Alternate mode cost", "Alternate mode cost", "Alternate mode cost", "Pułapka (przytrzymanie)");
};
instance ZDISM_SG2_12_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 12;
  onchgsetoption = "MenuSgYrdenAltCost";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_13(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 13;
  text[0] = Str_GetLocalizedString("Alt cost - Axii", "Alt cost - Axii", "Alt cost - Axii", "Koszt alternatywny - Axii");
  text[1] = Str_GetLocalizedString("Alternate mode cost", "Alternate mode cost", "Alternate mode cost", "Marionetka (przytrzymanie)");
};
instance ZDISM_SG2_13_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 13;
  onchgsetoption = "MenuSgAxiiAltCost";
  text[0] = Str_GetLocalizedString("0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50", "0|5|10|15|20|25|30|40|50");
};

instance ZDISM_SG2_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_TITLE_Y + 550;
};

// ---------------------------------------------------------------- Znaki - poza walką (runda 6)
// Ręcznie (nie z gen_sign_menu.py). SignAardUnlock, SignAxiiCharm i SignFires: choicebox zapisuje 0/1 wprost.
// MenuSgAxiiTranceTime / MenuSgAxiiPickpocketBonus / MenuSgAxiiPickpocketTime: numer pozycji, plugin przelicza
// (tablica signMenu w Config.cpp, ta sama kolejność wartości). Premia kradzieży działa tylko w paczce New Balance
// (skrypt zDismember_NB_Thief.d podmienia RX_GetThieftChance).
// Pozycje 7-9 (20.10): MenuSgPowerInt / MenuSgPowerIntMax - numer pozycji (signMenu w Config.cpp), obrażenia z inteligencji
// tylko w New Balance (zmienna ATR_INTELLECT; w Steam G2 bez skutku); SignHitNeutral - 0/1 wprost.
// Pozycje 10-13 (runda 7): SignPowerSource - 0/1/2 wprost (skalowanie wszystkich obrażeń znaków: mana / INT / oba);
// MenuSgAxiiRange, MenuSgIgniStreamMana, MenuSgQuenActiveMana - numer pozycji (signMenu w Config.cpp).

instance ZDISM_MENU_SIGNS3(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_SG3_HEADLINE";
  items[1]  = "ZDISM_SG3_1";
  items[2]  = "ZDISM_SG3_1_CHOICE";
  items[3]  = "ZDISM_SG3_2";
  items[4]  = "ZDISM_SG3_2_CHOICE";
  items[5]  = "ZDISM_SG3_3";
  items[6]  = "ZDISM_SG3_3_CHOICE";
  items[7]  = "ZDISM_SG3_4";
  items[8]  = "ZDISM_SG3_4_CHOICE";
  items[9]  = "ZDISM_SG3_5";
  items[10] = "ZDISM_SG3_5_CHOICE";
  items[11] = "ZDISM_SG3_6";
  items[12] = "ZDISM_SG3_6_CHOICE";
  items[13] = "ZDISM_SG3_7";
  items[14] = "ZDISM_SG3_7_CHOICE";
  items[15] = "ZDISM_SG3_8";
  items[16] = "ZDISM_SG3_8_CHOICE";
  items[17] = "ZDISM_SG3_9";
  items[18] = "ZDISM_SG3_9_CHOICE";
  items[19] = "ZDISM_SG3_10";
  items[20] = "ZDISM_SG3_10_CHOICE";
  items[21] = "ZDISM_SG3_11";
  items[22] = "ZDISM_SG3_11_CHOICE";
  items[23] = "ZDISM_SG3_12";
  items[24] = "ZDISM_SG3_12_CHOICE";
  items[25] = "ZDISM_SG3_13";
  items[26] = "ZDISM_SG3_13_CHOICE";
  items[27] = "ZDISM_SG3_NOTE";
  items[28] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_SG3_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Signs: out of combat, power, upkeep", "Signs: out of combat, power, upkeep",
    "Signs: out of combat, power, upkeep", "Znaki: poza walką, moc, koszty");
};

instance ZDISM_SG3_1(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Aard - locks", "Aard - locks", "Aard - locks", "Aard - zamki");
  text[1] = Str_GetLocalizedString("Opens pick-lockable locks in the cone (no keys)", "Opens pick-lockable locks in the cone (no keys)",
    "Opens pick-lockable locks in the cone (no keys)", "Otwiera zamki do wytrycha w stożku (bez kluczy)");
};
instance ZDISM_SG3_1_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "SignAardUnlock";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_SG3_2(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Axii - Charm", "Axii - Charm", "Axii - Charm", "Axii - Urok");
  text[1] = Str_GetLocalizedString("Out of combat like Charm scroll: forgets crimes", "Out of combat like Charm scroll: forgets crimes",
    "Out of combat like Charm scroll: forgets crimes", "Poza walką jak zwój Uroku: zapomina winy");
};
instance ZDISM_SG3_2_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "SignAxiiCharm";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_SG3_3(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Axii - pickpocket bonus", "Axii - pickpocket bonus", "Axii - pickpocket bonus", "Axii - premia kradzieży");
  text[1] = Str_GetLocalizedString("Pickpocket chance on Axii target (New Balance)", "Pickpocket chance on Axii target (New Balance)",
    "Pickpocket chance on Axii target (New Balance)", "Szansa kradzieży u celu Axii (New Balance)");
};
instance ZDISM_SG3_3_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuSgAxiiPickpocketBonus";
  text[0] = Str_GetLocalizedString("off|+10%|+20%|+25%|+30%|+40%|+50%", "off|+10%|+20%|+25%|+30%|+40%|+50%",
    "off|+10%|+20%|+25%|+30%|+40%|+50%", "wył.|+10%|+20%|+25%|+30%|+40%|+50%");
};

instance ZDISM_SG3_4(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Axii - bonus time", "Axii - bonus time", "Axii - bonus time", "Axii - czas premii");
  text[1] = Str_GetLocalizedString("How long the pickpocket bonus lasts", "How long the pickpocket bonus lasts",
    "How long the pickpocket bonus lasts", "Jak długo trwa premia kradzieży");
};
instance ZDISM_SG3_4_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuSgAxiiPickpocketTime";
  text[0] = Str_GetLocalizedString("as trance|30 s|60 s|90 s|120 s|180 s", "as trance|30 s|60 s|90 s|120 s|180 s",
    "as trance|30 s|60 s|90 s|120 s|180 s", "jak trans|30 s|60 s|90 s|120 s|180 s");
};

instance ZDISM_SG3_5(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Axii - trance", "Axii - trance", "Axii - trance", "Axii - trans");
  text[1] = Str_GetLocalizedString("Out of combat: human ignores you (power x1)", "Out of combat: human ignores you (power x1)",
    "Out of combat: human ignores you (power x1)", "Poza walką: człowiek cię nie widzi (moc x1)");
};
instance ZDISM_SG3_5_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MenuSgAxiiTranceTime";
  text[0] = Str_GetLocalizedString("off|10 s|20 s|30 s|45 s|60 s", "off|10 s|20 s|30 s|45 s|60 s",
    "off|10 s|20 s|30 s|45 s|60 s", "wył.|10 s|20 s|30 s|45 s|60 s");
};

instance ZDISM_SG3_6(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Sign fires", "Sign fires", "Sign fires", "Ogień znaków");
  text[1] = Str_GetLocalizedString("Aard puts out, Igni lights fires and torches", "Aard puts out, Igni lights fires and torches",
    "Aard puts out, Igni lights fires and torches", "Aard gasi, Igni zapala ogniska i pochodnie");
};
instance ZDISM_SG3_6_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "SignFires";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_SG3_7(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Damage from intelligence", "Damage from intelligence", "Damage from intelligence", "Obrażenia z inteligencji");
  text[1] = Str_GetLocalizedString("Sign damage per 10 INT (New Balance)", "Sign damage per 10 INT (New Balance)",
    "Sign damage per 10 INT (New Balance)", "Obrażenia znaków za każde 10 INT (New Balance)");
};
instance ZDISM_SG3_7_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuSgPowerInt";
  text[0] = Str_GetLocalizedString("off|+5%|+10%|+15%|+20%|+30%", "off|+5%|+10%|+15%|+20%|+30%",
    "off|+5%|+10%|+15%|+20%|+30%", "wył.|+5%|+10%|+15%|+20%|+30%");
};

instance ZDISM_SG3_8(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Intelligence - cap", "Intelligence - cap", "Intelligence - cap", "Inteligencja - limit");
  text[1] = Str_GetLocalizedString("Highest damage multiplier from INT", "Highest damage multiplier from INT",
    "Highest damage multiplier from INT", "Największy mnożnik obrażeń z INT");
};
instance ZDISM_SG3_8_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "MenuSgPowerIntMax";
  text[0] = Str_GetLocalizedString("max x2|max x3|max x5|max x8|max x10", "max x2|max x3|max x5|max x8|max x10",
    "max x2|max x3|max x5|max x8|max x10", "maks. x2|maks. x3|maks. x5|maks. x8|maks. x10");
};

instance ZDISM_SG3_9(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Signs hit neutral people", "Signs hit neutral people", "Signs hit neutral people", "Znaki trafiają neutralnych ludzi");
  text[1] = Str_GetLocalizedString("Aard, Igni, Quen like a spell (a crime)", "Aard, Igni, Quen like a spell (a crime)",
    "Aard, Igni, Quen like a spell (a crime)", "Aard, Igni, Quen jak czar gry (przestępstwo)");
};
instance ZDISM_SG3_9_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 9;
  onchgsetoption = "SignHitNeutral";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_SG3_10(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Sign power scaling", "Sign power scaling", "Sign power scaling", "Skalowanie mocy znaków");
  text[1] = Str_GetLocalizedString("All sign damage and burning; times from mana", "All sign damage and burning; times from mana",
    "All sign damage and burning; times from mana", "Wszystkie obrażenia i palenie; czasy z many");
};
instance ZDISM_SG3_10_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 10;
  onchgsetoption = "SignPowerSource";
  text[0] = Str_GetLocalizedString("max mana|intelligence|both (product)", "max mana|intelligence|both (product)",
    "max mana|intelligence|both (product)", "maks. mana|inteligencja|oba (iloczyn)");
};

instance ZDISM_SG3_11(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Axii - range", "Axii - range", "Axii - range", "Axii - zasięg");
  text[1] = Str_GetLocalizedString("Nearest NPC in a narrow cone, line of sight", "Nearest NPC in a narrow cone, line of sight",
    "Nearest NPC in a narrow cone, line of sight", "Najbliższy NPC w wąskim stożku, bez ścian");
};
instance ZDISM_SG3_11_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 11;
  onchgsetoption = "MenuSgAxiiRange";
  text[0] = Str_GetLocalizedString("5 m|8 m|10 m|12 m|15 m", "5 m|8 m|10 m|12 m|15 m", "5 m|8 m|10 m|12 m|15 m", "5 m|8 m|10 m|12 m|15 m");
};

instance ZDISM_SG3_12(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 12;
  text[0] = Str_GetLocalizedString("Upkeep - Igni stream", "Upkeep - Igni stream", "Upkeep - Igni stream", "Podtrzymanie - strumień Igni");
  text[1] = Str_GetLocalizedString("Mana per second while holding the stream", "Mana per second while holding the stream",
    "Mana per second while holding the stream", "Many na sekundę trzymania strumienia");
};
instance ZDISM_SG3_12_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 12;
  onchgsetoption = "MenuSgIgniStreamMana";
  text[0] = Str_GetLocalizedString("0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s", "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s",
    "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s", "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s");
};

instance ZDISM_SG3_13(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 13;
  text[0] = Str_GetLocalizedString("Upkeep - Quen active", "Upkeep - Quen active", "Upkeep - Quen active", "Podtrzymanie - aktywny Quen");
  text[1] = Str_GetLocalizedString("Mana per second of the active shield", "Mana per second of the active shield",
    "Mana per second of the active shield", "Many na sekundę aktywnej tarczy");
};
instance ZDISM_SG3_13_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 13;
  onchgsetoption = "MenuSgQuenActiveMana";
  text[0] = Str_GetLocalizedString("0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s", "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s",
    "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s", "0/s|2/s|4/s|6/s|8/s|10/s|12/s|15/s|20/s|30/s");
};

instance ZDISM_SG3_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_TITLE_Y + 550;
};


// ---------------------------------------------------------------- medytacja (z głównej strony)
// Meditate.cpp. Klucze Meditate (0/1) i MeditateRegen (0 brak, 1 stopniowa, 2 jak sen) - numer pozycji = wartość.
// Klawisze i godziny: pozycje MenuMeditate* plugin przelicza na MeditateKey / MeditateKeyAlt (kody DIK, ostatnia
// pozycja = własny z Gothic.ini), MeditateSpeed, MeditateRegenHours, MeditateSleepHours (Config.cpp, menuChoices -
// kolejność pozycji jak tutaj). Modyfikator drugiego klawisza tylko w Gothic.ini: MeditateKeyAltMods (1 Ctrl, 2 Shift, 4 Alt).

instance ZDISM_MENU_MEDITATE(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_MED_HEADLINE";
  items[1]  = "ZDISM_MED_ON";
  items[2]  = "ZDISM_MED_ON_CHOICE";
  items[3]  = "ZDISM_MED_KEY";
  items[4]  = "ZDISM_MED_KEY_CHOICE";
  items[5]  = "ZDISM_MED_KEYALT";
  items[6]  = "ZDISM_MED_KEYALT_CHOICE";
  items[7]  = "ZDISM_MED_SPEED";
  items[8]  = "ZDISM_MED_SPEED_CHOICE";
  items[9]  = "ZDISM_MED_REGEN";
  items[10] = "ZDISM_MED_REGEN_CHOICE";
  items[11] = "ZDISM_MED_REGENH";
  items[12] = "ZDISM_MED_REGENH_CHOICE";
  items[13] = "ZDISM_MED_SLEEPH";
  items[14] = "ZDISM_MED_SLEEPH_CHOICE";
  items[15] = "ZDISM_MED_NOTE";
  items[16] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_MED_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Meditation", "Meditation", "Meditation", "Medytacja");
};

instance ZDISM_MED_ON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Meditation", "Meditation", "Meditation", "Medytacja");
  text[1] = Str_GetLocalizedString("Out of combat: kneel, wait, regenerate (key below)", "Out of combat: kneel, wait, regenerate (key below)",
    "Out of combat: kneel, wait, regenerate (key below)", "Poza walką: klęczenie, czekanie, regeneracja (klawisz niżej)");
};
instance ZDISM_MED_ON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "Meditate";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_MED_KEY(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Meditation key", "Meditation key", "Meditation key", "Klawisz medytacji");
  text[1] = Str_GetLocalizedString("Starts meditation when nothing else uses it", "Starts meditation when nothing else uses it",
    "Starts meditation when nothing else uses it", "Zaczyna medytację, gdy nic innego go nie używa");
};
instance ZDISM_MED_KEY_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuMeditateKey";
  text[0] = Str_GetLocalizedString("ENTER|NUM ENTER|off|custom (Gothic.ini)", "ENTER|NUM ENTER|off|custom (Gothic.ini)",
    "ENTER|NUM ENTER|off|custom (Gothic.ini)", "ENTER|ENTER numeryczny|wył.|własny (Gothic.ini)");
};

instance ZDISM_MED_KEYALT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Second key", "Second key", "Second key", "Drugi klawisz");
  text[1] = Str_GetLocalizedString("Taken from the game (comma = torch / movement set)", "Taken from the game (comma = torch / movement set)",
    "Taken from the game (comma = torch / movement set)", "Gra go nie widzi (przecinek = pochodnia / zestaw ruchu)");
};
instance ZDISM_MED_KEYALT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuMeditateKeyAlt";
  text[0] = Str_GetLocalizedString("CTRL + comma|off|custom (Gothic.ini)", "CTRL + comma|off|custom (Gothic.ini)",
    "CTRL + comma|off|custom (Gothic.ini)", "CTRL + przecinek|wył.|własny (Gothic.ini)");
};

instance ZDISM_MED_SPEED(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Waiting speed", "Waiting speed", "Waiting speed", "Tempo czekania");
  text[1] = Str_GetLocalizedString("Time multiplier while waiting (also in the meditation window)", "Time multiplier while waiting (also in the meditation window)",
    "Time multiplier while waiting (also in the meditation window)", "Mnożnik czasu w czekaniu (zmienisz też w oknie medytacji)");
};
instance ZDISM_MED_SPEED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuMeditateSpeed";
  text[0] = "x5|x10|x15|x20|x25|x30|x40|x50|x60|x75|x100";
};

instance ZDISM_MED_REGEN(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Regeneration", "Regeneration", "Regeneration", "Regeneracja");
  text[1] = Str_GetLocalizedString("Health and mana while waiting", "Health and mana while waiting",
    "Health and mana while waiting", "Życie i mana w czasie czekania");
};
instance ZDISM_MED_REGEN_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MeditateRegen";
  text[0] = Str_GetLocalizedString("none|gradual|like sleep", "none|gradual|like sleep", "none|gradual|like sleep",
    "brak|stopniowa|jak sen");
};

instance ZDISM_MED_REGENH(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Gradual: full after", "Gradual: full after", "Gradual: full after", "Stopniowa: pełne po");
  text[1] = Str_GetLocalizedString("Game hours from empty to full health and mana", "Game hours from empty to full health and mana",
    "Game hours from empty to full health and mana", "Godziny gry od zera do pełnego życia i many");
};
instance ZDISM_MED_REGENH_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuMeditateRegenHours";
  text[0] = Str_GetLocalizedString("2 h|4 h|6 h|8 h|10 h|12 h|16 h|24 h", "2 h|4 h|6 h|8 h|10 h|12 h|16 h|24 h",
    "2 h|4 h|6 h|8 h|10 h|12 h|16 h|24 h", "2 godz.|4 godz.|6 godz.|8 godz.|10 godz.|12 godz.|16 godz.|24 godz.");
};

instance ZDISM_MED_SLEEPH(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Like sleep: at least", "Like sleep: at least", "Like sleep: at least", "Jak sen: co najmniej");
  text[1] = Str_GetLocalizedString("Full health and mana after waiting this long", "Full health and mana after waiting this long",
    "Full health and mana after waiting this long", "Pełne życie i mana po tylu godzinach czekania");
};
instance ZDISM_MED_SLEEPH_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuMeditateSleepHours";
  text[0] = Str_GetLocalizedString("1 h|2 h|3 h|4 h|5 h|6 h|8 h|10 h|12 h", "1 h|2 h|3 h|4 h|5 h|6 h|8 h|10 h|12 h",
    "1 h|2 h|3 h|4 h|5 h|6 h|8 h|10 h|12 h", "1 godz.|2 godz.|3 godz.|4 godz.|5 godz.|6 godz.|8 godz.|10 godz.|12 godz.");
};

instance ZDISM_MED_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 8;
};


// ---------------------------------------------------------------- interakcje z bronią w ręku (z głównej strony)
// Loot.cpp, LootInteract.cpp. MenuLoot jak na stronie Wiedźmina (0 wył., 1 wł., 2 wł. z podpowiedzią -> LootArmed
// i LootHint, Config.cpp); wył. wyłącza też drzwi, skrzynie i ciała. Pozostałe klucze: numer pozycji = wartość
// (LootArmedDoors 0 wył. / 1 bez zamka i skryptu / 2 wszystkie, LootArmedChests, LootArmedCorpses, LootArmedRedraw 0/1).
// Zasięg (LootRange) i promień wrogów (LootEnemyRadius) tylko w Gothic.ini.

instance ZDISM_MENU_LOOT(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_LOOT_HEADLINE";
  items[1]  = "ZDISM_LOOT_ITEMS";
  items[2]  = "ZDISM_LOOT_ITEMS_CHOICE";
  items[3]  = "ZDISM_LOOT_DOORS";
  items[4]  = "ZDISM_LOOT_DOORS_CHOICE";
  items[5]  = "ZDISM_LOOT_CHESTS";
  items[6]  = "ZDISM_LOOT_CHESTS_CHOICE";
  items[7]  = "ZDISM_LOOT_CORPSES";
  items[8]  = "ZDISM_LOOT_CORPSES_CHOICE";
  items[9]  = "ZDISM_LOOT_REDRAW";
  items[10] = "ZDISM_LOOT_REDRAW_CHOICE";
  items[11] = "ZDISM_LOOT_NOTE";
  items[12] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_LOOT_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Actions with weapon drawn", "Actions with weapon drawn", "Actions with weapon drawn",
    "Interakcje z bronią w ręku");
};

instance ZDISM_LOOT_ITEMS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Pick up items", "Pick up items", "Pick up items", "Podnoszenie przedmiotów");
  text[1] = Str_GetLocalizedString("Attack picks up items; off = all below off", "Attack picks up items; off = all below off", "Attack picks up items; off = all below off", "Atak podnosi przedmiot; wył. = wszystko wył.");
};
instance ZDISM_LOOT_ITEMS_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "MenuLoot";
  text[0] = Str_GetLocalizedString("off|on|on + hint", "off|on|on + hint", "off|on|on + hint", "wył.|wł.|wł. + podpowiedź");
};

instance ZDISM_LOOT_DOORS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Doors", "Doors", "Doors", "Drzwi");
  text[1] = Str_GetLocalizedString("Unlocked: armed; locked: sheathe, draw again", "Unlocked: armed; locked: sheathe, draw again", "Unlocked: armed; locked: sheathe, draw again", "Bez zamka z bronią; zamek: schowanie i dobycie");
};
instance ZDISM_LOOT_DOORS_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "LootArmedDoors";
  text[0] = Str_GetLocalizedString("off|unlocked only|all", "off|unlocked only|all", "off|unlocked only|all",
    "wył.|bez zamka|wszystkie");
};

instance ZDISM_LOOT_CHESTS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Chests", "Chests", "Chests", "Skrzynie");
  text[1] = Str_GetLocalizedString("Sheathe, normal opening, draw again", "Sheathe, normal opening, draw again", "Sheathe, normal opening, draw again", "Schowanie broni, otwarcie gry, dobycie");
};
instance ZDISM_LOOT_CHESTS_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "LootArmedChests";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_LOOT_CORPSES(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Search bodies", "Search bodies", "Search bodies", "Przeszukiwanie ciał");
  text[1] = Str_GetLocalizedString("Dead and unconscious; window sheathes weapon", "Dead and unconscious; window sheathes weapon", "Dead and unconscious; window sheathes weapon", "Martwi i nieprzytomni; okno chowa broń");
};
instance ZDISM_LOOT_CORPSES_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "LootArmedCorpses";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_LOOT_REDRAW(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Draw weapon again", "Draw weapon again", "Draw weapon again", "Ponowne dobycie broni");
  text[1] = Str_GetLocalizedString("After window / lock, if weapon was sheathed", "After window / lock, if weapon was sheathed", "After window / lock, if weapon was sheathed", "Po oknie / zamku, gdy broń była schowana");
};
instance ZDISM_LOOT_REDRAW_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "LootArmedRedraw";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_LOOT_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 6;
};


// ---------------------------------------------------------------- miecze na plecach (z wiersza 10 strony głównej)
// BackSwords.cpp. Klucze BackSwords, Back1H (0 przy pasie, 1 miecze, 2 cała broń 1H), SecondSword i SecondWithRanged
// (0 ukryty, 1 zamiast łuku) - numer pozycji = wartość. Tempo: pozycje MenuBackDrawSpeed / MenuBackSheatheSpeed
// (50..200 %, zdcore::BackSpeedMenuValues) i MenuBeltDrawSpeed (100..250 %, zdcore::BeltSpeedMenuValues), klawisz:
// MenuSecondSwordKey (G B Y R wył., ostatnia = własny; zdcore::SecondKeyMenuValues) - plugin przelicza je na
// BackDrawSpeed / BackSheatheSpeed / BeltDrawSpeed / SecondSwordKey (Config.cpp, menuChoices - kolejność jak tutaj).

instance ZDISM_MENU_BACK(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_BACK_HEADLINE";
  items[1]  = "ZDISM_BACK_ON";
  items[2]  = "ZDISM_BACK_ON_CHOICE";
  items[3]  = "ZDISM_BACK_1H";
  items[4]  = "ZDISM_BACK_1H_CHOICE";
  items[5]  = "ZDISM_BACK_DRAW";
  items[6]  = "ZDISM_BACK_DRAW_CHOICE";
  items[7]  = "ZDISM_BACK_SHEATHE";
  items[8]  = "ZDISM_BACK_SHEATHE_CHOICE";
  items[9]  = "ZDISM_BACK_BELT";
  items[10] = "ZDISM_BACK_BELT_CHOICE";
  items[11] = "ZDISM_BACK_SECOND";
  items[12] = "ZDISM_BACK_SECOND_CHOICE";
  items[13] = "ZDISM_BACK_KEY";
  items[14] = "ZDISM_BACK_KEY_CHOICE";
  items[15] = "ZDISM_BACK_RANGED";
  items[16] = "ZDISM_BACK_RANGED_CHOICE";
  items[17] = "ZDISM_BACK_NOTE";
  items[18] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_BACK_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Swords on the back", "Swords on the back", "Swords on the back", "Miecze na plecach");
};

instance ZDISM_BACK_ON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Draw from the back (W3)", "Draw from the back (W3)", "Draw from the back (W3)",
    "Dobywanie zza pleców (W3)");
  text[1] = Str_GetLocalizedString("2H and 1H on the back, Geralt's animations", "2H and 1H on the back, Geralt's animations",
    "2H and 1H on the back, Geralt's animations", "2H i 1H na plecach, animacje Geralta");
};
instance ZDISM_BACK_ON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "BackSwords";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_BACK_1H(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("One-handed weapon", "One-handed weapon", "One-handed weapon", "Broń jednoręczna");
  text[1] = Str_GetLocalizedString("At the belt or on the back like Geralt", "At the belt or on the back like Geralt",
    "At the belt or on the back like Geralt", "Przy pasie albo na plecach jak u Geralta");
};
instance ZDISM_BACK_1H_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "Back1H";
  text[0] = Str_GetLocalizedString("belt|swords on back|all on back", "belt|swords on back|all on back",
    "belt|swords on back|all on back", "pas|miecze na plecach|cała 1H na plecach");
};

instance ZDISM_BACK_DRAW(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Draw speed", "Draw speed", "Draw speed", "Tempo dobycia");
  text[1] = Str_GetLocalizedString("From the back; 100% = about 0.6 s", "From the back; 100% = about 0.6 s",
    "From the back; 100% = about 0.6 s", "Zza pleców; 100% = ok. 0,6 s");
};
instance ZDISM_BACK_DRAW_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuBackDrawSpeed";
  text[0] = "50%|75%|100%|125%|150%|175%|200%";
};

instance ZDISM_BACK_SHEATHE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Sheathe speed", "Sheathe speed", "Sheathe speed", "Tempo chowania");
  text[1] = Str_GetLocalizedString("On the back; 100% = about 0.7 s", "On the back; 100% = about 0.7 s",
    "On the back; 100% = about 0.7 s", "Na plecy; 100% = ok. 0,7 s");
};
instance ZDISM_BACK_SHEATHE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuBackSheatheSpeed";
  text[0] = "50%|75%|100%|125%|150%|175%|200%";
};

instance ZDISM_BACK_BELT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Draw at the belt", "Draw at the belt", "Draw at the belt", "Dobycie 1H przy pasie");
  text[1] = Str_GetLocalizedString("Game animation, only faster", "Game animation, only faster", "Game animation, only faster",
    "Animacja z gry, tylko szybsza");
};
instance ZDISM_BACK_BELT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "MenuBeltDrawSpeed";
  text[0] = Str_GetLocalizedString("as in game|125%|150%|175%|200%|250%", "as in game|125%|150%|175%|200%|250%",
    "as in game|125%|150%|175%|200%|250%", "jak w grze|125%|150%|175%|200%|250%");
};

instance ZDISM_BACK_SECOND(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Second sword", "Second sword", "Second sword", "Drugi miecz");
  text[1] = Str_GetLocalizedString("On the back, swapped with a key", "On the back, swapped with a key",
    "On the back, swapped with a key", "Na plecach, zamiana klawiszem");
};
instance ZDISM_BACK_SECOND_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "SecondSword";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_BACK_KEY(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Second sword key", "Second sword key", "Second sword key", "Klawisz drugiego miecza");
  text[1] = Str_GetLocalizedString("Inventory: pick the sword; outside: swap", "Inventory: pick the sword; outside: swap",
    "Inventory: pick the sword; outside: swap", "Ekwipunek: wybór miecza; poza nim: zamiana");
};
instance ZDISM_BACK_KEY_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  onchgsetoption = "MenuSecondSwordKey";
  text[0] = Str_GetLocalizedString("G|B|Y|R|off|custom (Gothic.ini)", "G|B|Y|R|off|custom (Gothic.ini)",
    "G|B|Y|R|off|custom (Gothic.ini)", "G|B|Y|R|wył.|własny (Gothic.ini)");
};

instance ZDISM_BACK_RANGED(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Second sword with a bow", "Second sword with a bow", "Second sword with a bow",
    "Drugi miecz przy łuku");
  text[1] = Str_GetLocalizedString("Bow or crossbow equipped", "Bow or crossbow equipped", "Bow or crossbow equipped",
    "Gdy założony łuk albo kusza");
};
instance ZDISM_BACK_RANGED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "SecondWithRanged";
  text[0] = Str_GetLocalizedString("hidden|replaces the bow", "hidden|replaces the bow", "hidden|replaces the bow",
    "ukryty|zamiast łuku");
};

instance ZDISM_BACK_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 9;
};


// ---------------------------------------------------------------- szybki strzał z kuszy (Crossbow.cpp, 23.10)

instance ZDISM_MENU_XBOW(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_XBOW_HEADLINE";
  items[1]  = "ZDISM_XBOW_ON";
  items[2]  = "ZDISM_XBOW_ON_CHOICE";
  items[3]  = "ZDISM_XBOW_SPEED";
  items[4]  = "ZDISM_XBOW_SPEED_CHOICE";
  items[5]  = "ZDISM_XBOW_SCALE";
  items[6]  = "ZDISM_XBOW_SCALE_CHOICE";
  items[7]  = "ZDISM_XBOW_SHIELD";
  items[8]  = "ZDISM_XBOW_SHIELD_CHOICE";
  items[9]  = "ZDISM_XBOW_DUAL";
  items[10] = "ZDISM_XBOW_DUAL_CHOICE";
  items[11] = "ZDISM_XBOW_HIT";
  items[12] = "ZDISM_XBOW_HIT_CHOICE";
  items[13] = "ZDISM_XBOW_NOTE";
  items[14] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_XBOW_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Quick crossbow shot", "Quick crossbow shot", "Quick crossbow shot", "Szybki strzał z kuszy");
};

instance ZDISM_XBOW_ON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Quick shot (W3)", "Quick shot (W3)", "Quick shot (W3)", "Szybki strzał (W3)");
  text[1] = Str_GetLocalizedString("F with crossbow picked by Shift+wheel", "F with crossbow picked by Shift+wheel",
    "F with crossbow picked by Shift+wheel", "F z kuszą wybraną Shift + kółkiem");
};
instance ZDISM_XBOW_ON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "Crossbow";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_XBOW_SPEED(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Shot speed", "Shot speed", "Shot speed", "Tempo strzału");
  text[1] = Str_GetLocalizedString("100% = bolt after about 0.5 s", "100% = bolt after about 0.5 s",
    "100% = bolt after about 0.5 s", "100% = bełt po ok. 0,5 s");
};
instance ZDISM_XBOW_SPEED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuXbowSpeed";
  text[0] = "70%|85%|100%|115%|130%|150%|175%|200%";
};

instance ZDISM_XBOW_SCALE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Crossbow size in hand", "Crossbow size in hand", "Crossbow size in hand",
    "Wielkość kuszy w dłoni");
  text[1] = Str_GetLocalizedString("Geralt's crossbow is smaller", "Geralt's crossbow is smaller", "Geralt's crossbow is smaller",
    "Kusza Geralta jest mniejsza niż gothicka");
};
instance ZDISM_XBOW_SCALE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuXbowHandScale";
  text[0] = "60%|70%|80%|90%|100%|110%|120%";
};

instance ZDISM_XBOW_SHIELD(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Shot with a shield", "Shot with a shield", "Shot with a shield", "Strzał z tarczą");
  text[1] = Str_GetLocalizedString("New Balance: crossbow next to the shield", "New Balance: crossbow next to the shield",
    "New Balance: crossbow next to the shield", "New Balance: kusza obok tarczy");
};
instance ZDISM_XBOW_SHIELD_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "XbowWithShield";
  text[0] = Str_GetLocalizedString("refused|allowed", "refused|allowed", "refused|allowed", "odmowa|dozwolony");
};

instance ZDISM_XBOW_DUAL(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Shot with two weapons", "Shot with two weapons", "Shot with two weapons",
    "Strzał z dwiema broniami");
  text[1] = Str_GetLocalizedString("New Balance: left weapon hidden for the shot", "New Balance: left weapon hidden for the shot",
    "New Balance: left weapon hidden for the shot", "New Balance: lewa broń ukryta na czas strzału");
};
instance ZDISM_XBOW_DUAL_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "XbowWithDual";
  text[0] = Str_GetLocalizedString("refused|allowed", "refused|allowed", "refused|allowed", "odmowa|dozwolony");
};

instance ZDISM_XBOW_HIT(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("A hit interrupts the shot", "A hit interrupts the shot", "A hit interrupts the shot",
    "Trafienie przerywa strzał");
  text[1] = Str_GetLocalizedString("Before the bolt: no bolt used (as in W3)", "Before the bolt: no bolt used (as in W3)",
    "Before the bolt: no bolt used (as in W3)", "Przed bełtem, bez kosztu (jak w W3)");
};
instance ZDISM_XBOW_HIT_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "XbowHitCancel";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_XBOW_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 7;
};


// ---------------------------------------------------------------- menu kołowe szybkiego wyboru (Wheel.cpp, 23.10)

instance ZDISM_MENU_WHEEL(C_MENU_DEF)
{
  backPic   = ZDISM_MENU_BACK_PIC;
  items[0]  = "ZDISM_WHEEL_HEADLINE";
  items[1]  = "ZDISM_WHEEL_ON";
  items[2]  = "ZDISM_WHEEL_ON_CHOICE";
  items[3]  = "ZDISM_WHEEL_SLOW";
  items[4]  = "ZDISM_WHEEL_SLOW_CHOICE";
  items[5]  = "ZDISM_WHEEL_HOLD";
  items[6]  = "ZDISM_WHEEL_HOLD_CHOICE";
  items[7]  = "ZDISM_WHEEL_SENS";
  items[8]  = "ZDISM_WHEEL_SENS_CHOICE";
  items[9]  = "ZDISM_WHEEL_CLICK";
  items[10] = "ZDISM_WHEEL_CLICK_CHOICE";
  items[11] = "ZDISM_WHEEL_AIM";
  items[12] = "ZDISM_WHEEL_AIM_CHOICE";
  items[13] = "ZDISM_WHEEL_KEY";
  items[14] = "ZDISM_WHEEL_KEY_VALUE";
  items[15] = "ZDISM_WHEEL_WALK";
  items[16] = "ZDISM_WHEEL_WALK_CHOICE";
  items[17] = "ZDISM_WHEEL_NOTE";
  items[18] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZDISM_WHEEL_HEADLINE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_HEADLINE();
  text[0] = Str_GetLocalizedString("Quick selection wheel", "Quick selection wheel", "Quick selection wheel", "Koło wyboru");
};

instance ZDISM_WHEEL_ON(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("Wheel (hold the key)", "Wheel (hold the key)", "Wheel (hold the key)", "Koło (przytrzymaj klawisz)");
  text[1] = Str_GetLocalizedString("Spells and crossbow, key below on this page", "Spells and crossbow, key below on this page",
    "Spells and crossbow, key below on this page", "Czary i kusza, klawisz niżej na tej stronie");
};
instance ZDISM_WHEEL_ON_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 1;
  onchgsetoption = "Wheel";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_WHEEL_SLOW(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("Time with the wheel open", "Time with the wheel open", "Time with the wheel open",
    "Czas przy otwartym kole");
  text[1] = Str_GetLocalizedString("Game speed in % (W3: about 15%)", "Game speed in % (W3: about 15%)",
    "Game speed in % (W3: about 15%)", "Tempo gry w % (W3: ok. 15%)");
};
instance ZDISM_WHEEL_SLOW_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 2;
  onchgsetoption = "MenuWheelSlowmo";
  text[0] = "5%|10%|15%|20%|25%|30%|50%|100%";
};

instance ZDISM_WHEEL_HOLD(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Hold to open", "Hold to open", "Hold to open", "Przytrzymanie klawisza");
  text[1] = Str_GetLocalizedString("Shorter = tap (Caps Lock: walk, Tab: inventory)", "Shorter = tap (Caps Lock: walk, Tab: inventory)",
    "Shorter = tap (Caps Lock: walk, Tab: inventory)", "Krócej = stuknięcie (Caps Lock: chód, Tab: ekwipunek)");
};
instance ZDISM_WHEEL_HOLD_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 3;
  onchgsetoption = "MenuWheelHoldMs";
  text[0] = "0,15 s|0,2 s|0,25 s|0,3 s|0,35 s|0,4 s|0,5 s";
};

instance ZDISM_WHEEL_SENS(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Mouse sensitivity", "Mouse sensitivity", "Mouse sensitivity", "Czułość myszy w kole");
  text[1] = Str_GetLocalizedString("Direction from the centre picks the field", "Direction from the centre picks the field",
    "Direction from the centre picks the field", "Kierunek od środka wybiera pole");
};
instance ZDISM_WHEEL_SENS_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 4;
  onchgsetoption = "MenuWheelMouseSens";
  text[0] = "50%|75%|100%|125%|150%|200%";
};

instance ZDISM_WHEEL_CLICK(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Left click confirms", "Left click confirms", "Left click confirms", "LPM zatwierdza");
  text[1] = Str_GetLocalizedString("Without releasing the key; right click / Esc cancels", "Without releasing the key; right click / Esc cancels",
    "Without releasing the key; right click / Esc cancels", "Bez puszczania klawisza; PPM / Esc anuluje");
};
instance ZDISM_WHEEL_CLICK_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 5;
  onchgsetoption = "WheelClickConfirm";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_WHEEL_AIM(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Time when aiming the crossbow", "Time when aiming the crossbow", "Time when aiming the crossbow",
    "Czas w celowaniu z kuszy");
  text[1] = Str_GetLocalizedString("Holding F with the crossbow (100% = no slow motion)", "Holding F with the crossbow (100% = no slow motion)",
    "Holding F with the crossbow (100% = no slow motion)", "Trzymane F przy kuszy (100% = bez spowolnienia)");
};
instance ZDISM_WHEEL_AIM_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 6;
  onchgsetoption = "MenuXbowAimSlowmo";
  text[0] = "100%|50%|30%|20%|15%|10%";
};

instance ZDISM_WHEEL_KEY(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Wheel key (Enter = assign)", "Wheel key (Enter = assign)", "Wheel key (Enter = assign)", "Klawisz koła (Enter = przypisz)");
  text[1] = Str_GetLocalizedString("Then press a key or mouse button (also mouse 4 / 5); Esc cancels", "Then press a key or mouse button (also mouse 4 / 5); Esc cancels",
    "Then press a key or mouse button (also mouse 4 / 5); Esc cancels", "Potem wciśnij klawisz albo przycisk myszy (też mysz 4 / 5); Esc anuluje");
  onselaction[0]   = 5;   // SEL_ACTION_CONCOMMANDS (stala bywa nieobecna w MENU.DAT modow) - zCMenu::HandleSelAction -> zcon.Evaluate
  onselaction_s[0] = "zdism wheel bind";
};
// tekst ustawia plugin (Game_MenuLoop -> SetText): nazwa klawisza i kolizje albo "Wciśnij klawisz..."
instance ZDISM_WHEEL_KEY_VALUE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 7;
  type  = MENU_ITEM_TEXT;
  flags = flags & ~IT_SELECTABLE;
  text[0] = "...";
};

instance ZDISM_WHEEL_WALK(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_ITEM_TEXT_BASE();
  posy += ZDISM_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Caps Lock tap = walk", "Caps Lock tap = walk", "Caps Lock tap = walk", "Stuknięcie Caps Lock = chód");
  text[1] = Str_GetLocalizedString("Walk toggle like Caps Lock in the game, without the Windows Caps Lock", "Walk toggle like Caps Lock in the game, without the Windows Caps Lock",
    "Walk toggle like Caps Lock in the game, without the Windows Caps Lock", "Przełącznik chodu jak dawny Caps Lock, bez wielkich liter w Windows");
};
instance ZDISM_WHEEL_WALK_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENUITEM_CHOICE_BASE();
  posy += ZDISM_MENU_DY * 8;
  onchgsetoption = "WheelTapWalk";
  text[0] = Str_GetLocalizedString(ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_EN, ZDISM_OFF_ON_PL);
};

instance ZDISM_WHEEL_NOTE(C_MENU_ITEM_DEF)
{
  C_ZDISM_MENU_NOTE();
  posy = ZDISM_MENU_START_Y + ZDISM_MENU_DY * 9;
};
