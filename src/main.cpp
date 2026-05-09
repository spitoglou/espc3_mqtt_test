/*
 * ESP32-C3 Super Mini  ->  RabbitMQ (via MQTT plugin)
 * ---------------------------------------------------
 * Publishes a JSON message every few seconds to a RabbitMQ broker
 * that has the MQTT plugin enabled.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "secrets.h"

// ---------- CONFIG ----------
constexpr const char* MQTT_TOPIC_TELEMETRY  = "sensors/esp32c3/data";
constexpr const char* MQTT_CLIENT_ID_PREFIX = "esp32c3-";
constexpr uint32_t    PUBLISH_INTERVAL_MS   = 5000;
constexpr uint16_t    MQTT_KEEPALIVE_S      = 30;
constexpr uint16_t    MQTT_BUFFER_SIZE      = 512;
constexpr uint32_t    MQTT_RETRY_DELAY_MS   = 3000;
constexpr uint32_t    SERIAL_WAIT_MS        = 2000;
constexpr uint8_t     LED_PIN               = 8;   // built-in, active LOW
constexpr uint16_t    LED_FLASH_MS          = 20;
// ----------------------------

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
String       mqttClientId;

uint32_t lastPublish = 0;
uint32_t msgCount    = 0;

inline void ledOn()  { digitalWrite(LED_PIN, LOW); }   // active low
inline void ledOff() { digitalWrite(LED_PIN, HIGH); }

void connectWiFi();
void connectMQTT();
void publishTelemetry();

void connectWiFi() {
  ledOn();
  Serial.printf("Connecting to WiFi \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n  connected. IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void connectMQTT() {
  ledOn();
  while (!mqtt.connected()) {
    Serial.printf("Connecting to RabbitMQ MQTT at %s:%u as \"%s\" (id=%s) ... ",
                  MQTT_HOST, MQTT_PORT, MQTT_USER, mqttClientId.c_str());
    if (mqtt.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("connected.");
    } else {
      // PubSubClient state codes: -4 timeout, -2 connect failed,
      // 4 bad credentials, 5 not authorised, etc.
      Serial.printf("failed, rc=%d. Retrying in %lu ms.\n",
                    mqtt.state(), (unsigned long)MQTT_RETRY_DELAY_MS);
      delay(MQTT_RETRY_DELAY_MS);
    }
  }
}

void publishTelemetry() {
  msgCount++;

  JsonDocument doc;
  doc["id"]        = msgCount;
  doc["uptime_ms"] = millis();
  doc["rssi"]      = WiFi.RSSI();
  doc["heap"]      = ESP.getFreeHeap();

  char payload[160];
  serializeJson(doc, payload, sizeof(payload));

  bool ok = mqtt.publish(MQTT_TOPIC_TELEMETRY, payload);

  // brief flash to signal publish activity
  ledOn();
  delay(LED_FLASH_MS);
  ledOff();

  Serial.printf("[%lu] %s -> \"%s\"  %s\n",
                (unsigned long)msgCount,
                ok ? "PUB " : "FAIL",
                MQTT_TOPIC_TELEMETRY, payload);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  ledOn();   // solid on through boot/connect

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < SERIAL_WAIT_MS) {
    delay(10);
  }
  Serial.println("\nESP32-C3  -> RabbitMQ (MQTT) starting...");

  connectWiFi();

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  mqttClientId = String(MQTT_CLIENT_ID_PREFIX) + mac;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  connectMQTT();

  ledOff();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected())             connectMQTT();
  ledOff();   // both links up
  mqtt.loop();

  uint32_t now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishTelemetry();
  }
}
