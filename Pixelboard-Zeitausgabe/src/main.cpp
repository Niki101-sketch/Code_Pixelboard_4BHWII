/**
 * @file Pixelboard_Uhrzeit_ESP32.cpp
 * @brief Uhrzeit Anzeige (HH:MM) auf 64x16 LED Pixelboard mit ESP32 und NTP
 */

#include <FastLED.h>
#include <LEDMatrix.h>
#include <LEDText.h>
#include <FontMatrise.h>
#include <WiFi.h>
#include <time.h>

// =============================================================================
// --- WLAN EINSTELLUNGEN (HIER DEINE DATEN EINTRAGEN) ------------------------
// =============================================================================

const char* ssid = "iPhone von Paul";          // Dein WLAN-Name
const char* password = "rootroot";  // Dein WLAN-Passwort

// NTP Server und Zeitzone
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;              // +1 Stunde für MEZ
const int daylightOffset_sec = 3600;          // +1 Stunde für Sommerzeit

// =============================================================================
// --- BENUTZER EINSTELLUNGEN --------------------------------------------------
// =============================================================================

CRGB uhrzeitFarbe = CRGB::White;  // Farbe der Uhrzeit
int updateInterval = 50;           // Update-Intervall in ms (für flüssige Anzeige)

// =============================================================================
// --- Hardware-Konfiguration --------------------------------------------------
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

cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelTop;
cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelBottom;

// --- Text Objekt -------------------------------------------------------------
cLEDText uhrzeitText;
static uint32_t lastUpdateMs = 0;
char uhrzeitString[6];  // "HH:MM" + null terminator

// --- Prototypen --------------------------------------------------------------
static void initWLAN();
static void initAnzeige();
static void updateUhrzeit();
static void scaleVertTo16();
static void verschiebeCanvas16EineZeileNachUnten();
static void blitPanelsFromCanvas16();
static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);
static void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);

static inline void clearCanvas8() {
  fill_solid(canvas8Leds, canvasWidth8 * canvasHeight8, CRGB::Black);
}
static inline void clearCanvas16() {
  fill_solid(canvas16Leds, canvasWidth16 * canvasHeight16, CRGB::Black);
}

// =============================================================================
// --- Setup -------------------------------------------------------------------
// =============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println(F("Pixelboard Uhr wird gestartet..."));

  // WLAN und Zeit initialisieren
  initWLAN();

  // LEDs und Anzeige initialisieren
  initAnzeige();

  Serial.println(F("Pixelboard Uhr bereit!"));
}

// =============================================================================
// --- Loop --------------------------------------------------------------------
// =============================================================================

void loop() {
  updateUhrzeit();
}

// =============================================================================
// --- WLAN Initialisierung ----------------------------------------------------
// =============================================================================

