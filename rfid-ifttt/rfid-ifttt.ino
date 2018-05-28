/*
Many thanks to nikxha from the ESP8266 forum
*/

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <IFTTTMaker.h>

// Include IFTTT key separately
#include "key.h"

/* wiring the MFRC522 to ESP8266 (ESP-12)
RST     = GPIO5
SDA(SS) = GPIO4 
MOSI    = GPIO13
MISO    = GPIO12
SCK     = GPIO14
GND     = GND
3.3V    = 3.3V
*/

#define RST_PIN 1  // RST-PIN for RC522
#define SS_PIN  3  // SDA-PIN for RC522

#define LED_PIN 2  // Built in LED

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// Set up IFTTT constants
#define TRIGGER_ON "lights_on"
#define TRIGGER_OFF "lights_off"
#define USE_IFTTT true

WiFiClientSecure client;
IFTTTMaker ifttt(IFTTT_KEY, client);

// Set up wifi configuration
const char *ssid =  "Obi-LAN Kenobi";
const char *pass =  "UseTheForce";
#define USE_WIFI true

int counter = 0;
int retries = 0;

void setup() {
  Serial.begin(115200);    // Initialize serial communications
  delay(250);
  Serial.println(F("Booting...."));
  
  SPI.begin();           // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max);

  if (USE_WIFI) {
    Serial.print(F("Connecting to: "));
    Serial.print(ssid);

    WiFi.mode(WIFI_STA); //We don't want the ESP to act as an AP
    WiFi.begin(ssid, pass); // Connect to the AP

    // No point moving forward if we don't have a connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    
    Serial.println(F("\nWiFi connected"));
  }

  Serial.println(F("Ready!"));

  // Set up leds
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Flash LED to know everything is all set
  blink(5);
}

void loop() {
  /*
    Look for new cards and try to read current card
    Run the 2 commands twice to "wake up" idled card? This makes sure
    that the following value is what we expect
  */
  if (mfrc522.PICC_IsNewCardPresent() || mfrc522.PICC_ReadCardSerial()
    || mfrc522.PICC_IsNewCardPresent() || mfrc522.PICC_ReadCardSerial()) {
    set_enabled(true);
    return;
  }

  set_enabled(false);
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
  digitalWrite(LED_PIN, val ? LOW : HIGH);
  
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

  // faster update of led
//  blink(3 + (on ? 2 : 0));
  
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

