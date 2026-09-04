/*
 * HardwareCheck
 * Automatically tests the relay and NeoPixels, then reports the selected
 * pin values through the Serial Monitor at 115200 baud.
 *
 * Hardware note: the MyDot carrier MUST be powered through its external
 * power jack. The Nano USB connection alone does not power these outputs.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

unsigned long lastStep = 0;
uint8_t step = 0;

void runStep() {
  // Advance one visible output test every three seconds.
  switch (step) {
    case 0:
      Serial.println("Relay ON");
      dot.setRelay(true);
      dot.clearPixels();
      break;

    case 1:
      Serial.println("Relay OFF");
      dot.setRelay(false);
      break;

    case 2:
      Serial.println("NeoPixels red");
      dot.setAllPixels(64, 0, 0);
      break;

    case 3:
      Serial.println("NeoPixels green");
      dot.setAllPixels(0, 64, 0);
      break;

    case 4:
      Serial.println("NeoPixels blue");
      dot.setAllPixels(0, 0, 64);
      break;

    default:
      Serial.println("NeoPixels off");
      dot.clearPixels();
      break;
  }

  step = (step + 1) % 6;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("MyDot hardware check");
  Serial.print("BUTTON_A = ");
  Serial.println(BUTTON_A);
  Serial.print("BUTTON_B = ");
  Serial.println(BUTTON_B);
  Serial.print("RELAY = ");
  Serial.println(RELAY);
  Serial.print("NEOPIXEL = ");
  Serial.println(PIN);
#if defined(ARDUINO_NANO_ESP32)
  Serial.print("NEOPIXEL GPIO = ");
  Serial.println(digitalPinToGPIONumber(PIN));
#endif
  Serial.print("DRV8830 I2C address = 0x");
  Serial.println(FAN_ADDRESS, HEX);

  dot.begin();
  // Report any driver fault before testing the other carrier outputs.
  Serial.print("DRV8830 fault = 0x");
  Serial.println(dot.getFanFault(), HEX);
  runStep();
}

void loop() {
  if (millis() - lastStep >= 3000) {
    lastStep = millis();
    runStep();
  }
}
