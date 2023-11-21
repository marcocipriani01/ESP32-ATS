#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define SERVER_PORT 80

#define BAUD_RATE 115200

#define WIFI_SSID "GL-AR150-321"
#define WIFI_PASSWORD "goodlife"
#define WIFI_TIMEOUT 15000
#define HOSTNAME "ESP32-ATS"

#define USER "nikcoli"
#define USER_PASSWORD "Caprile2020!"

#define PUSHOVER_USER "u766fgj7jkc9eb2v15aqt7hpt54evz"
#define PUSHOVER_TOKEN "af1xmemq64era2ajbb68t9a8hxd5am"

#define NVS_MARKER 0xAB

#define SAMPLING_INTERVAL 1000L

#define TEMP_HISTERESIS 3.0

#define VREF 3.3

#define NTC_B 3950.0
#define NTC_R0 100000.0
#define NTC_T0 298.15
#define NTC_R1 47000.0
#define NTC_WINDOW 5

#define VIN_dt_INTERVAL (60 * 1000L)
#define VIN_DIVIDER_R1 100000.0
#define VIN_DIVIDER_R2 12000.0
#define VIN_REGRESSION_M 0.9361
#define VIN_REGRESSION_Q 0.8146
#define VIN_DIVIDER_WINDOW 10
#define VIN_MAX 30.0

#define ADC_MAX 4095.0
#define ADC_RESOLUTION 12

#endif
