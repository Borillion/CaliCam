#include <Arduino.h>
#include <WiFi.h>

// put function declarations here:

void setup() {
  // put your setup code here, to run once:

  // initialize LED digital pin as an output.
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  const char *ssid = "**********";
  const char *password = "**********";

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");

    while (WiFi.status() == WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, HIGH); // turn the LED on
      delay(500);             // wait for 500 milliseconds
      digitalWrite(LED_BUILTIN, LOW);  // turn the LED off
      delay(500);             // wait for 500 milliseconds
    }


}

void loop() {
  // put your main code here, to run repeatedly:
  // Do nothing. Everything is done in another task by the web server
  delay(10000);
}