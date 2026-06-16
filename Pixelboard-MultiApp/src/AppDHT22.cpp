#include <FastLED.h>
#include <DHT.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include "AppDHT22.h"
#include "PanelUtils.h"

#define DHT_PIN  4
#define DHT_TYPE DHT22

static DHT dht(DHT_PIN, DHT_TYPE);

// ─── Canvas / Panel ───────────────────────────────────────────────────────────
static CRGB canvas8TopLeds   [64 * 8];
static CRGB canvas8BottomLeds[64 * 8];
static CRGB canvas16Leds     [64 * 16];

static cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Top;
static cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Bottom;
static Canvas16 canvas16;

static cLEDText tempText;
static cLEDText scrollText;

// ─── Sensordaten ──────────────────────────────────────────────────────────────
static float dhtTemp = NAN;
static float dhtHum  = NAN;

static char tempBuffer  [8]   = "--C";
static char scrollBuffer[48]  = "   Warte...   ";
static float lastTempDisplayed = -999.0f;

// ─── Thermometer-Icon (8×8) ───────────────────────────────────────────────────
static const uint8_t ICON_THERMO[8] = {
    0x18, 0x24, 0x24, 0x3C, 0x3C, 0x7E, 0x7E, 0x3C
};
static void drawThermoIcon(CRGB color) {
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            bool on = (ICON_THERMO[y] >> (7 - x)) & 1;
            canvas8Top(22 + x, y) = on ? color : CRGB::Black;
        }
}

// ─── Farbverlauf Temperatur (blau = kalt, rot = warm) ────────────────────────
static CRGB getTempColor(float t) {
    const float tMin = -5.0f, tMax = 35.0f;
    float f = (t - tMin) / (tMax - tMin);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    uint8_t hue = (uint8_t)(160.0f * (1.0f - f));
    return CHSV(hue, 255, 255);
}

// ─── Sensor auslesen ─────────────────────────────────────────────────────────
static void sensorLesen() {
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) return;  // alte Werte behalten

    dhtTemp = t;
    dhtHum  = h;

    snprintf(tempBuffer,   sizeof(tempBuffer),   "%.1fC",             dhtTemp);
    snprintf(scrollBuffer, sizeof(scrollBuffer), "   Hum: %.0f%%   Temp: %.1f C   ", dhtHum, dhtTemp);
    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    lastTempDisplayed = -999.0f;  // Farbupdate erzwingen
}

// ─── Display zusammenbauen ───────────────────────────────────────────────────
static void assembleCanvas16() {
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y + 8) = canvas8Top(x, y);
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y) = canvas8Bottom(x, y);
}

static void updateDHT22Display() {
    fill_solid(canvas8TopLeds, 64 * 8, CRGB::Black);

    if (!isnan(dhtTemp) && dhtTemp != lastTempDisplayed) {
        lastTempDisplayed = dhtTemp;
        CRGB tc = getTempColor(dhtTemp);
        tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, tc.r, tc.g, tc.b);
    }
    tempText.SetText((unsigned char*)tempBuffer, strlen(tempBuffer));
    tempText.UpdateText();

    if (scrollText.UpdateText() == -1) {
        scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    }

    assembleCanvas16();
    blitCanvas16(canvas16);
    showLeds();
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void dht22Init() {
    dht.begin();

    canvas8Top.SetLEDArray(canvas8TopLeds);
    canvas8Bottom.SetLEDArray(canvas8BottomLeds);
    canvas16.SetLEDArray(canvas16Leds);

    tempText.SetFont(MatriseFontData);
    tempText.Init(&canvas8Top, 32, 8, 0, 0);
    tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 80, 0);

    scrollText.SetFont(MatriseFontData);
    scrollText.Init(&canvas8Bottom, 32, 8, 0, 0);
    scrollText.SetScrollDirection(SCROLL_LEFT);
    scrollText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 0, 200, 255);

    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
}

// ─── Task ─────────────────────────────────────────────────────────────────────
void taskDHT22(void *pvParameters) {
    unsigned long lastRead = 0;

    while (1) {
        if (currentApp != APP_DHT22) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        updateDHT22Display();

        if (millis() - lastRead >= 2000UL) {
            sensorLesen();
            lastRead = millis();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
