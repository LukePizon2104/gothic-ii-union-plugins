#include "Ragdoll.h"
#include <algorithm>

namespace zdcore {
	static void AddLink(RagdollState& s, int a, int b, bool minOnly = false, float rest = -1.0f) {
		RagLink l;
		l.a = a;
		l.b = b;
		l.minOnly = minOnly;
		l.rest = rest >= 0.0f ? rest : Length(s.p[a].pos - s.p[b].pos);
		s.links.push_back(l);
	}

	void RagdollInit(RagdollState& s, const std::vector<Vec3>& pos, const Vec3& impulse) {
		s = RagdollState();
		s.p.resize(RAG_COUNT);
		for (int i = 0; i < RAG_COUNT; i++) {
			s.p[i].pos = s.p[i].prev = pos[i];
			s.p[i].radius = i == RAG_HEAD ? 10.0f : (i == RAG_PELVIS || i == RAG_CHEST) ? 12.0f
				: (i == RAG_L_HAND || i == RAG_R_HAND) ? 4.0f : (i == RAG_L_FOOT || i == RAG_R_FOOT) ? 5.0f : 6.0f;
		}
		// kosci
		const int bones[][2] = {
			{ RAG_PELVIS, RAG_CHEST }, { RAG_CHEST, RAG_HEAD },
			{ RAG_CHEST, RAG_L_SHOULDER }, { RAG_L_SHOULDER, RAG_L_ELBOW }, { RAG_L_ELBOW, RAG_L_HAND },
			{ RAG_CHEST, RAG_R_SHOULDER }, { RAG_R_SHOULDER, RAG_R_ELBOW }, { RAG_R_ELBOW, RAG_R_HAND },
			{ RAG_PELVIS, RAG_L_HIP }, { RAG_L_HIP, RAG_L_KNEE }, { RAG_L_KNEE, RAG_L_FOOT },
			{ RAG_PELVIS, RAG_R_HIP }, { RAG_R_HIP, RAG_R_KNEE }, { RAG_R_KNEE, RAG_R_FOOT },
		};
		for (auto& b : bones)
			AddLink(s, b[0], b[1]);
		// sztywny tulow (barki, biodra, miednica, klatka) i glowa trzymana przez barki
		const int brace[][2] = {
			{ RAG_L_SHOULDER, RAG_R_SHOULDER }, { RAG_L_HIP, RAG_R_HIP },
			{ RAG_CHEST, RAG_L_HIP }, { RAG_CHEST, RAG_R_HIP }, { RAG_PELVIS, RAG_L_SHOULDER }, { RAG_PELVIS, RAG_R_SHOULDER },
			{ RAG_L_SHOULDER, RAG_L_HIP }, { RAG_R_SHOULDER, RAG_R_HIP },
			{ RAG_HEAD, RAG_L_SHOULDER }, { RAG_HEAD, RAG_R_SHOULDER },
		};
		for (auto& b : brace)
			AddLink(s, b[0], b[1]);
		// lokcie i kolana nie skladaja sie do zera, glowa nie wchodzi w miednice
		auto len = [&](int a, int b) { return Length(pos[a] - pos[b]); };
		AddLink(s, RAG_L_SHOULDER, RAG_L_HAND, true, 0.5f * (len(RAG_L_SHOULDER, RAG_L_ELBOW) + len(RAG_L_ELBOW, RAG_L_HAND)));
		AddLink(s, RAG_R_SHOULDER, RAG_R_HAND, true, 0.5f * (len(RAG_R_SHOULDER, RAG_R_ELBOW) + len(RAG_R_ELBOW, RAG_R_HAND)));
		AddLink(s, RAG_L_HIP, RAG_L_FOOT, true, 0.55f * (len(RAG_L_HIP, RAG_L_KNEE) + len(RAG_L_KNEE, RAG_L_FOOT)));
		AddLink(s, RAG_R_HIP, RAG_R_FOOT, true, 0.55f * (len(RAG_R_HIP, RAG_R_KNEE) + len(RAG_R_KNEE, RAG_R_FOOT)));
		AddLink(s, RAG_HEAD, RAG_PELVIS, true, 0.9f * len(RAG_HEAD, RAG_PELVIS));
		// pchniecie: gora ciala dostaje pelny impuls, nogi mniej (cialo przewraca sie w strone ciosu)
		float y0 = pos[RAG_PELVIS].y, y1 = std::max(pos[RAG_HEAD].y, y0 + 1.0f);
		s.startVel.resize(RAG_COUNT);
		for (int i = 0; i < RAG_COUNT; i++) {
			float h = std::min(1.0f, std::max(0.0f, (pos[i].y - y0) / (y1 - y0)));
			s.startVel[i] = impulse * (0.25f + 0.75f * h);
		}
	}

