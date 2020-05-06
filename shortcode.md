# MD
```
Ctrl + Shift + V              // Markdown file preview 
```

# Port Scan
```
chgport - show all connecting device
```

# Platformio update
```
- run platformio terminal
- platformio run -e esp12e -t upload --upload-port=192.168.1.104
```

# RF Pilot
```
	ON	OFF
A	4474193	4474196
B	4477265	4477268
C	4478033	4478036
D	4478225	4478228
```

# ESP Change firmware
```
pip install esptool // install esptools
esptool.py --port COM3 erase_flash
esptool.py --chip esp8266 --port COM3 write_flash --flash_mode dio --flash_size detect 0x0 "C:/projekty/Micropython/esp8266-20191220-v1.12.bin"

ampy --port COM3 ls
ampy --port COM3 get boot.py boot.py
ampy --port COM3 get main.py main.py

ampy --port COM3 put main.py


ampy --port COM3 mkdir lib
ampy --port COM3 ls
ampy --port COM3 rm test.py
ampy --port COM3 rmdir /lib
ampy --port COM3 run main.py
```