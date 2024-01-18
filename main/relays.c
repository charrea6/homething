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
static RelayLockout_t *lockouts;
static RelayTimeout_t *timeouts;

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
    if (relayCount == 0) {
        return 0;
    }
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

static int initRelayLockout(DeviceProfile_RelayLockoutConfig_t *config, uint32_t lockoutCount)
{
    uint32_t i;
    if (lockoutCount == 0) {
        return 0;
    }

    lockouts = calloc(lockoutCount, sizeof(RelayLockout_t));
    if (lockouts == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay lockouts");
        return -1;
    }

    for (i = 0; i < lockoutCount; i ++) {
        relayLockoutInit(i, config[i].relay, config[i].id, &lockouts[i]);
    }
    return 0;
}

static int initRelayTimeout(DeviceProfile_RelayTimeoutConfig_t *config, uint32_t timeoutCount)
{
    uint32_t i;
    if (timeoutCount == 0) {
        return 0;
    }

    timeouts = calloc(timeoutCount, sizeof(RelayTimeout_t));
    if (timeouts == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for relay timeouts");
        return -1;
    }

    for (i = 0; i < timeoutCount; i ++) {
        relayTimeoutInit(i, config[i].relay, config[i].value, config[i].timeout, &timeouts[i]);
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
    if (draytonSCRCount > 0) {
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

    /* Now register the components that interact with relays */
    initRelayLockout(config->relayLockoutConfig, config->relayLockoutCount);
    initRelayTimeout(config->relayTimeoutConfig, config->relayTimeoutCount);

    return 0;
}