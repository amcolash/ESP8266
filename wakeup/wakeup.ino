#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>

#include <IRremoteESP8266.h>
#include <IRsend.h>

const char* ssid = "Obi-LAN Kenobi";
const char* password = "UseTheForce";

uint64_t OFF_CODE = 0x4CB3748B;
uint64_t ON_CODE = 0x4CB340BF;

const int SEND_PIN = 12;
IRsend irsend(SEND_PIN);

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupPins();

  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);

  setupWifi();
  setupOTA();
  setupServer();

  irsend.begin();

  Serial.println("Ready");
}

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    digitalWrite(0, 0);
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  
  ArduinoOTA.onEnd([]() {
    digitalWrite(0, 1);
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
}

void setupServer() {
  server.on("/", []() {
    Serial.println("request: /");
    String response = getCurrentStatus();
    response += "Hi There!";
    server.send(200, "text/plain", response);
  });

  // Normal power
  server.on("/on", []() { changePower(true, false); });
  server.on("/off", []() { changePower(false, false); });

  // Forced power
  server.on("/on_force", []() { changePower(true, true); });
  server.on("/off_force", []() { changePower(false, true); });

  server.on("/projector_on", []() { projector(true); });
  server.on("/projector_off", []() { projector(false); });  

  server.onNotFound([]() {
    Serial.println("request: /? Not found");
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
}

void setupPins() {
  // Pin 2, LED (BLUE)
  // Pin 4, Power LED
  // Pin 5, Power Switch
  // Pin 12, IR

  pinMode(2, OUTPUT);
  pinMode(4, INPUT);
  pinMode(5, OUTPUT);

  digitalWrite(2, 1);
  digitalWrite(5, 0);
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

//  digitalWrite(2, digitalRead(4));
}

void changePower(bool on, bool forced) {
  bool current = digitalRead(4);
  String response = getCurrentStatus();
  response += "Requested, on: ";
  response += on;
  response += ", forced: ";
  response += forced;
  response += "\n";
  
  if (current == on) {
    response += "Ok";
    server.send(200, "text/plain", response);

    digitalWrite(5, 1);
    delay(forced ? 5000 : 1000);
    digitalWrite(5, 0);
  } else {
    response += "Nothing Done";
    server.send(200, "text/plain", response);
  }

  Serial.println(response);
}

void projector(bool on) {
  if (on) {
    irsend.sendNEC(ON_CODE);
  } else {
    irsend.sendNEC(OFF_CODE);
    delay(100);
    irsend.sendNEC(OFF_CODE);
  }

  String response = "Changed projector power to ";
  response += (on ? "on" : "off");
  server.send(200, "text/plain", response);
}

String getCurrentStatus() {
  bool current = digitalRead(4);
  String response = "Current Status: ";
  response += !current;
  response += "\n";
  return response;
}

