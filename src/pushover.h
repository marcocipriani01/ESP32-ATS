#ifndef PUSHOVER_H
#define PUSHOVER_H

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#include "config.h"

extern const char* PUSHOVER_ROOT_CA;

int sendPushover(String title, String message);

#endif
