# MyDot library documentation

## Overview

MyDot is an Arduino library for the Microeden MyDot carrier and the Microeden cloud platform. It provides one API for the board peripherals and for optional Wi-Fi/MQTT communication:

- two push-buttons;
- a relay output;
- twelve NeoPixel LEDs;
- the onboard OLED display;
- the onboard BME690 environmental sensor, accessed through the Adafruit BME680-compatible API;
- the SD card interface;
- the DRV8830 fan/motor driver;
- Wi-Fi, NTP time, and Microeden cloud services.

The public API is declared in `src/MyDot.h`. The implementation is in `src/MyDot.cpp`.

## Hardware and power requirements

The carrier should be powered through its external DC jack when using the relay, NeoPixels, fan driver, or other carrier output stages. USB powers the Arduino Nano ESP32, but it does not provide the carrier output-stage power required by these peripherals.

The onboard environmental sensor is a BME690. The library uses the compatible Adafruit BME680 driver interface for initialization and readings.

The declared architectures are ESP32, SAMD, RP2040, and Mbed Nano Wi-Fi boards. The Nano ESP32 mapping is documented below; other boards use the compile-time pin definitions in `MyDot.h` and may require a carrier-specific wiring check.

For an Arduino Nano ESP32, select **Arduino Nano ESP32** in the Arduino IDE. The library supports either the board's Arduino pin numbering or **By GPIO number (legacy)** setting. MyDot translates the Nano ESP32 `D3` LED alias to the physical ESP32 GPIO before initializing the NeoPixel driver.

## Installation

Install **MyDot** from the Arduino IDE Library Manager, or copy this repository into the Arduino libraries folder.

The library declares these dependencies:

- Adafruit NeoPixel;
- Adafruit SSD1306;
- Adafruit BME680 Library;
- PubSubClient;
- ArduinoJson;
- SD;
- WiFiNINA.

The usual Arduino IDE workflow is:

1. Open **Sketch > Include Library > Manage Libraries**.
2. Search for `MyDot`.
3. Install the library and its dependencies.
4. Open an example from **File > Examples > MyDot**.

## Minimal sketch

```cpp
#include <MyDot.h>

MyDot dot;

void setup() {
  dot.begin();
}

void loop() {
  dot.run();
}
```

`begin()` initializes the board peripherals. Call `run()` repeatedly from `loop()` when using Wi-Fi or cloud functions; it also maintains the Wi-Fi/MQTT connections and executes the configured cloud synchronization callback.

## Credentials and TLS

Cloud examples include a local `microeden_secrets.h` file. The file contains placeholders for credentials and must be edited locally:

```cpp
#define SECRET_WIFI_SSID "your-wifi-name"
#define SECRET_WIFI_PASSWORD "your-wifi-password"
#define SECRET_DEVICE_ID "your-device-id"
#define SECRET_DEVICE_TOKEN "your-device-token"
```

Never commit real Wi-Fi passwords, device tokens, or other private credentials.

On ESP32, `beginCloud()` configures the bundled ISRG Root X1 certificate and therefore uses certificate validation rather than insecure TLS. Call `beginWiFi()` before `beginCloud()` and call `run()` continuously from `loop()`.

Example connection order:

```cpp
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  Serial.begin(115200);
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
}

void loop() {
  dot.run();
}
```

`beginWiFi()` starts the connection and waits for up to 15 seconds during setup. If the access point is unavailable, setup continues and `run()` retries in the background. Do not call `beginWiFi()` repeatedly from `loop()`.

## Board pin mapping

The following aliases are used by the library on the MyDot carrier:

| Function | Arduino Nano ESP32 alias |
| --- | --- |
| Button A | `A7` |
| Button B | `D4` |
| Relay | `D2` |
| NeoPixels | `D3` |
| SD card chip select | `D10` |

The fan driver, OLED, and BME690 use the carrier I2C bus. The default addresses are exposed as constants in `MyDot.h`:

| Device | Constant | Address |
| --- | --- | --- |
| DRV8830 fan driver | `FAN_ADDRESS` | `0x68` |
| OLED | `SCREEN_ADDRESS` | `0x3C` |
| BME690 | `BME_ADDRESS` | `0x77` |