	void RagdollStep(RagdollState& s, float dt, const TraceFn& trace, const RagdollParams& prm) {
		if (s.resting || dt <= 0.0f || s.p.size() != RAG_COUNT)
			return;
		if (s.age == 0.0f)
			for (int i = 0; i < RAG_COUNT; i++)
				s.p[i].prev = s.p[i].pos - s.startVel[i] * dt;
		s.age += dt;
		for (auto& q : s.p) {
			Vec3 v = (q.pos - q.prev) * prm.damping;
			q.prev = q.pos;
			q.pos += v + Vec3(0.0f, -prm.gravity * dt * dt, 0.0f);
		}
		// ziemia pod kazdym punktem raz na krok
		float ground[RAG_COUNT];
		for (int i = 0; i < RAG_COUNT; i++) {
			Vec3 hit, n;
			Vec3 from(s.p[i].pos.x, s.p[i].pos.y + 60.0f, s.p[i].pos.z);
			ground[i] = trace(from, Vec3(0.0f, -260.0f, 0.0f), hit, n) ? hit.y : -1e9f;
		}
		for (int it = 0; it < prm.iterations; it++) {
			for (auto& l : s.links) {
				Vec3 d = s.p[l.b].pos - s.p[l.a].pos;
				float len = Length(d);
				if (len < 1e-4f || (l.minOnly && len >= l.rest))
					continue;
				Vec3 c = d * (0.5f * (len - l.rest) / len);
				s.p[l.a].pos += c;
				s.p[l.b].pos = s.p[l.b].pos - c;
			}
			for (int i = 0; i < RAG_COUNT; i++) {
				RagParticle& q = s.p[i];
				float floor = ground[i] + q.radius;
				if (q.pos.y < floor) {
					q.pos.y = floor;
					q.prev.x += (q.pos.x - q.prev.x) * prm.friction;
					q.prev.z += (q.pos.z - q.prev.z) * prm.friction;
					if (q.prev.y < q.pos.y - 0.0f && it == prm.iterations - 1)
						q.prev.y = q.pos.y;   // bez odbicia
				}
			}
		}
		float maxSpeed = 0.0f;
		for (auto& q : s.p)
			maxSpeed = std::max(maxSpeed, Length(q.pos - q.prev) / dt);
		s.calm = maxSpeed < prm.restSpeed ? s.calm + dt : 0.0f;
		if (s.calm > prm.calmTime || s.age > prm.maxAge)
			s.resting = true;
	}

	Mat34 FrameFromAxes(const Vec3& up, const Vec3& xHint) {
		Vec3 y = Normalize(up);
		Vec3 x = xHint - y * Dot(xHint, y);
		if (Length(x) < 1e-4f)
			x = std::fabs(y.x) < 0.9f ? Vec3(1, 0, 0) - y * y.x : Vec3(0, 0, 1) - y * y.z;
		x = Normalize(x);
		Vec3 z = Cross(x, y);
		Mat34 f = Mat34::Identity();
		f.m[0][0] = x.x; f.m[1][0] = x.y; f.m[2][0] = x.z;
		f.m[0][1] = y.x; f.m[1][1] = y.y; f.m[2][1] = y.z;
		f.m[0][2] = z.x; f.m[1][2] = z.y; f.m[2][2] = z.z;
		return f;
	}

	Mat34 RotationBetween(const Vec3& aIn, const Vec3& bIn) {
		Vec3 a = Normalize(aIn), b = Normalize(bIn);
		float c = Dot(a, b);
		Mat34 r = Mat34::Identity();
		if (c > 0.99999f)
			return r;
		Vec3 axis = Cross(a, b);
		if (c < -0.99999f || Length(axis) < 1e-6f)
			axis = std::fabs(a.x) < 0.9f ? Cross(a, Vec3(1, 0, 0)) : Cross(a, Vec3(0, 1, 0));
		float deg = std::acos(std::max(-1.0f, std::min(1.0f, c))) * 180.0f / 3.14159265f;
		RotateAround(r, Vec3(), axis, deg);
		return r;
	}

