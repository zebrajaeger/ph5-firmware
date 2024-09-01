#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastAccelStepper.h>

class JsonStatus {
 public:
  static void convert(FastAccelStepper* stepperX, FastAccelStepper* stepperY, String& result) {
    JsonDocument doc;

    if (stepperX) {
      JsonObject x = doc["x"].to<JsonObject>();
      x["pos"] = stepperX->getCurrentPosition();
      x["speed"] = stepperX->getCurrentSpeedInMilliHz(false);
      x["running"] = stepperX->isRunning();
    }

    if (stepperY) {
      JsonObject y = doc["y"].to<JsonObject>();
      y["pos"] = stepperY->getCurrentPosition();
      y["speed"] = stepperY->getCurrentSpeedInMilliHz(false);
      y["running"] = stepperY->isRunning();
    }

    serializeJson(doc, result);
  }
};