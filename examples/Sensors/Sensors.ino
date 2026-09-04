/*
 * Sensors
 * Reads the onboard BME690 sensor and prints all available measurements.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  Serial.begin(115200);
  dot.begin();
}

void loop() {
  // A successful reading updates all sensor getter values.
  if (dot.readSensors()) {
    Serial.print("Temperature: ");
    Serial.print(dot.getTemperature(), 1);
    Serial.println(" C");

    Serial.print("Humidity: ");
    Serial.print(dot.getHumidity(), 1);
    Serial.println(" %");

    Serial.print("Pressure: ");
    Serial.print(dot.getPressure(), 1);
    Serial.println(" hPa");

    Serial.print("Gas resistance: ");
    Serial.print(dot.getGasResistance(), 1);
    Serial.println(" kOhm");

    Serial.println();
  }
  delay(2000);
}
