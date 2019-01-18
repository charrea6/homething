#include <stdlib.h>
#include "iot.h"

#include "switch.h"
#include "doorbell.h"

static void doorbellSwitchCallback(void *userData, int state);


static const char released[] = "released";
static const char pressed[] = "pressed";
static iotElement_t *element;
static iotElementPub_t *alertPub;

void doorbellInit(int8_t pin)
{
    iotValue_t initial;
    switchAdd(pin, doorbellSwitchCallback, NULL);

    iotElementAdd("doorbell", &element);
    initial.s = released;
    iotElementPubAdd(element, "", iotValueType_String, false, initial, &alertPub);
}

static void doorbellSwitchCallback(void *userData, int state)
{
    iotValue_t value;
    value.s = state == 0 ? pressed:released;
    iotElementPubUpdate(alertPub, value);
}
