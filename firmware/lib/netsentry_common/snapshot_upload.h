
#ifndef SNAPSHOT_UPLOAD_H
#define SNAPSHOT_UPLOAD_H
#include "esp_camera.h"

bool uploadSnapshot(camera_fb_t* fb, int eventId = -1);

#endif
