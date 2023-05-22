#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#include "config.h"
#include "pinmap.h"
#include "adc_lut.h"
#include "settings.h"
#include "MedianFilter.h"

double readNTC(uint8_t id);
double readVoltage();
double readClampVRMS();
double readCurrent();
double dmap(double x, double in_min, double in_max, double out_min, double out_max);

#endif
