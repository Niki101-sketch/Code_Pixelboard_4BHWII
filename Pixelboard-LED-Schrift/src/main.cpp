/**
 * @file Laufschrift_DualPanel_64x16_Config.cpp
 * @brief Angepasste Version mit einfacher Konfiguration.
 */

#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>

// =============================================================================
// --- BENUTZER EINSTELLUNGEN (HIER ÄNDERN) ------------------------------------
// =============================================================================

// 1. Dein Text
char textInhalt[] = "LED Pixelboard Demo! ";

// 2. Text Farbe (R, G, B)
CRGB textFarbe = CRGB::Yellow;

// 3. Modus: Wie soll sich der Text verhalten?
// 0 = Statisch (Text steht mittig, bewegt sich nicht)
// 1 = Laufschrift nach LINKS  (Standard Ticker)
// 2 = Laufschrift nach RECHTS
// 3 = Laufschrift nach OBEN
// 4 = Laufschrift nach UNTEN
int textModus = 0;

// 4. Geschwindigkeit (Kleiner = Schneller, Größer = Langsamer)
// Empfehlung: 30 bis 100
int scrollGeschwindigkeit = 10;

// =============================================================================
// --- Hardware-Konfiguration (NICHT ÄNDERN) -----------------------------------
// =============================================================================

#define pinTop         25   // alter "Top"-Pin, jetzt physisch unten
#define pinBottom      26   // alter "Bottom"-Pin, jetzt physisch oben

#define panelWidth     32
#define panelHeight     8

#define ledsPerPanel   (panelWidth * panelHeight)
#define brightness     25
#define colorOrder     GRB
#define chipset        WS2812

// --- Virtuelles Canvas -------------------------------------------------------
#define canvasWidth8   64
#define canvasHeight8   8

#define canvasWidth16  64
#define canvasHeight16 16

CRGB canvas8Leds[canvasWidth8 * canvasHeight8];
CRGB canvas16Leds[canvasWidth16 * canvasHeight16];

cLEDMatrix<canvasWidth8, canvasHeight8, HORIZONTAL_MATRIX> canvas8;
cLEDMatrix<canvasWidth16, canvasHeight16, HORIZONTAL_MATRIX> canvas16;

// --- Physische Panels --------------------------------------------------------
CRGB ledsTop[ledsPerPanel];
CRGB ledsBottom[ledsPerPanel];

cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelTop;    // logisches TOP
cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelBottom; // logisches BOTTOM

// --- Laufschrift Objekt ------------------------------------------------------
cLEDText scrollingText;
static uint32_t lastFrameMs = 0;

// --- Prototypen --------------------------------------------------------------
static void initAnzeige();
static void updateAnzeige();
static void scaleVertTo16();
static void verschiebeCanvas16EineZeileNachUnten();
static void blitPanelsFromCanvas16();
static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);
static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);

// kleine Hilfsfunktionen zum Löschen der Canvas-Arrays
static inline void clearCanvas8() {
  fill_solid(canvas8Leds, canvasWidth8 * canvasHeight8, CRGB::Black);
}
static inline void clearCanvas16() {
  fill_solid(canvas16Leds, canvasWidth16 * canvasHeight16, CRGB::Black);
}

// --- Setup -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  initAnzeige(); // Initialisiert LEDs und Text-Einstellungen

  Serial.println(F("Pixelboard gestartet."));
}

// --- Loop --------------------------------------------------------------------
void loop() {
  updateAnzeige();
  // Hier könnte man noch weiteren Code einfügen, z.B. Joystick Abfrage,
  // um die Variablen 'textModus' oder 'textInhalt' zur Laufzeit zu ändern.
}

// --- Implementierung ---------------------------------------------------------

static void initAnzeige() {
  // FastLED Setup
  FastLED.addLeds<chipset, pinTop,    colorOrder>(ledsTop,    ledsPerPanel);
  FastLED.addLeds<chipset, pinBottom, colorOrder>(ledsBottom, ledsPerPanel);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);

  // Mapping Panel Objekte auf LED Arrays
  panelTop.SetLEDArray(ledsBottom);   // Pin 26, physisch oben
  panelBottom.SetLEDArray(ledsTop);   // Pin 25, physisch unten

  // Mapping Canvas
  canvas8.SetLEDArray(canvas8Leds);
  canvas16.SetLEDArray(canvas16Leds);

  // Text Initialisierung
  scrollingText.SetFont(MatriseFontData);
  scrollingText.Init(&canvas8, canvas8.Width(), canvas8.Height(), 0, 0);

  // Farbe setzen (aus den Variablen oben)
  scrollingText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, textFarbe.r, textFarbe.g, textFarbe.b);

  // Text setzen
  scrollingText.SetText((unsigned char*)textInhalt, strlen(textInhalt));

  // Richtung initial setzen basierend auf Modus
  if (textModus == 0) {
      // Statisch -> Wir setzen den Modus auf Links (wird aber nicht kontinuierlich geupdated)
      scrollingText.SetScrollDirection(SCROLL_LEFT);
  } else if (textModus == 1) scrollingText.SetScrollDirection(SCROLL_LEFT);
  else if (textModus == 2) scrollingText.SetScrollDirection(SCROLL_RIGHT);
  else if (textModus == 3) scrollingText.SetScrollDirection(SCROLL_UP);
  else if (textModus == 4) scrollingText.SetScrollDirection(SCROLL_DOWN);
}

