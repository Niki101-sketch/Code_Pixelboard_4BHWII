#include <Preferences.h>
#include <math.h>
#include "AppPong.h"
#include "PixelFont.h"

static Preferences prefs;

// ─── Konstanten ───────────────────────────────────────────────────────────────
static const int   PADDLE_H  = 3;    // Paddle-Höhe in Pixeln
static const int   MAX_SCORE = 5;    // Punkte bis zum Sieg
static const int   TICK_MS   = 80;   // ms pro Ball-Schritt
static const float MAX_SPEED = 2.0f; // maximale Ball-Geschwindigkeit
static const float SPEED_INC = 0.15f;// Speedup pro Paddle-Treffer

// ─── Spielzustand ─────────────────────────────────────────────────────────────
enum PongState { PONG_MENU, PONG_PLAYING, PONG_WIN_FLASH, PONG_WIN_SHOW };
static volatile PongState pongState = PONG_MENU;

static float  ballX  = 15.5f, ballY  = 7.5f;
static float  ballDX = 1.0f,  ballDY = 0.7f;
static int    paddle1Y = 6,   paddle2Y = 6;
static volatile int score1 = 0, score2 = 0;
static int    highScore1 = 0, highScore2 = 0;
static int    winPlayer  = 1;

// ─── Input (aus taskInput, vom Display-Task gelesen) ──────────────────────────
static volatile JoystickRichtung inp1 = NEUTRAL;
static volatile JoystickRichtung inp2 = NEUTRAL;
static volatile bool inputPressed     = false;

// ─── Hilfsfunktionen ──────────────────────────────────────────────────────────
static void resetBall(bool towardLeft) {
    ballX  = 15.5f;
    ballY  = (float)(random(2, 14));
    float spd = 1.0f;
    ballDX = towardLeft ? -spd : spd;
    ballDY = (random(2) ? 1.0f : -1.0f) * 0.7f;
}

// ─── Öffentliche Schnittstelle ────────────────────────────────────────────────
void pongInit() {
    prefs.begin("pong", false);
    highScore1 = prefs.getInt("hs1", 0);
    highScore2 = prefs.getInt("hs2", 0);
    pongState  = PONG_MENU;
    score1     = 0;
    score2     = 0;
    resetBall(false);
}

void pongResetToMenu() {
    score1       = 0;
    score2       = 0;
    pongState    = PONG_MENU;
    inp1         = NEUTRAL;
    inp2         = NEUTRAL;
    inputPressed = false;
    resetBall(false);
}

void pongHandleInput(JoystickRichtung dir1, JoystickRichtung dir2, bool pressed) {
    inp1 = dir1;
    inp2 = dir2;
    if (pressed) inputPressed = true;
}

