#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

class OTAService {
private:
    String hostname;
    String password;
    
public:
    OTAService(String deviceHostname = "Pasillo", String otaPassword = "update123") 
        : hostname(deviceHostname), password(otaPassword) {}
    
    void init() {
        Serial.println("=== OTA INITIALIZATION STARTING ===");
        
        // Sprawdź status WiFi
        Serial.print("WiFi Status: ");
        Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
        Serial.print("WiFi IP: ");
        Serial.println(WiFi.localIP().toString());
        
        // Ustaw hostname urządzenia
        Serial.print("Setting hostname to: ");
        Serial.println(hostname);
        ArduinoOTA.setHostname(hostname.c_str());
        
        // Ustaw hasło dla OTA (opcjonalne, ale zalecane)
        Serial.print("Setting OTA password: ");
        Serial.println(password);
        ArduinoOTA.setPassword(password.c_str());
        
        // Ustaw port (domyślnie 8266)
        Serial.print("Setting OTA port to: ");
        Serial.println(8266);
        ArduinoOTA.setPort(8266);
        
        // Callback dla rozpoczęcia aktualizacji
        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH) {
                type = "sketch";
            } else { // U_SPIFFS
                type = "filesystem";
            }
            
            Serial.println("OTA Update Started - Type: " + type);
            Serial.println("Please wait, do not reset device...");
        });
        
        // Callback dla zakończenia aktualizacji
        ArduinoOTA.onEnd([]() {
            Serial.println("\nOTA Update Complete!");
            Serial.println("Device will restart...");
        });
        
        // Callback dla postępu aktualizacji
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("OTA Progress: %u%% (%u/%u bytes)\r", (progress / (total / 100)), progress, total);
        });
        
        // Callback dla błędów
        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf("OTA Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) {
                Serial.println("Authentication Failed - Check password");
            } else if (error == OTA_BEGIN_ERROR) {
                Serial.println("Begin Failed - Not enough space?");
            } else if (error == OTA_CONNECT_ERROR) {
                Serial.println("Connect Failed - Network issue?");
            } else if (error == OTA_RECEIVE_ERROR) {
                Serial.println("Receive Failed - Connection lost?");
            } else if (error == OTA_END_ERROR) {
                Serial.println("End Failed - Verification error?");
            }
        });
        
        // Rozpocznij OTA
        Serial.println("Starting ArduinoOTA.begin()...");
        ArduinoOTA.begin();
        Serial.println("ArduinoOTA.begin() completed successfully!");
        
        // Sprawdź czy WiFi nadal jest połączone
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connection confirmed after OTA init");
        } else {
            Serial.println("WARNING: WiFi disconnected after OTA init!");
        }
        
        Serial.println("===============================");
        Serial.println("OTA SERVICE READY");
        Serial.println("===============================");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.println("Hostname: " + hostname);
        Serial.println("OTA Password: " + password);
        Serial.println("OTA Port: 8266");
        Serial.println("===============================");
        Serial.println("Ready for OTA updates!");
        Serial.println("Use: pio run -e GrandeGuardarropa -t upload");
        Serial.println("===============================");
    }
    
    void handle() {
        // Ta funkcja musi być wywoływana w loop() aby obsługiwać żądania OTA
        ArduinoOTA.handle();
    }
    
    void printStatus() {
        static unsigned long lastPrint = 0;
        unsigned long now = millis();
        
        // Wyświetl status co 30 sekund
        if (now - lastPrint > 30000) {
            lastPrint = now;
            Serial.println("OTA Status Check:");
            Serial.println("- IP: " + WiFi.localIP().toString());
            Serial.println("- Hostname: " + hostname);
            Serial.println("- WiFi Status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
            Serial.println("- Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
            Serial.println("- Uptime: " + String(millis() / 1000) + "s");
            Serial.println("Ready for OTA...");
        }
    }
    
    void setHostname(String newHostname) {
        hostname = newHostname;
    }
    
    void setPassword(String newPassword) {
        password = newPassword;
    }
    
    String getHostname() {
        return hostname;
    }
    
    String getIPAddress() {
        return WiFi.localIP().toString();
    }
};

#endif
