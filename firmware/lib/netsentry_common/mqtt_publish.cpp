
#include "mqtt_publish.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "serial_utils.h"

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

QueueHandle_t snapshotRequestQueue;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  // Manually parsing a known, simple JSON shape rather than pulling in a
  // full JSON library for one field — a deliberate simplification.
  if (message.indexOf("capture_snapshot") >= 0) {
    int eventId = -1;
    int idx = message.indexOf("\"event_id\":");
    if (idx >= 0) {
      eventId = message.substring(idx + 11).toInt();
    }
    xQueueSend(snapshotRequestQueue, &eventId, 0);
    safePrintln("[MQTT] Received capture_snapshot command, event_id=" + String(eventId));
  }
}

static void reconnectMqtt() {
  int attempt = 0;
  while (!mqttClient.connected()) {
    String clientId = String(NODE_ID) + "-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      safePrintln("[MQTT] Connected as " + clientId);
      String cmdTopic = "sentinel/" + String(NODE_ID) + "/command";
      mqttClient.subscribe(cmdTopic.c_str());
    } else {
      int delaySec = min(30, (int)pow(2, attempt));  // exponential backoff, capped at 30s
      safePrintln("[MQTT] Connect failed, rc=" + String(mqttClient.state()) +
                  " retrying in " + String(delaySec) + "s");
      vTaskDelay(pdMS_TO_TICKS(delaySec * 1000));
      attempt++;
    }
  }
}

void initMqtt() {
  snapshotRequestQueue = xQueueCreate(5, sizeof(int));
  mqttClient.setBufferSize(1024);  // set buffer size to 1KB
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
  mqttClient.setCallback(mqttCallback);
}

void mqttPublishTask(void* parameter) {
  for (;;) {
    if (!mqttClient.connected()) {
        reconnectMqtt();
      }

    if (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      mqttClient.loop();  // must be called regularly to keep connection alive

      int rssi = WiFi.RSSI();
      String topic = "sentinel/" + String(NODE_ID) + "/telemetry";
      String payload = "{\"node_id\":\"" + String(NODE_ID) +
                        "\",\"rssi\":" + String(rssi) +
                        ",\"heap_free\":" + String(ESP.getFreeHeap()) + "}";

      bool sent = mqttClient.publish(topic.c_str(), payload.c_str());
      safePrintln("[MQTT] Publish " + String(sent ? "OK" : "FAILED") + " -> " + topic);

      xSemaphoreGive(networkMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // publish every 10s
  }
}

void publishRaw(const String& topic, const String& payload) {
  if (mqttClient.connected()) {
    bool sent = mqttClient.publish(topic.c_str(), payload.c_str());
    safePrintln("[MQTT] Publish " + String(sent ? "OK" : "FAILED") + " -> " + topic);
  } else {
    safePrintln("[MQTT] Skipped publish (not connected) -> " + topic);
  }
}
