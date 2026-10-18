/*
 * MyDot Dev Studio Bridge
 *
 * Serial bridge for the Microeden Dev Studio web editor. The board
 * receives newline-delimited commands, exposes its supported runtime commands,
 * stores a command sequence on the MyDot microSD card, and executes that
 * sequence locally.
 *
 * The serial protocol is intentionally independent from the web UI:
 *
 *   HELP                         Show the human-readable protocol help
 *   CAPS                         List machine-readable capabilities
 *   ADD <runtime command>        Append one instruction to the sequence
 *   LIST                         List the current sequence
 *   CLEAR                        Remove all instructions from memory
 *   SAVE                         Encrypt and save MyDot.run on SD
 *   LOAD                         Load and decrypt MyDot.run
 *   RUN                          Execute the sequence from the first command
 *   STOP                         Stop execution and switch outputs off
 *   EXEC <runtime command>       Execute one command immediately
 *   STATUS                       Report connections, resources, sequence, and runtime status
 *   LOGS ON|OFF|STATUS           Enable/disable verbose resident-runtime logs
 *
 * If /MyDot.run is present on the microSD at boot, it is verified,
 * loaded and started automatically so the saved project resumes after a
 * power interruption. LOAD, RUN and STOP remain available over Serial.
 *
 * Runtime instructions:
 *
 *   DELAY <milliseconds>
 *   RELAY ON|OFF|TOGGLE | RELAY STATUS <variable>
 *   BRIGHTNESS <0..255>
 *   PIXELS <red> <green> <blue> | PIXELS_SHOW | PIXELS_RANDOM
 *   PIXEL <index> <red> <green> <blue>
 *   CLEAR_PIXELS
 *   FAN <-100..100> | FAN STOP|STATUS|CLEAR_FAULT
 *   DISPLAY <text>
 *   DISPLAY RAW_SHOW
 *   SENSORS
 *   SENSOR_READ TEMPERATURE|PRESSURE|HUMIDITY|GAS <variable>
 *   SENSORS_LOG <path>
 *   I2C BEGIN [frequency] | SCAN | PING <address>
 *   I2C WRITE <address> <byte...> | READ <address> <length> [prefix]
 *   I2C TRANSFER <address> <writeCount> <readCount> <byte...> [prefix]
 *   I2C READ_REG <address> <register> <length> [prefix]
 *   I2C WRITE_REG <address> <register> <value>
 *   SERIAL <text>
 *   WIFI BEGIN|STATUS|RSSI
 *   TIME EPOCH|FORMAT <gmtOffset>
 *   CLOUD BEGIN|WRITE_*|SEND|STATUS|BUFFER|SYNC|ON_COMMAND|READ|READ_ONCE
 *   NETWORK STOP (editor-only teardown of Wi-Fi and My Microeden services)
 *   SD BEGIN|READ|WRITE|WRITE_LINE|APPEND|APPEND_LINE|EXISTS|REMOVE|OPEN|LIST|MKDIR|TOUCH
 *   BUTTON A|B PRESSED|CLICKED (or BUTTON READ A|B PRESSED|CLICKED)
 *   DIGITAL_READ <pin> <variable> | DIGITAL_WRITE <pin> <value>
 *   ANALOG_READ <pin> <variable> | ANALOG_WRITE <pin> <0..255>
 *   SET <variable> <value> | GET <variable> | VARS | UNSET <variable> | CLEAR_VARS
 *   IF_VAR <variable> <operator> <value> GOTO <name>
 *   LABEL <name> | GOTO <name>
 *   IF_BUTTON A|B PRESSED|CLICKED GOTO <name>
 *   WAIT_BUTTON A|B PRESSED|CLICKED [timeoutMs] [GOTO label]
 *   REPEAT <count> ... ENDREPEAT | BREAK | CONTINUE
 *   COLOR HEX_TO_RGB|RGB_TO_HEX
 *   WIDGET WRITE|READ <type> <key> ...
 *   NOOP
 *
 * Example session:
 *
 *   CAPS
 *   CLEAR
 *   ADD BRIGHTNESS 32
 *   ADD PIXELS 0 80 255
 *   ADD DELAY 1000
 *   ADD CLEAR_PIXELS
 *   ADD REPEAT 3
 *   ADD PIXELS_RANDOM
 *   ADD DELAY 250
 *   ADD ENDREPEAT
 *   SAVE
 *   RUN
 *
 * I2C sensor block example:
 *
 *   EXEC I2C BEGIN 400000
 *   EXEC I2C SCAN
 *   EXEC I2C READ_REG 0x76 0xD0 1 WHO
 *   EXEC SERIAL sensor-id=$WHO0
 *
 * The runtime file uses a portable XTEA-CTR stream encryption and a CRC32
 * integrity check. The key is compiled into this firmware, so the file is
 * protected from casual inspection but is not a substitute for secure key
 * storage if an attacker can extract the firmware.
 *
 * Hardware note: insert a FAT-formatted microSD card and power the MyDot
 * carrier through its external DC jack when using relay, NeoPixels, or fan
 * outputs. USB power alone is not sufficient for the carrier output stages.
 */

#include <MyDot.h>
#include <Wire.h>
#include <ctype.h>
#include <math.h>
#include "microeden_secrets.h"
#include "runtime_integer_expression.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>
#include <soc/soc_caps.h>
#endif

#if !defined(ARDUINO_ARCH_ESP32)
// The portable Arduino SD library is a thin wrapper around SdFat.  Its
// public SDClass intentionally omits the ESP32-only totalBytes()/usedBytes()
// helpers, but the underlying volume geometry is available on SAMD/RP2040.
#include <utility/SdFat.h>
#endif

// Keep unsolicited Bridge output off the USB port until the user requests
// verbose diagnostics. Protocol replies and explicit SERIAL instructions are
// enabled only for their duration. A slow browser must not dictate the cloud
// loop rate.
Stream& bridgePhysicalSerial() { return Serial; }
void beginBridgePhysicalSerial(unsigned long baud) { Serial.begin(baud); }
void limitBridgeUsbWriteWait() {
#if defined(ARDUINO_NANO_ESP32) && defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  // Keep this above zero: some ESP32 USB CDC core versions mishandle a zero
  // TX timeout. A stalled web reader should lose logs, not stop the runtime.
  Serial.setTxTimeoutMs(10);
#endif
}

class BridgeSerialOutput : public Stream {
 public:
  void begin(unsigned long baud) { beginBridgePhysicalSerial(baud); }
  int available() override { return bridgePhysicalSerial().available(); }
  int read() override { return bridgePhysicalSerial().read(); }
  int peek() override { return bridgePhysicalSerial().peek(); }
  void flush() override {
    if (outputEnabled) bridgePhysicalSerial().flush();
  }

  size_t write(uint8_t value) override { return write(&value, 1); }
  size_t write(const uint8_t* data, size_t size) override {
    return outputEnabled ? bridgePhysicalSerial().write(data, size) : size;
  }
  using Print::write;

  void setOutputEnabled(bool enabled) { outputEnabled = enabled; }
  bool isOutputEnabled() const { return outputEnabled; }

 private:
  bool outputEnabled = false;
};

BridgeSerialOutput bridgeSerial;
#undef Serial
#define Serial bridgeSerial

const char* RUNTIME_FILE = "/MyDot.run";
const uint16_t MAX_COMMAND_LENGTH = 96;
const uint16_t MAX_RUNTIME_BYTES = 4096;
const uint16_t MAX_SERIAL_LINE_LENGTH = 192;
const uint8_t MAX_RUNTIME_TASKS = 16;
// A browser can queue commands faster than they can be executed. Bound USB
// receive work so the network and resident runtime still get CPU time.
const uint8_t MAX_SERIAL_BYTES_PER_PASS = 96;
// Byte di controllo fuori banda. Il Dev Studio lo invia senza attendere la
// coda delle richieste normali, così STOP può essere rilevato anche mentre
// una lettura/scrittura della microSD è ancora in corso.
const uint8_t SERIAL_EMERGENCY_STOP = 0x03;
const uint8_t MAX_LOOP_DEPTH = 8;
const uint8_t MAX_RUNTIME_VARIABLES = 48;
// Numero massimo di pin GPIO che il runtime può aver portato in uscita. È un
// limite sui pin fisici, non sul numero di istruzioni del programma; basta a
// coprire tutti i GPIO utilizzabili dalle schede Nano supportate.
const uint8_t MAX_RUNTIME_OUTPUT_PINS = 32;
const uint8_t MAX_VARIABLE_NAME_LENGTH = 16;
const uint8_t MAX_VARIABLE_VALUE_LENGTH = 64;
const uint8_t MAX_I2C_TRANSFER_BYTES = 32;
const uint32_t DEFAULT_I2C_FREQUENCY = 100000UL;
const uint16_t I2C_TRANSACTION_TIMEOUT_MS = 50;
const uint8_t I2C_READ_RETRIES = 2;
const uint32_t BRIDGE_TASK_WATCHDOG_TIMEOUT_MS = 15000UL;
const uint32_t FILE_FORMAT_VERSION = 1;

// This key identifies this bridge firmware. Change it for a product-specific
// build; files made with another key intentionally fail the CRC check.
const uint32_t RUNTIME_KEY[4] = {
  0x4D79446FUL, 0x745F5274UL, 0x6E74696DUL, 0x653A7631UL
};

MyDot dot;
// Il programma runtime resta nel buffer decrittografato del file .run e viene
// interpretato una riga alla volta. Non usiamo più un array fisso di 32
// String: il limite effettivo è la dimensione complessiva del programma.
uint16_t runtimeCommandCount = 0;
uint16_t runtimeScriptLength = 0;
bool sdReady = false;
bool i2cReady = false;
uint32_t i2cFrequency = DEFAULT_I2C_FREQUENCY;
bool runtimeRunning = false;
bool runtimeLogsEnabled = false;
uint16_t runtimeIndex = 0;
unsigned long runtimeWaitUntil = 0;
struct RuntimeLoopFrame {
  uint16_t bodyIndex;
  uint16_t endIndex;
  uint32_t remaining;
};
RuntimeLoopFrame runtimeLoopStack[MAX_LOOP_DEPTH];
uint8_t runtimeLoopDepth = 0;
struct RuntimeTaskState {
  bool active;
  bool repeating;
  uint16_t startIndex;
  uint16_t endIndex;
  uint16_t index;
  unsigned long waitUntil;
  RuntimeLoopFrame loopStack[MAX_LOOP_DEPTH];
  uint8_t loopDepth;
  bool waitingForButton;
  uint8_t waitButton;
  bool waitForClick;
  bool waitHasTimeout;
  unsigned long buttonWaitUntil;
  bool buttonAClickPending;
  bool buttonBClickPending;
  bool waitingForCloudCommand;
  String cloudExpectedCommand;
};
RuntimeTaskState runtimeTasks[MAX_RUNTIME_TASKS];
uint8_t runtimeTaskCount = 0;
uint8_t runtimeTaskCursor = 0;
uint8_t runtimeCurrentTask = 0xFF;
struct RuntimeVariable {
  String name;
  String value;
  bool used;
};
RuntimeVariable runtimeVariables[MAX_RUNTIME_VARIABLES];
struct RuntimeOutputPin {
  int pin;
  bool analog;
};
RuntimeOutputPin runtimeOutputPins[MAX_RUNTIME_OUTPUT_PINS];
uint8_t runtimeOutputPinCount = 0;
bool runtimeWaitingForButton = false;
uint8_t runtimeWaitButton = 0;
bool runtimeWaitForClick = false;
bool runtimeWaitHasTimeout = false;
unsigned long runtimeButtonWaitUntil = 0;
// CLOUD ON_COMMAND is a cooperative wait, like WAIT_BUTTON. The optional
// `GOTO <label>` form is a non-blocking poll used by the continuous-flow
// dispatcher so multiple Cloud/widget listeners can share one runtime.
bool runtimeWaitingForCloudCommand = false;
String runtimeCloudExpectedCommand;
// I click fisici vengono memorizzati mentre il bridge esegue altre attività
// (per esempio una transazione I2C o una riconnessione). Senza questo latch
// un click breve poteva cadere tra due chiamate a serviceRuntime().
bool runtimeButtonAClickPending = false;
bool runtimeButtonBClickPending = false;
char serialLine[MAX_SERIAL_LINE_LENGTH + 1] = {};
uint16_t serialLineLength = 0;
bool serialLineOverflow = false;
bool emergencyStopRequested = false;
bool rebootRequested = false;
bool rebootRequestedFromSerial = false;
bool runtimeTransientFailure = false;
char decryptedScript[MAX_RUNTIME_BYTES + 1];

#if defined(ARDUINO_ARCH_ESP32)
bool bridgeTaskWatchdogReady = false;
const uint32_t BRIDGE_TRACE_MAGIC = 0x4D445442UL;
enum BridgeStage : uint8_t {
  BRIDGE_STAGE_SETUP = 1,
  BRIDGE_STAGE_SERIAL,
  BRIDGE_STAGE_BUTTONS,
  BRIDGE_STAGE_NETWORK,
  BRIDGE_STAGE_RUNTIME,
  BRIDGE_STAGE_COMMAND,
  BRIDGE_STAGE_REBOOT,
  BRIDGE_STAGE_IDLE,
  BRIDGE_STAGE_SERIAL_STATUS,
  BRIDGE_STAGE_SERIAL_CAPS,
  BRIDGE_STAGE_SERIAL_EXEC,
  BRIDGE_STAGE_SERIAL_OTHER,
  BRIDGE_STAGE_WIDGET_PUBLISH
};
struct BridgeTrace {
  uint32_t magic;
  uint32_t magicInverse;
  uint32_t freeHeap;
  uint32_t largestBlock;
  uint32_t minimumFreeHeap;
  uint16_t commandIndex;
  uint8_t stage;
};
// RTC memory survives the watchdog's warm reboot without a flash write on
// every instruction. Keep only fixed-size diagnostics, never credentials.
#if SOC_RTC_FAST_MEM_SUPPORTED || SOC_RTC_SLOW_MEM_SUPPORTED
RTC_NOINIT_ATTR BridgeTrace rtcBridgeTrace;
#else
BridgeTrace rtcBridgeTrace;
#endif
BridgeTrace previousBridgeTrace = {};
bool previousBridgeTraceValid = false;
esp_reset_reason_t bridgeResetReason = ESP_RST_UNKNOWN;
#endif

const char* currentBoardName();

