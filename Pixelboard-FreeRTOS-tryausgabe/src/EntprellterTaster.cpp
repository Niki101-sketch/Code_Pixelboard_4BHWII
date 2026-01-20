#include "EntprellterTaster.h"

// Konstruktor: Initialisierung des Tasters
EntprellterTaster::EntprellterTaster(int pin) : pin(pin), entprellterZustand(false), letzterZustand(false), letzteAenderung(0), 
    gedruecktStartzeit(0), wurdeBereitsGemeldet(false) {
    pinMode(pin, INPUT_PULLUP);
}

// Methode zum Aktualisieren des Zustands des Tasters
void EntprellterTaster::aktualisiere() {
    bool aktuellerZustand = !digitalRead(pin);
    unsigned long jetzt = millis();

    if (aktuellerZustand != letzterZustand) {
        letzteAenderung = jetzt;
    }

    if ((jetzt - letzteAenderung) > ENTPRELLZEIT) {
        if (aktuellerZustand != entprellterZustand) {
            entprellterZustand = aktuellerZustand;

            if (entprellterZustand) {
                gedruecktStartzeit = jetzt;  // Startzeit beim Drücken speichern
                wurdeBereitsGemeldet = false; // Zurücksetzen, wenn Taster gedrückt wird
            }
        }
    }

    letzterZustand = aktuellerZustand;
}

// Methode, um zu prüfen, ob der Taster gedrückt ist
bool EntprellterTaster::istGedrueckt() {
    return entprellterZustand;
}

// Methode, um zu prüfen, ob der Taster kurz gedrückt wurde
bool EntprellterTaster::wurdeGedrueckt() {
    unsigned long jetzt = millis();

    // Wenn der Taster losgelassen wurde und weniger als 1 Sekunde gedrückt war
    if (!entprellterZustand && !wurdeBereitsGemeldet && (jetzt - gedruecktStartzeit < 1000) && gedruecktStartzeit > 0) {
        wurdeBereitsGemeldet = true;  // Verhindern, dass mehrmals TRUE geliefert wird
        return true;
    }
    return false;
}

// Methode, um zu prüfen, ob der Taster lange gedrückt wurde
bool EntprellterTaster::wurdeLangeGedrueckt() {
    unsigned long jetzt = millis();

    // Wenn der Taster mehr als 1 Sekunde durchgehend gedrückt wurde
    if (entprellterZustand && (jetzt - gedruecktStartzeit >= 1000) && !wurdeBereitsGemeldet) {
        wurdeBereitsGemeldet = true;  // Verhindern, dass mehrmals TRUE geliefert wird
        return true;
    }
    return false;
}
