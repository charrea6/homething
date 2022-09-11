#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "deviceprofile.h"
#include "relay.h"

#include "switches.h"
#include "humidityfan.h"
#include "thermostat.h"

static const char TAG[] = "relays";

static int relayCount = 0;
static Relay_t *relays;

int initRelays(int norfSwitches)
{
    relays = calloc(norfSwitches, sizeof(Relay_t));
    if (relays == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relays");
        return -1;
    }
    return 0;
}

int addRelay(CborValue *entry, Notifications_ID_t *ids, uint32_t idCount)
{
    Relay_t *relay;
    uint32_t pin;
    uint32_t onLevel;
    bool controlled = false;
    uint32_t controller;
    uint32_t id;

    relay = &relays[relayCount];

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
        if (controller >= DeviceProfile_RelayController_Max) {
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

    relayInit(relayCount, (uint8_t)pin, (uint8_t)onLevel, relay);
    relayCount++;
    if (controlled) {
        switch(controller) {
        case DeviceProfile_RelayController_None:
            break;
        case DeviceProfile_RelayController_Switch:
            notificationsRegister(Notifications_Class_Switch, ids[id], switchRelayController, relay);
            break;
        case DeviceProfile_RelayController_Temperature: {
#ifdef CONFIG_THERMOSTAT
            Thermostat_t *thermostat = malloc(sizeof(Thermostat_t));
            if (thermostat) {
                thermostatInit(thermostat, relay, ids[id]);
            } else {
                ESP_LOGE(TAG, "setupRelay: Failed to allocate memory for thermostat");
            }
#else
            ESP_LOGW(TAG, "setupRelay: Unsupported thermostat controller!");
#endif
        }
        break;
        case DeviceProfile_RelayController_Humidity: {
#ifdef CONFIG_HUMIDISTAT
            HumidityFan_t *fanController = malloc(sizeof(HumidityFan_t));
            if (fanController) {
                humidityFanInit(fanController, relay, ids[id], CONFIG_HUMIDISTAT_THRESHOLD);
            } else {
                ESP_LOGE(TAG, "setupRelay: Failed to allocate memory for humidistat");
            }
#else
            ESP_LOGW(TAG, "setupRelay: Unsupported humidistat controller!");
#endif
        }
        break;
        default:
            ESP_LOGW(TAG, "setupRelay: Unknown controller type %u", controller);
            break;
        }
    }
    return 0;
}

