#ifndef _LIGHT_H_
#define _LIGHT_H_
#include <stdint.h>

#include "relay.h"
#include "iot.h"

typedef struct {
    Relay_t relay;
    iotElement_t *element;
    iotElementSub_t *control;
    iotElementPub_t *state;
    char name[7];
}Light_t;

void lightInit(Light_t *light, int8_t pin);
void lightToggle(Light_t *light);
void lightSetState(Light_t *light, RelayState state);
void lightSendUpdate(Light_t *light, MQTTClient *client, const char *topic);
#endif
