#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "iot.h"
#include "notifications.h"
#include "deviceprofile.h"
#include "dht.h"
#include "bmp280.h"
#include "si7021.h"
#include "sensors.h"

#define HUMIDITY_PUB_INDEX_HUMIDITY   0
#define HUMIDITY_PUB_INDEX_TEMPERTURE 1
#define HUMIDITY_PUB_INDEX_PRESSURE   2

#define TEMPERATURE_PUB_INDEX_TEMPERATURE 0
#define TEMPERATURE_PUB_INDEX_PRESSURE    1

#define SECS_TO_TICKS(secs) ((secs * 1000) / portTICK_RATE_MS)


struct HumiditySensor {
    Notifications_ID_t id;
    iotElement_t element;
    char temperatureStr[8];
    char humidityStr[8];
};

struct BME280_t {
    bmp280_t dev;
    struct HumiditySensor *sensor;
    char pressureStr[8];
    int32_t lastTemperature;
    uint32_t lastPressure;
    uint32_t lastHumidity;
};

struct SI7021_t {
    i2c_dev_t dev;
    struct HumiditySensor *sensor;
    float lastTemperature;
    float lastHumidity;
};

static int addHumiditySensor(struct HumiditySensor **sensor, uint32_t *sensorId);

static void temperatureUpdated(void *user,  NotificationsMessage_t *message);
static void humidityUpdated(void *user,  NotificationsMessage_t *message);

#ifdef CONFIG_BME280
static void pressureUpdated(void *user, NotificationsMessage_t *message);
static void bme280MeasureTimer(TimerHandle_t xTimer);
#endif

#ifdef CONFIG_SI7021
static void si7021MeasureTimer(TimerHandle_t xTimer);
#endif

static char const TAG[]="sensors";

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    humidityElementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, "temperature")
    )
);

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    htpElementDescription, // Humidity / Temperature / Pressure
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, "temperature"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, "pressure")
    )
);

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    tpElementDescription, // Temperature / Pressure
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_STRING, "pressure")
    )
);

static uint32_t nrofHumiditySensors = 0;
static uint32_t humiditySensorCount = 0;

static struct HumiditySensor *humiditySensors = NULL;

#ifdef CONFIG_BME280
static struct BME280_t *bme280Devices;
#endif

#ifdef CONFIG_SI7021
static struct SI7021_t *si7021Devices;
#endif

#ifdef CONFIG_DHT22
int initDHT22(int nrofSensors) {
    nrofHumiditySensors += nrofSensors;
    return 0;
}

Notifications_ID_t addDHT22(CborValue *entry) {
    struct HumiditySensor *dht;
    iotValue_t value;
    uint32_t pin;
    uint32_t sensorId;

    if (deviceProfileParserEntryGetUint32(entry, &pin)){
        ESP_LOGE(TAG, "addDHT22: Failed to get pin!");
        return NOTIFICATIONS_ID_ERROR;
    }
    if (addHumiditySensor(&dht, &sensorId)){
        return NOTIFICATIONS_ID_ERROR;
    }
    dht->id = dht22Add(pin);
    dht->element = iotNewElement(&humidityElementDescription, dht, "humidity%d", sensorId);

    value.s = dht->humidityStr;
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
    value.s = dht->temperatureStr;
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
    return dht->id;
}
#endif

#ifdef CONFIG_BME280
int initBME280(int nrofSensors) {
    bme280Devices = calloc(nrofSensors, sizeof(struct BME280_t));
    if (bme280Devices == NULL) {
        return -1;
    }
    nrofHumiditySensors += nrofSensors;
    
    return 0; 
}

