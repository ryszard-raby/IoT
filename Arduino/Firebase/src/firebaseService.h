#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <C:/auth.h>

FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;

class FirebaseService {

    std::string userid = USERID;
    std::string projectid = PROJECTID;
    std::string deviceid = DEVICEID;

public : 

    typedef void (*Callback)(String key, String value);

    void setCallback(Callback cb) {
        callback = cb;
    }

    void setTimestamp() {
        Firebase.RTDB.setTimestamp(&fbData, "usersData/" + userid + "/projects/" + projectid + "/devices/" + deviceid + "/time/");
    }

    String getTimestamp() {
        Firebase.RTDB.get(&fbData, "usersData/" + userid + "/projects/" + projectid + "/devices/" + deviceid + "/time/");
        return fbData.stringData();
    }

    void connectFirebase() {
        Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
        fbConfig.database_url = FIREBASE_DATABASE_URL;
        fbConfig.api_key = FIREBASE_API_KEY;
        fbAuth.user.email = FIREBASE_USER_EMAIL;
        fbAuth.user.password = FIREBASE_USER_PASSWORD;

        fbConfig.token_status_callback = tokenStatusCallback;

        #if defined(ESP8266)
            // required for large file data, increase Rx size as needed.
            fbData.setBSSLBufferSize(1024 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
        #endif

        fbConfig.fcs.download_buffer_size = 2048;

        Firebase.begin(&fbConfig, &fbAuth);

        Firebase.reconnectWiFi(true);

        std::string path = "usersData/" + userid + "/projects/" + projectid + "/devices/" + deviceid + "/pins/";

        if (!Firebase.RTDB.beginStream(&fbData, path))
            Serial.printf("Stream begin error %s\n\n", fbData.errorReason().c_str());
    }

    void firebaseStream() {
        if (Firebase.ready()) {
            if (!Firebase.RTDB.readStream(&fbData))
            Serial.printf("Stream read error %s\n\n", fbData.errorReason().c_str());
            if (fbData.streamTimeout())
            Serial.printf("Stream timeout, resuming...\n");
            if (fbData.streamAvailable()) {
                Serial.printf("Stream path: %s\nevent path: %s\ndata type: %s\nevent type: %s\nvalue: %d\n\n",
                    fbData.streamPath().c_str(),
                    fbData.dataPath().c_str(),
                    fbData.dataType().c_str(),
                    fbData.eventType().c_str(),
                    fbData.intData());
                    
                // Serial.printf("Stream data: %s\n\n", fbData.jsonString().c_str());

                if (fbData.dataType() == "json") {
                    FirebaseJson *json = fbData.jsonObjectPtr();
                    String jsonStr;
                    json->toString(jsonStr, true);
                    Serial.println(jsonStr);
                    size_t len = json->iteratorBegin();
                    String key, value = "";
                    int type = 0;
                    for (size_t i = 0; i < len; i++) {
                        json->iteratorGet(i, type, key, value);
                        Serial.printf("key: %s, value: %s, type: %s\n", 
                            key.c_str(), 
                            value.c_str(), 
                            type == FirebaseJson::JSON_OBJECT ? "object" : "array"
                            );
                        if (callback) {
                            callback(key.c_str(), value.c_str());
                        }
                    }
                }
            }
        }
        else {
            Serial.printf("Stream error %s\n\n", fbData.errorReason().c_str());
        }
    }
private:
    Callback callback;
};