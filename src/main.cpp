#include <Arduino.h>
#include <WiFi.h>
#include "camera_hal.h"
#include "web_server.h"
#include "wifi_config.h"


// put function declarations here:

void setup() {
  // put your setup code here, to run once:

  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("WiFi connection starting...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  
  esp_err_t esp_err = CameraHal::init();
  if (esp_err != ESP_OK) {
    Serial.printf("CameraHALInit failed with error 0x%x", esp_err);
    return;
  }

  if (WebServer::init() != ESP_OK) {
    Serial.println("Web server init failed");
    return;
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  // Do nothing. Everything is done in another task by the web server
  delay(10000);
}