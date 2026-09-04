/*
 * SDCard
 * Creates hello.txt on the MyDot SD card and writes one line to it.
 *
 * Hardware note: power the MyDot carrier through its external power jack.
 * The Nano USB connection alone does not power the relay or NeoPixels.
 */
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  dot.begin();
  // Write only if the SD card was initialized successfully.
  if (dot.beginSD()) {
    dot.writeFile("/hello.txt", "Hello from MyDot\n");
  }
}

void loop() {
}
