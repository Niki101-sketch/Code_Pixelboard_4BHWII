#include <Preferences.h>
#include "AppPacman.h"
#include "PixelFont.h"

// ─── Spielfeld 32×16 ──────────────────────────────────────────────────────────
#define PM_W 32
#define PM_H 16

// W=Wand, .=Dot — jede Zeile exakt 32 Zeichen (alle Längen verifiziert)
// Geisterhaus Reihen 6-8, x=11-20. Ausgänge bei x=14 und x=17.
static const char MAZE[PM_H][PM_W + 1] = {
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",  // row  0: 32W
    "W..............................W",   // row  1: 1+30+1=32
    "W.WWWWWWWWWWW......WWWWWWWWWWW.W",  // row  2: 1+1+11+6+11+1+1=32
    "W..............................W",   // row  3: 1+30+1=32
    "W.WWWWWWWWWWW......WWWWWWWWWWW.W",  // row  4: same as row 2
    "W...W..........WW..........W...W",  // row  5: 1+3+1+10+2+10+1+3+1=32
    "W..........WWW.WW.WWW..........W",  // row  6: 1+10+3+1+2+1+3+10+1=32
    "W..........W........W..........W",  // row  7: 1+10+1+8+1+10+1=32
    "W..........WWW.WW.WWW..........W",  // row  8: same as row 6
    "W...W..........WW..........W...W",  // row  9: same as row 5
    "W.WWWWWWWWWWW......WWWWWWWWWWW.W",  // row 10: same as row 2
    "W..............................W",   // row 11: 1+30+1=32
    "W.WWWWWWWWWWW......WWWWWWWWWWW.W",  // row 12: same as row 2
    "W..............................W",   // row 13: 1+30+1=32
    "W..............................W",   // row 14: 1+30+1=32
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",  // row 15: 32W
};

static inline bool isWall(int x, int y) {
    if (x < 0 || x >= PM_W || y < 0 || y >= PM_H) return true;
    return MAZE[y][x] == 'W';
}

// ─── Spielelemente ────────────────────────────────────────────────────────────
enum PacDir { PDIR_LEFT = 0, PDIR_RIGHT, PDIR_UP, PDIR_DOWN };

struct Ghost {
    int x, y;
    PacDir dir;
    bool scared;
    bool eaten;     // kurz unsichtbar nach Fressen
    uint32_t eatenTimer;
};

static const int GHOST_COUNT = 2;
static const int GHOST_HOME_X[2] = {14, 17};
static const int GHOST_HOME_Y    = 7;
static const CRGB GHOST_COLORS[2] = { CRGB::Red, CRGB::Cyan };

// Dot-Map (1 Bit pro Zelle, true = Dot noch vorhanden)
static bool dots[PM_H][PM_W];
static int8_t powerPellets[4][2]; // [idx][0=x, 1=y], -1 = gefressen

static const int PP_POS[4][2] = {{1,1},{30,1},{1,13},{30,13}};

static int  pacX, pacY;
static PacDir pacDir, pacNextDir;
static volatile int  pacScore  = 0;
static int  pacHighScore       = 0;
static int  totalDots          = 0;
static int  dotsEaten          = 0;
static bool newHighscore       = false;

static Ghost ghosts[GHOST_COUNT];

static uint32_t scaredTimer = 0;
static bool anyScared       = false;

static Preferences pacPrefs;

enum PacState { PAC_MENU, PAC_PLAYING, PAC_GAMEOVER, PAC_WIN };
static volatile PacState pacState = PAC_MENU;

// ─── Maze-Reset: alle Dots setzen ────────────────────────────────────────────
static void resetDots() {
    totalDots  = 0;
    dotsEaten  = 0;
    for (int y = 0; y < PM_H; y++)
        for (int x = 0; x < PM_W; x++)
            dots[y][x] = false;

    // Dots auf allen freien (Nicht-Wand) Zellen außer Geisterhaus-Inneres
    for (int y = 1; y < PM_H - 1; y++) {
        for (int x = 1; x < PM_W - 1; x++) {
            if (!isWall(x, y)) {
                // Geisterhaus-Inneres (rows 6-8, cols 12-19) leer lassen
                bool inGhostHouse = (y >= 6 && y <= 8 && x >= 12 && x <= 19);
                if (!inGhostHouse) {
                    dots[y][x] = true;
                    totalDots++;
                }
            }
        }
    }
    // Power-Pellets registrieren (überschreiben einen Dot)
    for (int i = 0; i < 4; i++) {
        powerPellets[i][0] = PP_POS[i][0];
        powerPellets[i][1] = PP_POS[i][1];
    }
}

