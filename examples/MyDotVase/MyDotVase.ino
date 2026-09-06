/*
 * MyDot Vase
 *
 * Reads an AZ-Delivery analog soil-moisture sensor on A0 and publishes the
 * reading to the Microeden cloud. The MyDot relay is used as the pump output.
 *
 * Cloud commands (the default command key is "content"):
 *   "on"   - keeps the pump relay on
 *   "off"  - turns the pump relay off
 *   "pump" - turns the pump on for two seconds, then turns it off
 *   "lights_on" / "lights_off" - switches the NeoPixel lights
 *   "lights_warm" / "lights_cool" - selects the color temperature
 *   "grow_veg" / "grow_bloom" / "grow_full" - selects a grow-light mode
 *   "lights_mode" with a numeric "mode" field from 0 to 4
 *   "brightness" with a numeric "brightness" field from 0 to 255
 *   or the compact widget form "brightness_128"
 *
 * Every pump activation increments pumpActivations and publishes telemetry.
 * A periodic telemetry message is also sent every five seconds.
 * Button A toggles the lights locally; button B cycles through all light modes.
 *
 * Hardware note: power the MyDot carrier through its external DC jack. USB
 * power alone is not sufficient for the relay or the pump output stage.
 * IMPORTANT: power the AZ-Delivery soil sensor from 3.3 V, not 5 V. A 5 V
 * analog output can exceed the board input range and damage the board.
 * Connect the sensor output to A0 and connect the pump driver/input to the
 * MyDot relay output. Do not connect a pump directly to an Arduino GPIO.
 */

#include <MyDot.h>
#include "microeden_secrets.h"
#include "MyDotVaseState.h"

MyDot dot;
MyDotVaseStateStore lightStateStore;
Slider brightnessSlider("brightness", dot);
Switch pumpStateWidget("pumpOn", dot);
Switch lightsStateWidget("lightsOn", dot);
Level lightModeWidget("lightModeIndex", dot);

#define SOIL_SENSOR_PIN A0
#define PUMP_RELAY_PIN RELAY
#define LIGHT_TOGGLE_BUTTON BUTTON_A
#define LIGHT_TEMPERATURE_BUTTON BUTTON_B

const unsigned long PUMP_DURATION_MS = 2000;
const unsigned long TELEMETRY_INTERVAL_MS = 5000;
const unsigned long BRIGHTNESS_SETTLE_MS = 300;

// Calibrate these values with the actual sensor and soil. The AZ-Delivery
// analog output normally decreases as the soil becomes wetter.
const int SOIL_RAW_DRY = 3000;
const int SOIL_RAW_WET = 1300;

// NeoPixel approximations of the available light modes. NeoPixels cannot
// reproduce a calibrated horticultural spectrum; these are visual presets.
const uint8_t COOL_WHITE_R = 220;
const uint8_t COOL_WHITE_G = 240;
const uint8_t COOL_WHITE_B = 255;
const uint8_t WARM_WHITE_R = 255;
const uint8_t WARM_WHITE_G = 150;
const uint8_t WARM_WHITE_B = 70;
const uint8_t GROW_VEG_R = 40;
const uint8_t GROW_VEG_G = 120;
const uint8_t GROW_VEG_B = 255;
const uint8_t GROW_BLOOM_R = 255;
const uint8_t GROW_BLOOM_G = 30;
const uint8_t GROW_BLOOM_B = 10;
const uint8_t GROW_FULL_R = 255;
const uint8_t GROW_FULL_G = 80;
const uint8_t GROW_FULL_B = 180;

enum LightMode {
  LIGHT_MODE_COOL_WHITE = 0,
  LIGHT_MODE_WARM_WHITE = 1,
  LIGHT_MODE_GROW_VEG = 2,
  LIGHT_MODE_GROW_BLOOM = 3,
  LIGHT_MODE_GROW_FULL = 4,
  LIGHT_MODE_COUNT = 5
};

bool pumpOn = false;
bool timedPumpActive = false;
unsigned long timedPumpStopAt = 0;
unsigned long pumpActivations = 0;
bool lightsOn = false;
LightMode lightMode = LIGHT_MODE_COOL_WHITE;
uint8_t ledBrightness = 128;
int lastCloudBrightness = -1;
bool brightnessUpdatePending = false;
unsigned long brightnessChangedAt = 0;

void saveLightState() {
  MyDotVaseLightState state;
  state.lightsOn = lightsOn;
  state.mode = (uint8_t)lightMode;
  state.brightness = ledBrightness;

  if (!lightStateStore.save(state)) {
    Serial.println("Warning: unable to save light state");
  }
}

int readSoilRaw() {
  // A short average makes the cloud value less sensitive to ADC noise.
  long total = 0;
  for (uint8_t sample = 0; sample < 8; ++sample) {
    total += analogRead(SOIL_SENSOR_PIN);
  }
  return total / 8;
}

int soilPercentFromRaw(int raw) {
  // Dry is 0% and wet is 100%. Change the calibration constants above if
  // your sensor is installed with a different range or orientation.
  long percent = map(raw, SOIL_RAW_DRY, SOIL_RAW_WET, 0, 100);
  return constrain((int)percent, 0, 100);
}

