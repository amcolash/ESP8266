#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include "font.h"

#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <SPI.h>
#include <time.h>
#include <Timezone.h>
#include <WiFiUdp.h>

#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define SCLK D5
#define DIN D7
#define DC D1
#define CE D2
#define RST D3
#define BL D0

Adafruit_PCD8544 display = Adafruit_PCD8544(SCLK, DIN, DC, CE, RST);

const unsigned char wifiIcon [] PROGMEM = {
  0x03, 0xc0, 0x1f, 0xf8, 0x78, 0x1e, 0xf0, 0x07, 0xcf, 0xf3, 0x1f, 0xf8, 0x38, 0x1c, 0x13, 0xc8, 
  0x07, 0xe0, 0x06, 0x60, 0x00, 0x00, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x00, 0x00
};

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Timezone data from https://github.com/JChristensen/Timezone/blob/master/examples/WorldClock/WorldClock.ino
// US Pacific Time Zone (Las Vegas, Los Angeles, Seattle)
TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -420};
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -480};
Timezone timezone(usPDT, usPST);

void setup()   {
  Serial.begin(115200);

  setupDisplay();

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wm;

  bool res = wm.autoConnect("ESPClock"); // password protected ap
  if(!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  }

  timeClient.begin();
  timeClient.update();
}

int scale = 2;
void setupDisplay() {
  // start display
  display.begin();

  // set contrast for display from 0-100 
  display.setContrast(55);

  // set brightness with PWM (0-1024)
  analogWrite(BL, 500);

  // Flip the display upside-down
  display.setRotation(2);
    
  display.clearDisplay();

  display.setTextColor(BLACK);
  display.setTextWrap(false);
  display.setFont(&Orbitron_Bold_22);

  display.drawBitmap(display.width() / 2 - 4, display.height() / 2 - 8, wifiIcon, 16, 15, BLACK);
  display.display();
}

void drawCentredString(const String &buf, int x, int y) {
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buf, x, y, &x1, &y1, &w, &h); //calc width of new string
    display.setCursor(x - w / 2, y - h / 2);
    display.print(buf);
}

time_t rawtime;
int minuteNum,hourNum, monthNum, dayNum;
int vOffset = 6;

void loop() {
  timeClient.update();
  
  rawtime = timeClient.getEpochTime();
  rawtime = timezone.toLocal(rawtime);

  // Since all of these variables are already functions, fancy variable names
  minuteNum = minute(rawtime);
  hourNum = hourFormat12(rawtime);
  monthNum = month(rawtime);
  dayNum = day(rawtime);

  display.clearDisplay();

  String time = String(String(hourNum) + ":" + String(minuteNum < 10 ? "0" + String(minuteNum) : String(minuteNum)));
  drawCentredString(time, display.width() / 2, display.height() / 2 + vOffset);

  String date = String(String(monthNum) + "/" + String(dayNum));
  drawCentredString(date, display.width() / 2, display.height() / 2 + 20 + vOffset);

  display.display();

  delay(1000);
}
