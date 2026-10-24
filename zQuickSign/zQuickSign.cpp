// zQuickSign - szybki "Znak" jak w Wiedzminie: rzut wybranej runy klawiszem bez przechodzenia
// w tryb magii - z wyciagnieta bronia (miecz zostaje w rece) albo bez broni, takze w biegu
// (Gothic II NotR 2.6, Gothic2.exe). Proof of concept.
//
// Plugin robi to, co ksiega czarow robi przy zwyklym rzucie, tylko bez trybu magii:
//   wcisniecie: oCMag_Book::Spell_Setup(gracz, cel z fokusu, 0) -> oCSpell::Setup
//               oCMag_Book::Spell_Invest() -> oCSpell::Invest; pierwsze wywolanie (manaInvested == 0)
//               od razu przekracza prog czasu i wola Spell_ProcessMana (koszt many)
//   SPL_SENDCAST -> oCMag_Book::Spell_Cast() (zuzycie zwoju, skrypt Spell_Cast_*, oCSpell::Cast)
//   SPL_SENDSTOP / SPL_DONTINVEST -> StopSelectedSpell
//   inny status (czar ladowany, strumien) -> trzymanie klawisza laduje dalej (Spell_Invest co klatke),
//   puszczenie = oCSpell::SetReleaseStatus (Spell_ProcessMana_Release) -> Spell_Cast albo Stop,
//   tak jak przy puszczeniu klawisza w trybie magii. SPL_DONTINVEST w trakcie ladowania (np. duza
//   kula ognia po 3. poziomie albo brak many na nastepny) = naladowany: koniec ladowania, czar
//   czeka w rece na puszczenie klawisza.
// Strumienie New Balance (StreamSpellIds) pobieraja mane, zadaja obrazenia i graja wlasna animacje
// (T_MAGRUN_2_FSCCAST/S_FSCCAST) w petli Game_Loop DLL NB, ktora wymaga fmode == 7 i trzymanego
// klawisza akcji: zinput->GetState(5) / GetState(3) albo lewego przycisku myszy (UNION_ABI.DLL
// 0x103B3ED0, dla SPL_FIRE_STREAM 0x103B9190; w NB 18.10.2026 0x103C44E0 / 0x103C9830 - plugin szuka ich
// po sygnaturach, zQuickSignNb.h). Dlatego przy strumieniu:
//   - fmode udaje 7 przez caly czas trzymania klawisza, z wyjatkiem oryginalnego PC_ActionMove
//     (ruch i walka gracza wybieraja galaz po trybie broni);
//   - oba wejscia do tych funkcji w DLL NB (jmp 0x103B3B43, call 0x1023A6DD; 18.10: 0x103C4143, 0x1024986D)
//     ida przez opakowanie,
//     ktore tylko na czas ich wykonania zglasza akcje/gore/lewy przycisk jako wcisniete (podmienione
//     GetState i GetMouseButtonPressedLeft w vtable zinput). Reszta gry widzi prawdziwe klawisze.
// Przy strumieniu nie gramy T_QSCAST - NB gra swoja animacje rak (T_MAGRUN_2_FSCCAST + FSLCAST), poza
// SPL_GURUMAGE_STREAM (0xBD) i SPL_DARKMAGE_STREAM (0xBF), ktore w NB nie maja animacji - tym dajemy te
// sama. Po strumieniu NB konczy na T_FS?CAST_2_MAGRUN -> S_MAGRUN (postawa magii na cale cialo),
// wiec poza trybem magii przez chwile zatrzymujemy te animacje i wracamy do postawy broni (tylko gdy
// zostala postawa magii - wymuszenie postawy w trakcie chodu blokowalo bohatera na 1-2 s).
// Po puszczeniu klawisza fmode udaje 7 jeszcze przez 2 przebiegi funkcji strumieni NB (max 400 ms),
// juz bez podsuwanego klawisza - NB widzi "puszczony przycisk w trybie magii" i sam konczy strumien
// (dzwiek MFX_ELECTRO_STREAM_CAST, animacje). Bez tego dzwiek potrafil sie zapetlic.
// ManaCostPercent (domyslnie 100) skaluje mane zuzyta przez szybki rzut - na przyszle wydanie jako
// kara za rzucanie z bronia.
// Odnowienie liczone od konca rzutu (natychmiastowy: od rzutu; ladowany: od puszczenia; strumien: od
// zakonczenia przez NB): CooldownMs + krag runy * CooldownPerCircleMs + zuzyta mana * CooldownPerManaMs.
// Nieudana proba (brak many, nic nie rzucone) nie uruchamia odnowienia. CooldownMessage=1 - komunikat
// z pozostalym czasem, gdy klawisz trafi w odnowienie.
// W trybie magii (runa w rece) klawisz dziala tak samo - rzut bez zatrzymywania sie; na czas
// ladowania z klawisza zwykla obsluga klawiszy magii (oCAIHuman::MagicMode) jest pomijana.
// Na czas tych wywolan samo pole fmode (+0x250) udaje tryb magii: Spell_ProcessMana wybiera
// Spell_Logic_* po Npc_GetActiveSpell(self), a oCNpc::GetActiveSpellNr zwraca -1, gdy fmode != 7 -
// bez tego kazdy rzut konczy sie SPL_SENDSTOP. To zapis pola, nie SetWeaponMode - animacje i sloty
// (miecz w rece) zostaja nietkniete.
//
// Animacja (UseAnim=1): przy rzucie T_QSCAST - gest rzutu tylko na tulow i rece, na warstwie 6, wiec
// nogi dalej biegna. Siedzi w nakladce Data\zQuickSign_Anims.vdf (build_anims.py, tylko dla New
// Balance - podmienia bazowe HUMANS.MSB). Bez niej FallbackAniName (domyslnie T_MAGRUN_2_FBTSHOOT -
// rzut Ognistej strzaly, jest w kazdym G2): cale cialo na warstwie 1, wiec gra tylko na postoju -
// w biegu, sprincie albo skoku podmienilaby animacje ruchu i zatrzymala bohatera (wtedy rzut bez
// animacji). Po niej silnik przechodzi w poze S_FBTSHOOT, wiec po FallbackAniMs zatrzymujemy ja
// i wracamy do postawy trybu chodu (_s_walk) - chyba ze silnik juz przejal warstwe 1.
// Gesty Znakow (SignGestures, tylko Gothic II z prywatna paczka zDismember_W3.vdf): gest Geralta zalezny
// od czaru (SignIgni/Quen/Yrden/Axii - listy id, reszta Aard), z mieczem w rece albo bez broni, na warstwie
// SIGN_LAYER (21 - nad zakretami 20, ktore silnik gasi co klatke ruchu), w dwoch wariantach: na postoju
// caly Geralt w miejscu (dlon do przodu niezaleznie od postawy pod spodem - garda W3 ma miednice bokiem),
// w ruchu tylko tulow i rece (nogi biegna dalej); gdy bohater ruszy w trakcie gestu, gest przechodzi na
// wariant ruchu od tej samej klatki. Natychmiastowy czar wylatuje z pchnieciem dloni (SignCastMs). Nakladki
// gestow zakladamy, gdy bohater stoi (zalozenie zatrzymuje wszystkie animacje postaci), i nie zdejmujemy
// (zdjecie tez). Gdy gestu nie da sie zagrac - zwykla animacja jak wyzej. Tryb alternatywny Znaku (zDismember):
// Aard i Yrden graja gest "w ziemie" calym cialem (SIGN_ANIS_ALT; SignAltCastMs / SignAltGestureMs) - bohatera
// w miejscu trzyma i skutek do dotkniecia ziemi odklada zDismember (eksport ZQS_SignGesture). Tryb zwykly Znaku W3
// (puszczenie F przed SignAltHold): gest od razu, rzut z koncem pchniecia dloni (SignCastSync, SignCastSyncMs z klatki
// animacji); przerwanie przed rzutem (trafienie, unik / przeskok / finisher - blokada ruchu, ekwipunek, zmiana runy) =
// bez skutku i bez kosztu, jak w W3 (CastSynced / CancelPendingCast).
// Sam Spell_Setup/Invest/Cast nie zatrzymuje rzucajacego (bez Interrupt, SetMovLock, StopAnisLayerRange,
// zmian sterowania animacja), poza zuzyciem ostatniego czaru w ksiedze - patrz DropRemoveWeaponMsg.
//
// Cel i kierunek (tylko Gothic II; w Gothic 1 cel jak dawniej z GetFocusVob):
//   - fokus gracza silnik zbiera z zakresami trybu broni: bez broni Focus_Normal (300 cm, 30 st.), z bronia biala
//     Focus_Melee (500 cm), wiec z dalszej odleglosci GetFocusVob() == 0 i czar szedl bez celu. Gdy biezacy fokus
//     nie jest dobrym celem czaru, PickTarget raz zbiera fokus jak w trybie magii (Focus_Magic, 3500 cm, tylko cele
//     dobre dla wybranego czaru) i przywraca fokus oraz wroga sprzed (MagicFocus=1);
//   - bez celu oCVisualFX::CalcTrajectory (0x48FAC5) wyznacza punkt docelowy z osi kosci emTrjOriginNode (ZS_RIGHTHAND)
//     w chwili rzutu: gest W3 pcha lewa dlonia, a prawa (z czarem) jest wtedy cofnieta i obrocona, wiec pocisk lecial
//     w bok. AimForward podaje wtedy punkt na osi bohatera, na wysokosci dloni (oCVisualFX::SetTarget(zVEC3&),
//     AimForward=1);
//   - na postoju bohater obraca sie do celu do chwili pchniecia dloni (TurnStep, TurnToTarget=1) - gest postoju
//     gra na cale cialo w miejscu, wiec razem z bohaterem obraca sie i gest;
//   - przy gescie W3 (postoj i ruch) efekt czaru siedzi w lewej dloni i z niej wylatuje: na ten rzut wezel wylotu
//     efektu (orgNode) wskazuje ZS_LEFTHAND zamiast ZS_RIGHTHAND, po Spell_Cast wraca (SignHandToLeft, SignLeftHand=1).
//     Bez gestu (zwykla animacja, Gothic 1, strumienie NB) bez zmian.
//   Gothic Free Aim (freeaiming.vdf) tu nie pomaga: podmienia cel na swoj vob celownika w oCSpell::Setup tylko przy
//   GFA_ACTIVE liczonym co klatke z prawdziwego fmode (tryb magii).
//   - celowanie od wcisniecia F (AimOnPress=1, opis przy AimStart): celownik na srodku ekranu, bohater obraca sie za kamera,
//     rzut przy puszczeniu F w punkt celownika; przy Znakach W3 zDismember pasek ladowania trybu alternatywnego.
//
// Kusza (23.10, XBOW_SLOT_23_10, spec [dokument projektowy - niepublikowany]): za ostatnim Znakiem w Shift + kolku jest
// slot kuszy (XbowSlot=1, gdy zDismember z paczka W3 i zalozona kusza - eksport ZDISM_XbowState). Gdy wybrana jest kusza,
// F strzela zamiast rzucac Znak: wcisniecie = ZDISM_XbowInput(1) (zDismember: siegniecie po kusze, cel automatyczny),
// trzymanie dluzej niz XbowHoldMs (300 ms, W3 ThrowHoldTest) = faza 2 (petla celowania) i celownik czarow, bohater obraca
// sie za kamera, LPM nie atakuje, opcjonalnie kamera dystansowa (XbowAimCamera - fmode udaje 6 tylko na czas
// oCAIHuman::ChangeCamModeBySituation, wywolanie 0x69BC02); puszczenie F = faza 3 (z punktem celownika po progu, bez -
// cel automatyczny), PPM w celowaniu = faza 4 (anulowanie bez strzalu i beltu). Animacje, pocisk, przerwania, odnowienie,
// odmowy i komunikaty prowadzi zDismember (Crossbow.cpp); w trakcie strzalu (ZDISM_XbowBusy) Znaki i LPM sa zablokowane.
// Wybor Shift + 4..0 przelacza z kuszy na rune. Eksporty dla zDismember / menu kolowego: ZQS_Busy, ZQS_Select,
// ZQS_CooldownLeftMs.
//
// Wybor runy pod F bez dobywania (tylko Gothic II, opis przy SelectUpdate): Shift + 4..0 ustawia rune z tego slotu
// jako wybrany czar ksiegi (SelectWithShift=1) - gra nie dobywa wtedy runy (hak bramki klawiszy run w oCGame::HandleEvent,
// 0x6FCFAD); Shift + kolko myszy przelacza po samych znakach W3 (SignWheel=1, SignWheelIds); komunikat "Znak: <nazwa>"
// i etykieta wybranej runy nad paskiem many przy dobytej broni bialej / piesciach (ShowSelected=1).
//
// Hook: wywolanie PC_ActionMove w oCAIHuman::_WalkCycle (0x69AE51) - co klatke, gdy gracz steruje
// postacia, w kazdym trybie broni (nie w dialogu); wywolanie MagicMode w PC_ActionMove (0x69A0CA).
//
// Ustawienia w system\zQuickSign.ini (nie w Gothic.ini - gra nadpisuje go przy wyjsciu).
// Ladowany przez Union z system\autorun (AutorunFilter = dll) jako NB_UNION_PLUGIN_ALLOWED_01.DLL -
// New Balance odrzuca DLL spoza swojej bialej listy nazw (patrz build.cmd).

#include <windows.h>
#include <stdarg.h>
#include <math.h>
#include <psapi.h>   // K32EnumProcessModules (kernel32) - szukanie eksportu zDismember
#include "zQuickSignSelect.h"   // czysta logika wyboru runy (testy: tests_zqs.cpp)
#include "zQuickSignNb.h"       // sygnatury funkcji strumieni w UNION_ABI.DLL (testy: tests_zqs.cpp)

// --- adresy Gothic2.exe 2.6 ---
static DWORD ADDR_PLAYER            = 0x00AB2684; // oCNpc::player
static DWORD ADDR_OGAME             = 0x00AB0884; // oCGame* ogame
static int   OFS_OGAME_TEXTVIEW     = 0x30;       // zCView*, na nim rysuje Daedalusowe PrintScreen
static DWORD ADDR_ZINPUT            = 0x008D1650; // zCInput* zinput
static const int   VT_INPUT_KEYPRESSED    = 0x1C;       // zCInput::KeyPressed(int dik)

// Obrazenia czarow: oCNpc::OnDamage_Hit (0x666610) liczy trafienie jak cios bronia biala, gdy
// atakujacy ma tryb broni <= 4 i desc.pVisualFX == 0 (0x6670AF): rzut koscia na talent broni
// (tryb 3/4; inne tryby = zawsze porazka), a przy porazce obrazenia * NPC_MINIMAL_PERCENT/100
// (0x66CA6B, w G2 10%). Zwykly czar trafia, gdy rzucajacy jest jeszcze w trybie magii (7);
// pocisk z szybkiego rzutu dolatuje, gdy bohater ma juz miecz albo piesci. Na czas liczenia
// trafienia od czaru bohatera (desc.pFXHit != 0) pole fmode udaje 7 - jak przy samym rzucie.
static DWORD ADDR_CALL_ONDAMAGEHIT  = 0x00666513; // oCNpc::OnDamage: call oCNpc::OnDamage_Hit
static int   OFS_DESC_NPCATTACKER   = 0x08;       // oSDamageDescriptor::pNpcAttacker
static int   OFS_DESC_FXHIT         = 0x10;       // oSDamageDescriptor::pFXHit (pocisk czaru)
static int   OFS_DESC_SPELLID       = 0x18;       // oSDamageDescriptor::nSpellID
static int   OFS_DESC_VISUALFX      = 0xD0;       // oSDamageDescriptor::pVisualFX (obrazenia w czasie)
static const int   FMODE_MELEE_MAX        = 4;          // OnDamage_Hit: tryb <= 4 = bron biala
static DWORD ADDR_CALL_ACTIONMOVE   = 0x0069AE51; // _WalkCycle: call oCAIHuman::PC_ActionMove
static DWORD ADDR_CALL_MAGICMODE    = 0x0069A0CA; // PC_ActionMove: call oCAIHuman::MagicMode
static const int   VT_INPUT_GETSTATE      = 0x04;       // float zCInput::GetState(unsigned short logicalKey)
static const int   VT_INPUT_MOUSELEFT     = 0x54;       // int zCInput::GetMouseButtonPressedLeft()
enum { GAME_UP = 3, GAME_ACTION = 5 };

// --- UNION_ABI.DLL (New Balance); DLL ma DYNAMICBASE, a kazda aktualizacja NB przesuwa kod - miejsca strumieni
// szukane po sygnaturach w pamieci DLL (zQuickSignNb.h). Bez UNION_ABI.DLL (Steam, inne mody) nic sie nie lata. ---
static const char* NB_DLL                 = "UNION_ABI.DLL";

static DWORD ADDR_NPC_GETSPELLBOOK  = 0x0073EA00; // oCMag_Book* oCNpc::GetSpellBook()
static DWORD ADDR_NPC_GETFOCUSVOB   = 0x00732BB0; // zCVob* oCNpc::GetFocusVob()
static DWORD ADDR_NPC_GETATTRIBUTE  = 0x0072FF20; // int oCNpc::GetAttribute(int)
static DWORD ADDR_NPC_GETMODEL      = 0x00738720; // zCModel* oCNpc::GetModel()
static DWORD ADDR_BOOK_SELSPELL     = 0x00477780; // oCSpell* oCMag_Book::GetSelectedSpell()
static DWORD ADDR_BOOK_SPELLITEM    = 0x00479C00; // oCItem* oCMag_Book::GetSpellItem(oCSpell*)
static int   OFS_ITEM_MAGCIRCLE     = 0x270;      // int oCItem::mag_circle
static DWORD ADDR_BOOK_SPELLSETUP   = 0x004763A0; // void oCMag_Book::Spell_Setup(oCNpc*, zCVob*, int)
static DWORD ADDR_BOOK_SPELLINVEST  = 0x00476530; // void oCMag_Book::Spell_Invest()
static DWORD ADDR_BOOK_SPELLCAST    = 0x004767A0; // void oCMag_Book::Spell_Cast()
static DWORD ADDR_BOOK_STOPSPELL    = 0x00477910; // void oCMag_Book::StopSelectedSpell()
static DWORD ADDR_SPELL_GETSTATUS   = 0x00486660; // int oCSpell::GetSpellStatus()
static DWORD ADDR_SPELL_RELEASE     = 0x00486670; // void oCSpell::SetReleaseStatus()
static DWORD ADDR_SPELL_GETSPELLID  = 0x00486480; // int oCSpell::GetSpellID()
static DWORD ADDR_MODEL_ANIID       = 0x00612070; // int zCModel::GetAniIDFromAniName(zSTRING const&)
static DWORD ADDR_MODEL_STARTANI    = 0x0057B070; // void zCModel::StartAni(int aniID, int startMode)
static DWORD ADDR_MODEL_ACTIVEANI   = 0x0057AB60; // zCModelAniActive* zCModel::GetActiveAni(int aniID)
static DWORD ADDR_MODEL_STOPANI     = 0x0057ABE0; // void zCModel::StopAni(int aniID)
static DWORD ADDR_MODEL_STOPACTIVE  = 0x0057ACB0; // void zCModel::StopAni(zCModelAniActive*)
static DWORD ADDR_MODEL_FADEACTIVE  = 0x0057F020; // void zCModel::FadeOutAni(zCModelAniActive*) (tylko G2 - gesty)
static DWORD ADDR_MODEL_ANIFROMID   = 0x00472F50; // zCModelAni* zCModel::GetAniFromAniID(int) - z najwyzszej nakladki
static DWORD ADDR_ANIACTIVE_SETFRAME = 0x00576CF0; // void zCModelAniActive::SetActFrame(float)
static DWORD ADDR_MODEL_APPLYOVL    = 0x005787F0; // int zCModel::ApplyModelProtoOverlay(zSTRING const&)
static DWORD ADDR_MODEL_REMOVEOVL   = 0x00578C40; // void zCModel::RemoveModelProtoOverlay(zSTRING const&)
static DWORD ADDR_AI_ISSTANDING     = 0x006ADEE0; // int oCAniCtrl_Human::IsStanding()
static DWORD ADDR_NPC_CHANGEATTR    = 0x0072FF60; // void oCNpc::ChangeAttribute(int, int)

// Cel i kierunek szybkiego rzutu (tylko Gothic II - PickTarget, AimForward, TurnStep)
static const DWORD ADDR_NPC_COLLECTFOCUS  = 0x00733A10; // void oCNpc::CollectFocusVob(int force)
static const DWORD ADDR_NPC_SETFOCUSVOB   = 0x00732B60; // void oCNpc::SetFocusVob(zCVob*)
static const DWORD ADDR_NPC_SETENEMY      = 0x00734BC0; // void oCNpc::SetEnemy(oCNpc*)
static const DWORD ADDR_FOCUS_CURRENT     = 0x00AB0738; // oCNpcFocus* oCNpcFocus::focus - zakresy fokusu biezacego trybu
static const DWORD ADDR_FOCUS_LIST        = 0x00AB06F8; // oCNpcFocus* oCNpcFocus::focuslist[6] (Focus_Normal..Focus_Magic)
static const DWORD ADDR_SPELL_VALIDTARGET = 0x004861D0; // int oCSpell::IsValidTarget(zCVob*)
static const DWORD ADDR_VOB_NODETOWORLD   = 0x00604A50; // zMAT4 zCVob::GetTrafoModelNodeToWorld(zCModelNodeInst*)
static const DWORD ADDR_VOB_HEADINGYWORLD = 0x0061C280; // void zCVob::SetHeadingYWorld(zVEC3 const&) - obrot w poziomie do punktu
static const DWORD ADDR_OBJ_RELEASE       = 0x0040C310; // int zCObject::Release()
static const DWORD ADDR_MODEL_SEARCHNODE  = 0x0057DFF0; // zCModelNodeInst* zCModel::SearchNode(zSTRING const&) - sam odczyt
static const DWORD ADDR_CLASSDEF_NPC      = 0x00AB1E20; // oCNpc::classDef
static const int OFS_NPC_FOCUSVOB   = 0x9AC; // zCVob* oCNpc::focus_vob
static const int OFS_NPC_ENEMY      = 0x498; // oCNpc* oCNpc::enemy
static const int OFS_NPC_HP         = 0x1B8; // int oCNpc::attribute[ATR_HITPOINTS]
static const int OFS_OBJ_REFCTR     = 0x04;  // int zCObject::refCtr
static const int OFS_CLASSDEF_BASE  = 0x3C;  // zCClassDef* zCClassDef::baseClassDef
static const int OFS_VOB_TRAFO      = 0x3C;  // zMAT4 zCVob::trafoObjToWorld (wierszami: pozycja [3][7][11], przod [2][6][10])
static const int OFS_SPELL_EFFECT   = 0x28;  // oCVisualFX* oCSpell::effect
static const int OFS_SPELL_CASTER   = 0x34;  // oCNpc* oCSpell::spellCasterNpc - bez niego IsValidTarget zawsze 0
static const int OFS_SPELL_ALGO     = 0x9C;  // int oCSpell::targetCollectAlgo
static const int OFS_SPELL_AZI      = 0xA8;  // int oCSpell::targetCollectAzi (0 = czar bez fokusu - kierunek)
static const int OFS_FX_TRJRANGE    = 0x1A4; // float oCVisualFX::emTrjTargetRange (m) - lot bez celu
static const int OFS_FX_ORGNODE     = 0x49C; // zCModelNodeInst* oCVisualFX::orgNode (emTrjOriginNode, np. ZS_RIGHTHAND)
static const int OFS_FX_ORIGIN      = 0x4A8; // zCVob* oCVisualFX::origin
static const int OFS_FX_TARGET      = 0x4B0; // zCVob* oCVisualFX::target
static const int VT_FX_SETTARGETPOS = 0x78;  // void oCVisualFX::SetTarget(zVEC3&, int recalc) - target = 0, targetPos = punkt
static const int VT_FX_SETTARGETVOB = 0x74;  // void oCVisualFX::SetTarget(zCVob*, int recalc) - wirtualna tuz przed (oVisFX.h)
// Celowanie od wcisniecia F (Gothic II, adresy G2A z naglowkow ZenGin)
static const DWORD ADDR_WORLD_TRACERAY    = 0x00621FA0; // int __fastcall zCWorld::TraceRayNearestHit(zVEC3 const&, zVEC3 const&, zCVob*, int)
static const int   OFS_WORLD_RAYREPORT    = 0x38;       // zTTraceRayReport zCWorld::traceRayReport (foundHit, foundVob, -, foundIntersection +0x0C)
static const int   OFS_SESSION_CAMVOB     = 0x14;       // zCVob* zCSession::camVob (oCGame)
static const int   AIM_RAY_FLAGS          = 0x4381;     // jak GFA_AimRay: VOB_IGNORE_NO_CD_DYN | POLY_NORMAL | POLY_IGNORE_TRANSP | POLY_TEST_WATER | VOB_IGNORE_PROJECTILES
static const float AIM_RAY_LENGTH         = 3000.0f;
static const DWORD ADDR_SCREEN            = 0x00AB6468; // zCView* screen
static const DWORD ADDR_VIEW_CTOR         = 0x007A5700; // zCView::zCView(int x1, int y1, int x2, int y2, zTviewID)
static const DWORD ADDR_VIEW_INSERTBACK   = 0x007A6130; // void zCView::InsertBack(zSTRING const&)
static const DWORD ADDR_VIEW_SETPOS       = 0x007A75B0; // void zCView::SetPos(int, int)
static const DWORD ADDR_VIEW_SETSIZE      = 0x007A77A0; // void zCView::SetSize(int, int)
static const DWORD ADDR_VIEW_INSERTITEM   = 0x007ABAD0; // void zCView::InsertItem(zCView*, int)
static const DWORD ADDR_VIEW_REMOVEITEM   = 0x007ABD10; // void zCView::RemoveItem(zCView*)
static const DWORD ADDR_VIEW_ANX          = 0x007A5E80; // int zCView::anx(int) - piksele -> jednostki wirtualne
static const DWORD ADDR_VIEW_ANY          = 0x007A5EC0; // int zCView::any(int)
static const DWORD ADDR_VIEW_SETCOLOR     = 0x007A6080; // void zCView::SetColor(zCOLOR const&) - zCView::Blit barwi nim teksture tla
static const DWORD AIM_COLOR_NORMAL       = 0xFFFFFFFF; // zCOLOR (bajty b,g,r,a = 0xAARRGGBB): bez barwienia
static const DWORD AIM_COLOR_CHARGE       = 0xFFC8E6FF; // pasek ladowania - jasnoniebieski
static const DWORD AIM_COLOR_ALT          = 0xFFFF7818; // tryb alternatywny Znaku - celownik i pasek pomaranczowe
static const int   TARGET_COLLECT_FOCUS_ALGO_MASK = (1 << 2) | (1 << 4) | (1 << 5); // FOCUS, FOCUS_FALLBACK_NONE / _CASTER
enum { FOCUS_MAGIC = 5 };
enum { TARGET_COLLECT_FOCUS = 2, TARGET_COLLECT_FOCUS_FALLBACK_NONE = 4, TARGET_COLLECT_FOCUS_FALLBACK_CASTER = 5 };
static const float TURN_RAD_PER_MS = 0.0063f; // obrot do celu: ok. 360 st./s (45 st. w 125 ms - przed pchnieciem dloni)
static const float TURN_DONE_RAD   = 0.03f;   // ok. 2 st. - na wprost
static const DWORD TURN_MAX_MS     = 800;

