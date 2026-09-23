#include "MyDot.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "MyDotRootCA.h"
#endif

// Arduino's common API intentionally does not standardise runtime RAM
// statistics.  The official Nano RP2040 Connect core is Mbed-based, though,
// so use its native stats API when that core exposes it.
#if (defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_NANO_RP2040_CONNECT)) && __has_include(<mbed_stats.h>)
#include <mbed_stats.h>
#define MYDOT_HAS_MBED_STATS 1
#else
#define MYDOT_HAS_MBED_STATS 0
#endif

#if MYDOT_HAS_RP2040_STATS
#include <RP2040.h>
#endif

#if defined(ARDUINO_ARCH_SAMD)
extern "C" void* _sbrk(int increment);
#endif

// PubSubClient uses a C-style callback and cannot carry a MyDot instance.
// The active object is saved here and used by mqttCallback below.
MyDot* MyDot::_instance = nullptr;

void MyDot::mqttCallback(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  if (!_instance) return;

  // Keep the raw incoming document for onCommand(), then mirror each field in
  // _stateDoc so typed widgets can read the latest known cloud state. Compact
  // widget messages are also copied to _pendingWidgetDoc: widgets must consume
  // those events once instead of treating a cached/echoed state as a command.
  JsonDocument incoming;
  DeserializationError error = deserializeJson(incoming, payload, length);
  if (!error) {
    _instance->_lastInboundDoc = incoming;
    JsonObject obj = incoming.as<JsonObject>();
    for (JsonPair kv : obj) {
      const char* key = kv.key().c_str();
      if (strcmp(key, "content") == 0) {
        String command = kv.value().as<String>();
        command.trim();
        if (command.length() > 0) {
          // Keep every non-empty content message. The dashboard worker can
          // publish an empty reset directly after the command; relying only
          // on _lastInboundDoc would then lose the command between loops.
          MyDot* device = _instance;
          uint8_t slot = (uint8_t)((device->_pendingCommandHead + device->_pendingCommandCount) % MyDot::MAX_PENDING_COMMANDS);
          if (device->_pendingCommandCount >= MyDot::MAX_PENDING_COMMANDS) {
            slot = device->_pendingCommandHead;
            device->_pendingCommandHead = (uint8_t)((device->_pendingCommandHead + 1) % MyDot::MAX_PENDING_COMMANDS);
          } else {
            ++device->_pendingCommandCount;
          }
          device->_pendingCommandQueue[slot] = command;
          Serial.print("MyDot: Cloud command received: ");
          Serial.println(command);
        }
        const char* val = kv.value().as<const char*>();
        if (val) {
          const char* sep = strchr(val, '_');
          if (sep) {
            int prefixLen = sep - val;
            String realKey = String(val).substring(0, prefixLen);
            String realVal = String(sep + 1);
            _instance->_stateDoc[realKey] = realVal;
            _instance->_pendingWidgetDoc[realKey] = realVal;
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
  : _currentFanSpeed(0),
    display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET),
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

bool MyDot::rebootSupported() const {
#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MBED) || MYDOT_HAS_RP2040_NATIVE_REBOOT
  return true;
#else
  return false;
#endif
}

bool MyDot::reboot() {
#if defined(ARDUINO_ARCH_ESP32)
  esp_restart();
  return true;
#elif defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_MBED)
  NVIC_SystemReset();
  return true;
#elif MYDOT_HAS_RP2040_NATIVE_REBOOT
  watchdog_reboot(0, 0, 10);
  return true;
#else
  return false;
#endif
}

bool MyDot::watchdogBegin(unsigned long timeoutMs) {
  // Una soglia troppo bassa renderebbe il watchdog inutilizzabile durante
  // operazioni legittime del Bridge (seriale, SD o TLS).
  if (timeoutMs < 100 || !rebootSupported()) return false;
  if (_watchdogEnabled && _watchdogTimeout == timeoutMs) return true;
  _watchdogTimeout = timeoutMs;
  _watchdogLastFeed = millis();
  _watchdogEnabled = true;
  return true;
}

void MyDot::watchdogFeed() {
  if (_watchdogEnabled) _watchdogLastFeed = millis();
}

void MyDot::watchdogStop() {
  _watchdogEnabled = false;
  _watchdogTimeout = 0;
  _watchdogLastFeed = 0;
}

bool MyDot::watchdogIsEnabled() const {
  return _watchdogEnabled;
}

void MyDot::serviceWatchdog() {
  if (!_watchdogEnabled || _watchdogTimeout == 0) return;
  if ((unsigned long)(millis() - _watchdogLastFeed) < _watchdogTimeout) return;
  _watchdogEnabled = false;
  Serial.println("MyDot: watchdog timeout; rebooting.");
  Serial.flush();
  delay(20);
  if (!reboot()) Serial.println("MyDot: watchdog reboot is not available on this board.");
}

// --- Wi-Fi and cloud setup ---

void MyDot::beginWiFi(const char* ssid, const char* password) {
#if !MYDOT_HAS_WIFI
  (void)ssid;
  (void)password;
  _wifiStarted = false;
  _timeConfigured = false;
  Serial.println("MyDot: Wi-Fi is not available on this board.");
  return;
#else
  // Save credentials for the automatic reconnect logic in run().
  strncpy(_ssid, ssid, sizeof(_ssid) - 1);
  _ssid[sizeof(_ssid) - 1] = '\0';
  strncpy(_password, password, sizeof(_password) - 1);
  _password[sizeof(_password) - 1] = '\0';
  _wifiStarted = true;
  _timeConfigured = false;
  _wifiRestartPending = false;
  _lastWiFiAttempt = millis();
  if (displayPresent) {
    clearDisplay();
    setCursor(0, 0);
    print("WiFi: ");
    println(ssid);
    showDisplay();
  }
  WiFi.begin(ssid, password);
  // La connessione viene sempre gestita da run(). Non attendere qui il
  // completamento dell'handshake: beginWiFi() può essere chiamato dal
  // programma runtime mentre il canale USB deve restare pronto a ricevere
  // STOP, STATUS e altri comandi di controllo.
  if (WiFi.status() == WL_CONNECTED) {
    if (displayPresent) {
      showDisplay();
    }
#if defined(ARDUINO_ARCH_ESP32)
    // TLS validation and getEpochTime() require a valid system clock on ESP32.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    _timeConfigured = true;
#endif
  } else {
    Serial.println("MyDot: Wi-Fi connection started; run() will continue it asynchronously.");
  }
#endif
}

void MyDot::beginCloud(const char* deviceId, const char* token) {
#if !MYDOT_HAS_CLOUD
  (void)deviceId;
  (void)token;
  _cloudConfigured = false;
  Serial.println("MyDot: My Microeden cloud is not available on this board.");
  return;
#else
  const String nextDeviceId = String(deviceId ? deviceId : "");
  const String nextToken = String(token ? token : "");
  // CLOUD BEGIN is often placed in the continuous flow. Re-running the same
  // setup command must not flush commands that arrived between two loop
  // iterations or restart the MQTT back-off timer on every pass.
  if (_cloudConfigured && _deviceId == nextDeviceId && _token == nextToken) {
    return;
  }
  _deviceId = nextDeviceId;
  _token = nextToken;
  _pendingCommandHead = 0;
  _pendingCommandCount = 0;
  for (uint8_t index = 0; index < MAX_PENDING_COMMANDS; ++index) {
    _pendingCommandQueue[index] = String();
  }

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
  // Un timeout breve evita che una connessione TCP/MQTT irraggiungibile
  // monopolizzi il loop e ritardi il canale USB di controllo.
  _mqtt.setSocketTimeout(2);
  _mqtt.setBufferSize(1024);
  _cloudConfigured = true;
  _lastMqttAttempt = 0;
  _mqttFailureCount = 0;
#endif
}

void MyDot::stopNetworkServices() {
  // Stop the MQTT client first so no callback can enqueue a command while the
  // Wi-Fi station is being taken down. Clearing the configuration flags keeps
  // run() from reconnecting the services after this explicit shutdown.
  _mqtt.disconnect();
  _cloudConfigured = false;
  _syncCallback = nullptr;
  _lastMqttAttempt = 0;
  _mqttFailureCount = 0;
  _pendingCommandHead = 0;
  _pendingCommandCount = 0;
  for (uint8_t index = 0; index < MAX_PENDING_COMMANDS; ++index) {
    _pendingCommandQueue[index] = String();
  }
  _payloadDoc.clear();
  _lastInboundDoc.clear();
  _stateDoc.clear();
  _pendingWidgetDoc.clear();

  _wifiStarted = false;
  _timeConfigured = false;
  _wifiRestartPending = false;
  _lastWiFiAttempt = 0;
  _wifiRestartAt = 0;
#if MYDOT_HAS_WIFI && defined(ARDUINO_ARCH_ESP32)
  // On ESP32 the optional argument also powers down the station radio. It
  // does not erase the credentials saved by beginWiFi().
  WiFi.disconnect(true);
#elif MYDOT_HAS_WIFI
  WiFi.disconnect();
#endif
}

void MyDot::setCloudBufferSize(uint16_t size) {
  _mqtt.setBufferSize(size);
}

void MyDot::setCloudSync(unsigned long interval, CloudSyncCallback callback) {
  _cloudInterval = interval;
  _syncCallback = callback;
}

void MyDot::run() {
  serviceWatchdog();
#if !MYDOT_HAS_WIFI
  // Peripheral-only boards still share the same runtime and serial Bridge;
  // there simply is no network state machine to service.
  return;
#else
  unsigned long now = millis();

  // run() is also used by peripheral-only sketches.  Do not touch the Wi-Fi
  // driver until beginWiFi() has explicitly been requested.
  if (!_wifiStarted) {
    return;
  }

  // A lost Wi-Fi link invalidates MQTT immediately.  This is important on
  // ESP32, where PubSubClient may otherwise still report the old TCP socket as
  // connected for a short time after the access point disappears.
  if (WiFi.status() != WL_CONNECTED) {
    if (_mqtt.connected()) {
      _mqtt.disconnect();
      Serial.println("MyDot: Wi-Fi lost; MQTT disconnected.");
    }
#if defined(ARDUINO_ARCH_ESP32)
    // Force NTP setup again after the station reconnects.  A new DHCP lease
    // can leave the previous time service unavailable until it is requested.
    _timeConfigured = false;
#endif

    // NINA boards are more reliable when the radio gets a short pause between
    // disconnect() and begin().  ESP32 can restart the station directly.
    if (_wifiRestartPending) {
      if (now - _wifiRestartAt >= 500) {
        _wifiRestartPending = false;
        WiFi.begin(_ssid, _password);
        _lastWiFiAttempt = now;
        Serial.println("MyDot: Wi-Fi reconnect attempt.");
      }
    } else if (_lastWiFiAttempt == 0 || now - _lastWiFiAttempt >= 10000) {
      _lastWiFiAttempt = now;
      WiFi.disconnect();
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_NANO_RP2040_CONNECT) || defined(ARDUINO_SAMD_NANO_33_IOT)
      _wifiRestartPending = true;
      _wifiRestartAt = now;
#else
      WiFi.begin(_ssid, _password);
      Serial.println("MyDot: Wi-Fi reconnect attempt.");
#endif
    }
    return;
  }

  // The link is back.  Clear any NINA restart state and restore NTP setup
  // after a reconnect so TLS and time helpers remain usable.
  _wifiRestartPending = false;
#if defined(ARDUINO_ARCH_ESP32)
  if (!_timeConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    _timeConfigured = true;
  }
#endif

  if (!_cloudConfigured) {
    return;
  }

  // Reconnect MQTT once Wi-Fi is available, then process inbound packets.
  if (!_mqtt.connected()) {
    if (_lastMqttAttempt == 0 || now - _lastMqttAttempt >= 5000) {
      _lastMqttAttempt = now;
      _mqtt.disconnect();

      if (_mqtt.connect(_clientId.c_str(), _deviceId.c_str(), _token.c_str())) {
        if (_mqtt.subscribe(_topicIn.c_str())) {
          _mqttFailureCount = 0;
          Serial.println("MyDot: MQTT connected.");
        } else {
          Serial.println("MyDot: MQTT connected, but subscription failed.");
          _mqtt.disconnect();
        }
      } else {
        if (_mqttFailureCount < 255) {
          ++_mqttFailureCount;
        }
        Serial.print("MyDot: MQTT reconnect failed (state ");
        Serial.print(_mqtt.state());
        Serial.println(").");

        // If the station still reports WL_CONNECTED but the Internet path is
        // gone, retrying the same TCP stack forever is not enough.  After
        // three failed broker attempts, restart Wi-Fi as well; the normal
        // branch above will then bring MQTT back up on the fresh link.
        if (_mqttFailureCount >= 3) {
          _mqttFailureCount = 0;
          WiFi.disconnect();
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_NANO_RP2040_CONNECT) || defined(ARDUINO_SAMD_NANO_33_IOT)
          _wifiRestartPending = true;
          _wifiRestartAt = now;
#else
          WiFi.begin(_ssid, _password);
          Serial.println("MyDot: restarting Wi-Fi after repeated MQTT failures.");
#endif
          _lastWiFiAttempt = now;
        }
      }
    }
  } else {
    _mqtt.loop();
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
#endif
}

bool MyDot::isWiFiConnected() {
#if !MYDOT_HAS_WIFI
  return false;
#else
  return WiFi.status() == WL_CONNECTED;
#endif
}

// --- Cloud send and receive ---

bool MyDot::sendCloud() {
#if !MYDOT_HAS_CLOUD
  return false;
#else
  if (_mqtt.connected()) {
    // Serialize all values accumulated through writeKeyWord(). On success the
    // outgoing document is cleared so the next publication starts empty.
    String output;
    serializeJson(_payloadDoc, output);
    bool success = _mqtt.publish(_topicOut.c_str(), output.c_str());
    // Keep the pending payload when MQTT rejects the publication. The next
    // reconnect can retry it instead of silently losing telemetry.
    if (success) _payloadDoc.clear();
    return success;
  }
  return false;
#endif
}

bool MyDot::isCloudConnected() {
#if !MYDOT_HAS_CLOUD
  return false;
#else
  return _mqtt.connected();
#endif
}

bool MyDot::onCommand(const char* expectedCmd, const char* key) {
  const char* commandKey = (key && *key) ? key : "content";
  String expected = expectedCmd ? String(expectedCmd) : String();
  expected.trim();

  // Content commands are consumed from the FIFO first. This also handles a
  // command/reset pair published in the same MQTT cycle.
  if (strcmp(commandKey, "content") == 0 && expected.length() > 0) {
    for (uint8_t offset = 0; offset < _pendingCommandCount; ++offset) {
      const uint8_t slot = (uint8_t)((_pendingCommandHead + offset) % MAX_PENDING_COMMANDS);
      String current = _pendingCommandQueue[slot];
      current.trim();
      if (!current.equalsIgnoreCase(expected)) continue;
      for (uint8_t shift = offset; shift + 1 < _pendingCommandCount; ++shift) {
        const uint8_t from = (uint8_t)((_pendingCommandHead + shift + 1) % MAX_PENDING_COMMANDS);
        const uint8_t to = (uint8_t)((_pendingCommandHead + shift) % MAX_PENDING_COMMANDS);
        _pendingCommandQueue[to] = _pendingCommandQueue[from];
      }
      const uint8_t tail = (uint8_t)((_pendingCommandHead + _pendingCommandCount - 1) % MAX_PENDING_COMMANDS);
      _pendingCommandQueue[tail] = String();
      --_pendingCommandCount;
      _lastInboundDoc.remove(commandKey);
      return true;
    }
  }

  if (!_lastInboundDoc[commandKey].isNull()) {
    String currentCmd = _lastInboundDoc[commandKey].as<String>();
    currentCmd.trim();
    // I widget pubblicano identificatori di comando; il confronto non deve
    // fallire per differenze di maiuscole/minuscole o spazi accidentali nel
    // payload MQTT.
    if (expected.length() > 0 && currentCmd.equalsIgnoreCase(expected)) {
      // Consume a command after matching so it is delivered only once.
      _lastInboundDoc.remove(commandKey);
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
#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_NANO_RP2040_CONNECT)
#if defined(PIN_NINA_GPIO0)
  return !digitalRead(PIN_NINA_GPIO0);
#elif defined(NINA_GPIO0)
  return !digitalRead(NINA_GPIO0);
#else
  return !digitalRead(BUTTON_A);
#endif
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

bool MyDot::peekButtonAClicked() {
  const bool currentState = isButtonAPressed();
  return currentState && !_lastButtonAState &&
         (millis() - _lastPressTimeA > 50);
}

bool MyDot::peekButtonBClicked() {
  const bool currentState = isButtonBPressed();
  return currentState && !_lastButtonBState &&
         (millis() - _lastPressTimeB > 50);
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

bool MyDot::isRelayOn() {
  return digitalRead(RELAY) == HIGH;
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

void MyDot::updateSensorDisplay() {
  if (!displayPresent || !readSensors()) {
    return;
  }

  clearDisplay();
  setCursor(0, 0);
  setTextSize(1);
  setTextColor(SSD1306_WHITE);
  print("T: ");
  print(getTemperature());
  println(" C");
  print("P: ");
  print(getPressure());
  println(" hPa");
  print("H: ");
  print(getHumidity());
  println(" %");
  print("G: ");
  print(getGasResistance());
  println(" kOhm");
  showDisplay();
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

namespace {

// Each board family owns its memory API here.  The rest of the library never
// calls ESP.get*(), mbed_stats, or _sbrk() directly, so a missing core API can
// only disable the adapter instead of breaking compilation for another Nano.
bool readEsp32RamStats(uint32_t& freeBytes, uint32_t& totalBytes) {
#if defined(ARDUINO_ARCH_ESP32)
  totalBytes = (uint32_t)ESP.getHeapSize();
  freeBytes = (uint32_t)ESP.getFreeHeap();
  return totalBytes > 0;
#else
  (void)freeBytes;
  (void)totalBytes;
  return false;
#endif
}

bool readMbedRamStats(uint32_t& freeBytes, uint32_t& totalBytes) {
#if MYDOT_HAS_MBED_STATS
  mbed_stats_sys_t systemStats = {};
  mbed_stats_heap_t heapStats = {};
  mbed_stats_sys_get(&systemStats);
  mbed_stats_heap_get(&heapStats);
  for (size_t index = 0; index < MBED_MAX_MEM_REGIONS; ++index) {
    totalBytes += systemStats.ram_size[index];
  }
  if (totalBytes == 0 || heapStats.reserved_size == 0) {
    freeBytes = 0;
    totalBytes = 0;
    return false;
  }
  const uint32_t usedHeap = heapStats.current_size + heapStats.overhead_size;
  freeBytes = totalBytes > usedHeap ? totalBytes - usedHeap : 0;
  return true;
#else
  (void)freeBytes;
  (void)totalBytes;
  return false;
#endif
}

bool readSamdRamStats(uint32_t& freeBytes, uint32_t& totalBytes) {
#if defined(ARDUINO_SAMD_NANO_33_IOT)
  // The SAMD21G18A used by Nano 33 IoT has 32 KiB SRAM. The SAMD Arduino
  // core exposes the newlib heap break; comparing it with the stack marker
  // gives the currently usable gap without inventing a heap API.
  char stackMarker;
  char* heapEnd = static_cast<char*>(_sbrk(0));
  const intptr_t gap = &stackMarker - heapEnd;
  totalBytes = 32UL * 1024UL;
  freeBytes = gap > 0 ? static_cast<uint32_t>(gap) : 0;
  return gap > 0;
#else
  (void)freeBytes;
  (void)totalBytes;
  return false;
#endif
}

bool readRp2040RamStats(uint32_t& freeBytes, uint32_t& totalBytes) {
#if MYDOT_HAS_RP2040_STATS
  const int total = rp2040.getTotalHeap();
  const int free = rp2040.getFreeHeap();
  if (total <= 0 || free < 0) return false;
  totalBytes = static_cast<uint32_t>(total);
  freeBytes = static_cast<uint32_t>(free > total ? total : free);
  return true;
#else
  (void)freeBytes;
  (void)totalBytes;
  return false;
#endif
}

}  // namespace

bool MyDot::getMemoryStats(uint32_t& freeBytes, uint32_t& totalBytes) const {
  freeBytes = 0;
  totalBytes = 0;

#if MYDOT_HAS_RAM_STATUS
#if defined(ARDUINO_ARCH_ESP32)
  return readEsp32RamStats(freeBytes, totalBytes);
#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_NANO_RP2040_CONNECT)
  return readMbedRamStats(freeBytes, totalBytes);
#elif defined(ARDUINO_SAMD_NANO_33_IOT)
  return readSamdRamStats(freeBytes, totalBytes);
#elif defined(ARDUINO_ARCH_RP2040)
  return readRp2040RamStats(freeBytes, totalBytes);
#endif
#endif

  return false;
}

File MyDot::openFile(const char* filename, const char* mode) {
#if defined(ARDUINO_ARCH_ESP32)
  return SD.open(filename, mode);
#else
  // The Arduino SD library used by SAMD/RP2040 boards accepts numeric
  // OpenMode flags, while the ESP32 SD library accepts mode strings.
  int sdMode = FILE_READ;
  if (mode != nullptr) {
    if (strcmp(mode, "w") == 0) {
      SD.remove(filename);
      sdMode = FILE_WRITE;
    } else if (strcmp(mode, "a") == 0) {
      sdMode = FILE_WRITE;
    }
  }
  return SD.open(filename, sdMode);
#endif
}

bool MyDot::fileExists(const char* filename) {
  return SD.exists(filename);
}

bool MyDot::makeDirectory(const String& path) {
  if (path.length() == 0) {
    return false;
  }
  return SD.mkdir(path.c_str());
}

bool MyDot::createFile(const String& path) {
  if (path.length() == 0 || SD.exists(path)) {
    return false;
  }
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    return false;
  }
  file.close();
  return true;
}

void MyDot::removeFile(const char* filename) {
  if (filename == nullptr || filename[0] == '\0') {
    return;
  }
  // SD.remove() vale per i file e rifiuta le directory. Riconosciamo quindi
  // il tipo di voce prima della rimozione, così il Bridge può cancellare una
  // cartella vuota tramite l'operazione rmdir() prevista dalla libreria SD.
  File entry = SD.open(filename);
  if (entry) {
    const bool directory = entry.isDirectory();
    entry.close();
    if (directory) {
      SD.rmdir(filename);
      return;
    }
  }
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
  _currentFanSpeed = 0;
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
#if !MYDOT_HAS_WIFI
  return 0;
#else
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return 0;
#endif
}

unsigned long MyDot::getEpochTime() {
#if !MYDOT_HAS_WIFI
  return 0;
#else
  if (WiFi.status() != WL_CONNECTED) return 0;

#if defined(ARDUINO_ARCH_ESP32)
  time_t now;
  time(&now);
  return (unsigned long)now;
#else
  return WiFi.getTime();
#endif
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
