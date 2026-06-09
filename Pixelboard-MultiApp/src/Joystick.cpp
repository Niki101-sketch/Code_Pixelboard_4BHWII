#include "Joystick.h"

// Konstruktor
Joystick::Joystick(int pinX, int pinY, int pinButton, int deadzoneWert, int centerWert,
                   bool invertX, bool invertY)
    : EntprellterTaster(pinButton),
      pinX(pinX),
      pinY(pinY),
      deadzone(deadzoneWert),
      center(centerWert),
      invertX(invertX),
      invertY(invertY),
      letzteXRichtung(0),
      letzteYRichtung(0) {
    
    pinMode(pinX, INPUT);
    pinMode(pinY, INPUT);
}

void Joystick::aktualisiere() {
    EntprellterTaster::aktualisiere();
}

int Joystick::getXRichtung() {
    int wert = analogRead(pinX);
    int richtung;

    if (wert < (center - deadzone)) {
        richtung = -1;  // Links
    } else if (wert > (center + deadzone)) {
        richtung = 1;   // Rechts
    } else {
        richtung = 0;   // Neutral
    }

    return invertX ? -richtung : richtung;
}

int Joystick::getYRichtung() {
    int wert = analogRead(pinY);
    int richtung;

    if (wert < (center - deadzone)) {
        richtung = 1;   // Unten (Y-Achse physisch invertiert)
    } else if (wert > (center + deadzone)) {
        richtung = -1;  // Oben
    } else {
        richtung = 0;   // Neutral
    }

    return invertY ? -richtung : richtung;
}

bool Joystick::istLinks() {
    return getXRichtung() == -1;
}

bool Joystick::istRechts() {
    return getXRichtung() == 1;
}

bool Joystick::istOben() {
    return getYRichtung() == -1;
}

bool Joystick::istUnten() {
    return getYRichtung() == 1;
}

bool Joystick::istNeutral() {
    return (getXRichtung() == 0 && getYRichtung() == 0);
}

JoystickRichtung Joystick::getRichtung() {
    int x = getXRichtung();
    int y = getYRichtung();
    
    if (x == -1) return LINKS;
    if (x == 1)  return RECHTS;
    if (y == -1) return OBEN;
    if (y == 1)  return UNTEN;
    
    return NEUTRAL;
}

bool Joystick::neueRichtungLinks() {
    int aktuelleRichtung = getXRichtung();
    
    if (aktuelleRichtung == -1 && letzteXRichtung != -1) {
        letzteXRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteXRichtung = aktuelleRichtung;
    return false;
}

bool Joystick::neueRichtungRechts() {
    int aktuelleRichtung = getXRichtung();
    
    if (aktuelleRichtung == 1 && letzteXRichtung != 1) {
        letzteXRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteXRichtung = aktuelleRichtung;
    return false;
}

bool Joystick::neueRichtungOben() {
    int aktuelleRichtung = getYRichtung();
    
    if (aktuelleRichtung == -1 && letzteYRichtung != -1) {
        letzteYRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteYRichtung = aktuelleRichtung;
    return false;
}

bool Joystick::neueRichtungUnten() {
    int aktuelleRichtung = getYRichtung();
    
    if (aktuelleRichtung == 1 && letzteYRichtung != 1) {
        letzteYRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteYRichtung = aktuelleRichtung;
    return false;
}
