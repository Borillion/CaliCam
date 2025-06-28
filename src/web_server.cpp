#include <Arduino.h>
#include "web_server.h"
#include <esp_camera.h>

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

    httpd_register_uri_handler(server, &uri);
    httpd_register_uri_handler(server, &uri_stream);
    httpd_register_uri_handler(server, &uri_snapshot);

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

    }   
    
    // get the camera sensor
    // set the corresponding values
    // handle unknown commands and errors
    // send response
}