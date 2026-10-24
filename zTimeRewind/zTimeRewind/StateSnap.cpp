// Supported with union (c) 2020 Union team
// Union SOURCE file

namespace GOTHIC_ENGINE {
	static zCBuffer* StateBuffer() {
		static zCBuffer* buf = new zCBuffer((unsigned long)(16u * 1024u * 1024u));
		return buf;
	}

	// Zapis przez archiwizator silnika do bufora w pamieci; fn dostaje gotowy archiwizator.
	static bool WriteBlob(std::vector<unsigned char>& out, const std::function<void(zCArchiver&)>& fn) {
		zCBuffer* buf = StateBuffer();
		buf->Reset();
		zCArchiver* arc = zarcFactory->CreateArchiverWrite(buf, zARC_MODE_BINARY_SAFE, TRUE, 0);
		if (!arc)
			return false;
		fn(*arc);
		arc->Close();
		// pozycja zapisu albo size_byte - ale nigdy pojemnosc bufora (GetSize = size_byte, semantyka niepewna)
		unsigned long size = buf->GetPos();
		unsigned long sb = buf->GetSize();
		if (sb > size && sb < buf->GetMaxSize())
			size = sb;
		out.assign(buf->GetBuffer(), buf->GetBuffer() + size);
		arc->Release();
		return size > 0;
	}

	static bool ReadBlob(const std::vector<unsigned char>& in, const std::function<void(zCArchiver&)>& fn) {
		if (in.empty())
			return false;
		zCBuffer buf((void*)in.data(), (unsigned long)in.size());
		buf.SetPosBegin();
		zCArchiver* arc = zarcFactory->CreateArchiverRead(&buf, 0);
		if (!arc)
			return false;
		fn(*arc);
		arc->Close();
		arc->Release();
		return true;
	}

	static oCInfoManager* Infos() {
		return ogame ? ogame->GetInfoManager() : nullptr;
	}

	// Zmienne zapisu - te same co zCParser::SaveGlobalVars, ale bez archiwizatora (ten bral ~140 ms co migawke,
	// 785 KB z nazwami - zaciecia co sekunde, log 16.10). Lista symboli liczona raz na parser (i liczbe symboli),
	// porcjami: przeglad ~89 tys. symboli to ~4 ms naraz (pierwsza migawka po wczytaniu gry).
	struct SavedGlobalsList {
		std::vector<zCPar_Symbol*> list;
		size_t values = 0;            // suma elementow = rozmiar migawki zmiennych (reserve)
		trcore::ChunkScan scan;
		std::vector<char> keep;       // KeepGlobals: 1 = symbol poza cofaniem (rownolegle do list)
		int keepGen = -1;             // trcfg.keepGlobalsGen, dla ktorego liczone keep
		size_t keepListSize = 0;
	};
	static SavedGlobalsList savedGlobals;

	// Doklada do listy najwyzej budget symboli (0 = do konca). true = lista kompletna dla biezacego parsera.
	static bool SavedGlobalsStep(int budget) {
		SavedGlobalsList& g = savedGlobals;
		int n = parser ? parser->symtab.GetNumInList() : 0;
		if (g.scan.Restart(parser, n)) {
			g.list.clear();
			g.values = 0;
		}
		int begin = 0, end = 0;
		if (g.scan.Next(budget, begin, end)) {
			ZPROF_SCOPE("StateSnap.Relist");   // przeglad symboli parsera porcjami (po wczytaniu gry)
			for (int i = begin; i < end; i++) {
				zCPar_Symbol* s = parser->symtab.GetSymbol(i);
				if (s && trcore::IsSavedGlobal(s->type, s->flags, s->ele)) {
					g.list.push_back(s);
					g.values += s->ele;
				}
			}
			if (g.scan.Done())
				ZLog("State: %d zmiennych zapisu (%u wartosci) z %d symboli", (int)g.list.size(), (unsigned)g.values, n);
		}
		return g.scan.Done();
	}

