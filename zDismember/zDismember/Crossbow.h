// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Szybki strzal z kuszy w stylu W3 (spec [dokument projektowy - niepublikowany], logika Core\Crossbow, animacje
	// anims\pipeline\make_xbow.py w prywatnej paczce zDismember_W3.vdf). Wejscie (F, stukniecie / przytrzymanie, celownik,
	// wybor slotu) prowadzi zQuickSign przez eksporty ZDISM_Xbow*; tu: animacje (pozyczone wpisy protoAnis nosnikow
	// Znakow), kusza z plecow do lewej dloni, pocisk silnika bez trybu dystansowego (udawany tryb kuszy na czas
	// SetupAIVob i trafienia), wybor celu, obrot, przerwania, odnowienie.
	void CrossbowLoad();                         // Game_Init / zdism reload: chwile z crossbow_w3.ini, paczka
	void CrossbowUpdate(float dt);               // petla gry
	void CrossbowApplyNodes(zCModel* m);         // hak zCModel::AdvanceAnis: skala / przesuniecie kuszy w dloni
	void CrossbowReset(const char* why);         // natychmiast: animacja, wizual, wpisy protoAnis (zapis, wczytanie, wyl.)
	void CrossbowLoadBegin();
	void CrossbowLoadEnd();                       // wybor slotu z ZDF_XBOW_SLOT
	void CrossbowBeforeSave();                    // ZDF_XBOW_SLOT aktualne w zapisie
	bool XbowInHand();                            // kusza w lewej dloni (BackSwords nie przywraca wtedy ZS_CROSSBOW)
	bool XbowBusy();                              // strzal w toku (wejscie .. wyjscie)
	// Obserwacja trafien (CombatHooks OnDamage): log trafienia naszym beltem i trafienie bohatera (przerwanie przed beltem).
	void XbowOnDamage(oCNpc* victim, oCNpc::oSDamageDescriptor& desc, int hpBefore, bool swallowed);
	// Na czas OnDamage naszym beltem poza oknem ReportCollisionToAI (wiadomosc obrazen z kolejki): tryb kuszy udawany.
	struct XbowDamageGuard {
		void* p = nullptr;
		XbowDamageGuard(oCNpc* victim, oCNpc::oSDamageDescriptor& desc);
		~XbowDamageGuard();
	};
	std::string CrossbowConsole(const std::string& args);   // zdism xbow [info|fire|shoot|aim|anis|reset]
}