void markBridgeStage(uint8_t stage, uint16_t commandIndex = 0) {
#if defined(ARDUINO_ARCH_ESP32)
  rtcBridgeTrace.commandIndex = commandIndex;
  rtcBridgeTrace.stage = stage;
#else
  (void)stage;
  (void)commandIndex;
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
const char* bridgeStageName(uint8_t stage) {
  switch (stage) {
    case BRIDGE_STAGE_SETUP: return "setup";
    case BRIDGE_STAGE_SERIAL: return "serial";
    case BRIDGE_STAGE_BUTTONS: return "buttons";
    case BRIDGE_STAGE_NETWORK: return "network";
    case BRIDGE_STAGE_RUNTIME: return "runtime";
    case BRIDGE_STAGE_COMMAND: return "command";
    case BRIDGE_STAGE_REBOOT: return "reboot";
    case BRIDGE_STAGE_IDLE: return "idle";
    case BRIDGE_STAGE_SERIAL_STATUS: return "serial-status";
    case BRIDGE_STAGE_SERIAL_CAPS: return "serial-caps";
    case BRIDGE_STAGE_SERIAL_EXEC: return "serial-exec";
    case BRIDGE_STAGE_SERIAL_OTHER: return "serial-other";
    case BRIDGE_STAGE_WIDGET_PUBLISH: return "widget-publish";
    default: return "unknown";
  }
}

void beginBridgeTrace() {
  bridgeResetReason = esp_reset_reason();
  previousBridgeTraceValid = bridgeResetReason != ESP_RST_POWERON &&
                             rtcBridgeTrace.magic == BRIDGE_TRACE_MAGIC &&
                             rtcBridgeTrace.magicInverse == ~BRIDGE_TRACE_MAGIC;
  if (previousBridgeTraceValid) previousBridgeTrace = rtcBridgeTrace;
  rtcBridgeTrace.magic = BRIDGE_TRACE_MAGIC;
  rtcBridgeTrace.magicInverse = ~BRIDGE_TRACE_MAGIC;
  rtcBridgeTrace.freeHeap = 0;
  rtcBridgeTrace.largestBlock = 0;
  rtcBridgeTrace.minimumFreeHeap = 0;
  markBridgeStage(BRIDGE_STAGE_SETUP);
}

void sampleBridgeMemory() {
  rtcBridgeTrace.freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  rtcBridgeTrace.largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  rtcBridgeTrace.minimumFreeHeap = esp_get_minimum_free_heap_size();
}
#endif

// The cooperative MyDot watchdog is useful while the main loop is healthy,
// but it cannot run if a core API blocks inside a driver call. The ESP32 task
// watchdog is the final recovery path for that case: after a reboot, setup()
// automatically loads /MyDot.run again.
void beginBridgeTaskWatchdog() {
#if defined(ARDUINO_ARCH_ESP32)
  esp_err_t status = ESP_OK;
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config = {};
  config.timeout_ms = BRIDGE_TASK_WATCHDOG_TIMEOUT_MS;
  config.idle_core_mask = 0;
  config.trigger_panic = true;
  status = esp_task_wdt_init(&config);
  if (status == ESP_ERR_INVALID_STATE) {
    // The ESP32 core may have initialized the watchdog before setup().
    status = esp_task_wdt_reconfigure(&config);
  }
#else
  status = esp_task_wdt_init(BRIDGE_TASK_WATCHDOG_TIMEOUT_MS / 1000UL, true);
  if (status == ESP_ERR_INVALID_STATE) status = ESP_OK;
#endif
  if (status == ESP_OK && esp_task_wdt_status(nullptr) != ESP_OK) {
    status = esp_task_wdt_add(nullptr);
  }
  bridgeTaskWatchdogReady = status == ESP_OK;
  if (!bridgeTaskWatchdogReady) {
    Serial.print(F("WARN bridge watchdog unavailable code="));
    Serial.println((int)status);
  }
#endif
}

void feedBridgeTaskWatchdog() {
#if defined(ARDUINO_ARCH_ESP32)
  if (bridgeTaskWatchdogReady) {
    esp_task_wdt_reset();
  }
#endif
}

void publishBridgeCloudSync() {
  dot.writeKeyWord("bridge", "MyDotDevStudioBridge");
  dot.writeKeyWord("runtimeCommands", (int)runtimeCommandCount);
  const bool sent = dot.sendCloud();
  Serial.println(sent ? F("OK CLOUD SYNC SEND") : F("WARN CLOUD SYNC SEND unavailable"));
}

struct RuntimeFileHeader {
  uint8_t magic[8];
  uint32_t version;
  uint32_t nonce;
  uint32_t payloadLength;
  uint32_t payloadCrc;
};

const uint8_t RUNTIME_MAGIC[8] = {
  'M', 'Y', 'D', 'O', 'T', 'R', 'T', '1'
};

void xteaEncrypt(uint32_t& v0, uint32_t& v1) {
  const uint32_t delta = 0x9E3779B9UL;
  uint32_t sum = 0;
  for (uint8_t round = 0; round < 32; ++round) {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^
          (sum + RUNTIME_KEY[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^
          (sum + RUNTIME_KEY[(sum >> 11) & 3]);
  }
}

class RuntimeCipher {
public:
  explicit RuntimeCipher(uint32_t nonce)
    : _nonce(nonce), _counter(0), _streamIndex(sizeof(_stream)) {}

  uint8_t crypt(uint8_t input) {
    if (_streamIndex >= sizeof(_stream)) {
      uint32_t left = _nonce;
      uint32_t right = _counter++;
      xteaEncrypt(left, right);
      _stream[0] = (uint8_t)(left & 0xFF);
      _stream[1] = (uint8_t)((left >> 8) & 0xFF);
      _stream[2] = (uint8_t)((left >> 16) & 0xFF);
      _stream[3] = (uint8_t)((left >> 24) & 0xFF);
      _stream[4] = (uint8_t)(right & 0xFF);
      _stream[5] = (uint8_t)((right >> 8) & 0xFF);
      _stream[6] = (uint8_t)((right >> 16) & 0xFF);
      _stream[7] = (uint8_t)((right >> 24) & 0xFF);
      _streamIndex = 0;
    }
    return input ^ _stream[_streamIndex++];
  }

private:
  uint32_t _nonce;
  uint32_t _counter;
  uint8_t _stream[8];
  uint8_t _streamIndex;
};

uint32_t crc32Update(uint32_t crc, uint8_t value) {
  crc ^= value;
  for (uint8_t bit = 0; bit < 8; ++bit) {
    crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
  }
  return crc;
}

uint32_t randomNonce() {
  return ((uint32_t)micros() << 16) ^ millis() ^
         (uint32_t)analogRead(A3) ^ (uint32_t)analogRead(A2);
}

bool writeUint32(File& file, uint32_t value) {
  uint8_t bytes[4] = {
    (uint8_t)(value & 0xFF),
    (uint8_t)((value >> 8) & 0xFF),
    (uint8_t)((value >> 16) & 0xFF),
    (uint8_t)((value >> 24) & 0xFF)
  };
  return file.write(bytes, sizeof(bytes)) == sizeof(bytes);
}

bool readUint32(File& file, uint32_t& value) {
  uint8_t bytes[4];
  if (file.read(bytes, sizeof(bytes)) != sizeof(bytes)) {
    return false;
  }
  value = (uint32_t)bytes[0] |
          ((uint32_t)bytes[1] << 8) |
          ((uint32_t)bytes[2] << 16) |
          ((uint32_t)bytes[3] << 24);
  return true;
}

String runtimeScriptLine(uint16_t start, uint16_t end) {
  if (start >= runtimeScriptLength) return String();
  if (end > runtimeScriptLength) end = runtimeScriptLength;
  char saved = decryptedScript[end];
  decryptedScript[end] = '\0';
  String line = String(decryptedScript + start);
  decryptedScript[end] = saved;
  line.trim();
  return line;
}

// Restituisce la riga runtime richiesta senza duplicare l'intero programma in
// un array di String. La scansione è lineare sul piccolo buffer del file .run;
// in questo modo il numero di istruzioni è vincolato solo ai byte disponibili.
String runtimeCommandAt(uint16_t target) {
  uint16_t current = 0;
  uint16_t start = 0;
  for (uint16_t index = 0; index <= runtimeScriptLength; ++index) {
    if (index < runtimeScriptLength && decryptedScript[index] != '\n') continue;
    // The continuous runtime used to allocate a String for every preceding
    // line on every instruction. Only allocate for the requested command;
    // otherwise a looping script creates thousands of needless heap churns.
    bool hasContent = false;
    for (uint16_t character = start; character < index; ++character) {
      const char value = decryptedScript[character];
      if (value != ' ' && value != '\t' && value != '\r') {
        hasContent = true;
        break;
      }
    }
    if (hasContent) {
      if (current == target) return runtimeScriptLine(start, index);
      ++current;
    }
    start = index + 1;
  }
  return String();
}

bool rebuildRuntimeCommandIndex(bool validate = true) {
  uint16_t count = 0;
  uint16_t start = 0;
  for (uint16_t index = 0; index <= runtimeScriptLength; ++index) {
    if (index < runtimeScriptLength && decryptedScript[index] != '\n') continue;
    String line = runtimeScriptLine(start, index);
    if (line.length() > 0) {
      if (validate && line.length() > MAX_COMMAND_LENGTH) return false;
      ++count;
    }
    start = index + 1;
  }
  runtimeCommandCount = count;
  return true;
}

size_t scriptLength() {
  return runtimeScriptLength;
}

uint32_t scriptCrc() {
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint16_t index = 0; index < runtimeScriptLength; ++index) {
    crc = crc32Update(crc, (uint8_t)decryptedScript[index]);
  }
  return ~crc;
}

void printHelp() {
  Serial.println(F("MyDot Dev Studio Bridge protocol:"));
  Serial.println(F("  HELP | CAPS | STATUS | LOGS ON|OFF|STATUS"));
  Serial.println(F("  ADD <runtime command>"));
  Serial.println(F("  LIST | CLEAR | SAVE | LOAD | RUN | STOP"));
  Serial.println(F("  REBOOT"));
  Serial.println(F("  EXEC <runtime command>"));
  Serial.println(F("Runtime commands:"));
  Serial.println(F("  DELAY <ms>"));
  Serial.println(F("  RELAY ON|OFF|TOGGLE | RELAY STATUS <variable>"));
  Serial.println(F("  BRIGHTNESS <0..255>"));
  Serial.println(F("  PIXELS <r> <g> <b> | PIXELS_SHOW | PIXELS_RANDOM"));
  Serial.println(F("  PIXEL <index> <r> <g> <b>"));
  Serial.println(F("  CLEAR_PIXELS | FAN <-100..100> | FAN STOP|STATUS|CLEAR_FAULT"));
  Serial.println(F("  DISPLAY <text> or DISPLAY ... | SENSORS | SENSOR_READ ... | SENSORS_LOG <path> | SERIAL <text> | NOOP"));
  Serial.println(F("  I2C BEGIN [frequency] | SCAN | PING <address>"));
  Serial.println(F("  I2C WRITE <address> <byte...> | READ <address> <length> [prefix]"));
  Serial.println(F("  I2C TRANSFER <address> <writeCount> <readCount> <byte...> [prefix]"));
  Serial.println(F("  I2C READ_REG <address> <register> <length> [prefix] | WRITE_REG <address> <register> <value>"));
  Serial.println(F("  WIFI ... | TIME ... | CLOUD ... | SD ... | SD LIST [path]"));
  Serial.println(F("  NETWORK STOP (stop Wi-Fi and My Microeden services)"));
  Serial.println(F("  WATCHDOG BEGIN <ms> | FEED | STOP (runtime; use EXEC for immediate)"));
  Serial.println(F("  BUTTON ... (A/B PRESSED/CLICKED or READ A/B ...) | COLOR ... | WIDGET ..."));
  Serial.println(F("  DIGITAL_READ/WRITE | ANALOG_READ/WRITE"));
  Serial.println(F("  SET <name> <value> | GET <name> | VARS | UNSET <name> | CLEAR_VARS"));
  Serial.println(F("  BASE_CONVERT <value> <fromBase> <toBase> <variable> (bases 2..36)"));
  Serial.println(F("  IF_VAR <name> <op> <value> GOTO <label>"));
  Serial.println(F("  LABEL <name> | GOTO <name> | IF_BUTTON A|B PRESSED|CLICKED GOTO <name>"));
  Serial.println(F("  WAIT_BUTTON A|B PRESSED|CLICKED [timeoutMs] [GOTO label]"));
  Serial.println(F("  REPEAT <count> ... ENDREPEAT | BREAK | CONTINUE"));
}

void printCapabilities() {
  Serial.println(F("CAPS VERSION 3"));
  Serial.println(F("CAPS CONTROL emergencyStop=ETX"));
  Serial.print(F("CAPS BOARD name="));
  Serial.println(currentBoardName());
  Serial.print(F("CAPS FEATURE wifi="));
  Serial.print(MYDOT_HAS_WIFI ? F("supported") : F("unavailable"));
  Serial.println();
  Serial.print(F("CAPS FEATURE cloud="));
  Serial.print(MYDOT_HAS_CLOUD ? F("supported") : F("unavailable"));
  Serial.println();
  Serial.print(F("CAPS FEATURE ram="));
  Serial.print(MYDOT_HAS_RAM_STATUS ? F("supported") : F("unavailable"));
  Serial.print(F(" backend="));
  Serial.println(F(MYDOT_RAM_BACKEND));
  Serial.print(F("CAPS FEATURE sdCapacity="));
  Serial.println(MYDOT_HAS_SD_CAPACITY ? F("supported") : F("unavailable"));
  Serial.print(F("CAPS FEATURE reboot="));
  Serial.println(dot.rebootSupported() ? F("supported") : F("unavailable"));
  Serial.print(F("CAPS FEATURE watchdog="));
  Serial.println(dot.rebootSupported() ? F("cooperative") : F("unavailable"));
  Serial.println(F("CAPS FEATURE parallelTasks=supported"));
  Serial.println(F("CAPS FEATURE runtimeTasks=16"));
  Serial.print(F("CAPS PINS "));
  Serial.println(F(MYDOT_PIN_MAP_NAME));
  Serial.println(F("CAPS TOPLEVEL HELP CAPS STATUS LOGS ADD LIST CLEAR SAVE LOAD RUN STOP REBOOT EXEC"));
  Serial.print(F("CAPS RUNTIME DELAY RELAY BRIGHTNESS PIXELS PIXEL PIXELS_SHOW PIXELS_RANDOM CLEAR_PIXELS FAN DISPLAY SENSORS SENSOR_READ SENSORS_LOG I2C SERIAL"));
#if MYDOT_HAS_WIFI
  Serial.print(F(" WIFI TIME"));
#endif
#if MYDOT_HAS_CLOUD
  Serial.print(F(" CLOUD"));
#endif
#if MYDOT_HAS_SD_CAPACITY
  Serial.print(F(" SD"));
#endif
  Serial.println(F(" BUTTON COLOR WIDGET DIGITAL_READ DIGITAL_WRITE ANALOG_READ ANALOG_WRITE SET BASE_CONVERT GET VARS UNSET CLEAR_VARS IF_VAR LABEL GOTO IF_BUTTON WAIT_BUTTON REPEAT ENDREPEAT BREAK CONTINUE WATCHDOG REBOOT NOOP"));
  // Non esiste più un tetto fisso al numero di righe: il limite effettivo è
  // la dimensione del payload e la lunghezza massima della singola riga.
  Serial.println(F("CAPS LIMITS commandLength=96 scriptBytes=4096"));
  Serial.println(F("CAPS FILE /MyDot.run encrypted=xtea-ctr crc=crc32"));

  Serial.println(F("CAPS COMMAND HELP args=none type=action"));
  Serial.println(F("CAPS COMMAND CAPS args=none type=metadata"));
  Serial.println(F("CAPS COMMAND STATUS args=none type=status"));
  Serial.println(F("CAPS COMMAND LOGS args=state type=enum values=ON|OFF|STATUS default=OFF"));
  Serial.println(F("CAPS COMMAND ADD args=command type=runtime"));
  Serial.println(F("CAPS COMMAND LIST args=none type=action"));
  Serial.println(F("CAPS COMMAND CLEAR args=none type=action"));
  Serial.println(F("CAPS COMMAND SAVE args=none type=storage"));
  Serial.println(F("CAPS COMMAND LOAD args=none type=storage"));
  Serial.println(F("CAPS COMMAND RUN args=none type=control"));
  Serial.println(F("CAPS COMMAND STOP args=none type=control"));
  if (dot.rebootSupported()) {
    Serial.println(F("CAPS COMMAND REBOOT args=none type=control"));
    Serial.println(F("CAPS COMMAND WATCHDOG_BEGIN args=timeoutMs type=control range=100..4294967295"));
    Serial.println(F("CAPS COMMAND WATCHDOG_FEED args=none type=control"));
    Serial.println(F("CAPS COMMAND WATCHDOG_STOP args=none type=control"));
  }
  // This command is metadata rather than a palette block: the Dev Studio
  // sends it automatically after STOP while programming the board.
  Serial.println(F("CAPS COMMAND NETWORK_STOP args=none type=metadata"));
  Serial.println(F("CAPS COMMAND EXEC args=command type=runtime"));

  Serial.println(F("CAPS COMMAND NOOP args=none type=action"));
  Serial.println(F("CAPS COMMAND DELAY args=milliseconds type=uint range=0..300000"));
  Serial.println(F("CAPS COMMAND SET args=name,value type=variable"));
  Serial.println(F("CAPS COMMAND BASE_CONVERT args=value,fromBase,toBase,variable type=variable result=variable"));
  Serial.println(F("CAPS COMMAND GET args=name type=variable"));
  Serial.println(F("CAPS COMMAND VARS args=none type=variable"));
  Serial.println(F("CAPS COMMAND UNSET args=name type=variable"));
  Serial.println(F("CAPS COMMAND CLEAR_VARS args=none type=variable"));
  Serial.println(F("CAPS COMMAND DIGITAL_READ args=pin,variable type=io"));
  Serial.println(F("CAPS COMMAND DIGITAL_WRITE args=pin,value type=io values=HIGH|LOW|ON|OFF|TRUE|FALSE"));
  Serial.println(F("CAPS COMMAND ANALOG_READ args=pin,variable type=io"));
  Serial.println(F("CAPS COMMAND ANALOG_WRITE args=pin,value type=io range=0..255"));
  Serial.println(F("CAPS COMMAND IF_VAR args=name,operator,value,GOTO,label type=control operators==|=|!=|>|>=|<|<="));
  Serial.println(F("CAPS COMMAND LABEL args=name type=control"));
  Serial.println(F("CAPS COMMAND GOTO args=name type=control"));
  Serial.println(F("CAPS COMMAND IF_BUTTON args=button,state,GOTO,label type=control values=A|B,PRESSED|CLICKED"));
  Serial.println(F("CAPS COMMAND WAIT_BUTTON args=button,state,timeoutMs type=control values=A|B,PRESSED|CLICKED"));
  Serial.println(F("CAPS COMMAND REPEAT args=count type=control range=1..4294967295"));
  Serial.println(F("CAPS COMMAND ENDREPEAT args=none type=control"));
  Serial.println(F("CAPS COMMAND BREAK args=none type=control"));
  Serial.println(F("CAPS COMMAND CONTINUE args=none type=control"));

  Serial.println(F("CAPS COMMAND RELAY args=state type=enum values=ON|OFF|TOGGLE|STATUS result=variable"));
  Serial.println(F("CAPS COMMAND BRIGHTNESS args=value type=uint range=0..255"));
  Serial.println(F("CAPS COMMAND PIXELS args=red,green,blue type=rgb range=0..255"));
  Serial.println(F("CAPS COMMAND PIXELS_SHOW args=none type=action"));
  Serial.println(F("CAPS COMMAND PIXELS_RANDOM args=none type=action"));
  Serial.println(F("CAPS COMMAND PIXEL args=index,red,green,blue type=rgb range=0..255"));
  Serial.println(F("CAPS COMMAND CLEAR_PIXELS args=none type=action"));
  // FAN accepts either a signed speed or one of the control words. Keep the
  // positional argument stable for the editor; the inspector renders it as
  // text so STOP/STATUS/CLEAR_FAULT remain available alongside -100..100.
  Serial.println(F("CAPS COMMAND FAN args=speed type=fan range=-100..100"));

  Serial.println(F("CAPS COMMAND DISPLAY_TEXT args=text type=string"));
  Serial.println(F("CAPS COMMAND DISPLAY_PRESENT args=none type=bool"));
  Serial.println(F("CAPS COMMAND DISPLAY_LOGO args=none type=action"));
  Serial.println(F("CAPS COMMAND DISPLAY_SENSOR args=none type=action"));
  Serial.println(F("CAPS COMMAND DISPLAY_CLEAR args=none type=action"));
  Serial.println(F("CAPS COMMAND DISPLAY_SHOW args=none type=action"));
  Serial.println(F("CAPS COMMAND DISPLAY_RAW_SHOW args=none type=action"));
  Serial.println(F("CAPS COMMAND DISPLAY_CURSOR args=x,y type=int"));
  Serial.println(F("CAPS COMMAND DISPLAY_SIZE args=size type=uint range=1..4"));
  Serial.println(F("CAPS COMMAND DISPLAY_COLOR args=color type=uint16 range=0..65535"));
  Serial.println(F("CAPS COMMAND DISPLAY_TEXT_AT args=x,y,size,text type=display"));
  Serial.println(F("CAPS COMMAND DISPLAY_PRINT args=text type=string"));
  Serial.println(F("CAPS COMMAND DISPLAY_PRINTLN args=text type=string"));
  Serial.println(F("CAPS COMMAND DISPLAY_NEWLINE args=none type=action"));
  Serial.println(F("CAPS COMMAND SENSORS args=none type=bme690"));
  Serial.println(F("CAPS COMMAND SENSOR_READ args=field,variable type=bme690 values=TEMPERATURE|PRESSURE|HUMIDITY|GAS"));
  Serial.println(F("CAPS COMMAND SENSORS_LOG args=path type=csv"));
  Serial.println(F("CAPS COMMAND I2C_BEGIN args=frequency type=i2c range=10000..1000000"));
  Serial.println(F("CAPS COMMAND I2C_SCAN args=none type=i2c"));
  Serial.println(F("CAPS COMMAND I2C_PING args=address type=i2c range=0x03..0x77"));
  Serial.println(F("CAPS COMMAND I2C_WRITE args=address,bytes type=i2c range=1..32"));
  Serial.println(F("CAPS COMMAND I2C_READ args=address,length,prefix type=i2c range=1..32"));
  Serial.println(F("CAPS COMMAND I2C_TRANSFER args=address,writeCount,readCount,bytes,prefix type=i2c range=0..32"));
  Serial.println(F("CAPS COMMAND I2C_READ_REG args=address,register,length,prefix type=i2c range=1..32"));
  Serial.println(F("CAPS COMMAND I2C_WRITE_REG args=address,register,value type=i2c"));
  Serial.println(F("CAPS COMMAND SERIAL args=text type=string"));

#if MYDOT_HAS_WIFI
  Serial.println(F("CAPS COMMAND WIFI_BEGIN args=ssid,password type=wifi"));
  Serial.println(F("CAPS COMMAND WIFI_STATUS args=none type=status"));
  Serial.println(F("CAPS COMMAND WIFI_RSSI args=none type=int"));
  Serial.println(F("CAPS COMMAND TIME_EPOCH args=none type=uint32"));
  Serial.println(F("CAPS COMMAND TIME_FORMAT args=gmtOffset type=int"));
#endif

#if MYDOT_HAS_CLOUD
  Serial.println(F("CAPS COMMAND CLOUD_BEGIN args=deviceId,token type=cloud"));
  Serial.println(F("CAPS COMMAND CLOUD_SEND args=none type=cloud"));
  Serial.println(F("CAPS COMMAND CLOUD_STATUS args=none type=status"));
  Serial.println(F("CAPS COMMAND CLOUD_BUFFER args=size type=uint range=128..65535"));
  Serial.println(F("CAPS COMMAND CLOUD_SYNC args=intervalMs type=uint"));
  Serial.println(F("CAPS COMMAND CLOUD_WRITE_TEXT args=key,value type=string"));
  Serial.println(F("CAPS COMMAND CLOUD_WRITE_INT args=key,value type=int"));
  Serial.println(F("CAPS COMMAND CLOUD_WRITE_FLOAT args=key,value type=float"));
  Serial.println(F("CAPS COMMAND CLOUD_WRITE_DOUBLE args=key,value type=double"));
  Serial.println(F("CAPS COMMAND CLOUD_WRITE_BOOL args=key,value type=bool values=TRUE|FALSE"));
  Serial.println(F("CAPS COMMAND CLOUD_ON_COMMAND args=expected,key type=command"));
  Serial.println(F("CAPS COMMAND CLOUD_READ args=key type=string"));
  Serial.println(F("CAPS COMMAND CLOUD_READ_ONCE args=key type=string"));
#endif

#if MYDOT_HAS_SD_CAPACITY
  Serial.println(F("CAPS COMMAND SD_BEGIN args=none type=storage"));
  Serial.println(F("CAPS COMMAND SD_EXISTS args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_MKDIR args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_TOUCH args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_REMOVE args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_WRITE args=path,text type=storage"));
  Serial.println(F("CAPS COMMAND SD_WRITE_LINE args=path,text type=storage"));
  Serial.println(F("CAPS COMMAND SD_APPEND args=path,text type=storage"));
  Serial.println(F("CAPS COMMAND SD_APPEND_LINE args=path,text type=storage"));
  Serial.println(F("CAPS COMMAND SD_OPEN args=path,mode type=storage values=r|w|a"));
  Serial.println(F("CAPS COMMAND SD_READ args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_READ_B64 args=path type=storage"));
  Serial.println(F("CAPS COMMAND SD_LIST args=path type=storage"));
#endif

  Serial.println(F("CAPS COMMAND BUTTON_STATUS args=none type=button"));
  Serial.println(F("CAPS COMMAND BUTTON_READ args=button,state type=button values=A|B,PRESSED|CLICKED"));
  Serial.println(F("CAPS COMMAND COLOR_HEX_TO_RGB args=hex type=color"));
  Serial.println(F("CAPS COMMAND COLOR_RGB_TO_HEX args=red,green,blue type=color range=0..255"));
  Serial.println(F("CAPS COMMAND WIDGET_READ args=type,key type=widget"));
  Serial.println(F("CAPS COMMAND WIDGET_WRITE args=type,key,value type=widget values=RAW|TEXT|COLOR|MAP|LEVEL|SLIDER|SWITCH|PUSHBUTTON|LED|PHOTO"));
  Serial.println(F("CAPS END"));
}

String takeToken(String& text) {
  text.trim();
  if (text.length() == 0) {
    return String();
  }
  int separator = text.indexOf(' ');
  if (separator < 0) {
    String token = text;
    text = String();
    return token;
  }
  String token = text.substring(0, separator);
  text = text.substring(separator + 1);
  text.trim();
  return token;
}

bool isRuntimeNameChar(char character) {
  return (character >= 'a' && character <= 'z') ||
         (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '_';
}

bool isValidRuntimeLabel(const String& value) {
  if (value.length() == 0 || value.length() > MAX_VARIABLE_NAME_LENGTH) return false;
  for (size_t index = 0; index < value.length(); ++index) {
    if (!isRuntimeNameChar(value[index]) && value[index] != '-') return false;
  }
  return true;
}

int findRuntimeVariable(String name) {
  name.trim();
  name.toUpperCase();
  for (uint8_t index = 0; index < MAX_RUNTIME_VARIABLES; ++index) {
    if (runtimeVariables[index].used && runtimeVariables[index].name == name) {
      return index;
    }
  }
  return -1;
}

String runtimeVariableValue(String name) {
  int index = findRuntimeVariable(name);
  return index >= 0 ? runtimeVariables[index].value : String();
}

bool setRuntimeVariable(String name, String value) {
  name.trim();
  value.trim();
  name.toUpperCase();
  if (name.length() == 0 || name.length() > MAX_VARIABLE_NAME_LENGTH ||
      value.length() > MAX_VARIABLE_VALUE_LENGTH) {
    return false;
  }
  for (size_t character = 0; character < name.length(); ++character) {
    if (!isRuntimeNameChar(name[character])) {
      return false;
    }
  }

  int existing = findRuntimeVariable(name);
  int slot = existing;
  if (slot < 0) {
    for (uint8_t index = 0; index < MAX_RUNTIME_VARIABLES; ++index) {
      if (!runtimeVariables[index].used) {
        slot = index;
        break;
      }
    }
  }
  if (slot < 0) {
    return false;
  }
  runtimeVariables[slot].name = name;
  runtimeVariables[slot].value = value;
  runtimeVariables[slot].used = true;
  return true;
}

bool unsetRuntimeVariable(String name) {
  int index = findRuntimeVariable(name);
  if (index < 0) {
    return false;
  }
  runtimeVariables[index].name = String();
  runtimeVariables[index].value = String();
  runtimeVariables[index].used = false;
  return true;
}

void clearRuntimeVariables() {
  for (uint8_t index = 0; index < MAX_RUNTIME_VARIABLES; ++index) {
    runtimeVariables[index].name = String();
    runtimeVariables[index].value = String();
    runtimeVariables[index].used = false;
  }
}

String expandRuntimeVariables(const String& input) {
  String output;
  // Most commands contain no variables; reserve once instead of growing the
  // heap allocation for every character as this runs on each loop iteration.
  if (!output.reserve(input.length())) return String();
  for (size_t index = 0; index < input.length();) {
    if (input[index] != '$') {
      output += input[index++];
      continue;
    }

    size_t nameStart = index + 1;
    size_t nameEnd = nameStart;
    bool braced = nameStart < input.length() && input[nameStart] == '{';
    if (braced) {
      ++nameStart;
      nameEnd = input.indexOf('}', nameStart);
      if (nameEnd == (size_t)-1) {
        output += input[index++];
        continue;
      }
    } else {
      while (nameEnd < input.length() && isRuntimeNameChar(input[nameEnd])) {
        ++nameEnd;
      }
    }
    if (nameEnd == nameStart) {
      output += input[index++];
      continue;
    }

    String name = input.substring(nameStart, nameEnd);
    int variable = findRuntimeVariable(name);
    if (variable < 0) {
      output += input.substring(index, braced ? nameEnd + 1 : nameEnd);
    } else {
      output += runtimeVariables[variable].value;
    }
    index = braced ? nameEnd + 1 : nameEnd;
  }
  return output;
}

bool parseIntToken(String& text, int& value) {
  String token = takeToken(text);
  token = expandRuntimeVariables(token);
  if (token.length() == 0) {
    return false;
  }
  char* end = nullptr;
  long parsed = strtol(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  value = (int)parsed;
  return true;
}

// LEVEL e SLIDER possono ricevere un numero prodotto da un sensore (per
// esempio 21.95 °C). Manteniamo parseIntToken rigorosa per pin, tempi, colori
// e altri parametri che richiedono davvero un intero.
bool parseWidgetNumberToken(String& text, double& value) {
  String token = takeToken(text);
  token = expandRuntimeVariables(token);
  if (token.length() == 0) {
    return false;
  }
  char* end = nullptr;
  double parsed = strtod(token.c_str(), &end);
  if (end == token.c_str() || *end != '\0' || !isfinite(parsed)) {
    return false;
  }
  value = parsed;
  return true;
}

bool parsePinToken(String& text, int& pin) {
  String token = takeToken(text);
  token = expandRuntimeVariables(token);
  token.toUpperCase();
  if (token.length() == 0) {
    return false;
  }
#if defined(ARDUINO_NANO_ESP32)
  if (token == "A0") pin = A0;
  else if (token == "A1") pin = A1;
  else if (token == "A2") pin = A2;
  else if (token == "A3") pin = A3;
  else if (token == "A4") pin = A4;
  else if (token == "A5") pin = A5;
  else if (token == "A6") pin = A6;
  else if (token == "A7") pin = A7;
  else if (token == "D0") pin = D0;
  else if (token == "D1") pin = D1;
  else if (token == "D2") pin = D2;
  else if (token == "D3") pin = D3;
  else if (token == "D4") pin = D4;
  else if (token == "D5") pin = D5;
  else if (token == "D6") pin = D6;
  else if (token == "D7") pin = D7;
  else if (token == "D8") pin = D8;
  else if (token == "D9") pin = D9;
  else if (token == "D10") pin = D10;
  else if (token == "D11") pin = D11;
  else if (token == "D12") pin = D12;
  else if (token == "D13") pin = D13;
  else {
    char* end = nullptr;
    long numeric = strtol(token.c_str(), &end, 10);
    if (end == token.c_str() || *end != '\0' || numeric < 0 || numeric > 255) {
      return false;
    }
    pin = (int)numeric;
  }
  return true;
#else
  if (token[0] == 'A' && token.length() == 2 && token[1] >= '0' && token[1] <= '7') {
    pin = A0 + (token[1] - '0');
    return true;
  }
  if (token[0] == 'D' && token.length() >= 2) {
    char* end = nullptr;
    long numeric = strtol(token.c_str() + 1, &end, 10);
    if (end != token.c_str() + 1 || *end != '\0' || numeric < 0 || numeric > 255) {
      return false;
    }
    pin = (int)numeric;
    return true;
  }
  char* end = nullptr;
  long numeric = strtol(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0' || numeric < 0 || numeric > 255) {
    return false;
  }
  pin = (int)numeric;
  return true;
#endif
}

bool parseOutputValue(String token, int& value) {
  token = expandRuntimeVariables(token);
  token.trim();
  String state = token;
  state.toUpperCase();
  if (state == "HIGH" || state == "ON" || state == "TRUE") {
    value = HIGH;
    return true;
  }
  if (state == "LOW" || state == "OFF" || state == "FALSE") {
    value = LOW;
    return true;
  }
  char* end = nullptr;
  long numeric = strtol(token.c_str(), &end, 10);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  value = (int)numeric;
  return true;
}

bool parseDoubleValue(String text, double& value) {
  text = expandRuntimeVariables(text);
  text.trim();
  char* end = nullptr;
  value = strtod(text.c_str(), &end);
  return text.length() > 0 && end != text.c_str() && *end == '\0';
}

bool compareRuntimeValues(String actual, String operation, String expected) {
  actual = expandRuntimeVariables(actual);
  expected = expandRuntimeVariables(expected);
  operation.toUpperCase();
  double actualNumber = 0;
  double expectedNumber = 0;
  bool numeric = parseDoubleValue(actual, actualNumber) &&
                 parseDoubleValue(expected, expectedNumber);
  if (numeric) {
    if (operation == "==" || operation == "=") return actualNumber == expectedNumber;
    if (operation == "!=") return actualNumber != expectedNumber;
    if (operation == ">") return actualNumber > expectedNumber;
    if (operation == ">=") return actualNumber >= expectedNumber;
    if (operation == "<") return actualNumber < expectedNumber;
    if (operation == "<=") return actualNumber <= expectedNumber;
    return false;
  }
  if (operation == "==" || operation == "=") return actual == expected;
  if (operation == "!=") return actual != expected;
  return false;
}

bool addRuntimeCommand(String command, bool report = true) {
  command.trim();
  if (command.length() == 0) {
    if (report) Serial.println(F("ERR ADD empty"));
    return false;
  }
  if (command.length() > MAX_COMMAND_LENGTH) {
    if (report) Serial.println(F("ERR ADD command-too-long"));
    return false;
  }
  if (scriptLength() + command.length() + 1 > MAX_RUNTIME_BYTES) {
    if (report) Serial.println(F("ERR ADD script-too-large"));
    return false;
  }
  const uint16_t length = (uint16_t)command.length();
  memcpy(decryptedScript + runtimeScriptLength, command.c_str(), length);
  runtimeScriptLength += length;
  decryptedScript[runtimeScriptLength++] = '\n';
  decryptedScript[runtimeScriptLength] = '\0';
  if (report) {
    Serial.print(F("OK ADD "));
    Serial.println(runtimeCommandCount);
  }
  ++runtimeCommandCount;
  return true;
}

void listRuntimeCommands() {
  Serial.println(F("BEGIN_LIST"));
  for (uint16_t index = 0; index < runtimeCommandCount; ++index) {
    Serial.print(F("CMD "));
    Serial.print(index);
    Serial.print(' ');
    Serial.println(runtimeCommandAt(index));
  }
  Serial.println(F("END_LIST"));
}

void rememberRuntimeOutputPin(int pin, bool analog) {
  for (uint8_t index = 0; index < runtimeOutputPinCount; ++index) {
    if (runtimeOutputPins[index].pin == pin) {
      runtimeOutputPins[index].analog = runtimeOutputPins[index].analog || analog;
      return;
    }
  }
  if (runtimeOutputPinCount >= MAX_RUNTIME_OUTPUT_PINS) return;
  runtimeOutputPins[runtimeOutputPinCount].pin = pin;
  runtimeOutputPins[runtimeOutputPinCount].analog = analog;
  ++runtimeOutputPinCount;
}

void stopRuntimeOutputs() {
  // Il watchdog appartiene alla sequenza residente: quando il runtime viene
  // fermato non deve più riavviare la scheda qualche secondo dopo lo STOP.
  dot.watchdogStop();
  dot.setRelay(false);
  dot.stopFan();
  dot.clearPixels();
  // STOP lascia la scheda in uno stato visivo neutro: il testo mostrato dal
  // programma non deve restare sul display dopo l'arresto del runtime.
  dot.clearDisplay();
  // `clearDisplay()` aggiorna soltanto il buffer della libreria; inviamo
  // esplicitamente il buffer vuoto al pannello OLED prima di confermare STOP.
  dot.showDisplay();

  // DIGITAL_WRITE e ANALOG_WRITE possono controllare GPIO esterni oltre al
  // relay. Riportiamo a LOW solo i pin effettivamente usati dal programma e
  // poi li rilasciamo, evitando di modificare pin che il runtime non ha mai
  // toccato.
  for (uint8_t index = 0; index < runtimeOutputPinCount; ++index) {
    const int pin = runtimeOutputPins[index].pin;
    if (runtimeOutputPins[index].analog) analogWrite(pin, 0);
    digitalWrite(pin, LOW);
    pinMode(pin, INPUT);
  }
  runtimeOutputPinCount = 0;
}

void resetRuntimeControlState();

void serviceButtonEdges() {
  if (dot.isButtonAClicked()) {
    runtimeButtonAClickPending = true;
    for (uint8_t index = 0; index < runtimeTaskCount; ++index) {
      if (runtimeTasks[index].active) runtimeTasks[index].buttonAClickPending = true;
    }
  }
  if (dot.isButtonBClicked()) {
    runtimeButtonBClickPending = true;
    for (uint8_t index = 0; index < runtimeTaskCount; ++index) {
      if (runtimeTasks[index].active) runtimeTasks[index].buttonBClickPending = true;
    }
  }
}

void stopRuntime() {
  runtimeRunning = false;
  runtimeWaitUntil = 0;
  resetRuntimeControlState();
  stopRuntimeOutputs();
  // A normal STOP is also used by autonomous sketches and therefore does not
  // tear down Wi-Fi/MQTT. The Dev Studio sends NETWORK STOP explicitly when
  // the interactive programming session must release those services.
  Serial.println(F("OK STOP"));
}

// Arresto fuori banda per il Dev Studio. Il byte ETX (0x03) non è una riga
// di comando e quindi non entra nella coda delle operazioni SD/rete. Le
// operazioni lunghe chiamano questa funzione periodicamente per lasciare
// sempre la precedenza al controllo del programma.
bool pollEmergencyStop() {
  while (Serial.available() > 0) {
    int next = Serial.peek();
    if (next != SERIAL_EMERGENCY_STOP) break;
    Serial.read();
    emergencyStopRequested = true;
  }
  if (!emergencyStopRequested) return false;
  emergencyStopRequested = false;
  const bool previousOutput = bridgeSerial.isOutputEnabled();
  bridgeSerial.setOutputEnabled(true);  // ETX is an explicit control request.
  stopRuntime();
  bridgeSerial.setOutputEnabled(previousOutput);
  return true;
}

bool parseBooleanToken(String token, bool& value) {
  token.trim();
  token = expandRuntimeVariables(token);
  token.toUpperCase();
  if (token == "TRUE" || token == "1" || token == "ON") {
    value = true;
    return true;
  }
  if (token == "FALSE" || token == "0" || token == "OFF") {
    value = false;
    return true;
  }
  return false;
}

void resetRuntimeControlState() {
  runtimeIndex = 0;
  runtimeWaitUntil = 0;
  runtimeLoopDepth = 0;
  runtimeWaitingForButton = false;
  runtimeWaitButton = 0;
  runtimeWaitForClick = false;
  runtimeWaitHasTimeout = false;
  runtimeButtonWaitUntil = 0;
  runtimeButtonAClickPending = false;
  runtimeButtonBClickPending = false;
  runtimeWaitingForCloudCommand = false;
  runtimeCloudExpectedCommand = String();
  runtimeTaskCount = 0;
  runtimeTaskCursor = 0;
  runtimeCurrentTask = 0xFF;
  for (uint8_t index = 0; index < MAX_RUNTIME_TASKS; ++index) {
    runtimeTasks[index].active = false;
    runtimeTasks[index].repeating = false;
    runtimeTasks[index].startIndex = 0;
    runtimeTasks[index].endIndex = 0;
    runtimeTasks[index].index = 0;
    runtimeTasks[index].waitUntil = 0;
    runtimeTasks[index].loopDepth = 0;
    runtimeTasks[index].waitingForButton = false;
    runtimeTasks[index].waitButton = 0;
    runtimeTasks[index].waitForClick = false;
    runtimeTasks[index].waitHasTimeout = false;
    runtimeTasks[index].buttonWaitUntil = 0;
    runtimeTasks[index].buttonAClickPending = false;
    runtimeTasks[index].buttonBClickPending = false;
    runtimeTasks[index].waitingForCloudCommand = false;
    runtimeTasks[index].cloudExpectedCommand = String();
  }
}

bool initializeRuntimeTasks() {
  resetRuntimeControlState();
  bool hasTaskMarkers = false;
  for (uint16_t index = 0; index < runtimeCommandCount; ++index) {
    String command = runtimeCommandAt(index);
    if (takeToken(command).equalsIgnoreCase("TASK")) {
      hasTaskMarkers = true;
      break;
    }
  }

  if (!hasTaskMarkers) {
    runtimeTaskCount = 1;
    runtimeTasks[0].active = runtimeCommandCount > 0;
    runtimeTasks[0].repeating = false;
    runtimeTasks[0].startIndex = 0;
    runtimeTasks[0].endIndex = runtimeCommandCount;
    runtimeTasks[0].index = 0;
    runtimeRunning = runtimeTasks[0].active;
    return runtimeRunning;
  }

  bool insideTask = false;
  uint16_t taskStart = 0;
  bool repeating = false;
  for (uint16_t index = 0; index < runtimeCommandCount; ++index) {
    String command = runtimeCommandAt(index);
    String operation = takeToken(command);
    operation.toUpperCase();
    if (operation != "TASK") continue;
    String action = takeToken(command);
    action.toUpperCase();
    if (action == "BEGIN") {
      String mode = takeToken(command);
      mode.toUpperCase();
      if (insideTask || command.length() > 0 || (mode != "AUTO" && mode != "ONCE" && mode != "LOOP")) {
        Serial.println(F("ERR RUN malformed-task-begin"));
        runtimeTaskCount = 0;
        return false;
      }
      if (runtimeTaskCount >= MAX_RUNTIME_TASKS) {
        Serial.println(F("ERR RUN task-limit-exceeded"));
        runtimeTaskCount = 0;
        return false;
      }
      insideTask = true;
      repeating = mode == "LOOP";
      taskStart = index + 1;
      continue;
    }
    if (action == "END") {
      if (!insideTask || command.length() > 0) {
        Serial.println(F("ERR RUN malformed-task-end"));
        runtimeTaskCount = 0;
        return false;
      }
      RuntimeTaskState& task = runtimeTasks[runtimeTaskCount++];
      task.active = true;
      task.repeating = repeating;
      task.startIndex = taskStart;
      task.endIndex = index;
      task.index = taskStart;
      task.waitUntil = 0;
      task.loopDepth = 0;
      task.waitingForButton = false;
      task.waitButton = 0;
      task.waitForClick = false;
      task.waitHasTimeout = false;
      task.buttonWaitUntil = 0;
      task.buttonAClickPending = false;
      task.buttonBClickPending = false;
      task.waitingForCloudCommand = false;
      task.cloudExpectedCommand = String();
      insideTask = false;
      continue;
    }
    Serial.println(F("ERR RUN malformed-task-command"));
    runtimeTaskCount = 0;
    return false;
  }
  if (insideTask || runtimeTaskCount == 0) {
    Serial.println(F("ERR RUN malformed-task-sequence"));
    runtimeTaskCount = 0;
    return false;
  }
  runtimeTaskCursor = 0;
  runtimeRunning = true;
  return true;
}

// Commands between TASK sections are shared manual flows generated by Dev
// Studio. They are not scheduled on their own, but a running task may jump to
// one of their labels. Keep task bodies and TASK markers distinct so such a
// jump cannot accidentally execute a second task with the caller's context.
int8_t runtimeCommandOwner(uint16_t index) {
  if (index >= runtimeCommandCount) return -2;
  for (uint8_t taskIndex = 0; taskIndex < runtimeTaskCount; ++taskIndex) {
    const RuntimeTaskState& task = runtimeTasks[taskIndex];
    if (index >= task.startIndex && index < task.endIndex) return (int8_t)taskIndex;
    if (index == task.endIndex ||
        (task.startIndex > 0 && index == task.startIndex - 1)) return -2;
  }
  return -1;
}

uint16_t runtimeRegionEnd(uint16_t index) {
  if (runtimeCurrentTask == 0xFF || runtimeCurrentTask >= runtimeTaskCount) {
    return runtimeCommandCount;
  }
  if (runtimeCommandOwner(index) != -1) {
    return runtimeTasks[runtimeCurrentTask].endIndex;
  }
  // A REPEAT or legacy Cloud wait inside a manual flow may inspect the rest
  // of that flow, but must stop before the next TASK section.
  while (index < runtimeCommandCount && runtimeCommandOwner(index) == -1) ++index;
  return index;
}

void loadRuntimeTaskContext(uint8_t taskIndex) {
  RuntimeTaskState& task = runtimeTasks[taskIndex];
  runtimeCurrentTask = taskIndex;
  runtimeIndex = task.index;
  runtimeWaitUntil = task.waitUntil;
  memcpy(runtimeLoopStack, task.loopStack, sizeof(runtimeLoopStack));
  runtimeLoopDepth = task.loopDepth;
  runtimeWaitingForButton = task.waitingForButton;
  runtimeWaitButton = task.waitButton;
  runtimeWaitForClick = task.waitForClick;
  runtimeWaitHasTimeout = task.waitHasTimeout;
  runtimeButtonWaitUntil = task.buttonWaitUntil;
  runtimeWaitingForCloudCommand = task.waitingForCloudCommand;
  runtimeCloudExpectedCommand = task.cloudExpectedCommand;
}

void saveRuntimeTaskContext(uint8_t taskIndex) {
  RuntimeTaskState& task = runtimeTasks[taskIndex];
  task.index = runtimeIndex;
  task.waitUntil = runtimeWaitUntil;
  memcpy(task.loopStack, runtimeLoopStack, sizeof(runtimeLoopStack));
  task.loopDepth = runtimeLoopDepth;
  task.waitingForButton = runtimeWaitingForButton;
  task.waitButton = runtimeWaitButton;
  task.waitForClick = runtimeWaitForClick;
  task.waitHasTimeout = runtimeWaitHasTimeout;
  task.buttonWaitUntil = runtimeButtonWaitUntil;
  task.waitingForCloudCommand = runtimeWaitingForCloudCommand;
  task.cloudExpectedCommand = runtimeCloudExpectedCommand;
}

void requestDeviceReboot() {
  runtimeRunning = false;
  runtimeWaitUntil = 0;
  resetRuntimeControlState();
  stopRuntimeOutputs();
  rebootRequested = true;
}

void serviceDeviceReboot() {
  if (!rebootRequested) return;
  rebootRequested = false;
  const bool previousOutput = bridgeSerial.isOutputEnabled();
  if (rebootRequestedFromSerial) bridgeSerial.setOutputEnabled(true);
  rebootRequestedFromSerial = false;
  if (!dot.rebootSupported()) {
    Serial.println(F("ERR REBOOT unsupported-on-board"));
    bridgeSerial.setOutputEnabled(previousOutput);
    return;
  }
  Serial.println(F("OK REBOOT"));
  Serial.flush();
  delay(50);
  dot.reboot();
  bridgeSerial.setOutputEnabled(previousOutput);
}

bool parseButtonCondition(String& text, uint8_t& button, bool& click) {
  String buttonToken = takeToken(text);
  String stateToken = takeToken(text);
  buttonToken.toUpperCase();
  stateToken.toUpperCase();
  if (buttonToken == "A") {
    button = 0;
  } else if (buttonToken == "B") {
    button = 1;
  } else {
    return false;
  }
  if (stateToken == "PRESSED") {
    click = false;
  } else if (stateToken == "CLICKED") {
    click = true;
  } else {
    return false;
  }
  return true;
}

bool buttonCondition(uint8_t button, bool click) {
  if (button == 0) {
    if (click) {
      if (runtimeCurrentTask != 0xFF && runtimeCurrentTask < runtimeTaskCount) {
        const bool result = runtimeTasks[runtimeCurrentTask].buttonAClickPending;
        runtimeTasks[runtimeCurrentTask].buttonAClickPending = false;
        return result;
      }
      const bool result = runtimeButtonAClickPending;
      runtimeButtonAClickPending = false;
      return result;
    }
    return dot.isButtonAPressed();
  }
  if (click) {
    if (runtimeCurrentTask != 0xFF && runtimeCurrentTask < runtimeTaskCount) {
      const bool result = runtimeTasks[runtimeCurrentTask].buttonBClickPending;
      runtimeTasks[runtimeCurrentTask].buttonBClickPending = false;
      return result;
    }
    const bool result = runtimeButtonBClickPending;
    runtimeButtonBClickPending = false;
    return result;
  }
  return dot.isButtonBPressed();
}

int findRuntimeLabel(const String& requestedLabel) {
  String target = requestedLabel;
  target.toUpperCase();
  for (uint16_t index = 0; index < runtimeCommandCount; ++index) {
    String command = runtimeCommandAt(index);
    String operation = takeToken(command);
    operation.toUpperCase();
    if (operation == "LABEL") {
      String label = takeToken(command);
      label.toUpperCase();
      if (label == target) {
        return index;
      }
    }
  }
  return -1;
}

int findMatchingEndRepeat(uint16_t repeatIndex) {
  uint8_t nesting = 0;
  const uint16_t limit = runtimeRegionEnd(repeatIndex);
  for (uint16_t index = repeatIndex; index < limit; ++index) {
    String command = runtimeCommandAt(index);
    String operation = takeToken(command);
    operation.toUpperCase();
    if (operation == "REPEAT") {
      ++nesting;
    } else if (operation == "ENDREPEAT") {
      if (nesting == 0) {
        return -1;
      }
      --nesting;
      if (nesting == 0) {
        return index;
      }
    }
  }
  return -1;
}

bool jumpToRuntimeLabel(String label) {
  int target = findRuntimeLabel(label);
  if (target < 0) {
    Serial.print(F("ERR RUNTIME label-not-found "));
    Serial.println(label);
    return false;
  }
  if (runtimeCurrentTask != 0xFF && runtimeCurrentTask < runtimeTaskCount) {
    const int8_t owner = runtimeCommandOwner((uint16_t)target);
    if (owner >= 0 && owner != (int8_t)runtimeCurrentTask) {
      // The compiler ends a manual flow with GOTO to the first continuous
      // task. That task is already scheduled independently: finish a ONCE
      // caller or restart a LOOP caller instead of running it twice.
      const RuntimeTaskState& destination = runtimeTasks[(uint8_t)owner];
      if (destination.repeating && (uint16_t)target == destination.startIndex) {
        runtimeIndex = runtimeTasks[runtimeCurrentTask].endIndex;
        runtimeLoopDepth = 0;
        return true;
      }
      Serial.println(F("ERR RUNTIME label-in-another-task"));
      return false;
    }
  }
  // A jump leaving one or more active repeat blocks unwinds those frames so a
  // later ENDREPEAT cannot accidentally operate on stale loop state. Jumps
  // that stay inside the current block preserve the loop counter.
  while (runtimeLoopDepth > 0) {
    RuntimeLoopFrame& frame = runtimeLoopStack[runtimeLoopDepth - 1];
    if (target >= frame.bodyIndex && target <= frame.endIndex) {
      break;
    }
    --runtimeLoopDepth;
  }
  runtimeIndex = (uint16_t)target;
  return true;
}

void printButtonStatus() {
  Serial.print(F("BUTTON A pressed="));
  Serial.print(dot.isButtonAPressed() ? F("true") : F("false"));
  Serial.print(F(" clicked="));
  Serial.print(dot.peekButtonAClicked() ? F("true") : F("false"));
  Serial.print(F(" B pressed="));
  Serial.print(dot.isButtonBPressed() ? F("true") : F("false"));
  Serial.print(F(" clicked="));
  Serial.println(dot.peekButtonBClicked() ? F("true") : F("false"));
}

// Compatibilità per i programmi .run creati prima del dispatcher cooperativo
// del Dev Studio. In quei programmi il primo WAIT_BUTTON poteva parcheggiare
// l'interprete prima degli eventuali CLOUD ON_COMMAND successivi. Continuiamo
// a sondare gli ascoltatori Cloud già presenti nella sequenza, senza cambiare
// la semantica dei WAIT_BUTTON nuovi (che arrivano con GOTO e non entrano mai
// nello stato di attesa bloccante).
void clearRuntimeCloudWait() {
  runtimeWaitingForCloudCommand = false;
  runtimeCloudExpectedCommand = String();
}

bool routeCloudCommandWhileWaiting(String& expectedCommand, bool waitingForCloud) {
  if (!runtimeRunning || runtimeCommandCount == 0) return false;

  if (waitingForCloud && expectedCommand.length() > 0 &&
      dot.onCommand(expectedCommand.c_str())) {
    clearRuntimeCloudWait();
    Serial.print(F("INFO CLOUD ON_COMMAND triggered command="));
    Serial.println(expectedCommand);
    return true;
  }

  // Cerca solo istruzioni CLOUD ON_COMMAND ancora da eseguire. Il comando
  // viene consumato qui e l'indice viene portato al primo comando del ramo
  // successivo; in questo modo un vecchio listener non viene eseguito una
  // seconda volta da executeRuntimeCommand().
  const uint16_t scanLimit = runtimeRegionEnd(runtimeIndex);
  for (uint16_t index = runtimeIndex; index < scanLimit; ++index) {
    String candidate = runtimeCommandAt(index);
    String operation = takeToken(candidate);
    operation.toUpperCase();
    if (operation != "CLOUD") continue;
    String cloudOperation = takeToken(candidate);
    cloudOperation.toUpperCase();
    if (cloudOperation != "ON_COMMAND") continue;

    String expected = takeToken(candidate);
    String key = takeToken(candidate);
    if (expected.length() == 0) continue;

    String jump;
    String optional = takeToken(candidate);
    if (key.equalsIgnoreCase("GOTO")) {
      // Forma breve: CLOUD ON_COMMAND <comando> GOTO <label>.
      jump = optional;
      key = String();
    } else if (optional.equalsIgnoreCase("GOTO")) {
      // Forma compatibile: CLOUD ON_COMMAND <comando> content GOTO <label>.
      jump = takeToken(candidate);
    }
    if (key.length() > 0 && !key.equalsIgnoreCase("content")) continue;
    if (candidate.length() > 0) continue;
    if (!dot.onCommand(expected.c_str())) continue;

    runtimeWaitingForButton = false;
    runtimeWaitHasTimeout = false;
    runtimeButtonWaitUntil = 0;
    runtimeButtonAClickPending = false;
    runtimeButtonBClickPending = false;
    clearRuntimeCloudWait();
    if (jump.length() > 0) {
      Serial.print(F("INFO CLOUD ON_COMMAND routed command="));
      Serial.println(expected);
      return jumpToRuntimeLabel(jump);
    }
    runtimeIndex = (uint16_t)(index + 1);
    Serial.print(F("INFO CLOUD ON_COMMAND routed command="));
    Serial.println(expected);
    return true;
  }
  return false;
}

const char SD_BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

bool printSdBase64(File& file) {
  uint8_t bytes[3] = {0, 0, 0};
  uint8_t lineLength = 0;
  while (file.available()) {
    if (pollEmergencyStop()) return false;
    uint8_t count = 0;
    while (count < 3 && file.available()) {
      if (pollEmergencyStop()) return false;
      bytes[count++] = (uint8_t)file.read();
    }
    uint32_t value = ((uint32_t)bytes[0] << 16) | ((uint32_t)bytes[1] << 8) | bytes[2];
    char encoded[4] = {
      SD_BASE64_ALPHABET[(value >> 18) & 0x3F],
      SD_BASE64_ALPHABET[(value >> 12) & 0x3F],
      count > 1 ? SD_BASE64_ALPHABET[(value >> 6) & 0x3F] : '=',
      count > 2 ? SD_BASE64_ALPHABET[value & 0x3F] : '='
    };
    for (uint8_t index = 0; index < 4; ++index) {
      Serial.write(encoded[index]);
      if (++lineLength >= 76) {
        Serial.println();
        lineLength = 0;
      }
    }
    bytes[0] = bytes[1] = bytes[2] = 0;
  }
  if (lineLength > 0) Serial.println();
  return true;
}

bool executeSdCommand(String rest) {
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "BEGIN") {
    if (rest.length() > 0) {
      Serial.println(F("ERR SD BEGIN unexpected-arguments"));
      return false;
    }
    sdReady = dot.beginSD();
    Serial.println(sdReady ? F("OK SD ready") : F("ERR SD unavailable"));
    return sdReady;
  }
  if (!sdReady) {
    Serial.println(F("ERR SD unavailable"));
    return false;
  }

  if (operation == "LIST") {
    String path = takeToken(rest);
    if (rest.length() > 0) {
      Serial.println(F("ERR SD LIST unexpected-arguments"));
      return false;
    }
    if (path.length() == 0) path = "/";
    File directory = dot.openFile(path.c_str(), "r");
    if (!directory) {
      Serial.println(F("ERR SD LIST open-failed"));
      return false;
    }
    if (!directory.isDirectory()) {
      directory.close();
      Serial.println(F("ERR SD LIST not-a-directory"));
      return false;
    }
    Serial.println(F("BEGIN_SD_LIST"));
    while (true) {
      if (pollEmergencyStop()) {
        directory.close();
        Serial.println(F("ERR SD list-aborted"));
        return false;
      }
      File entry = directory.openNextFile();
      if (!entry) {
        break;
      }
      Serial.print(F("FILE "));
      Serial.print(entry.name());
      if (entry.isDirectory()) {
        Serial.println(F(" DIR"));
      } else {
        Serial.print(F(" FILE size="));
        Serial.println(entry.size());
      }
      entry.close();
    }
    directory.close();
    Serial.println(F("END_SD_LIST"));
    return true;
  }

  if (operation == "EXISTS") {
    String path = takeToken(rest);
    if (rest.length() > 0) {
      Serial.println(F("ERR SD EXISTS unexpected-arguments"));
      return false;
    }
    bool exists = path.length() > 0 && dot.fileExists(path.c_str());
    Serial.println(exists ? F("SD EXISTS true") : F("SD EXISTS false"));
    return path.length() > 0;
  }

  if (operation == "MKDIR") {
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR SD MKDIR expected-path"));
      return false;
    }
    if (path == "/" || dot.fileExists(path.c_str())) {
      Serial.println(F("ERR SD MKDIR already-exists"));
      return false;
    }
    bool success = dot.makeDirectory(path);
    Serial.println(success ? F("OK SD MKDIR") : F("ERR SD mkdir-failed"));
    return success;
  }

  if (operation == "TOUCH") {
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR SD TOUCH expected-path"));
      return false;
    }
    if (path == "/" || dot.fileExists(path.c_str())) {
      Serial.println(F("ERR SD TOUCH already-exists"));
      return false;
    }
    bool success = dot.createFile(path);
    Serial.println(success ? F("OK SD TOUCH") : F("ERR SD touch-failed"));
    return success;
  }

  if (operation == "REMOVE") {
    String path = takeToken(rest);
    if (rest.length() > 0) {
      Serial.println(F("ERR SD REMOVE unexpected-arguments"));
      return false;
    }
    if (path.length() == 0 || !dot.fileExists(path.c_str())) {
      Serial.println(F("ERR SD file-not-found"));
      return false;
    }
    dot.removeFile(path.c_str());
    if (dot.fileExists(path.c_str())) {
      Serial.println(F("ERR SD remove-failed"));
      return false;
    }
    Serial.println(F("OK SD REMOVE"));
    return true;
  }

  if (operation == "WRITE" || operation == "WRITE_LINE" ||
      operation == "APPEND" || operation == "APPEND_LINE") {
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() == 0) {
      Serial.println(F("ERR SD expected-path-and-text"));
      return false;
    }
    bool replace = operation == "WRITE" || operation == "WRITE_LINE";
    bool line = operation == "WRITE_LINE" || operation == "APPEND_LINE";
    String text = rest;
    if (line) text += '\n';
    bool success = replace ? dot.writeFile(path, text) : dot.appendFile(path, text);
    Serial.println(success ? F("OK SD WRITE") : F("ERR SD write-failed"));
    return success;
  }

  if (operation == "OPEN") {
    String path = takeToken(rest);
    String mode = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR SD expected-path"));
      return false;
    }
    if (mode.length() == 0) mode = "r";
    mode.toLowerCase();
    if (mode != "r" && mode != "w" && mode != "a") {
      Serial.println(F("ERR SD OPEN invalid-mode"));
      return false;
    }
    File file = dot.openFile(path.c_str(), mode.c_str());
    bool success = (bool)file;
    if (file) file.close();
    Serial.println(success ? F("OK SD OPEN") : F("ERR SD open-failed"));
    return success;
  }

  if (operation == "READ") {
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR SD expected-path"));
      return false;
    }
    File file = dot.openFile(path.c_str(), "r");
    if (!file) {
      Serial.println(F("ERR SD open-failed"));
      return false;
    }
    Serial.println(F("BEGIN_SD_READ"));
    bool aborted = false;
    while (file.available()) {
      if (pollEmergencyStop()) {
        aborted = true;
        break;
      }
      Serial.write((uint8_t)file.read());
    }
    file.close();
    if (aborted) {
      Serial.println(F("ERR SD read-aborted"));
      return false;
    }
    Serial.println();
    Serial.println(F("END_SD_READ"));
    return true;
  }

  if (operation == "READ_B64") {
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR SD expected-path"));
      return false;
    }
    File file = dot.openFile(path.c_str(), "r");
    if (!file) {
      Serial.println(F("ERR SD open-failed"));
      return false;
    }
    Serial.println(F("BEGIN_SD_READ_B64"));
    const bool completed = printSdBase64(file);
    file.close();
    if (!completed) {
      Serial.println(F("ERR SD read-aborted"));
      return false;
    }
    Serial.println(F("END_SD_READ_B64"));
    return true;
  }

  Serial.println(F("ERR SD unknown-operation"));
  return false;
}

