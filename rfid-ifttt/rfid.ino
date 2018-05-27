/*
Many thanks to nikxha from the ESP8266 forum
*/

#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <IFTTTMaker.h>
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

#define RST_PIN  5  // RST-PIN für RC522 - RFID - SPI - Modul GPIO5 
#define SS_PIN  4  // SDA-PIN für RC522 - RFID - SPI - Modul GPIO4 

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// Set up IFTTT, IFTTT_KEY defined in separate file
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

  // Set up led
  pinMode(2, OUTPUT);
  
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

  // Flash LED to know everything is all set
  for (int i = 0; i < 5; i++) {
    digitalWrite(2, LOW);
    delay(250);
    digitalWrite(2, HIGH);
    delay(250);
  }
  
//  Serial.println(F("======================================================")); 
//  Serial.println(F("Scan for Card and print UID:"));
}

void loop() {
  // Set led based on present card, dimmed at 50%
  digitalWrite(2, counter > 0 ? LOW: HIGH);
  
  // Look for new cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    set_enabled(false);
    delay(200);
    return;
  }
  
  // Select one of the cards
  if (!mfrc522.PICC_ReadCardSerial()) {
    set_enabled(false);
    delay(200);
    return;
  }

  set_enabled(true);
  
  // Show some details of the PICC (that is: the tag/card)
//  Serial.print(F("Card UID:"));
//  dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
//  Serial.println();
}

// Helper routine to dump a byte array as hex values to Serial
void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void set_enabled(bool val) {
  if (counter == 1 && !val) {
    retries = 10;
    ifttt_trigger(false);
  } else if (counter == 0 && val) {
    retries = 10;
    ifttt_trigger(true);
  }

  if (val) {
    counter = 50;
  } else {
    if (counter > 1) counter--;
    else counter = 0;
  }
}

void ifttt_trigger(bool on) {
  if (!USE_IFTTT) return;

  // faster update of led, don't need to wait for safety delay
  digitalWrite(2, on ? LOW: HIGH);
  
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

