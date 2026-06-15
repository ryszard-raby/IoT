#include <Arduino.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <Oled.h>
#include <OTAService.h>

// ---- Zmienne globalne ----
uint64_t startUs = 0;
Timer timeNow;
system_clock::time_point timeSnapPoint;
bool ledState = false;
bool pendingPing = false;
String lastPingValue = "";  // ostatnia obsłużona wartość pingRequest

// ---- Serwisy ----
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

// ---- Callback Firebase ----
void onFirebaseData(String key, String value) {
  if (key == "time") {
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }
  if (key == "gpio2") {
    ledState = (value.toInt() != 0);
    // ESP8266: LED_BUILTIN aktywny LOW
    digitalWrite(LED_BUILTIN, ledState ? LOW : HIGH);
  }
  if (key == "pingRequest") {
    // Odpowiadaj tylko jeśli to NOWY ping (ignoruj echo z full-JSON streamu)
    if (value != lastPingValue) {
      lastPingValue = value;
      pendingPing = true;
    }
  }
}

// ---- Setup ----
void setup() {
  Serial.begin(9600);

  // OLED – przygotuj sloty: godzina (wiersz 0) i stan LED (wiersz 2)
  oled.init();
  oled.add(0, 0, "00:00:00", 2);
  oled.add(0, 40, "LED: OFF", 1);

  // WiFi
  wifiService.connect();

  // OTA
  otaService.init();

  // Firebase
  firebaseService.connectFirebase();
  firebaseService.setCallback([](String key, String value) {
    onFirebaseData(key, value);
  });
  firebaseService.setDeviceTime();
  firebaseService.setOnline(true);    // zgłoś obecność w bazie

  startUs = micros64();
  pinMode(LED_BUILTIN, OUTPUT);
}

// ---- Loop ----
void loop() {
  otaService.handle();
  firebaseService.firebaseStream();

  // Oblicz aktualny czas – micros64() to 64-bit µs, brak overflow
  int64_t elapsedUs = micros64() - startUs;
  timeNow = timeService.timerFromTimePoint(timeSnapPoint + microseconds(elapsedUs));

  // Formatuj czas
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
           timeNow.hour, timeNow.minute, timeNow.second);

  // Aktualizuj OLED: godzina + stan LED z Firebase
  oled.stack[0].text = String(timeBuf);
  oled.stack[1].text = ledState ? "LED: ON " : "LED: OFF";
  oled.print();

  // Odpowiedź na ping – poza callbackiem, żeby nie konfliktować ze streamem
  if (pendingPing) {
    pendingPing = false;
    firebaseService.setDeviceTime();
  }

  delay(100);
}