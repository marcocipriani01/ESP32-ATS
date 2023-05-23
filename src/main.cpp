#include "main.h"

#define NTC_COUNT (sizeof(ntcPins) / sizeof(uint8_t))

const uint8_t ntcPins[] = NTC;
MedianFilter* ntcFilters = new MedianFilter[NTC_COUNT]{NTC_WINDOW, NTC_WINDOW, NTC_WINDOW, NTC_WINDOW};
double ntcAverage = 0.0;
double ntcVariation = 0.0;
boolean fanOn = false;

MedianFilter vinFilter(VIN_DIVIDER_WINDOW);
MedianFilter clampFilter(CLAMP_WINDOW);
double soc = 0.0;
unsigned long clampErrorSince = 0L;

boolean warningSentNtc = false;

volatile boolean enableATS = true,
    enableATSNtc = true,
    enableATSVin = true,
    enableATSCurrent = true,
    enableATSSoc = true;

unsigned long lastSample = 0L;

TaskHandle_t wifiTask;

boolean printData = false;

void setup() {
    Serial.begin(BAUD_RATE);

    loadSettings();

    for (uint8_t i = 0; i < NTC_COUNT; i++) {
        pinMode(ntcPins[i], INPUT);
    }

    pinMode(CLAMP, INPUT);
    pinMode(VIN_DIVIDER, INPUT);

    pinMode(RELAY_ATS_GRID, OUTPUT);
    digitalWrite(RELAY_ATS_GRID, HIGH);
    pinMode(RELAY_ATS_SOLAR, OUTPUT);
    digitalWrite(RELAY_ATS_SOLAR, HIGH);
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

    Serial.println("INFO:\tESP32-ATS started.");
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
        if (printData)
            Serial.println("DATA:\tNTC average = " + String(ntcAverage) + "±" + String(ntcVariation) + "°C");

        if ((!fanOn) && (tempMax >= (settings.tempFanOn + (FAN_HISTERESIS / 2.0)))) {
            digitalWrite(RELAY_FAN, LOW);
            fanOn = true;
            Serial.println("INFO:\tFan turned on.");
        } else if (fanOn && (tempMin < (settings.tempFanOn - (FAN_HISTERESIS / 2.0)))) {
            digitalWrite(RELAY_FAN, HIGH);
            fanOn = false;
            Serial.println("INFO:\tFan turned off.");
        }

        if ((!warningSentNtc) && (tempMax >= settings.tempWarnHigh)) {
            //TODO: send high temp warning
            Serial.println("WARN:\tTemperature above threshold.");
            warningSentNtc = true;
        } else if (warningSentNtc && (tempMin < settings.tempWarnHigh)) {
            Serial.println("INFO:\tHigh temperature warning resolved.");
            warningSentNtc = false;
        } else if ((!warningSentNtc) && (tempMin <= settings.tempWarnLow)) {
            //TODO: send low temp warning
            Serial.println("WARN:\tTemperature below threshold.");
            warningSentNtc = true;
        } else if (warningSentNtc && (tempMax > settings.tempWarnLow)) {
            Serial.println("INFO:\tLow temperature warning resolved.");
            warningSentNtc = false;
        }

        if (enableATSNtc && (tempMax >= settings.tempCutOffHigh)) {
            Serial.println("ERR:\tTemperature above high cut-off.");
            enableATSNtc = false;
        } else if ((!enableATSNtc) && (tempMin < settings.tempCutOffHigh)) {
            Serial.println("INFO:\tTemperature error resolved.");
            enableATSNtc = true;
        } else if (enableATSNtc && (tempMin <= settings.tempCutOffLow)) {
            Serial.println("ERR:\tTemperature below low cut-off.");
            enableATSNtc = false;
        } else if ((!enableATSNtc) && (tempMax > settings.tempCutOffLow)) {
            Serial.println("INFO:\tTemperature error resolved.");
            enableATSNtc = true;
        }

        double vin = readVoltage();
        if (printData)
            Serial.println("DATA:\tVin = " + String(vin) + "V");
        if ((!enableATSVin) && (vin > settings.vBattRecovery)) {
            Serial.println("INFO:\tBattery voltage recovered.");
            enableATSVin = true;
        } else if (enableATSVin && (vin <= settings.vBattCuttOff)) {
            Serial.println("ERR:\tBattery voltage below cut-off.");
            enableATSVin = false;
        }

        soc = calcSoc(vin);
        if (printData)
            Serial.println("DATA:\tSoC = " + String(soc) + "%");
        if ((!enableATSSoc) && (soc > settings.socRecovery)) {
            Serial.println("INFO:\tBattery SoC recovered.");
            enableATSSoc = true;
        } else if (enableATSSoc && (soc <= settings.socCuttOff)) {
            Serial.println("ERR:\tBattery SoC below cut-off.");
            enableATSSoc = false;
        }

        double current = readCurrent();
        if (printData)
            Serial.println("DATA:\tCurrent = " + String(current) + "A");
        if (enableATSCurrent) {
            if (current >= settings.currentCuttOff) {
                if (clampErrorSince == 0L) {
                    clampErrorSince = t;
                } else if ((t - clampErrorSince) >= (2 * CLAMP_MEASURE_TIME)) {
                    enableATSCurrent = false;
                    clampErrorSince = 0L;
                }
            } else {
                clampErrorSince = 0L;
            }
        }
        
        enableATS = enableATSNtc && enableATSVin && enableATSSoc && enableATSCurrent;
        digitalWrite(RELAY_ATS_SOLAR, enableATS);
        if (enableATS) {
            digitalWrite(LED_GREEN, HIGH);
            digitalWrite(LED_RED, LOW);
        } else {
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(LED_RED, HIGH);
        }
        lastSample = t;
    }
}

void wifiLoop(void* parameter) {
    Serial.println("LOG:\tStarting Wi-Fi connection...");
    while (!AtsServer::connect()) {
        Serial.println("LOG:\tWiFi connection failed, retrying...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    Serial.print("LOG:\tIP Address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin(HOSTNAME)) {
        Serial.println("LOG:\tError setting up the MDNS responder!");
    } else {
        Serial.println("LOG:\tmDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }

    AtsServer::server.on("/", HTTP_GET, AtsServer::root);
    AtsServer::server.onNotFound(AtsServer::notFound);
    AtsServer::server.begin();

    Serial.println("LOG:\tServer is up.");

    while (1) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("LOG:\tWiFi connection failed, retrying...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            AtsServer::connect();
            Serial.print("LOG:\tIP Address: ");
            Serial.println(WiFi.localIP());
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void serialEvent() {
    while (Serial.available()) {
        if (Serial.read() != ':') continue;
        switch (Serial.read()) {
            case 'D': {
                printData = !printData;
                break;
            }

            case 'C': {
                Serial.println("LOG:\tCalibrating clamp...");
                double clampNoise = 0.0;
                const uint8_t samples = 10;
                for (uint8_t i = 0; i < samples; i++) {
                    clampNoise += readClampVRMS();
                }
                clampNoise /= samples;
                Serial.print("DATA:\tClamp noise: ");
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

void resetCurrentError() {
    clampErrorSince = 0L;
    enableATSCurrent = true;
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
