/**
 * @file DHT22_Pixelboard_ESP32.cpp
 * @brief DHT22 Temperatur & Luftfeuchtigkeit auf 64x16 LED Pixelboard mit ESP32
 *
 * Anzeige wechselt alle 3 Sekunden zwischen:
 *   - Temperatur:       "T:23.5"  (orange)
 *   - Luftfeuchtigkeit: "H:61.2"  (cyan)
 *
 * Benoetigte Bibliotheken (Arduino Library Manager):
 *   - FastLED
 *   - DHT sensor library  (by Adafruit)
 *   - Adafruit Unified Sensor
 *
 * Lokale Header (im Projektordner):
 *   - LEDMatrix.h
 *   - LEDText.h
 *   - FontMatrise.h
 */

#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include <DHT.h>

// =============================================================================
// --- BENUTZER EINSTELLUNGEN --------------------------------------------------
// =============================================================================

#define DHT_PIN             4       // GPIO-Pin des DHT22 Datenkabels
#define DHT_TYPE            DHT22

#define ANZEIGE_WECHSEL_MS  3000    // Wie lange jeder Wert angezeigt wird (ms)
#define SENSOR_INTERVALL_MS 2000    // Wie oft der Sensor abgefragt wird (ms)
#define DISPLAY_INTERVALL_MS  50    // Display-Refresh (ms)

// Farben
CRGB farbeTemperatur  = CRGB(255, 80,   0);   // Orange  -> Temperatur
CRGB farbeLuftfeuchte = CRGB(  0, 200, 255);  // Cyan    -> Luftfeuchtigkeit

// =============================================================================
// --- HARDWARE KONFIGURATION --------------------------------------------------
// =============================================================================

#define pinTop         25
#define pinBottom      26

#define panelWidth     32
#define panelHeight     8

#define ledsPerPanel   (panelWidth * panelHeight)
#define brightness     25
#define colorOrder     GRB
#define chipset        WS2812

// --- Virtuelles Canvas -------------------------------------------------------
#define canvasWidth8    64
#define canvasHeight8    8

#define canvasWidth16   64
#define canvasHeight16  16

CRGB canvas8Leds [canvasWidth8  * canvasHeight8 ];
CRGB canvas16Leds[canvasWidth16 * canvasHeight16];

cLEDMatrix<canvasWidth8,  canvasHeight8,  HORIZONTAL_MATRIX> canvas8;
cLEDMatrix<canvasWidth16, canvasHeight16, HORIZONTAL_MATRIX> canvas16;

// --- Physische Panels --------------------------------------------------------
CRGB ledsTop   [ledsPerPanel];
CRGB ledsBottom[ledsPerPanel];

cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelTop;
cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelBottom;

// --- Objekte -----------------------------------------------------------------
cLEDText sensorText;
DHT      dht(DHT_PIN, DHT_TYPE);

// --- Zustandsvariablen -------------------------------------------------------
static uint32_t lastDisplayUpdate  = 0;
static uint32_t lastSensorRead     = 0;
static uint32_t lastAnzeigeWechsel = 0;

static float aktTemperatur  = NAN;
static float aktLuftfeuchte = NAN;

static bool zeigeFeuchte = false;   // false = Temp, true = Feuchte

static char  anzeigeString[10];
static char  letzterString[10] = "";
static CRGB  aktFarbe;
static bool  textNeuSetzen = true;  // Beim Start direkt Text laden

// --- Prototypen --------------------------------------------------------------
static void initAnzeige();
static void sensorLesen();
static void anzeigeAktualisieren();
static void scaleVertTo16();
static void verschiebeCanvas16EineZeileNachUnten();
static void blitPanelsFromCanvas16();
static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);
static void rotatePanel180      (cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);

static inline void clearCanvas8() {
  fill_solid(canvas8Leds,  canvasWidth8  * canvasHeight8,  CRGB::Black);
}
static inline void clearCanvas16() {
  fill_solid(canvas16Leds, canvasWidth16 * canvasHeight16, CRGB::Black);
}

