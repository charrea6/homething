#include <stdlib.h>
#include "iot.h"

#include "switch.h"
#include "doorbell.h"

static void doorbellSwitchCallback(void *userData, int state);


static const char released[] = "released";
static const char pressed[] = "pressed";
static iotElement_t element;
static iotElementPub_t alertPub;

void doorbellInit(int pin)
{
    switchAdd(pin, doorbellSwitchCallback, NULL);
    element.name = "doorbell";
    iotElementAdd(&element);
    alertPub.name = "";
    alertPub.type = iotValueType_String;
    alertPub.value.s = released;
    alertPub.retain = false;
    iotElementPubAdd(&element, &alertPub);
}

static void doorbellSwitchCallback(void *userData, int state)
{
    iotValue_t value;
    value.s = state == 0 ? pressed:released;
    iotElementPubUpdate(&alertPub, value);
}
