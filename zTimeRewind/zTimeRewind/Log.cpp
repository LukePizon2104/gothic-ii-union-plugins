// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Plik otwarty raz, linie w buforze; zapis w ZLogFlush (koniec Game_Loop), przy duzym buforze, przy wyjsciu i przy
	// wyjatku krytycznym (_shared\ZLogFile.h). Dawniej fopen/fclose na kazda linie - z Defenderem nawet ms na linie.
	static zlogfile::LogFile trLogFile("zTimeRewind.log");

	static void ZLogV(const char* fmt, va_list args) {
		ZPROF_SCOPE("Log");
		trLogFile.WriteV(fmt, args);
	}

	void ZLog(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ZLogV(fmt, args);
		va_end(args);
	}

	void ZDebug(const char* fmt, ...) {
		if (!trcfg.debug)
			return;
		va_list args;
		va_start(args, fmt);
		ZLogV(fmt, args);
		va_end(args);
	}

	void ZLogFlush() {
		if (!trLogFile.Pending())
			return;
		ZPROF_SCOPE("Log");
		trLogFile.Flush();
	}

	// Linie profilera (przycinki, podsumowania) do zTimeRewind.log.
	void ZProfLine(const char* line) {
		ZLog("%s", line);
	}
}
