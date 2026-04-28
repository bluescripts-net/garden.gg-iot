#ifndef UPLOAD_H
#define UPLOAD_H

#include "esp_camera.h"
#include "config.h"

class Upload {
public:
  static bool send(const ConfigData& config, camera_fb_t* fb);

private:
  static const char* GARDENGG_HOST;
  static const int GARDENGG_PORT;
  static const char* BOUNDARY;
};

#endif