## Examples

Every example is self-contained and includes its own `microeden_secrets.h` placeholder file. Diagnostic examples use the Serial Monitor at **115200 baud**. The `Display` example is the example dedicated to OLED output; the other examples use Serial for diagnostics.

| Example | Demonstrates |
| --- | --- |
| `Basic` | Board initialization and the main service loop |
| `Buttons` | Pressed and clicked button states |
| `Relay` | Relay control and toggling |
| `Pixels` | Individual pixels, all-pixel colors, brightness, and clearing |
| `Display` | OLED logo and text output |
| `Sensors` | BME690 temperature, pressure, humidity, and gas resistance |
| `Fan` | DRV8830 speed, stop, fault read, and fault clear |
| `SDCard` | SD initialization and file operations |
| `WiFiStatus` | Wi-Fi connection and RSSI |
| `Time` | NTP time and formatted local time |
| `CloudPublish` | Cloud connection and key/value publishing |
| `CloudWidgets` | Cloud widget wrappers |
| `CloudCommands` | Reading and consuming cloud commands |
| `ColorConversion` | RGB and hexadecimal color conversion |
| `HardwareCheck` | Automatic relay and NeoPixel checks plus pin diagnostics |
| `MyDotVase` | Analog soil telemetry, pump commands, cool/warm/grow NeoPixel modes, local buttons, and cloud brightness |

`MyDotVase` reads the analog sensor from `A0` and publishes `soilRaw`, `soilPercent`, `pumpOn`, `pumpActivations`, `lightsOn`, `lightMode`, `lightModeIndex`, `brightness`, and `event`. It uses the `Slider`, `Switch`, and `Level` widget helpers for the `brightness`, `pumpOn`, `lightsOn`, and `lightModeIndex` keys. Pump commands are `on`, `off`, and `pump`; each explicit pump command publishes the current `pumpOn` state. Light commands are `lights_on`, `lights_off`, `lights_warm`, `lights_cool`, `grow_veg`, `grow_bloom`, and `grow_full`. A numeric mode can also be selected with `{"content":"lights_mode", "mode":2}`; the modes are cool white (`0`), warm white (`1`), vegetative grow (`2`), bloom grow (`3`), and full-spectrum grow (`4`). The cloud slider must use the key `brightness` and sends the compact `key_value` form, for example `{"content":"brightness_128"}`; the example reads the parsed value from the `Slider` state and applies the range 0–255. Slider changes are applied immediately, while cloud publication and persistent storage wait 300 ms for the slider to settle. Button A toggles the lights locally and button B cycles through all five modes. The grow colors are NeoPixel approximations, not calibrated horticultural spectra. Adjust `SOIL_RAW_DRY` and `SOIL_RAW_WET` in the sketch after measuring the actual sensor in dry and wet soil. The example assumes that the pump is driven through the MyDot relay and that the carrier is powered through its external DC jack.

**Safety note:** power the AZ-Delivery soil sensor from **3.3 V**, not 5 V. A 5 V analog output can exceed the board input range and damage the board.

`MyDotVaseState.h` persists the light state across resets. It selects ESP32 Preferences for Nano ESP32, the Mbed KVStore for the official Nano RP2040 Connect core, and EEPROM storage for RP2040 cores that provide EEPROM and for Nano 33 IoT. The saved fields are light power, mode, and brightness.

## Common lifecycle

Most sketches follow this pattern:

```cpp
void setup() {
  Serial.begin(115200);
  dot.begin();
}

void loop() {
  dot.run();
}
```

`run()` is safe to call on sketches that do not use cloud functions. It is especially important for cloud sketches because MQTT keep-alive, reconnection, inbound command processing, and scheduled synchronization are handled there.

## Public constants

The following constants are available from `MyDot.h`:

