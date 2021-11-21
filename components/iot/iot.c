
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "cbor.h"

#include "iot.h"
#include "iotInternal.h"
#include "wifi.h"
#include "notifications.h"
#include "sdkconfig.h"
#include "deviceprofile.h"

static const char *TAG="IOT";
const char *IOT_DEFAULT_CONTROL_STR="ctrl";

iotElement_t iotElementsHead = NULL;

static char mqttPathPrefix[MQTT_PATH_PREFIX_LEN];
static char mqttCommonCtrlSub[MQTT_COMMON_CTRL_SUB_LEN];

static bool iotElementPubSendUpdate(iotElement_t element, int pubId, iotValue_t value);
static bool iotElementSendUpdate(iotElement_t element);
static bool iotElementSubscribe(iotElement_t element);
static void iotWifiConnectionStatus(void *user,  NotificationsMessage_t *message);
static char *checkedPathBuffer(const char *elementName, const char *subTopic, char *buffer, size_t *bufferLen);

int iotInit(void)
{
    int result = 0;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    sprintf(mqttPathPrefix, "homething/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(mqttCommonCtrlSub, "%s/+/%s", mqttPathPrefix, IOT_DEFAULT_CONTROL_STR);

    ESP_LOGI(TAG, "device path: %s", mqttPathPrefix);

    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, iotWifiConnectionStatus, NULL);

    result = mqttInit();
    if (result != 0) {
        return result;
    }

    return iotDeviceInit();
}

void iotStart()
{
    iotDeviceStart();
}

iotElement_t iotNewElement(const iotElementDescription_t *desc, uint32_t flags, iotElementCallback_t callback,
                           void *userContext, const char *nameFormat, ...)
{
    va_list args;
    struct iotElement *newElement = malloc(sizeof(struct iotElement) + (sizeof(iotValue_t) * desc->nrofPubs));
    if (newElement == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for iotElement (nameFormat %s)", nameFormat);
        return NULL;
    }

    va_start(args, nameFormat);
    if (vasprintf(&newElement->name, nameFormat, args) == -1) {
        free(newElement);
        ESP_LOGE(TAG, "Failed to allocate memory for iotElement name (nameFormat %s)", nameFormat);
        return NULL;
    }
    va_end(args);
    memset(&newElement->values, 0, sizeof(iotValue_t) * desc->nrofPubs);
    newElement->desc = desc;
    newElement->flags = flags;
    newElement->callback = callback;
    newElement->userContext = userContext;
    newElement->next = iotElementsHead;
    iotElementsHead = newElement;
    return newElement;
}

void iotElementPublish(iotElement_t element, int pubId, iotValue_t value)
{
    bool updateRequired = false;

    if (pubId >= element->desc->nrofPubs) {
        ESP_LOGE(TAG, "Invalid publish id %d for element %s", pubId, element->name);
        return;
    }

    switch(VT_BARE_TYPE(PUB_GET_TYPE(element, pubId))) {
    case VT_BOOL:
        updateRequired = value.b != element->values[pubId].b;
        break;
    case VT_INT:
    case VT_HUNDREDTHS:
    case VT_PERCENT_RH:
    case VT_CELSIUS:
    case VT_KPA:
        updateRequired = value.i != element->values[pubId].i;
        break;
    case VT_FLOAT:
        updateRequired = value.f != element->values[pubId].f;
        break;
    case VT_STRING:
    case VT_BINARY:
        updateRequired = true;
        break;
    case VT_ON_CONNECT:
        break;
    default:
        ESP_LOGE(TAG, "Unknown pub type %d for %s!", PUB_GET_TYPE(element, pubId), PUB_GET_NAME(element, pubId));
        return;
    }
    if (updateRequired) {
        element->values[pubId] = value;
        if (mqttIsConnected) {
            iotElementPubSendUpdate(element, pubId, value);
        }
    }
}

