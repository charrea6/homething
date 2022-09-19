#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "deviceprofile.h"
#include "relays.h"
#include "relay.h"
#include "draytonscr.h"

static const char TAG[] = "relays";

static Relay_t *relays;


static int addGPIORelay(uint32_t id, DeviceProfile_RelayConfig_t *config)
{
    Relay_t *relay = &relays[id];

    relayInit((uint8_t)id, config->pin, config->level, relay);
    if (config->name) {
        iotElementSetHumanDescription(relay->element, config->name);
    }
    if (config->id) {
        relayRegister(relay, config->id);
    }
    return 0;
}

static int initGPIORelays(DeviceProfile_RelayConfig_t *relayConfig, uint32_t relayCount)
{
    uint32_t i;
    relays = calloc(relayCount, sizeof(Relay_t));
    if (relays == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relays");
        return -1;
    }
    for (i = 0; i < relayCount; i ++) {
        addGPIORelay(i, &relayConfig[i]);
    }
    return 0;
}

#ifdef CONFIG_DRAYTONSCR
static int addDraytonSCR(DeviceProfile_DraytonscrConfig_t *config) 
{
    Relay_t *relay = draytonSCRInit(config->pin, config->onCode, config->offCode);
    if (relay == NULL) {
        ESP_LOGE(TAG, "addDraytonSCR: Failed to allocate memory for draytonSCR");
        return -1;
    }
    if (config->name) {
        iotElementSetHumanDescription(relay->element, config->name);
    }
    if (config->id) {
        relayRegister(relay, config->id);
    }
    return 0;
}

static int initDraytonSCR(DeviceProfile_DraytonscrConfig_t *config, uint32_t draytonSCRCount)
{
    if (draytonSCRCount > 0){
        return addDraytonSCR(&config[0]);
    }
        
    return 0;
}
#endif

int initRelays(DeviceProfile_DeviceConfig_t *config)
{
    initGPIORelays(config->relayConfig, config->relayCount);

#ifdef CONFIG_DRAYTONSCR    
    initDraytonSCR(config->draytonscrConfig, config->draytonscrCount);
#endif
    return 0;
}