const char* lightModeName() {
  switch (lightMode) {
    case LIGHT_MODE_COOL_WHITE: return "coolWhite";
    case LIGHT_MODE_WARM_WHITE: return "warmWhite";
    case LIGHT_MODE_GROW_VEG: return "growVegetative";
    case LIGHT_MODE_GROW_BLOOM: return "growBloom";
    case LIGHT_MODE_GROW_FULL: return "growFull";
    default: return "unknown";
  }
}

void publishTelemetry(const char* eventName) {
  int soilRaw = readSoilRaw();
  int soilPercent = soilPercentFromRaw(soilRaw);

  dot.writeKeyWord("soilRaw", soilRaw);
  dot.writeKeyWord("soilPercent", soilPercent);
  pumpStateWidget.write(pumpOn);
  dot.writeKeyWord("pumpActivations", (double)pumpActivations);
  lightsStateWidget.write(lightsOn);
  dot.writeKeyWord("lightMode", lightModeName());
  lightModeWidget.write((int)lightMode);
  // The cloud Slider is bound to the key "brightness".
  brightnessSlider.write((int)ledBrightness);
  dot.writeKeyWord("event", eventName);
  dot.sendCloud();

  Serial.print("Telemetry - soilRaw: ");
  Serial.print(soilRaw);
  Serial.print(", soilPercent: ");
  Serial.print(soilPercent);
  Serial.print(", pumpOn: ");
  Serial.print(pumpOn ? "true" : "false");
  Serial.print(", lightsOn: ");
  Serial.print(lightsOn ? "true" : "false");
  Serial.print(", lightMode: ");
  Serial.print(lightModeName());
  Serial.print(", brightness: ");
  Serial.print(ledBrightness);
  Serial.print(", event: ");
  Serial.println(eventName);
}

void publishPeriodicTelemetry() {
  publishTelemetry("periodic");
}

void turnPumpOn() {
  if (!pumpOn) {
    dot.setRelay(true);
    pumpOn = true;
  }

  // A manual "on" command cancels an active two-second timer.
  timedPumpActive = false;
  // Publish the state even when the relay was already on, so the cloud gets
  // an acknowledgement for every explicit "on" command.
  publishTelemetry("on");
}

void turnPumpOff() {
  dot.setRelay(false);
  pumpOn = false;
  timedPumpActive = false;
  publishTelemetry("off");
}

void startTimedPump() {
  dot.setRelay(true);
  pumpOn = true;
  timedPumpActive = true;
  timedPumpStopAt = millis() + PUMP_DURATION_MS;
  ++pumpActivations;

  // This message is sent immediately when the pump is activated.
  publishTelemetry("pump");
}

void serviceTimedPump() {
  if (!timedPumpActive) {
    return;
  }

  // Signed subtraction keeps this timeout safe across millis() rollover.
  if ((long)(millis() - timedPumpStopAt) >= 0) {
    timedPumpActive = false;
    dot.setRelay(false);
    pumpOn = false;
    publishTelemetry("pumpStop");
  }
}

void applyLights() {
  // Apply the saved brightness even while the lights are off. This keeps the
  // NeoPixel driver's brightness register synchronized with persistent state,
  // so the next local/cloud lights-on action uses the restored level.
  dot.setBrightness(ledBrightness);

  if (!lightsOn) {
    dot.clearPixels();
    return;
  }

  switch (lightMode) {
    case LIGHT_MODE_WARM_WHITE:
      dot.setAllPixels(WARM_WHITE_R, WARM_WHITE_G, WARM_WHITE_B);
      break;
    case LIGHT_MODE_GROW_VEG:
      dot.setAllPixels(GROW_VEG_R, GROW_VEG_G, GROW_VEG_B);
      break;
    case LIGHT_MODE_GROW_BLOOM:
      dot.setAllPixels(GROW_BLOOM_R, GROW_BLOOM_G, GROW_BLOOM_B);
      break;
    case LIGHT_MODE_GROW_FULL:
      dot.setAllPixels(GROW_FULL_R, GROW_FULL_G, GROW_FULL_B);
      break;
    case LIGHT_MODE_COOL_WHITE:
    default:
      dot.setAllPixels(COOL_WHITE_R, COOL_WHITE_G, COOL_WHITE_B);
      break;
  }
}

void setLightsState(bool state, const char* eventName) {
  lightsOn = state;
  applyLights();
  saveLightState();
  publishTelemetry(eventName);
}

void setLightMode(LightMode mode, const char* eventName) {
  lightMode = mode;
  applyLights();
  saveLightState();
  publishTelemetry(eventName);
}

void setLightModeIndex(int modeIndex) {
  modeIndex = constrain(modeIndex, 0, LIGHT_MODE_COUNT - 1);
  setLightMode((LightMode)modeIndex, "mode");
}

void cycleLightMode() {
  uint8_t nextMode = ((uint8_t)lightMode + 1) % LIGHT_MODE_COUNT;
  setLightMode((LightMode)nextMode, "localMode");
}

