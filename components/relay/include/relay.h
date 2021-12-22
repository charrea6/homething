#ifndef _RELAY_H_
#define _RELAY_H_
#include "iot.h"

typedef struct Relay {
    union {
        uint32_t data;
        struct {
            uint8_t pin;
            uint8_t onLevel;
            bool on;
            uint8_t id;
        } fields;
    } u;
    iotElement_t element;
} Relay_t;

void relayInit(uint8_t id, uint8_t pin, uint8_t onLevel, Relay_t *relay);
void relaySetState(Relay_t *relay, bool on);
bool relayIsOn(Relay_t *relay);
#define relayId(relay) ((relay)->u.fields.id)
#endif
