#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>

#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define double_buffer
#include <PxMatrix.h>
#include <Fonts/TomThumb.h>
#include <Ticker.h>

/************************* Constants / Globals *************************/
#include "key.h"

// NTP Client, -8 is w/o DST, -7 is with DST
const long utcOffsetInSeconds = -8 * 60 * 60;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, utcOffsetInSeconds);

// Open weather map
const String zip = "98103";
const String currentEndpoint = "http://api.openweathermap.org/data/2.5/weather?zip=" + zip + "&units=imperial&APPID=";
const String forecastEndpoint = "http://api.openweathermap.org/data/2.5/forecast/daily?zip=" + zip + "&units=imperial&APPID=";
HTTPClient http;

// Display setup
#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2

#define SCALE_FACTOR 12  // Direct scaling factor (raise for higher bars, lower for shorter bars)

// Pins for LED MATRIX
PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);

const uint16_t BLACK = display.color565(0, 0, 0);
const int LIME_HUE = 85;

// 'wifi_small', 16x15px
const unsigned char wifiIcon [] PROGMEM = {
  0x03, 0xc0, 0x1f, 0xf8, 0x78, 0x1e, 0xf0, 0x07, 0xcf, 0xf3, 0x1f, 0xf8, 0x38, 0x1c, 0x13, 0xc8, 
  0x07, 0xe0, 0x06, 0x60, 0x00, 0x00, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x00, 0x00
};

byte RedLight;
byte GreenLight;
byte BlueLight;

# define BARS 16
const int BAR_SIZE = 64 / BARS;

int8_t lines[BARS], lastLines[BARS];
int i, val, sum;
float avg;

const float positiveMixFactor = 0.2;
const float negativeMixFactor = 0.01;
char timeDisplay[10];
int temperature = -999;

// roughly 60 updates a second * 60 sec * 60 min (every hour)
const long updateTime = 60 * 60 * 60;
long updateCount = updateTime;

int brightness = 100;

/************************* Setup *************************/

Ticker display_ticker;
void display_updater() {
  display.display(30);
}

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println();

  // Set RX pin as standard GPIO
  pinMode(RX, FUNCTION_3);

  // Set up I2C for ads with custom pins
  Wire.begin(D3, RX); // SDA,SCL

  setupDisplay();
  setupWifi();
  setupNTP();

  // Zero out arrays (just in case)
  for (i=0; i < BARS; i++) { lines[i] = -1; };
  for (i=0; i < BARS; i++) { lastLines[i] = -1; };
}

void setupDisplay() {
  // Init display
  display.begin(16);
  display.flushDisplay();
  display.setFastUpdate(true);
  display.setTextWrap(false);
  display.setBrightness(brightness);
  
  // Paint to display 500x a second
  display_ticker.attach(0.002, display_updater);

  // Draw wifi connecting icon
  display.drawBitmap(24, 9, wifiIcon, 16, 15, getColor(LIME_HUE, 255, 255));
  display.showBuffer();
}

void setupWifi() {
  // Start up wifi
  Serial.print(F("\nConnecting to: "));
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); //We don't want the ESP to act as an AP
  WiFi.begin(ssid, pass); // Connect to the configured AP

  // No point moving forward if we don't have a connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  // We have connected to wifi successfully
  Serial.println(F("\nWiFi connected"));

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupNTP() {
  // Set up NTP client
  Serial.println("Setting up NTP");
  timeClient.begin();
  delay(200);
  timeClient.update();
}

/************************* Loop *************************/
bool first = true;

void loop() {
  // Updating things over wifi is costly and makes the display freeze for a moment, only update things once in a while
  updateData();
  
  if (first) {
    Serial.println("Updated data and all set");
    first = false;
  }
  
  sampleData();

  // DO NOT USE FILL SCREEN - VERY EXPENSIVE (12x worse)!
  display.clearDisplay();
  
  drawBars();
  drawTime();
  drawWeather();
  
  display.showBuffer();
  
  delay(15);
}

void updateData() {
  updateCount++;
  if (updateCount > updateTime) {
    timeClient.update();
    updateCurrentWeather();
    updateCount = 0;
  }
}

void updateCurrentWeather() {
  http.begin(currentEndpoint  + openweatherKey); //Specify the URL
  int httpCode = http.GET();  //Make the request

  if (httpCode > 0) { //Check for the returning code
      String payload = http.getString();

      // JSON buffer based off of article https://randomnerdtutorials.com/decoding-and-encoding-json-with-arduino-or-esp8266/
      const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 
          2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 
          JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 390;
      DynamicJsonBuffer jsonBuffer(bufferSize);
      JsonObject& root = jsonBuffer.parseObject(payload);

      if (!root.success()) {
        temperature = -999;
      } else {
        temperature = (int) root["main"]["temp"];
      }
  } else {
    temperature = -999;
  }

  http.end(); //Free the resources
}

#define debugData false
bool gotData;

