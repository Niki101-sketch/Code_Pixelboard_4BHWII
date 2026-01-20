# Snake Game - Schritt 1: Pixel Test

## PlatformIO Projekt für ESP32

### Ordnerstruktur

```
snake_game/
├── platformio.ini          ← Konfiguration (Board, Libraries)
├── include/
│   ├── EntprellterTaster.h ← Header für Taster-Klasse
│   └── Joystick.h          ← Header für Joystick-Klasse
├── src/
│   ├── main.cpp            ← Hauptprogramm (Pixel-Test)
│   ├── EntprellterTaster.cpp
│   └── Joystick.cpp
└── README.md               ← Diese Datei
```

### Installation

1. **Ordner kopieren**: Den gesamten `snake_game` Ordner irgendwo auf deinem PC speichern

2. **In VSCode öffnen**: 
   - VSCode starten
   - File → Open Folder → `snake_game` auswählen
   - PlatformIO sollte das Projekt automatisch erkennen

3. **Build & Upload**:
   - Unten in der blauen Leiste auf den **Pfeil (→)** klicken für Upload
   - Oder: `Strg+Alt+U`
   - Beim ersten Mal wird FastLED automatisch heruntergeladen

4. **Serial Monitor öffnen**:
   - Unten in der blauen Leiste auf das **Stecker-Symbol** klicken
   - Oder: `Strg+Alt+S`
   - Baudrate: 115200

### Hardware-Anschlüsse

| ESP32 Pin | Funktion |
|-----------|----------|
| 25 | Data Pin oberes LED-Panel |
| 26 | Data Pin unteres LED-Panel |
| 32 | Joystick Button |
| 34 | Joystick X-Achse |
| 35 | Joystick Y-Achse |
| 3.3V | Joystick VCC (NICHT 5V!) |
| GND | Joystick GND |

### Was der Test macht

Der Code führt 12 Tests durch, um zu prüfen ob die Pixel-Ansteuerung stimmt:

1. Alle LEDs kurz weiß
2. Vier Ecken (Rot, Grün, Blau, Gelb)
3. Obere Zeile rot
4. Untere Zeile blau
5. Linke Spalte grün
6. Rechte Spalte gelb
7. Panel-Grenze (Mitte)
8. Kompletter Rahmen
9. Wandernder Pixel horizontal
10. Wandernder Pixel vertikal
11. Snake-Simulation
12. Zeilennummern-Muster

### Falls etwas nicht stimmt

**Pixel an falscher Stelle?** → Melde mir welcher Test falsch aussieht!

- Oben/Unten vertauscht?
- Links/Rechts gespiegelt?
- Nur halbe Breite leuchtet?
- Panels überlagern sich?

Dann passe ich die `updateDisplay()` Funktion an!

### Nächste Schritte

Nach erfolgreichem Test:
- Schritt 2: Snake-Bewegung
- Schritt 3: Joystick-Steuerung
- Schritt 4: Kollisionserkennung
- Schritt 5: Food & Wachstum
- Schritt 6: Game Over
