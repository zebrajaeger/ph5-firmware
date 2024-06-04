#pragma once
#include <Arduino.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

char appName[] = TOSTRING(APP_NAME);
char appVersion[] = TOSTRING(APP_VERSION);
char buildTime[] = TOSTRING(BUILD_TIME);
