#ifndef _BUTTON_MGR_H_
#define _BUTTON_MGR_H_

#include <Arduino.h>


typedef enum
{
  BTN_IN_CH1,
  BTN_IN_CH2,
  BTN_IN_CH3,
  BTN_IN_CH4,

  BTN_EX_CH1,
  BTN_EX_CH2,
  BTN_EX_CH3,
  BTN_EX_CH4,
  BTN_MAX_CH
} ButtonName_t;


class Button 
{
  public:
    Button();
    ~Button();
    
    bool begin(void);
    bool isPressed(ButtonName_t btn_name);
    bool isPressedEvent(ButtonName_t btn_name);
    bool clear(void);

  private:
    bool is_init;
};

#endif