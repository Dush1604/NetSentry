
#include "serial_utils.h"

static SemaphoreHandle_t serialMutex;

void initSerialMutex() {
  serialMutex = xSemaphoreCreateMutex();
}

void safePrintln(const String& msg) {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println(msg);
    xSemaphoreGive(serialMutex);
  }
}

SemaphoreHandle_t networkMutex;
void initNetworkMutex() {
  networkMutex = xSemaphoreCreateMutex();
}
