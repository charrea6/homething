
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

#include "MQTTClient.h"

#include "iot.h"
#include "sdkconfig.h"

static const char *TAG="IOT";
static const char *IOT_DEFAULT_CONTROL_STR="ctrl";

#define MQTT_PATH_PREFIX_LEN 23 // homething/<MAC 12 Hexchars> \0
#define MQTT_COMMON_CTRL_SUB_LEN (MQTT_PATH_PREFIX_LEN + 7) // "/+/ctrl"

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  8192
#define MQTT_CLIENT_THREAD_PRIO         8

#define MAX_TOPIC_NAME 512

#define POLL_INTERVAL_MS 10
#define UPTIME_UPDATE_MS 5000

#define MAX_LENGTH_WIFI_NAME 32
#define MAX_LENGTH_WIFI_PASSWORD 64
#define MAX_LENGTH_MQTT_SERVER 256
#define MAX_LENGTH_MQTT_USERNAME 65
#define MAX_LENGTH_MQTT_PASSWORD 65

#define NVS_READ_STR(key, out) nvsReadStr(handle, key, out, sizeof(out))

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
/* Event Group bits */
const int CONNECTED_BIT = BIT0;
const int MQTT_UPDATE_BIT = BIT1;

static iotElement_t *elements = NULL;

static iotElement_t deviceElement;
static iotElementPub_t deviceIPPub;
static iotElementPub_t deviceUptimePub;

static char wifiSsid[MAX_LENGTH_WIFI_NAME];
static char wifiPassword[MAX_LENGTH_WIFI_PASSWORD];

static char mqttServer[MAX_LENGTH_MQTT_SERVER];
static int mqttPort;
static char mqttUsername[MAX_LENGTH_MQTT_USERNAME];
static char mqttPassword[MAX_LENGTH_MQTT_PASSWORD];

static char mqttPathPrefix[MQTT_PATH_PREFIX_LEN];
static char mqttCommonCtrlSub[MQTT_COMMON_CTRL_SUB_LEN];
static char ipAddr[16]; // ddd.ddd.ddd.ddd\0

static void mqttClientThread(void* pvParameters);
static void mqttMessageArrived(MessageData *data);
static void wifiInitialise(void);

#ifdef CONFIG_CONNECTION_LED
#define LED_ON 0
#define LED_OFF 1

#define LED_STATE_DISCONNECTED 0
#define LED_STATE_WIFI_CONNECTED 1
#define LED_STATE_MQTT_CONNECTED 2

static TimerHandle_t ledTimer;

static void setupLed();
static void setLedState(int state);
static int nvsReadStr(nvs_handle handle, const char *key, char *out, size_t len);

#define IF_LED(func) func
#else
#define IF_LED(func)
#endif

int iotInit(void)
{
    int result = 0;
    nvs_handle handle;
    esp_err_t err;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    sprintf(mqttPathPrefix, "homething/%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(mqttCommonCtrlSub, "%s/+/%s", mqttPathPrefix, IOT_DEFAULT_CONTROL_STR);

    ESP_LOGI(TAG, "Initialised IOT - device path: %s", mqttPathPrefix);
    
    deviceElement.name = "device";
    iotElementAdd(&deviceElement);

    deviceIPPub.name = "ip";
    deviceIPPub.type = iotValueType_String;
    deviceIPPub.retain = true;
    deviceIPPub.value.s = "?";
    iotElementPubAdd(&deviceElement, &deviceIPPub);
    
    deviceUptimePub.name = "uptime";
    deviceUptimePub.type = iotValueType_Int;
    deviceUptimePub.retain = true;
    deviceUptimePub.value.i = 0;
    iotElementPubAdd(&deviceElement, &deviceUptimePub);

    IF_LED(setupLed());

    err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        result = NVS_READ_STR("ssid", wifiSsid);
        if (result == 0)
        {
            result = NVS_READ_STR("pass", wifiPassword);
        }
        nvs_close(handle);
    }
    else
    {
        result = 1;
    }
    
    if (result == 0)
    {
        err = nvs_open("mqtt", NVS_READONLY, &handle);
        if (err == ESP_OK)
        {
            result = NVS_READ_STR("host", mqttServer);
            if (result == 0)
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

            }
            if (result == 0)
            {
                if (NVS_READ_STR("user", mqttUsername))
                {
                    mqttUsername[0] = 0;
                }
                if (NVS_READ_STR("pass", mqttPassword))
                {
                    mqttPassword[0] = 0;
                }
            }
            nvs_close(handle);
        }
    }

    return result;
}

