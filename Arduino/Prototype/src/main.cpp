#include <Arduino.h>
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

/// Ustawia fizyczny pin z bramką AND – wywoływana tylko gdy zmieni się stan w Firebase.
/// Jedyny punkt styku logiki wirtualnej ze sprzętem (przez virtualPinToPhysical).
void applyPinState(const String& name) {
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

/// Callback – rejestruje zależność AND na poziomie buttona
void onButtonDependency(String dependentPin, int minValue, String depPin, int depValue, String label) {
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
  firebaseService.setDeviceTime();
  firebaseService.setOnline(true);    // zgłoś obecność w bazie

  startUs = micros64();
  pinMode(LED_BUILTIN, OUTPUT);
}

// ---- Loop ----
void loop() {
  otaService.handle();
  bool hadStreamData = firebaseService.firebaseStream();

  // Po przetworzeniu streamu – zastosuj stan na wszystkich pinach (z bramką AND)
  if (hadStreamData) {
    Serial.printf("[LOOP] stream data, pinStates=%d buttonDeps=%d\n", pinStates.size(), buttonDeps.size());
    for (auto const& kv : pinStates) {
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

  delay(100);
}