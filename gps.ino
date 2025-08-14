// gps.ino - GPS Modul für Packraft Blackbox (ohne externe Bibliothek)

HardwareSerial gpsSerial(1);

// Globale Variablen (werden von anderen Modulen verwendet)
float gpsLat = 0.0;
float gpsLon = 0.0;
bool gpsActive = false;

// Interne Variablen
unsigned long lastGPSFix = 0;
unsigned long lastGPSDebug = 0;

void initGPS() {
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=GPIO16, TX=GPIO17
  Serial.println("GPS initialisiert (ohne Bibliothek).");
  Serial.println("Warte auf GPS-Signal...");
}

void readGPS() {
  static String nmeaBuffer = "";
  static int invalidCount = 0;
  
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    
    if (c == '\n') {
      // NMEA Satz komplett
      if (nmeaBuffer.startsWith("$GPGGA") || nmeaBuffer.startsWith("$GNGGA")) {
        if (parseGGA(nmeaBuffer)) {
          lastGPSFix = millis();
          invalidCount = 0;
        } else {
          invalidCount++;
        }
      }
      else if (nmeaBuffer.startsWith("$GPRMC") || nmeaBuffer.startsWith("$GNRMC")) {
        // RMC als Backup falls GGA nicht funktioniert
        parseRMC(nmeaBuffer);
      }
      nmeaBuffer = "";
    } else if (c != '\r') {
      nmeaBuffer += c;
      // Verhindere Buffer-Überlauf
      if (nmeaBuffer.length() > 120) {
        nmeaBuffer = "";
      }
    }
  }
  
  // GPS als inaktiv markieren wenn 5 Sekunden kein Fix
  if (millis() - lastGPSFix > 5000) {
    if (gpsActive) {
      Serial.println("GPS-Signal verloren!");
      gpsActive = false;
    }
  }
  
  // Debug-Ausgabe alle 10 Sekunden
  if (millis() - lastGPSDebug > 10000) {
    lastGPSDebug = millis();
    if (gpsActive) {
      Serial.print("GPS OK - Lat: ");
      Serial.print(gpsLat, 6);
      Serial.print(", Lon: ");
      Serial.println(gpsLon, 6);
    } else {
      Serial.print("GPS wartet auf Signal... (");
      Serial.print(invalidCount);
      Serial.println(" ungültige Sätze)");
    }
  }
}

bool parseGGA(String nmea) {
  // $GPGGA,123456.00,5321.12345,N,01012.12345,E,1,08,1.0,100.0,M,40.0,M,,*67
  
  int commaIndex[15];
  int commaCount = 0;
  
  // Finde alle Kommas
  for (int i = 0; i < nmea.length() && commaCount < 15; i++) {
    if (nmea[i] == ',') {
      commaIndex[commaCount++] = i;
    }
  }
  
  if (commaCount >= 6) {
    // Fix Quality prüfen (Index 6)
    String fixStr = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
    int fixQuality = fixStr.toInt();
    
    if (fixQuality > 0) {  // 1 = GPS fix, 2 = DGPS fix
      // Latitude (Index 2-3)
      String latStr = nmea.substring(commaIndex[1] + 1, commaIndex[2]);
      String latDir = nmea.substring(commaIndex[2] + 1, commaIndex[3]);
      
      // Longitude (Index 4-5)
      String lonStr = nmea.substring(commaIndex[3] + 1, commaIndex[4]);
      String lonDir = nmea.substring(commaIndex[4] + 1, commaIndex[5]);
      
      if (latStr.length() >= 7 && lonStr.length() >= 7) {
        // Konvertiere NMEA Format (DDMM.MMMM) zu Dezimalgrad
        float latDeg = latStr.substring(0, 2).toFloat();
        float latMin = latStr.substring(2).toFloat();
        gpsLat = latDeg + (latMin / 60.0);
        if (latDir == "S") gpsLat = -gpsLat;
        
        float lonDeg = lonStr.substring(0, 3).toFloat();
        float lonMin = lonStr.substring(3).toFloat();
        gpsLon = lonDeg + (lonMin / 60.0);
        if (lonDir == "W") gpsLon = -gpsLon;
        
        // Validierung - Deutschland liegt etwa bei 47-55°N, 5-15°E
        if (gpsLat > 40 && gpsLat < 60 && gpsLon > 0 && gpsLon < 20) {
          if (!gpsActive) {
            Serial.println("GPS-Fix erhalten!");
            Serial.print("Position: ");
            Serial.print(gpsLat, 6);
            Serial.print(", ");
            Serial.println(gpsLon, 6);
          }
          gpsActive = true;
          return true;
        }
      }
    }
  }
  
  return false;
}

bool parseRMC(String nmea) {
  // $GPRMC,123456.00,A,5321.12345,N,01012.12345,E,0.5,45.0,010121,,,A*67
  // RMC als Backup, falls GGA nicht funktioniert
  
  int commaIndex[13];
  int commaCount = 0;
  
  for (int i = 0; i < nmea.length() && commaCount < 13; i++) {
    if (nmea[i] == ',') {
      commaIndex[commaCount++] = i;
    }
  }
  
  if (commaCount >= 7) {
    // Status prüfen (A = Active, V = Void)
    String status = nmea.substring(commaIndex[1] + 1, commaIndex[2]);
    
    if (status == "A") {
      // Latitude
      String latStr = nmea.substring(commaIndex[2] + 1, commaIndex[3]);
      String latDir = nmea.substring(commaIndex[3] + 1, commaIndex[4]);
      
      // Longitude
      String lonStr = nmea.substring(commaIndex[4] + 1, commaIndex[5]);
      String lonDir = nmea.substring(commaIndex[5] + 1, commaIndex[6]);
      
      if (latStr.length() >= 7 && lonStr.length() >= 7) {
        float latDeg = latStr.substring(0, 2).toFloat();
        float latMin = latStr.substring(2).toFloat();
        gpsLat = latDeg + (latMin / 60.0);
        if (latDir == "S") gpsLat = -gpsLat;
        
        float lonDeg = lonStr.substring(0, 3).toFloat();
        float lonMin = lonStr.substring(3).toFloat();
        gpsLon = lonDeg + (lonMin / 60.0);
        if (lonDir == "W") gpsLon = -gpsLon;
        
        // Validierung
        if (gpsLat > 40 && gpsLat < 60 && gpsLon > 0 && gpsLon < 20) {
          gpsActive = true;
          lastGPSFix = millis();
          return true;
        }
      }
    }
  }
  
  return false;
}

// Debug-Funktion zum Testen
void testGPS() {
  Serial.println("GPS Test:");
  Serial.print("  Active: ");
  Serial.println(gpsActive ? "JA" : "NEIN");
  Serial.print("  Latitude: ");
  Serial.println(gpsLat, 6);
  Serial.print("  Longitude: ");
  Serial.println(gpsLon, 6);
  Serial.print("  Letzter Fix vor: ");
  Serial.print((millis() - lastGPSFix) / 1000);
  Serial.println(" Sekunden");
}