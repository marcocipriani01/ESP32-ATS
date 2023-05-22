#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <EmonLib.h>

#include "config.h"
#include "pinmap.h"
#include "adc_lut.h"
#include "MedianFilter.h"

double readNTC(uint8_t id);
double readVoltage();
double readCurrent();
double calcIrms();

#endif
