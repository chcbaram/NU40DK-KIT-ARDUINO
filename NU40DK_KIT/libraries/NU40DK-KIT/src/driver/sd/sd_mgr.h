#ifndef _SD_MGR_H_
#define _SD_MGR_H_

#include <Arduino.h>

#include "def.h"


bool sdInit(void);
bool sdBegin(void);
bool sdIsDetected(void);

#endif