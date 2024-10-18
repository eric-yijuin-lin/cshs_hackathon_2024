#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// 馬達 PINstatic
const int servoPin = 13;
const char* ssid = "iPhone-YJL";
const char* password = "12345678";
const char* canId = "CSHS1234";

String serverName = "https://70db-163-22-153-183.ngrok-free.app/";   // REPLACE WITH YOUR Raspberry Pi IP ADDRESS
//String serverName = "example.com";   // OR REPLACE WITH YOUR DOMAIN NAME

// 搭配servo1.attach(servoPin, 1000, 2000) 時，以下的參數可以達到近似的角度控制
static const int servoRotateSpeed = 130;
static const int servoStopSpeed = 90; 
static const double time_of_degree = (double)2200 / 360; 
Servo servo1;

void setup() {
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
}

void loop() {
  String canCommand = getGarbageCanCommand(canId);
  if (canCommand == "" || canCommand == "full") {
    delay(10000);
    return;
  } else if (canCommand.indexOf("rotate") > -1) {
    rotateBase(canCommand);
  }
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

void rotateBase(String rotateCommand) {
    rotateCommand.replace("rotate", ""); // 刪除 rotate 文字，取出角度
    int targetDegree = rotateCommand.toInt();
    double goTime = targetDegree * time_of_degree;
    double backTime = (360 - targetDegree) * time_of_degree;
    Serial.print("targetDegree: ");
    Serial.print(targetDegree);
    Serial.print("goTime: ");
    Serial.print(goTime);
    Serial.print("backTime: ");
    Serial.println(backTime);

    servo1.attach(servoPin, 1000, 2000); // 為避免用壞馬達，把 min max 設定在 1000~2000
    servo1.write(servoRotateSpeed);
    delay(goTime);

    servo1.write(servoStopSpeed);
    updateGarbageCanCommand(canId, "dump");

    String nextCommand = "";
    while (nextCommand != "resume") {
      nextCommand = getGarbageCanCommand(canId);
    }

    servo1.write(servoRotateSpeed);
    delay(backTime);
    servo1.write(servoStopSpeed);
    delay(2000);
    servo1.detach();
}
