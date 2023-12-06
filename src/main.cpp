#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
// #include "DigitLedDisplay.h"

/* Arduino Pin to Display Pin
   7 to DIN,
   6 to CS,
   5 to CLK */
// #define DIN 2
// #define CS 5
// #define CLK 4 
// DigitLedDisplay ld = DigitLedDisplay(DIN, CS, CLK);

#define BUZZER_PIN 20 // Buzzer pin
#define CLICK_DURATION 20 // Click duration in ms

#define TACTILE_SWITCH_PIN 10 // Tactile switch pin


/**
 * @brief GET data from an LNbits endpoint
 * 
 * @param endpointUrl 
 * @return String 
 */
String getEndpointData(String url) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      return payload;
    } else {
      Serial.println("Error in HTTP request");
    }
    http.end();
  } else {
    Serial.println("Not connected to WiFi");
  }
  return "";
}

void initWiFi() {
  // WiFiManager for auto-connecting to Wi-Fi
  WiFiManager wifiManager;
  wifiManager.autoConnect("AutoConnectAP");

  Serial.println("Connected to WiFi");
}


void displayMempoolFees() {
  uint16_t pagingDelay = 2000;
  DynamicJsonDocument doc(200);
  String line = getEndpointData("https://mempool.space/api/v1/fees/recommended");
  Serial.println("line is");
  Serial.println(line);
}


void setup() {
  delay(5000);
  Serial.begin(115200);
  Serial.println("Booted up");

  initWiFi();

  delay(2000);
}

void loop() {

  displayMempoolFees();

}