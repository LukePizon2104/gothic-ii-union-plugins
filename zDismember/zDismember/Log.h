// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Dopisuje linie do zDismember.log (katalog glowny gry). Buforowane - na dysk w ZLogFlush.
	void ZLog(const char* fmt, ...);
	// Jak ZLog, ale tylko przy Debug=1.
	void ZDebug(const char* fmt, ...);
	// Zapis bufora logu na dysk: koniec Game_Loop, menu, wczytanie / zapis, wyjscie (przy awarii - sam handler).
	void ZLogFlush();
	// Ujscie linii profilera (zprof::Init).
	void ZProfLine(const char* line);
	// Licznik klatek gry (Game_Loop) - do laczenia trafienia ze smiercia w tej samej klatce.
	extern unsigned int zdFrame;
}
