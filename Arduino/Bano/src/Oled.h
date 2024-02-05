#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Screen address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int SDA_pin = 14; //Pin D5
int SCL_pin = 12; //Pin D6

class Slot {
public:
    int x;
    int y;
    String text;
    int size;

    Slot(int x, int y, String text, int size) : x(x), y(y), text(text), size(size) {};
};

class Oled {
    const int quantity = 10;

public:
    std::vector<Slot> stack;

    void add(int x, int y, const String& text, int size) {
        stack.push_back(Slot(x, y, text, size));
    }

    void init() {
        Wire.begin(SDA_pin ,SCL_pin, SCREEN_ADDRESS);

        display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
        display.setTextColor(SSD1306_WHITE);
        display.display();

        delay(1000);
    }

    void print() {
        display.clearDisplay();
        for(size_t i = 0; i <= stack.size(); ++i) {
            display.setCursor(stack[i].x, stack[i].y);
            display.setTextSize(stack[i].size);
            display.print(stack[i].text);
        }
        display.display();
    }

};

