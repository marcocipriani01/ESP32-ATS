#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

#include "config.h"

#define EEPROM_START 0
#define EEPROM_MARKER 0x0F

typedef struct {
    uint8_t marker;
    double clampNoise;
} Settings;

extern Settings settings;

void resetSettings();
void loadSettings();
void saveSettings();

#endif
