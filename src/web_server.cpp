#include <Arduino.h>
#include "web_server.h"
#include <esp_camera.h>
#include "camera_hal.h"

httpd_handle_t WebServer::server = NULL;

// A unique string that separates individual JPEG frames in a multipart MIME stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static const char* HTML_STREAM = R"(
<!DOCTYPE html>
<html>
<body>
    <img src="/stream" />
</body>
</html>
)";

static const char* HTML = R"(
<!DOCTYPE html>
<html>
<body>
    <p>Welcome to the CaliCam Web Server!</p>
</body>
</html>
)";

typedef struct setting_handler_t {
    //pointer to the setting string
    const char *key;
    //pointer to function that sets the variable
    //passing two arguments, the camera sensor struct ( im sensor.h), and val to set
    int (*handler)(sensor_t *, int);
} setting_handler_t;

esp_err_t WebServer::init() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    if (httpd_start(&server, &config) != ESP_OK) {
        return ESP_FAIL;
    }

    httpd_uri_t uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_index,
        .user_ctx = NULL
    };

    httpd_uri_t uri_stream = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = handle_stream,
        .user_ctx = NULL
    };

    httpd_uri_t uri_snapshot = {
        .uri = "/snapshot",
        .method = HTTP_GET,
        .handler = handle_snapshot,
        .user_ctx = NULL
    };

    httpd_uri_t uri_cmd = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = handle_command,
        .user_ctx = NULL
    };

        httpd_uri_t stat_cmd = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = handle_command,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &uri);
    httpd_register_uri_handler(server, &uri_stream);
    httpd_register_uri_handler(server, &uri_snapshot);
    
    httpd_register_uri_handler(server, &stat_cmd);

    return ESP_OK;
}

esp_err_t WebServer::handle_index(httpd_req_t *req) {
    return httpd_resp_send(req, HTML, strlen(HTML));
}

esp_err_t WebServer::handle_stream(httpd_req_t *req) {
    // To Do: is this really the best way? What about a websocket here?
    while(true) {
        Serial.println("Camera frame capture starting..");
        // Get frame from camera
        camera_fb_t *frame = esp_camera_fb_get();
        
        if (!frame) {
            Serial.println("Camera frame capture failed");
            // return -1 if the frame was not captured
            return ESP_FAIL;
        }

        // configure the http headers to militpart mime stream
        esp_err_t res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
        if (res != ESP_OK) {
            return res;
        }

        // return a stream boundry header in response to http request
        res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        if (res != ESP_OK) {
            // return the framebuffer for reuse
            esp_camera_fb_return(frame);
            return res;     
        }

        // this should be large enough for the part header, 80 bytes is enough for the header, but we use 128 to be safe ( 64 * 2 )
        char part_buf[128]; 

        // Get current timestamp
        struct timeval tv;
        gettimeofday(&tv, NULL);

        // write the mime part header info into the part buffer
        // snprintf returns the number of bytes written, so we can use it to send the chunk
        // the part header contains the content type, content length, and timestamp
        size_t part_len = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, frame->len, frame->timestamp.tv_sec, frame->timestamp.tv_usec);

        // ESP32's HTTP server function for sending partial responses
        res = httpd_resp_send_chunk(req, part_buf, part_len);
        //if the response was not sent successfully, return the framebuffer for reuse, and return the error code for the caller
        if (res != ESP_OK) {
            esp_camera_fb_return(frame);
            return res;
        }


        // send jpeg data
        res = httpd_resp_send_chunk(req, (const char *)frame->buf, frame->len);
        if (res != ESP_OK) {
            // return the framebuffer for reuse
            esp_camera_fb_return(frame);
            return res;
        }

        // // Debug print frame data
        // Serial.println("######### Frame ########");
        // for(size_t i = 0; i < frame->len; i++) {
        //     Serial.printf("%02X ", frame->buf[i]);
        // }    
        // Serial.println("######### Frame ########");

        // release frame
        esp_camera_fb_return(frame);

    }
}

esp_err_t WebServer::handle_snapshot(httpd_req_t *req) {
    Serial.println("Camera frame capture starting..");
    // Get frame from camera
    camera_fb_t *frame = esp_camera_fb_get();

    if (!frame) {
        Serial.println("Camera frame capture failed");
        // return -1 if the frame was not captured
        return ESP_FAIL;
    }

    // Set the mime type for the response
    // Set the Content-Disposition header to inline, so the browser will display the image, default filename is capture.jpg when saved by user
    // Allow any origin to access the image
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // Format timestamp from frame->timestamp to "seconds.microseconds"
    char ts_header[32];
    // snprintf formats the timestamp into a string with seconds and microseconds
    snprintf(ts_header, sizeof(ts_header), "%ld.%06ld", frame->timestamp.tv_sec, frame->timestamp.tv_usec);
    // Set the X-Timestamp header to unix style formatted timestamp
    httpd_resp_set_hdr(req, "X-Timestamp", ts_header);

    
    esp_err_t res = httpd_resp_send(req, (const char *)frame->buf, frame->len);
    esp_camera_fb_return(frame);

    return res;
}

