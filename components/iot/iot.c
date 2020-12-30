
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "tcpip_adapter.h"

#include "mqtt_client.h"

#include "iot.h"
#include "wifi.h"
#include "sdkconfig.h"
#include "deviceprofile.h"

static const char *TAG="IOT";
static const char *IOT_DEFAULT_CONTROL_STR="ctrl";

#define MQTT_TASK_STACK_SIZE (3 * 1024)

#define MQTT_PATH_PREFIX_LEN 23 // homething/<MAC 12 Hexchars> \0
#define MQTT_COMMON_CTRL_SUB_LEN (MQTT_PATH_PREFIX_LEN + 7) // "/+/ctrl"

#define MAX_TOPIC_NAME 512

#define POLL_INTERVAL_MS 10
#define UPTIME_UPDATE_MS 5000

#define MAX_LENGTH_MQTT_SERVER 256
#define MAX_LENGTH_MQTT_USERNAME 65
#define MAX_LENGTH_MQTT_PASSWORD 65

#define PUB_INDEX_TYPE 0
#define PUB_INDEX_NAME 1

#define SUB_INDEX_TYPE 0
#define SUB_INDEX_NAME 1

#define PUB_GET_TYPE(_element, _pubId) ((_element)->desc->pubs[_pubId][PUB_INDEX_TYPE])
#define PUB_GET_NAME(_element, _pubId) (&(_element)->desc->pubs[_pubId][PUB_INDEX_NAME])

#define SUB_GET_TYPE(_element, _subId) ((_element)->desc->subs[_subId].type_name[SUB_INDEX_TYPE])
#define SUB_GET_NAME(_element, _subId) (&(_element)->desc->subs[_subId].type_name[SUB_INDEX_NAME])
#define SUB_GET_CALLBACK(_element, _subId) ((_element)->desc->subs[_subId].callback)

#define _HEX_TYPE(v) 0x0 ## v
#define HEX_TYPE(type) _HEX_TYPE(type)

#define VT_BOOL   HEX_TYPE(IOT_VALUE_TYPE_BOOL)
#define VT_INT    HEX_TYPE(IOT_VALUE_TYPE_INT)
#define VT_FLOAT  HEX_TYPE(IOT_VALUE_TYPE_FLOAT)
#define VT_STRING HEX_TYPE(IOT_VALUE_TYPE_STRING)
#define VT_BINARY HEX_TYPE(IOT_VALUE_TYPE_BINARY)

#define VT_RETAINED_BOOL   HEX_TYPE(IOT_VALUE_TYPE_RETAINED_BOOL)
#define VT_RETAINED_INT    HEX_TYPE(IOT_VALUE_TYPE_RETAINED_INT)
#define VT_RETAINED_FLOAT  HEX_TYPE(IOT_VALUE_TYPE_RETAINED_FLOAT)
#define VT_RETAINED_STRING HEX_TYPE(IOT_VALUE_TYPE_RETAINED_STRING)
#define VT_RETAINED_BINARY HEX_TYPE(IOT_VALUE_TYPE_RETAINED_BINARY)

#define VT_BARE_TYPE(type) (type & 0x7f)
#define VT_IS_RETAINED(type) ((type & 0x80) == 0x80) ? 1:0
struct iotElement {
    const iotElementDescription_t *desc;
    char *name;
    void *userContext;
    struct iotElement *next;
    iotValue_t values[];
};

static iotElement_t elementsHead = NULL;

#define DEVICE_PUB_INDEX_UPTIME     0
#define DEVICE_PUB_INDEX_IP         1
#define DEVICE_PUB_INDEX_MEM_FREE   2
#define DEVICE_PUB_INDEX_MEM_LOW    3
#define DEVICE_PUB_INDEX_TASK_STATS 4

static iotElement_t deviceElement;

static esp_mqtt_client_handle_t mqttClient;
static SemaphoreHandle_t mqttMutex;

static bool mqttIsSetup = false;
static bool mqttIsConnected = false;

#define MUTEX_LOCK() do{}while(xSemaphoreTake(mqttMutex, portTICK_PERIOD_MS) != pdTRUE)
#define MUTEX_UNLOCK() xSemaphoreGive(mqttMutex)

static char mqttServer[MAX_LENGTH_MQTT_SERVER];
static int mqttPort;
static char mqttUsername[MAX_LENGTH_MQTT_USERNAME];
static char mqttPassword[MAX_LENGTH_MQTT_PASSWORD];

static char mqttPathPrefix[MQTT_PATH_PREFIX_LEN];
static char mqttCommonCtrlSub[MQTT_COMMON_CTRL_SUB_LEN];

