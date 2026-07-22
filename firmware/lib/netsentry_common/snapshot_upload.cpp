
#include "snapshot_upload.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "config.h"
#include "serial_utils.h"

bool uploadSnapshot(camera_fb_t* fb) {
  if (!fb) return false;

  if (xSemaphoreTake(networkMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    safePrintln("[Upload] Could not acquire network lock, skipping");
    return false;
  }

  HTTPClient http;
  String url = "http://" + String(BACKEND_HOST) + ":" + String(BACKEND_PORT) +
               "/images/upload?device_id=" + String(NODE_ID);

  // Manually building a multipart/form-data body — HTTPClient doesn't have
  // a built-in helper for this, so we construct the exact byte layout
  // the HTTP multipart spec requires.
  String boundary = "NetSentryBoundary7MA4YWxkTrZu0gW";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"file\"; filename=\"snapshot.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t totalLen = head.length() + fb->len + tail.length();
  uint8_t* body = (uint8_t*)malloc(totalLen);
  if (!body) {
    safePrintln("[Upload] Failed to allocate body buffer (" + String(totalLen) + " bytes)");
    return false;
  }

  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), fb->buf, fb->len);
  memcpy(body + head.length() + fb->len, tail.c_str(), tail.length());

  http.begin(url);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Connection", "close");
  http.setTimeout(10000);

  int httpCode = http.POST(body, totalLen);
  free(body);
  http.end();

  if (httpCode == 200) {
    safePrintln("[Upload] Snapshot uploaded (" + String(fb->len) + " bytes)");
    return true;
  } else {
    safePrintln("[Upload] Upload failed, HTTP code: " + String(httpCode));
    return false;
  }

  xSemaphoreGive(networkMutex);
  return httpCode == 200;
}
