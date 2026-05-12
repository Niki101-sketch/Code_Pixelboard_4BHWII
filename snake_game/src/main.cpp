#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include "Joystick.h"
#include "SnakeGame.h"

// Hardware
#define PIN_UNTEN 26
#define PIN_OBEN  25
#define JOY_X    33
#define JOY_Y    32
#define JOY_SW   14

#define SPIELFELD_BREITE 32
#define SPIELFELD_HOEHE  16

CRGB ledsUnten[256], ledsOben[256];
SnakeGame   game(SPIELFELD_BREITE, SPIELFELD_HOEHE);
Joystick    joy(JOY_X, JOY_Y, JOY_SW);
Preferences prefs;

// States: MENU → COUNTDOWN → PLAYING → GAMEOVER → HIGHSCORE → MENU
enum GameState { STATE_MENU, STATE_COUNTDOWN, STATE_PLAYING, STATE_GAMEOVER, STATE_HIGHSCORE };
volatile GameState currentState = STATE_MENU;

// Menü-Einstellungen (0=Speed, 1=Food, 2=WallWrap, 3=Farbe)
int menuSelection      = 0;
int selectedSpeedLevel = 2;  // 1–7
int selectedFoodAmount = 1;  // 1–8
volatile int selectedWallWrap = 0;
int selectedColor      = 0;  // 0=Grün  1=Blau  2=Lila  3=Regenbogen

volatile int  currentScore   = 0;
int           highScore      = 0;
volatile bool isNewHighscore = false;

volatile Direction nextDirection = DIR_RIGHT;
JoystickRichtung   letzteMenueRichtung = NEUTRAL;

volatile bool wechselAngefordert = false;

TaskHandle_t hInput   = NULL;
TaskHandle_t hGame    = NULL;
TaskHandle_t hDisplay = NULL;

const int MENU_Y[4] = {3, 6, 9, 12};

// ─────────────────────────────────────────────────────────────
// LED Mapping
// ─────────────────────────────────────────────────────────────
int berechneIndexUnten(int x, int y) {
    int s = 31 - x;
    return s * 8 + ((s % 2 == 0) ? (7 - y) : y);
}
int berechneIndexOben(int x, int y) {
    return x * 8 + ((x % 2 == 0) ? y : (7 - y));
}
void setPixel(int x, int y, CRGB f) {
    if (x < 0 || x >= 32 || y < 0 || y >= 16) return;
    if (y < 8) ledsOben[berechneIndexOben(x, y)] = f;
    else       ledsUnten[berechneIndexUnten(x, y - 8)] = f;
}

// ─────────────────────────────────────────────────────────────
// Schlangenfarben
// ─────────────────────────────────────────────────────────────
CRGB getBodyColor(int seg, int len) {
    uint8_t b = (uint8_t)max(40, 200 - (seg * 160 / max(len, 1)));
    switch (selectedColor) {
        case 1: return CRGB(0, 0, b);
        case 2: return CRGB(b / 2, 0, b);
        case 3: return CHSV((uint8_t)(seg * 255 / max(len, 1)), 220, b);
        default: return CRGB(0, b, 0);
    }
}
CRGB getHeadColor() {
    switch (selectedColor) {
        case 1: return CRGB(0, 180, 255);
        case 2: return CRGB(200, 0, 255);
        case 3: return CHSV(0, 255, 255);
        default: return CRGB::Lime;
    }
}

// ─────────────────────────────────────────────────────────────
// 3×5 Pixel-Font  (Bit2=links, Bit1=mitte, Bit0=rechts)
// ─────────────────────────────────────────────────────────────
const uint8_t DIGIT_FONT[10][5] = {
    {7,5,5,5,7}, // 0
    {2,6,2,2,7}, // 1
    {7,1,7,4,7}, // 2
    {7,1,7,1,7}, // 3
    {5,5,7,1,1}, // 4
    {7,4,7,1,7}, // 5
    {7,4,7,5,7}, // 6
    {7,1,1,1,1}, // 7
    {7,5,7,5,7}, // 8
    {7,5,7,1,7}, // 9
};
const uint8_t LETTER_S[5] = {7,4,7,1,7};
const uint8_t LETTER_H[5] = {5,5,7,5,5};

