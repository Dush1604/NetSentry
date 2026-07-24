
#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H
#include <Arduino.h>

extern QueueHandle_t snapshotRequestQueue;

void initMqtt();
void mqttPublishTask(void* parameter);
void publishRaw(const String& topic, const String& payload);

#endif
