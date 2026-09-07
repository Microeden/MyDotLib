#ifndef MYDOT_VASE_STATE_H
#define MYDOT_VASE_STATE_H

/*
 * Persistent light-state storage for the MyDot Vase example.
 *
 * Storage backends are selected from the board definition:
 *   - Nano ESP32: Preferences (ESP32 NVS)
 *   - Nano RP2040 Connect with the official Mbed core: Mbed KVStore
 *   - RP2040 cores that provide EEPROM: EEPROM
 *   - Nano 33 IoT: EEPROM emulation
 *
 * The record contains the light state and the configured pump duration.
 */

#include <Arduino.h>
#include <stddef.h>

#if defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>
#define MYDOT_VASE_STORAGE_PREFERENCES

#elif defined(ARDUINO_NANO_RP2040_CONNECT) && defined(ARDUINO_ARCH_MBED)

#include <kvstore_global_api.h>
#define MYDOT_VASE_STORAGE_MBED_KV

#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_SAMD_NANO_33_IOT) || defined(ARDUINO_ARCH_SAMD)

#include <EEPROM.h>
#define MYDOT_VASE_STORAGE_EEPROM

#else

#error "MyDot Vase state storage supports Nano ESP32, Nano RP2040 Connect, and Nano 33 IoT only."

#endif

struct MyDotVaseLightState {
  bool lightsOn;
  uint8_t mode;
  uint8_t brightness;
  uint8_t pumpDurationSeconds;
};

static const uint8_t MYDOT_VASE_DEFAULT_PUMP_DURATION_SECONDS = 2;

class MyDotVaseStateStore {
public:
  bool begin() {
#if defined(MYDOT_VASE_STORAGE_PREFERENCES)
    _started = _preferences.begin("mydotvase", false);
#elif defined(MYDOT_VASE_STORAGE_MBED_KV)
    // The Mbed KVStore global API initializes its backend on first use.
    _started = true;
#elif defined(MYDOT_VASE_STORAGE_EEPROM)
#if defined(ARDUINO_ARCH_RP2040)
    EEPROM.begin(sizeof(Record));
    _started = true;
#else
    _started = true;
#endif
#endif
    return _started;
  }

  bool load(MyDotVaseLightState& state) {
    if (!_started) {
      return false;
    }

    Record record;
    if (readRecord(record) && isValid(record)) {
      state.lightsOn = record.lightsOn != 0;
      state.mode = record.mode;
      state.brightness = record.brightness;
      state.pumpDurationSeconds = record.pumpDurationSeconds;
      return true;
    }

    // Keep light settings created by version 1.0.7 and earlier. The new
    // pump-duration field falls back to the default until the next save.
    LegacyRecord legacy;
    if (!readLegacyRecord(legacy) || !isLegacyValid(legacy)) {
      return false;
    }

    state.lightsOn = legacy.lightsOn != 0;
    state.mode = legacy.mode;
    state.brightness = legacy.brightness;
    state.pumpDurationSeconds = MYDOT_VASE_DEFAULT_PUMP_DURATION_SECONDS;
    return true;
  }

  bool save(const MyDotVaseLightState& state) {
    if (!_started) {
      return false;
    }

    Record record = {};
    record.magic = RECORD_MAGIC;
    record.version = RECORD_VERSION;
    record.lightsOn = state.lightsOn ? 1 : 0;
    record.mode = state.mode;
    record.brightness = state.brightness;
    record.pumpDurationSeconds = state.pumpDurationSeconds;
    record.checksum = calculateChecksum(record);
    return writeRecord(record);
  }

private:
  static const uint32_t RECORD_MAGIC = 0x4D445653UL;  // "MDVS"
  // Version 4 adds the persisted pump duration to the state record.
  static const uint8_t RECORD_VERSION = 4;

  struct Record {
    uint32_t magic;
    uint8_t version;
    uint8_t lightsOn;
    uint8_t mode;
    uint8_t brightness;
    uint8_t pumpDurationSeconds;
    uint8_t checksum;
  };

