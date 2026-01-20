#include <Arduino.h>
#include <FastLED.h>
#include "Joystick.h"
#include "SnakeGame.h"

// Hardware-Pins aus deinen funktionierenden Tests
#define PIN_UNTEN 25
#define PIN_OBEN 26
#define JOY_X 34
#define JOY_Y 35
#define JOY_SW 32

#define PANEL_BREITE 32
#define PANEL_HOEHE 8
#define SPIELFELD_BREITE 32
#define SPIELFELD_HOEHE 16
#define HELLIGKEIT 20

// Farben nach deinen Wünschen
#define FARBE_SNAKE_KOPF CRGB::Lime
#define FARBE_SNAKE_BODY CRGB::Green
#define FARBE_FOOD CRGB::Red

CRGB ledsUnten[PANEL_BREITE * PANEL_HOEHE];
CRGB ledsOben[PANEL_BREITE * PANEL_HOEHE];

SnakeGame game(SPIELFELD_BREITE, SPIELFELD_HOEHE);
Joystick joy(JOY_X, JOY_Y, JOY_SW);

// Deine bewährten Mapping-Funktionen
int berechneIndexUnten(int x, int y) {
    int spalteVonRechts = (PANEL_BREITE - 1) - x;
    int ledInSpalte = (spalteVonRechts % 2 == 0) ? (PANEL_HOEHE - 1) - y : y;
    return spalteVonRechts * PANEL_HOEHE + ledInSpalte;
}

int berechneIndexOben(int x, int y) {
    int ledInSpalte = (x % 2 == 0) ? y : (PANEL_HOEHE - 1) - y;
    return x * PANEL_HOEHE + ledInSpalte;
}

void setPixel(int x, int y, CRGB farbe) {
    if (x < 0 || x >= SPIELFELD_BREITE || y < 0 || y >= SPIELFELD_HOEHE) return;
    if (y < 8) ledsOben[berechneIndexOben(x, y)] = farbe;
    else ledsUnten[berechneIndexUnten(x, y - 8)] = farbe;
}

// Task 1: Joystick-Abfrage (hohe Priorität)
void taskJoystick(void *pvParameters) {
    while (1) {
        joy.aktualisiere();
        if (joy.istOben()) game.setDirection(DIR_UP);
        else if (joy.istUnten()) game.setDirection(DIR_DOWN);
        else if (joy.istLinks()) game.setDirection(DIR_LEFT);
        else if (joy.istRechts()) game.setDirection(DIR_RIGHT);
        vTaskDelay(pdMS_TO_TICKS(20)); //
    }
}

// Task 2: Game-Loop und Rendering
void taskSnake(void *pvParameters) {
    while (1) {
        if (!game.update()) {
            // Game Over: Alles rot blinken und neu starten
            fill_solid(ledsOben, 256, CRGB::Red);
            fill_solid(ledsUnten, 256, CRGB::Red);
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(1000));
            game.reset();
        }

        // Zeichnen
        FastLED.clear();
        // Futter
        setPixel(game.getFood().x, game.getFood().y, FARBE_FOOD);
        // Körper
        Point* body = game.getBody();
        for (int i = 1; i < game.getLength(); i++) {
            setPixel(body[i].x, body[i].y, FARBE_SNAKE_BODY);
        }
        // Kopf (andere Farbe laut Wunsch!)
        setPixel(body[0].x, body[0].y, FARBE_SNAKE_KOPF);
        
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(150)); // Geschwindigkeit der Schlange
    }
}

void setup() {
    Serial.begin(115200);
    FastLED.addLeds<WS2812, PIN_UNTEN, GRB>(ledsUnten, 256);
    FastLED.addLeds<WS2812, PIN_OBEN, GRB>(ledsOben, 256);
    FastLED.setBrightness(HELLIGKEIT);

    // Tasks erstellen
    xTaskCreatePinnedToCore(taskJoystick, "JoyTask", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskSnake, "SnakeTask", 4096, NULL, 1, NULL, 0);
}

void loop() {
    // Leer lassen bei FreeRTOS
}