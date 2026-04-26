#include <EduBot.h>




void setup() {
  // put your setup code here, to run once:
  edubot.begin(115200);                                   
}

void loop() {
  // put your main code here, to run repeatedly:
  static uint32_t pre_time; 


  if (millis()-pre_time >= 50)
  {
    pre_time = millis();

    Serial.print("gX : ");
    Serial.print(edubot.imu.getGyroX());
    Serial.print("\tgY : ");
    Serial.print(edubot.imu.getGyroY());

    Serial.print("\tgZ : ");
    Serial.print(edubot.imu.getGyroZ());
    Serial.println();    
  }   
}