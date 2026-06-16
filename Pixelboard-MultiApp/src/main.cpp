#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include "soc/soc.h"            // Brownout-Detektor (Standalone-Betrieb am Netzteil)
#include "soc/rtc_cntl_reg.h"
#include "Shared.h"
#include "Joystick.h"
#include "PixelFont.h"
#include "AppSnake.h"
#include "AppWetter.h"
#include "AppUhrzeit.h"
#include "AppDHT22.h"
#include "AppPacman.h"
#include "AppPong.h"

// ─── Hardware ─────────────────────────────────────────────────────────────────
#define PIN_TOP    25
#define PIN_BOTTOM 26
#define JOY_X      33
#define JOY_Y      32
#define JOY_SW     14
#define JOY2_X     35
#define JOY2_Y     34
#define JOY2_SW    27

// ─── WiFi / NTP ───────────────────────────────────────────────────────────────
static const char* WIFI_SSID     = "iPhone von Paul";
static const char* WIFI_PASSWORD = "rootroot";
static const char* NTP_SERVER    = "pool.ntp.org";

// ─── Shared LED-Arrays ────────────────────────────────────────────────────────
CRGB ledsTop[256];
CRGB ledsBottom[256];

// ─── LED-Ausgabe-Mutex (siehe Shared.h / showLeds()) ──────────────────────────
SemaphoreHandle_t ledMutex = NULL;

// ─── Joysticks ────────────────────────────────────────────────────────────────
Joystick joy (JOY_X,  JOY_Y,  JOY_SW);
// joy2 ist gespiegelt verdrahtet -> beide Achsen invertieren
Joystick joy2(JOY2_X, JOY2_Y, JOY2_SW, 1000, 2048, true, true);

// ─── App-Zustand ──────────────────────────────────────────────────────────────
volatile AppID currentApp           = APP_SNAKE;
volatile bool  appWechselAngefordert = false;
bool           wifiOK                = false;

// ─── App-Auswahl-Menü ─────────────────────────────────────────────────────────
volatile bool  appMenuOpen      = false;
volatile AppID menuSelectedApp  = APP_SNAKE;
volatile bool  menuNavLeft      = false;
volatile bool  menuNavRight     = false;
volatile bool  menuNavUp        = false;
volatile bool  menuNavDown      = false;
volatile bool  menuConfirm      = false;

// ─── Task-Handles ─────────────────────────────────────────────────────────────
TaskHandle_t hSnakeLogic   = NULL;
TaskHandle_t hSnakeDisplay = NULL;
TaskHandle_t hWetter       = NULL;
TaskHandle_t hUhrzeit      = NULL;
TaskHandle_t hDHT22        = NULL;
TaskHandle_t hPacman       = NULL;
TaskHandle_t hPong         = NULL;

// ─── LED-Mapping (32×16) ──────────────────────────────────────────────────────
static int indexTop(int x, int y) {
    return x * 8 + ((x % 2 == 0) ? y : (7 - y));
}
static int indexBottom(int x, int y) {
    int s = 31 - x;
    return s * 8 + ((s % 2 == 0) ? (7 - y) : y);
}
void setPixel(int x, int y, CRGB f) {
    if (x < 0 || x >= 32 || y < 0 || y >= 16) return;
    if (y < 8) ledsTop[indexTop(x, y)]           = f;
    else       ledsBottom[indexBottom(x, y - 8)] = f;
}

// ─── App suspend/resume ───────────────────────────────────────────────────────
static void suspendCurrentApp() {
    // Mutex zuerst holen: so kann der Display-Task niemals mitten in
    // FastLED.show() suspendiert werden (das würde die RMT-Ausgabe blockieren).
    if (ledMutex) xSemaphoreTake(ledMutex, portMAX_DELAY);
    switch (currentApp) {
        case APP_SNAKE:
            if (hSnakeLogic)   vTaskSuspend(hSnakeLogic);
            if (hSnakeDisplay) vTaskSuspend(hSnakeDisplay);
            break;
        case APP_WETTER:
            if (hWetter) vTaskSuspend(hWetter);
            break;
        case APP_UHRZEIT:
            if (hUhrzeit) vTaskSuspend(hUhrzeit);
            break;
        case APP_DHT22:
            if (hDHT22) vTaskSuspend(hDHT22);
            break;
        case APP_PACMAN:
            if (hPacman) vTaskSuspend(hPacman);
            break;
        case APP_PONG:
            if (hPong) vTaskSuspend(hPong);
            break;
        default: break;
    }
    if (ledMutex) xSemaphoreGive(ledMutex);
}

static void resumeCurrentApp() {
    switch (currentApp) {
        case APP_SNAKE:
            snakeResetToMenu();
            if (hSnakeLogic)   vTaskResume(hSnakeLogic);
            if (hSnakeDisplay) vTaskResume(hSnakeDisplay);
            break;
        case APP_WETTER:
            if (hWetter) vTaskResume(hWetter);
            break;
        case APP_UHRZEIT:
            if (hUhrzeit) vTaskResume(hUhrzeit);
            break;
        case APP_DHT22:
            if (hDHT22) vTaskResume(hDHT22);
            break;
        case APP_PACMAN:
            pacmanResetToMenu();
            if (hPacman) vTaskResume(hPacman);
            break;
        case APP_PONG:
            pongResetToMenu();
            if (hPong) vTaskResume(hPong);
            break;
        default: break;
    }
}

