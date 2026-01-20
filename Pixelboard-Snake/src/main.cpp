/**
 * @file Snake_Game_Simple.cpp
 * @brief EINFACHES Snake-Spiel für 64x16 LED Matrix
 */

#include <Arduino.h>
#include <FastLED.h>
#include "Joystick.h"

// =============================================================================
// --- HARDWARE KONFIGURATION --------------------------------------------------
// =============================================================================

#define PIN_TOP         25
#define PIN_BOTTOM      26

#define JOY_PIN_X       34
#define JOY_PIN_Y       35
#define JOY_PIN_SW      32

#define MATRIX_WIDTH    64
#define MATRIX_HEIGHT   16
#define NUM_LEDS        (MATRIX_WIDTH * MATRIX_HEIGHT)

#define BRIGHTNESS      40

CRGB leds[NUM_LEDS];
Joystick joystick(JOY_PIN_X, JOY_PIN_Y, JOY_PIN_SW);

// =============================================================================
// --- SPIELZUSTÄNDE -----------------------------------------------------------
// =============================================================================

enum GameState {
    MENU,
    PLAYING,
    GAME_OVER
};

GameState currentState = MENU;

enum Difficulty {
    EASY = 0,
    MEDIUM = 1,
    HARD = 2
};

Difficulty currentLevel = MEDIUM;

const int SPEED[] = {400, 250, 150};

// =============================================================================
// --- SNAKE -------------------------------------------------------------------
// =============================================================================

struct Pos {
    int x;
    int y;
};

const int MAX_SNAKE = 256;
Pos snake[MAX_SNAKE];
int snakeLength = 3;

enum Dir {
    RIGHT = 0,
    LEFT = 1,
    UP = 2,
    DOWN = 3
};

Dir currentDir = RIGHT;
Dir nextDir = RIGHT;

Pos fruit;
int score = 0;

// =============================================================================
// --- TIMING ------------------------------------------------------------------
// =============================================================================

unsigned long lastMove = 0;
unsigned long lastBlink = 0;
bool blinkState = false;

// =============================================================================
// --- HILFSFUNKTIONEN ---------------------------------------------------------
// =============================================================================

// Berechne LED-Index für Position (X,Y)
int getIndex(int x, int y) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) {
        return -1;
    }
    
    // Vertical Zig-Zag Layout
    int index;
    if (x % 2 == 0) {
        // Gerade Spalte: von oben nach unten
        index = x * MATRIX_HEIGHT + y;
    } else {
        // Ungerade Spalte: von unten nach oben
        index = x * MATRIX_HEIGHT + (MATRIX_HEIGHT - 1 - y);
    }
    
    return index;
}

void setPixel(int x, int y, CRGB color) {
    int idx = getIndex(x, y);
    if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = color;
    }
}

void clearDisplay() {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
}

// =============================================================================
// --- MENU --------------------------------------------------------------------
// =============================================================================

void drawMenu() {
    clearDisplay();
    
    // Einfache Anzeige
    if (currentLevel == EASY) {
        setPixel(10, 4, CRGB::Yellow);
        setPixel(11, 4, CRGB::Yellow);
        setPixel(12, 4, CRGB::Yellow);
    }
    if (currentLevel == MEDIUM) {
        setPixel(10, 8, CRGB::Yellow);
        setPixel(11, 8, CRGB::Yellow);
        setPixel(12, 8, CRGB::Yellow);
    }
    if (currentLevel == HARD) {
        setPixel(10, 12, CRGB::Yellow);
        setPixel(11, 12, CRGB::Yellow);
        setPixel(12, 12, CRGB::Yellow);
    }
    
    FastLED.show();
}

void updateMenu() {
    if (joystick.neueRichtungOben()) {
        currentLevel = (Difficulty)((currentLevel - 1 + 3) % 3);
        Serial.print("Level: ");
        Serial.println(currentLevel);
    }
    if (joystick.neueRichtungUnten()) {
        currentLevel = (Difficulty)((currentLevel + 1) % 3);
        Serial.print("Level: ");
        Serial.println(currentLevel);
    }
    if (joystick.wurdeGedrueckt()) {
        Serial.println("START!");
        initGame();
        currentState = PLAYING;
    }
}

// =============================================================================
// --- SPIEL -------------------------------------------------------------------
// =============================================================================

void initGame() {
    snakeLength = 3;
    snake[0] = {MATRIX_WIDTH / 2, MATRIX_HEIGHT / 2};
    snake[1] = {MATRIX_WIDTH / 2 - 1, MATRIX_HEIGHT / 2};
    snake[2] = {MATRIX_WIDTH / 2 - 2, MATRIX_HEIGHT / 2};
    
    currentDir = RIGHT;
    nextDir = RIGHT;
    score = 0;
    
    generateFruit();
    lastMove = millis();
    
    Serial.println("=== GAME START ===");
    Serial.print("Snake at: X=");
    Serial.print(snake[0].x);
    Serial.print(" Y=");
    Serial.println(snake[0].y);
}