| Constant | Value | Purpose |
| --- | ---: | --- |
| `FAN_ADDRESS` | `0x68` | DRV8830 I2C address |
| `SCREEN_ADDRESS` | `0x3C` | SSD1306 OLED I2C address |
| `BME_ADDRESS` | `0x77` | BME690 I2C address |
| `SCREEN_WIDTH` | `128` | OLED width in pixels |
| `SCREEN_HEIGHT` | `64` | OLED height in pixels |
| `OLED_RESET` | `-1` | OLED reset pin configuration |
| `NUMPIXELS` | `12` | Number of NeoPixels on the carrier |

The pin macros `BUTTON_A`, `BUTTON_B`, `RELAY`, `PIN`, and `SD_CS` are selected at compile time for the target board. Use the board mapping above when wiring application hardware.

## API reference

### Core methods

#### `MyDot()`

Constructs a MyDot controller object. Construction does not initialize hardware; call `begin()` from `setup()`.

#### `void begin()`

Initializes the board peripherals:

- random number generation;
- buttons with `INPUT_PULLUP`;
- relay output, initially off;
- NeoPixel strip;
- I2C;
- the DRV8830, OLED, and BME690 when detected.

The OLED logo is drawn when the display is present. NeoPixels are cleared during initialization.

#### `void run()`

Services the library. When cloud features are configured, it handles Wi-Fi reconnection, MQTT reconnection, MQTT traffic, and the optional periodic cloud callback. Call it as often as possible from `loop()` and avoid long blocking delays.

### Fan and DRV8830 motor driver

The DRV8830 is connected through I2C at `FAN_ADDRESS` (`0x68`). The carrier must have external power for the driver output stage.

#### `void setFanSpeed(int speed)`

Sets the requested fan or motor speed. The documented input range is `-100` to `100`:

- positive values select the forward direction;
- negative values select the reverse direction;
- `0` stops the output.

Values outside this range are limited by the driver configuration. The requested value is retained by `getFanSpeed()`.

```cpp
dot.setFanSpeed(60);   // forward, approximately 60%
dot.setFanSpeed(-40);  // reverse, approximately 40%
dot.setFanSpeed(0);    // stop
```

#### `void stopFan()`

Stops the fan or motor by writing the DRV8830 stop command.

#### `uint8_t getFanFault()`

Reads and returns the DRV8830 fault register. The DRV8830 status bits are:

| Bit | Meaning |
| --- | --- |
| `D7` | Clear/status control bit |
| `D4` | Current-limit event (`ILIMIT`) |
| `D3` | Over-temperature shutdown (`OTS`) |
| `D2` | Undervoltage lockout (`UVLO`) |
| `D1` | Over-current protection (`OCP`) |
| `D0` | Fault indication (`FAULT`) |

For example, `0x04` means that the `UVLO` bit is set. Check the carrier supply, wiring, and motor load before retrying.

#### `void clearFanFault()`

Writes the DRV8830 clear command. Clearing a latched status does not repair an active power, wiring, over-current, or thermal condition; read the fault register again after the cause has been removed.

#### `int getFanSpeed()`

Returns the speed value most recently requested through `setFanSpeed()`. It is the requested software value, not a measured RPM or output-voltage reading.

### Wi-Fi and time

#### `void beginWiFi(const char* ssid, const char* password)`

Starts the Wi-Fi connection using the supplied credentials and waits for up to 15 seconds for `WL_CONNECTED`. On ESP32 it also configures NTP servers (`pool.ntp.org` and `time.nist.gov`).

If the initial connection fails, the method returns instead of blocking forever. Call `run()` continuously from `loop()`; it disconnects a stale MQTT session, retries Wi-Fi every 10 seconds, and reconnects MQTT every 5 seconds after Wi-Fi is restored. If Wi-Fi still reports connected but three consecutive MQTT attempts fail, the library restarts the Wi-Fi station as well. Call this method once from `setup()` rather than from `loop()`.

#### `bool isWiFiConnected()`

Returns `true` when the Wi-Fi interface reports `WL_CONNECTED`; otherwise returns `false`.

#### `long getWiFiRSSI()`

Returns the current Wi-Fi signal strength in dBm. A more negative value represents a weaker signal. The result is meaningful after Wi-Fi has been initialized.

