#include "Joystick.h"

// Konstruktor
Joystick::Joystick(int pinX, int pinY, int pinButton, int deadzoneWert, int centerWert)
    : EntprellterTaster(pinButton),
      pinX(pinX), 
      pinY(pinY), 
      deadzone(deadzoneWert), 
      center(centerWert),
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
    
    if (wert < (center - deadzone)) {
        return -1;  // Links
    } else if (wert > (center + deadzone)) {
        return 1;   // Rechts
    } else {
        return 0;   // Neutral
    }
}

int Joystick::getYRichtung() {
    int wert = analogRead(pinY);
    
    if (wert < (center - deadzone)) {
        return -1;  // Oben
    } else if (wert > (center + deadzone)) {
        return 1;   // Unten
    } else {
        return 0;   // Neutral
    }
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
