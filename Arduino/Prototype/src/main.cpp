#include <Arduino.h>
#include <Servo.h>
#include <WiFiService.h>
#include <FirebaseService.h>
#include <TimeService.h>
#include <Oled.h>
#include <OTAService.h>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

// ---- Struktura revertu (powrót pinu do minValue po czasie) ----
struct PendingRevert {
  String pinName;        // wirtualna nazwa pinu – do zapisu w Firebase
  int revertValue;
  uint64_t revertAtUs;   // micros64() moment revertu (monotoniczny, odporny na zmianę timeSnapPoint)
};

// ---- Struktura zależności AND na poziomie buttona ----
struct ButtonDep {
  String depPin;     // pin od którego zależy (np. "aktywacja")
  int depValue;      // oczekiwana wartość
  int minValue;      // wartość do wymuszenia gdy warunek nie spełniony
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
std::set<String> firedToday;           // klucze "pin:HH:MM" już odpalone dziś
std::vector<PendingRevert> reverts;    // aktywne reverty (powrót do minValue)
std::map<String, int> pinStates;       // bieżący stan wszystkich pinów (do sprawdzania zależności AND)
std::map<String, ButtonDep> buttonDeps; // zależności AND na poziomie buttona (key = dependentPin)

// ── Tryby pinów / Servo / Wejścia ──
std::map<String, String> pinModes;     // pin → "output" | "input" | "servo"
std::map<String, Servo> servos;        // pin → Servo instance (tylko dla trybu servo)
std::map<String, int> inputStates;     // ostatni odczytany stan pinu wejściowego (do detekcji zmiany)
uint64_t lastInputPollUs = 0;          // micros64() ostatniego odpytywania wejść

// ---- Serwisy ----
WiFiService wifiService;
FirebaseService firebaseService;
TimeService timeService;
Oled oled;
OTAService otaService;

// ---- Pomocnicze ----

/// Jedyny punkt mapowania wirtualnej nazwy pinu na fizyczny pin ESP8266.
/// Wywoływana tylko przed digitalWrite/pinMode. Zwraca -1 dla pinów wirtualnych.
int virtualPinToPhysical(const String& name) {
  if (name.startsWith("gpio") || name.startsWith("GPIO")) {
    return name.substring(4).toInt();
  }
  // Dla nazw niestandardowych – mapa (rozszerzaj wg potrzeb)
  if (name == "LED_BUILTIN" || name == "led") return LED_BUILTIN;
  return -1;
}

/// Buduje klucz do zbioru firedToday: "pin:HH:MM"
String firedKey(const String& pin, int hour, int minute) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%s:%02d:%02d", pin.c_str(), hour, minute);
  return String(buf);
}

/// Ustawia fizyczny pin z bramką AND – tylko dla trybu output.
/// Piny input i servo są obsługiwane osobno (odczyt / Servo.write).
void applyPinState(const String& name) {
  // Input, servo i PWM – nie ruszamy przez digitalWrite
  auto modeIt = pinModes.find(name);
  if (modeIt != pinModes.end() && modeIt->second != "output") return;

  int physicalPin = virtualPinToPhysical(name);
  if (physicalPin < 0) return;

  int fbVal = pinStates.count(name) ? pinStates[name] : 0;
  int targetValue = fbVal;

  auto depIt = buttonDeps.find(name);
  if (depIt != buttonDeps.end()) {
    int depState = pinStates.count(depIt->second.depPin) ? pinStates[depIt->second.depPin] : -1;
    if (depState != depIt->second.depValue) {
      targetValue = depIt->second.minValue;
    }
  }

  pinMode(physicalPin, OUTPUT);
  digitalWrite(physicalPin, targetValue ? HIGH : LOW);
}

