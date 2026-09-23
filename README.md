# MyDot

Documentation: [English](MyDot_Documentation.md) · [Italiano](MyDot_Documentation_IT.md)

Arduino library for the Microeden MyDot board and the Microeden cloud platform.

## Installation

Install **MyDot** from the Arduino IDE Library Manager. The IDE installs the declared
dependencies when possible: Adafruit NeoPixel, Adafruit SSD1306, Adafruit BME680,
PubSubClient, ArduinoJson, SD, and WiFiNINA.

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
conversion. **MyDotDevStudioBridge** additionally exposes the complete API over a
newline-delimited serial protocol and can save encrypted runtime sequences to
microSD for the Dev Studio web editor, including generic I²C primitives that can
be wrapped by dedicated sensor blocks. Each sketch focuses on one library capability.
The complete bridge protocol is documented in
[`examples/MyDotDevStudioBridge/COMMANDS.md`](examples/MyDotDevStudioBridge/COMMANDS.md).
An Italian translation is available in
[`examples/MyDotDevStudioBridge/COMMANDS_IT.md`](examples/MyDotDevStudioBridge/COMMANDS_IT.md).
Use **HardwareCheck** to verify the relay and NeoPixels automatically and print
the pin values selected at compile time.
Diagnostic examples use the Serial Monitor at 115200 baud; **Display** demonstrates OLED output.

## Supported boards and pin mapping

The library supports ESP32, SAMD, RP2040, and Mbed Nano Wi-Fi boards. The MyDot
carrier uses the standard Nano header on every supported Nano, so its signals
are always `A7` (button A), `D4` (button B), `D2` (relay), `D3` (LEDs), and
`D10` (SD card). On the Nano ESP32 these aliases also work with either Arduino
pin numbering or **By GPIO number (legacy)** selected in the Arduino IDE; MyDot
converts `D3` to its physical ESP32 GPIO before initializing the LED driver.

## ESP32 TLS

`beginCloud()` automatically configures the bundled Let’s Encrypt **ISRG Root X1**
certificate on ESP32. It does not use insecure TLS. `beginWiFi()` configures NTP on
ESP32, so call it before `beginCloud()` and keep calling `dot.run()` in `loop()`.
If Wi-Fi drops, `run()` disconnects the stale MQTT session and retries Wi-Fi and
MQTT automatically.

Call `stopNetworkServices()` when an interactive editor session must release
its Wi-Fi and MQTT resources. It clears the cloud callback and prevents
`run()` from reconnecting until the program calls `beginWiFi()`/`beginCloud()`
again. The Dev Studio Bridge exposes this operation as `EXEC NETWORK STOP`;
the ordinary `STOP` command intentionally only stops the resident sequence so
autonomous runtime behaviour is unchanged.

## Carrier power

Power the MyDot carrier through its external power jack when using the relay,
NeoPixels, or fan driver. USB powers the Arduino Nano ESP32, but does not supply
the carrier output stages.
