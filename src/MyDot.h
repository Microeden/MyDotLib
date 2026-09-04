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

// U3 (DRV8830) has A0 and A1 tied to +5V on the MyDot V1.0 carrier.
// The datasheet's 8-bit 0xD0 write address is 0x68 in Arduino's 7-bit form.
#define FAN_ADDRESS 0x68
#define SCREEN_ADDRESS 0x3C
#define BME_ADDRESS 0x77

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Pin definitions follow the MyDot carrier schematic. Nano ESP32 aliases are
// used so the carrier works with both Arduino and legacy GPIO pin numbering.
#if defined(ARDUINO_NANO_ESP32)
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
#include <WiFi.h>
#include <WiFiClientSecure.h>
#define BUTTON_A 14
#define BUTTON_B 7
#define RELAY 5
#define PIN 6
#define SD_CS 10
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_SAMD_NANO_33_IOT)
#include <WiFiNINA.h>
#define BUTTON_A A7
#define BUTTON_B 4
#define RELAY 2
#define PIN 3
#define SD_CS 21
#else
#define BUTTON_A A7
#define BUTTON_B 4
#define RELAY 2
#define PIN 3
#define SD_CS 21
#endif

#define NUMPIXELS 12

class MyDot {
public:
  // Creates peripheral drivers; call begin() once from setup() before use.
  MyDot();
  void begin();
  // Keeps Wi-Fi and MQTT connections alive; call continuously from loop().
  void run();

  // --- Fan driver ---

  void setFanSpeed(int speed);
  void stopFan();
  uint8_t getFanFault();
  void clearFanFault();
  int getFanSpeed();

  // --- SD card ---

  bool beginSD();
  File openFile(const char* filename, const char* mode = FILE_READ);
  bool fileExists(const char* filename);
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

  // --- Relay ---
  void setRelay(bool state);
  void toggleRelay();

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

#if defined(ARDUINO_ARCH_ESP32)
  WiFiClientSecure _netClient;
#else
  WiFiSSLClient _netClient;
#endif


  // MQTT transport and JSON documents for outgoing, inbound and cached state.
  PubSubClient _mqtt;
  JsonDocument _payloadDoc;
  JsonDocument _lastInboundDoc;
  JsonDocument _stateDoc;

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
