/*
 * Display
 * Shows a text message on the MyDot OLED display.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  dot.displayPrint("Hello, MyDot!", 0, 24, 2);
}

void loop() {
}