bool executeColorCommand(String rest) {
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "HEX_TO_RGB") {
    String hex = takeToken(rest);
    if (hex.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR COLOR expected-hex"));
      return false;
    }
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    String normalizedHex = hex;
    if (normalizedHex.startsWith("#")) normalizedHex.remove(0, 1);
    if (normalizedHex.length() != 6) {
      Serial.println(F("ERR COLOR expected-hex"));
      return false;
    }
    for (size_t index = 0; index < normalizedHex.length(); ++index) {
      char digit = normalizedHex[index];
      if (!((digit >= '0' && digit <= '9') ||
            (digit >= 'A' && digit <= 'F') ||
            (digit >= 'a' && digit <= 'f'))) {
        Serial.println(F("ERR COLOR expected-hex"));
        return false;
      }
    }
    MyDot::hexToRGB(hex, red, green, blue);
    Serial.print(F("COLOR RGB "));
    Serial.print(red);
    Serial.print(' ');
    Serial.print(green);
    Serial.print(' ');
    Serial.println(blue);
    return true;
  }

  if (operation == "RGB_TO_HEX") {
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!parseIntToken(rest, red) || !parseIntToken(rest, green) ||
        !parseIntToken(rest, blue) || red < 0 || red > 255 ||
        green < 0 || green > 255 || blue < 0 || blue > 255 || rest.length() > 0) {
      Serial.println(F("ERR COLOR expected-r-g-b-0..255"));
      return false;
    }
    Serial.print(F("COLOR HEX "));
    Serial.println(MyDot::rgbToHex((uint8_t)red, (uint8_t)green, (uint8_t)blue));
    return true;
  }

  Serial.println(F("ERR COLOR unknown-operation"));
  return false;
}

