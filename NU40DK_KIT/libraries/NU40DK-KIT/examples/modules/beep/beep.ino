#include <NU40DK.h>



NU40DK nu40;



void setup() {

  Serial1.begin(115200);

  nu40.begin(115200);  
  nu40.cliBegin(Serial1);
}


void loop() {
  static uint32_t pre_time = 0;
  static uint32_t h_beep[4] = {0, };
  static uint32_t h_file = 0;

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

      if (i == BTN_EX_CH1)
        nu40.i2s.beep(NOTE_C4, 200);
      
      if (i == BTN_EX_CH2)
        nu40.i2s.beep(NOTE_D4, 200);
      
      if (i == BTN_EX_CH3)
        nu40.i2s.beep(NOTE_E4, 200);

      if (i == BTN_EX_CH4)
        nu40.i2s.beep(NOTE_F4, 200);
    }
  }
  delay(10);  
}