esp_err_t WebServer::handle_command(httpd_req_t *req) { 

    char param[256];
    char key[32];
    char val[32];
    int value = 0;
    

    // get the string from the request using req_get_url_query_str
    esp_err_t res = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (res != ESP_OK) {
        return res;
    }
    // ESP_OK has a zero val, so check if httpd_req_get_url_query_str returned the same.
    if( res == ESP_OK) {
        // extract the var and value for each parameter
        esp_err_t res_var = httpd_query_key_value(param, "var", key, sizeof(key));
        esp_err_t res_val = httpd_query_key_value(param, "val", val, sizeof(val));

        if( res_var != ESP_OK )
        {
            Serial.println("Missing or invalid var parameter");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Missing val parameter");
            return ESP_FAIL;
        }

        if( res_val != ESP_OK )
        {
            Serial.println("Missing or invalid val parameter");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Missing var parameter");
            return ESP_FAIL;
        }

            value = std::stoi(val);
            // Use serial printf for variables in the logging
            Serial.printf("%s = %d\n", key, value);
    
            // get the camera sensor
            sensor_t *sensor = CameraHal::get_sensor();
            if(!sensor) {
                Serial.println("Failed to get camera sensor");
                return ESP_FAIL;
            }
    

        // set the corresponding values
        // We could set the variables using strcmp and a big if-else block, 
        // but there is a better way with dispatch tables and lambda functions


        // Each key associates a lambda function that takes a pointer to the sensor struct and an val to set
        // The lambda then calls the setter function on for each paramter to update that particular setting.
        static setting_handler_t handlers[] = {
            { "framesize", [](sensor_t *s, int val) {
                return s->pixformat == PIXFORMAT_JPEG ? s->set_framesize(s, (framesize_t)val) : ESP_OK;
            }},
            //key              []inherit nothing into the lambda function, (sensor_t *s, int val) the function takes two parameters.
            { "contrast",      [](sensor_t *s, int val) { return s->set_contrast(s, val); } },
            { "brightness",    [](sensor_t *s, int val) { return s->set_brightness(s, val); } },
            { "saturation",    [](sensor_t *s, int val) { return s->set_saturation(s, val); } },
            { "gainceiling",   [](sensor_t *s, int val) { return s->set_gainceiling(s, (gainceiling_t)val); } },
            { "quality",       [](sensor_t *s, int val) { return s->set_quality(s, val); } },
            { "colorbar",      [](sensor_t *s, int val) { return s->set_colorbar(s, val); } },
            { "awb",           [](sensor_t *s, int val) { return s->set_whitebal(s, val); } },
            { "agc",           [](sensor_t *s, int val) { return s->set_gain_ctrl(s, val); } },
            { "aec",           [](sensor_t *s, int val) { return s->set_exposure_ctrl(s, val); } },
            { "hmirror",       [](sensor_t *s, int val) { return s->set_hmirror(s, val); } },
            { "vflip",         [](sensor_t *s, int val) { return s->set_vflip(s, val); } },
            { "aec2",          [](sensor_t *s, int val) { return s->set_aec2(s, val); } },
            { "awb_gain",      [](sensor_t *s, int val) { return s->set_awb_gain(s, val); } },
            { "agc_gain",      [](sensor_t *s, int val) { return s->set_agc_gain(s, val); } },
            { "aec_value",     [](sensor_t *s, int val) { return s->set_aec_value(s, val); } },
            { "special_effect",[](sensor_t *s, int val) { return s->set_special_effect(s, val); } },
            { "wb_mode",       [](sensor_t *s, int val) { return s->set_wb_mode(s, val); } },
            { "ae_level",      [](sensor_t *s, int val) { return s->set_ae_level(s, val); } },
            { "dcw",           [](sensor_t *s, int val) { return s->set_dcw(s, val); } },
            { "bpc",           [](sensor_t *s, int val) { return s->set_bpc(s, val); } },
            { "wpc",           [](sensor_t *s, int val) { return s->set_wpc(s, val); } },
            { "raw_gma",       [](sensor_t *s, int val) { return s->set_raw_gma(s, val); } },
            { "lenc",          [](sensor_t *s, int val) { return s->set_lenc(s, val); } }
        };

        // Check if the provided key matches any known handler; handle unknown commands below.
        // Get the number of handlers in the handlers struct
        #define NUM_HANDLERS (sizeof(handlers) / sizeof(handlers[0]))

        res = -1;

        for(int i = 0; i < NUM_HANDLERS; i++){
            if (!strcmp(key, handlers[i].key)) {
                res = handlers[i].handler(sensor, value);
                break;
            }
        }
        if (res == -1) {
            Serial.println("Unknown command");
            return ESP_FAIL;
        }

        // send response
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
       
    }   

     return ESP_FAIL;
}

esp_err_t WebServer::handle_status(httpd_req_t *req) {

    static char json_response[1024];

    sensor_t *sensor = esp_camera_sensor_get();
    char *p_json = json_response;

    Serial.printf("%d sensor detected.\n", sensor->id.PID);
    return ESP_OK;
}
