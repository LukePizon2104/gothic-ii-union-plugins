// Logika bez silnika: udany blok (parada) - efekt zetkniecia broni (iskry / drzazgi), punkt zetkniecia ostrzy,
// emitery czasteczek i krotki proceduralny odrzut broni i ramion (krzywa, osie obrotu stawow, wagi wezlow).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "ZMath.h"

namespace zdcore {
	// C_Item.material (oTSndMaterial silnika: SND_MAT_WOOD..SND_MAT_GLAS).
	enum : int { ITEM_MAT_WOOD = 0, ITEM_MAT_STONE = 1, ITEM_MAT_METAL = 2, ITEM_MAT_LEATHER = 3, ITEM_MAT_CLAY = 4, ITEM_MAT_GLAS = 5 };

	enum class ParryMat {
		NoWeapon,   // bez broni w rece (piesci, pazury potworow)
		Hard,   // metal, kamien (i nieznany material z modu)
		Wood,   // drewno: palki, buzdygany, kostury, laski (G2: MAT_WOOD) albo ParryWoodWeapons
		Soft,   // skora, glina, szklo
	};

	// hasWeapon: bron biala w rece (tryb 1H/2H/sztylet); material: C_Item.material; instance / visual: nazwa instancji
	// i model broni; woodTokens (wielkie litery): fragmenty nazw broni drewnianych mimo materialu (mody).
	ParryMat ClassifyParryMat(bool hasWeapon, int material, const std::string& instance, const std::string& visual,
		const std::vector<std::string>& woodTokens);
	const char* ParryMatName(ParryMat m);

	enum : uint32_t { PARRY_FX_NONE = 0, PARRY_FX_SPARKS = 1, PARRY_FX_SPLINTERS = 2, PARRY_FX_FLASH = 4 };
	// Efekt zetkniecia dwoch broni: drewno po ktorejkolwiek stronie = drzazgi, dwie twarde = iskry z blyskiem,
	// twarda o miekka = nic; bez broni po ktorejkolwiek stronie = nic (silnik przy piesciach tez nie daje czasteczek).
	uint32_t ParryFx(ParryMat a, ParryMat b);

	// Czy parada to zetkniecie broni. aniName: animacja parady broniacego (silnik uznaje parade po "PARADE" w nazwie),
	// overlayName: nakladka (.MDS), z ktorej ta animacja gra. Blok T_xHPARADE_* (Gothic, zEBA, styl W3) - tak.
	// T_xHPARADEJUMPB to odskok (Gothic) albo ruch W3 z naszych nakladek (uniki, przewroty, ataki z wyskoku) - nie,
	// poza kontra W3 (HUMANS_ZDF_MV_COUNTER_* / BASH_* / KICK_*), ktora zaczyna sie odbiciem bronia.
	bool IsBlockParade(const std::string& aniName, const std::string& overlayName);

	// Najblizsze punkty odcinkow p0-p1 i q0-q1: p0 + s (p1 - p0), q0 + t (q1 - q0), s i t w [0, 1].
	void ClosestSegmentPoints(const Vec3& p0, const Vec3& p1, const Vec3& q0, const Vec3& q1, float& s, float& t);

	// Ostrze broni w rece: dlon (ZS_RIGHTHAND), kierunek (jednostkowy) i dlugosc od dloni do konca (cm).
	struct Blade {
		Vec3 hand, dir;
		float length = 0.0f;
	};
	// Odleglosc jelca od dloni: zetkniecie liczymy na glowni, nie na rekojesci.
	float BladeGuard(float length);
	// Punkt zetkniecia broni: srodek najblizszych punktow glowni; gdy mijaja sie dalej niz maxGap (animacje Gothica
	// rzadko trafiaja ostrzem w ostrze) - punkt fallbackT glowni broniacego (od jelca do konca).
	Vec3 ParryContactPoint(const Blade& attacker, const Blade& defender, float maxGap = 45.0f, float fallbackT = 0.6f);
	// Dlugosc i zwrot ostrza wzdluz osi X slotu ZS_RIGHTHAND z bboxa modelu broni (minX, maxX w ukladzie slotu).
	// false = bbox bez sensu (brak modelu) - dlugosc z zasiegu broni (C_Item.range, przycieta), zwrot -X.
	bool BladeFromBox(float minX, float maxX, int itemRange, float& length, float& dirSign);

	// Krzywa odrzutu w czasie t (s): szybki narost do 1 w peakFrac * duration, potem gasnaca oscylacja z malym odbiciem
	// ponizej zera i gladkim zejsciem do 0 na koncu. Poza (0, duration) = 0.
	float RecoilCurve(float t, float duration, float peakFrac = 0.2f);
	// Kierunek, w ktorym odrzut pcha punkt zetkniecia. attackerToDefender: od napastnika do broniacego (pion pomijany).
	// Broniacy: bron i rece do tylu (od napastnika), lekko w dol; napastnik: odbicie do siebie i w gore.
	Vec3 RecoilPush(const Vec3& attackerToDefender, bool defender);
	// Os obrotu stawu, przy ktorej punkt zetkniecia przesuwa sie w strone push (dodatni kat): (contact - joint) x push.
	// Zero, gdy ramie zerowe albo rownolegle do pchniecia.
	Vec3 RecoilAxis(const Vec3& joint, const Vec3& contact, const Vec3& push);
	// Mnoznik sily i czasu odrzutu dla roli (broniacy / napastnik - odbity cios mocniej i dluzej).
	float RecoilRoleScale(bool defender);
	float RecoilRoleDuration(bool defender);

