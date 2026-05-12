#include <time.h>
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include "AppUhrzeit.h"
#include "PanelUtils.h"

// ─── Canvas / Panel ───────────────────────────────────────────────────────────
static CRGB canvas8Leds [64 * 8];
static CRGB canvas16Leds[64 * 16];

static cLEDMatrix<64, 8, HORIZONTAL_MATRIX> canvas8;
static Canvas16  canvas16;
static PanelMat  panelTop;
static PanelMat  panelBottom;

static cLEDText uhrzeitText;
static char     uhrzeitStr[6];

static void scaleVertTo16() {
    for (uint8_t y = 0; y < 8; y++) {
        uint8_t y0 = 2 * y, y1 = y0 + 1;
        for (uint8_t x = 0; x < 64; x++) {
            CRGB c = canvas8(x, y);
            canvas16(x, y0) = c;
            canvas16(x, y1) = c;
        }
    }
}

static void updateUhrzeit() {
    struct tm t;
    if (!getLocalTime(&t)) {
        strcpy(uhrzeitStr, "--:--");
    } else {
        char sep = (t.tm_sec % 2 == 0) ? ':' : ' ';
        sprintf(uhrzeitStr, "%02d%c%02d", t.tm_hour, sep, t.tm_min);
    }

    fill_solid(canvas8Leds, 64 * 8, CRGB::Black);
    uhrzeitText.SetText((unsigned char*)uhrzeitStr, strlen(uhrzeitStr));
    uhrzeitText.UpdateText();

    scaleVertTo16();
    verschiebeEineZeileNachUnten(canvas16);
    blitPanels(canvas16, panelTop, panelBottom);
    FastLED.show();
}

void uhrzeitInit() {
    panelTop.SetLEDArray(ledsBottom);
    panelBottom.SetLEDArray(ledsTop);
    canvas8.SetLEDArray(canvas8Leds);
    canvas16.SetLEDArray(canvas16Leds);

    uhrzeitText.SetFont(MatriseFontData);
    uhrzeitText.Init(&canvas8, 64, 8, 0, 0);
    uhrzeitText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 255, 255);
}

void taskUhrzeit(void *pvParameters) {
    while (1) {
        updateUhrzeit();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
