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
#include <ESP32Servo.h>

const char* ssid = "iPhone-YJL";
const char* password = "12345678";
const char* canId = "CSHS1234";

String serverName = "https://70db-163-22-153-183.ngrok-free.app/";   // REPLACE WITH YOUR Raspberry Pi IP ADDRESS
//String serverName = "example.com";   // OR REPLACE WITH YOUR DOMAIN NAME


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


#define IR_GPIO_NUM     12 // 紅外線 PIN
#define SERVO_PIN       13 // 馬達 PIN

const int timerInterval = 30000;    // time between each HTTP POST image
unsigned long previousMillis = 0;   // last time image was sent
Servo servo1;

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
    config.jpeg_quality = 2;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  // 紅外線引腳設定
  pinMode(IR_GPIO_NUM, INPUT);
}

void loop() {
  String canCommand = getGarbageCanCommand(canId);
  if (canCommand == "" || canCommand == "full") {
    delay(10000);
    return;
  } else if (canCommand == "ready") {
    int ir_read = digitalRead(IR_GPIO_NUM);
    while (ir_read == 1) { // 紅外線讀到 1 代表沒有遮擋，繼續等待
      delay(1000);
      ir_read = digitalRead(IR_GPIO_NUM);
      Serial.print("ir_read=");
      Serial.println(ir_read);
    }
    String garbageClass = classifyGarbage();
    String rotateCommand = getRotateCommand(garbageClass);
    updateGarbageCanCommand(canId, rotateCommand);
  } else if (canCommand == "dump") {
    dumpGarbage();
    updateGarbageCanCommand(canId, "resume");
    delay(2000);
  }
}

void checkConnection() {
  WiFiClientSecure *client_s = new WiFiClientSecure;
  if(client_s) {
    client_s->setInsecure();
    String query = serverName + "/hello";
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

String classifyGarbage() {
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
    
  String garbageClass = "unknow";
  WiFiClientSecure *client_s = new WiFiClientSecure;
  if(client_s) {
    client_s->setInsecure();
    String query = serverName + "/classify-garbage";
    HTTPClient https;
    Serial.println("[POST] " + query);
    if (https.begin(*client_s, query)) {
      https.addHeader("Content-Type", "multipart/form-data;boundary=RandomNerdTutorials");
      int httpCode = https.POST(concatenated, totalLength);
      if (httpCode > 0) {
       Serial.printf("[STATUS CODE] %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          garbageClass = https.getString();
          Serial.printf("[RESPONSE] classify result:  %s\n", garbageClass);
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
  return garbageClass;
}

String getGarbageCanCommand(String id) {
  WiFiClientSecure *client_s = new WiFiClientSecure;
  String command = "";
  if(client_s) {
      client_s->setInsecure();
      String query = serverName + "/get-command?id=" + id;
      HTTPClient https;
      Serial.println("[GET] " + query);
      if (https.begin(*client_s, query)) {
      int httpCode = https.GET();
      if (httpCode > 0) {
      Serial.printf("[STATUS CODE] %d\n", httpCode);
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
              command = https.getString();
              Serial.printf("[RESPONSE] %s\n", command);
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
  return command;
}

String getRotateCommand(String garbageClass) {
  if (garbageClass == "plastic")
    return "rotate45";
  else if (garbageClass == "glass")
    return "rotate165";
  else if (garbageClass == "metal")
    return "rotate285";
  else
    return "rotate0";
}

void updateGarbageCanCommand(String id, String command) {
  WiFiClientSecure *client_s = new WiFiClientSecure;
  if(client_s) {
      client_s->setInsecure();
      String query = serverName + "/update-command?id=" + id + "&cmd=" + command;
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

void dumpGarbage() {
  servo1.attach(SERVO_PIN);
  for(int posDegrees = 30; posDegrees <= 120; posDegrees++) {
    servo1.write(posDegrees);
    Serial.println(posDegrees);
    delay(20);
  }
  delay(2000);
  for(int posDegrees = 120; posDegrees >= 30; posDegrees--) {
    servo1.write(posDegrees);
    Serial.println(posDegrees);
    delay(20);
  }
  delay(1000);
  servo1.detach();
}