#### `unsigned long getEpochTime()`

Returns the current Unix epoch time as provided by the board's time service.

#### `String getFormattedTime(int gmtOffset = 1)`

Returns the current time as a formatted string using the supplied UTC offset in hours. The default offset is UTC+1.

### Microeden cloud connection

#### `void beginCloud(const char* deviceId, const char* token)`

Configures the MQTT topics and TLS connection for a MyDot device. The MQTT endpoint is `microeden.io` on port `8243`. On ESP32, the method installs the bundled ISRG Root X1 CA certificate before enabling TLS.

Call `beginWiFi()` first. After initialization, call `run()` continuously so Wi-Fi and MQTT can be maintained. If Wi-Fi drops, `run()` closes the MQTT session and reconnects both services automatically without requiring a reset.

#### `bool isCloudConnected()`

Returns `true` when the MQTT client is connected to the Microeden cloud.

#### `void setCloudBufferSize(uint16_t size)`

Sets the MQTT/JSON buffer size used by the cloud client. Increase it before writing large payloads or receiving larger commands. The default configured by `beginCloud()` is 1024 bytes.

#### `bool sendCloud()`

Serializes and publishes the current cloud payload when MQTT is connected. On a successful publish, the pending payload is cleared. Returns `false` when the client is disconnected or publishing fails.

#### `void setCloudSync(unsigned long interval, CloudSyncCallback callback)`

Registers a callback that `run()` invokes periodically while the cloud connection is active.

```cpp
void syncValues() {
  dot.writeKeyWord("temperature", dot.getTemperature());
  dot.sendCloud();
}

void setup() {
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
  dot.setCloudSync(10000, syncValues);  // every 10 seconds
}
```

The interval is expressed in milliseconds. Keep the callback short and avoid blocking network operations inside it.

### Cloud key/value payloads

#### `void writeKeyWord(const char* key, const char* value)`

Adds or replaces a string value in the outgoing cloud JSON document.

#### `void writeKeyWord(const char* key, double value)`

Adds or replaces a double-precision numeric value.

#### `void writeKeyWord(const char* key, float value)`

Adds or replaces a floating-point value.

#### `void writeKeyWord(const char* key, int value)`

Adds or replaces an integer value.

#### `void writeKeyWord(const char* key, bool value)`

Adds or replaces a Boolean value.

#### `void writeKeyWord(const char* key, const String& value)`

Adds or replaces an Arduino `String` value.

Call `sendCloud()` after adding the values you want to publish.

#### `bool onCommand(const char* expectedCmd, const char* key = "content")`

Checks the latest inbound cloud document. It returns `true` when the value stored under `key` matches `expectedCmd`, and consumes that matching command so it is not reported repeatedly.

#### `template <typename T> T readKeyWord(const char* key = "content")`

Reads and returns the value under `key` from the latest inbound cloud document, converted to the requested type `T`.

```cpp
if (dot.onCommand("ON")) {
  dot.setRelay(true);
}

int requestedLevel = dot.readKeyWord<int>("level");
```

### Color conversion

#### `static void hexToRGB(String hex, uint8_t& r, uint8_t& g, uint8_t& b)`

Converts a six-digit hexadecimal color into red, green, and blue components. An optional leading `#` is accepted. Invalid input produces `0, 0, 0`.

```cpp
uint8_t r, g, b;
MyDot::hexToRGB("#3366CC", r, g, b);
dot.setAllPixels(r, g, b);
```

#### `static String rgbToHex(uint8_t r, uint8_t g, uint8_t b)`

Converts RGB components to an uppercase string in the form `#RRGGBB`.

### Buttons

Buttons use the internal pull-up resistors and are active-low.

#### `bool isButtonAPressed()`

Returns `true` while button A is physically held down.

#### `bool isButtonBPressed()`

Returns `true` while button B is physically held down.

#### `bool isButtonAClicked()`

Returns `true` once when a new debounced press is detected on button A. A new click is not reported until the button has been released.

#### `bool isButtonBClicked()`

