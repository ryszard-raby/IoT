#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <C:\Users\Richart\OneDrive\auth\auth.h>

// ---- Fallback – dopóki auth.h nie ma IOINIT2_DEVICEID__PROTOTYPE ----
#ifndef IOINIT2_DEVICEID__PROTOTYPE
#define IOINIT2_DEVICEID__PROTOTYPE DEVICEID__PROTOTYPE
#endif

// ---- Firebase globals (singleton per device) ----
FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;

class FirebaseService {
    // Inicjalizacja w ciele klasy – bezpieczna nawet gdy makro ma zbędny średnik
    std::string deviceId = IOINIT2_DEVICEID__PROTOTYPE;

public:
    using Callback = void (*)(String key, String value);

    /// Opcjonalnie nadpisz device ID (np. z auth.h po aktualizacji)
    void setDeviceId(const std::string& id) { deviceId = id; }

    void setCallback(Callback cb) { callback = cb; }

    // ========== Nowe API – struktura ioinit2 ==========

    /// Ustawia timestamp serwera na devices/{deviceId}/time
    void setDeviceTime() {
        Firebase.RTDB.setTimestamp(&fbData, path("time"));
    }

    /// Zapisuje stan GPIO: devices/{deviceId}/state/gpio/{index}
    void setGpio(int index, int value) {
        Firebase.RTDB.setInt(&fbData, path("state/gpio/" + std::to_string(index)), value);
    }

    /// Ustawia online/offline: devices/{deviceId}/online
    void setOnline(bool online) {
        Firebase.RTDB.setBool(&fbData, path("online"), online);
    }

    /// Generyczny set int  → devices/{deviceId}/{key}
    void setData(const std::string& key, int value) {
        Firebase.RTDB.setInt(&fbData, path(key), value);
    }

    /// Generyczny set string → devices/{deviceId}/{key}
    void setData(const std::string& key, const std::string& value) {
        Firebase.RTDB.setString(&fbData, path(key), value);
    }

    // ========== Połączenie i stream ==========

    void connectFirebase() {
        Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
        Serial.printf("Device ID: %s\n", deviceId.c_str());

        fbConfig.database_url = IOINIT2_FIREBASE_DATABASE_URL;
        fbConfig.api_key    = IOINIT2_FIREBASE_API_KEY;
        fbAuth.user.email   = FIREBASE_USER_EMAIL;
        fbAuth.user.password = FIREBASE_USER_PASSWORD;
        fbConfig.token_status_callback = tokenStatusCallback;

#if defined(ESP8266)
        fbData.setBSSLBufferSize(1024, 1024);
#endif
        fbConfig.fcs.download_buffer_size = 2048;

        Firebase.begin(&fbConfig, &fbAuth);
        Firebase.reconnectWiFi(true);

        std::string streamPath = path("");  // devices/{deviceId}/
        if (!Firebase.RTDB.beginStream(&fbData, streamPath)) {
            Serial.printf("Stream begin error: %s\n", fbData.errorReason().c_str());
        } else {
            Serial.printf("Stream listening on: %s\n", streamPath.c_str());
        }
    }

    void firebaseStream() {
        if (!Firebase.ready()) return;

        if (!Firebase.RTDB.readStream(&fbData)) {
            Serial.printf("Stream read error: %s\n", fbData.errorReason().c_str());
            return;
        }

        if (fbData.streamTimeout()) {
            Serial.println("Stream timeout, resuming…");
            return;
        }

        if (!fbData.streamAvailable()) return;
        if (fbData.dataType() != "json")   return;

        FirebaseJson* json = fbData.jsonObjectPtr();
        size_t len = json->iteratorBegin();
        String key, value;
        int type = 0;

        for (size_t i = 0; i < len; i++) {
            json->iteratorGet(i, type, key, value);

            if (type == FirebaseJson::JSON_OBJECT && key == "state") {
                // Przekaż wszystkie klucze ze state do callbacku (led, gpio2, brightness, …)
                flattenState(json);
            } else if (callback) {
                callback(key, value);
            }
        }
        json->iteratorEnd();
    }

    bool isConnected() { return Firebase.ready(); }

    // ========== Helpers ==========

    /// Zwraca ścieżkę devices/{deviceId}/ + subpath
    std::string path(const std::string& subpath) const {
        if (subpath.empty()) return "devices/" + deviceId;
        return "devices/" + deviceId + "/" + subpath;
    }

private:
    Callback callback = nullptr;

    /// Parsuje obiekt "state" – przekazuje wszystkie jego klucze do callbacku
    /// Np. {"led":1, "gpio2":0, "brightness":128} → callback("led","1"), callback("gpio2","0"), …
    void flattenState(FirebaseJson* json) {
        FirebaseJsonData stateData;
        if (!json->get(stateData, "state")) return;

        FirebaseJson stateJson;
        stateJson.setJsonData(stateData.stringValue);

        size_t len = stateJson.iteratorBegin();
        String key, value;
        int type = 0;
        for (size_t i = 0; i < len; i++) {
            stateJson.iteratorGet(i, type, key, value);
            if (callback) callback(key, value);
        }
        stateJson.iteratorEnd();
    }
};