static void mqttMessageArrived(char *mqttTopic, int mqttTopicLen, char *data, int dataLen);
static void mqttStart(void);
static bool iotElementPubSendUpdate(iotElement_t element, int pubId, iotValue_t value);
static bool iotElementSendUpdate(iotElement_t element);
static bool iotElementSubscribe(iotElement_t element);
static void iotUpdateUptime(TimerHandle_t xTimer);
static void iotUpdateMemoryStats(void);
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static uint8_t taskStatsBuffer[1024];
static iotBinaryValue_t taskStatsBinaryValue;
static void iotUpdateTaskStats(TimerHandle_t xTimer);
#endif
static void iotWifiConnectionStatus(bool connected);
static void iotDeviceControl(void *userData, iotElement_t element, iotValue_t value);

#ifdef CONFIG_CONNECTION_LED

#define LED_SUBSYS_WIFI 0
#define LED_SUBSYS_MQTT 1
#define LED_STATE_ALL_CONNECTED ((1 << LED_SUBSYS_WIFI) | (1 << LED_SUBSYS_MQTT))

#define LED_ON 0
#define LED_OFF 1

static TimerHandle_t ledTimer;
static uint8_t connState = 0;
static void setupLed();
static void setLedState(int subsystem, bool state);
#define SET_LED_STATE(subsystem, state) setLedState(subsystem, state)
#else
#define SET_LED_STATE(subsystem, state)
#endif

IOT_DESCRIBE_ELEMENT(
    deviceElementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "uptime"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, "ip"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "memFree"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "memLow")
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
        , IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BINARY, "taskStats")
#endif
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(IOT_VALUE_TYPE_STRING, IOT_SUB_DEFAULT_NAME, iotDeviceControl)
    )
);


int iotInit(void)
{
    int result = 0;
    nvs_handle handle;
    esp_err_t err;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    #ifdef CONFIG_CONNECTION_LED
    setupLed();
    #endif
    result = wifiInit(iotWifiConnectionStatus);
    if (result) 
    {
        ESP_LOGE(TAG, "Wifi init failed");
        return result;
    }

    mqttMutex = xSemaphoreCreateMutex();
    if (mqttMutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create mqttMutex!");
    }

    sprintf(mqttPathPrefix, "homething/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(mqttCommonCtrlSub, "%s/+/%s", mqttPathPrefix, IOT_DEFAULT_CONTROL_STR);

    ESP_LOGI(TAG, "Initialised IOT - device path: %s", mqttPathPrefix);
    deviceElement = iotNewElement(&deviceElementDescription, NULL, "device");
    iotUpdateMemoryStats();

    err = nvs_open("mqtt", NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        size_t len = sizeof(mqttServer);
        if (nvs_get_str(handle, "host", mqttServer, &len) == ESP_OK)
        {
            uint16_t p;
            err = nvs_get_u16(handle, "port", &p);
            if (err == ESP_OK)
            {   
                mqttPort = (int) p;
            }
            else
            {
                mqttPort = 1883;
            }
            len = sizeof(mqttUsername);
            if (nvs_get_str(handle, "user", mqttUsername, &len) != ESP_OK)
            {
                mqttUsername[0] = 0;
            }
            len = sizeof(mqttPassword);
            if (nvs_get_str(handle, "pass", mqttPassword, &len) != ESP_OK)
            {
                mqttPassword[0] = 0;
            }
        }
        else
        {
            mqttServer[0] = 0;
        }
        
        nvs_close(handle);
    }

    return result;
}

void iotStart()
{
    mqttStart();
    wifiStart();

    xTimerStart(xTimerCreate("updUptime", UPTIME_UPDATE_MS / portTICK_RATE_MS, pdTRUE, NULL, iotUpdateUptime), 0);

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    xTimerStart(xTimerCreate("stats", 30*1000 / portTICK_RATE_MS, pdTRUE, NULL, iotUpdateTaskStats), 0);
#endif
}

iotElement_t iotNewElement(const iotElementDescription_t *desc, void *userContext, const char const *nameFormat, ...)
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
    newElement->userContext = userContext;
    newElement->next = elementsHead;
    elementsHead = newElement;
    return newElement;
}


