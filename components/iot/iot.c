
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "MQTTClient.h"

#include "iot.h"
#include "sdkconfig.h"

static const char *TAG="IOT";

#define MQTT_CLIENT_THREAD_NAME         "mqtt_client_thread"
#define MQTT_CLIENT_THREAD_STACK_WORDS  8192
#define MQTT_CLIENT_THREAD_PRIO         8

#define MAX_TOPIC_NAME 512

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int MQTT_UPDATE_BIT = BIT1;

static const char *mqttPathPrefix;

static int elementCount = 0;
static iotElement_t elements[IOT_MAX_ELEMENT] = {0};

static void mqttClientThread(void* pvParameters);
static void mqttMessageArrived(MessageData *data);
static void wifiInitialise(void);

void iotInit(const char *roomPath)
{
    mqttPathPrefix = roomPath;   
    ESP_LOGI(TAG, "Initialised path: %s", roomPath);
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

void iotElementAdd(const char *name, iotElement_t **ppElement)
{
    iotElement_t *element;
    if (elementCount >= IOT_MAX_ELEMENT)
    {
        *ppElement = NULL;
        return;
    }
    element = &elements[elementCount];
    elementCount++;
    
    element->name = name;
    *ppElement = element;
    ESP_LOGI(TAG, "Added element \"%s\"", name);
}

void iotElementSubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, iotElementSubUpdateCallback_t callback, void *userData, iotElementSub_t **ppSub)
{
    iotElementSub_t *sub = NULL;
    int i;
    size_t len;

    for (i=0; i < IOT_MAX_SUB; i++)
    {
        if (element->subs[i].name == NULL)
        {
            sub = &element->subs[i];
            break;
        }
    }
    *ppSub = sub;
    if (sub == NULL)
    {
        ESP_LOGE(TAG, "No more subs available in element %s", element->name);
        return;
    }
    
    len = strlen(mqttPathPrefix) + 1 + strlen(element->name) + 1 + strlen(name) + 1;
    sub->path = (char *)malloc(len);
    if (sub->path == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate path for Element %s Sub %s", element->name, name);
        return;
    }
    sub->name = name;
    sprintf(sub->path, "%s/%s/%s", mqttPathPrefix, element->name, sub->name);
    sub->type = type;
    sub->callback = callback;
    sub->userData = userData;
    ESP_LOGI(TAG, "Added sub \"%s\" to element \"%s\"", name, element->name);
}

void iotElementPubAdd(iotElement_t *element, const char *name, enum iotValueType_e type, iotValue_t initial, iotElementPub_t **ppPub)
{
    iotElementPub_t *pub = NULL;
    int i;
    
    for (i=0; i < IOT_MAX_PUB; i++)
    {
        if (element->pubs[i].name == NULL)
        {
            pub = &element->pubs[i];
            break;
        }
    }
    *ppPub = pub;
    if (pub == NULL)
    {
        ESP_LOGE(TAG, "No more pubs available in element %s", element->name);
        return;
    }
    pub->name = name;
    pub->type = type;
    pub->value = initial;
    pub->updateRequired = false;
    ESP_LOGI(TAG, "Added pub \"%s\" to element \"%s\"", name, element->name);
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
        ESP_LOGI(TAG, "PUB: Flagging update required for %s", pub->name);
    }
    else
    {
        ESP_LOGI(TAG, "PUB: No update required for %s", pub->name);
    }
    
}

static void iotElementPubSendUpdate(iotElement_t *element, iotElementPub_t *pub, MQTTClient *client)
{
    char path[MAX_TOPIC_NAME + 1];
    MQTTMessage message;
    char payload[30] = "";
    int rc;

    if (pub->name[0] == 0)
    {
        sprintf(path, "%s/%s", mqttPathPrefix, element->name);
    }
    else
    {
        sprintf(path, "%s/%s/%s", mqttPathPrefix, element->name, pub->name);
    }
    

    message.qos = QOS0;
    message.retained = 0;
    
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
    } 
    else 
    {
        ESP_LOGI(TAG, "PUB: Sent %s (%d) to %s", (char *)message.payload, message.payloadlen, path);
    }
    pub->updateRequired = false;
}

static void iotElementSendUpdate(iotElement_t *element, bool force, MQTTClient *client)
{
    int p;
    for (p = 0; p < IOT_MAX_PUB; p ++)
    {
        if (element->pubs[p].name != NULL)
        {
            if (element->pubs[p].updateRequired || force)
            {
                iotElementPubSendUpdate(element, &element->pubs[p], client);
            }
        }
    }
}

