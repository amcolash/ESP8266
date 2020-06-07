/*
Code starting point from: https://github.com/Jorgen-VikingGod/ESP8266-MFRC522
Thanks to the Arduino community for further support and all of the wonderful
libraries out there.
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <Timezone.h>
#include <ESP8266Ping.h>

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

// Set up assistant constants
const char *TRIGGER_ON  = "run lights on routine";
const char *TRIGGER_OFF = "lights off";

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

// Set up http requests
HTTPClient http;

// Counters
int counter = 0;
int retries = 0;
int delta = 0;

// Daylight savings time, defined when time acquired
int DST = 0;
int sunrise = 0;
int sunset = 0;

bool isOn = false;

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

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up NTP client
  Serial.println("Setting up NTP");
  timeClient.begin();

  // Set up RFID reader
  Serial.println("Setting up RFID");
  delay(100);
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);  // Try to boost antenna gain
  delay(100);

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

  delay(200);
}

/* Ping the given device once to see if it is on the wifi network */
int pingCounter = 0;
int pingThreshold = 12;
bool device_present() {
  pingCounter++;
  
  // Since loop delay ~400ms, counter = 300 = 1.5 minutes, counter = 12 = 5 seconds
  // This large delay is to try and help my phone's battery out 
  if (pingCounter > pingThreshold) {
    Serial.println("Pinging phone");
    
    bool alive = Ping.ping(ip, 1);

    pingCounter = 0;
    pingThreshold = alive ? 300 : 12;
    
    return alive;
  }

  return isOn;
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
  digitalWrite(LED_PIN, (val || counter > 15) ? LOW : HIGH);

//  Serial.print("Enabled? ");
//  Serial.print(val);
//  Serial.print(", ");
//  Serial.print(counter);
//  Serial.print(", ");
//  Serial.println(millis() - delta);

  delta = millis();
  
  if (counter == 1 && !val) {
    retries = 10;
    trigger(false);
  } else if (counter == 0 && val) {
    retries = 10;
    trigger(true);
  }

  if (val) {
    counter = 20;
  } else {
    if (counter > 1) counter--;
    else counter = 0;
  }
}

void trigger(bool on) {
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

  isOn = on;
  
  Serial.print("Sending: ");
  Serial.print(on);
  Serial.print(", ");

  String url = "http://192.168.1.101:8004/assistant";

  http.begin(url);  //Specify request destination
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"user\":\"amcolash\",\"command\":\"" + String(on ? TRIGGER_ON : TRIGGER_OFF) + "\"}");
  
  Serial.print("running command: ");
  Serial.println(body);
  int httpCode = http.POST(body);
  

  if (httpCode > 0) {
    if (httpCode == 200) {
      retries = 0;
    } else {
      Serial.println("Failed");
      delay(1000);
      if (retries > 0) {
        Serial.print("Retrying... ");
        Serial.println(retries);
        retries--;
        trigger(on);
      }
    }
    
    String payload = http.getString();
    Serial.println(payload);
  }

  http.end();   //Close connection
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