void iotElementPublish(iotElement_t element, int pubId, iotValue_t value)
{
    bool updateRequired = false;

    if (pubId >= element->desc->nrofPubs){
        ESP_LOGE(TAG, "Invalid publish id %d for element %s", pubId, element->name);
        return;
    }

    switch(VT_BARE_TYPE(PUB_GET_TYPE(element, pubId)))
    {
        case VT_BOOL:
            updateRequired = value.b != element->values[pubId].b;
            break;
        case VT_INT:
            updateRequired = value.i != element->values[pubId].i;
            break;
        case VT_FLOAT:
            updateRequired = value.f != element->values[pubId].f;
            break;
        case VT_STRING:
        case VT_BINARY:
            updateRequired = true;
            break;
        default:
            ESP_LOGE(TAG, "Unknown pub type %d for %s!", PUB_GET_TYPE(element, pubId), PUB_GET_NAME(element, pubId));
            return;
    }
    if (updateRequired)
    {
        element->values[pubId] = value;

        MUTEX_LOCK();
        if (mqttIsConnected)
        {
            iotElementPubSendUpdate(element, pubId, value);
        }
        MUTEX_UNLOCK();
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
 
    if (element->desc->pubs[pubId][PUB_INDEX_NAME] == 0)
    {
        asprintf(&path, "%s/%s", prefix, element->name);
    }
    else
    {
        asprintf(&path, "%s/%s/%s", prefix, element->name, PUB_GET_NAME(element, pubId));
    }

    if (path == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate path when publishing message");
        return false;
    }
    char valueType = PUB_GET_TYPE(element, pubId);
    int retain = VT_IS_RETAINED(valueType);

    switch(VT_BARE_TYPE(valueType))
    {
        case VT_BOOL:
            message = (value.b) ? "on":"off";
            break;

        case VT_INT:
            sprintf(payload, "%d", value.i);
            break;

        case VT_FLOAT:
            sprintf(payload, "%f", value.f);
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

    rc = esp_mqtt_client_publish(mqttClient, path, message, messageLen, 0, retain);
    free(path);
    if (rc != 0) 
    {
        ESP_LOGW(TAG, "PUB: Failed to send message to %s rc %d", path, rc);
        return false;
    } 
    else 
    {
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
    for (i = 0; i < element->desc->nrofPubs && result; i++)
    {
        MUTEX_LOCK();
        result = iotElementPubSendUpdate(element, i, element->values[i]);
        MUTEX_UNLOCK();
    }
    
    return result;
}

int iotStrToBool(const char *str, bool *out)
{
    if ((strcasecmp(str, "on") == 0) || (strcasecmp(str, "true") == 0))
    {
        *out = true;
    }
    else if ((strcasecmp(str, "off") == 0) || (strcasecmp(str, "false") == 0))
    {
        *out = false;
    }
    else
    {
        return 1;
    }
    return 0;
}

static void iotElementSubUpdate(iotElement_t element, int subId, char *payload, size_t len)
{
    iotValue_t value;
    iotBinaryValue_t binValue;
    const char *name;
    if (element->desc->subs[subId].type_name[SUB_INDEX_NAME] == 0)
    {
        name = IOT_DEFAULT_CONTROL_STR;
    }
    else
    {
        name = SUB_GET_NAME(element, subId);
    }
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s/%s\"", payload, element->name, name);
    switch(VT_BARE_TYPE(SUB_GET_TYPE(element, subId)))
    {
        case VT_BOOL:
        if (iotStrToBool(payload, &value.b))
        {
            ESP_LOGW(TAG, "Invalid value for bool type (%s)", payload);
            return;
        }
        break;
        
        case VT_INT:
        if (sscanf(payload, "%d", &value.i) == 0)
        {
            ESP_LOGW(TAG, "Invalid value for int type (%s)", payload);
            return;
        }
        break;
        
        case VT_FLOAT:
        if (sscanf(payload, "%f", &value.f) == 0)
        {
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
    SUB_GET_CALLBACK(element, subId)(element->userContext, element, value);
}

static bool iotSubscribe(char *topic)
{
    int rc;
    if ((rc = esp_mqtt_client_subscribe(mqttClient, topic, 2)) == -1) 
    {
        ESP_LOGE(TAG, "SUB: Return code from MQTT subscribe is %d for \"%s\"", rc, topic);
        return false;
    } 
    else 
    {
        ESP_LOGI(TAG, "SUB: MQTT subscribe to topic \"%s\"", topic);
    }
    return true;
}

static bool iotElementSubscribe(iotElement_t element)
{
    int i;
    for (i = 0; i < element->desc->nrofSubs; i++)
    {
        if (element->desc->subs[i].type_name[SUB_INDEX_NAME] != 0)
        {
            char *path = NULL;
            bool result = false;
            asprintf(&path, "%s/%s/%s", mqttPathPrefix, element->name, SUB_GET_NAME(element, i));
            if (path != NULL)
            {
                result = iotSubscribe(path);
                free(path);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to allocate memory when subscribing to path %s/%s", element->name, SUB_GET_NAME(element, i));
            }
            
            if (!result)
            {
                return false;
            }
        }
    }
    return true;
}

static void iotUpdateMemoryStats(void) 
{
    iotValue_t value;
    value.i = (int) esp_get_free_heap_size();
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_MEM_FREE, value);
    value.i = (int)esp_get_minimum_free_heap_size();
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_MEM_LOW, value);
}

static void iotUpdateUptime(TimerHandle_t xTimer)
{
    iotValue_t value;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    value.i = tv.tv_sec;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_UPTIME, value);
   
    iotUpdateMemoryStats();
}

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static void iotUpdateTaskStats(TimerHandle_t xTimer)
{
    iotValue_t value;
    CborEncoder encoder, taskArrayEncoder, taskEntryEncoder;
    CborError cborErr;
    int i;
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = malloc(sizeof(TaskStatus_t) * nrofTasks);
    if (tasksStatus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        return;
    }
    nrofTasks = uxTaskGetSystemState( tasksStatus, nrofTasks, NULL);
    taskStatsBinaryValue.data = taskStatsBuffer;
    value.bin = &taskStatsBinaryValue;
    cbor_encoder_init(&encoder, taskStatsBuffer, sizeof(taskStatsBuffer), 0);

    cborErr = cbor_encoder_create_array(&encoder, &taskArrayEncoder, nrofTasks);
    if (cborErr != CborNoError) {
        ESP_LOGE(TAG, "Failed to create task stats array");
        goto error;
    }

    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
    printf("Name                : Stack Left\n"
           "--------------------------------\n");

    for (i=0; i < nrofTasks; i++) {
        cborErr = cbor_encoder_create_array(&taskArrayEncoder, &taskEntryEncoder, 2);
        if (cborErr != CborNoError) {
            ESP_LOGE(TAG, "Failed to create task entry array");
            goto error;
        }
        cbor_encode_text_stringz(&taskEntryEncoder, tasksStatus[i].pcTaskName);
        cbor_encode_uint(&taskEntryEncoder, tasksStatus[i].usStackHighWaterMark);
        cbor_encoder_close_container(&taskArrayEncoder, &taskEntryEncoder);
        printf("%-20s: % 10d\n", tasksStatus[i].pcTaskName, tasksStatus[i].usStackHighWaterMark);
    }
    cbor_encoder_close_container(&encoder, &taskArrayEncoder);
    taskStatsBinaryValue.len = cbor_encoder_get_buffer_size(&encoder, taskStatsBuffer);
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_TASK_STATS, value);
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');

    free(tasksStatus);
    return;

error:
    taskStatsBinaryValue.len = 0;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_TASK_STATS, value);
    free(tasksStatus);
}
#endif


