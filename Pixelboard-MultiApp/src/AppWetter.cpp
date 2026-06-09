#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include "AppWetter.h"
#include "PanelUtils.h"

static const char* API_URL =
    "http://api.openweathermap.org/data/2.5/weather"
    "?q=Innsbruck,AT&appid=ec59e8958e52a071ca78979743962031"
    "&units=metric&lang=de";

// ─── Canvas / Panel ───────────────────────────────────────────────────────────
static CRGB canvas8TopLeds   [64 * 8];
static CRGB canvas8BottomLeds[64 * 8];
static CRGB canvas16Leds     [64 * 16];

static cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Top;
static cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Bottom;
static Canvas16 canvas16;

static cLEDText tempText;
static cLEDText scrollText;

// ─── Wetterdaten ──────────────────────────────────────────────────────────────
struct WeatherData {
    char  city[32];
    float temp;
    int   humidity;
    float windSpeed;
    char  description[64];
    int   weatherId;
    bool  valid;
};
static WeatherData weather    = {0};
static char scrollBuffer[160];
static char tempBuffer[8];
static float lastTempDisplayed = -999.0f;  // Caching: SetText nur bei Änderung

// ─── Wetter-Icons (8×8) ───────────────────────────────────────────────────────
static const uint8_t ICON_SUN  [8] = {0x18,0x3C,0xFF,0xFF,0xFF,0xFF,0x3C,0x18};
static const uint8_t ICON_CLOUD[8] = {0x1C,0x3E,0x7F,0xFF,0xFF,0x00,0x00,0x00};
static const uint8_t ICON_RAIN [8] = {0x1C,0x3E,0xFF,0xFF,0x55,0xAA,0x55,0x00};
static const uint8_t ICON_SNOW [8] = {0x24,0x7E,0xDB,0x3C,0x3C,0xDB,0x7E,0x24};
static const uint8_t ICON_STORM[8] = {0x3C,0x7E,0xFF,0xFF,0x18,0x30,0x18,0x00};
static const uint8_t ICON_FOG  [8] = {0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00};

static CRGB getTempColor(float t) {
    // Weicher Verlauf statt harter Stufen: kalt = blau, heiß = rot.
    // Temperatur −5..35 °C wird auf den HSV-Farbton 160 (blau) → 0 (rot)
    // gemappt; Zwischenwerte ergeben Cyan/Grün/Gelb/Orange.
    const float tMin = -5.0f, tMax = 35.0f;
    float f = (t - tMin) / (tMax - tMin);
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    uint8_t hue = (uint8_t)(160.0f * (1.0f - f));
    return CHSV(hue, 255, 255);
}
static const uint8_t* selectIcon(int id) {
    if (id >= 200 && id < 300) return ICON_STORM;
    if (id >= 300 && id < 600) return ICON_RAIN;
    if (id >= 600 && id < 700) return ICON_SNOW;
    if (id >= 700 && id < 800) return ICON_FOG;
    if (id == 800)             return ICON_SUN;
    return ICON_CLOUD;
}
static void drawIcon(const uint8_t* icon, CRGB color) {
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 8; x++) {
            bool on = (icon[y] >> (7 - x)) & 1;
            canvas8Top(22 + x, y) = on ? color : CRGB::Black;
        }
}

static void assembleCanvas16() {
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y + 8) = canvas8Top(x, y);
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y) = canvas8Bottom(x, y);
}

static void fetchWeatherData() {
    if (!wifiOK || WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(API_URL);
    int code = http.GET();
    if (code <= 0) { http.end(); return; }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return;

    strlcpy(weather.city,        doc["name"]                      | "?", sizeof(weather.city));
    strlcpy(weather.description, doc["weather"][0]["description"] | "?", sizeof(weather.description));
    weather.temp      = doc["main"]["temp"]     | 0.0f;
    weather.humidity  = doc["main"]["humidity"] | 0;
    weather.windSpeed = doc["wind"]["speed"]    | 0.0f;
    weather.weatherId = doc["weather"][0]["id"] | 800;
    weather.valid     = true;

    snprintf(tempBuffer, sizeof(tempBuffer), "%.0fC", weather.temp);
    // Temperatur steht jetzt statisch (farbig) oben links → nicht mehr im Lauftext
    snprintf(scrollBuffer, sizeof(scrollBuffer),
             "   %s  %s  Hum:%d%%  Wind:%.1fm/s   ",
             weather.city, weather.description,
             weather.humidity, weather.windSpeed);
    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    lastTempDisplayed = -999.0f;  // Text wurde geändert → neu rendern
}

static void updateWetterDisplay() {
    if (!weather.valid) return;

    fill_solid(canvas8TopLeds, 64 * 8, CRGB::Black);

    // Farbe nur bei Temperaturänderung neu setzen. Den Text dagegen JEDEN Frame
    // neu setzen: nach SetText rendert das erste UpdateText an Position 0 ohne
    // zu scrollen → die Temperatur bleibt statisch stehen statt wegzulaufen.
    if (weather.temp != lastTempDisplayed) {
        lastTempDisplayed = weather.temp;
        CRGB tc = getTempColor(weather.temp);
        tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, tc.r, tc.g, tc.b);
    }
    tempText.SetText((unsigned char*)tempBuffer, strlen(tempBuffer));
    tempText.UpdateText();

    drawIcon(selectIcon(weather.weatherId), CRGB::White);

    if (scrollText.UpdateText() == -1) {
        scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    }

    assembleCanvas16();
    blitCanvas16(canvas16);   // gleiche Ausrichtung wie Snake (setPixel)
    showLeds();
}

void wetterInit() {
    canvas8Top.SetLEDArray(canvas8TopLeds);
    canvas8Bottom.SetLEDArray(canvas8BottomLeds);
    canvas16.SetLEDArray(canvas16Leds);

    tempText.SetFont(MatriseFontData);
    tempText.Init(&canvas8Top, 24, 8, 0, 0);
    tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 255, 0);

    scrollText.SetFont(MatriseFontData);
    scrollText.Init(&canvas8Bottom, 64, 8, 0, 0);
    scrollText.SetScrollDirection(SCROLL_LEFT);
    scrollText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 255, 255);

    strncpy(scrollBuffer, "   Verbinde...   ", sizeof(scrollBuffer));
    strncpy(tempBuffer,   "--C",               sizeof(tempBuffer));
    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    weather.valid     = true;
    weather.weatherId = 802;
    weather.temp      = 0.0f;
}

void taskWetter(void *pvParameters) {
    unsigned long lastFetch = 0;
    bool          fetchedOnce = false;

    while (1) {
        // Nur zeichnen/abrufen, wenn diese App aktiv ist – kein HTTP-Block beim
        // Boot und keine Wetter-Frames im gemeinsamen LED-Puffer während Snake.
        if (currentApp != APP_WETTER) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        updateWetterDisplay();   // sofortiges Bild ("Verbinde..." / letzte Daten)

        if (!fetchedOnce || millis() - lastFetch > 600000UL) {
            fetchWeatherData();
            lastFetch   = millis();
            fetchedOnce = true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