// ─── App-Auswahl-Menü zeichnen ────────────────────────────────────────────────
// App-Icons (5×5, 5 Bytes, jeweils Bits 4–0 = Spalten 0–4)
static const uint8_t ICON_SNAKE[5]  = {0x07,0x1F,0x1C,0x1F,0x07}; // Schlangenkopf
static const uint8_t ICON_WETTER[5] = {0x04,0x0E,0x1F,0x0E,0x04}; // Sonne
static const uint8_t ICON_UHR[5]    = {0x0E,0x15,0x17,0x11,0x0E}; // Uhr
static const uint8_t ICON_DHT22[5]  = {0x06,0x09,0x09,0x0F,0x0F}; // Thermometer
static const uint8_t ICON_PACMAN[5] = {0x0E,0x1C,0x18,0x1C,0x0E}; // Pacman-Mund
static const uint8_t ICON_PONG[5]   = {0x11,0x11,0x15,0x11,0x11}; // Zwei Paddles + Ball

static const uint8_t* APP_ICONS[APP_COUNT] = {
    ICON_SNAKE, ICON_WETTER, ICON_UHR, ICON_DHT22, ICON_PACMAN, ICON_PONG
};
static const CRGB APP_COLORS[APP_COUNT] = {
    CRGB::Green, CRGB::Yellow, CRGB::White, CRGB::Cyan, CHSV(43,255,255), CRGB::Magenta
};

// 2×3 Grid: Zeile 0 = Info-Apps, Zeile 1 = Spiele
static const AppID MENU_GRID[2][3] = {
    { APP_WETTER, APP_UHRZEIT, APP_DHT22 },
    { APP_SNAKE,  APP_PONG,    APP_PACMAN }
};
static const int ICON_COL_X[3] = { 3, 13, 23 };  // x-Start je Spalte
static const int ICON_ROW_Y[2] = { 1,  9 };       // y-Start je Zeile
static const int DOT_ROW_Y[2]  = { 7, 15 };       // Indikatorpunkt je Zeile

static void drawAppMenu() {
    FastLED.clear();
    // Trennlinie zwischen Info- und Spiele-Zeile
    for (int x = 0; x < 32; x++) setPixel(x, 8, CRGB(15, 15, 15));

    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 3; col++) {
            AppID app = MENU_GRID[row][col];
            bool  sel = (app == menuSelectedApp);
            CRGB  c   = sel ? APP_COLORS[app]
                            : CRGB(APP_COLORS[app].r/4, APP_COLORS[app].g/4, APP_COLORS[app].b/4);
            int ix = ICON_COL_X[col];
            int iy = ICON_ROW_Y[row];
            for (int r = 0; r < 5; r++) {
                uint8_t bits = APP_ICONS[app][r];
                for (int c2 = 0; c2 < 5; c2++) {
                    if (bits & (1 << (4 - c2))) setPixel(ix + c2, iy + r, c);
                }
            }
            if (sel) setPixel(ix + 2, DOT_ROW_Y[row], APP_COLORS[app]);
        }
    }
}

