#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 4      // Pin, an dem der DHT22 angeschlossen ist
#define DHTTYPE DHT22 // DHT 22 (AM2302)

DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("DHT22 Sensor wird gestartet...");
    dht.begin();
}

void loop() {
    delay(2000); // Alle 2 Sekunden auslesen

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // °C

    // Fehlerprüfung
    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Fehler beim Auslesen des DHT22!");
        return;
    }

    Serial.print("Luftfeuchtigkeit: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperatur: ");
    Serial.print(temperature);
    Serial.println(" °C");
}