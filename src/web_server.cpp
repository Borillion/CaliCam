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

static int print_reg(char *p_json, sensor_t *sensor, uint16_t reg, uint32_t mask){
    return sprintf(p_json, "\"0x%x\":%u,", reg, sensor->get_reg(sensor, reg, mask));
}

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

        httpd_uri_t uri_stat = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = handle_status,
        .user_ctx = NULL
    };

    httpd_uri_t uri_stat = {
        .uri = "/xclk",
        .method = HTTP_GET,
        .handler = handle_xclk,
        .user_ctx = NULL
    };

    httpd_uri_t uri_greg = {
        .uri = "/greg",
        .method = HTTP_GET,
        .handler = handle_getreg,
        .user_ctx = NULL
    };

    httpd_uri_t uri_sreg = {
        .uri = "/sreg",
        .method = HTTP_GET,
        .handler = handle_setreg,
        .user_ctx = NULL
    };

    httpd_uri_t uri_spll = {
        .uri = "/spll",
        .method = HTTP_GET,
        .handler = handle_setreg,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &uri);
    httpd_register_uri_handler(server, &uri_stream);
    httpd_register_uri_handler(server, &uri_snapshot);
    httpd_register_uri_handler(server, &uri_cmd);
    httpd_register_uri_handler(server, &uri_stat);
    httpd_register_uri_handler(server, &uri_greg);
    httpd_register_uri_handler(server, &uri_sreg);
    httpd_register_uri_handler(server, &uri_spll);

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
    uint16_t installed_sensor = sensor->id.PID;
    const char *name = camera_model_name(installed_sensor);

    Serial.printf("Detected camera: %x  - %s Sensor\n", installed_sensor, name);

    *p_json++ = '{';

    if (installed_sensor == OV5640_PID || installed_sensor == OV3660_PID) {
        // read the AWB Manual Control register (bit 0: 0 = auto, 1 = manual)
        p_json += print_reg(p_json, sensor, 0x3406, 0xFF);
        // read the AEC Exposure Time High‑Byte register (shouldn't this be 0xFFFF 16 bits? exposure bits [19:16])
        p_json += print_reg(p_json, sensor, 0x3500, 0xFFFF0);
        // read the AEC Control register (manual/auto mode bits: bit 0 = AEC manual, bit 1 = AGC manual)
        p_json += print_reg(p_json, sensor, 0x3503, 0xFF);
        // read the AEC AGC gain low byte register (Gain[7:0])
        p_json += print_reg(p_json, sensor, 0x350B, 0xFF);
        // read the AEC extra-exposure high byte register (extends 20-bit exposure time)
        p_json += print_reg(p_json, sensor, 0x350C, 0xFF);

        // read the Gamma correction register [0x5480–0x5490]: part of gamma curve settings
        for (int reg = 0x5480; reg <= 0x5490; reg++) {
            p_json += print_reg(p_json, sensor, reg, 0xFF);
        }

        // read the Color Matrix control register [0x5380–0x538B]: coefficients for RGB-to-YUV conversion
        for (int reg = 0x5380; reg <= 0x538B; reg++) {
            p_json += print_reg(p_json, sensor, reg, 0xFF);
        }

        // read SDE (Special Digital Effects) UV-adjust control register [0x5580–0x5589]
        for (int reg = 0x5580; reg < 0x558A; reg++) {
            p_json += print_reg(p_json, sensor, reg, 0xFF);
        }
        // read SDE CTRL10 (0x558A): UV adjust threshold high bit (bit 0), mask 9 bits
        p_json += print_reg(p_json, sensor, 0x558A, 0x1FF);

    } else if (installed_sensor == OV2640_PID) {
        // PLL control register (controls clock settings, frame rate, etc.)
        p_json += print_reg(p_json, sensor, 0xd3, 0xFF);
        // Clock divider register: sets the internal clock division ratio , influencing overall sensor speed
        p_json += print_reg(p_json, sensor, 0x111, 0xFF);
        // Unknown function register, is it undocumented?
        p_json += print_reg(p_json, sensor, 0x132, 0xFF);
    }

    // Sensor master clock frequency in MHz (feeds the sensor timing block)
    p_json += sprintf(p_json, "\"xclk\":%u,", sensor->xclk_freq_hz / 1000000);
    // Pixel output format (e.g., JPEG, RGB565, GRAYSCALE, YUV422)
    p_json += sprintf(p_json, "\"pixformat\":%u,", sensor->pixformat);
    // Resolution / frame size selection (e.g., UXGA, SVGA, QVGA)
    p_json += sprintf(p_json, "\"framesize\":%u,", sensor->status.framesize);
    // JPEG compression quality (0 = max quality, 63 = lowest quality)
    p_json += sprintf(p_json, "\"quality\":%u,", sensor->status.quality);
    // Brightness adjustment level (signed: –2 = darker to +2 = brighter)
    p_json += sprintf(p_json, "\"brightness\":%d,", sensor->status.brightness);
    // Contrast adjustment (signed: –2 = low to +2 = high contrast)
    p_json += sprintf(p_json, "\"contrast\":%d,", sensor->status.contrast);
    // Color saturation adjustment (signed: –2 = desaturated to +2 = vivid)
    p_json += sprintf(p_json, "\"saturation\":%d,", sensor->status.saturation);
    // Sharpness (edge enhancement level: –2 = soft to +2 = sharp)
    p_json += sprintf(p_json, "\"sharpness\":%d,", sensor->status.sharpness);
    // DSP special effect mode (0 = none, 1 = negative, … 6 = sepia)
    p_json += sprintf(p_json, "\"special_effect\":%u,", sensor->status.special_effect);
    // White balance mode (0 = auto or preset value when AWB disabled)
    p_json += sprintf(p_json, "\"wb_mode\":%u,", sensor->status.wb_mode);
    // Auto White Balance enable flag (0 = off, 1 = on)
    p_json += sprintf(p_json, "\"awb\":%u,", sensor->status.awb);
    // AWB gain enable flag (0 = off, 1 = on; uses white balance gains)
    p_json += sprintf(p_json, "\"awb_gain\":%u,", sensor->status.awb_gain);
    // Auto Exposure Control enable flag (0 = off, 1 = on)
    p_json += sprintf(p_json, "\"aec\":%u,", sensor->status.aec);
    // AEC stage 2 enable (secondary exposure algorithm, 0 = off, 1 = on)
    p_json += sprintf(p_json, "\"aec2\":%u,", sensor->status.aec2);
    // AE compensation level (signed –2 = darker to +2 = brighter target)
    p_json += sprintf(p_json, "\"ae_level\":%d,", sensor->status.ae_level);
    // Manual exposure target when AEC is disabled (0–1200 typical)
    p_json += sprintf(p_json, "\"aec_value\":%u,", sensor->status.aec_value);
    // Auto Gain Control enable flag (0 = off, 1 = on)
    p_json += sprintf(p_json, "\"agc\":%u,", sensor->status.agc);
    // Manual AGC gain value when AGC is on (0–30 typical)
    p_json += sprintf(p_json, "\"agc_gain\":%u,", sensor->status.agc_gain);
    // Gain ceiling setting (0–6): maximum auto gain limit
    p_json += sprintf(p_json, "\"gainceiling\":%u,", sensor->status.gainceiling);
    // Black pixel compensation (defective pixel correction) flag
    p_json += sprintf(p_json, "\"bpc\":%u,", sensor->status.bpc);
    // White pixel compensation (defective pixel correction) flag
    p_json += sprintf(p_json, "\"wpc\":%u,", sensor->status.wpc);
    // Raw gamma correction enable flag (0 = off, 1 = on)
    p_json += sprintf(p_json, "\"raw_gma\":%u,", sensor->status.raw_gma);
    // Lens shading correction (vignetting compensation) flag
    p_json += sprintf(p_json, "\"lenc\":%u,", sensor->status.lenc);
    // Horizontal mirror image flag (flip left-right)
    p_json += sprintf(p_json, "\"hmirror\":%u,", sensor->status.hmirror);
    // Downsampling/Cropping Window enable (reduces output size)
    p_json += sprintf(p_json, "\"dcw\":%u,", sensor->status.dcw);
    // Colorbar test pattern output flag (0 = normal, 1 = test pattern)
    p_json += sprintf(p_json, "\"colorbar\":%u", sensor->status.colorbar);

    // Remove the last comma if present
    if (*(p_json - 1) == ',') {
        p_json--;
    }
    *p_json++ = '}';
    *p_json = 0;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

