#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include <nvs_flash.h>

#include "config.h"

typedef struct {
    double vBattCuttOff;
    double vBattRecovery;
    double socCuttOff;
    double socRecovery;
    double tempFanOn;
    double tempCutOffHigh;
    double tempCutOffLow;
    double tempDanger;
    boolean enableFireSensor;
} Settings;

extern Preferences preferences;
extern Settings settings;

void prefsInit();
void loadSettings();
void saveSettings();

#endif