// Wybor runy pod F bez dobywania (tylko Gothic II, opis przy SelectUpdate)
static const DWORD ADDR_CALL_SPELLKEYGATE = 0x006FCFAD; // oCGame::HandleEvent, case DIK_4..DIK_0: call 0x6805C0 (bramka klawiszy run)
static const DWORD ADDR_BOOK_NOOFSPELLBYKEY = 0x00479CE0; // int oCMag_Book::GetNoOfSpellByKey(int keyNo) - indeks + 1, 0 = brak
static const DWORD ADDR_BOOK_SETFRONTSPELL  = 0x004789C0; // void oCMag_Book::SetFrontSpell(int) - poza trybem magii tylko spellnr
static const DWORD ADDR_NPC_HASMAGIC      = 0x0073D380; // int oCNpc::HasMagic()
static const DWORD ADDR_NPC_GETANICTRL    = 0x00734B40; // oCAniCtrl_Human* oCNpc::GetAnictrl()
static const DWORD ADDR_NPC_INTERACTMOB   = 0x0074ACA0; // oCMobInter* oCNpc::GetInteractMob()
static const DWORD ADDR_INFOMAN_GET       = 0x0065F790; // static oCInformationManager& GetInformationManager()
static const DWORD ADDR_INFOMAN_FINISHED  = 0x006609D0; // int oCInformationManager::HasFinished() - 0 = dialog trwa
static const DWORD ADDR_ZCON              = 0x00AB3860; // zCConsole zcon (obiekt)
static const DWORD ADDR_CON_ISVISIBLE     = 0x00783890; // int zCConsole::IsVisible()
static const DWORD ADDR_MENU_GETACTIVE    = 0x004DDD60; // static zCMenu* zCMenu::GetActive() - menu, status, dziennik
static const DWORD ADDR_VIEW_VTABLE       = 0x0083E30C; // zCView::`vftable'{for zCViewBase} - 12 wpisow, [-1] RTTI
static const DWORD ADDR_VIEW_DRAWITEMS    = 0x007A6750; // void zCView::DrawItems() - wpis 11 vtable
static const DWORD ADDR_VIEW_SETFONT      = 0x007A9920; // void zCView::SetFont(zSTRING const&)
static const DWORD ADDR_VIEW_CREATETEXT   = 0x007A7AB0; // zCViewText* zCView::CreateText(int, int, zSTRING const&, float, zCOLOR&, int timed, int colored)
static const DWORD ADDR_VIEW_CLRPRINTWIN  = 0x007AA140; // void zCView::ClrPrintwin()
static const DWORD ADDR_VIEW_FONTSIZE     = 0x007A9A10; // int zCView::FontSize(zSTRING&) - szerokosc w jednostkach widoku
static const DWORD ADDR_VIEW_FONTY        = 0x007A99F0; // int zCView::FontY()
static const int   VT_INPUT_GETMOUSEPOS   = 0x50;       // void zCInput::GetMousePos(float&, float&, float& z) - z: kolko
static const int   OFS_BOOK_SPELLNR       = 0x24;       // int oCMag_Book::spellnr - wybrany czar (indeks)
static const int   OFS_BOOK_OPEN          = 0x48;       // int oCMag_Book::open
static const int   OFS_SPELL_KEYNO        = 0x24;       // int oCSpell::keyNo (1..7 = klawisze 4..0)
static const int   OFS_ITEM_NAME          = 0x124;      // zSTRING oCItem::name ("Runa")
static const int   OFS_ITEM_DESC          = 0x274;      // zSTRING oCItem::description ("Ognista strzala", "Znak Aard")
static const int   OFS_ANICTRL_ACTION     = 0x14C;      // int oCAniCtrl_Human::actionMode (5 = plywanie - gra nie zmienia run)
static const int   OFS_OGAME_MANABAR      = 0x94;       // oCViewStatusBar* oCGame::manaBar
static const int   OFS_OGAME_SHOWSTATUS   = 0x9C;       // int oCGame::showPlayerStatus (0 w dialogu, przerywnikach)
static const int   OFS_OGAME_SINGLESTEP   = 0x11C;      // int oCGame::singleStep (pauza)
static const int   OFS_VIEW_ONDESK        = 0x1C;       // int zCView::ondesk - wstawiony do rodzica
static const int   OFS_VIEW_VPOS          = 0x44;       // int zCView::vposx, vposy, vsizex, vsizey (jednostki rodzica)
static const DWORD SEL_COLOR_LABEL        = 0xFFFFE6A0; // etykieta wybranej runy - jasnozloty
static const DWORD SEL_COLOR_MSG          = 0xFFFFFFFF;
static const char* SEL_FONT               = "FONT_OLD_10_WHITE.TGA";
static const float SEL_MSG_MS             = 1500.0f;
enum { DIK_LSHIFT = 0x2A, DIK_RSHIFT = 0x36, ANI_ACTION_SWIM = 5 };

static DWORD ADDR_ZSTRING_CTOR      = 0x004010C0; // zSTRING::zSTRING(char const*)
static DWORD ADDR_ZSTRING_DTOR      = 0x00401160; // zSTRING::~zSTRING()
static DWORD ADDR_PRINTTIMEDCXY     = 0x007A7FC0; // zCView::PrintTimedCXY(zSTRING const&, float, zCOLOR*)

static int OFS_AI_NPC         = 0x12C; // oCAIHuman (oCAniCtrl_Human) -> oCNpc* npc
static int OFS_NPC_FMODE      = 0x250; // int fmode
static int OFS_NPC_INVENTORY  = 0x668; // oCNpcInventory
static int VT_CONTAINER_ISOPEN = 0x2C; // oCItemContainer::IsOpen()
static int OFS_SPELL_MANAINV  = 0x48;  // int manaInvested
static const int ATR_MANA           = 2;

// animacje (G1 i G2A tak samo)
static const int OFS_ANI_ID          = 0x4C;   // int zCModelAni::aniID
static const int OFS_ANI_LAYER       = 0x6C;   // int zCModelAni::layer
static const int OFS_MODEL_NUMACTIVE = 0x34;   // int zCModel::numActiveAnis
static const int OFS_MODEL_ACTIVE    = 0x38;   // zCModelAniActive* zCModel::aniChannels[6]
static const int OFS_ACTIVE_FADING   = 0x1C;   // int zCModelAniActive::isFadingOut
static const int OFS_ACTIVE_ACTFRAME = 0x0C;   // float zCModelAniActive::actFrame
static const int OFS_AI_WALKMODE     = 0x160;  // int oCAniCtrl_Human::walkmode
static const int OFS_AI_S_WALK       = 0x1008; // int oCAniCtrl_Human::_s_walk - postawa biezacego trybu chodu i broni
enum { ANI_WALKMODE_SNEAK = 2 };               // bieg 0, chod 1, skradanie 2, dalej brodzenie/plywanie/nurkowanie

// Ostatni czar z ksiegi (tylko Gothic II): oCMsgWeapon(EV_REMOVEWEAPON) w kolejce bohatera (DropRemoveWeaponMsg)
static const int OFS_VOB_EM         = 0xFC;       // zCEventManager* zCVob::eventManager (G2A)
static const int OFS_BOOK_NSPELLS   = 0x08;       // oCMag_Book::spells.numInArray
static const int VT_EM_NUMMSG       = 0x48;       // int zCEventManager::GetNumMessages()
static const int VT_EM_GETMSG       = 0x4C;       // zCEventMessage* zCEventManager::GetEventMessage(int)
static const int VT_EM_DELETE       = 0x6C;       // void zCEventManager::Delete(zCEventMessage*) - z listy + zwolnienie
static DWORD ADDR_CLASSDEF_MSGWEAPON = 0x00AB2CC0; // oCMsgWeapon::classDef (vtable +0 _GetClassDef)
static const int OFS_MSG_SUBTYPE    = 0x24;       // WORD zCEventMessage::subType
static const int OFS_NPCMSG_FLAGS   = 0x40;       // oCNpcMessage: bit 2 inUse (juz wykonywana)
enum { EV_REMOVEWEAPON = 3 };

enum { FMODE_MAGIC = 7 };
// statusy Spell_Logic (Constants.d)
enum { SPL_DONTINVEST = 0, SPL_RECEIVEINVEST = 1, SPL_SENDCAST = 2, SPL_SENDSTOP = 3, SPL_NEXTLEVEL = 4,
       SPL_CANINVEST_NO_MANADEC = 8 };

// __thiscall wolany jako __fastcall z pustym edx
typedef int   (__fastcall* ActionMove_t)(void* self, void* edx, int arg);
typedef int   (__fastcall* GetInt_t)(void* self, void* edx);
typedef void* (__fastcall* GetPtr_t)(void* self, void* edx);
typedef void  (__fastcall* Void_t)(void* self, void* edx);
typedef void  (__fastcall* SpellSetup_t)(void* self, void* edx, void* npc, void* target, int arg);
typedef void  (__fastcall* SpellSetupG1_t)(void* self, void* edx, void* npc, void* target);
typedef int   (__fastcall* GetAttribute_t)(void* self, void* edx, int index);
typedef int   (__fastcall* AniId_t)(void* self, void* edx, void* name);
typedef void  (__fastcall* StartAni_t)(void* self, void* edx, int aniID, int startMode);
typedef int   (__fastcall* KeyPressed_t)(void* self, void* edx, int dik);
typedef void  (__fastcall* ZStringCtor_t)(void* self, void* edx, const char* text);
typedef void  (__fastcall* ZStringDtor_t)(void* self, void* edx);
typedef void  (__fastcall* PrintTimedCXY_t)(void* self, void* edx, void* text, float timeMs, void* color);

typedef float (__fastcall* GetState_t)(void* self, void* edx, unsigned short key);
typedef void* (__fastcall* ActiveAni_t)(void* self, void* edx, int aniID);
typedef void  (__fastcall* StopAni_t)(void* self, void* edx, int aniID);
typedef void  (__fastcall* ChangeAttr_t)(void* self, void* edx, int index, int value);
typedef void* (__fastcall* GetSpellItem_t)(void* self, void* edx, void* spell);
typedef int   (__fastcall* ApplyOverlay_t)(void* self, void* edx, void* name);
typedef void  (__fastcall* SetActFrame_t)(void* self, void* edx, float frame);
typedef void  (__fastcall* RemoveOverlay_t)(void* self, void* edx, void* name);
typedef void* (__fastcall* PtrByInt_t)(void* self, void* edx, int index);
typedef void  (__fastcall* VoidByPtr_t)(void* self, void* edx, void* ptr);
typedef void  (__fastcall* VoidByInt_t)(void* self, void* edx, int value);
typedef int   (__fastcall* IntByPtr_t)(void* self, void* edx, void* ptr);
typedef void  (__fastcall* SetTargetPos_t)(void* self, void* edx, float* pos, int recalc);
typedef void* (__fastcall* NodeToWorld_t)(void* self, void* edx, float* trafo, void* node); // zMAT4 przez ukryty wskaznik
typedef void* (__fastcall* PtrByPtr_t)(void* self, void* edx, void* ptr);
typedef int   (__fastcall* TraceRay_t)(void* world, const float* from, const float* dir, void* ignore, int flags);
typedef void  (__fastcall* ViewCtor_t)(void* self, void* edx, int x1, int y1, int x2, int y2, int id);
typedef void  (__fastcall* VoidByTwoInt_t)(void* self, void* edx, int a, int b);
typedef void  (__fastcall* InsertItem_t)(void* self, void* edx, void* item, int a);
typedef int   (__fastcall* IntByInt_t)(void* self, void* edx, int value);
typedef void  (__fastcall* SetTargetVob_t)(void* self, void* edx, void* vob, int recalc);
typedef void* (__cdecl* GetPtrStatic_t)();
typedef void* (__fastcall* CreateText_t)(void* self, void* edx, int x, int y, void* text, float timeMs, void* color, int timed,
	int colored);
typedef void  (__fastcall* GetMousePos_t)(void* self, void* edx, float* x, float* y, float* z);

static ActionMove_t origActionMove;
static GetInt_t     origMagicMode;
static GetState_t   origGetState;
static GetInt_t     origMouseLeft;

// ustawienia
static int  castKey = 0x21;    // DIK_F
static int  cooldownMs = 500;  // odnowienie - baza
static int  cooldownPerCircleMs = 0; // + za krag runy
static int  cooldownPerManaMs = 0;   // + za punkt zuzytej many
static bool cooldownMessage = true;
static bool useAnim = true;
static char aniName[64] = "T_QSCAST";
static char fallbackAniName[64] = "T_MAGRUN_2_FBTSHOOT";
static int  fallbackAniMs = 320;
static char streamIdsText[256] = "174,180,186,187,188,189,190,191"; // SPL_*STREAM z New Balance
static int  streamIds[32];
static int  streamIdCount;
static int  manaPercent = 100; // zuzycie many przez szybki rzut, % zwyklego
static bool spellDamageFix = true; // pelne obrazenia czarow trafiajacych, gdy bohater ma bron biala
static bool magicFocus = true;     // cel z zasiegiem fokusu magii (PickTarget)
static bool aimForward = true;     // bez celu czar prosto przed bohatera (AimForward)
static bool turnToTarget = true;   // na postoju bohater obraca sie do celu (TurnStep)
static bool signLeftHand = true;   // przy gescie W3 czar wylatuje z lewej dloni (SignHandToLeft)
// Celowanie od wcisniecia F (Gothic II): celownik czarow (tekstura Gothic Free Aim, rozmiar z Gothic.ini [GFA] reticleSizePx),
// bohater obraca sie za kamera, rzut przy puszczeniu F w punkt celownika (AimOnPress=1; 0 = rzut przy wcisnieciu, auto-cel).
static bool aimOnPress = true;
static char aimReticleTex[64] = "GFA_RETICLECIRCLECROSS.TGA";
static int  aimReticlePx = 64;
// Wybor runy pod F bez dobywania (Gothic II): Shift + 4..0, Shift + kolko po znakach W3, etykieta wybranej runy
static bool selectWithShift = true;
static bool signWheel = true;
static bool showSelected = true;
static char signWheelIdsText[128] = "90,91,92,93,94";   // SPL_Aard..SPL_Quen z zDismember_Signs.d
static int  signWheelIds[32];
static int  signWheelIdCount;
static bool xbowSlot = true;       // XbowSlot: slot kuszy zDismember w Shift + kolku (za ostatnim Znakiem)
static int  xbowHoldMs = 300;      // XbowHoldMs: dluzsze trzymanie F przy kuszy = celownik (W3 ThrowHoldTest 0,3 s)
static bool xbowAimCamera = true;  // XbowAimCamera: w celowaniu z kuszy kamera dystansowa (zza ramienia)
static bool XbowBusy();            // strzal z kuszy w toku (zDismember ZDISM_XbowBusy) - definicja przy QuickCastBusy
static int  zebaWheel;             // zExtendedBattleAnimations z EnableMouseWheel=1 (Shift + kolko = styl walki) - do logu
static bool isGothic1;             // GothicMod.exe G1 1.08k (Othello) - adresy z InitGothic1
static bool debugLog;          // system\zQuickSign.log
static char iniPath[MAX_PATH];
static char logPath[MAX_PATH];

// stan
static DWORD cooldownUntil;     // GetTickCount, od kiedy wolno rzucic
static int   castCircle;        // krag runy biezacego rzutu
static int   manaSpentTotal;    // mana zuzyta przez biezacy rzut
static bool  castHappened;      // biezacy rzut cos wypuscil (Spell_Cast)
static bool  keyWasDown;
static bool  investing;        // czar ladowany / strumien - trzymany klawisz
static bool  charged;          // naladowany do konca, czeka na puszczenie klawisza
static bool  aiming;           // F trzymane (AimOnPress): rzut przy puszczeniu, klawisz akcji zablokowany
static bool  aimInstant;       // czar natychmiastowy (SENDCAST przy Invest) czeka w rece na puszczenie F
static bool  aimReticleOn;     // celownik widoczny (czar z kierunkiem): obrot za kamera, rzut w punkt celownika
static bool  aimAltOn;         // Znak W3 przeszedl w tryb alternatywny (kolor celownika i paska)
static int   aimSignState = -1; // ostatni wynik ZDISM_SignHoldState (0 = nie Znak W3, -1 = jeszcze nie pytano)
static DWORD aimStartTick;
static void* aimView;          // zCView celownika (wlasny - celownik GFA rysuje tylko tryb magii GFA)
static bool  aimViewShown;
static void* aimBarBack;       // zCView paska ladowania trybu alternatywnego: tlo BAR_BACK.TGA
static void* aimBarFill;       // i wypelnienie WHITE.TGA barwione SetColor
static bool  aimBarShown;
static DWORD aimTurnLast;
static bool  streaming;        // strumien NB: fmode udaje 7 przez caly czas trzymania
static int   realMode;         // prawdziwy fmode na czas strumienia
static void* investSpell;
static DWORD lastStatusLog;
static int   lastStatus;
static bool  aniMissingLogged;
static DWORD fallbackCleanupAt; // GetTickCount, kiedy sprzatnac animacje zastepcza
static void* heroAi;            // oCAIHuman bohatera z biezacego HookActionMove (stan ruchu do animacji)
static void* leftFx;            // efekt czaru z wezlem wylotu podmienionym na lewa dlon (SignHandToLeft), 0 = brak
static void* leftFxNode;        // jego wezel sprzed podmiany (ZS_RIGHTHAND) - do przywrocenia
static bool  turning;           // bohater obraca sie na postoju do punktu celu (TurnStep)
static float turnX, turnZ;      // pozycja celu z chwili rzutu - bez trzymania wskaznika NPC miedzy klatkami
static DWORD turnStart, turnLast;

// Gesty Znakow Geralta (tylko Gothic II; prywatna paczka zDismember_W3.vdf, make_signs.py): po 10 gestow (Znak x bez
// broni / z bronia biala) w dwoch nakladkach, wszystkie na warstwie SIGN_LAYER, pod nazwami losowych animacji przy
// meblach i jedzeniu oraz gestow strazy (gra je tylko AI postaci NPC):
//   HUMANS_ZDF_SIGNS (SIGN_ANIS) - ruch: tylko tulow i rece, nogi dalej biegna; tulow ustawiony wzgledem miednicy
//     biegu (patrzy do przodu), wiec na postoju w gardzie W3 (miednica bokiem, 77 st. w 1H) dlon szla w bok;
//   HUMANS_ZDF_SIGNS_STAND (SIGN_ANIS_STAND) - postoj: cale cialo w miejscu (korzen bez ruchu w poziomie, stopy stoja),
//     wiec postawa pod spodem nie ma znaczenia - dlon zawsze do przodu.
// Wariant wybiera HeroStanding przy starcie gestu; gdy bohater ruszy w trakcie gestu postoju, WatchGesture przelacza na
// wariant ruchu od tej samej klatki (oba maja te same klatki) - StartAni na tej samej warstwie wygasza gest postoju,
// nogi przechodza w bieg. Nakladki zakladane, gdy bohater stoi, i nigdy nie zdejmowane - zalozenie nowej nakladki
// (ApplyModelProtoOverlay 0x578933) i zdjecie (RemoveModelProtoOverlay) zatrzymuja w silniku wszystkie animacje postaci
// (warstwy 0..99999): w biegu albo skoku hamowaly bohatera. Ponowne zalozenie zwraca 0 i nic nie robi, wiec o tym,
// czy gest gra, decyduje warstwa animacji pod nazwa gestu (SignAniId). Nazwe rozstrzyga ostatnio zalozona nakladka,
// ktora ja ma (GetAniFromAniID szuka od konca listy) - HUMANS_REMASTER z zEBA ma wszystkie losowe animacje, wiec
// zalozona po naszej przykrywa gesty; wtedy na postoju zdejmujemy i zakladamy nasza znowu na wierzch.
// Warstwa 21: 20 (zakrety) silnik gasi co klatke ruchu (FadeOutAnisLayerRange(20, 20)), 2..8 gasi StandUp (bron),
// w HUMANS zajete 1, 2, 4, 9, 11, 20 - uzasadnienie w make_signs.py (LAYER).
// Znak wybiera lista id czarow w ini (reszta = Aard); natychmiastowy czar wylatuje z pchnieciem dloni (SignCastMs).
enum { SIGN_AARD, SIGN_IGNI, SIGN_QUEN, SIGN_YRDEN, SIGN_AXII, SIGN_COUNT };
static const char* SIGN_INI_KEYS[SIGN_COUNT] = { 0, "SignIgni", "SignQuen", "SignYrden", "SignAxii" };
static const char* SIGN_OVERLAY = "HUMANS_ZDF_SIGNS";             // jak OVERLAY w make_signs.py (ruch)
static const char* SIGN_OVERLAY_STAND = "HUMANS_ZDF_SIGNS_STAND"; // jak OVERLAY_STAND (postoj)
static const int   SIGN_LAYER = 21;               // jak LAYER w make_signs.py
static void* signModel;                           // model bohatera z zalozonymi nakladkami gestow
static void* signFailModel;                       // model, na ktory nakladka nie wchodzi (np. postac zwierzecia)
static void* signStandFailModel;                  // model bez nakladki postoju (stara paczka) - tylko gesty tulowia
static bool  signWaitLogged;                      // log: gest pominiety, bo nakladka czeka na postoj
static const char* SIGN_ANIS[SIGN_COUNT][2] = {   // ruch, [znak][z bronia] - jak SIGN_ANIS w make_signs.py
	{ "R_CHAIR_RANDOM_1", "R_CHAIR_RANDOM_2" }, { "R_CHAIR_RANDOM_3", "R_CHAIR_RANDOM_4" },
	{ "T_BAUMSAEGE_RANDOM_1", "T_REPAIR_RANDOM_1" }, { "T_STOVE_RANDOM_1", "T_FOODHUGE_RANDOM_1" },
	{ "T_RICE_RANDOM_1", "T_RICE_RANDOM_2" } };
static const char* SIGN_ANIS_STAND[SIGN_COUNT][2] = {   // postoj - jak SIGN_ANIS_STAND w make_signs.py
	{ "T_FOOD_RANDOM_1", "T_FOOD_RANDOM_2" }, { "T_MEAT_RANDOM_1", "T_JOINT_RANDOM_1" },
	{ "T_POTION_RANDOM_1", "T_POTION_RANDOM_2" }, { "T_POTION_RANDOM_3", "T_PRAY_RANDOM" },
	{ "T_LGUARD_GREET", "T_HGUARD_GREET" } };