void iotStart()
{
    wifiInitialise();
    xTaskCreate(mqttClientThread,
                MQTT_CLIENT_THREAD_NAME,
                MQTT_CLIENT_THREAD_STACK_WORDS,
                NULL,
                MQTT_CLIENT_THREAD_PRIO,
                NULL);
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
        sub->name = "ctrl";
    }
    name = sub->name;
    
    len = strlen(mqttPathPrefix) + 1 + strlen(element->name) + 1 + strlen(name) + 1;
    sub->path = (char *)malloc(len);
    if (sub->path == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate path for Element %s Sub %s", element->name, name);
        return;
    }
    sprintf(sub->path, "%s/%s/%s", mqttPathPrefix, element->name, name);
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
    }
    else
    {
        ESP_LOGI(TAG, "PUB: No update required for %s%s%s",pub->element->name, pub->name[0]?"/":"", pub->name);
    }    
}

static bool iotElementPubSendUpdate(iotElement_t *element, iotElementPub_t *pub, MQTTClient *client)
{
    char path[MAX_TOPIC_NAME + 1];
    MQTTMessage message;
    char payload[30] = "";
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

    message.qos = QOS0;
    message.retained = pub->retain?1:0;
    
    switch(pub->type)
    {
        case iotValueType_Bool:
        message.payload = (pub->value.b) ? "on":"off";
        break;
        case iotValueType_Int:
        message.payload = payload;
        sprintf(payload, "%d", pub->value.i);
        break;
        case iotValueType_Float:
        message.payload = payload;
        sprintf(payload, "%f", pub->value.f);
        break;
        case iotValueType_String:
        message.payload = (char*)pub->value.s;
        break;
    }
    message.payloadlen = strlen((char*)message.payload);

    if ((rc = MQTTPublish(client, path, &message)) != 0) 
    {
        ESP_LOGW(TAG, "PUB: Failed to send message to %s rc %d", path, rc);
        return false;
    } 
    else 
    {
        ESP_LOGV(TAG, "PUB: Sent %s (%d) to %s", (char *)message.payload, message.payloadlen, path);
    }
    pub->updateRequired = false;
    return true;
}

static bool iotElementSendUpdate(iotElement_t *element, bool force, MQTTClient *client)
{
    for (iotElementPub_t *pub = element->pubs;pub != NULL; pub = pub->next)
    {
        if (pub->updateRequired || force)
        {
            if (!iotElementPubSendUpdate(element, pub, client))
            {
                return false;
            }
        }

    }
    return true;
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
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s\"", payload, sub->name);
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

