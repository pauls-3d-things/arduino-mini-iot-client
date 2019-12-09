#include <Adafruit_APDS9960.h>
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

#define CODE_BME_NOT_FOUND 7
#define CODE_CCS_NOT_FOUND 6
#define CODE_APDS_NOT_FOUND 5
#define CODE_CCS_ERROR_READING 4
#define CODE_CCS_SKIPPING 3
#define CODE_CCS_WAITING 2
#define CODE_WIFI_WAITING 1

void ledCode(int num) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(250);
  for (uint8_t r = 0; r < 3; r++) {
    delay(500);
    for (uint8_t i = 0; i < num; i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
  }
}

void waitForWifi() {
  uint8_t tries = 0;
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);

  do {
    ledCode(CODE_WIFI_WAITING);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(4000);

    tries++;

    if (tries == 16) {
      // if the esp can't connect go back to sleep for a minute
      ESP.deepSleep(1 * 60 * 1000 * 1000);  // zzz 1 minutes
    }
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
Adafruit_APDS9960 apds;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  ledCode(3);

  rst_info* rstinfo = ESP.getResetInfoPtr();
  Serial.println(rstinfo->reason);

  while (!bme.begin()) {
    Serial.println("BME280 not found ...");
    ledCode(CODE_BME_NOT_FOUND);
    delay(1000);
  }

  uint8_t tries = 0;
  while (!ccs.begin(rstinfo->reason != REASON_DEEP_SLEEP_AWAKE)) {
    Serial.println("CCS811 not found ...");
    ledCode(CODE_CCS_NOT_FOUND);
    delay(1000);
    if (tries == 60) {
      ccs.begin(true);  // restart the device
      delay(1000);
    }
    tries++;
  }

  while (!apds.begin()) {
    Serial.println("APDS device not found");
    ledCode(CODE_APDS_NOT_FOUND);
    delay(1000);
  }
  apds.enableColor(true);
  apds.enableProximity(false);
}

void loop() {
  // values from BME280
  float temp(NAN), hum(NAN), pres(NAN);
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_hPa);
  bme.read(pres, temp, hum, tempUnit, presUnit);

  // values from CCS811
  float eCO2(NAN), tVOC(NAN), ccsTemp(NAN);
  boolean waitForData = true;
  while (waitForData) {
    // calibrate the device
    ccsTemp = ccs.calculateTemperature();
    ccs.setEnvironmentalData(hum, temp);

    if (ccs.available()) {
      uint8_t errCode = ccs.readData();
      if (errCode == 0) {
        eCO2 = ccs.geteCO2();
        tVOC = ccs.getTVOC();
        waitForData = false;
      } else {
        Serial.println("CCS811: error reading data " + String(errCode));
        ledCode(CODE_CCS_ERROR_READING);
      }

      if (eCO2 == 0 && tVOC == 0) {
        Serial.println("CCS811: Skipping zero values...");
        ledCode(CODE_CCS_SKIPPING);
        waitForData = true;
      }
    } else {
      Serial.println("CCS811: waiting");
        ledCode(CODE_CCS_WAITING);
    }
    delay(500);
  }

  // value form A0
  int analogValue = analogRead(A0);

  // values from APDS9660
  uint16_t rt, gt, bt, ct;
  apds.getColorData(&rt, &gt, &bt, &ct);

  String data = String(temp) + "," + String(hum) + "," + String(pres)                         // BME280 data
                + "," + String(ccsTemp) + "," + String(eCO2) + "," + String(tVOC)             // CCS811 data
                + "," + String(analogValue)                                                   // A0
                + "," + String(rt) + "," + String(gt) + "," + String(bt) + "," + String(ct);  // APDS9660
  Serial.println(data);

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Waiting for wifi ...");
    waitForWifi();
  }
  postData("sensors.csv", data, true, true);  // append to file, prefix timestamp

  Serial.println("entering deep sleep");
  ESP.deepSleep(5 * 60 * 1000 * 1000);  // zzz 5 minutes
}