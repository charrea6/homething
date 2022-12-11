
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

#include "iot.h"
#include "iotInternal.h"
#include "wifi.h"
#include "notifications.h"
#include "sdkconfig.h"

#define HUNDRETHS_MAX_INTEGER_DIGITS 8
#define HUNDRETHS_MAX_DECIMAL_DIGITS 2
#define HUNDRETHS_MAX_DIGITS (HUNDRETHS_MAX_INTEGER_DIGITS + HUNDRETHS_MAX_DECIMAL_DIGITS)


static const char *TAG="IOT";
const char *IOT_DEFAULT_CONTROL_STR="ctrl";

iotElement_t iotElementsHead = NULL;

static char mqttPathPrefix[MQTT_PATH_PREFIX_LEN];
static char mqttCommonCtrlSub[MQTT_COMMON_CTRL_SUB_LEN];
static iotValueUpdatePolicy_e valueUpdatePolicy = IOT_VALUE_UPDATE_POLICY_ON_CHANGE;

static bool iotElementPubSendUpdate(iotElement_t element, int pubId, iotValue_t value);
static bool iotElementSendUpdate(iotElement_t element);
static bool iotElementSubscribe(iotElement_t element);
static void iotWifiConnectionStatus(void *user,  NotificationsMessage_t *message);
static char *checkedPathBuffer(const char *elementName, const char *subTopic, char *buffer, size_t *bufferLen);

int iotInit(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    sprintf(mqttPathPrefix, "homething/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(mqttCommonCtrlSub, "%s/+/%s", mqttPathPrefix, IOT_DEFAULT_CONTROL_STR);

    ESP_LOGI(TAG, "device path: %s", mqttPathPrefix);

    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, iotWifiConnectionStatus, NULL);
    return mqttInit();
}

