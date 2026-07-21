
#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H
#include <Arduino.h>

void initMqtt();
void mqttPublishTask(void* parameter);
void publishRaw(const String& topic, const String& payload);

#endif
