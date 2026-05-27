#include <WiFi.h>
#include "camera.h"
#include <Arduino.h>
#include "esp_heap_caps.h"


// camera definitions

unsigned long lastCapture = 0;
const unsigned long interval = 1000; // 1 second

const int framerate = 15;
int currentFramerate = 0;
const float frameInterval = 1000.0 / framerate; // ms per frame


const char* host = "192.168.100.112"; // IP address of the computer/server receiving audio
const int port = 8888;

#define hostedSsid "McCabe Doorbell Camera"
#define hostedPassword "DoorBells"

int connectivity = 0;
int printConnectivity = 30;

void setup() {
  Serial.begin(115200);
  initCamera();
  delay(200);

  WiFi.begin("DeepintoHell", "mycowisdead");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("...");
    delay(400);
  }
  Serial.println("connected!!!");
  Serial.println(WiFi.localIP());

  connectTCP();
}


void loop() {

  unsigned long now = millis();
  
  //debugLog("Now playing real video!");
  float elapsed = now - lastCapture;

  // check if it's time for the next frame
  if (elapsed >= frameInterval) {
    lastCapture = now;
    connectivity++;
    if (connectivity > printConnectivity) 
    {
      connectivity = 0;
      Serial.print("RSSI: ");
      Serial.println(WiFi.RSSI());
    }

    takePicture();  // your picture-taking function
  }
}
