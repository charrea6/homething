#ifndef _SWITCH_H_
#define _SWITCH_H_
#include "notifications.h"

int switchInit(void);
Notifications_ID_t switchAdd(int pin, uint8_t noiseFilter);
void switchStart(void);
#endif
