#include <WiFi.h>
#include "camera.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include "esp_heap_caps.h"

// ================= CAMERA / TIMING =================

unsigned long lastCapture = 0;
const int framerate = 15;
const float frameInterval = 1000.0 / framerate;

// ================= SERVER =================

const char* host = "192.168.100.150";
const int port = 8888;

const char* versionURL = "http://192.168.100.150/version";

// ================= VERSION =================

String currentVersion = "1.0.0";

unsigned long lastVersionCheck = 0;
const unsigned long versionCheckInterval = 60000; // 60s

// ================= WIFI =================

#define hostedSsid "McCabe Doorbell Camera"
#define hostedPassword "DoorBells"


uint8_t bssid[] = {0xA8, 0x5B, 0xF7, 0x51, 0xBE, 0x20};

// ================= PLACEHOLDER OTA =================

void updateFirmware(String newVersion) {
  Serial.println("UPDATE NEEDED -> " + newVersion);

  // keep simple for now
  // later you can plug in httpUpdate here
}

// ================= VERSION CHECK =================

void checkForUpdate() {
  return; // SKIP UPDATES FOR TESTING PURPOSES
  HTTPClient http;
  http.begin(versionURL);

  int code = http.GET();

  if (code != 200) {
    http.end();
    return; // silently skip if unreachable
  }

  String serverVersion = http.getString();
  serverVersion.trim();

  http.end();

  if (serverVersion.length() == 0) return;

  if (serverVersion != currentVersion) {
    updateFirmware(serverVersion);
  }
}

// ================= SETUP =================

void setup() {
  Serial.begin(115200);

  initCamera();
  delay(200);

  WiFi.begin("DeepintoHell", "mycowisdead", 0, bssid);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("...");
    delay(400);
  }

  Serial.println("connected!!!");
  Serial.println(WiFi.localIP());

  connectTCP();
}

// ================= LOOP =================

void loop() {
  unsigned long now = millis();

  // ===== FRAME CAPTURE =====
  if (now - lastCapture >= frameInterval) {
    lastCapture = now;
    takePicture();
  }

  // ===== VERSION CHECK (non-blocking) =====
  /*if (now - lastVersionCheck >= versionCheckInterval) {
    lastVersionCheck = now;
    checkForUpdate();
  }*/
}