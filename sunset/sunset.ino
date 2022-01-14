#include <ESP8266WiFi.h> 
#include <ezTime.h>

#include "config.h"

// Constants that need to be configured
#define LED D1
#define TIMEZONE "America/Los_Angeles"

// Debug vars
#define DEBUG

#ifdef DEBUG
  #define MAX_BRIGHTNESS 150
#else
  #define MAX_BRIGHTNESS 1023
#endif

// Internal State
int brightness = 0;
int targetBrightness = 0;

int fadeDuration = 30 * 60 * 1000;
int fadeSegment = fadeDuration / MAX_BRIGHTNESS;
long timer;

Timezone tz;

// Setup functions

void setup() {
  Serial.begin(115200);

  setupPins();
  setupWifi();
  setupNtp();

  Serial.println("---------------------");
  scheduleEvents();
}

void setupPins() {
  pinMode(LED, OUTPUT);
  analogWriteRange(1024);

  targetBrightness = MAX_BRIGHTNESS;
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

// Util functions

// Runs every day at midnight, also schedules a new event at midnight
void scheduleEvents() {


//  time_t next = nextDay();
  time_t next = nextMin();
  setEvent(scheduleEvents, next);

  Serial.println("Event added: SecheduleEvents [" + dateTime(next, RSS) + "]");
}

time_t nextMin() {
  // Get current time  
  tmElements_t tm;
  breakTime(tz.now(), tm);

  // Add on a day to this (the library nicely does proper rolling over math, except maybe for new year?)
  time_t next = makeTime(tm.Hour, tm.Minute, tm.Second + 5, tm.Day, tm.Month, tm.Year);
  return next;
}

time_t nextDay() {
  // Get current time  
  tmElements_t tm;
  breakTime(tz.now(), tm);

  // Add on a day to this (the library nicely does proper rolling over math, except maybe for new year?)
  time_t next = makeTime(0, 0, 0, tm.Day + 1, tm.Month, tm.Year);
  return next;
}
