# MyDot

Arduino library for the Microeden MyDot board and the Microeden cloud platform.

## Installation

Install **MyDot** from the Arduino IDE Library Manager. The IDE installs the declared
dependencies when possible: Adafruit NeoPixel, Adafruit SSD1306, Adafruit BME680,
PubSubClient, and ArduinoJson.

The onboard MyDot environmental sensor is a BME690; the implementation uses the
compatible Adafruit BME680 driver API.

The carrier's DRV8830 motor driver is addressed at I2C address `0x68` (7-bit).
Its A0 and A1 address pins are tied to +5V on the V1.0 schematic.

## Examples

Open `File > Examples > MyDot` in the Arduino IDE. Each example is self-contained
and includes a `microeden_secrets.h` file next to its sketch. It contains only
placeholders: replace the four `SECRET_*` values locally before using Wi-Fi or cloud
examples. Never commit real credentials.

The examples cover board initialization, buttons, relay, pixels, display, sensors,
fan, SD card, Wi-Fi signal, time, cloud publishing, widgets, commands, and color
conversion. Each sketch focuses on one library capability.
Use **HardwareCheck** to verify the relay and NeoPixels automatically and print
the pin values selected at compile time.
Diagnostic examples use the Serial Monitor at 115200 baud; **Display** demonstrates OLED output.

## Supported boards and pin mapping

The library supports ESP32, SAMD, and RP2040 Wi-Fi boards. For an **Arduino Nano
ESP32** with the MyDot shield, it uses the board
aliases from the MyDot pinout: `A7` (button A), `D4` (button B), `D2` (relay),
`D3` (LEDs), and `D10` (SD card). This works with either Arduino pin numbering
or **By GPIO number (legacy)** selected in the Arduino IDE. For NeoPixels, MyDot
converts `D3` to its physical ESP32 GPIO before initializing the ESP32 LED driver.

## ESP32 TLS

`beginCloud()` automatically configures the bundled Let’s Encrypt **ISRG Root X1**
certificate on ESP32. It does not use insecure TLS. `beginWiFi()` configures NTP on
ESP32, so call it before `beginCloud()` and allow the connection to complete.

## Carrier power

Power the MyDot carrier through its external power jack when using the relay,
NeoPixels, or fan driver. USB powers the Arduino Nano ESP32, but does not supply
the carrier output stages.
