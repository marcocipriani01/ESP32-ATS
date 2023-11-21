#include "settings.h"

Settings settings;
Preferences preferences;

void prefsInit() {
    if (!preferences.begin("ats", false)) {
        Serial.println("WARN:\tNVS error, resetting.");
        nvs_flash_erase();
        nvs_flash_init();
        if (!preferences.begin("ats", false)) {
            while (1) {
                Serial.println("ERR:\tNVS error, halt.");
                delay(1000);
            }
        }
    }
}

void loadSettings() {
    prefsInit();
    settings.vBattCuttOff = preferences.getDouble("vBattCuttOff", 12.9);
    settings.vBattRecovery = preferences.getDouble("vBattRecovery", 13.2);
    settings.socCuttOff = preferences.getDouble("socCuttOff", 0.0);
    settings.socRecovery = preferences.getDouble("socRecovery", 1.0);
    settings.tempFanOn = preferences.getDouble("tempFanOn", 30.0 + (TEMP_HISTERESIS / 2.0));
    settings.tempCutOffHigh = preferences.getDouble("tempCutOffHigh", 40.0);
    settings.tempCutOffLow = preferences.getDouble("tempCutOffLow", 5.0);
    settings.tempDanger = preferences.getDouble("tempDanger", 50.0);
    settings.enableFireSensor = preferences.getBool("fireSensor", false);
    boolean save = false;
    if (preferences.getUInt("NVS_MARKER", 0) != NVS_MARKER)
        save = true;
    preferences.end();
    if (save) {
        Serial.println("INFO:\tSaving settings.");
        delay(100);
        saveSettings();
    }
}

void saveSettings() {
    prefsInit();
    Serial.println("INFO:\tSaving configuration.");
    preferences.putUInt("NVS_MARKER", NVS_MARKER);
    preferences.putDouble("vBattCuttOff", settings.vBattCuttOff);
    preferences.putDouble("vBattRecovery", settings.vBattRecovery);
    preferences.putDouble("socCuttOff", settings.socCuttOff);
    preferences.putDouble("socRecovery", settings.socRecovery);
    preferences.putDouble("tempFanOn", settings.tempFanOn);
    preferences.putDouble("tempCutOffHigh", settings.tempCutOffHigh);
    preferences.putDouble("tempCutOffLow", settings.tempCutOffLow);
    preferences.putDouble("tempDanger", settings.tempDanger);
    preferences.putBool("fireSensor", settings.enableFireSensor);
    preferences.end();
}
