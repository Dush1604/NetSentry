
#include "camera_capture.h"
#include "esp_camera.h"
#include "serial_utils.h"
#include "snapshot_upload.h"
#include "mqtt_publish.h"

#define MOTION_THRESHOLD_PERCENT 25.0 // frame-size change % that counts as "motion" — tune based on real-world testing

// Camera pin definitions for Freenove ESP32-WROVER CAM — fixed by hardware, do not change
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
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }
  config.grab_mode = CAMERA_GRAB_LATEST;   // your FB-OVF fix from earlier

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  } else {
    Serial.println("Camera initialized OK");
  }
}

void cameraCaptureTask(void* parameter) {
  size_t previousLen = 0;
  bool firstFrame = true;

  for (;;) {
    int requestedEventId;
    if (xQueueReceive(snapshotRequestQueue, &requestedEventId, 0) == pdTRUE) {
      safePrintln("[Camera] Remote snapshot requested for event_id=" + String(requestedEventId));
      camera_fb_t* remoteFb = esp_camera_fb_get();
      if (remoteFb) {
        uploadSnapshot(remoteFb, requestedEventId);
        esp_camera_fb_return(remoteFb);
      }
    }

    camera_fb_t* fb = esp_camera_fb_get();

    if (!fb) {
      safePrintln("[Camera] Capture FAILED | Free heap: " + String(ESP.getFreeHeap()));
    } else {
      safePrintln("[Camera] Captured frame: " + String(fb->len) + " bytes, " +
                  String(fb->width) + "x" + String(fb->height));

      if (!firstFrame && previousLen > 0) {
        long diff = (long)fb->len - (long)previousLen;
        float percentChange = abs(diff) * 100.0 / previousLen;

        if (percentChange > MOTION_THRESHOLD_PERCENT) {
          safePrintln("[Motion] " + String(percentChange, 1) + "% frame-size change — uploading snapshot");
          uploadSnapshot(fb);
        }
      }

      previousLen = fb->len;
      firstFrame = false;
      esp_camera_fb_return(fb);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));  // check every 10s
  }
}
