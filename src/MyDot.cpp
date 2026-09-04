#include "MyDot.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "MyDotRootCA.h"
#endif

// PubSubClient uses a C-style callback and cannot carry a MyDot instance.
// The active object is saved here and used by mqttCallback below.
MyDot* MyDot::_instance = nullptr;

void MyDot::mqttCallback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  if (!_instance) return;

  // Keep the raw incoming document for onCommand(), then mirror each field in
  // _stateDoc so typed widgets can read the latest known cloud state.
  JsonDocument incoming;
  DeserializationError error = deserializeJson(incoming, payload, length);
  if (!error) {
    _instance->_lastInboundDoc = incoming;
    JsonObject obj = incoming.as<JsonObject>();
    for (JsonPair kv : obj) {
      const char* key = kv.key().c_str();
      if (strcmp(key, "content") == 0) {
        const char* val = kv.value().as<const char*>();
        if (val) {
          const char* sep = strchr(val, '_');
          if (sep) {
            int prefixLen = sep - val;
            String realKey = String(val).substring(0, prefixLen);
            String realVal = String(sep + 1);
            _instance->_stateDoc[realKey] = realVal;
          }
        }
      } else {
        _instance->_stateDoc[key] = kv.value();
      }
    }
  }
}

static const unsigned char PROGMEM logo_bmp[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xf8, 0x7f, 0xff, 0xff, 0xf8, 0x07, 0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x87, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x8c, 0x70, 0xf8, 0x7c, 0x3e, 0x10, 0xe0, 0x07, 0x87, 0xf8, 0x87, 0xe1, 0xf1, 0x87, 0xff,
  0xff, 0x80, 0x00, 0x38, 0x70, 0x0e, 0x00, 0xc0, 0x06, 0x01, 0xe0, 0x07, 0x80, 0x70, 0x03, 0xff,
  0xff, 0x80, 0x00, 0x38, 0x60, 0x06, 0x01, 0x80, 0x04, 0x00, 0xe0, 0x07, 0x00, 0x70, 0x03, 0xff,
  0xff, 0x80, 0x00, 0x18, 0x60, 0x06, 0x07, 0x00, 0x04, 0x30, 0xc0, 0x07, 0x0c, 0x30, 0x01, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x63, 0xde, 0x0f, 0x00, 0x04, 0x78, 0xc3, 0x86, 0x1c, 0x30, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x63, 0xfe, 0x1f, 0x00, 0x04, 0x00, 0xc3, 0x86, 0x00, 0x30, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x63, 0xfe, 0x1f, 0x00, 0x04, 0x00, 0xc3, 0x86, 0x00, 0x30, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x63, 0xfe, 0x1f, 0x00, 0x0c, 0x7f, 0xc3, 0x86, 0x1f, 0xf0, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x61, 0x86, 0x1f, 0x00, 0x0c, 0x33, 0xc1, 0x07, 0x0c, 0xf0, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x60, 0x06, 0x1f, 0x80, 0x1c, 0x00, 0xe0, 0x07, 0x00, 0x30, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x70, 0x0e, 0x1f, 0x80, 0x1e, 0x01, 0xe0, 0x07, 0x80, 0x70, 0xe1, 0xff,
  0xff, 0x87, 0x0e, 0x18, 0x78, 0x1e, 0x1f, 0xe0, 0x7f, 0x03, 0xf0, 0x47, 0xc0, 0xf0, 0xe1, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

MyDot::MyDot()
  : display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
    pixels(NUMPIXELS,
#if defined(ARDUINO_NANO_ESP32)
           digitalPinToGPIONumber(PIN),
#else
           PIN,
#endif
           NEO_GRB + NEO_KHZ800),
    bme(&Wire),
    lastButtonAState(false),
    lastButtonBState(false),
    displayPresent(false),
    lastReadTime(0),
    _mqtt(_netClient) {
  // Only one MyDot object can receive PubSubClient callback messages.
  _instance = this;
}

// --- Wi-Fi and cloud setup ---

void MyDot::beginWiFi(const char* ssid, const char* password) {
  // Save credentials for the automatic reconnect logic in run().
  strncpy(_ssid, ssid, sizeof(_ssid) - 1);
  _ssid[sizeof(_ssid) - 1] = '\0';
  strncpy(_password, password, sizeof(_password) - 1);
  _password[sizeof(_password) - 1] = '\0';
  if (displayPresent) {
    clearDisplay();
    setCursor(0, 0);
    print("WiFi: ");
    println(ssid);
    showDisplay();
  }
  WiFi.begin(ssid, password);
  // Wi-Fi connection is deliberately blocking during setup for simple sketches.
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  if (displayPresent) {
    showDisplay();
  }
#if defined(ARDUINO_ARCH_ESP32)
  // TLS validation and getEpochTime() require a valid system clock on ESP32.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
#endif
}

void MyDot::beginCloud(const char* deviceId, const char* token) {
  _deviceId = String(deviceId);
  _token = token;

  _clientId = "MICROEDEN-MYDEVICE-" + _deviceId;
  _topicIn = "microeden/dot/" + _deviceId + "/inbox";
  _topicOut = "microeden/dot/" + _deviceId + "/outbox";

#if defined(ARDUINO_ARCH_ESP32)
  // Use certificate validation rather than insecure TLS connections.
  _netClient.setCACert(MYDOT_ROOT_CA);
#endif

  _mqtt.setServer("microeden.io", 8243);
  _mqtt.setCallback(mqttCallback);
  _mqtt.setKeepAlive(60);
  _mqtt.setSocketTimeout(5);
  _mqtt.setBufferSize(1024);
}

void MyDot::setCloudBufferSize(uint16_t size) {
  _mqtt.setBufferSize(size);
}

void MyDot::setCloudSync(unsigned long interval, CloudSyncCallback callback) {
  _cloudInterval = interval;
  _syncCallback = callback;
}

void MyDot::run() {
  unsigned long now = millis();

  // Reconnect Wi-Fi periodically. NINA-based boards require a short reset
  // delay after disconnecting, while ESP32 can reconnect immediately.
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastWiFiRetry = 0;
    static bool waitingNinaReset = false;
    static unsigned long ninaResetTimer = 0;

    if (!waitingNinaReset) {
      if (now - lastWiFiRetry > 10000 || lastWiFiRetry == 0) {
        lastWiFiRetry = now;

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_SAMD_NANO_33_IOT)
        WiFi.disconnect();
        waitingNinaReset = true;
        ninaResetTimer = now;
#else
        WiFi.disconnect(true);
        WiFi.begin(_ssid, _password);
#endif
      }
    } else {

      if (now - ninaResetTimer >= 500) {
        waitingNinaReset = false;
        WiFi.begin(_ssid, _password);
      }
    }
  }

  else {
    // Reconnect MQTT once Wi-Fi is available, then process inbound packets.
    if (!_mqtt.connected()) {
      static unsigned long lastMqttRetry = 0;
      if (now - lastMqttRetry > 5000 || lastMqttRetry == 0) {
        lastMqttRetry = now;
        _mqtt.disconnect();

        if (_mqtt.connect(_clientId.c_str(), _deviceId.c_str(), _token.c_str())) {
          _mqtt.subscribe(_topicIn.c_str());
        } else {
          //
        }
      }
    } else {
      _mqtt.loop();
    }
  }


  // Invoke the optional application callback at the requested cloud interval.
  if (_mqtt.connected() && _syncCallback != nullptr) {
    if (now - _lastCloudSync >= _cloudInterval) {
      _lastCloudSync = now;
      _syncCallback();
    }
  }

  const unsigned long READ_INTERVAL = 2000;
  static unsigned long lastReadTime = 0;
  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;
  }
}

