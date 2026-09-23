#ifndef MYDOT_H
#define MYDOT_H

/*
 * MyDot is the hardware abstraction for the Microeden MyDot carrier.
 * It owns the onboard peripherals (OLED, NeoPixels, BME690, fan, relay and
 * SD card) and optionally manages Wi-Fi and the Microeden MQTT connection.
 */
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME680.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SD.h>

// Reset nativo della piattaforma. Il watchdog esposto dalla libreria usa una
// API comune e viene alimentato da run(); il riavvio invece delega al core
// quando esiste una funzione ufficiale per quella scheda.
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#elif defined(ARDUINO_ARCH_SAMD)
#include <sam.h>
#elif defined(ARDUINO_ARCH_MBED)
#include <mbed.h>
#elif defined(ARDUINO_ARCH_RP2040)
#if defined(__has_include)
#if __has_include(<hardware/watchdog.h>)
#include <hardware/watchdog.h>
#define MYDOT_HAS_RP2040_NATIVE_REBOOT 1
#endif
#endif
#endif

#ifndef MYDOT_HAS_RP2040_NATIVE_REBOOT
#define MYDOT_HAS_RP2040_NATIVE_REBOOT 0
#endif

// U3 (DRV8830) has A0 and A1 tied to +5V on the MyDot V1.0 carrier.
// The datasheet's 8-bit 0xD0 write address is 0x68 in Arduino's 7-bit form.
#define FAN_ADDRESS 0x68
#define SCREEN_ADDRESS 0x3C
#define BME_ADDRESS 0x77

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// The popular Arduino-Pico core exposes a native RP2040 heap helper, while
// the official Nano RP2040 Connect uses the separate Mbed adapter below.
// Detect the optional helper without making it a dependency of other cores.
#ifndef MYDOT_HAS_RP2040_STATS
#define MYDOT_HAS_RP2040_STATS 0
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_NANO_RP2040_CONNECT)
#if defined(__has_include)
#if __has_include(<RP2040.h>)
#undef MYDOT_HAS_RP2040_STATS
#define MYDOT_HAS_RP2040_STATS 1
#endif
#endif
#endif
#endif

