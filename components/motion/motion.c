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
    iotElementPubUpdate(motion->pub, value);
}

void motionInit(Motion_t *motion, int pin)
{
    iotValue_t initialValue;
    sprintf(motion->name, "motion%d", motionCount);
    motionCount++;
    iotElementAdd(motion->name, &motion->element);
    initialValue.s = motionStopped;
    iotElementPubAdd(motion->element, "", iotValueType_String, false, initialValue, &motion->pub);
    switchAdd(pin, motionCallback, motion);
}