static bool iotSubscribe(MQTTClient *client, char *topic)
{
    int rc;
    if ((rc = MQTTSubscribe(client, topic, 2, mqttMessageArrived)) != 0) 
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

static bool iotElementSubscribe(iotElement_t *element, MQTTClient *client)
{
    for (iotElementSub_t *sub = element->subs; sub != NULL; sub = sub->next)
    {
        if (sub->name != IOT_DEFAULT_CONTROL)
        {
            if (!iotSubscribe(client, sub->path))
            {
                return false;
            }
        }
    }
    return true;
}

static esp_err_t wifiEventHandler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        sprintf(ipAddr, IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifiInitialise(void)
{
    uint8_t mac[6];
    char hostname[23];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(hostname, "homething-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    tcpip_adapter_init();
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname);

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifiEventHandler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char *)wifi_config.sta.ssid, wifiSsid);
    strcpy((char *)wifi_config.sta.password, wifiPassword);
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void mqttMessageArrived(MessageData* data)
{
    char *topic, *topicStart;
    char *payload;
    bool found = false;
    size_t len = strlen(mqttPathPrefix);
    
    topicStart = topic = malloc(data->topicName->lenstring.len + 1);
    if (topic == NULL)
    {
        ESP_LOGE(TAG, "Not enough memory to copy topic!");
        return;
    }
    memcpy(topic, data->topicName->lenstring.data, data->topicName->lenstring.len);    
    topic[data->topicName->lenstring.len] = 0;

    ESP_LOGI(TAG, "Message arrived, topic %s payload %d", topic, data->message->payloadlen);
    payload = malloc(data->message->payloadlen + 1);
    if (payload == NULL)
    {
        ESP_LOGE(TAG, "Not enough memory to copy payload!");
        return;
    }
    memcpy(payload, data->message->payload, data->message->payloadlen);
    payload[data->message->payloadlen] = 0;
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
                    if (strcmp(topic, sub->name) == 0)
                    {
                        iotElementSubUpdate(sub, payload, data->message->payloadlen);
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

static void mqttClientThread(void* pvParameters)
{
    MQTTClient client;
    Network network;
    unsigned char sendbuf[80], readbuf[80] = {0};
    int rc = 0;
    bool loop = true;
    unsigned int loopCount=0;
    iotValue_t value;

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    ESP_LOGI(TAG, "mqtt client thread starts");

    NetworkInit(&network);
    MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

    while (true)
    {
        ESP_LOGI(TAG, "Waiting for network connection");
        IF_LED(setLedState(LED_STATE_DISCONNECTED));
        loop = true;
        while(loop)
        {
            /* Wait for the callback to set the CONNECTED_BIT in the
            event group.
            */
            xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                false, true, portMAX_DELAY);
            ESP_LOGI(TAG, "Connected to AP");
            IF_LED(setLedState(LED_STATE_WIFI_CONNECTED));

            value.s = ipAddr;
            iotElementPubUpdate(&deviceIPPub, value);

            if ((rc = NetworkConnect(&network, mqttServer, mqttPort)) != 0) 
            {
                ESP_LOGE(TAG, "Return code from network connect is %d, pausing for 5 seconds before reconnecting", rc);
                vTaskDelay(5000 / portTICK_RATE_MS);  //wait for 5 seconds
            }
            else 
            {
                loop = false;
            }
        }
        connectData.MQTTVersion = 3;
        connectData.clientID.cstring = mqttPathPrefix; // We use our unique MQTT path prefix as our client id here because it's unique and identifies this device nicely.
        if (mqttUsername[0])
        {
            connectData.username.cstring = mqttUsername;
            connectData.password.cstring = mqttPassword;
        }
        if ((rc = MQTTConnect(&client, &connectData)) != 0) 
        {
            ESP_LOGE(TAG, "Return code from MQTT connect is %d", rc);
            vTaskDelay(5000 / portTICK_RATE_MS);  //wait for 5 seconds
        } 
        else 
        {
            ESP_LOGI(TAG, "MQTT Connected");
            IF_LED(setLedState(LED_STATE_MQTT_CONNECTED));
            loop = true;
            iotSubscribe(&client, mqttCommonCtrlSub);

            for (iotElement_t *element = elements; (element != NULL) && loop; element = element->next)
            {
                loop = iotElementSubscribe(element, &client);
                if (loop)
                {
                    loop = iotElementSendUpdate(element, true, &client);
                }
            }
            
            for (loopCount = 0; loop; loopCount++)
            {
                for (iotElement_t *element = elements; (element != NULL) && loop; element = element->next)
                {
                    loop = iotElementSendUpdate(element, false, &client);
                }
#if defined(MQTT_TASK)
                MutexLock(&client.mutex);
#endif
                if (MQTTYield(&client, POLL_INTERVAL_MS) < 0)
                {
                    loop = false;
                }
#if defined(MQTT_TASK)
                MutexUnlock(&client.mutex);
#endif
                if (loopCount == (UPTIME_UPDATE_MS / POLL_INTERVAL_MS))
                {
                    struct timeval tv;
                    gettimeofday(&tv, NULL);
                    value.i = tv.tv_sec;
                    iotElementPubUpdate(&deviceUptimePub, value);
                    loopCount = 0;
                }
            }
        }       
        ESP_LOGW(TAG, "Lost connection to MQTT Server");
        network.disconnect(&network);
    }
}

static int nvsReadStr(nvs_handle handle, const char *key, char *out, size_t len)
{
    esp_err_t err = nvs_get_str(handle, key, out, &len);
    if (err != ESP_OK)
    {
        return 1;
    }
    return 0;
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
}

static void setLedState(int state)
{
    switch(state)
    {
        case LED_STATE_DISCONNECTED:
        case LED_STATE_WIFI_CONNECTED:
            if (xTimerIsTimerActive(ledTimer) == pdFALSE)
            {
                xTimerStart(ledTimer, 0);
            }
            break;
        case LED_STATE_MQTT_CONNECTED:
        default:
            if (xTimerIsTimerActive(ledTimer) == pdTRUE)
            {
                xTimerStop(ledTimer, 0);
            }
            gpio_set_level(CONFIG_CONNECTION_LED_PIN, LED_ON);
            break;
    }
}


#endif