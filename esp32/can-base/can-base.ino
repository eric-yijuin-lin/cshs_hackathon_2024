#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>

// 馬達 PINstatic
const int servoPin = 13;
const char* ssid = "iPhone-YJL";
const char* password = "12345678";
const char* canId = "CSHS1234";
WiFiClientSecure *client_s = nullptr;

String serverName = "https://8f03-163-22-153-229.ngrok-free.app";   // REPLACE WITH YOUR Raspberry Pi IP ADDRESS
//String serverName = "example.com";   // OR REPLACE WITH YOUR DOMAIN NAME

// 搭配servo1.attach(servoPin, 1000, 2000) 時，以下的參數可以達到近似的角度控制
int servoRotateSpeed = 120;
int servoStopSpeed = 90; 
double time_of_degree = (double)2850 / 360; 
Servo servo1;


// GPS 模組也用序列 (Serial) 通訊，這裡選用 16號與 17 號腳位做 GPS 資料的傳輸
const int RXD2 = 16;
const int TXD2 = 17;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);
String gps_lat = "23.76170563"; // 緯度
String gps_lng = "120.6814866"; // 經度

// 超音波模組腳位與距離計算變數
const int trigPin1 = 5;
const int echoPin1 = 18;
const int trigPin2 = 25;
const int echoPin2 = 26;
const int trigPin3 = 22;
const int echoPin3 = 23;
const float soundSpeed = 0.034;
long duration;
float distanceCm;

// 暫停各項工作的相關變數
unsigned long lastStatusUpdateTime = 0;  // 最後一次更新垃圾桶狀態
const unsigned long statusUpdateInterval = 10000;  // 10 秒 (10,000 毫秒)
const int httpDelay = 2000;

void setup() {
  // Serial 1, 115200Hz, debug 訊息用
  Serial.begin(115200);
  // Serial 2, 9600Hz, GPS 模組用
  gpsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

  // 超音波模組設定
  pinMode(trigPin1, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin1, INPUT); // Sets the echoPin as an Input
  pinMode(trigPin2, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin2, INPUT); // Sets the echoPin as an Input
  pinMode(trigPin3, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin3, INPUT); // Sets the echoPin as an Input

  // wifi 設定
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
  client_s = new WiFiClientSecure;
  client_s->setInsecure();
}

void loop() {
  String canCommand = getGarbageCanCommand(canId);
  if (canCommand == "" || canCommand == "full") {
    delay(10000);
    return;
  } else if (canCommand.indexOf("rotate") > -1) {
    rotateBase(canCommand);
  } else if (canCommand.indexOf("degreeTime") > -1) {
    setTimeOfRoteateDegree(canCommand);
  } else {
    float garbageDistance1 = getGarbageDistance(trigPin1, echoPin1);
    Serial.print("garbage distance1 = ");
    Serial.println(garbageDistance1);
    float garbageDistance2 = getGarbageDistance(trigPin2, echoPin2);
    Serial.print("garbage distance2 = ");
    Serial.println(garbageDistance2);
    float garbageDistance3 = getGarbageDistance(trigPin3, echoPin3);
    Serial.print("garbage distance3 = ");
    Serial.println(garbageDistance3);

    Serial.println("try getting location by GPS...");
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
    if (gps.location.isUpdated()) {
      gps.encode(gpsSerial.read());
      // char gpsData = gpsSerial.read();
      // Serial.println(gpsData);
      Serial.println(gps.location.lat(), 6);
      Serial.println(gps.location.lng(), 6);
      gps_lat = String(gps.location.lat(), 6);
      gps_lng = String(gps.location.lng(), 6);
    } else {
      Serial.print("Failed to get location.");
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastStatusUpdateTime >= statusUpdateInterval) {
      // Perform the task
      updateCanStatus(canId, gps_lat, gps_lng, garbageDistance1, garbageDistance2, garbageDistance3);
      // Update the last execution time to the current time
      lastStatusUpdateTime = currentTime;
      if (garbageDistance1 < 10 || garbageDistance2 < 10 || garbageDistance3 < 10) {
        updateGarbageCanCommand(canId, "full");
        notifyGarbageClean(canId);
        return;
      }
    }
  }
}

String getGarbageCanCommand(String id) {
  String command = "";
  if(client_s) {
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
  delay(httpDelay);
  return command;
}

void updateGarbageCanCommand(String id, String command) {
  if(client_s) {
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
  delay(httpDelay);
}

void notifyGarbageClean(String id) {
  if(client_s) {
    String query = serverName + "/notify-clean?id=" + id;
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
  delay(httpDelay);
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

    updateGarbageCanCommand(canId, "ready");
}

float getGarbageDistance(int trigPin, int echoPin) {
  // 對發射腳位設定低電壓，以重設超音波發設
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // 對發射腳位設定低 10 微秒 (1/1000000 秒) 的高電壓，發送超音波
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  // 發射完關閉
  digitalWrite(trigPin, LOW);
  
  // 從回音腳位讀取訊號，獲得超音波傳播的時間，以微秒為單位
  duration = pulseIn(echoPin, HIGH);
  
  // 從傳播時間計算距離 (公分)
  distanceCm = duration * soundSpeed/2;

  return distanceCm;
}

void setTimeOfRoteateDegree(String canCommand) {
    // 設定 time of degree
    canCommand.replace("degreeTime", "");
    int degreeTime = canCommand.toInt();
    time_of_degree = (double)degreeTime / 360;
    // 以 90 度做測試
    double goTime = 90 * time_of_degree;
    double backTime = 270 * time_of_degree;

    servo1.attach(servoPin, 1000, 2000); // 為避免用壞馬達，把 min max 設定在 1000~2000
    servo1.write(servoRotateSpeed);
    delay(goTime);

    servo1.write(servoStopSpeed);
    delay(2000);

    servo1.write(servoRotateSpeed);
    delay(backTime);
    servo1.write(servoStopSpeed);
    delay(2000);
    servo1.detach();
    updateGarbageCanCommand(canId, "ready");
}

void updateCanStatus(String id, String lat, String lng, float d1, float d2, float d3) {
  if(client_s) {
    String query = serverName + "/update-can?"
      + "id=" + id 
      + "&lat=" + gps_lat 
      + "&lng=" + gps_lng 
      + "&d1=" + String(d1, 2) 
      + "&d2=" + String(d2, 2) 
      + "&d3=" + String(d3, 2);
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
