#pragma once
#include <Arduino.h>
#include <FastLED.h>

// ─── App-IDs ──────────────────────────────────────────────────────────────────
enum AppID { APP_SNAKE = 0, APP_WETTER = 1, APP_UHRZEIT = 2, APP_DHT22 = 3, APP_COUNT = 4 };

// ─── Shared LED-Arrays (definiert in main.cpp) ────────────────────────────────
extern CRGB ledsTop[256];    // Pin 25
extern CRGB ledsBottom[256]; // Pin 26

// ─── App-Zustand ──────────────────────────────────────────────────────────────
extern volatile AppID currentApp;
extern volatile bool  appWechselAngefordert;
extern bool           wifiOK;

// ─── Task-Handles (definiert in main.cpp) ─────────────────────────────────────
extern TaskHandle_t hSnakeLogic;
extern TaskHandle_t hSnakeDisplay;
extern TaskHandle_t hWetter;
extern TaskHandle_t hUhrzeit;
extern TaskHandle_t hDHT22;

// ─── Pixel-Helfer (Snake-Mapping, deckt volles 32×16 Raster ab) ───────────────
void setPixel(int x, int y, CRGB f);