  struct LegacyRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t lightsOn;
    uint8_t mode;
    uint8_t brightness;
    uint8_t checksum;
  };

  static const uint8_t LEGACY_RECORD_VERSION = 3;

  bool _started = false;

#if defined(MYDOT_VASE_STORAGE_PREFERENCES)
  Preferences _preferences;
#endif

  static uint8_t calculateChecksum(const Record& record) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint8_t checksum = 0xA5;

    // The checksum byte itself is excluded from the calculation.
    for (size_t i = 0; i < offsetof(Record, checksum); ++i) {
      checksum ^= bytes[i];
    }
    return checksum;
  }

  static bool isValid(const Record& record) {
    return record.magic == RECORD_MAGIC &&
           record.version == RECORD_VERSION &&
           record.checksum == calculateChecksum(record);
  }

  static uint8_t calculateLegacyChecksum(const LegacyRecord& record) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    uint8_t checksum = 0xA5;
    for (size_t i = 0; i < offsetof(LegacyRecord, checksum); ++i) {
      checksum ^= bytes[i];
    }
    return checksum;
  }

  static bool isLegacyValid(const LegacyRecord& record) {
    return record.magic == RECORD_MAGIC &&
           record.version == LEGACY_RECORD_VERSION &&
           record.checksum == calculateLegacyChecksum(record);
  }

  bool readRecord(Record& record) {
#if defined(MYDOT_VASE_STORAGE_PREFERENCES)
    if (_preferences.getBytesLength("lights") != sizeof(Record)) {
      return false;
    }
    return _preferences.getBytes("lights", &record, sizeof(Record)) == sizeof(Record);

#elif defined(MYDOT_VASE_STORAGE_MBED_KV)
    size_t actualSize = 0;
    return kv_get("/kv/mydotvase_lights", &record, sizeof(Record), &actualSize) == 0 &&
           actualSize == sizeof(Record);

#elif defined(MYDOT_VASE_STORAGE_EEPROM)
    for (size_t i = 0; i < sizeof(Record); ++i) {
      reinterpret_cast<uint8_t*>(&record)[i] = EEPROM.read(i);
    }
    return true;
#endif
  }

  bool readLegacyRecord(LegacyRecord& record) {
#if defined(MYDOT_VASE_STORAGE_PREFERENCES)
    if (_preferences.getBytesLength("lights") != sizeof(LegacyRecord)) {
      return false;
    }
    return _preferences.getBytes("lights", &record, sizeof(LegacyRecord)) == sizeof(LegacyRecord);

#elif defined(MYDOT_VASE_STORAGE_MBED_KV)
    size_t actualSize = 0;
    return kv_get("/kv/mydotvase_lights", &record, sizeof(LegacyRecord), &actualSize) == 0 &&
           actualSize == sizeof(LegacyRecord);

#elif defined(MYDOT_VASE_STORAGE_EEPROM)
    for (size_t i = 0; i < sizeof(LegacyRecord); ++i) {
      reinterpret_cast<uint8_t*>(&record)[i] = EEPROM.read(i);
    }
    return true;
#endif
  }

  bool writeRecord(const Record& record) {
#if defined(MYDOT_VASE_STORAGE_PREFERENCES)
    return _preferences.putBytes("lights", &record, sizeof(Record)) == sizeof(Record);

#elif defined(MYDOT_VASE_STORAGE_MBED_KV)
    return kv_set("/kv/mydotvase_lights", &record, sizeof(Record), 0) == 0;

#elif defined(MYDOT_VASE_STORAGE_EEPROM)
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record);
    for (size_t i = 0; i < sizeof(Record); ++i) {
      EEPROM.update(i, bytes[i]);
    }
#if defined(ARDUINO_ARCH_RP2040)
    EEPROM.commit();
    return true;
#else
    return true;
#endif
#endif
  }
};

#endif
