META
{
  Parser = Menu;
  After = zUnionMenu.d;
};

// Menu zTimeRewind w "Union & Pluginy" (zParserExtender). Kodowanie: UTF-8 bez BOM (jak zDismember_Menu.d).
// Wartości trafiają do Gothic.ini [ZTIMEREWIND] przez choicebox (zapisuje NUMER pozycji). Klucze Menu* plugin
// przelicza na wartości przy wyjściu z menu - kolejność pozycji MUSI zgadzać się z tablicą menuChoices
// w Config.cpp. Pozostałe klucze to 0/1 (wył/wł) albo numer trybu.

const int ZTR_MENU_TEXT_X       = 1000;
const int ZTR_MENU_TEXT_DX      = 8000;
const int ZTR_MENU_TEXT_DY      = 750;
const int ZTR_MENU_CHOICE_X     = 5300;
const int ZTR_MENU_CHOICE_DX    = 2400;
const int ZTR_MENU_CHOICE_DY    = 350;
const int ZTR_MENU_CHOICE_YPLUS = 120;
const int ZTR_MENU_TITLE_Y      = 450;
const int ZTR_MENU_START_Y      = 1200;
const int ZTR_MENU_DY           = 430;

const string ZTR_MENU_ITEM_BACK_PIC   = "";
const string ZTR_MENU_BACK_PIC        = "MENU_INGAME.TGA";
const string ZTR_MENU_CHOICE_BACK_PIC = "MENU_CHOICE_BACK.TGA";

const string ZTR_OFF_ON_EN = "off|on";
const string ZTR_OFF_ON_PL = "wył.|wł.";


// ---------------------------------------------------------------- wpis na liście "Union & Pluginy"

instance MENUITEM_UNION_AUTO_ZTIMEREWIND(C_MENU_ITEM_UNION_DEF)
{
  text[0] = Str_GetLocalizedString(
    "zTimeRewind - Sands of Time rune",
    "zTimeRewind - Sands of Time rune",
    "zTimeRewind - Sands of Time rune",
    "zTimeRewind - runa Piaski Czasu"
  );
  text[1] = Str_GetLocalizedString("Prince of Persia rewind: scope, speed, mana", "Prince of Persia rewind: scope, speed, mana",
    "Prince of Persia rewind: scope, speed, mana", "Cofanie czasu (Prince of Persia): zakres, mana");
  onselaction[0]   = SEL_ACTION_STARTMENU;
  onselaction_s[0] = "ZTR_MENU_MAIN";
};


// ---------------------------------------------------------------- wspólne

instance C_ZTR_MENU_ITEM_TEXT_BASE(C_MENU_ITEM_DEF)
{
  fontname       = MENU_FONT_SMALL;   // etykiety opcji - duza czcionka nie miesci sie przy 800x600
  backpic        = ZTR_MENU_ITEM_BACK_PIC;
  posx           = ZTR_MENU_TEXT_X;
  posy           = ZTR_MENU_START_Y;
  dimx           = ZTR_MENU_TEXT_DX;
  dimy           = ZTR_MENU_TEXT_DY;
  flags          = flags | IT_EFFECTS_NEXT;
  onselaction[0] = SEL_ACTION_UNDEF;
};

instance C_ZTR_MENUITEM_CHOICE_BASE(C_MENU_ITEM_DEF)
{
  backpic               = ZTR_MENU_CHOICE_BACK_PIC;
  type                  = MENU_ITEM_CHOICEBOX;
  fontname              = MENU_FONT_SMALL;
  posx                  = ZTR_MENU_CHOICE_X;
  posy                  = ZTR_MENU_START_Y + ZTR_MENU_CHOICE_YPLUS;
  dimx                  = ZTR_MENU_CHOICE_DX;
  dimy                  = ZTR_MENU_CHOICE_DY;
  onchgsetoptionsection = "ZTIMEREWIND";
  flags                 = flags & ~IT_SELECTABLE;
  flags                 = flags | IT_TXT_CENTER;
};


// ---------------------------------------------------------------- strona