// The MyDot carrier uses the same standard Nano header for every supported
// Arduino Nano. These are carrier signal names, not per-board rewiring. The
// Nano ESP32 branch keeps the board aliases so its Arduino/GPIO numbering mode
// is handled correctly; the other Nano cores use the same logical header pins.
#if defined(ARDUINO_NANO_ESP32)
#define MYDOT_HAS_WIFI 1
#define MYDOT_HAS_CLOUD 1
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "esp32"
#define MYDOT_HAS_SD_CAPACITY 1
#define MYDOT_PIN_BUTTON_A_NAME "A7"
#define MYDOT_PIN_BUTTON_B_NAME "D4"
#define MYDOT_PIN_RELAY_NAME "D2"
#define MYDOT_PIN_PIXELS_NAME "D3"
#define MYDOT_PIN_SD_CS_NAME "D10"
#define MYDOT_PIN_MAP_NAME "buttonA=A7 buttonB=D4 relay=D2 pixels=D3 sdCS=D10"
#include <WiFi.h>
#include <WiFiClientSecure.h>
// Keep the MyDot shield aligned with the Nano pinout in either Arduino or
// GPIO legacy numbering mode.
#define BUTTON_A A7
#define BUTTON_B D4
#define RELAY D2
#define PIN D3
#define SD_CS D10
#elif defined(ARDUINO_ARCH_ESP32)
#define MYDOT_HAS_WIFI 1
#define MYDOT_HAS_CLOUD 1
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "esp32"
#define MYDOT_HAS_SD_CAPACITY 1
#define MYDOT_PIN_BUTTON_A_NAME "14"
#define MYDOT_PIN_BUTTON_B_NAME "7"
#define MYDOT_PIN_RELAY_NAME "5"
#define MYDOT_PIN_PIXELS_NAME "6"
#define MYDOT_PIN_SD_CS_NAME "10"
#define MYDOT_PIN_MAP_NAME "buttonA=14 buttonB=7 relay=5 pixels=6 sdCS=10"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define BUTTON_A 14
#define BUTTON_B 7
#define RELAY 5
#define PIN 6
#define SD_CS 10
#elif defined(ARDUINO_NANO_RP2040_CONNECT) || defined(ARDUINO_SAMD_NANO_33_IOT)
#define MYDOT_HAS_WIFI 1
#define MYDOT_HAS_CLOUD 1
#if defined(ARDUINO_NANO_RP2040_CONNECT)
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "mbed"
#else
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "samd"
#endif
#define MYDOT_HAS_SD_CAPACITY 1
#define MYDOT_PIN_BUTTON_A_NAME "A7"
#define MYDOT_PIN_BUTTON_B_NAME "D4"
#define MYDOT_PIN_RELAY_NAME "D2"
#define MYDOT_PIN_PIXELS_NAME "D3"
#define MYDOT_PIN_SD_CS_NAME "D10"
#define MYDOT_PIN_MAP_NAME "buttonA=A7 buttonB=D4 relay=D2 pixels=D3 sdCS=D10"
#include <WiFiNINA.h>
#define BUTTON_A A7
#define BUTTON_B 4
#define RELAY 2
#define PIN 3
#define SD_CS 10
#elif defined(ARDUINO_ARCH_RP2040)
#define MYDOT_HAS_WIFI 0
#define MYDOT_HAS_CLOUD 0
#if MYDOT_HAS_RP2040_STATS
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "rp2040"
#else
#define MYDOT_HAS_RAM_STATUS 0
#define MYDOT_RAM_BACKEND "rp2040-unavailable"
#endif
#define MYDOT_HAS_SD_CAPACITY 1
#define MYDOT_PIN_BUTTON_A_NAME "A7"
#define MYDOT_PIN_BUTTON_B_NAME "D4"
#define MYDOT_PIN_RELAY_NAME "D2"
#define MYDOT_PIN_PIXELS_NAME "D3"
#define MYDOT_PIN_SD_CS_NAME "D10"
#define MYDOT_PIN_MAP_NAME "buttonA=A7 buttonB=D4 relay=D2 pixels=D3 sdCS=D10"
#define BUTTON_A A7
#define BUTTON_B 4
#define RELAY 2
#define PIN 3
#define SD_CS 10
#else
#define MYDOT_HAS_WIFI 0
#define MYDOT_HAS_CLOUD 0
#if defined(ARDUINO_ARCH_MBED)
#define MYDOT_HAS_RAM_STATUS 1
#define MYDOT_RAM_BACKEND "mbed"
#else
#define MYDOT_HAS_RAM_STATUS 0
#define MYDOT_RAM_BACKEND "unavailable"
#endif
#define MYDOT_HAS_SD_CAPACITY 1
#define MYDOT_PIN_BUTTON_A_NAME "A7"
#define MYDOT_PIN_BUTTON_B_NAME "D4"
#define MYDOT_PIN_RELAY_NAME "D2"
#define MYDOT_PIN_PIXELS_NAME "D3"
#define MYDOT_PIN_SD_CS_NAME "D10"
#define MYDOT_PIN_MAP_NAME "buttonA=A7 buttonB=D4 relay=D2 pixels=D3 sdCS=D10"
#define BUTTON_A A7
#define BUTTON_B 4
#define RELAY 2
#define PIN 3
#define SD_CS 10
#endif

struct MyDotPinMap {
  const char* buttonA;
  const char* buttonB;
  const char* relay;
  const char* pixels;
  const char* sdCs;
};

// This is the single carrier map exposed to diagnostics and integrations.
// Alias text is deliberately kept separate from the core's pin object: on the
// Nano RP2040 Connect, A7 is a typed NinaPin and is not convertible to int.
static const MyDotPinMap MYDOT_PIN_MAP = {
  MYDOT_PIN_BUTTON_A_NAME,
  MYDOT_PIN_BUTTON_B_NAME,
  MYDOT_PIN_RELAY_NAME,
  MYDOT_PIN_PIXELS_NAME,
  MYDOT_PIN_SD_CS_NAME
};

