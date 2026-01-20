/**
 * @file Snake_DualPanel_64x16.cpp
 * @brief Snake-Spiel für zwei 8x32 Panels (gesamt 64x16)
 * 
 * Hardware:
 *   - Panel Pin 26 = physisch oben
 *   - Panel Pin 25 = physisch unten
 *   - Joystick für Steuerung
 * 
 * Spielregeln:
 *   - Steuere die Schlange mit dem Joystick
 *   - Sammle rote Nahrung um zu wachsen
 *   - Vermeide Wände und deinen eigenen Körper
 *   - Joystick-Button zum Neustart nach Game Over
 */

#include <FastLED.h>
#include <LEDMatrix.h>

// --- Joystick Konfiguration --------------------------------------------------
#define JOY_CENTER   2048
#define JOY_DEADZONE 500

class Taster {
  private:
    int pin;
    bool lastState;
    bool pressed;
    
  public:
    Taster(int p) {
      pin = p;
      pinMode(pin, INPUT_PULLUP);
      lastState = HIGH;
      pressed = false;
    }
    
    bool wurdGedrueckt() {
      bool current = digitalRead(pin);
      bool result = false;
      
      if (current == LOW && lastState == HIGH) {
        pressed = true;
        result = true;
      }
      lastState = current;
      return result;
    }
};

class Joystick {
  private:
    int pinX, pinY;
    Taster* button;
    
  public:
    Joystick(int x, int y, int sw) {
      pinX = x;
      pinY = y;
      pinMode(pinX, INPUT);
      pinMode(pinY, INPUT);
      button = new Taster(sw);
    }
    
    int getXDir() {
      int val = analogRead(pinX);
      if (val < (JOY_CENTER - JOY_DEADZONE)) return -1;
      if (val > (JOY_CENTER + JOY_DEADZONE)) return 1;
      return 0;
    }
    
    int getYDir() {
      int val = analogRead(pinY);
      if (val < (JOY_CENTER - JOY_DEADZONE)) return -1;
      if (val > (JOY_CENTER + JOY_DEADZONE)) return 1;
      return 0;
    }
    
    bool buttonPressed() {
      return button->wurdGedrueckt();
    }
};

// --- Hardware-Konfiguration --------------------------------------------------
#define pinTop         25
#define pinBottom      26
#define panelWidth     32
#define panelHeight     8
#define ledsPerPanel   (panelWidth * panelHeight)
#define brightness     25
#define colorOrder     GRB
#define chipset        WS2812
// --- Spielfeld ---------------------------------------------------------------
#define FIELD_WIDTH    64
#define FIELD_HEIGHT   16
#define MAX_SNAKE_LEN  (FIELD_WIDTH * FIELD_HEIGHT)

CRGB canvasLeds[FIELD_WIDTH * FIELD_HEIGHT];
cLEDMatrix<FIELD_WIDTH, FIELD_HEIGHT, HORIZONTAL_MATRIX> canvas;

CRGB ledsTop[ledsPerPanel];
CRGB ledsBottom[ledsPerPanel];

cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelTop;
cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> panelBottom;

// --- Joystick ----------------------------------------------------------------
#define JOY_PIN_X  34  // Analog Pin
#define JOY_PIN_Y  35  // Analog Pin
#define JOY_PIN_SW 32  // Button Pin
Joystick* myJoystick;

// --- Snake Struktur ----------------------------------------------------------
struct Point {
  int x, y;
};

Point snake[MAX_SNAKE_LEN];
int snakeLength = 3;
Point direction = {1, 0};  // Start: nach rechts
Point food = {10, 8};
bool gameOver = false;
int score = 0;

uint32_t lastMoveTime = 0;
uint16_t moveInterval = 1000;  // Geschwindigkeit in ms

// --- Farben ------------------------------------------------------------------
CRGB COLOR_SNAKE_HEAD = CRGB(0, 255, 0);    // Grün
CRGB COLOR_SNAKE_BODY = CRGB(0, 150, 0);    // Dunkelgrün
CRGB COLOR_FOOD = CRGB(255, 0, 0);          // Rot
CRGB COLOR_GAMEOVER = CRGB(255, 0, 0);      // Rot

