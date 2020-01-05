#ifndef _RELAY_H_
#define _RELAY_H_

typedef enum RelayState {
    RelayState_Off,
    RelayState_On
} RelayState_t; 

typedef struct Relay {
    int8_t pin;
    RelayState_t state;
} Relay_t;

void relaySetOnLevel(int level);
void relayInit(int8_t pin, Relay_t *relay);
void relaySetState(Relay_t *relay, RelayState_t state);
#define relayGetState(relay) ((relay)->state)

#endif