static bool iotElementPubSendUpdate(iotElement_t element, int pubId, iotValue_t value)
{
    char *path;
    char payload[30] = "";
    char *message = payload;
    int messageLen = -1;
    const char *prefix = mqttPathPrefix;
    int rc;
    iotElementCallback_t callback = NULL;
    iotElementCallbackDetails_t details;

    if (element->desc->pubs[pubId][PUB_INDEX_NAME] == 0) {
        asprintf(&path, "%s/%s", prefix, element->name);
    } else {
        asprintf(&path, "%s/%s/%s", prefix, element->name, PUB_GET_NAME(element, pubId));
    }

    if (path == NULL) {
        ESP_LOGE(TAG, "Failed to allocate path when publishing message");
        return false;
    }
    iotValueType_t valueType = PUB_GET_TYPE(element, pubId);
    int retain = VT_IS_RETAINED(valueType);

    if (VT_BARE_TYPE(valueType) == VT_ON_CONNECT) {
        callback = element->callback;
        if (callback == NULL) {
            ESP_LOGE(TAG, "Element callback was NULL when publishing message");
            free(path);
            return false;
        }
        details.index = pubId;
        callback(element->userContext, element, IOT_CALLBACK_ON_CONNECT, &details);
        valueType = details.valueType;
        value = details.value;
    }

    switch(VT_BARE_TYPE(valueType)) {
    case VT_BOOL:
        message = (value.b) ? "on":"off";
        break;

    case VT_INT:
        sprintf(payload, "%d", value.i);
        break;

    case VT_FLOAT:
        sprintf(payload, "%f", value.f);
        break;

    case VT_HUNDREDTHS:
    case VT_PERCENT_RH:
    case VT_CELSIUS:
    case VT_KPA: {
        char *str = payload;
        int hundredths = value.i;
        if (hundredths < 0) {
            hundredths *= -1;
            str[0] = '-';
            str++;
        }
        sprintf(str, "%d.%02d", hundredths / 100, hundredths % 100);
    }
    break;

    case VT_STRING:
        message = (char*)value.s;
        if (message == NULL) {
            message = "";
        }
        break;

    case VT_BINARY:
        if (value.bin == NULL) {
            return false;
        }
        message = (char*)value.bin->data;
        messageLen = (int)value.bin->len;
        break;

    default:
        free(path);
        return false;
    }
    if (messageLen == -1) {
        messageLen = strlen((char*)message);
    }

    rc = iotMqttPublish(path, message, messageLen, 0, retain);
    free(path);
    if (callback) {
        callback(element->userContext, element, IOT_CALLBACK_ON_CONNECT_RELEASE, &details);
    }
    if (rc != 0) {
        ESP_LOGW(TAG, "PUB: Failed to send message to %s rc %d", path, rc);
        return false;
    } else {
        if (VT_BARE_TYPE(valueType) == VT_BINARY) {
            ESP_LOGV(TAG, "PUB: Sent %d bytes to %s (retain: %d)", messageLen, path, retain);
        } else {
            ESP_LOGV(TAG, "PUB: Sent %s (%d) to %s (retain: %d)", (char *)message, messageLen, path, retain);
        }
    }
    return true;
}

static bool iotElementSendUpdate(iotElement_t element)
{
    bool result = true;
    int i;
    for (i = 0; i < element->desc->nrofPubs && result; i++) {
        result = iotElementPubSendUpdate(element, i, element->values[i]);
    }

    return result;
}

int iotStrToBool(const char *str, bool *out)
{
    if ((strcasecmp(str, "on") == 0) || (strcasecmp(str, "true") == 0)) {
        *out = true;
    } else if ((strcasecmp(str, "off") == 0) || (strcasecmp(str, "false") == 0)) {
        *out = false;
    } else {
        return 1;
    }
    return 0;
}

