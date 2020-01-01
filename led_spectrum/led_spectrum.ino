#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h> 
#include <time.h>
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
time_t rawtime;
struct tm *ti;

// web server
ESP8266WebServer server(80);

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

int8_t lines[BARS];
float lastLines[BARS];
int i, val, sum;
float avg, floatVal, runningAvg;

struct {
  uint8_t valid = 0;
  uint8_t textHue = 0;
  uint8_t textSaturation = 0;
  uint8_t barHues[BARS];
  uint8_t barSaturation[BARS];
} colorData;

# define EEPROM_ADDRESS 0

// I2C blending variables
const float positiveMixFactor = 0.2;
const float negativeMixFactor = 0.07;

char timeDisplay[10];
int temperature = -999;

// Scrolling Text
const float scrollSpeed = 0.2;
float dateOffset = 0;
int dateDirection = 0;
float songOffset = 64;
int songDirection = 0;
String currentSong;
bool songEnabled = true;

// roughly 60 updates a second * 60 sec * 60 min (every hour)
const long updateTime = 60 * 60 * 60;
long updateCount = updateTime;

int brightness = 100;
int brightnessOverride = -1;

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

  setupColors();
  setupDisplay();
  setupWifi();
  setupNTP();
  setupServer();

  // Zero out arrays (just in case)
  for (i=0; i < BARS; i++) {
    lines[i] = -1;
    lastLines[i] = -1;
  };
}

void setupColors() {
  // get existing data from EEPROM
  EEPROM.begin(sizeof colorData);
  EEPROM.get(EEPROM_ADDRESS, colorData);

  // reset data if 1st byte is incorrect
  if (colorData.valid != 42) resetColors();

  // only writes if the bytes are different
  writeEeprom();
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

  // Draw wifi connecting icon on both buffers
  display.drawBitmap(24, 9, wifiIcon, 16, 15, getColor(colorData.textHue, colorData.textSaturation, 255));
  display.showBuffer();
  display.drawBitmap(24, 9, wifiIcon, 16, 15, getColor(colorData.textHue, colorData.textSaturation, 255));
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

  // Continually update NTP until it is actually set (and we are not in 1969 at the epoch)
  int year = 0;
  while (year < 2019) {
    delay(100);
    timeClient.update();
    
    rawtime = timeClient.getEpochTime();
    ti = localtime(&rawtime);

    year = ti->tm_year + 1900;
  }
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/brightness", HTTP_GET, getBrightness);
  server.on("/brightness", HTTP_POST, setBrightness);
  server.on("/song", HTTP_GET, getSongEnabled);
  server.on("/song", HTTP_POST, setSong);
  server.on("/color", HTTP_GET, getColors);
  server.on("/color", HTTP_POST, setColors);
  server.onNotFound(handleNotFound);

  server.begin();
}

/********************** Web Server **********************/
void handleRoot() {
  server.send(200, "text/plain", "Hello world!");   // Send HTTP status 200 (Ok) and send some text to the browser/client
}

void getBrightness() {
  server.send(200, "text/plain", String(brightnessOverride));
}

String setBrightnessString = "Set brightness to ";
void setBrightness() {
  if(server.hasArg("brightness")) {
    brightnessOverride = server.arg("brightness").toInt();
    server.send(200, "text/plain", setBrightnessString + brightnessOverride);
  } else {
    server.send(400, "text/plain", "400: Invalid Request");
  }
}

String songEnabledString = "songEnabled: ";
void getSongEnabled() {
  server.send(200, "text/plain", songEnabledString + songEnabled);
}

void setSong() {
  if(server.hasArg("enabled")) {
    songEnabled = server.arg("enabled") == "true";
    server.send(200, "text/plain", songEnabledString + songEnabled);
  } else if(server.hasArg("song") && songEnabled) {
    currentSong = server.arg("song");
    dateDirection = -1;
    server.send(200, "text/plain", "Setting new song");
  } else {
    server.send(400, "text/plain", "400: Invalid Request");
  }
}

void getColors() {
  const int colorCapacity = JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(2 * 16);
  StaticJsonDocument<colorCapacity> colorDoc;
  char output[256];

  colorDoc["textHue"] = (int) colorData.textHue;
  colorDoc["textSaturation"] = (int) colorData.textSaturation;

  JsonArray barHues = colorDoc.createNestedArray("barHues");
  JsonArray barSaturation = colorDoc.createNestedArray("barSaturation");
  for (i = 0; i < 16; i++) {
    barHues.add(colorData.barHues[i]);
    barSaturation.add(colorData.barSaturation[i]);
  }

  serializeJson(colorDoc, output);

  server.send(200, "text/json", output);
}

void setColors() {
  if (server.hasArg("data")) {
    const int colorCapacity = JSON_OBJECT_SIZE(4) + 2 * JSON_ARRAY_SIZE(16) + 180;
    StaticJsonDocument<colorCapacity> colorDoc;
    deserializeJson(colorDoc, server.arg("data"));

    colorData.textHue = colorDoc["textHue"];
    colorData.textSaturation = colorDoc["textSaturation"];

    JsonArray barHues = colorDoc["barHues"].as<JsonArray>();
    JsonArray barSaturation = colorDoc["barSaturation"].as<JsonArray>();

    for (i = 0; i < 16; i++) {
      colorData.barHues[i] = barHues[i];
      colorData.barSaturation[i] = barSaturation[i];
    }
    writeEeprom();

    // send the set colors as the response
    getColors();
  } else if (server.hasArg("reset")) {
    resetColors();
    writeEeprom();

    // send the reset colors as the response
    getColors();
  } else {
    server.send(400, "text/plain", "400: Invalid Request");
  }
}


