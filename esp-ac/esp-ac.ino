#include <Adafruit_NeoPixel.h>
#include <ArduinoHA.h>
#include <Arduino_JSON.h>
#include <DHT.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <ir_Trotec.h>
#include <WiFiClient.h>
#include <WiFiManager.h>

#include "keys.h"

#define BROKER_ADDR IPAddress(192, 168, 1, 101)

WiFiManager wifiManager;

const uint16_t kIrLed = D1;    // IR Out Pin
const uint16_t kRecvPin = D4;  // IR In Pin
const uint16_t dhtVcc = D0;    // DHT-22 VCC (Since it is unstable and needs restarts)
const uint16_t dhtData = D3;   // DHT-22 Data Pin

const uint16_t ledPin = D5;
const uint16_t photoPin = A0;

#define UPDATE_SPEED 5

IRTrotec3550 ac(kIrLed);  // Set the GPIO to be used for sending messages.
IRrecv irrecv(kRecvPin, 1024, 15, true);
decode_results results;  // Somewhere to store IR decoding results

#define DHTTYPE DHT22
DHT dht(dhtData, DHTTYPE);

long nextDataUpdate = 0;
long irDebounce = 0;

WiFiClient client;
HADevice device;
HAMqtt mqtt(client, device);

// By default HAHVAC supports only reporting of the temperature.
// You can enable feature you need using the second argument of the constructor.
// Please check the documentation of the HAHVAC class.
HAHVAC hvac(
  "Portable_AC",
  HAHVAC::TargetTemperatureFeature | HAHVAC::PowerFeature | HAHVAC::ModesFeature | HAHVAC::SwingFeature | HAHVAC::FanFeature,
  HASensorNumber::PrecisionP1);

// Make temperature and humidity sensors to keep track of things. Need temp (additionally since it rounds to int for some reason from hvac)
HASensorNumber temperatureSensor("AC_Temperature_Sensor", HASensorNumber::PrecisionP1);
HASensorNumber humiditySensor("AC_Humidity_Sensor", HASensorNumber::PrecisionP1);

// Make a sensor to keep track of the ambient light status
HASensorNumber lightSensor("LivingRoom_Light_Sensor", HASensorNumber::PrecisionP1);

Adafruit_NeoPixel pixels(1, ledPin, NEO_GRB + NEO_KHZ800);
long nextLockUpdate = 0;
long ledTimer = 0;

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

#define MAX_BRIGHTNESS 180
struct Color current = { r: 0, g: 0, b: 0 };
struct Color target = { r: 0, g: 0, b: 0 };
int lastBrightness = 0;

void setup() {
  Serial.begin(115200);

  Serial.println("Starting...");

  // connect to wifi
  wifiManager.autoConnect("ESP-AC");

  // Set up IR
  irrecv.enableIRIn();

  // Set up status led
  pixels.begin();

  // Set up photoresistor
  pinMode(photoPin, INPUT);

  // Set up temp/humidity sensor
  pinMode(dhtVcc, OUTPUT);
  DHT_restart();
  dht.begin();

  // Set default state for AC (IR data)
  ac.begin();

  ac.setMode(kTrotecCool);
  ac.setSwingV(true);
  ac.setPower(false);
  ac.setFan(kTrotecFanHigh);
  ac.setTemp(62, false);

  // Unique ID must be set!
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Add device availability to HA
  device.enableSharedAvailability();
  device.enableLastWill();

  // set device's mqtt details (optional)
  device.setName("Portable Air Conditioner");
  device.setModel("FP10234US-WH");
  device.setManufacturer("Costway");
  device.setSoftwareVersion("1.0.0");

  // assign callbacks (optional)
  hvac.onTargetTemperatureCommand(onTargetTemperatureCommand);
  hvac.onPowerCommand(onPowerCommand);
  hvac.onModeCommand(onModeCommand);
  hvac.onFanModeCommand(onFanModeCommand);
  hvac.onSwingModeCommand(onSwingModeCommand);

  // configure HVAC (optional)
  hvac.setName("Air Conditioner");
  hvac.setMinTemp(62);
  hvac.setMaxTemp(86);
  hvac.setTempStep(1);
  hvac.setTemperatureUnit(HAHVAC::FahrenheitUnit);

  // You can choose which modes should be available in the HA panel
  hvac.setModes(HAHVAC::OffMode | HAHVAC::CoolMode | HAHVAC::DryMode | HAHVAC::FanOnlyMode);
  hvac.setFanModes(HAHVAC::FanMode::LowFanMode | HAHVAC::FanMode::MediumFanMode | HAHVAC::FanMode::HighFanMode);
  hvac.setSwingModes(HAHVAC::SwingMode::OnSwingMode | HAHVAC::SwingMode::OffSwingMode);

  humiditySensor.setName("AC_Humidity");
  humiditySensor.setIcon("mdi:water-percent");
  humiditySensor.setUnitOfMeasurement("%");

  temperatureSensor.setName("AC_Temperature");
  temperatureSensor.setIcon("mdi:thermometer");
  temperatureSensor.setUnitOfMeasurement("°F");

  lightSensor.setName("Ambient_Light");
  lightSensor.setIcon("mdi:brightness-5");
  lightSensor.setUnitOfMeasurement("%");

  mqtt.begin(BROKER_ADDR);

  onAcUpdate("Init");

  // Mqtt loop takes a moment to establish connection - don't handle IR results for a moment after boot
  irDebounce = millis() + 2000;
}

