#ifndef NU40DK_KIT_H_
#define NU40DK_KIT_H_

#include <Arduino.h>
#include <SD.h>

#include "driver/driver.h"


#define NU40DK_VER_STR            "NU40DK V260426R1"



class NU40DK
{
  public:
    NU40DK();
    ~NU40DK();
    
    OLed            lcd;
    SDLib::SDClass& sd;
    Button          btn;

    
    bool begin(int baud);
    bool update(void);

    void ledOn(void);
    void ledOff(void);
    void ledToggle(void);

    bool cliBegin(Stream &port);

  private:
    bool printInitLog(const char *str_msg, bool ret);
    bool is_init;
    bool is_cli_begin;

    uint32_t pre_time[8];
};

#endif 
