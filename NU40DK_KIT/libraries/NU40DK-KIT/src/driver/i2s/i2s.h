#ifndef _OLED_H_
#define _OLED_H_

#include <Arduino.h>
#include <PDM.h>


class NU40DK_I2S 
{
  public:
    NU40DK_I2S();
    ~NU40DK_I2S();
    
    bool begin(void);

  private:
    bool is_init;
};



#endif