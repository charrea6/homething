#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "sdkconfig.h"
#include "iot.h"
#include "iotInternal.h"
#include "notifications.h"
#include "utils.h"

#include "cJSON.h"

#ifdef CONFIG_ENABLE_HOMEASSISTANT_DISCOVERY

#define ON_NULL_GOTO_ERROR(check, msg) if( (check) == NULL) { errorMsg = msg; goto error;}

extern char appVersion[];

static const char TAG[]="IOT-HA";

static const char DEG_C[] = "\xc2\xb0C";
static const char PERCENT_RH[] = "% RH";
static const char KPA[] = "kPa";

static const char NAME[] = "name";
static const char STATE_TOPIC[] = "stat_t";
static const char DEVCIE_CLASS[] = "dev_cla";
static const char UNIQUE_ID[] = "uniq_id";
static const char UNIT_OF_MEASUREMENT[] = "unit_of_meas";
static const char SW_VERSION[] = "sw";
static const char IDENTIFIERS[] = "ids";

static void iotHomeAssistantConnectionStatus(void *user,  NotificationsMessage_t *message);
static void iotHomeAssistantGenerateDiscovery(void);
static char *iotHomeAssistantGetName(void);
static cJSON *iotHomeAssistantCreateDevice(char *name, uint8_t *mac);
static void iotHomeAssistantHandleSensor(iotElement_t element, char* name, uint8_t *mac);
static void iotHomeAssitantHandleSensorPub(iotElement_t element, int pubId, char *name, uint8_t *mac);
static cJSON *iotHomeAddStringfToObject(cJSON *object, const char * const key, char *format, ...);

void iotHomeAssistantInit(void)
{
    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_MQTT, iotHomeAssistantConnectionStatus, NULL);
}

static void iotHomeAssistantConnectionStatus(void *user,  NotificationsMessage_t *message)
{
    ESP_LOGI(TAG, "Sending Home Assistant Discovery notifications");
    if (message->data.connectionState == Notifications_ConnectionState_Connected) {
        iotHomeAssistantGenerateDiscovery();
    }
}

static void iotHomeAssistantGenerateDiscovery(void)
{
    iotElement_t element;
    char *name = iotHomeAssistantGetName();
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    for (element = iotElementsHead; element; element = element->next) {
        if ((element->flags & IOT_ELEMENT_FLAGS_DONT_ANNOUNCE) != 0) {
            continue;
        }

        if (element->desc->nrofSubs == 0) {
            // Only handle sensors for the moment
            iotHomeAssistantHandleSensor(element, name, mac);
        }
    }
    if (name != NULL) {
        free(name);
    }
}

static void iotHomeAssistantHandleSensor(iotElement_t element, char* name, uint8_t *mac)
{
    int i;
    for (i = 0; i < element->desc->nrofPubs; i++) {
        switch(VT_BARE_TYPE(PUB_GET_TYPE(element, i))) {
        case VT_PERCENT_RH:
        case VT_CELSIUS:
        case VT_KPA:
            iotHomeAssitantHandleSensorPub(element, i, name, mac);
            break;
        default:
            break;
        }
    }
}

static void iotHomeAssitantHandleSensorPub(iotElement_t element, int pubId, char *name, uint8_t *mac)
{
    char *errorMsg = NULL;
    char *stateTopic = NULL;
    char *discoveryTopic = NULL;
    char *discoveryJSON = NULL;
    char *uniqueID = NULL;
    cJSON *deviceObj;
    iotValueType_t valueType = PUB_GET_TYPE(element, pubId);
    cJSON *configObj = cJSON_CreateObject();
    ON_NULL_GOTO_ERROR(configObj, "create object");

    stateTopic = iotElementPubTopic(element, pubId);
    ON_NULL_GOTO_ERROR(stateTopic, "get state topic");

    ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, STATE_TOPIC, stateTopic), "add state topic");

    deviceObj = iotHomeAssistantCreateDevice(name, mac);
    ON_NULL_GOTO_ERROR(deviceObj, "Device Object");
    cJSON_AddItemToObject(configObj, "device", deviceObj);

    switch(VT_BARE_TYPE(valueType)) {
    case VT_PERCENT_RH:
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, DEVCIE_CLASS, "humidity"), "add device class");
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, UNIT_OF_MEASUREMENT, PERCENT_RH), "add unit_of_measurement");
        break;
    case VT_CELSIUS:
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, DEVCIE_CLASS, "temperature"), "add device class");
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, UNIT_OF_MEASUREMENT, DEG_C), "add unit_of_measurement");
        break;
    case VT_KPA:
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, DEVCIE_CLASS, "pressure"), "add device class");
        ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, UNIT_OF_MEASUREMENT, KPA), "add unit_of_measurement");
        break;
    default:
        break;
    }

    asprintf(&uniqueID, "homething%02x%02x%02x%02x%02x%02x/%s_%s",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], element->name, PUB_GET_NAME(element, pubId));
    ON_NULL_GOTO_ERROR(uniqueID, "create unique id");
    ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(configObj, UNIQUE_ID, uniqueID), "add unique");

    asprintf(&discoveryTopic, "homeassistant/sensor/%s/config", uniqueID);
    ON_NULL_GOTO_ERROR(discoveryTopic, "discovery topic");

    discoveryJSON = cJSON_Print(configObj);
    iotMqttPublish(discoveryTopic, discoveryJSON, strlen(discoveryJSON), 0, 1);

    ESP_LOGI(TAG, "Sent discovery information for %s %s", element->name, PUB_GET_NAME(element, pubId));
error:
    if (errorMsg) {
        ESP_LOGE(TAG, "iotHomeAssitantHandleSensorPub: failed @ %s", errorMsg);
    }
    if (uniqueID) {
        free(uniqueID);
    }
    if (discoveryTopic) {
        free(discoveryTopic);
    }
    if (discoveryJSON) {
        free(discoveryJSON);
    }
    if (configObj) {
        cJSON_Delete(configObj);
    }
}

static char *iotHomeAssistantGetName(void)
{
    char *name = NULL;
    esp_err_t err;
    nvs_handle handle;
    err = nvs_open("thing", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_str_alloc(handle, "desc", &name);
        nvs_close(handle);
        if (err == ESP_OK) {
            return name;
        }
    }
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    asprintf(&name, "HomeThing (%02x%02x%02x%02x%02x%02x)", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return name;
}

static cJSON *iotHomeAssistantCreateDevice(char *name, uint8_t *mac)
{
    char *errorMsg;
    char macStr[18];
    cJSON *deviceObj = cJSON_CreateObject();

    ON_NULL_GOTO_ERROR(deviceObj, "create obj");
    ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(deviceObj, NAME, name), "add name");
    ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(deviceObj, SW_VERSION, appVersion), "add version");
    sprintf(macStr, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ON_NULL_GOTO_ERROR(cJSON_AddStringToObject(deviceObj, IDENTIFIERS, macStr), "add identifiers");

    return deviceObj;

error:
    ESP_LOGE(TAG, "iotHomeAssistantCreateDevice: Failed @ %s", errorMsg);
    if (deviceObj) {
        cJSON_Delete(deviceObj);
    }
    return NULL;
}

static cJSON *iotHomeAddStringfToObject(cJSON *object, const char * const key, char *format, ...)
{
    va_list args;
    cJSON *result;
    char *str = NULL;
    va_start(format, args);
    vasprintf(&str, format, args);
    if (str == NULL) {
        goto error;
    }
    result = cJSON_AddStringToObject(object, key, str);
error:
    if (str) {
        free(str);
    }
    return result;
}

#endif