Notifications_ID_t addBME280(CborValue *entry) {
    struct HumiditySensor *bme;
    iotValue_t value;
    uint32_t sensorId;
    DeviceProfile_I2CDetails_t i2cDetails;
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    struct BME280_t *dev;
    esp_err_t err;
    
    if (deviceProfileParserEntryGetI2CDetails(entry, &i2cDetails)){
        ESP_LOGE(TAG, "Failed to get I2C details!");
        return NOTIFICATIONS_ID_ERROR;
    }
    
    if (addHumiditySensor(&bme, &sensorId)) {
        return NOTIFICATIONS_ID_ERROR;
    }

    dev = bme280Devices;
    memset(dev, 0, sizeof(struct BME280_t));
    dev->sensor = bme;
    err = bmp280_init_desc(&dev->dev, i2cDetails.addr, 0, i2cDetails.sda, i2cDetails.scl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addBME280: init desc failed! err = %d", err);
        return NOTIFICATIONS_ID_ERROR;
    }
    err = bmp280_init(&dev->dev, &params);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addBME280: init failed! err = %d", err);
        return NOTIFICATIONS_ID_ERROR;
    }
    bme280Devices ++;
    
    bool bme280p = dev->dev.id == BME280_CHIP_ID;
    ESP_LOGI(TAG, "addBME280: found %s\n", bme280p ? "BME280" : "BMP280");

    if (bme280p) {
        bme->element = iotNewElement(&htpElementDescription, bme, "humidity%d", sensorId);

        value.s = bme->humidityStr;
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
        value.s = bme->temperatureStr;
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
        strcpy(dev->pressureStr, "0.00");
        value.s = dev->pressureStr;
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_PRESSURE, value);
    } else {
        bme->element = iotNewElement(&tpElementDescription, bme, "temperature%d", sensorId);

        value.s = bme->temperatureStr;
        iotElementPublish(bme->element, TEMPERATURE_PUB_INDEX_TEMPERATURE, value);
        strcpy(dev->pressureStr, "0.00");
        value.s = dev->pressureStr;
        iotElementPublish(bme->element, TEMPERATURE_PUB_INDEX_PRESSURE, value);

    }

    bme->id = NOTIFICATIONS_MAKE_I2C_ID(i2cDetails.sda, i2cDetails.scl, i2cDetails.addr);
    notificationsRegister(Notifications_Class_Pressure, bme->id, pressureUpdated, dev);
    xTimerStart(xTimerCreate("bme", SECS_TO_TICKS(5), pdTRUE, dev, bme280MeasureTimer), 0);

    return bme->id;
}

static void bme280MeasureTimer(TimerHandle_t xTimer) {
    struct BME280_t *dev = pvTimerGetTimerID(xTimer);
    int32_t temperature;
    uint32_t humidity, pressure;
    esp_err_t err;
    Notifications_ID_t id = dev->sensor->id;
    NotificationsData_t data;
    err = bmp280_read_fixed(&dev->dev, &temperature, &pressure, &humidity);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bme280MeasureTimer: Failed reading sensor %d", err);
        return;
    }
    if (temperature != dev->lastTemperature) {
        data.temperature = temperature;
        dev->lastTemperature = temperature;
        notificationsNotify(Notifications_Class_Temperature, id, &data);
    }
    if ((dev->dev.id == BME280_CHIP_ID) && (humidity != dev->lastHumidity)) {
        data.humidity = (humidity * 100) / 1024;
        dev->lastHumidity = humidity;
        notificationsNotify(Notifications_Class_Humidity, id, &data);
    }

    if (pressure != dev->lastPressure) {
        data.pressure = pressure / 256;
        dev->lastPressure = pressure;
        notificationsNotify(Notifications_Class_Pressure, id, &data);
    }
}
#endif

#ifdef CONFIG_SI7021
int initSI7021(int nrofSensors) {
    si7021Devices = calloc(nrofSensors, sizeof(struct SI7021_t));
    if (si7021Devices == NULL) {
        return -1;
    }
    nrofHumiditySensors += nrofSensors;
    
    return 0; 
}

