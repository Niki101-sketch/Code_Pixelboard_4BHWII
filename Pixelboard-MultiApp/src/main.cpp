#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <time.h>
#include "Shared.h"
#include "Joystick.h"
#include "AppSnake.h"
#include "AppWetter.h"
#include "AppUhrzeit.h"

// ─── Hardware ─────────────────────────────────────────────────────────────────
#define PIN_TOP    25
#define PIN_BOTTOM 26
#define JOY_X      33
#define JOY_Y      32
#define JOY_SW     14

// ─── WiFi / NTP ───────────────────────────────────────────────────────────────
static const char* WIFI_SSID     = "iPhone von Paul";
static const char* WIFI_PASSWORD = "rootroot";
static const char* NTP_SERVER    = "pool.ntp.org";

// ─── Shared LED-Arrays ────────────────────────────────────────────────────────
CRGB ledsTop[256];    // Pin 25
CRGB ledsBottom[256]; // Pin 26

// ─── Joystick ─────────────────────────────────────────────────────────────────
Joystick joy(JOY_X, JOY_Y, JOY_SW);

// ─── App-Zustand ──────────────────────────────────────────────────────────────
volatile AppID currentApp            = APP_SNAKE;
volatile bool appWechselAngefordert  = false;
bool          wifiOK                 = false;

// ─── Task-Handles ─────────────────────────────────────────────────────────────
TaskHandle_t hSnakeLogic   = NULL;
TaskHandle_t hSnakeDisplay = NULL;
TaskHandle_t hWetter       = NULL;
TaskHandle_t hUhrzeit      = NULL;

// ─── LED-Mapping (32×16, identisch mit Snake-Projektion) ──────────────────────
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

// ─── App-Wechsel ──────────────────────────────────────────────────────────────
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
        default: break;
    }
}

// ─── taskManager: Wechsel-Animation + App umschalten ─────────────────────────
void taskManager(void *pvParameters) {
    while (1) {
        if (appWechselAngefordert) {
            appWechselAngefordert = false;

            suspendCurrentApp();

            // Wischeffekt: weißer Streifen von links nach rechts
            for (int x = 0; x < 32; x++) {
                FastLED.clear();
                for (int y = 0; y < 16; y++) setPixel(x, y, CRGB::White);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(18));
            }
            FastLED.clear();
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(80));

            currentApp = (AppID)((currentApp + 1) % APP_COUNT);
            resumeCurrentApp();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ─── taskInput: gemeinsamer Joystick-Handler (läuft immer) ───────────────────
void taskInput(void *pvParameters) {
    while (1) {
        joy.aktualisiere();
        JoystickRichtung dir     = joy.getRichtung();
        bool             pressed = joy.wurdeGedrueckt();

        if (currentApp == APP_SNAKE) {
            snakeHandleInput(dir, pressed);
        }
        // Wetter + Uhrzeit brauchen keinen Joystick-Input

        if (joy.wurdeLangeGedrueckt()) {
            appWechselAngefordert = true;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── WiFi + NTP ───────────────────────────────────────────────────────────────
static void startWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    // nicht blockierend — Verbindung läuft im Hintergrund weiter
}

static void finishWiFi() {
    // noch max 5s warten falls die Startup-Animation nicht gereicht hat
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        delay(500);
    }
    wifiOK = (WiFi.status() == WL_CONNECTED);
    if (!wifiOK) return;

    configTime(3600, 3600, NTP_SERVER);
    struct tm t;
    for (int i = 0; i < 10 && !getLocalTime(&t); i++) {
        delay(500);
    }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    FastLED.addLeds<WS2812, PIN_TOP,    GRB>(ledsTop,    256);
    FastLED.addLeds<WS2812, PIN_BOTTOM, GRB>(ledsBottom, 256);
    FastLED.setBrightness(25);
    FastLED.clear(true);

    startWiFi();        // startet async, verbindet im Hintergrund
    snakeInit();        // Highscore laden + ~1.5s Startup-Animation (WiFi verbindet währenddessen)
    finishWiFi();       // kurz nachwarten + NTP synchronisieren
    wetterInit();
    uhrzeitInit();

    // Tasks erstellen
    // Core 0: Display-Tasks
    xTaskCreatePinnedToCore(taskSnakeDisplay, "SnakeDsp", 4096, NULL, 1, &hSnakeDisplay, 0);
    xTaskCreatePinnedToCore(taskWetter,       "Wetter",   8192, NULL, 1, &hWetter,       0);
    xTaskCreatePinnedToCore(taskUhrzeit,      "Uhrzeit",  4096, NULL, 1, &hUhrzeit,      0);

    // Core 1: Logik-Tasks
    xTaskCreatePinnedToCore(taskSnakeLogic, "SnakeLog", 3072, NULL, 2, &hSnakeLogic, 1);
    xTaskCreatePinnedToCore(taskManager,    "Manager",  3072, NULL, 2, NULL,         1);
    xTaskCreatePinnedToCore(taskInput,      "Input",    2048, NULL, 3, NULL,         1);

    // Wetter + Uhrzeit starten sofort suspended (Snake ist aktive App)
    vTaskSuspend(hWetter);
    vTaskSuspend(hUhrzeit);
}

void loop() {}
