#include <Arduino.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <Oled.h>
#include <OTAService.h>
#include <vector>
#include <set>
#include <algorithm>

// ---- Struktura revertu (powrót GPIO do minValue po czasie) ----
struct PendingRevert {
  int gpioPin;
  String gpioName;       // nazwa pinu np. "gpio5" – do zapisu w Firebase
  int revertValue;
  uint64_t revertAtUs;   // micros64() moment revertu (monotoniczny, odporny na zmianę timeSnapPoint)
};

// ---- Zmienne globalne ----
uint64_t startUs = 0;
Timer timeNow;
system_clock::time_point timeSnapPoint;
bool ledState = false;
bool pendingPing = false;
String lastPingValue = "";  // ostatnia obsłużona wartość pingRequest

// ---- Harmonogram ----
std::vector<ScheduleEntry> schedules;
std::set<String> firedToday;           // klucze "gpio:HH:MM" już odpalone dziś
std::vector<PendingRevert> reverts;    // aktywne reverty (powrót do minValue)

// ---- Serwisy ----
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

// ---- Pomocnicze ----

/// Mapuje nazwę GPIO (np. "gpio5") na numer pinu ESP8266.
/// Zwraca -1 jeśli nazwa nie pasuje.
int gpioNameToPin(const String& name) {
  if (name.startsWith("gpio") || name.startsWith("GPIO")) {
    return name.substring(4).toInt();
  }
  // Dla nazw niestandardowych – mapa (rozszerzaj wg potrzeb)
  if (name == "LED_BUILTIN" || name == "led") return LED_BUILTIN;
  return -1;
}

/// Buduje klucz do zbioru firedToday: "gpio:HH:MM"
String firedKey(const String& gpio, int hour, int minute) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%s:%02d:%02d", gpio.c_str(), hour, minute);
  return String(buf);
}

/// Callback – odbiera harmonogramy z Firebase i przebudowuje wektor schedules
void onScheduleEntry(const ScheduleEntry& entry) {
  // Dodajemy do wektora (duplikaty możliwe przy restreamie – czyścimy przed)
  schedules.push_back(entry);
  Serial.printf("[SCHEDULE] gpio=%s trigger=%02d:%02d value=%d dur=%dmin\n",
                entry.gpio.c_str(), entry.triggerHour, entry.triggerMinute,
                entry.value, entry.durationMinutes);
}

/// Callback – czyści harmonogramy przed załadowaniem nowych z Firebase
/// gpio == "" → wyczyść tylko schedules (reverty mają własny timer)
/// gpio != "" → wyczyść schedules + reverts tylko dla danego pinu
void onScheduleClear(const String& gpio) {
  if (gpio.length() == 0) {
    // Czyścimy tylko schedules – pending reverts muszą przetrwać
    schedules.clear();
    Serial.println("[SCHEDULE] Cleared schedules (reverts preserved)");
  } else {
    schedules.erase(
      std::remove_if(schedules.begin(), schedules.end(),
        [&gpio](const ScheduleEntry& e) { return e.gpio == gpio; }),
      schedules.end()
    );
    // Usuń reverty dla tego GPIO (bezpieczne – zmiana konfiguracji)
    reverts.erase(
      std::remove_if(reverts.begin(), reverts.end(),
        [&gpio](const PendingRevert& r) { return r.gpioName == gpio; }),
      reverts.end()
    );
    Serial.printf("[SCHEDULE] Cleared gpio=%s\n", gpio.c_str());
  }
}

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
  firebaseService.setScheduleCallback(onScheduleEntry);
  firebaseService.setScheduleClearCallback(onScheduleClear);
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

  // ── Harmonogram ─────────────────────────────────────────────────────

  // Reset firedToday o północy (gdy godzina i minuta = 0, a zbiór niepusty)
  if (timeNow.hour == 0 && timeNow.minute == 0 && !firedToday.empty()) {
    firedToday.clear();
    Serial.println("[SCHEDULE] Nowy dzień – reset firedToday");
  }

  // 1. Sprawdź reverty (powrót GPIO do minValue) – używamy micros64() (monotoniczny)
  uint64_t nowUs = micros64();
  for (auto it = reverts.begin(); it != reverts.end(); ) {
    if (nowUs >= it->revertAtUs) {
      digitalWrite(it->gpioPin, it->revertValue ? HIGH : LOW);
      firebaseService.setData("state/" + std::string(it->gpioName.c_str()), it->revertValue);
      Serial.printf("[SCHEDULE] Revert %s (pin=%d) → %d\n",
                    it->gpioName.c_str(), it->gpioPin, it->revertValue);
      it = reverts.erase(it);
    } else {
      ++it;
    }
  }

  // 2. Sprawdź harmonogramy
  for (const auto& entry : schedules) {
    if (timeNow.hour != entry.triggerHour || timeNow.minute != entry.triggerMinute)
      continue;

    String key = firedKey(entry.gpio, entry.triggerHour, entry.triggerMinute);
    if (firedToday.count(key)) continue;  // już odpalone dziś

    int pin = gpioNameToPin(entry.gpio);
    if (pin < 0) {
      Serial.printf("[SCHEDULE] Nieznany pin: %s\n", entry.gpio.c_str());
      continue;
    }

    // Ustaw pin
    pinMode(pin, OUTPUT);
    digitalWrite(pin, entry.value ? HIGH : LOW);
    firebaseService.setData("state/" + std::string(entry.gpio.c_str()), entry.value);
    firedToday.insert(key);

    Serial.printf("[SCHEDULE] FIRE %s (pin=%d) → %d", entry.gpio.c_str(), pin, entry.value);

    // Zaplanuj revert jeśli durationMinutes > 0
    if (entry.durationMinutes > 0) {
      PendingRevert rev;
      rev.gpioPin = pin;
      rev.gpioName = entry.gpio;
      rev.revertValue = entry.minValue;
      rev.revertAtUs = micros64() + uint64_t(entry.durationMinutes) * 60ULL * 1000000ULL;
      reverts.push_back(rev);
      Serial.printf(" | revert za %dmin → %d", entry.durationMinutes, entry.minValue);
    }
    Serial.println();
  }

  delay(100);
}