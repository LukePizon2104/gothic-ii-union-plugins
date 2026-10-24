// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Uniki i przewroty (W3, prywatna paczka): System\zDismember\moves_w3.ini + nakladki HUMANS_ZDF_MV_*.
	// Klawisz uniku + kierunek (Q/E albo A/D w bok, S w tyl, W w przod) w trybie walki 1H/2H/piesci.
	// Przewrot pod klawiszem skoku (Alt): z bokiem / tylem od razu, w przod = Alt+W i drugie Alt w 0,30 s (bez drugiego
	// = skok gry). Gdy ruch nie moze wystartowac (skok, upadek, atak gry, trafienie) klawisz skoku idzie od razu do gry.
	// Przewrot w tyl konczy sie przodem do przeciwnika (klip W3 z obrotem), ruch w strone kamery przyspiesza na ten
	// czas wygladzanie kamery (zCMovementTracker). Ruchy ida po zboczach do kata chodu Gothica, utkniecie w scianie
	// konczy ruch plynnie.
	// Kontra (COUNTER_L/R, Counters=1): parada rozpoczeta w trakcie zamachu przeciwnika (1H/2H) = odbicie i riposta,
	// z szansa repel (bark / kopniak: cios obuchowy, zatoczenie silnika + odepchniecie); wypad celowany w przeciwnika
	// (zasieg reach=).
	// Atak z wyskoku (JUMPATK_F, _F2, _F3 1H, JUMPATK2H_F... 2H - Rend i ciecia W3 z wyskokiem): w walce bronia biala
	// klawisz skoku (Alt, sam albo z W) i atak gry w 0,22 s (z W 0,30 s; tez w tej samej klatce co Alt) - wypad
	// z wyskokiem i ciecie w wroga przed bohaterem (CreateHit, jak cios bronia). Wariant losowany przy kazdym uzyciu (bez
	// powtorzenia poprzedniego), z celem sposrod tych, ktorych wypad pasuje do odleglosci. Sam Alt bez ataku = po 0,22 s
	// skok gry, Alt+W bez drugiego Alt / ataku = po 0,30 s.
	// Atak w biegu (RUNATK_F/_F2 1H, RUNATK2H_F/_F2 2H): w stylu W3 atak w biegu gry (bieg do przodu + atak,
	// oCNpc::EV_AttackRun) przejety - ruch W3 calym cialem z wypadem i ciosem zamiast gornej polowy nad biegiem; wariant
	// z daleka / z bliska wg odleglosci celu. W innych stylach (Gothic, zEBA) atak w biegu gry bez zmian. Wejscie
	// z biegu / sprintu mieszane (petla biegu wygasa pod ruchem), bez celu wypad z rozpedem sprintu; z W trzymanym po
	// ciosie prosto z powrotem w bieg / sprint (bez gardy), kolejny atak w biegu w tym czasie czeka w buforze.
	// Unik / przewrot z wrogiem w celowniku (fokus albo enemy bohatera): bok = krazenie wokol niego, tyl = odskok,
	// bohater caly czas obraca sie przodem do wroga (przod - bez sledzenia, flankowanie). Mysz / klawisze obrotu
	// w trakcie obracaja bohatera i reszte toru (z celem tylko lekka korekta). Postacie nie blokuja uniku / przewrotu
	// (przenikanie, omijanie srodka), koniec wypychany obok postaci.
	// Blok w koncowce ruchu (po ciosie kontry / ataku, po glownej drodze uniku / przewrotu) przerywa ruch parada gry.
	// Unik / przewrot (17.10, Core/Evade): atak, blok albo kolejny unik / przewrot wcisniety w trakcie czeka w buforze
	// (EvadeExitBuffer) i wychodzi od okna wyjscia exit= z moves_w3.ini (po glownej drodze, stopa na ziemi) - atak przez
	// komunikat gry EV_ATTACKFORWARD / LEFT / RIGHT z kierunku wcisniecia (jak FightMelee: akcja + W / A,Q / D,E; kombo,
	// trafienie, doskok Reach jak zwykle), kolejny ruch po 0,1 s przejscia. Od
	// EvadeFaceLead s przed oknem bohater obraca sie przodem do wroga (reszta toru w kierunku sprzed obrotu). Unik tuz
	// przed ciosem wroga (PerfectDodge*): wrog na chwile wolniej sie obraca i nie zaczyna ataku, pierwszy cios bohatera
	// szybszy, blysk + dzwiek.
	void MovesLoad();
	// Po zmianie klawiszy (menu): ponowne sprawdzenie kolizji z [KEYS] (Alt = skok).
	void MovesReloadKeys();
	// Klawisze gry w walce: skok (GAME_SMOVE/2) przy klawiszu przewrotu / skoku i atak (GAME_ACTION*) w oknie ataku
	// z wyskoku: -1 bez zmian, 0 = silnik ma go nie widziec, 1 = wcisniety (skok oddany grze po oknie).
	// toggled: wywolanie z GetToggled.
	int MovesGameKeyOverride(unsigned short logicalKey, bool toggled);
	void MovesUpdate(float dt);
	bool MovesActive();
	// Unik / przewrot konczy sie atakiem gry z okna wyjscia (ruch jeszcze wygasa) - doskok ciosu (Reach) juz dziala.
	bool MovesAttackHandoff();
	// Okno nietykalnosci trwajacego ruchu.
	bool MovesHeroInvulnerable();
	void MovesFinishNow();
	int MovesCount();
	const std::vector<zdcore::MoveDef>& MovesDefs();
	// Konsola: lista ruchow i wymuszenie (bez sprawdzania klawiszy i konsoli; JUMPATK_* / RUNATK_* = atak z wypadem
	// z wyborem celu, dokladnie ten wariant).
	std::string MovesList();

	// Bohater w ludzkim modelu (po przemianie w zwierze nakladki HUMANS_* sie nie nadaja).
	bool HeroIsHuman();
	// Chwyt broni z prototypu modelu (spoczynek) - po naszych animacjach, gdy zostaly przerwane z przekreconym slotem
	// (animacje zEBA czesto nie steruja ZS_RIGHTHAND i trzymalyby bron tak, jak ja zostawilismy).
	void ResetWeaponGrip(oCNpc* npc);
	const char* MovesForce(const std::string& id);
}
