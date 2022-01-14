#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h> 
#include <ezTime.h>

#include "config.h"
#include "sundata.h"
#include "util.h"

// Constants that need to be configured
#define LED D1
#define TIMEZONE "America/Los_Angeles"

ESP8266WebServer server(80);


// Setup functions

void setup() {
  Serial.begin(115200);
  Serial.println();

  setupPins();
  setupWifi();
  setupNtp();
  setupServer();

  Serial.println("---------------------");
  scheduleEvents();
}

void setupPins() {
  pinMode(LED, OUTPUT);
  analogWriteRange(1024);
}

void setupWifi() {
  Serial.println("Connecting to: " + String(ssid));

  WiFi.mode(WIFI_STA); //We don't want the ESP to act as an AP
  WiFi.begin(ssid, password); // Connect to the configured AP

  // No point moving forward if we don't have a connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // We have connected to wifi successfully
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupNtp() {
  Serial.println("Syncing clock with ntp");

  // sync ntp
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());

  // set up default timezone
  tz.setLocation(TIMEZONE);
  tz.setDefault();
  
  Serial.println("Default time: " + dateTime());
}

void setupServer() {
  server.on("/off", turnOff);
  server.on("/on", turnOn);
  server.on("/brightness", setBrightness);
  
  server.begin();
}


// Loop functions

void loop() {
  // Check ezTime events (including ntp sync)
  events();

  // Handle server as needed
  server.handleClient();

  if (millis() - timer > fadeSegment) {
    targetBrightness = constrain(targetBrightness, 0, MAX_BRIGHTNESS);
    
    if (brightness < targetBrightness) brightness++;
    else if (brightness > targetBrightness) brightness--;

    timer = millis();
  }

  analogWrite(LED, on ? brightness : 0);
}


// Server functions

void turnOn() {
  on = true;
  success();
}

void turnOff() {
  on = false;
  success();
}

void setBrightness() {
  int16_t value = getArg("value");
  
  if (value != -1) {
    brightness = value;
    targetBrightness = value;
    
    success();
  } else {
    server.send(400, "text/plain", "Invalid parameters passed");
  }
}

int16_t getArg(String arg) {
  for (int i = 0; i < server.args(); i++) {
    int value = server.arg(i).toInt();
    if (server.argName(i) == arg && value >= 0 && value <= 1023) {
      return value;
    }
  }

  return -1;
}

void success() {
  server.send(200, "text/plain", "Ok");
}
