/*
 * Basic
 * Initializes the MyDot board, Wi-Fi, and the Microeden cloud connection.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  // Connect to the Wi-Fi network defined in microeden_secrets.h.
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
}

void loop() {
  dot.run();
}
