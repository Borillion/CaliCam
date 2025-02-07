#include <sys/time.h>
#include <esp_camera.h>
#include <esp_err.h>

class CameraContext {
    private:
        struct StreamContext {
            camera_fb_t *fb; // camera frame buffer pointer
            uint8_t *jpeg_buf; // pointer to jpeg buffer
            size_t jpg_buf_len; // buffer size
            esp_err_t status; // fb capture status
            struct timeval timestamp; // timestamp of frame
        };

        bool is_streaming = false;
        StreamContext stream_ctx;

    public:
        CameraContext() {
            stream_ctx = {nullptr, nullptr, 0, ESP_OK, {0, 0}};
        }

        esp_err_t init();
};

