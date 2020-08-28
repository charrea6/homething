#include <stdio.h>

#include "motion.h"
#include "switch.h"

static int motionCount = 0;

static const char motionDetected[] = "motion detected";
static const char motionStopped[] = "motion stopped";

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, IOT_PUB_USE_ELEMENT)
    )
);

static void motionCallback(void *userData, int state)
{
    Motion_t *motion = userData;
    iotValue_t value;
    value.s = state?motionDetected:motionStopped;
    iotElementPublish(motion->element, 0, value);
}

void motionInit(Motion_t *motion, int pin)
{   
    motion->element = iotNewElement(&elementDescription, NULL, "motion%d", motionCount);
    motionCount++;
    switchAdd(pin, motionCallback, motion);
}