// --- Prototypen --------------------------------------------------------------
void initHardware();
void initGame();
void updateGame();
void handleInput();
void moveSnake();
void checkCollisions();
void spawnFood();
void renderGame();
void blitToPhysicalPanels();
void showGameOver();
void mirrorPanelHorizontal(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);
void rotatePanel180(cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel);

// --- Setup -------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  
  initHardware();
  initGame();
  
  Serial.println(F("Snake-Spiel gestartet!"));
  Serial.println(F("Steuerung: Joystick"));
  Serial.println(F("Neustart: Joystick-Button"));
}

// --- Loop --------------------------------------------------------------------
void loop() {
  if (gameOver) {
    showGameOver();
    
    // Button zum Neustart
    if (myJoystick->buttonPressed()) {
      initGame();
    }
    delay(100);
    return;
  }
  
  handleInput();
  
  uint32_t now = millis();
  if (now - lastMoveTime >= moveInterval) {
    lastMoveTime = now;
    moveSnake();
    checkCollisions();
  }
  
  renderGame();
  blitToPhysicalPanels();
  FastLED.show();
  
  delay(10);
}

// --- Implementierung ---------------------------------------------------------

void initHardware() {
  // FastLED Setup
  FastLED.addLeds<chipset, pinTop, colorOrder>(ledsTop, ledsPerPanel);
  FastLED.addLeds<chipset, pinBottom, colorOrder>(ledsBottom, ledsPerPanel);
  FastLED.setBrightness(brightness);
  FastLED.clear(true);
  
  // Panel Mapping (siehe Original-Code Kommentare)
  panelTop.SetLEDArray(ledsBottom);     // Pin 26, physisch oben
  panelBottom.SetLEDArray(ledsTop);     // Pin 25, physisch unten
  
  canvas.SetLEDArray(canvasLeds);
  
  // Joystick initialisieren
  myJoystick = new Joystick(JOY_PIN_X, JOY_PIN_Y, JOY_PIN_SW);
  
  // Startsequenz: kurz grün blinken
  fill_solid(ledsBottom, ledsPerPanel, CRGB::Green);
  fill_solid(ledsTop, ledsPerPanel, CRGB::Green);
  FastLED.show();
  delay(300);
  FastLED.clear(true);
}

void initGame() {
  gameOver = false;
  snakeLength = 10;
  score = 0;
  
  // Snake in der Mitte starten
  snake[0] = {FIELD_WIDTH / 2, FIELD_HEIGHT / 2};      // Kopf
  snake[1] = {FIELD_WIDTH / 2 - 1, FIELD_HEIGHT / 2};  // Körper
  snake[2] = {FIELD_WIDTH / 2 - 2, FIELD_HEIGHT / 2};  // Schwanz
  
  direction = {1, 0};  // Nach rechts
  
  spawnFood();
  
  lastMoveTime = millis();
  moveInterval = 150;
  
  Serial.println(F("Neues Spiel gestartet!"));
}

void handleInput() {
  int xDir = myJoystick->getXDir();
  int yDir = myJoystick->getYDir();
  
  // Neue Richtung nur setzen, wenn nicht in Gegenrichtung
  // (verhindert, dass Snake in sich selbst fährt)
  if (xDir != 0) {
    if (!(direction.x == -xDir && direction.y == 0)) {
      direction.x = xDir;
      direction.y = 0;
    }
  } else if (yDir != 0) {
    if (!(direction.x == 0 && direction.y == -yDir)) {
      direction.x = 0;
      direction.y = yDir;
    }
  }
}

void moveSnake() {
  // Neue Kopfposition berechnen
  Point newHead = {
    snake[0].x + direction.x,
    snake[0].y + direction.y
  };
  
  // Körper verschieben (vom Schwanz zum Kopf)
  for (int i = snakeLength - 1; i > 0; i--) {
    snake[i] = snake[i - 1];
  }
  
  snake[0] = newHead;
}

