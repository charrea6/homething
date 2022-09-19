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

static void addLed(DeviceProfile_LedConfig_t *config, int id, struct Led *led);
static void ledControl(struct Led *led, iotValue_t value);
static void ledElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);

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
        IOT_DESCRIBE_SUB(STRING, IOT_SUB_DEFAULT_NAME)
    )
);

int initLeds(DeviceProfile_LedConfig_t *config, uint32_t ledCount)
{
    int i;
    leds = calloc(ledCount, sizeof(struct Led));
    if (leds == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for leds");
        return -1;
    }
    for (i = 0; i < ledCount; i++) {
        addLed(&config[i], i, &leds[i]);
    }
    return 0;
}

static void addLed(DeviceProfile_LedConfig_t *config, int id, struct Led *led)
{
    iotValue_t value;
    led->element = iotNewElement(&elementDescription, 0, ledElementCallback, led, "led%d", id);
    led->led = notificationLedNew(config->pin);
    led->state = NULL;
    value.s = OFF;
    iotElementPublish(led->element, 0, value);
    if (config->name) {
        iotElementSetHumanDescription(led->element, config->name);
    }
}


static void ledControl(struct Led  *led, iotValue_t value)
{
    NotificationLedPattern_t pattern;
    if (iotStrToBool(value.s, &pattern.on) == 0) {
        pattern.onTime = 0;
        pattern.offTime = 0;
        notificationLedSetPattern(led->led, &pattern);
        if (led->state) {
            free(led->state);
            led->state = NULL;
        }
        value.s = pattern.on ? ON:OFF;
        iotElementPublish(led->element, 0, value);
    } else {
        if (sscanf(value.s, "pulse,%u,%u", &pattern.onTime, &pattern.offTime) == 2) {

            if ((pattern.onTime > 10) && (pattern.offTime > 10)) {
                pattern.on = true;
                notificationLedSetPattern(led->led, &pattern);
                if (led->state) {
                    free(led->state);
                    led->state = NULL;
                }
                asprintf(&led->state, "pulse,%u,%u", pattern.onTime, pattern.offTime);
                value.s = led->state;
                iotElementPublish(led->element, 0, value);
            }
        }
    }
}

static void ledElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    if (reason == IOT_CALLBACK_ON_SUB) {
        ledControl((struct Led*) userData, details->value);
    }
}