//  by lingshunlab.com
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <TinyGPS++.h>

// Define the RX and TX pins for Serial 2
#define RXD2 16
#define TXD2 17

#define GPS_BAUD 9600

// Create an instance of the HardwareSerial class for Serial 2
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);


int counter = 0;

// 宣告 u8g2 實體
// 选择正确的屏幕芯片SH1106，分辨率128X64，接线方式I2C
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void setup() {
  // Serial Monitor
  Serial.begin(115200);
  
  // Start Serial 2 with the defined RX and TX pins and a baud rate of 9600
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Serial 2 started at 9600 baud rate");

  u8g2.begin(); // 初始化u8g2
  u8g2.enableUTF8Print();  // 启动UTF8，需要显示中文，必须要开启
}

void loop() {
  String gps_heading = "";
  String gps_body_1 = "";
  String gps_body_2 = "";

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  if (gps.location.isUpdated()) {
    gps.encode(gpsSerial.read());
    gps_heading = "GPS Located!";
    // char gpsData = gpsSerial.read();
    // Serial.println(gpsData);
    Serial.println(gps.location.lat(), 6);
    Serial.println(gps.location.lng(), 6);
    gps_body_1 = "lat:" + String(gps.location.lat(), 6);
    gps_body_2 = "lng:" + String(gps.location.lng(), 6);
  }
  else {
    gps_heading = "Search GPS";
    counter++;
    int dotNum = counter % 3 + 1;
    for (int i=0; i < dotNum; i++)
      gps_heading = gps_heading + ".";
  }

  // 定義字體，u8g2_font_unifont_t_chinese2 字庫只支援常見的 (簡體) 中文
  u8g2.setFont(u8g2_font_unifont_t_chinese2);
  u8g2.setFontDirection(0);
  u8g2.firstPage(); 
  do {
    u8g2.setCursor(8, 20);
    u8g2.print(gps_heading);
    u8g2.setCursor(8, 35);
    u8g2.print(gps_body_1);
    u8g2.setCursor(8, 50);
    u8g2.print(gps_body_2);
  } while ( u8g2.nextPage() );
  delay(1000);
}