/// Callback – odbiera tryb pinu z Firebase i konfiguruje sprzęt
void onPinMode(String pin, String mode) {
  // Ignoruj pusty tryb
  if (mode.length() == 0) return;

  auto it = pinModes.find(pin);
  // Bez zmian – pomiń
  if (it != pinModes.end() && it->second == mode) return;

  // "output" jest domyślny – nie nadpisuj nim jawnie ustawionego trybu (pwm/servo/input).
  // Zapobiega to błędnemu revertowi przy re-streamie pełnego obiektu z Firebase,
  // gdzie jedna ze ścieżek parsowania może nie zawierać pola mode.
  if (mode == "output" && it != pinModes.end() && it->second != "output") {
    Serial.printf("[MODE] %s – ignoring OUTPUT override (currently %s)\n",
                  pin.c_str(), it->second.c_str());
    return;
  }

  pinModes[pin] = mode;
  int physicalPin = virtualPinToPhysical(pin);

  if (mode == "input") {
    if (physicalPin >= 0) {
      pinMode(physicalPin, INPUT);
      inputStates[pin] = -1;  // wymuś pierwszy odczyt
      Serial.printf("[MODE] %s → INPUT (pin=%d)\n", pin.c_str(), physicalPin);
    } else {
      Serial.printf("[MODE] %s → INPUT (virtual)\n", pin.c_str());
    }
  } else if (mode == "servo") {
    if (physicalPin >= 0) {
      // Jeśli servo już istnieje, odepnij przed ponownym przypięciem
      auto it = servos.find(pin);
      if (it != servos.end()) it->second.detach();
      Servo srv;
      srv.attach(physicalPin);
      servos[pin] = srv;
      Serial.printf("[MODE] %s → SERVO attached to pin %d\n", pin.c_str(), physicalPin);
    } else {
      Serial.printf("[MODE] %s → SERVO (virtual – brak fizycznego pinu)\n", pin.c_str());
    }
  } else if (mode == "pwm") {
    // PWM – wyjście analogowe (0–255), np. jasność LED
    if (physicalPin >= 0) {
      // Usuń servo jeśli było
      auto it = servos.find(pin);
      if (it != servos.end()) { it->second.detach(); servos.erase(it); }
      inputStates.erase(pin);
      pinMode(physicalPin, OUTPUT);
      analogWrite(physicalPin, 0);
      Serial.printf("[MODE] %s → PWM (pin=%d)\n", pin.c_str(), physicalPin);
    } else {
      Serial.printf("[MODE] %s → PWM (virtual)\n", pin.c_str());
    }
  } else {
    // output (domyślny) – usuń z servo jeśli było, applyPinState zajmie się resztą
    auto it = servos.find(pin);
    if (it != servos.end()) { it->second.detach(); servos.erase(it); }
    inputStates.erase(pin);
    Serial.printf("[MODE] %s → OUTPUT\n", pin.c_str());
    applyPinState(pin);
  }
}

/// Callback – odbiera harmonogramy z Firebase i przebudowuje wektor schedules
void onScheduleEntry(const ScheduleEntry& entry) {
  // Dodajemy do wektora (duplikaty możliwe przy restreamie – czyścimy przed)
  schedules.push_back(entry);
  Serial.printf("[SCHEDULE] pin=%s trigger=%02d:%02d value=%d dur=%dmin",
                entry.pin.c_str(), entry.triggerHour, entry.triggerMinute,
                entry.value, entry.durationMinutes);
  if (entry.dependencyPin.length() > 0) {
    Serial.printf(" dep=%s==%d", entry.dependencyPin.c_str(), entry.dependencyValue);
  }
  Serial.println();
}

/// Callback – rejestruje lub usuwa zależność AND na poziomie buttona
void onButtonDependency(String dependentPin, int minValue, String depPin, int depValue, String label) {
  if (depPin.length() == 0) {
    // Brak pola dependency w JSON → usuń zależność dla tego pinu
    if (buttonDeps.erase(dependentPin)) {
      Serial.printf("[DEP] %s – dependency cleared\n", dependentPin.c_str());
    }
    return;
  }
  ButtonDep dep;
  dep.depPin = depPin;
  dep.depValue = depValue;
  dep.minValue = minValue;
  buttonDeps[dependentPin] = dep;
  Serial.printf("[DEP] %s depends on %s==%d\n",
                dependentPin.c_str(), depPin.c_str(), depValue);
  applyPinState(dependentPin);
}

