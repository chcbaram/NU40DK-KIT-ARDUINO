#ifndef _DEF_H_
#define _DEF_H_


#include <Arduino.h>


#define _USE_HW_I2S
#define      HW_I2S_LCD             0

#define _USE_HW_MIXER
#define      HW_MIXER_MAX_CH        4
#define      HW_MIXER_MAX_BUF_LEN   (48*2*4*4) // 48Khz * Stereo * 4ms * 4



#endif