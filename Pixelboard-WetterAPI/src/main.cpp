#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Zugangsdaten - HIER EINTRAGEN!
const char* ssid = "iPhone von Paul";
const char* password = "rootroot";

// OpenWeatherMap API - Kostenloser API Key (registriere dich auf openweathermap.org)
const char* apiKey = "ec59e8958e52a071ca78979743962031";
const char* city = "Innsbruck";
const char* countryCode = "AT";

// API Endpoint
String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + 
                    String(city) + "," + String(countryCode) + 
                    "&appid=" + String(apiKey) + "&units=metric&lang=de";

// Update Intervall (10 Minuten = 600000 ms)
unsigned long lastTime = 0;
unsigned long timerDelay = 600000;

void connectWiFi() {
  Serial.println("Verbinde mit WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden!");
    Serial.print("IP Adresse: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Verbindung fehlgeschlagen!");
  }
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    Serial.println("\n=== Wetterdaten abrufen ===");
    http.begin(serverPath.c_str());
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      String payload = http.getString();
      
      // JSON parsen
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print("JSON Parsing fehlgeschlagen: ");
        Serial.println(error.c_str());
        return;
      }
      
      // Wetterdaten extrahieren
      const char* cityName = doc["name"];
      float temp = doc["main"]["temp"];
      float feelsLike = doc["main"]["feels_like"];
      int humidity = doc["main"]["humidity"];
      int pressure = doc["main"]["pressure"];
      float windSpeed = doc["wind"]["speed"];
      const char* description = doc["weather"][0]["description"];
      
      // Ausgabe auf dem Monitor
      Serial.println("\n╔═══════════════════════════════════════╗");
      Serial.println("║       WETTERDATEN INNSBRUCK          ║");
      Serial.println("╚═══════════════════════════════════════╝");
      Serial.println();
      Serial.print("🏙️  Stadt: ");
      Serial.println(cityName);
      Serial.print("🌡️  Temperatur: ");
      Serial.print(temp);
      Serial.println(" °C");
      Serial.print("🤔 Gefühlt: ");
      Serial.print(feelsLike);
      Serial.println(" °C");
      Serial.print("💧 Luftfeuchtigkeit: ");
      Serial.print(humidity);
      Serial.println(" %");
      Serial.print("📊 Luftdruck: ");
      Serial.print(pressure);
      Serial.println(" hPa");
      Serial.print("💨 Windgeschwindigkeit: ");
      Serial.print(windSpeed);
      Serial.println(" m/s");
      Serial.print("☁️  Beschreibung: ");
      Serial.println(description);
      Serial.println("───────────────────────────────────────");
      
    } else {
      Serial.print("Fehler beim Abrufen: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi nicht verbunden!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=================================");
  Serial.println("ESP32 Wetter Station - Innsbruck");
  Serial.println("=================================\n");
  
  // WiFi verbinden
  connectWiFi();
  
  // Erste Abfrage direkt nach Start
  if (WiFi.status() == WL_CONNECTED) {
    delay(2000);
    getWeatherData();
  }
}

void loop() {
  // Wetterdaten alle 10 Minuten aktualisieren
  if ((millis() - lastTime) > timerDelay) {
    getWeatherData();
    lastTime = millis();
  }
}