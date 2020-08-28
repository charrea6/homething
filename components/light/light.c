#include <stdlib.h>
#include <stdio.h>

#include "light.h"

static void lightControl(Light_t *light, iotElement_t *element, iotValue_t value);

static int lightCount = 0;

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BOOL, "state")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(IOT_VALUE_TYPE_BOOL, IOT_SUB_DEFAULT_NAME, (iotElementSubUpdateCallback_t)lightControl)
    )
);

void lightInit(Light_t *light, int8_t pin)
{
    relayInit(pin, &light->relay);
    light->element = iotNewElement(&elementDescription, light, "light%d", lightCount);
    lightCount ++;
}

void lightToggle(Light_t *light)
{
    lightSetState(light, light->relay.state ==RelayState_Off ? RelayState_On:RelayState_Off);
}

void lightSetState(Light_t *light, RelayState_t state)
{
    if (light->relay.state != state)
    {
        iotValue_t value;
        relaySetState(&light->relay, state);
        value.b = light->relay.state == RelayState_On ? true:false;
        iotElementPublish(light->element, 0, value);
    }
}

static void lightControl(Light_t *light, iotElement_t *element, iotValue_t value)
{
    lightSetState(light, value.b?RelayState_On:RelayState_Off);
}