static void initWLAN() {
  Serial.println(F("\n========================================"));
  Serial.println(F("WLAN INITIALISIERUNG STARTET"));
  Serial.println(F("========================================"));
  
  Serial.print(F("SSID: "));
  Serial.println(ssid);
  Serial.print(F("Passwort Länge: "));
  Serial.println(strlen(password));

  WiFi.mode(WIFI_STA);
  delay(100);
  
  Serial.println(F("Starte WiFi.begin()..."));
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    if (attempts % 10 == 9) {
      Serial.print(F(" [Status: "));
      Serial.print(WiFi.status());
      Serial.println(F("]"));
    }
    attempts++;
  }

  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("========================================"));
    Serial.println(F("WLAN ERFOLGREICH VERBUNDEN!"));
    Serial.println(F("========================================"));
    Serial.print(F("IP Adresse: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("Gateway: "));
    Serial.println(WiFi.gatewayIP());
    Serial.print(F("DNS: "));
    Serial.println(WiFi.dnsIP());
    Serial.print(F("Signal Stärke (RSSI): "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));

    Serial.println(F("\n--- NTP ZEIT KONFIGURATION ---"));
    Serial.print(F("NTP Server: "));
    Serial.println(ntpServer);
    Serial.print(F("GMT Offset: "));
    Serial.print(gmtOffset_sec);
    Serial.println(F(" Sekunden"));
    Serial.print(F("Daylight Offset: "));
    Serial.print(daylightOffset_sec);
    Serial.println(F(" Sekunden"));
    
    // NTP Zeit konfigurieren
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    Serial.println(F("Warte auf NTP Zeitsynchronisation..."));
    
    struct tm timeinfo;
    int timeAttempts = 0;
    while (!getLocalTime(&timeinfo) && timeAttempts < 20) {
      delay(500);
      Serial.print(".");
      timeAttempts++;
    }
    
    Serial.println();
    
    if (timeAttempts < 20) {
      Serial.println(F("========================================"));
      Serial.println(F("ZEIT ERFOLGREICH SYNCHRONISIERT!"));
      Serial.println(F("========================================"));
      Serial.print(F("Datum: "));
      Serial.print(timeinfo.tm_mday);
      Serial.print(F("."));
      Serial.print(timeinfo.tm_mon + 1);
      Serial.print(F("."));
      Serial.println(timeinfo.tm_year + 1900);
      Serial.print(F("Uhrzeit: "));
      Serial.print(timeinfo.tm_hour);
      Serial.print(F(":"));
      Serial.print(timeinfo.tm_min);
      Serial.print(F(":"));
      Serial.println(timeinfo.tm_sec);
      Serial.println(&timeinfo, "Wochentag: %A");
    } else {
      Serial.println(F("========================================"));
      Serial.println(F("FEHLER: ZEITSYNCHRONISATION FEHLGESCHLAGEN!"));
      Serial.println(F("========================================"));
      Serial.println(F("Mögliche Ursachen:"));
      Serial.println(F("- NTP Server nicht erreichbar"));
      Serial.println(F("- Firewall blockiert NTP (Port 123)"));
      Serial.println(F("- DNS Problem"));
    }
  } else {
    Serial.println(F("========================================"));
    Serial.println(F("FEHLER: WLAN VERBINDUNG FEHLGESCHLAGEN!"));
    Serial.println(F("========================================"));
    Serial.print(F("WiFi Status Code: "));
    Serial.println(WiFi.status());
    Serial.println(F("\nStatus Codes:"));
    Serial.println(F("0 = WL_IDLE_STATUS"));
    Serial.println(F("1 = WL_NO_SSID_AVAIL (SSID nicht gefunden)"));
    Serial.println(F("3 = WL_CONNECTED"));
    Serial.println(F("4 = WL_CONNECT_FAILED (Verbindung fehlgeschlagen)"));
    Serial.println(F("6 = WL_DISCONNECTED"));
    Serial.println(F("\nÜberprüfe:"));
    Serial.println(F("- SSID korrekt?"));
    Serial.println(F("- Passwort korrekt?"));
    Serial.println(F("- Router eingeschaltet?"));
    Serial.println(F("- 2.4 GHz WLAN aktiv? (ESP32 unterstützt kein 5 GHz!)"));
  }
  
  Serial.println(F("========================================\n"));
}

// =============================================================================
// --- Anzeige Initialisierung -------------------------------------------------
// =============================================================================

static void initAnzeige() {
  Serial.println(F("\n========================================"));
  Serial.println(F("LED INITIALISIERUNG STARTET"));
  Serial.println(F("========================================"));
  
  // FastLED Setup
  Serial.println(F("FastLED.addLeds() für Top Panel (Pin 25)..."));
  FastLED.addLeds<chipset, pinTop, colorOrder>(ledsTop, ledsPerPanel);
  
  Serial.println(F("FastLED.addLeds() für Bottom Panel (Pin 26)..."));
  FastLED.addLeds<chipset, pinBottom, colorOrder>(ledsBottom, ledsPerPanel);
  
  Serial.print(F("Setze Helligkeit auf: "));
  Serial.println(brightness);
  FastLED.setBrightness(brightness);
  
  Serial.println(F("Lösche alle LEDs..."));
  FastLED.clear(true);

  // Mapping Panel Objekte auf LED Arrays
  Serial.println(F("Mappe Panel Objekte auf LED Arrays..."));
  panelTop.SetLEDArray(ledsBottom);
  panelBottom.SetLEDArray(ledsTop);

  // Mapping Canvas
  Serial.println(F("Initialisiere Canvas8 und Canvas16..."));
  canvas8.SetLEDArray(canvas8Leds);
  canvas16.SetLEDArray(canvas16Leds);

  // Text Initialisierung
  Serial.println(F("Initialisiere Text Objekt..."));
  uhrzeitText.SetFont(MatriseFontData);
  
  Serial.print(F("Init Text mit Canvas8 ("));
  Serial.print(canvas8.Width());
  Serial.print(F("x"));
  Serial.print(canvas8.Height());
  Serial.println(F(")..."));
  uhrzeitText.Init(&canvas8, canvas8.Width(), canvas8.Height(), 0, 0);
  
  Serial.print(F("Setze Text Farbe: R="));
  Serial.print(uhrzeitFarbe.r);
  Serial.print(F(", G="));
  Serial.print(uhrzeitFarbe.g);
  Serial.print(F(", B="));
  Serial.println(uhrzeitFarbe.b);
  uhrzeitText.SetTextColrOptions(COLR_RGB | COLR_SINGLE, 
                                  uhrzeitFarbe.r, 
                                  uhrzeitFarbe.g, 
                                  uhrzeitFarbe.b);
  
  Serial.println(F("LED Initialisierung abgeschlossen!"));
  Serial.println(F("========================================\n"));
}

