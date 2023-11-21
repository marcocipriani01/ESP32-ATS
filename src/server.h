#ifndef ATS_SERVER_H
#define ATS_SERVER_H

#include "config.h"
#if SERVER_PORT == 443
#define ASYNC_TCP_SSL_ENABLED 1
#include "ssl_cert.h"
#endif

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

extern AsyncWebServer server;

boolean wifiConnect();

void notFoundPage(AsyncWebServerRequest *request);

#endif
