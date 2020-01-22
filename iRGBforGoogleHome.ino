
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsClient.h> //  https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries 
#include <ArduinoJson.h> // https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <math.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

int red_light_pin = D2;
int green_light_pin = D3;
int blue_light_pin = D4;


int red = 255;
int green = 255;
int blue = 255;
float brightness = 1;



#define MyApiKey "xxxxxxx-xxxxxx-xxxxxxxxx-xxxxxxxx" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define MySSID "xxx" // TODO: Change to your Wifi network SSID
#define MyWifiPassword "xxxxx" // TODO: Change to your Wifi network password

#define API_ENDPOINT "http://sinric.com"
#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

void turnOn(String deviceId) {
  if (deviceId == "5exxxxxxxxxxxxxxxxxxx") // Device ID of first device
  {
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    RGB_color(red, green, blue);
  }
  else if (deviceId == "5axxxxxxxxxxxxxxxxxxx") // Device ID of second device
  {
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
  }
  else {
    Serial.print("Turn on for unknown device id: ");
    Serial.println(deviceId);
  }
}

void turnOff(String deviceId) {
  if (deviceId == "5exxxxxxxxxxxxxxxxxx") // Device ID of first device
  {
    Serial.print("Turn off Device ID: ");
    Serial.println(deviceId);
    RGB_color(0, 0, 0);
  }
  else if (deviceId == "5axxxxxxxxxxxxxxxxxxx") // Device ID of second device
  {
    Serial.print("Turn off Device ID: ");
    Serial.println(deviceId);
  }
  else {
    Serial.print("Turn off for unknown device id: ");
    Serial.println(deviceId);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      isConnected = false;
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
        isConnected = true;
        Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
        Serial.printf("Waiting for commands from sinric.com ...\n");
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);

        /*
           {"deviceId":"5e28764500886b552b7519fc","action":"action.devices.commands.OnOff","value":{"on":true}}

        */

#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);
#endif
        String deviceId = json ["deviceId"];
        String action = json ["action"];

        if (action == "action.devices.commands.OnOff") { // Switch or Light
          String value = json ["value"]["on"];
          if (value == "true") {
            turnOn(deviceId);
          } else {
            turnOff(deviceId);
          }
        }
        else if (action == "action.devices.commands.ColorAbsolute") {
          int inOLE = json ["value"]["color"]["spectrumRGB"];

          Serial.println(inOLE);
          OLEtoRBG(inOLE);
          RGB_color(red, green, blue);

        }
        else if (action == "action.devices.commands.BrightnessAbsolute") {
          Serial.print("Brightness set to: ");
          brightness = json ["value"]["brightness"];
          Serial.print(brightness);
          Serial.println("%");
          brightness = brightness / 100;
          Serial.print("So, multiplier is: ");
          Serial.println(brightness);
          RGB_color(red, green, blue);
        }
        else if (action == "test") {
          Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
    default: break;
  }
}

void OLEtoRBG(int ole) {
  red = round((ole / 65536) % 256);
  green = round((ole / 256) % 256);
  blue = round(ole % 256);
}

void RGB_color(int red_light_value, int green_light_value, int blue_light_value)
{
  Serial.println("Setting color to: ");
  Serial.print(red);
  Serial.print(",");
  Serial.print(green);
  Serial.print(",");
  Serial.println(blue);
  Serial.print("At brightness: ");
  Serial.println(brightness);
  analogWrite(red_light_pin, (red_light_value * brightness));
  analogWrite(green_light_pin, (green_light_value * brightness));
  analogWrite(blue_light_pin, (blue_light_value * brightness));
}

void setup() {
  Serial.begin(115200);

  pinMode(red_light_pin, OUTPUT);
  pinMode(green_light_pin, OUTPUT);
  pinMode(blue_light_pin, OUTPUT);

  RGB_color(red, green, blue);

  WiFiMulti.addAP(MySSID, MyWifiPassword);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(MySSID);

  // Waiting for Wifi connect
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);

  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets
}

void loop() {
  webSocket.loop();

  if (isConnected) {
    uint64_t now = millis();

    // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
    if ((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
      heartbeatTimestamp = now;
      webSocket.sendTXT("H");
    }
  }
}