bool executeDisplayCommand(String rest) {
  String original = rest;
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "PRESENT") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY PRESENT unexpected-arguments"));
      return false;
    }
    Serial.println(dot.isDisplayPresent() ? F("DISPLAY present=true") : F("DISPLAY present=false"));
    return true;
  }
  if (operation == "LOGO") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY LOGO unexpected-arguments"));
      return false;
    }
    dot.drawLogo();
    return true;
  }
  if (operation == "SENSOR") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY SENSOR unexpected-arguments"));
      return false;
    }
    dot.updateSensorDisplay();
    return true;
  }
  if (operation == "CLEAR") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY CLEAR unexpected-arguments"));
      return false;
    }
    dot.clearDisplay();
    dot.showDisplay();
    return true;
  }
  if (operation == "SHOW") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY SHOW unexpected-arguments"));
      return false;
    }
    dot.showDisplay();
    return true;
  }
  if (operation == "RAW_SHOW") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY RAW_SHOW unexpected-arguments"));
      return false;
    }
    // Expose the raw Adafruit_SSD1306 reference for editor protocols that
    // need to issue a native display() call directly.
    dot.getDisplay().display();
    return true;
  }
  if (operation == "CURSOR") {
    int x = 0;
    int y = 0;
    if (!parseIntToken(rest, x) || !parseIntToken(rest, y) || rest.length() > 0) {
      Serial.println(F("ERR DISPLAY CURSOR expected-x-y"));
      return false;
    }
    dot.setCursor((int16_t)x, (int16_t)y);
    return true;
  }
  if (operation == "SIZE") {
    int size = 0;
    if (!parseIntToken(rest, size) || size < 1 || size > 4 || rest.length() > 0) {
      Serial.println(F("ERR DISPLAY SIZE expected-1..4"));
      return false;
    }
    dot.setTextSize((uint8_t)size);
    return true;
  }
  if (operation == "COLOR") {
    int color = 0;
    if (!parseIntToken(rest, color) || color < 0 || color > 65535 || rest.length() > 0) {
      Serial.println(F("ERR DISPLAY COLOR expected-0..65535"));
      return false;
    }
    dot.setTextColor((uint16_t)color);
    return true;
  }
  if (operation == "TEXT") {
    if (rest.length() == 0) {
      Serial.println(F("ERR DISPLAY TEXT missing-text"));
      return false;
    }
    dot.displayPrint(rest);
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY TEXT"));
    return true;
  }
  if (operation == "TEXT_AT") {
    int x = 0;
    int y = 0;
    int size = 1;
    if (!parseIntToken(rest, x) || !parseIntToken(rest, y) ||
        !parseIntToken(rest, size) || rest.length() == 0) {
      Serial.println(F("ERR DISPLAY TEXT_AT expected-x-y-size-text"));
      return false;
    }
    dot.displayPrint(rest, x, y, (uint8_t)size);
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY TEXT_AT"));
    return true;
  }
  if (operation == "PRINT") {
    dot.print(rest);
    dot.showDisplay();
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY PRINT"));
    return true;
  }
  if (operation == "PRINTLN") {
    dot.println(rest);
    dot.showDisplay();
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY PRINTLN"));
    return true;
  }
  if (operation == "NEWLINE") {
    if (rest.length() > 0) {
      Serial.println(F("ERR DISPLAY NEWLINE unexpected-arguments"));
      return false;
    }
    dot.println();
    dot.showDisplay();
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY NEWLINE"));
    return true;
  }

  // Keep the compact DISPLAY <text> form in addition to the structured
  // DISPLAY TEXT/TEXT_AT operations used by the editor protocol.
  if (original.length() > 0) {
    dot.displayPrint(original);
    if (!dot.isDisplayPresent()) Serial.println(F("WARN DISPLAY unavailable"));
    Serial.println(F("OK DISPLAY TEXT"));
    return true;
  }
  Serial.println(F("ERR DISPLAY unknown-operation"));
  return false;
}

