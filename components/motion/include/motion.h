#ifndef _MOTION_H_
#define _MOTION_H_
#include "iot.h"

typedef struct Motion{
    iotElement_t element;
}Motion_t;

void motionInit(Motion_t *motion, int pin);
#endif