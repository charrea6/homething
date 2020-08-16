
#include <stdlib.h>
#include <string.h>

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

static const char *TAG="IOT";
static const char *IOT_DEFAULT_CONTROL_STR="ctrl";

#define MQTT_PATH_PREFIX_LEN 23 // homething/<MAC 12 Hexchars> \0
#define MQTT_COMMON_CTRL_SUB_LEN (MQTT_PATH_PREFIX_LEN + 7) // "/+/ctrl"

#define MAX_TOPIC_NAME 512

#define POLL_INTERVAL_MS 10
#define UPTIME_UPDATE_MS 5000

#define MAX_LENGTH_MQTT_SERVER 256
#define MAX_LENGTH_MQTT_USERNAME 65
#define MAX_LENGTH_MQTT_PASSWORD 65


static iotElement_t *elements = NULL;

static iotElement_t deviceElement;
static iotElementPub_t deviceUptimePub;
static iotElementPub_t deviceIPPub;

static TimerHandle_t uptimeTimer;
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
static bool iotElementPubSendUpdate(iotElement_t *element, iotElementPub_t *pub);
static void iotUpdateUptime(TimerHandle_t xTimer);
static void iotWifiConnectionStatus(bool connected);

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
#define SET_LED_STATE(state)
#endif


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
    
    deviceElement.name = "device";
    iotElementAdd(&deviceElement);

    deviceUptimePub.name = "uptime";
    deviceUptimePub.type = iotValueType_Int;
    deviceUptimePub.retain = true;
    deviceUptimePub.value.i = 0;
    iotElementPubAdd(&deviceElement, &deviceUptimePub);

    deviceIPPub.name = "ip";
    deviceIPPub.type = iotValueType_String;
    deviceIPPub.retain = true;
    deviceIPPub.value.s = wifiGetIPAddrStr();
    iotElementPubAdd(&deviceElement, &deviceIPPub);

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
    uptimeTimer = xTimerCreate("updUptime", UPTIME_UPDATE_MS / portTICK_RATE_MS, pdTRUE, NULL, iotUpdateUptime);
    xTimerStart(uptimeTimer, 0);
}

void iotElementAdd(iotElement_t *element)
{
    element->next = elements;
    elements = element;
    element->pubs = NULL;
    elements->subs = NULL;
    ESP_LOGI(TAG, "Added element \"%s\"", element->name);
}

void iotElementSubAdd(iotElement_t *element, iotElementSub_t *sub)
{
    size_t len;
    const char *name;
    if (sub->name == IOT_DEFAULT_CONTROL)
    {
        name = IOT_DEFAULT_CONTROL_STR;
    }
    else
    {
        name = sub->name;
        
        len = strlen(mqttPathPrefix) + 1 + strlen(element->name) + 1 + strlen(name) + 1;
        sub->path = (char *)malloc(len);
        if (sub->path == NULL)
        {
            ESP_LOGE(TAG, "Failed to allocate path for Element %s Sub %s", element->name, name);
            return;
        }
        sprintf(sub->path, "%s/%s/%s", mqttPathPrefix, element->name, name);
    }
    sub->element = element;
    sub->next = element->subs;
    element->subs = sub;
    ESP_LOGI(TAG, "Added sub \"%s\" to element \"%s\"", name, element->name);
}

void iotElementPubAdd(iotElement_t *element, iotElementPub_t *pub)
{
    pub->next = element->pubs;
    element->pubs = pub;
    pub->element = element;
    pub->updateRequired = false;
    ESP_LOGI(TAG, "Added pub \"%s\" to element \"%s\"", pub->name, element->name);
}

void iotElementPubUpdate(iotElementPub_t *pub, iotValue_t value)
{
    switch(pub->type)
    {
        case iotValueType_Bool:
        pub->updateRequired = value.b != pub->value.b;
        break;
        case iotValueType_Int:
        pub->updateRequired = value.i != pub->value.i;
        break;
        case iotValueType_Float:
        pub->updateRequired = value.f != pub->value.f;
        break;
        case iotValueType_String:
        pub->updateRequired = true;
        break;
    }
    pub->value = value;
    if (pub->updateRequired)
    {
        if (pub != &deviceUptimePub) {
            ESP_LOGI(TAG, "PUB: Flagging update required for %s%s%s", pub->element->name, pub->name[0]?"/":"", pub->name);
        }
        MUTEX_LOCK();
        if (mqttIsConnected)
        {
            iotElementPubSendUpdate(pub->element, pub);
        }
        MUTEX_UNLOCK();
    }
    else
    {
        ESP_LOGI(TAG, "PUB: No update required for %s%s%s",pub->element->name, pub->name[0]?"/":"", pub->name);
    }    
}