// ─── taskManager ─────────────────────────────────────────────────────────────
void taskManager(void *pvParameters) {
    static int menuRow = 0;
    static int menuCol = 0;

    while (1) {
        // Menü öffnen
        if (appWechselAngefordert && !appMenuOpen) {
            appWechselAngefordert = false;
            suspendCurrentApp();
            // Cursor auf aktuelle App setzen
            menuSelectedApp = currentApp;
            for (int r = 0; r < 2; r++)
                for (int c = 0; c < 3; c++)
                    if (MENU_GRID[r][c] == currentApp) { menuRow = r; menuCol = c; }
            appMenuOpen = true;
        }

        // Menü bedienen
        if (appMenuOpen) {
            if (menuNavLeft)  { menuNavLeft  = false; menuCol = (menuCol + 2) % 3; menuSelectedApp = MENU_GRID[menuRow][menuCol]; }
            if (menuNavRight) { menuNavRight = false; menuCol = (menuCol + 1) % 3; menuSelectedApp = MENU_GRID[menuRow][menuCol]; }
            if (menuNavUp)    { menuNavUp    = false; menuRow = 0;                  menuSelectedApp = MENU_GRID[menuRow][menuCol]; }
            if (menuNavDown)  { menuNavDown  = false; menuRow = 1;                  menuSelectedApp = MENU_GRID[menuRow][menuCol]; }
            drawAppMenu();
            showLeds();

            if (menuConfirm) {
                menuConfirm = false;
                appMenuOpen = false;
                currentApp  = menuSelectedApp;
                // kurze Wisch-Animation
                for (int x = 0; x < 32; x++) {
                    FastLED.clear();
                    for (int y = 0; y < 16; y++) setPixel(x, y, APP_COLORS[currentApp]);
                    showLeds();
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                FastLED.clear();
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(60));
                resumeCurrentApp();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

// ─── taskInput ────────────────────────────────────────────────────────────────
void taskInput(void *pvParameters) {
    JoystickRichtung lastMenuDir = NEUTRAL;

    while (1) {
        joy.aktualisiere();
        joy2.aktualisiere();

        JoystickRichtung dir = joy.getRichtung();
        if (dir == NEUTRAL) dir = joy2.getRichtung();

        bool pressed1 = joy.wurdeGedrueckt();
        bool pressed2 = joy2.wurdeGedrueckt();
        bool pressed  = pressed1 || pressed2;

        bool lang1 = joy.wurdeLangeGedrueckt();
        bool lang2 = joy2.wurdeLangeGedrueckt();

        if (appMenuOpen) {
            // Joystick-Navigation im Menü
            if (dir != lastMenuDir) {
                if (dir == LINKS)  menuNavLeft  = true;
                if (dir == RECHTS) menuNavRight = true;
                if (dir == OBEN)   menuNavUp    = true;
                if (dir == UNTEN)  menuNavDown  = true;
                lastMenuDir = dir;
            }
            if (dir == NEUTRAL) lastMenuDir = NEUTRAL;
            if (pressed)        menuConfirm  = true;
        } else {
            // Normaler App-Input
            if (currentApp == APP_SNAKE)  snakeHandleInput(dir, pressed);
            if (currentApp == APP_PACMAN) pacmanHandleInput(dir, pressed);
            if (currentApp == APP_PONG)   pongHandleInput(joy.getRichtung(), joy2.getRichtung(), pressed);
            // Wetter, Uhrzeit, DHT22 brauchen keinen Input

            if (lang1 || lang2) appWechselAngefordert = true;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── WiFi + NTP ───────────────────────────────────────────────────────────────
static void startWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// Wartet im Hintergrund auf die Verbindung und synchronisiert dann die Zeit.
// So blockiert der Boot NICHT auf das WLAN – ohne Netz (z.B. nur Netzteil) laufen
// Snake/Pacman/DHT22 sofort; Uhrzeit/Wetter aktualisieren sich, sobald verbunden.
static void taskWiFi(void *pvParameters) {
    while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));
    wifiOK = true;                          // verbunden -> Wetter darf abrufen
    configTime(3600, 3600, NTP_SERVER);     // Zeit synchronisieren (für Uhrzeit)
    vTaskDelete(NULL);
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    // Brownout-Detektor abschalten: verhindert Reset-Schleifen durch kurze
    // Spannungseinbrüche (WiFi-Funkpeak + LED-Strom) an schwächeren Netzteilen.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // Mutex VOR allen Ausgaben/Tasks erzeugen (snakeInit() zeigt schon Frames).
    ledMutex = xSemaphoreCreateMutex();

    FastLED.addLeds<WS2812, PIN_TOP,    GRB>(ledsTop,    256);
    FastLED.addLeds<WS2812, PIN_BOTTOM, GRB>(ledsBottom, 256);
    FastLED.setBrightness(25);
    FastLED.clear(true);

    startWiFi();
    snakeInit();
    wetterInit();
    uhrzeitInit();
    dht22Init();
    pacmanInit();
    pongInit();

    // WLAN/NTP im Hintergrund verbinden (blockiert den Start nicht)
    xTaskCreatePinnedToCore(taskWiFi, "WiFi", 4096, NULL, 1, NULL, 0);

    // Core 0: Display-Tasks
    xTaskCreatePinnedToCore(taskSnakeDisplay, "SnakeDsp", 4096, NULL, 1, &hSnakeDisplay, 0);
    xTaskCreatePinnedToCore(taskWetter,       "Wetter",   8192, NULL, 1, &hWetter,       0);
    xTaskCreatePinnedToCore(taskUhrzeit,      "Uhrzeit",  4096, NULL, 1, &hUhrzeit,      0);
    xTaskCreatePinnedToCore(taskDHT22,        "DHT22",    4096, NULL, 1, &hDHT22,        0);
    xTaskCreatePinnedToCore(taskPacmanDisplay,"PacDsp",   5120, NULL, 1, &hPacman,       0);
    xTaskCreatePinnedToCore(taskPongDisplay,  "PongDsp",  4096, NULL, 1, &hPong,         0);

    // Core 1: Logik-Tasks
    xTaskCreatePinnedToCore(taskSnakeLogic,  "SnakeLog", 3072, NULL, 2, &hSnakeLogic, 1);
    xTaskCreatePinnedToCore(taskPacmanLogic, "PacLog",   3072, NULL, 2, NULL,         1);
    xTaskCreatePinnedToCore(taskManager,     "Manager",  3072, NULL, 2, NULL,         1);
    xTaskCreatePinnedToCore(taskInput,       "Input",    2048, NULL, 3, NULL,         1);

    // Alles außer Snake suspended (Snake ist aktive Start-App)
    vTaskSuspend(hWetter);
    vTaskSuspend(hUhrzeit);
    vTaskSuspend(hDHT22);
    vTaskSuspend(hPacman);
    vTaskSuspend(hPong);
}

void loop() {}
