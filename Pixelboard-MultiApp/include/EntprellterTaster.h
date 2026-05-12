#ifndef ENTPRELLTER_TASTER_H
#define ENTPRELLTER_TASTER_H

#include <Arduino.h>

class EntprellterTaster {
public:
    // Konstruktor
    EntprellterTaster(int pin);

    // Methode zum Aktualisieren des Zustands
    void aktualisiere();

    // Methode, um zu prüfen, ob der Taster gedrückt ist
    bool istGedrueckt();

    // Methode, um zu prüfen, ob der Taster kurz gedrückt wurde
    bool wurdeGedrueckt();

    // Methode, um zu prüfen, ob der Taster lange gedrückt wurde
    bool wurdeLangeGedrueckt();

private:
    const int pin;
    bool entprellterZustand;
    bool letzterZustand;
    unsigned long letzteAenderung;
    unsigned long gedruecktStartzeit;
    bool wurdeBereitsGemeldet;
    const unsigned long ENTPRELLZEIT = 20; // Entprellzeit in Millisekunden
};

#endif
