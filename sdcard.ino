#include <SD.h>
extern float gpsLat;
extern float gpsLon;
extern bool gpsActive;
extern float temp, hum, press;


#define SD_CS 4
bool sdAvailable = false;

void initSD() {
  if (!SD.begin(SD_CS)) {
    Serial.println("SD-Initialisierung fehlgeschlagen!");
    sdAvailable = false;
    return;
  }

  File testFile = SD.open("/test.txt", FILE_WRITE);
  if (testFile) {
    testFile.println("SD test OK");
    testFile.close();
    Serial.println("SD-Karte bereit.");
    sdAvailable = true;
  } else {
    Serial.println("SD-Dateitest fehlgeschlagen!");
    sdAvailable = false;
    return;
  }

  // CSV-Header nur schreiben, wenn Datei noch nicht existiert
  if (!SD.exists("/log.csv")) {
    File logFile = SD.open("/log.csv", FILE_WRITE);
    if (logFile) {
      logFile.println("Zeit (s),Temperatur (°C),Feuchtigkeit (%),Druck (hPa),Latitude,Longitude");
      logFile.close();
      Serial.println("Header in log.csv geschrieben.");
    }
  }
}


//logging

void logDataToSD() {
  if (!sdAvailable) return;

  File logFile = SD.open("/log.csv", FILE_APPEND);
  if (!logFile) {
    Serial.println("Fehler beim Öffnen von log.csv");
    return;
  }

  unsigned long t = millis() / 1000;
  logFile.print(t);             // Zeit
  logFile.print(",");
  logFile.print(temp, 1);       // Temperatur
  logFile.print(",");
  logFile.print(hum, 1);        // Luftfeuchtigkeit
  logFile.print(",");
  logFile.print(press, 1);      // Luftdruck
  logFile.print(",");

  if (gpsActive) {
    logFile.print(gpsLat, 6);   // Latitude
    logFile.print(",");
    logFile.println(gpsLon, 6); // Longitude
  } else {
    logFile.println("0.0,0.0");
  }

  logFile.close();
}
