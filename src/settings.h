#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

#include "config.h"

#define EEPROM_START 0

typedef struct {
    uint8_t marker;
    double clampNoise;
    double vBattCuttOff;
    double vBattRecovery;
    double socCuttOff;
    double socRecovery;
    double currentCuttOff;
    unsigned long currentCuttOffTimeOut;
    double tempFanOn;
    double tempCutOffHigh;
    double tempCutOffLow;
    double tempDanger;
} Settings;

extern Settings settings;

void resetSettings();
void loadSettings();
void saveSettings();

#endif