// =============================================================================
// --- SETUP -------------------------------------------------------------------
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("DHT22 Pixelboard startet..."));

  dht.begin();
  Serial.println(F("DHT22 initialisiert."));

  initAnzeige();

  // DHT22 braucht ~2 Sekunden nach Power-On bis zur ersten validen Messung
  delay(2000);
  sensorLesen();

  Serial.println(F("Bereit!"));
}

// =============================================================================
// --- LOOP --------------------------------------------------------------------
// =============================================================================

void loop() {
  const uint32_t now = millis();

  // --- Sensor periodisch auslesen -------------------------------------------
  if (now - lastSensorRead >= SENSOR_INTERVALL_MS) {
    lastSensorRead = now;
    sensorLesen();
  }

  // --- Anzeigemodus wechseln ------------------------------------------------
  if (now - lastAnzeigeWechsel >= ANZEIGE_WECHSEL_MS) {
    lastAnzeigeWechsel = now;
    zeigeFeuchte  = !zeigeFeuchte;
    textNeuSetzen = true;   // Farbe/Inhalt hat sich geaendert -> neu laden
  }

  // --- Display aktualisieren ------------------------------------------------
  if (now - lastDisplayUpdate >= DISPLAY_INTERVALL_MS) {
    lastDisplayUpdate = now;
    anzeigeAktualisieren();
  }
}

// =============================================================================
// --- SENSOR LESEN ------------------------------------------------------------
// =============================================================================

static void sensorLesen() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println(F("WARNUNG: DHT22 Lesung fehlgeschlagen! Pruefe Verkabelung."));
    // Alte Werte behalten -> Anzeige bleibt sichtbar statt leer
    return;
  }

  aktTemperatur  = t;
  aktLuftfeuchte = h;

  Serial.print(F("Temperatur: "));
  Serial.print(aktTemperatur, 1);
  Serial.print(F(" C  |  Luftfeuchtigkeit: "));
  Serial.print(aktLuftfeuchte, 1);
  Serial.println(F(" %"));

  // Wenn sich der Messwert geaendert hat -> Text neu rendern
  textNeuSetzen = true;
}

// =============================================================================
// --- ANZEIGE AKTUALISIEREN ---------------------------------------------------
// =============================================================================

static void anzeigeAktualisieren() {

  // --- Anzeigestring und Farbe zusammenbauen --------------------------------
  if (!zeigeFeuchte) {
    if (isnan(aktTemperatur)) {
      strcpy(anzeigeString, "T:---");
    } else {
      char wert[7];
      dtostrf(aktTemperatur, 4, 1, wert);
      snprintf(anzeigeString, sizeof(anzeigeString), "T:%s", wert);
    }
    aktFarbe = farbeTemperatur;
  } else {
    if (isnan(aktLuftfeuchte)) {
      strcpy(anzeigeString, "H:---");
    } else {
      char wert[7];
      dtostrf(aktLuftfeuchte, 4, 1, wert);
      snprintf(anzeigeString, sizeof(anzeigeString), "H:%s", wert);
    }
    aktFarbe = farbeLuftfeuchte;
  }

  // --- Neu laden wenn sich Text oder Farbe geaendert hat -------------------
  // Verhindert unnoetige SetText-Aufrufe mitten im laufenden Render-Zyklus
  if (textNeuSetzen || strcmp(anzeigeString, letzterString) != 0) {
    textNeuSetzen = false;
    strcpy(letzterString, anzeigeString);

    sensorText.SetTextColrOptions(COLR_RGB | COLR_SINGLE,
                                  aktFarbe.r, aktFarbe.g, aktFarbe.b);
    sensorText.SetText((unsigned char*)anzeigeString, strlen(anzeigeString));

    Serial.print(F("Zeige: "));
    Serial.println(anzeigeString);
  }

  // --- Canvas loeschen und Text rendern ------------------------------------
  clearCanvas8();

  // UpdateText() gibt -1 zurueck wenn der Text komplett gerender wurde.
  // Sofort wieder SetText() aufrufen damit die Anzeige dauerhaft stehen
  // bleibt (statisch, kein Scrollen).
  if (sensorText.UpdateText() == -1) {
    sensorText.SetText((unsigned char*)anzeigeString, strlen(anzeigeString));
  }

  // --- Auf 16px skalieren, verschieben, auf Panels blitten -----------------
  scaleVertTo16();
  verschiebeCanvas16EineZeileNachUnten();
  blitPanelsFromCanvas16();

  FastLED.show();
}

