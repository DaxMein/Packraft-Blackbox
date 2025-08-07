#include <FS.h>
#include <SPIFFS.h>

// Forward declarations for functions defined in other files
void initBME();
void initGPS();
void initSD();
void initDisplay();
void readBME();
void readGPS();
void updateDisplay();
void logDataToSD();
void updateWebServer();
void initWiFi();
void updateConnectedClients();

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Packraft Blackbox...");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS konnte nicht gestartet werden");
    return;
  }
  Serial.println("SPIFFS initialized successfully");
  
  initBME();
  initGPS();
  initSD();
  initDisplay(); 
  initWiFi();  
  
  Serial.println("Setup completed");
}

unsigned long lastSDCheck = 0;
unsigned long lastLog = 0;

void loop() {
  readBME();
  readGPS();
  updateConnectedClients();

  // Check SD card every 3 seconds
  if (millis() - lastSDCheck > 3000) {
    initSD(); 
    lastSDCheck = millis();
  }

  // Log data every second
  if (millis() - lastLog > 1000) {
    logDataToSD();
    lastLog = millis();
  }

  updateDisplay(); 
  updateWebServer();
  
  delay(1000);
}
