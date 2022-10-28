#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#define STATUS D5
#define SHUTDOWN D1
#define OUT_STATE D0

#define SSID "Obi-LAN Kenobi"
#define PASSWORD "UseTheForce"

#define SERVER "http://192.168.1.146:3000"

#define IP  IPAddress(192, 168, 1, 130)
#define GATEWAY IPAddress(192, 168, 1, 1)
#define MASK IPAddress(255, 255, 255, 0)

// the MAC address of your wifi router
uint8_t home_mac[6] = { 0x60, 0x38, 0xE0, 0xB3, 0x43, 0xD8 };   // 60:38:E0:B3:43:D8
int channel = 10;    // the wifi channel to be used

//#define SSID "CenturyLinkB588"
//#define PASSWORD "GreenChileOrWhat"

//#define SERVER "http://192.168.0.10:3000"

//#define IP  IPAddress(192, 168, 0, 27)
//#define GATEWAY IPAddress(192, 168, 0, 1)
//#define MASK IPAddress(255, 255, 255, 0)

// the MAC address of your wifi router
//uint8_t home_mac[6] = { 0x44, 0x65, 0x7F, 0x8D, 0x07, 0x49 };   // 44:65:7f:8d:07:49
//int channel = 11;    // the wifi channel to be used

#define DEBUG true

uint8_t cycle = 0;
long timer = 0;

void setupSerial() {
  Serial.begin(115200);

  if (DEBUG) delay(100);
  if (DEBUG) Serial.println("\n-----------------");
  if (DEBUG) Serial.println("Turning On");
}

void setupPins() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SHUTDOWN, OUTPUT);
  pinMode(STATUS, INPUT_PULLUP);
  pinMode(OUT_STATE, OUTPUT);

  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(OUT_STATE, LOW);
  digitalWrite(SHUTDOWN, LOW);
}

void slowWifiConnect() {
  if (DEBUG) Serial.println("No (or wrong) saved WiFi credentials. Doing a fresh connect.");
  int counter = 0;
  
  // persistent and autoconnect should be true by default, but lets make sure.
  WiFi.setAutoConnect(true);  // autoconnect from saved credentials
  WiFi.persistent(true);     // save the wifi credentials to flash
 
// Note the two forms of WiFi.begin() below. If the first version is used
// then no wifi-scan required as the RF channel and the AP mac-address are provided.
// so next boot, all this info is saved for a fast connect.
// If the second version is used, a scan is required and for me, adds about 2-seconds
// to my connect time. (on a cold-boot, ie power-cycle)
 
//  WiFi.begin(SSID, PASSWORD, channel, home_mac, false);
// 
//  // now wait for good connection, or reset
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(500);
//    if (DEBUG) Serial.print(".");
//    if (++counter > 20) {        // allow up to 10-sec to connect to wifi
//      counter = 0;
//
//      if (DEBUG) Serial.println("Slow connect-scanning");
//
//      // If that failed, try by scanning - last chance
//      WiFi.begin(SSID, PASSWORD);
//      while (WiFi.status() != WL_CONNECTED) {
//        delay(500);
//        if (DEBUG) Serial.print(".");
//        if (++counter > 30) {
//          if (DEBUG) Serial.println("wifi timed-out. Rebooting..");
//          if (DEBUG) Serial.flush();
//          if (DEBUG) delay(10);  // so the serial message has time to get sent
//          ESP.restart();
//        }
//      }
//    }
//  }

  if (DEBUG) Serial.println("Slow connect-scanning");
  WiFi.begin(SSID, PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (DEBUG) Serial.print(".");
    if (++counter > 20) {        // allow up to 10-sec to connect to wifi
      if (DEBUG) Serial.println("wifi timed-out. Rebooting..");
      delay(10);  // so the serial message has time to get sent
      ESP.restart();
    }
  }
  
  if (DEBUG) Serial.println("WiFi connected and credentials saved");
}

void setupWifi() {
  WiFi.config(IP, GATEWAY, MASK);

  if (DEBUG) Serial.println("Trying fast WiFi connect.");

  // so even though no WiFi.connect() so far, check and see if we are connecting.
  // The 1st time sketch runs, this will time-out and THEN it accesses WiFi.connect().
  // After the first time (and a successful connect), next time it connects very fast
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(5);     // use small delays, NOT 500ms
    if (++counter > 600) break;     // 3 sec timeout
  }
  //if timed-out, connect the slow-way
  if (counter > 600) slowWifiConnect();

  if (DEBUG) Serial.println(WiFi.localIP());

  if (DEBUG) digitalWrite(LED_BUILTIN, HIGH);
//  if (DEBUG) Serial.println(WiFi.localIP());
}

void updateState() {
  if (DEBUG) Serial.println("Sending data to server");
  
  WiFiClient wifiClient;
  bool state = digitalRead(STATUS);
  digitalWrite(OUT_STATE, state);

  HTTPClient http;
  long duration = millis() - timer;
  if (state) http.begin(wifiClient, String(SERVER) + "/on?duration=" + String(duration));
  else http.begin(wifiClient, String(SERVER) + "/off?duration=" + String(duration));
  
  http.GET();
  http.end();
}

void shutdown() {
  if (DEBUG) Serial.println("Turning off");
  if (DEBUG) Serial.flush();

  digitalWrite(SHUTDOWN, HIGH);
}

void setup() {
  timer = millis();
  
  enableWiFiAtBootTime();
  
  setupSerial();
  setupPins();
  setupWifi();

  updateState();

  shutdown();
//  ESP.restart();
}

void loop() {
  
}