// Tryb alternatywny (23.10; przytrzymanie F dluzej niz SignAltHold - decyduje zDismember, ZDISM_SignHoldState alt = 1):
// Aard (fala dookola) i Yrden (pulapka) maja gest "w ziemie" z W3 calym cialem w miejscu - od zamachu przez dotkniecie
// ziemi lewa dlonia do wstania (make_signs.py SIGN_ANIS_ALT, w nakladce postoju HUMANS_ZDF_SIGNS_STAND, gesty strazy G1
// nieuzywane przez bohatera). [znak][klasa broni qs::SignWeaponClass]: 0 bez broni, 1 sztylet / 1H (W3 rp), 2 2H (W3 lp).
// Gest gra takze w ruchu: bohatera na czas gestu trzyma w miejscu zDismember (blokada ruchu), a skutek (fala, glif) odklada
// do dotkniecia ziemi - czas i dlugosc gestu bierze z eksportu ZQS_SignGesture (SignAltCastMs / SignAltGestureMs).
// Pozostale Znaki (0) graja w trybie alternatywnym gest trybu zwyklego.
static const char* SIGN_ANIS_ALT[SIGN_COUNT][3] = {   // [znak][klasa broni] - jak SIGN_ANIS_ALT w make_signs.py
	{ "T_LGUARD_COMEIN", "T_LGUARD_ALLRIGHT", "T_LGUARD_NOENTRY" }, { 0, 0, 0 }, { 0, 0, 0 },
	{ "T_HGUARD_COMEIN", "T_HGUARD_NOENTRY", "T_HGUARD_LOOKAROUND" }, { 0, 0, 0 } };
// domyslnie id czarow Gothica II NK (Constants.d): ogien / leczenie, swiatlo, teleporty, przemiany /
// przywolania i obszarowe / sen, strach, urok, zmniejszenie, nadecie, wysysanie
static char signIdsText[SIGN_COUNT][256] = { "", "19,23,30,37,38,42",
	"0,1,3,5,7,8,9,10,11,12,13,14,15,16,17,18,21,28,41,47,48,49,50,51,52,53,54,55,56,57,58",
	"22,25,31,35,39,40,44,45,60,70,75,76,81,82,83,84,88,89", "27,32,46,59,74,80,86,87" };
static int   signIds[SIGN_COUNT][64];
static int   signIdCount[SIGN_COUNT];
static bool  signGestures = true;
static char  signCastMsText[64] = "320,320,320,320,200";   // pchniecie dloni od startu gestu (Axii: wlasciwe pchniecie kl. 4-8)
static char  signGestureMsText[64] = "680,680,680,680,440"; // dlugosc gestu
static int   signCastMs[SIGN_COUNT], signGestureMs[SIGN_COUNT];
static char  signAltCastMsText[64] = "440,0,0,440,0";       // gest alternatywny: dotkniecie ziemi od startu (make_signs ALT_BEFORE)
static char  signAltGestureMsText[64] = "1120,0,0,1120,0";  // dlugosc gestu alternatywnego (do wstania; 0 = bez gestu)
static int   signAltCastMs[SIGN_COUNT], signAltGestureMs[SIGN_COUNT];
static char  gestureOverlay[48];           // nazwa animacji biezacego gestu ("" = brak)
static bool  gestureAlt;                   // gra gest trybu alternatywnego (caly w miejscu, bez wariantu ruchu)
static int   gestureSign = -1;             // Znak biezacego gestu
static int   gestureCastMs;                // ms od startu gestu do pchniecia dloni / dotkniecia ziemi (ZQS_SignGesture)
static int   gestureLenMs;                 // dlugosc biezacego gestu (ms)
static DWORD gestureRemoveAt;
static int   gestureAniId = -1;            // grajacy wariant gestu
static int   gestureMoveId = -1;           // wariant ruchu tego gestu (przelaczenie, gdy bohater ruszy)
static bool  gestureStanding;              // gra wariant postoju (cale cialo)
static DWORD gestureStart, gestureEnd;    // GetTickCount: start gestu i koniec jego animacji
static bool  gestureWasActive;
static int   gestureRestarts;             // wznowienia po zatrzymaniu przez silnik (np. rzut czaru)
static bool  pendingCast;                  // natychmiastowy czar czeka na pchniecie dloni
static DWORD pendingCastAt;
static void* pendingSpell;
// Tryb zwykly Znaku W3 z koncem pchniecia dloni (23.10, SignCastSync): rzut odlozony po puszczeniu F; przerwanie gestu
// przed rzutem = bez skutku i bez kosztu (jak w W3) - CancelPendingCast.
static bool  signCastSync = true;          // SignCastSync: 1 = skutek Znaku W3 z koncem pchniecia, 0 = przy puszczeniu F
static char  signCastSyncMsText[64] = "360,400,360,360,320";   // koniec pchniecia od startu gestu (make_signs.py push_end)
static int   signCastSyncMs[SIGN_COUNT];
static int   signSyncLateMs = 300;         // SignSyncLateMs: rzut spozniony o tyle (bohater zablokowany - unik, finisher) = przerwany
static bool  pendingSync;                  // odlozony rzut Znaku W3 z puszczenia F (zasady przerwania)
static bool  pendingKeepAim;               // celownik trwa do rzutu (punkt celu z chwili rzutu)
static const char* pendingCancel;          // powod przerwania z hakow (trafienie) - obsluga w HookActionMove

// opakowanie funkcji strumieni NB (patrz InstallNbStreamHooks)
static int   nbHooksState;      // 0 = jeszcze nie, 1 = zalozone, -1 = nie da sie (inna wersja NB)
static DWORD nbStreamTarget, nbFireStreamTarget;
static DWORD nbStreamRet, nbFireStreamRet;
static volatile int inNbStream; // > 0 w trakcie funkcji strumieni NB
static volatile int nbWrapCalls; // licznik wywolan funkcji strumieni NB
static bool  streamEnding;      // po puszczeniu: fmode jeszcze udaje 7, zeby NB zakonczyl strumien
static DWORD endStart;
static int   endWrapCalls;
static int   fakeInputCalls;    // ile razy podsunelismy wcisniety klawisz (do logu)
static int   streamSpellId;
static DWORD stanceFixUntil;    // po strumieniu: pilnowanie, zeby NB nie zostawil postawy magii
static int   lastMana;          // ManaCostPercent
static int   manaFrac;          // reszta z dzielenia przez 100

// ---------------------------------------------------------------------------

static void Log(const char* fmt, ...) {
	if (!debugLog)
		return;
	char line[256];
	int n = wsprintfA(line, "[%10u] ", GetTickCount());
	va_list args;
	va_start(args, fmt);
	n += wvsprintfA(line + n, fmt, args);
	va_end(args);
	line[n++] = '\r';
	line[n++] = '\n';
	HANDLE f = CreateFileA(logPath, FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, 0, 0);
	if (f == INVALID_HANDLE_VALUE)
		return;
	DWORD written;
	WriteFile(f, line, n, &written, 0);
	CloseHandle(f);
}

static void ShowMessage(const char* text) {
	// tak jak PrintScreen(..., -1, -1, ...): widok tekstu gry, kolor fontu (NULL)
	BYTE* game = *(BYTE**)ADDR_OGAME;
	void* view = game ? *(void**)(game + OFS_OGAME_TEXTVIEW) : 0;
	if (!view)
		return;
	BYTE zstr[0x14];
	((ZStringCtor_t)ADDR_ZSTRING_CTOR)(zstr, 0, text);
	((PrintTimedCXY_t)ADDR_PRINTTIMEDCXY)(view, 0, zstr, 2000.0f, 0);
	((ZStringDtor_t)ADDR_ZSTRING_DTOR)(zstr, 0);
}

static bool CastKeyDown() {
	void* input = *(void**)ADDR_ZINPUT;
	if (!input)
		return false;
	KeyPressed_t keyPressed = (KeyPressed_t)(*(DWORD**)input)[VT_INPUT_KEYPRESSED / 4];
	return keyPressed(input, 0, castKey) != 0;
}

static bool InventoryOpen(BYTE* npc) {
	BYTE* inv = npc + OFS_NPC_INVENTORY;
	return ((GetInt_t)(*(DWORD**)inv)[VT_CONTAINER_ISOPEN / 4])(inv, 0) != 0;
}

static int AniId(void* model, const char* name) {
	BYTE zstr[0x14];
	((ZStringCtor_t)ADDR_ZSTRING_CTOR)(zstr, 0, name);
	int id = ((AniId_t)ADDR_MODEL_ANIID)(model, 0, zstr);
	((ZStringDtor_t)ADDR_ZSTRING_DTOR)(zstr, 0);
	return id;
}

