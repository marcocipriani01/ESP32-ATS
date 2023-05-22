#include "main.h"

#define NTC_COUNT (sizeof(ntcPins) / sizeof(uint8_t))

uint8_t ntcPins[] = NTC;
MedianFilter* ntcFilters = new MedianFilter[NTC_COUNT] {NTC_WINDOW, NTC_WINDOW, NTC_WINDOW, NTC_WINDOW};

MedianFilter vinFilter(VIN_DIVIDER_WINDOW);
MedianFilter clampFilter(CLAMP_WINDOW);

void setup() {
    Serial.begin(BAUD_RATE);

    for (uint8_t i = 0; i < NTC_COUNT; i++) {
        pinMode(ntcPins[i], INPUT);
    }
    
    pinMode(CLAMP, INPUT);
    pinMode(VIN_DIVIDER, INPUT);

    pinMode(RELAY_FAN, OUTPUT);
    digitalWrite(RELAY_FAN, HIGH);
    pinMode(RELAY_ATS_SOLAR, OUTPUT);
    digitalWrite(RELAY_ATS_SOLAR, HIGH);
    pinMode(RELAY_ATS_GRID, OUTPUT);
    digitalWrite(RELAY_ATS_GRID, HIGH);
    pinMode(RELAY_AUX, OUTPUT);
    digitalWrite(RELAY_AUX, HIGH);

    pinMode(ATS_SENSE_SOLAR, INPUT);
    pinMode(ATS_SENSE_GRID, INPUT);

    pinMode(FIRE_SENS0, INPUT);
    pinMode(FIRE_SENS1, INPUT);

    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, LOW);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    analogReadResolution(ADC_RESOLUTION);
}

void loop() {
    Serial.println(readCurrent());
    delay(100);
}

double readNTC(uint8_t id) {
    double Vout = ADC_LUT[analogRead(ntcPins[id])] * VREF / ADC_MAX;
    double Rt = NTC_R1 * Vout / (VREF - Vout);
    return ntcFilters[id].add(1.0 / (1.0 / NTC_T0 + log(Rt / NTC_R0) / NTC_B) - 273.15);
}

double readVoltage() {
    double vin = (ADC_LUT[analogRead(VIN_DIVIDER)] * VREF / ADC_MAX) / (VIN_DIVIDER_R2 / (VIN_DIVIDER_R1 + VIN_DIVIDER_R2));
    vin = vinFilter.add(VIN_REGRESSION_M * vin + VIN_REGRESSION_Q);
    return constrain(vin, 0.0, VIN_MAX);
}

double readCurrent() {
    return clampFilter.add(calcIrms());
}

// Taken from the EmonLib library
// https://github.com/openenergymonitor/EmonLib
// by OpenEnergyMonitor
// GNU Affero General Public License v3.0
double calcIrms() {
    double sumI = 0.0, offsetI = ADC_MAX / 2.0;
    for (unsigned int n = 0; n < CLAMP_SAMPLES; n++) {
        double sampleI = ADC_LUT[analogRead(CLAMP)];
        // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset,
        // then subtract this - signal is now centered on 0 counts.
        offsetI = (offsetI + (sampleI - offsetI) / ADC_MAX);
        double filteredI = sampleI - offsetI;
        // Root-mean-square method current
        // 1) Square current values
        // 2) Sum
        sumI += (filteredI * filteredI);
    }
    return CLAMP_A_1V * ((VREF / 1000.0) / (ADC_COUNTS)) * sqrt(sumI / CLAMP_SAMPLES);
}
