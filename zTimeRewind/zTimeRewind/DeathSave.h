// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Auto-cofniecie przy smierci bohatera (AutoRewindOnDeath): hook oCNpc::DoDie - smierc bohatera z runa
	// w plecaku i mana na start zamienia sie w samo cofniecie o AutoRewindSeconds.
	bool DeathSaveTry();
}
