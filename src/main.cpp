#include "main.h"

#define NTC_COUNT (sizeof(ntcPins) / sizeof(uint8_t))

const uint8_t ntcPins[] = NTC;
MedianFilter* ntcFilters = new MedianFilter[NTC_COUNT]{NTC_WINDOW, NTC_WINDOW, NTC_WINDOW, NTC_WINDOW};
volatile double ntcAverage = 0.0;
volatile double ntcVariation = 0.0;
volatile boolean fanOnNtc = false, fanOnFire = false;

MedianFilter vinFilter(VIN_DIVIDER_WINDOW);
volatile double vin = 0.0;
volatile double soc = 0.0;
double lastVin = 0.0;
volatile double vindt = 0.0;
unsigned long lastVindtMeasure = 0L;

volatile boolean enableATS = false, enableATSNtcHigh = false, enableATSNtcLow = false,
    enableATSNtcDanger = true, enableATSVin = false, enableATSSoc = false, enableATSFire = true;

volatile unsigned long atsActiveSince = 0L, atsOffSince = 0L;

unsigned long lastSample = 0L;

TaskHandle_t wifiTask;

boolean printData = false;

void setup() {
    Serial.begin(BAUD_RATE);
    Serial.println("INFO:\tSettings init...");

    loadSettings();

    Serial.println("INFO:\tGPIO init...");
    for (uint8_t i = 0; i < NTC_COUNT; i++) {
        pinMode(ntcPins[i], INPUT);
    }

    pinMode(VIN_DIVIDER, INPUT);

    pinMode(RELAY_ATS_GRID, OUTPUT);
    digitalWrite(RELAY_ATS_GRID, HIGH);
    pinMode(RELAY_ATS_SOLAR, OUTPUT);
    digitalWrite(RELAY_ATS_SOLAR, HIGH);
    pinMode(RELAY_FAN, OUTPUT);
    digitalWrite(RELAY_FAN, HIGH);
    pinMode(RELAY_AUX, OUTPUT);
    digitalWrite(RELAY_AUX, HIGH);

    pinMode(FIRE_SENS0, INPUT);
    pinMode(FIRE_SENS1, INPUT_PULLUP);

    pinMode(LED_RED, OUTPUT);
    digitalWrite(LED_RED, LOW);
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, LOW);

    analogReadResolution(ADC_RESOLUTION);

    Serial.println("INFO:\tSPIFFS init...");
    if (!SPIFFS.begin(true)) {
        while (1) {
            Serial.println("ERR:\tAn Error has occurred while mounting SPIFFS.");
            delay(1000);
        }
    }

    xTaskCreatePinnedToCore(wifiLoop, "WiFiLoop", 8192, NULL, 10, &wifiTask, (xPortGetCoreID() == 0) ? 1 : 0);

    Serial.println("INFO:\tESP32-ATS started.");
}