	// Kompletna lista (w razie potrzeby dokonczona od razu - wczytanie migawki, konsola).
	static const std::vector<zCPar_Symbol*>& SavedGlobals() {
		SavedGlobalsStep(0);
		return savedGlobals.list;
	}

	bool TrStatePrepare(int relistBudget) {
		if (!parser || !ogame)
			return false;
		return SavedGlobalsStep(relistBudget);
	}

	static void CaptureGlobals(std::vector<int>& out) {
		ZPROF_SCOPE("StateSnap.Globals");
		const std::vector<zCPar_Symbol*>& list = SavedGlobals();
		out.clear();
		out.reserve(savedGlobals.values);   // bufor z puli ma juz pojemnosc - bez alokacji
		for (zCPar_Symbol* s : list)
			for (int i = 0; i < (int)s->ele; i++) {
				int v = 0;
				s->GetValue(v, i);
				out.push_back(v);
			}
	}

	// Znaczniki KeepGlobals dla biezacej listy (po zmianie ini albo nowej liscie symboli). Log raz na przeliczenie.
	static const std::vector<char>& KeepFlags() {
		SavedGlobalsList& g = savedGlobals;
		const std::vector<zCPar_Symbol*>& list = SavedGlobals();
		if (g.keepGen == trcfg.keepGlobalsGen && g.keepListSize == list.size() && g.keep.size() == list.size())
			return g.keep;
		trcore::NameList names;
		names.Parse(trcfg.keepGlobals);
		g.keep.assign(list.size(), 0);
		std::string found;
		int n = 0;
		for (size_t i = 0; i < list.size(); i++)
			if (!names.Empty() && names.Match(list[i]->name.ToChar())) {
				g.keep[i] = 1;
				n++;
				if (found.size() < 400)
					found += (found.empty() ? "" : ", ") + std::string(list[i]->name.ToChar());
			}
		g.keepGen = trcfg.keepGlobalsGen;
		g.keepListSize = list.size();
		ZLog("State: zmienne poza cofaniem (KeepGlobals) %d z %u pozycji ini: %s", n, (unsigned)names.Items().size(),
			n ? found.c_str() : "brak w skryptach tej gry");
		return g.keep;
	}

	// Zmienne zapisu z migawki. KeepGlobals zostaja jak teraz - log pokazuje, co cofanie by w nich zmienilo
	// (rozstrzyga zgloszenia "po cofnieciu bohater szybszy": ANIACCELERATION w NB). Lista zmienionych do logu skrocona.
	static int ApplyGlobals(const std::vector<int>& in, int& keptDiff) {
		const std::vector<zCPar_Symbol*>& list = SavedGlobals();
		const std::vector<char>& keep = KeepFlags();
		size_t k = 0;
		int changed = 0;
		keptDiff = 0;
		std::string changedNames, keptNames;
		for (size_t si = 0; si < list.size(); si++) {
			zCPar_Symbol* s = list[si];
			if (k + s->ele > in.size())
				break;
			bool kept = si < keep.size() && keep[si];
			for (int i = 0; i < (int)s->ele; i++, k++) {
				int v = 0;
				s->GetValue(v, i);
				if (v == in[k])
					continue;
				char item[96];
				if (s->ele > 1)
					sprintf_s(item, "%s[%d] %d->%d", s->name.ToChar(), i, v, in[k]);
				else
					sprintf_s(item, "%s %d->%d", s->name.ToChar(), v, in[k]);
				if (kept) {
					keptDiff++;
					if (keptNames.size() < 600)
						keptNames += (keptNames.empty() ? "" : ", ") + std::string(item);
					continue;
				}
				s->SetValue(in[k], i);
				changed++;
				if (changedNames.size() < 900)
					changedNames += (changedNames.empty() ? "" : ", ") + std::string(item);
			}
		}
		if (keptDiff)
			ZLog("State: KeepGlobals bez cofania (teraz->migawka, zostaje teraz): %s", keptNames.c_str());
		if (changed)
			ZLog("State: cofniete zmienne (%d): %s%s", changed, changedNames.c_str(), changedNames.size() >= 900 ? ", ..." : "");
		return changed;
	}