bool executeCloudCommand(String rest, bool allowDelay) {
#if !MYDOT_HAS_CLOUD
  (void)rest;
  (void)allowDelay;
  Serial.println(F("ERR CLOUD unavailable-on-board"));
  return false;
#else
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "BEGIN") {
    String deviceId = takeToken(rest);
    String token = takeToken(rest);
    if (deviceId.length() == 0 || token.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR CLOUD BEGIN expected-deviceId-token"));
      return false;
    }
    dot.beginCloud(deviceId.c_str(), token.c_str());
    Serial.println(F("OK CLOUD BEGIN"));
    return true;
  }
  if (operation == "SEND") {
    if (rest.length() > 0) {
      Serial.println(F("ERR CLOUD SEND unexpected-arguments"));
      return false;
    }
    bool success = dot.sendCloud();
    Serial.println(success ? F("OK CLOUD SEND") : F("ERR CLOUD SEND"));
    if (!success) runtimeTransientFailure = true;
    return success;
  }
  if (operation == "STATUS") {
    if (rest.length() > 0) {
      Serial.println(F("ERR CLOUD STATUS unexpected-arguments"));
      return false;
    }
    Serial.println(dot.isCloudConnected() ? F("CLOUD connected=true") : F("CLOUD connected=false"));
    return true;
  }
  if (operation == "BUFFER") {
    int size = 0;
    if (!parseIntToken(rest, size) || size < 128 || size > 65535 || rest.length() > 0) {
      Serial.println(F("ERR CLOUD BUFFER expected-128..65535"));
      return false;
    }
    dot.setCloudBufferSize((uint16_t)size);
    Serial.println(F("OK CLOUD BUFFER"));
    return true;
  }
  if (operation == "SYNC") {
    int interval = 0;
    if (!parseIntToken(rest, interval) || interval < 1 || rest.length() > 0) {
      Serial.println(F("ERR CLOUD SYNC expected-positive-ms"));
      return false;
    }
    dot.setCloudSync((unsigned long)interval, publishBridgeCloudSync);
    Serial.println(F("OK CLOUD SYNC"));
    return true;
  }
  if (operation == "WRITE_TEXT") {
    String key = takeToken(rest);
    if (key.length() == 0 || rest.length() == 0) {
      Serial.println(F("ERR CLOUD WRITE_TEXT expected-key-value"));
      return false;
    }
    dot.writeKeyWord(key.c_str(), rest);
    Serial.println(F("OK CLOUD WRITE_TEXT"));
    return true;
  }
  if (operation == "WRITE_INT") {
    String key = takeToken(rest);
    int value = 0;
    if (key.length() == 0 || !parseIntToken(rest, value) || rest.length() > 0) {
      Serial.println(F("ERR CLOUD WRITE_INT expected-key-value"));
      return false;
    }
    dot.writeKeyWord(key.c_str(), value);
    Serial.println(F("OK CLOUD WRITE_INT"));
    return true;
  }
  if (operation == "WRITE_FLOAT" || operation == "WRITE_DOUBLE") {
    String key = takeToken(rest);
    String value = takeToken(rest);
    double number = 0;
    if (key.length() == 0 || value.length() == 0 || rest.length() > 0 ||
        !parseDoubleValue(value, number)) {
      Serial.println(F("ERR CLOUD WRITE_NUMBER expected-key-value"));
      return false;
    }
    if (operation == "WRITE_FLOAT") {
      dot.writeKeyWord(key.c_str(), (float)number);
    } else {
      dot.writeKeyWord(key.c_str(), number);
    }
    Serial.println(F("OK CLOUD WRITE_NUMBER"));
    return true;
  }
  if (operation == "WRITE_BOOL") {
    String key = takeToken(rest);
    bool value = false;
    if (key.length() == 0 || !parseBooleanToken(takeToken(rest), value) || rest.length() > 0) {
      Serial.println(F("ERR CLOUD WRITE_BOOL expected-key-true-false"));
      return false;
    }
    dot.writeKeyWord(key.c_str(), value);
    Serial.println(F("OK CLOUD WRITE_BOOL"));
    return true;
  }
  if (operation == "ON_COMMAND") {
    String expected = takeToken(rest);
    String key = takeToken(rest);
    bool nonBlocking = false;
    String jumpLabel;
    // In a continuous flow the editor can append `GOTO <label>` to an event
    // source. In that form the command is a cooperative poll: a non-matching
    // packet simply lets the interpreter inspect the next source instead of
    // parking the whole runtime on the first listener. The legacy optional
    // key form remains accepted for direct EXEC/RUN commands.
    if (key.equalsIgnoreCase("GOTO")) {
      nonBlocking = true;
      jumpLabel = takeToken(rest);
      key = String();
    } else if (key.length() > 0 && rest.length() > 0) {
      // Accept the legacy explicit `content` key together with the new
      // non-blocking continuation form: CLOUD ON_COMMAND CMD content GOTO L.
      String optionalGoto = takeToken(rest);
      if (optionalGoto.equalsIgnoreCase("GOTO")) {
        nonBlocking = true;
        jumpLabel = takeToken(rest);
      }
    }
    key.trim();
    key.toLowerCase();
    // The Bridge receives widget commands in the standard `content` field.
    // Keep the optional key accepted for backwards compatibility, but the
    // runtime wait below intentionally uses the library default when no key
    // is supplied by the generated flow.
    if (expected.length() == 0 || (nonBlocking && !isValidRuntimeLabel(jumpLabel)) || rest.length() > 0) {
      Serial.println(F("ERR CLOUD ON_COMMAND expected-command"));
      return false;
    }
    if (nonBlocking) {
      if (dot.onCommand(expected.c_str())) {
        Serial.print(F("INFO CLOUD ON_COMMAND routed command="));
        Serial.println(expected);
        return jumpToRuntimeLabel(jumpLabel);
      }
      return true;
    }
    if (allowDelay && key.length() > 0 && key != "content") {
      Serial.println(F("ERR RUNTIME CLOUD ON_COMMAND only-content-supported"));
      return false;
    }
    if (allowDelay) {
      // A runtime flow treats ON_COMMAND as a cooperative wait. The current
      // instruction has already been advanced by serviceRuntime(), so when
      // no command is available we park the executor and resume at the next
      // command only after the expected content arrives.
      if (dot.onCommand(expected.c_str())) {
        Serial.print(F("INFO CLOUD ON_COMMAND triggered command="));
        Serial.println(expected);
        return true;
      }
      runtimeWaitingForCloudCommand = true;
      runtimeCloudExpectedCommand = expected;
      Serial.print(F("INFO CLOUD ON_COMMAND waiting command="));
      Serial.println(expected);
      return true;
    }
    bool received = key.length() == 0
                      ? dot.onCommand(expected.c_str())
                      : dot.onCommand(expected.c_str(), key.c_str());
    Serial.println(received ? F("CLOUD COMMAND true") : F("CLOUD COMMAND false"));
    return true;
  }
  if (operation == "READ") {
    String key = takeToken(rest);
    if (key.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR CLOUD READ expected-key"));
      return false;
    }
    Serial.print(F("CLOUD VALUE "));
    Serial.print(key);
    Serial.print('=');
    Serial.println(dot.readKeyWord<String>(key.c_str()));
    return true;
  }
  if (operation == "READ_ONCE") {
    String key = takeToken(rest);
    String value;
    if (key.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR CLOUD READ_ONCE expected-key"));
      return false;
    }
    if (dot.readKeyWordOnce<String>(key.c_str(), value)) {
      Serial.print(F("CLOUD VALUE_ONCE "));
      Serial.print(key);
      Serial.print('=');
      Serial.println(value);
    } else {
      Serial.println(F("CLOUD VALUE_ONCE empty"));
    }
    return true;
  }

  Serial.println(F("ERR CLOUD unknown-operation"));
  return false;
#endif
}

bool executeWidgetCommand(String rest) {
  String operation = takeToken(rest);
  operation.toUpperCase();
  String type = takeToken(rest);
  type.toUpperCase();
  String key = takeToken(rest);
  if (type.length() == 0 || key.length() == 0) {
    Serial.println(F("ERR WIDGET expected-operation-type-key"));
    return false;
  }

  if (operation == "READ") {
    if (rest.length() > 0) {
      Serial.println(F("ERR WIDGET READ unexpected-arguments"));
      return false;
    }
    Serial.print(F("WIDGET VALUE "));
    Serial.print(key);
    Serial.print('=');
    if (type == "COLOR") {
      ColorWheel widget(key.c_str(), dot);
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      widget.getRGB(red, green, blue);
      Serial.print(red);
      Serial.print(' ');
      Serial.print(green);
      Serial.print(' ');
      Serial.println(blue);
    } else if (type == "SLIDER") {
      Slider widget(key.c_str(), dot);
      Serial.println(widget.read());
    } else if (type == "LEVEL") {
      Level widget(key.c_str(), dot);
      Serial.println(widget.read());
    } else if (type == "SWITCH") {
      Switch widget(key.c_str(), dot);
      Serial.println(widget.read() ? F("true") : F("false"));
    } else if (type == "PUSHBUTTON") {
      Pushbutton widget(key.c_str(), dot);
      Serial.println(widget.read() ? F("true") : F("false"));
    } else if (type == "LED") {
      Led widget(key.c_str(), dot);
      Serial.println(widget.read() ? F("true") : F("false"));
    } else if (type == "PHOTO") {
      Photo widget(key.c_str(), dot);
      Serial.println(widget.read());
    } else if (type == "MAP") {
      Map widget(key.c_str(), dot);
      Serial.println(widget.read());
    } else if (type == "RAW" || type == "TEXT") {
      CloudWidget<String> widget(key.c_str(), dot);
      Serial.println(widget.read());
    } else {
      Serial.println(F("unsupported-type"));
      return false;
    }
    return true;
  }

  if (operation != "WRITE") {
    Serial.println(F("ERR WIDGET expected-READ-or-WRITE"));
    return false;
  }

  if (type == "COLOR") {
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!parseIntToken(rest, red) || !parseIntToken(rest, green) ||
        !parseIntToken(rest, blue) || rest.length() > 0) {
      Serial.println(F("ERR WIDGET COLOR expected-r-g-b"));
      return false;
    }
    ColorWheel widget(key.c_str(), dot);
    widget.write((uint8_t)constrain(red, 0, 255),
                 (uint8_t)constrain(green, 0, 255),
                 (uint8_t)constrain(blue, 0, 255));
  } else if (type == "MAP") {
    String first = takeToken(rest);
    String firstOperation = first;
    firstOperation.toUpperCase();
    Map widget(key.c_str(), dot);
    if (firstOperation == "TEXT") {
      if (rest.length() == 0) {
        Serial.println(F("ERR WIDGET MAP TEXT expected-value"));
        return false;
      }
      widget.write(rest);
    } else {
      String longitude = takeToken(rest);
      double latitudeValue = 0;
      double longitudeValue = 0;
      if (first.length() == 0 || longitude.length() == 0 || rest.length() > 0 ||
          !parseDoubleValue(first, latitudeValue) ||
          !parseDoubleValue(longitude, longitudeValue) ||
          latitudeValue < -90 || latitudeValue > 90 ||
          longitudeValue < -180 || longitudeValue > 180) {
        Serial.println(F("ERR WIDGET MAP expected-latitude-longitude"));
        return false;
      }
      widget.write(latitudeValue, longitudeValue);
    }
  } else if (type == "LEVEL" || type == "SLIDER") {
    double value = 0;
    if (!parseWidgetNumberToken(rest, value) || rest.length() > 0) {
      Serial.println(F("ERR WIDGET expected-numeric-value"));
      return false;
    }
    // The public Level/Slider helpers are intentionally integer-typed for
    // values read from the dashboard. Runtime blocks can also forward sensor
    // values such as 21.95, so publish the numeric JSON value directly here.
    dot.writeKeyWord(key.c_str(), value);
  } else if (type == "SWITCH" || type == "PUSHBUTTON" || type == "LED") {
    bool value = false;
    if (!parseBooleanToken(takeToken(rest), value) || rest.length() > 0) {
      Serial.println(F("ERR WIDGET expected-boolean-value"));
      return false;
    }
    if (type == "SWITCH") {
      Switch widget(key.c_str(), dot);
      widget.write(value);
    } else if (type == "PUSHBUTTON") {
      Pushbutton widget(key.c_str(), dot);
      widget.write(value);
    } else {
      Led widget(key.c_str(), dot);
      widget.write(value);
    }
  } else if (type == "PHOTO") {
    if (rest.length() == 0) {
      Serial.println(F("ERR WIDGET PHOTO expected-value"));
      return false;
    }
    Photo widget(key.c_str(), dot);
    widget.write(rest);
  } else if (type == "RAW" || type == "TEXT") {
    if (rest.length() == 0) {
      Serial.println(F("ERR WIDGET RAW expected-value"));
      return false;
    }
    CloudWidget<String> widget(key.c_str(), dot);
    widget.write(rest);
  } else {
    Serial.println(F("ERR WIDGET unknown-type"));
    return false;
  }

  // A visual "send to My Microeden" block must publish its value, not only
  // add it to the local JSON buffer. A failed publish leaves the payload in
  // MyDot for the next write or CLOUD SEND after MQTT reconnects.
#if defined(ARDUINO_ARCH_ESP32)
  if (runtimeCurrentTask != 0xFF && runtimeRunning) {
    markBridgeStage(BRIDGE_STAGE_WIDGET_PUBLISH, runtimeIndex - 1);
  }
#endif
  const bool published = dot.sendCloud();
#if defined(ARDUINO_ARCH_ESP32)
  if (runtimeCurrentTask != 0xFF && runtimeRunning) {
    markBridgeStage(BRIDGE_STAGE_COMMAND, runtimeIndex - 1);
  }
#endif
  Serial.println(F("OK WIDGET WRITE"));
  if (!published) Serial.println(F("WARN CLOUD SEND deferred"));
  return true;
}

bool parseI2cNumberToken(String& text, long& value) {
  String token = takeToken(text);
  token = expandRuntimeVariables(token);
  if (token.length() == 0) {
    return false;
  }
  char* end = nullptr;
  // Treat values as decimal unless they explicitly use the 0x hexadecimal
  // prefix. This avoids interpreting a decimal token such as "010" as octal.
  int base = (token.startsWith("0x") || token.startsWith("0X")) ? 16 : 10;
  long parsed = strtol(token.c_str(), &end, base);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  value = parsed;
  return true;
}

bool parseI2cAddressToken(String& text, uint8_t& address) {
  long parsed = 0;
  if (!parseI2cNumberToken(text, parsed) || parsed < 0x03 || parsed > 0x77) {
    return false;
  }
  address = (uint8_t)parsed;
  return true;
}

bool parseI2cByteToken(String& text, uint8_t& value) {
  long parsed = 0;
  if (!parseI2cNumberToken(text, parsed) || parsed < 0 || parsed > 0xFF) {
    return false;
  }
  value = (uint8_t)parsed;
  return true;
}

void printI2cAddress(uint8_t address) {
  Serial.print(F("0x"));
  if (address < 0x10) Serial.print('0');
  Serial.print(address, HEX);
}

void printI2cByte(uint8_t value) {
  if (value < 0x10) Serial.print('0');
  Serial.print(value, HEX);
}

bool validateI2cVariablePrefix(const String& prefix, uint8_t readLength) {
  if (prefix.length() == 0) {
    return true;
  }
  for (size_t index = 0; index < prefix.length(); ++index) {
    if (!isRuntimeNameChar(prefix[index])) return false;
  }
  // The byte index is appended to the prefix (0..31). Keep the resulting
  // variable name within the interpreter's normal 16-character limit.
  uint8_t suffixLength = readLength > 9 ? 2 : 1;
  if (prefix.length() + suffixLength > MAX_VARIABLE_NAME_LENGTH) {
    return false;
  }
  for (size_t index = 0; index < prefix.length(); ++index) {
    if (!isRuntimeNameChar(prefix[index])) {
      return false;
    }
  }
  return true;
}

bool ensureI2cBus() {
  if (!i2cReady) {
    Wire.begin();
    Wire.setClock(i2cFrequency);
#if defined(ARDUINO_ARCH_ESP32)
    // Prevent a missing device or a held SDA/SCL line from blocking the
    // runtime forever. Arduino-ESP32 exposes this timeout on TwoWire.
    Wire.setTimeOut(I2C_TRANSACTION_TIMEOUT_MS);
#elif defined(ARDUINO_ARCH_AVR)
    // Keep this bridge portable to AVR cores that expose the Wire timeout API.
    Wire.setWireTimeout((uint32_t)I2C_TRANSACTION_TIMEOUT_MS * 1000UL, true);
#endif
    i2cReady = true;
  }
  return true;
}

void recoverI2cBus() {
#if defined(ARDUINO_ARCH_ESP32)
  // The ESP32 Wire implementation can be restarted after a timed-out
  // transaction. This releases the peripheral lock before the next retry.
  Wire.end();
#endif
  i2cReady = false;
  ensureI2cBus();
}

bool readI2cBytes(uint8_t address, uint8_t length, const String& variablePrefix,
                  const uint8_t* writeBytes, uint8_t writeLength) {
  if (!ensureI2cBus()) {
    Serial.println(F("ERR I2C unavailable"));
    return false;
  }

  for (uint8_t attempt = 0; attempt <= I2C_READ_RETRIES; ++attempt) {
    if (writeLength > 0) {
      // Repeat the register/command write on every attempt. Retrying only
      // requestFrom() would read from the wrong position on many sensors.
      Wire.beginTransmission(address);
      for (uint8_t index = 0; index < writeLength; ++index) {
        Wire.write(writeBytes[index]);
      }
      uint8_t status = Wire.endTransmission(false);
      if (status != 0) {
        Serial.print(F("WARN I2C write-status attempt="));
        Serial.print(attempt + 1);
        Serial.print(F(" status="));
        Serial.println(status);
        recoverI2cBus();
        continue;
      }
    }
    uint8_t requested = Wire.requestFrom(address, length, (uint8_t)true);
    (void)requested;
    uint8_t received = 0;
    uint8_t values[MAX_I2C_TRANSFER_BYTES];
    while (Wire.available() && received < length) {
      values[received++] = (uint8_t)Wire.read();
    }

    if (received == length) {
      Serial.print(F("I2C DATA "));
      printI2cAddress(address);
      Serial.print(' ');
      for (uint8_t index = 0; index < length; ++index) {
        if (index > 0) Serial.print(' ');
        printI2cByte(values[index]);
        if (variablePrefix.length() > 0) {
          String variableName = variablePrefix + String(index);
          if (!setRuntimeVariable(variableName, String(values[index]))) {
            Serial.println();
            Serial.println(F("ERR I2C invalid-variable-prefix"));
            return false;
          }
        }
      }
      Serial.println();
      Serial.print(F("OK I2C READ length="));
      Serial.println(received);
      return true;
    }

    Serial.print(F("WARN I2C short-read attempt="));
    Serial.print(attempt + 1);
    Serial.print(F(" expected="));
    Serial.print(length);
    Serial.print(F(" received="));
    Serial.println(received);
    recoverI2cBus();
  }

  Serial.println(F("ERR I2C read-failed; runtime will continue"));
  runtimeTransientFailure = true;
  return false;
}

bool readI2cBytes(uint8_t address, uint8_t length, const String& variablePrefix) {
  return readI2cBytes(address, length, variablePrefix, nullptr, 0);
}

bool performI2cTransfer(uint8_t address, const uint8_t* writeBytes,
                        uint8_t writeLength, uint8_t readLength,
                        const String& variablePrefix) {
  if (readLength > 0) {
    return readI2cBytes(address, readLength, variablePrefix,
                        writeBytes, writeLength);
  }
  if (!ensureI2cBus()) {
    Serial.println(F("ERR I2C unavailable"));
    return false;
  }

  if (writeLength > 0) {
    Wire.beginTransmission(address);
    for (uint8_t index = 0; index < writeLength; ++index) {
      Wire.write(writeBytes[index]);
    }
    uint8_t status = Wire.endTransmission(true);
    if (status != 0) {
      Serial.print(F("ERR I2C write-status="));
      Serial.println(status);
      // A failed write can leave the peripheral state machine out of sync;
      // restart the bus before the next runtime instruction.
      recoverI2cBus();
      runtimeTransientFailure = true;
      return false;
    }
  }

  Serial.println(F("OK I2C WRITE"));
  return true;
}

bool executeI2cCommand(String rest) {
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "BEGIN") {
    if (rest.length() > 0) {
      int frequency = 0;
      if (!parseIntToken(rest, frequency) || frequency < 10000 || frequency > 1000000 || rest.length() > 0) {
        Serial.println(F("ERR I2C BEGIN expected-frequency-10000..1000000"));
        return false;
      }
      i2cFrequency = (uint32_t)frequency;
    }
    // Recreate the peripheral with the newly selected clock. On ESP32 the
    // explicit end also releases a peripheral left in an error state.
#if defined(ARDUINO_ARCH_ESP32)
    Wire.end();
#endif
    i2cReady = false;
    ensureI2cBus();
    Serial.print(F("OK I2C BEGIN frequency="));
    Serial.println(i2cFrequency);
    return true;
  }

  if (operation == "SCAN") {
    if (rest.length() > 0) {
      Serial.println(F("ERR I2C SCAN unexpected-arguments"));
      return false;
    }
    ensureI2cBus();
    uint8_t found = 0;
    Serial.println(F("I2C SCAN BEGIN"));
    for (uint8_t address = 0x03; address <= 0x77; ++address) {
      Wire.beginTransmission(address);
      if (Wire.endTransmission() == 0) {
        Serial.print(F("I2C DEVICE "));
        printI2cAddress(address);
        Serial.println();
        ++found;
      }
    }
    Serial.print(F("I2C SCAN END count="));
    Serial.println(found);
    return true;
  }

  if (operation == "PING") {
    uint8_t address = 0;
    if (!parseI2cAddressToken(rest, address) || rest.length() > 0) {
      Serial.println(F("ERR I2C PING expected-address-0x03..0x77"));
      return false;
    }
    ensureI2cBus();
    Wire.beginTransmission(address);
    uint8_t status = Wire.endTransmission();
    Serial.print(F("I2C PING "));
    printI2cAddress(address);
    Serial.println(status == 0 ? F(" ACK") : F(" NAK"));
    return status == 0;
  }

  if (operation == "WRITE") {
    uint8_t address = 0;
    if (!parseI2cAddressToken(rest, address)) {
      Serial.println(F("ERR I2C WRITE expected-address"));
      return false;
    }
    uint8_t bytes[MAX_I2C_TRANSFER_BYTES];
    uint8_t count = 0;
    while (rest.length() > 0) {
      if (count >= MAX_I2C_TRANSFER_BYTES || !parseI2cByteToken(rest, bytes[count])) {
        Serial.println(F("ERR I2C WRITE expected-1..32-bytes"));
        return false;
      }
      ++count;
    }
    if (count == 0) {
      Serial.println(F("ERR I2C WRITE expected-data"));
      return false;
    }
    return performI2cTransfer(address, bytes, count, 0, String());
  }

  if (operation == "READ") {
    uint8_t address = 0;
    int length = 0;
    if (!parseI2cAddressToken(rest, address) || !parseIntToken(rest, length) ||
        length < 1 || length > MAX_I2C_TRANSFER_BYTES) {
      Serial.println(F("ERR I2C READ expected-address-length-1..32"));
      return false;
    }
    String prefix = takeToken(rest);
    if (rest.length() > 0 || !validateI2cVariablePrefix(prefix, (uint8_t)length)) {
      Serial.println(F("ERR I2C READ invalid-prefix"));
      return false;
    }
    return readI2cBytes(address, (uint8_t)length, prefix);
  }

  if (operation == "TRANSFER") {
    uint8_t address = 0;
    int writeLength = 0;
    int readLength = 0;
    if (!parseI2cAddressToken(rest, address) || !parseIntToken(rest, writeLength) ||
        !parseIntToken(rest, readLength) || writeLength < 0 ||
        writeLength > MAX_I2C_TRANSFER_BYTES || readLength < 0 ||
        readLength > MAX_I2C_TRANSFER_BYTES || (writeLength == 0 && readLength == 0)) {
      Serial.println(F("ERR I2C TRANSFER expected-address-writeCount-readCount"));
      return false;
    }
    uint8_t bytes[MAX_I2C_TRANSFER_BYTES];
    for (int index = 0; index < writeLength; ++index) {
      if (!parseI2cByteToken(rest, bytes[index])) {
        Serial.println(F("ERR I2C TRANSFER invalid-write-byte"));
        return false;
      }
    }
    String prefix = takeToken(rest);
    if (rest.length() > 0 ||
        !validateI2cVariablePrefix(prefix, (uint8_t)readLength)) {
      Serial.println(F("ERR I2C TRANSFER invalid-prefix"));
      return false;
    }
    return performI2cTransfer(address, bytes, (uint8_t)writeLength,
                              (uint8_t)readLength, prefix);
  }

  if (operation == "READ_REG") {
    uint8_t address = 0;
    uint8_t registerAddress = 0;
    int length = 0;
    if (!parseI2cAddressToken(rest, address) ||
        !parseI2cByteToken(rest, registerAddress) ||
        !parseIntToken(rest, length) || length < 1 ||
        length > MAX_I2C_TRANSFER_BYTES) {
      Serial.println(F("ERR I2C READ_REG expected-address-register-length-1..32"));
      return false;
    }
    String prefix = takeToken(rest);
    if (rest.length() > 0 || !validateI2cVariablePrefix(prefix, (uint8_t)length)) {
      Serial.println(F("ERR I2C READ_REG invalid-prefix"));
      return false;
    }
    return performI2cTransfer(address, &registerAddress, 1,
                              (uint8_t)length, prefix);
  }

  if (operation == "WRITE_REG") {
    uint8_t address = 0;
    uint8_t registerAddress = 0;
    uint8_t value = 0;
    if (!parseI2cAddressToken(rest, address) ||
        !parseI2cByteToken(rest, registerAddress) ||
        !parseI2cByteToken(rest, value) || rest.length() > 0) {
      Serial.println(F("ERR I2C WRITE_REG expected-address-register-value"));
      return false;
    }
    uint8_t bytes[2] = {registerAddress, value};
    return performI2cTransfer(address, bytes, 2, 0, String());
  }

  Serial.println(F("ERR I2C unknown-operation"));
  return false;
}