void loop() {
  mqtt.loop();

  if (irrecv.decode(&results)) {
    // Serial.println("Got IR");
    if (millis() > irDebounce && (results.decode_type == decode_type_t::TROTEC || results.decode_type == decode_type_t::TROTEC_3550)) {
      ac.setRaw(results.state);
      onAcUpdate("IR Decode");

      updateMqtt();
    }
  }

  if (millis() > nextDataUpdate) {
    updateDHT();

    float brightness = analogRead(photoPin) / 1024. * 100;
    lightSensor.setValue(brightness);

    nextDataUpdate = millis() + 2000;
  }

  if (millis() > nextLockUpdate) {
    updateLockStatus();
    nextLockUpdate = millis() + 5000;
  }

  if (millis() > ledTimer) {
    updateLed();
    ledTimer = millis() + UPDATE_SPEED;
  }

}

/** Utils */

void updateDHT() {
  float temperature = dht.readTemperature(true); // Read temperature and convert to Farenheit
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    DHT_restart();
  } else {
    hvac.setCurrentTemperature(temperature);
    humiditySensor.setValue(humidity);
    temperatureSensor.setValue(temperature);
  }
}

uint8_t counter = 0;
void updateLed() {
  if (current.r > target.r) current.r--;
  if (current.r < target.r) current.r++;

  if (current.g > target.g) current.g--;
  if (current.g < target.g) current.g++;

  if (current.b > target.b) current.b--;
  if (current.b < target.b) current.b++;

  pixels.setPixelColor(0, current.r, current.g, current.b);

  uint16_t brightnessReading = analogRead(photoPin);
  // int brightness = map(brightnessReading, 0, 1024, 1, 250);

  // Ease in-out for range 0-511, any reading from 512-1024 is set to MAX_BRIGHTNESS
  int newBrightness = -(cos(3.1415 * brightnessReading / 512.) - 1) / 2. * MAX_BRIGHTNESS;
  if (brightnessReading >= 512) newBrightness = MAX_BRIGHTNESS;

  int brightness = 0.9 * lastBrightness + 0.1 * newBrightness;
  brightness = max(1, brightness);
  lastBrightness = brightness;

  pixels.setBrightness(brightness);

  counter++;
  if (counter > 50) {
    Serial.print("reading:");
    Serial.print(brightnessReading);
    Serial.print(",new:");
    Serial.print(newBrightness);
    Serial.print(",weighted:");
    Serial.println(brightness);

    counter = 0;
  }

  pixels.show();
}

void updateLockStatus() {
  bool locked = getHaEntityState(HA_ENDPOINT);

  Serial.print("Locked?: ");
  Serial.println(locked);

  target.r = locked ? 255 : 0;
  target.g = locked ? 0   : 255;
  target.b = 0;
}