esp_err_t WebServer::handle_xclk(httpd_req_t *req) {

    char param[256];
    char key[32];
    char val[32];
    int xclock = 0;

    esp_err_t res = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (res != ESP_OK) {
        return res;
    }
    // ESP_OK has a zero val, so check if httpd_req_get_url_query_str returned the same.
    if (res == ESP_OK) {
        // extract the var and value for each parameter
        esp_err_t res_var = httpd_query_key_value(param, "var", key, sizeof(key));
        esp_err_t res_val = httpd_query_key_value(param, "val", val, sizeof(val));

        if (res_var != ESP_OK)
        {
            Serial.println("Missing or invalid var parameter");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Missing val parameter");
            return ESP_FAIL;
        }

        if (res_val != ESP_OK)
        {
            Serial.println("Missing or invalid val parameter");
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Missing var parameter");
            return ESP_FAIL;
        }

        xclock = std::stoi(val);
        // Use serial printf for variables in the logging
        Serial.printf("%s = %d\n", key, xclock);

        sensor_t *sensor = esp_camera_sensor_get();
        int set_res = sensor->set_xclk(sensor, LEDC_TIMER_0, xclock);
        if (set_res) {
            return httpd_resp_send_500(req);
        }

        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        return httpd_resp_send(req, NULL, 0);
    }

    return ESP_FAIL;
}

