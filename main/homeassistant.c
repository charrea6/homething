#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"
#include "cJSON_AddOns.h"
#include "iot.h"
#include "notifications.h"
#include "updater.h"
#include "nvs_flash.h"
#include "utils.h"

static const char *TAG="hass";
static const char *DESC="desc";

static const char *DEGREES_C="\xC2\xB0""C";

#if CONFIG_IDF_TARGET_ESP32
    static const char *MODEL="esp32";
#else
    static const char *MODEL="esp8266";
#endif

struct DeviceDetails {
    char identifier[13];
    char *deviceDescription;
};

static void onMqttStatusUpdated(void *user,  NotificationsMessage_t *message);
static void sendDiscoveryMessages();
static char* deviceGetDescription(void);
static void processElement(struct DeviceDetails *deviceDetails, iotElement_t element);
static void processSensor(struct DeviceDetails *deviceDetails, iotElement_t element);
static void processSwitch(struct DeviceDetails *deviceDetails, iotElement_t element);
static void addDeviceDetails(struct DeviceDetails *deviceDetails, cJSON * obj);
static void sendDiscoveryMessage(struct DeviceDetails *deviceDetails, const char* type,
                                 iotElement_t element, const char *pubName, cJSON *object);

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
    struct DeviceDetails details;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(details.identifier, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    details.deviceDescription = deviceGetDescription();

    while(iotElementIterate(&iterator, true, &element)) {
        processElement(&details, element);
    }
    if (details.deviceDescription != NULL) {
        free(details.deviceDescription);
    }
}

static void processElement(struct DeviceDetails *deviceDetails, iotElement_t element)
{
    const iotElementDescription_t *description = iotElementGetDescription(element);
    switch(description->type) {
    case IOT_ELEMENT_TYPE_SENSOR_HUMIDITY:
    case IOT_ELEMENT_TYPE_SENSOR_TEMPERATURE:
        processSensor(deviceDetails, element);
        break;
    case IOT_ELEMENT_TYPE_SWITCH:
        processSwitch(deviceDetails, element);
        break;
    default:
        break;
    }
}

static void processSensor(struct DeviceDetails *deviceDetails, iotElement_t element)
{
    const iotElementDescription_t *description = iotElementGetDescription(element);
    int pubId;
    for (pubId = 0; pubId < description->nrofPubs; pubId ++) {
        const char *deviceClass = NULL;
        const char *unitOfMeasurement = NULL;
        switch (description->pubs[pubId].type) {
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

        cJSON_AddStringReferenceToObjectCS(object, "device_class", deviceClass);
        cJSON_AddStringReferenceToObjectCS(object, "unit_of_measurement", unitOfMeasurement);

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

        cJSON_AddStringToObjectCS(object, "state_topic", stateTopic);
        free(stateTopic);

        const char *pubName = iotElementGetPubName(element, pubId);
        sendDiscoveryMessage(deviceDetails, "sensor", element, pubName, object);
    }
}

static void processSwitch(struct DeviceDetails *deviceDetails, iotElement_t element)
{
    const iotElementDescription_t *description = iotElementGetDescription(element);
    const char *pubName = NULL;
    const char *ctrlName = NULL;
    if ((description->nrofPubs == 0) || (description->nrofSubs == 0)) {
        return;
    }
    pubName = iotElementGetPubName(element, 0);
    ctrlName = iotElementGetSubName(element, 0);
    cJSON *object = cJSON_CreateObject();
    if (object == NULL) {
        return;
    }
    char *topic = NULL;
    asprintf(&topic, "~/%s", pubName);
    if (topic == NULL) {
        cJSON_Delete(object);
        return;
    }
    cJSON_AddStringToObjectCS(object, "state_topic", topic);
    free(topic);

    asprintf(&topic, "~/%s", ctrlName);
    if (topic == NULL) {
        cJSON_Delete(object);
        return;
    }
    cJSON_AddStringToObjectCS(object, "command_topic", topic);
    free(topic);

    size_t topicSize = 0;
    iotElementGetBasePath(element, NULL, &topicSize);
    if (topicSize == 0) {
        cJSON_Delete(object);
        return;
    }
    topic = malloc(topicSize);
    if (topic == NULL) {
        cJSON_Delete(object);
        return;
    }
    if (iotElementGetBasePath(element, topic, &topicSize) == NULL) {
        free(topic);
        cJSON_Delete(object);
        return;
    }
    cJSON_AddStringToObjectCS(object, "~", topic);
    free(topic);

    cJSON_AddStringReferenceToObjectCS(object, "payload_off", "off");
    cJSON_AddStringReferenceToObjectCS(object, "payload_on", "on");

    sendDiscoveryMessage(deviceDetails, "switch", element, NULL, object);
}

static void sendDiscoveryMessage(struct DeviceDetails *deviceDetails, const char* type,
                                 iotElement_t element, const char *pubName, cJSON *object)
{
    const char *elementName = iotElementGetName(element);
    char *uniqueId = NULL;

    asprintf(&uniqueId, "%s:%s:%s", deviceDetails->identifier, elementName, pubName == NULL ? "":pubName);    
    if (uniqueId != NULL) {
        cJSON_AddStringToObjectCS(object, "unique_id", uniqueId);
    }

    char *name = NULL;
    if ((pubName == NULL) || (pubName[0] == 0)) {
        asprintf(&name, "%s %s", deviceDetails->deviceDescription, elementName);
    } else {
        asprintf(&name, "%s %s %s", deviceDetails->deviceDescription, elementName, pubName);
    }
    if (name != NULL) {
        cJSON_AddStringToObjectCS(object, "name", name);
        free(name);
    }

    addDeviceDetails(deviceDetails, object);

    char *json = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    if (json == NULL) {
        return;
    }

    char *discoveryPath = NULL;
    asprintf(&discoveryPath, "homeassistant/%s/%s/%s_%s/config", type, deviceDetails->identifier,
            iotElementGetName(element), pubName == NULL ? "":pubName);
    if (discoveryPath != NULL) {
        iotMqttPublish(discoveryPath, json, strlen(json), 0, 1);
        free(discoveryPath);
    }
    free(json);
}

static void addDeviceDetails(struct DeviceDetails *deviceDetails, cJSON * obj)
{
    cJSON *devObject = cJSON_AddObjectToObjectCS(obj, "dev");
    if (devObject == NULL) {
        return;
    }

    cJSON_AddStringToObjectCS(devObject, "ids", deviceDetails->identifier);

    if (deviceDetails->deviceDescription) {
        cJSON_AddStringToObjectCS(devObject, "name", deviceDetails->deviceDescription);
    }

    cJSON_AddStringToObjectCS(devObject, "sw", updaterGetVersion());
    cJSON_AddStringReferenceToObjectCS(devObject, "mf", "Homething");
    cJSON_AddStringToObjectCS(devObject, "mdl", MODEL);
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