void generateFruit() {
    bool valid = false;
    while (!valid) {
        fruit.x = random(0, MATRIX_WIDTH);
        fruit.y = random(0, MATRIX_HEIGHT);
        
        valid = true;
        for (int i = 0; i < snakeLength; i++) {
            if (snake[i].x == fruit.x && snake[i].y == fruit.y) {
                valid = false;
                break;
            }
        }
    }
}

void moveSnake() {
    currentDir = nextDir;
    
    Pos newHead = snake[0];
    
    switch (currentDir) {
        case RIGHT: newHead.x++; break;
        case LEFT:  newHead.x--; break;
        case UP:    newHead.y--; break;
        case DOWN:  newHead.y++; break;
    }
    
    // Kollision mit Wand
    if (newHead.x < 0 || newHead.x >= MATRIX_WIDTH || 
        newHead.y < 0 || newHead.y >= MATRIX_HEIGHT) {
        currentState = GAME_OVER;
        Serial.println("GAME OVER: Wall");
        return;
    }
    
    // Kollision mit sich selbst
    for (int i = 0; i < snakeLength; i++) {
        if (newHead.x == snake[i].x && newHead.y == snake[i].y) {
            currentState = GAME_OVER;
            Serial.println("GAME OVER: Self");
            return;
        }
    }
    
    // Frucht gegessen?
    bool eatFruit = (newHead.x == fruit.x && newHead.y == fruit.y);
    
    if (eatFruit) {
        score++;
        Serial.print("Score: ");
        Serial.println(score);
        if (snakeLength < MAX_SNAKE) {
            snakeLength++;
        }
        generateFruit();
    }
    
    // Snake verschieben
    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = newHead;
}

void updateGame() {
    // Richtung ändern
    if (joystick.neueRichtungOben() && currentDir != DOWN) {
        nextDir = UP;
    }
    if (joystick.neueRichtungUnten() && currentDir != UP) {
        nextDir = DOWN;
    }
    if (joystick.neueRichtungLinks() && currentDir != RIGHT) {
        nextDir = LEFT;
    }
    if (joystick.neueRichtungRechts() && currentDir != LEFT) {
        nextDir = RIGHT;
    }
    
    // Snake bewegen
    unsigned long now = millis();
    if (now - lastMove >= SPEED[currentLevel]) {
        lastMove = now;
        moveSnake();
    }
}

void drawGame() {
    clearDisplay();
    
    // Frucht
    setPixel(fruit.x, fruit.y, CRGB::Red);
    
    // Snake
    for (int i = 0; i < snakeLength; i++) {
        CRGB color = (i == 0) ? CRGB::Green : CRGB::Lime;
        setPixel(snake[i].x, snake[i].y, color);
    }
    
    FastLED.show();
}

// =============================================================================
// --- GAME OVER ---------------------------------------------------------------
// =============================================================================

void drawGameOver() {
    clearDisplay();
    
    if (blinkState) {
        // Zeige Score als Punkte
        for (int i = 0; i < min(score, 10); i++) {
            setPixel(MATRIX_WIDTH / 2 - 5 + i, MATRIX_HEIGHT / 2, CRGB::Red);
        }
    }
    
    FastLED.show();
}

void updateGameOver() {
    unsigned long now = millis();
    if (now - lastBlink >= 500) {
        lastBlink = now;
        blinkState = !blinkState;
    }
    
    if (joystick.wurdeGedrueckt()) {
        currentState = MENU;
        Serial.println("Back to menu");
    }
}

// =============================================================================
// --- SETUP & LOOP ------------------------------------------------------------
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== SIMPLE SNAKE ===");
    
    FastLED.addLeds<WS2812, PIN_TOP, GRB>(leds, 0, MATRIX_WIDTH * MATRIX_HEIGHT / 2);
    FastLED.addLeds<WS2812, PIN_BOTTOM, GRB>(leds, MATRIX_WIDTH * MATRIX_HEIGHT / 2, MATRIX_WIDTH * MATRIX_HEIGHT / 2);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);
    
    randomSeed(analogRead(0));
    
    // Test: 4 Eckpunkte
    setPixel(0, 0, CRGB::White);
    setPixel(MATRIX_WIDTH - 1, 0, CRGB::Red);
    setPixel(0, MATRIX_HEIGHT - 1, CRGB::Blue);
    setPixel(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1, CRGB::Green);
    FastLED.show();
    delay(2000);
    
    Serial.println("Ready!");
}

void loop() {
    joystick.aktualisiere();
    
    switch (currentState) {
        case MENU:
            updateMenu();
            drawMenu();
            break;
            
        case PLAYING:
            updateGame();
            drawGame();
            break;
            
        case GAME_OVER:
            updateGameOver();
            drawGameOver();
            break;
    }
}