void loop() {
    unsigned long t = millis();
    if ((t - lastSample) >= SAMPLING_INTERVAL) {
        double tempMin = 350.0, tempMax = -100.0;
        for (uint8_t i = 0; i < NTC_COUNT; i++) {
            double val = readNTC(i);
            if (printData) Serial.println("DATA:\tNTC" + String(i) + " = " + String(val) + "°C");
            if (val < tempMin) tempMin = val;
            if (val > tempMax) tempMax = val;
        }
        ntcAverage = (tempMin + tempMax) / 2.0;
        ntcVariation = (tempMax - tempMin) / 2.0;
        if (printData) Serial.println("DATA:\tNTC average = " + String(ntcAverage) + "±" + String(ntcVariation) + "°C");

        if ((!fanOnNtc) && (tempMax >= (settings.tempFanOn + (TEMP_HISTERESIS / 2.0)))) {
            fanOnNtc = true;
            Serial.println("INFO:\tFan turned on.");
        } else if (fanOnNtc && (tempMax < (settings.tempFanOn - (TEMP_HISTERESIS / 2.0)))) {
            fanOnNtc = false;
            Serial.println("INFO:\tFan turned off.");
        }

        if (enableATSNtcHigh && (tempMax >= (settings.tempCutOffHigh + (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("ERR:\tTemperature above high cut-off.");
            sendPushover("Monitoraggio temperaura ATS", "Temperatura troppo alta, sistema disabilitato.");
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
            sendPushover("Monitoraggio temperaura ATS", "Temperatura troppo bassa, sistema disabilitato.");
            enableATSNtcLow = false;
        } else if ((!enableATSNtcLow) && (tempMin > (settings.tempCutOffLow + (TEMP_HISTERESIS / 2.0)))) {
            Serial.println("INFO:\tTemperature error resolved.");
            enableATSNtcLow = true;
        }

        double vinTemp = (ADC_LUT[analogRead(VIN_DIVIDER)] * VREF / ADC_MAX) / (VIN_DIVIDER_R2 / (VIN_DIVIDER_R1 + VIN_DIVIDER_R2));
        vinTemp = constrain(vinTemp, 0.0, VIN_MAX);
        vinTemp = vinFilter.add(VIN_REGRESSION_M * vinTemp + VIN_REGRESSION_Q);
        if (vinFilter.isReady()) {
            vin = vinTemp;
            if (printData) Serial.println("DATA:\tVin = " + String(vin) + "V");
            if ((!enableATSVin) && (vin > settings.vBattRecovery)) {
                Serial.println("INFO:\tBattery voltage recovered.");
                enableATSVin = true;
            } else if (enableATSVin && (vin <= settings.vBattCuttOff)) {
                Serial.println("ERR:\tBattery voltage below cut-off.");
                enableATSVin = false;
            }
            soc = calcSoc(vin);
            if (printData) Serial.println("DATA:\tSoC = " + String(soc) + "%");
            if ((!enableATSSoc) && (soc > settings.socRecovery)) {
                Serial.println("INFO:\tBattery SoC recovered.");
                enableATSSoc = true;
            } else if (enableATSSoc && (soc <= settings.socCuttOff)) {
                Serial.println("ERR:\tBattery SoC below cut-off.");
                enableATSSoc = false;
            }
            if (lastVindtMeasure == 0L) {
                lastVindtMeasure = t;
                lastVin = vin;
            } else if ((t - lastVindtMeasure) >= VIN_dt_INTERVAL) {
                vindt = (3.6e+6) * (vin - lastVin) / ((double)(t - lastVindtMeasure));
                lastVin = vin;
                lastVindtMeasure = t;
            }
        }

        if (settings.enableFireSensor) {
            boolean fireDetected = !digitalRead(FIRE_SENS0);
            if (printData) Serial.println("DATA:\tFire detected = " + String(fireDetected));
            if (enableATSFire && fireDetected) {
                sendPushover("Monitoraggio temperatura ATS", "Rilevato incendio, sistema disabilitato. Riabilitare manualmente nell'interfaccia web.");
                Serial.println("ERR:\tFire detected!");
                enableATSFire = false;
                fanOnFire = true;
            }
        } else {
            enableATSFire = true;
            fanOnFire = false;
        }

        boolean newATSVal =
            enableATSNtcHigh && enableATSNtcLow && enableATSNtcDanger && enableATSVin && enableATSSoc && enableATSFire;
        if (newATSVal && (!enableATS)) {
            sendPushover("ESP32-ATS", "Relè attivato, osservatorio alimentato a batterie.");
            atsActiveSince = t;
            atsOffSince = 0L;
        } else if ((!newATSVal) && enableATS) {
            sendPushover("ESP32-ATS", "Relè disattivato, osservatorio alimentato da rete.");
            atsOffSince = t;
            atsActiveSince = 0L;
        }
        enableATS = newATSVal;
        digitalWrite(RELAY_ATS_SOLAR, !enableATS);
        digitalWrite(RELAY_FAN, !(fanOnNtc || fanOnFire));
        digitalWrite(LED_GREEN, enableATS);
        digitalWrite(LED_RED, !enableATS);
        lastSample = t;
    }
}

void wifiLoop(void* parameter) {
    delay(500);
    Serial.println("INFO:\tStarting Wi-Fi connection...");
    while (!wifiConnect()) {
        Serial.println("INFO:\tWiFi connection failed, retrying...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    Serial.print("INFO:\tIP Address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin(HOSTNAME)) {
        Serial.println("INFO:\tError setting up the MDNS responder!");
    } else {
        Serial.println("INFO:\tmDNS responder started");
        MDNS.addService("http", "tcp", 80);
    }

    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html").setAuthentication(USER, USER_PASSWORD);
    server.onNotFound(notFoundPage);
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "content-type");

    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        DynamicJsonDocument json(1024);
        unsigned long t = millis();
        json["ntc_avg"] = ntcAverage;
        json["ntc_var"] = ntcVariation;
        json["fan"] = (boolean)(fanOnNtc || fanOnFire);
        json["vin"] = vin;
        json["vin_led"] = enableATSVin;
        json["soc"] = soc;
        json["relay_state"] = enableATS;
        json["ntc_ok"] = (boolean)(enableATSNtcHigh && enableATSNtcLow && enableATSNtcDanger);
        json["soc_led"] = enableATSSoc;
        json["flame"] = enableATSFire;
        json["ip"] = WiFi.localIP().toString();
        json["uptime"] = millis() / 1000L;
        if (enableATS) {
            json["solar_uptime"] = (t - atsActiveSince) / 1000L;
            json["grid_uptime"] = -1;
        } else {
            json["solar_uptime"] = -1;
            json["grid_uptime"] = (t - atsOffSince) / 1000L;
        }
        json["vindt"] = vindt;
        serializeJson(json, *response);
        request->send(response);
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncResponseStream* response = request->beginResponseStream("application/json");
        DynamicJsonDocument json(1024);
        json["vBattCuttOff"] = settings.vBattCuttOff;
        json["vBattRecovery"] = settings.vBattRecovery;
        json["socCuttOff"] = settings.socCuttOff;
        json["socRecovery"] = settings.socRecovery;
        json["tempFanOn"] = settings.tempFanOn;
        json["tempCutOffHigh"] = settings.tempCutOffHigh;
        json["tempCutOffLow"] = settings.tempCutOffLow;
        json["tempDanger"] = settings.tempDanger;
        json["enableFireSensor"] = (int) settings.enableFireSensor;
        serializeJsonPretty(json, *response);
        request->send(response);
    });
    server.on("/api/config-upd", HTTP_GET, handleConfigUpdate);

    server.on("/api/clear-errors", HTTP_GET, [](AsyncWebServerRequest* request) {
        resetErrors();
        Serial.println("INFO:\tErrors cleared.");
        request->send(200, "text/plain", "OK");
    });

#if SERVER_PORT == 443
    server.beginSecure(sslCert, sslPrivKey, "");
#else
    server.begin();
#endif

    Serial.println("INFO:\tServer is up.");

    while (1) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("INFO:\tWiFi connection failed, retrying...");
            vTaskDelay(500 / portTICK_PERIOD_MS);
            wifiConnect();
            Serial.print("INFO:\tIP Address: ");
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

            case 'E': {
                nvs_flash_erase();
                nvs_flash_init();
                saveSettings();
            }

            case 'S': {
                saveSettings();
            }

            case 'R': {
                Serial.println("INFO:\tReboot...");
                delay(100);
                ESP.restart();
                break;
            }

            case 'Q': {
                resetErrors();
                break;
            }

            case 'P': {
                int code = sendPushover("ESP32-ATS", "Messaggio di test!");
                Serial.println("INFO:\tPushover test message sent, response code: " + String(code));
                break;
            }

            case 'L': {
                Serial.println("Configuration:");
                Serial.print("\t- vBattCuttOff = ");
                Serial.println(settings.vBattCuttOff, 3);
                Serial.print("\t- socCuttOff = ");
                Serial.println(settings.socCuttOff, 3);
                Serial.print("\t- socCuttOff = ");
                Serial.println(settings.socCuttOff, 3);
                Serial.print("\t- socRecovery = ");
                Serial.println(settings.socRecovery, 3);
                Serial.print("\t- tempFanOn = ");
                Serial.println(settings.tempFanOn, 3);
                Serial.print("\t- tempCutOffHigh = ");
                Serial.println(settings.tempCutOffHigh, 3);
                Serial.print("\t- tempCutOffLow = ");
                Serial.println(settings.tempCutOffLow, 3);
                Serial.print("\t- tempDanger = ");
                Serial.println(settings.tempDanger, 3);
                Serial.print("\t- enableFireSensor = ");
                Serial.println(settings.enableFireSensor);
            }
        }
    }
}

