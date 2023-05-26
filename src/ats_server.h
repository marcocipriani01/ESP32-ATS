#ifndef ATS_SERVER_H
#define ATS_SERVER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

#include "config.h"

namespace ATSServer {
extern AsyncWebServer server;

boolean connect();

void notFound(AsyncWebServerRequest *request);
}  // namespace ATSServer

#endif
