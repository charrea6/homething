#include <stdbool.h>
#include "gpiox.h"
#include "relay.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char TAG[] = "relay";
static int relayOnLevel = 0;

void relaySetOnLevel(int level)
{
    relayOnLevel = level & 1;
}

void relayInit(int8_t pin, Relay_t *relay)
{
    GPIOX_Pins_t pins;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_SET(pins, pin);

    gpioxSetup(&pins, GPIOX_MODE_OUT);
    relay->pin = pin;
    relaySetState(relay, RelayState_Off);
}

void relaySetState(Relay_t *relay, RelayState_t state)
{
    int l;
    GPIOX_Pins_t pins, values;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_CLEAR_ALL(values);
    GPIOX_PINS_SET(pins, relay->pin);
    if (state == RelayState_On)
    {
        l = relayOnLevel;
    }
    else
    {
        l = relayOnLevel ^ 1;
    }
    
    if (l){
        GPIOX_PINS_SET(values, relay->pin);
    }
    ESP_LOGI(TAG, "Relay %d: Set state %d (pin value %d)", relay->pin, state, l);
    gpioxSetPins(&pins, &values);
    relay->state = state;
}
