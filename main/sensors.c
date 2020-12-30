#include <stdlib.h>
#include <stdint.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "iot.h"
#include "notifications.h"
#include "deviceprofile.h"
#include "dht.h"

#include "sensors.h"

#define HUMIDITY_PUB_INDEX_HUMIDITY   0
#define HUMIDITY_PUB_INDEX_TEMPERTURE 1

static void temperatureUpdated(void *user,  NotificationsMessage_t *message);
static void humidityUpdated(void *user,  NotificationsMessage_t *message);

static char const TAG[]="sensors";

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    humidityElementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, "temperature")
    )
);

static uint32_t humiditySensorCount = 0;


struct DHT22 {
    Notifications_ID_t id;
    char temperatureStr[6];
    char humidityStr[6];
    iotElement_t element;
};
static int dht22Count = 0;
static struct DHT22 *dht22Sensors = NULL;

int initDHT22(int nrofDHT22) {
    dht22Sensors = calloc(sizeof(struct DHT22), nrofDHT22);
    if (dht22Sensors == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for DHT22 sensors");
        return -1;
    }
    notificationsRegister(Notifications_Class_Temperature, NOTIFICATIONS_ID_ALL, temperatureUpdated, NULL);
    notificationsRegister(Notifications_Class_Humidity, NOTIFICATIONS_ID_ALL, humidityUpdated, NULL);
    return 0;
}

Notifications_ID_t addDHT22(CborValue *entry) {
    struct DHT22 *dht;
    iotValue_t value;
    uint32_t pin;

    if (deviceProfileParserEntryGetUint32(entry, &pin)){
        ESP_LOGE(TAG, "addDHT22: Failed to get pin!");
        return NOTIFICATIONS_ID_ERROR;
    }
    dht = &dht22Sensors[dht22Count];
    dht22Count++;
    
    dht->id = dht22Add(pin);
    
    sprintf(dht->temperatureStr, "0.0");
    sprintf(dht->humidityStr, "0.0");
    dht->element = iotNewElement(&humidityElementDescription, dht, "humidity%d", humiditySensorCount);
    humiditySensorCount++;

    value.s = dht->humidityStr;
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
    value.s = dht->temperatureStr;
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
    return dht->id;
} 

static void temperatureUpdated(void *user,  NotificationsMessage_t *message) {
    uint32_t i;
    iotValue_t value;

    for (i = 0; i < dht22Count; i ++) {
        if (dht22Sensors[i].id == message->id){
            sprintf(dht22Sensors[i].temperatureStr, "%d.%d", message->data.temperature / 10, message->data.temperature % 10);
            value.s = dht22Sensors[i].temperatureStr;
            iotElementPublish(dht22Sensors[i].element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
            break;
        }
    }
}

static void humidityUpdated(void *user,  NotificationsMessage_t *message) {
    uint32_t i;
    iotValue_t value;

    for (i = 0; i < dht22Count; i ++) {
        if (dht22Sensors[i].id == message->id){
            sprintf(dht22Sensors[i].humidityStr, "%d.%d", message->data.humidity / 10, message->data.humidity % 10);
            value.s = dht22Sensors[i].humidityStr;
            iotElementPublish(dht22Sensors[i].element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
            break;
        }
    }
}