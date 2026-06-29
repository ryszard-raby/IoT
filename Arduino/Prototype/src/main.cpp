#include <Arduino.h>
#include <Servo.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <Oled.h>
#include <OTAService.h>
#include <PinManager.h>
#include <ScheduleManager.h>

// ── Instancje menedżerów ──
PinManager pinManager;
ScheduleManager scheduleManager;

// ── Czas ──
uint64_t startUs = 0;
Timer timeNow;
system_clock::time_point timeSnapPoint;

// ── LED / Ping ──
bool ledState = false;
bool pendingPing = false;
String lastPingValue = "";

// ── Serwisy ──
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

// ── Callbacki przekazujące do menedżerów (wolne funkcje dla C-style wskaźników) ──

void _onPinMode(String pin, String mode) {
  pinManager.onPinMode(pin, mode);
}

void _onButtonDependency(String dependentPin, int minValue, String depPin, int depValue, String label) {
  pinManager.onButtonDependency(dependentPin, minValue, depPin, depValue, label);
}

void _onScheduleEntry(const ScheduleEntry& entry) {
  scheduleManager.onScheduleEntry(entry);
}

void _onScheduleClear(const String& pin) {
  scheduleManager.onScheduleClear(pin);
}

/// Główny callback Firebase – łączy dane z Firebase z menedżerami.
void onFirebaseData(String key, String value) {
  if (key == "time") {
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }
  if (key == "gpio2") {
    ledState = (value.toInt() != 0);
  }
  if (key == "pingRequest") {
    if (value != lastPingValue) {
      lastPingValue = value;
      pendingPing = true;
    }
  }

  // Śledź stan pinów – przy każdej zmianie zapisz w PinManager
  if (key != "time" && key != "pingRequest" && key != "online" &&
      key != "name" && key != "type" && key != "lastSeen" && key != "createdAt" &&
      key != "dependency" && key != "schedule" && key != "label" && key != "gpio" &&
      !key.startsWith("-")) {
    pinManager.pinStates[key] = value.toInt();
  }
}

// ── Setup ──
void setup() {
  Serial.begin(9600);

  // OLED
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
  firebaseService.setScheduleCallback(_onScheduleEntry);
  firebaseService.setScheduleClearCallback(_onScheduleClear);
  firebaseService.setDependencyCallback(_onButtonDependency);
  firebaseService.setModeCallback(_onPinMode);
  firebaseService.setDeviceTime();
  firebaseService.setOnline(true);

  startUs = micros64();
  pinMode(LED_BUILTIN, OUTPUT);
}

// ── Loop ──
void loop() {
  otaService.handle();
  bool hadStreamData = firebaseService.firebaseStream();

  // Po przetworzeniu streamu – zastosuj stan na wszystkich pinach
  if (hadStreamData) {
    pinManager.applyAllPinStates();
  }

  // Oblicz aktualny czas
  int64_t elapsedUs = micros64() - startUs;
  timeNow = timeService.timerFromTimePoint(timeSnapPoint + microseconds(elapsedUs));

  // OLED: godzina + stan LED
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
           timeNow.hour, timeNow.minute, timeNow.second);
  oled.stack[0].text = String(timeBuf);
  oled.stack[1].text = ledState ? "LED: ON " : "LED: OFF";
  oled.print();

  // Odpowiedź na ping
  if (pendingPing) {
    pendingPing = false;
    firebaseService.setDeviceTime();
  }

  // ── Harmonogram ──
  scheduleManager.checkReverts(firebaseService);
  scheduleManager.checkTriggers(timeNow, pinManager.pinStates, firebaseService);

  // ── Odpytywanie wejść ──
  pinManager.pollInputs(firebaseService);

  delay(100);
}