static void iotElementSubUpdate(iotElement_t element, int subId, char *payload, size_t len)
{
    iotValue_t value;
    iotBinaryValue_t binValue;
    bool allowNegative = false;
    const char *name;
    if (element->desc->subs[subId].type_name[SUB_INDEX_NAME] == 0) {
        name = IOT_DEFAULT_CONTROL_STR;
    } else {
        name = SUB_GET_NAME(element, subId);
    }
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s/%s\"", payload, element->name, name);
    switch(VT_BARE_TYPE(SUB_GET_TYPE(element, subId))) {
    case VT_BOOL:
        if (iotStrToBool(payload, &value.b)) {
            ESP_LOGW(TAG, "Invalid value for bool type (%s)", payload);
            return;
        }
        break;

    case VT_INT:
        if (sscanf(payload, "%d", &value.i) == 0) {
            ESP_LOGW(TAG, "Invalid value for int type (%s)", payload);
            return;
        }
        break;

    case VT_CELSIUS:
    case VT_HUNDREDTHS:
        allowNegative = true; /* fallthrough */
    case VT_PERCENT_RH:
    case VT_KPA: {
        char *decimalPoint = NULL;
        if ((payload[0] == '-')  && (!allowNegative)) {
            ESP_LOGW(TAG, "Negative values not allowed for topic (%s)", payload);
            return;
        }
        decimalPoint = strchr(payload, '.');
        if (decimalPoint != NULL) {
            *decimalPoint = 0;
        }

        if (sscanf(payload, "%d", &value.i) ==  0) {
            ESP_LOGW(TAG, "Invalid value for topic type (%s)", payload);
            return;
        }

        value.i *= 100;
        if (decimalPoint != NULL) {
            unsigned int hundredths = 0;
            if (strlen(decimalPoint +1) > 2) {
                decimalPoint[3] = 0;
            }
            if (sscanf(payload, "%u", &hundredths) ==  0) {
                ESP_LOGW(TAG, "Invalid value for topic type (%s)", payload);
                return;
            }
            value.i += hundredths;
        }
    }
    break;

    case VT_FLOAT:
        if (sscanf(payload, "%f", &value.f) == 0) {
            ESP_LOGW(TAG, "Invalid value for float type (%s)", payload);
            return;
        }
        break;

    case VT_STRING:
        value.s = payload;
        break;

    case VT_BINARY:
        binValue.data = (uint8_t*)payload;
        binValue.len = len;
        value.bin = &binValue;
        break;

    default:
        ESP_LOGE(TAG, "Unknown value type %d for %s/%s", SUB_GET_TYPE(element, subId), element->name, name);
        return;
    }
    iotElementCallbackDetails_t details;
    details.index = subId;
    details.value = value;
    element->callback(element->userContext, element, IOT_CALLBACK_ON_SUB, &details);
}

static bool iotElementSubscribe(iotElement_t element)
{
    int i;
    for (i = 0; i < element->desc->nrofSubs; i++) {
        if (element->desc->subs[i].type_name[SUB_INDEX_NAME] != 0) {
            char *path = NULL;
            bool result = false;
            asprintf(&path, "%s/%s/%s", mqttPathPrefix, element->name, SUB_GET_NAME(element, i));
            if (path != NULL) {
                result = mqttSubscribe(path);
                free(path);
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory when subscribing to path %s/%s", element->name, SUB_GET_NAME(element, i));
            }

            if (!result) {
                return false;
            }
        }
    }
    return true;
}

