#include "sd_mgr.h"
#include <SD.h>
#include <SPI.h>
#include "driver/cli/cli.h"


#define SD_SCK      27  
#define SD_MISO     4   
#define SD_MOSI     26  
#define SD_CS       23  
#define SD_DEC      _PINNUM(1, 15)



static void sd_thread(void* arg);
static void cliCmd(cli_args_t *args);

static TaskHandle_t  sd_handle;
static bool is_begin = false;



bool sdInit(void)
{    
  pinMode(SD_DEC, INPUT_PULLUP);

  SPI.setPins(SD_MISO, SD_SCK, SD_MOSI);
  SPI.begin();

  xTaskCreate(sd_thread, "sd", 1*1024, NULL, TASK_PRIO_LOW, &sd_handle);
  
  cliAdd("sd", cliCmd);
  return true;
}

bool sdBegin(void)
{
  bool ret = true;

  if (!SD.begin(32000000UL, SD_CS))
    ret = false;

  is_begin = ret;

  return ret;
}

bool sdIsDetected(void)
{
  return digitalRead(SD_DEC);
}

void sd_thread(void* arg)
{
  (void) arg;

  while(1)
  {
    if (is_begin && sdIsDetected() == false)
    {
      is_begin = false;
    }
    delay(10);
  }
}

void sdPrintDirectory(File dir, int numTabs)
{
  while (true)
  {
    File entry = dir.openNextFile();
    if (!entry)
    {
      // 더 이상 파일이 없으면 탈출
      break;
    }

    // 들여쓰기로 계층 구조 표시
    for (uint8_t i = 0; i < numTabs; i++)
    {
      cliPrintf("\t");
    }

    cliPrintf("%s", entry.name());

    if (entry.isDirectory())
    {
      cliPrintf("/\n");
      // sdPrintDirectory(entry, numTabs + 1); // 폴더 내부로 진입 (재귀)
    }
    else
    {
      // 파일인 경우 크기 출력
      cliPrintf("\t\t");
      cliPrintf("%d\n", entry.size());
    }
    entry.close();
  }
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    cliPrintf("is_detected : %d\n", sdIsDetected()); 
    cliPrintf("is_begin    : %d\n", is_begin); 
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "dir") == true)
  {
    if (sdIsDetected() && is_begin)
    {
      File root = SD.open("/"); 
      
      sdPrintDirectory(root, 0);      
    }
    else
    {

    }
    ret = true;
  }  

  if (!ret)
  {
    cliPrintf("sd info\n");
    cliPrintf("sd dir\n");
  }
}