// =============================================================================
// --- Uhrzeit Update ----------------------------------------------------------
// =============================================================================

static void updateUhrzeit() {
  const uint32_t now = millis();

  if (now - lastUpdateMs < (uint32_t)updateInterval) return;
  lastUpdateMs = now;

  Serial.println(F("=== UPDATE UHRZEIT START ==="));
  Serial.print(F("Millis: "));
  Serial.println(now);

  // WiFi Status prüfen
  Serial.print(F("WiFi Status: "));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("Verbunden"));
  } else {
    Serial.print(F("NICHT verbunden! Status Code: "));
    Serial.println(WiFi.status());
  }

  // Aktuelle Zeit holen
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println(F("FEHLER: getLocalTime() fehlgeschlagen!"));
    strcpy(uhrzeitString, "--:--");
  } else {
    // Zeit formatieren als HH:MM
    sprintf(uhrzeitString, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    
    // Detaillierte Zeit Ausgabe
    Serial.println(F("Zeit erfolgreich abgerufen:"));
    Serial.print(F("  Datum: "));
    Serial.print(timeinfo.tm_mday);
    Serial.print(F("."));
    Serial.print(timeinfo.tm_mon + 1);
    Serial.print(F("."));
    Serial.println(timeinfo.tm_year + 1900);
    Serial.print(F("  Uhrzeit: "));
    Serial.print(uhrzeitString);
    Serial.print(F(" ("));
    Serial.print(timeinfo.tm_hour);
    Serial.print(F(":"));
    Serial.print(timeinfo.tm_min);
    Serial.print(F(":"));
    Serial.print(timeinfo.tm_sec);
    Serial.println(F(")"));
  }

  Serial.print(F("Anzuzeigender String: '"));
  Serial.print(uhrzeitString);
  Serial.print(F("' (Länge: "));
  Serial.print(strlen(uhrzeitString));
  Serial.println(F(")"));

  // Canvas löschen
  Serial.println(F("Lösche Canvas8..."));
  clearCanvas8();

  // Text setzen und zentrieren
  Serial.println(F("Setze Text..."));
  uhrzeitText.SetText((unsigned char*)uhrzeitString, strlen(uhrzeitString));
  
  // Text zeichnen (statisch, mittig)
  int textWidth = strlen(uhrzeitString) * 6;
  int startX = (canvasWidth8 - textWidth) / 2;
  
  Serial.print(F("Text Breite (geschätzt): "));
  Serial.print(textWidth);
  Serial.print(F(" Pixel, Start X: "));
  Serial.println(startX);
  
  // Position setzen und Text rendern
  Serial.println(F("Rendere Text mit UpdateText()..."));
  int renderResult = uhrzeitText.UpdateText();
  Serial.print(F("UpdateText() Rückgabe: "));
  Serial.println(renderResult);

  // --- Skalieren auf 16px Höhe ---
  Serial.println(F("Skaliere auf 16px..."));
  scaleVertTo16();

  // --- Verschieben (Hardware-Korrektur) ---
  Serial.println(F("Verschiebe Canvas..."));
  verschiebeCanvas16EineZeileNachUnten();

  // --- Auf Panels mappen & Hardware-Fixes ---
  Serial.println(F("Mappe auf Panels..."));
  blitPanelsFromCanvas16();

  // Anzeigen
  Serial.println(F("FastLED.show()..."));
  FastLED.show();
  
  Serial.println(F("=== UPDATE UHRZEIT ENDE ===\n"));
}

// =============================================================================
// --- Hardware Helper Funktionen ----------------------------------------------
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
  // Oberes physisches Panel (Pin 26) bekommt logisches unten
  for (uint8_t y = 0; y < panelHeight; y++) {
    const uint8_t ySrc = y + panelHeight;
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelTop(x, y) = canvas16(x, ySrc);
    }
  }

  // Unteres physisches Panel (Pin 25) bekommt logisches oben
  for (uint8_t y = 0; y < panelHeight; y++) {
    const uint8_t ySrc = y;
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelBottom(x, y) = canvas16(x, ySrc);
    }
  }

  // Spiegelung an der y-Achse für beide Panels
  mirrorPanelHorizontal(panelTop);
  mirrorPanelHorizontal(panelBottom);

  // Kopfüber montiertes Panel drehen
  rotatePanel180(panelTop);
}

static void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      const uint8_t xo = panelWidth - 1 - x;
      CRGB tmp = panel(x, y);
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
      CRGB tmp = panel(x, y);
      panel(x, y) = panel(xo, yo);
      panel(xo, yo) = tmp;
    }
  }
}