// --- Cloud send and receive ---

bool MyDot::sendCloud() {
  if (_mqtt.connected()) {
    // Serialize all values accumulated through writeKeyWord(), then clear the
    // outgoing document so the next publication starts with an empty payload.
    String output;
    serializeJson(_payloadDoc, output);
    bool success = _mqtt.publish(_topicOut.c_str(), output.c_str());
    _payloadDoc.clear();
    return success;
  }
  return false;
}

bool MyDot::isCloudConnected() {
  return _mqtt.connected();
}

bool MyDot::onCommand(const char* expectedCmd, const char* key) {
  if (_lastInboundDoc.containsKey(key)) {
    String currentCmd = _lastInboundDoc[key].as<String>();
    if (currentCmd == expectedCmd) {
      // Consume a command after matching so it is delivered only once.
      _lastInboundDoc.remove(key);
      return true;
    }
  }
  return false;
}

void MyDot::writeKeyWord(const char* key, const char* value) {
  _payloadDoc[key] = value;
}
void MyDot::writeKeyWord(const char* key, double value) {
  _payloadDoc[key] = value;
}
void MyDot::writeKeyWord(const char* key, float value) {
  _payloadDoc[key] = value;
}
void MyDot::writeKeyWord(const char* key, int value) {
  _payloadDoc[key] = value;
}
void MyDot::writeKeyWord(const char* key, bool value) {
  _payloadDoc[key] = value;
}
void MyDot::writeKeyWord(const char* key, const String& value) {
  _payloadDoc[key] = value;
}


