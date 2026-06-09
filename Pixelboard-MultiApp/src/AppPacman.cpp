#include <Preferences.h>
#include "AppPacman.h"
#include "PixelFont.h"

// ─── Spielfeld 32×16 ──────────────────────────────────────────────────────────
#define PM_W 32
#define PM_H 16

// Abwechslungsreiches, ASYMMETRISCHES Labyrinth mit durchgehend 1 Feld breiten
// Gängen und OHNE SACKGASSEN (jede Zelle hat >=2 Ausgänge -> viele Schleifen/Wege).
// Per tools/genmaze.py (seed 7) erzeugt + validiert: keine 2×2-Flächen, keine
// Sackgassen, voll verbunden. Geister starten frei auf zentralen Feldern (kein Pen).
static const char MAZE[PM_H][PM_W + 1] = {
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
    "W.........W...............W...WW",
    "W.W.WWWWW.W.WWWWWWWWW.W.W.W.W.WW",
    "W...W...W.W.....W...W.W.W.....WW",
    "WWWWW.W.W.W.WWW.W.W.W.W.WWWWW.WW",
    "W.....W.....W...W...W.W.W...W.WW",
    "W.WWWWW.WWW.W.W.W.W.W.W.W.W.W.WW",
    "W.............W.W.W.....W.....WW",
    "W.WWWWWWWWWWWWW.W.W.WWWWWWW.W.WW",
    "W.W.............W...W.......W.WW",
    "W.W.W.W.W.WWWWW.W.W.W.W.WWW.W.WW",
    "W.......W.........W.W.W...W.W.WW",
    "W.W.WWW.WWWWWWWWW.W.W.W.W.W.W.WW",
    "W.................W.....W.....WW",
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
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
    bool eaten;          // unterwegs als Augen zurück ins Haus
    uint32_t eatenTimer;
    bool forceReverse;   // bei Moduswechsel einmalig umkehren (klassisch)
};

static const int  GHOST_COUNT     = 2;
static const int  GHOST_HOME_X[2] = {13, 17};   // zentrale Startfelder (kein Pen)
static const int  GHOST_HOME_Y    = 7;
static const CRGB GHOST_COLORS[2] = { CRGB::Red, CRGB::Cyan };

static const int PAC_START_X = 15;
static const int PAC_START_Y = 13;

// Dot-Map (true = Dot noch vorhanden)
static bool dots[PM_H][PM_W];
static int8_t powerPellets[4][2]; // [idx][0=x, 1=y], -1 = gefressen
static const int PP_POS[4][2] = {{1,1},{29,1},{1,13},{29,13}};

static int  pacX, pacY;
static PacDir pacDir, pacNextDir;
static volatile int  pacScore  = 0;
static int  pacHighScore       = 0;
static int  totalDots          = 0;
static int  dotsEaten          = 0;
static bool newHighscore       = false;
static int  ghostChain         = 0;   // gefressene Geister je Power-Pellet (Verdopplung)

static Ghost ghosts[GHOST_COUNT];

// ─── Geschwindigkeit & Modus ───────────────────────────────────────────────────
// pacUpdate() wird alle LOGIC_TICK_MS aufgerufen. Pacman zieht jeden Tick; Geister
// ziehen je nach Zustand seltener -> verängstigte Geister sind langsam = fangbar.
#define LOGIC_TICK_MS  135       // Grundtakt (höher = langsameres Gameplay)
#define FRIGHT_MS      7000UL    // Dauer "verängstigt" nach Power-Pellet
#define SCATTER_MS     5000UL    // Streu-Phase (Geister zu den Ecken)
#define CHASE_MS       18000UL   // Jagd-Phase

static uint32_t gameTick    = 0;
static uint32_t scaredTimer = 0;
static bool     anyScared   = false;

enum GhostMode { MODE_SCATTER, MODE_CHASE };
static GhostMode globalMode  = MODE_SCATTER;
static uint32_t  modeStartMs = 0;

static Preferences pacPrefs;

enum PacState { PAC_MENU, PAC_PLAYING, PAC_GAMEOVER, PAC_WIN };
static volatile PacState pacState = PAC_MENU;

// ─── Helfer ─────────────────────────────────────────────────────────────────────
static bool canMove(int x, int y) { return !isWall(x, y); }

static int isPowerPellet(int x, int y) {
    for (int i = 0; i < 4; i++)
        if (powerPellets[i][0] == x && powerPellets[i][1] == y) return i;
    return -1;
}

