#pragma once

#include <Arduino.h>
#include <Servo.h>
#include <map>

// ---- Struktura zależności AND na poziomie buttona ----
struct ButtonDep {
  String depPin;     // pin od którego zależy (np. "aktywacja")
  int depValue;      // oczekiwana wartość
  int minValue;      // wartość do wymuszenia gdy warunek nie spełniony
};

// ---- Forward declaration ----
class FirebaseService;

/// Zarządza stanem pinów, trybami (output/input/servo/pwm),
/// zależnościami AND oraz odpytywaniem wejść cyfrowych.
class PinManager {
public:
  /// Jedyny punkt mapowania wirtualnej nazwy pinu na fizyczny pin ESP8266.
  static int virtualPinToPhysical(const String& name) {
    if (name.startsWith("gpio") || name.startsWith("GPIO")) {
      return name.substring(4).toInt();
    }
    if (name == "LED_BUILTIN" || name == "led") return LED_BUILTIN;
    return -1;
  }

  // ---- Stan pinów ----
  std::map<String, int> pinStates;       // bieżący stan (z Firebase)
  std::map<String, String> pinModes;     // "output" | "input" | "servo" | "pwm"
  std::map<String, Servo> servos;        // instancje Servo
  std::map<String, int> inputStates;     // ostatni odczyt wejścia
  std::map<String, ButtonDep> buttonDeps; // zależności AND

  uint64_t lastInputPollUs = 0;          // micros64() ostatniego odpytywania wejść

  // ========== Metody ==========

  /// Ustawia fizyczny pin z bramką AND – tylko dla trybu output.
  void applyPinState(const String& name) {
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

  /// Callback – odbiera tryb pinu z Firebase i konfiguruje sprzęt.
  void onPinMode(const String& pin, const String& mode) {
    if (mode.length() == 0) return;

    auto it = pinModes.find(pin);
    if (it != pinModes.end() && it->second == mode) return;

    // "output" jest domyślny – nie nadpisuj nim jawnie ustawionego trybu
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
        inputStates[pin] = -1;
        Serial.printf("[MODE] %s → INPUT (pin=%d)\n", pin.c_str(), physicalPin);
      } else {
        Serial.printf("[MODE] %s → INPUT (virtual)\n", pin.c_str());
      }
    } else if (mode == "servo") {
      if (physicalPin >= 0) {
        auto sit = servos.find(pin);
        if (sit != servos.end()) sit->second.detach();
        Servo srv;
        srv.attach(physicalPin);
        servos[pin] = srv;
        Serial.printf("[MODE] %s → SERVO attached to pin %d\n", pin.c_str(), physicalPin);
      } else {
        Serial.printf("[MODE] %s → SERVO (virtual)\n", pin.c_str());
      }
    } else if (mode == "pwm") {
      if (physicalPin >= 0) {
        auto sit = servos.find(pin);
        if (sit != servos.end()) { sit->second.detach(); servos.erase(sit); }
        inputStates.erase(pin);
        pinMode(physicalPin, OUTPUT);
        analogWrite(physicalPin, 0);
        Serial.printf("[MODE] %s → PWM (pin=%d)\n", pin.c_str(), physicalPin);
      } else {
        Serial.printf("[MODE] %s → PWM (virtual)\n", pin.c_str());
      }
    } else {
      // output (domyślny)
      auto sit = servos.find(pin);
      if (sit != servos.end()) { sit->second.detach(); servos.erase(sit); }
      inputStates.erase(pin);
      Serial.printf("[MODE] %s → OUTPUT\n", pin.c_str());
      applyPinState(pin);
    }
  }

  /// Callback – rejestruje lub usuwa zależność AND na poziomie buttona.
  void onButtonDependency(String dependentPin, int minValue, String depPin, int depValue, String label) {
    if (depPin.length() == 0) {
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

  /// Po przetworzeniu streamu Firebase – stosuje stan na wszystkich pinach
  /// (servo → kąt, pwm → analogWrite, output → digitalWrite z bramką AND).
  void applyAllPinStates() {
    Serial.printf("[PIN] applyAllPinStates: pinStates=%d buttonDeps=%d\n",
                  pinStates.size(), buttonDeps.size());
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
      // Output – standardowa ścieżka
      applyPinState(kv.first);
    }
  }

  /// Odpytywanie wejść cyfrowych co ~500ms – zmiany wysyła do Firebase.
  void pollInputs(FirebaseService& firebase);
};

// Implementacja pollInputs (potrzebuje FirebaseService::setData)
#include "FirebaseService.h"

inline void PinManager::pollInputs(FirebaseService& firebase) {
  uint64_t nowUs = micros64();
  if (nowUs - lastInputPollUs < 500000ULL) return;
  lastInputPollUs = nowUs;

  for (auto const& kv : pinModes) {
    if (kv.second != "input") continue;
    int physicalPin = virtualPinToPhysical(kv.first);
    if (physicalPin < 0) continue;
    int val = digitalRead(physicalPin);
    int prev = inputStates.count(kv.first) ? inputStates[kv.first] : -1;
    if (val != prev) {
      inputStates[kv.first] = val;
      firebase.setData("state/" + std::string(kv.first.c_str()), val);
      Serial.printf("[INPUT] %s (pin=%d) → %d\n", kv.first.c_str(), physicalPin, val);
    }
  }
}