bool parseNumericBase(String token, uint8_t& base) {
  token.trim();
  token.toUpperCase();
  if (token == "BIN" || token == "BINARY") base = 2;
  else if (token == "OCT" || token == "OCTAL") base = 8;
  else if (token == "DEC" || token == "DECIMAL") base = 10;
  else if (token == "HEX" || token == "HEXADECIMAL") base = 16;
  else {
    char* end = nullptr;
    long parsed = strtol(token.c_str(), &end, 10);
    if (token.length() == 0 || end == token.c_str() || *end != '\0' || parsed < 2 || parsed > 36) return false;
    base = (uint8_t)parsed;
  }
  return true;
}

bool parseBaseInteger(const String& input, uint8_t base, uint64_t& magnitude, bool& negative) {
  String text = input;
  text.trim();
  text.toUpperCase();
  if (text.length() == 0) return false;

  size_t position = 0;
  negative = false;
  if (text[position] == '+' || text[position] == '-') {
    negative = text[position] == '-';
    ++position;
  }
  if (position >= text.length()) return false;

  const String digits = text.substring(position);
  if ((base == 2 && digits.startsWith("0B")) ||
      (base == 8 && digits.startsWith("0O")) ||
      (base == 16 && digits.startsWith("0X"))) {
    position += 2;
  }
  if (position >= text.length()) return false;

  magnitude = 0;
  const uint64_t maximum = (uint64_t)-1;
  for (; position < text.length(); ++position) {
    const char character = text[position];
    uint8_t digit = 0;
    if (character >= '0' && character <= '9') digit = (uint8_t)(character - '0');
    else if (character >= 'A' && character <= 'Z') digit = (uint8_t)(character - 'A' + 10);
    else return false;
    if (digit >= base || magnitude > (maximum - digit) / base) return false;
    magnitude = magnitude * base + digit;
  }
  if (negative && magnitude > (1ULL << 63)) return false;
  if (magnitude == 0) negative = false;
  return true;
}

String formatBaseInteger(uint64_t magnitude, uint8_t base, bool negative) {
  const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char digits[66];
  size_t cursor = sizeof(digits) - 1;
  digits[cursor] = '\0';
  do {
    digits[--cursor] = alphabet[magnitude % base];
    magnitude /= base;
  } while (magnitude > 0);
  String result;
  if (negative) result += '-';
  result += &digits[cursor];
  return result;
}

bool executeRuntimeCommand(String command, bool allowDelay) {
  command.trim();
  if (command.length() == 0 || command.startsWith("#")) {
    return true;
  }
  command = expandRuntimeVariables(command);

  String rest = command;
  String operation = takeToken(rest);
  operation.toUpperCase();

  if (operation == "REBOOT") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME REBOOT unexpected-arguments"));
      return false;
    }
    requestDeviceReboot();
    return true;
  }

  if (operation == "WATCHDOG") {
    String watchdogOperation = takeToken(rest);
    watchdogOperation.toUpperCase();
    if (watchdogOperation == "BEGIN") {
      String timeoutToken = takeToken(rest);
      char* end = nullptr;
      unsigned long timeoutMs = timeoutToken.length() > 0
                                  ? strtoul(timeoutToken.c_str(), &end, 10)
                                  : 0;
      if (timeoutToken.length() == 0 || end == timeoutToken.c_str() || *end != '\0' ||
          timeoutMs < 100 || rest.length() > 0 || !dot.watchdogBegin(timeoutMs)) {
        Serial.println(F("ERR RUNTIME WATCHDOG BEGIN expected-ms-at-least-100"));
        return false;
      }
      return true;
    }
    if (watchdogOperation == "FEED") {
      if (rest.length() > 0 || !dot.watchdogIsEnabled()) {
        Serial.println(F("ERR RUNTIME WATCHDOG FEED not-enabled"));
        return false;
      }
      dot.watchdogFeed();
      return true;
    }
    if (watchdogOperation == "STOP") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME WATCHDOG STOP unexpected-arguments"));
        return false;
      }
      dot.watchdogStop();
      return true;
    }
    Serial.println(F("ERR RUNTIME WATCHDOG expected-BEGIN-FEED-STOP"));
    return false;
  }

  if (operation == "NOOP") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME NOOP unexpected-arguments"));
      return false;
    }
    return true;
  }

  if (operation == "SET") {
    String name = takeToken(rest);
    String value = rest;
    value.trim();
    if (name.length() == 0 || value.length() == 0) {
      Serial.println(F("ERR RUNTIME SET expected-name-and-value"));
      return false;
    }
    if (value.startsWith("=")) {
      value.remove(0, 1);
      value.trim();
      long long calculated = 0;
      if (!myDotEvaluateIntegerExpression(value, calculated)) {
        Serial.println(F("ERR RUNTIME SET invalid-integer-expression"));
        return false;
      }
      value = myDotIntegerToString(calculated);
    }
    if (!setRuntimeVariable(name, value)) {
      Serial.println(F("ERR RUNTIME SET expected-name-and-value"));
      return false;
    }
    return true;
  }

  if (operation == "BASE_CONVERT") {
    String input = takeToken(rest);
    String fromToken = takeToken(rest);
    String toToken = takeToken(rest);
    String outputVariable = takeToken(rest);
    uint8_t fromBase = 0;
    uint8_t toBase = 0;
    uint64_t magnitude = 0;
    bool negative = false;
    if (input.length() == 0 || fromToken.length() == 0 || toToken.length() == 0 ||
        outputVariable.length() == 0 || rest.length() > 0 ||
        !parseNumericBase(fromToken, fromBase) || !parseNumericBase(toToken, toBase)) {
      Serial.println(F("ERR RUNTIME BASE_CONVERT expected-value-bases-variable"));
      return false;
    }
    if (!parseBaseInteger(input, fromBase, magnitude, negative)) {
      Serial.println(F("ERR RUNTIME BASE_CONVERT invalid-integer"));
      return false;
    }
    if (!setRuntimeVariable(outputVariable, formatBaseInteger(magnitude, toBase, negative))) {
      Serial.println(F("ERR RUNTIME BASE_CONVERT invalid-variable"));
      return false;
    }
    return true;
  }

  if (operation == "GET") {
    String name = takeToken(rest);
    int variable = findRuntimeVariable(name);
    if (variable < 0) {
      Serial.println(F("ERR RUNTIME GET variable-not-found"));
      return false;
    }
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME GET unexpected-arguments"));
      return false;
    }
    Serial.print(F("VAR "));
    Serial.print(runtimeVariables[variable].name);
    Serial.print('=');
    Serial.println(runtimeVariables[variable].value);
    return true;
  }

  if (operation == "VARS") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME VARS unexpected-arguments"));
      return false;
    }
    Serial.println(F("BEGIN_VARS"));
    for (uint8_t index = 0; index < MAX_RUNTIME_VARIABLES; ++index) {
      if (runtimeVariables[index].used) {
        Serial.print(F("VAR "));
        Serial.print(runtimeVariables[index].name);
        Serial.print('=');
        Serial.println(runtimeVariables[index].value);
      }
    }
    Serial.println(F("END_VARS"));
    return true;
  }

  if (operation == "UNSET") {
    String name = takeToken(rest);
    if (rest.length() > 0 || !unsetRuntimeVariable(name)) {
      Serial.println(F("ERR RUNTIME UNSET variable-not-found"));
      return false;
    }
    return true;
  }

  if (operation == "CLEAR_VARS") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME CLEAR_VARS unexpected-arguments"));
      return false;
    }
    clearRuntimeVariables();
    return true;
  }

  if (operation == "DIGITAL_READ") {
    int pin = 0;
    String variableName;
    if (!parsePinToken(rest, pin) || (variableName = takeToken(rest)).length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME DIGITAL_READ expected-pin-variable"));
      return false;
    }
    pinMode(pin, INPUT);
    int value = digitalRead(pin);
    if (!setRuntimeVariable(variableName, String(value))) {
      Serial.println(F("ERR RUNTIME DIGITAL_READ invalid-variable"));
      return false;
    }
    Serial.print(F("VAR "));
    Serial.print(variableName);
    Serial.print('=');
    Serial.println(value);
    return true;
  }

  if (operation == "ANALOG_READ") {
    int pin = 0;
    String variableName;
    if (!parsePinToken(rest, pin) || (variableName = takeToken(rest)).length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME ANALOG_READ expected-pin-variable"));
      return false;
    }
    int value = analogRead(pin);
    if (!setRuntimeVariable(variableName, String(value))) {
      Serial.println(F("ERR RUNTIME ANALOG_READ invalid-variable"));
      return false;
    }
    Serial.print(F("VAR "));
    Serial.print(variableName);
    Serial.print('=');
    Serial.println(value);
    return true;
  }

  if (operation == "DIGITAL_WRITE") {
    int pin = 0;
    int value = LOW;
    if (!parsePinToken(rest, pin) || !parseOutputValue(takeToken(rest), value) || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME DIGITAL_WRITE expected-pin-value"));
      return false;
    }
    rememberRuntimeOutputPin(pin, false);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value == 0 ? LOW : HIGH);
    return true;
  }

  if (operation == "ANALOG_WRITE") {
    int pin = 0;
    int value = 0;
    if (!parsePinToken(rest, pin) || !parseIntToken(rest, value) ||
        value < 0 || value > 255 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME ANALOG_WRITE expected-pin-0..255"));
      return false;
    }
    rememberRuntimeOutputPin(pin, true);
    pinMode(pin, OUTPUT);
    analogWrite(pin, value);
    return true;
  }

  if (operation == "IF_VAR") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC IF_VAR use-RUN"));
      return false;
    }
    String name = takeToken(rest);
    int variable = findRuntimeVariable(name);
    String comparison = takeToken(rest);
    String expected = takeToken(rest);
    String jump = takeToken(rest);
    String label = takeToken(rest);
    jump.toUpperCase();
    if (variable < 0 || comparison.length() == 0 || expected.length() == 0 ||
        jump != "GOTO" || !isValidRuntimeLabel(label) || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME IF_VAR expected-name-op-value-GOTO-label"));
      return false;
    }
    if (compareRuntimeValues(runtimeVariables[variable].value, comparison, expected)) {
      return jumpToRuntimeLabel(label);
    }
    return true;
  }

  if (operation == "LABEL") {
    String label = takeToken(rest);
    if (!isValidRuntimeLabel(label) || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME LABEL expected-name"));
      return false;
    }
    return true;
  }

  if (operation == "GOTO") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC GOTO use-RUN"));
      return false;
    }
    String label = takeToken(rest);
    if (!isValidRuntimeLabel(label) || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME GOTO expected-label"));
      return false;
    }
    return jumpToRuntimeLabel(label);
  }

  if (operation == "IF_BUTTON") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC IF_BUTTON use-RUN"));
      return false;
    }
    uint8_t button = 0;
    bool click = false;
    String condition = rest;
    if (!parseButtonCondition(condition, button, click)) {
      Serial.println(F("ERR RUNTIME IF_BUTTON expected-A-or-B-and-PRESSED-or-CLICKED"));
      return false;
    }
    String jump = takeToken(condition);
    jump.toUpperCase();
    String label = takeToken(condition);
    if (jump != "GOTO" || !isValidRuntimeLabel(label) || condition.length() > 0) {
      Serial.println(F("ERR RUNTIME IF_BUTTON expected-GOTO-label"));
      return false;
    }
    if (buttonCondition(button, click)) {
      return jumpToRuntimeLabel(label);
    }
    return true;
  }

  if (operation == "WAIT_BUTTON") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC WAIT_BUTTON use-RUN"));
      return false;
    }
    uint8_t button = 0;
    bool click = false;
    if (!parseButtonCondition(rest, button, click)) {
      Serial.println(F("ERR RUNTIME WAIT_BUTTON expected-A-or-B-and-PRESSED-or-CLICKED"));
      return false;
    }
    int timeout = 0;
    bool nonBlocking = false;
    String jumpLabel;
    if (rest.length() > 0) {
      String optional = takeToken(rest);
      if (optional.equalsIgnoreCase("GOTO")) {
        nonBlocking = true;
        jumpLabel = takeToken(rest);
      } else {
        String timeoutText = optional;
        char* end = nullptr;
        long parsed = strtol(timeoutText.c_str(), &end, 10);
        if (end == timeoutText.c_str() || *end != '\0' || parsed < 0 || parsed > 300000) {
          Serial.println(F("ERR RUNTIME WAIT_BUTTON invalid-timeout"));
          return false;
        }
        timeout = (int)parsed;
        if (rest.length() > 0) {
          String jump = takeToken(rest);
          if (!jump.equalsIgnoreCase("GOTO")) {
            Serial.println(F("ERR RUNTIME WAIT_BUTTON invalid-timeout"));
            return false;
          }
          nonBlocking = true;
          jumpLabel = takeToken(rest);
        }
      }
    }
    if (nonBlocking && !isValidRuntimeLabel(jumpLabel)) {
      Serial.println(F("ERR RUNTIME WAIT_BUTTON invalid-timeout"));
      return false;
    }
    if (nonBlocking) {
      if (buttonCondition(button, click)) return jumpToRuntimeLabel(jumpLabel);
      return true;
    }
    if (buttonCondition(button, click)) {
      return true;
    }
    runtimeWaitingForButton = true;
    runtimeWaitButton = button;
    runtimeWaitForClick = click;
    runtimeWaitHasTimeout = timeout > 0;
    runtimeButtonWaitUntil = runtimeWaitHasTimeout
                              ? millis() + (unsigned long)timeout
                              : 0;
    Serial.print(F("INFO WAIT_BUTTON waiting button="));
    Serial.print(button == 0 ? 'A' : 'B');
    Serial.print(F(" state="));
    Serial.println(click ? F("CLICKED") : F("PRESSED"));
    return true;
  }

  if (operation == "REPEAT") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC REPEAT use-RUN"));
      return false;
    }
    int count = 0;
    if (!parseIntToken(rest, count) || count < 1 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME REPEAT expected-positive-count"));
      return false;
    }
    if (runtimeLoopDepth >= MAX_LOOP_DEPTH) {
      Serial.println(F("ERR RUNTIME REPEAT nesting-limit"));
      return false;
    }
    uint16_t repeatIndex = runtimeIndex == 0 ? 0 : runtimeIndex - 1;
    int endIndex = findMatchingEndRepeat(repeatIndex);
    if (endIndex < 0) {
      Serial.println(F("ERR RUNTIME REPEAT missing-ENDREPEAT"));
      return false;
    }
    runtimeLoopStack[runtimeLoopDepth].bodyIndex = runtimeIndex;
    runtimeLoopStack[runtimeLoopDepth].endIndex = (uint16_t)endIndex;
    runtimeLoopStack[runtimeLoopDepth].remaining = (uint32_t)count;
    ++runtimeLoopDepth;
    return true;
  }

  if (operation == "ENDREPEAT") {
    if (!allowDelay) {
      Serial.println(F("ERR EXEC ENDREPEAT use-RUN"));
      return false;
    }
    if (rest.length() > 0 || runtimeLoopDepth == 0) {
      Serial.println(F("ERR RUNTIME ENDREPEAT without-REPEAT"));
      return false;
    }
    RuntimeLoopFrame& frame = runtimeLoopStack[runtimeLoopDepth - 1];
    uint16_t endIndex = runtimeIndex == 0 ? 0 : runtimeIndex - 1;
    if (frame.endIndex != endIndex) {
      Serial.println(F("ERR RUNTIME ENDREPEAT flow-mismatch"));
      return false;
    }
    if (frame.remaining > 1) {
      --frame.remaining;
      runtimeIndex = frame.bodyIndex;
    } else {
      --runtimeLoopDepth;
    }
    return true;
  }

  if (operation == "BREAK" || operation == "CONTINUE") {
    if (!allowDelay) {
      Serial.print(F("ERR EXEC "));
      Serial.print(operation);
      Serial.println(F(" use-RUN"));
      return false;
    }
    if (rest.length() > 0 || runtimeLoopDepth == 0) {
      Serial.print(F("ERR RUNTIME "));
      Serial.print(operation);
      Serial.println(F(" outside-REPEAT"));
      return false;
    }
    RuntimeLoopFrame& frame = runtimeLoopStack[runtimeLoopDepth - 1];
    if (operation == "BREAK") {
      runtimeIndex = frame.endIndex + 1;
      --runtimeLoopDepth;
    } else {
      runtimeIndex = frame.endIndex;
    }
    return true;
  }

  if (operation == "DELAY") {
    int milliseconds = 0;
    if (!parseIntToken(rest, milliseconds) || milliseconds < 0 || milliseconds > 300000 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME DELAY invalid-ms"));
      return false;
    }
    if (!allowDelay) {
      Serial.println(F("ERR EXEC DELAY use-RUN"));
      return false;
    }
    runtimeWaitUntil = millis() + (unsigned long)milliseconds;
    return true;
  }

  if (operation == "RELAY") {
    String state = takeToken(rest);
    state.toUpperCase();
    if (state == "STATUS") {
      String variableName = takeToken(rest);
      if (variableName.length() == 0 || rest.length() > 0) {
        Serial.println(F("ERR RUNTIME RELAY STATUS expected-variable"));
        return false;
      }
      const bool relayOn = dot.isRelayOn();
      const String relayValue = relayOn ? String(F("ON")) : String(F("OFF"));
      if (!setRuntimeVariable(variableName, relayValue)) {
        Serial.println(F("ERR RUNTIME RELAY STATUS invalid-variable"));
        return false;
      }
      Serial.print(F("VAR "));
      Serial.print(variableName);
      Serial.print('=');
      Serial.println(relayValue);
      return true;
    }
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME RELAY unexpected-arguments"));
      return false;
    }
    if (state == "ON") {
      dot.setRelay(true);
    } else if (state == "OFF") {
      dot.setRelay(false);
    } else if (state == "TOGGLE") {
      dot.toggleRelay();
    } else {
      Serial.println(F("ERR RUNTIME RELAY expected-ON-OFF-TOGGLE-STATUS"));
      return false;
    }
    return true;
  }

  if (operation == "BRIGHTNESS") {
    int brightness = 0;
    if (!parseIntToken(rest, brightness) || brightness < 0 || brightness > 255 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME BRIGHTNESS expected-0..255"));
      return false;
    }
    dot.setBrightness((uint8_t)brightness);
    return true;
  }

  if (operation == "PIXELS") {
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!parseIntToken(rest, red) || !parseIntToken(rest, green) ||
        !parseIntToken(rest, blue) || red < 0 || red > 255 ||
        green < 0 || green > 255 || blue < 0 || blue > 255 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME PIXELS expected-r-g-b-0..255"));
      return false;
    }
    dot.setAllPixels((uint8_t)red, (uint8_t)green, (uint8_t)blue);
    return true;
  }

  if (operation == "PIXELS_SHOW") {
    dot.showPixels();
    return true;
  }

  if (operation == "PIXELS_RANDOM") {
    dot.setRandomPixels();
    return true;
  }

  if (operation == "PIXEL") {
    int index = 0;
    int red = 0;
    int green = 0;
    int blue = 0;
    if (!parseIntToken(rest, index) || !parseIntToken(rest, red) ||
        !parseIntToken(rest, green) || !parseIntToken(rest, blue) ||
        index < 0 || index >= NUMPIXELS || red < 0 || red > 255 ||
        green < 0 || green > 255 || blue < 0 || blue > 255 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME PIXEL expected-index-r-g-b"));
      return false;
    }
    dot.setPixel((uint16_t)index, (uint8_t)red, (uint8_t)green, (uint8_t)blue);
    return true;
  }

  if (operation == "CLEAR_PIXELS") {
    dot.clearPixels();
    return true;
  }

  if (operation == "FAN") {
    String value = takeToken(rest);
    value.toUpperCase();
    if (value == "STATUS") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME FAN STATUS unexpected-arguments"));
        return false;
      }
      Serial.print(F("FAN speed="));
      Serial.print(dot.getFanSpeed());
      Serial.print(F(" fault=0x"));
      Serial.println(dot.getFanFault(), HEX);
      return true;
    }
    if (value == "CLEAR_FAULT") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME FAN CLEAR_FAULT unexpected-arguments"));
        return false;
      }
      dot.clearFanFault();
      Serial.println(F("OK FAN CLEAR_FAULT"));
      return true;
    }
    if (value == "STOP") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME FAN STOP unexpected-arguments"));
        return false;
      }
      dot.stopFan();
      return true;
    }
    char* end = nullptr;
    long speed = strtol(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0' || speed < -100 || speed > 100 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME FAN expected--100..100-or-STOP"));
      return false;
    }
    dot.setFanSpeed((int)speed);
    return true;
  }

  if (operation == "DISPLAY") {
    return executeDisplayCommand(rest);
  }

  if (operation == "SENSORS") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME SENSORS unexpected-arguments"));
      return false;
    }
    if (!dot.readSensors()) {
      Serial.println(F("ERR RUNTIME SENSORS unavailable"));
      runtimeTransientFailure = true;
      return false;
    }
    Serial.print(F("SENSOR temperature="));
    Serial.print(dot.getTemperature());
    Serial.print(F(" pressure="));
    Serial.print(dot.getPressure());
    Serial.print(F(" humidity="));
    Serial.print(dot.getHumidity());
    Serial.print(F(" gas="));
    Serial.println(dot.getGasResistance());
    return true;
  }

  if (operation == "SENSOR_READ") {
    String field = takeToken(rest);
    String variableName = takeToken(rest);
    field.toUpperCase();
    if (field.length() == 0 || variableName.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME SENSOR_READ expected-field-variable"));
      return false;
    }
    if (!dot.readSensors()) {
      Serial.println(F("ERR RUNTIME SENSOR_READ unavailable"));
      runtimeTransientFailure = true;
      return false;
    }

    String value;
    if (field == "TEMPERATURE" || field == "TEMP") {
      value = String(dot.getTemperature(), 2);
    } else if (field == "PRESSURE") {
      value = String(dot.getPressure(), 2);
    } else if (field == "HUMIDITY" || field == "HUM") {
      value = String(dot.getHumidity(), 2);
    } else if (field == "GAS" || field == "GAS_RESISTANCE") {
      value = String(dot.getGasResistance(), 2);
    } else {
      Serial.println(F("ERR RUNTIME SENSOR_READ unknown-field"));
      return false;
    }
    if (!setRuntimeVariable(variableName, value)) {
      Serial.println(F("ERR RUNTIME SENSOR_READ invalid-variable"));
      return false;
    }
    Serial.print(F("VAR "));
    Serial.print(variableName);
    Serial.print('=');
    Serial.println(value);
    return true;
  }

  if (operation == "SENSORS_LOG") {
    if (!sdReady) {
      Serial.println(F("ERR RUNTIME SENSORS_LOG sd-unavailable"));
      return false;
    }
    String path = takeToken(rest);
    if (path.length() == 0 || rest.length() > 0) {
      Serial.println(F("ERR RUNTIME SENSORS_LOG expected-path"));
      return false;
    }
    if (!dot.readSensors()) {
      Serial.println(F("ERR RUNTIME SENSORS_LOG unavailable"));
      runtimeTransientFailure = true;
      return false;
    }

    if (!dot.fileExists(path.c_str())) {
      const String header = "epoch,time,temperature_c,pressure_hpa,humidity_percent,gas_kohm\n";
      if (!dot.writeFile(path, header)) {
        Serial.println(F("ERR RUNTIME SENSORS_LOG header-failed"));
        return false;
      }
    }

    String line = String(dot.getEpochTime()) + "," +
                  dot.getFormattedTime() + "," +
                  String(dot.getTemperature(), 2) + "," +
                  String(dot.getPressure(), 2) + "," +
                  String(dot.getHumidity(), 2) + "," +
                  String(dot.getGasResistance(), 2) + "\n";
    if (!dot.appendFile(path, line)) {
      Serial.println(F("ERR RUNTIME SENSORS_LOG write-failed"));
      return false;
    }
    Serial.print(F("OK SENSORS_LOG "));
    Serial.println(path);
    return true;
  }

  if (operation == "I2C") {
    return executeI2cCommand(rest);
  }

  if (operation == "SERIAL") {
    Serial.println(rest);
    return true;
  }

  // NETWORK STOP is deliberately separate from the normal STOP command.
  // STOP only halts the resident sequence, while this editor-only command
  // also tears down Wi-Fi/MQTT services started by that sequence. Keeping
  // the two operations separate preserves autonomous runtime behaviour.
  if (operation == "NETWORK") {
    String networkOperation = takeToken(rest);
    networkOperation.toUpperCase();
    if (networkOperation == "STOP" && rest.length() == 0) {
      dot.stopNetworkServices();
      Serial.println(F("OK NETWORK STOP"));
      return true;
    }
    Serial.println(F("ERR RUNTIME NETWORK expected-STOP"));
    return false;
  }

  if (operation == "WIFI") {
#if !MYDOT_HAS_WIFI
    (void)rest;
    Serial.println(F("ERR RUNTIME WIFI unavailable-on-board"));
    return false;
#else
    String wifiOperation = takeToken(rest);
    wifiOperation.toUpperCase();
    if (wifiOperation == "BEGIN") {
      String ssid = takeToken(rest);
      String password = takeToken(rest);
      if (ssid.length() == 0 || password.length() == 0 || rest.length() > 0) {
        Serial.println(F("ERR RUNTIME WIFI BEGIN expected-ssid-password"));
        return false;
      }
      dot.beginWiFi(ssid.c_str(), password.c_str());
      Serial.println(F("OK WIFI BEGIN"));
      return true;
    }
    if (wifiOperation == "STATUS") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME WIFI STATUS unexpected-arguments"));
        return false;
      }
      Serial.println(dot.isWiFiConnected() ? F("WIFI connected=true") : F("WIFI connected=false"));
      return true;
    }
    if (wifiOperation == "RSSI") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME WIFI RSSI unexpected-arguments"));
        return false;
      }
      Serial.print(F("WIFI RSSI "));
      Serial.println(dot.getWiFiRSSI());
      return true;
    }
    Serial.println(F("ERR RUNTIME WIFI unknown-operation"));
    return false;