void MyDot::begin() {
  // Initialize board GPIOs and I2C peripherals. Missing optional peripherals
  // are tolerated so basic features remain usable during hardware diagnosis.
  randomSeed(analogRead(A3) + micros() + millis());

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  digitalWrite(RELAY, LOW);

  // The NeoPixel driver receives the physical ESP32 GPIO on Nano ESP32.
  pixels.begin();
  clearPixels();

  Wire.begin();

  // Probe the motor driver without making it a requirement for other features.
  validateAddress(FAN_ADDRESS);

  if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    displayPresent = true;
    drawLogo();
  }

  if (bme.begin()) {
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);
  }

  clearPixels();
}

// --- Buttons and relay ---

bool MyDot::isButtonAPressed() {
#if defined(ARDUINO_ARCH_RP2040)
  return !digitalRead(PIN_NINA_GPIO0);
#else
  return !digitalRead(BUTTON_A);
#endif
}

bool MyDot::isButtonBPressed() {
  return !digitalRead(BUTTON_B);
}

bool MyDot::isButtonAClicked() {
  bool currentState = isButtonAPressed();

  // Report only a debounced rising edge; holding the button does not repeat.
  if (currentState && !_lastButtonAState && (millis() - _lastPressTimeA > 50)) {
    _lastButtonAState = true;
    _lastPressTimeA = millis();
    return true;
  }

  if (!currentState) {
    _lastButtonAState = false;
  }

  return false;
}

bool MyDot::isButtonBClicked() {
  bool currentState = isButtonBPressed();

  if (currentState && !_lastButtonBState && (millis() - _lastPressTimeB > 50)) {
    _lastButtonBState = true;
    _lastPressTimeB = millis();
    return true;
  }

  if (!currentState) {
    _lastButtonBState = false;
  }

  return false;
}

void MyDot::setRelay(bool state) {
  digitalWrite(RELAY, state ? HIGH : LOW);
  delay(10);
  pixels.show();
}

void MyDot::toggleRelay() {
  digitalWrite(RELAY, !digitalRead(RELAY));
  delay(10);
  pixels.show();
}

// --- NeoPixels ---

void MyDot::setAllPixels(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }
  pixels.show();
}

void MyDot::setPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if (n < pixels.numPixels()) {
    pixels.setPixelColor(n, pixels.Color(r, g, b));
    pixels.show();
  }
}

void MyDot::showPixels() {
  pixels.show();
}

void MyDot::setBrightness(uint8_t b) {
  pixels.setBrightness(b);
  pixels.show();
}

void MyDot::clearPixels() {
  pixels.clear();
  pixels.show();
}

void MyDot::setRandomPixels() {
  uint8_t redAmount = random(0, 256);
  uint8_t greenAmount = random(0, 256);
  uint8_t blueAmount = random(0, 256);
  setAllPixels(redAmount, greenAmount, blueAmount);
  pixels.show();
}

// --- OLED display ---

void MyDot::drawLogo() {
  if (!displayPresent) return;
  display.clearDisplay();
  display.drawBitmap(0, 0, logo_bmp, 128, 64, SSD1306_WHITE);
  ;
  display.display();
}

void MyDot::hexToRGB(String hex, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (hex.startsWith("#")) {
    hex.remove(0, 1);
  }
  if (hex.length() == 6) {
    long number = strtol(hex.c_str(), NULL, 16);
    r = (number >> 16) & 0xFF;
    g = (number >> 8) & 0xFF;
    b = number & 0xFF;
  } else {
    r = 0;
    g = 0;
    b = 0;
  }
}

String MyDot::rgbToHex(uint8_t r, uint8_t g, uint8_t b) {
  char hex[8];
  sprintf(hex, "#%02X%02X%02X", r, g, b);
  return String(hex);
}

void MyDot::clearDisplay() {
  if (displayPresent) display.clearDisplay();
}

void MyDot::showDisplay() {
  if (displayPresent) display.display();
}

