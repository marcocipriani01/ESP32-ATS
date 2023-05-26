#include "main.h"

#define NTC_COUNT (sizeof(ntcPins) / sizeof(uint8_t))

const uint8_t ntcPins[] = NTC;
MedianFilter* ntcFilters = new MedianFilter[NTC_COUNT]{NTC_WINDOW, NTC_WINDOW, NTC_WINDOW, NTC_WINDOW};
volatile double ntcAverage = 0.0;
volatile double ntcVariation = 0.0;
volatile boolean fanOnNtc = false,
    fanOnFire = false;

MedianFilter vinFilter(VIN_DIVIDER_WINDOW);
MedianFilter clampFilter(CLAMP_WINDOW);
volatile double soc = 0.0;
unsigned long clampErrorSince = 0L;

volatile boolean enableATS = true,
    enableATSNtcHigh = true,
    enableATSNtcLow = true,
    enableATSNtcDanger = true,
    enableATSVin = true,
    enableATSCurrent = true,
    enableATSSoc = true,
    enableATSFire = true,
    enableATSRelayMatch = true;

unsigned long lastSample = 0L;

TaskHandle_t wifiTask;

boolean printData = false;

volatile boolean atsSolarDetected = false,
    gridPowerLoss = false;

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
            if (printData)
                Serial.println("DATA:\tNTC" + String(i) + " = " + String(val) + "°C");
            if (val < tempMin) tempMin = val;
            if (val > tempMax) tempMax = val;
        }
        ntcAverage = (tempMin + tempMax) / 2.0;
        ntcVariation = (tempMax - tempMin) / 2.0;
        if (printData)
            Serial.println("DATA:\tNTC average = " + String(ntcAverage) + "±" + String(ntcVariation) + "°C");

        if ((!fanOnNtc) && (tempMax >= (settings.tempFanOn + (TEMP_HISTERESIS / 2.0)))) {
            fanOnNtc = true;
            Serial.println("INFO:\tFan turned on.");
        } else if (fanOnNtc && (tempMax < (settings.tempFanOn - (TEMP_HISTERESIS / 2.0)))) {
            fanOnNtc = false;
            Serial.println("INFO:\tFan turned off.");
        }

        if (enableATSNtcHigh && (tempMax >= (settings.tempCutOffHigh + (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("ERR:\tTemperature above high cut-off.");
            sendPushover("Monitoraggio temperaura ATS",
                "Temperatura troppo alta, sistema disabilitato.");
            enableATSNtcHigh = false;
        } else if ((!enableATSNtcHigh) && (tempMax < (settings.tempCutOffHigh - (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("INFO:\tTemperature error resolved.");
            enableATSNtcHigh = true;
        }
        if (enableATSNtcDanger && (tempMax >= (settings.tempDanger + (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("ERR:\tTemperature above high cut-off.");
            sendPushover("Monitoraggio temperaura ATS",
                "Temperatura troppo alta, sistema disabilitato. Riabilitare manualmente nell'interfaccia web.");
            enableATSNtcDanger = false;
        }
        if (enableATSNtcLow && (tempMin <= (settings.tempCutOffLow - (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("ERR:\tTemperature below low cut-off.");
            sendPushover("Monitoraggio temperaura ATS",
                "Temperatura troppo bassa, sistema disabilitato.");
            enableATSNtcLow = false;
        } else if ((!enableATSNtcLow) && (tempMin > (settings.tempCutOffLow + (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("INFO:\tTemperature error resolved.");
            enableATSNtcLow = true;
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
                    sendPushover("Monitoraggio potenza ATS",
                        "Corrente troppo alta, sistema disabilitato. Riabilitare manualmente nell'interfaccia web.");
                    enableATSCurrent = false;
                    clampErrorSince = 0L;
                }
            } else {
                clampErrorSince = 0L;
            }
        }

        boolean fireDetected = !digitalRead(FIRE_SENS0);
        if (printData)
            Serial.println("DATA:\tFire detected = " + String(fireDetected));
        if (enableATSFire && fireDetected) {
            sendPushover("Monitoraggio temperatura ATS",
                "Rilevato incendio, sistema disabilitato. Riabilitare manualmente nell'interfaccia web.");
            Serial.println("ERR:\tFire detected!");
            enableATSFire = false;
            fanOnFire = true;
        }

        atsSolarDetected = enableATS;
        //atsSolarDetected = !digitalRead(ATS_SENSE_SOLAR);
        gridPowerLoss = digitalRead(ATS_SENSE_GRID);
        if (printData) {
            Serial.println("DATA:\tATS solar output sense = " + String(atsSolarDetected));
            Serial.println("DATA:\tGrid power loss = " + String(gridPowerLoss));
        }
        if (enableATSRelayMatch && (enableATS != atsSolarDetected)) {
            Serial.println("ERR:\tATS relay problem: detected status doesn't match expected status.");
            sendPushover("Monitoraggio potenza ATS",
                    "Problema al relè, sistema disabilitato. Riabilitare manualmente nell'interfaccia web.");
            enableATSRelayMatch = false;
        }
        
        enableATS = enableATSNtcHigh && enableATSNtcLow && enableATSNtcDanger &&
            enableATSVin && enableATSSoc && enableATSCurrent && enableATSFire && enableATSRelayMatch;
        digitalWrite(RELAY_ATS_SOLAR, enableATS);
        digitalWrite(RELAY_FAN, !(fanOnNtc || fanOnFire));
        digitalWrite(LED_GREEN, enableATS);
        digitalWrite(LED_RED, !enableATS);
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

            case 'P': {
                int code = sendPushover("ESP32-ATS", "Messaggio di test!");
                Serial.println("LOG:\tPushover test message sent, response code: " + String(code));
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
