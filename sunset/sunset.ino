#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h> 
#include <WiFiManager.h>
#include <ezTime.h>

#include "ClickButton.h"
#include "LittleFS.h"

#include "sundata.h"
#include "util.h"

// Constants that need to be configured
#define LED D1
#define BUTTON D2
#define TIMEZONE "America/Los_Angeles"

ESP8266WebServer server(80);
ClickButton button(BUTTON, LOW, CLICKBTN_PULLUP);

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
  log("Mounting filesystem");
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting Little File System - reformatting");
    LittleFS.format();
    return;
  }

  // Rotate log file at 50kb
  File f = LittleFS.open(logPath, "r");
  if (f && f.size() > 50 * 1000) rotateLog();
  f.close();

  // Load in persisted data
  f = LittleFS.open(dataPath, "r");
  if (f) f.read((byte *)&data, sizeof(data));
  f.close();
}

void setupPins() {
  log("Setting up pins");
  pinMode(LED, OUTPUT);
  analogWriteRange(1024);
}

void setupWifi() {
  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  bool res = wm.autoConnect("ESPSunset"); // password protected ap
  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  }

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
  server.on("/toggle", toggleHandler);
  server.on("/alarm", setAlarm);
  server.on("/restart", restart);
  
  server.begin();
}


// Loop functions

void loop() {
  // Check ezTime events (including ntp sync)
  events();

  // Handle server as needed
  server.handleClient();

  // Check button for updates
  updateButtons(); 

  if (millis() - timer > (fastFade ? 2 : fadeSegment)) {
    targetBrightness = constrain(targetBrightness, 0, MAX_BRIGHTNESS);
    
    if (brightness < targetBrightness) brightness++;
    else if (brightness > targetBrightness) brightness--;

    timer = millis();
  }

  if (brightness == targetBrightness) fastFade = false;
  analogWrite(LED, on ? brightness : 0);
}

void updateButtons() {
  button.Update();
  if (button.clicks == 1) toggle();
}


// Server functions

void turnOn() {
  on = true;
  success();
  log("Turning on");
}

void turnOff() {
  on = false;
  success();
  log("Turning off");
}

void toggleHandler() {
  toggle();
  success();
  log("Toggling state, new value: " + String(targetBrightness));
}

void handleBrightness() {
  int16_t value = getArg("value");
  
  if (value != -1) {
    on = true;
    fastFade = true;
    targetBrightness = value;

    success();

    log("Setting brightness to: " + String(value));
  } else {
    server.send(200, "application/json", "{ \"brightness\": " + String(brightness) + ",\"targetBrightness\": " + String(targetBrightness) + " }");
  }
}

bool getLog() {
  if (getArg("reset") != -1) rotateLog(); 
  
  File f = LittleFS.open(logPath, "r");

  if (f) {
    server.streamFile(f, "text/plain");
    f.close();
  }

  return true;
}

void setAlarm() {
  int16_t hour = getArg("hour");
  int16_t minute = getArg("minute");

  if (hour != -1 && minute != -1) {
    data.alarmHour = hour;
    data.alarmMinute = minute;

    server.send(200, "application/json", "{ \"alarmHour\": " + String(data.alarmHour) + ",\"alarmMinute\": " + String(data.alarmMinute) + " }");

    File f = LittleFS.open(dataPath, "w");
    f.write((byte *)&data, sizeof(data));
    f.close();

    log("Set alarm to " + String(data.alarmHour) + ":" + String(data.alarmMinute));

    // Reschedule events because alarm might change sunrise time
    scheduleEvents();
  } else {
    server.send(200, "application/json", "{ \"alarmHour\": " + String(data.alarmHour) + ",\"alarmMinute\": " + String(data.alarmMinute) + " }");
  }
}

void restart() {
  success();
  ESP.restart();
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