// ─── Geister initialisieren ───────────────────────────────────────────────────
static void resetGhosts() {
    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].x = GHOST_HOME_X[i];
        ghosts[i].y = GHOST_HOME_Y;
        ghosts[i].dir = (i == 0) ? PDIR_LEFT : PDIR_RIGHT;
        ghosts[i].scared = false;
        ghosts[i].eaten  = false;
        ghosts[i].eatenTimer = 0;
    }
    anyScared = false;
    scaredTimer = 0;
}

// ─── Spielfeld-Reset ──────────────────────────────────────────────────────────
static void resetGame() {
    pacX    = 15;
    pacY    = 11;
    pacDir  = PDIR_RIGHT;
    pacNextDir = PDIR_RIGHT;
    pacScore = 0;
    newHighscore = false;
    resetDots();
    resetGhosts();
    pacState = PAC_PLAYING;
}

// ─── Hilfsfunktion: ist Zelle begehbar? ───────────────────────────────────────
static bool canMove(int x, int y) {
    return !isWall(x, y);
}

// ─── Power-Pellet an Position? ───────────────────────────────────────────────
static int isPowerPellet(int x, int y) {
    for (int i = 0; i < 4; i++)
        if (powerPellets[i][0] == x && powerPellets[i][1] == y) return i;
    return -1;
}

// ─── Ghost-KI ─────────────────────────────────────────────────────────────────
static void moveGhost(Ghost &g) {
    if (g.eaten) return;

    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = { 0, 0,-1, 1};
    PacDir opp[4] = {PDIR_RIGHT, PDIR_LEFT, PDIR_DOWN, PDIR_UP};

    // Mögliche Richtungen sammeln (keine Wand, nicht rückwärts)
    PacDir choices[4];
    int    nChoices = 0;
    for (int d = 0; d < 4; d++) {
        if (d == (int)opp[g.dir]) continue;
        int nx = g.x + dx[d];
        int ny = g.y + dy[d];
        if (canMove(nx, ny)) choices[nChoices++] = (PacDir)d;
    }
    if (nChoices == 0) {
        // Sackgasse → umdrehen
        int d = (int)opp[g.dir];
        g.x += dx[d]; g.y += dy[d];
        g.dir = (PacDir)d;
        return;
    }

    PacDir chosen = choices[0];
    if (!g.scared) {
        // 50%: Richtung wählen die Pacman nähert
        int bestDist = 9999;
        PacDir bestDir = choices[0];
        for (int i = 0; i < nChoices; i++) {
            int nx = g.x + dx[(int)choices[i]];
            int ny = g.y + dy[(int)choices[i]];
            int dist = abs(nx - pacX) + abs(ny - pacY);
            if (dist < bestDist) { bestDist = dist; bestDir = choices[i]; }
        }
        chosen = (random(100) < 50) ? bestDir : choices[random(nChoices)];
    } else {
        // Scared: von Pacman wegbewegen
        int worstDist = -1;
        PacDir worstDir = choices[0];
        for (int i = 0; i < nChoices; i++) {
            int nx = g.x + dx[(int)choices[i]];
            int ny = g.y + dy[(int)choices[i]];
            int dist = abs(nx - pacX) + abs(ny - pacY);
            if (dist > worstDist) { worstDist = dist; worstDir = choices[i]; }
        }
        chosen = (random(100) < 60) ? worstDir : choices[random(nChoices)];
    }

    g.x += dx[(int)chosen];
    g.y += dy[(int)chosen];
    g.dir = chosen;
}

