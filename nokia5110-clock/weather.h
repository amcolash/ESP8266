#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "keys.h"

WiFiClient client;  // or WiFiClientSecure for HTTPS
HTTPClient http;

int ERROR = -999;

int getWeather() {
  // Send request
  http.useHTTP10(true);
  http.begin(client, HA_URL);
  http.addHeader("content-type", "application/json");
  http.addHeader("Authorization", HA_API_KEY);
  int status = http.GET();

  if (status == 200) {
    // Parse response
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getStream());

    // Disconnect
    http.end();

    return (int) doc["state"].as<float>();
  } else {
    return ERROR;
  }
}