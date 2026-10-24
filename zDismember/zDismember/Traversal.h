// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Przeskoki i wspinanie w biegu (parkour W3, prywatna paczka): System\zDismember\traversal_w3.ini + nakladki
	// HUMANS_ZDF_MV_TRAV_*. Klawisz skoku gry (Alt) w biegu do przodu (W) przed pasujaca przeszkoda: wspiecie na krawedz
	// (gora 30-250 cm, platforma z miejscem na postac) albo przeskok (cienka gora, ziemia za nia od -120 do +40 cm).
	// Bez przeszkody / gdy nie pasuje - zwykly skok gry od razu. Bez broni zawsze, z dobyta bronia / piesciami przy
	// TraversalCombat=1 (pierwszenstwo przed przewrotem Alt+W, Alt i atakiem z wyskoku tylko przed pasujaca przeszkoda).
	// Wykrywanie: promienie przed bohaterem (Core TravDetect), klip wg wysokosci (kubelek) i nogi podpartej w biegu,
	// start, gdy sciana jest w odleglosci klipu (do 0,5 s biegu czekania). Vob prowadzony po torze 3D bez fizyki skoku
	// i bez kolizji (sciana przeszkody), z kontrola promieniem po drodze; na koncu z W prosto w bieg albo postawa.
	void TraversalLoad();
	// Klawisz skoku: rozstrzygniecie wcisniecia (raz na wcisniecie; z haka wejscia i z MovesUpdate - kolejnosc
	// dowolna). true = przejety (gra i uniki go nie widza) - ruch trwa, czeka na odleglosc albo wcisniecie bylo nasze.
	bool TraversalTakesJump();
	// Klawisze gry w czasie dosuwania chwytu w locie do zawisu (S + skok dla silnika = zawis trzymany): -1 bez zmian,
	// 1 = wcisniety. toggled: wywolanie z GetToggled (bez zmian).
	int TraversalGameKeyOverride(unsigned short logicalKey, bool toggled);
	void TraversalUpdate(float dt);
	// Ruch trwa (nakladka, blokada sterowania) - uniki, style i finishery czekaja.
	bool TraversalActive();
	// 24.10: tor przeskoku / wyjscie w bieg / skok gry z biegu w locie - lokomocja W3 bez zmian (plan nakladek, tempo).
	bool TraversalHoldsMotion();
	void TraversalFinishNow();
	int TraversalCount();
	const std::vector<zdcore::TravDef>& TraversalDefs();
	// Konsola: zdism trav [info|go|list|show|hide|kropki|linie] - wynik wykrywania przed bohaterem, wymuszenie, lista
	// klipow, debug wizualny (wlacz / wylacz, linie z kropek albo DrawLine).
	std::string TraversalConsole(const std::string& arg);
	// Debug wizualny (TraversalDebug=1 albo zdism trav show): wykrywanie co 0,1 s przed bohaterem, rysowanie promieni,
	// sciany, gory, okna startu, konca ruchu i skoku gry; Alt zamraza obraz decyzji na 5 s. Z LoopUpdate co klatke.
	void TraversalDebugUpdate(float dt);
	// Tor przeszkod (zdism trav tor): usuniecie vobow toru (wczytanie / zmiana swiata, "tor usun").
	void TraversalCourseClear();
	// Diagnostyka (21.10): W trzymane, a bohater nie rusza do przodu - wpis ze stanem bohatera w logu. Z LoopUpdate.
	void HeroForwardDiagUpdate(float dt);
}