Notifications_ID_t addSI7021(CborValue *entry) {
    struct HumiditySensor *sensor;
    iotValue_t value;
    uint32_t sensorId;
    DeviceProfile_I2CDetails_t i2cDetails;
    struct SI7021_t *dev;
    esp_err_t err;
    
    if (deviceProfileParserEntryGetI2CDetails(entry, &i2cDetails)){
        ESP_LOGE(TAG, "Failed to get I2C details!");
        return NOTIFICATIONS_ID_ERROR;
    }
    
    if (addHumiditySensor(&sensor, &sensorId)) {
        return NOTIFICATIONS_ID_ERROR;
    }

    dev = si7021Devices;
    memset(dev, 0, sizeof(struct SI7021_t));
    dev->sensor = sensor;
    err = si7021_init_desc(&dev->dev, 0, i2cDetails.sda, i2cDetails.scl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addSI7021: Failed to init %d", err);
        return NOTIFICATIONS_ID_ERROR;
    }
    si7021Devices++;

    sensor->element = iotNewElement(&humidityElementDescription, sensor, "humidity%d", sensorId);
    value.s = sensor->humidityStr;
    iotElementPublish(sensor->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
    value.s = sensor->temperatureStr;
    iotElementPublish(sensor->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
    sensor->id = NOTIFICATIONS_MAKE_I2C_ID(i2cDetails.sda, i2cDetails.scl, SI7021_I2C_ADDR);
    xTimerStart(xTimerCreate("si7021", SECS_TO_TICKS(5), pdTRUE, dev, si7021MeasureTimer), 0);
    return sensor->id;
}

static void si7021MeasureTimer(TimerHandle_t xTimer) {
    struct SI7021_t *dev = pvTimerGetTimerID(xTimer);
    esp_err_t err;
    float temperature, humidity;
    NotificationsData_t data;

    err = si7021_measure_temperature(&dev->dev, &temperature);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read temperature %d", err);
        return;
    }
    
    if (temperature != dev->lastTemperature){
        data.temperature = (int32_t) (temperature * 100.0);
        notificationsNotify(Notifications_Class_Temperature, dev->sensor->id, &data);
        dev->lastTemperature = temperature;
    }

    err = si7021_measure_humidity(&dev->dev, &humidity);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read humidity %d", err);
        return;
    }
    
    if (humidity != dev->lastHumidity){
        data.humidity = (int32_t) (humidity * 100.0);
        notificationsNotify(Notifications_Class_Humidity, dev->sensor->id, &data);
        dev->lastHumidity = humidity;
    }
}
#endif
static void temperatureUpdated(void *user,  NotificationsMessage_t *message) {
    uint32_t i;
    iotValue_t value;

    for (i = 0; i < humiditySensorCount; i ++) {
        if (humiditySensors[i].id == message->id){
            int16_t temperature = message->data.temperature;
            char *str = humiditySensors[i].temperatureStr;
            if (temperature < 0) {
                temperature *= -1;
                str[0] = '-';
                str++;
            }
            sprintf(str, "%d.%02d", temperature / 100, temperature % 100);
            ESP_LOGI(TAG, "Temperature Updated: 0x%08x %s", message->id, humiditySensors[i].temperatureStr);
            value.s = humiditySensors[i].temperatureStr;
            iotElementPublish(humiditySensors[i].element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
            break;
        }
    }
}

static void humidityUpdated(void *user,  NotificationsMessage_t *message) {
    uint32_t i;
    iotValue_t value;

    for (i = 0; i < humiditySensorCount; i ++) {
        if (humiditySensors[i].id == message->id){
            sprintf(humiditySensors[i].humidityStr, "%d.%02d", message->data.humidity / 100, message->data.humidity % 100);
            ESP_LOGI(TAG, "Humidity Updated: 0x%08x %s", message->id, humiditySensors[i].humidityStr);
            value.s = humiditySensors[i].humidityStr;
            iotElementPublish(humiditySensors[i].element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
            break;
        }
    }
}

#ifdef CONFIG_BME280
static void pressureUpdated(void *user, NotificationsMessage_t *message) {
    iotValue_t value;
    struct BME280_t *dev = user;
    sprintf(dev->pressureStr, "%d.%02d", message->data.pressure / 100, message->data.pressure % 100);
    value.s = dev->pressureStr;
    ESP_LOGI(TAG, "Pressure Updated: 0x%08x %s", message->id, dev->pressureStr);
    iotElementPublish(dev->sensor->element, (dev->dev.id == BME280_CHIP_ID) ? HUMIDITY_PUB_INDEX_PRESSURE: TEMPERATURE_PUB_INDEX_PRESSURE, value);
}
#endif

static int addHumiditySensor(struct HumiditySensor **sensor, uint32_t *sensorId) {
    struct HumiditySensor *newSensor;
    if (humiditySensors == NULL) {
        humiditySensors = calloc(sizeof(struct HumiditySensor), nrofHumiditySensors);
        if (humiditySensors == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for humidity sensors");
            return -1;
        }
        notificationsRegister(Notifications_Class_Temperature, NOTIFICATIONS_ID_ALL, temperatureUpdated, NULL);
        notificationsRegister(Notifications_Class_Humidity, NOTIFICATIONS_ID_ALL, humidityUpdated, NULL);
    }
    if (humiditySensorCount >= nrofHumiditySensors){
        ESP_LOGE(TAG, "All humidity sensors already allocated!");
        return -1;
    }
    newSensor = &humiditySensors[humiditySensorCount];
    newSensor->id = NOTIFICATIONS_ID_ERROR;
    strcpy(newSensor->temperatureStr, "0.00");
    strcpy(newSensor->humidityStr, "0.00");
    *sensor = newSensor;
    *sensorId = humiditySensorCount;
    humiditySensorCount ++;
    return 0;
}