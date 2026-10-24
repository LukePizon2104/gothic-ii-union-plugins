// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// args[0] = nazwa podkomendy (wielkimi literami), dalej argumenty.
	typedef std::function<zSTRING(const std::vector<std::string>& args)> TrCommandFn;

	// Rejestruje "tr <name>". Komendy dzialaja przez hook zCConsole::Evaluate (AddEvalFunc w Steam G2 zajete).
	void TrConsoleRegister(const char* name, const char* help, TrCommandFn fn);

	// NPC, na ktorym dzialaja komendy: fokus gracza albo najblizszy w promieniu 20 m (takze martwy).
	oCNpc* TrConsoleTarget();
}