// ─── Spiellogik-Schritt ───────────────────────────────────────────────────────
static void pacUpdate() {
    if (pacState != PAC_PLAYING) return;

    // Richtungswunsch prüfen
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = { 0, 0,-1, 1};
    int nx = pacX + dx[(int)pacNextDir];
    int ny = pacY + dy[(int)pacNextDir];
    if (canMove(nx, ny)) pacDir = pacNextDir;

    // Pacman bewegen
    nx = pacX + dx[(int)pacDir];
    ny = pacY + dy[(int)pacDir];
    if (canMove(nx, ny)) { pacX = nx; pacY = ny; }

    // Dot fressen
    if (dots[pacY][pacX]) {
        dots[pacY][pacX] = false;
        dotsEaten++;
        int pp = isPowerPellet(pacX, pacY);
        if (pp >= 0) {
            pacScore += 50;
            for (int i = 0; i < GHOST_COUNT; i++)
                if (!ghosts[i].eaten) ghosts[i].scared = true;
            anyScared   = true;
            scaredTimer = millis();
            // Power-Pellet-Position deaktivieren
            powerPellets[pp][0] = -1;
            powerPellets[pp][1] = -1;
        } else {
            pacScore += 10;
        }
    }

    // Scared-Timeout (5 Sekunden)
    if (anyScared && millis() - scaredTimer > 5000UL) {
        for (int i = 0; i < GHOST_COUNT; i++) ghosts[i].scared = false;
        anyScared = false;
    }

    // Geister bewegen
    for (int i = 0; i < GHOST_COUNT; i++) {
        // Eaten-Respawn nach 2 Sekunden
        if (ghosts[i].eaten && millis() - ghosts[i].eatenTimer > 2000UL) {
            ghosts[i].x = GHOST_HOME_X[i];
            ghosts[i].y = GHOST_HOME_Y;
            ghosts[i].eaten  = false;
            ghosts[i].scared = false;
        }
        if (!ghosts[i].eaten) moveGhost(ghosts[i]);
    }

    // Kollision Pacman – Geist
    for (int i = 0; i < GHOST_COUNT; i++) {
        if (ghosts[i].eaten) continue;
        if (ghosts[i].x == pacX && ghosts[i].y == pacY) {
            if (ghosts[i].scared) {
                // Pacman frisst Geist
                ghosts[i].eaten  = true;
                ghosts[i].scared = false;
                ghosts[i].eatenTimer = millis();
                pacScore += 200;
            } else {
                // Game Over
                newHighscore = (pacScore > pacHighScore);
                if (newHighscore) pacHighScore = pacScore;
                pacState = PAC_GAMEOVER;
                return;
            }
        }
    }

    // Gewonnen?
    if (dotsEaten >= totalDots) {
        newHighscore = (pacScore > pacHighScore);
        if (newHighscore) pacHighScore = pacScore;
        pacState = PAC_WIN;
    }
}

// ─── Öffentliche Schnittstelle ────────────────────────────────────────────────
void pacmanInit() {
    pacPrefs.begin("pacman", false);
    pacHighScore = pacPrefs.getInt("hs", 0);
}

void pacmanResetToMenu() {
    pacState = PAC_MENU;
}

void pacmanHandleInput(JoystickRichtung dir, bool pressed) {
    if (pacState == PAC_MENU) {
        if (pressed) resetGame();
        return;
    }
    if (pacState == PAC_GAMEOVER || pacState == PAC_WIN) {
        if (pressed) pacState = PAC_MENU;
        return;
    }
    if (pacState == PAC_PLAYING) {
        if      (dir == LINKS)  pacNextDir = PDIR_LEFT;
        else if (dir == RECHTS) pacNextDir = PDIR_RIGHT;
        else if (dir == OBEN)   pacNextDir = PDIR_UP;
        else if (dir == UNTEN)  pacNextDir = PDIR_DOWN;
    }
}

