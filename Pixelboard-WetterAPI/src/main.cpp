#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>

// =============================================================================
// --- WLAN & API EINSTELLUNGEN ------------------------------------------------
// =============================================================================

const char* ssid       = "iPhone von Paul";
const char* password   = "rootroot";
const char* apiKey     = "ec59e8958e52a071ca78979743962031";
const char* city       = "Innsbruck";
const char* countryCode = "AT";

String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" +
                    String(city) + "," + String(countryCode) +
                    "&appid=" + String(apiKey) + "&units=metric&lang=de";

// =============================================================================
// --- HARDWARE KONFIGURATION --------------------------------------------------
// =============================================================================

#define pinTop       25
#define pinBottom    26
#define panelWidth   32
#define panelHeight   8
#define ledsPerPanel (panelWidth * panelHeight)
#define brightness   25

// Zwei separate 64x8 Canvases (je eines pro physischem Panel)
CRGB canvas8TopLeds[64 * 8];
CRGB canvas8BottomLeds[64 * 8];
CRGB canvas16Leds[64 * 16];

cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Top;
cLEDMatrix<64, 8,  HORIZONTAL_MATRIX> canvas8Bottom;
cLEDMatrix<64, 16, HORIZONTAL_MATRIX> canvas16;

CRGB ledsTop[ledsPerPanel];
CRGB ledsBottom[ledsPerPanel];

cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelTop;
cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelBottom;

// =============================================================================
// --- TEXT OBJEKTE ------------------------------------------------------------
// =============================================================================

cLEDText tempText;    // Temperatur (statisch, oben links, 24px breit)
cLEDText scrollText;  // Scrolltext (unten, 64px Scroll-Puffer)

// =============================================================================
// --- WETTERDATEN STRUCT ------------------------------------------------------
// =============================================================================

struct WeatherData {
    char  city[32];
    float temp;
    int   humidity;
    float windSpeed;
    char  description[64];
    int   weatherId;
    bool  valid;
};
WeatherData weather = {0};

char scrollBuffer[160];  // "   Innsbruck  14C  leichter Regen  Hum:75%  Wind:3.2m/s   "
char tempBuffer[8];      // "14C"

// =============================================================================
// --- TIMER -------------------------------------------------------------------
// =============================================================================

unsigned long lastWeatherUpdate = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long weatherInterval = 600000;  // 10 Minuten
const int           displayInterval = 50;      // ms

// =============================================================================
// --- WETTER-ICONS (8x8 Pixel-Bitmaps) ----------------------------------------
// 1 Bit = 1 Pixel, MSB = links, jede Zeile = 1 Byte
// =============================================================================

const uint8_t ICON_SUN[8]   = {0x18, 0x3C, 0xFF, 0xFF, 0xFF, 0xFF, 0x3C, 0x18};
const uint8_t ICON_CLOUD[8] = {0x1C, 0x3E, 0x7F, 0xFF, 0xFF, 0x00, 0x00, 0x00};
const uint8_t ICON_RAIN[8]  = {0x1C, 0x3E, 0xFF, 0xFF, 0x55, 0xAA, 0x55, 0x00};
const uint8_t ICON_SNOW[8]  = {0x24, 0x7E, 0xDB, 0x3C, 0x3C, 0xDB, 0x7E, 0x24};
const uint8_t ICON_STORM[8] = {0x3C, 0x7E, 0xFF, 0xFF, 0x18, 0x30, 0x18, 0x00};
const uint8_t ICON_FOG[8]   = {0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00};

// =============================================================================
// --- PROTOTYPEN --------------------------------------------------------------
// =============================================================================

static void initLEDs();
static void initWiFiAndFetch();
static void getWeatherData();
static void updateDisplay();
static void drawIcon(const uint8_t* icon, CRGB color);
static CRGB getTempColor(float t);
static const uint8_t* selectIcon(int id);
static void assembleCanvas16();
static void verschiebeCanvas16EineZeileNachUnten();
static void blitPanelsFromCanvas16();
static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);
static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);

// =============================================================================
// --- SETUP & LOOP ------------------------------------------------------------
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println(F("\n=== ESP32 Wetter-Pixelboard ==="));

    initLEDs();
    initWiFiAndFetch();
}

void loop() {
    // Wetter alle 10 Minuten aktualisieren
    if ((millis() - lastWeatherUpdate) > weatherInterval) {
        getWeatherData();
        lastWeatherUpdate = millis();
    }

    // Display jeden Frame aktualisieren
    updateDisplay();
}

// =============================================================================
// --- WIFI & WETTERDATEN ------------------------------------------------------
// =============================================================================

static void initWiFiAndFetch() {
    Serial.println(F("Verbinde mit WiFi..."));
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(F("\nWiFi verbunden!"));
        Serial.print(F("IP: "));
        Serial.println(WiFi.localIP());
        delay(2000);
        getWeatherData();
        lastWeatherUpdate = millis();
    } else {
        Serial.println(F("\nWiFi Verbindung fehlgeschlagen!"));
    }
}

