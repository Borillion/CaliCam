#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_https_server.h>

class WebServer {
  public:
    static esp_err_t init();
    static void stop();

  private:
    static httpd_handle_t server;
    static esp_err_t handle_index(httpd_req_t *req);
};

#endif
