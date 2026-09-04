/*
 * CloudCommands
 * Receives LED_ON and LED_OFF commands from the Microeden cloud.
 *
 * Hardware note: the MyDot carrier MUST be powered through its external
 * power jack. The Nano USB connection alone does not power the NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
}

void loop() {
  dot.run();
  // onCommand returns true once for each received matching command.
  if (dot.onCommand("LED_ON")) {
    dot.setAllPixels(0, 255, 0);
  }

  if (dot.onCommand("LED_OFF")) {
    dot.clearPixels();
  }
}
