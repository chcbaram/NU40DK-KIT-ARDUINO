#include <NU40DK.h>



NU40DK nu40;



void setup() {

  Serial1.begin(115200);

  nu40.begin(115200);  
  nu40.cliBegin(Serial1);
}


void loop() {
  static uint32_t pre_time = 0;

  if (millis()-pre_time >= 500)
  {
    pre_time = millis();
    nu40.ledToggle();
  }  
  
  for (int i=0; i<BTN_MAX_CH; i++)
  {
    if (nu40.btn.isPressedEvent((ButtonName_t)i))
    {
      nu40.lcd.logPrintf("%d Pressed", i);
    }
  }
  delay(10);  
}
