#pragma once
#include "Shared.h"
#include "Joystick.h"

void pongInit();
void pongResetToMenu();
void pongHandleInput(JoystickRichtung dir1, JoystickRichtung dir2, bool pressed);
void taskPongDisplay(void *pvParameters);