static void StartAniByName(BYTE* npc, const char* name) {
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	int id = model ? AniId(model, name) : -1;
	if (id >= 0)
		((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
}

static int SetOverlay(void* model, const char* name, bool apply) {
	char file[64];
	wsprintfA(file, "%s.MDS", name);
	BYTE zstr[0x14];
	((ZStringCtor_t)ADDR_ZSTRING_CTOR)(zstr, 0, file);
	int ok = 1;
	if (apply)
		ok = ((ApplyOverlay_t)ADDR_MODEL_APPLYOVL)(model, 0, zstr);
	else
		((RemoveOverlay_t)ADDR_MODEL_REMOVEOVL)(model, 0, zstr);
	((ZStringDtor_t)ADDR_ZSTRING_DTOR)(zstr, 0);
	return ok;
}

static int SignFor(int spellId) {
	for (int s = SIGN_AARD + 1; s < SIGN_COUNT; s++)
		for (int i = 0; i < signIdCount[s]; i++)
			if (signIds[s][i] == spellId)
				return s;
	return SIGN_AARD;
}

// Koniec pilnowania gestu (nakladka zostaje na modelu - patrz wyzej).
static void EndGesture(BYTE*) {
	gestureOverlay[0] = 0;
	gestureRemoveAt = 0;
	gestureAniId = -1;
	gestureMoveId = -1;
	gestureStanding = false;
	gestureAlt = false;
	gestureSign = -1;
	gestureCastMs = gestureLenMs = 0;
}

static const char* StanceAni(int mode) {
	static const char* names[] = { "S_RUN", "S_FISTRUN", "S_1HRUN", "S_1HRUN", "S_2HRUN", "S_BOWRUN", "S_CBOWRUN", "S_MAGRUN" };
	return mode >= 0 && mode < 8 ? names[mode] : 0;
}

static int AniLayer(void* ani) {
	return ani ? *(int*)((BYTE*)ani + OFS_ANI_LAYER) : -1;
}

static bool IsFading(void* act) {
	return *(int*)((BYTE*)act + OFS_ACTIVE_FADING) != 0;
}

// Bohater stoi na ladzie: w warstwie 1 postawa biezacego trybu chodu (nie bieg, sprint, skok, upadek, plywanie).
static bool HeroStanding(void* ai) {
	return ai && *(int*)((BYTE*)ai + OFS_AI_WALKMODE) <= ANI_WALKMODE_SNEAK
		&& ((GetInt_t)ADDR_AI_ISSTANDING)(ai, 0) != 0;
}

// Postawa trybu chodu i broni jak w silniku (_s_walk), awaryjnie po nazwie z trybu broni.
static void StartStance(BYTE* npc, void* model, void* ai) {
	int id = ai ? *(int*)((BYTE*)ai + OFS_AI_S_WALK) : -1;
	if (id < 0) {
		const char* stance = StanceAni(*(int*)(npc + OFS_NPC_FMODE));
		id = stance ? AniId(model, stance) : -1;
	}
	if (id >= 0)
		((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
}

// Id gestu z nakladki: nazwa prowadzi do animacji na warstwie SIGN_LAYER (bazowa w HUMANS ma 2), inaczej -1 (nakladki
// nie ma albo przykryla ja nakladka zalozona pozniej z ta sama nazwa).
static int SignAniId(void* model, const char* name) {
	int id = AniId(model, name);
	return id >= 0 && AniLayer(((PtrByInt_t)ADDR_MODEL_ANIFROMID)(model, 0, id)) == SIGN_LAYER ? id : -1;
}

// Zdjecie (gdy jest - inaczej RemoveModelProtoOverlay nic nie robi) i zalozenie na wierzch listy nakladek modelu.
static void ReapplyOverlay(void* model, const char* name) {
	SetOverlay(model, name, false);
	SetOverlay(model, name, true);
}

// Nakladki gestow zakladamy tylko, gdy bohater stoi i gra wylacznie warstwa 1 (bez dobycia broni, trafienia, zakretu):
// zalozenie i zdjecie nakladki zatrzymuje wszystkie animacje postaci, wiec zaraz potem wraca postawa. Wolane co klatke
// z HookActionMove (az sie uda) i przy gescie (po signModel = 0 - sprawdzenie od nowa). Wynik ApplyModelProtoOverlay bez
// znaczenia (0 takze dla juz zalozonej) - liczy sie warstwa animacji gestu. Nakladka zalozona, ale przykryta pozniejsza
// (np. HUMANS_REMASTER z zEBA) wraca na wierzch. Bez nakladki postoju (stara paczka) gesty tylko tulowia, jak dawniej.
static bool EnsureSignOverlay(BYTE* npc, void* ai) {
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!model || model == signFailModel)
		return false;
	if (model == signModel)
		return true;
	bool moveOn = SignAniId(model, SIGN_ANIS[0][0]) >= 0;
	bool standOn = model == signStandFailModel || SignAniId(model, SIGN_ANIS_STAND[0][0]) >= 0;
	if (moveOn && standOn) {
		signModel = model;
		return true;
	}
	if (!HeroStanding(ai))
		return false;
	int n = *(int*)((BYTE*)model + OFS_MODEL_NUMACTIVE);
	void** active = (void**)((BYTE*)model + OFS_MODEL_ACTIVE);
	for (int i = 0; i < n && i < 6; i++)
		if (active[i] && AniLayer(*(void**)active[i]) != 1)
			return false;
	if (!moveOn)
		ReapplyOverlay(model, SIGN_OVERLAY);
	if (!standOn)
		ReapplyOverlay(model, SIGN_OVERLAY_STAND);
	StartStance(npc, model, ai);
	if (SignAniId(model, SIGN_ANIS[0][0]) < 0) {
		// np. bohater w postaci zwierzecia (nakladka HUMANS nie pasuje) - na ten model juz nie probujemy
		signFailModel = model;
		Log("nie da sie zalozyc %s na model bohatera - zwykla animacja rzutu", SIGN_OVERLAY);
		return false;
	}
	if (!standOn && SignAniId(model, SIGN_ANIS_STAND[0][0]) < 0) {
		signStandFailModel = model;
		Log("nie da sie zalozyc %s (stara paczka?) - na postoju tez gest tylko tulowia i rak", SIGN_OVERLAY_STAND);
	}
	signModel = model;
	signWaitLogged = false;
	Log("na postoju zalozone na wierzch:%s%s (postawa przywrocona)", moveOn ? "" : " HUMANS_ZDF_SIGNS",
		standOn ? "" : " HUMANS_ZDF_SIGNS_STAND");
	return true;
}

// Aktywny gest id z nakladki (warstwa SIGN_LAYER, niewygaszany). stopOther - zatrzymuje inne aktywne animacje o tym id
// (np. bazowa z HUMANS na warstwie 2, gdyby ruszyla zamiast gestu, albo wygaszany poprzedni gest).
static void* SignActive(void* model, int id, bool stopOther) {
	void* list[6];
	int n = *(int*)((BYTE*)model + OFS_MODEL_NUMACTIVE);
	if (n > 6)
		n = 6;
	memcpy(list, (BYTE*)model + OFS_MODEL_ACTIVE, n * sizeof(void*));   // StopAni przesuwa tablice
	void* found = 0;
	for (int i = 0; i < n; i++) {
		BYTE* ani = list[i] ? *(BYTE**)list[i] : 0;
		if (!ani || *(int*)(ani + OFS_ANI_ID) != id)
			continue;
		if (AniLayer(ani) == SIGN_LAYER && !IsFading(list[i]))
			found = list[i];
		else if (stopOther)
			((VoidByPtr_t)ADDR_MODEL_STOPACTIVE)(model, 0, list[i]);
	}
	return found;
}

// Start gestu z kontrola, ze aktywna jest animacja z nakladki. StartAni nic nie robi, gdy ta animacja juz gra (restart -
// najpierw zatrzymanie, gest od poczatku), i nie startuje przy 6 aktywnych animacjach (0x57B3E5) - wtedy false.
static bool StartSignAni(void* model, int id, bool restart) {
	void* act = SignActive(model, id, true);
	if (act && restart)
		((VoidByPtr_t)ADDR_MODEL_STOPACTIVE)(model, 0, act);
	((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
	return SignActive(model, id, true) != 0;
}

// Gest trybu alternatywnego "w ziemie" (SIGN_ANIS_ALT) na calym ciele, takze w ruchu (bohatera trzyma zDismember). -> ms do
// dotkniecia ziemi, -1 = Znak bez gestu alternatywnego albo stara paczka W3 bez tych nazw (wtedy gest trybu zwyklego).
static int StartAltGesture(void* model, int sign, int mode, int spellId) {
	const char* name = SIGN_ANIS_ALT[sign][qs::SignWeaponClass(mode)];
	if (!name || signAltGestureMs[sign] <= 0)
		return -1;
	int id = SignAniId(model, name);
	if (id < 0 || !StartSignAni(model, id, true)) {
		Log("   gest trybu alternatywnego %s %s - gest trybu zwyklego", name,
			id < 0 ? "nie z nakladki (stara paczka zDismember_W3.vdf?)" : "nie wystartowal");
		return -1;
	}
	lstrcpyA(gestureOverlay, name);
	gestureAniId = id;
	gestureMoveId = -1;
	gestureStanding = true;
	gestureAlt = true;
	gestureSign = sign;
	gestureCastMs = signAltCastMs[sign];
	gestureLenMs = signAltGestureMs[sign];
	gestureStart = GetTickCount();
	gestureEnd = gestureStart + gestureLenMs;
	gestureRemoveAt = gestureEnd + 400;
	gestureRestarts = 0;
	gestureWasActive = true;
	Log("   gest %s (tryb alternatywny - cale cialo w miejscu, czar %d, tryb broni %d, warstwa %d), ziemia po %d ms, "
		"dlugosc %d ms", name, spellId, mode, SIGN_LAYER, gestureCastMs, gestureLenMs);
	return gestureCastMs;
}

// Gest Znaku dla czaru: -> ms do pchniecia dloni, -1 = brak gestu (zwykla animacja rzutu). alt = rzut w trybie
// alternatywnym (zDismember) - Aard / Yrden graja wtedy gest "w ziemie" (StartAltGesture).
static int StartGesture(BYTE* npc, int spellId, bool alt = false) {
	if (!signGestures || !useAnim || isGothic1)
		return -1;
	int mode = *(int*)(npc + OFS_NPC_FMODE);
	int sword = mode >= 2 && mode <= 4 ? 1 : 0;   // sztylet, 1H, 2H w rece
	int sign = SignFor(spellId);
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!model)
		return -1;
	EndGesture(npc);
	const char* name = SIGN_ANIS[sign][sword];
	const char* standName = SIGN_ANIS_STAND[sign][sword];
	if (!EnsureSignOverlay(npc, heroAi)) {
		// w ruchu nakladki nie zakladamy (zatrzymalaby bohatera) - zalozy sie na najblizszym postoju
		if (!signWaitLogged && model != signFailModel)
			Log("   gest %s pominiety - nakladka %s czeka na postoj bohatera", name, SIGN_OVERLAY);
		signWaitLogged = true;
		return -1;
	}
	if (alt) {
		int ms = StartAltGesture(model, sign, mode, spellId);
		if (ms >= 0)
			return ms;
	}
	// na postoju cale cialo (dlon do przodu niezaleznie od postawy), w ruchu tylko tulow i rece
	bool standing = HeroStanding(heroAi);
	int moveId = SignAniId(model, name);
	int standId = standing ? SignAniId(model, standName) : -1;
	if (moveId < 0 || (standing && standId < 0 && model != signStandFailModel)) {
		// nakladka zniknela z modelu albo przykryla ja pozniejsza z tymi samymi nazwami (np. HUMANS_REMASTER z zEBA) -
		// sprawdzenie od nowa: na postoju zalozy sie znowu na wierzch, w ruchu na najblizszym postoju
		Log("   gest %s / %s nie z nakladki - zakladam nakladki gestow od nowa", name, standName);
		signModel = 0;
		EnsureSignOverlay(npc, heroAi);
		moveId = SignAniId(model, name);
		standId = standing ? SignAniId(model, standName) : -1;
		if (moveId < 0) {
			int id = AniId(model, name);
			Log("   gest %s: animacja z warstwy %d zamiast %d - zwykla animacja rzutu", name,
				id >= 0 ? AniLayer(((PtrByInt_t)ADDR_MODEL_ANIFROMID)(model, 0, id)) : -1, SIGN_LAYER);
			signModel = 0;
			return -1;
		}
	}
	int id = standId >= 0 ? standId : moveId;
	if (!StartSignAni(model, id, true)) {
		Log("   gest %s nie wystartowal (aktywnych animacji %d) - zwykla animacja rzutu", id == standId ? standName : name,
			*(int*)((BYTE*)model + OFS_MODEL_NUMACTIVE));
		return -1;
	}
	lstrcpyA(gestureOverlay, id == standId ? standName : name);
	gestureAniId = id;
	gestureMoveId = moveId;
	gestureStanding = id == standId;
	gestureAlt = false;
	gestureSign = sign;
	gestureCastMs = signCastMs[sign];
	gestureLenMs = signGestureMs[sign];
	gestureStart = GetTickCount();
	gestureEnd = gestureStart + signGestureMs[sign];
	gestureRemoveAt = gestureEnd + 400;   // + wygaszenie gestu (blend 0,3 s)
	gestureRestarts = 0;
	gestureWasActive = true;
	Log("   gest %s (%s, czar %d, warstwa %d), pchniecie po %d ms", gestureOverlay,
		gestureStanding ? "postoj - cale cialo" : "ruch - tulow i rece", spellId, SIGN_LAYER, signCastMs[sign]);
	return signCastMs[sign];
}

// Bohater ruszyl (bieg, sprint, skok, unik, strafe) w trakcie gestu postoju: na cale cialo trzymalby nogi w miejscu (bohater
// slizgalby sie z zamrozonymi nogami) - dalej wariant ruchu, tylko tulow i rece, od tej samej klatki (oba warianty maja te
// same klatki, make_signs.py). Ta sama warstwa: StartAni wygasza gest postoju (FadeOutAni z jego blend_out 0,2 s),
// wiec tulow nie skacze, a nogi plynnie przechodza w bieg.
// Gest trybu alternatywnego nie ma wlasnego wariantu ruchu: w ruchu bohatera trzyma zDismember (blokada ruchu - wtedy
// HookActionMove w ogole nie jest wolany), wiec tu trafiamy tylko bez zDismember albo przy SignAltInPlace=0 - wtedy dalej
// wariant ruchu gestu zwyklego od jego pchniecia dloni (qs::MoveSwitchFrame).
static void SwitchToMoveGesture(void* model, DWORD now) {
	gestureStanding = false;
	void* act = SignActive(model, gestureAniId, false);
	float frame = act ? *(float*)((BYTE*)act + OFS_ACTIVE_ACTFRAME) : (now - gestureStart) / 40.0f;
	bool wasAlt = gestureAlt;
	if (wasAlt) {
		BYTE* npc = *(BYTE**)ADDR_PLAYER;
		int mode = npc ? *(int*)(npc + OFS_NPC_FMODE) : 0;
		gestureMoveId = gestureSign >= 0 ? SignAniId(model, SIGN_ANIS[gestureSign][mode >= 2 && mode <= 4 ? 1 : 0]) : -1;
		frame = qs::MoveSwitchFrame(true, frame, gestureSign >= 0 ? signCastMs[gestureSign] : 0);
		gestureAlt = false;   // StartAni na tej samej warstwie wygasza gest alternatywny (blend_out 0,3 s)
		// os czasu gestu zwyklego (WatchGesture wznawia od (teraz - start) / 40 i konczy pilnowanie po jego dlugosci)
		gestureStart = now - (DWORD)(frame * 40.0f);
		gestureEnd = gestureStart + (gestureSign >= 0 ? signGestureMs[gestureSign] : 0);
		gestureRemoveAt = gestureEnd + 400;
	}
	gestureAniId = gestureMoveId;
	void* moveAct = gestureMoveId >= 0 && StartSignAni(model, gestureMoveId, false) ? SignActive(model, gestureMoveId, false) : 0;
	if (moveAct)
		((SetActFrame_t)ADDR_ANIACTIVE_SETFRAME)(moveAct, 0, frame);
	Log("   bohater ruszyl po %u ms - gest dalej tylko tulow i rece od klatki %d%s%s", now - gestureStart, (int)frame,
		wasAlt ? " (gest alternatywny bez blokady ruchu z zDismember - wariant ruchu gestu zwyklego)" : "",
		moveAct ? "" : " - nie wystartowal, koniec pilnowania");
	if (!moveAct)
		EndGesture(0);
}

// Gest zatrzymany przez silnik przed koncem (np. rzut czaru przerywa animacje wyzszych warstw) - wznawiamy go
// od biezacej klatki, najwyzej 3 razy na gest (potem koniec pilnowania). Log mowi, kiedy to sie dzieje. Na warstwie
// SIGN_LAYER gasza go tylko przerwania postaci (Interrupt, StartAIState) i zakladanie/zdejmowanie nakladek.
static void WatchGesture(BYTE* npc) {
	if (!gestureOverlay[0] || gestureAniId < 0)
		return;
	DWORD now = GetTickCount();
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!model || (int)(now - gestureEnd) >= 0)
		return;
	if (gestureStanding && !HeroStanding(heroAi)) {
		SwitchToMoveGesture(model, now);
		if (gestureAniId < 0)
			return;
	}
	if (SignActive(model, gestureAniId, false)) {
		gestureWasActive = true;
		return;
	}
	float frame = (now - gestureStart) / 40.0f;   // 25 kl./s
	Log("   gest zatrzymany po %u ms (klatka %d)%s", now - gestureStart, (int)frame,
		gestureRestarts < 3 ? " - wznawiam" : " - bez wznawiania");
	if (gestureRestarts >= 3) {
		EndGesture(npc);
		return;
	}
	gestureRestarts++;
	void* act = StartSignAni(model, gestureAniId, false) ? SignActive(model, gestureAniId, false) : 0;
	if (act)
		((SetActFrame_t)ADDR_ANIACTIVE_SETFRAME)(act, 0, frame);
	else {
		Log("   gest: StartAni nie wystartowal gestu - koniec pilnowania");
		EndGesture(npc);
	}
}

// Konczy animacje zastepcza: rzut Ognistej strzaly przechodzi w poze S_FBTSHOOT (warstwa 1, w petli). Gra tylko na
// postoju, wiec jesli nadal trwa (niewygaszana), po jej zatrzymaniu warstwa 1 jest pusta - wraca postawa trybu chodu.
// Wygaszana albo juz nieaktywna = silnik przejal warstwe 1 (ruszyl, skoczyl) - niczego nie wymuszamy (wymuszona
// postawa w ruchu blokowala bohatera).
static void CleanupFallbackAni(BYTE* npc, void* aiHuman) {
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!model)
		return;
	const char* names[] = { fallbackAniName, "S_FBTSHOOT", "T_FIBCAST_2_FIBSHOOT", "S_FIBSHOOT" };
	bool stopped = false;
	for (int i = 0; i < 4; i++) {
		int id = AniId(model, names[i]);
		void* act = id >= 0 ? ((ActiveAni_t)ADDR_MODEL_ACTIVEANI)(model, 0, id) : 0;
		if (act && !IsFading(act)) {
			((StopAni_t)ADDR_MODEL_STOPANI)(model, 0, id);
			stopped = true;
		}
	}
	if (!stopped) {
		Log("   animacja zastepcza juz zakonczona (silnik przejal ruch)");
		return;
	}
	StartStance(npc, model, aiHuman);
	Log("   animacja zastepcza zakonczona, postawa przywrocona");
}

// Zwykla animacja rzutu (bez gestu): T_QSCAST (tylko tulow i rece - takze w ruchu) albo zastepcza na cale cialo, ta
// tylko na postoju: na warstwie 1 podmienilaby bieg, sprint albo skok i zatrzymala bohatera (rzut wtedy bez animacji).
static void PlayCastAni(BYTE* npc) {
	if (!useAnim)
		return;
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!model)
		return;
	int id = AniId(model, aniName);
	if (id >= 0) {
		((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
		return;
	}
	// brak T_QSCAST (gra bez nakladki animacji) - animacja zastepcza
	id = AniId(model, fallbackAniName);
	if (!aniMissingLogged)
		Log("brak animacji %s - zastepcza %s %s", aniName, fallbackAniName, id >= 0 ? "(tylko na postoju)" : "(tez brak!)");
	aniMissingLogged = true;
	if (id < 0)
		return;
	if (!HeroStanding(heroAi)) {
		Log("   bez animacji zastepczej - bohater w ruchu (bieg/skok/sprint zostaje)");
		return;
	}
	((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
	fallbackCleanupAt = GetTickCount() + fallbackAniMs;
}

// Zatrzymuje koncowke strumienia NB (-> S_MAGRUN) poza trybem magii i wraca do postawy broni.
static void FixStanceAfterStream(BYTE* npc) {
	int mode = *(int*)(npc + OFS_NPC_FMODE);
	const char* stance = StanceAni(mode);
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!stance || !model)
		return;
	static const char* magicAnis[] = { "T_FSCCAST_2_MAGRUN", "T_FSLCAST_2_MAGRUN", "S_FSCCAST", "S_FSLCAST", "S_MAGRUN" };
	bool magicStance = false;  // T_FS?CAST_2_MAGRUN (-> S_MAGRUN) albo S_MAGRUN - cale cialo
	for (int i = 0; i < 5; i++) {
		int id = AniId(model, magicAnis[i]);
		if (id >= 0 && ((ActiveAni_t)ADDR_MODEL_ACTIVEANI)(model, 0, id)) {
			((StopAni_t)ADDR_MODEL_STOPANI)(model, 0, id);
			Log("   po strumieniu zatrzymane %s", magicAnis[i]);
			if (i < 2 || i == 4)
				magicStance = true;
		}
	}
	if (magicStance) {
		Log("   przywracam postawe %s", stance);
		int id = AniId(model, stance);
		if (id >= 0 && !((ActiveAni_t)ADDR_MODEL_ACTIVEANI)(model, 0, id))
			((StartAni_t)ADDR_MODEL_STARTANI)(model, 0, id, 0);
	}
}

static void ManaTrackStart(BYTE* npc) {
	lastMana = ((GetAttribute_t)ADDR_NPC_GETATTRIBUTE)(npc, 0, ATR_MANA);
	manaFrac = 0;
	manaSpentTotal = 0;
}

// Dolicza (albo zwraca) roznice wzgledem ManaCostPercent od many zuzytej od ostatniego wywolania.
static void ManaTrackUpdate(BYTE* npc) {
	GetAttribute_t getAttr = (GetAttribute_t)ADDR_NPC_GETATTRIBUTE;
	int cur = getAttr(npc, 0, ATR_MANA);
	if (manaPercent != 100 && cur < lastMana) {
		manaFrac += (lastMana - cur) * (manaPercent - 100);
		int extra = manaFrac / 100;
		manaFrac -= extra * 100;
		if (extra > cur)
			extra = cur;
		if (extra)
			((ChangeAttr_t)ADDR_NPC_CHANGEATTR)(npc, 0, ATR_MANA, -extra);
		cur = getAttr(npc, 0, ATR_MANA);
	}
	if (cur < lastMana)
		manaSpentTotal += lastMana - cur;
	lastMana = cur;
}

// --- strumienie NB: wcisniety klawisz akcji tylko dla funkcji strumieni NB ---

static float __fastcall HookGetState(void* self, void* edx, unsigned short key) {
	if (inNbStream > 0 && streaming && (key == GAME_ACTION || key == GAME_UP)) {
		fakeInputCalls++;
		return 1.0f;
	}
	return origGetState(self, edx, key);
}

static int __fastcall HookMouseLeft(void* self, void* edx) {
	if (inNbStream > 0 && streaming) {
		fakeInputCalls++;
		return 1;
	}
	return origMouseLeft(self, edx);
}

// Wejscie z call albo jmp (wywolanie koncowe) - na szczycie stosu jest adres powrotu. Zdejmujemy go,
// wolamy cel (jego ret N zdejmie ewentualne argumenty), przywracamy adres i wracamy. ecx/eax/edx
// nietkniete. Nie jest wspolbiezne - funkcje strumieni NB nie wolaja same siebie.
__declspec(naked) static void NbStreamWrap() {
	__asm {
		inc  inNbStream
		inc  nbWrapCalls
		pop  nbStreamRet
		call nbStreamTarget
		dec  inNbStream
		push nbStreamRet
		ret
	}
}

__declspec(naked) static void NbFireStreamWrap() {
	__asm {
		inc  inNbStream
		inc  nbWrapCalls
		pop  nbFireStreamRet
		call nbFireStreamTarget
		dec  inNbStream
		push nbFireStreamRet
		ret
	}
}

static bool PatchRel32(DWORD site, BYTE opcode, DWORD expectedTarget, void* hook, DWORD* orig) {
	BYTE* p = (BYTE*)site;
	if (*p != opcode || site + 5 + *(int*)(p + 1) != expectedTarget)
		return false;
	DWORD oldProtect;
	if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
		return false;
	if (orig)
		*orig = expectedTarget;
	*(int*)(p + 1) = (int)((DWORD)hook - (site + 5));
	VirtualProtect(p, 5, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), p, 5);
	return true;
}

static void PatchVtable(void* object, int offset, void* hook, void** orig) {
	DWORD* slot = *(DWORD**)object + offset / 4;
	DWORD oldProtect;
	if (!VirtualProtect(slot, 4, PAGE_READWRITE, &oldProtect))
		return;
	*orig = (void*)*slot;
	*slot = (DWORD)hook;
	VirtualProtect(slot, 4, oldProtect, &oldProtect);
}

// Zakladane przy pierwszym strumieniu - DLL NB i zinput istnieja dopiero po starcie gry.
static bool InstallNbStreamHooks() {
	if (nbHooksState)
		return nbHooksState > 0;
	nbHooksState = -1;
	DWORD base = (DWORD)GetModuleHandleA(NB_DLL);
	void* input = *(void**)ADDR_ZINPUT;
	if (!base || !input) {
		Log("strumienie: brak %s albo zinput - bez wsparcia strumieni", NB_DLL);
		return false;
	}
	// sygnatury w sekcjach kodu DLL w pamieci (po relokacji - adresy wewnatrz DLL w sygnaturach sa dowolne)
	sigscan::CodeRange code[8];
	int nCode = sigscan::ExecSections((const BYTE*)base, base, code, 8);
	qs::NbStreamSites s = {};
	int found = nCode ? qs::FindNbStreamSites(code, nCode, &s) : -1;
	if (found != qs::NBF_OK) {
		Log("strumienie: inna wersja %s (baza %08X): %s (sekcji kodu %d, funkcje %d/%d, jmp %d call %d, ogien call %d jmp %d)"
			" - bez wsparcia strumieni", NB_DLL, base, found < 0 ? "brak sekcji kodu" : qs::NbFindResultText(found), nCode,
			s.streamFnHits, s.fireFnHits, s.streamJmp, s.streamCall, s.fireCall, s.fireJmp);
		return false;
	}
	// oba miejsca sprawdzone przed lataniem (PatchRel32 sprawdza jeszcze raz opcode i cel) - zeby nie zostawic polowy
	if (!PatchRel32(s.streamSite, 0xE9, s.streamFn, &NbStreamWrap, &nbStreamTarget))
		return false;
	if (!PatchRel32(s.fireSite, 0xE8, s.fireFn, &NbFireStreamWrap, &nbFireStreamTarget)) {
		PatchRel32(s.streamSite, 0xE9, (DWORD)&NbStreamWrap, (void*)nbStreamTarget, 0);   // cofniecie pierwszego
		return false;
	}
	PatchVtable(input, VT_INPUT_GETSTATE, &HookGetState, (void**)&origGetState);
	PatchVtable(input, VT_INPUT_MOUSELEFT, &HookMouseLeft, (void**)&origMouseLeft);
	nbHooksState = 1;
	Log("strumienie: haki NB zalozone (baza %s %08X; jmp %08X -> %08X, call %08X -> %08X, przy bazie 0x10000000:"
		" %08X / %08X)", NB_DLL, base, s.streamSite, s.streamFn, s.fireSite, s.fireFn,
		s.streamSite - base + 0x10000000, s.fireSite - base + 0x10000000);
	return true;
}

// --- wywolania ksiegi z fmode udajacym tryb magii --------------------------

struct MagicModeSpoof {
	int& fmode;
	int saved;
	explicit MagicModeSpoof(BYTE* npc) : fmode(*(int*)(npc + OFS_NPC_FMODE)), saved(fmode) { fmode = FMODE_MAGIC; }
	~MagicModeSpoof() { fmode = saved; }
};

static int SpellStatus(void* spell) {
	return ((GetInt_t)ADDR_SPELL_GETSTATUS)(spell, 0);
}

static int Mana(BYTE* npc) {
	return ((GetAttribute_t)ADDR_NPC_GETATTRIBUTE)(npc, 0, ATR_MANA);
}

// oCMag_Book::wld (+0x18) ustawia dopiero Open (0x47702B: wld = owner->homeWorld). Szybki rzut
// ksiegi nie otwiera, a Spell_Cast przy zuzyciu ostatniego zwoju (MultiSlot, ilosc 0) wola
// wld->RemoveVob (0x476CC1) - bez zwyklego trybu magii w tej sesji wld == NULL i gra pada.
static int OFS_BOOK_WLD     = 0x18; // zCWorld* oCMag_Book::wld
static int OFS_VOB_HOMEWRLD = 0xB8; // zCWorld* zCVob::homeWorld

static void EnsureBookWorld(BYTE* npc, void* book) {
	void** wld = (void**)((BYTE*)book + OFS_BOOK_WLD);
	if (!*wld && npc) {
		*wld = *(void**)(npc + OFS_VOB_HOMEWRLD);
		Log("   ksiega bez swiata - ustawiony swiat bohatera %p", *wld);
	}
}

// --- cel i kierunek rzutu (tylko Gothic II) --------------------------------

static void AddRef(void* obj) {
	if (obj)
		++*(int*)((BYTE*)obj + OFS_OBJ_REFCTR);
}

static void Release(void* obj) {
	if (obj)
		((GetInt_t)ADDR_OBJ_RELEASE)(obj, 0);
}

static bool IsNpc(void* vob) {
	if (!vob)
		return false;
	for (BYTE* cd = (BYTE*)((GetPtr_t)(*(DWORD**)vob)[0])(vob, 0); cd; cd = *(BYTE**)(cd + OFS_CLASSDEF_BASE))
		if ((DWORD)cd == ADDR_CLASSDEF_NPC)
			return true;
	return false;
}

static const float* VobTrafo(void* vob) {
	return (const float*)((BYTE*)vob + OFS_VOB_TRAFO);
}

static int VobDistance(void* a, void* b) {
	const float* p = VobTrafo(a);
	const float* q = VobTrafo(b);
	float dx = q[3] - p[3], dy = q[7] - p[7], dz = q[11] - p[11];
	return (int)sqrtf(dx * dx + dy * dy + dz * dz);
}

// Dobry cel czaru: zywy NPC przyjety przez oCSpell::IsValidTarget (kat, zasieg i linia wzroku z ustawien czaru, typ celu).
static bool IsSpellTarget(void* spell, void* vob) {
	return IsNpc(vob) && *(int*)((BYTE*)vob + OFS_NPC_HP) > 0
		&& ((IntByPtr_t)ADDR_SPELL_VALIDTARGET)(spell, 0, vob) != 0;
}

// Fokus jak w trybie magii: silnik zbiera go z zakresami Focus_Magic, a przy fmode 7 przepuszcza tylko cele dobre dla
// wybranego czaru (CollectFocusVob 0x733FCF -> IsValidTarget). Na jedno wywolanie CollectFocusVob(1) (1 = bez czekania
// na odstep 100-250 ms) podstawiamy tryb fokusu magii i fmode 7, potem wracaja tryb, fokus i wrog sprzed - fokus na
// ekranie nie mignie, a walka wrecz nie dostaje dalekiego wroga. Lista vobow bohatera zostaje z zasiegu magii do
// najblizszego zwyklego zbierania fokusu.
static void* MagicFocusTarget(BYTE* npc) {
	void** current = (void**)ADDR_FOCUS_CURRENT;
	void* magic = ((void**)ADDR_FOCUS_LIST)[FOCUS_MAGIC];
	if (!magic || !*current)
		return 0;
	void* oldFocus = *(void**)(npc + OFS_NPC_FOCUSVOB);
	void* oldEnemy = *(void**)(npc + OFS_NPC_ENEMY);
	AddRef(oldFocus);   // CollectFocusVob zwalnia fokus i wroga
	AddRef(oldEnemy);
	void* savedMode = *current;
	*current = magic;
	{
		MagicModeSpoof spoof(npc);
		((VoidByInt_t)ADDR_NPC_COLLECTFOCUS)(npc, 0, 1);
	}
	*current = savedMode;
	void* found = *(void**)(npc + OFS_NPC_FOCUSVOB);
	AddRef(found);
	((VoidByPtr_t)ADDR_NPC_SETFOCUSVOB)(npc, 0, oldFocus);
	((VoidByPtr_t)ADDR_NPC_SETENEMY)(npc, 0, oldEnemy);
	Release(oldFocus);
	Release(oldEnemy);
	Release(found);     // NPC trzyma swiat - wskaznik wazny do Spell_Setup w tej klatce
	return found;
}

typedef int (__cdecl* DismHoldState_t)(void* spell, float* frac, int* alt, int* wantsAim, int* showBar);
static DismHoldState_t DismHoldFn();

// Czy czar to Znak zDismember (eksport ZDISM_SignHoldState = 1) - wtedy cele liczy zDismember.
static bool IsDismSign(void* spell) {
	DismHoldState_t fn = DismHoldFn();
	float frac = 0.0f;
	int alt = 0, wantsAim = 0, showBar = 0;
	return fn && spell && fn(spell, &frac, &alt, &wantsAim, &showBar) == 1;
}

// Cel szybkiego rzutu dla czarow celujacych w fokus (TARGET_COLLECT_FOCUS*): biezacy fokus, jesli to dobry cel czaru
// (widoczny fokus wygrywa), inaczej fokus magii (MagicFocusTarget). Nic - dawny fokus (Spell_Setup odrzuci przedmiot
// albo moba, a czar bez celu poleci prosto - AimForward). Inne czary (swiatlo, przywolania, na siebie) jak dawniej.
static void* PickTarget(BYTE* npc, void* spell, const char** how) {
	void* focus = ((GetPtr_t)ADDR_NPC_GETFOCUSVOB)(npc, 0);
	*how = focus ? "fokus" : "brak";
	if (isGothic1 || !magicFocus)
		return focus;
	int algo = *(int*)((BYTE*)spell + OFS_SPELL_ALGO);
	if (algo != TARGET_COLLECT_FOCUS && algo != TARGET_COLLECT_FOCUS_FALLBACK_NONE
		&& algo != TARGET_COLLECT_FOCUS_FALLBACK_CASTER)
		return focus;
	bool dismSign = IsDismSign(spell);
	if (!qs::WantsMagicFocus(algo, *(int*)((BYTE*)spell + OFS_SPELL_AZI), dismSign)) {
		// Znaki W3 Aard / Igni / Yrden (kat 0) i kazdy Znak zDismember (tez Axii): bez szukania celu, stozek / krag / cel
		// Axii z celownika liczy zDismember - bez wymogu wrogosci i niezaleznie od fokusu (brak celu nie blokuje rzutu;
		// biezacy fokus idzie dalej - zDismember bierze go, jesli lezy w stozku)
		*how = dismSign ? (focus ? "fokus - cel wybiera zDismember" : "brak - cel wybiera zDismember")
			: "kierunek - cele liczy zDismember";
		return focus;
	}
	void** caster = (void**)((BYTE*)spell + OFS_SPELL_CASTER);
	if (!*caster)
		*caster = npc;   // IsValidTarget liczy kat i zasieg od rzucajacego - Spell_Setup i tak go tu wpisze
	if (focus && IsSpellTarget(spell, focus))
		return focus;
	if (*(int*)(npc + OFS_NPC_FMODE) == FMODE_MAGIC)
		return focus;    // w trybie magii fokus juz jest fokusem magii
	void* found = MagicFocusTarget(npc);
	if (found && IsSpellTarget(spell, found)) {
		*how = "fokus magii";
		return found;
	}
	if (focus)
		*how = "fokus - nie cel czaru";
	return focus;
}

// Bez celu (effect->target == 0, targetPos == 0) CalcTrajectory (0x48F7C0, 0x48FAC5) bierze trafo kosci emTrjOriginNode
// (ZS_RIGHTHAND) z chwili rzutu i punkt emTrjTargetRange m wzdluz jej osi (z emTrjTargetAzi/Elev) - w zwyklych
// animacjach rzutu dlon celuje do przodu, a gesty W3 pchaja lewa dlonia (make_signs.py: BIP01 L HAND), prawa z
// ZS_RIGHTHAND jest wtedy cofnieta i obrocona, wiec czar lecial w bok. Tu podajemy punkt na osi bohatera (kierunek voba
// w poziomie) w tej samej odleglosci, na wysokosci kosci - pocisk startuje z dloni i zbiega do linii wzroku bohatera.
// SetTarget(pos, 1) przelicza tor od razu (tez dzieciom efektu), a klucz rzutu i tak liczy go od nowa z targetPos.
// Tylko czary celujace w fokus - swiatlo, przywolania itp. maja zostac tam, gdzie stawia je skrypt efektu.
static void AimForward(BYTE* npc, void* spell) {
	if (isGothic1 || !aimForward || !spell || (aiming && aimReticleOn))   // celownik - kierunek daje AimApply
		return;
	BYTE* fx = *(BYTE**)((BYTE*)spell + OFS_SPELL_EFFECT);
	int algo = *(int*)((BYTE*)spell + OFS_SPELL_ALGO);
	if (!fx || *(void**)(fx + OFS_FX_TARGET) || !*(void**)(fx + OFS_FX_ORIGIN)
		|| (algo != TARGET_COLLECT_FOCUS && algo != TARGET_COLLECT_FOCUS_FALLBACK_NONE))
		return;
	float range = *(float*)(fx + OFS_FX_TRJRANGE) * 100.0f;
	if (!(range > 0.0f))
		return;   // efekt bez lotu (zostaje w miejscu kosci)
	const float* h = VobTrafo(npc);
	float ax = h[2], az = h[10];
	float len = sqrtf(ax * ax + az * az);
	if (len < 0.001f)
		return;
	void* origin = *(void**)(fx + OFS_FX_ORIGIN);
	void* node = *(void**)(fx + OFS_FX_ORGNODE);
	float m[16];
	const float* from = VobTrafo(origin);
	if (node) {
		((NodeToWorld_t)ADDR_VOB_NODETOWORLD)(origin, 0, m, node);
		from = m;
	}
	float pos[3] = { h[3] + ax / len * range, from[7], h[11] + az / len * range };
	((SetTargetPos_t)(*(DWORD**)fx)[VT_FX_SETTARGETPOS / 4])(fx, 0, pos, 1);
	Log("   bez celu - czar prosto przed bohatera (%d cm, wysokosc %s)", (int)range, node ? "kosci dloni" : "bohatera");
}

static void* ModelNode(void* model, const char* name) {
	BYTE zstr[0x14];
	((ZStringCtor_t)ADDR_ZSTRING_CTOR)(zstr, 0, name);
	void* node = ((PtrByPtr_t)ADDR_MODEL_SEARCHNODE)(model, 0, zstr);
	((ZStringDtor_t)ADDR_ZSTRING_DTOR)(zstr, 0);
	return node;
}

// Wylot z lewej dloni przy gescie W3 (gesty pchaja lewa dlonia - make_signs.py: BIP01 L HAND). Efekt czaru ma wezel
// wylotu orgNode (fx+0x49C): oCVisualFX::Init (0x4920D4) ustawia go przy kazdym Spell_Setup z emTrjOriginNode skryptu
// efektu (zCModel::SearchNode, wskaznik bez licznika referencji - wezel zyje tyle co model). Czytaja go tylko
// DoMovements (tryb FIXED: efekt co klatke w wezle - zarzewie w dloni do rzutu), InitEffect i CalcTrajectory (start toru
// przy rzucie). Podmiana na ZS_LEFTHAND tego samego modelu jest wiec tylko zmiana wskaznika: zarzewie przechodzi do
// lewej dloni, pocisk startuje z niej. Tylko efekt glowny w prawej dloni bohatera (emTrjOriginNode = ZS_RIGHTHAND) -
// efekty dzieci i inne wezly bez zmian. Przywrocenie zaraz po Spell_Cast (CastNow) albo przed zatrzymaniem czaru
// (StopNow); leftFx czytamy tylko, gdy to wciaz biezacy efekt czaru w ksiedze (inaczej zapominamy - nastepny
// Spell_Setup i tak ustawi wezel od nowa).
static void SignHandToLeft(BYTE* npc, void* spell) {
	if (isGothic1 || !signLeftHand || !spell)
		return;
	BYTE* fx = *(BYTE**)((BYTE*)spell + OFS_SPELL_EFFECT);
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	if (!fx || fx == leftFx || !model || *(BYTE**)(fx + OFS_FX_ORIGIN) != npc)
		return;
	void* node = *(void**)(fx + OFS_FX_ORGNODE);
	if (!node || node != ModelNode(model, "ZS_RIGHTHAND"))
		return;
	void* left = ModelNode(model, "ZS_LEFTHAND");
	if (!left) {
		Log("   brak ZS_LEFTHAND w modelu - czar z prawej dloni");
		return;
	}
	leftFx = fx;
	leftFxNode = node;
	*(void**)(fx + OFS_FX_ORGNODE) = left;
	Log("   czar w lewej dloni (gest)");
}

// Przywraca wezel wylotu efektu czaru spell, jesli to on ma podmieniony; leftFx zawsze kasowane.
static void SignHandRestore(void* spell) {
	BYTE* fx = spell ? *(BYTE**)((BYTE*)spell + OFS_SPELL_EFFECT) : 0;
	if (leftFx && fx == leftFx)
		*(void**)(fx + OFS_FX_ORGNODE) = leftFxNode;
	leftFx = 0;
}

// Kat (rad, w poziomie, ze znakiem) od kierunku bohatera do punktu (x, z) i kierunek bohatera; false - punkt w miejscu
// bohatera.
static bool YawToPoint(BYTE* npc, float x, float z, float* fx, float* fz, float* angle) {
	const float* h = VobTrafo(npc);
	float dx = x - h[3], dz = z - h[11];
	float dl = sqrtf(dx * dx + dz * dz), fl = sqrtf(h[2] * h[2] + h[10] * h[10]);
	if (dl < 1.0f || fl < 0.001f)
		return false;
	*fx = h[2] / fl;
	*fz = h[10] / fl;
	dx /= dl;
	dz /= dl;
	*angle = atan2f(*fx * dz - *fz * dx, *fx * dx + *fz * dz);
	return true;
}

static void StopTurn() {
	turning = false;
}

// Na postoju bohater obraca sie do celu (gest postoju gra na cale cialo w miejscu, wiec obraca sie razem z nim). W ruchu
// nie - kierunek biegu nalezy do gracza, a czar i tak leci do celu. Punkt celu z chwili rzutu: obrot trwa ulamek sekundy,
// a wskaznik NPC trzymany miedzy klatkami wymagalby AddRef (i Release po ewentualnej zmianie swiata).
static void StartTurn(BYTE* npc, void* target) {
	StopTurn();
	float fx, fz, angle;
	if (isGothic1 || !turnToTarget || !IsNpc(target) || !HeroStanding(heroAi))
		return;
	const float* t = VobTrafo(target);
	if (!YawToPoint(npc, t[3], t[11], &fx, &fz, &angle) || fabsf(angle) < TURN_DONE_RAD)
		return;
	turnX = t[3];
	turnZ = t[11];
	turning = true;
	turnStart = turnLast = GetTickCount();
	Log("   obrot do celu o %d st.", (int)(angle * 57.2958f));
}

// Co klatke: obrot o najwyzej TURN_RAD_PER_MS * czas (zCVob::SetHeadingYWorld - z kolizja, jak obrot w EV_AttackFinish).
static void TurnStep(BYTE* npc) {
	if (!turning)
		return;
	DWORD now = GetTickCount();
	float fx, fz, angle;
	const char* end = 0;
	if (!HeroStanding(heroAi))
		end = "bohater ruszyl";
	else if (now - turnStart >= TURN_MAX_MS)
		end = "limit czasu";
	else if (!YawToPoint(npc, turnX, turnZ, &fx, &fz, &angle))
		end = "cel w miejscu bohatera";
	else if (fabsf(angle) < TURN_DONE_RAD)
		end = "cel na wprost";
	if (end) {
		Log("   obrot do celu: koniec po %u ms (%s)", now - turnStart, end);
		StopTurn();
		return;
	}
	float step = TURN_RAD_PER_MS * (float)(now - turnLast);
	if (step <= 0.0f)
		return;   // ten sam takt zegara
	turnLast = now;
	if (angle > step)
		angle = step;
	else if (angle < -step)
		angle = -step;
	float c = cosf(angle), s = sinf(angle);
	const float* h = VobTrafo(npc);
	float pos[3] = { h[3] + (fx * c - fz * s) * 100.0f, h[7], h[11] + (fx * s + fz * c) * 100.0f };
	((VoidByPtr_t)ADDR_VOB_HEADINGYWORLD)(npc, 0, pos);
}

// --- celowanie od wcisniecia F (tylko Gothic II) ------------------------------
// AimOnPress=1: wcisniecie F od razu wlacza celowanie - bez lewego przycisku myszy (runda 5: F + LPM; LPM kolidowal ze
// sterowaniem Gothic 1 i z atakiem). Celownik czarow na srodku ekranu (tekstura Gothic Free Aim GFA_RETICLECIRCLECROSS.TGA -
// ta sama co celownik czarow GFA, rozmiar [GFA] reticleSizePx z Gothic.ini; wlasny widok, bo celownik GFA rysuja tylko jego
// haki trybu magii), bohater obraca sie za kamera, klawisz akcji (LPM) nie atakuje i nie rzuca czaru trybu magii.
// Puszczenie F = rzut w punkt celownika (promien jak GFA_AimRay: z kamery wzdluz jej osi, start przesuniety o odleglosc
// kamera - bohater, flagi GFA). Czar natychmiastowy (SENDCAST przy pierwszym Invest) czeka w rece do puszczenia F.
// Szybkie stukniecie F: bohater nie zdazy sie obrocic, ale czar i tak leci w punkt celownika (kierunek kamery) - na
// postoju bohater obraca sie do tego punktu juz po rzucie (StartTurnToPoint, TurnStep).
// Celownik tylko dla czarow z kierunkiem (AimHudUpdate): zwykle runy celujace w fokus (targetCollectAlgo FOCUS*), Znaki W3
// zDismember wedlug eksportu ZDISM_SignHoldState (Quen - tarcza na bohaterze - bez celownika; po przejsciu w tryb
// alternatywny bez kierunku, jak fala Aard wokol bohatera, celownik znika; strumien Igni i pulapka Yrden dalej celuja).
// Bez celownika: rzut dalej przy puszczeniu F, ale bez obrotu za kamera i bez podmiany celu czaru (AimForward jak dawniej).
// Znaki W3: przytrzymanie dluzej niz SignAltHold (zDismember, menu strojenia znakow, domyslnie 0,5 s) = tryb alternatywny.
// Pod celownikiem pasek ladowania (tlo BAR_BACK.TGA, wypelnienie WHITE.TGA) od wcisniecia F do progu; w chwili przejscia
// w tryb alternatywny celownik i pasek robia sie pomaranczowe. Kolor: zCView::color (+0x24) - zCView::Blit rysuje nim
// teksture tla (DrawTile, 0x7A66B8), alpha z pola alpha (+0x28). Zrodlem prawdy o czasie trzymania i progu jest zDismember
// (SignHold) - bez zDismember albo przy czarze, ktory nie jest Znakiem W3, paska nie ma.
// Znaki zDismember pytaja o celowanie przez eksport ZQS_AimActive (1 = celownik widoczny; punkt licza same).

// int ZDISM_SignHoldState(oCSpell*, float* frac, int* alt, int* wantsAim, int* showBar): 0 = nie Znak W3 bohatera.
typedef int (__cdecl* DismHoldState_t)(void* spell, float* frac, int* alt, int* wantsAim, int* showBar);

// Eksport zDismember: po nazwie modulu (Steam: zDismember.dll, New Balance: slot DIFF_01), a gdy nie ma - po wszystkich
// modulach procesu (Union moze ladowac DLL z VDF pod inna sciezka). Pluginy laduja sie w roznej kolejnosci - ponowna
// proba co 5 s.
static DismHoldState_t DismHoldFn() {
	static DismHoldState_t fn;
	static DWORD next;
	if (fn || isGothic1)
		return fn;
	DWORD now = GetTickCount();
	if ((int)(now - next) < 0)
		return 0;
	next = now + 5000;
	static const char* names[] = { "zDismember.dll", "NB_UNION_PLUGIN_DIFF_01.DLL" };
	for (int i = 0; i < 2 && !fn; i++) {
		HMODULE m = GetModuleHandleA(names[i]);
		if (m)
			fn = (DismHoldState_t)GetProcAddress(m, "ZDISM_SignHoldState");
	}
	if (!fn) {
		HMODULE mods[512];
		DWORD need = 0;
		if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need))
			for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 512 && !fn; i++)
				fn = (DismHoldState_t)GetProcAddress(mods[i], "ZDISM_SignHoldState");
	}
	if (fn)
		Log("zDismember: ZDISM_SignHoldState znaleziony - pasek ladowania trybu alternatywnego Znakow");
	return fn;
}

