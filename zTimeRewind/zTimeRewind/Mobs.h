// Supported with union (c) 2020 Union team
// Union HEADER file

namespace GOTHIC_ENGINE {
	// Cofa zmiany stanu drzwi i skrzyn nowsze niz t (stan sprzed zdarzenia + animacja S_S<stan>).
	void MobsRevertAfter(float t);
	// Usuwa zdarzenia starsze niz bufor.
	void MobsTrim(float oldest);
	void MobsReset();
}
