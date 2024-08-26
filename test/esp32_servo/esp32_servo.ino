/*********
 * 手邊有的 & 適合做垃圾桶的伺服馬達是 360度的 MG996R
 * 無法控制角度，所以目前用速度與 delay 時間硬幹
 * 目前測試， servo.write(130) 配合 delay (320) 差不多剛好 1 圈
*********/

#include <ESP32Servo.h>

static const int servoPin = 13;

Servo servo1;
int test = 100;

void setup() {
  Serial.begin(115200);
  servo1.attach(servoPin, 1000, 2000);
  for (int i=0; i<10; i++) {
    Serial.println("hello, ");
    Serial.println(i);
  }
}

void loop() {
  for (int i=0; i<10; i++) {
    if (i % 2 == 0) {
      servo1.write(130);
      Serial.println("going~");
      delay(3200);
    }
    else {
      servo1.write(100);
      Serial.println("take a break");
      delay(3000);
    }
  }
}