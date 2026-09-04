/*
 * CloudPublish
 * Publishes a text value to the Microeden cloud every five seconds.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  // On ESP32, beginCloud configures the bundled ISRG Root X1 CA automatically.
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
}

void loop() {
  dot.run();
  // Queue and send a value only after MQTT has connected.
  if (dot.isCloudConnected()) {
    dot.writeKeyWord("message", "Hello from MyDot");
    dot.sendCloud();
    delay(5000);
  }
}
