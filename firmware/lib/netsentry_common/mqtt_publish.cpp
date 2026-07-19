
#include "mqtt_publish.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "serial_utils.h"

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);

static void reconnectMqtt() {
  int attempt = 0;
  while (!mqttClient.connected()) {
    String clientId = String(NODE_ID) + "-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      safePrintln("[MQTT] Connected as " + clientId);
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
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
}

void mqttPublishTask(void* parameter) {
  for (;;) {
    if (!mqttClient.connected()) {
      reconnectMqtt();
    }
    mqttClient.loop();  // must be called regularly to keep connection alive

    int rssi = WiFi.RSSI();
    String topic = "sentinel/" + String(NODE_ID) + "/telemetry";
    String payload = "{\"node_id\":\"" + String(NODE_ID) +
                      "\",\"rssi\":" + String(rssi) +
                      ",\"heap_free\":" + String(ESP.getFreeHeap()) + "}";

    bool sent = mqttClient.publish(topic.c_str(), payload.c_str());
    safePrintln("[MQTT] Publish " + String(sent ? "OK" : "FAILED") + " -> " + topic);

    vTaskDelay(pdMS_TO_TICKS(10000));  // publish every 10s
  }
}
