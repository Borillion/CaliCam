#include "web_server.h"

httpd_handle_t WebServer::server = NULL;

// A unique string that separates individual JPEG frames in a multipart MIME stream
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

static const char* HTML = "<!DOCTYPE html><html><body>Hello, Streamer!</body></html>";

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

    httpd_register_uri_handler(server, &uri);

    return ESP_OK;
}

esp_err_t WebServer::handle_index(httpd_req_t *req) {
    return httpd_resp_send(req, HTML, strlen(HTML));
}

esp_err_t WebServer::handle_stream(httpd_req_t *req) {
    // To Do: is this really the best way? What about a websocket here?
    
    // configure the http headers to militpart mime stream
    // Get frame from camera
    // send header
    // send jpeg data
    // release frame
    // check if client disconnected

    //return res;
}