	// Wezly odrzutu i ich wagi (kat wezla = sila x waga x krzywa). 1H: tulow, prawe ramie, przedramie, dlon i slot broni.
	// 2H: wiecej tulowia (niesie obie rece z bronia razem) i obie rece, bez slotu (sam rozerwalby chwyt lewej dloni).
	struct RecoilJoint {
		const char* node;
		float weight;
	};
	const std::vector<RecoilJoint>& RecoilJoints(bool twoHanded);

	// Os z ukladu modelu w ukladzie rodzica wezla (parentToModel - macierz rodzica w ukladzie modelu, moze miec skale).
	Vec3 AxisInParent(const Mat34& parentToModel, const Vec3& axisModel);
	// Obrot bazy (3x3) macierzy lokalnej wezla wokol osi (w ukladzie rodzica) przez staw: przesuniecie bez zmian.
	void RotateBasis(Mat34& local, const Vec3& axis, float deg);

	// Odrzut w haku zCModel::AdvanceAnis: silnik nie zawsze liczy macierz lokalna wezla od nowa - gdy w wezle jest
	// dokladnie macierz, ktora zapisalismy w poprzedniej klatce, baza (poza z animacji) zostaje stara (bez kumulacji).
	struct RecoilNodeState {
		Mat34 base = Mat34::Identity();
		Mat34 written = Mat34::Identity();
		bool has = false;
		bool wrote = false;
	};
	bool SameMat(const Mat34& a, const Mat34& b);
	Mat34 RecoilBase(RecoilNodeState& s, const Mat34& current);
	void RecoilWritten(RecoilNodeState& s, const Mat34& written);
	// Koniec odrzutu: true i out = baza, gdy w wezle zostala nasza macierz (silnik jej nie nadpisal).
	bool RecoilRestore(const RecoilNodeState& s, const Mat34& current, Mat34& out);

	// Pola skryptowe C_PARTICLEFX (nazwy jak w ParticleFX.d, bez przyrostka _S). Emitery parady budujemy w kodzie -
	// skrypty gry maja tylko CPFX_IAI_METAL/STONE/WOOD (iskry 2x2 cm, ~8 sztuk) i brak par materialow (METAL_WOOD).
	struct EmitterSpec {
		std::string name;
		float ppsValue = 0.0f;
		std::string ppsScaleKeys = "1";
		int ppsIsLooping = 0;
		int ppsIsSmooth = 0;
		float ppsFPS = 10.0f;
		std::string shpType = "POINT", shpFOR = "world", shpOffsetVec = "0 0 0", shpDistribType = "RAND", shpDim = "0";
		int shpIsVolume = 0;
		std::string dirMode = "NONE", dirFOR = "world";
		float dirAngleHead = 0.0f, dirAngleHeadVar = 0.0f, dirAngleElev = 0.0f, dirAngleElevVar = 0.0f;
		float velAvg = 0.0f, velVar = 0.0f;           // cm/ms
		float lspPartAvg = 0.0f, lspPartVar = 0.0f;   // ms
		std::string flyGravity = "0 0 0";              // cm/ms^2
		int flyCollDet = 0;
		std::string visName, visOrientation = "NONE";
		int visTexIsQuadPoly = 1;
		float visTexAniFPS = 0.0f;
		int visTexAniIsLooping = 0;
		std::string visTexColorStart = "255 255 255", visTexColorEnd = "255 255 255", visSizeStart = "1 1";
		float visSizeEndScale = 1.0f;
		std::string visAlphaFunc = "BLEND";
		float visAlphaStart = 255.0f, visAlphaEnd = 0.0f;
	};
	EmitterSpec ParrySparksSpec();      // iskry: smugi ZSPARK1 wzdluz predkosci, ADD
	EmitterSpec ParryFlashSpec();       // blysk w punkcie zetkniecia (1-2 czasteczki CFLARESTAR)
	EmitterSpec ParrySplintersSpec();   // drzazgi: CPFX_WOOD (jak CPFX_IAI_WOOD, wieksze i szybsze)
	// Liczba czasteczek jednorazowego wytrysku: ppsValue x suma kluczy / ppsFPS.
	float EmitterBurstCount(const EmitterSpec& s);
	// Czas zycia efektu (s): emisja + najdluzsza czasteczka + zapas - po nim vob efektu znika.
	float EmitterLifetime(const EmitterSpec& s);
}