#endif
  }

  if (operation == "TIME") {
#if !MYDOT_HAS_WIFI
    (void)rest;
    Serial.println(F("ERR RUNTIME TIME unavailable-on-board"));
    return false;
#else
    String timeOperation = takeToken(rest);
    timeOperation.toUpperCase();
    if (timeOperation == "EPOCH") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME TIME EPOCH unexpected-arguments"));
        return false;
      }
      Serial.print(F("TIME EPOCH "));
      Serial.println(dot.getEpochTime());
      return true;
    }
    if (timeOperation == "FORMAT") {
      int offset = 1;
      if (rest.length() > 0 && (!parseIntToken(rest, offset) || rest.length() > 0)) {
        Serial.println(F("ERR RUNTIME TIME FORMAT expected-gmt-offset"));
        return false;
      }
      Serial.print(F("TIME FORMAT "));
      Serial.println(dot.getFormattedTime(offset));
      return true;
    }
    Serial.println(F("ERR RUNTIME TIME unknown-operation"));
    return false;
#endif
  }

  if (operation == "CLOUD") {
    return executeCloudCommand(rest, allowDelay);
  }

  if (operation == "SD") {
#if !MYDOT_HAS_SD_CAPACITY
    (void)rest;
    Serial.println(F("ERR RUNTIME SD unavailable-on-board"));
    return false;
#else
    return executeSdCommand(rest);
#endif
  }

  if (operation == "BUTTON") {
    String button = takeToken(rest);
    button.toUpperCase();
    if (button == "STATUS") {
      if (rest.length() > 0) {
        Serial.println(F("ERR RUNTIME BUTTON STATUS unexpected-arguments"));
        return false;
      }
      printButtonStatus();
      return true;
    }
    // CAPS espone BUTTON_READ come comando strutturato. L'editor lo
    // trasforma in "BUTTON READ A PRESSED"; manteniamo anche la forma breve
    // "BUTTON A PRESSED" già documentata per compatibilità.
    if (button == "READ") {
      button = takeToken(rest);
      String readState = takeToken(rest);
      button.toUpperCase();
      readState.toUpperCase();
      if ((button != "A" && button != "B") ||
          (readState != "PRESSED" && readState != "CLICKED") ||
          rest.length() > 0) {
        Serial.println(F("ERR RUNTIME BUTTON READ expected-A-or-B-and-PRESSED-or-CLICKED"));
        return false;
      }
      bool clicked = readState == "CLICKED";
      bool result = button == "A"
                     ? (clicked ? buttonCondition(0, true) : dot.isButtonAPressed())
                     : (clicked ? buttonCondition(1, true) : dot.isButtonBPressed());
      Serial.print(F("BUTTON "));
      Serial.print(button);
      Serial.print(' ');
      Serial.print(readState);
      Serial.print('=');
      Serial.println(result ? F("true") : F("false"));
      return true;
    }
    if (button != "A" && button != "B") {
      Serial.println(F("ERR RUNTIME BUTTON expected-A-B-or-STATUS"));
      return false;
    }
    String state = takeToken(rest);
    state.toUpperCase();
    bool pressed = state == "PRESSED";
    bool clicked = state == "CLICKED";
    if (!pressed && !clicked) {
      Serial.println(F("ERR RUNTIME BUTTON expected-PRESSED-or-CLICKED"));
      return false;
    }
    if (rest.length() > 0) {
      Serial.println(F("ERR RUNTIME BUTTON unexpected-arguments"));
      return false;
    }
    bool result = button == "A"
                   ? (pressed ? dot.isButtonAPressed() : buttonCondition(0, true))
                   : (pressed ? dot.isButtonBPressed() : buttonCondition(1, true));
    Serial.print(F("BUTTON "));
    Serial.print(button);
    Serial.print(' ');
    Serial.print(state);
    Serial.print('=');
    Serial.println(result ? F("true") : F("false"));
    return true;
  }

  if (operation == "COLOR") {
    return executeColorCommand(rest);
  }

  if (operation == "WIDGET") {
    return executeWidgetCommand(rest);
  }

  Serial.print(F("ERR RUNTIME unknown-command "));
  Serial.println(operation);
  return false;
}

bool isExplicitSerialCommand(const String& command) {
  static const char name[] = "SERIAL";
  if (command.length() < 6) return false;
  for (uint8_t index = 0; index < 6; ++index) {
    if (toupper((unsigned char)command[index]) != name[index]) return false;
  }
  return command.length() == 6 || command[6] == ' ' || command[6] == '\t';
}

void serviceRuntime() {
  // Anche il ciclo runtime deve lasciare immediatamente la precedenza al
  // canale di controllo USB.
  if (pollEmergencyStop()) return;
  if (!runtimeRunning) {
    return;
  }

  if (runtimeTaskCount == 0 && !initializeRuntimeTasks()) {
    runtimeRunning = false;
    return;
  }

  const uint32_t now = millis();
  for (uint8_t offset = 0; offset < runtimeTaskCount; ++offset) {
    const uint8_t taskIndex = (uint8_t)((runtimeTaskCursor + offset) % runtimeTaskCount);
    RuntimeTaskState& task = runtimeTasks[taskIndex];
    if (!task.active) continue;
    loadRuntimeTaskContext(taskIndex);

    // Ogni attività conserva le proprie attese e lo stack dei cicli. Una
    // scadenza futura o un pulsante non premuto mette in pausa solo questa
    // attività; il ciclo continua a cercare un'altra attività pronta.
    if (runtimeWaitingForButton || runtimeWaitingForCloudCommand) {
      String expected = runtimeCloudExpectedCommand;
      routeCloudCommandWhileWaiting(expected, runtimeWaitingForCloudCommand);
    }
    if (runtimeWaitingForCloudCommand) {
      if (dot.onCommand(runtimeCloudExpectedCommand.c_str())) {
        runtimeWaitingForCloudCommand = false;
        runtimeCloudExpectedCommand = String();
        Serial.println(F("INFO CLOUD ON_COMMAND triggered"));
      } else {
        saveRuntimeTaskContext(taskIndex);
        continue;
      }
    }
    if (runtimeWaitingForButton) {
      if (buttonCondition(runtimeWaitButton, runtimeWaitForClick)) {
        runtimeWaitingForButton = false;
        runtimeWaitHasTimeout = false;
        runtimeButtonWaitUntil = 0;
        Serial.println(F("INFO WAIT_BUTTON triggered"));
      } else if (runtimeWaitHasTimeout &&
                 (long)(millis() - runtimeButtonWaitUntil) >= 0) {
        runtimeWaitingForButton = false;
        runtimeWaitHasTimeout = false;
        runtimeButtonWaitUntil = 0;
        Serial.println(F("INFO WAIT_BUTTON timeout"));
      } else {
        saveRuntimeTaskContext(taskIndex);
        continue;
      }
    }

    if (runtimeWaitUntil != 0 && (long)(now - runtimeWaitUntil) < 0) {
      saveRuntimeTaskContext(taskIndex);
      continue;
    }
    runtimeWaitUntil = 0;

    const int8_t commandOwner = runtimeCommandOwner(runtimeIndex);
    if (commandOwner != (int8_t)taskIndex && commandOwner != -1) {
      if (runtimeIndex != task.endIndex) {
        // Reaching another task or a TASK marker is not a valid fallthrough.
        runtimeRunning = false;
        resetRuntimeControlState();
        stopRuntimeOutputs();
        Serial.println(F("ERR RUN task-boundary"));
        return;
      }
      if (task.repeating) {
        runtimeIndex = task.startIndex;
        runtimeLoopDepth = 0;
        runtimeWaitingForButton = false;
        runtimeWaitHasTimeout = false;
        runtimeButtonWaitUntil = 0;
        runtimeWaitingForCloudCommand = false;
        runtimeCloudExpectedCommand = String();
        saveRuntimeTaskContext(taskIndex);
        continue;
      }
      task.active = false;
      saveRuntimeTaskContext(taskIndex);
      continue;
    }

    String command = runtimeCommandAt(runtimeIndex);
    ++runtimeIndex;
    runtimeTransientFailure = false;
#if defined(ARDUINO_ARCH_ESP32)
    markBridgeStage(BRIDGE_STAGE_COMMAND, runtimeIndex - 1);
#endif
    const bool previousOutput = bridgeSerial.isOutputEnabled();
    bridgeSerial.setOutputEnabled(runtimeLogsEnabled || isExplicitSerialCommand(command));
    const bool commandSucceeded = executeRuntimeCommand(command, true);
    bridgeSerial.setOutputEnabled(previousOutput);
    if (!commandSucceeded) {
      if (runtimeTransientFailure) {
        // Sensor, I2C and MQTT outages only delay the activity that issued
        // the command; other activities remain runnable on the next pass.
        runtimeWaitUntil = millis() + 250;
        saveRuntimeTaskContext(taskIndex);
        runtimeTaskCursor = (uint8_t)((taskIndex + 1) % runtimeTaskCount);
        Serial.println(F("WARN RUN temporary I/O failure; continuing"));
        return;
      }
      const uint16_t stoppedIndex = runtimeIndex == 0 ? 0 : runtimeIndex - 1;
      runtimeRunning = false;
      resetRuntimeControlState();
      stopRuntimeOutputs();
      Serial.println(F("ERR RUN stopped"));
      Serial.print(F("INFO RUN stopped-index="));
      Serial.println(stoppedIndex);
      return;
    }
    if (!runtimeRunning || runtimeTaskCount == 0) return;
    saveRuntimeTaskContext(taskIndex);
    runtimeTaskCursor = (uint8_t)((taskIndex + 1) % runtimeTaskCount);
    return;
  }

  bool anyActive = false;
  for (uint8_t index = 0; index < runtimeTaskCount; ++index) {
    if (runtimeTasks[index].active) {
      anyActive = true;
      break;
    }
  }
  if (!anyActive) {
    runtimeRunning = false;
    resetRuntimeControlState();
    // Le uscite si spengono solo quando sono terminate tutte le attività
    // una tantum; la conclusione di una catena non ferma le altre.
    stopRuntimeOutputs();
    Serial.println(F("OK COMPLETE"));
  }
}

bool saveRuntime() {
  if (!sdReady) {
    Serial.println(F("ERR SAVE sd-unavailable"));
    return false;
  }

  size_t length = scriptLength();
  if (length > MAX_RUNTIME_BYTES) {
    Serial.println(F("ERR SAVE script-too-large"));
    return false;
  }

  RuntimeFileHeader header = {};
  memcpy(header.magic, RUNTIME_MAGIC, sizeof(header.magic));
  header.version = FILE_FORMAT_VERSION;
  header.nonce = randomNonce();
  header.payloadLength = (uint32_t)length;
  header.payloadCrc = scriptCrc();

  File file = dot.openFile(RUNTIME_FILE, "w");
  if (!file) {
    Serial.println(F("ERR SAVE open-failed"));
    return false;
  }

  bool success = file.write(header.magic, sizeof(header.magic)) == sizeof(header.magic);
  if (success) {
    success = writeUint32(file, header.version);
  }
  if (success) {
    success = writeUint32(file, header.nonce);
  }
  if (success) {
    success = writeUint32(file, header.payloadLength);
  }
  if (success) {
    success = writeUint32(file, header.payloadCrc);
  }

  RuntimeCipher cipher(header.nonce);
  bool stopRequestedDuringSave = false;
  for (uint16_t index = 0; success && index < runtimeScriptLength; ++index) {
    if (pollEmergencyStop()) {
      // Non interrompere la scrittura del file .run a metà: il programma
      // residente deve restare valido. Il runtime è già stato fermato da
      // pollEmergencyStop(); completiamo il salvataggio e confermiamo poi.
      stopRequestedDuringSave = true;
    }
    uint8_t encrypted = cipher.crypt((uint8_t)decryptedScript[index]);
    success = file.write(&encrypted, 1) == 1;
  }
  file.close();

  if (success) {
    Serial.print(F("OK SAVE "));
    Serial.print(length);
    Serial.println(F(" bytes"));
    if (stopRequestedDuringSave) Serial.println(F("INFO STOP applied after SAVE"));
  } else {
    Serial.println(F("ERR SAVE write-failed"));
  }
  return success;
}

bool loadRuntime() {
  if (!sdReady) {
    Serial.println(F("ERR LOAD sd-unavailable"));
    return false;
  }

  File file = dot.openFile(RUNTIME_FILE, "r");
  if (!file) {
    Serial.println(F("ERR LOAD file-not-found"));
    return false;
  }

  RuntimeFileHeader header = {};
  bool valid = file.read(header.magic, sizeof(header.magic)) == sizeof(header.magic);
  valid = valid && readUint32(file, header.version);
  valid = valid && readUint32(file, header.nonce);
  valid = valid && readUint32(file, header.payloadLength);
  valid = valid && readUint32(file, header.payloadCrc);
  valid = valid && memcmp(header.magic, RUNTIME_MAGIC, sizeof(header.magic)) == 0;
  valid = valid && header.version == FILE_FORMAT_VERSION;
  valid = valid && header.payloadLength <= MAX_RUNTIME_BYTES;
  if (!valid) {
    file.close();
    Serial.println(F("ERR LOAD invalid-header"));
    return false;
  }

  RuntimeCipher cipher(header.nonce);
  for (uint32_t index = 0; index < header.payloadLength; ++index) {
    if (pollEmergencyStop()) {
      file.close();
      Serial.println(F("ERR LOAD aborted"));
      return false;
    }
    int encrypted = file.read();
    if (encrypted < 0) {
      file.close();
      Serial.println(F("ERR LOAD truncated-file"));
      return false;
    }
    decryptedScript[index] = (char)cipher.crypt((uint8_t)encrypted);
  }
  file.close();
  decryptedScript[header.payloadLength] = '\0';

  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t index = 0; index < header.payloadLength; ++index) {
    crc = crc32Update(crc, (uint8_t)decryptedScript[index]);
  }
  if (~crc != header.payloadCrc) {
    Serial.println(F("ERR LOAD crc-mismatch"));
    return false;
  }

  // LOAD può arrivare anche mentre il runtime precedente è ancora attivo.
  // Prima di sostituire lo script riportiamo quindi le uscite e il display
  // allo stato sicuro, evitando che il programma precedente resti visibile
  // mentre quello nuovo viene preparato.
  stopRuntimeOutputs();
  runtimeRunning = false;
  runtimeIndex = 0;
  runtimeWaitUntil = 0;
  resetRuntimeControlState();
  runtimeScriptLength = (uint16_t)header.payloadLength;
  if (!rebuildRuntimeCommandIndex(true)) {
    runtimeScriptLength = 0;
    runtimeCommandCount = 0;
    decryptedScript[0] = '\0';
    Serial.println(F("ERR LOAD command-too-long"));
    return false;
  }

  Serial.print(F("OK LOAD "));
  Serial.print(runtimeCommandCount);
  Serial.println(F(" commands"));
  return true;
}

