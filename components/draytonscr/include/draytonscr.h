#ifndef _DRAYTONSCR_H_
#define _DRAYTONSCR_H_
#include <stdint.h>
#include "iot.h"

typedef struct DraytonSCR DraytonSCR_t;

DraytonSCR_t *draytonSCRInit(uint8_t pin, const char *onSequence, const char *offSequence);
void draytonSCRSetState(DraytonSCR_t *draytonSCR, bool on);
bool draytonSCRIsOn(DraytonSCR_t *draytonSCR);
#endif