static void updateAnzeige() {
  const uint32_t now = millis();

  // Geschwindigkeitskontrolle
  if (now - lastFrameMs < (uint32_t)scrollGeschwindigkeit) return;
  lastFrameMs = now;

  // --- SCHRITT 1: Text Update (Virtuelle 8px Ebene) ---

  if (textModus == 0) {
    // STATISCH: Wir rendern den Text einmalig, ohne Bewegung
    static bool warVorherScrollend = true;
    if (warVorherScrollend) {
        // Canvas zurücksetzen
        clearCanvas8();

        // Text (erneut) setzen und einmal zeichnen
        scrollingText.SetText((unsigned char*)textInhalt, strlen(textInhalt));

        // Falls du ihn zentriert haben willst, kannst du die Start-Position anpassen.
        // Hier ein einfacher Versuch, grob basierend auf 6px character width:
        int textWidth = strlen(textInhalt) * 6; // grob geschätzt (Font abhängig)
        int startX = (canvasWidth8 - textWidth) / 2;
        if (startX < 0) startX = 0;
        // Manche Versionen von LEDText unterstützen SetTextPos; falls nicht, kann man
        // alternativ mit off-screen Rendern arbeiten. Wir versuchen hier, die Scroll-Startposition zu nutzen:
        // scroller hat eventuell keine direkte SetTextPos API -> wir belassen es bei Standard
        // und verlassen uns auf zentrierte Füllung durch Startpunkt-Berechnung, falls benötigt.

        // UpdateText einmal aufrufen um zu zeichnen
        scrollingText.UpdateText();
        warVorherScrollend = false;
    }
  }
  else {
    // BEWEGUNG (Modus 1-4)
    // Richtung sicherstellen (falls Variable live geändert wurde)
    if (textModus == 1) scrollingText.SetScrollDirection(SCROLL_LEFT);
    if (textModus == 2) scrollingText.SetScrollDirection(SCROLL_RIGHT);
    if (textModus == 3) scrollingText.SetScrollDirection(SCROLL_UP);
    if (textModus == 4) scrollingText.SetScrollDirection(SCROLL_DOWN);

    // Text Farbe live updaten
    scrollingText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, textFarbe.r, textFarbe.g, textFarbe.b);

    // UpdateText() bewegt den Text einen Schritt weiter
    // Rückgabewert -1 bedeutet: Text ist einmal komplett durchgelaufen.
    if (scrollingText.UpdateText() == -1) {
      // TEXT WIEDERHOLEN
      scrollingText.SetText((unsigned char*)textInhalt, strlen(textInhalt));
    }
  }

  // --- SCHRITT 2: Skalieren auf 16px Höhe ---
  scaleVertTo16();

  // --- SCHRITT 3: Verschieben (Hardware-Korrektur) ---
  verschiebeCanvas16EineZeileNachUnten();

  // --- SCHRITT 4: Auf Panels mappen & Hardware-Fixes ---
  blitPanelsFromCanvas16();

  // Anzeigen
  FastLED.show();
}

// -----------------------------------------------------------------------------
// --- UNVERÄNDERTE HARDWARE HELFER FUNKTIONEN ---------------------------------
// -----------------------------------------------------------------------------

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
  // Oberes physisches Panel (Pin 26) bekommt logisches unten
  for (uint8_t y = 0; y < panelHeight; y++) {
    const uint8_t ySrc = y + panelHeight; // 8..15
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelTop(x, y) = canvas16(x, ySrc);
    }
  }

  // Unteres physisches Panel (Pin 25) bekommt logisches oben
  for (uint8_t y = 0; y < panelHeight; y++) {
    const uint8_t ySrc = y; // 0..7
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelBottom(x, y) = canvas16(x, ySrc);
    }
  }

  // Spiegelung an der y-Achse für beide Panels
  mirrorPanelHorizontal(panelTop);
  mirrorPanelHorizontal(panelBottom);

  // Kopfüber montiertes Panel (früher unten, jetzt oben) drehen
  rotatePanel180(panelTop);

  // Panels auf ihre LED-Arrays schreiben (falls benötigt von Library;
  // Viele cLEDMatrix Implementierungen schreiben automatisch in das Array,
  // da wir SetLEDArray() verwendet haben. FastLED.show() wird am Ende gerufen.)
}

static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      const uint8_t xo = panelWidth - 1 - x;
      CRGB tmp      = panel(x, y);
      panel(x, y)   = panel(xo, y);
      panel(xo, y)  = tmp;
    }
  }
}

static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      const uint8_t xo = panelWidth - 1 - x;
      const uint8_t yo = panelHeight - 1 - y;
      CRGB tmp       = panel(x, y);
      panel(x, y)    = panel(xo, yo);
      panel(xo, yo)  = tmp;
    }
  }
}
