/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-post-image-photo-server/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"

const char* ssid = "iPhone-YJL";
const char* password = "12345678";

String serverName = "127.0.0.1:9002";   // REPLACE WITH YOUR Raspberry Pi IP ADDRESS
//String serverName = "example.com";   // OR REPLACE WITH YOUR DOMAIN NAME

String serverPath = "/upload.php";     // The default serverPath should be upload.php

const int serverPort = 80;

String payloadPrefix = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"file\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
String payloadSurfix = "\r\n--RandomNerdTutorials--\r\n";

// CAMERA_MODEL_AI_THINKER
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

const int timerInterval = 30000;    // time between each HTTP POST image
unsigned long previousMillis = 0;   // last time image was sent

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // sendPhoto(); 
}

void loop() {
  unsigned long currentMillis = millis();
  if (!Serial.available()) {
    Serial.println("waiting input...");
    delay(1000);
    return;
  }
  char c = Serial.read();
  Serial.print("serial read: ");
  Serial.println(c);

  if (c == 't') {
    testHttpGet();
    return;
  }
  else if (c == 's') {
    testPhoto();
    previousMillis = currentMillis;
    return;
  }
}

void testHttpGet() {
  WiFiClientSecure *client_s = new WiFiClientSecure;
  if(client_s) {
    client_s->setInsecure();
    String query = "https://902c-2001-b400-e255-b5a6-2446-196f-633-368d.ngrok-free.app/hello";
    HTTPClient https;
    Serial.println("[GET] " + query);
    if (https.begin(*client_s, query)) {
      int httpCode = https.GET();
      if (httpCode > 0) {
       Serial.printf("[STATUS CODE] %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.printf("[RESPONSE] %s\n", payload);
        }
      }
      else {
        Serial.printf("[ERROR]: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  }
  else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
}

void testPhoto() {
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  size_t prefixLen = payloadPrefix.length();
  uint8_t *prefix = (uint8_t *)payloadPrefix.c_str();
  size_t fbLen = fb->len;
  uint8_t *fbBuf = fb->buf;
  size_t surfixLen = payloadSurfix.length();
  uint8_t *surfix = (uint8_t *)payloadSurfix.c_str();

  // Step 1: Calculate the total length
  size_t totalLength = fbLen + prefixLen + surfixLen;
  
  // Step 2: Allocate new buffer
  uint8_t* concatenated = (uint8_t*)malloc(totalLength);

  // Step 3: Copy each array into the buffer
  memcpy(concatenated, prefix, prefixLen);
  memcpy(concatenated + prefixLen, fbBuf, fbLen);
  memcpy(concatenated + prefixLen + fbLen, surfix, surfixLen);
    
  
  WiFiClientSecure *client_s = new WiFiClientSecure;
  if(client_s) {
    client_s->setInsecure();
    String query = "https://9ab7-2001-b400-e255-b5a6-b433-2ab5-f384-e41f.ngrok-free.app/test_upload";
    HTTPClient https;
    Serial.println("[POST] " + query);
    if (https.begin(*client_s, query)) {
      https.addHeader("Content-Type", "multipart/form-data;boundary=RandomNerdTutorials");
      int httpCode = https.POST(concatenated, totalLength);
      if (httpCode > 0) {
       Serial.printf("[STATUS CODE] %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.printf("[RESPONSE] %s\n", payload);
        }
      }
      else {
        Serial.printf("[ERROR]: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    }
  }
  else {
    Serial.printf("[HTTPS] Unable to connect\n");
  }
  esp_camera_fb_return(fb);
}