#ifndef _MOTION_H_
#define _MOTION_H_
#include "iot.h"

typedef struct Motion{
    char name[8];
    iotElement_t *element;
    iotElementPub_t *pub;
}Motion_t;

void motionInit(Motion_t *motion, int pin);
#endif