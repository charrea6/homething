#include <stdlib.h>
#include "iot.h"

#include "switch.h"
#include "doorbell.h"

static void doorbellSwitchCallback(void *userData, int state);


static const char released[] = "released";
static const char pressed[] = "pressed";
static iotElement_t element;

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, IOT_PUB_USE_ELEMENT)
    )
);

void doorbellInit(int pin)
{
    switchAdd(pin, doorbellSwitchCallback, NULL);
    element = iotNewElement(&elementDescription, NULL, "doorbell");
}

static void doorbellSwitchCallback(void *userData, int state)
{
    iotValue_t value;
    value.s = state == 0 ? pressed:released;
    iotElementPublish(element, 0, value);
}
