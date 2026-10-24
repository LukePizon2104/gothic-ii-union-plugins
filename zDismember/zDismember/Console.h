// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// args[0] = nazwa podkomendy (wielkimi literami), dalej argumenty.
	typedef std::function<zSTRING(const std::vector<std::string>& args)> ZCommandFn;

	// Rejestruje "zdism <name>". Komendy dzialaja przez hook zCConsole::Evaluate (AddEvalFunc ma 14 miejsc,
	// w Steam G2 zajete - wypchniety zostal oCGame::ConsoleEvalFunc i z gry zniknely 'insert' itd.).
	void ZConsoleRegister(const char* name, const char* help, ZCommandFn fn);

	// NPC, na ktorym dzialaja komendy: fokus gracza albo najblizszy w promieniu 20 m (takze martwy).
	oCNpc* ZConsoleTarget();
}
