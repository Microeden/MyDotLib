/*
 * Pixels
 * Sets all MyDot NeoPixels to blue at a low brightness level.
 *
 * Hardware note: the MyDot carrier MUST be powered through its external
 * power jack. The Nano USB connection alone does not power the NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  // Set brightness before assigning the color.
  dot.setBrightness(32);
  dot.setAllPixels(0, 64, 255);
}

void loop() {
}
