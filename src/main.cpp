#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <MAX30105.h>
#include <Wire.h>
#include <heartRate.h>

#include "config.h"
// or define these
// #define WIFI_SSID "<YOUR-SSID>"
// #define WIFI_PASS "<YOUR-PASS>"
// #define HOSTNAME "your-device-01"
// #define MINI_IOT_SERVER "<your-mini-iot-host>"

#define REPORTING_PERIOD_MS 3 * 1000
unsigned long tsLastReport = 0;

MAX30105 sensor;

const byte RATE_SIZE = 4;  // Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE];     // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  // Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;

void waitForWifi() {
  Serial.println("Starting wifi...");

  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  do {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("Waiting for wifi...");
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
  Serial.begin(115200);
  waitForWifi();

  while (!sensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("ERROR: Failed to initialize pulse oximeter");
    delay(1000);
  }

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  sensor.setup();                     // Configure sensor with default settings
  sensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate sensor is running
  sensor.setPulseAmplitudeGreen(0);   // Turn off Green LED
}

void loop() {
  long irValue = sensor.getIR();

  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  Serial.print("IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(beatsPerMinute);
  Serial.print(", Avg BPM=");
  Serial.print(beatAvg);

  if (irValue < 50000) {
    Serial.print(" No finger?");
  } else {
    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
      tsLastReport = millis();

      String data = String(irValue) + "," + String(beatsPerMinute) + "," + String(beatAvg);
      Serial.println(data);
      postData("sensor.csv", data, true, true);
    }
  }

  Serial.println();
}