static void getWeatherData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("WiFi nicht verbunden!"));
        return;
    }

    HTTPClient http;
    Serial.println(F("\n=== Wetterdaten abrufen ==="));
    http.begin(serverPath.c_str());
    int code = http.GET();

    if (code <= 0) {
        Serial.print(F("HTTP Fehler: "));
        Serial.println(code);
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print(F("JSON Fehler: "));
        Serial.println(err.c_str());
        return;
    }

    // Daten extrahieren
    strlcpy(weather.city,        doc["name"]                      | "?",  sizeof(weather.city));
    strlcpy(weather.description, doc["weather"][0]["description"] | "?",  sizeof(weather.description));
    weather.temp      = doc["main"]["temp"]     | 0.0f;
    weather.humidity  = doc["main"]["humidity"] | 0;
    weather.windSpeed = doc["wind"]["speed"]    | 0.0f;
    weather.weatherId = doc["weather"][0]["id"] | 800;
    weather.valid     = true;

    // Temp-Buffer für oberes Panel (z.B. "14C")
    snprintf(tempBuffer, sizeof(tempBuffer), "%.0fC", weather.temp);

    // Scrolltext für unteres Panel
    snprintf(scrollBuffer, sizeof(scrollBuffer),
        "   %s  %.0fC  %s  Hum:%d%%  Wind:%.1fm/s   ",
        weather.city, weather.temp, weather.description,
        weather.humidity, weather.windSpeed);

    // Scrolltext neu starten
    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));

    // Serial Ausgabe
    Serial.println(F("\n╔═══════════════════════════════════════╗"));
    Serial.println(F("║       WETTERDATEN INNSBRUCK           ║"));
    Serial.println(F("╚═══════════════════════════════════════╝"));
    Serial.print(F("Stadt: "));        Serial.println(weather.city);
    Serial.print(F("Temperatur: "));   Serial.print(weather.temp);   Serial.println(F(" °C"));
    Serial.print(F("Luftfeucht.: ")); Serial.print(weather.humidity); Serial.println(F(" %"));
    Serial.print(F("Wind: "));        Serial.print(weather.windSpeed); Serial.println(F(" m/s"));
    Serial.print(F("Beschreibung: ")); Serial.println(weather.description);
    Serial.print(F("WeatherID: "));   Serial.println(weather.weatherId);
    Serial.println(F("───────────────────────────────────────"));
}

// =============================================================================
// --- LED INITIALISIERUNG -----------------------------------------------------
// =============================================================================

static void initLEDs() {
    FastLED.addLeds<WS2812, pinTop,    GRB>(ledsTop,    ledsPerPanel);
    FastLED.addLeds<WS2812, pinBottom, GRB>(ledsBottom, ledsPerPanel);
    FastLED.setBrightness(brightness);
    FastLED.clear(true);

    // Panel-Objekte auf LED-Arrays mappen (swap wie in Referenzprojekten)
    panelTop.SetLEDArray(ledsBottom);
    panelBottom.SetLEDArray(ledsTop);

    // Canvas-Objekte initialisieren
    canvas8Top.SetLEDArray(canvas8TopLeds);
    canvas8Bottom.SetLEDArray(canvas8BottomLeds);
    canvas16.SetLEDArray(canvas16Leds);

    // Temperatur-Text: statisch, links oben, 24px breit
    tempText.SetFont(MatriseFontData);
    tempText.Init(&canvas8Top, 24, 8, 0, 0);
    tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 255, 0);

    // Scroll-Text: scrollend links, voller 64px Puffer
    // WICHTIG: SetText VOR dem ersten UpdateText aufrufen!
    scrollText.SetFont(MatriseFontData);
    scrollText.Init(&canvas8Bottom, 64, 8, 0, 0);
    scrollText.SetScrollDirection(SCROLL_LEFT);
    scrollText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 255, 255, 255);

    // Lade-Text sofort anzeigen (vor Wetterdaten)
    strncpy(scrollBuffer, "   Verbinde...   WLAN   ", sizeof(scrollBuffer));
    strncpy(tempBuffer,   "--C",                       sizeof(tempBuffer));
    scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));

    // Platzhalter-Werte setzen damit updateDisplay() sofort zeichnet
    weather.valid     = true;
    weather.weatherId = 802;   // Wolken als Standard-Icon
    weather.temp      = 0.0f;

    Serial.println(F("LED-Anzeige initialisiert."));

    // Kurzer LED-Test: alle LEDs kurz weiß aufleuchten lassen
    fill_solid(ledsTop,    ledsPerPanel, CRGB::White);
    fill_solid(ledsBottom, ledsPerPanel, CRGB::White);
    FastLED.show();
    delay(500);
    FastLED.clear(true);
    FastLED.show();
}

// =============================================================================
// --- DISPLAY UPDATE ----------------------------------------------------------
// =============================================================================