// ─── taskPacmanLogic ──────────────────────────────────────────────────────────
void taskPacmanLogic(void *pvParameters) {
    while (1) {
        if (currentApp == APP_PACMAN && pacState == PAC_PLAYING) {
            pacUpdate();
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// ─── taskPacmanDisplay ────────────────────────────────────────────────────────
void taskPacmanDisplay(void *pvParameters) {
    int blinkTick = 0;

    while (1) {
        if (currentApp != APP_PACMAN) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        FastLED.clear();
        blinkTick++;

        if (pacState == PAC_MENU) {
            // Rahmen
            for (int x = 0; x < 32; x++) { setPixel(x,0,CRGB::Yellow); setPixel(x,15,CRGB::Yellow); }
            for (int y = 0; y < 16; y++) { setPixel(0,y,CRGB::Yellow); setPixel(31,y,CRGB::Yellow); }
            // Pacman-Pixel (animiert)
            uint8_t br = (blinkTick % 8 < 4) ? 255 : 150;
            setPixel(6, 5, CHSV(43, 255, br));
            setPixel(7, 5, CHSV(43, 255, br));
            setPixel(6, 6, CHSV(43, 255, br));
            setPixel(7, 6, CHSV(43, 255, br));
            // Geist-Pixel
            setPixel(11, 5, CRGB::Red);
            setPixel(12, 5, CRGB::Red);
            setPixel(11, 6, CRGB::Red);
            setPixel(12, 6, CRGB::Red);
            setPixel(15, 5, CRGB::Cyan);
            setPixel(16, 5, CRGB::Cyan);
            setPixel(15, 6, CRGB::Cyan);
            setPixel(16, 6, CRGB::Cyan);
            // Score / Highscore
            pfDrawChar(1, 9, PF_H, CRGB::Cyan);
            pfDrawColon(5, 9, CRGB::Cyan);
            pfDrawNumberRight(9, pacHighScore, CRGB::Cyan);
            // Blinke-Hinweis: Punkt blinkt unten rechts
            if (blinkTick % 20 < 10) setPixel(16, 14, CRGB::White);
        }

        else if (pacState == PAC_PLAYING) {
            // Wände
            for (int y = 0; y < PM_H; y++)
                for (int x = 0; x < PM_W; x++)
                    if (isWall(x, y)) setPixel(x, y, CRGB(0, 0, 80));

            // Dots
            bool ppBlink = (blinkTick % 8 < 4);
            for (int y = 0; y < PM_H; y++) {
                for (int x = 0; x < PM_W; x++) {
                    if (!dots[y][x]) continue;
                    bool isPP = false;
                    for (int i = 0; i < 4; i++)
                        if (powerPellets[i][0]==x && powerPellets[i][1]==y) { isPP=true; break; }
                    if (isPP)
                        setPixel(x, y, ppBlink ? CRGB::White : CRGB(80,80,80));
                    else
                        setPixel(x, y, CRGB(35, 35, 35));
                }
            }

            // Geister
            for (int i = 0; i < GHOST_COUNT; i++) {
                if (ghosts[i].eaten) continue;
                CRGB gc;
                if (ghosts[i].scared) {
                    gc = (blinkTick % 6 < 3) ? CRGB::Blue : CRGB(80,80,255);
                } else {
                    gc = GHOST_COLORS[i];
                }
                setPixel(ghosts[i].x, ghosts[i].y, gc);
            }

            // Pacman (animiert: gelb / orange wechselt = Mund-Effekt)
            CRGB pc = (blinkTick % 6 < 4) ? CHSV(43,255,255) : CHSV(43,255,140);
            setPixel(pacX, pacY, pc);
        }

        else if (pacState == PAC_GAMEOVER) {
            // Rotes Blitzen (wie Snake)
            for (int f = 0; f < 4; f++) {
                fill_solid(ledsTop,    256, CRGB::Red);
                fill_solid(ledsBottom, 256, CRGB::Red);
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(180));
                FastLED.clear();
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(130));
            }
            if (newHighscore) pacPrefs.putInt("hs", pacHighScore);
            // Score anzeigen
            FastLED.clear();
            for (int x = 0; x < 32; x++) { setPixel(x,0,CRGB::White); setPixel(x,15,CRGB::White); }
            pfDrawChar(1, 2, PF_S, CRGB::Yellow);
            pfDrawColon(5, 2, CRGB::Yellow);
            pfDrawNumberRight(2, pacScore, CRGB::Yellow);
            CRGB hc = newHighscore ? CRGB::Gold : CRGB::Cyan;
            pfDrawChar(1, 9, PF_H, hc);
            pfDrawColon(5, 9, hc);
            pfDrawNumberRight(9, pacHighScore, hc);
            showLeds();
            // Warten bis Taster gedrückt (durch pacmanHandleInput)
            while (pacState == PAC_GAMEOVER) vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        else if (pacState == PAC_WIN) {
            // Grünes Blitzen
            for (int f = 0; f < 4; f++) {
                fill_solid(ledsTop,    256, CRGB::Green);
                fill_solid(ledsBottom, 256, CRGB::Green);
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(180));
                FastLED.clear();
                showLeds();
                vTaskDelay(pdMS_TO_TICKS(130));
            }
            if (newHighscore) pacPrefs.putInt("hs", pacHighScore);
            FastLED.clear();
            for (int x = 0; x < 32; x++) { setPixel(x,0,CRGB::Green); setPixel(x,15,CRGB::Green); }
            pfDrawChar(1, 2, PF_S, CRGB::Yellow);
            pfDrawColon(5, 2, CRGB::Yellow);
            pfDrawNumberRight(2, pacScore, CRGB::Yellow);
            CRGB hc2 = newHighscore ? CRGB::Gold : CRGB::Cyan;
            pfDrawChar(1, 9, PF_H, hc2);
            pfDrawColon(5, 9, hc2);
            pfDrawNumberRight(9, pacHighScore, hc2);
            showLeds();
            while (pacState == PAC_WIN) vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        showLeds();
        vTaskDelay(pdMS_TO_TICKS(33));
    }
}
