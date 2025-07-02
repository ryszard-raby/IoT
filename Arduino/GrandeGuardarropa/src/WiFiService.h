#include <ESP8266WiFi.h>
#include <C:/Users/rysza/OneDrive/auth/auth.h>

class WiFiService {
    public:
    void connect() {
        WiFi.mode(WIFI_STA);
        // WiFi.begin(WIFI_SSID, WIFI_PASS);
        WiFi.begin(WIFI_SSID, WIFI_PASS);

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        Serial.println(".......");
        Serial.println("Connected with IP:");
        Serial.println(WiFi.localIP());
        Serial.println();
    }
};