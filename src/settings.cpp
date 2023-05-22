#include "settings.h"

Settings settings;

void resetSettings() {
    settings.marker = EEPROM_MARKER;
    settings.clampNoise = CLAMP_NOISE_DEFAULT;
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