	bool TrCaptureState(TrStateSnap& out) {
		ZPROF_SCOPE("StateSnap.Capture");   // migawka zmiennych skryptow, dziennika, dialogow (co StateInterval)
		if (!parser || !ogame)
			return false;
		LARGE_INTEGER f, a, g, l, b;
		QueryPerformanceFrequency(&f);
		QueryPerformanceCounter(&a);
		CaptureGlobals(out.globals);
		bool ok = !out.globals.empty();
		QueryPerformanceCounter(&g);
		{
			ZPROF_SCOPE("StateSnap.Log");   // dziennik przez archiwizator silnika
			ok = WriteBlob(out.log, [](zCArchiver& arc) { oCLogManager::GetLogManager().Archive(arc); }) && ok;
		}
		QueryPerformanceCounter(&l);
		{
			ZPROF_SCOPE("StateSnap.Told");   // flagi odbytych dialogow
			out.told.clear();
			if (oCInfoManager* im = Infos())
				for (auto it = im->infoList.next; it; it = it->next)
					out.told.push_back(it->data ? it->data->told : 0);
		}
		TrCaptureGuilds(out.guilds);
		QueryPerformanceCounter(&b);
		double k = 1000.0 / f.QuadPart;
		double ms = (b.QuadPart - a.QuadPart) * k;
		if (ms > 5.0 || trcfg.debug)
			ZLog("State: migawka %.2f ms (zmienne %u: %.2f ms, dziennik %u B: %.2f ms, dialogi %u: %.2f ms)%s", ms,
				(unsigned)out.globals.size(), (g.QuadPart - a.QuadPart) * k, (unsigned)out.log.size(), (l.QuadPart - g.QuadPart) * k,
				(unsigned)out.told.size(), (b.QuadPart - l.QuadPart) * k, ms > 5.0 ? " - WOLNO" : "");
		return ok;
	}

	bool TrApplyState(const TrStateSnap& s) {
		ZPROF_SCOPE("StateSnap.Apply");
		if (!parser || !ogame)
			return false;
		int keptDiff = 0;
		int globalsChanged = ApplyGlobals(s.globals, keptDiff);
		bool ok = !s.globals.empty();
		// Unarchive dziennika dokleja tematy (0x6636B0) - najpierw czyszczenie
		oCLogManager::GetLogManager().Clear();
		ok = ReadBlob(s.log, [](zCArchiver& arc) { oCLogManager::GetLogManager().Unarchive(arc); }) && ok;
		size_t i = 0, changed = 0;
		if (oCInfoManager* im = Infos())
			for (auto it = im->infoList.next; it && i < s.told.size(); it = it->next, i++)
				if (it->data && it->data->told != s.told[i]) {
					it->data->told = s.told[i];
					changed++;
				}
		int guildsChanged = TrApplyGuilds(s.guilds);
		ZLog("State: wczytana migawka (zmienne zmienione %d z %u, poza cofaniem %d, dziennik %d, dialogi zmienione %u z %u, postawy gildii zmienione %d z %u)",
			globalsChanged, (unsigned)s.globals.size(), keptDiff, (int)!s.log.empty(), (unsigned)changed, (unsigned)s.told.size(),
			guildsChanged, (unsigned)s.guilds.size());
		return ok;
	}

	void StateRegisterCommands() {
		static TrStateSnap slot;
		TrConsoleRegister("STATE", "tr state save|load|info - migawka stanu skryptow (test)", [](const std::vector<std::string>& a) {
			std::string cmd = a.size() > 1 ? a[1] : "";
			if (cmd == "SAVE")
				return zSTRING(TrCaptureState(slot) ? "state: zapisane" : "state: blad zapisu");
			if (cmd == "LOAD")
				return zSTRING(TrApplyState(slot) ? "state: wczytane" : "state: blad odczytu");
			char buf[128];
			sprintf_s(buf, "state: zmienne %u, dziennik %u B, dialogi %u", (unsigned)slot.globals.size(),
				(unsigned)slot.log.size(), (unsigned)slot.told.size());
			return zSTRING(buf);
		});
	}
}
