#include <Wire.h>
#include <Adafruit_BME280.h>

#define SDA_PIN 21
#define SCL_PIN 22

float temp = 0;
float hum = 0;
float press = 0;

Adafruit_BME280 bme;

void initBME() {
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!bme.begin(0x76)) {
    Serial.println("BME280 nicht gefunden!");
    while (1);
  }
  Serial.println("BME280 bereit!");
}

void readBME() {
  temp = bme.readTemperature();
  hum = bme.readHumidity();
  press = bme.readPressure() / 100.0F;

  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print(" °C, Feuchte: ");
  Serial.print(hum);
  Serial.print(" %, Druck: ");
  Serial.print(press);
  Serial.println(" hPa");
}
