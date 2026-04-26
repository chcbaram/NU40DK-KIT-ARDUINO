#include "button.h"
#include "driver/cli/cli.h"


#define BUTTON_MAX_COUNT      (BTN_EX_CH4 + 1) 
#define DEBOUNCE_INTERVAL_MS  50


typedef struct
{
  bool last_stable_state; // 확정된 상태
  bool last_raw_state;    // 직전에 읽은 생(raw) 상태
  uint32_t last_time;     // 상태가 변하기 시작한 시점 (ms)
  bool event_flag;        // 눌림 이벤트 발생 플래그
} ButtonObj_t;


static void button_thread(void* arg);
static void cliCmd(cli_args_t *args);
static bool buttonPinRead(ButtonName_t btn_name);
static bool buttonInit(void);
static bool buttonIsPressed(ButtonName_t btn_name);
static bool buttonIsPressedEvent(ButtonName_t btn_name);
static bool buttonClearFlag(void);

static TaskHandle_t  button_handle;
static ButtonObj_t button_list[BUTTON_MAX_COUNT];
static uint32_t button_pin[BUTTON_MAX_COUNT] = 
{
  _PINNUM(0, 11),
  _PINNUM(0, 12),
  _PINNUM(0, 24),
  _PINNUM(0, 25),

  _PINNUM(0, 31),
  _PINNUM(1, 7),
  _PINNUM(1, 6),
  _PINNUM(1, 5),
};


Button::Button()
{
    
}

Button::~Button()
{
    
}

bool Button::begin(void)
{
  return buttonInit();
}

bool Button::isPressed(ButtonName_t btn_name)
{
  return buttonIsPressed(btn_name);
}

bool Button::isPressedEvent(ButtonName_t btn_name)
{
  return buttonIsPressedEvent(btn_name);
}

bool Button::clear(void)
{
  buttonClearFlag();
  return true;
}

bool buttonInit(void)
{
  for (int i = 0; i < BUTTON_MAX_COUNT; i++)
  {
    button_list[i].last_stable_state = buttonPinRead((ButtonName_t)i); // 현재 핀 상태로 초기값 설정
    button_list[i].last_raw_state    = button_list[i].last_stable_state;
    button_list[i].last_time         = millis();
    button_list[i].event_flag        = false;

    pinMode(button_pin[i], INPUT_PULLUP);
  }

  xTaskCreate(button_thread, "button", 1*1024, NULL, TASK_PRIO_LOW, &button_handle);
  
  cliAdd("button", cliCmd);

  return true;
}

bool buttonClearFlag(void)
{
  for (int i = 0; i < BUTTON_MAX_COUNT; i++)
  {
    button_list[i].event_flag        = false;
  }
  return true;
}

bool buttonPinRead(ButtonName_t btn_name)
{
  if (btn_name >= BUTTON_MAX_COUNT)
    return false;

  return !digitalRead(button_pin[btn_name]);
}

bool buttonIsPressed(ButtonName_t btn_name)
{
  if (btn_name >= BUTTON_MAX_COUNT)
    return false;
  return button_list[btn_name].last_stable_state;
}

bool buttonIsPressedEvent(ButtonName_t btn_name)
{
  if (btn_name >= BUTTON_MAX_COUNT)
    return false;

  // 원자적(Atomic) 처리가 필요할 수 있으나, 일반적인 플래그 방식 적용
  bool event = button_list[btn_name].event_flag;
  button_list[btn_name].event_flag = false;
  return event;
}

void button_thread(void* arg)
{
  (void) arg;

  while (1)
  {
    uint32_t current_time = millis();

    for (int i = 0; i < BUTTON_MAX_COUNT; i++)
    {
      bool current_raw = buttonPinRead((ButtonName_t)i);

      // 1. 현재 읽은 값(raw)이 직전 루프의 값과 달라졌다면? (변화의 시작)
      if (current_raw != button_list[i].last_raw_state)
      {
        button_list[i].last_time = current_time; // 시간 기록 업데이트
      }

      // 2. 마지막 변화로부터 설정한 디바운스 시간 이상 경과했는지 확인
      if ((current_time - button_list[i].last_time) >= DEBOUNCE_INTERVAL_MS)
      {
        // 3. 안정된 상태가 현재 확정된 상태와 다르다면 상태 업데이트
        if (current_raw != button_list[i].last_stable_state)
        {
          // Falling -> Rising (Pressed Event) 감지
          if (current_raw == true && button_list[i].last_stable_state == false)
          {
            button_list[i].event_flag = true;
          }
          button_list[i].last_stable_state = current_raw;
        }
      }

      // 현재 상태를 다음 루프를 위해 저장
      button_list[i].last_raw_state = current_raw;
    }

    // 스레드 점유율을 낮추기 위한 최소한의 휴식 (성능에 영향 미미)
    delay(1);
  }
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show") == true)
  {
    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_COUNT; i++)
      {
        cliPrintf("%d", (int)buttonIsPressed((ButtonName_t)i));
      }
      cliPrintf("\n");
      delay(100);
    }
    ret = true;
  }  

  if (args->argc == 1 && args->isStr(0, "event") == true)
  {
    buttonClearFlag();
    
    while(cliKeepLoop())
    {
      for (int i=0; i<BUTTON_MAX_COUNT; i++)
      {
        if (buttonIsPressedEvent((ButtonName_t)i))
        {
          cliPrintf("%d PressedEvent\n", i);
        }
      }      
      delay(10);
    }
    ret = true;
  }  

  if (!ret)
  {
    cliPrintf("button info\n");
    cliPrintf("button show\n");
    cliPrintf("button event\n");
  }
}