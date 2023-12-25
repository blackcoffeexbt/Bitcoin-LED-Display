#pragma once

#include "structs.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>

extern DisplayData displayData;
  
// json doc for settings
StaticJsonDocument<200> settingsDoc;

void saveSetting(String setting, String value) {
  // set the value in the json doc
  settingsDoc[setting] = value;
  // save to SPIFFS
  File settingsFile = SPIFFS.open("/settings.json", "w");
  if (!settingsFile) {
    Serial.println("Failed to open settings file for writing");
    return;
  }
  // serial print
  Serial.println("Saving settings to file");
  serializeJson(settingsDoc, Serial);

  serializeJson(settingsDoc, settingsFile);
  settingsFile.close();
}

// load settings from SPIFFS (settings.json)
void loadSettings() {
  File settingsFile = SPIFFS.open("/settings.json", "r");
  if (!settingsFile) {
    Serial.println("Failed to open settings file");
    return;
  }
  size_t size = settingsFile.size();
  if (size > 1024) {
    Serial.println("Settings file size is too large");
    return;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  settingsFile.readBytes(buf.get(), size);
  DeserializationError error = deserializeJson(settingsDoc, buf.get());
  if (error) {
    Serial.println("Failed to parse settings file");
    return;
  }
  int screen_number = settingsDoc["screen_number"];
  // set displayData by enum int
  displayData = static_cast<DisplayData>(screen_number);
}