instance ZTR_MENU_MAIN(C_MENU_DEF)
{
  backPic   = ZTR_MENU_BACK_PIC;
  items[0]  = "ZTR_HEADLINE";
  items[1]  = "ZTR_ENABLED";
  items[2]  = "ZTR_ENABLED_CHOICE";
  items[3]  = "ZTR_SCOPE";
  items[4]  = "ZTR_SCOPE_CHOICE";
  items[5]  = "ZTR_BUFFER";
  items[6]  = "ZTR_BUFFER_CHOICE";
  items[7]  = "ZTR_SPEED";
  items[8]  = "ZTR_SPEED_CHOICE";
  items[9]  = "ZTR_MANASTART";
  items[10] = "ZTR_MANASTART_CHOICE";
  items[11] = "ZTR_MANASEC";
  items[12] = "ZTR_MANASEC_CHOICE";
  items[13] = "ZTR_MINREWIND";
  items[14] = "ZTR_MINREWIND_CHOICE";
  items[15] = "ZTR_PUPPET";
  items[16] = "ZTR_PUPPET_CHOICE";
  items[17] = "ZTR_DEATH";
  items[18] = "ZTR_DEATH_CHOICE";
  items[19] = "ZTR_DEATHSEC";
  items[20] = "ZTR_DEATHSEC_CHOICE";
  items[21] = "ZTR_RESTORE";
  items[22] = "ZTR_RESTORE_CHOICE";
  items[23] = "ZTR_DEBUG";
  items[24] = "ZTR_DEBUG_CHOICE";
  items[25] = "ZTR_NOTE";
  items[26] = "UNION_MENUITEM_BACK";
  flags     = flags | MENU_SHOW_INFO;
};

instance ZTR_HEADLINE(C_MENU_ITEM_DEF)
{
  type    = MENU_ITEM_TEXT;
  posx    = 0;
  posy    = ZTR_MENU_TITLE_Y;
  dimx    = 8100;
  flags   = flags & ~IT_SELECTABLE;
  flags   = flags | IT_TXT_CENTER;
  text[0] = Str_GetLocalizedString("zTimeRewind - Sands of Time", "zTimeRewind - Sands of Time",
    "zTimeRewind - Sands of Time", "zTimeRewind - Piaski Czasu");
};

instance ZTR_NOTE(C_MENU_ITEM_DEF)
{
  type     = MENU_ITEM_TEXT;
  posx     = 0;
  posy     = ZTR_MENU_START_Y + ZTR_MENU_DY * 12 + 150;
  dimx     = 8100;
  fontname = MENU_FONT_SMALL;
  flags    = flags & ~IT_SELECTABLE;
  flags    = flags | IT_TXT_CENTER;
  text[0]  = Str_GetLocalizedString("Changes apply on menu exit", "Changes apply on menu exit",
    "Changes apply on menu exit", "Zmiany działają po wyjściu z menu");
};

instance ZTR_ENABLED(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 0;
  text[0] = Str_GetLocalizedString("Plugin", "Plugin", "Plugin", "Plugin");
  text[1] = Str_GetLocalizedString("Whole zTimeRewind on / off", "Whole zTimeRewind on / off",
    "Whole zTimeRewind on / off", "Cały zTimeRewind włączony / wyłączony");
};
instance ZTR_ENABLED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 0;
  onchgsetoption = "Enabled";
  text[0] = Str_GetLocalizedString(ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_PL);
};

instance ZTR_SCOPE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 1;
  text[0] = Str_GetLocalizedString("What rewinds", "What rewinds", "What rewinds", "Co się cofa");
  text[1] = Str_GetLocalizedString("World only, or everything (dialogues, quests)", "World only, or everything (dialogues, quests)",
    "World only, or everything (dialogues, quests)", "Świat albo wszystko (też dialogi, questy)");
};
instance ZTR_SCOPE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 1;
  onchgsetoption = "RewindScope";
  text[0] = Str_GetLocalizedString("world only|everything", "world only|everything", "world only|everything",
    "tylko świat|wszystko");
};

instance ZTR_BUFFER(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 2;
  text[0] = Str_GetLocalizedString("How far back", "How far back", "How far back", "Jak daleko wstecz");
  text[1] = Str_GetLocalizedString("Length of the recorded past", "Length of the recorded past",
    "Length of the recorded past", "Ile sekund przeszłości jest zapamiętywane");
};
instance ZTR_BUFFER_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 2;
  onchgsetoption = "MenuBufferSeconds";
  text[0] = "10 s|20 s|30 s|45 s|60 s";
};

instance ZTR_SPEED(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 3;
  text[0] = Str_GetLocalizedString("Rewind speed", "Rewind speed", "Rewind speed", "Prędkość przewijania");
  text[1] = Str_GetLocalizedString("Seconds rewound per second of holding", "Seconds rewound per second of holding",
    "Seconds rewound per second of holding", "Ile sekund cofa się na sekundę trzymania");
};
instance ZTR_SPEED_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 3;
  onchgsetoption = "MenuRewindSpeed";
  text[0] = "1x|2x|3x|4x|5x";
};

instance ZTR_MANASTART(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 4;
  text[0] = Str_GetLocalizedString("Cast cost", "Cast cost", "Cast cost", "Koszt rzucenia");
  text[1] = Str_GetLocalizedString("Mana taken when the rewind starts", "Mana taken when the rewind starts",
    "Mana taken when the rewind starts", "Mana pobierana na start cofania");
};
instance ZTR_MANASTART_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 4;
  onchgsetoption = "MenuManaStart";
  text[0] = "0|5|10|20|30";
};