// PubSubClient always needs a Client object, even on boards that do not have
// an onboard Wi-Fi transport (Nano 33 BLE, BLE Sense, Matter, ...).  Keeping
// a no-op transport here lets the same MyDot library and Bridge compile on
// those boards; the network API then reports "unavailable" instead of making
// the whole firmware board-dependent at compile time.
#if !MYDOT_HAS_CLOUD
class MyDotNullClient : public Client {
public:
  int connect(IPAddress, uint16_t) override { return 0; }
  int connect(const char*, uint16_t) override { return 0; }
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  int available() override { return 0; }
  int read() override { return -1; }
  int read(uint8_t*, size_t) override { return 0; }
  int peek() override { return -1; }
  void flush() override {}
  void stop() override {}
  uint8_t connected() override { return 0; }
  operator bool() override { return false; }
};
#endif

#define NUMPIXELS 12

class MyDot {
public:
  // Creates peripheral drivers; call begin() once from setup() before use.
  MyDot();
  void begin();
  // Keeps Wi-Fi and MQTT connections alive; call continuously from loop().
  void run();
  // Reboots the current board using its native reset API. Returns false only
  // when the selected Arduino core does not expose a safe reset primitive.
  bool reboot();
  bool rebootSupported() const;
  // Portable cooperative watchdog. Call watchdogBegin() once, then feed it
  // explicitly with watchdogFeed() from the healthy loop; run() checks the
  // deadline and reboots the board when the feed is late.
  bool watchdogBegin(unsigned long timeoutMs);
  void watchdogFeed();
  void watchdogStop();
  bool watchdogIsEnabled() const;
  // Stops the networking services started by the runtime program. This is
  // intentionally separate from run()/STOP so an editor can tear down its
  // programming session without changing autonomous runtime semantics.
  void stopNetworkServices();

  // --- Fan driver ---

  void setFanSpeed(int speed);
  void stopFan();
  uint8_t getFanFault();
  void clearFanFault();
  int getFanSpeed();

  // --- SD card ---

  bool beginSD();
  // Returns board-specific runtime memory information.  The Bridge uses this
  // instead of assuming the ESP32 heap API exists on every Nano core.
  bool getMemoryStats(uint32_t& freeBytes, uint32_t& totalBytes) const;
  // `mode` uses the familiar SD strings: "r" (read), "w" (replace), or
  // "a" (append). The implementation maps them to the selected board's SD
  // library, whose native mode type differs between ESP32 and SAMD/RP2040.
  File openFile(const char* filename, const char* mode = "r");
  bool fileExists(const char* filename);
  bool makeDirectory(const String& path);
  bool createFile(const String& path);
  void removeFile(const char* filename);
  bool writeFile(const String& path, const String& message);
  bool appendFile(const String& path, const String& message);

  // --- Wi-Fi ---
  void beginWiFi(const char* ssid, const char* password);
  bool isWiFiConnected();

  long getWiFiRSSI();
  unsigned long getEpochTime();
  String getFormattedTime(int gmtOffset = 1);

  // --- Microeden cloud ---
  // Configures MQTT credentials. On ESP32, TLS uses the bundled root CA.
  void beginCloud(const char* deviceId, const char* token);
  // Adds a value to the JSON payload sent by sendCloud().
  void writeKeyWord(const char* key, const char* value);
  void writeKeyWord(const char* key, double value);
  void writeKeyWord(const char* key, float value);
  void writeKeyWord(const char* key, int value);
  void writeKeyWord(const char* key, bool value);
  void writeKeyWord(const char* key, const String& value);

  static void hexToRGB(String hex, uint8_t& r, uint8_t& g, uint8_t& b);
  static String rgbToHex(uint8_t r, uint8_t g, uint8_t b);