static void updateDisplay() {
    if ((millis() - lastDisplayUpdate) < (unsigned long)displayInterval) return;
    lastDisplayUpdate = millis();

    if (!weather.valid) return;

    // --- OBERES PANEL: Temperatur (links) + Icon (rechts) ---
    fill_solid(canvas8TopLeds, 64 * 8, CRGB::Black);

    // Temperaturfarbe setzen und Text einmal statisch rendern
    CRGB tempColor = getTempColor(weather.temp);
    tempText.SetTextColrOptions(COLR_RGB | COLR_SINGLE,
                                tempColor.r, tempColor.g, tempColor.b);
    tempText.SetText((unsigned char*)tempBuffer, strlen(tempBuffer));
    tempText.UpdateText();

    // Wetter-Icon rechts (x=24..31) zeichnen
    drawIcon(selectIcon(weather.weatherId), CRGB::White);

    // --- UNTERES PANEL: Scrolltext ---
    if (scrollText.UpdateText() == -1) {
        scrollText.SetText((unsigned char*)scrollBuffer, strlen(scrollBuffer));
    }

    // --- Canvas16 zusammenbauen & anzeigen ---
    assembleCanvas16();
    verschiebeCanvas16EineZeileNachUnten();
    blitPanelsFromCanvas16();
    FastLED.show();
}

// =============================================================================
// --- HILFS-FUNKTIONEN --------------------------------------------------------
// =============================================================================

static CRGB getTempColor(float t) {
    if (t <  0) return CRGB(0,   0,   255);  // Blau     – Frost
    if (t <  5) return CRGB(0,   128, 255);  // Hellblau – kalt
    if (t < 15) return CRGB(0,   255, 255);  // Cyan     – kühl
    if (t < 22) return CRGB(255, 255, 0);    // Gelb     – mild
    if (t < 28) return CRGB(255, 128, 0);    // Orange   – warm
    return          CRGB(255, 0,   0);        // Rot      – heiß
}

static const uint8_t* selectIcon(int id) {
    if (id >= 200 && id < 300) return ICON_STORM;
    if (id >= 300 && id < 600) return ICON_RAIN;
    if (id >= 600 && id < 700) return ICON_SNOW;
    if (id >= 700 && id < 800) return ICON_FOG;
    if (id == 800)             return ICON_SUN;
    return ICON_CLOUD;  // 801-804
}

static void drawIcon(const uint8_t* icon, CRGB color) {
    // Icon in canvas8Top bei x=24..31, y=0..7 einzeichnen
    for (uint8_t y = 0; y < 8; y++) {
        for (uint8_t x = 0; x < 8; x++) {
            bool on = (icon[y] >> (7 - x)) & 1;
            canvas8Top(22 + x, y) = on ? color : CRGB::Black;
        }
    }
}

// Ersetzt scaleVertTo16(): zwei 64x8 Canvases direkt in canvas16 kopieren
static void assembleCanvas16() {
    // canvas8Top  → canvas16 y=8..15 (physisch oberes Panel)
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y + 8) = canvas8Top(x, y);

    // canvas8Bottom → canvas16 y=0..7 (physisch unteres Panel)
    for (uint8_t y = 0; y < 8; y++)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y) = canvas8Bottom(x, y);
}

// =============================================================================
// --- HARDWARE HELFER (unverändert aus Pixelboard-Zeitausgabe) ----------------
// =============================================================================

static void verschiebeCanvas16EineZeileNachUnten() {
    for (int y = 15; y > 0; y--)
        for (uint8_t x = 0; x < 64; x++)
            canvas16(x, y) = canvas16(x, y - 1);
    for (uint8_t x = 0; x < 64; x++)
        canvas16(x, 0) = CRGB::Black;
}

static void blitPanelsFromCanvas16() {
    // Oberes physisches Panel (Pin 26) bekommt canvas16 unten (y=8..15)
    for (uint8_t y = 0; y < panelHeight; y++)
        for (uint8_t x = 0; x < panelWidth; x++)
            panelTop(x, y) = canvas16(x, y + panelHeight);

    // Unteres physisches Panel (Pin 25) bekommt canvas16 oben (y=0..7)
    for (uint8_t y = 0; y < panelHeight; y++)
        for (uint8_t x = 0; x < panelWidth; x++)
            panelBottom(x, y) = canvas16(x, y);

    mirrorPanelHorizontal(panelTop);
    mirrorPanelHorizontal(panelBottom);
    rotatePanel180(panelTop);
}

static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
    for (uint8_t y = 0; y < panelHeight; y++) {
        for (uint8_t x = 0; x < panelWidth / 2; x++) {
            const uint8_t xo = panelWidth - 1 - x;
            CRGB tmp    = panel(x, y);
            panel(x, y) = panel(xo, y);
            panel(xo, y) = tmp;
        }
    }
}

static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
    for (uint8_t y = 0; y < panelHeight; y++) {
        for (uint8_t x = 0; x < panelWidth / 2; x++) {
            const uint8_t xo = panelWidth - 1 - x;
            const uint8_t yo = panelHeight - 1 - y;
            CRGB tmp     = panel(x, y);
            panel(x, y)  = panel(xo, yo);
            panel(xo, yo) = tmp;
        }
    }
}