void drawChar(int x, int y, const uint8_t p[5], CRGB c) {
    for (int r = 0; r < 5; r++) {
        if (p[r] & 4) setPixel(x,     y + r, c);
        if (p[r] & 2) setPixel(x + 1, y + r, c);
        if (p[r] & 1) setPixel(x + 2, y + r, c);
    }
}
void drawDigit(int x, int y, int d, CRGB c) { drawChar(x, y, DIGIT_FONT[d], c); }

void drawColon(int x, int y, CRGB c) {   // 2 Punkte übereinander
    setPixel(x, y + 1, c);
    setPixel(x, y + 3, c);
}

// Zahl zentriert im rechten Bereich (x = 9..30) — für Highscore-Screen
void drawNumberRight(int y, int num, CRGB c) {
    num = constrain(num, 0, 999);
    int d[3]; int n = 0;
    if (num >= 100) d[n++] = num / 100;
    if (n > 0 || num >= 10) d[n++] = (num / 10) % 10;
    d[n++] = num % 10;
    int sx = 9 + (21 - (n * 4 - 1)) / 2;
    for (int i = 0; i < n; i++) drawDigit(sx + i * 4, y, d[i], c);
}

// Zahl zentriert auf dem gesamten 32px-Bildschirm — für Countdown
void drawNumberCenter(int y, int num, CRGB c) {
    num = constrain(num, 0, 9);
    int sx = (SPIELFELD_BREITE - 3) / 2;
    drawDigit(sx, y, num, c);
}

// ─────────────────────────────────────────────────────────────
// Startup-Animation: grüne Schlange läuft einmal ums Spielfeld
// ─────────────────────────────────────────────────────────────
void runStartAnimation() {
    // Randpfad: oben → rechts → unten → links  (92 Punkte)
    const int N = 92;
    int bx[N], by[N];
    int idx = 0;
    for (int x = 0;  x < 32; x++) { bx[idx] = x;  by[idx] = 0;  idx++; }
    for (int y = 1;  y < 16; y++) { bx[idx] = 31; by[idx] = y;  idx++; }
    for (int x = 30; x >= 0; x--) { bx[idx] = x;  by[idx] = 15; idx++; }
    for (int y = 14; y >= 1; y--) { bx[idx] = 0;  by[idx] = y;  idx++; }

    const int TAIL = 8;
    for (int step = 0; step < N + TAIL; step++) {
        FastLED.clear();
        for (int t = 0; t < TAIL; t++) {
            int pos = step - t;
            if (pos < 0 || pos >= N) continue;
            uint8_t b = (uint8_t)max(0, 210 - t * 26);
            setPixel(bx[pos], by[pos], (t == 0) ? CRGB::Lime : CRGB(0, b, 0));
        }
        FastLED.show();
        delay(16); // ~62 Schritte/s → ~1.5 s Gesamtdauer
    }
    delay(120);
    FastLED.clear();
    FastLED.show();
}

// ─────────────────────────────────────────────────────────────
// Hilfsfunktion: weißer Spielfeldrand
// ─────────────────────────────────────────────────────────────
void drawBorder() {
    for (int x = 0; x < 32; x++) { setPixel(x, 0, CRGB::White); setPixel(x, 15, CRGB::White); }
    for (int y = 0; y < 16; y++) { setPixel(0, y, CRGB::White); setPixel(31, y, CRGB::White); }
}

