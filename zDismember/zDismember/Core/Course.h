// Tor przeszkod do testow przeskokow (zdism trav tor): uklad przeszkod z modeli gry wg ich bboxow - bez silnika.
// Uklad toru: x w prawo od osi (cm), z do przodu od startu, y nad ziemia przeszkody (plugin bierze ziemie z promienia
// w dol przy przodzie kazdej przeszkody).
#pragma once
#include <string>
#include <vector>

namespace zdcore {

	// Klocek: wizual gry (nazwa .3DS) i jego bbox w ukladzie voba (x szerokosc, y wysokosc, z glebokosc).
	struct CoursePiece {
		std::string visual;
		float minX = 0.0f, minY = 0.0f, minZ = 0.0f, maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
		float SizeX() const { return maxX - minX; }
		float SizeY() const { return maxY - minY; }
		float SizeZ() const { return maxZ - minZ; }
	};

	// Wizuale toru (plugin pyta silnik o bbox kazdego; brak modelu = przeszkody z nim pominiete).
	extern const char* const COURSE_STONE;      // plyta 100 x 30 x 100 (stopnie co 30)
	extern const char* const COURSE_CRATE;      // skrzynia portowa 120 x 100 x 100
	extern const char* const COURSE_WALL;       // mur 350 x 300 x 100
	extern const char* const COURSE_PARAPET;    // cienki murek 179 x 107 x 23
	extern const char* const COURSE_LOG;        // belka 161 x 30 x 29 (zerdz plotu)
	extern const char* const COURSE_POST;       // slupek 23 x 158 x 23
	extern const char* const COURSE_BARREL;     // beczka 80 x 111 x 80 (okragla)
	extern const char* const COURSE_PLATE;      // plyta 370 x 10 x 200 (dach / rampa pod katem)
	extern const char* const COURSE_PILLAR;     // waski slup 40 x 300 x 24
	extern const char* const COURSE_PILE;       // stos skrzyn z portu 307 x 210 x 287 (nierowna gora)
	const std::vector<std::string>& CourseVisuals();

	// Postawienie voba: pozycja (origin voba) w ukladzie toru i obrot (yaw wokol pionu, pitch wokol x - przod w gore).
	struct CoursePlace {
		std::string visual;
		float x = 0.0f, y = 0.0f, z = 0.0f;
		float yawDeg = 0.0f, pitchDeg = 0.0f;
	};

	struct CourseObstacle {
		std::string name;          // np. "stopien 100"
		std::string expect;        // co powinno sie stac (wspiecie / przeskok / odmowa ...)
		float x = 0.0f;            // os rzedu
		float z = 0.0f;            // przod przeszkody (sciana) od startu
		float height = 0.0f;       // gora nad ziemia (z bboxow), dla kilku poziomow - najwyzsza krawedz chwytu
		float width = 0.0f, depth = 0.0f;   // obrys (przod - tyl)
		// 24.10: miejsce testu (napis i pomiar po postawieniu w swiecie): z od przodu przeszkody i wysokosc krawedzi tam
		// wzgledem ziemi przed przeszkoda (chwyt w locie: mur 10,5 m za przodem platformy - napis nad nim, nie nad platforma)
		float featureZ = 0.0f;
		float featureH = -1.0f;    // < 0 = height
		std::vector<CoursePlace> parts;
	};

	struct CoursePlan {
		std::vector<CourseObstacle> obstacles;
		std::vector<std::string> skipped;   // przeszkody pominiete (brak modelu)
		float minX = 0.0f, maxX = 0.0f, length = 0.0f;   // pas toru (do sprawdzenia, czy plaski i pusty)
	};

	struct CourseParams {
		float start = 600.0f;      // rozbieg przed pierwsza przeszkoda w rzedzie (cm)
		float gap = 600.0f;        // odstep miedzy przeszkodami w rzedzie (rozbieg)
		float rowSpacing = 550.0f; // odstep rzedow (4 rzedy)
		float stepWidth = 200.0f, stepDepth = 200.0f;   // blok stopnia (szerzej niz bohater, glebokosc >= 1 m)
		float deckDepth = 600.0f;  // gora stopni i platform razem z pokladem z plyt za blokiem (runda 12)
	};

	// Stos warstw z klockow (indeksy pieces, od dolu): suma wysokosci najblizej celu - koszt |roznica| + 3 cm na klocek,
	// najwyzej maxLayers. Pusty, gdy brak klockow.
	std::vector<int> CourseStack(const std::vector<CoursePiece>& pieces, float target, int maxLayers = 8);

	// Uklad: trzy rzedy - stopnie 30..350, przeszkody rodzajow (murek, plot, beczka, slup, nawis, stos skrzyn), dachy
	// i skoki (dach 30 / 45 st., przepasc, wysoka polka). catalog: dostepne klocki (z bboxami).
	CoursePlan CourseBuild(const std::vector<CoursePiece>& catalog, const CourseParams& p = CourseParams());

	// Linia do logu / konsoli: "stopien 100: z 14 m, rzad -5 m, wys. 100 cm (NW_HARBOUR_CRATE_01 x4) - wspiecie".
	std::string CourseDescribe(const CourseObstacle& o);
}
