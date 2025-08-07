HardwareSerial gpsSerial(1);
float gpsLat = 0.0;
float gpsLon = 0.0;
bool gpsActive = false;

extern float gpsLat;
extern float gpsLon;


void initGPS() {
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17
  Serial.println("GPS initialisiert.");
}

void readGPS() {
  gpsActive = false;
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    Serial.print(c);
    if (c == '$') gpsActive = true;
  }
}

