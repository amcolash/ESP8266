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

uint64_t STEREO_POWER = 0xA81;
uint64_t STEREO_VOLUME_UP = 0x481;
uint64_t STEREO_VOLUME_DOWN = 0xC81;

const int SEND_PIN = 12;
IRsend irsend(SEND_PIN);

ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200,SERIAL_8N1,SERIAL_TX_ONLY);
  Serial.println("\nBooting");

  digitalWrite(2, 1);

  Serial.println("Setup Pins");
  setupPins();

  Serial.println("Setup Wifi");
  setupWifi();
  Serial.println("Setup ota");
  setupOTA();
  Serial.println("Setup server");
  setupServer();

  Serial.println("Setup ir");
  irsend.begin();

  digitalWrite(2, 0);

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

  if (MDNS.begin("esp8266-remote")) {
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
  server.on("/", []() { sendResponse(getCurrentStatus()); });

  // Normal power
  server.on("/on", []() { changePower(true, false); });
  server.on("/off", []() { changePower(false, false); });

  // Forced power
  server.on("/on_force", []() { changePower(true, true); });
  server.on("/off_force", []() { changePower(false, true); });

  server.on("/projector_on", []() { projector(true); });
  server.on("/projector_off", []() { projector(false); });
  
  server.on("/stereo", []() { stereoPower(); });
  server.on("/stereo_up", []() { stereoVolume(1); });
  server.on("/stereo_down", []() { stereoVolume(-1); });
  server.on("/stereo_volume", []() { stereoVolume(); });

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

  digitalWrite(2, digitalRead(4));
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

    digitalWrite(5, 1);
    delay(forced ? 5000 : 1500);
    digitalWrite(5, 0);
  } else {
    response += "Nothing Done";
  }

  sendResponse(response);
  Serial.println(response);
}

void projector(bool on) {
  uint64_t code = on ? ON_CODE : OFF_CODE;
  
  // Add in multiple IR messages just in case...
  int count = 4;

  for (int i = 0; i < count; i++) {
    irsend.sendNEC(code);
    delay(250);
  }

  String response = "Sent projector power command ";
  response += (on ? "on" : "off");
  sendResponse(response);
}

void stereoPower() {
    irsend.sendSony(STEREO_POWER, 12, 5);
    sendResponse("Send stereo power command");
}

void stereoVolume() {
  if (server.hasArg("volume")) {
    int volume = server.arg("volume").toInt();
    volume = constrain(volume, 0, 30);

    for (int i = 0; i < 30; i++) {
      irsend.sendSony(STEREO_VOLUME_DOWN, 12, 3);
      delay(100);
    }

    for (int i = 0; i < volume; i++) {
      irsend.sendSony(STEREO_VOLUME_UP, 12, 3);
      delay(100);
    }

    String response = "Setting volume to ";
    response += volume;
    sendResponse(response);
  } else {
    server.send(400, "text/plain", "No volume specified");
  }
}

void stereoVolume(int steps) {
  if (server.hasArg("volume")) {
      steps = server.arg("volume").toInt();
      steps = constrain(steps, -30, 30);
  }
  
  uint64_t code = steps > 0 ? STEREO_VOLUME_UP : STEREO_VOLUME_DOWN;
  if (steps < 0) steps *= -1;
  
  for (int i = 0; i < steps; i++) {
    irsend.sendSony(code, 12, 3);
    delay(200);
  }
  
  String response = "Sent stereo volume command of ";
  response += steps;
  sendResponse(response);
}

String getCurrentStatus() {
  bool current = digitalRead(4);
  String response = "Current Status: ";
  response += !current;
  response += "\n";
  return response;
}

void sendResponse(String response) {
  // CORS! Yuck this sucks that I need this
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", response);

  Serial.println(response);
}

