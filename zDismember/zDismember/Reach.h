// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Ciosy stylu W3 bohatera (T_xHATTACKL/R i kombinacja S_xHATTACK z nakladek HUMANS_ZDF_W3_*), grane przez system
	// walki gry. Silnik sprawdza trafienie raz, w klatce DEF_OPT_FRAME (oCAniCtrl_Human::HitCombo): cel ciosu (fokus
	// z chwili startu ataku) w zasiegu walki (od bohatera do bboxa celu <= FIGHT_RANGE_BASE + zasieg broni,
	// oCNpc::IsInFightRange), w kacie +-30 st. (IsInFightFocus) i na tej samej wysokosci. Ciecia W3 trafiaja pozniej niz
	// natywne (0,40 / 0,48 s zamiast 0,08-0,20 s), wiec cel czesciej zdazy odejsc.
	// Doskok do celu (W3AttackAssist, jak w W3): w zamachu bohater obraca sie do wroga w stozku przed soba i dosuwa
	// (z kolizja, po terenie) do idealnej odleglosci w chwili ciosu (zasieg - W3AssistMargin), najwyzej W3AssistMaxPath cm
	// na ciecie z predkoscia do W3AssistSpeed; blizej - bez ruchu albo lekko w tyl. Pusty cel ciosu silnika (bez fokusu)
	// dostaje wroga, do ktorego bohater doskakuje.
	// Premia zasiegu (W3AttackReach, cm): w tescie trafienia ciosu W3 bohatera zasieg silnika + premia.
	// Debug=1: wpis w logu przy kazdym ciosie bohatera (cel, odleglosc przed / po, doskok, kat, wynik).
	void ReachUpdate(float dt);
	// Wczytanie / zapis / zdism reload - bez celu i bez trwajacego doskoku.
	void ReachReset();
}