void iotMqttProcessMessage(char *topic, char *data, int dataLen)
{
    bool found = false;
    size_t len = strlen(mqttPathPrefix);

    if ((strncmp(topic, mqttPathPrefix, len) == 0) && (topic[len] == '/')) {
        topic += len + 1;
        for (iotElement_t element = iotElementsHead; element != NULL; element = element->next) {
            len = strlen(element->name);
            if ((strncmp(topic, element->name, len) == 0) && (topic[len] == '/')) {
                topic += len + 1;
                int i;
                for (i = 0; i < element->desc->nrofSubs; i++) {
                    const char *name;
                    if (element->desc->subs[i].type_name[SUB_INDEX_NAME] == 0) {
                        name = IOT_DEFAULT_CONTROL_STR;
                    } else {
                        name = SUB_GET_NAME(element, i);
                    }
                    if (strcmp(topic, name) == 0) {
                        iotElementSubUpdate(element, i, data, dataLen);
                        found = true;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        ESP_LOGW(TAG, "Unexpected message, topic %s", topic);
    }
}

void iotMqttConnected(void)
{
    mqttSubscribe(mqttCommonCtrlSub);

    for (iotElement_t element = iotElementsHead; (element != NULL); element = element->next) {
        if (iotElementSubscribe(element)) {
            iotElementSendUpdate(element);
        }
    }
}

static void iotWifiConnectionStatus(void *user,  NotificationsMessage_t *message)
{
    bool connected = message->data.connectionState == Notifications_ConnectionState_Connected;
    if (mqttIsSetup) {
        mqttNetworkConnected(connected);
    }
}

bool iotElementIterate(iotElementIterator_t *iterator, bool onlyAnnounced, iotElement_t *nextElement)
{
    iotElement_t element = (iotElement_t)*iterator;
    if (element == NULL) {
        element = iotElementsHead;
    }
    for (; (element != NULL); element = element->next) {
        if (onlyAnnounced && ((element->flags & IOT_ELEMENT_FLAGS_DONT_ANNOUNCE) != 0)) {
            continue;
        }
        *nextElement = element;
        *iterator = element->next;
        return true;
    }
    return false;
}

char *iotElementGetName(iotElement_t element)
{
    return element->name;
}

const iotElementDescription_t *iotElementGetDescription(iotElement_t element)
{
    return element->desc;
}

static char *checkedPathBuffer(const char *elementName, const char *subTopic, char *buffer, size_t *bufferLen)
{
    char *result = NULL;
    size_t required = strlen(mqttPathPrefix) + 1  /* / */ +
                      strlen(elementName) + 1 /* / */ +
                      (subTopic != NULL ? strlen(subTopic) + 1 /* \0 */ :  0);
    if (required <= *bufferLen) {
        if (subTopic == NULL) {
            sprintf(buffer, "%s/%s", mqttPathPrefix, elementName);
        } else {
            sprintf(buffer, "%s/%s/%s", mqttPathPrefix, elementName, subTopic);
        }
        result = buffer;
    }

    *bufferLen = required;
    return result;
}

char *iotElementGetBasePath(iotElement_t element, char *buffer, size_t *bufferLen)
{
    return checkedPathBuffer(element->name, NULL, buffer, bufferLen);
}

char *iotElementGetPubPath(iotElement_t element, int pubId, char *buffer, size_t *bufferLen)
{
    const char *pubName;
    if (pubId >= element->desc->nrofPubs) {
        bufferLen = 0;
        return NULL;
    }

    if (element->desc->pubs[pubId][PUB_INDEX_NAME] == 0) {
        pubName = NULL;
    } else {
        pubName = &element->desc->pubs[pubId][PUB_INDEX_NAME];
    }

    return checkedPathBuffer(element->name, pubName, buffer, bufferLen);
}

char *iotElementGetSubPath(iotElement_t element, int subId, char *buffer, size_t *bufferLen)
{
    const char *subName;
    if (subId >= element->desc->nrofSubs) {
        bufferLen = 0;
        return NULL;
    }

    if (element->desc->subs[subId].type_name[SUB_INDEX_NAME] == 0) {
        subName = IOT_DEFAULT_CONTROL_STR;
    } else {
        subName = &element->desc->subs[subId].type_name[SUB_INDEX_NAME];
    }

    return checkedPathBuffer(element->name, subName, buffer, bufferLen);
}

const char *iotElementGetPubName(iotElement_t element, int pubId)
{
    if (pubId >= element->desc->nrofPubs) {
        return NULL;
    }

    return &element->desc->pubs[pubId][PUB_INDEX_NAME];
}

const char *iotElementGetSubName(iotElement_t element, int subId)
{
    const char *subName;
    if (subId >= element->desc->nrofSubs) {
        return NULL;
    }

    if (element->desc->subs[subId].type_name[SUB_INDEX_NAME] == 0) {
        subName = IOT_DEFAULT_CONTROL_STR;
    } else {
        subName = &element->desc->subs[subId].type_name[SUB_INDEX_NAME];
    }
    return subName;
}