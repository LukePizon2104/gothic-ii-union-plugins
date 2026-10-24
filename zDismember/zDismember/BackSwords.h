// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Miecze na plecach (spec [dokument projektowy - niepublikowany], logika Core\BackSwords, animacje
	// anims\pipeline\make_draw.py w prywatnej paczce zDismember_W3.vdf): dobywanie zza plecow W3 (2H, 1H z Back1H),
	// szybsze natywne dobycie 1H przy pasie, drugi miecz na plecach (wezel ZS_BOW) i zamiana klawiszem.
	void BackSwordsLoad();                               // Game_Init / zdism reload: pozycje A / B, paczka W3, klawisz
	void BackSwordsUpdate(float dt);                     // petla: klawisz, zamiana, plan, wpisy nakladek, wizual drugiego
	void BackSwordsApply(zCModel* m);                    // hak zCModel::AdvanceAnis: wezly slotow na plecach
	std::vector<std::string> BackSwordsOverlays();       // nakladki dobycia do planu Styles (puste = wylaczone / brak paczki)
	std::vector<std::string> BackSwordsOverlayNames();   // wszystkie nasze nakladki dobycia z paczki (zawczasu)
	bool BackSwordsIsOurOverlay(const std::string& name);
	void BackSwordsOverlayFailed(const std::string& name);   // plan nie zalozyl nakladki - bez ponawiania
	void BackSwordsBeforeSave();                         // zmienne skryptu ZDF_BACK_* aktualne w zapisie
	void BackSwordsLoadBegin();
	void BackSwordsLoadEnd();                            // stan z zapisu (drugi miecz, pozycja broni), prototypy B / BELT
	void BackSwordsRestore(const char* why);             // wylaczone: wpisy i tempo nakladek z pliku, wizualy i wezly gry
	bool BackSwordsSwapping();
	std::string BackSwordsConsole(const std::string& args);
}
