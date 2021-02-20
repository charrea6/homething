#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "deviceprofile.h"
#include "iot.h"

#include "relays.h"
#include "relay.h"

#include "switches.h"
#include "switch.h"

#include "sensors.h"
#include "dht.h"
#include "humidityfan.h"
#include "led.h"

static const char TAG[] = "profile";

typedef int (*initFunc_t)(int);
typedef Notifications_ID_t (*addFunc_t)(CborValue *);

struct Component {
    initFunc_t init;
    addFunc_t add;
} components[DeviceProfile_EntryType_Max] = {
    SWITCHES_COMPONENT,
    {initRelays, NULL},
    DHT22_COMPONENT,
    SI7021_COMPONENT,
    {NULL, NULL},
    BME280_COMPONENT,
    DS18x20_COMPONENT,
    LED_COMPONENT
};

void processProfile(void)
{
    int nrofEntryTypes[DeviceProfile_EntryType_Max] = {0};
    uint8_t relayIndex = 0;
    int entryCount = 0, entryIndex = 0;
    int i;
    uint8_t *profile = NULL;
    size_t profileLen = 0;
    DeviceProfile_Parser_t parser;
    CborValue entry;
    DeviceProfile_EntryType_e entryType;
    Notifications_ID_t *ids = NULL;

    if (deviceProfileGetProfile(&profile, &profileLen)) {
        ESP_LOGE(TAG, "Failed to load profile!");
        goto error;
    }

    if (deviceProfileParseProfile(profile, profileLen, &parser)) {
        goto error;
    }

    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if (entryType < DeviceProfile_EntryType_Max) {
            nrofEntryTypes[entryType]++;
        }
        entryCount ++;
        deviceProfileParserCloseEntry(&parser, &entry);
    }

    for (i=DeviceProfile_EntryType_GPIOSwitch; i <DeviceProfile_EntryType_Max; i++) {
        if ((components[i].init != NULL) && (nrofEntryTypes[i] != 0)) {
            ESP_LOGI(TAG, "EntryType %d Count %d", i, nrofEntryTypes[i]);
            if (components[i].init(nrofEntryTypes[i])) {
                goto error;
            }
        }
    }

    ids = calloc(entryCount, sizeof(Notifications_ID_t));
    if (ids == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ids");
        goto error;
    }

    for (i = 0; i < entryCount; i ++) {
        ids[i] = NOTIFICATIONS_ID_ERROR;
    }

    /* Process sensors and switches */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if ((entryType < DeviceProfile_EntryType_Max) && (components[entryType].add != NULL)) {
            ids[entryIndex] = components[entryType].add(&entry);
        }
        entryIndex ++;
        deviceProfileParserCloseEntry(&parser, &entry);
    }

    /* Process relays last so we can find sensor/switch ids */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if (entryType == DeviceProfile_EntryType_Relay) {
            if (addRelay(&entry, ids, entryCount)) {
                goto error;
            }
            relayIndex ++;
        }
        deviceProfileParserCloseEntry(&parser, &entry);
    }
error:
    if (ids) {
        free(ids);
    }
}
