// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Kanal masek odciec dla zTimeRewind (plugins\_shared\ZCutLink.h): Game_Init publikuje funkcje odczytu i ustawienia
	// maski w pamieci nazwanej procesu. zTimeRewind zapisuje maske w migawkach postaci i przy cofnieciu czasu przywraca
	// czesci ciala. Bez zTimeRewind nic sie nie dzieje.
	void RewindLinkInit();
	// Maska odciec postaci = mask (zdjete bity: koniec sciskania, odlamki i krew z kikuta usuniete; nowe bity: sciskanie
	// bez odlamka). flags zcut::FLAG_*: ALIVE - ragdoll zwlok porzucony; REVIVE - przed ResetPos: nakladki ofiary
	// finishera / oszolomienia zdjete, ogon finishera i oszolomienie zakonczone. who - do logu. Zwraca zcut::DONE_*.
	uint32_t RewindRestoreCuts(oCNpc* npc, uint32_t mask, uint32_t flags, const char* who);
}