void checkCollisions() {
  Point head = snake[0];
  
  // Wandkollision
  if (head.x < 0 || head.x >= FIELD_WIDTH || 
      head.y < 0 || head.y >= FIELD_HEIGHT) {
    gameOver = true;
    Serial.println(F("Game Over - Wand getroffen!"));
    return;
  }
  
  // Selbstkollision
  for (int i = 1; i < snakeLength; i++) {
    if (head.x == snake[i].x && head.y == snake[i].y) {
      gameOver = true;
      Serial.println(F("Game Over - Selbstkollision!"));
      return;
    }
  }
  
  // Nahrung gegessen?
  if (head.x == food.x && head.y == food.y) {
    score++;
    snakeLength++;
    
    // Geschwindigkeit erhöhen
    if (moveInterval > 50) {
      moveInterval -= 5;
    }
    
    spawnFood();
    
    Serial.print(F("Score: "));
    Serial.print(score);
    Serial.print(F(" | Länge: "));
    Serial.println(snakeLength);
  }
}

void spawnFood() {
  bool validPosition = false;
  
  while (!validPosition) {
    food.x = random(FIELD_WIDTH);
    food.y = random(FIELD_HEIGHT);
    
    // Prüfen, ob Position frei ist
    validPosition = true;
    for (int i = 0; i < snakeLength; i++) {
      if (food.x == snake[i].x && food.y == snake[i].y) {
        validPosition = false;
        break;
      }
    }
  }
}

void renderGame() {
  // Canvas löschen
  fill_solid(canvasLeds, FIELD_WIDTH * FIELD_HEIGHT, CRGB::Black);
  
  // Snake zeichnen
  for (int i = 0; i < snakeLength; i++) {
    CRGB color = (i == 0) ? COLOR_SNAKE_HEAD : COLOR_SNAKE_BODY;
    canvas(snake[i].x, snake[i].y) = color;
  }
  
  // Nahrung zeichnen (blinkend)
  if ((millis() / 200) % 2 == 0) {
    canvas(food.x, food.y) = COLOR_FOOD;
  }
}

void blitToPhysicalPanels() {
  // Oberes Panel (Pin 26) - untere Hälfte des Canvas (y=8..15)
  for (uint8_t y = 0; y < panelHeight; y++) {
    uint8_t ySrc = y + panelHeight;
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelTop(x, y) = canvas(x, ySrc);
    }
  }
  
  // Unteres Panel (Pin 25) - obere Hälfte des Canvas (y=0..7)
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth; x++) {
      panelBottom(x, y) = canvas(x, y);
    }
  }
  
  // Orientierungskorrekturen
  mirrorPanelHorizontal(panelTop);
  mirrorPanelHorizontal(panelBottom);
  rotatePanel180(panelTop);
}

void showGameOver() {
  static uint32_t lastBlink = 0;
  static bool blinkState = false;
  
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    blinkState = !blinkState;
    
    if (blinkState) {
      // Ganzes Display rot
      fill_solid(canvasLeds, FIELD_WIDTH * FIELD_HEIGHT, COLOR_GAMEOVER);
    } else {
      // Snake und Score anzeigen
      renderGame();
    }
    
    blitToPhysicalPanels();
    FastLED.show();
  }
}

void mirrorPanelHorizontal(
  cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel
) {
  for (uint8_t y = 0; y < panelHeight; y++) {
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      uint8_t xo = panelWidth - 1 - x;
      CRGB tmp = panel(x, y);
      panel(x, y) = panel(xo, y);
      panel(xo, y) = tmp;
    }
  }
}

void rotatePanel180(
  cLEDMatrix<panelWidth, panelHeight, VERTICAL_ZIGZAG_MATRIX> &panel
) {
  for (uint8_t y = 0; y < panelHeight / 2; y++) {
    for (uint8_t x = 0; x < panelWidth; x++) {
      uint8_t xo = panelWidth - 1 - x;
      uint8_t yo = panelHeight - 1 - y;
      
      CRGB tmp = panel(x, y);
      panel(x, y) = panel(xo, yo);
      panel(xo, yo) = tmp;
    }
  }
  
  // Mittlere Zeile bei ungerader Höhe
  if (panelHeight % 2 == 1) {
    uint8_t midY = panelHeight / 2;
    for (uint8_t x = 0; x < panelWidth / 2; x++) {
      uint8_t xo = panelWidth - 1 - x;
      CRGB tmp = panel(x, midY);
      panel(x, midY) = panel(xo, midY);
      panel(xo, midY) = tmp;
    }
  }
}