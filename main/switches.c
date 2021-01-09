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

static void switchUpdated(void *user,  NotificationsMessage_t *message);

static uint32_t switchTypeCounts[DeviceProfile_SwitchType_Max] = {0};

static uint32_t switchCount = 0;

static const char const *switchTypes[] = {
    "toggle%d",
    "onOff%d",
    "momentary%d",
    "contact%d",
};

static const char const *switchStateOn[] = {
    "toggled",
    "off",
    "released",
    "open",
    "motion detected"
};

static const char const *switchStateOff[] ={
    "toggled",
    "on",
    "pressed",
    "closed",
    "motion stopped"
};
 
static const char TAG[] = "switches";

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, IOT_PUB_USE_ELEMENT)
    )
);

static struct Switch {
    Notifications_ID_t id;
    uint32_t type;
    iotElement_t element;
} *switches = NULL;

int initSwitches(int norfSwitches) {
    notificationsRegister(Notifications_Class_Switch, NOTIFICATIONS_ID_ALL, switchUpdated, NULL);
    switches = calloc(norfSwitches, sizeof(struct Switch));
    if (switches == NULL){
        ESP_LOGE(TAG, "Failed to allocate memory for switch ids");
    }
    return 0;
}

Notifications_ID_t addSwitch(CborValue *entry) {
    uint32_t pin;
    uint32_t type = DeviceProfile_SwitchType_Toggle;
    Notifications_ID_t id;

    if (deviceProfileParserEntryGetUint32(entry, &pin)){
        ESP_LOGE(TAG, "addSwitch: Failed to get pin!");
        return NOTIFICATIONS_ID_ERROR;
    }

    if (deviceProfileParserEntryGetUint32(entry, &type)){
        ESP_LOGW(TAG, "addSwitch: Failed to get type using default Toggle.");
    }

    if (type >= DeviceProfile_SwitchType_Max) {
        ESP_LOGE(TAG, "addSwitch: Unknown switch type %u", type);
        return NOTIFICATIONS_ID_ERROR;
    }
    id = switchAdd(pin);
    switches[switchCount].id = id;
    switches[switchCount].type = type;
    switches[switchCount].element = iotNewElement(&elementDescription, 0, NULL, switchTypes[type], switchTypeCounts[type]);
    switchTypeCounts[type]++;
    switchCount++;
    return id;
}

static void switchUpdated(void *user,  NotificationsMessage_t *message){
    uint32_t i;
    for (i = 0; i < switchCount; i ++) {
        if (switches[i].id == message->id){
            iotValue_t value;
            if (message->data.switchState) {
                value.s = switchStateOn[switches[i].type];
            } else {
                value.s = switchStateOff[switches[i].type];
            }
            iotElementPublish(switches[i].element, 0, value);
            break;
        }
    }
}

void switchRelayController(void *user, NotificationsMessage_t *message) {
    Relay_t *relay = user;
    uint32_t i;
    for (i = 0; i < switchCount; i ++) {
        if (switches[i].id == message->id){
            switch(switches[i].type){
                case DeviceProfile_SwitchType_Toggle:
                    relaySetState(relay, !relayIsOn(relay));
                    break;
                case DeviceProfile_SwitchType_OnOff:
                    relaySetState(relay, message->data.switchState);
                    break;
                default:
                break;
            }
        }
    }
}