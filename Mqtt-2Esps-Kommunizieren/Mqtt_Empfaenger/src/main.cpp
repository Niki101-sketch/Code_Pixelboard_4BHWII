#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "SPL_ROBO_FrisB_2_4GHz";
const char* password = "123456789";

const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

bool subscribed = false;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("📩 Topic: ");
  Serial.print(topic);
  Serial.print(" | Nachricht: ");

  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("WLAN...");
  }

  Serial.println("WLAN OK");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("MQTT verbinden...");

    if (client.connect("ESP_EMPFAENGER_01")) {
      Serial.println("MQTT verbunden ✔");

      // NUR EINMAL SUBSCRIBE
      if (!subscribed) {
        subscribed = true;

        if (client.subscribe("pixel/test")) {
          Serial.println("Subscribed ✔");
        } else {
          Serial.println("Subscribe fehlgeschlagen ❌");
        }
      }

    } else {
      Serial.print("Fehler rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop(); // GANZ WICHTIG
}