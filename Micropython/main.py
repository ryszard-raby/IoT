print("ESP8266")

#import blynklib_mp as blynklib
import blynklib

import network
import utime as time
from machine import Pin

WIFI_SSID = ''
WIFI_PASS = ''
BLYNK_AUTH = ''


print("Connecting to WiFi network '{}'".format(WIFI_SSID))
wifi = network.WLAN(network.STA_IF)
wifi.active(True)
wifi.connect(WIFI_SSID, WIFI_PASS)
while not wifi.isconnected():
    time.sleep(1)
    print('WiFi connect retry ...')
print('WiFi IP:', wifi.ifconfig()[0])

print("Connecting to Blynk server...")
blynk = blynklib.Blynk(BLYNK_AUTH)
adc = machine.ADC(0)


@blynk.handle_event('read V1')
def read_handler(vpin):
    p_value = adc.read()
    print('Current potentiometer value={}'.format(p_value))
    blynk.virtual_write(vpin, p_value)


while True:
    blynk.run()