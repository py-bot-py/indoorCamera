#define CAMERA_MODEL_AI_THINKER

#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFiUdp.h>
#include "FS.h"
#include "SD_MMC.h"
#include <WiFi.h>

#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// ===== Hardcoded ESP32-CAM (AI-Thinker) pin map =====
// This completely bypasses any Arduino macro-scoping bugs
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#endif

// ======================================================
// Globals (defined in your .cpp / .ino)
// ======================================================
extern bool motionDetected;
extern bool motionDetection;

extern WiFiUDP udpCamera;
extern const char* udpAddress;
extern const int udpPort;

extern camera_fb_t *prev_frame;

extern int prebufferFrame;

extern File videoFile;

extern int cameraMode;

// ======================================================
// Camera / system functions
// ======================================================
void initCamera();
void takePicture();
bool waitForOkAck(unsigned long timeoutMs);
bool sendFrameUDP(camera_fb_t* fb);

bool sendFrameTCP(camera_fb_t* fb, WiFiClient& tcpClient);
bool connectTCP();

#endif