instance ZTR_MANASEC(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 5;
  text[0] = Str_GetLocalizedString("Mana per second", "Mana per second", "Mana per second", "Mana za sekundę");
  text[1] = Str_GetLocalizedString("Mana for every second rewound", "Mana for every second rewound",
    "Mana for every second rewound", "Mana za każdą cofniętą sekundę");
};
instance ZTR_MANASEC_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 5;
  onchgsetoption = "MenuManaPerSecond";
  text[0] = "0|2|5|10";
};

instance ZTR_MINREWIND(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 6;
  text[0] = Str_GetLocalizedString("Shortest rewind", "Shortest rewind", "Shortest rewind", "Najkrótsze cofnięcie");
  text[1] = Str_GetLocalizedString("A quick click still rewinds this much", "A quick click still rewinds this much",
    "A quick click still rewinds this much", "Krótkie kliknięcie cofa co najmniej tyle");
};
instance ZTR_MINREWIND_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 6;
  onchgsetoption = "MenuMinRewindSeconds";
  text[0] = "0 s|1 s|2 s|3 s|5 s";
};

instance ZTR_PUPPET(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 7;
  text[0] = Str_GetLocalizedString("Rewind animation", "Rewind animation", "Rewind animation", "Animacje przy cofaniu");
  text[1] = Str_GetLocalizedString("Characters frozen in pose or animated backwards",
    "Characters frozen in pose or animated backwards", "Characters frozen in pose or animated backwards",
    "Postacie w zastygłej pozie albo z animacją od tyłu");
};
instance ZTR_PUPPET_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 7;
  onchgsetoption = "PuppetAnims";
  text[0] = Str_GetLocalizedString("frozen|backwards", "frozen|backwards", "frozen|backwards", "zastygła poza|od tyłu");
};

instance ZTR_DEATH(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 8;
  text[0] = Str_GetLocalizedString("Rewind on death", "Rewind on death", "Rewind on death", "Ratunek przed śmiercią");
  text[1] = Str_GetLocalizedString("With rune and mana, death rewinds time instead", "With rune and mana, death rewinds time instead",
    "With rune and mana, death rewinds time instead", "Z runą w plecaku i maną śmierć cofa czas");
};
instance ZTR_DEATH_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 8;
  onchgsetoption = "AutoRewindOnDeath";
  text[0] = Str_GetLocalizedString(ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_PL);
};

instance ZTR_DEATHSEC(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 9;
  text[0] = Str_GetLocalizedString("Rewind on death by", "Rewind on death by", "Rewind on death by", "Ratunek cofa o");
  text[1] = Str_GetLocalizedString("How far back the rescue rewinds", "How far back the rescue rewinds",
    "How far back the rescue rewinds", "O ile sekund cofa ratunek przed śmiercią");
};
instance ZTR_DEATHSEC_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 9;
  onchgsetoption = "MenuAutoRewindSeconds";
  text[0] = "5 s|10 s|15 s|20 s|30 s";
};

instance ZTR_RESTORE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 10;
  text[0] = Str_GetLocalizedString("Used items back", "Used items back", "Used items back", "Zwrot zużytych rzeczy");
  text[1] = Str_GetLocalizedString("Scrolls, potions, food, arrows used come back",
    "Scrolls, potions, food, arrows used come back", "Scrolls, potions, food, arrows used come back",
    "Po cofnięciu wracają zwoje, mikstury, strzały");
};
instance ZTR_RESTORE_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 10;
  onchgsetoption = "MenuRestoreItems";
  text[0] = Str_GetLocalizedString("off|scrolls|all|from ini", "off|scrolls|all|from ini", "off|scrolls|all|from ini",
    "wył.|zwoje|wszystko|z ini");
};

instance ZTR_DEBUG(C_MENU_ITEM_DEF)
{
  C_ZTR_MENU_ITEM_TEXT_BASE();
  posy += ZTR_MENU_DY * 11;
  text[0] = Str_GetLocalizedString("Diagnostic log", "Diagnostic log", "Diagnostic log", "Log diagnostyczny");
  text[1] = Str_GetLocalizedString("Detailed zTimeRewind.log for testing", "Detailed zTimeRewind.log for testing",
    "Detailed zTimeRewind.log for testing", "Szczegółowy zTimeRewind.log do testów");
};
instance ZTR_DEBUG_CHOICE(C_MENU_ITEM_DEF)
{
  C_ZTR_MENUITEM_CHOICE_BASE();
  posy += ZTR_MENU_DY * 11;
  onchgsetoption = "Debug";
  text[0] = Str_GetLocalizedString(ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_EN, ZTR_OFF_ON_PL);
};
