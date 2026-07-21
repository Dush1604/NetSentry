
#include "arp_scanner.h"
#include <WiFi.h>
#include <ESPping.h>
#include "serial_utils.h"
#include "mqtt_publish.h"

extern "C" {
  #include "lwip/etharp.h"
}

// Scans the /24 subnet the ESP32 is currently on.
// For each host that responds to a ping, we look up its MAC via the
// lwIP ARP table (pinging forces MAC resolution as a side effect).
void arpScannerTask(void* parameter) {
  for (;;) {
    IPAddress localIP = WiFi.localIP();
    IPAddress subnetMask = WiFi.subnetMask();

    // Only handles the common /24 home-network case (last octet varies 1-254)
    IPAddress baseIP(localIP[0], localIP[1], localIP[2], 0);

    String foundDevices = "[";
    bool first = true;
    int foundCount = 0;

    safePrintln("[ArpScanner] Starting subnet sweep...");

    for (int host = 1; host < 255; host++) {
      IPAddress target(baseIP[0], baseIP[1], baseIP[2], host);
      if (target == localIP) continue;  // skip ourselves

      bool responded = Ping.ping(target, 1);  // single ping, fast timeout
      if (responded) {
        ip4_addr_t ip4;
        ip4.addr = static_cast<uint32_t>(target);

        struct eth_addr* macFound = nullptr;
        const ip4_addr_t* ipFound = nullptr;
        s8_t idx = etharp_find_addr(NULL, &ip4, &macFound, &ipFound);

        if (idx >= 0 && macFound != nullptr) {
          char macStr[18];
          snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    macFound->addr[0], macFound->addr[1], macFound->addr[2],
                    macFound->addr[3], macFound->addr[4], macFound->addr[5]);

          if (!first) foundDevices += ",";
          foundDevices += "{\"ip\":\"" + target.toString() + "\",\"mac\":\"" + String(macStr) + "\"}";
          first = false;
          foundCount++;
        }
      }
      vTaskDelay(pdMS_TO_TICKS(10));  // brief yield between hosts, avoid hogging the WiFi stack
    }

    foundDevices += "]";
    safePrintln("[ArpScanner] Sweep complete. Found " + String(foundCount) + " devices.");

    String topic = "sentinel/" + String(NODE_ID) + "/devices";
    publishRaw(topic, foundDevices);

    vTaskDelay(pdMS_TO_TICKS(300000));  // full sweep every 5 minutes — a 254-host sweep is slow, no need to run it often
  }
}
