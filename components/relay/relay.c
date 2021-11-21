#include <stdbool.h>
#include "gpiox.h"
#include "relay.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"

static void relayElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);

static const char TAG[] = "relay";

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_ELEMENT_TYPE_SWITCH,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, BOOL, "state")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(BOOL, IOT_SUB_DEFAULT_NAME)
    )
);

void relayInit(uint8_t id, uint8_t pin, uint8_t onLevel, Relay_t *relay)
{
    GPIOX_Pins_t pins;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_SET(pins, pin);

    gpioxSetup(&pins, GPIOX_MODE_OUT);
    relay->u.fields.id = id;
    relay->u.fields.pin = pin;
    relay->u.fields.onLevel = onLevel & 1;
    relay->element = iotNewElement(&elementDescription, 0, relayElementCallback, relay, "relay%d", id);
    relay->u.fields.on = true; // So that we can set it to false in relaySetState!
    relaySetState(relay, false);
}

void relaySetState(Relay_t *relay, bool on)
{
    if (on == relayIsOn(relay)) {
        return;
    }

    int l;
    GPIOX_Pins_t pins, values;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_CLEAR_ALL(values);
    GPIOX_PINS_SET(pins, relay->u.fields.pin);

    if (on) {
        l = relay->u.fields.onLevel;
    } else {
        l = relay->u.fields.onLevel ^ 1;
    }

    if (l) {
        GPIOX_PINS_SET(values, relay->u.fields.pin);
    }
    ESP_LOGI(TAG, "Relay %d: Is on? %s (pin value %d)", relay->u.fields.pin, on ? "On":"Off", l);
    gpioxSetPins(&pins, &values);
    relay->u.fields.on = on;

    iotValue_t value;
    value.b = on;
    iotElementPublish(relay->element, 0, value);
}

static void relayElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    if (IOT_CALLBACK_ON_SUB) {
        relaySetState((Relay_t *)userData, details->value.b);
    }
}