esp_err_t WebServer::handle_getreg(httpd_req_t *req) {
    char param[256];
    char reg_str[16];
    char mask_str[16];
    char response[64];  // Increased to hold formatted JSON

    esp_err_t res = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (res != ESP_OK) {
        return res;
    }

    // extract the var and value for each parameter
    esp_err_t res_var = httpd_query_key_value(param, "register", reg_str, sizeof(reg_str));
    esp_err_t res_val = httpd_query_key_value(param, "mask", mask_str, sizeof(mask_str));

    if (res_var != ESP_OK) {
        Serial.println("Missing or invalid 'register' parameter");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing register parameter");
        return ESP_FAIL;
    }

    if (res_val != ESP_OK) {
        Serial.println("Missing or invalid 'mask' parameter");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mask parameter");
        return ESP_FAIL;
    }

    int reg = strtol(reg_str, NULL, 0);
    int mask = strtol(mask_str, NULL, 0);

    sensor_t *sensor = esp_camera_sensor_get();
    int value = sensor->get_reg(sensor, reg, mask);
    if (value < 0) {
        return httpd_resp_send_500(req);
    }

    Serial.printf("register: 0x%04x, mask: 0x%04x, value: 0x%08x, masked value: 0x%08x\n",
        reg, mask, value, (value & mask));

    // Minimal JSON response
    snprintf(response, sizeof(response),
             "{ \"reg\": \"0x%X\", \"mask\": \"0x%X\", \"value\": \"0x%X\", \"masked\": \"0x%X\" }",
             reg, mask, value, value & mask);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    // HTTPD_RESP_USE_STRLEN is a special constant defined in ESP-IDF used with httpd_resp_send() 
    // to automatically calculate the string length using strlen() instead of manually specifying it.
    // Consider fixing the rest of the code to use this instead of strlen over and over?
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebServer::handle_setreg(httpd_req_t *req) {
    
    char param[256];
    char reg_str[16];
    char val_str[16];
    char mask_str[16];
    char response[64];

    esp_err_t res = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (res != ESP_OK) {
        return res;
    }

    // extract the var and value for each parameter
    esp_err_t res_var = httpd_query_key_value(param, "register", reg_str, sizeof(reg_str));
    esp_err_t res_mask = httpd_query_key_value(param, "mask", mask_str, sizeof(mask_str));
    esp_err_t res_val = httpd_query_key_value(param, "value", val_str, sizeof(val_str));

    if (res_var != ESP_OK) {
        Serial.println("Missing or invalid 'register' parameter");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing register parameter");
        return ESP_FAIL;
    }

    if (res_mask != ESP_OK) {
        Serial.println("Missing or invalid 'mask' parameter");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mask parameter");
        return ESP_FAIL;
    }

    if (res_val != ESP_OK) {
        Serial.println("Missing or invalid 'mask' parameter");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing mask parameter");
        return ESP_FAIL;
    }

    int reg = strtol(reg_str, NULL, 0);
    int mask = strtol(mask_str, NULL, 0);
    int value = strtol(val_str, NULL,0);

    Serial.printf("register: 0x%04x, mask: 0x%04x, value: 0x%08x, masked value: 0x%08x\n",
        reg, mask, value, (value & mask));

    snprintf(response, sizeof(response),
             "{ \"reg\": \"0x%X\", \"mask\": \"0x%X\", \"value\": \"0x%X\", \"masked\": \"0x%X\" }",
             reg, mask, value, value & mask);


    sensor_t *sensor = esp_camera_sensor_get();
    int res = sensor->set_reg(sensor, reg, mask, value);
    if (res) {
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");

    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t WebServer::handle_setpll(httpd_req_t *req) {

    char param[256];

    // Struct for holding PLL Parameters
    typedef struct {
        int bypass;
        int mul;
        int sys;
        int root;
        int pre;
        int seld5;
        int pclken;
        int pclk;
    } pll_params_t;

    typedef struct {
        const char *key;
        int *field;
    } param_map_t;

    // Fetch URL query string
    esp_err_t res = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (res != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing PLL Parameters");
    }

    // Zero initialize PLL struct
    pll_params_t pll = {0};

    // Mapping of query keys to struct fields
    param_map_t map[] = {
        { "bypass",  &pll.bypass  },
        { "mul",     &pll.mul     },
        { "sys",     &pll.sys     },
        { "root",    &pll.root    },
        { "pre",     &pll.pre     },
        { "seld5",   &pll.seld5   },
        { "pclken",  &pll.pclken  },
        { "pclk",    &pll.pclk    },
    };

    // Parse each key from the URL query
    for (int i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        char val[16] = {0};
        if (httpd_query_key_value(param, map[i].key, val, sizeof(val)) == ESP_OK) {
            *(map[i].field) = atoi(val);
        }
    }

    // Print for debugging
    Serial.printf("Set PLL: bypass=%d, mul=%d, sys=%d, root=%d, pre=%d, seld5=%d, pclken=%d, pclk=%d\n",
        pll.bypass, pll.mul, pll.sys, pll.root, pll.pre, pll.seld5, pll.pclken, pll.pclk);

    sensor_t *sensor = esp_camera_sensor_get();
    if (sensor->set_pll(sensor, pll.bypass, pll.mul, pll.sys, pll.root, pll.pre, pll.seld5, pll.pclken, pll.pclk)) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set PLL");
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // More informative response:
    const char *ok_msg = "PLL parameters set";
    return httpd_resp_send(req, ok_msg, HTTPD_RESP_USE_STRLEN);
}

esp_err_t handle_setresolution(httpd_req_t *req){

    char param[256];

    typedef struct {
        // set the subrectangle of the image sensor output 
        // from which data will be read, useful for zoom or edge noise removal
        int startX;
        int startY;
        int endX;
        int endY;

        //Offset the window to fine tune alignment
        // Perhaps to adjust for lens misalignment?
        int offsetX;
        int offsetY;

        // Total X Y capture space of sensor
        int totalX;
        int totalY;

        // Output resolution or final framebuffer size
        int outputX;
        int outputY;

        // Hardware or software image scaling switch
        bool scale;

        // Combine adjecent pixels for better low light resolution
        // Improve signal to noise ratio, but reduces the pixel count
        bool binning;
    } resolution_params_t;


    typedef struct {
        const char *key;
        int *field;
    } param_map_t;

    typedef struct {
        const char *key;
        bool *field;
    } bool_param_map_t;

    resolution_params_t res_params = {0}; 

    // Get the full query string (e.g., ?sx=0&sy=0&...)
    esp_err_t result = httpd_req_get_url_query_str(req, param, sizeof(param));
    if (result != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing resolution parameters");
    }

    // Map of parameter names to fields in the struct
    param_map_t param_map[] = {
        { "sx",   &res_params.startX },
        { "sy",   &res_params.startY },
        { "ex",   &res_params.endX },
        { "ey",   &res_params.endY },
        { "offx", &res_params.offsetX },
        { "offy", &res_params.offsetY },
        { "tx",   &res_params.totalX },
        { "ty",   &res_params.totalY },
        { "ox",   &res_params.outputX },
        { "oy",   &res_params.outputY },  
    };

        // Boolean parameter mapping
        bool_param_map_t bool_map[] = {
        { "scale",   &res_params.scale },
        { "binning", &res_params.binning }
    };



}