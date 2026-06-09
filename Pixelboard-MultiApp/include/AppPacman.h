#pragma once
#include "Shared.h"
#include "Joystick.h"

void pacmanInit();
void pacmanResetToMenu();
void pacmanHandleInput(JoystickRichtung dir, bool pressed);
void taskPacmanDisplay(void *pvParameters);
void taskPacmanLogic(void *pvParameters);