Returns `true` once when a new debounced press is detected on button B. A new click is not reported until the button has been released.

The click methods apply a debounce interval of approximately 50 ms. Poll them frequently from `loop()`.

### Relay

The relay output is connected to the carrier's `RELAY` pin. External carrier power is required.

#### `void setRelay(bool state)`

Sets the relay state. `true` energizes the relay; `false` de-energizes it.

#### `void toggleRelay()`

Inverts the current relay output state.

```cpp
if (dot.isButtonAClicked()) {
  dot.toggleRelay();
}
```

### NeoPixels

The library controls `NUMPIXELS` (12) addressable LEDs on the carrier. Color values use the 0–255 range.

#### `void setAllPixels(uint8_t r, uint8_t g, uint8_t b)`

Sets the same RGB color on every pixel and updates the strip immediately.

#### `void setPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b)`

Sets one pixel by zero-based index and updates the strip. Out-of-range indices are ignored.

#### `void showPixels()`

Sends the current in-memory pixel colors to the LEDs. Use this after a series of changes when you want one update instead of one update per change.

#### `void setBrightness(uint8_t b)`

Sets the global NeoPixel brightness from `0` (off) to `255` (full brightness) and updates the strip.

#### `void setRandomPixels()`

Assigns random colors to all pixels and updates the strip.

#### `void clearPixels()`

Sets every pixel to black and updates the strip.

```cpp
dot.setBrightness(80);
dot.setAllPixels(0, 40, 180);
dot.setPixel(0, 255, 0, 0);
dot.showPixels();
```

### OLED display

The display uses the SSD1306 controller at `SCREEN_ADDRESS` (`0x3C`), with `SCREEN_WIDTH` 128 and `SCREEN_HEIGHT` 64. The OLED is optional; check `isDisplayPresent()` before relying on it.

#### `void drawLogo()`

Draws the MyDot logo on the display buffer and sends it to the OLED.

#### `void updateSensorDisplay()`

Public compatibility declaration for refreshing a sensor display. The current library source does not provide an implementation for this entry point; application code should call `readSensors()` and then use the display primitives below to render values explicitly.

#### `void clearDisplay()`

Clears the display buffer.

#### `void showDisplay()`

Transfers the current display buffer to the OLED.

#### `void setCursor(int16_t x, int16_t y)`

Sets the text cursor position in pixels.

#### `void setTextSize(uint8_t s)`

Sets the Adafruit GFX text scale.

#### `void setTextColor(uint16_t c)`

Sets the text color, normally `SSD1306_WHITE` or `SSD1306_BLACK`.

#### `void displayPrint(const String& text, bool clear = true)`

Prints text using the current cursor, text size, and text color. When `clear` is `true`, the display buffer is cleared before printing and the result is sent to the OLED.

#### `void displayPrint(const String& text, int x, int y, uint8_t size = 1, bool clear = true)`

Sets the cursor and text size, prints the supplied text, and optionally clears the buffer first.

#### `Adafruit_SSD1306& getDisplay()`

Returns a reference to the underlying Adafruit SSD1306 object for advanced drawing operations.

#### `bool isDisplayPresent()`

Returns `true` when the OLED was detected and initialized successfully.

#### `template <typename T> void print(T val)`

Prints a value to the display buffer using the underlying Adafruit GFX object.

#### `template <typename T> void println(T val)`

Prints a value followed by a line break to the display buffer.

#### `void println()`

Prints a line break to the display buffer.

For low-level display operations, call `showDisplay()` after `print()`, `println()`, or other buffer changes.

### BME690 environmental sensor

The onboard sensor is a BME690. The API exposes temperature, pressure, relative humidity, and gas resistance through the Adafruit BME680-compatible measurement interface.

#### `bool readSensors()`

Triggers a new measurement and returns `true` when the reading is available.

#### `float getTemperature()`

Returns temperature in degrees Celsius from the most recent successful reading.

#### `float getPressure()`

Returns atmospheric pressure in hPa from the most recent successful reading.

#### `float getHumidity()`

Returns relative humidity in percent from the most recent successful reading.

#### `float getGasResistance()`

