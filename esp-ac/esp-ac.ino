
#include <IRrecv.h>
#include <IRsend.h>
#include <ir_Trotec.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ArduinoHA.h>
#include <DHT.h>

#define BROKER_ADDR IPAddress(192, 168, 1, 101)

WiFiManager wifiManager;

const uint16_t kIrLed = D1;    // IR Out Pin
const uint16_t kRecvPin = D4;  // IR In Pin
const uint16_t dhtVcc = D0;    // DHT-22 VCC (Since it is unstable and needs restarts)
const uint16_t dhtData = D3;   // DHT-22 Data Pin

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
  HAHVAC::TargetTemperatureFeature | HAHVAC::PowerFeature | HAHVAC::ModesFeature | HAHVAC::SwingFeature | HAHVAC::FanFeature);

HASensorNumber humiditySensor("Humidity_Sensor", HASensorNumber::PrecisionP1);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // connect to wifi
  wifiManager.autoConnect("ESP-AC");

  // Set up IR
  irrecv.enableIRIn();

  // Set up temp/humidity sensor
  pinMode(dhtVcc, OUTPUT);
  DHT_restart();
  dht.begin();

  // Set default state for AC (IR data)
  ac.begin();

  ac.setMode(kTrotecCool);
  ac.setSwingV(true);
  ac.setPower(false);
  ac.setFan(kTrotecFanLow);
  ac.setTemp(69, false);

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

  humiditySensor.setName("Humidity");
  humiditySensor.setIcon("mdi:water-percent");
  humiditySensor.setUnitOfMeasurement("%");

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
    float temperature = (dht.readTemperature() * (9 / 5.)) + 32;
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      DHT_restart();
    } else {
      hvac.setCurrentTemperature(temperature);
      humiditySensor.setValue(humidity);
    }

    nextDataUpdate = millis() + 2000;
  }
}

/** Utils */

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
  delay(1);

  digitalWrite(dhtVcc, HIGH);
  digitalWrite(dhtData, HIGH);

  // Delay is needed before sampling begins. A cycle of sampling is skipped
  // to prevent hiccups by halting the processor.
}

void onAcUpdate(char* debugInfo) {
  Serial.print("AC state updated: [");
  Serial.print(debugInfo);
  Serial.print("] ");
  Serial.println(ac.toString());

  ac.send();

  irDebounce = millis() + 100;
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