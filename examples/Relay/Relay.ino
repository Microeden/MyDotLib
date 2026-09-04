/*
 * Relay
 * Toggles the MyDot relay every time button A is clicked.
 *
 * Hardware note: the MyDot carrier MUST be powered through its external
 * power jack. The Nano USB connection alone does not power the relay.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
}

void loop() {
  if (dot.isButtonAClicked()) {
    dot.toggleRelay();
  }
}
