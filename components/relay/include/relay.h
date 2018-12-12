#ifndef _RELAY_H_
#define _RELAY_H_

typedef enum RelayState {
    RelayState_Off,
    RelayState_On
} RelayState; 

typedef struct Relay {
    int8_t pin;
    RelayState state;
} Relay_t;

void relayInit(int8_t pin, Relay_t *relay);
void relaySetState(Relay_t *relay, RelayState state);
RelayState relayGetState(Relay_t *relay);

#endif