// ─────────────────────────────────────────────────────────────
// Task 1: Input  (Core 1, Prio 3)
// ─────────────────────────────────────────────────────────────
void taskInput(void *pvParameters) {
    while (1) {
        joy.aktualisiere();
        JoystickRichtung dir = joy.getRichtung();

        if (currentState == STATE_MENU) {
            if (dir != letzteMenueRichtung) {
                if (dir == OBEN  && menuSelection > 0) menuSelection--;
                if (dir == UNTEN && menuSelection < 3) menuSelection++;

                if      (menuSelection == 0) {
                    if (dir == RECHTS && selectedSpeedLevel < 7) selectedSpeedLevel++;
                    if (dir == LINKS  && selectedSpeedLevel > 1) selectedSpeedLevel--;
                }
                else if (menuSelection == 1) {
                    if (dir == RECHTS && selectedFoodAmount < 8) selectedFoodAmount++;
                    if (dir == LINKS  && selectedFoodAmount > 1) selectedFoodAmount--;
                }
                else if (menuSelection == 2) {
                    if (dir == RECHTS || dir == LINKS) selectedWallWrap = !selectedWallWrap;
                }
                else if (menuSelection == 3) {
                    if (dir == RECHTS) selectedColor = (selectedColor + 1) % 4;
                    if (dir == LINKS)  selectedColor = (selectedColor + 3) % 4;
                }
                letzteMenueRichtung = dir;
            }
            if (joy.wurdeGedrueckt()) {
                game.setWallWrap(selectedWallWrap == 1);
                game.reset(selectedFoodAmount);
                nextDirection = DIR_RIGHT;
                currentState  = STATE_COUNTDOWN;   // erst Countdown, dann Spiel
            }
        }
        else if (currentState == STATE_COUNTDOWN || currentState == STATE_PLAYING) {
            // Richtungseingabe auch während Countdown möglich
            if      (dir == OBEN)   nextDirection = DIR_UP;
            else if (dir == UNTEN)  nextDirection = DIR_DOWN;
            else if (dir == LINKS)  nextDirection = DIR_LEFT;
            else if (dir == RECHTS) nextDirection = DIR_RIGHT;
        }
        else if (currentState == STATE_HIGHSCORE) {
            if (joy.wurdeGedrueckt()) currentState = STATE_MENU;
        }

        // Langer Druck (>1s) → App-Wechsel anfordern
        if (joy.wurdeLangeGedrueckt()) {
            wechselAngefordert = true;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─────────────────────────────────────────────────────────────
// Task 2: Spiellogik  (Core 1, Prio 2)
// ─────────────────────────────────────────────────────────────
void taskGameLogic(void *pvParameters) {
    while (1) {
        if (currentState == STATE_PLAYING) {
            game.setDirection(nextDirection);
            if (!game.update()) {
                currentScore   = game.getScore();
                isNewHighscore = (currentScore > highScore);
                if (isNewHighscore) highScore = currentScore;
                currentState   = STATE_GAMEOVER;
            }
        }
        int bonus    = min(game.getScore() * 2, 60);
        int delay_ms = max(50, 360 - (selectedSpeedLevel * 45) - bonus);
        vTaskDelay(pdMS_TO_TICKS(currentState == STATE_PLAYING ? delay_ms : 100));
    }
}

// ─────────────────────────────────────────────────────────────
// Task 3: Display  (Core 0, ~30 FPS)
// ─────────────────────────────────────────────────────────────
void taskDisplay(void *pvParameters) {
    int blinkTick = 0;
    while (1) {
        // App-Wechsel: weißer Wischeffekt → zurück zu Menü (Platzhalter)
        if (wechselAngefordert) {
            for (int x = 0; x < 32; x++) {
                FastLED.clear();
                for (int y = 0; y < 16; y++) setPixel(x, y, CRGB::White);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            FastLED.clear();
            FastLED.show();
            currentState       = STATE_MENU;
            wechselAngefordert = false;
        }

        FastLED.clear();
        blinkTick++;

        // ── MENÜ ─────────────────────────────────────────────
        if (currentState == STATE_MENU) {
            for (int x = 0; x < 32; x++) {
                setPixel(x, 0,  CRGB::White);
                setPixel(x, 15, CRGB::White);
            }

            // Blinkender Cursor
            CRGB cur = (blinkTick % 20 < 10) ? CRGB::White : CRGB(55, 55, 55);
            setPixel(4, MENU_Y[menuSelection], cur);

            // Speed (blau)
            for (int i = 0; i < selectedSpeedLevel; i++)
                setPixel(8 + i * 2, MENU_Y[0], CRGB::Blue);

            // Food (rot)
            for (int i = 0; i < selectedFoodAmount; i++)
                setPixel(8 + i * 2, MENU_Y[1], CRGB::Red);

            // Wall-Wrap (grün/dunkelrot)
            CRGB wc = (selectedWallWrap == 1) ? CRGB::Green : CRGB(50, 0, 0);
            setPixel(8, MENU_Y[2], wc); setPixel(10, MENU_Y[2], wc);

            // Farbe – Mini-Schlangen-Vorschau
            setPixel(8,  MENU_Y[3], getHeadColor());
            setPixel(10, MENU_Y[3], getBodyColor(1, 3));
            setPixel(12, MENU_Y[3], getBodyColor(2, 3));
        }

        // ── COUNTDOWN 3-2-1 ──────────────────────────────────
        else if (currentState == STATE_COUNTDOWN) {
            for (int num = 3; num >= 1; num--) {
                FastLED.clear();
                drawBorder();
                // Zahl groß in der Mitte anzeigen (y=5, 5 Zeilen hoch)
                drawNumberCenter(5, num, CRGB::White);
                // Fortschrittsbalken unten: zeigt wie lange noch
                int barLen = (num - 1) * 10; // 3→20, 2→10, 1→0
                for (int x = 0; x < barLen; x++)
                    setPixel(1 + x, 14, CRGB(0, 80, 0));
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(750));
            }
            // GO! – grüner Blitz
            fill_solid(ledsOben,  256, CRGB::Green);
            fill_solid(ledsUnten, 256, CRGB::Green);
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(180));
            currentState = STATE_PLAYING;
        }

        // ── SPIELEN ──────────────────────────────────────────
        else if (currentState == STATE_PLAYING) {
            drawBorder();

            // Schlange mit Farbverlauf
            Point* b   = game.getBody();
            int    len = game.getLength();
            for (int i = len - 1; i > 0; i--)
                setPixel(b[i].x, b[i].y, getBodyColor(i, len));
            setPixel(b[0].x, b[0].y, getHeadColor());

            // Blinkendes Food
            bool hell = (blinkTick % 10 < 7);
            for (int i = 0; i < selectedFoodAmount; i++)
                setPixel(game.getFoodArray()[i].x, game.getFoodArray()[i].y,
                         hell ? CRGB::Red : CRGB(150, 0, 0));
        }

        // ── GAME OVER ─────────────────────────────────────────
        else if (currentState == STATE_GAMEOVER) {
            CRGB col = isNewHighscore ? CRGB::Gold : CRGB::Red;
            for (int f = 0; f < 4; f++) {
                fill_solid(ledsOben,  256, col);
                fill_solid(ledsUnten, 256, col);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(180));
                FastLED.clear();
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(130));
            }
            // Highscore dauerhaft im Flash speichern
            if (isNewHighscore) prefs.putInt("hs", highScore);
            currentState = STATE_HIGHSCORE;
        }

        // ── HIGHSCORE-SCREEN ──────────────────────────────────
        // "S:" links (x=1–5), Zahl in der Mitte-rechts
        // "H:" links (x=1–5), Zahl in der Mitte-rechts
        else if (currentState == STATE_HIGHSCORE) {
            for (int x = 0; x < 32; x++) {
                setPixel(x, 0,  CRGB::White);
                setPixel(x, 15, CRGB::White);
            }

            // Score (gelb, y=2..6)
            drawChar(1, 2, LETTER_S, CRGB::Yellow);
            drawColon(5, 2, CRGB::Yellow);
            drawNumberRight(2, currentScore, CRGB::Yellow);

            // Trennlinie (y=8, sehr gedimmt)
            for (int x = 2; x < 30; x += 2) setPixel(x, 8, CRGB(22, 22, 22));

            // Highscore (cyan, y=9..13) — blinkt gold bei neuem Rekord
            CRGB hc = (isNewHighscore && (blinkTick % 16 < 8)) ? CRGB::Gold : CRGB::Cyan;
            drawChar(1, 9, LETTER_H, hc);
            drawColon(5, 9, hc);
            drawNumberRight(9, highScore, hc);

            // Button-Hinweis: blinkendes weißes Pixel (y=14, Mitte)
            if (blinkTick % 20 < 10) setPixel(16, 14, CRGB::White);
        }

        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}

// ─────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────
void setup() {
    FastLED.addLeds<WS2812, PIN_UNTEN, GRB>(ledsUnten, 256);
    FastLED.addLeds<WS2812, PIN_OBEN,  GRB>(ledsOben,  256);
    FastLED.setBrightness(20);

    // Highscore aus dem Flash laden
    prefs.begin("snake", false);
    highScore = prefs.getInt("hs", 0);

    // Startup-Animation (läuft einmalig beim Einschalten)
    runStartAnimation();

    xTaskCreatePinnedToCore(taskInput,     "Input",   2048, NULL, 3, &hInput,   1);
    xTaskCreatePinnedToCore(taskGameLogic, "Game",    2048, NULL, 2, &hGame,    1);
    xTaskCreatePinnedToCore(taskDisplay,   "Display", 4096, NULL, 1, &hDisplay, 0);
}

void loop() {}