// Czar z kierunkiem (zwykla runa): celuje w fokus - pocisk, sen, strach itp.; swiatlo, przywolania, leczenie, deszcz
// ognia (TARGET_COLLECT_NONE / CASTER / ALL) - bez celownika.
static bool SpellAims(void* spell) {
	int algo = spell ? *(int*)((BYTE*)spell + OFS_SPELL_ALGO) : -1;
	return algo >= 0 && algo < 32 && (TARGET_COLLECT_FOCUS_ALGO_MASK & (1 << algo)) != 0;
}

static void* NewHudView(const char* tex) {
	void* v = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x200);   // sizeof(zCView) = 0x100
	if (!v)
		return 0;
	((ViewCtor_t)ADDR_VIEW_CTOR)(v, 0, 0, 0, 100, 100, 2 /* VIEW_ITEM */);
	BYTE zstr[0x14];
	((ZStringCtor_t)ADDR_ZSTRING_CTOR)(zstr, 0, tex);
	((VoidByPtr_t)ADDR_VIEW_INSERTBACK)(v, 0, zstr);
	((ZStringDtor_t)ADDR_ZSTRING_DTOR)(zstr, 0);
	return v;
}

static void HudColor(void* v, DWORD color) {
	((VoidByPtr_t)ADDR_VIEW_SETCOLOR)(v, 0, &color);
}

// Wstawienie / zdjecie z ekranu. zCView::InsertItem dopisuje na koniec listy (zList::InsertLast, 0x7ABCC8), a DrawItems
// rysuje po kolei - wstawione pozniej (wypelnienie paska po tle) rysuje sie na wierzchu.
static void HudShow(void* screen, void* v, bool show) {
	if (show)
		((InsertItem_t)ADDR_VIEW_INSERTITEM)(screen, 0, v, 0);
	else
		((VoidByPtr_t)ADDR_VIEW_REMOVEITEM)(screen, 0, v);
}

// Celownik i pasek ladowania; zmiana widocznosci tylko przy zmianie stanu (bez migotania), rozmiar i kolor co klatke
// (tanie - zmiana rozdzielczosci w trakcie tez sie przelicza). Pasek: szerokosc celownika (min. 48 px), wysokosc 1/8
// (min. 6 px), 6 px pod celownikiem, wypelnienie z marginesem 2 px.
static void AimHudDraw(bool reticle, bool bar, float frac, bool alt) {
	void* screen = *(void**)ADDR_SCREEN;
	if (!screen)
		return;
	if ((reticle || bar) && !aimView) {
		aimView = NewHudView(aimReticleTex);
		aimBarBack = NewHudView("BAR_BACK.TGA");
		aimBarFill = NewHudView("WHITE.TGA");
		if (!aimView || !aimBarBack || !aimBarFill) {
			aimView = 0;   // brak pamieci - bez celownika (nie zwalniamy - zCView bez destruktora tutaj)
			return;
		}
	}
	if (!aimView)
		return;
	IntByInt_t anx = (IntByInt_t)ADDR_VIEW_ANX, any = (IntByInt_t)ADDR_VIEW_ANY;
	VoidByTwoInt_t setSize = (VoidByTwoInt_t)ADDR_VIEW_SETSIZE, setPos = (VoidByTwoInt_t)ADDR_VIEW_SETPOS;
	int rw = anx(screen, 0, aimReticlePx), rh = any(screen, 0, aimReticlePx);
	if (reticle) {
		setSize(aimView, 0, rw, rh);
		setPos(aimView, 0, 4096 - rw / 2, 4096 - rh / 2);
		HudColor(aimView, alt ? AIM_COLOR_ALT : AIM_COLOR_NORMAL);
	}
	if (reticle != aimViewShown) {
		HudShow(screen, aimView, reticle);
		aimViewShown = reticle;
	}
	if (bar) {
		int wPx = aimReticlePx > 48 ? aimReticlePx : 48, hPx = aimReticlePx / 8 > 6 ? aimReticlePx / 8 : 6;
		int bw = anx(screen, 0, wPx), bh = any(screen, 0, hPx), mx = anx(screen, 0, 2), my = any(screen, 0, 2);
		int bx = 4096 - bw / 2, by = 4096 + rh / 2 + any(screen, 0, 6);
		int fw = (int)((float)(bw - 2 * mx) * (frac < 0.0f ? 0.0f : frac > 1.0f ? 1.0f : frac));
		setSize(aimBarBack, 0, bw, bh);
		setPos(aimBarBack, 0, bx, by);
		setSize(aimBarFill, 0, fw > 1 ? fw : 1, bh - 2 * my > 1 ? bh - 2 * my : 1);
		setPos(aimBarFill, 0, bx + mx, by + my);
		HudColor(aimBarFill, alt ? AIM_COLOR_ALT : AIM_COLOR_CHARGE);
	}
	if (bar != aimBarShown) {
		HudShow(screen, aimBarBack, bar);   // tlo przed wypelnieniem - kolejnosc rysowania
		HudShow(screen, aimBarFill, bar);
		aimBarShown = bar;
	}
}

