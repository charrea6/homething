#ifndef _DRAYTONSCR_H_
#define _DRAYTONSCR_H_
#include <stdint.h>
#include "iot.h"
#include "relay.h"

Relay_t *draytonSCRInit(uint8_t pin, const char *onSequence, const char *offSequence);
#endif