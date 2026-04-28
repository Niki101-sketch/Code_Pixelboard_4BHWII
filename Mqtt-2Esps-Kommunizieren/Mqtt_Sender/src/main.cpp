#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "SPL_ROBO_FrisB_2_4GHz";
const char* password = "123456789";

const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastSend = 0;
const long interval = 3000; // 3 Sekunden

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("WLAN...");
  }

  Serial.println("WLAN OK");

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("MQTT verbinden...");

    if (client.connect("ESP_SENDER_01")) {
      Serial.println("MQTT verbunden ✔");
    } else {
      Serial.print("Fehler rc=");
      Serial.println(client.state());
      delay(3000); // WICHTIG: kein Spam!
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();

  if (now - lastSend > interval) {
    lastSend = now;

    String msg = "Ping vom Sender";

    bool ok = client.publish("pixel/test", msg.c_str());

    if (ok) {
      Serial.println("Gesendet ✔: " + msg);
    } else {
      Serial.println("Senden fehlgeschlagen ❌");
    }
  }
}