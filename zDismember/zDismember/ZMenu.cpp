// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	// Okno listy pluginu (opis w ZMenu.h).
	struct ZMenuQueued {
		zdcore::MenuKey key;
		int dik;
	};
	static bool zmCapture = false;
	static std::string zmWho;
	static std::vector<ZMenuQueued> zmQueue;
	static const size_t ZM_QUEUE_MAX = 32;
	static zdcore::MenuRepeat zmRepeat;
	static int zmRepeatDik = 0;
	static zCView* zmView = nullptr;
	static std::string zmViewBack;
	static DWORD zmLoopTick = 0;   // ostatni ZMenuInputUpdate (petla gry)

	static const zCOLOR ZM_TITLE(255, 200, 120, 255);
	static const zCOLOR ZM_HEADER(200, 200, 200, 255);
	static const zCOLOR ZM_ROW(230, 230, 230, 255);
	static const zCOLOR ZM_SELECTED(255, 220, 90, 255);
	static const zCOLOR ZM_SELECTED_DIM(190, 170, 110, 255);
	static const zCOLOR ZM_DISABLED(120, 120, 120, 255);
	static const zCOLOR ZM_HINT(170, 170, 170, 255);

	// ---------- mysz ----------
	struct ZMouseQueued {
		zdcore::MenuMouseKind kind;
		float px, py;
	};
	static bool zmMouseOn = false;          // wlasciciel chce kursora
	static bool zmMouseOrbit = false;       // PPM przeciagany = ruch dla gry (kamera medytacji)
	static bool zmCurInit = false;
	static float zmCurX = 0.0f, zmCurY = 0.0f;   // piksele ekranu
	static bool zmLmbPrev = false;
	static bool zmRmbDown = false, zmRmbDrag = false;
	static float zmRmbMoved = 0.0f;
	static DWORD zmRmbStart = 0;
	static std::vector<ZMouseQueued> zmMouseQ;
	static zdcore::MenuHitMap zmHit;
	static ZMenuLayout zmHitLay;
	static zCView* zmCursorView = nullptr;
	static const float ZM_DRAG_UNITS = 6.0f;      // ruch myszy z PPM ponad tyle = przeciaganie (kamera), nie klik
	static const DWORD ZM_CLICK_MS = 450;         // dluzsze trzymanie PPM bez ruchu - tez nie klik
	static const char* const ZM_CURSOR_TEX = "LO.TGA";   // strzalka z mapy gry (jak kursor ItemMap)
	static const int ZM_CURSOR_PX = 26;

	static bool ZmButton(int vk) {
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	}

	static void ZmMousePush(zdcore::MenuMouseKind k) {
		if (zmMouseQ.size() < 64)
			zmMouseQ.push_back({ k, zmCurX, zmCurY });
	}

	static float ZmScreenW() {
		return screen ? (float)std::max(1, screen->nax(8192)) : 1920.0f;
	}

	static float ZmScreenH() {
		return screen ? (float)std::max(1, screen->nay(8192)) : 1080.0f;
	}

	void ZMenuMouse(bool cursor, bool orbitRmb) {
		cursor = cursor && zcfg.menuMouse && zmCapture;
		if (cursor && !zmMouseOn) {
			if (!zmCurInit) {
				// srodek ostatniego okna (albo ekranu)
				float sx = zmHit.valid ? (zmHitLay.x + zmHitLay.w / 2) / 8192.0f : 0.5f;
				float sy = zmHit.valid ? (zmHitLay.y + zmHitLay.h / 3) / 8192.0f : 0.5f;
				zmCurX = sx * ZmScreenW();
				zmCurY = sy * ZmScreenH();
				zmCurInit = true;
			}
			zmLmbPrev = ZmButton(VK_LBUTTON);   // przycisk trzymany przy otwarciu nie klika
			zmRmbDown = zmRmbDrag = false;
			zmMouseQ.clear();
			ZDebug("Okno: mysz - kursor wlaczony (%s), PPM z ruchem = kamera: %d", zmWho.c_str(), orbitRmb);
		}
		if (!cursor && zmMouseOn) {
			zmMouseQ.clear();
			zmRmbDown = zmRmbDrag = false;
		}
		zmMouseOn = cursor;
		zmMouseOrbit = orbitRmb;
	}

	bool ZMenuMouseOrbiting() {
		return zmMouseOn && zmRmbDrag;
	}

	bool ZMenuNextMouse(zdcore::MenuMouseKind& kind, int& wx, int& wy) {
		if (zmMouseQ.empty())
			return false;
		ZMouseQueued q = zmMouseQ.front();
		zmMouseQ.erase(zmMouseQ.begin());
		kind = q.kind;
		float sx = q.px * 8192.0f / ZmScreenW(), sy = q.py * 8192.0f / ZmScreenH();
		wx = zmHitLay.w > 0 ? (int)((sx - zmHitLay.x) * 8192.0f / zmHitLay.w) : -1;
		wy = zmHitLay.h > 0 ? (int)((sy - zmHitLay.y) * 8192.0f / zmHitLay.h) : -1;
		return true;
	}

	const zdcore::MenuHitMap& ZMenuHitMap() {
		return zmHit;
	}

	// Krok myszy w haku ProcessInputEvents (po oryginale - delty tej klatki gotowe): kursor albo kamera (PPM z ruchem),
	// przyciski z fizycznego stanu, kolko z delty Z. Delty X / Y / Z silnika zerowane, gdy idzie do kursora.
	static void ZmMouseStep(zCInput_Win32* in) {
		if (!zmCapture || !zmMouseOn || !in)
			return;
		if (zmLoopTick && GetTickCount() - zmLoopTick > 2000)
			return;   // petla gry stoi (menu gry, wczytywanie) - mysz dla gry
		float dx = 0.0f, dy = 0.0f, dz = 0.0f;
		in->GetMousePos(dx, dy, dz);
		bool lmb = ZmButton(VK_LBUTTON), rmb = ZmButton(VK_RBUTTON);
		DWORD now = GetTickCount();
		if (rmb && !zmRmbDown) {
			zmRmbDown = true;
			zmRmbDrag = false;
			zmRmbMoved = 0.0f;
			zmRmbStart = now;
		}
		if (zmRmbDown && rmb) {
			zmRmbMoved += fabsf(dx) + fabsf(dy);
			if (zmMouseOrbit && zmRmbMoved > ZM_DRAG_UNITS)
				zmRmbDrag = true;
		}
		if (zmRmbDown && !rmb) {
			if (!zmRmbDrag && now - zmRmbStart < ZM_CLICK_MS && zmRmbMoved <= ZM_DRAG_UNITS * 4.0f)
				ZmMousePush(zdcore::MMOUSE_RIGHT);
			zmRmbDown = zmRmbDrag = false;
		}
		if (!zmRmbDrag) {
			float ox = zmCurX, oy = zmCurY;
			zmCurX = std::clamp(zmCurX + dx * zcfg.menuMouseSpeed, 0.0f, ZmScreenW() - 1.0f);
			zmCurY = std::clamp(zmCurY + dy * zcfg.menuMouseSpeed, 0.0f, ZmScreenH() - 1.0f);
			if (zmCurX != ox || zmCurY != oy)
				ZmMousePush(zdcore::MMOUSE_MOVE);
			*(int*)0x008D165C = 0;   // ruch nie trafia do kamery ani do obrotu bohatera
			*(int*)0x008D1660 = 0;
		}
		if (dz != 0.0f) {
			int notches = std::max(1, std::min(5, (int)(fabsf(dz) / 120.0f + 0.5f)));
			for (int i = 0; i < notches; i++)
				ZmMousePush(dz > 0.0f ? zdcore::MMOUSE_WHEEL_UP : zdcore::MMOUSE_WHEEL_DOWN);
			*(int*)0x008D1664 = 0;
		}
		if (lmb && !zmLmbPrev)
			ZmMousePush(zdcore::MMOUSE_LEFT);
		zmLmbPrev = lmb;
	}

	void ZMenuCapture(bool on, const char* who) {
		if (on == zmCapture)
			return;
		zmCapture = on;
		if (!on) {
			zmMouseOn = false;
			zmMouseQ.clear();
			zmRmbDown = zmRmbDrag = false;
		}
		zmQueue.clear();
		zmRepeat = zdcore::MenuRepeat();
		zmRepeatDik = 0;
		zmWho = who ? who : "";
		ZDebug("Okno: klawiatura %s (%s)", on ? "przejeta - gra nie widzi wcisniec" : "oddana grze", zmWho.c_str());
	}

	bool ZMenuCapturing() {
		return zmCapture;
	}

	bool ZMenuKeyEvent(unsigned short dik, int state) {
		if (!zmCapture)
			return false;
		zdcore::MenuKey k = zdcore::MenuKeyFromDik(dik);
		if (!state) {
			zdcore::MenuRepeatRelease(zmRepeat, k);
			return false;   // puszczenie dla silnika (klawisz trzymany przed otwarciem okna)
		}
		if (zmQueue.size() < ZM_QUEUE_MAX)
			zmQueue.push_back({ k, (int)dik });
		if (zdcore::MenuKeyRepeats(k)) {
			zdcore::MenuRepeatPress(zmRepeat, k);
			zmRepeatDik = dik;
		}
		return true;
	}

	bool ZMenuNextKey(zdcore::MenuKey& key, int& dik) {
		if (zmQueue.empty())
			return false;
		key = zmQueue.front().key;
		dik = zmQueue.front().dik;
		zmQueue.erase(zmQueue.begin());
		return true;
	}

	void ZMenuClearKeys() {
		zmQueue.clear();
	}

	// Klawisz fizycznie trzymany (kod DIK -> skan -> VK; rozszerzone 0x80+ = prefiks E0, np. strzalki).
	static bool ZmPhysicallyDown(int dik) {
		if (dik <= 0 || dik >= 0x100)
			return false;
		UINT sc = (dik & 0x80) ? (0xE000u | (UINT)(dik & 0x7F)) : (UINT)dik;
		UINT vk = MapVirtualKeyA(sc, MAPVK_VSC_TO_VK_EX);
		return vk != 0 && (GetAsyncKeyState((int)vk) & 0x8000) != 0;
	}

	void ZMenuInputUpdate(float realDt) {
		zmLoopTick = GetTickCount();
		if (!zmCapture)
			return;
		if (zmRepeat.key != zdcore::MKEY_NONE && !ZmPhysicallyDown(zmRepeatDik))
			zdcore::MenuRepeatRelease(zmRepeat, zmRepeat.key);
		int n = zdcore::MenuRepeatStep(zmRepeat, realDt);
		for (int i = 0; i < n && zmQueue.size() < ZM_QUEUE_MAX; i++)
			zmQueue.push_back({ zmRepeat.key, zmRepeatDik });
	}

	// ---------- klawiatura silnika (G2A) ----------
	// zCInput_Win32::ProcessInputEvents 0x4D5700 (wirtualna, raz na klatke): zeruje stany klawiszy logicznych
	// wstrzykniete przez SetState i skokiem w ogonie (0x4D3A90) czyta bufor DirectInput klawiatury (do 16 zdarzen):
	// keyevent[dik] = wcisniety (bajty od 0x8D1678), przy wcisnieciu keytoggle[dik] = swieze (0x8D18B8) i kod na koniec
	// bufora klawiszy (zCArray<int> 0x8D1D50 - z niego GetKey -> zCInputCallback::GetInput -> HandleEvent gry, okien
	// i konsoli). Po oryginale nowe wpisy bufora (od dlugosci sprzed wywolania) przechodza przez filtr pluginu;
	// polkniety = usuniety z bufora, keyevent i keytoggle wyzerowane (KeyPressed / KeyToggled / GetState go nie widza).
	// Puszczenie fizyczne zapisuje potem keyevent = 0 - bez zmiany.
	struct ZmKeyBuffer {
		int* data;
		int numAlloc;
		int num;
	};
	static const int ZM_KEY_CODES = 0x240;   // keyevent / keytoggle: klawiatura 0..0xFF i przyciski myszy 0x20x

	// Wcisniecie z bufora: okno pluginu otwarte - do jego kolejki, inaczej klawisze otwierajace okna (medytacja).
	// Bezpiecznik: okno otwarte, a petla gry stoi (menu gry, ekran wczytywania) dluzej niz ZM_STALE_MS - klawisze dla
	// gry, zeby blad pluginu nie odebral klawiatury.
	static const DWORD ZM_STALE_MS = 2000;
	static bool ZmFilterPress(int code) {
		if (code < 0 || code >= 0x10000)
			return false;
		if (WheelBindPress(code))   // przypisanie klawisza kola (menu Union / zdism wheel bind) - tez w menu gry
			return true;
		if (zmCapture) {
			if (zmLoopTick && GetTickCount() - zmLoopTick > ZM_STALE_MS) {
				static bool warned = false;
				if (!warned) {
					warned = true;
					ZLog("Okno: klawiatura przejeta (%s), a petla gry stoi ponad %u ms - klawisze dla gry", zmWho.c_str(),
						(unsigned)ZM_STALE_MS);
				}
				return false;
			}
			return ZMenuKeyEvent((unsigned short)code, 1);
		}
		if (WheelFilterPress(code))   // menu kolowe: wcisniecie Tab uzbrojone (stukniecie wraca do gry w WheelInputStep)
			return true;
		if (PathMenuKeyEvent((unsigned short)code))   // okno gildii i sciezek NB (PathMenuKey)
			return true;
		return MeditateKeyEvent((unsigned short)code, 1);
	}

	static void __fastcall ZmProcessInput(zCInput_Win32* self, void* edx);
	static CInvoke<void(__fastcall*)(zCInput_Win32*, void*)> Ivk_zCInput_Win32_ProcessInputEvents(
		ENGINE == Engine_G2A ? 0x004D5700 : 0, &ZmProcessInput, ENGINE == Engine_G2A ? IvkEnabled(ENGINE) : IVK_DISABLED);
	static void __fastcall ZmProcessInput(zCInput_Win32* self, void* edx) {
		ZHookHit(ZH_INPUT_PROCESS);
		ZmKeyBuffer* buf = (ZmKeyBuffer*)0x008D1D50;
		int before = buf->num;
		Ivk_zCInput_Win32_ProcessInputEvents(self, edx);
		if (ENGINE != Engine_G2A || !zcfg.enabled)
			return;
		ZmMouseStep(self);   // kursor okna: delty myszy tej klatki (przed filtrem wcisniec)
		if (buf->data && buf->num > before && before >= 0) {
			unsigned char* keyEvent = (unsigned char*)0x008D1678;
			unsigned char* keyToggle = (unsigned char*)0x008D18B8;
			int w = before;
			for (int r = before; r < buf->num; r++) {
				int code = buf->data[r];
				if (ZmFilterPress(code)) {
					if (code >= 0 && code < ZM_KEY_CODES) {
						keyEvent[code] = 0;
						keyToggle[code] = 0;
					}
					continue;
				}
				buf->data[w++] = code;
			}
			buf->num = w;
		}
		WheelInputStep(self);   // menu kolowe: prog przytrzymania / stukniecie (Tab do bufora po filtrze), mysz kola
	}

	static zCView* ZmView(const ZMenuLayout& lay) {
		if (!zmView) {
			zmView = new zCView(0, 0, 8192, 8192);
			zmView->SetAlphaBlendFunc(zTRnd_AlphaBlendFunc::zRND_ALPHA_FUNC_BLEND);
		}
		// tlo: New Balance - okno kowalstwa NB (jak zTransmog), gdzie indziej tlo wyboru w dialogu
		std::string back = !lay.back.empty() ? lay.back : GetModuleHandleA("UNION_ABI.DLL") ? "AS_BACK_CRAFT.TGA" : "DLG_CHOICE.TGA";
		if (back != zmViewBack) {
			zmView->InsertBack(zSTRING(back.c_str()));
			zmViewBack = back;
		}
		return zmView;
	}

	static int ZmWidth(zCView* v, const std::string& s) {
		zSTRING z(s.c_str());
		return v->FontSize(z);
	}

	static void ZmPrintCentered(zCView* v, int y, const std::string& s, const zCOLOR& c) {
		v->SetFontColor(c);
		v->Print(std::max(0, 4096 - ZmWidth(v, s) / 2), y, zSTRING(s.c_str()));
	}

	// Zawijanie po slowach do szerokosci w jednostkach okna.
	static std::vector<std::string> ZmWrap(zCView* v, const std::string& text, int width) {
		std::vector<std::string> out;
		std::string line, word;
		auto flushWord = [&]() {
			if (word.empty())
				return;
			std::string cand = line.empty() ? word : line + " " + word;
			if (!line.empty() && ZmWidth(v, cand) > width) {
				out.push_back(line);
				line = word;
			}
			else
				line = cand;
			word.clear();
		};
		for (char ch : text) {
			if (ch == ' ')
				flushWord();
			else
				word += ch;
		}
		flushWord();
		if (!line.empty())
			out.push_back(line);
		return out;
	}

	static void ZmBegin(zCView* v, const ZMenuLayout& lay) {
		screen->InsertItem(v);
		v->ClrPrintwin();
		v->SetPos(lay.x, lay.y);
		v->SetSize(lay.w, lay.h);
		v->SetTransparency(lay.transparency);
	}

	static void ZmEnd(zCView* v) {
		v->Blit();
		screen->RemoveItem(v);
		int sx, sy, ssx, ssy;
		screen->GetViewport(sx, sy, ssx, ssy);
		zrenderer->SetViewport(sx, sy, ssx, ssy);
	}

	// Kursor myszy okna (tekstura strzalki z mapy gry, czubek w lewym gornym rogu) - nad oknem.
	static void ZmDrawCursor() {
		if (!zmMouseOn || !zmCurInit || !screen)
			return;
		if (!zmCursorView) {
			zmCursorView = new zCView(0, 0, 0, 0);
			zmCursorView->InsertBack(zSTRING(ZM_CURSOR_TEX));
		}
		screen->InsertItem(zmCursorView);
		zmCursorView->SetSize(screen->anx(ZM_CURSOR_PX), screen->any(ZM_CURSOR_PX));
		zmCursorView->SetPos(screen->anx((int)zmCurX), screen->any((int)zmCurY));
		zmCursorView->SetColor(zCOLOR(255, 255, 255, 255));
		zmCursorView->Blit();
		screen->RemoveItem(zmCursorView);
		int sx, sy, ssx, ssy;
		screen->GetViewport(sx, sy, ssx, ssy);
		zrenderer->SetViewport(sx, sy, ssx, ssy);
	}

	void ZMenuDraw(zdcore::MenuList& m, const ZMenuLayout& lay) {
		if (!screen || !zrenderer)
			return;
		ZPROF_SCOPE("Menu.Draw");
		zCView* v = ZmView(lay);
		ZmBegin(v, lay);
		const int margin = 450;
		v->SetFont(zSTRING(lay.titleFont));
		int bigY = v->FontY();
		int y = bigY / 3;
		ZmPrintCentered(v, y, m.title, ZM_TITLE);
		y += bigY + bigY / 8;
		v->SetFont(zSTRING(lay.font));
		int fy = std::max(v->FontY(), 1);
		zdcore::MenuHitMap hit;
		hit.valid = true;
		hit.x0 = 0;
		hit.x1 = 8192;
		// zakladki: biezaca w nawiasach i wyrozniona, reszta szara; wysrodkowane jako jedna linia
		if (!m.tabs.empty()) {
			std::vector<std::string> parts;
			int total = 0, gap = ZmWidth(v, "   ");
			for (size_t i = 0; i < m.tabs.size(); i++) {
				parts.push_back((int)i == m.tab ? "[ " + m.tabs[i] + " ]" : m.tabs[i]);
				total += ZmWidth(v, parts.back()) + (i ? gap : 0);
			}
			int x = std::max(0, 4096 - total / 2);
			for (size_t i = 0; i < parts.size(); i++) {
				v->SetFontColor((int)i == m.tab ? ZM_SELECTED : ZM_HINT);
				v->Print(x, y, zSTRING(parts[i].c_str()));
				int w = ZmWidth(v, parts[i]);
				zdcore::MenuHitTab t;
				t.tab = (int)i;
				t.x0 = x - gap / 2;
				t.x1 = x + w + gap / 2;
				t.y0 = y;
				t.y1 = y + fy;
				hit.tabs.push_back(t);
				x += w + gap;
			}
			y += fy + fy / 2;
		}
		for (auto& h : m.header) {
			ZmPrintCentered(v, y, h, ZM_HEADER);
			y += fy;
		}
		y += fy / 2;
		// opis zaznaczonego wiersza (m.hintLines linii) i podpowiedzi klawiszy na dole
		int hintLines = std::clamp(m.hintLines, 1, 6);
		std::vector<std::string> hint;
		if (m.cursor >= 0 && m.cursor < (int)m.rows.size() && !m.rows[m.cursor].hint.empty()) {
			hint = ZmWrap(v, m.rows[m.cursor].hint, 8192 - 2 * margin);
			if ((int)hint.size() > hintLines)
				hint.resize(hintLines);
		}
		std::vector<std::string> footer;
		for (auto& f : m.footer)
			for (auto& l : ZmWrap(v, f, 8192 - 2 * margin))
				footer.push_back(l);
		int bottom = 8192 - fy / 2 - fy * (int)footer.size() - fy / 2 - fy * hintLines;
		int rowH = fy + fy / 3;
		int maxRows = std::max(1, (bottom - y) / rowH);
		std::vector<int> rows = zdcore::MenuVisibleRows(m, maxRows);
		int rowsTop = y;
		int arrowW = ZmWidth(v, "< ");
		for (int idx : rows) {
			const zdcore::MenuRow& r = m.rows[idx];
			bool sel = idx == m.cursor;
			bool grey = !r.enabled || r.kind == zdcore::MROW_TEXT;
			v->SetFontColor(grey ? ZM_DISABLED : r.dim ? (sel ? ZM_SELECTED_DIM : ZM_DISABLED) : sel ? ZM_SELECTED : ZM_ROW);
			v->Print(margin, y, zSTRING(((sel ? "> " : "   ") + r.label).c_str()));
			std::string val = zdcore::MenuValueText(r);
			zdcore::MenuHitRow hr;
			hr.row = idx;
			hr.y0 = y - fy / 6;
			hr.y1 = y - fy / 6 + rowH;
			if (!val.empty()) {
				int vw = ZmWidth(v, val);
				v->Print(8192 - margin - vw, y, zSTRING(val.c_str()));
				hr.valX0 = 8192 - margin - vw;
				hr.valX1 = 8192 - margin;
				hr.arrowW = arrowW;
			}
			hit.rows.push_back(hr);
			y += rowH;
		}
		zmHit = hit;
		zmHitLay = lay;
		// znaczniki przewijania
		int visibleCount = 0;
		for (auto& r : m.rows)
			visibleCount += r.visible ? 1 : 0;
		v->SetFontColor(ZM_HINT);
		if (m.first > 0)
			v->Print(8192 - margin / 2 - ZmWidth(v, "^"), rowsTop, zSTRING("^"));
		if (m.first + (int)rows.size() < visibleCount)
			v->Print(8192 - margin / 2 - ZmWidth(v, "v"), y - rowH, zSTRING("v"));
		int hy = 8192 - fy / 2 - fy * (int)footer.size() - fy / 2 - fy * hintLines;
		for (auto& l : hint) {
			v->Print(margin, hy, zSTRING(l.c_str()));
			hy += fy;
		}
		int fyy = 8192 - fy / 2 - fy * (int)footer.size();
		for (auto& l : footer) {
			v->Print(margin, fyy, zSTRING(l.c_str()));
			fyy += fy;
		}
		ZmEnd(v);
		ZmDrawCursor();
	}

	void ZMenuDrawLines(const std::vector<std::string>& lines, const ZMenuLayout& lay) {
		if (!screen || !zrenderer || lines.empty())
			return;
		ZPROF_SCOPE("Menu.Draw");
		zCView* v = ZmView(lay);
		ZmBegin(v, lay);
		v->SetFont(zSTRING(lay.font));
		int fy = std::max(v->FontY(), 1);
		int total = fy * (int)lines.size() + fy / 2 * ((int)lines.size() - 1);
		int y = std::max(0, (8192 - total) / 2);
		for (size_t i = 0; i < lines.size(); i++) {
			ZmPrintCentered(v, y, lines[i], i == 0 ? ZM_SELECTED : ZM_HINT);
			y += fy + fy / 2;
		}
		ZmEnd(v);
	}
}
