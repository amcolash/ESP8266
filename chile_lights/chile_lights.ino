#include <ArduinoHA.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#define IN1_PIN         D0
#define IN2_PIN         D5

#define BROKER_ADDR     IPAddress(192, 168, 1, 101)

WiFiManager wifiManager;
WiFiClient client;

HADevice device;
HAMqtt mqtt(client, device);

HALight light("ChileLights", HALight::BrightnessFeature);

// count how far through pwm cycle the ISR is
volatile uint8_t pwmCounter = 0;
// whether to set IN1 or IN2, flip the other
volatile bool flip = false;

// if the light state is on or off
volatile bool on = true;

// current value shown on leds
volatile uint8_t current = 255;
// target brightness and value reported to HA
uint8_t brightness = 255;

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

// Flip light pin output to work with motor driver. Cannot use analogWrite for pwm, since
// we already need to use Timer1 for this interrupt. Therefore, we roll our own pwm as well.
void IRAM_ATTR flipLights() {
  pwmCounter++;

  // If "active" side of pwm, go high
  if (pwmCounter < current) {
    if (flip) {
      digitalWrite(IN1_PIN, HIGH);
      digitalWrite(IN2_PIN, LOW);
    } else {
      digitalWrite(IN1_PIN, LOW);
      digitalWrite(IN2_PIN, HIGH);
    }

    // next cycle will be flipped from this one
    flip = !flip;
  } else {
    // Low at end of cycle
    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);
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

  // set up Timer1 ISR to flip light output back and forth
  timer1_attachInterrupt(flipLights);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
  timer1_write(100);

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

  // begin mqtt broker
  mqtt.begin(BROKER_ADDR);

  // wait until mqtt has connected
  while (!mqtt.isConnected()) {
    mqtt.loop();  // Process MQTT connection
    delay(100);
  }

  // force the state of the lights to default values
  light.setState(on);
  light.setBrightness(brightness);
}

uint8_t counter = 0;

void loop() {
  mqtt.loop();

  // Counter limits the speed of fading
  if (counter == 0) {
    // If turning off, slowly fade off
    if (!on) {
       if (current > 0) current--;
    }
    // If normal, slowly moving current brightness closer to target
    else if (current < brightness) current++;
    else if (current > brightness) current--;
  }

  // count up slowly to make fading more gradual
  counter = (counter + 1) % 127;
}