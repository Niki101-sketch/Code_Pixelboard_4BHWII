#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include "Shared.h"
#include "Joystick.h"
#include "PixelFont.h"
#include "AppSnake.h"
#include "AppWetter.h"
#include "AppUhrzeit.h"
#include "AppDHT22.h"
#include "AppPacman.h"

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

// ─── Joysticks ────────────────────────────────────────────────────────────────
Joystick joy (JOY_X,  JOY_Y,  JOY_SW);
Joystick joy2(JOY2_X, JOY2_Y, JOY2_SW);

// ─── App-Zustand ──────────────────────────────────────────────────────────────
volatile AppID currentApp           = APP_SNAKE;
volatile bool  appWechselAngefordert = false;
bool           wifiOK                = false;

// ─── App-Auswahl-Menü ─────────────────────────────────────────────────────────
volatile bool  appMenuOpen      = false;
volatile AppID menuSelectedApp  = APP_SNAKE;
volatile bool  menuNavLeft      = false;
volatile bool  menuNavRight     = false;
volatile bool  menuConfirm      = false;

// ─── Task-Handles ─────────────────────────────────────────────────────────────
TaskHandle_t hSnakeLogic   = NULL;
TaskHandle_t hSnakeDisplay = NULL;
TaskHandle_t hWetter       = NULL;
TaskHandle_t hUhrzeit      = NULL;
TaskHandle_t hDHT22        = NULL;
TaskHandle_t hPacman       = NULL;

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
        default: break;
    }
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

static const uint8_t* APP_ICONS[APP_COUNT] = {
    ICON_SNAKE, ICON_WETTER, ICON_UHR, ICON_DHT22, ICON_PACMAN
};
static const CRGB APP_COLORS[APP_COUNT] = {
    CRGB::Green, CRGB::Yellow, CRGB::White, CRGB::Cyan, CHSV(43,255,255)
};
// Icon-X-Positionen (je 5px breit, 1px Abstand, zentriert)
static const int ICON_X[5] = {1, 7, 13, 19, 25};
static const int ICON_Y     = 4;

static void drawAppMenu() {
    FastLED.clear();
    for (int i = 0; i < APP_COUNT; i++) {
        bool sel = (i == (int)menuSelectedApp);
        CRGB c   = sel ? APP_COLORS[i] : CRGB(APP_COLORS[i].r/4, APP_COLORS[i].g/4, APP_COLORS[i].b/4);
        for (int row = 0; row < 5; row++) {
            uint8_t bits = APP_ICONS[i][row];
            for (int col = 0; col < 5; col++) {
                if (bits & (1 << (4 - col))) setPixel(ICON_X[i] + col, ICON_Y + row, c);
            }
        }
        // Indikator-Punkt unter ausgewähltem Icon
        if (sel) setPixel(ICON_X[i] + 2, ICON_Y + 6, APP_COLORS[i]);
    }
}

// ─── taskManager ─────────────────────────────────────────────────────────────
void taskManager(void *pvParameters) {
    while (1) {
        // Menü öffnen
        if (appWechselAngefordert && !appMenuOpen) {
            appWechselAngefordert = false;
            suspendCurrentApp();
            menuSelectedApp = currentApp;
            appMenuOpen     = true;
        }

        // Menü bedienen
        if (appMenuOpen) {
            if (menuNavLeft) {
                menuNavLeft = false;
                menuSelectedApp = (AppID)(((int)menuSelectedApp - 1 + APP_COUNT) % APP_COUNT);
            }
            if (menuNavRight) {
                menuNavRight = false;
                menuSelectedApp = (AppID)(((int)menuSelectedApp + 1) % APP_COUNT);
            }
            drawAppMenu();
            FastLED.show();

            if (menuConfirm) {
                menuConfirm = false;
                appMenuOpen = false;
                currentApp  = menuSelectedApp;
                // kurze Wisch-Animation
                for (int x = 0; x < 32; x++) {
                    FastLED.clear();
                    for (int y = 0; y < 16; y++) setPixel(x, y, APP_COLORS[currentApp]);
                    FastLED.show();
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                FastLED.clear();
                FastLED.show();
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
                lastMenuDir = dir;
            }
            if (dir == NEUTRAL) lastMenuDir = NEUTRAL;
            if (pressed)        menuConfirm  = true;
        } else {
            // Normaler App-Input
            if (currentApp == APP_SNAKE)  snakeHandleInput(dir, pressed);
            if (currentApp == APP_PACMAN) pacmanHandleInput(dir, pressed);
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

static void finishWiFi() {
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) delay(500);
    wifiOK = (WiFi.status() == WL_CONNECTED);
    if (!wifiOK) return;
    configTime(3600, 3600, NTP_SERVER);
    struct tm t;
    for (int i = 0; i < 10 && !getLocalTime(&t); i++) delay(500);
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    FastLED.addLeds<WS2812, PIN_TOP,    GRB>(ledsTop,    256);
    FastLED.addLeds<WS2812, PIN_BOTTOM, GRB>(ledsBottom, 256);
    FastLED.setBrightness(25);
    FastLED.clear(true);

    startWiFi();
    snakeInit();
    finishWiFi();
    wetterInit();
    uhrzeitInit();
    dht22Init();
    pacmanInit();

    // Core 0: Display-Tasks
    xTaskCreatePinnedToCore(taskSnakeDisplay, "SnakeDsp", 4096, NULL, 1, &hSnakeDisplay, 0);
    xTaskCreatePinnedToCore(taskWetter,       "Wetter",   8192, NULL, 1, &hWetter,       0);
    xTaskCreatePinnedToCore(taskUhrzeit,      "Uhrzeit",  4096, NULL, 1, &hUhrzeit,      0);
    xTaskCreatePinnedToCore(taskDHT22,        "DHT22",    4096, NULL, 1, &hDHT22,        0);
    xTaskCreatePinnedToCore(taskPacmanDisplay,"PacDsp",   5120, NULL, 1, &hPacman,       0);

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
}

void loop() {}
