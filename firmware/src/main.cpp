
#include <WiFi.h>
#include "config.h"
#include "network_probe.h"
#include "camera_capture.h"
#include "serial_utils.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  initSerialMutex();

  Serial.print("Node ID: "); Serial.println(NODE_ID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("Connected. IP: "); Serial.println(WiFi.localIP());

  initCamera();

  xTaskCreatePinnedToCore(networkProbeTask, "NetworkProbe", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(cameraCaptureTask, "CameraCaptureTest", 8192, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
