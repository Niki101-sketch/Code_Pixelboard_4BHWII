#include "SnakeGame.h"

SnakeGame::SnakeGame(int width, int height) : boardWidth(width), boardHeight(height) {
    reset();
}

void SnakeGame::reset() {
    length = 3;
    currentDir = DIR_RIGHT;
    // Startposition in der Mitte
    for (int i = 0; i < length; i++) {
        body[i] = { boardWidth / 2 - i, boardHeight / 2 };
    }
    spawnFood();
}

void SnakeGame::setDirection(Direction newDir) {
    // Verhindern, dass die Schlange direkt in die entgegengesetzte Richtung umkehrt
    if ((newDir == DIR_UP && currentDir != DIR_DOWN) ||
        (newDir == DIR_DOWN && currentDir != DIR_UP) ||
        (newDir == DIR_LEFT && currentDir != DIR_RIGHT) ||
        (newDir == DIR_RIGHT && currentDir != DIR_LEFT)) {
        currentDir = newDir;
    }
}

void SnakeGame::spawnFood() {
    do {
        food.x = random(0, boardWidth);
        food.y = random(0, boardHeight);
    } while (isPointOnSnake(food)); // Futter darf nicht auf der Schlange erscheinen
}

bool SnakeGame::isPointOnSnake(Point p) {
    for (int i = 0; i < length; i++) {
        if (body[i].x == p.x && body[i].y == p.y) return true;
    }
    return false;
}

bool SnakeGame::update() {
    // 1. Schwanzsegmente nachrücken lassen
    for (int i = length - 1; i > 0; i--) {
        body[i] = body[i - 1];
    }

    // 2. Kopf in aktuelle Richtung bewegen
    if (currentDir == DIR_UP) body[0].y--;
    else if (currentDir == DIR_DOWN) body[0].y++;
    else if (currentDir == DIR_LEFT) body[0].x--;
    else if (currentDir == DIR_RIGHT) body[0].x++;

    // 3. Kollision mit Wand prüfen
    if (body[0].x < 0 || body[0].x >= boardWidth || body[0].y < 0 || body[0].y >= boardHeight) {
        return false; 
    }

    // 4. Kollision mit eigenem Körper prüfen
    for (int i = 1; i < length; i++) {
        if (body[0].x == body[i].x && body[0].y == body[i].y) return false;
    }

    // 5. Futter fressen prüfen
    if (body[0].x == food.x && body[0].y == food.y) {
        if (length < MAX_SNAKE_LENGTH) {
            length++;
            // Das neue Ende wird im nächsten Frame korrekt gesetzt
        }
        spawnFood();
    }

    return true;
}