  bool onCommand(const char* expectedCmd, const char* key = "content");

  template<typename T>
  T readKeyWord(const char* key = "content") {
    // _stateDoc is updated by mqttCallback when a cloud message arrives.
    return _stateDoc[key].as<T>();
  }

  // Reads and consumes a newly received compact widget value. Unlike
  // readKeyWord(), this does not return an old value from the cached cloud
  // state, so a telemetry/state echo cannot be mistaken for a new command.
  template<typename T>
  bool readKeyWordOnce(const char* key, T& value) {
    if (_pendingWidgetDoc[key].isNull()) {
      return false;
    }

    value = _pendingWidgetDoc[key].as<T>();
    _pendingWidgetDoc.remove(key);
    return true;
  }

  bool sendCloud();
  bool isCloudConnected();
  void setCloudBufferSize(uint16_t size);
  typedef void (*CloudSyncCallback)();
  void setCloudSync(unsigned long interval, CloudSyncCallback callback);

  // --- Buttons ---
  bool isButtonAPressed();
  bool isButtonBPressed();
  bool isButtonAClicked();
  bool isButtonBClicked();
  // Read the pending debounced edge without consuming it. This is useful for
  // diagnostics/status panels; the isButton*Clicked methods remain the
  // consuming operations used by runtime conditions.
  bool peekButtonAClicked();
  bool peekButtonBClicked();

  // --- Relay ---
  void setRelay(bool state);
  void toggleRelay();
  // Returns the current electrical state of the relay output without
  // changing it. This is useful for bridges and runtime blocks that expose
  // the relay state to the next operation.
  bool isRelayOn();

  // --- NeoPixels ---
  void setAllPixels(uint8_t r, uint8_t g, uint8_t b);
  void setPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
  void showPixels();
  void setBrightness(uint8_t b);
  void setRandomPixels();
  void clearPixels();

  // --- OLED display ---
  void drawLogo();
  void updateSensorDisplay();
  void clearDisplay();
  void showDisplay();
  void setCursor(int16_t x, int16_t y);
  void setTextSize(uint8_t s);
  void setTextColor(uint16_t c);
  void displayPrint(const String& text, bool clear = true);
  void displayPrint(const String& text, int x, int y, uint8_t size = 1, bool clear = true);
  Adafruit_SSD1306& getDisplay() { return display; }
  bool isDisplayPresent() { return displayPresent; }

  template<typename T>
  void print(T val) {
    if (displayPresent) display.print(val);
  }

  template<typename T>
  void println(T val) {
    if (displayPresent) display.println(val);
  }

  void println() {
    if (displayPresent) display.println();
  }

  // --- BME690 sensor ---
  bool readSensors();
  float getTemperature();
  float getPressure();
  float getHumidity();
  float getGasResistance();

private:
  // Debounce state for edge-triggered button click detection.
  unsigned long _lastPressTimeA = 0;
  unsigned long _lastPressTimeB = 0;
  bool _lastButtonAState = false;
  bool _lastButtonBState = false;

  bool _isInitialized = false;

  // Last requested fan speed in the -100 to 100 range.
  int _currentFanSpeed;

  char _ssid[32];
  char _password[64];

  // Connection lifecycle state.  Keeping this in the object (instead of in
  // function-local static variables) prevents one MyDot instance from
  // affecting another and lets run() resume a connection after a drop.
  bool _wifiStarted = false;
  bool _cloudConfigured = false;
  bool _timeConfigured = false;
  bool _wifiRestartPending = false;
  unsigned long _lastWiFiAttempt = 0;
  unsigned long _wifiRestartAt = 0;
  unsigned long _lastMqttAttempt = 0;
  uint8_t _mqttFailureCount = 0;

  bool _watchdogEnabled = false;
  unsigned long _watchdogTimeout = 0;
  unsigned long _watchdogLastFeed = 0;
  void serviceWatchdog();

  Adafruit_SSD1306 display;
  Adafruit_NeoPixel pixels;
  Adafruit_BME680 bme;

