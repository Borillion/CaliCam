#ifndef CAMERA_HAL_H
#define CAMERA_HAL_H

#include <Arduino.h>
#include <esp_err.h>
#include <esp_camera.h>

class CameraHal {
  public:
    static esp_err_t init();

  private:
    static camera_config_t create_config();

};

#endif // CAMERA_HAL_H