#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <Arduino.h>
#include <FastLED.h>

#define MAX_SNAKE_LENGTH 512 // 32 * 16 Pixel

struct Point {
    int x;
    int y;
};

enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

class SnakeGame {
public:
    SnakeGame(int width, int height);
    void reset();
    bool update(); // Rückgabe: false wenn Game Over
    void setDirection(Direction newDir);
    Point getHead() { return body[0]; }
    Point getFood() { return food; }
    int getLength() { return length; }
    Point* getBody() { return body; }

private:
    int boardWidth;
    int boardHeight;
    Point body[MAX_SNAKE_LENGTH];
    int length;
    Direction currentDir;
    Point food;

    void spawnFood();
    bool isPointOnSnake(Point p);
};

#endif