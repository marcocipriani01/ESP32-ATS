#include "ats_server.h"

namespace AtsServer {
AsyncWebServer server(80);

boolean connect() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);

    unsigned long start = millis();
    while ((WiFi.status() != WL_CONNECTED) && ((millis() - start) <= WIFI_TIMEOUT)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    return (WiFi.status() == WL_CONNECTED);
}

void root(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello, world");
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}
}  // namespace AtsServer