void resetErrors() {
    Serial.println("INFO:\tResetting errors...");
    enableATSNtcDanger = true;
    enableATSFire = true;
    fanOnFire = false;
}

double readNTC(uint8_t id) {
    double Vout = ADC_LUT[analogRead(ntcPins[id])] * VREF / ADC_MAX;
    double Rt = NTC_R1 * Vout / (VREF - Vout);
    double temp = ntcFilters[id].add(1.0 / (1.0 / NTC_T0 + log(Rt / NTC_R0) / NTC_B) - 273.15);
    return constrain(temp, -50.0, 300.0);
}

void handleConfigUpdate(AsyncWebServerRequest* request) {
    Serial.println("INFO:\tNew configuration:");
    if (request->hasParam("vBattCuttOff")) {
        settings.vBattCuttOff = request->getParam("vBattCuttOff")->value().toDouble();
        Serial.print("\t- vBattCuttOff = ");
        Serial.println(settings.vBattCuttOff, 3);
    }
    if (request->hasParam("vBattRecovery")) {
        settings.vBattRecovery = request->getParam("vBattRecovery")->value().toDouble();
        Serial.print("\t- socCuttOff = ");
        Serial.println(settings.socCuttOff, 3);
    }
    if (request->hasParam("socCuttOff")) {
        settings.socCuttOff = request->getParam("socCuttOff")->value().toDouble();
        Serial.print("\t- socCuttOff = ");
        Serial.println(settings.socCuttOff, 3);
    }
    if (request->hasParam("socRecovery")) {
        settings.socRecovery = request->getParam("socRecovery")->value().toDouble();
        Serial.print("\t- socRecovery = ");
        Serial.println(settings.socRecovery, 3);
    }
    if (request->hasParam("tempFanOn")) {
        settings.tempFanOn = request->getParam("tempFanOn")->value().toDouble();
        Serial.print("\t- tempFanOn = ");
        Serial.println(settings.tempFanOn, 3);
    }
    if (request->hasParam("tempCutOffHigh")) {
        settings.tempCutOffHigh = request->getParam("tempCutOffHigh")->value().toDouble();
        Serial.print("\t- tempCutOffHigh = ");
        Serial.println(settings.tempCutOffHigh, 3);
    }
    if (request->hasParam("tempCutOffLow")) {
        settings.tempCutOffLow = request->getParam("tempCutOffLow")->value().toDouble();
        Serial.print("\t- tempCutOffLow = ");
        Serial.println(settings.tempCutOffLow, 3);
    }
    if (request->hasParam("tempDanger")) {
        settings.tempDanger = request->getParam("tempDanger")->value().toDouble();
        Serial.print("\t- tempDanger = ");
        Serial.println(settings.tempDanger, 3);
    }
    if (request->hasParam("enableFireSensor")) {
        settings.enableFireSensor = (request->getParam("enableFireSensor")->value().toInt() > 0);
        Serial.print("\t- enableFireSensor = ");
        Serial.println(settings.enableFireSensor);
    }

    saveSettings();
    request->send(200, "application/text", "OK.");
}
