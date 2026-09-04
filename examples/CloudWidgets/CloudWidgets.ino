/*
 * CloudWidgets
 * Publishes a sensor value and example widget states to the Microeden cloud.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;
Level temperature("temperature", dot);
Switch relay("relay", dot);
ColorWheel color("color", dot);

void publishWidgets() {
  // Do not publish stale values when the sensor reading fails.
  if (!dot.readSensors()) {
    return;
  }

  temperature.write((int)dot.getTemperature());
  relay.write(false);
  color.write(0, 64, 255);
  dot.sendCloud();
}

void setup() {
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
  dot.setCloudSync(5000, publishWidgets);
}

void loop() {
  dot.run();
}
