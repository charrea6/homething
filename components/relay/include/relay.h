#ifndef _RELAY_H_
#define _RELAY_H_
#include "iot.h"

typedef struct Relay Relay_t;

typedef struct RelayInterface {
    void (*setState)(Relay_t *, bool);
}RelayInterface_t;

struct Relay {
    const RelayInterface_t *intf;
    union {
        uint32_t data;
        struct {
            uint8_t pin;
            uint8_t onLevel;
            bool on;
            uint8_t id;
        } fields;
    };
    iotElement_t element;
};

void relayInit(uint8_t id, uint8_t pin, uint8_t onLevel, Relay_t *relay);
void relayNewIOTElement(Relay_t *relay, char *nameFmt);
void relaySetState(Relay_t *relay, bool on);
bool relayIsOn(Relay_t *relay);
const char* relayGetName(Relay_t *relay);

void relayRegister(char *id, Relay_t *);
Relay_t* relayFind(char *id);
#endif
