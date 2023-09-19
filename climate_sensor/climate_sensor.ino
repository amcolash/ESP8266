#include <ArduinoHA.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#define BROKER_ADDR IPAddress(192, 168, 1, 101)

#define ID "1"
#define CONCAT(a) (a "" ID)

#define DEVICE_NAME CONCAT("Climate Station ")
#define HUMIDITY_ID CONCAT("Climate_Humidity_Sensor_")
#define TEMPERATURE_ID CONCAT("Climate_Temperature_Sensor_")

#define DHT_VCC D1
#define DHT_DATA D2
#define DHT_TYPE DHT22   // DHT 22  (AM2302), AM2321

DHT dht(DHT_DATA, DHT_TYPE);

WiFiManager wifiManager;
WiFiClient client;

HADevice device;
HAMqtt mqtt(client, device);

HASensorNumber humiditySensor(HUMIDITY_ID, HASensorNumber::PrecisionP1);
HASensorNumber temperatureSensor(TEMPERATURE_ID, HASensorNumber::PrecisionP1);

long nextUpdate = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");

  // connect to wifi
  wifiManager.autoConnect("ESP-Climate");

  // Set up DHT22
  pinMode(DHT_VCC, OUTPUT);
  dhtReset();

  // Unique ID must be set!
  byte mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  device.setUniqueId(mac, sizeof(mac));

  // Add device availability to HA
  device.enableSharedAvailability();
  device.enableLastWill();

  // set device's mqtt details (optional)
  device.setName(DEVICE_NAME);
  device.setSoftwareVersion("1.0.0");

  humiditySensor.setName(HUMIDITY_ID);
  humiditySensor.setIcon("mdi:water-percent");
  humiditySensor.setUnitOfMeasurement("%");

  temperatureSensor.setName(TEMPERATURE_ID);
  temperatureSensor.setIcon("mdi:thermometer");
  temperatureSensor.setUnitOfMeasurement("°F");

  mqtt.begin(BROKER_ADDR);

  dht.begin();
  nextUpdate = millis() + 2000;
}

void dhtReset() {
  Serial.println("Reset DHT");
  digitalWrite(DHT_VCC, false);
  delay(10);
  digitalWrite(DHT_VCC, true);
}

void loop() {
  mqtt.loop();

  if (millis() > nextUpdate) {
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(true); // The `true` converts the value to Farenheit

    // If the reading was invalid, try to reset device
    if (isnan(humidity) || isnan(temperature)) {
      dhtReset();
    } else {
      humiditySensor.setValue(humidity);
      temperatureSensor.setValue(temperature);
    }

    nextUpdate = millis() + 2000;
  }
}