// ─── Display-Task ─────────────────────────────────────────────────────────────
void taskPongDisplay(void *pvParameters) {
    unsigned long lastTick = 0;
    int blinkTick = 0;

    while (1) {
        if (currentApp != APP_PONG) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        FastLED.clear();
        blinkTick++;

        // ── MENU ──────────────────────────────────────────────────────────────
        if (pongState == PONG_MENU) {
            // "PONG" zentriert, 4 Zeichen × 4px = 19px → x=7..25
            pfDrawChar(7,  3, PF_P, CRGB::White);
            pfDrawDigit(11, 3, 0,   CRGB::White);  // O = Ziffer 0
            pfDrawChar(15, 3, PF_N, CRGB::White);
            pfDrawChar(19, 3, PF_G, CRGB::White);

            // Highscores: "1:X" links grün, "2:X" rechts blau
            pfDrawDigit(1,  9, 1, CRGB(0, 180, 0));
            pfDrawColon(5,  9, CRGB(0, 180, 0));
            pfDrawDigit(7,  9, highScore1 % 10, CRGB(0, 180, 0));

            pfDrawDigit(17, 9, 2, CRGB(0, 80, 255));
            pfDrawColon(21, 9, CRGB(0, 80, 255));
            pfDrawDigit(23, 9, highScore2 % 10, CRGB(0, 80, 255));

            // Blinkendes Startzeichen
            if (blinkTick % 20 < 10) {
                setPixel(15, 14, CRGB::White);
                setPixel(16, 14, CRGB::White);
            }

            if (inputPressed) {
                inputPressed = false;
                lastTick     = millis();
                paddle1Y     = 6;
                paddle2Y     = 6;
                resetBall(false);
                pongState    = PONG_PLAYING;
            }
        }

        // ── PLAYING ───────────────────────────────────────────────────────────
        else if (pongState == PONG_PLAYING) {
            // Paddle-Bewegung (jeder Frame = 33ms)
            if (inp1 == OBEN  && paddle1Y > 0)             paddle1Y--;
            if (inp1 == UNTEN && paddle1Y < 16 - PADDLE_H) paddle1Y++;
            if (inp2 == OBEN  && paddle2Y > 0)             paddle2Y--;
            if (inp2 == UNTEN && paddle2Y < 16 - PADDLE_H) paddle2Y++;

            // Ball-Schritt alle TICK_MS
            unsigned long now = millis();
            if (now - lastTick >= (unsigned long)TICK_MS) {
                lastTick = now;

                ballX += ballDX;
                ballY += ballDY;

                // Ober-/Unterwand
                if (ballY <= 0.0f)  { ballY = 0.0f;  ballDY =  fabsf(ballDY); }
                if (ballY >= 15.0f) { ballY = 15.0f; ballDY = -fabsf(ballDY); }

                // Linkes Paddle (x=1)
                if (ballDX < 0.0f && ballX <= 1.5f) {
                    int by = (int)roundf(ballY);
                    if (by >= paddle1Y && by < paddle1Y + PADDLE_H) {
                        float spd = fabsf(ballDX) + SPEED_INC;
                        if (spd > MAX_SPEED) spd = MAX_SPEED;
                        ballDX = spd;
                        float rel = (ballY - paddle1Y) / (float)PADDLE_H - 0.5f;
                        ballDY = rel * 2.0f;
                        if (fabsf(ballDY) < 0.3f) ballDY = (ballDY >= 0 ? 0.3f : -0.3f);
                        ballX = 1.5f;
                    } else if (ballX < 0.5f) {
                        score2++;
                        if (score2 >= MAX_SCORE) { winPlayer = 2; pongState = PONG_WIN_FLASH; }
                        else                       resetBall(true);
                    }
                }

                // Rechtes Paddle (x=30)
                if (ballDX > 0.0f && ballX >= 29.5f) {
                    int by = (int)roundf(ballY);
                    if (by >= paddle2Y && by < paddle2Y + PADDLE_H) {
                        float spd = fabsf(ballDX) + SPEED_INC;
                        if (spd > MAX_SPEED) spd = MAX_SPEED;
                        ballDX = -spd;
                        float rel = (ballY - paddle2Y) / (float)PADDLE_H - 0.5f;
                        ballDY = rel * 2.0f;
                        if (fabsf(ballDY) < 0.3f) ballDY = (ballDY >= 0 ? 0.3f : -0.3f);
                        ballX = 29.5f;
                    } else if (ballX > 30.5f) {
                        score1++;
                        if (score1 >= MAX_SCORE) { winPlayer = 1; pongState = PONG_WIN_FLASH; }
                        else                       resetBall(false);
                    }
                }
            }

            // Mittellinie (gedimmt, gestrichelt)
            for (int y = 1; y < 15; y += 2) setPixel(15, y, CRGB(20, 20, 20));

            // Paddles
            for (int py = 0; py < PADDLE_H; py++) {
                setPixel(1,  paddle1Y + py, CRGB::Green);
                setPixel(30, paddle2Y + py, CRGB(0, 80, 255));
            }

            // Ball
            setPixel((int)roundf(ballX), (int)roundf(ballY), CRGB::White);

            // Score-Anzeige oben
            pfDrawDigit(12, 0, score1, CRGB(0, 150, 0));
            pfDrawDigit(17, 0, score2, CRGB(0, 50, 200));
        }

        // ── WIN FLASH ─────────────────────────────────────────────────────────
        else if (pongState == PONG_WIN_FLASH) {
            CRGB wc = (winPlayer == 1) ? CRGB::Green : CRGB(0, 80, 255);
            for (int f = 0; f < 5; f++) {
                fill_solid(ledsTop,    256, wc);
                fill_solid(ledsBottom, 256, wc);
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(150));
                FastLED.clear();
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            if (winPlayer == 1 && score1 > highScore1) {
                highScore1 = score1;
                prefs.putInt("hs1", highScore1);
            }
            if (winPlayer == 2 && score2 > highScore2) {
                highScore2 = score2;
                prefs.putInt("hs2", highScore2);
            }
            inputPressed = false;
            pongState    = PONG_WIN_SHOW;
            continue;
        }

        // ── WIN SHOW ──────────────────────────────────────────────────────────
        else if (pongState == PONG_WIN_SHOW) {
            CRGB wc = (winPlayer == 1) ? CRGB::Green : CRGB(0, 80, 255);

            // "P1 WIN" oder "P2 WIN"
            pfDrawChar(3,  2, PF_P, wc);
            pfDrawDigit(7,  2, winPlayer, wc);
            pfDrawChar(13, 2, PF_W, wc);
            pfDrawChar(17, 2, PF_I, wc);
            pfDrawChar(21, 2, PF_N, wc);

            // Endstand
            pfDrawDigit(10, 9, score1, CRGB(0, 150, 0));
            pfDrawColon(14, 9, CRGB::White);
            pfDrawDigit(17, 9, score2, CRGB(0, 50, 200));

            if (inputPressed) {
                inputPressed = false;
                score1       = 0;
                score2       = 0;
                pongState    = PONG_MENU;
            }
        }

        showLeds();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
