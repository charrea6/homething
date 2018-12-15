#include <stdbool.h>
#include "driver/gpio.h"
#include "relay.h"

void relayInit(int8_t pin, Relay_t *relay)
{
    gpio_config_t config;

    relay->pin = pin;    
    config.pin_bit_mask = 1 << pin;
    config.mode = GPIO_MODE_DEF_OUTPUT;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    relaySetState(relay, RelayState_Off);
}

void relaySetState(Relay_t *relay, RelayState_t state)
{
    int l;
#ifdef CONFIG_RELAY_ON_HIGH
    l = state == RelayState_On ? 1:0;
#else
    l = state == RelayState_On ? 0:1;
#endif
    gpio_set_level(relay->pin, l);
    relay->state = state;
}
