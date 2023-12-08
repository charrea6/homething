#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"
#include "switch.h"
#include "deviceprofile.h"
#include "relay.h"
#include "notifications.h"


struct SwitchTypeInfo {
    enum DeviceProfile_Choices_Switch_Type type;
    const char *deviceName;
    const char *highState;
    const char *lowState;
};

static struct Switch {
    Notifications_ID_t id;
    const struct SwitchTypeInfo *typeInfo;
    iotElement_t element;
    const char *relayId;
    Relay_t *relay;
} *switches = NULL;

static void addSwitch(struct Switch *switchInstance, DeviceProfile_SwitchConfig_t *config, uint32_t devId);
static void switchUpdated(void *user,  NotificationsMessage_t *message);
static void switchInitFinished(void *user, NotificationsMessage_t *message);
static void switchRelayController(struct Switch *switchInstance, bool switchState);

static uint32_t switchCount = 0;


static const struct SwitchTypeInfo allSwitchTypeInfo[] = {
    {
        .type = DeviceProfile_Choices_Switch_Type_Momentary,
        .deviceName = "momentary%d",
        .highState = "released",
        .lowState = "pressed",
    },
    {
        .type = DeviceProfile_Choices_Switch_Type_Toggle,
        .deviceName = "toggle%d",
        .highState = "toggled",
        .lowState = "toggled",
    },
    {
        .type = DeviceProfile_Choices_Switch_Type_Onoff,
        .deviceName = "onOff%d",
        .highState = "off",
        .lowState = "on",
    },
    {
        .type = DeviceProfile_Choices_Switch_Type_Contact,
        .deviceName = "contact%d",
        .highState = "open",
        .lowState = "closed",
    },
    {
        .type = DeviceProfile_Choices_Switch_Type_Motion,
        .deviceName = "motion%d",
        .highState = "motion detected",
        .lowState = "motion stopped",
    }
};

static const char TAG[] = "switches";

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    elementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(NOT_RETAINED, STRING, IOT_PUB_USE_ELEMENT)
    )
);


int initSwitches(DeviceProfile_SwitchConfig_t *switchConfigs, int norfSwitches)
{
    int i;
    uint32_t switchTypeCounts[DeviceProfile_Choices_Switch_Type_ChoiceCount] = {0};

    notificationsRegister(Notifications_Class_Switch, NOTIFICATIONS_ID_ALL, switchUpdated, NULL);
    notificationsRegister(Notifications_Class_System, NOTIFICATIONS_ID_ALL, switchInitFinished, NULL);
    switches = calloc(norfSwitches, sizeof(struct Switch));
    if (switches == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for switch ids");
    }
    switchCount = norfSwitches;
    ESP_LOGI(TAG, "%u switches", norfSwitches);
    for (i = 0; i < norfSwitches; i ++) {
        if (switchConfigs[i].type >= DeviceProfile_Choices_Switch_Type_ChoiceCount) {
            ESP_LOGE(TAG, "Skipping entry with invalid switch type: %u", switchConfigs[i].type);
            continue;
        }
        addSwitch(&switches[i], &switchConfigs[i], switchTypeCounts[switchConfigs[i].type]);
        switchTypeCounts[switchConfigs[i].type] ++;
    }

    return 0;
}

void addSwitch(struct Switch *switchInstance, DeviceProfile_SwitchConfig_t *config, uint32_t devId)
{
    Notifications_ID_t id;
    const struct SwitchTypeInfo *typeInfo = NULL;
    int i;

    for (i =0; i < sizeof(allSwitchTypeInfo) / sizeof(struct SwitchTypeInfo); i++) {
        if (allSwitchTypeInfo[i].type == config->type) {
            typeInfo = &allSwitchTypeInfo[i];
            break;
        }
    }

    if (typeInfo == NULL) {
        ESP_LOGE(TAG, "Failed to find type info");
        return;
    }
    
    id = switchAdd(config->pin, (uint8_t)config->noiseFilter);
    switchInstance->id = id;
    switchInstance->typeInfo = typeInfo;
    switchInstance->relayId = config->relay;
    switchInstance->element = iotNewElement(&elementDescription, 0, NULL, NULL, typeInfo->deviceName, devId);
    if (config->id) {
        notificationsRegisterId(id, config->id);
    }
}

static void switchInitFinished(void *user, NotificationsMessage_t *message)
{
    uint32_t i;
    for (i = 0; i < switchCount; i ++) {
        if (switches[i].relayId != NULL) {
            switches[i].relay = relayFind(switches[i].relayId);
        }
    }
}

static void switchUpdated(void *user,  NotificationsMessage_t *message)
{
    uint32_t i;
    for (i = 0; i < switchCount; i ++) {
        if (switches[i].id == message->id) {
            iotValue_t value;
            const struct SwitchTypeInfo *typeInfo = switches[i].typeInfo;
            value.s = message->data.switchState ? typeInfo->highState: typeInfo->lowState;
            iotElementPublish(switches[i].element, 0, value);
            if (switches[i].relay) {
                switchRelayController(&switches[i], message->data.switchState);
            }
            break;
        }
    }
}

static void switchRelayController(struct Switch *switchInstance, bool switchState)
{
    Relay_t *relay = switchInstance->relay;
    switch(switchInstance->typeInfo->type) {
    case DeviceProfile_Choices_Switch_Type_Momentary:
        if (switchState) {
            relaySetState(relay, !relayIsOn(relay));
        }
        break;
    case DeviceProfile_Choices_Switch_Type_Toggle:
        relaySetState(relay, !relayIsOn(relay));
        break;
    case DeviceProfile_Choices_Switch_Type_Onoff:
        relaySetState(relay, switchState);
        break;
    default:
        break;
    }
}