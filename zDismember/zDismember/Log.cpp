// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	unsigned int zdFrame = 0;

	// Plik otwarty raz, linie w buforze; zapis w ZLogFlush (koniec Game_Loop), przy duzym buforze, przy wyjsciu i przy
	// wyjatku krytycznym (_shared\ZLogFile.h). Dawniej fopen/fclose na kazda linie - z Defenderem nawet ms na linie.
	static zlogfile::LogFile zdLogFile("zDismember.log");

	static void ZLogV(const char* fmt, va_list args) {
		ZPROF_SCOPE("Log");
		zdLogFile.WriteV(fmt, args);
	}

	void ZLog(const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		ZLogV(fmt, args);
		va_end(args);
	}

	void ZDebug(const char* fmt, ...) {
		if (!zcfg.debug)
			return;
		va_list args;
		va_start(args, fmt);
		ZLogV(fmt, args);
		va_end(args);
	}

	void ZLogFlush() {
		if (!zdLogFile.Pending())
			return;
		ZPROF_SCOPE("Log");
		zdLogFile.Flush();
	}

	// Linie profilera (przycinki, podsumowania) do zDismember.log.
	void ZProfLine(const char* line) {
		ZLog("%s", line);
	}
}
