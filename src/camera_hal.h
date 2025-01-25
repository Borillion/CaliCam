#ifndef CAMERA_HAL_H
#define CAMERA_HAL_H

#include <Arduino.h>
#include <esp_err.h>
#include <esp_camera.h>

class CameraHal {
  public:
    static esp_err_t init();
    static sensor_t* get_sensor();

  private:
    static camera_config_t create_config();
    static void configure_sensor(sensor_t* sensor);

};

#endif // CAMERA_HAL_H