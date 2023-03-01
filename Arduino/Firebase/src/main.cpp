#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif

#include <Arduino.h>
#include <Firebase_ESP_Client.h>


// Provide the token generation process info.
#include <addons/TokenHelper.h>

#include <C:/auth.h>

FirebaseData fbData;
FirebaseAuth fbAuth;
FirebaseConfig fbConfig;

unsigned long dataMillis = 0;
int cout = 0;
uint8_t builtInLed = 2;

std::string userid = USERID;
std::string projectid = PROJECTID;
std::string deviceid = DEVICEID;

int intData;

void connectWiFi() {
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

void setup() {
  Serial.begin(9600);

  pinMode(builtInLed, OUTPUT);

  connectWiFi();
  connectFirebase();
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
        }
      }
    }
  }
  else {
    Serial.printf("Stream error %s\n\n", fbData.errorReason().c_str());
  }
}

void loop() {
  firebaseStream();
}