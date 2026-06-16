#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <C:\Users\Richart\OneDrive\auth\auth.h>
#include <vector>

// ---- Fallback – dopóki auth.h nie ma IOINIT2_DEVICEID__PROTOTYPE ----
#ifndef IOINIT2_DEVICEID__PROTOTYPE
#define IOINIT2_DEVICEID__PROTOTYPE DEVICEID__PROTOTYPE
#endif

// ---- Struktura pojedynczego zdarzenia harmonogramu ----
struct ScheduleEntry {
    String gpio;          // nazwa pinu, np. "gpio5"
    int minValue;         // wartość revert (z ButtonConfig.minValue)
    int triggerHour;      // godzina wyzwolenia (0–23)
    int triggerMinute;    // minuta wyzwolenia (0–59)
    int value;            // wartość do ustawienia
    int durationMinutes;  // czas trwania w minutach (0 = bez revertu)
    // ── Button-level AND dependency ──
    String dependencyGpio;   // GPIO do sprawdzenia (puste = brak)
    int dependencyValue;     // oczekiwana wartość GPIO
};

// ---- Firebase globals (singleton per device) ----
FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;

class FirebaseService {
    // Inicjalizacja w ciele klasy – bezpieczna nawet gdy makro ma zbędny średnik
    std::string deviceId = IOINIT2_DEVICEID__PROTOTYPE;

public:
    using Callback = void (*)(String key, String value);
    using ScheduleCallback = void (*)(const ScheduleEntry& entry);
    /// gpio – nazwa pinu do wyczyszczenia; pusty string = wyczyść wszystko
    using ScheduleClearCallback = void (*)(const String& gpio);
    /// Rejestruje zależność AND na poziomie buttona: dependentGpio zależy od depGpio==depValue
    using DependencyCallback = void (*)(String dependentGpio, int minValue, String depGpio, int depValue, String label);

    /// Opcjonalnie nadpisz device ID (np. z auth.h po aktualizacji)
    void setDeviceId(const std::string& id) { deviceId = id; }

    void setCallback(Callback cb) { callback = cb; }
    void setScheduleCallback(ScheduleCallback cb) { scheduleCallback = cb; }
    void setScheduleClearCallback(ScheduleClearCallback cb) { scheduleClearCallback = cb; }
    void setDependencyCallback(DependencyCallback cb) { dependencyCallback = cb; }

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

