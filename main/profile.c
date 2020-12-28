#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "deviceprofile.h"
#include "relay.h"

#include "switches.h"
#include "switch.h"
#include "iot.h"
#include "dht.h"
#include "humidityfan.h"

static const char TAG[] = "profile";

int setupRelay(CborValue *entry, uint8_t relayId, Relay_t *relay, Notifications_ID_t *ids, uint32_t idCount) {
    uint32_t pin;
    uint32_t onLevel;
    bool controlled = false;
    uint32_t controller;
    uint32_t id;

    if (deviceProfileParserEntryGetUint32(entry, &pin) == -1) {
        ESP_LOGE(TAG, "setupRelay: Failed to get pin");
        return  -1;
    }

    if (deviceProfileParserEntryGetUint32(entry, &onLevel) == -1) {
        ESP_LOGE(TAG, "setupRelay: Failed to get on levels");
        return  -1;
    }

    if (deviceProfileParserEntryGetUint32(entry, &controller) == 0) {
        controlled = true;
        if (controller >= DeviceProfile_RelayController_Max){
            ESP_LOGE(TAG, "setupRelay: Controller type %u invalid!", controller);
            return  -1;
        }
        if (controller != DeviceProfile_RelayController_None) {
            if (deviceProfileParserEntryGetUint32(entry, &id) == -1) {
                ESP_LOGE(TAG, "setupRelay: Failed to get controller id");
                return  -1;
            }
            if (id >= idCount) {
                ESP_LOGE(TAG, "setupRelay: Controller id %u too big!", id);
                return  -1;
            }
        }
    }

    relayInit(relayId, (uint8_t)pin, (uint8_t)onLevel, relay);
    if (controlled) {
        switch(controller) {
            case DeviceProfile_RelayController_Switch:
            notificationsRegister(Notifications_Class_Switch, ids[id], switchRelayController, relay);
            break;
            case DeviceProfile_RelayController_Temperature:
            break;
            case DeviceProfile_RelayController_Humidity:{
                HumidityFan_t *fanController = malloc(sizeof(HumidityFan_t));
                if (fanController) {
                    humidityFanInit(fanController, relay, ids[id], CONFIG_FAN_HUMIDITY);
                }
            }
            break;
            default:
            ESP_LOGW(TAG, "setupRelay: Unknown controller type %u", controller);
            break;
        }
    }
    return 0;
}


int processProfile(void)
{
    Relay_t *relays = NULL;
    int nrofRelays=0, nrofSwitches=0;
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
        if (entryType == DeviceProfile_EntryType_GPIOSwitch) {
            nrofSwitches ++;
        }
        if (entryType == DeviceProfile_EntryType_Relay){
            nrofRelays ++;
        }
        entryCount ++;
        deviceProfileParserCloseEntry(&parser, &entry);
    }
    
    ESP_LOGW(TAG, "Switches: %d Relays: %d", nrofSwitches, nrofRelays);
    
    if (initSwitches(nrofSwitches) == -1) {
        goto error;
    }

    ids = calloc(entryCount, sizeof(Notifications_ID_t));
    if (ids == NULL){
        ESP_LOGE(TAG, "Failed to allocate memory for ids");
        goto error;
    }

    for (i = 0; i < entryCount; i ++){
        ids[i] = NOTIFICATIONS_ID_ERROR;
    }

    relays = calloc(nrofRelays, sizeof(Relay_t));
    if (relays == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relays");
        goto error;
    }

    /* Process sensors and switches */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if (entryType == DeviceProfile_EntryType_GPIOSwitch) {
            ids[entryIndex] = addSwitch(&entry);
        }
        
        entryIndex ++;
        deviceProfileParserCloseEntry(&parser, &entry);
    }
    
    /* Process relays last so we can find sensor/switch ids */
    deviceProfileParseProfile(profile, profileLen, &parser);
    while(!deviceProfileParserNextEntry(&parser, &entry, &entryType)) {
        if (entryType == DeviceProfile_EntryType_Relay){
            if (setupRelay(&entry, relayIndex, &relays[relayIndex], ids, entryCount)) {
                goto error;
            }
            relayIndex ++;
        }
        deviceProfileParserCloseEntry(&parser, &entry);
    }

    free(ids);
    free(profile);
    return 0;
error:
    if (ids) {
        free(ids);
    }
    if (relays) {
        free(relays);
    }

    if (profile){
        free(profile);
    }    
    return -1;
}
