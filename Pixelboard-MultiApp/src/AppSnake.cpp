#include <Preferences.h>
#include "AppSnake.h"
#include "SnakeGame.h"
#include "PixelFont.h"

// ─── Spielfeld ────────────────────────────────────────────────────────────────
#define BREITE 32
#define HOEHE  16

// ─── Snake-Spielzustand ───────────────────────────────────────────────────────
static SnakeGame game(BREITE, HOEHE);
static Preferences prefs;

enum GameState { STATE_MENU, STATE_COUNTDOWN, STATE_PLAYING, STATE_GAMEOVER, STATE_HIGHSCORE };
static volatile GameState currentState = STATE_MENU;

static int menuSelection      = 0;
static int selectedSpeedLevel = 2;
static int selectedFoodAmount = 1;
static volatile int  selectedWallWrap  = 0;
static int selectedColor      = 0;  // 0=Grün  1=Blau  2=Lila  3=Regenbogen

static volatile int  currentScore   = 0;
static int           highScore      = 0;
static volatile bool isNewHighscore = false;

static volatile Direction nextDirection = DIR_RIGHT;
static JoystickRichtung   letzteMenueRichtung = NEUTRAL;

static const int MENU_Y[4] = {3, 6, 9, 12};

// ─── Schlangenfarben ──────────────────────────────────────────────────────────
static CRGB getBodyColor(int seg, int len) {
    uint8_t b = (uint8_t)max(40, 200 - (seg * 160 / max(len, 1)));
    switch (selectedColor) {
        case 1: return CRGB(0, 0, b);
        case 2: return CRGB(b / 2, 0, b);
        case 3: return CHSV((uint8_t)(seg * 255 / max(len, 1)), 220, b);
        default: return CRGB(0, b, 0);
    }
}
static CRGB getHeadColor() {
    switch (selectedColor) {
        case 1: return CRGB(0, 180, 255);
        case 2: return CRGB(200, 0, 255);
        case 3: return CHSV(0, 255, 255);
        default: return CRGB::Lime;
    }
}

// ─── 3×5 Pixel-Font → PixelFont.h ────────────────────────────────────────────
static void drawNumberCenter(int y, int num, CRGB c) {
    num = constrain(num, 0, 9);
    pfDrawDigit((BREITE - 3) / 2, y, num, c);
}
static void drawBorder() {
    for (int x = 0; x < 32; x++) { setPixel(x, 0, CRGB::White); setPixel(x, 15, CRGB::White); }
    for (int y = 0; y < 16; y++) { setPixel(0, y, CRGB::White); setPixel(31, y, CRGB::White); }
}

// ─── Startup-Animation ────────────────────────────────────────────────────────
static void runStartAnimation() {
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
        delay(16);
    }
    delay(120);
    FastLED.clear();
    FastLED.show();
}

// ─── Öffentliche Schnittstelle ────────────────────────────────────────────────
void snakeInit() {
    prefs.begin("snake", false);
    highScore = prefs.getInt("hs", 0);
    runStartAnimation();
}

void snakeResetToMenu() {
    currentState        = STATE_MENU;
    menuSelection       = 0;
    nextDirection       = DIR_RIGHT;
    letzteMenueRichtung = NEUTRAL;
}

void snakeHandleInput(JoystickRichtung dir, bool pressed) {
    if (currentState == STATE_MENU) {
        if (dir != letzteMenueRichtung) {
            if (dir == OBEN  && menuSelection > 0) menuSelection--;
            if (dir == UNTEN && menuSelection < 3) menuSelection++;

            if (menuSelection == 0) {
                if (dir == RECHTS && selectedSpeedLevel < 7) selectedSpeedLevel++;
                if (dir == LINKS  && selectedSpeedLevel > 1) selectedSpeedLevel--;
            } else if (menuSelection == 1) {
                if (dir == RECHTS && selectedFoodAmount < 8) selectedFoodAmount++;
                if (dir == LINKS  && selectedFoodAmount > 1) selectedFoodAmount--;
            } else if (menuSelection == 2) {
                if (dir == RECHTS || dir == LINKS) selectedWallWrap = !selectedWallWrap;
            } else if (menuSelection == 3) {
                if (dir == RECHTS) selectedColor = (selectedColor + 1) % 4;
                if (dir == LINKS)  selectedColor = (selectedColor + 3) % 4;
            }
            letzteMenueRichtung = dir;
        }
        if (pressed) {
            game.setWallWrap(selectedWallWrap == 1);
            game.reset(selectedFoodAmount);
            nextDirection = DIR_RIGHT;
            currentState  = STATE_COUNTDOWN;
        }
    } else if (currentState == STATE_COUNTDOWN || currentState == STATE_PLAYING) {
        if      (dir == OBEN)   nextDirection = DIR_UP;
        else if (dir == UNTEN)  nextDirection = DIR_DOWN;
        else if (dir == LINKS)  nextDirection = DIR_LEFT;
        else if (dir == RECHTS) nextDirection = DIR_RIGHT;
    } else if (currentState == STATE_HIGHSCORE) {
        if (pressed) currentState = STATE_MENU;
    }
}