// `uint64_t` è necessario per rappresentare correttamente schede SD oltre
// 4 GB. Il cast a `unsigned long` tronca il valore sui core ESP32 e faceva
// apparire una capacità totale errata nel Dev Studio. Stampiamo quindi il
// numero decimale senza passare da un tipo a 32 bit.
void printUint64(uint64_t value) {
  char digits[21];
  digits[20] = '\0';
  uint8_t index = 20;
  do {
    digits[--index] = (char)('0' + (value % 10));
    value /= 10;
  } while (value > 0 && index > 0);
  Serial.print(&digits[index]);
}

#if !defined(ARDUINO_ARCH_ESP32)
struct PortableSdStats {
  uint64_t cardBytes;
  uint64_t totalBytes;
  uint64_t usedBytes;
  uint64_t freeBytes;
};

uint64_t scanPortableSdDirectory(File& directory, uint8_t depth = 0) {
  if (depth > 8) return 0;
  uint64_t used = 0;
  while (true) {
    File entry = directory.openNextFile();
    if (!entry) break;
    if (entry.isDirectory()) {
      used += scanPortableSdDirectory(entry, (uint8_t)(depth + 1));
    } else {
      used += (uint64_t)entry.size();
    }
    entry.close();
  }
  return used;
}

bool countPortableFreeClusters(SdVolume& volume, Sd2Card* card, uint64_t& freeClusters) {
  freeClusters = 0;
  const uint8_t fatType = volume.fatType();
  if (fatType != 16 && fatType != 32) return false;

  uint8_t block[512];
  const uint32_t firstCluster = 2;
  const uint32_t lastCluster = volume.clusterCount() + 1;
  const uint32_t entriesPerBlock = fatType == 16 ? 256UL : 128UL;
  const uint32_t fatBlocks = volume.blocksPerFat();

  for (uint32_t blockIndex = 0; blockIndex < fatBlocks; ++blockIndex) {
    if (pollEmergencyStop()) return false;
    if (!card->readBlock(volume.fatStartBlock() + blockIndex, block)) {
      return false;
    }
    const uint32_t first = blockIndex * entriesPerBlock;
    const uint32_t last = first + entriesPerBlock;
    const uint32_t from = first < firstCluster ? firstCluster : first;
    const uint32_t to = last > lastCluster + 1 ? lastCluster + 1 : last;
    for (uint32_t cluster = from; cluster < to; ++cluster) {
      const uint32_t entry = cluster - first;
      uint32_t value = 0;
      if (fatType == 16) {
        const uint32_t offset = entry * 2UL;
        value = (uint32_t)block[offset] | ((uint32_t)block[offset + 1] << 8);
      } else {
        const uint32_t offset = entry * 4UL;
        value = (uint32_t)block[offset] |
                ((uint32_t)block[offset + 1] << 8) |
                ((uint32_t)block[offset + 2] << 16) |
                ((uint32_t)block[offset + 3] << 24);
        value &= 0x0FFFFFFFUL;
      }
      if (value == 0) ++freeClusters;
    }
  }
  return true;
}

bool readPortableSdStats(PortableSdStats& stats) {
  stats = {};
  Sd2Card* card = SdVolume::sdCard();
  if (card == nullptr) return false;

  // Re-read the FAT geometry without touching the SDClass private members.
  // The card object is the one initialized by SD.begin().
  SdVolume volume;
  if (!volume.init(card)) return false;

  const uint32_t cardBlocks = card->cardSize();
  if (cardBlocks == 0 || cardBlocks == 0xFFFFFFFFUL) return false;
  stats.cardBytes = (uint64_t)cardBlocks * 512ULL;
  stats.totalBytes = (uint64_t)volume.clusterCount() *
                     (uint64_t)volume.blocksPerCluster() * 512ULL;

  uint64_t freeClusters = 0;
  if (countPortableFreeClusters(volume, card, freeClusters)) {
    stats.freeBytes = freeClusters * (uint64_t)volume.blocksPerCluster() * 512ULL;
    if (stats.freeBytes > stats.totalBytes) stats.freeBytes = stats.totalBytes;
    stats.usedBytes = stats.totalBytes - stats.freeBytes;
    return true;
  }

  // FAT12 is not used by normal microSD cards, but retain a portable
  // fallback for old/small media where the FAT entry format is not covered
  // by the fast counter above.
  File root = SD.open("/");
  if (!root || !root.isDirectory() || stats.totalBytes == 0) {
    if (root) root.close();
    return false;
  }
  stats.usedBytes = scanPortableSdDirectory(root);
  root.close();
  if (stats.usedBytes > stats.totalBytes) stats.usedBytes = stats.totalBytes;
  stats.freeBytes = stats.totalBytes - stats.usedBytes;
  return true;
}
#endif

// Nome leggibile della scheda su cui è stato compilato il Bridge. I macro
// sono forniti dai rispettivi core Arduino e permettono al Dev Studio di
// distinguere la piattaforma reale dal valore predefinito del progetto.
const char* currentBoardName() {
#if defined(ARDUINO_NANO_ESP32)
  return "Nano ESP32";
#elif defined(ARDUINO_SAMD_NANO_33_IOT)
  return "Nano 33 IoT";
#elif defined(ARDUINO_NANO_RP2040_CONNECT)
  return "Nano RP2040 Connect";
#elif defined(ARDUINO_NANO_MATTER)
  return "Nano Matter";
#elif defined(ARDUINO_NANO33BLESENSE) || defined(ARDUINO_NANO33BLE_SENSE)
  return "Nano 33 BLE Sense";
#elif defined(ARDUINO_NANO33BLE)
  return "Nano 33 BLE";
#elif defined(ARDUINO_ARCH_ESP32)
  return "ESP32";
#elif defined(ARDUINO_ARCH_RP2040)
  return "RP2040";
#elif defined(ARDUINO_ARCH_SAMD)
  return "SAMD";
#elif defined(ARDUINO_ARCH_NRF52840)
  return "nRF52840";
#else
  return "MyDot";
#endif
}

bool runtimeAnyTaskWaitingForButton() {
  if (runtimeTaskCount == 0) return runtimeWaitingForButton;
  for (uint8_t index = 0; index < runtimeTaskCount; ++index) {
    if (runtimeTasks[index].active && runtimeTasks[index].waitingForButton) return true;
  }
  return false;
}

bool runtimeAnyTaskWaitingForCloud() {
  if (runtimeTaskCount == 0) return runtimeWaitingForCloudCommand;
  for (uint8_t index = 0; index < runtimeTaskCount; ++index) {
    if (runtimeTasks[index].active && runtimeTasks[index].waitingForCloudCommand) return true;
  }
  return false;
}

void printStatus() {
  Serial.print(F("STATUS sd="));
  Serial.print(sdReady ? F("ready") : F("unavailable"));
  Serial.print(F(" commands="));
  Serial.print(runtimeCommandCount);
  uint8_t variableCount = 0;
  for (uint8_t index = 0; index < MAX_RUNTIME_VARIABLES; ++index) {
    if (runtimeVariables[index].used) ++variableCount;
  }
  Serial.print(F(" variables="));
  Serial.print(variableCount);
  Serial.print(F(" runtimeLogs="));
  Serial.print(runtimeLogsEnabled ? F("on") : F("off"));
  Serial.print(F(" running="));
  Serial.print(runtimeRunning ? F("true") : F("false"));
  Serial.print(F(" tasks="));
  Serial.print(runtimeTaskCount);
  Serial.print(F(" waitingButton="));
  Serial.print(runtimeAnyTaskWaitingForButton() ? F("true") : F("false"));
  Serial.print(F(" waitingCloud="));
  Serial.print(runtimeAnyTaskWaitingForCloud() ? F("true") : F("false"));
  Serial.print(F(" loopDepth="));
  Serial.print(runtimeLoopDepth);
  Serial.print(F(" index="));
  Serial.print(runtimeIndex);
  Serial.print(F(" board=\""));
  Serial.print(currentBoardName());
  Serial.print(F("\""));

  // STATUS resta una singola riga per mantenere compatibilità con i Dev
  // Studio già pubblicati, ma ora espone anche le risorse utili alla
  // diagnostica. MyDot incapsula già il driver corretto per ESP32/NINA, per
  //ciò non è necessario duplicare qui la logica di riconnessione.
#if MYDOT_HAS_WIFI
  const bool wifiConnected = dot.isWiFiConnected();
  Serial.print(F(" wifi="));
  Serial.print(wifiConnected ? F("connected") : F("disconnected"));
  Serial.print(F(" wifiRssi="));
  Serial.print(wifiConnected ? dot.getWiFiRSSI() : 0);
  Serial.print(F(" cloud="));
#if MYDOT_HAS_CLOUD
  Serial.print(dot.isCloudConnected() ? F("connected") : F("disconnected"));
#else
  Serial.print(F("unavailable"));
#endif
#else
  Serial.print(F(" wifi=unavailable wifiRssi=0 cloud=unavailable"));
#endif

  // ESP32 espone direttamente le metriche del filesystem. Sulle altre Nano
  // il backend portabile legge la geometria FAT e calcola lo spazio dati
  // usato durante la scansione, senza chiamare API specifiche ESP32.
#if !MYDOT_HAS_SD_CAPACITY
  Serial.print(F(" sdCardBytes=unknown sdTotalBytes=unknown sdUsedBytes=unknown sdFreeBytes=unknown"));
#elif defined(ARDUINO_ARCH_ESP32)
  if (sdReady) {
    const uint64_t total = SD.totalBytes();
    uint64_t card = SD.cardSize();
    // Alcune versioni del driver restituiscono 0 per `cardSize()` anche se
    // il filesystem è montato correttamente. In quel caso la capacità
    // utilizzabile è il fallback più affidabile disponibile.
    if (card == 0 || card == (uint64_t)-1) card = total;
    const uint64_t used = SD.usedBytes();
    Serial.print(F(" sdCardBytes="));
    printUint64(card);
    Serial.print(F(" sdTotalBytes="));
    printUint64(total);
    Serial.print(F(" sdUsedBytes="));
    printUint64(used);
    Serial.print(F(" sdFreeBytes="));
    printUint64(total >= used ? total - used : 0);
  } else {
    Serial.print(F(" sdCardBytes=unknown sdTotalBytes=unknown sdUsedBytes=unknown sdFreeBytes=unknown"));
  }
#else
  PortableSdStats portableSdStats = {};
  if (sdReady && readPortableSdStats(portableSdStats)) {
    Serial.print(F(" sdCardBytes="));
    printUint64(portableSdStats.cardBytes);
    Serial.print(F(" sdTotalBytes="));
    printUint64(portableSdStats.totalBytes);
    Serial.print(F(" sdUsedBytes="));
    printUint64(portableSdStats.usedBytes);
    Serial.print(F(" sdFreeBytes="));
    printUint64(portableSdStats.freeBytes);
  } else {
    Serial.print(F(" sdCardBytes=unknown sdTotalBytes=unknown sdUsedBytes=unknown sdFreeBytes=unknown"));
  }
#endif

  // MyDot seleziona il backend corretto: ESP.get*Heap() su ESP32, Mbed
  // statistics su Nano RP2040 Connect e heap/stack gap sul SAMD21.
  uint32_t ramFree = 0;
  uint32_t ramTotal = 0;
  if (dot.getMemoryStats(ramFree, ramTotal)) {
    Serial.print(F(" ramFreeBytes="));
    Serial.print((unsigned long)ramFree);
    Serial.print(F(" ramTotalBytes="));
    Serial.print((unsigned long)ramTotal);
  } else {
    Serial.print(F(" ramFreeBytes=unknown ramTotalBytes=unknown"));
  }
#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F(" taskWatchdog="));
  Serial.print(bridgeTaskWatchdogReady ? F("ready") : F("unavailable"));
  Serial.print(F(" resetReason="));
  Serial.print((int)bridgeResetReason);
  Serial.print(F(" heapLargestBytes="));
  Serial.print((unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.print(F(" heapMinimumBytes="));
  Serial.print((unsigned long)esp_get_minimum_free_heap_size());
  if (previousBridgeTraceValid) {
    Serial.print(F(" previousStage="));
    Serial.print(bridgeStageName(previousBridgeTrace.stage));
    Serial.print(F(" previousIndex="));
    Serial.print(previousBridgeTrace.commandIndex);
    Serial.print(F(" previousHeapBytes="));
    Serial.print((unsigned long)previousBridgeTrace.freeHeap);
    Serial.print(F(" previousLargestBytes="));
    Serial.print((unsigned long)previousBridgeTrace.largestBlock);
    Serial.print(F(" previousMinimumBytes="));
    Serial.print((unsigned long)previousBridgeTrace.minimumFreeHeap);
  }
#endif
  Serial.println();
}

void processSerialCommand(String command) {
  command.trim();
  if (command.length() == 0) {
    return;
  }

  String rest = command;
  String operation = takeToken(rest);
  operation.toUpperCase();
#if defined(ARDUINO_ARCH_ESP32)
  // If a browser request itself stalls, the next STATUS identifies which
  // request was in flight instead of reporting only the generic serial stage.
  if (operation == "STATUS") markBridgeStage(BRIDGE_STAGE_SERIAL_STATUS);
  else if (operation == "CAPS") markBridgeStage(BRIDGE_STAGE_SERIAL_CAPS);
  else if (operation == "EXEC") markBridgeStage(BRIDGE_STAGE_SERIAL_EXEC);
  else markBridgeStage(BRIDGE_STAGE_SERIAL_OTHER);
#endif

  if (operation == "HELP") {
    if (rest.length() > 0) Serial.println(F("ERR HELP unexpected-arguments"));
    else printHelp();
  } else if (operation == "CAPS") {
    if (rest.length() > 0) Serial.println(F("ERR CAPS unexpected-arguments"));
    else printCapabilities();
  } else if (operation == "STATUS") {
    if (rest.length() > 0) Serial.println(F("ERR STATUS unexpected-arguments"));
    else printStatus();
  } else if (operation == "LOGS") {
    rest.trim();
    rest.toUpperCase();
    if (rest == "ON") {
      runtimeLogsEnabled = true;
      dot.setDiagnosticStream(&bridgeSerial);
    } else if (rest == "OFF") {
      runtimeLogsEnabled = false;
      dot.setDiagnosticStream(nullptr);
    }
    else if (rest != "STATUS") {
      Serial.println(F("ERR LOGS expected-ON-OFF-STATUS"));
      return;
    }
    Serial.println(runtimeLogsEnabled ? F("LOGS ON") : F("LOGS OFF"));
  } else if (operation == "ADD") {
    addRuntimeCommand(rest);
  } else if (operation == "LIST") {
    if (rest.length() > 0) Serial.println(F("ERR LIST unexpected-arguments"));
    else listRuntimeCommands();
  } else if (operation == "CLEAR") {
    if (rest.length() > 0) {
      Serial.println(F("ERR CLEAR unexpected-arguments"));
      return;
    }
    runtimeCommandCount = 0;
    runtimeScriptLength = 0;
    decryptedScript[0] = '\0';
    runtimeRunning = false;
    runtimeIndex = 0;
    runtimeWaitUntil = 0;
    resetRuntimeControlState();
    // Conferma subito la cancellazione della sequenza. Le uscite vengono
    // portate allo stato sicuro subito dopo: un eventuale dispositivo I2C
    // non disponibile non deve far scadere il comando CLEAR nel Dev Studio.
    Serial.println(F("OK CLEAR"));
    // CLEAR ferma davvero l'esecuzione: non lasciare relay, ventola o
    // NeoPixel nello stato dell'ultima sequenza quando l'utente prepara un
    // nuovo programma dal Dev Studio.
    stopRuntimeOutputs();
  } else if (operation == "SAVE") {
    if (rest.length() > 0) Serial.println(F("ERR SAVE unexpected-arguments"));
    else saveRuntime();
  } else if (operation == "LOAD") {
    if (rest.length() > 0) Serial.println(F("ERR LOAD unexpected-arguments"));
    else loadRuntime();
  } else if (operation == "RUN") {
    if (rest.length() > 0) {
      Serial.println(F("ERR RUN unexpected-arguments"));
      return;
    }
    if (runtimeCommandCount == 0) {
      Serial.println(F("ERR RUN empty-sequence"));
    } else if (initializeRuntimeTasks()) {
      Serial.println(F("OK RUN"));
    } else {
      runtimeRunning = false;
    }
  } else if (operation == "STOP") {
    if (rest.length() > 0) Serial.println(F("ERR STOP unexpected-arguments"));
    else stopRuntime();
  } else if (operation == "REBOOT") {
    if (rest.length() > 0) Serial.println(F("ERR REBOOT unexpected-arguments"));
    else requestDeviceReboot();
  } else if (operation == "EXEC") {
    if (rest.length() == 0) {
      Serial.println(F("ERR EXEC missing-command"));
    } else {
      const uint8_t previousRuntimeTask = runtimeCurrentTask;
      runtimeCurrentTask = 0xFF;
      const bool executed = executeRuntimeCommand(rest, false);
      runtimeCurrentTask = runtimeRunning && runtimeTaskCount > 0 ? previousRuntimeTask : 0xFF;
      if (executed) Serial.println(F("OK EXEC"));
    }
  } else {
    Serial.print(F("ERR UNKNOWN "));
    Serial.println(operation);
  }
}

void serviceSerial() {
  uint8_t bytesProcessed = 0;
  while (bytesProcessed < MAX_SERIAL_BYTES_PER_PASS && Serial.available() > 0) {
    char character = (char)Serial.read();
    ++bytesProcessed;
    if ((uint8_t)character == SERIAL_EMERGENCY_STOP) {
      emergencyStopRequested = true;
      pollEmergencyStop();
      serialLineLength = 0;
      serialLine[0] = '\0';
      serialLineOverflow = false;
      break;
    }
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      serialLine[serialLineLength] = '\0';
      // Every completed input line is a user/editor request, so its reply is
      // visible even when autonomous diagnostics are disabled.
      bridgeSerial.setOutputEnabled(true);
      if (serialLineOverflow) {
        Serial.println(F("ERR SERIAL line-too-long"));
      } else {
        processSerialCommand(String(serialLine));
      }
      if (rebootRequested) rebootRequestedFromSerial = true;
      bridgeSerial.setOutputEnabled(runtimeLogsEnabled);
      serialLineLength = 0;
      serialLine[0] = '\0';
      serialLineOverflow = false;
      // One complete request per loop pass prevents browser traffic from
      // starving Wi-Fi, MQTT, the runtime and the task-watchdog feed.
      break;
    } else if (!serialLineOverflow) {
      if (serialLineLength >= MAX_SERIAL_LINE_LENGTH) {
        serialLineOverflow = true;
      } else {
        serialLine[serialLineLength++] = character;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  limitBridgeUsbWriteWait();
  dot.setDiagnosticStream(nullptr);
#if defined(ARDUINO_ARCH_ESP32)
  beginBridgeTrace();
  sampleBridgeMemory();
#endif
  // Arm the native watchdog before peripheral and SD initialization too:
  // a stuck I2C bus during boot must not leave the Bridge unresponsive.
  beginBridgeTaskWatchdog();
  dot.begin();

  sdReady = dot.beginSD();
  if (sdReady) {
    Serial.println(F("OK SD ready"));
    if (dot.fileExists(RUNTIME_FILE)) {
      Serial.println(F("INFO runtime file found; loading it"));
      if (!loadRuntime()) {
        Serial.println(F("WARN runtime auto-load failed; use LOAD to retry"));
      } else if (runtimeCommandCount > 0) {
        // A saved runtime is the user's resident program: after a power cycle
        // restore it and start it automatically. RUN remains available for
        // manual restarts, while STOP can still halt it at any time.
        if (initializeRuntimeTasks()) Serial.println(F("INFO runtime auto-start"));
        else Serial.println(F("WARN runtime task sequence invalid; use RUN to retry"));
      }
    }
  } else {
    Serial.println(F("WARN SD unavailable; serial runtime still available"));
  }

  Serial.println(F("MyDot Dev Studio Bridge ready"));
  Serial.println(F("Send HELP or CAPS; commands end with newline"));
}

void loop() {
  feedBridgeTaskWatchdog();
#if defined(ARDUINO_ARCH_ESP32)
  static unsigned long lastMemorySample = 0;
  if (millis() - lastMemorySample >= 1000UL) {
    lastMemorySample = millis();
    sampleBridgeMemory();
  }
  markBridgeStage(BRIDGE_STAGE_SERIAL);
#endif
  serviceSerial();
#if defined(ARDUINO_ARCH_ESP32)
  markBridgeStage(BRIDGE_STAGE_BUTTONS);
#endif
  // Campiona gli edge prima delle attività potenzialmente lente (Wi-Fi/MQTT)
  // così un click breve resta disponibile per WAIT_BUTTON.
  serviceButtonEdges();
  // Keep Wi-Fi/MQTT state machines alive while the serial protocol and
  // runtime sequence are serviced. This is required after WIFI/CLOUD BEGIN.
  // Poll MQTT before servicing a waiting CLOUD command so a packet received in
  // this loop is consumed immediately instead of waiting for the next tick.
#if defined(ARDUINO_ARCH_ESP32)
  markBridgeStage(BRIDGE_STAGE_NETWORK);
#endif
  dot.run();
#if defined(ARDUINO_ARCH_ESP32)
  markBridgeStage(BRIDGE_STAGE_RUNTIME, runtimeIndex);
#endif
  serviceRuntime();
#if defined(ARDUINO_ARCH_ESP32)
  markBridgeStage(BRIDGE_STAGE_REBOOT);
#endif
  serviceDeviceReboot();
#if defined(ARDUINO_ARCH_ESP32)
  markBridgeStage(BRIDGE_STAGE_IDLE);
#endif
}
