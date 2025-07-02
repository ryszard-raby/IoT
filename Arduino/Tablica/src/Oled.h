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

int rowMargin = 25;

class Slot {
public:
    int x;
    int y;
    String text;
    int size;

    Slot(int x, int y, String text, int size) : x(x), y(y), text(text), size(size) {};
};

class Row {
public:
    int line;
    Slot col1;
    Slot col2;
    Slot col3;

    Row(int line, Slot col1, Slot col2, Slot col3) : col1(0, 10 * line + rowMargin, col1.text, 1), col2(20, 10 * line + rowMargin, col2.text, 1), col3(90, 10 * line + rowMargin, col3.text, 1) {}
};

class Oled {
    const int quantity = 10;

public:
    std::vector<Slot> stack;
    std::vector<Slot> rows;

    void add(int x, int y, const String& text, int size) {
        stack.push_back(Slot(x, y, text, size));
    }

    void addRow(int line, const String& col1, const String& col2, const String& col3) {
        rows.push_back(Slot(0, 10 * line + rowMargin, col1, 1));
        rows.push_back(Slot(20, 10 * line + rowMargin, col2, 1));
        rows.push_back(Slot(90, 10 * line + rowMargin, col3, 1));
    }

    void updateRow(int line, const String& col1, const String& col2, const String& col3) {
        rows[line * 3].text = col1;
        rows[line * 3 + 1].text = col2;
        rows[line * 3 + 2].text = col3;
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
        for(size_t i = 0; i < rows.size(); ++i) {
            display.setCursor(rows[i].x, rows[i].y);
            display.setTextSize(rows[i].size);
            display.print(rows[i].text);
        }
        display.display();
    }

};