void setLightBrightness(int brightness) {
  uint8_t requestedBrightness = (uint8_t)constrain(brightness, 0, 255);
  if (requestedBrightness == ledBrightness) {
    return;
  }

  ledBrightness = requestedBrightness;
  applyLights();

  // Persist immediately. The delayed part below is only used to coalesce
  // telemetry while a slider is being dragged; a reset immediately after a
  // change must never lose the selected brightness.
  saveLightState();

  // A slider can generate many values while it is being dragged. Apply each
  // value immediately, then publish only the final settled value.
  brightnessUpdatePending = true;
  brightnessChangedAt = millis();
}

void servicePendingBrightnessUpdate() {
  if (!brightnessUpdatePending ||
      millis() - brightnessChangedAt < BRIGHTNESS_SETTLE_MS) {
    return;
  }

  brightnessUpdatePending = false;
  publishTelemetry("brightness");
}

void restoreLightState() {
  MyDotVaseLightState state;
  if (!lightStateStore.load(state)) {
    Serial.println("No saved light state; using defaults");
    lastCloudBrightness = ledBrightness;
    applyLights();
    return;
  }

  lightsOn = state.lightsOn;
  lightMode = (state.mode < LIGHT_MODE_COUNT)
                ? (LightMode)state.mode
                : LIGHT_MODE_COOL_WHITE;
  ledBrightness = state.brightness;
  lastCloudBrightness = ledBrightness;
  applyLights();

  Serial.print("Restored light state: ");
  Serial.print(lightsOn ? "on, " : "off, ");
  Serial.print(lightModeName());
  Serial.print(", brightness ");
  Serial.println(ledBrightness);
}

void handleCloudCommands() {
  if (dot.onCommand("on")) {
    turnPumpOn();
  }

  if (dot.onCommand("off")) {
    turnPumpOff();
  }

  if (dot.onCommand("pump")) {
    startTimedPump();
  }

  if (dot.onCommand("lights_on")) {
    setLightsState(true, "lightsOn");
  }

  if (dot.onCommand("lights_off")) {
    setLightsState(false, "lightsOff");
  }

  if (dot.onCommand("lights_warm")) {
    setLightMode(LIGHT_MODE_WARM_WHITE, "warmWhite");
  }

  if (dot.onCommand("lights_cool")) {
    setLightMode(LIGHT_MODE_COOL_WHITE, "coolWhite");
  }

  if (dot.onCommand("grow_veg")) {
    setLightMode(LIGHT_MODE_GROW_VEG, "growVegetative");
  }

  if (dot.onCommand("grow_bloom")) {
    setLightMode(LIGHT_MODE_GROW_BLOOM, "growBloom");
  }

  if (dot.onCommand("grow_full")) {
    setLightMode(LIGHT_MODE_GROW_FULL, "growFull");
  }

  // Expected command JSON: {"content":"lights_mode", "mode":2}
  if (dot.onCommand("lights_mode")) {
    setLightModeIndex(dot.readKeyWord<int>("mode"));
  }
}

void handleCloudBrightnessSlider() {
  // The Slider widget receives the compact form "brightness_128". Its read()
  // method converts the parsed value to an integer and returns -1 until the
  // first slider message arrives.
  int requestedBrightness = brightnessSlider.read();
  if (requestedBrightness < 0) {
    return;
  }

  requestedBrightness = constrain(requestedBrightness, 0, 255);
  if (requestedBrightness != lastCloudBrightness) {
    lastCloudBrightness = requestedBrightness;
    Serial.print("Brightness value from cloud: ");
    Serial.println(requestedBrightness);
    Serial.print("Cloud brightness received: ");
    Serial.println(requestedBrightness);
    setLightBrightness(requestedBrightness);
  }
}

void handleLocalLightControls() {
  if (dot.isButtonAClicked()) {
    bool newState = !lightsOn;
    Serial.print("Button A: lights ");
    Serial.println(newState ? "ON" : "OFF");
    setLightsState(newState, newState ? "localLightsOn" : "localLightsOff");
  }

  if (dot.isButtonBClicked()) {
    Serial.println("Button B: next light mode");
    cycleLightMode();
  }
}

void setup() {
  Serial.begin(115200);

  dot.begin();
  if (!lightStateStore.begin()) {
    Serial.println("Warning: light-state storage unavailable");
  }

  // Keep the application pin assignment explicit. dot.begin() also
  // initializes the relay, but these lines document and enforce the wiring.
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(LIGHT_TOGGLE_BUTTON, INPUT_PULLUP);
  pinMode(LIGHT_TEMPERATURE_BUTTON, INPUT_PULLUP);
  digitalWrite(PUMP_RELAY_PIN, LOW);
  restoreLightState();

  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
  dot.setCloudSync(TELEMETRY_INTERVAL_MS, publishPeriodicTelemetry);
}

void loop() {
  // Keep local controls responsive even when a Wi-Fi or MQTT operation takes
  // time. Light changes are applied before servicing the cloud connection.
  handleLocalLightControls();
  serviceTimedPump();

  dot.run();
  handleCloudCommands();
  handleCloudBrightnessSlider();
  servicePendingBrightnessUpdate();
}
