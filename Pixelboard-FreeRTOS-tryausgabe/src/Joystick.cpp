#include "Joystick.h"

// Konstruktor: Initialisiert Joystick mit X-, Y- und Button-Pins
Joystick::Joystick(int pinX, int pinY, int pinButton, int deadzoneWert, int centerWert)
    : EntprellterTaster(pinButton), // Button-Funktionalität von Basisklasse
      pinX(pinX), 
      pinY(pinY), 
      deadzone(deadzoneWert), 
      center(centerWert),
      letzteXRichtung(0),
      letzteYRichtung(0) {
    
    // Analoge Pins als Input konfigurieren
    pinMode(pinX, INPUT);
    pinMode(pinY, INPUT);
}

// Aktualisiere sowohl Joystick als auch Button-Zustand
void Joystick::aktualisiere() {
    // Button-Zustand aktualisieren (von Basisklasse)
    EntprellterTaster::aktualisiere();
    
    // Joystick-Positionen werden live beim Abrufen gelesen
    // (kein zusätzlicher State erforderlich)
}

// Gibt X-Richtung zurück: -1 (Links), 0 (Neutral), 1 (Rechts)
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

// Gibt Y-Richtung zurück: -1 (Oben), 0 (Neutral), 1 (Unten)
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

// Einfache Richtungs-Checks
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

// Gibt die primäre Richtung als Enum zurück
// Priorität: X-Achse vor Y-Achse bei diagonalen Bewegungen
JoystickRichtung Joystick::getRichtung() {
    int x = getXRichtung();
    int y = getYRichtung();
    
    // X-Achse hat Priorität
    if (x == -1) return LINKS;
    if (x == 1)  return RECHTS;
    
    // Dann Y-Achse
    if (y == -1) return OBEN;
    if (y == 1)  return UNTEN;
    
    return NEUTRAL;
}

// Gibt TRUE zurück, wenn die Richtung GERADE NEU nach links bewegt wurde
bool Joystick::neueRichtungLinks() {
    int aktuelleRichtung = getXRichtung();
    
    if (aktuelleRichtung == -1 && letzteXRichtung != -1) {
        letzteXRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteXRichtung = aktuelleRichtung;
    return false;
}

// Gibt TRUE zurück, wenn die Richtung GERADE NEU nach rechts bewegt wurde
bool Joystick::neueRichtungRechts() {
    int aktuelleRichtung = getXRichtung();
    
    if (aktuelleRichtung == 1 && letzteXRichtung != 1) {
        letzteXRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteXRichtung = aktuelleRichtung;
    return false;
}

// Gibt TRUE zurück, wenn die Richtung GERADE NEU nach oben bewegt wurde
bool Joystick::neueRichtungOben() {
    int aktuelleRichtung = getYRichtung();
    
    if (aktuelleRichtung == -1 && letzteYRichtung != -1) {
        letzteYRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteYRichtung = aktuelleRichtung;
    return false;
}

// Gibt TRUE zurück, wenn die Richtung GERADE NEU nach unten bewegt wurde
bool Joystick::neueRichtungUnten() {
    int aktuelleRichtung = getYRichtung();
    
    if (aktuelleRichtung == 1 && letzteYRichtung != 1) {
        letzteYRichtung = aktuelleRichtung;
        return true;
    }
    
    letzteYRichtung = aktuelleRichtung;
    return false;
}
