// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Finisher: przygotowanie (wybor definicji, miejsce) -> ustawienie -> animacja -> ciecie -> powrot.
	// pose: zdcore::POSE_STANDING (odroczona smierc) / POSE_DAZED (egzekucja oszolomionego).
	bool DirectorTryStart(oCNpc* victim, uint32_t pose, const zdcore::FinisherDef* forced = nullptr);
	// Rezyser trzyma model bohatera: ustawienie, animacja i Settle (sterowanie juz u gracza, ale nakladka finishera
	// jeszcze na modelu - do jej zdjecia nie przekladamy stylow).
	bool DirectorActive();
	bool DirectorIsVictim(oCNpc* npc);
	void DirectorUpdate(float dt);
	// Natychmiastowe zakonczenie (zapis, zmiana swiata): ciecie i smierc, sprzatanie.
	void DirectorFinishNow();
	// Cofniecie czasu wskrzesza ofiare: konczy ogon jej animacji (prowadzenie po torze po oddaniu sterowania). False, gdy
	// postac nie jest w ogonie. Trwajacego finishera nie rusza (runa i ratunek nie startuja w trakcie).
	bool DirectorForgetVictim(oCNpc* npc);
	void DirectorInit();

	// Zalozenie / zdjecie nakladki .MDS bez zatrzymania animacji: silnik w Apply/RemoveModelProtoOverlay zatrzymuje
	// wszystkie animacje modelu (warstwy 0..99999) - te same animacje wracaja w tych samych klatkach i przejsciach.
	// Animacje ze zdejmowanej nakladki nie wracaja.
	bool ZApplyOverlayKeepAnis(zCModel* m, const std::string& mds);
	void ZRemoveOverlayKeepAnis(zCModel* m, const std::string& mds);
}