static int _safestrcmp(const char *constant, int conLen, const char *variable, int len) {
    if (conLen > len) {
        return -1;
    }
    return strncmp(constant, variable, len);
}
#define safestrcmp(constant, variable, len) _safestrcmp(constant, sizeof(constant), variable, len)
#define SETPROFILE "setprofile"
static void iotDeviceControl(void *userData, iotElement_t element, iotValue_t value)
{
    if (safestrcmp("restart", (const char *) value.bin->data, (int)value.bin->len) == 0) {
        esp_restart();
    }
    else if (safestrcmp(SETPROFILE, (const char *) value.bin->data, (int)value.bin->len) == 0) {
        uint8_t *profile = value.bin->data + sizeof(SETPROFILE);
        size_t profileLen = value.bin->len - sizeof(SETPROFILE);
        if (deviceProfileSetProfile(profile, profileLen) == 0) {
            esp_restart();
        }
    }
}

static void iotWifiConnectionStatus(bool connected)
{
    iotValue_t value;
    SET_LED_STATE(LED_SUBSYS_WIFI, connected);
    if (connected){
        value.s = wifiGetIPAddrStr();
        iotElementPublish(deviceElement, DEVICE_PUB_INDEX_IP, value);
    }
    if (mqttIsSetup)
    {
        if (connected)
        {
            esp_mqtt_client_start(mqttClient);
        }
        else
        {
            esp_mqtt_client_stop(mqttClient);
        }
    }
}