// Co klatke celowania (i zaraz po AimStart): czy celownik ma sens, pasek ladowania Znaku W3, logi przejsc.
static void AimHudUpdate(BYTE* npc) {
	if (!aiming) {
		AimHudDraw(false, false, 0.0f, false);
		return;
	}
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	void* spell = book ? ((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0) : 0;
	float frac = 0.0f;
	int alt = 0, wants = 0, bar = 0;
	DismHoldState_t fn = spell ? DismHoldFn() : 0;
	int sign = fn ? fn(spell, &frac, &alt, &wants, &bar) : 0;
	bool reticle = sign ? wants != 0 : SpellAims(spell);
	bool showBar = sign && bar;
	DWORD held = GetTickCount() - aimStartTick;
	if (sign != aimSignState)
		Log("   celowanie: %s", sign ? "Znak W3 (zDismember) - pasek ladowania trybu alternatywnego" : "zwykla runa - bez paska");
	if (alt && !aimAltOn)
		Log("   prog trybu alternatywnego przekroczony po %u ms - %s", held,
			reticle ? "celownik i pasek pomaranczowe" : "celownik i pasek ukryte (tryb bez kierunku)");
	if (aimSignState < 0)   // pierwsza klatka celowania
		Log("   %s", reticle ? "celownik pokazany" : "bez celownika - czar bez kierunku (rzut przy puszczeniu F)");
	else if (reticle != aimReticleOn)
		Log("   celownik %s po %u ms", reticle ? "pokazany" : "ukryty (tryb bez kierunku)", held);
	aimSignState = sign;
	aimAltOn = alt != 0;
	aimReticleOn = reticle;
	AimHudDraw(reticle, showBar, frac, aimAltOn);
}

// Promien celownika: point = trafienie (albo koniec promienia), hitVob = trafiony vob (bez referencji, wazny w tej klatce).
static bool AimRay(BYTE* npc, float* point, void** hitVob) {
	BYTE* game = *(BYTE**)ADDR_OGAME;
	void* cam = game ? *(void**)(game + OFS_SESSION_CAMVOB) : 0;
	void* world = *(void**)(npc + OFS_VOB_HOMEWRLD);
	*hitVob = 0;
	if (!cam || !world)
		return false;
	const float* c = VobTrafo(cam);
	const float* h = VobTrafo(npc);
	float dx = h[3] - c[3], dy = h[7] - c[7], dz = h[11] - c[11];
	float d = sqrtf(dx * dx + dy * dy + dz * dz);
	float from[3] = { c[3] + c[2] * d, c[7] + c[6] * d, c[11] + c[10] * d };
	float dir[3] = { c[2] * AIM_RAY_LENGTH, c[6] * AIM_RAY_LENGTH, c[10] * AIM_RAY_LENGTH };
	point[0] = from[0] + dir[0];
	point[1] = from[1] + dir[1];
	point[2] = from[2] + dir[2];
	if (((TraceRay_t)ADDR_WORLD_TRACERAY)(world, from, dir, npc, AIM_RAY_FLAGS)) {
		BYTE* rep = (BYTE*)world + OFS_WORLD_RAYREPORT;
		if (*(int*)rep) {
			*hitVob = *(void**)(rep + 0x04);
			memcpy(point, rep + 0x0C, sizeof(float) * 3);
		}
	}
	return true;
}

// npc = bohater (gracz trzyma F).
static void AimStart(BYTE* npc, const char* why) {
	if (aiming)
		return;
	aiming = true;
	aimReticleOn = false;
	aimAltOn = false;
	aimSignState = -1;
	aimStartTick = aimTurnLast = GetTickCount();
	StopTurn();
	Log("   celowanie od wcisniecia F (%s) - celownik %s %d px", why, aimReticleTex, aimReticlePx);
	AimHudUpdate(npc);   // od razu - szybkie stukniecie F ma juz kierunek z celownika
}

static void AimEnd() {
	if (aiming)
		Log("   celowanie - koniec po %u ms", GetTickCount() - aimStartTick);
	aiming = false;
	aimInstant = false;
	aimReticleOn = false;
	aimAltOn = false;
	AimHudDraw(false, false, 0.0f, false);
}

// Co klatke celowania z celownikiem: obrot bohatera do kierunku kamery (do ok. 720 st./s, SetHeadingYWorld - z kolizja).
static void AimTurnStep(BYTE* npc) {
	BYTE* game = *(BYTE**)ADDR_OGAME;
	void* cam = game ? *(void**)(game + OFS_SESSION_CAMVOB) : 0;
	DWORD now = GetTickCount();
	float step = 2.0f * TURN_RAD_PER_MS * (float)(now - aimTurnLast);
	aimTurnLast = now;
	if (!cam || step <= 0.0f)
		return;
	const float* c = VobTrafo(cam);
	const float* h = VobTrafo(npc);
	float fx, fz, angle;
	if (!YawToPoint(npc, h[3] + c[2] * 1000.0f, h[11] + c[10] * 1000.0f, &fx, &fz, &angle) || fabsf(angle) < TURN_DONE_RAD)
		return;
	if (angle > step)
		angle = step;
	else if (angle < -step)
		angle = -step;
	float cs = cosf(angle), sn = sinf(angle);
	float pos[3] = { h[3] + (fx * cs - fz * sn) * 100.0f, h[7], h[11] + (fx * sn + fz * cs) * 100.0f };
	((VoidByPtr_t)ADDR_VOB_HEADINGYWORLD)(npc, 0, pos);
}

// Po rzucie z celownika na postoju: obrot do punktu celownika (TurnStep) - przy szybkim stuknieciu F bohater nie zdazyl
// obrocic sie za kamera, a czar juz leci w punkt celownika; gest postoju gra na cale cialo, wiec obraca sie razem z nim.
static void StartTurnToPoint(BYTE* npc, float x, float z) {
	StopTurn();
	float fx, fz, angle;
	if (isGothic1 || !turnToTarget || !HeroStanding(heroAi))
		return;
	if (!YawToPoint(npc, x, z, &fx, &fz, &angle) || fabsf(angle) < TURN_DONE_RAD)
		return;
	turnX = x;
	turnZ = z;
	turning = true;
	turnStart = turnLast = GetTickCount();
	Log("   obrot do punktu celownika o %d st. (po rzucie)", (int)(angle * 57.2958f));
}

// Przed rzutem z celownika: efekt czaru leci do NPC pod celownikiem (gdy to dobry cel czaru) albo w punkt celownika.
static void AimApply(BYTE* npc, void* spell) {
	BYTE* fx = spell ? *(BYTE**)((BYTE*)spell + OFS_SPELL_EFFECT) : 0;
	float point[3];
	void* hit;
	if (!AimRay(npc, point, &hit))
		return;
	DWORD held = GetTickCount() - aimStartTick;
	if (fx && hit && IsSpellTarget(spell, hit)) {
		((SetTargetVob_t)(*(DWORD**)fx)[VT_FX_SETTARGETVOB / 4])(fx, 0, hit, 1);
		Log("   celowanie: rzut w NPC pod celownikiem (%d cm), F trzymane %u ms", VobDistance(npc, hit), held);
	}
	else if (fx) {
		((SetTargetPos_t)(*(DWORD**)fx)[VT_FX_SETTARGETPOS / 4])(fx, 0, point, 1);
		Log("   celowanie: rzut w punkt celownika (%d, %d, %d), F trzymane %u ms", (int)point[0], (int)point[1], (int)point[2], held);
	}
	else
		Log("   celowanie: czar bez efektu do podmiany celu (Znak W3 - punkt bierze zDismember), F trzymane %u ms", held);
	StartTurnToPoint(npc, point[0], point[2]);
}

static int EmNumMessages(BYTE* npc) {
	BYTE* em = isGothic1 ? 0 : *(BYTE**)(npc + OFS_VOB_EM);
	return em ? ((GetInt_t)(*(DWORD**)em)[VT_EM_NUMMSG / 4])(em, 0) : 0;
}

// Spell_Cast po zuzyciu ostatniego czaru w ksiedze (ostatni zwoj, ksiega pusta - 0x476CCA) wysyla bohaterowi
// oCMsgWeapon(EV_REMOVEWEAPON) (0x476CFF): w trybie magii to wyjscie z magii, ale przy szybkim rzucie z mieczem
// bohater schowalby bron (EV_RemoveWeapon -> StandUp - zatrzymanie w biegu). Poza trybem magii usuwamy te wiadomosc,
// zanim kolejka ja wykona (oCNpcMessage::IsJob = 1 - czeka na koncu listy). Tylko Gothic II (adresy G2A).
static void DropRemoveWeaponMsg(BYTE* npc, void* book, int msgsBefore) {
	if (isGothic1 || *(int*)((BYTE*)book + OFS_BOOK_NSPELLS) != 0)
		return;
	BYTE* em = *(BYTE**)(npc + OFS_VOB_EM);
	if (!em)
		return;
	DWORD* vt = *(DWORD**)em;
	int n = ((GetInt_t)vt[VT_EM_NUMMSG / 4])(em, 0);
	for (int i = n - 1; i >= 0 && i >= msgsBefore; i--) {
		BYTE* msg = (BYTE*)((PtrByInt_t)vt[VT_EM_GETMSG / 4])(em, 0, i);
		if (!msg || (DWORD)((GetPtr_t)(*(DWORD**)msg)[0])(msg, 0) != ADDR_CLASSDEF_MSGWEAPON)
			continue;
		if (*(WORD*)(msg + OFS_MSG_SUBTYPE) != EV_REMOVEWEAPON || (*(int*)(msg + OFS_NPCMSG_FLAGS) & 4))
			continue;
		((VoidByPtr_t)vt[VT_EM_DELETE / 4])(em, 0, msg);
		Log("   ostatni czar z ksiegi - usuniete EV_REMOVEWEAPON (bron zostaje w rece)");
		return;
	}
}

// playAni: gest Znaku albo zwykla animacja rzutu (false = gest juz trwa albo animacje wybral wywolujacy).
// alt: rzut w trybie alternatywnym Znaku W3 (zDismember) - gest "w ziemie" (StartAltGesture), gdy Znak go ma.
static void CastNow(BYTE* npc, void* book, void* spell, bool playAni = true, bool alt = false) {
	bool gesture = false;
	if (playAni) {
		gesture = StartGesture(npc, ((GetInt_t)ADDR_SPELL_GETSPELLID)(spell, 0), alt) >= 0;
		if (!gesture)
			PlayCastAni(npc);
	}
	EnsureBookWorld(npc, book);
	if (!streaming && !streamEnding) {   // strumienie NB prowadzi NB
		if (gesture)
			SignHandToLeft(npc, spell);   // gest od tej klatki (rzut po naladowaniu) - wylot z lewej dloni
		AimForward(npc, spell);
		if (aiming && aimReticleOn)
			AimApply(npc, spell);   // rzut w punkt celownika (czar z kierunkiem)
	}
	// Spell_Cast oddaje efekt swiatu (oCSpell::effect = 0) - wlasna referencja, zeby po rzucie przywrocic wezel wylotu
	BYTE* fx = *(BYTE**)((BYTE*)spell + OFS_SPELL_EFFECT);
	bool leftHand = fx && fx == leftFx;
	if (leftHand)
		AddRef(fx);
	int mode = (streaming || streamEnding) ? realMode : *(int*)(npc + OFS_NPC_FMODE);
	int msgsBefore = EmNumMessages(npc);
	{
		MagicModeSpoof spoof(npc);
		((Void_t)ADDR_BOOK_SPELLCAST)(book, 0);
	}
	if (leftHand) {
		*(void**)(fx + OFS_FX_ORGNODE) = leftFxNode;   // tor juz policzony od lewej dloni
		Release(fx);
	}
	leftFx = 0;
	if (mode != FMODE_MAGIC)
		DropRemoveWeaponMsg(npc, book, msgsBefore);
	castHappened = true;
	Log("   rzucony, status po rzucie %d, mana %d", SpellStatus(spell), Mana(npc));
}

static void StartCooldown(BYTE* npc) {
	ManaTrackUpdate(npc);
	if (!castHappened && manaSpentTotal <= 0) {
		Log("   bez odnowienia (nic nie rzucone)");
		return;
	}
	int cd = cooldownMs + castCircle * cooldownPerCircleMs + manaSpentTotal * cooldownPerManaMs;
	if (cd < 0)
		cd = 0;
	cooldownUntil = GetTickCount() + cd;
	Log("   odnowienie %d ms (krag %d, mana %d)", cd, castCircle, manaSpentTotal);
}

static void StopNow(BYTE* npc, void* book) {
	SignHandRestore(((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0));
	{
		MagicModeSpoof spoof(npc);
		((Void_t)ADDR_BOOK_STOPSPELL)(book, 0);
	}
	Log("   zatrzymany, mana %d", Mana(npc));
}

static bool IsStopStatus(int status) {
	return status == SPL_SENDSTOP || status == SPL_DONTINVEST;
}

static bool IsStreamSpell(int id) {
	for (int i = 0; i < streamIdCount; i++)
		if (streamIds[i] == id)
			return true;
	return false;
}

static void EndInvest(BYTE* npc) {
	investing = false;
	charged = false;
	if (!(pendingCast && pendingKeepAim))
		AimEnd();   // Znak W3 odlozony do konca pchniecia: celownik do rzutu (AimApply / ZQS_AimActive w chwili rzutu)
	if (streaming) {
		// fmode zostaje 7 jeszcze chwile (FinishStreamEnding), bez podsuwanego klawisza
		streaming = false;
		streamEnding = true;
		endStart = GetTickCount();
		endWrapCalls = nbWrapCalls;
		ManaTrackUpdate(npc);
		return;	// odnowienie po zakonczeniu strumienia przez NB (FinishStreamEnding)
	}
	StartCooldown(npc);
}

static void FinishStreamEnding(BYTE* npc) {
	streamEnding = false;
	*(int*)(npc + OFS_NPC_FMODE) = realMode;
	Log("   strumien zakonczony po %u ms, przebiegi NB %d", GetTickCount() - endStart, nbWrapCalls - endWrapCalls);
	if (realMode != FMODE_MAGIC)
		stanceFixUntil = GetTickCount() + 1500;
	StartCooldown(npc);
}

// aim: celowanie od wcisniecia F (AimOnPress) - rzut przy puszczeniu F (czar natychmiastowy czeka w rece), celownik dla
// czarow z kierunkiem.
static void StartQuickCast(BYTE* npc, bool aim) {
	if (XbowBusy()) {
		Log("F: strzal z kuszy w toku - bez Znaku");
		return;
	}
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	void* spell = book ? ((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0) : 0;
	if (!spell) {
		Log("F: brak wybranej runy");
		ShowMessage("Brak wybranej runy");
		return;
	}
	const char* how;
	void* target = PickTarget(npc, spell, &how);
	int manaBefore = Mana(npc);
	int spellId = ((GetInt_t)ADDR_SPELL_GETSPELLID)(spell, 0);
	ManaTrackStart(npc);
	stanceFixUntil = 0;
	castHappened = false;
	void* item = ((GetSpellItem_t)ADDR_BOOK_SPELLITEM)(book, 0, spell);
	castCircle = item ? *(int*)((BYTE*)item + OFS_ITEM_MAGCIRCLE) : 0;
	int setupStatus, status;
	EnsureBookWorld(npc, book);
	{
		MagicModeSpoof spoof(npc);
		if (isGothic1)
			((SpellSetupG1_t)ADDR_BOOK_SPELLSETUP)(book, 0, npc, target);
		else
			((SpellSetup_t)ADDR_BOOK_SPELLSETUP)(book, 0, npc, target, 0);
		setupStatus = SpellStatus(spell);
		((Void_t)ADDR_BOOK_SPELLINVEST)(book, 0);
		status = SpellStatus(spell);
	}
	Log("F: tryb %d, czar id %d (%08X), cel %08X (%s, %d cm), mana %d, status po Setup %d, po Invest %d, manaInvested %d",
		*(int*)(npc + OFS_NPC_FMODE), spellId, (DWORD)spell, (DWORD)target, how,
		target && !isGothic1 ? VobDistance(npc, target) : 0, manaBefore, setupStatus,
		status, *(int*)((BYTE*)spell + OFS_SPELL_MANAINV));

	if (status == SPL_SENDCAST && aim) {
		// celowanie: czar natychmiastowy w rece do puszczenia F (ContinueQuickCast: charged + aimInstant)
		investing = true;
		charged = true;
		aimInstant = true;
		investSpell = spell;
		lastStatus = status;
		lastStatusLog = GetTickCount();
		AimStart(npc, "czar natychmiastowy - rzut przy puszczeniu F");
	}
	else if (status == SPL_SENDCAST) {
		int delay = StartGesture(npc, spellId);
		if (delay >= 0)
			SignHandToLeft(npc, spell);   // gest W3 - zarzewie do rzutu i wylot z lewej dloni
		if (delay > 0) {
			// czar w rece, wylatuje z pchnieciem dloni (FirePendingCast)
			pendingCast = true;
			pendingSpell = spell;
			pendingCastAt = GetTickCount() + delay;
			Log("   rzut za %d ms (gest)", delay);
		}
		else {
			if (delay < 0)
				PlayCastAni(npc);   // gestu nie ma - zwykla animacja (bez drugiej proby gestu w CastNow)
			CastNow(npc, book, spell, false);
			StartCooldown(npc);
		}
	}
	else if (IsStopStatus(status))
		StopNow(npc, book);
	else {
		// czar ladowany albo strumien: laduje sie, dopoki klawisz jest wcisniety
		investing = true;
		charged = false;
		investSpell = spell;
		lastStatus = status;
		lastStatusLog = GetTickCount();
		// tryb strumienia tylko z New Balance - w innych modach te same id moga byc zwyklymi czarami
		if (IsStreamSpell(spellId) && InstallNbStreamHooks()) {
			fakeInputCalls = 0;
			streamSpellId = spellId;
			streaming = true;
			// Uduszenie i strumien mroku nie maja w NB animacji rak - dajemy te z elektrokinezy
			if (spellId == 0xBD || spellId == 0xBF) {
				StartAniByName(npc, "T_MAGRUN_2_FSCCAST");
				StartAniByName(npc, "T_MAGRUN_2_FSLCAST");
			}
			realMode = *(int*)(npc + OFS_NPC_FMODE);
			*(int*)(npc + OFS_NPC_FMODE) = FMODE_MAGIC;
			Log("   strumien - fmode udaje 7 do puszczenia (prawdziwy %d)", realMode);
		}
		else
			Log("   ladowanie...");
		if (aim)
			AimStart(npc, "ladowanie");
	}
	// czar jeszcze w rece (gest do pchniecia dloni, ladowanie) - na postoju obrot do celu
	if (target && (pendingCast || investing) && !aiming)
		StartTurn(npc, target);
	else
		StopTurn();
}

static void EndPendingSync() {
	if (pendingKeepAim)
		AimEnd();
	pendingSync = pendingKeepAim = false;
	pendingCancel = 0;
}

// Znak W3 przerwany przed koncem pchniecia dloni (jak w W3): czar z reki bez rzutu - bez skutku, many i odnowienia
// (mane i odnowienie liczy zDismember w oCSpell::Cast, ktorego nie bylo; zQuickSign odnawia tylko po rzucie). Gest
// wygaszany (FadeOutAni z jego blend_out), zeby nie dograc pchniecia bez skutku.
static void CancelPendingCast(BYTE* npc, const char* why) {
	pendingCast = false;
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	if (book)
		StopNow(npc, book);
	void* model = ((GetPtr_t)ADDR_NPC_GETMODEL)(npc, 0);
	void* act = model && gestureAniId >= 0 ? SignActive(model, gestureAniId, false) : 0;
	if (act && !isGothic1)
		((VoidByPtr_t)ADDR_MODEL_FADEACTIVE)(model, 0, act);
	EndGesture(npc);
	Log("   Znak przerwany przed koncem pchniecia (%s) - bez skutku i bez kosztu%s", why, act ? ", gest wygaszony" : "");
	EndPendingSync();
}

static void FirePendingCast(BYTE* npc) {
	pendingCast = false;
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	void* spell = book ? ((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0) : 0;
	if (!spell || spell != pendingSpell) {
		Log("   rzut odwolany (zmiana runy w trakcie gestu)");
		if (book)
			StopNow(npc, book);
		EndPendingSync();
		return;
	}
	if (pendingSync) {
		// HookActionMove nie byl wolany (blokada ruchu: unik / przewrot / przeskok / finisher, dialog) - gest minal bez rzutu
		int late = (int)(GetTickCount() - pendingCastAt);
		if (qs::SyncVerdict(true, false, false, late, signSyncLateMs) == qs::QSYNC_LATE) {
			char why[64];
			wsprintfA(why, "spozniony o %d ms - bohater zablokowany", late);
			CancelPendingCast(npc, why);
			return;
		}
	}
	CastNow(npc, book, spell, false);
	StartCooldown(npc);
	WatchGesture(npc);   // rzut moze przerwac gest - od razu wznowic od pchniecia
	EndPendingSync();
}

// Tryb zwykly Znaku W3 (puszczenie F przed SignAltHold): gest od razu, rzut (Spell_Cast - w zDismember skutek, mana,
// odnowienie) z koncem pchniecia dloni (SignCastSyncMs z klatki animacji, make_signs.py push_end). false = nie Znak W3
// zDismember albo SignCastSync=0 - rzut od razu jak dawniej (wolajacy robi CastNow).
static bool CastSynced(BYTE* npc, void* book, void* spell) {
	if (!signCastSync || isGothic1)
		return false;
	DismHoldState_t fn = DismHoldFn();
	if (!fn || !fn(spell, 0, 0, 0, 0))
		return false;
	int spellId = ((GetInt_t)ADDR_SPELL_GETSPELLID)(spell, 0);
	int delay = StartGesture(npc, spellId);
	if (delay < 0) {
		PlayCastAni(npc);   // gestu nie ma (nakladka czeka na postoj) - rzut od razu
		CastNow(npc, book, spell, false);
		return true;
	}
	int ms = qs::SyncDelayMs(signCastSyncMs[SignFor(spellId)], delay);
	SignHandToLeft(npc, spell);   // zarzewie w lewej dloni do rzutu
	pendingCast = true;
	pendingSync = true;
	pendingSpell = spell;
	pendingCastAt = GetTickCount() + ms;
	pendingKeepAim = aiming && aimReticleOn;
	pendingCancel = 0;
	Log("   rzut z koncem pchniecia dloni za %d ms%s", ms, pendingKeepAim ? " (celownik do rzutu)" : "");
	return true;
}

static void ContinueQuickCast(BYTE* npc, bool keyDown) {
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	void* spell = book ? ((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0) : 0;
	if (spell && spell != investSpell && streaming
		&& ((GetInt_t)ADDR_SPELL_GETSPELLID)(spell, 0) == streamSpellId) {
		// NB potrafi w trakcie strumienia podmienic obiekt czaru - to wciaz ten sam czar
		Log("   nowy obiekt czaru %08X (to samo id) - strumien trwa", (DWORD)spell);
		investSpell = spell;
	}
	if (!spell || spell != investSpell) {
		// zmiana runy w trakcie ladowania
		Log("   ladowanie przerwane (zmiana runy)");
		if (book)
			StopNow(npc, book);
		EndInvest(npc);
		return;
	}

	if (keyDown && charged)
		return;	// naladowany - czeka w rece na puszczenie

	int status;
	{
		MagicModeSpoof spoof(npc);
		if (keyDown)
			((Void_t)ADDR_BOOK_SPELLINVEST)(book, 0);
		else if (!aimInstant)
			((Void_t)ADDR_SPELL_RELEASE)(spell, 0);
		// czar natychmiastowy z celowania: status SENDCAST z pierwszego Invest - rzut bez SetReleaseStatus (skrypt
		// Spell_ProcessMana_Release dalby SENDSTOP)
		status = aimInstant && !keyDown ? SPL_SENDCAST : SpellStatus(spell);
	}

	ManaTrackUpdate(npc);
	DWORD now = GetTickCount();
	if (keyDown && (status != lastStatus || now - lastStatusLog >= 500)) {
		Log("   trzymany: status %d, manaInvested %d, mana %d, podsuniety klawisz NB %d razy", status,
			*(int*)((BYTE*)spell + OFS_SPELL_MANAINV), Mana(npc), fakeInputCalls);
		lastStatus = status;
		lastStatusLog = now;
	}

	if (!keyDown) {
		Log("   puszczony: status %d, manaInvested %d", status, *(int*)((BYTE*)spell + OFS_SPELL_MANAINV));
		if (status == SPL_SENDCAST) {
			if (!CastSynced(npc, book, spell))   // Znak W3: rzut z koncem pchniecia dloni (SignCastSync)
				CastNow(npc, book, spell);
		}
		else
			StopNow(npc, book);
		EndInvest(npc);
	}
	else if (status == SPL_SENDCAST) {
		DismHoldState_t fn = DismHoldFn();
		int alt = 0;
		bool isAlt = fn && fn(spell, 0, &alt, 0, 0) && alt;
		Log("   naladowany do konca - rzut%s", isAlt ? " (Znak W3: prog trybu alternatywnego przekroczony - rzut od razu)" : "");
		CastNow(npc, book, spell, true, isAlt);
		EndInvest(npc);
	}
	else if (status == SPL_DONTINVEST) {
		// jak w trybie magii: koniec ladowania (max poziom albo brak many na nastepny), czar zostaje
		// w rece do puszczenia klawisza
		charged = true;
		Log("   naladowany - czeka na puszczenie");
	}
	else if (status == SPL_SENDSTOP) {
		Log("   ladowanie zatrzymane przez skrypt");
		StopNow(npc, book);
		EndInvest(npc);
	}
}

// --- wybor runy pod F bez dobywania (tylko Gothic II) -------------------------
// Klawisze run gra obsluguje w oCGame::HandleEvent (0x6FC170): DIK_4..DIK_0 (0x05..0x0B) ida przez tablice skokow na
// 0x6FCFAD - bramka 0x6805C0 (oCAniCtrl_Human::IsStanding / IsWalking / mob / luk), potem keyNo = znak klawisza - 0x33,
// w trybie magii klawisz runy w rece = schowanie (EV_REMOVEWEAPON), inaczej GetNoOfSpellByKey(keyNo) ->
// SetFrontSpell(n - 1), nazwa czaru na ekranie i, gdy ksiega zamknieta, wyciagniecie runy (oCMsgWeapon EV_DRAWWEAPON, tryb 7).
// Hak na wywolanie bramki (SpellKeyGateWrap): przy trzymanym Shift (SelectWithShift=1) bramka zwraca 0 - gra konczy
// obsluge klawisza (0x6FD3E5, klawisz uznany za obsluzony), bez dobycia runy i zmiany broni.
// Sam wybor robi SelectUpdate co klatke z HookActionMove (odczyt klawiszy zinput->KeyPressed, nowe wcisniecie cyfry przy
// trzymanym Shift - qs::DigitEdge), niezaleznie od tego, czy klawisz w ogole doszedl do HandleEvent (inny plugin moze go
// przechwycic wczesniej). SetFrontSpell (0x4789C0) poza trybem magii tylko wpisuje spellnr (0x478A9E) - to zmiana
// wybranego czaru ksiegi bez zadnych animacji; w trybie magii zamienia rune w rece (jak klawisz gry, z jej bramkami).
// Shift + kolko (SignWheel=1): nastepny / poprzedni znak W3 (SignWheelIds) w kolejnosci slotow, z zawijaniem
// (qs::WheelPick). Kolko z zCInput::GetMousePos (z) - gra robi z niego tez klawisze MOUSE_WHEELUP/DOWN (0x20A/0x20B).
// Komunikat "Znak: <nazwa>" (opis przedmiotu runy) i etykieta wybranej runy nad paskiem many (ShowSelected=1) - wlasne
// widoki zCView z kopia vtable, w ktorej DrawItems (HookViewDrawItems) decyduje co klatke rysowania o widocznosci.

static DWORD origSpellKeyGate;          // cel wywolania pod ADDR_CALL_SPELLKEYGATE (0x6805C0 albo hak innego pluginu)
static bool  digitPrev[qs::KEYNO_MAX + 1];
static DWORD selPollLast;               // ostatni SelectUpdate - przerwa = trzymane klawisze to nie nowe wcisniecia
static DWORD gateBlockLogTick;
static void* selView;                   // etykieta wybranej runy (pelny ekran, tylko tekst)
static void* msgView;                   // komunikat zmiany wyboru
static DWORD viewVt[13];                // kopia vtable zCView ([0] = RTTI) z DrawItems -> HookViewDrawItems
static char  selText[96];               // biezacy tekst etykiety ("" = brak)
static int   selX = -1, selY = -1;

struct ZStr {
	BYTE b[0x14];
	explicit ZStr(const char* s) { ((ZStringCtor_t)ADDR_ZSTRING_CTOR)(b, 0, s); }
	~ZStr() { ((ZStringDtor_t)ADDR_ZSTRING_DTOR)(b, 0); }
};

// Tekst zSTRING z gry (std::string w srodku: wskaznik +8, dlugosc +0x0C).
static const char* ZStrText(const BYTE* z) {
	const char* p = *(const char* const*)(z + 8);
	return p ? p : "";
}

static bool KeyDown(void* input, int dik) {
	return ((KeyPressed_t)(*(DWORD**)input)[VT_INPUT_KEYPRESSED / 4])(input, 0, dik) != 0;
}

static bool ShiftDown() {
	void* input = *(void**)ADDR_ZINPUT;
	return input && (KeyDown(input, DIK_LSHIFT) || KeyDown(input, DIK_RSHIFT));
}

static bool ConsoleOpen() {
	return ((GetInt_t)ADDR_CON_ISVISIBLE)((void*)ADDR_ZCON, 0) != 0;
}

static bool MenuActive() {
	return ((GetPtrStatic_t)ADDR_MENU_GETACTIVE)() != 0;
}

static bool DialogActive() {
	void* im = ((GetPtrStatic_t)ADDR_INFOMAN_GET)();
	return im && ((GetInt_t)ADDR_INFOMAN_FINISHED)(im, 0) == 0;
}

static int RealMode(BYTE* npc) {
	return (streaming || streamEnding) ? realMode : *(int*)(npc + OFS_NPC_FMODE);
}

static bool QuickCastBusy() {
	return investing || pendingCast || aiming || streaming || streamEnding;
}

// --- kusza (zDismember: ZDISM_XbowState / Select / Input / Busy) -------------------------------------------------------
typedef int (__cdecl* XbowState_t)(int* avail, int* selected, int* bolts, int* cooldownMs, char* label, int size);
typedef int (__cdecl* XbowSelect_t)(int on);
typedef int (__cdecl* XbowInput_t)(int phase, const float* point, void* hitVob);
typedef int (__cdecl* XbowBusy_t)();
static XbowState_t  xbStateFn;
static XbowSelect_t xbSelectFn;
static XbowInput_t  xbInputFn;
static XbowBusy_t   xbBusyFn;
static bool  xbPressed;        // F wcisniete przy kuszy i strzal przyjety przez zDismember
static DWORD xbPressTick;
static bool  xbHoldSent;       // faza 2 wyslana (celowanie)
static bool  xbAiming;         // celownik kuszy na ekranie
static bool  xbRmbPrev;
static DWORD xbAimTurnLast;
static float xbAimYaw0;         // kierunek bohatera na starcie celowania (log obrotu w celowaniu)

static float HeroYawDeg(BYTE* npc) {
	const float* h = (const float*)(npc + 0x3C);   // zCVob::trafoObjToWorld: przod [2] [10]
	return atan2f(h[2], h[10]) * 57.2958f;
}

// Eksport zDismember po nazwie modulu (Steam / Kroniki: zDismember.dll, NB: slot DIFF_01), a bez nich po wszystkich modulach
// procesu (DLL z VDF). Ponowna proba co 5 s (pluginy laduja sie w roznej kolejnosci).
static bool XbowFns() {
	static DWORD next;
	if (xbStateFn && xbSelectFn && xbInputFn && xbBusyFn)
		return true;
	if (isGothic1 || !xbowSlot)
		return false;
	DWORD now = GetTickCount();
	if ((int)(now - next) < 0)
		return false;
	next = now + 5000;
	HMODULE found = 0;
	static const char* names[] = { "zDismember.dll", "NB_UNION_PLUGIN_DIFF_01.DLL" };
	for (int i = 0; i < 2 && !found; i++) {
		HMODULE m = GetModuleHandleA(names[i]);
		if (m && GetProcAddress(m, "ZDISM_XbowState"))
			found = m;
	}
	if (!found) {
		HMODULE mods[512];
		DWORD need = 0;
		if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &need))
			for (DWORD i = 0; i < need / sizeof(HMODULE) && i < 512 && !found; i++)
				if (GetProcAddress(mods[i], "ZDISM_XbowState"))
					found = mods[i];
	}
	if (!found)
		return false;
	xbStateFn = (XbowState_t)GetProcAddress(found, "ZDISM_XbowState");
	xbSelectFn = (XbowSelect_t)GetProcAddress(found, "ZDISM_XbowSelect");
	xbInputFn = (XbowInput_t)GetProcAddress(found, "ZDISM_XbowInput");
	xbBusyFn = (XbowBusy_t)GetProcAddress(found, "ZDISM_XbowBusy");
	bool ok = xbStateFn && xbSelectFn && xbInputFn && xbBusyFn;
	Log("zDismember: eksporty kuszy %s - slot kuszy pod F", ok ? "znalezione" : "niepelne (stara wersja?) - bez");
	return ok;
}

// Slot kuszy: dostepny (zalozona kusza, paczka W3) i wybrany. label = "Kusza: <nazwa> (belty: N)".
static bool XbowState(bool* selected, char* label, int size) {
	if (selected)
		*selected = false;
	if (!XbowFns())
		return false;
	int avail = 0, sel = 0, bolts = 0, cd = 0;
	char name[64] = "";
	xbStateFn(&avail, &sel, &bolts, &cd, name, sizeof(name));
	if (selected)
		*selected = avail && sel;
	if (label && size > 0)
		qs::FormatXbowText(label, size, name, bolts);
	return avail != 0;
}

static bool XbowSelected() {
	bool sel = false;
	return XbowState(&sel, 0, 0) && sel;
}

static bool XbowBusy() {
	return XbowFns() && xbBusyFn() != 0;
}

static void XbowAimEnd() {
	if (xbAiming) {
		BYTE* npc = *(BYTE**)ADDR_PLAYER;
		float d = npc ? HeroYawDeg(npc) - xbAimYaw0 : 0.0f;
		while (d > 180.0f)
			d -= 360.0f;
		while (d < -180.0f)
			d += 360.0f;
		Log("   kusza: celownik - koniec, bohater obrocil sie w celowaniu o %d st.", (int)d);
		if (!aiming)
			AimHudDraw(false, false, 0.0f, false);
	}
	xbAiming = false;
}

// Nazwa runy: opis przedmiotu (runy G2: "Ognista strzala", znaki W3: "Znak Aard"), inaczej nazwa ("Runa").
static const char* SpellName(void* book, void* spell) {
	BYTE* item = spell ? (BYTE*)((GetSpellItem_t)ADDR_BOOK_SPELLITEM)(book, 0, spell) : 0;
	if (!item)
		return "";
	const char* s = ZStrText(item + OFS_ITEM_DESC);
	return *s ? s : ZStrText(item + OFS_ITEM_NAME);
}

static int SpellKeyNo(void* spell) {
	return spell ? *(int*)((BYTE*)spell + OFS_SPELL_KEYNO) : 0;
}

static void __fastcall HookViewDrawItems(void* self, void* edx);

// Widok na caly ekran bez tla (zCView::Blit bez backTex rysuje tylko tekst - BlitText), z czcionka gry i wlasna vtable.
static void* NewTextView() {
	void* v = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 0x200);   // sizeof(zCView) = 0x100
	if (!v)
		return 0;
	((ViewCtor_t)ADDR_VIEW_CTOR)(v, 0, 0, 0, 8192, 8192, 2 /* VIEW_ITEM */);
	((VoidByTwoInt_t)ADDR_VIEW_SETPOS)(v, 0, 0, 0);
	((VoidByTwoInt_t)ADDR_VIEW_SETSIZE)(v, 0, 8192, 8192);
	ZStr font(SEL_FONT);
	((VoidByPtr_t)ADDR_VIEW_SETFONT)(v, 0, font.b);
	if (!viewVt[1]) {
		memcpy(viewVt, (DWORD*)ADDR_VIEW_VTABLE - 1, sizeof(viewVt));
		viewVt[1 + 11] = (DWORD)&HookViewDrawItems;
	}
	*(DWORD**)v = viewVt + 1;
	return v;
}

static bool EnsureOnScreen(void* v) {
	void* screen = *(void**)ADDR_SCREEN;
	if (!v || !screen)
		return false;
	if (!*(int*)((BYTE*)v + OFS_VIEW_ONDESK))
		((InsertItem_t)ADDR_VIEW_INSERTITEM)(screen, 0, v, 0);
	return true;
}

static void ViewText(void* v, int x, int y, ZStr& text, float timeMs, DWORD color) {
	((CreateText_t)ADDR_VIEW_CREATETEXT)(v, 0, x, y, text.b, timeMs, &color, timeMs > 0.0f ? 1 : 0, 1);
}

// Komunikat zmiany wyboru: wlasny widok - nowy zastepuje poprzedni (szybkie krecenie kolkiem nie nawarstwia napisow).
static void SelMessage(const char* text) {
	if (!msgView)
		msgView = NewTextView();
	if (!EnsureOnScreen(msgView))
		return;
	ZStr z(text);
	int w = ((IntByPtr_t)ADDR_VIEW_FONTSIZE)(msgView, 0, z.b);
	int h = ((GetInt_t)ADDR_VIEW_FONTY)(msgView, 0);
	((Void_t)ADDR_VIEW_CLRPRINTWIN)(msgView, 0);
	ViewText(msgView, 4096 - w / 2, 4096 - 3 * h, z, SEL_MSG_MS, SEL_COLOR_MSG);   // nad srodkiem (celownik)
}

// Etykieta widoczna: bron biala albo piesci w rece (tryby 1..4), wybrany czar, gra bez menu, dialogu, ekwipunku i pauzy,
// z paskami stanu (showPlayerStatus).
static bool SelLabelWanted(BYTE* npc, void** book, void** spell) {
	BYTE* game = *(BYTE**)ADDR_OGAME;
	if (!showSelected || isGothic1 || !npc || !game || !*(int*)(game + OFS_OGAME_SHOWSTATUS)
		|| *(int*)(game + OFS_OGAME_SINGLESTEP))
		return false;
	int mode = RealMode(npc);
	if (mode < 1 || mode > 4 || MenuActive() || DialogActive() || InventoryOpen(npc))
		return false;
	if (XbowSelected()) {
		*book = 0;
		*spell = 0;
		return true;   // kusza wybrana pod F - etykieta kuszy (SelLabelRefresh)
	}
	*book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	*spell = *book ? ((GetPtr_t)ADDR_BOOK_SELSPELL)(*book, 0) : 0;
	return *spell != 0;
}

// Co klatke rysowania etykiety: widocznosc i tekst; tekst i pozycja zmieniane tylko przy zmianie (bez migania).
// Pozycja: nad paskiem many, wyrownana do jego prawej krawedzi (bez paska - prawy dolny rog).
static bool SelLabelRefresh() {
	BYTE* npc = *(BYTE**)ADDR_PLAYER;
	void *book = 0, *spell = 0;
	if (!SelLabelWanted(npc, &book, &spell))
		return false;
	char text[96];
	if (!spell)
		XbowState(0, text, sizeof(text));   // kusza wybrana
	else
		qs::FormatSignText(text, sizeof(text), SpellName(book, spell));
	void* screen = *(void**)ADDR_SCREEN;
	BYTE* game = *(BYTE**)ADDR_OGAME;
	BYTE* bar = *(BYTE**)(game + OFS_OGAME_MANABAR);
	int right = 8192 - ((IntByInt_t)ADDR_VIEW_ANX)(screen, 0, 16), top = 8192 - ((IntByInt_t)ADDR_VIEW_ANY)(screen, 0, 48);
	if (bar && *(int*)(bar + OFS_VIEW_VPOS + 8) > 0) {
		const int* p = (const int*)(bar + OFS_VIEW_VPOS);
		right = p[0] + p[2];
		top = p[1];
	}
	ZStr z(text);
	int w = ((IntByPtr_t)ADDR_VIEW_FONTSIZE)(selView, 0, z.b);
	int h = ((GetInt_t)ADDR_VIEW_FONTY)(selView, 0);
	int x = right - w, y = top - h - ((IntByInt_t)ADDR_VIEW_ANY)(screen, 0, 4);
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if (x != selX || y != selY || lstrcmpA(text, selText) != 0) {
		((Void_t)ADDR_VIEW_CLRPRINTWIN)(selView, 0);
		ViewText(selView, x, y, z, 0.0f, SEL_COLOR_LABEL);
		Log("etykieta: \"%s\" (x %d, y %d)", text, x, y);
		lstrcpynA(selText, text, sizeof(selText));
		selX = x;
		selY = y;
	}
	return true;
}

// DrawItems naszych widokow (wpis 11 kopii vtable): rodzic (screen) wola go co klatke dla kazdego wstawionego widoku.
static void __fastcall HookViewDrawItems(void* self, void* edx) {
	if (self == selView) {
		if (!SelLabelRefresh())
			return;
	}
	else if (MenuActive())
		return;
	((Void_t)ADDR_VIEW_DRAWITEMS)(self, edx);
}

// Wspolne dla Shift + cyfra i kolka: index - czar w ksiedze, source/slot - do logu.
static void ApplySelection(BYTE* npc, void* book, int index, const char* source) {
	int mode = RealMode(npc);
	int cur = *(int*)((BYTE*)book + OFS_BOOK_SPELLNR);
	if (mode == FMODE_MAGIC && index != cur) {
		// runa w rece: zamiana w rece jak klawisz gry - z jej bramkami (postoj / chod, magia, bez plywania i moba)
		BYTE* ani = (BYTE*)((GetPtr_t)ADDR_NPC_GETANICTRL)(npc, 0);
		bool gate = ((GetInt_t)(origSpellKeyGate ? origSpellKeyGate : 0x006805C0))(npc, 0) != 0;
		if (!gate || !((GetInt_t)ADDR_NPC_HASMAGIC)(npc, 0) || !ani || *(int*)(ani + OFS_ANICTRL_ACTION) == ANI_ACTION_SWIM
			|| ((GetPtr_t)ADDR_NPC_INTERACTMOB)(npc, 0)) {
			Log("wybor (%s): odmowa - runa w rece, gra teraz nie zmienia runy (bramka %d)", source, gate);
			SelMessage("Teraz nie mo\xBFna zmieni\xE6 runy");   // cp1250: z z kropka, c z kreska
			return;
		}
	}
	else if (mode != FMODE_MAGIC && *(int*)((BYTE*)book + OFS_BOOK_OPEN)) {
		Log("wybor (%s): odmowa - ksiega otwarta poza trybem magii (dobywanie / chowanie runy w toku)", source);
		return;
	}
	if (index != cur)
		((VoidByInt_t)ADDR_BOOK_SETFRONTSPELL)(book, 0, index);
	if (XbowSelected()) {
		xbSelectFn(0);
		Log("wybor (%s): kusza odznaczona - F rzuca Znak", source);
	}
	void* spell = ((GetPtr_t)ADDR_BOOK_SELSPELL)(book, 0);
	const char* name = SpellName(book, spell);
	char text[96];
	qs::FormatSignText(text, sizeof(text), name);
	SelMessage(text);
	Log("wybor (%s): slot %d, czar id %d, \"%s\", tryb %d%s%s", source, qs::KeyNoToSlot(SpellKeyNo(spell)),
		spell ? ((GetInt_t)ADDR_SPELL_GETSPELLID)(spell, 0) : -1, name, mode, index == cur ? " - juz wybrany" : "",
		mode == FMODE_MAGIC && index != cur ? " - zamiana runy w rece" : "");
}

// Wspolne warunki wyboru: ekwipunek / konsola - nic; szybki rzut w toku (ladowanie, celowanie, gest, strumien) - odmowa.
static void* SelectBook(BYTE* npc, const char* source) {
	if (InventoryOpen(npc) || ConsoleOpen())
		return 0;
	if (QuickCastBusy()) {
		Log("wybor (%s): odmowa - szybki rzut w toku (ladowanie %d, gest %d, celowanie %d, strumien %d)", source, investing,
			pendingCast, aiming, streaming || streamEnding);
		SelMessage("Zmiana znaku po zako\xF1" "czeniu rzutu");   // cp1250: n z kreska
		return 0;
	}
	return ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
}

static void SelectBySlot(BYTE* npc, int keyNo) {
	char source[16];
	int slot = qs::KeyNoToSlot(keyNo);
	wsprintfA(source, "Shift+%d", slot % 10);
	void* book = SelectBook(npc, source);
	if (!book)
		return;
	int n = ((IntByInt_t)ADDR_BOOK_NOOFSPELLBYKEY)(book, 0, keyNo);
	if (!n) {
		char text[48];
		wsprintfA(text, "Brak runy w slocie %d", slot);
		Log("wybor (%s): slot %d pusty", source, slot);
		SelMessage(text);
		return;
	}
	ApplySelection(npc, book, n - 1, source);
}

// Kusza wybrana pod F (z kolka albo ZQS_Select): komunikat jak przy Znaku.
static void ApplyXbowSelection(const char* source) {
	char text[96];
	XbowState(0, text, sizeof(text));
	xbSelectFn(1);
	SelMessage(text);
	Log("wybor (%s): %s - F strzela z kuszy", source, text);
}

static void SelectByWheel(BYTE* npc, int dir) {
	const char* source = dir > 0 ? "kolko w dol" : "kolko w gore";
	if (xbPressed || XbowBusy()) {
		Log("wybor (%s): odmowa - strzal z kuszy w toku", source);
		return;
	}
	void* book = SelectBook(npc, source);
	if (!book && (InventoryOpen(npc) || ConsoleOpen() || QuickCastBusy()))
		return;   // SelectBook juz odmowil (z komunikatem)
	bool xbSel = false;
	bool xbAvail = XbowState(&xbSel, 0, 0);
	if (!book && !xbAvail)
		return;
	void** spells = book ? *(void***)book : 0;
	int n = book ? *(int*)((BYTE*)book + OFS_BOOK_NSPELLS) : 0;
	if (n > 64)
		n = 64;
	int keyNo[64];
	bool isSign[64];
	for (int i = 0; i < n; i++) {
		keyNo[i] = SpellKeyNo(spells[i]);
		isSign[i] = spells[i] && qs::InList(((GetInt_t)ADDR_SPELL_GETSPELLID)(spells[i], 0), signWheelIds, signWheelIdCount);
	}
	int cur = xbSel ? n : (book ? *(int*)((BYTE*)book + OFS_BOOK_SPELLNR) : -1);
	int index = qs::WheelPickXbow(keyNo, isSign, n, cur, dir, xbAvail);
	if (xbAvail && index == n) {
		if (!xbSel)
			ApplyXbowSelection(source);
		return;
	}
	if (index < 0 || !book) {
		Log("wybor (%s): brak znakow W3 w ksiedze (czarow %d, SignWheelIds %s)", source, n, signWheelIdsText);
		SelMessage("Brak znak\xF3w w ksi\xEA" "dze");   // cp1250: o z kreska, e z ogonkiem
		return;
	}
	ApplySelection(npc, book, index, source);
}

// Co klatke z HookActionMove (bohater steruje postacia): Shift + 4..0, Shift + kolko, etykieta na ekranie.
static void SelectUpdate(BYTE* npc) {
	if (isGothic1)
		return;
	DWORD now = GetTickCount();
	bool resync = now - selPollLast > 250;   // po dialogu, menu, ladowaniu - trzymany klawisz to nie nowe wcisniecie
	selPollLast = now;
	void* input = *(void**)ADDR_ZINPUT;
	if (!input)
		return;
	bool shift = KeyDown(input, DIK_LSHIFT) || KeyDown(input, DIK_RSHIFT);
	if (selectWithShift) {
		bool keys[qs::KEYNO_MAX + 1] = {};
		for (int k = 1; k <= qs::KEYNO_MAX; k++)
			keys[k] = KeyDown(input, qs::DIK_4 + k - 1);
		int keyNo = qs::DigitEdge(digitPrev, keys, shift && !resync);
		if (keyNo)
			SelectBySlot(npc, keyNo);
	}
	if (signWheel && shift && !resync) {
		float x = 0.0f, y = 0.0f, z = 0.0f;
		((GetMousePos_t)(*(DWORD**)input)[VT_INPUT_GETMOUSEPOS / 4])(input, 0, &x, &y, &z);
		int dir = qs::WheelDir(z);
		if (dir)
			SelectByWheel(npc, dir);
	}
	if (showSelected) {
		if (!selView)
			selView = NewTextView();
		EnsureOnScreen(selView);   // wstawiony raz; po ewentualnym zdjeciu przez gre wraca
	}
}

// Bramka klawiszy run w oCGame::HandleEvent (patrz wyzej): Shift trzymany = 0 (gra nie obsluguje klawisza 4..0).
static int __cdecl SpellKeyGate(void* npc, int dik) {
	if (selectWithShift && qs::DikToKeyNo(dik) && ShiftDown()) {
		DWORD now = GetTickCount();
		if (now - gateBlockLogTick > 500)
			Log("klawisz %d z Shift - obsluga klawisza runy przez gre zablokowana (bez dobycia)", qs::KeyNoToSlot(qs::DikToKeyNo(dik)) % 10);
		gateBlockLogTick = now;
		return 0;
	}
	return ((GetInt_t)origSpellKeyGate)(npc, 0);
}

// Wejscie z call w HandleEvent: ecx = gracz, esi = klawisz (DIK, argument HandleEvent). Rejestry ebx/esi/edi/ebp
// zachowuje funkcja C.
__declspec(naked) static void SpellKeyGateWrap() {
	__asm {
		push esi
		push ecx
		call SpellKeyGate
		add  esp, 8
		ret
	}
}

// ---------------------------------------------------------------------------

// W trybie magii (runa w rece) w trakcie ladowania z klawisza pomijamy zwykla obsluge klawiszy magii -
// bez wcisnietej akcji MagicCheckSpellStates zatrzymalby nasz czar. Ruch dziala dalej (return 0).
static int __fastcall HookMagicMode(void* self, void* edx) {
	BYTE* npc = *(BYTE**)((BYTE*)self + OFS_AI_NPC);
	if (investing && npc && npc == *(BYTE**)ADDR_PLAYER)
		return 0;
	return origMagicMode(self, edx);
}

typedef void (__fastcall* OnDamageHit_t)(void* self, void* edx, BYTE* desc);
static OnDamageHit_t origOnDamageHit;

static void __fastcall HookOnDamageHit(void* self, void* edx, BYTE* desc) {
	BYTE* attacker = desc ? *(BYTE**)(desc + OFS_DESC_NPCATTACKER) : nullptr;
	int* fmode = nullptr;
	int saved = 0;
	if (spellDamageFix && attacker && attacker == *(BYTE**)ADDR_PLAYER
		&& *(void**)(desc + OFS_DESC_FXHIT) && !*(void**)(desc + OFS_DESC_VISUALFX)) {
		fmode = (int*)(attacker + OFS_NPC_FMODE);
		saved = *fmode;
		if (saved <= FMODE_MELEE_MAX) {
			*fmode = FMODE_MAGIC;
			Log("trafienie czarem %d przy trybie %d - obrazenia liczone jak w trybie magii",
				*(int*)(desc + OFS_DESC_SPELLID), saved);
		} else
			fmode = nullptr;
	}
	// Znak W3 odlozony do konca pchniecia (SignCastSync): trafienie bohatera przez wroga (spadek zycia; bez taktow palenia
	// EV_DAMAGE_PER_FRAME - flaga OverlayActivate 0x1000, bez upadku - brak napastnika) przerywa gest - obsluga w HookActionMove.
	// Trafienia polkniete wczesniej (Quen, unik zDismember - hak oCNpc::OnDamage) tu nie dochodza.
	BYTE* hero = *(BYTE**)ADDR_PLAYER;
	bool watchHit = pendingCast && pendingSync && !isGothic1 && self == hero && attacker && attacker != hero && desc
		&& !(*(DWORD*)desc & 0x1000);
	int hp0 = watchHit ? *(int*)((BYTE*)self + OFS_NPC_HP) : 0;
	origOnDamageHit(self, edx, desc);
	if (fmode && *fmode == FMODE_MAGIC)
		*fmode = saved;
	if (watchHit && *(int*)((BYTE*)self + OFS_NPC_HP) < hp0 && !pendingCancel)
		pendingCancel = "trafienie";
}

static int __fastcall HookActionMove(void* self, void* edx, int arg) {
	BYTE* npc = *(BYTE**)((BYTE*)self + OFS_AI_NPC);
	bool hero = npc && npc == *(BYTE**)ADDR_PLAYER;
	if (hero) {
		heroAi = self;
		SelectUpdate(npc);   // Shift + 4..0 / Shift + kolko - wybor runy pod F bez dobywania
		bool down = CastKeyDown();
		bool hit = down && !keyWasDown;
		keyWasDown = down;
		int mode = (streaming || streamEnding) ? realMode : *(int*)(npc + OFS_NPC_FMODE);

		// nakladka gestow zawczasu, na postoju - przy rzucie w biegu jej zalozenie zatrzymaloby bohatera
		if (signGestures && useAnim && !isGothic1 && !investing && !pendingCast && !streaming && !streamEnding
			&& !fallbackCleanupAt)
			EnsureSignOverlay(npc, self);

		if (streamEnding && (nbWrapCalls - endWrapCalls >= 2 || GetTickCount() - endStart >= 400))
			FinishStreamEnding(npc);

		if (fallbackCleanupAt && (int)(GetTickCount() - fallbackCleanupAt) >= 0) {
			fallbackCleanupAt = 0;
			CleanupFallbackAni(npc, self);
		}

		if (pendingCast && pendingSync) {
			// Znak W3 odlozony do konca pchniecia: trafienie (HookOnDamageHit) albo otwarty ekwipunek = bez skutku i kosztu
			int v = qs::SyncVerdict(true, pendingCancel != 0, InventoryOpen(npc), -1, signSyncLateMs);
			if (v != qs::QSYNC_OK)
				CancelPendingCast(npc, v == qs::QSYNC_HIT ? pendingCancel : "ekwipunek");
		}
		TurnStep(npc);   // przed rzutem odlozonym - pchniecie dloni juz w strone celu
		WatchGesture(npc);
		if (pendingCast && (int)(GetTickCount() - pendingCastAt) >= 0)
			FirePendingCast(npc);
		if (gestureOverlay[0] && (int)(GetTickCount() - gestureRemoveAt) >= 0)
			EndGesture(npc);

		if (!investing && stanceFixUntil) {
			if (GetTickCount() < stanceFixUntil)
				FixStanceAfterStream(npc);
			else
				stanceFixUntil = 0;
		}

		if (aiming && aimReticleOn)
			AimTurnStep(npc);   // przed rzutem - pchniecie dloni juz w strone kamery

		if (xbPressed) {
			// F przy kuszy: prog przytrzymania, puszczenie, PPM; zDismember skonczyl / przerwal strzal - koniec
			DWORD now = GetTickCount();
			void* input = *(void**)ADDR_ZINPUT;
			bool rmb = input && ((GetInt_t)(*(DWORD**)input)[0x5C / 4])(input, 0) != 0;   // GetMouseButtonPressedRight
			bool rmbHit = rmb && !xbRmbPrev;
			xbRmbPrev = rmb;
			if (!XbowBusy()) {
				Log("   kusza: strzal zakonczony / przerwany przez zDismember (F %s)", down ? "trzymane" : "puszczone");
				xbPressed = false;
				XbowAimEnd();
			}
			else if (xbAiming && rmbHit) {
				xbInputFn(4, 0, 0);
				Log("   kusza: PPM - celowanie anulowane (bez strzalu i beltu)");
				xbPressed = false;
				XbowAimEnd();
			}
			else if (down && !xbHoldSent && qs::XbowHoldReached(xbPressTick, now, xbowHoldMs)) {
				xbHoldSent = true;
				if (xbInputFn(2, 0, 0) == 0) {
					xbAiming = true;
					xbAimTurnLast = aimTurnLast = now;
					xbAimYaw0 = HeroYawDeg(npc);
					Log("   kusza: F trzymane %u ms - celownik %s %d px%s", now - xbPressTick, aimReticleTex, aimReticlePx,
						xbowAimCamera ? ", kamera dystansowa (bohatera obraca mysz, bez obrotu za kamera)" : ", obrot za kamera");
				}
			}
			else if (!down) {
				if (xbHoldSent && xbAiming) {
					float point[3];
					void* hitVob = 0;
					AimRay(npc, point, &hitVob);
					xbInputFn(3, point, IsNpc(hitVob) ? hitVob : 0);
					Log("   kusza: puszczone po %u ms - strzal w %s (%d, %d, %d)", now - xbPressTick, IsNpc(hitVob) ? "NPC pod celownikiem" :
						"punkt celownika", (int)point[0], (int)point[1], (int)point[2]);
				}
				else {
					xbInputFn(3, 0, 0);
					Log("   kusza: stukniecie (%u ms) - cel automatyczny", now - xbPressTick);
				}
				xbPressed = false;
				XbowAimEnd();
			}
			if (xbAiming) {
				// Obrot za kamera tylko przy zwyklej kamerze: kamera dystansowa (CamModRanged) stoi zza prawego ramienia, jej os
				// nie jest rownolegla do bohatera - obrot bohatera do niej obracal kamere dalej i bohater krecil sie w prawo bez
				// konca (zgloszenie 23.10). Z kamera dystansowa bohatera obraca mysz jak zwykle.
				if (!xbowAimCamera) {
					aimTurnLast = xbAimTurnLast;
					AimTurnStep(npc);   // bohater za kamera
					xbAimTurnLast = aimTurnLast;
				}
				AimHudDraw(true, false, 0.0f, false);
			}
		}
		else if (hit && xbowSlot && !investing && !pendingCast && !streamEnding && !InventoryOpen(npc) && XbowSelected()) {
			int r = xbInputFn(1, 0, 0);
			if (r == 0) {
				xbPressed = true;
				xbHoldSent = false;
				xbPressTick = GetTickCount();
				xbRmbPrev = true;   // PPM wcisniete juz przy F - nie anuluje
				Log("F: kusza - strzal przyjety (tryb %d)", mode);
			}
			else
				Log("F: kusza - odmowa zDismember %d (komunikat w grze, powod w zDismember.log)", r);
		}
		else if (investing) {
			if (InventoryOpen(npc)) {
				Log("   ladowanie przerwane (tryb %d)", mode);
				void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
				if (book)
					StopNow(npc, book);
				EndInvest(npc);
			}
			else
				ContinueQuickCast(npc, down);
		}
		else if (hit && !streamEnding && !pendingCast && !InventoryOpen(npc)) {
			DWORD now = GetTickCount();
			if ((int)(cooldownUntil - now) <= 0)
				StartQuickCast(npc, aimOnPress && !isGothic1);
			else if (cooldownMessage) {
				int left = (int)(cooldownUntil - now);
				char text[64];
				// cp1250: \xEA = e z ogonkiem
				wsprintfA(text, "Znak si\xEA odnawia: %d,%d s", left / 1000, (left % 1000) / 100);
				ShowMessage(text);
			}
		}
		if (aiming)
			AimHudUpdate(npc);   // po Invest tej klatki - ladowanie Znaku W3 juz policzone przez zDismember
	}

	// ruch i walka w tej klatce jak zwykle - z prawdziwym trybem broni, takze w trakcie strumienia
	if (hero && (streaming || streamEnding)) {
		int& fmode = *(int*)(npc + OFS_NPC_FMODE);
		fmode = realMode;
		int result = origActionMove(self, edx, arg);
		if (streaming || streamEnding) {
			if (fmode != realMode) {
				// cos zmienilo bron w trakcie strumienia - przyjmujemy nowy tryb
				Log("   tryb zmieniony w trakcie strumienia: %d -> %d", realMode, fmode);
				realMode = fmode;
			}
			fmode = FMODE_MAGIC;
		}
		return result;
	}
	// celowanie: LPM (klawisz akcji) nie atakuje i nie rzuca czaru z trybu magii; strzal z kuszy - jak w W3 bez ciosu
	if (hero && (aiming || xbPressed || XbowBusy()))
		arg = 0;
	return origActionMove(self, edx, arg);
}

// ---------------------------------------------------------------------------

// "174,180,186" -> out[]; zwraca liczbe wpisow (max).
static int ParseIds(const char* s, int* out, int max) {
	int n = 0;
	while (*s && n < max) {
		while (*s && (*s < '0' || *s > '9'))
			s++;
		if (!*s)
			break;
		int v = 0;
		while (*s >= '0' && *s <= '9')
			v = v * 10 + (*s++ - '0');
		out[n++] = v;
	}
	return n;
}

static void LoadSettings() {
	GetModuleFileNameA(0, iniPath, MAX_PATH);
	char* slash = strrchr(iniPath, '\\');
	strcpy_s(slash ? slash + 1 : iniPath, MAX_PATH - (slash ? slash + 1 - iniPath : 0), "zQuickSign.ini");
	strcpy_s(logPath, iniPath);
	strcpy_s(strrchr(logPath, '.'), 5, ".log");

	castKey    = GetPrivateProfileIntA("ZQUICKSIGN", "KeyDik", 0x21, iniPath);
	cooldownMs = GetPrivateProfileIntA("ZQUICKSIGN", "CooldownMs", 500, iniPath);
	cooldownPerCircleMs = GetPrivateProfileIntA("ZQUICKSIGN", "CooldownPerCircleMs", 0, iniPath);
	cooldownPerManaMs = GetPrivateProfileIntA("ZQUICKSIGN", "CooldownPerManaMs", 0, iniPath);
	cooldownMessage = GetPrivateProfileIntA("ZQUICKSIGN", "CooldownMessage", 1, iniPath) != 0;
	useAnim    = GetPrivateProfileIntA("ZQUICKSIGN", "UseAnim", 1, iniPath) != 0;
	GetPrivateProfileStringA("ZQUICKSIGN", "AniName", "T_QSCAST", aniName, sizeof(aniName), iniPath);
	GetPrivateProfileStringA("ZQUICKSIGN", "FallbackAniName", "T_MAGRUN_2_FBTSHOOT", fallbackAniName,
		sizeof(fallbackAniName), iniPath);
	fallbackAniMs = GetPrivateProfileIntA("ZQUICKSIGN", "FallbackAniMs", 320, iniPath);
	GetPrivateProfileStringA("ZQUICKSIGN", "StreamSpellIds", streamIdsText, streamIdsText,
		sizeof(streamIdsText), iniPath);
	manaPercent = GetPrivateProfileIntA("ZQUICKSIGN", "ManaCostPercent", 100, iniPath);
	if (manaPercent < 0)
		manaPercent = 0;
	spellDamageFix = GetPrivateProfileIntA("ZQUICKSIGN", "SpellDamageFix", 1, iniPath) != 0;
	magicFocus = GetPrivateProfileIntA("ZQUICKSIGN", "MagicFocus", 1, iniPath) != 0;
	aimForward = GetPrivateProfileIntA("ZQUICKSIGN", "AimForward", 1, iniPath) != 0;
	turnToTarget = GetPrivateProfileIntA("ZQUICKSIGN", "TurnToTarget", 1, iniPath) != 0;
	signLeftHand = GetPrivateProfileIntA("ZQUICKSIGN", "SignLeftHand", 1, iniPath) != 0;
	// AimOnPress zastepuje AimWithMouse (runda 5: celowanie F + LPM) - stara wartosc jako domyslna, stary klucz usuwany
	int aimOld = GetPrivateProfileIntA("ZQUICKSIGN", "AimWithMouse", 1, iniPath);
	aimOnPress = GetPrivateProfileIntA("ZQUICKSIGN", "AimOnPress", aimOld, iniPath) != 0;
	GetPrivateProfileStringA("ZQUICKSIGN", "AimReticle", aimReticleTex, aimReticleTex, sizeof(aimReticleTex), iniPath);
	{
		// rozmiar celownika jak w Gothic Free Aim: Gothic.ini [GFA] reticleSizePx (Gothic.ini obok zQuickSign.ini)
		char gothicIni[MAX_PATH];
		lstrcpyA(gothicIni, iniPath);
		char* slash = strrchr(gothicIni, '\\');
		lstrcpyA(slash ? slash + 1 : gothicIni, "Gothic.ini");
		aimReticlePx = GetPrivateProfileIntA("GFA", "reticleSizePx", 64, gothicIni);
		if (aimReticlePx < 16 || aimReticlePx > 512)
			aimReticlePx = 64;
		// zExtendedBattleAnimations: Shift + kolko = zmiana stylu walki (Gothic.ini [ZEXTENDEDBATTLEANIMATIONS]
		// EnableMouseWheel, menu "Style switch: Shift+wheel") - ta sama kombinacja co SignWheel, tylko do logu
		char zebaPath[MAX_PATH];
		lstrcpyA(zebaPath, gothicIni);
		char* sysDir = strrchr(zebaPath, '\\');
		if (sysDir)
			*sysDir = 0;
		sysDir = strrchr(zebaPath, '\\');
		lstrcpyA(sysDir ? sysDir + 1 : zebaPath, "Data\\zExtendedBattleAnimations.vdf");
		if (GetFileAttributesA(zebaPath) != INVALID_FILE_ATTRIBUTES)
			zebaWheel = GetPrivateProfileIntA("ZEXTENDEDBATTLEANIMATIONS", "EnableMouseWheel", 1, gothicIni) != 0 ? 1 : 0;
	}
	selectWithShift = GetPrivateProfileIntA("ZQUICKSIGN", "SelectWithShift", 1, iniPath) != 0;
	signWheel = GetPrivateProfileIntA("ZQUICKSIGN", "SignWheel", 1, iniPath) != 0;
	showSelected = GetPrivateProfileIntA("ZQUICKSIGN", "ShowSelected", 1, iniPath) != 0;
	GetPrivateProfileStringA("ZQUICKSIGN", "SignWheelIds", signWheelIdsText, signWheelIdsText, sizeof(signWheelIdsText), iniPath);
	signWheelIdCount = ParseIds(signWheelIdsText, signWheelIds, 32);
	xbowSlot = GetPrivateProfileIntA("ZQUICKSIGN", "XbowSlot", 1, iniPath) != 0;
	xbowHoldMs = GetPrivateProfileIntA("ZQUICKSIGN", "XbowHoldMs", 300, iniPath);
	if (xbowHoldMs < 50)
		xbowHoldMs = 50;
	xbowAimCamera = GetPrivateProfileIntA("ZQUICKSIGN", "XbowAimCamera", 1, iniPath) != 0;
	debugLog   = GetPrivateProfileIntA("ZQUICKSIGN", "Debug", 0, iniPath) != 0;

	streamIdCount = ParseIds(streamIdsText, streamIds, 32);

	// gesty domyslnie tylko tam, gdzie jest prywatna paczka W3 (Data\zDismember_W3.vdf obok system\)
	char packPath[MAX_PATH];
	lstrcpyA(packPath, iniPath);
	char* sys = strrchr(packPath, '\\');
	if (sys)
		*sys = 0;
	sys = strrchr(packPath, '\\');
	lstrcpyA(sys ? sys + 1 : packPath, "Data\\zDismember_W3.vdf");
	bool hasPack = GetFileAttributesA(packPath) != INVALID_FILE_ATTRIBUTES;
	signGestures = GetPrivateProfileIntA("ZQUICKSIGN", "SignGestures", hasPack ? 1 : 0, iniPath) != 0;
	GetPrivateProfileStringA("ZQUICKSIGN", "SignCastMs", signCastMsText, signCastMsText, sizeof(signCastMsText), iniPath);
	GetPrivateProfileStringA("ZQUICKSIGN", "SignGestureMs", signGestureMsText, signGestureMsText, sizeof(signGestureMsText), iniPath);
	int castMs[SIGN_COUNT] = { 320, 320, 320, 320, 200 }, gestMs[SIGN_COUNT] = { 680, 680, 680, 680, 440 };
	ParseIds(signCastMsText, castMs, SIGN_COUNT);
	ParseIds(signGestureMsText, gestMs, SIGN_COUNT);
	// tryb alternatywny "w ziemie" (make_signs.py ALT_BEFORE / ALT_AFTER): 0 = Znak bez gestu alternatywnego
	GetPrivateProfileStringA("ZQUICKSIGN", "SignAltCastMs", signAltCastMsText, signAltCastMsText, sizeof(signAltCastMsText), iniPath);
	GetPrivateProfileStringA("ZQUICKSIGN", "SignAltGestureMs", signAltGestureMsText, signAltGestureMsText,
		sizeof(signAltGestureMsText), iniPath);
	int altCastMs[SIGN_COUNT] = { 440, 0, 0, 440, 0 }, altGestMs[SIGN_COUNT] = { 1120, 0, 0, 1120, 0 };
	ParseIds(signAltCastMsText, altCastMs, SIGN_COUNT);
	ParseIds(signAltGestureMsText, altGestMs, SIGN_COUNT);
	// tryb zwykly Znakow W3: skutek z koncem pchniecia dloni (make_signs.py push_end), 0 = przy puszczeniu F jak dawniej
	signCastSync = GetPrivateProfileIntA("ZQUICKSIGN", "SignCastSync", 1, iniPath) != 0;
	GetPrivateProfileStringA("ZQUICKSIGN", "SignCastSyncMs", signCastSyncMsText, signCastSyncMsText, sizeof(signCastSyncMsText),
		iniPath);
	int syncMs[SIGN_COUNT] = { 360, 400, 360, 360, 320 };
	ParseIds(signCastSyncMsText, syncMs, SIGN_COUNT);
	signSyncLateMs = GetPrivateProfileIntA("ZQUICKSIGN", "SignSyncLateMs", 300, iniPath);
	if (signSyncLateMs < 50)
		signSyncLateMs = 50;
	for (int s = 0; s < SIGN_COUNT; s++) {
		signCastMs[s] = castMs[s];
		signGestureMs[s] = gestMs[s];
		signAltCastMs[s] = altCastMs[s];
		signAltGestureMs[s] = altGestMs[s];
		signCastSyncMs[s] = syncMs[s];
		if (!SIGN_INI_KEYS[s])
			continue;
		GetPrivateProfileStringA("ZQUICKSIGN", SIGN_INI_KEYS[s], signIdsText[s], signIdsText[s], sizeof(signIdsText[s]), iniPath);
		signIdCount[s] = ParseIds(signIdsText[s], signIds[s], 64);
	}

	// zapis wartosci, zeby plik pokazywal, co mozna zmienic
	char buf[16];
	wsprintfA(buf, "%d", castKey);
	WritePrivateProfileStringA("ZQUICKSIGN", "KeyDik", buf, iniPath);
	wsprintfA(buf, "%d", cooldownMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "CooldownMs", buf, iniPath);
	wsprintfA(buf, "%d", cooldownPerCircleMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "CooldownPerCircleMs", buf, iniPath);
	wsprintfA(buf, "%d", cooldownPerManaMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "CooldownPerManaMs", buf, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "CooldownMessage", cooldownMessage ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "UseAnim", useAnim ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "AniName", aniName, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "FallbackAniName", fallbackAniName, iniPath);
	wsprintfA(buf, "%d", fallbackAniMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "FallbackAniMs", buf, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "StreamSpellIds", streamIdsText, iniPath);
	wsprintfA(buf, "%d", manaPercent);
	WritePrivateProfileStringA("ZQUICKSIGN", "ManaCostPercent", buf, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SpellDamageFix", spellDamageFix ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "MagicFocus", magicFocus ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "AimForward", aimForward ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "TurnToTarget", turnToTarget ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignLeftHand", signLeftHand ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "AimOnPress", aimOnPress ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "AimWithMouse", 0, iniPath);   // usuniecie starego klucza
	WritePrivateProfileStringA("ZQUICKSIGN", "AimReticle", aimReticleTex, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SelectWithShift", selectWithShift ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignWheel", signWheel ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignWheelIds", signWheelIdsText, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "ShowSelected", showSelected ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignGestures", signGestures ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignCastMs", signCastMsText, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignGestureMs", signGestureMsText, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignAltCastMs", signAltCastMsText, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignAltGestureMs", signAltGestureMsText, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignCastSync", signCastSync ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignCastSyncMs", signCastSyncMsText, iniPath);
	wsprintfA(buf, "%d", signSyncLateMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "SignSyncLateMs", buf, iniPath);
	for (int s = 0; s < SIGN_COUNT; s++)
		if (SIGN_INI_KEYS[s])
			WritePrivateProfileStringA("ZQUICKSIGN", SIGN_INI_KEYS[s], signIdsText[s], iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "XbowSlot", xbowSlot ? "1" : "0", iniPath);
	wsprintfA(buf, "%d", xbowHoldMs);
	WritePrivateProfileStringA("ZQUICKSIGN", "XbowHoldMs", buf, iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "XbowAimCamera", xbowAimCamera ? "1" : "0", iniPath);
	WritePrivateProfileStringA("ZQUICKSIGN", "Debug", debugLog ? "1" : "0", iniPath);
}

// --- Gothic 1 1.08k (Othello: GothicMod.exe) ---
// Adresy z naglowkow ZenGin Gothic_I_Classic, miejsca wywolan z deasemblacji GothicMod.exe.
// Roznice wzgledem G2A: Spell_Setup(oCNpc*, zCVob*) bez trzeciego argumentu, inne przesuniecia
// fmode/inventory2/mag_circle/homeWorld/pVisualFX, IsOpen w vtable pod 0x1C. OnDamage_Hit G1 przy
// trybie broni 3-6 wybiera talent i rzuca na krytyk (0x73244C) - tryb 7 na czas trafienia czaru
// daje to samo co zwykly rzut.
static void InitGothic1() {
	isGothic1 = true;
	ADDR_PLAYER           = 0x008DBBB0;
	ADDR_OGAME            = 0x008DA6BC;
	ADDR_ZINPUT           = 0x0086CCA0;
	ADDR_CALL_ONDAMAGEHIT = 0x0073138F;
	ADDR_CALL_ACTIONMOVE  = 0x00614BF1;
	ADDR_CALL_MAGICMODE   = 0x00613F76;
	ADDR_NPC_GETSPELLBOOK = 0x0069B3C0;
	ADDR_NPC_GETFOCUSVOB  = 0x0068FFC0;
	ADDR_NPC_GETATTRIBUTE = 0x0068D7B0;
	ADDR_NPC_GETMODEL     = 0x00695300;
	ADDR_BOOK_SELSPELL    = 0x00470AC0;
	ADDR_BOOK_SPELLITEM   = 0x00472E40;
	ADDR_BOOK_SPELLSETUP  = 0x0046F800; // void oCMag_Book::Spell_Setup(oCNpc*, zCVob*)
	ADDR_BOOK_SPELLINVEST = 0x0046F990;
	ADDR_BOOK_SPELLCAST   = 0x0046FC00;
	ADDR_BOOK_STOPSPELL   = 0x00470C50;
	ADDR_SPELL_GETSTATUS  = 0x0047E390;
	ADDR_SPELL_RELEASE    = 0x0047E3B0;
	ADDR_SPELL_GETSPELLID = 0x0047E180;
	ADDR_MODEL_ANIID      = 0x0047EC50;
	ADDR_MODEL_STARTANI   = 0x005612A0;
	ADDR_MODEL_ACTIVEANI  = 0x00560D90;
	ADDR_MODEL_STOPANI    = 0x00560E10;
	ADDR_MODEL_STOPACTIVE = 0x00560EE0; // gesty w G1 wylaczone - tylko dla porzadku
	ADDR_MODEL_ANIFROMID  = 0x0046D1E0;
	ADDR_AI_ISSTANDING    = 0x006255E0;
	ADDR_NPC_CHANGEATTR   = 0x0068D7C0;
	ADDR_ZSTRING_CTOR     = 0x004013A0;
	ADDR_ZSTRING_DTOR     = 0x00401260;
	ADDR_PRINTTIMEDCXY    = 0x006FE440;
	OFS_NPC_FMODE         = 0x208;
	OFS_NPC_INVENTORY     = 0x550;
	VT_CONTAINER_ISOPEN   = 0x1C;
	OFS_ITEM_MAGCIRCLE    = 0x23C;
	OFS_VOB_HOMEWRLD      = 0xA8;
	OFS_DESC_VISUALFX     = 0xC8;
}

// Bajty wokol wywolan, w ktore sie wpinamy - chronia przed patchowaniem innej wersji exe. Samo
// wywolanie moze juz prowadzic do hooka innego pluginu, wiec cel czytamy z rel32 i wolamy dalej
// to, co tam bylo.
static bool IsSupportedExe() {
	static const BYTE ctx[] = { 0x50, 0x8B, 0xCE, 0xE8 };                 // push eax / mov ecx,esi / call
	static const BYTE magicCtx[] = { 0x7F, 0x0F, 0x8B, 0x44, 0x24, 0x0C }; // G2A 0x69A0B9
	static const BYTE magicCtxG1[] = { 0x8B, 0xCD, 0xE8 };                 // G1 0x613F74: mov ecx,ebp / call
	if (memcmp((void*)0x0069AE4E, ctx, sizeof(ctx)) == 0
		&& memcmp((void*)0x0069A0B9, magicCtx, sizeof(magicCtx)) == 0
		&& *(BYTE*)ADDR_CALL_MAGICMODE == 0xE8)
		return true;                                                       // Gothic II NotR 2.6
	if (memcmp((void*)0x00614BEE, ctx, sizeof(ctx)) == 0
		&& memcmp((void*)0x00613F74, magicCtxG1, sizeof(magicCtxG1)) == 0) {
		InitGothic1();                                                     // Gothic 1 1.08k
		return true;
	}
	return false;
}

static void PatchCall(DWORD site, void* hook, void** orig) {
	BYTE* p = (BYTE*)site;
	DWORD oldProtect;
	if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
		return;
	*orig = (void*)(site + 5 + *(int*)(p + 1));
	*(int*)(p + 1) = (int)((DWORD)hook - (site + 5));
	VirtualProtect(p, 5, oldProtect, &oldProtect);
	FlushInstructionCache(GetCurrentProcess(), p, 5);
}

// Kamera dystansowa w celowaniu z kuszy (XbowAimCamera): oCAIHuman::DoAI co klatke wola ChangeCamModeBySituation
// (wywolanie 0x69BC02), ktora dla fmode 6 wybiera CamModRanged (ujecie zza ramienia), dla 1..4 CamModMelee. Na czas tego
// jednego wywolania bohatera z celownikiem kuszy fmode udaje 6 - kamera przechodzi plynnie i wraca po celowaniu.
static DWORD ADDR_CALL_CAMMODE = 0x0069BC02;   // oCAIHuman::DoAI: call oCAIHuman::ChangeCamModeBySituation (0x69CD60)
static Void_t origCamMode;

static void __fastcall HookCamMode(void* self, void* edx) {
	BYTE* npc = self ? *(BYTE**)((BYTE*)self + OFS_AI_NPC) : 0;
	if (xbAiming && xbowAimCamera && npc && npc == *(BYTE**)ADDR_PLAYER) {
		int& fmode = *(int*)(npc + OFS_NPC_FMODE);
		int saved = fmode;
		fmode = 6;
		origCamMode(self, edx);
		fmode = saved;
		return;
	}
	origCamMode(self, edx);
}

// Dla zDismember i menu kolowego (23.10): 1 = szybki rzut w toku (ladowanie, celowanie, gest do pchniecia, strumien albo
// gest Znaku jeszcze gra) - zDismember nie zacznie wtedy strzalu z kuszy.
extern "C" __declspec(dllexport) int __cdecl ZQS_Busy() {
	return QuickCastBusy() || (gestureOverlay[0] && (int)(GetTickCount() - gestureEnd) < 0) ? 1 : 0;
}

// Dla menu kolowego: wybor pod F - index = czar w ksiedze bohatera (jak Shift + kolko), -2 = kusza. 1 = wybrano.
extern "C" __declspec(dllexport) int __cdecl ZQS_Select(int index) {
	BYTE* npc = *(BYTE**)ADDR_PLAYER;
	if (!npc || isGothic1 || QuickCastBusy() || xbPressed || XbowBusy())
		return 0;
	if (index == -2) {
		if (!XbowState(0, 0, 0))
			return 0;
		ApplyXbowSelection("ZQS_Select");
		return 1;
	}
	void* book = ((GetPtr_t)ADDR_NPC_GETSPELLBOOK)(npc, 0);
	if (!book || index < 0 || index >= *(int*)((BYTE*)book + OFS_BOOK_NSPELLS))
		return 0;
	ApplySelection(npc, book, index, "ZQS_Select");
	return 1;
}

// Dla menu kolowego: ms do konca odnowienia Znakow (0 = gotowe).
extern "C" __declspec(dllexport) int __cdecl ZQS_CooldownLeftMs() {
	int left = (int)(cooldownUntil - GetTickCount());
	return left > 0 ? left : 0;
}

// Dla zDismember (Znaki Wiedzmina): 1 = celownik widoczny (F trzymane, czar z kierunkiem - takze w chwili rzutu: Spell_Cast
// przy puszczeniu F albo po progu trybu alternatywnego), wtedy znaki biora kierunek, punkt i cel z celownika. O trybie
// alternatywnym decyduje zDismember (SignAltHold) - zQuickSign tylko rysuje ladowanie (ZDISM_SignHoldState).
extern "C" __declspec(dllexport) int __cdecl ZQS_AimActive() {
	return aiming && aimReticleOn ? 1 : 0;
}

// Dla zDismember (Znaki Wiedzmina, 23.10): stan biezacego gestu Znaku - zDismember pyta w haku oCSpell::Cast, ktory wola
// Spell_Cast z CastNow zaraz po starcie gestu. 1 = gest gra: *castMs = ms do pchniecia dloni / dotkniecia ziemi (0, gdy juz
// minelo), *lengthMs = ms do konca gestu, *alt = 1 dla gestu trybu alternatywnego "w ziemie" (caly w miejscu - zDismember
// trzyma wtedy bohatera blokada ruchu i odklada skutek do dotkniecia ziemi). 0 = brak gestu (zwykla animacja rzutu, gesty
// wylaczone, gest juz sie skonczyl). Wskazniki moga byc 0.
extern "C" __declspec(dllexport) int __cdecl ZQS_SignGesture(int* castMs, int* lengthMs, int* alt) {
	int c = 0, l = 0;
	bool on = gestureOverlay[0] && gestureAniId >= 0
		&& qs::GestureTimeLeft(gestureCastMs, gestureLenMs, gestureStart, GetTickCount(), &c, &l);
	if (castMs)
		*castMs = on ? c : 0;
	if (lengthMs)
		*lengthMs = on ? l : 0;
	if (alt)
		*alt = on && gestureAlt ? 1 : 0;
	return on ? 1 : 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
		if (IsSupportedExe()) {
			LoadSettings();
			Log("zaladowany (%s), klawisz %d, odnowienie %d ms, animacja %d (%s), fokus magii %d, bez celu prosto %d, obrot %d, lewa dlon %d, celowanie od wcisniecia F %d",
				isGothic1 ? "Gothic 1" : "Gothic II NK", castKey, cooldownMs, useAnim, aniName, magicFocus, aimForward,
				turnToTarget, signLeftHand, aimOnPress && !isGothic1);
			Log("Znaki W3: gesty %d, skutek z koncem pchniecia %d (SignCastSyncMs %s, przerwanie po spoznieniu %d ms), "
				"gest alternatywny %s", signGestures, signCastSync, signCastSyncMsText, signSyncLateMs, signAltGestureMsText);
			PatchCall(ADDR_CALL_ACTIONMOVE, &HookActionMove, (void**)&origActionMove);
			PatchCall(ADDR_CALL_MAGICMODE, &HookMagicMode, (void**)&origMagicMode);
			if (!isGothic1) {
				// jmp 0x6FD0B8 / call bramki / test eax,eax / je - case DIK_4..DIK_0 w oCGame::HandleEvent
				static const BYTE keyCtx[] = { 0xE9, 0x0B, 0x01, 0x00, 0x00, 0xE8 };
				static const BYTE keyAfter[] = { 0x85, 0xC0, 0x0F, 0x84 };
				if (memcmp((void*)(ADDR_CALL_SPELLKEYGATE - 5), keyCtx, sizeof(keyCtx)) == 0
					&& memcmp((void*)(ADDR_CALL_SPELLKEYGATE + 5), keyAfter, sizeof(keyAfter)) == 0)
					PatchCall(ADDR_CALL_SPELLKEYGATE, &SpellKeyGateWrap, (void**)&origSpellKeyGate);
				else {
					selectWithShift = false;
					Log("klawisze run: nieznane bajty pod 0x%08X - bez Shift + 4..0", ADDR_CALL_SPELLKEYGATE - 5);
				}
				Log("wybor runy: Shift + 4..0 %d, Shift + kolko %d (znaki %s), etykieta %d", selectWithShift, signWheel,
					signWheelIdsText, showSelected);
				if (signWheel && zebaWheel)
					Log("UWAGA: zExtendedBattleAnimations ma EnableMouseWheel=1 - Shift + kolko zmienia tez styl walki zEBA "
						"(wylacz jedno: SignWheel=0 tutaj albo EnableMouseWheel=0 w Gothic.ini)");
			}
			// mov ecx,esi / call ChangeCamModeBySituation (oCAIHuman::DoAI) - kamera dystansowa w celowaniu z kuszy
			if (!isGothic1) {
				static const BYTE camCtx[] = { 0x8B, 0xCE, 0xE8 };
				if (memcmp((void*)(ADDR_CALL_CAMMODE - 2), camCtx, sizeof(camCtx)) == 0) {
					PatchCall(ADDR_CALL_CAMMODE, &HookCamMode, (void**)&origCamMode);
					Log("kusza: slot %d (prog przytrzymania %d ms), kamera dystansowa w celowaniu %d (hak 0x%08X -> 0x%08X%s)",
						xbowSlot, xbowHoldMs, xbowAimCamera, ADDR_CALL_CAMMODE, (DWORD)origCamMode,
						(DWORD)origCamMode == 0x0069CD60 ? " - ChangeCamModeBySituation" : " - hak innego pluginu");
				}
				else {
					xbowAimCamera = false;
					Log("kusza: nieznane bajty pod 0x%08X - bez kamery dystansowej w celowaniu", ADDR_CALL_CAMMODE - 2);
				}
			}
			// push esi / mov ecx,edi / call OnDamage_Hit (0x666510) - jak wyzej, cel z rel32
			static const BYTE dmgCtx[] = { 0x56, 0x8B, 0xCF, 0xE8 };
			if (memcmp((void*)(ADDR_CALL_ONDAMAGEHIT - 3), dmgCtx, sizeof(dmgCtx)) == 0)
				PatchCall(ADDR_CALL_ONDAMAGEHIT, &HookOnDamageHit, (void**)&origOnDamageHit);
			else
				Log("OnDamage_Hit: nieznane bajty pod 0x%08X - bez poprawki obrazen czarow", ADDR_CALL_ONDAMAGEHIT - 3);
		}
	}
	return TRUE;
}
