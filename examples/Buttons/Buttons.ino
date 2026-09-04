/*
 * Buttons
 * Prints a message to the Serial Monitor when either MyDot button is clicked.
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
  Serial.println("Press button A or B");
}

void loop() {
  if (dot.isButtonAClicked()) {
    Serial.println("Button A clicked");
  }

  if (dot.isButtonBClicked()) {
    Serial.println("Button B clicked");
  }
}
