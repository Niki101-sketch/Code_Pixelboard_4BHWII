#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "DEIN_WLAN";
const char* password = "PASSWORT";

const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Verbinde WLAN...");
  }

  Serial.println("WLAN verbunden");

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP_SENDER")) {
      Serial.println("MQTT verbunden");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  client.publish("pixel/test", "Hallo vom ESP!");
  delay(2000);
}