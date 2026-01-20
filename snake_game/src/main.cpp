#include <Arduino.h>
#include <FastLED.h>
#include "Joystick.h"
#include "SnakeGame.h"

// Hardware
#define PIN_UNTEN 25
#define PIN_OBEN 26
#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 33  // Pin 33 für den Button

#define SPIELFELD_BREITE 32
#define SPIELFELD_HOEHE 16

CRGB ledsUnten[256], ledsOben[256];
SnakeGame game(SPIELFELD_BREITE, SPIELFELD_HOEHE);
Joystick joy(JOY_X, JOY_Y, JOY_SW);

enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAMEOVER };
volatile GameState currentState = STATE_MENU;

// Einstellungen
int menuSelection = 0; 
int selectedSpeedLevel = 2;
int selectedFoodAmount = 1;

// Puffer für die nächste Richtung, damit der Input-Task den Game-Task füttert
volatile Direction nextDirection = DIR_RIGHT; 
JoystickRichtung letzteMenueRichtung = NEUTRAL;

// --- Mapping Funktionen (unverändert) ---
int berechneIndexUnten(int x, int y) {
    int spVonR = 31 - x;
    return spVonR * 8 + ((spVonR % 2 == 0) ? (7 - y) : y);
}
int berechneIndexOben(int x, int y) {
    return x * 8 + ((x % 2 == 0) ? y : (7 - y));
}
void setPixel(int x, int y, CRGB f) {
    if (x < 0 || x >= 32 || y < 0 || y >= 16) return;
    if (y < 8) ledsOben[berechneIndexOben(x, y)] = f;
    else ledsUnten[berechneIndexUnten(x, y - 8)] = f;
}

// --- Task 1: Input (Sehr schnell!) ---
void taskInput(void *pvParameters) {
    while (1) {
        joy.aktualisiere();
        JoystickRichtung dir = joy.getRichtung();

        if (currentState == STATE_MENU) {
            if (dir != letzteMenueRichtung) {
                if (dir == OBEN) menuSelection = 0;
                if (dir == UNTEN) menuSelection = 1;
                if (menuSelection == 0) {
                    if (dir == RECHTS && selectedSpeedLevel < 5) selectedSpeedLevel++;
                    if (dir == LINKS && selectedSpeedLevel > 1) selectedSpeedLevel--;
                } else {
                    if (dir == RECHTS && selectedFoodAmount < 5) selectedFoodAmount++;
                    if (dir == LINKS && selectedFoodAmount > 1) selectedFoodAmount--;
                }
                letzteMenueRichtung = dir;
            }
            if (joy.wurdeGedrueckt()) {
                game.reset(selectedFoodAmount);
                currentState = STATE_PLAYING;
            }
        } 
        else if (currentState == STATE_PLAYING) {
            if (dir == OBEN) nextDirection = DIR_UP;
            else if (dir == UNTEN) nextDirection = DIR_DOWN;
            else if (dir == LINKS) nextDirection = DIR_LEFT;
            else if (dir == RECHTS) nextDirection = DIR_RIGHT;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // Check alle 20ms
    }
}

// --- Task 2: Spiellogik (Variable Geschwindigkeit) ---
void taskGameLogic(void *pvParameters) {
    while (1) {
        if (currentState == STATE_PLAYING) {
            game.setDirection(nextDirection);
            if (!game.update()) currentState = STATE_GAMEOVER;
        }
        
        int speedDelay = 400 - (selectedSpeedLevel * 60);
        vTaskDelay(pdMS_TO_TICKS(currentState == STATE_PLAYING ? speedDelay : 100));
    }
}

// --- Task 3: Display (Konstante 30 FPS) ---
void taskDisplay(void *pvParameters) {
    while (1) {
        FastLED.clear();
        if (currentState == STATE_MENU) {
            // Rahmen im Menü
            for(int x=0; x<32; x++) { setPixel(x,0,CRGB::White); setPixel(x,15,CRGB::White); }
            // Speed Balken (Blau)
            for(int i=0; i<selectedSpeedLevel; i++) setPixel(8 + i*2, 5, CRGB::Blue);
            if(menuSelection == 0) setPixel(5, 5, CRGB::White);
            // Food Balken (Rot)
            for(int i=0; i<selectedFoodAmount; i++) setPixel(8 + i*2, 10, CRGB::Red);
            if(menuSelection == 1) setPixel(5, 10, CRGB::White);
        } 
        else if (currentState == STATE_PLAYING) {
            // Weißer Rand
            for(int x=0; x<32; x++) { setPixel(x,0,CRGB::White); setPixel(x,15,CRGB::White); }
            for(int y=0; y<16; y++) { setPixel(0,y,CRGB::White); setPixel(31,y,CRGB::White); }
            // Food & Snake
            for(int i=0; i<selectedFoodAmount; i++) setPixel(game.getFoodArray()[i].x, game.getFoodArray()[i].y, CRGB::Red);
            Point* b = game.getBody();
            for(int i=1; i<game.getLength(); i++) setPixel(b[i].x, b[i].y, CRGB::Green);
            setPixel(b[0].x, b[0].y, CRGB::Lime);
        }
        else if (currentState == STATE_GAMEOVER) {
            fill_solid(ledsOben, 256, CRGB::Red); fill_solid(ledsUnten, 256, CRGB::Red);
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(1200));
            currentState = STATE_MENU;
        }
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
    }
}

void setup() {
    FastLED.addLeds<WS2812, PIN_UNTEN, GRB>(ledsUnten, 256);
    FastLED.addLeds<WS2812, PIN_OBEN, GRB>(ledsOben, 256);
    FastLED.setBrightness(20);

    // Tasks auf die Kerne verteilen
    xTaskCreatePinnedToCore(taskInput, "Input", 2048, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(taskGameLogic, "Game", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskDisplay, "Display", 4096, NULL, 1, NULL, 0);
}

void loop() {}