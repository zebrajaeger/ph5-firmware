//credentials.default.h
#pragma once

#include <ESPAsync_WiFiManager_Lite.h>

bool LOAD_DEFAULT_CONFIG_DATA = false;
ESP_WM_LITE_Configuration defaultConfig =
{
  "ESP32_Async",
  "SSID1",  "password1",
  "SSID2",  "password2",
  "ESP32_Async-Control",
  0
};