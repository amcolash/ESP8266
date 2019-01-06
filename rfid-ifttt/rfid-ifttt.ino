/*
Code starting point from: https://github.com/Jorgen-VikingGod/ESP8266-MFRC522
Thanks to the Arduino community for further support and all of the wonderful
libraries out there.
*/

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <Timezone.h>
#include <IFTTTMaker.h>
#include <ESP8266Ping.h>

// Include IFTTT key separately
#include "key.h"

// Include the sunrise/sunset data I collected
#include "sundata.h"

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO0
SDA(SS) = GPIO3 
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
*/

// Define pins
const int RST_PIN = 0;  // RST-PIN for RC522
const int SS_PIN = 3;  // SDA/SS-PIN for RC522
const int LED_PIN = 2;  // Built in LED (2)

// Set up IFTTT constants
const char *TRIGGER_ON  = "lights_on";
const char *TRIGGER_OFF = "lights_off";
const bool USE_IFTTT = true;

// Set up wifi configuration
const char *ssid = "Obi-LAN Kenobi";
const char *pass = "UseTheForce";

// IP to ping
IPAddress ip (192, 168, 1, 102);

// Set up timezone, see: 
TimeChangeRule usPDT = {"PDT", Second, Sun, Mar, 2, -60 * 7};  //UTC -7 hours
TimeChangeRule usPST = {"PST", First, Sun, Nov, 2, -60 * 8};   //UTC -8 hours
Timezone myTZ(usPDT, usPST);
TimeChangeRule *tcr;        // pointer to the time change rule, use to get TZ abbrev

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// MRFC522 instance
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// Set up ifttt
WiFiClientSecure client;
IFTTTMaker ifttt(IFTTT_KEY, client);

// Counters
int counter = 0;
int retries = 0;

// Daylight savings time, defined when time acquired
int DST = 0;
int sunrise = 0;
int sunset = 0;

void setup() {
  Serial.begin(115200);    // Initialize serial communications
  Serial.println(F("Booting...."));
  delay(250);
  
  Serial.print(F("Connecting to: "));
  Serial.print(ssid);

  WiFi.mode(WIFI_STA); //We don't want the ESP to act as an AP
  WiFi.begin(ssid, pass); // Connect to the configured AP

  // No point moving forward if we don't have a connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // We have connected to wifi successfully
  Serial.println(F("\nWiFi connected"));

  // Set up NTP client
  timeClient.begin();

  // Set up RFID reader
  delay(200);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  // Try to boost antenna gain
  delay(200);

  // Set up leds
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Flash LED to know everything is all set
  blink(5);

  // All set
  Serial.println(F("\nReady!"));
}

void loop() {
  // This will only do a "real" update every 60 seconds to keep clock up to date
  timeClient.update();

  // Either card or wifi device will turn on the lights, need both to disappear for them to turn off
  set_enabled(card_present() || device_present());
}

/* Ping the given device once to see if it is on the wifi network */
bool device_present() {
  return Ping.ping(ip, 1);
}

/*
  Look for new cards and try to read current card
  Run the 2 commands twice to "wake up" idled card? This makes sure
  that the following value is what we expect
*/
bool card_present() {
  return mfrc522.PICC_IsNewCardPresent() || mfrc522.PICC_ReadCardSerial()
    || mfrc522.PICC_IsNewCardPresent() || mfrc522.PICC_ReadCardSerial();
}

void blink(int num) {
  for (int i = 0; i < num; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(250);
    digitalWrite(LED_PIN, HIGH);
    delay(250);
  }
}

void set_enabled(bool val) {
  // Account for a little bit of jitter
  digitalWrite(LED_PIN, (val || counter > 20) ? LOW : HIGH);
  
  if (counter == 1 && !val) {
    retries = 10;
    ifttt_trigger(false);
  } else if (counter == 0 && val) {
    retries = 10;
    ifttt_trigger(true);
  }

  if (val) {
    counter = 25;
  } else {
    if (counter > 1) counter--;
    else counter = 0;
  }

  delay(200);
}

void ifttt_trigger(bool on) {
  if (!USE_IFTTT) return;

  // Update sunrise/sunset, check if we actually need to run
  get_sunrise_sunset();
  time_t t = get_time_local();
  tm *now = gmtime(&t);

  // Simple check based on sunset/sunrise hour (not too picky here with minutes)
  int shifted_sunrise = (sunrise / 60) + 1;
  int shifted_sunset = (sunset / 60) - 1;
  if (on && now->tm_hour >= shifted_sunrise && now->tm_hour < shifted_sunset) {
    Serial.println("Failed time check, not turning on lights");
    return;
  }
  
  Serial.print("Sending: ");
  Serial.print(on);
  if(ifttt.triggerEvent(on ? TRIGGER_ON : TRIGGER_OFF, ssid)) {
    Serial.println(", Success!");
    retries = 0;
    delay(1000);
  } else {
    Serial.println(", Failed!");
    if (retries > 0) Serial.println("Retrying...");
    delay(1000);
    if (retries > 0) {
      retries--;
      ifttt_trigger(on);
    }
  }
}

time_t get_time_local() {
  return myTZ.toLocal(now(), &tcr);
}

void get_sunrise_sunset() {
  // update local time here to match NTP client
  setTime(timeClient.getEpochTime());
  DST = myTZ.locIsDST(now()) ? 1 : 0;
  
  time_t now = get_time_local();
  tm *tmp = gmtime(&now);
  int day = (tmp->tm_mon * 30.5) + tmp->tm_mday; // approximate day of year

  if (day >= 0 && day <= 366) {
    sunrise = SUNRISE_DATA[day];
    sunset = SUNSET_DATA[day];
  }
}

