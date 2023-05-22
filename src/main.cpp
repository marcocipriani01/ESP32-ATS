#include "main.h"

#define NTC_COUNT (sizeof(ntcPins) / sizeof(uint8_t))

const uint8_t ntcPins[] = NTC;
MedianFilter* ntcFilters = new MedianFilter[NTC_COUNT]{NTC_WINDOW, NTC_WINDOW, NTC_WINDOW, NTC_WINDOW};
double ntcAverage = 0.0;
double ntcVariation = 0.0;

MedianFilter vinFilter(VIN_DIVIDER_WINDOW);
MedianFilter clampFilter(CLAMP_WINDOW);

boolean warningSent = false;

volatile boolean enableSolarNtc = true, enableGridNtc = true;
volatile boolean atsSolarOn = true, atsGridOn = true;

unsigned long lastSample = 0L;

TaskHandle_t wifiTask;

void setup() {
    Serial.begin(BAUD_RATE);

    loadSettings();

    for (uint8_t i = 0; i < NTC_COUNT; i++) {
        pinMode(ntcPins[i], INPUT);
    }

    pinMode(CLAMP, INPUT);
    pinMode(VIN_DIVIDER, INPUT);

    pinMode(RELAY_FAN, OUTPUT);
    digitalWrite(RELAY_FAN, HIGH);
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

    xTaskCreatePinnedToCore(wifiLoop, "WiFiLoop", 8192, NULL, 10, &wifiTask, (xPortGetCoreID() == 0) ? 1 : 0);
}

void loop() {
    unsigned long t = millis();
    if ((t - lastSample) >= SAMPLING_INTERVAL) {
        double tempMin = 350.0, tempMax = -100.0;
        for (uint8_t i = 0; i < NTC_COUNT; i++) {
            double val = readNTC(i);
            if (val < tempMin) tempMin = val;
            if (val > tempMax) tempMax = val;
        }
        ntcAverage = (tempMin + tempMax) / 2.0;
        ntcVariation = (tempMax - tempMin) / 2.0;

        if (tempMax >= (settings.tempFanOn + (FAN_HISTERESIS / 2.0)))
            digitalWrite(RELAY_FAN, LOW);
        else if (tempMin <= (settings.tempFanOn - (FAN_HISTERESIS / 2.0)))
            digitalWrite(RELAY_FAN, HIGH);

        if (!warningSent) {
            if (tempMax >= settings.tempWarnHigh) {
                warningSent = true;
                //TODO: send high temp warning
            } else if (tempMin <= settings.tempWarnLow) {
                warningSent = true;
                //TODO: send low temp warning
            } else {
                warningSent = false;
            }
        }

        if (tempMax >= settings.tempCutOffHigh)
            enableSolarNtc = false;
        else if (tempMin <= settings.tempCutOffLow)
            enableSolarNtc = true;

        
        
        atsSolarOn = enableSolarNtc;
        pinMode(RELAY_ATS_SOLAR, OUTPUT);
        digitalWrite(RELAY_ATS_SOLAR, !atsSolarOn);
        atsGridOn = enableGridNtc;
        pinMode(RELAY_ATS_GRID, OUTPUT);
        digitalWrite(RELAY_ATS_GRID, !atsGridOn);
        lastSample = t;
    }
}

void wifiLoop(void* parameter) {
    Serial.println(">Starting Wi-Fi connection...");
    while (!AtsServer::connect()) {
        Serial.println(">WiFi connection failed, retrying...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    Serial.print(">IP Address: ");
    Serial.println(WiFi.localIP());

    AtsServer::server.on("/", HTTP_GET, AtsServer::root);
    AtsServer::server.onNotFound(AtsServer::notFound);
    AtsServer::server.begin();

    Serial.println(">Server is up.");

    while (1) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println(">WiFi connection failed, retrying...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            AtsServer::connect();
            Serial.print(">IP Address: ");
            Serial.println(WiFi.localIP());
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void serialEvent() {
    while (Serial.available()) {
        if (Serial.read() != ':') continue;
        switch (Serial.read()) {
            case 'C': {
                Serial.println(">Calibrating clamp...");
                double clampNoise = 0.0;
                const uint8_t samples = 10;
                for (uint8_t i = 0; i < samples; i++) {
                    clampNoise += readClampVRMS();
                }
                clampNoise /= samples;
                Serial.print(">Clamp noise: ");
                Serial.print(clampNoise * 1000.0, 5);
                Serial.println("mV");
                settings.clampNoise = clampNoise;
                saveSettings();
                break;
            }
        }
    }
}

double readNTC(uint8_t id) {
    double Vout = ADC_LUT[analogRead(ntcPins[id])] * VREF / ADC_MAX;
    double Rt = NTC_R1 * Vout / (VREF - Vout);
    double temp = ntcFilters[id].add(1.0 / (1.0 / NTC_T0 + log(Rt / NTC_R0) / NTC_B) - 273.15);
    return constrain(temp, -50.0, 300.0);
}

double readVoltage() {
    double vin = (ADC_LUT[analogRead(VIN_DIVIDER)] * VREF / ADC_MAX) / (VIN_DIVIDER_R2 / (VIN_DIVIDER_R1 + VIN_DIVIDER_R2));
    vin = vinFilter.add(VIN_REGRESSION_M * vin + VIN_REGRESSION_Q);
    return constrain(vin, 0.0, VIN_MAX);
} 

double readClampVRMS() {
    double maxValue = 0.0;
    double minValue = ADC_MAX;
    unsigned long start_time = millis();
    while ((millis() - start_time) < CLAMP_MEASURE_TIME) {
        double readValue = ADC_LUT[analogRead(CLAMP)];
        if (readValue > maxValue)
            maxValue = readValue;
        else if (readValue < minValue)
            minValue = readValue;
    }
    return (maxValue - minValue) * VREF / ADC_MAX;
}

double readCurrent() {
    return dmap(clampFilter.add(readClampVRMS()), settings.clampNoise, CLAMP_CALIBRATION_X, 0.0, CLAMP_CALIBRATION_Y);
}

double dmap(double x, double in_min, double in_max, double out_min, double out_max) {
    double val = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    if (val < out_min) return out_min;
    if (val > out_max) return out_max;
    return val;
}
