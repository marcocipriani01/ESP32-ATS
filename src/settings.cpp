#include "settings.h"

Settings settings;

void resetSettings() {
    settings.marker = EEPROM_MARKER;
    settings.clampNoise = CLAMP_NOISE_DEFAULT;
    settings.vBattCuttOff = 10.25;
    settings.vBattRecovery = 10.5;
    settings.socCuttOff = 20.0;
    settings.socRecovery = 30.0;
    settings.currentCuttOff = 0.9 * 600.0 / 230.0;
    settings.currentCuttOffTimeOut = 5 * 1000L;
    settings.tempFanOn = 30.0;
    settings.tempWarnHigh = 36.0;
    settings.tempWarnLow = 34.0;
    settings.tempCutOffHigh = 37.0;
    settings.tempCutOffLow = 35.0;
}

void loadSettings() {
#ifdef ESP32
    EEPROM.begin(sizeof(Settings) + 1);
#endif
    EEPROM.get(EEPROM_START, settings);
    if (settings.marker != EEPROM_MARKER) resetSettings();
}

void saveSettings() {
    EEPROM.put(EEPROM_START, settings);
}
