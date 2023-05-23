#ifndef CONFIG_H
#define CONFIG_H

#define BAUD_RATE 115200

#define SSID "4G-90007273"
#define PASSWORD "V7special"
#define WIFI_TIMEOUT 15000
#define HOSTNAME "ESP32-ATS"

#define PUSHOVER_USER "u766fgj7jkc9eb2v15aqt7hpt54evz"
#define PUSHOVER_TOKEN "af1xmemq64era2ajbb68t9a8hxd5am"

#define SAMPLING_INTERVAL 1000L

#define FAN_HISTERESIS 3.0

#define VREF 3.3

#define NTC_B 3950.0
#define NTC_R0 100000.0
#define NTC_T0 298.15
#define NTC_R1 47000.0
#define NTC_WINDOW 5

#define CLAMP_NOISE_DEFAULT 0.052945
#define CLAMP_CALIBRATION_X (1.0853685 / 4)
#define CLAMP_CALIBRATION_Y 0.44
#define CLAMP_MEASURE_TIME 1000L
#define CLAMP_WINDOW 5

#define VIN_DIVIDER_R1 100000.0
#define VIN_DIVIDER_R2 12000.0
#define VIN_REGRESSION_M 0.9361
#define VIN_REGRESSION_Q 0.8146
#define VIN_DIVIDER_WINDOW 10
#define VIN_MAX 30.0

#define ADC_MAX 4095.0
#define ADC_RESOLUTION 12

#endif