// =============================================================================
// --- ANZEIGE INITIALISIERUNG -------------------------------------------------
// =============================================================================

static void initAnzeige() {
  Serial.println(F("Initialisiere LEDs..."));

  FastLED.addLeds<chipset, pinTop,    colorOrder>(ledsTop,    ledsPerPanel);
  FastLED.addLeds<chipset, pinBottom, colorOrder>(ledsBottom, ledsPerPanel);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);

  // Panel-Objekte auf LED-Arrays mappen (Verkabelungsreihenfolge beachten)
  panelTop.SetLEDArray(ledsBottom);
  panelBottom.SetLEDArray(ledsTop);

  canvas8.SetLEDArray(canvas8Leds);
  canvas16.SetLEDArray(canvas16Leds);

  // Text-Objekt initialisieren
  sensorText.SetFont(MatriseFontData);
  sensorText.Init(&canvas8, canvas8.Width(), canvas8.Height(), 0, 0);

  // Startfarbe setzen (Temperatur = Orange)
  sensorText.SetTextColrOptions(COLR_RGB | COLR_SINGLE,
                                farbeTemperatur.r,
                                farbeTemperatur.g,
                                farbeTemperatur.b);

  Serial.println(F("LEDs bereit."));
}

// =============================================================================
// --- HARDWARE HELPER FUNKTIONEN ----------------------------------------------
// (identisch zum Uhrzeit-Projekt - bewaehrte Konfiguration beibehalten)
// =============================================================================

static void scaleVertTo16() {
  for (uint8_t y8 = 0; y8 < canvasHeight8; y8++) {
    const uint8_t y16a = 2 * y8;
    const uint8_t y16b = y16a + 1;
    for (uint8_t x = 0; x < canvasWidth8; x++) {
      const CRGB c = canvas8(x, y8);
      canvas16(x, y16a) = c;
      canvas16(x, y16b) = c;
    }
  }
}

static void verschiebeCanvas16EineZeileNachUnten() {
  for (int y = canvasHeight16 - 1; y > 0; y--) {
    for (uint8_t x = 0; x < canvasWidth16; x++) {
      canvas16(x, y) = canvas16(x, y - 1);
    }
  }
  for (uint8_t x = 0; x < canvasWidth16; x++) {
    canvas16(x, 0) = CRGB::Black;
  }
}

static void blitPanelsFromCanvas16() {
  // Oberes physisches Panel bekommt logisch untere Haelfte des Canvas
  for (uint8_t y = 0; y < panelHeight; y++) {
    const uint8_t ySrc = y + panelHeight;
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelTop(x, y) = canvas16(x, ySrc);
    }
  }

  // Unteres physisches Panel bekommt logisch obere Haelfte des Canvas
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelBottom(x, y) = canvas16(x, y);
    }
  }

  // Spiegelung an der Y-Achse fuer beide Panels
  mirrorPanelHorizontal(panelTop);
  mirrorPanelHorizontal(panelBottom);

  // Kopfueber montiertes Panel drehen
  rotatePanel180(panelTop);
}

static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      const uint8_t xo = panelWidth - 1 - x;
      CRGB tmp    = panel(x, y);
      panel(x, y)  = panel(xo, y);
      panel(xo, y) = tmp;
    }
  }
}

static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      const uint8_t xo = panelWidth  - 1 - x;
      const uint8_t yo = panelHeight - 1 - y;
      CRGB tmp     = panel(x, y);
      panel(x, y)   = panel(xo, yo);
      panel(xo, yo) = tmp;
    }
  }
}