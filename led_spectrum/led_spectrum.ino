#define double_buffer
#include <PxMatrix.h>
#include <Ticker.h>
#include <fix_fft.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// Set up wifi configuration
const char *ssid = "Obi-LAN Kenobi";
const char *pass = "UseTheForce";

// NTP Client
const long utcOffsetInSeconds = -8 * 60 * 60;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, utcOffsetInSeconds);

#define P_LAT 16
#define P_A 5
#define P_B 4
#define P_C 15
#define P_D 12
#define P_E 0
#define P_OE 2

#define SCALE_FACTOR 12  // Direct scaling factor (raise for higher bars, lower for shorter bars)
const float log_scale = 64./log(64./SCALE_FACTOR + 1.);  // Attempts to create an equivalent to SCALE_FACTOR for log function

// Pins for LED MATRIX
PxMATRIX display(64,32,P_LAT, P_OE,P_A,P_B,P_C,P_D);

uint16_t RED = display.color565(255, 0, 0);
uint16_t CYAN = display.color565(0, 255, 255);
uint16_t BLACK = display.color565(0, 0, 0);
uint16_t LIME = display.color565(20, 255, 20);

// Wifi icon by Gregbaker, from wikimedia: https://commons.wikimedia.org/wiki/File:Wireless-icon.svg
// 'wifi', 26x24px
const unsigned char wifiIcon [] PROGMEM = {
  0x00, 0x3f, 0x00, 0x00, 0x01, 0xff, 0xf0, 0x00, 0x0f, 0xff, 0xfc, 0x00, 0x1f, 0xe1, 0xfe, 0x00, 
  0x3f, 0x00, 0x1f, 0x80, 0x7c, 0x00, 0x0f, 0xc0, 0xf0, 0x7f, 0x83, 0xc0, 0x61, 0xff, 0xf1, 0x80, 
  0x07, 0xff, 0xf8, 0x00, 0x0f, 0xc0, 0xfc, 0x00, 0x0f, 0x00, 0x3c, 0x00, 0x06, 0x1e, 0x08, 0x00, 
  0x00, 0x7f, 0x80, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x60, 0xc0, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 
  0x00, 0x3f, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 'wifi_small', 16x15px
const unsigned char wifiIconSmall [] PROGMEM = {
  0x03, 0xc0, 0x1f, 0xf8, 0x78, 0x1e, 0xf0, 0x07, 0xcf, 0xf3, 0x1f, 0xf8, 0x38, 0x1c, 0x13, 0xc8, 
  0x07, 0xe0, 0x06, 0x60, 0x00, 0x00, 0x03, 0xc0, 0x03, 0xc0, 0x03, 0xc0, 0x00, 0x00
};


byte RedLight;
byte GreenLight;
byte BlueLight;

# define BARS 16
int BAR_SIZE = 64 / BARS;

int8_t data[128], prev[64], lines[BARS], lastLines[BARS];
int i=0,val;


Ticker display_ticker;
void display_updater() {
  display.display(0);
}

void setup() {
  Serial.begin(115200);

  // Init display
  display.begin(16);
  display.setFastUpdate(true);
  display.flushDisplay();
  
  // Paint to display 500x a second
  display_ticker.attach(0.002, display_updater);

  // Draw wifi connecting icon
//  display.drawBitmap(19, 4, wifiIcon, 26, 24, LIME);
  display.drawBitmap(24, 9, wifiIconSmall, 16, 15, LIME);
  display.showBuffer();

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

  // Set up NTP client
  Serial.println("Setting up NTP");
  timeClient.begin();

  for (i=0; i < 64; i++) { prev[i] = 0; };
  for (i=0; i < BARS; i++) { lines[i] = 0; };
}

unsigned long microseconds;
float mixFactor = 0.25;
float colorOffset = 0;
int idle = 0;
int averageIdle = 0;
char timeDisplay[10];

// roughly 50 updates a second * 60 sec * 60 min * 8 hour
long ntpUpdateTime = 50 * 60 * 60 * 8;
long ntpCount = ntpUpdateTime;

void loop() {
  // Updating with ntp is costly and makes the display freeze for a moment, only update once in a while
  ntpCount++;
  if (ntpCount > ntpUpdateTime) {
    timeClient.update();
    ntpCount = 0;
  }
  
  int total = 0;
  for (i=0; i < 128; i++){
//    microseconds = micros();

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
  
  for (i=0; i < 64;i++){
    if(data[i] < 0) data[i] = 0;
    else data[i] *= SCALE_FACTOR;
//    else data[i] *= log_scale*log((float)(data[i]+1));

//    data[i] = data[i] * mixFactor + prev[i] * (1. - mixFactor);
//    prev[i] = data[i];

//    display.drawFastVLine(i, 32 - data[i], data[i], LIME);

    lines[i / BAR_SIZE] += (data[i] / BAR_SIZE);
  }

  display.fillScreen(BLACK);
  for (i=0; i < BARS; i++) {
    lines[i] = max((float) 1, lines[i] * mixFactor + lastLines[i] * (1 - mixFactor));
    lastLines[i] = lines[i];

//    setLedColorHSV((int) ((i - colorOffset) / BARS * 256) % 256, 255 - ((lines[i] / 32.) * 128), 255);
//    setLedColorHSV((int) ((i - colorOffset) / BARS * 256) % 256, lines[i] + 224, 255);
    setLedColorHSV((int) ((i - colorOffset) / BARS * 256) % 256, 255, 255);
    
    uint16_t color = display.color565(RedLight, GreenLight, BlueLight);
    
//    display.drawFastHLine(i*BAR_SIZE, 32 - min(lines[i] + 0, 16), BAR_SIZE, color);
    display.fillRect(i*BAR_SIZE, 32 - min(lines[i] + 0, 16), BAR_SIZE, lines[i], color);
//    display.fillRect(i*BAR_SIZE, 16, BAR_SIZE, 16, color);

    lines[i] = 0;
  }

  char* am = "AM";
  int hour = timeClient.getHours();
  if (hour >= 12) {
    hour -= 12;
    am = "PM";
  }
  int minute = timeClient.getMinutes();

  sprintf(timeDisplay, "%d:%02d%s", hour, minute, am);
  
  display.setTextColor(LIME);
  display.setCursor((64 - (strlen(timeDisplay) * 6)) / 2, 4);
  display.print(timeDisplay);
  
  display.showBuffer();
//  colorOffset += 0.01;

  delay(20);
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