static void mqttMessageArrived(char *mqttTopic, int mqttTopicLen, char *data, int dataLen)
{
    char *topic, *topicStart;
    char *payload;
    bool found = false;
    size_t len = strlen(mqttPathPrefix);
    
    topicStart = topic = malloc(mqttTopicLen + 1);
    if (topic == NULL)
    {
        ESP_LOGE(TAG, "Not enough memory to copy topic!");
        return;
    }
    memcpy(topic, mqttTopic, mqttTopicLen);    
    topic[mqttTopicLen] = 0;

    ESP_LOGI(TAG, "Message arrived, topic %s payload %d", topic, dataLen);
    payload = malloc(dataLen + 1);
    if (payload == NULL)
    {
        free(topicStart);
        ESP_LOGE(TAG, "Not enough memory to copy payload!");
        return;
    }
    memcpy(payload, data, dataLen);
    payload[dataLen] = 0;
    if ((strncmp(topic, mqttPathPrefix, len) == 0) && (topic[len] == '/'))
    {
        topic += len + 1;
        for (iotElement_t element = elementsHead; element != NULL; element = element->next)
        {
            len = strlen(element->name);
            if ((strncmp(topic, element->name, len) == 0) && (topic[len] == '/'))
            {
                topic += len + 1;
                int i;
                for (i = 0; i < element->desc->nrofSubs; i++)
                {
                    const char *name;
                    if (element->desc->subs[i].type_name[SUB_INDEX_NAME] == 0)
                    {
                        name = IOT_DEFAULT_CONTROL_STR;
                    }
                    else
                    {
                        name = SUB_GET_NAME(element, i);
                    }
                    if (strcmp(topic, name) == 0)
                    {
                        iotElementSubUpdate(element, i, payload, dataLen);
                        found = true;
                        break;
                    }
                }
                
            }
        }
    }

    if (!found)
    {
        ESP_LOGW(TAG, "Unexpected message, topic %s", topic);
    }
    
    free(payload);
    free(topicStart);
}

static void mqttConnected(void) 
{
    iotSubscribe(mqttCommonCtrlSub);

    for (iotElement_t element = elementsHead; (element != NULL); element = element->next)
    {
        if (iotElementSubscribe(element))
        {
            iotElementSendUpdate(element);
        }
    }
}

static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            SET_LED_STATE(LED_SUBSYS_MQTT, true);
            mqttConnected();
            MUTEX_LOCK();
            mqttIsConnected = true;
            MUTEX_UNLOCK();
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            SET_LED_STATE(LED_SUBSYS_MQTT, false);
            MUTEX_LOCK();
            mqttIsConnected = false;
            MUTEX_UNLOCK();
            break;

        case MQTT_EVENT_SUBSCRIBED:
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            break;

        case MQTT_EVENT_PUBLISHED:
            break;

        case MQTT_EVENT_DATA:
            mqttMessageArrived(event->topic, event->topic_len, event->data, event->data_len);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

static void mqttStart(void)
{
    if (mqttServer[0] == 0) {
        return;
    } 

    esp_mqtt_client_config_t mqtt_cfg = {
        .transport = MQTT_TRANSPORT_OVER_TCP,
        .host = mqttServer,
        .port = mqttPort,
        .event_handle = mqttEventHandler,
        .task_stack = MQTT_TASK_STACK_SIZE,
    };
    if (mqttUsername[0] != 0) {
        mqtt_cfg.username = mqttUsername;
        mqtt_cfg.password = mqttPassword;
    }
    
    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    mqttIsSetup = true;
}


#ifdef CONFIG_CONNECTION_LED
static void onLedTimer(TimerHandle_t xTimer)
{
    gpio_set_level(CONFIG_CONNECTION_LED_PIN, !gpio_get_level(CONFIG_CONNECTION_LED_PIN));
}

static void setupLed()
{
    gpio_config_t config;

    config.pin_bit_mask = 1 << CONFIG_CONNECTION_LED_PIN;
    config.mode = GPIO_MODE_DEF_OUTPUT;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    gpio_set_level(CONFIG_CONNECTION_LED_PIN, LED_OFF);
    ledTimer = xTimerCreate("CONNLED", 500 / portTICK_RATE_MS, pdTRUE, NULL, onLedTimer);
    xTimerStart(ledTimer, 0);
}

static void setLedState(int subsystem, bool state)
{
    uint8_t oldState = connState;
    if (state)
    {
        connState |= 1<< subsystem;
    }
    else
    {
        connState &= ~(1<< subsystem);
    }
    if (oldState != connState)
    {
        if (connState == LED_STATE_ALL_CONNECTED)
        {
            if (xTimerIsTimerActive(ledTimer) == pdTRUE)
            {
                xTimerStop(ledTimer, 0);
            }
            gpio_set_level(CONFIG_CONNECTION_LED_PIN, LED_ON);
        }
        else
        {
            if (xTimerIsTimerActive(ledTimer) == pdFALSE)
            {
                xTimerStart(ledTimer, 0);
            }
        }
    }
}
#endif