#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Wire.h>

#include "key.h"

uint8_t ledAddress = 0x03;
ESP8266WebServer server(80);

// Main Code
void setup() {
  Serial.begin(19200);

  Wire.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to wifi");
  Serial.println(WiFi.localIP());

  server.on("/mode", setMode);
  server.on("/brightness", setBrightness);
  server.on("/off", turnOff);
  server.on("/on", turnOn);
  
  server.begin(); //Start the server
}

void loop() {
  server.handleClient();
}

// Server handlers
void setMode() {
  int16_t value = getArg("value");
  
  if (value != -1) {
    send(0x00, value);
    success();
  } else {
    server.send(400, "text/plain", "Invalid parameters passed");
  }
}

void setBrightness() {
  int16_t value = getArg("value");
  
  if (value != -1) {
    send(0x02, value);
    success();
  } else {
    server.send(400, "text/plain", "Invalid parameters passed");
  }
}

void turnOff() {
  send(0x01, 0x00);
  success();
}

void turnOn() {
  send(0x01, 0x01);
  success();
}


// Utils
void send(uint8_t address, uint8_t value) {
  Wire.beginTransmission(ledAddress);
  Wire.write(address);
  Wire.write(value);
  Wire.endTransmission();
}

int16_t getArg(String arg) {
  for (int i = 0; i < server.args(); i++) {
    int value = server.arg(i).toInt();
    if (server.argName(i) == arg && value >= 0 && value <= 255) {
      return value;
    }
  }

  return -1;
}

void success() {
  server.send(200, "text/plain", "Ok");
}