// ─── Maze-Reset: alle Dots setzen ────────────────────────────────────────────
static void resetDots() {
    totalDots = 0;
    dotsEaten = 0;
    for (int y = 0; y < PM_H; y++)
        for (int x = 0; x < PM_W; x++)
            dots[y][x] = false;

    for (int y = 1; y < PM_H - 1; y++) {
        for (int x = 1; x < PM_W - 1; x++) {
            if (!isWall(x, y)) {
                dots[y][x] = true;
                totalDots++;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        powerPellets[i][0] = PP_POS[i][0];
        powerPellets[i][1] = PP_POS[i][1];
    }
}

static void resetGhosts() {
    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].x = GHOST_HOME_X[i];
        ghosts[i].y = GHOST_HOME_Y;
        ghosts[i].dir = (i == 0) ? PDIR_LEFT : PDIR_RIGHT;
        ghosts[i].scared = false;
        ghosts[i].eaten  = false;
        ghosts[i].eatenTimer  = 0;
        ghosts[i].forceReverse = false;
    }
    anyScared   = false;
    scaredTimer = 0;
    globalMode  = MODE_SCATTER;
    modeStartMs = millis();
}

static void resetGame() {
    pacX = PAC_START_X;
    pacY = PAC_START_Y;
    pacDir     = PDIR_LEFT;
    pacNextDir = PDIR_LEFT;
    pacScore   = 0;
    newHighscore = false;
    ghostChain = 0;
    gameTick   = 0;
    resetDots();
    resetGhosts();
    pacState = PAC_PLAYING;
}

// ─── Geist-Ziel (klassische Persönlichkeiten) ─────────────────────────────────
static void ghostTarget(int gi, int &tx, int &ty) {
    Ghost &g = ghosts[gi];
    if (g.eaten) {                       // gefressen -> heim ins Haus
        tx = GHOST_HOME_X[gi]; ty = GHOST_HOME_Y; return;
    }
    if (globalMode == MODE_SCATTER) {    // Streuphase -> eigene Ecke
        if (gi == 0) { tx = PM_W - 2; ty = 1; }   // rot: oben rechts
        else         { tx = 1;        ty = 1; }   // cyan: oben links
        return;
    }
    // Jagdphase
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = { 0, 0,-1, 1};
    if (gi == 0) {                       // "Blinky": direkt auf Pacman
        tx = pacX; ty = pacY;
    } else {                             // "Pinky": 4 Felder vor Pacman (Hinterhalt)
        tx = pacX + dx[(int)pacDir] * 4;
        ty = pacY + dy[(int)pacDir] * 4;
    }
}

// ─── Modus Scatter/Chase ────────────────────────────────────────────────────────
static void updateGhostMode() {
    uint32_t now = millis();
    uint32_t dur = (globalMode == MODE_SCATTER) ? SCATTER_MS : CHASE_MS;
    if (now - modeStartMs >= dur) {
        globalMode  = (globalMode == MODE_SCATTER) ? MODE_CHASE : MODE_SCATTER;
        modeStartMs = now;
        // klassisch: bei Moduswechsel kehren normale Geister um
        for (int i = 0; i < GHOST_COUNT; i++)
            if (!ghosts[i].eaten && !ghosts[i].scared) ghosts[i].forceReverse = true;
    }
}

// ─── Geist einen Schritt bewegen ─────────────────────────────────────────────
static void stepGhost(int gi) {
    Ghost &g = ghosts[gi];
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = { 0, 0,-1, 1};
    int opp[4] = {PDIR_RIGHT, PDIR_LEFT, PDIR_DOWN, PDIR_UP};

    // 1) Verängstigt: zufällige gültige Richtung (kein Rückwärts)
    if (g.scared) {
        PacDir choices[4]; int n = 0;
        for (int d = 0; d < 4; d++) {
            if (d == opp[g.dir]) continue;
            int nx = g.x + dx[d], ny = g.y + dy[d];
            if (!canMove(nx, ny)) continue;
            choices[n++] = (PacDir)d;
        }
        if (n == 0) {
            int d = opp[g.dir];
            if (canMove(g.x + dx[d], g.y + dy[d])) { g.x += dx[d]; g.y += dy[d]; g.dir = (PacDir)d; }
            return;
        }
        PacDir c = choices[random(n)];
        g.x += dx[(int)c]; g.y += dy[(int)c]; g.dir = c;
        return;
    }

    // 2) Normal/Augen: Greedy zum Ziel; kein Rückwärts; Tie-Break UP,LEFT,DOWN,RIGHT
    int tx, ty; ghostTarget(gi, tx, ty);
    int order[4] = {PDIR_UP, PDIR_LEFT, PDIR_DOWN, PDIR_RIGHT};
    int  bestD = -1;
    long bestDist = 0x7fffffffL;
    for (int k = 0; k < 4; k++) {
        int d = order[k];
        if (!g.forceReverse && d == opp[g.dir]) continue;
        int nx = g.x + dx[d], ny = g.y + dy[d];
        if (!canMove(nx, ny)) continue;
        long dist = (long)(nx - tx) * (nx - tx) + (long)(ny - ty) * (ny - ty);
        if (dist < bestDist) { bestDist = dist; bestD = d; }
    }
    if (bestD < 0) {  // Sackgasse -> umkehren
        int d = opp[g.dir];
        if (canMove(g.x + dx[d], g.y + dy[d])) bestD = d;
    }
    g.forceReverse = false;
    if (bestD >= 0) { g.x += dx[bestD]; g.y += dy[bestD]; g.dir = (PacDir)bestD; }
}

