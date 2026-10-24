// Logika bez silnika: ragdoll zwlok - 15 punktow szkieletu, wiezy dlugosci kosci, usztywnienie tulowia, limity zgiec,
// calkowanie Verleta z grawitacja, ziemia z TraceFn i tarciem. Plugin obraca kosci modelu wzdluz odcinkow.
#pragma once
#include <vector>
#include "ZMath.h"
#include "Ballistics.h"

namespace zdcore {
	enum RagNode {
		RAG_PELVIS, RAG_CHEST, RAG_HEAD,
		RAG_L_SHOULDER, RAG_L_ELBOW, RAG_L_HAND, RAG_R_SHOULDER, RAG_R_ELBOW, RAG_R_HAND,
		RAG_L_HIP, RAG_L_KNEE, RAG_L_FOOT, RAG_R_HIP, RAG_R_KNEE, RAG_R_FOOT,
		RAG_COUNT
	};

	struct RagParticle {
		Vec3 pos, prev;
		float radius = 6.0f;
	};

	struct RagLink {
		int a = 0, b = 0;
		float rest = 0.0f;
		bool minOnly = false;   // limit zgiecia: tylko rozpycha ponizej 'rest'
	};

	struct RagdollParams {
		float gravity = 981.0f;
		float damping = 0.995f;      // zachowana predkosc na krok
		int iterations = 12;
		float friction = 0.7f;       // utrata predkosci poziomej przy kontakcie z ziemia (0..1)
		float restSpeed = 12.0f;     // cm/s - ponizej przez calmTime = spoczynek
		float calmTime = 0.4f;
		float maxAge = 5.0f;
	};

	struct RagdollState {
		std::vector<RagParticle> p;
		std::vector<RagLink> links;
		std::vector<Vec3> startVel;
		float age = 0.0f, calm = 0.0f;
		bool resting = false;
	};

	// pos: RAG_COUNT punktow w swiecie (cm); impulse: predkosc gornej czesci ciala po ciosie (cm/s).
	void RagdollInit(RagdollState& s, const std::vector<Vec3>& pos, const Vec3& impulse);
	void RagdollStep(RagdollState& s, float dt, const TraceFn& trace, const RagdollParams& prm = RagdollParams());

	// Odtwarzanie pozy modelu z punktow: role wezlow (sztywny = jak na starcie wzgledem rodzica; korzen/miednica/klatka
	// = obrot ukladu tulowia; konczyna = skret od rodzica + zgiecie wzdluz odcinka from->to).
	enum RagRole { RAG_ROLE_RIGID, RAG_ROLE_ROOT, RAG_ROLE_PELVIS, RAG_ROLE_CHEST, RAG_ROLE_LIMB };

	struct RagPoseRig {
		std::vector<int> order, parent, role, from, to;
		std::vector<Mat34> startLocal, startObj;
		std::vector<Vec3> dir0;                 // kierunek konczyny na starcie (uklad modelu)
		Mat34 pelvisF0, chestF0;
	};

	// particlesWorld: RAG_COUNT punktow na starcie; objToWorld: uklad modelu -> swiat.
	RagPoseRig BuildRagPoseRig(const std::vector<int>& parent, const std::vector<Mat34>& startLocal, const std::vector<int>& role,
		const std::vector<int>& from, const std::vector<int>& to, const std::vector<Vec3>& particlesWorld, const Mat34& objToWorld);
	// Lokalne macierze wezlow (wzgledem rodzica) dla aktualnych punktow.
	void RagdollPose(const RagPoseRig& rig, const std::vector<Vec3>& particlesWorld, const Mat34& objToWorld, std::vector<Mat34>& locals);

	// Uklad ortonormalny: Y wzdluz 'up', X jak najblizej 'xHint' (kolumny macierzy = osie, bez przesuniecia).
	Mat34 FrameFromAxes(const Vec3& up, const Vec3& xHint);
	// Najkrotszy obrot przenoszacy kierunek a na b.
	Mat34 RotationBetween(const Vec3& a, const Vec3& b);
	Mat34 Mul(const Mat34& a, const Mat34& b);        // a * b (afiniczne 3x4)
	Mat34 Transposed3(const Mat34& a);                // odwrotnosc samego obrotu
	Mat34 InverseRigid(const Mat34& a);               // odwrotnosc obrotu z przesunieciem
}