    bool firebaseStream() {
        if (!Firebase.ready()) return false;

        if (!Firebase.RTDB.readStream(&fbData)) {
            Serial.printf("Stream read error: %s\n", fbData.errorReason().c_str());
            return false;
        }

        if (fbData.streamTimeout()) {
            Serial.println("Stream timeout, resuming…");
            return false;
        }

        if (!fbData.streamAvailable()) return false;

        // ---- JSON (całe obiekty, np. {"state":{...}}) ----
        if (fbData.dataType() == "json") {
            String dp = fbData.dataPath();
            FirebaseJson* json = fbData.jsonObjectPtr();

            // ---- Pod-ścieżka /buttons lub /buttons/{id} (aktualizacja UI) ----
            if (dp == "/buttons") {
                // Cały obiekt buttons – klucze to buttonId
                if (scheduleClearCallback) scheduleClearCallback("");
                parseButtonsJson(json);
                return true;
            }
            if (dp.startsWith("/buttons/")) {
                // Pojedynczy button – klucze to pola (gpio, schedule, …)
                parseSingleButtonJson(json);
                return true;
            }

            // ---- Pełny obiekt urządzenia (klucze: state, buttons, time, …) ----
            size_t len = json->iteratorBegin();
            String key, value;
            int type = 0;

            for (size_t i = 0; i < len; i++) {
                json->iteratorGet(i, type, key, value);

                if (type == FirebaseJson::JSON_OBJECT && key == "state") {
                    flattenState(json);
                } else if (type == FirebaseJson::JSON_OBJECT && key == "buttons") {
                    // Przekazujemy string JSON zamiast wskaźnika – unikamy kolizji iteratora
                    if (scheduleClearCallback) scheduleClearCallback("");
                    parseButtonsJsonString(value);
                } else if (callback) {
                    callback(key, value);
                }
            }
            json->iteratorEnd();
            return true;
        }

        // ---- Pojedyncze wartości (int, string, bool, float) ----
        // dataPath() → np. "/state/gpio2"  → klucz = "gpio2"
        String path = fbData.dataPath();
        // Pomiń aktualizacje pod /buttons/ – to nie są stany GPIO
        if (path.startsWith("/buttons/")) return true;
        int lastSlash = path.lastIndexOf('/');
        String key = (lastSlash >= 0) ? path.substring(lastSlash + 1) : path;
        String value = fbData.stringData();

        if (callback) callback(key, value);
        return true;
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
    ScheduleCallback scheduleCallback = nullptr;
    ScheduleClearCallback scheduleClearCallback = nullptr;
    DependencyCallback dependencyCallback = nullptr;

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

    /// Parsuje pełny obiekt "buttons" (klucze = buttonId) z FirebaseJson* (ścieżka /buttons)
    void parseButtonsJson(FirebaseJson* json) {
        size_t len = json->iteratorBegin();
        String key, value;
        int type = 0;
        for (size_t i = 0; i < len; i++) {
            json->iteratorGet(i, type, key, value);
            if (type == FirebaseJson::JSON_OBJECT) {
                parseOneButton(value);
            }
        }
        json->iteratorEnd();
    }

    /// Parsuje pełny obiekt "buttons" z JSON stringa (z pełnego obiektu urządzenia)
    void parseButtonsJsonString(const String& buttonsJsonStr) {
        FirebaseJson buttonsJson;
        buttonsJson.setJsonData(buttonsJsonStr);
        parseButtonsJson(&buttonsJson);
    }

    /// Parsuje pojedynczy button (ścieżka /buttons/{id}) – klucze to gpio, schedule, …
    void parseSingleButtonJson(FirebaseJson* json) {
        FirebaseJsonData gpioData, minValData, labelData;
        if (!json->get(gpioData, "gpio")) return;
        if (!json->get(minValData, "minValue")) return;
        String gpio = gpioData.stringValue;
        int minValue = minValData.intValue;
        String label = "";
        if (json->get(labelData, "label")) label = labelData.stringValue;

        // ── Button-level dependency (AND gate) – zawsze wyodrębnij ──
        String depGpio = "";
        int depValue = 0;
        FirebaseJsonData depData;
        if (json->get(depData, "dependency") && depData.stringValue.length() > 0) {
            FirebaseJson depObj;
            depObj.setJsonData(depData.stringValue);
            FirebaseJsonData dGpio, dVal;
            if (depObj.get(dGpio, "gpio"))  depGpio = dGpio.stringValue;
            if (depObj.get(dVal,  "value")) depValue = dVal.intValue;
        }

        // Zarejestruj zależność (nawet bez schedule – do wymuszania OFF)
        if (depGpio.length() > 0 && dependencyCallback) {
            dependencyCallback(gpio, minValue, depGpio, depValue, label);
        }

        // Sprawdź czy ten button ma schedule
        FirebaseJsonData schedData;
        if (!json->get(schedData, "schedule")) return;
        if (schedData.type != "array" && schedData.type != "jsonArray") return;

        // Wyczyść stare wpisy dla tego GPIO przed dodaniem nowych
        if (scheduleClearCallback) scheduleClearCallback(gpio);

        parseScheduleArray(schedData.stringValue, gpio, minValue, depGpio, depValue);
    }

    /// Parsuje pojedynczy button z JSON stringa i dodaje jego harmonogramy
    void parseOneButton(const String& btnJsonStr) {
        FirebaseJson btnObj;
        btnObj.setJsonData(btnJsonStr);

        FirebaseJsonData gpioData, minValData, labelData;
        if (!btnObj.get(gpioData, "gpio")) return;
        if (!btnObj.get(minValData, "minValue")) return;
        String gpio = gpioData.stringValue;
        int minValue = minValData.intValue;
        String label = "";
        if (btnObj.get(labelData, "label")) label = labelData.stringValue;

        // ── Button-level dependency (AND gate) – zawsze wyodrębnij ──
        String depGpio = "";
        int depValue = 0;
        FirebaseJsonData depData;
        if (btnObj.get(depData, "dependency") && depData.stringValue.length() > 0) {
            FirebaseJson depObj;
            depObj.setJsonData(depData.stringValue);
            FirebaseJsonData dGpio, dVal;
            if (depObj.get(dGpio, "gpio"))  depGpio = dGpio.stringValue;
            if (depObj.get(dVal,  "value")) depValue = dVal.intValue;
        }

        // Zarejestruj zależność (nawet bez schedule – do wymuszania OFF)
        if (depGpio.length() > 0 && dependencyCallback) {
            dependencyCallback(gpio, minValue, depGpio, depValue, label);
        }

        // Sprawdź czy ten button ma schedule
        FirebaseJsonData schedData;
        if (!btnObj.get(schedData, "schedule")) return;
        if (schedData.type != "array" && schedData.type != "jsonArray") return;

        parseScheduleArray(schedData.stringValue, gpio, minValue, depGpio, depValue);
    }

    /// Parsuje tablicę schedule i wywołuje scheduleCallback dla każdego zdarzenia
    void parseScheduleArray(const String& schedJson, const String& gpio, int minValue,
                            const String& depGpio, int depValue) {
        FirebaseJsonArray schedArr;
        schedArr.setJsonArrayData(schedJson);

        size_t schedLen = schedArr.size();
        for (size_t j = 0; j < schedLen; j++) {
            FirebaseJsonData evData;
            if (!schedArr.get(evData, j)) continue;

            FirebaseJson evObj;
            evObj.setJsonData(evData.stringValue);

            FirebaseJsonData thData, tmData, vData, dData;
            if (!evObj.get(thData, "triggerHour")) continue;
            if (!evObj.get(tmData, "triggerMinute")) continue;
            if (!evObj.get(vData, "value")) continue;

            ScheduleEntry entry;
            entry.gpio = gpio;
            entry.minValue = minValue;
            entry.triggerHour = thData.intValue;
            entry.triggerMinute = tmData.intValue;
            entry.value = vData.intValue;
            entry.durationMinutes = 0;
            entry.dependencyGpio = depGpio;
            entry.dependencyValue = depValue;

            if (evObj.get(dData, "durationMinutes")) {
                entry.durationMinutes = dData.intValue;
            }

            if (scheduleCallback) scheduleCallback(entry);
        }
    }
};