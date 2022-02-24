#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h> 
#include <ezTime.h>

#include "LittleFS.h"

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
  delay(100);
  Serial.println();

  setupFS();
  
  log("\n---------------------\n");

  setupPins();
  setupWifi();
  setupNtp();
  setupServer();

  scheduleEvents();
}

void setupFS() {
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting Little File System - reformatting");
    LittleFS.format();
    return;
  }

  // Rotate log file at 50kb
  File f = LittleFS.open(logPath, "r");
  if (f && f.size() > 50 * 1000) {
    f.close();
    LittleFS.remove(logPath);
    log("Rotated Log File");
  }
}

void setupPins() {
  pinMode(LED, OUTPUT);
  analogWriteRange(1024);
}

void setupWifi() {
  log("Connecting to: " + String(ssid));

  WiFi.mode(WIFI_STA); //We don't want the ESP to act as an AP
  WiFi.begin(ssid, password); // Connect to the configured AP

  // No point moving forward if we don't have a connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  // We have connected to wifi successfully
  log("WiFi connected");
  log("IP address: " + WiFi.localIP().toString());
}

void setupNtp() {
  log("Syncing clock with ntp");

  // sync ntp
  waitForSync();
  log("UTC: " + UTC.dateTime());

  // set up default timezone
  tz.setLocation(TIMEZONE);
  tz.setDefault();
  
  log("Default time: " + dateTime());
}

void setupServer() {
  server.on("/off", turnOff);
  server.on("/on", turnOn);
  server.on("/brightness", handleBrightness);
  server.on("/log", getLog);
  
  server.begin();
}


// Loop functions

void loop() {
  // Check ezTime events (including ntp sync)
  events();

  // Handle server as needed
  server.handleClient();

  if (millis() - timer > (fastFade ? 5 : fadeSegment)) {
    targetBrightness = constrain(targetBrightness, 0, MAX_BRIGHTNESS);
    
    if (brightness < targetBrightness) brightness++;
    else if (brightness > targetBrightness) brightness--;

    timer = millis();
  }

  if (brightness == targetBrightness) fastFade = false;

  analogWrite(LED, on ? brightness : 0);
}


// Server functions

void turnOn() {
  on = true;
  log("Turning on");
  success();
}

void turnOff() {
  on = false;
  log("Turning off");
  success();
}


void handleBrightness() {
  int16_t value = getArg("value");
  
  if (value != -1) {
    on = true;
    fastFade = true;
    
    targetBrightness = value;

    log("Setting brightness to: " + String(value));
    
    success();
  } else {
    server.send(200, "application/json", "{ \"brightness\": " + String(brightness) + ",\"targetBrightness\": " + String(targetBrightness) + " }");
  }
}

bool getLog() {
  File f = LittleFS.open(logPath, "r");

  if (f) {
    server.streamFile(f, "text/plain");
    f.close();
  }

  return true;
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
