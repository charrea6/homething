#include <stdbool.h>
#include "gpiox.h"
#include "relay.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char TAG[] = "relay";

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
#ifdef CONFIG_RELAY_ON_HIGH
    l = state == RelayState_On ? 1:0;
#else
    l = state == RelayState_On ? 0:1;
#endif
    if (l){
        GPIOX_PINS_SET(values, relay->pin);
    }
    ESP_LOGI(TAG, "Relay %d: Set state %d (pin value %d)", relay->pin, state, l);
    gpioxSetPins(&pins, &values);
    relay->state = state;
}
