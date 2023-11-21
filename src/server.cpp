#include "server.h"

AsyncWebServer server(SERVER_PORT);

boolean wifiConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long start = millis();
    while ((WiFi.status() != WL_CONNECTED) && ((millis() - start) <= WIFI_TIMEOUT)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    return (WiFi.status() == WL_CONNECTED);
}

void notFoundPage(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}
