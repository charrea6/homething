#ifndef _DRAYTONSCR_H_
#define _DRAYTONSCR_H_
#include <stdint.h>
#include "iot.h"


void draytonSCRInit(uint8_t pin, const char *onSequence, const char *offSequence);
void draytonSCRSetState(bool on);
#endif