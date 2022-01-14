#include <ESP8266WiFi.h> 
#include <ezTime.h>

#include "config.h"
#include "sundata.h"
#include "util.h"

// Constants that need to be configured
#define LED D1
#define TIMEZONE "America/Los_Angeles"

// Setup functions

void setup() {
  Serial.begin(115200);
  Serial.println();

  setupPins();
  setupWifi();
  setupNtp();

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

// Loop functions

void loop() {
  // Check ezTime events (including ntp sync)
  events();
  
  if (millis() - timer > fadeSegment) {
    targetBrightness = constrain(targetBrightness, 0, MAX_BRIGHTNESS);
    
    if (brightness < targetBrightness) brightness++;
    else if (brightness > targetBrightness) brightness--;
    
    analogWrite(LED, brightness);

    timer = millis();
  }
}
