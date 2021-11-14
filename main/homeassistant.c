#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "iot.h"
#include "notifications.h"
#include "updater.h"
#include "nvs_flash.h"
#include "utils.h"

static const char *TAG="hass";
static const char *DESC="desc";

static const char *DEGREES_C = "\xC2\xB0""C";

static void onMqttStatusUpdated(void *user,  NotificationsMessage_t *message);
static void sendDiscoveryMessages();
static char* deviceGetDescription(void);
static void processElement(char *identifier, char *deviceDescription, iotElement_t element);
static void processSensor(char *identifier, char *deviceDescription, iotElement_t element);
static void processSwitch(char *identifier, char *deviceDescription, iotElement_t element);
static void addDeviceDetails(char *identifier, char *deviceDescription, cJSON * obj);

void homeAssistantDiscoveryInit(void)
{
    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_MQTT, onMqttStatusUpdated, NULL);
}

static void onMqttStatusUpdated(void *user,  NotificationsMessage_t *message)
{
    if (message->data.connectionState == Notifications_ConnectionState_Connected) {
        sendDiscoveryMessages();
    }
}

static void sendDiscoveryMessages()
{
    iotElementIterator_t iterator = IOT_ELEMENT_ITERATOR_START;
    iotElement_t element;
    char identifier[13];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(identifier, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    char *deviceDescription = deviceGetDescription();

    while(iotElementIterate(&iterator, true, &element)) {
        processElement(identifier, deviceDescription, element);
    }
    if (deviceDescription != NULL) {
        free(deviceDescription);
    }
}

static void processElement(char *identifier, char *deviceDescription, iotElement_t element)
{
    const iotElementDescription_t *description = iotElementGetDescription(element);
    switch(description->type) {
    case IOT_ELEMENT_TYPE_SENSOR_HUMIDITY:
    case IOT_ELEMENT_TYPE_SENSOR_TEMPERATURE:
        processSensor(identifier, deviceDescription, element);
        break;
    case IOT_ELEMENT_TYPE_SWITCH:
        processSwitch(identifier, deviceDescription, element);
        break;
    default:
        break;
    }
}

static void processSensor(char *identifier, char *deviceDescription, iotElement_t element)
{
    const iotElementDescription_t *description = iotElementGetDescription(element);
    int pubId;
    for (pubId = 0; pubId < description->nrofPubs; pubId ++) {
        const char *deviceClass = NULL;
        const char *unitOfMeasurement = NULL;
        switch (description->pubs[pubId][0] & 0x7f) {
        case IOT_VALUE_TYPE_PERCENT_RH:
            deviceClass = "humidity";
            unitOfMeasurement = "% RH";
            break;
        case IOT_VALUE_TYPE_CELSIUS:
            deviceClass = "temperature";
            unitOfMeasurement = DEGREES_C;
            break;
        case IOT_VALUE_TYPE_KPA:
            deviceClass = "pressure";
            unitOfMeasurement = "kPa";
            break;
        default:
            break;
        }
        if (deviceClass == NULL) {
            continue;
        }
        cJSON *object = cJSON_CreateObject();
        if (object == NULL) {
            continue;
        }

        cJSON_AddStringToObject(object, "device_class", deviceClass);
        cJSON_AddStringToObject(object, "unit_of_measurement", unitOfMeasurement);

        const char *pubName = iotElementGetPubName(element, pubId);
        const char *elementName = iotElementGetName(element);
        char *uniqueId = NULL;

        asprintf(&uniqueId, "%s:%s:%s", identifier, elementName, pubName);
        if (uniqueId != NULL) {
            cJSON_AddStringToObject(object, "unique_id", uniqueId);
        }

        char *name = NULL;
        if (pubName[0] == 0) {
            asprintf(&name, "%s %s", deviceDescription, elementName);
        } else {
            asprintf(&name, "%s %s %s", deviceDescription, elementName, pubName);
        }
        if (name != NULL) {
            cJSON_AddStringToObject(object, "name", name);
            free(name);
        }

        char *stateTopic = NULL;
        size_t stateTopicSize = 0;

        iotElementGetPubPath(element, pubId, stateTopic, &stateTopicSize);
        if (stateTopicSize == 0) {
            cJSON_Delete(object);
            continue;
        }
        stateTopic = malloc(stateTopicSize);
        if (stateTopic == NULL) {
            cJSON_Delete(object);
            continue;
        }
        if (iotElementGetPubPath(element, pubId, stateTopic, &stateTopicSize) == NULL) {
            free(stateTopic);
            cJSON_Delete(object);
            continue;
        }
        cJSON_AddStringToObject(object, "state_topic", stateTopic);
        free(stateTopic);

        addDeviceDetails(identifier, deviceDescription, object);

        char *json = cJSON_PrintUnformatted(object);
        cJSON_Delete(object);
        if (json == NULL) {
            continue;
        }

        char *discoveryPath = NULL;
        asprintf(&discoveryPath, "homeassistant/sensor/%s/%s_%s/config", identifier,
                 iotElementGetName(element), iotElementGetPubName(element, pubId));
        printf("Element = %p Name = %s path = %s\n", element, iotElementGetName(element), discoveryPath);

        if (discoveryPath != NULL) {
            iotMqttPublish(discoveryPath, json, strlen(json), 0, 1);
            free(discoveryPath);
        }
        free(json);
    }
}

static void processSwitch(char *identifier, char *deviceDescription, iotElement_t element)
{

}

static void addDeviceDetails(char *identifier, char *deviceDescription, cJSON * obj)
{
    cJSON *devObject = cJSON_AddObjectToObject(obj, "dev");
    if (devObject == NULL) {
        return;
    }

    cJSON_AddStringToObject(devObject, "ids", identifier);

    if (deviceDescription) {
        cJSON_AddStringToObject(devObject, "name", deviceDescription);
    }

    cJSON_AddStringToObject(devObject, "sw", updaterGetVersion());
}

static char* deviceGetDescription(void)
{
    esp_err_t err;
    nvs_handle handle;
    char *desc = NULL;

    err = nvs_open("thing", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open thing section (%d)", err);
        return NULL;
    }
    err = nvs_get_str_alloc(handle, DESC, &desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get description (%d)", err);
        return NULL;
    }
    return desc;
}