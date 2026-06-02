#pragma once
#include <FastLED.h>
#include <LEDMatrix.h>
#include "Shared.h"   // setPixel() — identisches Mapping wie Snake

// Gemeinsame Hilfsfunktionen für AppWetter und AppUhrzeit.
// Die Apps rendern ihren Text/ihre Icons in einen 64×16-Canvas (LEDText-Logik)
// und geben ihn anschließend über blitCanvas16() aus.

using Canvas16  = cLEDMatrix<64, 16, HORIZONTAL_MATRIX>;

// Gibt den 32×16-Ausschnitt des Canvas über das GLEICHE setPixel()-Mapping aus
// wie Snake. Damit haben Uhrzeit und Wetter exakt dieselbe Ausrichtung.
//   - canvas16 nutzt die LEDText-Konvention y=0 = unten (y-up).
//   - setPixel nutzt die Snake-Konvention y=0 = oben (y-down).
// → vertikale Umrechnung mit (15 - y), keine horizontale Spiegelung.
inline void blitCanvas16(Canvas16& c16) {
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 32; x++)
            setPixel(x, 15 - y, c16(x, y));
}
