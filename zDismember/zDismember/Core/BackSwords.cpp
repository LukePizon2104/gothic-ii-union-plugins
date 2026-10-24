// Logika bez silnika: miecze na plecach (BackSwords.h).
#include "BackSwords.h"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace zdcore {
	bool BackPlan::operator==(const BackPlan& o) const {
		return primarySpot == o.primarySpot && primaryNode == o.primaryNode && second == o.second && secondSpot == o.secondSpot &&
			hideCrossbow == o.hideCrossbow && draw1H == o.draw1H && draw2H == o.draw2H;
	}

	BackPlan PlanBack(const BackSettings& s, const BackState& st) {
		BackPlan p;
		bool w3 = s.on && st.overlays;
		// natywne 1H przy pasie: kopia z HUMANS w tempie BeltDrawSpeed (100 = pusty wpis, gra nakladka talentu / zEBA)
		int belt = s.on && st.overlays && s.beltSpeed != 100 ? DRAWV_BELT : DRAWV_NATIVE;
		if (!w3) {
			p.draw1H = DRAWV_NATIVE;
			p.draw2H = DRAWV_NATIVE;
			return p;
		}
		int back = st.primaryAtB ? SPOT_B : SPOT_A;
		int backDraw = st.primaryAtB ? DRAWV_B : DRAWV_A;
		if (st.dualPair) {
			// para NB: prawa w natywnym ZS_LONGSWORD, lewa w slocie tarczy, dobycie z nakladki NB (pusty wpis 2H); drugi
			// miecz moze wisiec (ZS_BOW), 1H jak przy broni 2H
			p.draw2H = DRAWV_NATIVE;
			p.draw1H = s.back1H == BACK1H_OFF ? belt : backDraw;
			p.secondSpot = SPOT_B;
			bool rangedP = st.bowEquipped || st.cbowEquipped;
			p.second = s.second && st.secondValid && (!rangedP || s.withRanged == SECOND_RANGED_REPLACE);
			p.hideCrossbow = p.second && st.cbowEquipped;
			return p;
		}
		bool oneOnBack = s.back1H == BACK1H_ALL || (s.back1H == BACK1H_SWORDS && st.primaryBlade);
		if (st.primaryType == BACK_TYPE_2H) {
			p.primarySpot = back;
			p.primaryNode = BNODE_LONGSWORD;
		}
		else if (st.primaryType == BACK_TYPE_1H && oneOnBack) {
			p.primarySpot = back;
			p.primaryNode = BNODE_SWORD;
		}
		// wariant dla typu zalozonej broni; drugi typ - jak na miejscu zalozonej (po zamianie i tak przeliczony)
		p.draw2H = backDraw;
		if (st.primaryType == BACK_TYPE_1H)
			p.draw1H = oneOnBack ? backDraw : belt;
		else
			p.draw1H = s.back1H == BACK1H_OFF ? belt : backDraw;
		// drugi miecz: wolne miejsce na plecach (przy broni przy pasie - B, jak srebro Geralta / w miejscu luku)
		p.secondSpot = p.primarySpot == SPOT_B ? SPOT_A : SPOT_B;
		bool ranged = st.bowEquipped || st.cbowEquipped;
		p.second = s.second && st.secondValid && (!ranged || s.withRanged == SECOND_RANGED_REPLACE);
		p.hideCrossbow = p.second && st.cbowEquipped;
		return p;
	}

	const char* BackSpotLabel(int spot) {
		switch (spot) {
		case SPOT_A: return "plecy A (stal)";
		case SPOT_B: return "plecy B (srebro)";
		default: return "natywnie";
		}
	}

	const char* DrawVariantLabel(int v) {
		switch (v) {
		case DRAWV_BELT: return "natywne z pasa (tempo BeltDrawSpeed)";
		case DRAWV_A: return "W3 zza plecow A";
		case DRAWV_B: return "W3 zza plecow B";
		default: return "natywne (gra / zEBA)";
		}
	}

	std::string BackPlanText(const BackPlan& p) {
		char buf[320];
		snprintf(buf, sizeof(buf), "bron %s%s, drugi miecz %s%s, dobycie 1H: %s, 2H: %s", BackSpotLabel(p.primarySpot),
			p.primaryNode == BNODE_SWORD ? " (wezel ZS_SWORD)" : p.primaryNode == BNODE_LONGSWORD ? " (wezel ZS_LONGSWORD)" : "",
			p.second ? BackSpotLabel(p.secondSpot) : "niewidoczny", p.hideCrossbow ? " (kusza ukryta)" : "",
			DrawVariantLabel(p.draw1H), DrawVariantLabel(p.draw2H));
		return buf;
	}

	bool PrimaryAtBAfterSwap(const BackPlan& before) {
		return before.secondSpot == SPOT_B;
	}

	// ---------- macierze

	BackMat BackIdentity() {
		BackMat r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = i == j ? 1.0f : 0.0f;
		return r;
	}

	BackMat BackMul(const BackMat& a, const BackMat& b) {
		BackMat r;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				float v = 0.0f;
				for (int k = 0; k < 3; k++)
					v += a.m[i][k] * b.m[k][j];
				if (j == 3)
					v += a.m[i][3];
				r.m[i][j] = v;
			}
		}
		return r;
	}

	BackMat BackInverse(const BackMat& a) {
		BackMat r;
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++)
				r.m[i][j] = a.m[j][i];
		for (int i = 0; i < 3; i++)
			r.m[i][3] = -(r.m[i][0] * a.m[0][3] + r.m[i][1] * a.m[1][3] + r.m[i][2] * a.m[2][3]);
		return r;
	}

	static BackMat RotAxis(int axis, float deg) {
		BackMat r = BackIdentity();
		float a = deg * 3.14159265358979f / 180.0f, c = cosf(a), s = sinf(a);
		int i = (axis + 1) % 3, j = (axis + 2) % 3;
		r.m[i][i] = c;
		r.m[i][j] = -s;
		r.m[j][i] = s;
		r.m[j][j] = c;
		return r;
	}

	BackMat BackAdjust(const BackMat& base, const float adj[6]) {
		BackMat t = BackIdentity();
		t.m[0][3] = adj[0];
		t.m[1][3] = adj[1];
		t.m[2][3] = adj[2];
		BackMat r = BackMul(BackMul(RotAxis(0, adj[3]), RotAxis(1, adj[4])), RotAxis(2, adj[5]));
		return BackMul(base, BackMul(t, r));
	}

	bool ParseFloats(const std::string& text, float* out, int count) {
		const char* p = text.c_str();
		float tmp[16];
		if (count > 16)
			return false;
		for (int i = 0; i < count; i++) {
			while (*p == ' ' || *p == '\t' || *p == ',')
				p++;
			char* end = nullptr;
			double v = strtod(p, &end);
			if (end == p)
				return false;
			tmp[i] = (float)v;
			p = end;
		}
		while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r' || *p == '\n')
			p++;
		if (*p && *p != ';')
			return false;
		for (int i = 0; i < count; i++)
			out[i] = tmp[i];
		return true;
	}

	bool ParseBackIni(const std::string& text, BackMat& a, BackMat& b, std::string& err) {
		bool hasA = false, hasB = false;
		size_t pos = 0;
		while (pos < text.size()) {
			size_t eol = text.find('\n', pos);
			std::string line = text.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
			pos = eol == std::string::npos ? text.size() : eol + 1;
			while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
				line.pop_back();
			size_t eq = line.find('=');
			if (line.empty() || line[0] == ';' || line[0] == '[' || eq == std::string::npos)
				continue;
			std::string key = line.substr(0, eq);
			for (auto& c : key)
				c = (char)toupper((unsigned char)c);
			BackMat* dst = key == "POSA" ? &a : key == "POSB" ? &b : nullptr;
			if (!dst)
				continue;
			float v[12];
			if (!ParseFloats(line.substr(eq + 1), v, 12)) {
				err = "zla macierz " + key;
				return false;
			}
			for (int i = 0; i < 3; i++)
				for (int j = 0; j < 4; j++)
					dst->m[i][j] = v[i * 4 + j];
			(dst == &a ? hasA : hasB) = true;
		}
		if (!hasA || !hasB) {
			err = !hasA ? "brak PosA" : "brak PosB";
			return false;
		}
		return true;
	}

	float BackMatDistance(const BackMat& a, const BackMat& b) {
		float dx = a.m[0][3] - b.m[0][3], dy = a.m[1][3] - b.m[1][3], dz = a.m[2][3] - b.m[2][3];
		return sqrtf(dx * dx + dy * dy + dz * dz);
	}

	bool BackMatFinite(const BackMat& a) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++) {
				float v = a.m[i][j];
				if (!(v == v) || v > 1e5f || v < -1e5f)
					return false;
			}
		return true;
	}

	float BackOffsetError(const BackMat& spineObj, const BackMat& nodeObj, const BackMat& want) {
		if (!BackMatFinite(spineObj) || !BackMatFinite(nodeObj) || !BackMatFinite(want))
			return -1.0f;
		// bez odwrotnosci (macierze wezlow moga miec skale - sciskanie odcietych czesci): punkt z planu w ukladzie modelu
		return BackMatDistance(BackMul(spineObj, want), nodeObj);
	}

	bool BackRenderDrift(float swordToSpine, float expected, float refNow, float refRest, float& drift) {
		drift = fabsf(swordToSpine - expected);
		if (!(swordToSpine == swordToSpine) || !(refNow == refNow))
			return false;
		if (fabsf(refNow - refRest) > BACK_DIAG_TOL)
			return false;   // wezly kontrolne tez rozjechane - macierze renderu nieaktualne, nie miecz
		return drift > BACK_DIAG_TOL;
	}

	// ---------- zapis

	uint32_t BackNameHash(const std::string& instance) {
		uint32_t h = 2166136261u;
		for (char c : instance) {
			h ^= (uint32_t)(unsigned char)toupper((unsigned char)c);
			h *= 16777619u;
		}
		h &= 0x7FFFFFFFu;
		return h ? h : 1u;
	}

	// ---------- zamiana

	void BackSwapStart(BackSwapRun& r) {
		r = BackSwapRun();
		r.phase = SWP_SHEATHE;
	}

	static BackSwapAction Phase(BackSwapRun& r, int phase) {
		r.phase = phase;
		r.phaseT = 0.0f;
		return SWA_NONE;
	}

	BackSwapAction BackSwapStep(BackSwapRun& r, const BackSwapTick& t) {
		if (r.phase == SWP_IDLE)
			return SWA_NONE;
		r.phaseT += t.dt;
		r.total += t.dt;
		if (t.interrupted) {
			r.why = "przerwana";
			r.phase = SWP_IDLE;
			return SWA_FAIL;
		}
		switch (r.phase) {
		case SWP_SHEATHE:
			if (!r.removeSent) {
				r.removeSent = true;
				return SWA_REMOVE;
			}
			if (t.weaponMode == 0 && !t.weaponMsg) {
				Phase(r, SWP_EQUIP);
				return SWA_EQUIP;
			}
			if (!t.weaponMsg && r.phaseT > BACK_SWAP_GRACE) {
				r.why = "chowanie przerwane (komunikat broni zniknal, bron w rece)";
				r.phase = SWP_IDLE;
				return SWA_FAIL;
			}
			if (r.phaseT > BACK_SWAP_PHASE_LIMIT) {
				r.why = "chowanie broni trwa za dlugo";
				r.phase = SWP_IDLE;
				return SWA_FAIL;
			}
			return SWA_NONE;
		case SWP_EQUIP:
			if (!t.newEquipped) {
				r.why = "gra nie zalozyla drugiej broni (wymagania / skrypt)";
				r.phase = SWP_IDLE;
				return SWA_FAIL_REDRAW;
			}
			Phase(r, SWP_DRAW);
			r.drawSent = true;
			return SWA_DRAW;
		case SWP_DRAW:
			if (t.weaponMode != 0 && !t.weaponMsg) {
				r.why = "gotowe";
				r.phase = SWP_IDLE;
				return SWA_DONE;
			}
			if (!t.weaponMsg && r.phaseT > BACK_SWAP_GRACE) {
				r.why = "dobycie przerwane (bron zostala schowana)";
				r.phase = SWP_IDLE;
				return SWA_DONE;
			}
			if (r.phaseT > BACK_SWAP_PHASE_LIMIT) {
				r.why = "dobycie trwa za dlugo";
				r.phase = SWP_IDLE;
				return SWA_DONE;
			}
			return SWA_NONE;
		default:
			r.phase = SWP_IDLE;
			return SWA_NONE;
		}
	}

	const char* BackSwapPhaseLabel(int phase) {
		switch (phase) {
		case SWP_SHEATHE: return "chowanie";
		case SWP_EQUIP: return "zakladanie";
		case SWP_DRAW: return "dobycie";
		default: return "-";
		}
	}

	std::string BackSwapRefusal(const BackSwapGate& g) {
		if (g.swapping)
			return "zamiana juz trwa";
		if (!g.secondValid)
			return "brak drugiego miecza (ekwipunek: bron biala + klawisz zamiany)";
		if (g.dead)
			return "bohater nie zyje / nieprzytomny";
		if (g.console)
			return "otwarta konsola";
		if (g.inventoryOpen)
			return "otwarty ekwipunek";
		if (g.dialog)
			return "rozmowa";
		if (g.mounted)
			return "na wierzchowcu";
		if (g.busy)
			return "trwa " + (g.busyWhat.empty() ? std::string("inna akcja") : g.busyWhat);
		if (g.weaponMsg)
			return "trwa dobywanie / chowanie broni";
		if (g.dualPair)
			return "bron parowa NB (para *_RIGHT / *_LEFT) - zamien w ekwipunku";
		if (g.attacking)
			return "trwa cios";
		return "";
	}

	// ---------- klawisz drugiego miecza

	SecondKeyEvent SecondKeyStep(SecondKeyState& s, const SecondKeyInput& in) {
		SecondKeyEvent ev;
		bool phys = in.physical && in.focus && !in.typing;
		bool down = in.engine || phys;
		if (!down) {
			if (s.held && !s.engineSeen) {
				ev.releasedUnseen = true;
				ev.heldFrames = s.frames;
			}
			s = SecondKeyState();
			return ev;
		}
		if (s.held) {
			s.frames++;
			s.engineSeen = s.engineSeen || in.engine;
			return ev;
		}
		s.held = true;
		s.frames = 1;
		s.engineSeen = in.engine;
		ev.pressed = in.engine ? SKS_ENGINE : SKS_PHYSICAL;
		return ev;
	}

	const char* SecondKeySourceLabel(int source) {
		switch (source) {
		case SKS_ENGINE: return "silnik (zinput)";
		case SKS_PHYSICAL: return "fizyczny (GetAsyncKeyState)";
		default: return "-";
		}
	}

	const std::vector<int>& BackSpeedMenuValues() {
		static const std::vector<int> v = { 50, 75, 100, 125, 150, 175, 200 };
		return v;
	}

	const std::vector<int>& BeltSpeedMenuValues() {
		static const std::vector<int> v = { 100, 125, 150, 175, 200, 250 };
		return v;
	}

	const std::vector<int>& SecondKeyMenuValues() {
		static const std::vector<int> v = { 34, 48, 21, 19, 0 };   // G B Y R wyl.
		return v;
	}
}
