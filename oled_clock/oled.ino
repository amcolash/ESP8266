#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>

// Required for LIGHT_SLEEP_T delay mode
extern "C" {
  #include "user_interface.h"
}

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

const char *ssid     = "mcolash";
const char *password = "candyraisins";

const long utcOffsetInSeconds = - 60 * 60 * 6;
const long updateInterval = 10 * 60 * 1000;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, updateInterval);

// ----------------   Helpers from: https://github.com/ThingPulse/esp8266-oled-ssd1306/blob/master/src/OLEDDisplay.cpp   -------------------

void setContrast(uint8_t contrast, uint8_t precharge, uint8_t comdetect) {
  display.ssd1306_command(SSD1306_SETPRECHARGE); //0xD9
  display.ssd1306_command(precharge); //0xF1 default, to lower the contrast, put 1-1F
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(contrast); // 0-255
  display.ssd1306_command(SSD1306_SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
  display.ssd1306_command(comdetect);  //0x40 default, to lower the contrast, put 0
  display.ssd1306_command(SSD1306_DISPLAYALLON_RESUME);
  display.ssd1306_command(SSD1306_NORMALDISPLAY);
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void setBrightness(uint8_t brightness) {
  uint8_t contrast = brightness;
  if (brightness < 128) {
    // Magic values to get a smooth/ step-free transition
    contrast = brightness * 1.171;
  } else {
    contrast = brightness * 1.171 - 43;
  }

  uint8_t precharge = 241;
  if (brightness == 0) {
    precharge = 0;
  }
  uint8_t comdetect = brightness / 8;

  setContrast(contrast, precharge, comdetect);
}
// ---------------------- End Brightness -------------------


void setup()   {
  Serial.begin(115200);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print ( "." );
  }
  Serial.println(" Connected");

  timeClient.begin();
  timeClient.update();

  // start display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // from 0-128
  setBrightness(0);
    
  display.clearDisplay();
  display.display();

  display.setTextSize(2);
  display.setTextColor(WHITE);
}

int counter = 10000;
int lastMin = -1;

void loop() {
  counter++;

  if (counter > 10000) {
    Serial.println("Update NTP");
    wifi_set_sleep_type(NONE_SLEEP_T);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print (".");
    }
    
    timeClient.update();
    wifi_set_sleep_type(LIGHT_SLEEP_T);

    counter = 0;
  }

  int minutes = timeClient.getMinutes();
  if (minutes == lastMin) {
    delay(500);
    return;
  }
  lastMin = minutes;
  
  display.clearDisplay();

  display.setCursor(0,8);

  int hour = timeClient.getHours();
  
  time_t rawtime = timeClient.getEpochTime();
  struct tm * ti;
  ti = localtime (&rawtime);

  uint8_t month = ti->tm_mon + 1;
  uint8_t day = ti->tm_mday;
  
  if (hour > 12) hour -= 12;
  if (hour == 0) hour = 12;

  if (hour < 10) display.print(" ");
  display.print(hour);
  display.print(":");
  if (minutes < 10) display.print("0");
  display.println(minutes);

  display.setCursor(display.getCursorX(), display.getCursorY() + 8);

  display.print(month);
  display.print("/");
  display.print(day);

  display.display();
}
