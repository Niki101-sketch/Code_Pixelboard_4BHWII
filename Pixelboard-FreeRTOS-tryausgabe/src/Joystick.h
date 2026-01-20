#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>
#include "EntprellterTaster.h"

// Joystick Richtungen als Enum für bessere Lesbarkeit
enum JoystickRichtung {
    NEUTRAL = 0,
    LINKS = 1,
    RECHTS = 2,
    OBEN = 3,
    UNTEN = 4
};

class Joystick : public EntprellterTaster {
public:
    // Konstruktor
    Joystick(int pinX, int pinY, int pinButton, 
             int deadzoneWert = 1000, int centerWert = 2048);

    // Methode zum Aktualisieren - MUSS regelmäßig aufgerufen werden!
    void aktualisiere();

    // Richtungs-Abfragen (gibt -1, 0, oder 1 zurück)
    int getXRichtung();  // -1 = Links, 0 = Neutral, 1 = Rechts
    int getYRichtung();  // -1 = Oben, 0 = Neutral, 1 = Unten

    // Einfache Richtungs-Checks (gibt true/false zurück)
    bool istLinks();
    bool istRechts();
    bool istOben();
    bool istUnten();
    bool istNeutral();

    // Richtung als Enum (für Switch-Case verwendbar)
    JoystickRichtung getRichtung();

    // Erweiterte Funktionen: Neue Bewegung erkennen
    // (Gibt nur einmal TRUE zurück, wenn Richtung neu bewegt wird)
    bool neueRichtungLinks();
    bool neueRichtungRechts();
    bool neueRichtungOben();
    bool neueRichtungUnten();

    // Von EntprellterTaster geerbt:
    // - istGedrueckt()          - Ist der Button gerade gedrückt?
    // - wurdeGedrueckt()        - Wurde kurz gedrückt? (einmalig)
    // - wurdeLangeGedrueckt()   - Wurde lang gedrückt? (einmalig)

private:
    const int pinX;
    const int pinY;
    const int deadzone;
    const int center;

    // Für "neue Richtung" Erkennung
    int letzteXRichtung;
    int letzteYRichtung;
};

#endif
