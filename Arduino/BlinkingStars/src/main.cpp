#include <Arduino.h>
#include <TimeOut.h>

const int analogInPin = A0;
const int controlPin = 4;

const int configPin_1 = 16;
const int configPin_2 = 5;

int analogValueTemp = 2000;
int fadeDirection = 1;
int fadeValue = 0;
int easeType = 0;
int minValue = 0;
int maxValue = 1023;
int step = 10;

Interval interval0;

void serialPrint(int value) {
    Serial.printf("\n");
    Serial.printf("value[%u]\r", value);  
}

int analogValue() {
  return map(analogRead(analogInPin), 0, 1023, 0, 200);
}

int easeTypeValue () {
  if (digitalRead(configPin_1) == HIGH){
    return 1;
  }
  if (digitalRead(configPin_2) == HIGH){
    return 2;
  }
  return 0;
}

void fade() {

  fadeValue = fadeValue + fadeDirection;

  easeType = easeTypeValue();
  // serialPrint(digitalRead(configPin_1));

  switch(easeType) {
    case 1:
      {
        if (fadeValue >= maxValue) {
          fadeValue = minValue;
        }
        else if (fadeValue <= minValue) {
          fadeDirection = step;
        }
      }
      break;
    case 2:
      {
        if (fadeValue >= maxValue) {
          fadeDirection = -step;
        }
        else if (fadeValue <= minValue) {
          fadeValue = maxValue;
        }
      }
      break;    
    default:
      {
        if (fadeValue >= maxValue) {
          fadeDirection = -step;
        }
        else if (fadeValue <= minValue) {
          fadeDirection = step;
        }
      }
  }

  analogWrite(controlPin, fadeValue);
}

void setup() {
  Serial.begin(9600);
  pinMode(analogInPin, OUTPUT);
  pinMode(controlPin, INPUT);
  pinMode(configPin_1, INPUT);
  pinMode(configPin_2, INPUT);
  digitalWrite(controlPin, 1);
  digitalWrite(configPin_1, 0);
  digitalWrite(configPin_2, 0);
  interval0.interval(15, fade);
}

void loop() {

  Interval::handler();

  int getAnalogValue = analogValue();

  if (abs(analogValueTemp - getAnalogValue) > 1) {
    interval0.cancel();
    analogValueTemp = getAnalogValue;
    interval0.interval(analogValue(), fade);
  }
}