void handleNotFound(){
  server.send(404, "text/plain", "404: Not found"); // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
}

/************************* Loop *************************/
bool first = true;

void loop() {
  server.handleClient();
  
  // Updating things over wifi is costly and makes the display freeze for a moment, only update things once in a while
  updateData();
  
  if (first) {
    Serial.println("Updated data and all set");
    first = false;
  }
  
  sampleData();

  draw();

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
      StaticJsonDocument<bufferSize> doc;
      deserializeJson(doc, payload);
      
      temperature = (int) doc["main"]["temp"];
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
      // found these magic numbers based off of my own ambient light
      val = max(10, (int) (0.004 * val * val));

      if (brightnessOverride > 0) {
        brightness = brightnessOverride;
      } else {
        brightness = 0.95 * brightness + 0.05 * val;
      }
      
      display.setBrightness(brightness);

      if (debugData) Serial.println(brightness);
    }

    i++;

    if (debugData) Serial.printf("%d ", val);
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

void draw() {
  // DO NOT USE FILL SCREEN - VERY EXPENSIVE (12x worse)!
  display.clearDisplay();
  
  drawBars();
  drawTime();
  drawDate();
  drawSong();
  drawWeather();
  
  display.showBuffer();
}

uint16_t color;
void drawBars() {
  sum = 0;
  // ignore band 1 from the sum to try and filter more out
  for (i = 1; i < BARS; i++) {
    sum += lines[i];
  }
  avg = sum / BARS;
  runningAvg = runningAvg * 0.95 + avg * 0.05;

  for (i=0; i < BARS; i++) {
    color = getColor(colorData.barHues[i], colorData.barSaturation[i], 255);

    // use a float to keep track of things so that the bars mix well between frames
    floatVal = lines[i];
    
    // If we can't talk to I2C, red error bar
    if (floatVal == -1) {
      color = getColor(0, 255, 255);
      floatVal = 1;
    }
    
    // Positive (upwards) movement is quicker than negative (downwards) movement
    if (floatVal > lastLines[i]) {
      floatVal = floatVal * positiveMixFactor + lastLines[i] * (1 - positiveMixFactor);
    } else {
      floatVal = floatVal * negativeMixFactor + lastLines[i] * (1 - negativeMixFactor);
    }

    // If not much is going on, flatline the whole thing (avg silence is about 1.1-1.3), music > 2 usually
    if (runningAvg < 1.4 && floatVal >= 1.4) {
      floatVal *= 0.3;
    }
    
    floatVal = constrain(floatVal, 1, 14);
    lastLines[i] = floatVal;
    
    val = floor(floatVal);
    display.fillRect(i*BAR_SIZE, 32 - val, BAR_SIZE, val, color);

    lines[i] = 0;
  }
}

#define AM "AM"
#define PM "PM"
char* am;
uint8_t day, month, minute, hour;

void drawTime() {
  display.setTextColor(getColor(colorData.textHue, colorData.textSaturation, 255));
  display.setFont();

  hour = timeClient.getHours();
  minute = timeClient.getMinutes();
  
  am = AM;
  if (hour >= 12) {
    am = PM;
    if (hour > 12) hour -= 12;
  }
  if (hour == 0) hour = 12;

  display.setCursor(2 - (hour > 9 ? 1 : 0), 2);
  display.print(hour);
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.print(":");
  display.setCursor(display.getCursorX() - 2, display.getCursorY());
  display.printf("%02d", minute);
  display.setCursor(display.getCursorX() + 1, display.getCursorY());
  display.print(am);
}

void drawDate() {
  if (songDirection != 0) return;
  
  dateOffset += scrollSpeed * dateDirection;
  
  if (dateOffset > 0) dateDirection = 0;
  if (dateOffset < -20) {
    dateDirection = 0;
    songDirection = -1;
  }

  display.setTextColor(getColor(colorData.textHue, colorData.textSaturation, 255));
  display.setFont(&TomThumb);

  rawtime = timeClient.getEpochTime();
  ti = localtime(&rawtime);

  day = ti->tm_mday;
  month = ti->tm_mon + 1;
  
  display.setCursor(2 + dateOffset, 16);
  display.printf("%d/%d", month, day);
}
  
void drawSong() {
  if (songDirection == 0) return;

  int songLength = currentSong.length() * 4;
  
  songOffset += scrollSpeed * songDirection;
  if (songOffset < -songLength) {
    songDirection = 0;
    songOffset = 64;
    dateDirection = 1;
    currentSong = "";
  }
  
  display.setTextColor(getColor(colorData.textHue, colorData.textSaturation, 255));
  display.setFont(&TomThumb);
  
  display.setCursor(2 + songOffset, 16);
  display.print(currentSong);
}

void drawWeather() {
  display.setTextColor(getColor(colorData.textHue, colorData.textSaturation, 255));
  display.setFont(&TomThumb);

  if (temperature > -999) {
    display.setCursor(65 - 4*getNumberLength(temperature) - 6 + numberOfOnes(temperature), 7);
  
    display.print(temperature);
    display.setCursor(display.getCursorX(), display.getCursorY() - 1);
    display.print("o");
  }
}

/************************* Util *************************/

void resetColors() {
  // Lime
  colorData.textHue = 85;
  colorData.textSaturation = 255;
  
  // Rainbow
  for (i=0; i < BARS; i++) {
    colorData.barHues[i] = ((int) ((float) i / BARS * 255) % 255);
    colorData.barSaturation[i] = 255;
  }

  colorData.valid = 42;
}

void writeEeprom() {
  EEPROM.put(EEPROM_ADDRESS, colorData);
  EEPROM.commit();
}

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
