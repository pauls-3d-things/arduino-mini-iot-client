#include <Adafruit_CCS811.h>
#include <Arduino.h>
#include <BME280I2C.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <SPI.h>  // avoid BME280Spi.cpp:34:17: fatal error: SPI.h: No such file or directory

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

BME280I2C::Settings settings(BME280::OSR_X1, BME280::OSR_X1, BME280::OSR_X1, BME280::Mode_Forced,
                             BME280::StandbyTime_1000ms, BME280::Filter_Off, BME280::SpiEnable_False,
                             BME280I2C::I2CAddr_0x76);
BME280I2C bme(settings);

Adafruit_CCS811 ccs;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  while (!bme.begin()) {
    Serial.println("BME280 not found ...");
    delay(1000);
  }
  if (!ccs.begin()) {
    Serial.println("CCS811 not found ...");
    delay(1000);
  }
}

void loop() {
  // values from BME280
  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  bme.read(pres, temp, hum, tempUnit, presUnit);

  // value from CCS811
  float eCO2(NAN), tVOC(NAN), ccsTemp(NAN);
  boolean waitForData = true;
  while (waitForData) {
    if (ccs.available()) {
      // calibrate the device
      ccsTemp = ccs.calculateTemperature();
      ccs.setTempOffset(ccsTemp - temp);

      uint8_t errCode = ccs.readData();
      if (errCode == 0) {
        eCO2 = ccs.geteCO2();
        tVOC = ccs.getTVOC();
        waitForData = false;
      } else {
        Serial.println("Error reading from CCS811... " + String(errCode));
      }
    } else {
      Serial.println("Waiting for CCS811...");
    }
    delay(500);
  }

  String data = String(temp) + "," + String(hum) + "," + String(pres)               // BME280 data
                + "," + String(ccsTemp) + "," + String(eCO2) + "," + String(tVOC);  // CCS811 data
  Serial.println(data);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Waiting for wifi ...");
    waitForWifi();
  }
  postData("sensors.csv", data, true, true);  // append to file, prefix timestamp

  delay(1000 * 60 * 5); // wait for 5 minutes
}