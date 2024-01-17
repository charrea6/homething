#include <stdbool.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"
#include "relay.h"

static const char TAG[] = "lockout";

static void lockoutSetState(Relay_t *relay, bool on);
static void lockoutRelaySetState(Relay_t *relay, bool on);
static bool lockoutRelayIsOn(Relay_t *relay);

static const RelayInterface_t lockoutIntf = {
    .setState = lockoutSetState,
    .isOn = NULL
};

static const RelayInterface_t substituteIntf = {
    .setState = lockoutRelaySetState,
    .isOn = lockoutRelayIsOn
};


void relayLockoutInit(uint8_t id, char *relayId, char *lockoutId, RelayLockout_t *lockout)
{
    Relay_t *state = &lockout->state;
    state->element = NULL;
    state->intf = &lockoutIntf;
    state->fields.id = id;
    state->fields.on = true; // So that we can set it to false in relaySetState!
    relayNewIOTElement(state, "lockout%d");
    relaySetState(state, false);
    
    Relay_t *substituteRelay = &lockout->substituteRelay;
    substituteRelay->intf = &substituteIntf;
    substituteRelay->data = 0u;
    substituteRelay->element = NULL;

    lockout->relay = relayFind(relayId);
    if (lockout->relay == NULL) {
        ESP_LOGE(TAG, "Could not find relay %s!", relayId);
    }
    if (lockoutId != NULL) {
        relayRegister(&lockout->substituteRelay, lockoutId);
    }
}

static void lockoutSetState(Relay_t *relay, bool on)
{
    relay->fields.on = on;
}

static void lockoutRelaySetState(Relay_t *relay, bool on)
{
    RelayLockout_t *lockout = (RelayLockout_t *)relay;
    if (!lockout->state.fields.on) {
        if (lockout->relay != NULL) {
            relaySetState(lockout->relay, on);
        }
    }
}


bool lockoutRelayIsOn(Relay_t *relay)
{
    RelayLockout_t *lockout = (RelayLockout_t *)relay;
    if (lockout->relay == NULL) {
        return false;
    }
    return relayIsOn(lockout->relay);
}

