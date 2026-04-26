#include "cli.h"



void cli_thread(void* arg);


static TaskHandle_t  cli_handle;
static bool is_begin = false;


bool cliMgrInit(void)
{
  xTaskCreate(cli_thread, "cli", 4*1024, NULL, TASK_PRIO_LOW, &cli_handle);
  
  return true;
}

void cli_thread(void* arg)
{
  (void) arg;

  while(1)
  {
    cliMain();
    delay(5);
  }
}