#include "SnakeGame.h"

SnakeGame::SnakeGame(int width, int height) : boardWidth(width), boardHeight(height) {
    // Initialisierung der Variablen
    activeFoodCount = 1; 
    reset(1);
}

void SnakeGame::reset(int foodCount) {
    activeFoodCount = foodCount;
    length = 3;
    currentDir = DIR_RIGHT;

    // Startposition: Mittig, aber so, dass wir nicht im Rand starten
    int startX = boardWidth / 2;
    int startY = boardHeight / 2;

    for (int i = 0; i < length; i++) {
        body[i] = { startX - i, startY };
    }

    // Alle Futter-Pixel initialisieren
    for (int i = 0; i < activeFoodCount; i++) {
        spawnFood(i);
    }
}

void SnakeGame::setDirection(Direction newDir) {
    // Verhindert 180-Grad-Wenden (Selbstmord)
    if ((newDir == DIR_UP && currentDir != DIR_DOWN) ||
        (newDir == DIR_DOWN && currentDir != DIR_UP) ||
        (newDir == DIR_LEFT && currentDir != DIR_RIGHT) ||
        (newDir == DIR_RIGHT && currentDir != DIR_LEFT)) {
        currentDir = newDir;
    }
}

void SnakeGame::spawnFood(int index) {
    bool valid;
    do {
        valid = true;
        // Futter nur INNERHALB des weißen Randes spawnen (1 bis Breite-2)
        foodItems[index].x = random(1, boardWidth - 1);
        foodItems[index].y = random(1, boardHeight - 1);

        // Prüfen, ob das Futter auf der Schlange liegt
        if (isPointOnSnake(foodItems[index])) valid = false;
        
        // Prüfen, ob das Futter auf einem anderen Futter-Pixel liegt
        for (int i = 0; i < activeFoodCount; i++) {
            if (i != index && foodItems[i].x == foodItems[index].x && foodItems[i].y == foodItems[index].y) {
                valid = false;
            }
        }
    } while (!valid);
}

bool SnakeGame::isPointOnSnake(Point p) {
    for (int i = 0; i < length; i++) {
        if (body[i].x == p.x && body[i].y == p.y) return true;
    }
    return false;
}

bool SnakeGame::update() {
    // 1. Körpersegmente nachziehen (von hinten nach vorne)
    for (int i = length - 1; i > 0; i--) {
        body[i] = body[i - 1];
    }

    // 2. Kopf bewegen
    if (currentDir == DIR_UP) body[0].y--;
    else if (currentDir == DIR_DOWN) body[0].y++;
    else if (currentDir == DIR_LEFT) body[0].x--;
    else if (currentDir == DIR_RIGHT) body[0].x++;

    // 3. Kollision mit dem WEISSEN RAND prüfen
    // Da der Rand bei 0 und Max liegt, stirbt die Schlange dort
    if (body[0].x <= 0 || body[0].x >= boardWidth - 1 || 
        body[0].y <= 0 || body[0].y >= boardHeight - 1) {
        return false; 
    }

    // 4. Kollision mit eigenem Körper
    for (int i = 1; i < length; i++) {
        if (body[0].x == body[i].x && body[0].y == body[i].y) return false;
    }

    // 5. Check: Hat der Kopf IRGENDEIN Futter gefressen?
    for (int i = 0; i < activeFoodCount; i++) {
        if (body[0].x == foodItems[i].x && body[0].y == foodItems[i].y) {
            if (length < MAX_SNAKE_LENGTH) {
                length++;
            }
            spawnFood(i); // Nur diesen einen gefressenen Punkt neu spawnen
            break; // Nur ein Essen pro Frame möglich
        }
    }

    return true;
}