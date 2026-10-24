// Logika bez silnika: miecze na plecach (BackSwords.cpp, spec [dokument projektowy - niepublikowany]).
// Gdzie wisi bron bohatera (pas / plecy A = stal Geralta / plecy B = srebro), ktory wariant animacji dobycia graja
// nakladki HUMANS_ZDF_DRAW_1H / _2H, drugi miecz na plecach, zamiana broni (chowanie -> zalozenie -> dobycie),
// macierze slotow wzgledem BIP01 SPINE2 i zapis drugiej broni w zmiennych skryptu.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zdcore {
	// Miejsce broni: natywne (pas 1H / natywny ZS_LONGSWORD - wezel bez zmian), plecy A (stal), plecy B (srebro).
	enum BackSpot { SPOT_NATIVE = 0, SPOT_A = 1, SPOT_B = 2 };
	// Wariant animacji dobycia jednego typu broni (wpisy nakladki HUMANS_ZDF_DRAW_xH): natywny (pusty wpis - gra nakladka
	// talentu / zEBA spod spodu), BELT (natywne 1H z HUMANS w tempie BeltDrawSpeed), W3 z pozycji A albo B.
	enum DrawVariant { DRAWV_NATIVE = 0, DRAWV_BELT = 1, DRAWV_A = 2, DRAWV_B = 3, DRAWV_COUNT };
	// Wezel slotu, ktory plugin ustawia na plecach (po animacji, w haku zCModel::AdvanceAnis).
	enum BackNode { BNODE_NONE = 0, BNODE_SWORD = 1, BNODE_LONGSWORD = 2, BNODE_BOW = 3 };
	// Back1H: 0 przy pasie, 1 miecze (ITEM_SWD z ostrzem) na plecach, 2 kazda bron 1H na plecach
	enum Back1HMode { BACK1H_OFF = 0, BACK1H_SWORDS = 1, BACK1H_ALL = 2, BACK1H_COUNT };
	// SecondWithRanged: przy zalozonym luku / kuszy drugi miecz 0 ukryty, 1 zamiast luku na plecach (luk / kusza ukryte)
	enum SecondRanged { SECOND_RANGED_HIDE = 0, SECOND_RANGED_REPLACE = 1, SECOND_RANGED_COUNT };
	// Typ broni jak SwapTypeOfItemFlags (AniSwap.h): 0 brak, 1 1H, 2 2H.
	enum { BACK_TYPE_NONE = 0, BACK_TYPE_1H = 1, BACK_TYPE_2H = 2 };

	struct BackSettings {
		bool on = true;                       // BackSwords: dobywanie zza plecow W3, 2H i 1H na plecach
		int back1H = BACK1H_SWORDS;           // Back1H
		int beltSpeed = 150;                  // BeltDrawSpeed: % tempa natywnego dobycia 1H przy pasie (100 = gra bez zmian)
		bool second = true;                   // SecondSword: drugi miecz na plecach i zamiana
		int withRanged = SECOND_RANGED_HIDE;  // SecondWithRanged
	};

	struct BackState {
		int primaryType = BACK_TYPE_NONE;     // zalozona bron biala
		bool primaryBlade = false;            // 1H: miecz z ostrzem (ITEM_SWD, nie obuch) - Back1H=1
		bool primaryAtB = false;              // zalozona bron w pozycji B (po zamianie z drugim mieczem)
		bool secondValid = false;             // drugi miecz jest w ekwipunku (niezalozony)
		bool bowEquipped = false, cbowEquipped = false;
		bool overlays = false;                // nakladki dobycia W3 sa (prywatna paczka zDismember_W3.vdf) i pozycje A / B
		bool dualPair = false;                // bron parowa New Balance (DualState != DUAL_NONE): polozenie i dobycie
		                                      // natywne NB (animacje 1x2 / 2x2 dobywaja obie bronie), bez zamiany
	};

	struct BackPlan {
		int primarySpot = SPOT_NATIVE;
		int primaryNode = BNODE_NONE;         // wezel slotu zalozonej broni do przestawienia (ZS_SWORD / ZS_LONGSWORD)
		bool second = false;                  // drugi miecz widoczny na plecach (wizual w wezle ZS_BOW)
		int secondSpot = SPOT_NATIVE;         // A / B
		bool hideCrossbow = false;            // kusza ukryta (drugi miecz zamiast niej); luk znika sam - ten sam wezel
		int draw1H = DRAWV_NATIVE, draw2H = DRAWV_NATIVE;
		bool operator==(const BackPlan& o) const;
		bool operator!=(const BackPlan& o) const { return !(*this == o); }
	};
	BackPlan PlanBack(const BackSettings& s, const BackState& st);
	const char* BackSpotLabel(int spot);
	const char* DrawVariantLabel(int v);
	std::string BackPlanText(const BackPlan& p);
	// Po zamianie: zalozona bron zajmuje miejsce drugiego miecza (B, gdy drugi wisial w B), dawna idzie w wolne.
	bool PrimaryAtBAfterSwap(const BackPlan& before);

	// ---------- macierze (3x4 wierszami jak zMAT4: obrot 3x3 + przesuniecie w 4. kolumnie, cm)
	struct BackMat {
		float m[3][4];
	};
	BackMat BackIdentity();
	BackMat BackMul(const BackMat& a, const BackMat& b);       // a * b (afinicznie)
	BackMat BackInverse(const BackMat& a);                      // odwrotnosc przeksztalcenia sztywnego
	// Przesuniecie z ini (BackAdjustA / B): x y z (cm) rx ry rz (st.), w ukladzie miecza: M * T(xyz) * Rx * Ry * Rz.
	BackMat BackAdjust(const BackMat& base, const float adj[6]);
	bool ParseFloats(const std::string& text, float* out, int count);
	// backswords_w3.ini z make_draw.py: PosA= / PosB= (12 liczb). err = opis bledu.
	bool ParseBackIni(const std::string& text, BackMat& a, BackMat& b, std::string& err);
	float BackMatDistance(const BackMat& a, const BackMat& b);  // odleglosc przesuniec (cm)
	bool BackMatFinite(const BackMat& a);                       // bez NaN / nieskonczonosci i bez wartosci > 1e5
	// Straznik przestawienia wezla (23.10): polozenie wezla wzgledem BIP01 SPINE2 w ukladzie modelu (spineObj, nodeObj -
	// iloczyny lokalnych macierzy po rodzicach, jak u silnika) a pozycja z planu want: blad przesuniecia w cm; -1 =
	// macierze niepoprawne. Duzy blad = lancuch wezlow nieaktualny (inny model / szkielet) - wtedy natywny slot.
	float BackOffsetError(const BackMat& spineObj, const BackMat& nodeObj, const BackMat& want);
	const float BACK_GUARD_TOL = 5.0f;      // cm - wiecej = przestawienie odrzucone w tej klatce
	// Diagnostyka renderu: odleglosc miecza od kregoslupa w macierzach z ostatniego renderu (trafoObjToCam) a oczekiwana
	// |want| - wpis, gdy roznica wieksza (miecz widoczny poza plecami); BACK_DIAG_REF = ta sama roznica dla pary wezlow
	// kontrolnych (renderer nie odswiezyl macierzy - wpis pomijany).
	const float BACK_DIAG_TOL = 30.0f;
	bool BackRenderDrift(float swordToSpine, float expected, float refNow, float refRest, float& drift);

	// ---------- druga bron w zapisie (zmienne skryptu ZDF_BACK_SECOND / ZDF_BACK_FLAGS)
	// Skrot nazwy instancji (wielkie litery, FNV-1a, 31 bitow, nigdy 0 - 0 = brak).
	uint32_t BackNameHash(const std::string& instance);
	enum { BACK_FLAG_PRIMARY_B = 1 };

	// ---------- zamiana z drugim mieczem
	enum BackSwapPhase { SWP_IDLE = 0, SWP_SHEATHE, SWP_EQUIP, SWP_DRAW };
	enum BackSwapAction { SWA_NONE = 0, SWA_REMOVE, SWA_EQUIP, SWA_DRAW, SWA_DONE, SWA_FAIL, SWA_FAIL_REDRAW };
	struct BackSwapRun {
		int phase = SWP_IDLE;
		float phaseT = 0.0f;                  // s w biezacej fazie
		float total = 0.0f;
		bool removeSent = false, drawSent = false;
		std::string why;                      // powod konca (log)
	};
	struct BackSwapTick {
		float dt = 0.0f;
		int weaponMode = 0;                   // tryb broni bohatera teraz (0 = schowana)
		bool weaponMsg = false;               // komunikat broni w kolejce bohatera
		bool newEquipped = false;             // zalozona bron to dawny drugi miecz (po SWA_EQUIP)
		bool interrupted = false;             // trafienie, rozmowa, smierc, finisher, wczytanie...
	};
	const float BACK_SWAP_PHASE_LIMIT = 3.0f;   // s na schowanie / dobycie (dluzej = zaciecie)
	const float BACK_SWAP_GRACE = 0.25f;        // s po wyslaniu komunikatu: brak go w kolejce i bron bez zmian = odrzucony
	// Start zamiany z dobyta bronia biala (chowanie -> zalozenie -> dobycie).
	void BackSwapStart(BackSwapRun& r);
	BackSwapAction BackSwapStep(BackSwapRun& r, const BackSwapTick& t);
	const char* BackSwapPhaseLabel(int phase);

	// Czy zamiane mozna zaczac teraz: "" = tak, inaczej powod (log / napis).
	struct BackSwapGate {
		bool secondValid = false, inventoryOpen = false, dialog = false, console = false, busy = false;
		bool dead = false, swapping = false, weaponMsg = false, attacking = false, mounted = false;
		bool dualPair = false;                // para broni NB (zamiana z drugim mieczem rozbilaby pare)
		std::string busyWhat;
	};
	std::string BackSwapRefusal(const BackSwapGate& g);

	// ---------- klawisz drugiego miecza: stan silnika (zinput->KeyPressed) + zapas fizyczny (GetAsyncKeyState)
	// Wcisniecie = zbocze stanu "silnik LUB fizycznie" (jedno na przytrzymanie, niezaleznie od tego, ktore zrodlo pierwsze).
	// Fizyczny stan liczy sie tylko przy oknie gry na pierwszym planie i poza polem tekstowym (wyszukiwarka HSI).
	// Puszczenie po przytrzymaniu, ktorego silnik ani razu nie widzial = dowod, ze cos czysci klawiature gry (log).
	enum SecondKeySource { SKS_NONE = 0, SKS_ENGINE, SKS_PHYSICAL };
	struct SecondKeyInput {
		bool engine = false;     // zinput->KeyPressed(klawisz) i modyfikatory
		bool physical = false;   // GetAsyncKeyState(klawisz) i modyfikatory
		bool focus = false;      // okno gry na pierwszym planie
		bool typing = false;     // pole tekstowe ma klawiature (wyszukiwarka HSI) - fizyczny stan pominiety
	};
	struct SecondKeyState {
		bool held = false;       // przytrzymanie trwa (dowolne zrodlo)
		bool engineSeen = false; // silnik widzial to przytrzymanie
		int frames = 0;          // klatki przytrzymania
	};
	struct SecondKeyEvent {
		int pressed = SKS_NONE;      // nowe wcisniecie w tej klatce: zrodlo
		bool releasedUnseen = false; // przytrzymanie skonczone, silnik go nie widzial
		int heldFrames = 0;          // klatki skonczonego przytrzymania
	};
	SecondKeyEvent SecondKeyStep(SecondKeyState& s, const SecondKeyInput& in);
	const char* SecondKeySourceLabel(int source);

	// Menu: tempo (%) -> pozycja listy i z powrotem (MenuBackDrawSpeed itd.)
	const std::vector<int>& BackSpeedMenuValues();      // 50..200 %
	const std::vector<int>& BeltSpeedMenuValues();      // 100 (natywnie) .. 250 %
	const std::vector<int>& SecondKeyMenuValues();      // G, B, Y, R, wyl.
}
