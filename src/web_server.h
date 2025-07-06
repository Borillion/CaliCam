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
    static esp_err_t handle_stream(httpd_req_t *req);
    static esp_err_t handle_snapshot(httpd_req_t *req);
    // static means that the function belongs to this class, rather than to any particular instance
    static esp_err_t handle_command(httpd_req_t *req);
    //Get camera status and settings as json
    static esp_err_t handle_status(httpd_req_t *req);
    //setclock
    static esp_err_t handle_xclk(httpd_req_t *req);
    //get reg
    static esp_err_t handle_getreg(httpd_req_t *req);
    //set reg
    static esp_err_t handle_setreg(httpd_req_t *req);
    //setpll
    static esp_err_t handle_setpll(httpd_req_t *req);
    //setres
    static esp_err_t handle_setresolution(http_req_t *req);
};

#endif
