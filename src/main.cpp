#include "main.h"

void setup() {
    Serial.begin(BAUD_RATE);

    pinMode(NTC0, INPUT);
    pinMode(NTC1, INPUT);
    pinMode(NTC2, INPUT);
    pinMode(NTC3, INPUT);
    
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
    Serial.print("NTC0: ");
    Serial.print(readNtc(NTC0));
    Serial.print(", NTC1: ");
    Serial.print(readNtc(NTC1));
    Serial.print(", NTC2: ");
    Serial.print(readNtc(NTC2));
    Serial.print(", NTC3: ");
    Serial.println(readNtc(NTC3));
}

double readNtc(uint8_t pin) {
    double Vout = ADC_LUT[analogRead(pin)] * NTC_VS / ADC_MAX;
    double Rt = NTC_R1 * Vout / (NTC_VS - Vout);
    return 1.0 / (1.0 / NTC_T0 + log(Rt / NTC_R0) / NTC_B) - 273.15;
}
