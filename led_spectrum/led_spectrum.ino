#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <NTPClient.h>

#define double_buffer
#include <PxMatrix.h>
#include <Fonts/TomThumb.h>

#include <Ticker.h>
#include <fix_fft.h>

#include "key.h"

// Set up wifi configuration
const char *ssid = "Obi-LAN Kenobi";
const char *pass = "UseTheForce";

// NTP Client, -8 is with DST, -7 is w/o DST
const long utcOffsetInSeconds = -8 * 60 * 60;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, utcOffsetInSeconds);

// Open weather map
const String location = "Seattle,us";
const String openweatherEndpoint = "http://api.openweathermap.org/data/2.5/weather?q=" + location + "&units=imperial&APPID=";
HTTPClient http;

// JSON buffer based off of article https://randomnerdtutorials.com/decoding-and-encoding-json-with-arduino-or-esp8266/
const size_t bufferSize = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(1) + 
    2*JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(5) + 
    JSON_OBJECT_SIZE(6) + JSON_OBJECT_SIZE(12) + 390;
DynamicJsonBuffer jsonBuffer(bufferSize);

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

uint16_t BLACK = display.color565(0, 0, 0);
uint16_t LIME = display.color565(20, 255, 20);

// 'wifi_small', 16x15px
const unsigned char wifiIcon [] PROGMEM = {
  0x03, 0xc0, 0x1f, 0xf8, 0x78, 0x1e, 0xf0, 0x07, 0xcf, 0xf3, 0x1f, 0xf8, 0x38, 0x1c, 0x13, 0xc8, 
  0x07, 0xe0, 0x06, 0x60, 0x00, 0x00, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x00, 0x00
};


byte RedLight;
byte GreenLight;
byte BlueLight;

# define BARS 16
int BAR_SIZE = 64 / BARS;

int8_t data[128], prev[64], lines[BARS], lastLines[BARS];
int i,val;

float mixFactor = 0.25;
char timeDisplay[10];
int temperature = -999;

// roughly 50 updates a second * 60 sec * 60 min (every hour)
long updateTime = 50 * 60 * 60;
long updateCount = updateTime;


Ticker display_ticker;
void display_updater() {
  display.display(0);
}

void setup() {
  Serial.begin(115200);

  setupDisplay();
  setupWifi();
  setupNTP();

  // Zero out arrays (just in case)
  for (i=0; i < 64; i++) { prev[i] = 0; };
  for (i=0; i < BARS; i++) { lines[i] = 0; };
}

void loop() {
  // Updating things over wifi is costly and makes the display freeze for a moment, only update things once in a while
  updateData();
  
  sampleData();

  display.fillScreen(BLACK);
  drawBars();
  drawTime();
  display.showBuffer();
  
  delay(20);
}

void setupDisplay() {
  // Init display
  display.begin(16);
  display.flushDisplay();
  display.setFastUpdate(true);
  display.setTextWrap(false);
  
  // Paint to display 500x a second
  display_ticker.attach(0.002, display_updater);

  // Draw wifi connecting icon
  display.drawBitmap(24, 9, wifiIcon, 16, 15, LIME);
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


void updateData() {
  updateCount++;
  if (updateCount > updateTime) {
    timeClient.update();
    updateWeather();
    updateCount = 0;

    int hour = timeClient.getHours();

    // tweak these if needed
    int morning = 7; // AM
    int evening = 5; // PM
    int night = 11; // PM
    
    if (hour >= (night + 12) || hour < morning) { display.setBrightness(100); }
    if (hour >= (evening + 12)) { display.setBrightness(200); }
    else { display.setBrightness(255); }
  }
}

void updateWeather() {
  http.begin(openweatherEndpoint  + openweatherKey); //Specify the URL
  int httpCode = http.GET();  //Make the request

  if (httpCode > 0) { //Check for the returning code

      String payload = http.getString();
      Serial.println(httpCode);
      Serial.println(payload);
      JsonObject& root = jsonBuffer.parseObject(payload);

      if (!root.success()) {
        temperature = -999;
      } else {
        temperature = (int) root["main"]["temp"];
      }
    }

  else {
    Serial.println("Error on HTTP request");
  }

  http.end(); //Free the resources
}

void sampleData() {
  int total = 0;
  for (i=0; i < 128; i++){
    val = (analogRead(A0) >> 2) - 128;  // fit to 8 bit int
    data[i] = val;
    total += val;

    if (i % 16 == 0) {
      delay(1);
    }
  };

  int avg = total / 128;

  for (i=0; i < 128; i++){
    data[i] -= avg;
  }

  fix_fftr(data,7,0);
  
  for (i=0; i < 64; i++){
    if(data[i] < 0) data[i] = 0;
    else data[i] *= SCALE_FACTOR;

    lines[i / BAR_SIZE] += (data[i] / BAR_SIZE);
  }
}

void drawBars() {
  for (i=0; i < BARS; i++) {
    lines[i] = max((float) 1, lines[i] * mixFactor + lastLines[i] * (1 - mixFactor));
    lastLines[i] = lines[i];

    setLedColorHSV((int) ((float) i / BARS * 255) % 255, 255, 255);
    uint16_t color = display.color565(RedLight, GreenLight, BlueLight);
    
    display.fillRect(i*BAR_SIZE, 32 - min(lines[i] + 0, 16), BAR_SIZE, lines[i], color);

    lines[i] = 0;
  }
}

void drawTime() {
  display.setTextColor(LIME);
  display.setFont();

  int hour = timeClient.getHours();
  int minute = timeClient.getMinutes();
  char* am = "AM";
  
  if (hour >= 12) {
    am = "PM";
    if (hour > 12) {
       hour -= 12; 
    }
  }

  display.setCursor(3, 4);
  display.print(hour);
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.print(":");
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.printf("%02d", minute);
  display.setCursor(display.getCursorX() + 1, display.getCursorY());
  display.print(am);

  if (temperature > -999) {
    if (temperature < 0 || temperature > 99) { display.setCursor(64 - 4*4 - 2, 3); }
    else { display.setCursor(64 - 4*3 - 2, 3); }

    display.setFont(&TomThumb);
    display.print(temperature);
    
    display.setCursor(display.getCursorX(), display.getCursorY() - 1);
    display.print("o");
  }
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
