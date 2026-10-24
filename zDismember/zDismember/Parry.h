// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Udany blok (parada) bohatera i NPC. Silnik w klatce ciosu (oCAniCtrl_Human::HitCombo 0x6B0260) pyta cel o CanParade
	// i zamiast CreateHit wola StartParadeEffects (0x6B16F0) na sterowniku NAPASTNIKA: dzwiek zderzenia broni
	// + czasteczki CPFX_IAI_<material> przy dloni napastnika (ZS_RIGHTHAND). W skryptach sa tylko CPFX_IAI_METAL/STONE/WOOD
	// (iskry 2x2 cm, ok. 8 sztuk - praktycznie niewidoczne), para roznych materialow (CPFX_IAI_METAL_WOOD) nie ma emitera,
	// a przy piesciach silnik czasteczek nie daje wcale. Hak doklada:
	//  - ParryFx: iskry z blyskiem (metal o metal) albo drzazgi (drewno, kostur) w punkcie zetkniecia broni,
	//  - ParryRecoil: krotki proceduralny odrzut broni, rak i tulowia u broniacego i napastnika (w haku AdvanceAnis).
	// Odskok parady (T_xHPARADEJUMPB) i ruchy W3 na nim (uniki, przewroty) - bez efektow; kontra W3 - z efektami.

	// Z hooka zCModel::AdvanceAnis: po animacji i ragdollu, przed sciskaniem odcietych czesci.
	void ParryRecoilApply(zCModel* model);
	void ParryUpdate(float dt);
	// Usuwa efekty ze swiata i konczy odrzuty (zapis, wczytanie, zmiana swiata).
	void ParryClearAll();
	// Konsola: blok bez walki - attacker uderza, defender blokuje (efekt i odrzut jak przy prawdziwej paradzie).
	std::string ParryTest(oCNpc* attacker, oCNpc* defender);
	// Blysk z iskrami w punkcie (unik w ostatniej chwili, Dodge.cpp) - te same emitery co przy bloku.
	void ParryFxFlash(const zVEC3& pos);
}
