#ifndef SNAKE_GAME_H
#define SNAKE_GAME_H

#include <Arduino.h>
#include <FastLED.h>

#define MAX_SNAKE_LENGTH 512
#define MAX_FOOD 5 // Maximal 5 Futter-Pixel gleichzeitig

struct Point { int x, y; };
enum Direction { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

class SnakeGame {
public:
    SnakeGame(int width, int height);
    void reset(int foodCount);
    bool update();
    void setDirection(Direction newDir);
    
    Point* getBody() { return body; }
    int getLength() { return length; }
    Point* getFoodArray() { return foodItems; }
    int getCurrentFoodCount() { return activeFoodCount; }

private:
    int boardWidth, boardHeight;
    Point body[MAX_SNAKE_LENGTH];
    int length;
    Direction currentDir;
    Point foodItems[MAX_FOOD];
    int activeFoodCount;

    void spawnFood(int index);
    bool isPointOnSnake(Point p);
};

#endif