#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "config.h"
// or define these
// #define WIFI_SSID "<YOUR-SSID>"
// #define WIFI_PASS "<YOUR-PASS>"
// #define HOSTNAME "your-device-01"
// #define MINI_IOT_SERVER "<your-mini-iot-host>"

void waitForWifi() {
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  do {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(4000);
  } while (WiFi.status() != WL_CONNECTED);
}

void postData(String filename, String payload, boolean append, boolean tsprefix) {
  HTTPClient http;
  http.begin(String("http://") + MINI_IOT_SERVER         //
             + "/files/" + HOSTNAME                      //
             + "/" + filename                            //
             + "?append=" + (append ? "true" : "false")  //
             + "&tsprefix=" + (tsprefix ? "true" : "false"));
  http.addHeader("Content-Type", "text/plain");
  http.POST(payload);
  // http.writeToStream(&Serial);
  http.end();
}

void setup() {
  waitForWifi();

  postData("test.csv", "it works", true, true);

  ESP.deepSleep(5 * 60 * 1000 * 1000);  // sleep for 5 minutes
}

void loop() {}