	Mat34 Mul(const Mat34& a, const Mat34& b) {
		Mat34 r;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				float v = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
				r.m[i][j] = j == 3 ? v + a.m[i][3] : v;
			}
		}
		return r;
	}

	Mat34 Transposed3(const Mat34& a) {
		Mat34 r = Mat34::Identity();
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				r.m[i][j] = a.m[j][i];
		return r;
	}

	Mat34 InverseRigid(const Mat34& a) {
		Mat34 r = Transposed3(a);
		Vec3 t = Rotate(r, a.Translation());
		r.m[0][3] = -t.x;
		r.m[1][3] = -t.y;
		r.m[2][3] = -t.z;
		return r;
	}

	static Mat34 RotPart(const Mat34& m) {
		Mat34 r = m;
		r.m[0][3] = r.m[1][3] = r.m[2][3] = 0.0f;
		return r;
	}

	static void TorsoFrames(const std::vector<Vec3>& p, Mat34& pelvisF, Mat34& chestF) {
		pelvisF = FrameFromAxes(p[RAG_CHEST] - p[RAG_PELVIS], p[RAG_L_HIP] - p[RAG_R_HIP]);
		chestF = FrameFromAxes((p[RAG_CHEST] - p[RAG_PELVIS]) + (p[RAG_HEAD] - p[RAG_CHEST]), p[RAG_L_SHOULDER] - p[RAG_R_SHOULDER]);
	}

	static std::vector<Vec3> ToObj(const std::vector<Vec3>& world, const Mat34& objToWorld) {
		Mat34 inv = InverseRigid(objToWorld);
		std::vector<Vec3> out(world.size());
		for (size_t i = 0; i < world.size(); i++)
			out[i] = Transform(inv, world[i]);
		return out;
	}

	RagPoseRig BuildRagPoseRig(const std::vector<int>& parent, const std::vector<Mat34>& startLocal, const std::vector<int>& role,
		const std::vector<int>& from, const std::vector<int>& to, const std::vector<Vec3>& particlesWorld, const Mat34& objToWorld) {
		RagPoseRig r;
		size_t n = parent.size();
		r.parent = parent;
		r.role = role;
		r.from = from;
		r.to = to;
		r.startLocal = startLocal;
		r.startObj.resize(n);
		std::vector<char> done(n, 0);
		for (bool progress = true; progress;) {
			progress = false;
			for (size_t i = 0; i < n; i++)
				if (!done[i] && (parent[i] < 0 || done[parent[i]])) {
					r.startObj[i] = parent[i] < 0 ? startLocal[i] : Mul(r.startObj[parent[i]], startLocal[i]);
					done[i] = 1;
					r.order.push_back((int)i);
					progress = true;
				}
		}
		std::vector<Vec3> p = ToObj(particlesWorld, objToWorld);
		TorsoFrames(p, r.pelvisF0, r.chestF0);
		r.dir0.assign(n, Vec3());
		for (size_t i = 0; i < n; i++)
			if (role[i] == RAG_ROLE_LIMB && from[i] >= 0 && to[i] >= 0)
				r.dir0[i] = p[to[i]] - p[from[i]];
		return r;
	}

	void RagdollPose(const RagPoseRig& rig, const std::vector<Vec3>& particlesWorld, const Mat34& objToWorld, std::vector<Mat34>& locals) {
		size_t n = rig.parent.size();
		locals.resize(n);
		std::vector<Vec3> p = ToObj(particlesWorld, objToWorld);
		Mat34 pelvisF, chestF;
		TorsoFrames(p, pelvisF, chestF);
		Mat34 dPelvis = Mul(pelvisF, Transposed3(rig.pelvisF0)), dChest = Mul(chestF, Transposed3(rig.chestF0));
		std::vector<Mat34> obj(n);
		for (int i : rig.order) {
			int par = rig.parent[i];
			Mat34 parentObj = par >= 0 ? obj[par] : Mat34::Identity();
			Mat34 o;
			if (rig.role[i] == RAG_ROLE_RIGID)
				o = Mul(parentObj, rig.startLocal[i]);
			else {
				Mat34 delta;
				if (rig.role[i] == RAG_ROLE_ROOT || rig.role[i] == RAG_ROLE_PELVIS)
					delta = dPelvis;
				else if (rig.role[i] == RAG_ROLE_CHEST)
					delta = dChest;
				else {
					// skret od rodzica, potem zgiecie wzdluz odcinka
					Mat34 rp = par >= 0 ? Mul(RotPart(parentObj), Transposed3(rig.startObj[par])) : Mat34::Identity();
					delta = Mul(RotationBetween(Rotate(rp, rig.dir0[i]), p[rig.to[i]] - p[rig.from[i]]), rp);
				}
				o = Mul(delta, RotPart(rig.startObj[i]));
				Vec3 t = rig.role[i] == RAG_ROLE_ROOT ? p[RAG_PELVIS] : Transform(parentObj, rig.startLocal[i].Translation());
				o.m[0][3] = t.x;
				o.m[1][3] = t.y;
				o.m[2][3] = t.z;
			}
			obj[i] = o;
			locals[i] = par >= 0 ? Mul(InverseRigid(parentObj), o) : o;
		}
	}
}
