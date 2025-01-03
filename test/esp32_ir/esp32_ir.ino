#define IR_GPIO_NUM 13

void setup() {
    Serial.begin(115200);
    pinMode(IR_GPIO_NUM, INPUT);
}

void loop() {
  int ir_read = digitalRead(IR_GPIO_NUM);
  Serial.print("ir_read=");
  Serial.println(ir_read);
  if (ir_read == 1) {
    delay(20);
    return;
  }
}