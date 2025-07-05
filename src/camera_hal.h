#ifndef CAMERA_HAL_H
#define CAMERA_HAL_H

#include <Arduino.h>
#include <esp_err.h>
#include <esp_camera.h>

static inline const char *camera_model_name(uint16_t pid) {
    switch (pid) {
        case OV9650_PID:   return "OV9650";
        case OV7725_PID:   return "OV7725";
        case OV2640_PID:   return "OV2640";
        case OV3660_PID:   return "OV3660";
        case OV5640_PID:   return "OV5640";
        case OV7670_PID:   return "OV7670";
        case NT99141_PID:  return "NT99141";
        case GC2145_PID:   return "GC2145";
        case GC032A_PID:   return "GC032A";
        case GC0308_PID:   return "GC0308";
        case BF3005_PID:   return "BF3005";
        case BF20A6_PID:   return "BF20A6";
        case SC101IOT_PID: return "SC101IOT";
        case SC030IOT_PID: return "SC030IOT";
        case SC031GS_PID:  return "SC031GS";
        default:           return "Unknown";
    }
}


class CameraHal {
  public:
    static esp_err_t init();
    static sensor_t* get_sensor();

  private:
    static camera_config_t create_config();
    static void configure_sensor(sensor_t* sensor);

};

#endif // CAMERA_HAL_H