  bool lastButtonAState;
  bool lastButtonBState;

  bool displayPresent;
  unsigned long lastReadTime;

  bool validateAddress(uint8_t address);

  // --- Cloud connection state ---
  char _host[64];
  const uint16_t _port = 8243;
  String _deviceId;
  String _token;
  String _clientId;
  String _topicIn;
  String _topicOut;

#if defined(ARDUINO_ARCH_ESP32) && MYDOT_HAS_CLOUD
  WiFiClientSecure _netClient;
#elif MYDOT_HAS_CLOUD
  WiFiSSLClient _netClient;
#else
  MyDotNullClient _netClient;
#endif


  // MQTT transport and JSON documents for outgoing, inbound and cached state.
  PubSubClient _mqtt;
  JsonDocument _payloadDoc;
  JsonDocument _lastInboundDoc;
  JsonDocument _stateDoc;
  JsonDocument _pendingWidgetDoc;

  // MQTT commands can arrive in bursts (the dashboard workers may publish a
  // command followed immediately by an empty reset message). Keeping a small
  // FIFO prevents the reset/latest-payload model from dropping the command
  // before onCommand() gets a chance to consume it.
  static const uint8_t MAX_PENDING_COMMANDS = 8;
  String _pendingCommandQueue[MAX_PENDING_COMMANDS];
  uint8_t _pendingCommandHead = 0;
  uint8_t _pendingCommandCount = 0;

  // PubSubClient requires a static callback, so it forwards to this instance.
  static MyDot* _instance;
  static void mqttCallback(char* topic, byte* payload, unsigned int length);

  unsigned long _cloudInterval = 3000;
  unsigned long _lastCloudSync = 0;
  CloudSyncCallback _syncCallback = nullptr;
};

// --- Cloud widget helpers ---
// Widgets bind a Microeden dashboard key to a typed read/write interface.

template<typename T>
class CloudWidget {
protected:
  const char* _key;
  MyDot* _device;
public:
  CloudWidget(const char* key, MyDot& device)
    : _key(key), _device(&device) {}

  void write(T value) {
    _device->writeKeyWord(_key, value);
  }

  T read() {
    return _device->readKeyWord<T>(_key);
  }
};

class ColorWheel : public CloudWidget<String> {
public:
  ColorWheel(const char* k, MyDot& d)
    : CloudWidget(k, d) {}

  void getRGB(uint8_t& r, uint8_t& g, uint8_t& b) {
    MyDot::hexToRGB(read(), r, g, b);
  }

  void write(uint8_t r, uint8_t g, uint8_t b) {
    String hex = MyDot::rgbToHex(r, g, b);
    _device->writeKeyWord(_key, hex);
  }
};

class Level : public CloudWidget<int> {
public:
  Level(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
};
class Slider : public CloudWidget<int> {
public:
  Slider(const char* k, MyDot& d)
    : CloudWidget(k, d) {}

  // Slider widget messages arrive as the compact text form "key_value".
  // Consume only a newly received widget event. Return -1 when there is no
  // new value so a cached/echoed cloud state cannot reset the application.
  int read() {
    String value;
    if (!_device->readKeyWordOnce<String>(_key, value)) {
      return -1;
    }
    return value.toInt();
  }
};
class Switch : public CloudWidget<bool> {
public:
  Switch(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
};
class Pushbutton : public CloudWidget<bool> {
public:
  Pushbutton(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
};
class Led : public CloudWidget<bool> {
public:
  Led(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
};
class Photo : public CloudWidget<String> {
public:
  Photo(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
};
class Map : public CloudWidget<String> {
public:
  Map(const char* k, MyDot& d)
    : CloudWidget(k, d) {}
  void write(double lat, double lng) {
    String coord = String(lat, 6) + "," + String(lng, 6);
    _device->writeKeyWord(_key, coord);
  }
  void write(const String& value) {
    CloudWidget<String>::write(value);
  }
};

#endif
