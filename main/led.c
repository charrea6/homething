#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "deviceprofile.h"
#include "iot.h"
#include "led.h"
#include "notificationled.h"

struct Led {
    NotificationLed_t led;
    iotElement_t element;
    char *state;
};

static struct Led *leds;
static int ledCount = 0;

static void ledControl(struct Led *led, iotElement_t *element, iotValue_t value);

static const char TAG[] = "led";
static const char OFF[] = "off";
static const char ON[] = "on";

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_ELEMENT_TYPE_SWITCH,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, STRING, "state")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(STRING, IOT_SUB_DEFAULT_NAME, (iotElementSubUpdateCallback_t)ledControl)
    )
);

int initLeds(int nrofLeds)
{
    leds = calloc(nrofLeds, sizeof(struct Led));
    if (leds == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for leds");
        return -1;
    }
    return 0;
}

Notifications_ID_t addLed(CborValue *entry)
{
    uint32_t pin;
    struct Led *led;
    iotValue_t value;

    if (deviceProfileParserEntryGetUint32(entry, &pin)) {
        ESP_LOGE(TAG, "Failed to get pin!");
        return NOTIFICATIONS_ID_ERROR;
    }
    led = leds ++;
    led->element = iotNewElement(&elementDescription, 0, led, "led%d", ledCount);
    led->led = notificationLedNew(pin);
    led->state = NULL;
    value.s = OFF;
    iotElementPublish(led->element, 0, value);

    ledCount ++;
    return 0;
}

static void ledControl(struct Led  *led, iotElement_t *element, iotValue_t value)
{
    NotificationLedPattern_t pattern;
    if (led->state) {
        free(led->state);
        led->state = NULL;
    }
    if (iotStrToBool(value.s, &pattern.on) == 0) {
        pattern.onTime = 0;
        pattern.offTime = 0;
        notificationLedSetPattern(led->led, &pattern);
        value.s = pattern.on ? ON:OFF;
        iotElementPublish(led->element, 0, value);
    } else {
        if (sscanf(value.s, "pulse,%u,%u", &pattern.onTime, &pattern.offTime) == 2) {

            if ((pattern.onTime > 10) && (pattern.offTime > 10)) {
                pattern.on = true;
                notificationLedSetPattern(led->led, &pattern);
                asprintf(&led->state, "pulse,%u,%u", pattern.onTime, pattern.offTime);
                value.s = led->state;
                iotElementPublish(led->element, 0, value);
            }
        }
    }
}