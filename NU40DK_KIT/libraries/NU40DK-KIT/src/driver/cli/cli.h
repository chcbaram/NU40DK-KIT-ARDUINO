#ifndef CLI_H_
#define CLI_H_


#include "Arduino.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>




#define HW_CLI_CMD_LIST_MAX 64
#define HW_CLI_CMD_NAME_MAX 16
#define HW_CLI_LINE_HIS_MAX 8
#define HW_CLI_LINE_BUF_MAX 64

#define CLI_CMD_LIST_MAX      HW_CLI_CMD_LIST_MAX
#define CLI_CMD_NAME_MAX      HW_CLI_CMD_NAME_MAX

#define CLI_LINE_HIS_MAX      HW_CLI_LINE_HIS_MAX
#define CLI_LINE_BUF_MAX      HW_CLI_LINE_BUF_MAX




typedef struct
{
  uint16_t   argc;
  char     **argv;

  int32_t  (*getData)(uint8_t index);
  float    (*getFloat)(uint8_t index);
  char    *(*getStr)(uint8_t index);
  bool     (*isStr)(uint8_t index, const char *p_str);
} cli_args_t;


bool cliInit(Stream &port);
bool cliOpen(uint8_t ch = 0, uint32_t baud = 115200);
bool cliIsBusy(void);
bool cliOpenLog(uint8_t ch, uint32_t baud);
bool cliMain(void);
void cliPrintf(const char *fmt, ...);
bool cliAdd(const char *cmd_str, void (*p_func)(cli_args_t *));
bool cliKeepLoop(void);
void cliPutch(uint8_t data);
uint8_t  cliGetPort(void);
uint32_t cliAvailable(void);
uint8_t  cliRead(void);
uint32_t cliWrite(uint8_t *p_data, uint32_t length);
bool cliRunStr(const char *fmt, ...);
void cliShowCursor(bool visibility);
void cliMoveUp(uint8_t y);
void cliMoveDown(uint8_t y);
void cliBegin(void);

#endif
