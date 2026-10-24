// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	void FxInit();                                   // Game_Init
	void FxBegin();                                  // start przewijania: dzwiek startu + petla
	void FxEnd();                                    // koniec: stop petli + dzwiek konca
	void FxUpdate(float rewound, float available);   // co klatke przewijania
	void FxRender();                                 // Game_PostLoop: nakladka i pasek
}
