#pragma once
#include "Shared.h"
#include "Joystick.h"

void snakeInit();
void snakeHandleInput(JoystickRichtung dir, bool pressed);
void snakeResetToMenu();
void taskSnakeLogic(void *pvParameters);
void taskSnakeDisplay(void *pvParameters);
