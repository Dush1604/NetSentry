
#include "network_probe.h"
#include <ESPping.h>
#include <WiFi.h>
#include "serial_utils.h"

void networkProbeTask(void* parameter) {
  IPAddress gateway = WiFi.gatewayIP();

  for (;;) {
    int rssi = WiFi.RSSI();

    bool gatewayOk = Ping.ping(gateway, 3);
    float gatewayAvgMs = gatewayOk ? Ping.averageTime() : -1;

    bool externalOk = Ping.ping("8.8.8.8", 3);
    float externalAvgMs = externalOk ? Ping.averageTime() : -1;

    String line = "[NetworkProbe] RSSI: " + String(rssi) + " dBm | "
                  "Gateway RTT: " + (gatewayOk ? String(gatewayAvgMs, 1) + " ms" : "FAIL") + " | "
                  "8.8.8.8 RTT: " + (externalOk ? String(externalAvgMs, 1) + " ms" : "FAIL");
    safePrintln(line);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
