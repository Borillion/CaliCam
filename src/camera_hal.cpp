#include <esp_err.h>
#include "camera_hal.h"
#include "pinout_sense_camera.h"

class CameraConfig;

esp_err_t CameraHal::init() {

    return ESP_OK;

}

camera_config_t CameraHal::create_camera_config() {
    camera_config_t config;

    return config;
}