void sampleData() {
  Wire.requestFrom(1, 17);

  gotData = Wire.available();
  i = 0;
  while (Wire.available()) {
    val = (int) Wire.read();

    if (i < 16) {
      // fft data for 1st 16 bytes
      
      // max bar height is 16, fixes really tall bars in some cases (which are out of the view)
      lines[i] = min(val, 16);
    } else {
      // light sensor data for 17th byte
      val = max((int) (d * 0.75), 10);

      brightness = 0.95 * brightness + 0.05 * val;
      
      display.setBrightness(brightness);

      if (debugData) Serial.println(brightness);
    }

    i++;

    if (debugData) {
      Serial.print(d);
      Serial.print(" ");
    }
  }
  if (debugData && gotData) Serial.println();

  // put bars into an invalid state if no I2C connection
  if (!gotData) {
    for (i = 0; i < BARS; i++) {
      lines[i] = -1;
    }
  }
}

/************************* Draw *************************/

void drawBars() {
  sum = 0;
  for (i = 0; i < BARS; i++) {
    sum += lines[i];
  }
  avg = sum / BARS;

  // basic and naive noise and signal filtering
  if (avg > 0 && avg < 3) {
    for (i = 0; i < BARS; i++) {
      lines[i] = 1;
    }
  }
  
  for (i=0; i < BARS; i++) {
    uint16_t color = getColor((int) ((float) i / BARS * 255) % 255, 255, 255);

    // If we can't talk to I2C, red error bar
    if (lines[i] == -1) {
      color = getColor(0, 255, 255);
      lines[i] = 1;
    }
    
    // Positive (upwards) movement is quicker than negative (downwards) movement
    if (lines[i] > lastLines[i]) {
      lines[i] = max((float) 1, lines[i] * positiveMixFactor + lastLines[i] * (1 - positiveMixFactor));
    } else {
      lines[i] = max((float) 1, lines[i] * negativeMixFactor + lastLines[i] * (1 - negativeMixFactor));  
    }
    
    lastLines[i] = lines[i];

    display.fillRect(i*BAR_SIZE, 32 - min(lines[i] + 0, 16), BAR_SIZE, lines[i], color);

    lines[i] = 0;
  }
}

#define AM "AM"
#define PM "PM"
char* am;

void drawTime() {
  display.setTextColor(getColor(LIME_HUE, 255, 255));
  display.setFont();

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  
  am = AM;
  if (hour >= 12) {
    am = PM;
    if (hour > 12) hour -= 12;
  }
  if (hour == 0) hour = 12;

  display.setCursor(3, 3);
  display.print(hour);
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.print(":");
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.printf("%02d", minute);
  display.setCursor(display.getCursorX() + 1, display.getCursorY());
  display.print(am);
}

void drawWeather() {
  display.setTextColor(getColor(LIME_HUE, 255, 255));
  display.setFont(&TomThumb);

  if (temperature > -999) {
    display.setCursor(64 - 4*getNumberLength(temperature) - 6 + numberOfOnes(temperature), 8);
  
    display.print(temperature);
    display.setCursor(display.getCursorX(), display.getCursorY() - 1);
    display.print("o");
  }
}

/************************* Util *************************/

uint16_t getColor(int hue, int saturation, int value) {
    value = (int) (0.5 * value + 0.5 * value * brightness / 255.);
    setLedColorHSV(hue, saturation, value);
    
  return display.color565(RedLight, GreenLight, BlueLight);
}

int getNumberLength(int number) {
  int length = 0;
  if (number > -10 && number < 10) length = 1;
  else if (number > -100 && number < 100) length = 2;
  else length = 3;

  if (number < 0) length++;

  return length;
}

int numberOfOnes(int number) {
  int count = 0;
  
  if (abs(number) % 10 == 1) count++; // ones
  number /= 10;
  if (abs(number) % 10 == 1) count++; // tens
  number /= 10;
  if (abs(number) % 100 == 1) count++; // hundreds

  return count;
}

// Not quite sure about this one...
void setLedColorHSV(byte h, byte s, byte v) {
  // this is the algorithm to convert from RGB to HSV
  h = (h * 192) / 256;  // 0..191
  unsigned int i = h / 32;   // We want a value of 0 thru 5
  unsigned int f = (h % 32) * 8;   // 'fractional' part of 'i' 0..248 in jumps

  unsigned int sInv = 255 - s;  // 0 -> 0xff, 0xff -> 0
  unsigned int fInv = 255 - f;  // 0 -> 0xff, 0xff -> 0
  byte pv = v * sInv / 256;  // pv will be in range 0 - 255
  byte qv = v * (256 - s * f / 256) / 256;
  byte tv = v * (256 - s * fInv / 256) / 256;

  switch (i) {
  case 0:
    RedLight = v;
    GreenLight = tv;
    BlueLight = pv;
    break;
  case 1:
    RedLight = qv;
    GreenLight = v;
    BlueLight = pv;
    break;
  case 2:
    RedLight = pv;
    GreenLight = v;
    BlueLight = tv;
    break;
  case 3:
    RedLight = pv;
    GreenLight = qv;
    BlueLight = v;
    break;
  case 4:
    RedLight = tv;
    GreenLight = pv;
    BlueLight = v;
    break;
  case 5:
    RedLight = v;
    GreenLight = pv;
    BlueLight = qv;
    break;
  }
}
