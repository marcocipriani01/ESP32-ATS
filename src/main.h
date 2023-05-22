#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#include "config.h"
#include "pinmap.h"
#include "adc_lut.h"

double readNtc(uint8_t pin);

#endif