void iotSetValueUpdatePolicy(iotValueUpdatePolicy_e policy)
{
    valueUpdatePolicy = policy;
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
    newElement->humanDescription = NULL;
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
    if (valueUpdatePolicy == IOT_VALUE_UPDATE_POLICY_ALWAYS) {
        updateRequired = true;
    } else {
        switch(element->desc->pubs[pubId].type) {
        case IOT_VALUE_TYPE_BOOL:
            updateRequired = value.b != element->values[pubId].b;
            break;
        case IOT_VALUE_TYPE_INT:
        case IOT_VALUE_TYPE_HUNDREDTHS:
        case IOT_VALUE_TYPE_PERCENT_RH:
        case IOT_VALUE_TYPE_CELSIUS:
        case IOT_VALUE_TYPE_KPA:
            updateRequired = value.i != element->values[pubId].i;
            break;
        case IOT_VALUE_TYPE_FLOAT:
            updateRequired = value.f != element->values[pubId].f;
            break;
        case IOT_VALUE_TYPE_STRING:
        case IOT_VALUE_TYPE_BINARY:
            updateRequired = true;
            break;
        case IOT_VALUE_TYPE_ON_CONNECT:
            break;
        default:
            ESP_LOGE(TAG, "Unknown pub type %d for %s!", element->desc->pubs[pubId].type, element->desc->pubs[pubId].name);
            return;
        }
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

    if (element->desc->pubs[pubId].name[0] == 0) {
        asprintf(&path, "%s/%s", prefix, element->name);
    } else {
        asprintf(&path, "%s/%s/%s", prefix, element->name, element->desc->pubs[pubId].name);
    }

    if (path == NULL) {
        ESP_LOGE(TAG, "Failed to allocate path when publishing message");
        return false;
    }
    iotValueType_t valueType = element->desc->pubs[pubId].type;
    int retain = element->desc->pubs[pubId].retained;

    if (valueType == IOT_VALUE_TYPE_ON_CONNECT) {
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

    switch(valueType) {
    case IOT_VALUE_TYPE_BOOL:
        message = (value.b) ? "on":"off";
        break;

    case IOT_VALUE_TYPE_INT:
        sprintf(payload, "%d", value.i);
        break;

    case IOT_VALUE_TYPE_FLOAT:
        sprintf(payload, "%f", value.f);
        break;

    case IOT_VALUE_TYPE_HUNDREDTHS:
    case IOT_VALUE_TYPE_PERCENT_RH:
    case IOT_VALUE_TYPE_CELSIUS:
    case IOT_VALUE_TYPE_KPA: {
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

    case IOT_VALUE_TYPE_STRING:
        message = (char*)value.s;
        if (message == NULL) {
            message = "";
        }
        break;

    case IOT_VALUE_TYPE_BINARY:
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
        if (valueType == IOT_VALUE_TYPE_BINARY) {
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

int iotParseString(const char *str, const iotValueType_t type, iotValue_t *out)
{
    bool allowNegative = false;

    switch(type) {
    case IOT_VALUE_TYPE_BOOL:
        if (iotStrToBool(str, &out->b)) {
            return -1;
        }
        break;

    case IOT_VALUE_TYPE_INT:
        if (sscanf(str, "%d", &out->i) == 0) {
            return -1;
        }
        break;

    case IOT_VALUE_TYPE_CELSIUS:
    case IOT_VALUE_TYPE_HUNDREDTHS:
        allowNegative = true; /* fallthrough */
    case IOT_VALUE_TYPE_PERCENT_RH:
    case IOT_VALUE_TYPE_KPA: {
        int hundreths = 0;
        int i;
        const char *ch = str;
        bool negative = false;

        if (*ch == '-') {
            if (!allowNegative) {
                return -1;
            }
            negative = true;
            ch++;
        }

        for (i = 0; i < HUNDRETHS_MAX_INTEGER_DIGITS && *ch; i++, ch++) {
            if (*ch == '.') {
                break;
            }
            if ((*ch >= '0') && (*ch <= '9')) {
                hundreths = (hundreths * 10) + (*ch - '0');
            } else {
                return -1;
            }
        }
        if (*ch == '.') {
            ch++;
            for (i = 0; i < HUNDRETHS_MAX_DECIMAL_DIGITS && *ch; i++, ch++) {
                if ((*ch >= '0') && (*ch <= '9')) {
                    hundreths = (hundreths * 10) + (*ch - '0');
                } else {
                    return -1;
                }
            }
            if (i < HUNDRETHS_MAX_DECIMAL_DIGITS) {
                for (; i < HUNDRETHS_MAX_DECIMAL_DIGITS; i++) {
                    hundreths *= 10;
                }
            }
        } else {
            hundreths *= 100;
        }
        if (negative) {
            hundreths *= -1;
        }
        out->i = hundreths;
    }
    break;

    case IOT_VALUE_TYPE_FLOAT:
        if (sscanf(str, "%f", &out->f) == 0) {
            return -1;
        }
        break;

    case IOT_VALUE_TYPE_STRING:
        out->s = str;
        break;

    case IOT_VALUE_TYPE_BINARY:
    default:
        return -1;
    }

    return 0;
}

static void iotElementSubUpdate(iotElement_t element, int subId, char *payload, size_t len)
{
    iotValue_t value;
    iotBinaryValue_t binValue;
    const char *name;
    if (element->desc->subs[subId].name[0] == 0) {
        name = IOT_DEFAULT_CONTROL_STR;
    } else {
        name = element->desc->subs[subId].name;
    }
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s/%s\"", payload, element->name, name);

    if (element->desc->subs[subId].type == IOT_VALUE_TYPE_BINARY) {
        binValue.data = (uint8_t*)payload;
        binValue.len = len;
        value.bin = &binValue;
    } else {
        if (iotParseString(payload, element->desc->subs[subId].type, &value)) {
            ESP_LOGE(TAG, "Failed to parse value type %d for %s/%s", element->desc->subs[subId].type, element->name, name);
            return;
        }
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
        if (element->desc->subs[i].name[0] != 0) {
            char *path = NULL;
            bool result = false;
            asprintf(&path, "%s/%s/%s", mqttPathPrefix, element->name, element->desc->subs[i].name);
            if (path != NULL) {
                result = mqttSubscribe(path);
                free(path);
            } else {
                ESP_LOGE(TAG, "Failed to allocate memory when subscribing to path %s/%s", element->name, element->desc->subs[i].name);
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
                    if (element->desc->subs[i].name[0] == 0) {
                        name = IOT_DEFAULT_CONTROL_STR;
                    } else {
                        name = element->desc->subs[i].name;
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

    if (element->desc->pubs[pubId].name[0] == 0) {
        pubName = NULL;
    } else {
        pubName = element->desc->pubs[pubId].name;
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

    if (element->desc->subs[subId].name[0] == 0) {
        subName = IOT_DEFAULT_CONTROL_STR;
    } else {
        subName = element->desc->subs[subId].name;
    }

    return checkedPathBuffer(element->name, subName, buffer, bufferLen);
}

const char *iotElementGetPubName(iotElement_t element, int pubId)
{
    if (pubId >= element->desc->nrofPubs) {
        return NULL;
    }

    return element->desc->pubs[pubId].name;
}

const char *iotElementGetSubName(iotElement_t element, int subId)
{
    const char *subName;
    if (subId >= element->desc->nrofSubs) {
        return NULL;
    }

    if (element->desc->subs[subId].name[0] == 0) {
        subName = IOT_DEFAULT_CONTROL_STR;
    } else {
        subName = element->desc->subs[subId].name;
    }
    return subName;
}

void iotElementSetHumanDescription(iotElement_t element, char *description)
{
    element->humanDescription = description;
}

char *iotElementGetHumanDescription(iotElement_t element)
{
    return element->humanDescription;
}

iotElement_t iotFindElementByHumanDescription(char *description)
{
    for (iotElement_t current = iotElementsHead; current; current = current->next) {
        if (strcmp(current->humanDescription, description) == 0) {
            return current;
        }
    }
    return NULL;
}