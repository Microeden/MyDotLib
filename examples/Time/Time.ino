/*
 * Time
 * Connects to Wi-Fi and prints the current NTP-synchronized time.
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
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
}

void loop() {
  Serial.println(dot.getFormattedTime(2));
  delay(1000);
}