// ─── Geschwindigkeit: zieht dieser Geist diesen Tick? ─────────────────────────
static bool ghostMovesThisTick(int gi) {
    if (ghosts[gi].eaten)  return true;              // Augen: schnell heim
    if (ghosts[gi].scared) return (gameTick % 2) == 0;  // 50 % -> langsam, fangbar
    return (gameTick % 4) != 0;                       // ~75 % -> etwas langsamer als Pacman
}

// ─── Pacman bewegen + Dots fressen ────────────────────────────────────────────
static void movePacman() {
    int dx[4] = {-1, 1, 0, 0};
    int dy[4] = { 0, 0,-1, 1};

    int nx = pacX + dx[(int)pacNextDir];
    int ny = pacY + dy[(int)pacNextDir];
    if (canMove(nx, ny)) pacDir = pacNextDir;

    nx = pacX + dx[(int)pacDir];
    ny = pacY + dy[(int)pacDir];
    if (canMove(nx, ny)) { pacX = nx; pacY = ny; }

    if (dots[pacY][pacX]) {
        dots[pacY][pacX] = false;
        dotsEaten++;
        int pp = isPowerPellet(pacX, pacY);
        if (pp >= 0) {
            pacScore += 50;
            ghostChain = 0;            // neue Verdopplungs-Kette
            for (int i = 0; i < GHOST_COUNT; i++)
                if (!ghosts[i].eaten) { ghosts[i].scared = true; ghosts[i].forceReverse = true; }
            anyScared   = true;
            scaredTimer = millis();
            powerPellets[pp][0] = -1;
            powerPellets[pp][1] = -1;
        } else {
            pacScore += 10;
        }
    }
}

// ─── Kollision Pacman ↔ Geist ──────────────────────────────────────────────────
static bool handleCollisions() {  // true = Game Over
    for (int i = 0; i < GHOST_COUNT; i++) {
        if (ghosts[i].eaten) continue;
        if (ghosts[i].x == pacX && ghosts[i].y == pacY) {
            if (ghosts[i].scared) {
                ghosts[i].eaten  = true;
                ghosts[i].scared = false;
                ghosts[i].eatenTimer = millis();
                // klassische Verdopplung: 200, 400, 800, 1600 ...
                ghostChain++;
                pacScore += 200 * (1 << min(ghostChain - 1, 3));
            } else {
                newHighscore = (pacScore > pacHighScore);
                if (newHighscore) pacHighScore = pacScore;
                pacState = PAC_GAMEOVER;
                return true;
            }
        }
    }
    return false;
}

// ─── Spiellogik-Schritt ───────────────────────────────────────────────────────
static void pacUpdate() {
    if (pacState != PAC_PLAYING) return;
    gameTick++;

    updateGhostMode();

    if (anyScared && millis() - scaredTimer > FRIGHT_MS) {
        for (int i = 0; i < GHOST_COUNT; i++) ghosts[i].scared = false;
        anyScared = false;
    }

    // Pacman zieht jeden Tick
    movePacman();
    if (handleCollisions()) return;

    if (dotsEaten >= totalDots) {
        newHighscore = (pacScore > pacHighScore);
        if (newHighscore) pacHighScore = pacScore;
        pacState = PAC_WIN;
        return;
    }

    // Geister ziehen je nach Geschwindigkeit
    for (int i = 0; i < GHOST_COUNT; i++) {
        if (ghosts[i].eaten) {
            bool home = (ghosts[i].x == GHOST_HOME_X[i] && ghosts[i].y == GHOST_HOME_Y);
            if (home || millis() - ghosts[i].eatenTimer > 5000UL) {
                ghosts[i].x = GHOST_HOME_X[i];
                ghosts[i].y = GHOST_HOME_Y;
                ghosts[i].eaten  = false;
                ghosts[i].scared = false;
                ghosts[i].dir = (i == 0) ? PDIR_RIGHT : PDIR_LEFT;
                continue;
            }
        }
        if (ghostMovesThisTick(i)) stepGhost(i);
    }
    handleCollisions();
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
        vTaskDelay(pdMS_TO_TICKS(LOGIC_TICK_MS));
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
            bool frightEnding = anyScared && (millis() - scaredTimer > FRIGHT_MS - 2000UL);
            for (int i = 0; i < GHOST_COUNT; i++) {
                if (ghosts[i].eaten) {
                    setPixel(ghosts[i].x, ghosts[i].y, CRGB(40, 40, 70));  // Augen
                    continue;
                }
                CRGB gc;
                if (ghosts[i].scared) {
                    if (frightEnding) gc = (blinkTick % 4 < 2) ? CRGB::White : CRGB::Blue;
                    else              gc = (blinkTick % 6 < 3) ? CRGB::Blue  : CRGB(80,80,255);
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
