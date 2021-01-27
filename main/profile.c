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

static const char TAG[] = "profile";

typedef int (*initFunc_t)(int);
typedef Notifications_ID_t (*addFunc_t)(CborValue *); 

static const initFunc_t initFuncs[DeviceProfile_EntryType_Max] = {
    initSwitches,
    initRelays,
#ifdef CONFIG_DHT22
    initDHT22,
#else
    NULL,
#endif
#ifdef CONFIG_SI7021    
    initSI7021,
#else
    NULL,
#endif
    NULL,
#ifdef CONFIG_BME280
    initBME280,
#else
    NULL,
#endif
#ifdef CONFIG_DS18x20
    initDS18x20
#else
    NULL
#endif
};

static const addFunc_t addFuncs[DeviceProfile_EntryType_Max] = {
    addSwitch,
    NULL,
#ifdef CONFIG_DHT22
    addDHT22,
#else
    NULL,
#endif
#ifdef CONFIG_SI7021
    addSI7021,
#else
    NULL,
#endif
    NULL,
#ifdef CONFIG_BME280
    addBME280,
#else
    NULL,
#endif
#ifdef CONFIG_DS18x20
    addDS18x20
#else
    NULL
#endif
};

int processProfile(void)
{
    int ret = -1;
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
        if ((initFuncs[i] != NULL) && (nrofEntryTypes[i] != 0)){
            ESP_LOGI(TAG, "EntryType %d Count %d", i, nrofEntryTypes[i]);
            if (initFuncs[i](nrofEntryTypes[i])) {
                goto error;
            }
        }
    }

    ids = calloc(entryCount, sizeof(Notifications_ID_t));
    if (ids == NULL){
        ESP_LOGE(TAG, "Failed to allocate memory for ids");
        goto error;
    }

    for (i = 0; i < entryCount; i ++){
        ids[i] = NOTIFICATIONS_ID_ERROR;
    }

    /* Process sensors and switches */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if ((entryType < DeviceProfile_EntryType_Max) && (addFuncs[entryType] != NULL)) {
            ids[entryIndex] = addFuncs[entryType](&entry);
        }
        entryIndex ++;
        deviceProfileParserCloseEntry(&parser, &entry);
    }
    
    /* Process relays last so we can find sensor/switch ids */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if (entryType == DeviceProfile_EntryType_Relay){
            if (addRelay(&entry, ids, entryCount)) {
                goto error;
            }
            relayIndex ++;
        }
        deviceProfileParserCloseEntry(&parser, &entry);
    }
    ret = 0;
error:
    if (ids) {
        free(ids);
    }
    return ret;
}
