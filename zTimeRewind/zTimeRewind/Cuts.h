// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Odciete czesci ciala zDismember w cofnieciu czasu (kanal plugins\_shared\ZCutLink.h, logika trcore Core\Cuts.h).
	// Bez zDismember (tablica bez funkcji) wszystko tu jest bez skutku - nagrywanie i Commit jak dotad.

	// Game_Init: dolaczenie do kanalu (zDismember moze sie zaladowac pozniej - sprawdzane przy kazdym uzyciu).
	void TrCutsInit();
	// Wczytanie swiata: linia w logu, gdy zmienil sie stan polaczenia z zDismember.
	void TrCutsLogState();
	// Tick nagrywania postaci: maska odciec do toru (tylko zmiany; zmiana = linia w logu).
	void TrRecordCuts(TrActor& a, float t);
	// Start przewijania: stan podgladu postaci (martwa / z odcieciami = liczona w klatkach).
	void TrCutsBegin(TrActor& a);
	// Klatka przewijania (marionetki): czesci wracaja, gdy kursor minie chwile odciecia, ragdoll zwlok znika, gdy minie smierc.
	void TrCutsPreview(TrActor& a, float t, const trcore::Sample& s);
	// Commit postaci - PRZED ustawieniem (HardSet / ResetPos): maska z chwili docelowej, przy wstawaniu sprzatanie zDismember.
	void TrCutsCommit(TrActor& a, float t, const trcore::Sample& target, trcore::CommitAction action, bool deadNow, bool knockedNow);
	// Podsumowanie jednego Commit (linia w logu, gdy cos przywrocono).
	void TrCutsCommitBegin();
	void TrCutsCommitEnd();
}
