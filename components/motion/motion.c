#include <stdio.h>

#include "motion.h"
#include "switch.h"

static int motionCount = 0;

static const char motionDetected[] = "motion detected";
static const char motionStopped[] = "motion stopped";

static void motionCallback(void *userData, int state)
{
    Motion_t *motion = userData;
    iotValue_t value;
    value.s = state?motionDetected:motionStopped;
    iotElementPubUpdate(&motion->pub, value);
}

void motionInit(Motion_t *motion, int pin)
{
    iotElementPub_t *pub;
    sprintf(motion->name, "motion%d", motionCount);
    motionCount++;
    motion->element.name = motion->name;
    iotElementAdd(&motion->element);
    pub = &motion->pub;
    pub->name = "";
    pub->type = iotValueType_String;
    pub->retain = false;
    pub->value.s = motionStopped;
    iotElementPubAdd(&motion->element, &motion->pub);
    switchAdd(pin, motionCallback, motion);
}