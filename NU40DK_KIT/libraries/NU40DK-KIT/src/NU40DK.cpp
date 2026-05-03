#include "NU40DK.h"





NU40DK::NU40DK(void) : sd(SDLib::SD)
{
  is_init = false;
  is_cli_begin = false;
}

NU40DK::~NU40DK()
{ 
}

bool NU40DK::begin(int baud)
{
  bool ret = false;
  bool ret_log = false;

  if (is_init == true)
  {
    return true;
  }

  driverInit();

  Serial.begin(baud);
  

  pinMode(PIN_LED1, OUTPUT);
  ledOff();

  
  ret = lcd.begin();
  lcd.clearDisplay();
  lcd.setTextSize(1);
  lcd.setTextWrap(false);
  lcd.setCursor(0, 0);

  lcd.logPrint("NU40DK Begin...");
  lcd.logPrint(" ");
  
  btn.begin();
  i2s.begin();

  cliMgrInit();
  sdInit();


  if (sdIsDetected())
  {
    lcd.logPrint("OK SD Detected");
    sdBegin();
  }
  else
  {
    lcd.logPrint("E_ SD Empty");
  }

  for (int i=0; i<8; i++)
  {
    pre_time[i] = millis();
  }


  is_init = true;
  return true;
}

bool NU40DK::printInitLog(const char *str_msg, bool ret)
{
  if (ret == true)
  {
    Serial.print("[OK] ");
    if (lcd.isInit())
    {
      lcd.print("[OK] ");
    }
  }
  else
  {
    Serial.print("[E_] ");
    if (lcd.isInit())
    {
      lcd.print("[E_] ");
    }
  }
  
  Serial.println(str_msg);
  if (lcd.isInit())
  {
    lcd.println(str_msg);
  }

  return ret;
}

bool NU40DK::update(void)
{
  uint32_t cur_time;

  cur_time = millis();

  if (cur_time-pre_time[0] >= 10)
  {
    pre_time[0] = cur_time;
  }

  return true;
}

void NU40DK::ledOn(void)
{  
  digitalWrite(PIN_LED1, LOW);
}

void NU40DK::ledOff(void)
{
  digitalWrite(PIN_LED1, HIGH);
}

void NU40DK::ledToggle(void)
{
  digitalWrite(PIN_LED1, !digitalRead(PIN_LED1));
}

bool NU40DK::cliBegin(Stream &port)
{
  cliInit(port);
  cliOpen();  

  is_cli_begin = true;

  return true;
}

void taskUpdate(void *pvParameters) 
{
  NU40DK *p_class =  (NU40DK *)pvParameters;
  
  for (;;)
  {
    p_class->update();
    vTaskDelay(2);
  }
}