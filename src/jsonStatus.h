#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastAccelStepper.h>

#include "camera.h"

class JsonStatus {
 public:
  static void convert(FastAccelStepper* stepperX, FastAccelStepper* stepperY, Camera& camera, String& result) {
    JsonDocument doc;
    doc["t"] = millis();

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

    JsonObject c = doc["c"].to<JsonObject>();
    c["focus"] = camera.isFocusing();
    c["trigger"] = camera.isTriggering();

    serializeJson(doc, result);
    doc.shrinkToFit();
  }
};