/*
 * Fan
 * Sets the fan driver to 50 percent forward speed.
 *
 * Hardware note: the MyDot carrier MUST be powered through its external
 * power jack. The Nano USB connection alone does not power the fan driver.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  dot.setFanSpeed(50);
}

void loop() {
}
