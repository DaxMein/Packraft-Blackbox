#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

extern float temp, hum, press;
extern bool gpsActive;
extern bool sdAvailable;
extern int connectedClients;


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// GPS-Icon (16x8)
const unsigned char gpsIconActive [] PROGMEM = {
  0b00011000, 0b00000100,
  0b00110000, 0b00010010,
  0b01110000, 0b01001010,
  0b01111110, 0b00101010,
  0b01110000, 0b01001010,
  0b00110000, 0b00010010,
  0b01011000, 0b00000100,
  0b11100000, 0b00000000
};

const unsigned char gpsIconInactive [] PROGMEM = {
  0b00011000, 0b00000000,
  0b00110000, 0b00000000,
  0b01110000, 0b00000000,
  0b01111110, 0b00000000,
  0b01110000, 0b00000000,
  0b00110000, 0b00000000,
  0b01011000, 0b00000000,
  0b11100000, 0b00000000
};

// SD-Icon (16x8)
const unsigned char sdIconActive [] PROGMEM = {
  0b11111110, 0b00000001,
  0b10000001, 0b00000001,
  0b10000000, 0b10000010,
  0b10000000, 0b10000010,
  0b10000000, 0b10100100,
  0b10000000, 0b10010100,
  0b11111111, 0b10001000,
  0b00000000, 0b00000000
};

const unsigned char sdIconInactive [] PROGMEM = {
  0b11111110, 0b00000000,
  0b10000001, 0b00100010,
  0b10000000, 0b10010100,
  0b10000000, 0b10001000,
  0b10000000, 0b10010100,
  0b10000000, 0b10100010,
  0b11111111, 0b10000000,
  0b00000000, 0b00000000
};

// --- WiFi-Icon (8x8) ---
const unsigned char wifiIcon [] PROGMEM = {
  0b00000000,
  0b00111100,
  0b01000010,
  0b10011001,
  0b00100100,
  0b00000000,
  0b00011000,
  0b00000000
};


void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED nicht gefunden!");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Display OK");
  display.display();
}

void showBMEOnDisplay() {
  display.clearDisplay();

  // Icons oben rechts
  display.drawBitmap(112, 20, sdAvailable ? sdIconActive : sdIconInactive, 16, 8, SSD1306_WHITE);
  display.drawBitmap(112, 0, gpsActive ? gpsIconActive : gpsIconInactive, 16, 8, SSD1306_WHITE);

  // Wetterdaten links
  display.setCursor(0, 0);
  display.printf("T: %.1f C", temp);
  display.setCursor(0, 10);
  display.printf("H: %.1f %%", hum);
  display.setCursor(0, 20);
  display.printf("P: %.1f hPa", press);
  // Zeile 2: Luftfeuchte + WiFi-Clients
display.setCursor(0, 10);
display.printf("H: %.1f %%", hum);

  // WiFi-Icon (8x8) + Client-Anzahl
  display.drawBitmap(112, 10, wifiIcon, 8, 8, SSD1306_WHITE);
  display.setCursor(122, 10);
  display.printf("%d", connectedClients);


  display.display();
}

void updateDisplay() {
  showBMEOnDisplay();
}