Returns gas resistance in kOhm from the most recent successful reading.

```cpp
if (dot.readSensors()) {
  Serial.print("Temperature: ");
  Serial.print(dot.getTemperature());
  Serial.println(" C");
  Serial.print("Pressure: ");
  Serial.print(dot.getPressure());
  Serial.println(" hPa");
  Serial.print("Humidity: ");
  Serial.print(dot.getHumidity());
  Serial.println(" %");
  Serial.print("Gas resistance: ");
  Serial.print(dot.getGasResistance());
  Serial.println(" kOhm");
}
```

### SD card

The SD card chip-select pin is `SD_CS` (`D10` on the Nano ESP32 mapping).

#### `bool beginSD()`

Initializes the SD card interface and returns whether initialization succeeded.

#### `File openFile(const char* filename, const char* mode = "r")`

Opens a file and returns the resulting `File` object. Use `"r"` for read,
`"w"` for replace/write, or `"a"` for append. MyDot maps these strings to the
native SD mode type for the selected board.

#### `bool fileExists(const char* filename)`

Returns `true` when the specified path exists on the SD card.

#### `void removeFile(const char* filename)`

Removes the specified file from the SD card.

#### `bool writeFile(const String& path, const String& message)`

Writes a message to a file, replacing an existing file at the same path.

#### `bool appendFile(const String& path, const String& message)`

Appends a message to a file, creating or opening it as supported by the selected board's SD implementation.

### Cloud widgets

The widget classes provide typed wrappers around a cloud key. Each widget is constructed with a key and a reference to a `MyDot` instance.

#### `template <typename T> CloudWidget<T>(const char* key, MyDot& device)`

Creates a generic widget bound to `key`.

#### `void CloudWidget<T>::write(T value)`

Adds a typed value to the outgoing cloud payload under the widget's key. Call `sendCloud()` to publish the payload.

#### `T CloudWidget<T>::read()`

Reads the typed value from the widget's cloud key.

`Slider::read()` returns the latest compact `key_value` slider value as an integer. It returns `-1` until a value has been received, so `0` remains a valid brightness value.

#### `ColorWheel`

`ColorWheel(const char* key, MyDot& device)` constructs a color widget.

`void getRGB(uint8_t& r, uint8_t& g, uint8_t& b)` reads the current color and converts it to RGB components.

`void write(uint8_t r, uint8_t g, uint8_t b)` adds an RGB color to the outgoing payload. Call `sendCloud()` to publish it.

#### `Level`, `Slider`, `Switch`, `Pushbutton`, `Led`, and `Photo`

Each class has a constructor with the same signature:

```cpp
ClassName(const char* key, MyDot& device);
```

Use the inherited `write()` and `read()` methods with the type appropriate for the widget.

#### `Map`

`Map(const char* key, MyDot& device)` constructs a map widget.

`void write(double lat, double lng)` adds latitude and longitude to the outgoing payload. Call `sendCloud()` to publish them.

`void write(const String& value)` adds a preformatted map value to the outgoing payload.

## Troubleshooting

### Relay, NeoPixels, or fan output is inactive

Power the carrier through the external DC jack. USB alone can power the Nano ESP32 while leaving the carrier output stages unpowered.

### Nano ESP32 pin behavior is unexpected

Select **Arduino Nano ESP32** and verify the Arduino IDE's pin-numbering option. MyDot uses the Nano aliases shown in the pin-mapping table and performs the ESP32 NeoPixel pin conversion internally.

### `getFanFault()` returns `0x04`

The DRV8830 `0x04` status indicates `UVLO` (undervoltage lockout). Check the carrier supply voltage, motor wiring, connector polarity, and whether the supply collapses when the load starts. Clear the fault only after correcting the cause.

### Cloud connection does not start

Check the four values in `microeden_secrets.h`, call `beginWiFi()` before `beginCloud()`, and keep calling `run()` in `loop()`. On ESP32, the library validates the server certificate using the bundled ISRG Root X1 CA.

## License

See the repository `LICENSE` file for the terms that apply to the MyDot library.
