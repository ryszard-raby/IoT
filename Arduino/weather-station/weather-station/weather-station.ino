#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Adafruit_Sensor.h>

#include <C:/auth/auth.h>
#include <C:/auth/blynkToken.h>
// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = Token_WeatherStation;

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
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  
  if (pinValue == 0) digitalWrite(2, LOW); 
  if (pinValue == 1) digitalWrite(2, HIGH); 
  // process received value
}

double val = 0.0;


void sendSensor()
{
  float h = dht.readHumidity();
  float t = dht.readTemperature(); // or dht.readTemperature(true) for Fahrenheit
  
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V5, h);
  Blynk.virtualWrite(V6, t);

  
 // int WiFiStrenght = WiFi.RSSI();
  val = analogRead(A0);
  Blynk.virtualWrite(V7, val);
  val = map(val, 0, 1024, 100, 0);
  Blynk.virtualWrite(V8, val);
}

void setup()
{
  pinMode(16, OUTPUT);
  // Debug console
  Serial.begin(9600);
  Blynk.begin(auth, ssid, pass);
  // You can also specify server:
  //Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 8442);
  //Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8442);
  timer.setInterval(1000L, sendSensor);
}

void loop()
{
  Blynk.run();
  timer.run();
}

