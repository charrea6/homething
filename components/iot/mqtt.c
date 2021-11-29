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
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "cbor.h"

#include "iot.h"
#include "wifi.h"
#include "notifications.h"
#include "sdkconfig.h"
#include "iotInternal.h"

static const char *TAG="IOT-MQTT";

#define MQTT_TASK_STACK_SIZE (3 * 1024)

#define MAX_TOPIC_NAME 512

#define MAX_LENGTH_MQTT_SERVER 256
#define MAX_LENGTH_MQTT_USERNAME 65
#define MAX_LENGTH_MQTT_PASSWORD 65

bool mqttIsSetup = false;
bool mqttIsConnected = false;

static esp_mqtt_client_handle_t mqttClient;

static char mqttServer[MAX_LENGTH_MQTT_SERVER];
static int mqttPort;
static char mqttUsername[MAX_LENGTH_MQTT_USERNAME];
static char mqttPassword[MAX_LENGTH_MQTT_PASSWORD];
static SemaphoreHandle_t sendMutex;

static void mqttMessageArrived(char *mqttTopic, int mqttTopicLen, char *data, int dataLen);
static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event);

int mqttInit(void)
{
    nvs_handle handle;
    esp_err_t err;

    sendMutex = xSemaphoreCreateMutex();

    err = nvs_open("mqtt", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        size_t len = sizeof(mqttServer);
        if (nvs_get_str(handle, "host", mqttServer, &len) == ESP_OK) {
            uint16_t p;
            err = nvs_get_u16(handle, "port", &p);
            if (err == ESP_OK) {
                mqttPort = (int) p;
            } else {
                mqttPort = 1883;
            }
            len = sizeof(mqttUsername);
            if (nvs_get_str(handle, "user", mqttUsername, &len) != ESP_OK) {
                mqttUsername[0] = 0;
            }
            len = sizeof(mqttPassword);
            if (nvs_get_str(handle, "pass", mqttPassword, &len) != ESP_OK) {
                mqttPassword[0] = 0;
            }
        } else {
            mqttServer[0] = 0;
        }

        nvs_close(handle);
    }
    if (mqttServer[0] == 0) {
        return 0;
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
    return 0;
}

bool iotMqttIsConnected(void)
{
    return mqttIsConnected;
}

int iotMqttPublish(const char *topic, const char *data, int len, int qos, int retain)
{
    int result;
    xSemaphoreTake(sendMutex, portMAX_DELAY);
    result = esp_mqtt_client_publish(mqttClient, topic, data, len, qos, retain);
    xSemaphoreGive(sendMutex);
    return result;
}

bool mqttSubscribe(char *topic)
{
    int rc;
    if ((rc = esp_mqtt_client_subscribe(mqttClient, topic, 2)) == -1) {
        ESP_LOGE(TAG, "SUB: Return code from MQTT subscribe is %d for \"%s\"", rc, topic);
        return false;
    } else {
        ESP_LOGI(TAG, "SUB: MQTT subscribe to topic \"%s\"", topic);
    }
    return true;
}

static void mqttMessageArrived(char *mqttTopic, int mqttTopicLen, char *data, int dataLen)
{
    char *topic, *topicStart;
    char *payload;

    topicStart = topic = malloc(mqttTopicLen + 1);
    if (topic == NULL) {
        ESP_LOGE(TAG, "Not enough memory to copy topic!");
        return;
    }
    memcpy(topic, mqttTopic, mqttTopicLen);
    topic[mqttTopicLen] = 0;

    ESP_LOGI(TAG, "Message arrived, topic %s payload %d", topic, dataLen);
    payload = malloc(dataLen + 1);
    if (payload == NULL) {
        free(topicStart);
        ESP_LOGE(TAG, "Not enough memory to copy payload!");
        return;
    }
    memcpy(payload, data, dataLen);
    payload[dataLen] = 0;

    iotMqttProcessMessage(topic, payload, dataLen);

    free(payload);
    free(topicStart);
}

static esp_err_t mqttEventHandler(esp_mqtt_event_handle_t event)
{
    NotificationsData_t notification;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        iotMqttConnected();
        mqttIsConnected = true;
        notification.connectionState = Notifications_ConnectionState_Connected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_MQTT, &notification);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT Disconnected");
        mqttIsConnected = false;
        notification.connectionState = Notifications_ConnectionState_Disconnected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_MQTT, &notification);
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

    default:
        break;
    }
    return ESP_OK;
}

void mqttNetworkConnected(bool connected)
{
    if (connected) {
        esp_mqtt_client_start(mqttClient);
    } else {
        esp_mqtt_client_stop(mqttClient);
    }
}