bool getHaEntityState(String url) {
  WiFiClient client;
  HTTPClient http;

  // Your IP address with path or Domain name with URL path
  http.begin(client, url);
  http.addHeader("Authorization", HA_API_KEY);

  int responseCode = http.GET();
  if (responseCode == 200) {
    JSONVar sensor = JSON.parse(http.getString());
    String state = sensor["state"];
    http.end();

    return state == "on";
  } else {
    Serial.print("Something went wrong: ");
    Serial.println(responseCode);
  }

  http.end();
  return false;
}

void updateMqtt() {
  hvac.setTargetTemperature(ac.getTemp());
  hvac.setSwingMode(ac.getSwingV() ? HAHVAC::SwingMode::OnSwingMode : HAHVAC::SwingMode::OffSwingMode);

  if (ac.getFan() == kTrotecFanLow) hvac.setFanMode(HAHVAC::LowFanMode);
  if (ac.getFan() == kTrotecFanMed) hvac.setFanMode(HAHVAC::MediumFanMode);
  if (ac.getFan() == kTrotecFanHigh) hvac.setFanMode(HAHVAC::HighFanMode);

  if (ac.getMode() == kTrotecCool) hvac.setMode(HAHVAC::Mode::CoolMode);
  if (ac.getMode() == kTrotecDry) hvac.setMode(HAHVAC::Mode::DryMode);
  if (ac.getMode() == kTrotecFan) hvac.setMode(HAHVAC::Mode::FanOnlyMode);
  if (!ac.getPower()) hvac.setMode(HAHVAC::OffMode);
}

// Physically power recycle the DHT using a relay/GPIO
void DHT_restart() {
  Serial.println("Restarting DHT");

  digitalWrite(dhtVcc, LOW);
  digitalWrite(dhtData, LOW);

  // Delay is needed before sampling begins. A cycle of sampling is skipped
  // to prevent hiccups by halting the processor.
  delay(1);

  digitalWrite(dhtVcc, HIGH);
  digitalWrite(dhtData, HIGH);
}

void onAcUpdate(String debugInfo) {
  Serial.print("AC state updated: [");
  Serial.print(debugInfo);
  Serial.print("] ");
  Serial.println(ac.toString());

  ac.send();

  irDebounce = millis() + 500;
}

/** Callbacks */

void onTargetTemperatureCommand(HANumeric temperature, HAHVAC* sender) {
  uint8_t temp = (uint8_t) temperature.toFloat();

  ac.setTemp(temp, false);
  ac.setPower(true);

  onAcUpdate("onTargetTemperatureCommand");
  sender->setTargetTemperature(temperature);  // report target temperature back to the HA panel
  sender->setMode(HAHVAC::Mode::CoolMode);    // also turn on power for HA
}

void onPowerCommand(bool state, HAHVAC* sender) {
  ac.setPower(state);
  onAcUpdate("onPowerCommand");

  if (state) sender->setMode(HAHVAC::CoolMode);
  else sender->setMode(HAHVAC::OffMode);
}

void onModeCommand(HAHVAC::Mode mode, HAHVAC* sender) {
  if (mode == HAHVAC::Mode::OffMode) ac.setPower(false);
  if (mode == HAHVAC::Mode::CoolMode) ac.setMode(kTrotecCool);
  if (mode == HAHVAC::Mode::DryMode) ac.setMode(kTrotecDry);
  if (mode == HAHVAC::Mode::FanOnlyMode) ac.setMode(kTrotecFan);

  onAcUpdate("onModeCommand");
  sender->setMode(mode);  // report mode back to the HA panel
}

void onFanModeCommand(HAHVAC::FanMode mode, HAHVAC* sender) {
  if (mode == HAHVAC::FanMode::HighFanMode) ac.setFan(kTrotecFanHigh);
  if (mode == HAHVAC::FanMode::MediumFanMode) ac.setFan(kTrotecFanMed);
  if (mode == HAHVAC::FanMode::LowFanMode) ac.setFan(kTrotecFanLow);

  onAcUpdate("onFanModeCommand");
  sender->setFanMode(mode);  // report mode back to the HA panel
}

void onSwingModeCommand(HAHVAC::SwingMode mode, HAHVAC* sender) {
  if (mode == HAHVAC::SwingMode::OffSwingMode) ac.setSwingV(false);
  if (mode == HAHVAC::SwingMode::OnSwingMode) ac.setSwingV(true);

  onAcUpdate("onSwingModeCommand");
  sender->setSwingMode(mode);
}
