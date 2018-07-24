#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>
#include <stdio.h>
#include <stdlib.h>

#include <C:/auth/auth.h>
#include <blynkAuth.h>
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = BlynkToken;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = AuthSsid;
char pass[] = AuthPass;

#define DHTPIN 14
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// This function will be called every time Slider Widget
// in Blynk app writes values to the Virtual Pin V1

double val = 0.0;

int In1 = 16; // step motor1
int In2 = 14; // dir motor 1
int In3 = 13; // step motor2
int In4 = 12; // dir motor2

int step_m1 = 0;
int step_m2 = 0;

int start(int motor,int steps){
    for(int i=0; i<= steps; i++){
      digitalWrite(motor, HIGH);
      delay(2); 
      digitalWrite(motor, LOW);
      delay(2); 
    }
  }

BLYNK_WRITE(V0){
  int pinValue = param.asInt();
  analogWrite(A0, param.asInt() / 4);
}

BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  if (param.asInt()==1) start(In1,step_m1);

  // process received value
}

BLYNK_WRITE(V2)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  if (param.asInt()==1) digitalWrite(In2, LOW);
  else digitalWrite(In2, HIGH);

  // process received value
}

BLYNK_WRITE(V3)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  if (param.asInt()==1) start(In3,step_m2);

  // process received value
}

BLYNK_WRITE(V4)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  if (param.asInt()==1) digitalWrite(In4, LOW);
  else digitalWrite(In4, HIGH);

  // process received value
}

BLYNK_WRITE(V5){
  step_m1 = param.asInt();
}

BLYNK_WRITE(V6){
  step_m2 = param.asInt();
}

void setup()
{
  pinMode(In1, OUTPUT);
  pinMode(In2, OUTPUT);
  pinMode(In3, OUTPUT);
  pinMode(In4, OUTPUT);
  digitalWrite(In1, LOW);
  digitalWrite(In2, LOW);
  digitalWrite(In3, LOW);
  digitalWrite(In4, LOW);
  pinMode(A0, OUTPUT);
  // Debug console
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 8442);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8442);
  //timer.setInterval(1000L, sendSensor);
}

void loop()
{
  Blynk.run();
  timer.run();
}

