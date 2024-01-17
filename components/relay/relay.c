#include <stdbool.h>
#include "gpiox.h"
#include "relay.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"

static void relayElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);
static void gpioRelaySetState(Relay_t *relay, bool on);

static const char TAG[] = "relay";

static const RelayInterface_t gpioIntf = {
    .setState = gpioRelaySetState,
    .isOn = NULL
};

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
    relay->element = NULL;
    relay->intf = &gpioIntf;
    relay->fields.id = id;
    relay->fields.pin = pin;
    relay->fields.onLevel = onLevel & 1;
    relay->fields.on = true; // So that we can set it to false in relaySetState!
    relayNewIOTElement(relay, "relay%d");
    relaySetState(relay, false);
}

void relayNewIOTElement(Relay_t *relay, char *nameFmt)
{
    relay->element = iotNewElement(&elementDescription, 0, relayElementCallback, relay, nameFmt, relay->fields.id);
}

void relaySetState(Relay_t *relay, bool on)
{
    if (on == relayIsOn(relay)) {
        return;
    }
    relay->intf->setState(relay, on);
    if (relay->element) {
        iotValue_t value;
        value.b = relay->fields.on;
        iotElementPublish(relay->element, 0, value);
    }
}

bool relayIsOn(Relay_t *relay)
{
    if (relay->intf->isOn) {
        return relay->intf->isOn(relay);
    }
    return relay->fields.on;
}

const char* relayGetName(Relay_t *relay)
{
    if (relay->element) {
        return iotElementGetName(relay->element);
    }
    return "";
}

static void gpioRelaySetState(Relay_t *relay, bool on)
{
    int l;
    GPIOX_Pins_t pins, values;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_CLEAR_ALL(values);
    GPIOX_PINS_SET(pins, relay->fields.pin);

    if (on) {
        l = relay->fields.onLevel;
    } else {
        l = relay->fields.onLevel ^ 1;
    }

    if (l) {
        GPIOX_PINS_SET(values, relay->fields.pin);
    }
    ESP_LOGI(TAG, "Relay %d: Is on? %s (pin value %d)", relay->fields.pin, on ? "On":"Off", l);
    gpioxSetPins(&pins, &values);
    relay->fields.on = on;
}


static void relayElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    if (reason == IOT_CALLBACK_ON_SUB) {
        relaySetState((Relay_t *)userData, details->value.b);
    }
}
