/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-servo-motor-web-server-arduino-ide/
  Based on the ESP32Servo Sweep Example
*********/

#include <ESP32Servo.h>

static const int servoPin = 13;
static const int servoRotateSpeed = 130;
static const int servoStopSpeed = 90; // 搭配servo1.attach(servoPin, 1000, 2000) 時，speed = 90 會停下
static const double time_of_degree = (double)2200 / 360;

Servo servo1;
int test = 100;

void setup() {
  Serial.begin(115200);
  servo1.attach(servoPin, 1000, 2000); // 為避免用壞馬達，把 min max 設定在 1000~2000
}

// 360 度馬達
void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    int targetDegree = 0;
    switch(c) {
      case 'w': targetDegree = 0; break;
      case 'a': targetDegree = 90; break;
      case 's': targetDegree = 180; break;
      case 'd': targetDegree = 270; break;
      default: targetDegree = 0; break;
    }

    double goTime = targetDegree * time_of_degree;
    double backTime = (360 - targetDegree) * time_of_degree;
    Serial.print("targetDegree: ");
    Serial.print(targetDegree);
    Serial.print("goTime: ");
    Serial.print(goTime);
    Serial.print("backTime: ");
    Serial.println(backTime);

    servo1.write(servoRotateSpeed);
    delay(goTime);

    servo1.write(servoStopSpeed);
    delay(2000);

    servo1.write(servoRotateSpeed);
    delay(backTime);
    servo1.write(servoStopSpeed);
    delay(20);
  }
}


// 180 度馬達
// void loop() {
//   for(int posDegrees = 0; posDegrees <= 180; posDegrees += 2) {
//     servo1.write(posDegrees);
//     Serial.println(posDegrees);
//     delay(50);
//   }

//   for(int posDegrees = 180; posDegrees >= 0; posDegrees -= 2) {
//     servo1.write(posDegrees);
//     Serial.println(posDegrees);
//     delay(50);
//   }
// }