#ifndef _RELAY_H_
#define _RELAY_H_
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "iot.h"
#include "notifications.h"

typedef struct Relay Relay_t;

typedef struct RelayInterface {
    void (*setState)(Relay_t *, bool);
    bool (*isOn)(Relay_t *);
} RelayInterface_t;

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
    Notifications_ID_t id;
    iotElement_t element;
};

typedef struct RelayTimeout {
    bool targetValue;
    uint32_t timeoutSeconds;
    TimerHandle_t timer;
    uint32_t secondsLeft;
    Relay_t *relay;
    iotElement_t element;
    char *stateStr;
} RelayTimeout_t;

typedef struct RelayLockout {
    Relay_t substituteRelay;
    Relay_t state;
    Relay_t *relay;
} RelayLockout_t;

void relayInit(uint8_t id, uint8_t pin, uint8_t onLevel, Relay_t *relay);
void relayNewIOTElement(Relay_t *relay, char *nameFmt);
void relaySetState(Relay_t *relay, bool on);
bool relayIsOn(Relay_t *relay);
const char* relayGetName(Relay_t *relay);

void relayRegister(Relay_t *relay, const char *id);
Relay_t* relayFind(const char *id);

void relayLockoutInit(uint8_t id, char *relayId, char *lockoutId, RelayLockout_t *lockout);

void relayTimeoutInit(uint8_t id, char *relay, bool targetValue, uint32_t seconds, RelayTimeout_t *timeout);
#endif
