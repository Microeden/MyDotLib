/*
 * ColorConversion
 * Converts a hexadecimal color to RGB values and converts it back to hex.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

void setup() {
  Serial.begin(115200);
  uint8_t red, green, blue;
  MyDot::hexToRGB("#0040FF", red, green, blue);
  Serial.println(MyDot::rgbToHex(red, green, blue));
}

void loop() {
}