void MyDot::setCursor(int16_t x, int16_t y) {
  if (displayPresent) display.setCursor(x, y);
}

void MyDot::setTextSize(uint8_t s) {
  if (displayPresent) display.setTextSize(s);
}

void MyDot::setTextColor(uint16_t c) {
  if (displayPresent) display.setTextColor(c);
}

void MyDot::displayPrint(const String& text, bool clear) {
  displayPrint(text, 0, 0, 1, clear);
}

void MyDot::displayPrint(const String& text, int x, int y, uint8_t size, bool clear) {
  if (!displayPresent) return;

  if (clear) display.clearDisplay();

  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
  display.display();
}

// --- BME690 sensor ---

bool MyDot::readSensors() {
  if (!bme.performReading()) {
    return false;
  }
  return true;
}

float MyDot::getTemperature() {
  return bme.temperature;
}
float MyDot::getPressure() {
  return bme.pressure / 100.0;
}
float MyDot::getHumidity() {
  return bme.humidity;
}
float MyDot::getGasResistance() {
  return bme.gas_resistance / 1000.0;
}

// --- I2C utility ---

bool MyDot::validateAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}


// --- SD card ---

bool MyDot::beginSD() {
  if (!SD.begin(SD_CS)) {
    //
    return false;
  }
  //
  return true;
}

File MyDot::openFile(const char* filename, const char* mode) {
  return SD.open(filename, mode);
}

bool MyDot::fileExists(const char* filename) {
  return SD.exists(filename);
}

void MyDot::removeFile(const char* filename) {
  SD.remove(filename);
}

bool MyDot::writeFile(const String& path, const String& message) {
  if (SD.exists(path)) {
    SD.remove(path);
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    //
    return false;
  }
  bool success = file.print(message);
  file.close();
  return success;
}

bool MyDot::appendFile(const String& path, const String& message) {
  File file;
#if defined(ARDUINO_ARCH_ESP32)
  file = SD.open(path, FILE_APPEND);
#else
  file = SD.open(path, FILE_WRITE);
#endif
  if (!file) {
    return false;
  }
  bool success = file.print(message);
  file.close();
  return success;
}

// --- Fan driver ---

int MyDot::getFanSpeed() {
  return _currentFanSpeed;
}

void MyDot::setFanSpeed(int speed) {
  // The fan controller expects direction bits plus a 6-bit speed setpoint.
  _currentFanSpeed = speed;
  uint8_t regValue = 0;
  if (speed == 0) {
    regValue = 0x00;
  } else {
    uint8_t dir = (speed > 0) ? 0x02 : 0x01;
    int absSpeed = abs(speed);
    if (absSpeed > 100) absSpeed = 100;
    uint8_t vset = map(absSpeed, 1, 100, 6, 63);
    regValue = (vset << 2) | dir;
  }
  Wire.beginTransmission(FAN_ADDRESS);
  Wire.write(0x00);
  Wire.write(regValue);
  Wire.endTransmission();
}

void MyDot::stopFan() {
  Wire.beginTransmission(FAN_ADDRESS);
  Wire.write(0x00);
  Wire.write(0x03);
  Wire.endTransmission();
}

uint8_t MyDot::getFanFault() {
  Wire.beginTransmission(FAN_ADDRESS);
  Wire.write(0x01);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)FAN_ADDRESS, (uint8_t)1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0;
}

void MyDot::clearFanFault() {
  Wire.beginTransmission(FAN_ADDRESS);
  Wire.write(0x01);
  Wire.write(0x80);
  Wire.endTransmission();
}


long MyDot::getWiFiRSSI() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return 0;
}

unsigned long MyDot::getEpochTime() {
  if (WiFi.status() != WL_CONNECTED) return 0;

#if defined(ARDUINO_ARCH_ESP32)
  time_t now;
  time(&now);
  return (unsigned long)now;
#else
  return WiFi.getTime();
#endif
}

String MyDot::getFormattedTime(int gmtOffset) {
  unsigned long epoch = getEpochTime();

  if (epoch < 100000) {
    return "00:00:00";
  }

  epoch += (gmtOffset * 3600);

  int hours = (epoch % 86400L) / 3600;
  int minutes = (epoch % 3600) / 60;
  int seconds = epoch % 60;

  char buf[10];
  sprintf(buf, "%02d:%02d:%02d", hours, minutes, seconds);

  return String(buf);
}
