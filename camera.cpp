#define CAMERA_MODEL_AI_THINKER

#include "camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <base64.h>
#include <WiFiUdp.h>
#include <WiFi.h>

#define MOTION_THRESHOLD 25
#define MOTION_PIXEL_COUNT 5000

WiFiUDP udpCamera;
const char* udpAddress = "192.168.100.112"; // receiver IP
const int udpPort = 1239;

camera_fb_t *prev_frame = nullptr;

// tcp definitions:
// Global TCP client and server details
WiFiClient tcpClient;
const char* tcpAddress = "192.168.100.150"; // IP of the machine running the Python script
const int tcpPort = 1242;                   // Port matching the Python script



void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;; // 800 x 600
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.jpeg_quality = 4;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available\
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}



void takePicture() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  //sendFrameUDP(fb);
  sendFrameTCP(fb, tcpClient);
  esp_camera_fb_return(fb);
}




bool waitForOkAck(unsigned long timeoutMs = 200) {
  char reply[16];
  unsigned long start = millis();

  while (millis() - start < timeoutMs) {
    int packetSize = udpCamera.parsePacket();
    if (packetSize) {
      int len = udpCamera.read(reply, sizeof(reply) - 1);
      if (len > 0) {
        reply[len] = '\0';

        if (strcmp(reply, "ok") == 0) {
          return true;
        }
      }
    }
    delay(1);
  }

  return false;
}

bool sendFrameUDP(camera_fb_t* fb) {
  const int chunkSize = 1400; // safe UDP size
  int totalSize = fb->len;
  int numChunks = (totalSize + chunkSize - 1) / chunkSize;

  const int MAX_RETRIES = 10;

  // -------------------------
  // 1. SEND HEADER WITH ACK
  // -------------------------
  bool headerOk = false;

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {

    udpCamera.beginPacket(udpAddress, udpPort);
    udpCamera.printf("==%d==", chunkSize);
    udpCamera.endPacket();

    if (waitForOkAck()) {
      headerOk = true;
      break;
    }
  }

  if (!headerOk) {
    Serial.println("Failed to get ACK for frame header");
    return false;
  }

  // -------------------------
  // 2. SEND FRAME CHUNKS
  // -------------------------
  for (int i = 0; i < numChunks; i++) {
    int offset = i * chunkSize;
    int len = min(chunkSize, totalSize - offset);

    udpCamera.beginPacket(udpAddress, udpPort);

    // chunk index (for reassembly)
    udpCamera.write((uint8_t*)&i, sizeof(i));

    // image data
    udpCamera.write(fb->buf + offset, len);

    udpCamera.endPacket();

    delay(2);
  }

  return true;
}

// =========== TCP STUFF

// Function to establish or check the TCP connection
bool connectTCP() {
  // If we are already connected, just return true
  if (tcpClient.connected()) {
    return true;
  }

  Serial.printf("Attempting to connect to TCP server at %s:%d...\n", tcpAddress, tcpPort);
  
  // Attempt to connect (timeout is handled automatically by the WiFiClient library)
  if (tcpClient.connect(tcpAddress, tcpPort)) {
    Serial.println("TCP Connection successful!");
    
    // Optional: disable Nagle's algorithm to reduce latency for live streaming
    tcpClient.setNoDelay(true); 
    
    return true;
  } else {
    Serial.println("TCP Connection failed!");
    return false;
  }
}

// Pass an already connected WiFiClient object to keep the connection alive between frames
bool sendFrameTCP(camera_fb_t* fb, WiFiClient& tcpClient) {
  if (!tcpClient.connected()) {
    Serial.println("TCP Client not connected!");
    connectTCP();
    return false;
  }

  uint32_t frameLen = fb->len;

  if (tcpClient.write((uint8_t*)&frameLen, sizeof(frameLen)) != sizeof(frameLen)) {
    Serial.println("Failed to send frame length");
    tcpClient.stop();
    return false;
  }

  const int chunkSize = 1024; // smaller = more stable
  size_t totalSent = 0;
  uint32_t start = millis();

  while (totalSent < fb->len) {

    if (!tcpClient.connected()) {
      Serial.println("TCP disconnected mid-frame");
      connectTCP();
      return false;
    }

    int toSend = min(chunkSize, (int)(fb->len - totalSent));
    int sent = tcpClient.write(fb->buf + totalSent, toSend);

    if (sent > 0) {
      totalSent += sent;
      start = millis(); // reset timeout on progress
    } else {
      // no progress → wait briefly instead of freezing CPU
      delay(1);
    }

    // HARD timeout protection (prevents freeze)
    if (millis() - start > 2000) {
      Serial.println("TCP send timeout!");
      tcpClient.stop();
      connectTCP();
      return false;
    }
  }

  tcpClient.flush();
  return true;
}