#include <stdlib.h>
#include "MQTTClient.h"

#include "light.h"

static void lightControl(Light_t *light, iotElementSub_t *sub, iotValue_t value);

static int lightCount = 0;

void lightInit(Light_t *light, int8_t pin)
{
    iotElementPub_t *state;
    iotElementSub_t *control;
    sprintf(light->name, "light%d", lightCount);
    lightCount = (lightCount + 1) % 10;
    
    relayInit(pin, &light->relay);
    
    light->element.name = light->name;
    iotElementAdd(&light->element);
    
    state = &light->state;
    state->name = "state";
    state->type = iotValueType_Bool;
    state->retain = true;
    state->value.b = light->relay.state == RelayState_On ? true:false;
    iotElementPubAdd(&light->element, &light->state);
    
    control = &light->control;
    control->name = IOT_DEFAULT_CONTROL;
    control->type = iotValueType_Bool;
    control->callback = (iotElementSubUpdateCallback_t)lightControl;
    control->userData = light;
    iotElementSubAdd(&light->element, &light->control);
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
        iotElementPubUpdate(&light->state, value);
    }
}

static void lightControl(Light_t *light, iotElementSub_t *sub, iotValue_t value)
{
    lightSetState(light, value.b?RelayState_On:RelayState_Off);
}