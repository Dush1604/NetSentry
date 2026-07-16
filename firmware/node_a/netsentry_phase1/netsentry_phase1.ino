/*
  NetSentry — Phase 1
  Proves out two things independently, before any networking infra exists:
    1. Network sensing: WiFi RSSI + ICMP ping latency to gateway and 8.8.8.8
    2. Camera capture: can we grab a still and read its size/quality

  Board: Freenove ESP32-WROVER CAM
  Arduino IDE board setting: "ESP32 Wrover Module"
  PSRAM: Enabled | Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)

  Two FreeRTOS tasks run independently, pinned to separate cores.
  This is intentional groundwork for later phases (ArpScanner, MotionDetect,
  SnapshotCapture, MqttPublish will all become additional tasks).
*/

#include "WiFi.h"
#include <ESP32Ping.h>
#include "esp_camera.h"

// ---- WiFi credentials ----
// ---- WiFi credentials (see config.h.example) ----
#include "config.h"

// ---- Camera pin definitions for Freenove ESP32-WROVER CAM ----
// (this is the CAMERA_MODEL_WROVER_KIT pin map — fixed, do not change)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     21
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Handle so both tasks can print to Serial without garbling each other's output
SemaphoreHandle_t serialMutex;

void safePrintln(const String& msg) {
  if (xSemaphoreTake(serialMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    Serial.println(msg);
    xSemaphoreGive(serialMutex);
  }
}

// ---------------------------------------------------------------------------
// Task 1: NetworkProbe — runs every 5s, measures RSSI + ping latency
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Task 2: CameraCaptureTest — runs every 30s, proves capture pipeline works
// ---------------------------------------------------------------------------
void cameraCaptureTask(void* parameter) {
  for (;;) {
    camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {
      safePrintln("[Camera] Capture FAILED");
    } else {
      String line = "[Camera] Captured frame: " + String(fb->len) + " bytes, "
                    + String(fb->width) + "x" + String(fb->height);
      safePrintln(line);
      esp_camera_fb_return(fb);
    }

    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // This board has 8MB PSRAM — use it for larger frames + double buffering
  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;   // 800x600, good balance for Phase 1
    config.jpeg_quality = 12;             // lower number = higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;   // fallback if PSRAM isn't detected
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  } else {
    Serial.println("Camera initialized OK");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  serialMutex = xSemaphoreCreateMutex();

  // --- WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  // --- Camera ---
  initCamera();

  // --- Tasks, pinned to separate cores so camera capture never blocks network probing ---
  xTaskCreatePinnedToCore(
    networkProbeTask, "NetworkProbe", 4096, NULL, 1, NULL, 0  // core 0
  );

  xTaskCreatePinnedToCore(
    cameraCaptureTask, "CameraCaptureTest", 8192, NULL, 1, NULL, 1  // core 1
  );
}

void loop() {
  // Intentionally empty — all work happens in the FreeRTOS tasks above.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
