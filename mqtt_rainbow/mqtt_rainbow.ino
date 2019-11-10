#include "config.h"

// set up the feeds
AdafruitIO_Feed *blue = io.feed("rainbow.blue");
AdafruitIO_Feed *green = io.feed("rainbow.green");
AdafruitIO_Feed *purple = io.feed("rainbow.purple");
AdafruitIO_Feed *red = io.feed("rainbow.red");
AdafruitIO_Feed *yellow = io.feed("rainbow.yellow");

void setup() {
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Try to connect to wifi before io stuff
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to wifi");

  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("Connecting to Adafruit IO");

  // start MQTT connection to io.adafruit.com
  io.connect();

  // set up message handlers for the feeds
  blue->onMessage(handleMessage);
  green->onMessage(handleMessage);
  purple->onMessage(handleMessage);
  red->onMessage(handleMessage);
  yellow->onMessage(handleMessage);

  // wait for an MQTT connection
  while(io.status() < AIO_CONNECTED) {
    Serial.println(io.statusText());
    delay(500);
  }

  digitalWrite(LED_BUILTIN, HIGH);

  // Because Adafruit IO doesn't support the MQTT retain flag, we can use the
  // get() function to ask IO to resend the last value for this feed to just
  // this MQTT client after the io client is connected.
  blue->get();
  green->get();
  purple->get();
  red->get();
  yellow->get();

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
}

void loop() {
  // io.run(); is required for all sketches it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();
}

void handleMessage(AdafruitIO_Data *data) {
  Serial.print("received ");
  Serial.print(data->feedName());
  Serial.print(" <- ");
  Serial.println(data->value());
}