/// Callback – czyści harmonogramy przed załadowaniem nowych z Firebase
/// pin == "" → wyczyść tylko schedules (reverty mają własny timer)
/// pin != "" → wyczyść schedules + reverts tylko dla danego pinu
void onScheduleClear(const String& pin) {
  if (pin.length() == 0) {
    // Czyścimy tylko schedules – pending reverts muszą przetrwać
    schedules.clear();
    Serial.println("[SCHEDULE] Cleared schedules (reverts preserved)");
  } else {
    schedules.erase(
      std::remove_if(schedules.begin(), schedules.end(),
        [&pin](const ScheduleEntry& e) { return e.pin == pin; }),
      schedules.end()
    );
    // Usuń reverty dla tego pinu (bezpieczne – zmiana konfiguracji)
    reverts.erase(
      std::remove_if(reverts.begin(), reverts.end(),
        [&pin](const PendingRevert& r) { return r.pinName == pin; }),
      reverts.end()
    );
    Serial.printf("[SCHEDULE] Cleared pin=%s\n", pin.c_str());
  }
}

// ---- Callback Firebase ----
void onFirebaseData(String key, String value) {
  if (key == "time") {
    timeSnapPoint = timeService.snapTimePoint(std::stoll(value.c_str()));
  }
  if (key == "gpio2") {
    ledState = (value.toInt() != 0);
    // LED_BUILTIN jest teraz sterowany przez pętlę synchronizacji pinów (krok 2)
  }
  if (key == "pingRequest") {
    // Odpowiadaj tylko jeśli to NOWY ping (ignoruj echo z full-JSON streamu)
    if (value != lastPingValue) {
      lastPingValue = value;
      pendingPing = true;
    }
  }

  // Śledź stan pinów – przy każdej zmianie zastosuj na pinie + pinach zależnych
  if (key != "time" && key != "pingRequest" && key != "online" &&
      key != "name" && key != "type" && key != "lastSeen" && key != "createdAt" &&
      key != "dependency" && key != "schedule" && key != "label" && key != "gpio" &&
      !key.startsWith("-")) {
    pinStates[key] = value.toInt();
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
  firebaseService.setDependencyCallback(onButtonDependency);
  firebaseService.setModeCallback(onPinMode);
  firebaseService.setDeviceTime();
  firebaseService.setOnline(true);    // zgłoś obecność w bazie

  startUs = micros64();
  pinMode(LED_BUILTIN, OUTPUT);
}

// ---- Loop ----
void loop() {
  otaService.handle();
  bool hadStreamData = firebaseService.firebaseStream();

  // Po przetworzeniu streamu – zastosuj stan na wszystkich pinach
  if (hadStreamData) {
    Serial.printf("[LOOP] stream data, pinStates=%d buttonDeps=%d\n", pinStates.size(), buttonDeps.size());
    for (auto const& kv : pinStates) {

      // ── Bramka AND (dependency) – wspólna dla wszystkich trybów ──
      int targetValue = kv.second;
      auto depIt = buttonDeps.find(kv.first);
      if (depIt != buttonDeps.end()) {
        int depState = pinStates.count(depIt->second.depPin) ? pinStates[depIt->second.depPin] : -1;
        if (depState != depIt->second.depValue) {
          targetValue = depIt->second.minValue;
        }
      }

      // Servo – ustaw kąt
      // PWM – ustaw wypełnienie (analogWrite 0–255)
      auto modeIt = pinModes.find(kv.first);
      if (modeIt != pinModes.end()) {
        if (modeIt->second == "servo") {
          auto srvIt = servos.find(kv.first);
          if (srvIt != servos.end()) {
            int angle = constrain(targetValue, 0, 180);
            srvIt->second.write(angle);
            Serial.printf("[SERVO] %s → %d°\n", kv.first.c_str(), angle);
          }
          continue;
        }
        if (modeIt->second == "pwm") {
          int physicalPin = virtualPinToPhysical(kv.first);
          if (physicalPin >= 0) {
            int duty = constrain(targetValue, 0, 255);
            analogWrite(physicalPin, duty);
            Serial.printf("[PWM] %s (pin=%d) → %d/255\n", kv.first.c_str(), physicalPin, duty);
          }
          continue;
        }
      }
      // Output – standardowa ścieżka (applyPinState też sprawdza buttonDeps)
      applyPinState(kv.first);
    }
  }

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

  // 1. Sprawdź reverty (powrót pinu do minValue) – używamy micros64() (monotoniczny)
  uint64_t nowUs = micros64();
  for (auto it = reverts.begin(); it != reverts.end(); ) {
    if (nowUs >= it->revertAtUs) {
      int physicalPin = virtualPinToPhysical(it->pinName);
      if (physicalPin >= 0) {
        digitalWrite(physicalPin, it->revertValue ? HIGH : LOW);
        Serial.printf("[SCHEDULE] Revert %s (pin=%d) → %d\n",
                      it->pinName.c_str(), physicalPin, it->revertValue);
      } else {
        Serial.printf("[SCHEDULE] Revert virtual %s → %d\n",
                      it->pinName.c_str(), it->revertValue);
      }
      firebaseService.setData("state/" + std::string(it->pinName.c_str()), it->revertValue);
      it = reverts.erase(it);
    } else {
      ++it;
    }
  }

  // 2. Sprawdź harmonogramy (czasowe) – jedna ścieżka wirtualna
  for (const auto& entry : schedules) {
    if (timeNow.hour != entry.triggerHour || timeNow.minute != entry.triggerMinute)
      continue;

    String key = firedKey(entry.pin, entry.triggerHour, entry.triggerMinute);
    if (firedToday.count(key)) continue;  // już odpalone dziś

    // ── Sprawdź zależność AND (button-level) – jeśli warunek nie spełniony, pomiń ──
    if (entry.dependencyPin.length() > 0) {
      int depState = pinStates.count(entry.dependencyPin) ? pinStates[entry.dependencyPin] : -1;
      if (depState != entry.dependencyValue) {
        Serial.printf("[SCHEDULE] SKIP %s – AND dependency %s==%d (actual=%d)\n",
                      entry.pin.c_str(), entry.dependencyPin.c_str(),
                      entry.dependencyValue, depState);
        continue;
      }
    }

    // Zawsze aktualizuj Firebase i oznacz jako odpalone (zapobiega spamowi)
    firebaseService.setData("state/" + std::string(entry.pin.c_str()), entry.value);
    firedToday.insert(key);

    // Mapuj wirtualną nazwę na fizyczny pin – tylko jeśli istnieje, steruj sprzętem
    int physicalPin = virtualPinToPhysical(entry.pin);
    if (physicalPin >= 0) {
      pinMode(physicalPin, OUTPUT);
      digitalWrite(physicalPin, entry.value ? HIGH : LOW);
      Serial.printf("[SCHEDULE] FIRE %s (pin=%d) → %d", entry.pin.c_str(), physicalPin, entry.value);
    } else {
      Serial.printf("[SCHEDULE] FIRE virtual %s → %d", entry.pin.c_str(), entry.value);
    }

    // Zaplanuj revert jeśli durationMinutes > 0 (zawsze – również dla wirtualnych)
    if (entry.durationMinutes > 0) {
      PendingRevert rev;
      rev.pinName = entry.pin;
      rev.revertValue = entry.minValue;
      rev.revertAtUs = micros64() + uint64_t(entry.durationMinutes) * 60ULL * 1000000ULL;
      reverts.push_back(rev);
      Serial.printf(" | revert za %dmin → %d", entry.durationMinutes, entry.minValue);
    }
    Serial.println();
  }

  // ── Odpytywanie wejść cyfrowych (co ~500ms) ────────────────────────
  if (nowUs - lastInputPollUs >= 500000ULL) {
    lastInputPollUs = nowUs;
    for (auto const& kv : pinModes) {
      if (kv.second != "input") continue;
      int physicalPin = virtualPinToPhysical(kv.first);
      if (physicalPin < 0) continue;
      int val = digitalRead(physicalPin);
      int prev = inputStates.count(kv.first) ? inputStates[kv.first] : -1;
      if (val != prev) {
        inputStates[kv.first] = val;
        firebaseService.setData("state/" + std::string(kv.first.c_str()), val);
        Serial.printf("[INPUT] %s (pin=%d) → %d\n", kv.first.c_str(), physicalPin, val);
      }
    }
  }

  delay(100);
}