#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#include <WiFi.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

#include "soc.h"
#include "utils.h"
#include "config.h"
#include "pinmap.h"
#include "adc_lut.h"
#include "settings.h"
#include "pushover.h"
#include "ats_server.h"
#include "MedianFilter.h"

void wifiLoop(void* parameter);

void resetErrors();
double readNTC(uint8_t id);
double readVoltage();
void resetCurrentError();
double readClampVRMS();
double dmap(double x, double in_min, double in_max, double out_min, double out_max);

#endif
