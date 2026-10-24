// Logika bez silnika: kursor czasu w trybie przewijania (predkosc, granice bufora, mana).
#pragma once

namespace trcore {
	enum class StopReason { Running, Released, BufferEnd, NoMana };   // bez "None" - makro Union

	struct CursorParams {
		float speed = 3.0f;           // sekundy cofniete na sekunde rzeczywista
		float manaPerSecond = 5.0f;   // mana za sekunde cofnietego czasu
		float minSeconds = 3.0f;      // puszczenie wczesniej = cofanie dalej do tej wartosci
	};

	// Auto-cofniecie przy smierci bohatera: opcja wlaczona, runa w plecaku, nic juz nie przewija,
	// w buforze co najmniej 1 s i mana na koszt rzucenia. Inaczej bohater ginie normalnie.
	bool ShouldAutoRewind(bool enabled, bool hasRune, bool busy, float bufferSeconds, int mana, int manaStart);

	class RewindCursor {
	public:
		void Begin(float now, float oldest, const CursorParams& params);
		// Krok o realDt sekund czasu rzeczywistego. Zwraca, ile pelnych punktow many pobrac teraz
		// (nigdy wiecej niz manaAvailable); po zatrzymaniu zwraca 0 i nie rusza kursora.
		int Step(float realDt, int manaAvailable);
		void Release() { released = true; }
		float Time() const { return t; }
		float Rewound() const { return start - t; }
		bool Stopped() const { return reason != StopReason::Running; }
		StopReason Reason() const { return reason; }

	private:
		CursorParams p;
		float start = 0.0f, t = 0.0f, oldest = 0.0f;
		float manaFrac = 0.0f;        // niezaplacona czesc punktu many
		bool released = false;
		StopReason reason = StopReason::Running;
	};
}