static bool iotElementPubSendUpdate(iotElement_t *element, iotElementPub_t *pub)
{
    char path[MAX_TOPIC_NAME + 1];
    char payload[30] = "";
    char *message = payload;
    int messageLen = 0;
    const char *prefix = mqttPathPrefix;
    int rc;
 
    if (pub->name[0] == 0)
    {
        sprintf(path, "%s/%s", prefix, element->name);
    }
    else
    {
        sprintf(path, "%s/%s/%s", prefix, element->name, pub->name);
    }

    switch(pub->type)
    {
        case iotValueType_Bool:
        message = (pub->value.b) ? "on":"off";
        break;
        case iotValueType_Int:
        sprintf(payload, "%d", pub->value.i);
        break;
        case iotValueType_Float:
        sprintf(payload, "%f", pub->value.f);
        break;
        case iotValueType_String:
        message = (char*)pub->value.s;
        break;
    }
    messageLen = strlen((char*)message);

    rc = esp_mqtt_client_publish(mqttClient, path, message, messageLen, 0, pub->retain ? 1:0);
    if (rc != 0) 
    {
        ESP_LOGW(TAG, "PUB: Failed to send message to %s rc %d", path, rc);
        return false;
    } 
    else 
    {
        ESP_LOGV(TAG, "PUB: Sent %s (%d) to %s", (char *)message, messageLen, path);
    }
    pub->updateRequired = false;
    return true;
}

static bool iotElementSendUpdate(iotElement_t *element, bool force)
{
    bool result = true;
    MUTEX_LOCK();
    if (mqttIsConnected || force)
    {
        for (iotElementPub_t *pub = element->pubs;pub != NULL; pub = pub->next)
        {
            if (pub->updateRequired || force)
            {
                if (!iotElementPubSendUpdate(element, pub))
                {
                    result = false;
                    break;
                }
            }

        }
    }
    MUTEX_UNLOCK();
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

static void iotElementSubUpdate(iotElementSub_t *sub, char *payload, size_t len)
{
    iotValue_t value;
    const char *name;
    if (sub->name == IOT_DEFAULT_CONTROL)
    {
        name = IOT_DEFAULT_CONTROL_STR;
    }
    else
    {
        name = sub->name;
    }
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s/%s\"", payload, sub->element->name, name);
    switch(sub->type)
    {
        case iotValueType_Bool:
        if (iotStrToBool(payload, &value.b))
        {
            ESP_LOGW(TAG, "Invalid value for bool type (%s)", payload);
            return;
        }
        break;
        
        case iotValueType_Int:
        if (sscanf(payload, "%d", &value.i) == 0)
        {
            ESP_LOGW(TAG, "Invalid value for int type (%s)", payload);
            return;
        }
        break;
        
        case iotValueType_Float:
        if (sscanf(payload, "%f", &value.f) == 0)
        {
            ESP_LOGW(TAG, "Invalid value for float type (%s)", payload);
            return;
        }
        break;

        case iotValueType_String:
        value.s = payload;
        break;
        
        default:
        ESP_LOGE(TAG, "Unknown value type! %d", sub->type);
        return;
    }
    sub->callback(sub->userData, sub, value);
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

static bool iotElementSubscribe(iotElement_t *element)
{
    for (iotElementSub_t *sub = element->subs; sub != NULL; sub = sub->next)
    {
        if (sub->name != IOT_DEFAULT_CONTROL)
        {
            if (!iotSubscribe(sub->path))
            {
                return false;
            }
        }
    }
    return true;
}

static void iotUpdateUptime(TimerHandle_t xTimer)
{
    iotValue_t value;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    value.i = tv.tv_sec;
    iotElementPubUpdate(&deviceUptimePub, value);
}

static void iotWifiConnectionStatus(bool connected)
{
    iotValue_t value;
    SET_LED_STATE(LED_SUBSYS_WIFI, connected);
    value.s = wifiGetIPAddrStr();
    iotElementPubUpdate(&deviceIPPub, value);
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
        ESP_LOGE(TAG, "Not enough memory to copy payload!");
        return;
    }
    memcpy(payload, data, dataLen);
    payload[dataLen] = 0;
    if ((strncmp(topic, mqttPathPrefix, len) == 0) && (topic[len] == '/'))
    {
        topic += len + 1;
        for (iotElement_t *element = elements; element != NULL; element = element->next)
        {
            len = strlen(element->name);
            if ((strncmp(topic, element->name, len) == 0) && (topic[len] == '/'))
            {
                topic += len + 1;
                for (iotElementSub_t *sub = element->subs; sub != NULL; sub = sub->next)
                {
                    const char *name;
                    if (sub->name == IOT_DEFAULT_CONTROL)
                    {
                        name = IOT_DEFAULT_CONTROL_STR;
                    }
                    else
                    {
                        name = sub->name;
                    }
                    if (strcmp(topic, name) == 0)
                    {
                        iotElementSubUpdate(sub, payload, dataLen);
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

    for (iotElement_t *element = elements; (element != NULL); element = element->next)
    {
        if (iotElementSubscribe(element))
        {
            iotElementSendUpdate(element, true);
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
        .task_stack = 7 * 1024,
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