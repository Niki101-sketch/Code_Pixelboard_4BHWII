// JOYSTICK TEST PROGRAMM
// Umbenennen zu main.cpp um zu testen!
/*
#include <Arduino.h>
#include "Joystick.h"

// Joystick Pins (ESP32 HTL Pixelboard)
#define JOY_PIN_X  34   // Analog X
#define JOY_PIN_Y  35   // Analog Y
#define JOY_PIN_SW 32   // Button

// Joystick Objekt
Joystick joystick(JOY_PIN_X, JOY_PIN_Y, JOY_PIN_SW);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== JOYSTICK TEST ===");
  Serial.println("Bewege den Joystick und drücke den Button!");
  Serial.println("=====================\n");
}

void loop() {
  // Joystick aktualisieren (WICHTIG!)
  joystick.aktualisiere();
  
  // === RICHTUNGEN TESTEN ===
  if (joystick.istLinks()) {
    Serial.println("<-- LINKS");
  }
  if (joystick.istRechts()) {
    Serial.println("--> RECHTS");
  }
  if (joystick.istOben()) {
    Serial.println("^^^ OBEN");
  }
  if (joystick.istUnten()) {
    Serial.println("vvv UNTEN");
  }
  
  // === NEUE RICHTUNGEN (nur einmal) ===
  if (joystick.neueRichtungLinks()) {
    Serial.println(">>> Neu nach LINKS bewegt!");
  }
  if (joystick.neueRichtungRechts()) {
    Serial.println(">>> Neu nach RECHTS bewegt!");
  }
  if (joystick.neueRichtungOben()) {
    Serial.println(">>> Neu nach OBEN bewegt!");
  }
  if (joystick.neueRichtungUnten()) {
    Serial.println(">>> Neu nach UNTEN bewegt!");
  }
  
  // === BUTTON TESTEN ===
  if (joystick.wurdeGedrueckt()) {
    Serial.println("### BUTTON: Kurzer Druck! ###");
  }
  if (joystick.wurdeLangeGedrueckt()) {
    Serial.println("### BUTTON: LANGER DRUCK! ###");
  }
  
  delay(50);  // Kleine Pause
}
*/