// ─── taskSnakeLogic ───────────────────────────────────────────────────────────
void taskSnakeLogic(void *pvParameters) {
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

// ─── taskSnakeDisplay ─────────────────────────────────────────────────────────
void taskSnakeDisplay(void *pvParameters) {
    int blinkTick = 0;
    while (1) {
        // Nur zeichnen, wenn Snake aktiv ist – verhindert Fremd-Frames im
        // gemeinsamen LED-Puffer beim Umschalten.
        if (currentApp != APP_SNAKE) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        FastLED.clear();
        blinkTick++;

        if (currentState == STATE_MENU) {
            for (int x = 0; x < 32; x++) {
                setPixel(x, 0,  CRGB::White);
                setPixel(x, 15, CRGB::White);
            }
            CRGB cur = (blinkTick % 20 < 10) ? CRGB::White : CRGB(55, 55, 55);
            setPixel(4, MENU_Y[menuSelection], cur);

            for (int i = 0; i < selectedSpeedLevel; i++)
                setPixel(8 + i * 2, MENU_Y[0], CRGB::Blue);
            for (int i = 0; i < selectedFoodAmount; i++)
                setPixel(8 + i * 2, MENU_Y[1], CRGB::Red);

            CRGB wc = (selectedWallWrap == 1) ? CRGB::Green : CRGB(50, 0, 0);
            setPixel(8,  MENU_Y[2], wc); setPixel(10, MENU_Y[2], wc);

            setPixel(8,  MENU_Y[3], getHeadColor());
            setPixel(10, MENU_Y[3], getBodyColor(1, 3));
            setPixel(12, MENU_Y[3], getBodyColor(2, 3));
        }

        else if (currentState == STATE_COUNTDOWN) {
            for (int num = 3; num >= 1; num--) {
                FastLED.clear();
                drawBorder();
                drawNumberCenter(5, num, CRGB::White);
                int barLen = (num - 1) * 10;
                for (int x = 0; x < barLen; x++)
                    setPixel(1 + x, 14, CRGB(0, 80, 0));
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(750));
            }
            fill_solid(ledsTop,    256, CRGB::Green);
            fill_solid(ledsBottom, 256, CRGB::Green);
            FastLED.show();
            vTaskDelay(pdMS_TO_TICKS(180));
            currentState = STATE_PLAYING;
            continue;  // kein FastLED.show() mit leerem Buffer am Loop-Ende
        }

        else if (currentState == STATE_PLAYING) {
            drawBorder();
            Point* b   = game.getBody();
            int    len = game.getLength();
            for (int i = len - 1; i > 0; i--)
                setPixel(b[i].x, b[i].y, getBodyColor(i, len));
            setPixel(b[0].x, b[0].y, getHeadColor());

            bool hell = (blinkTick % 10 < 7);
            for (int i = 0; i < selectedFoodAmount; i++)
                setPixel(game.getFoodArray()[i].x, game.getFoodArray()[i].y,
                         hell ? CRGB::Red : CRGB(150, 0, 0));
        }

        else if (currentState == STATE_GAMEOVER) {
            CRGB col = isNewHighscore ? CRGB::Gold : CRGB::Red;
            for (int f = 0; f < 4; f++) {
                fill_solid(ledsTop,    256, col);
                fill_solid(ledsBottom, 256, col);
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(180));
                FastLED.clear();
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(130));
            }
            if (isNewHighscore) prefs.putInt("hs", highScore);
            currentState = STATE_HIGHSCORE;
            continue;  // kein FastLED.show() mit leerem Buffer am Loop-Ende
        }

        else if (currentState == STATE_HIGHSCORE) {
            for (int x = 0; x < 32; x++) {
                setPixel(x, 0,  CRGB::White);
                setPixel(x, 15, CRGB::White);
            }
            pfDrawChar(1, 2, PF_S, CRGB::Yellow);
            pfDrawColon(5, 2, CRGB::Yellow);
            pfDrawNumberRight(2, currentScore, CRGB::Yellow);

            for (int x = 2; x < 30; x += 2) setPixel(x, 8, CRGB(22, 22, 22));

            CRGB hc = (isNewHighscore && (blinkTick % 16 < 8)) ? CRGB::Gold : CRGB::Cyan;
            pfDrawChar(1, 9, PF_H, hc);
            pfDrawColon(5, 9, hc);
            pfDrawNumberRight(9, highScore, hc);

            if (blinkTick % 20 < 10) setPixel(16, 14, CRGB::White);
        }

        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
