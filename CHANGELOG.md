# Changelog

## 1.0.5

- Added automatic Wi-Fi and MQTT reconnection with stale-session recovery.
- Added the MyDot Vase cloud/local lighting, pump, and soil-sensor example.
- Added persistent light-state storage and documented the cloud widget APIs.
- Improved Nano ESP32 and Nano RP2040 Connect compatibility.
- Declared the SD and WiFiNINA dependencies for Library Manager installs.

## 1.0.4

- Restructured the library according to the Arduino 1.5 library format.
- Added complete examples for the MyDot peripherals and cloud methods.
- Added placeholder `microeden_secrets.h` files so credentials are never stored in the repository.
- Added the ESP32 ISRG Root X1 certificate for validated TLS connections.
- Corrected Nano ESP32 pin aliases and NeoPixel GPIO handling.
- Restored the MyDot V1.0 carrier DRV8830 address to `0x68` (A0/A1 tied to +5 V).
