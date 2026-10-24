// Logika bez silnika: przywracanie odcietych czesci po cofnieciu czasu (zTimeRewind wola nas przez _shared\ZCutLink.h).
// Silnik: RewindLink.cpp (sciskanie kosci, odlamki, kikuty, ragdoll, nakladki ofiary).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "PartMask.h"
#include "Text.h"

namespace zdcore {
	const uint32_t CUT_MASK_BITS = 0x1FFu;   // 9 czesci (PartMask.h)

	// Zmiana maski odciec postaci: co przywrocic (bylo odciete, w masce docelowej nie ma), co scisnac na nowo.
	struct CutRestorePlan {
		uint32_t restore = 0;   // zdjecie sciskania + usuniecie odlamkow i krwi z kikuta tej czesci
		uint32_t apply = 0;     // sciskanie bez odlamka i bez krwi (stan z chwili docelowej, np. zwloki cofniete
		                        // w czasie, w ktorym czesc juz byla odcieta)
		uint32_t result = 0;    // maska po zmianie
		bool Any() const { return restore != 0 || apply != 0; }
	};

	inline CutRestorePlan PlanCutRestore(uint32_t before, uint32_t target) {
		CutRestorePlan p;
		before &= CUT_MASK_BITS;
		target &= CUT_MASK_BITS;
		p.restore = before & ~target;
		p.apply = target & ~before;
		p.result = target;
		return p;
	}

	// Nazwy czesci z maski do logu: "HEAD ARM_L"; "-" dla 0.
	inline std::string MaskPartNames(uint32_t mask) {
		std::string s;
		for (int i = 0; i < (int)Part::NoPart; i++)
			if (MaskHas(mask, (Part)i)) {
				if (!s.empty())
					s += " ";
				s += PartName((Part)i);
			}
		return s.empty() ? "-" : s;
	}

	// Nazwa nakladki bez rozszerzenia, wielkimi literami ("humans_zdf_daze.mds" -> "HUMANS_ZDF_DAZE").
	inline std::string OverlayBaseName(const std::string& name) {
		std::string n = Upper(Trim(name));
		size_t dot = n.find('.');
		return dot == std::string::npos ? n : n.substr(0, dot);
	}

	// Nakladki postaci do zdjecia, gdy wstaje po cofnieciu czasu: ofiary finisherow (victimOverlay z finishers.ini -
	// zostaja na zwlokach, bo smierc z nich gra) i oszolomienia (DazeOverlay). Bez powtorzen, OverlayBaseName.
	inline std::vector<std::string> VictimOverlayList(const std::vector<std::string>& victimOverlays, const std::string& daze) {
		std::vector<std::string> out;
		auto add = [&](const std::string& n) {
			std::string b = OverlayBaseName(n);
			if (b.empty())
				return;
			for (const std::string& o : out)
				if (o == b)
					return;
			out.push_back(b);
		};
		for (const std::string& v : victimOverlays)
			add(v);
		add(daze);
		return out;
	}

	// Prototyp modelu (modelProtoName, z rozszerzeniem albo bez) jest na liscie VictimOverlayList.
	inline bool IsListedOverlay(const std::string& protoName, const std::vector<std::string>& list) {
		std::string b = OverlayBaseName(protoName);
		for (const std::string& o : list)
			if (o == b)
				return true;
		return false;
	}
}