static void iotElementSubUpdate(iotElementSub_t *sub, char *payload, size_t len)
{
    iotValue_t value;
    ESP_LOGI(TAG, "SUB: new message \"%s\" for \"%s\"", payload, sub->name);
    switch(sub->type)
    {
        case iotValueType_Bool:
        if ((strcasecmp(payload, "on") == 0) || (strcasecmp(payload, "true") == 0))
        {
            value.b = true;
        }
        else if ((strcasecmp(payload, "off") == 0) || (strcasecmp(payload, "false") == 0))
        {
            value.b = false;
        }
        else
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

static void iotElementSubSubscribe(iotElement_t *element, iotElementSub_t *sub, MQTTClient *client)
{
    int rc;
    if ((rc = MQTTSubscribe(client, sub->path, 2, mqttMessageArrived)) != 0) 
    {
        ESP_LOGE(TAG, "SUB: Return code from MQTT subscribe is %d for \"%s\"", rc, sub->path);
    } 
    else 
    {
        ESP_LOGI(TAG, "SUB: element %s sub %s MQTT subscribe to topic \"%s\"", element->name, sub->name, sub->path);
    }
}

static void iotElementSubscribe(iotElement_t *element, MQTTClient *client)
{
    int s;
    for (s = 0; s < IOT_MAX_SUB; s ++)
    {
        if (element->subs[s].name != NULL)
        {
            iotElementSubSubscribe(element, &element->subs[s], client);
        }
    }
}

static esp_err_t wifiEventHandler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
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
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(wifiEventHandler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void mqttMessageArrived(MessageData* data)
{
    int e;
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

    if ((strncmp(topic, mqttPathPrefix, len) == 0) || (topic[len] == '/'))
    {
        topic += len + 1;
        for (e=0;e<elementCount; e++)
        {
            len = strlen(elements[e].name);
            if ((strncmp(topic, elements[e].name, len) == 0) && (topic[len] == '/'))
            {
                int s;
                iotElement_t *element = &elements[e];
                topic += len + 1;
                for (s=0; s < IOT_MAX_SUB; s++)
                {
                    if (strcmp(topic, element->subs[s].name) == 0)
                    {
                        iotElementSubUpdate(&element->subs[s], payload, data->message->payloadlen);
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
    int rc = 0, e;
    bool loop = true;

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;

    ESP_LOGI(TAG, "mqtt client thread starts");

    NetworkInit(&network);
    MQTTClientInit(&client, &network, 30000, sendbuf, sizeof(sendbuf), readbuf, sizeof(readbuf));

    while (true)
    {
        ESP_LOGI(TAG, "Waiting for network connection");
        loop = true;
        while(loop)
        {
            /* Wait for the callback to set the CONNECTED_BIT in the
            event group.
            */
            xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                false, true, portMAX_DELAY);
            ESP_LOGI(TAG, "Connected to AP");

            if ((rc = NetworkConnect(&network, CONFIG_MQTT_HOST, CONFIG_MQTT_PORT)) != 0) 
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
        connectData.clientID.cstring = CONFIG_MQTT_CLIENT_ID;

#ifdef CONFIG_MQTT_USERNAME
        connectData.username.cstring = CONFIG_MQTT_USERNAME;
#endif
#ifdef CONFIG_MQTT_PASSWORD
        connectData.password.cstring = CONFIG_MQTT_PASSWORD;
#endif

        if ((rc = MQTTConnect(&client, &connectData)) != 0) {
            ESP_LOGE(TAG, "Return code from MQTT connect is %d", rc);
        } else {
            ESP_LOGI(TAG, "MQTT Connected");
        }

        for (e=0;e<elementCount; e++)
        {
            iotElementSubscribe(&elements[e], &client);
            iotElementSendUpdate(&elements[e], true, &client);
        }
        loop = true;
        while(loop)
        {
            for (e=0;e<elementCount; e++)
            {
                iotElementSendUpdate(&elements[e], false, &client);
            }
#if defined(MQTT_TASK)
            MutexLock(&client.mutex);
#endif
            if (MQTTYield(&client, 100) < 0)
            {
                loop = false;
            }
#if defined(MQTT_TASK)
            MutexUnlock(&client.mutex);
#endif
        }
        ESP_LOGW(TAG, "Lost connection to MQTT Server");
        network.disconnect(&network);       
    }
}