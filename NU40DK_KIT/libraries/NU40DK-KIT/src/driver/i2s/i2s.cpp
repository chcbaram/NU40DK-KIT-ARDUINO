#include "i2s.h"


#define I2S_LRC_PIN  40  
#define I2S_BCLK_PIN 41  
#define I2S_DIN_PIN  19  
#define I2S_SD_MODE  42  




NU40DK_I2S::~NU40DK_I2S()
{
    
}

bool NU40DK_I2S::begin(void)
{
  bool ret;  

  is_init = true;
  return ret;
}
