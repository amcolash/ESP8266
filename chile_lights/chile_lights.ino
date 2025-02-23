#include <ArduinoHA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#define IN1_PIN         D0
#define IN2_PIN         D5

#define BROKER_ADDR     IPAddress(192, 168, 1, 101)

uint16_t count = 0;
bool flip = false;

WiFiManager wifiManager;
WiFiClient client;

HADevice device;
HAMqtt mqtt(client, device);

HALight light("ChileLights", HALight::BrightnessFeature);

volatile bool on = false;
volatile uint8_t brightness = 1;

void onStateCommand(bool state, HALight* sender) {
  on = state;
  sender->setState(state); // report state back to the Home Assistant

  Serial.print("state: ");
  Serial.println(state);
}

void onBrightnessCommand(uint8_t newBrightness, HALight* sender) {
  brightness = newBrightness;
  sender->setBrightness(newBrightness); // report brightness back to the Home Assistant

  Serial.print("brightness: ");
  Serial.println(newBrightness);
}

void flipLights() {
  if (on) {
    if (flip) {
      analogWrite(IN1_PIN, brightness);
      analogWrite(IN2_PIN, 0);
    } else {
      analogWrite(IN1_PIN, 0);
      analogWrite(IN2_PIN, brightness);
    }

    flip = !flip;
  } else {
    analogWrite(IN1_PIN, 0);
    analogWrite(IN2_PIN, 0);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // Setup hardware
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);

  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);

  // connect to wifi
  wifiManager.autoConnect("ESP-ChileLights");

  // Unique ID must be set!
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Add device availability to HA
  device.enableSharedAvailability();
  device.enableLastWill();

  // set device's details (optional)
  device.setName("Chile Lights");
  device.setSoftwareVersion("1.0.0");

  // handle light states
  light.setName("Chile Lights");
  light.setBrightnessScale(255);
  light.onStateCommand(onStateCommand);
  light.onBrightnessCommand(onBrightnessCommand); // optional

  mqtt.begin(BROKER_ADDR);
}

void loop() {
  mqtt.loop();

  flipLights();
}