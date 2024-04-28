#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
#include "ds18x20.h"
#include "sensors.h"
#include "sensorsInternal.h"

#define HUMIDITY_PUB_INDEX_HUMIDITY   0
#define HUMIDITY_PUB_INDEX_TEMPERATURE 1
#define HUMIDITY_PUB_INDEX_PRESSURE   2

#define TEMPERATURE_PUB_INDEX_TEMPERATURE 0
#define TEMPERATURE_PUB_INDEX_PRESSURE    1


struct BME280 {
    bmp280_t dev;
};

struct SI7021 {
    i2c_dev_t dev;
};

struct DS18x20Sensor {
    Notifications_ID_t id;
    iotElement_t element;
    ds18x20_addr_t addr;
};

struct DS18x20Pin {
    int8_t pin;
    int nrofSensors;
    float temperatureCorrection;
    struct DS18x20Sensor *sensors;
    TimerHandle_t measureTimer;
    TimerHandle_t readTimer;
};

#ifdef CONFIG_DHT22
static void dht22MeasureTimer(Sensor_t *sensor);
#endif

#ifdef CONFIG_BME280
static void bme280MeasureTimer(Sensor_t *sensor);
#endif

#ifdef CONFIG_SI7021
static void si7021MeasureTimer(Sensor_t *sensor);
#endif

#ifdef CONFIG_DS18x20
static void ds18x20MeasureTimer(TimerHandle_t xTimer);
static void ds18x20ReadTimer(TimerHandle_t xTimer);
#endif

static char const TAG[]="sensorsTHP";
static uint32_t nrofHumiditySensors = 0;
static uint32_t nrofTemperatureSensors = 0;

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021)
IOT_DESCRIBE_ELEMENT_NO_SUBS(
    humidityElementDescription,
    IOT_ELEMENT_TYPE_SENSOR_HUMIDITY,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, PERCENT_RH, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, "temperature")
    )
);
#endif

#if defined(CONFIG_BME280)
IOT_DESCRIBE_ELEMENT_NO_SUBS(
    htpElementDescription, // Humidity / Temperature / Pressure
    IOT_ELEMENT_TYPE_SENSOR_HUMIDITY,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, PERCENT_RH, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, "temperature"),
        IOT_DESCRIBE_PUB(RETAINED, KPA, "pressure")
    )
);

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    tpElementDescription, // Temperature / Pressure
    IOT_ELEMENT_TYPE_SENSOR_TEMPERATURE,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, IOT_PUB_USE_ELEMENT),
        IOT_DESCRIBE_PUB(RETAINED, KPA, "pressure")
    )
);
#endif

#if defined(CONFIG_DS18x20)
IOT_DESCRIBE_ELEMENT_NO_SUBS(
    temperatureElementDescription, // Temperature
    IOT_ELEMENT_TYPE_SENSOR_TEMPERATURE,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, IOT_PUB_USE_ELEMENT)
    )
);
#endif

#ifdef CONFIG_BME280
static struct BME280 *bme280Devices;
#endif

#ifdef CONFIG_SI7021
static struct SI7021 *si7021Devices;
#endif

#ifdef CONFIG_DS18x20
static struct DS18x20Pin *ds18x20Pins;
#endif


#ifdef CONFIG_DHT22
int sensorsDHT22Init(int nrofSensors)
{
    return nrofSensors;
}

int sensorsDHT22Add(DeviceProfile_Dht22Config_t *config)
{
    struct Sensor *dht;
    gpio_config_t pinConfig;
    iotValue_t value;
    uint32_t sensorId = nrofHumiditySensors ++;

    if (sensorsAddSensor(&dht)) {
        return -1;
    }

    pinConfig.pin_bit_mask = 1<<config->pin;
    pinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pinConfig.intr_type = GPIO_INTR_DISABLE;
    pinConfig.mode = GPIO_MODE_INPUT;
    pinConfig.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&pinConfig);

    if (config->id) {
        dht->id = notificationsNewId(config->id);
    }
    dht->element = iotNewElement(&humidityElementDescription, 0, NULL, dht, "humidity%d", sensorId);
    if (config->name) {
        iotElementSetHumanDescription(dht->element, config->name);
    }
    dht->details.pin = config->pin;

    value.i = 0;
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_TEMPERATURE, value);

    sensorsCreateSecondsTimer(dht, "dht22", 5, dht22MeasureTimer);
    return 0;
}

static void dht22MeasureTimer(Sensor_t *sensor)
{
    int16_t temperature, humidity;

    if (dht_read_data(DHT_TYPE_AM2301, sensor->details.pin, &humidity, &temperature) != ESP_OK) {
        ESP_LOGE(TAG, "dht22MeasureTimer: Failed reading sensor pin %d", sensor->details.pin);
        return;
    }
    sensorsUpdateForHundredth(sensor, HUMIDITY_PUB_INDEX_TEMPERATURE, Notifications_Class_Temperature, temperature * 10);
    sensorsUpdateForHundredth(sensor, HUMIDITY_PUB_INDEX_HUMIDITY, Notifications_Class_Humidity, humidity * 10);
}
#endif

#ifdef CONFIG_BME280
int sensorsBME280Init(int nrofSensors)
{
    bme280Devices = calloc(nrofSensors, sizeof(struct BME280));
    if (bme280Devices == NULL) {
        return -1;
    }
    return nrofSensors;
}

int sensorsBME280Add(DeviceProfile_Bme280Config_t *config)
{
    struct Sensor *bme;
    iotValue_t value;

    bmp280_params_t params;
    bmp280_init_default_params(&params);
    struct BME280 *dev;
    esp_err_t err;

    if (sensorsAddSensor(&bme)) {
        return -1;
    }

    dev = bme280Devices;
    bme->details.dev = dev;
    memset(dev, 0, sizeof(struct BME280));

    err = bmp280_init_desc(&dev->dev, config->addr, 0, config->sda, config->scl);
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
    ESP_LOGI(TAG, "addBME280: found %s", bme280p ? "BME280" : "BMP280");

    value.i = 0;

    if (bme280p) {
        uint32_t sensorId = nrofHumiditySensors ++;
        bme->element = iotNewElement(&htpElementDescription, 0, NULL, bme, "humidity%d", sensorId);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_TEMPERATURE, value);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_PRESSURE, value);
    } else {
        uint32_t sensorId = nrofTemperatureSensors ++;
        bme->element = iotNewElement(&tpElementDescription, 0, NULL, bme, "temperature%d", sensorId);
        iotElementPublish(bme->element, TEMPERATURE_PUB_INDEX_TEMPERATURE, value);
        iotElementPublish(bme->element, TEMPERATURE_PUB_INDEX_PRESSURE, value);
    }
    if (config->id) {
        bme->id = notificationsNewId(config->id);
    }
    if (config->name) {
        iotElementSetHumanDescription(bme->element, config->name);
    }

    sensorsCreateSecondsTimer(bme, "bme280", 5, bme280MeasureTimer);
    return 0;
}

static void bme280MeasureTimer(Sensor_t *sensor)
{
    struct BME280 *bme = sensor->details.dev;
    bool bme280p = bme->dev.id == BME280_CHIP_ID;
    int32_t temperature;
    uint32_t humidity, pressure;
    esp_err_t err;

    err = bmp280_read_fixed(&bme->dev, &temperature, &pressure, &humidity);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bme280MeasureTimer: Failed reading sensor %d", err);
        return;
    }
    sensorsUpdateForHundredth(sensor, bme280p ? HUMIDITY_PUB_INDEX_TEMPERATURE: TEMPERATURE_PUB_INDEX_TEMPERATURE,  Notifications_Class_Temperature, temperature);

    if (bme280p) {
        sensorsUpdateForHundredth(sensor, HUMIDITY_PUB_INDEX_HUMIDITY, Notifications_Class_Humidity, (humidity * 100) / 1024);
    }

    sensorsUpdateForHundredth(sensor, bme280p ? HUMIDITY_PUB_INDEX_PRESSURE: TEMPERATURE_PUB_INDEX_PRESSURE, Notifications_Class_Pressure, pressure / 256);
}
#endif

#ifdef CONFIG_SI7021
int sensorsSI7021Init(int nrofSensors)
{
    si7021Devices = calloc(nrofSensors, sizeof(struct SI7021));
    if (si7021Devices == NULL) {
        return -1;
    }
    return nrofSensors;
}

int sensorsSI7021Add(DeviceProfile_Si7021Config_t *config)
{
    struct Sensor *sensor;
    iotValue_t value;
    uint32_t sensorId = nrofHumiditySensors ++;
    struct SI7021 *dev;
    esp_err_t err;

    if (sensorsAddSensor(&sensor)) {
        return -1;
    }

    dev = si7021Devices;
    memset(dev, 0, sizeof(struct SI7021));
    err = si7021_init_desc(&dev->dev, 0, config->sda, config->scl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addSI7021: Failed to init %d", err);
        return -1;
    }

    err = si7021_reset(&dev->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addSI7021: Failed to reset %d", err);
        return -1;
    }

    err = si7021_set_heater(&dev->dev, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addSI7021: Failed to disable heater %d", err);
        return -1;
    }
    si7021Devices++;

    sensor->element = iotNewElement(&humidityElementDescription, 0, NULL, sensor, "humidity%d", sensorId);
    if (config->id) {
        sensor->id = notificationsNewId(config->id);
    }
    if (config->name) {
        iotElementSetHumanDescription(sensor->element, config->name);
    }

    value.i = 0;
    iotElementPublish(sensor->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
    iotElementPublish(sensor->element, HUMIDITY_PUB_INDEX_TEMPERATURE, value);
    sensor->details.dev = dev;
    sensorsCreateSecondsTimer(sensor, "si7021", 5, si7021MeasureTimer);
    return 0;
}

static void si7021MeasureTimer(Sensor_t *sensor)
{
    struct SI7021 *dev = sensor->details.dev;
    esp_err_t err;
    float temperature, humidity;

    err = si7021_measure_temperature(&dev->dev, &temperature);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read temperature %d", err);
        return;
    }

    sensorsUpdateForHundredth(sensor, HUMIDITY_PUB_INDEX_TEMPERATURE, Notifications_Class_Temperature, (int32_t) (temperature * 100.0));

    err = si7021_measure_humidity(&dev->dev, &humidity);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read humidity %d", err);
        return;
    }
    sensorsUpdateForHundredth(sensor, HUMIDITY_PUB_INDEX_HUMIDITY, Notifications_Class_Humidity, (int32_t) (humidity * 100.0));
}
#endif

#ifdef CONFIG_DS18x20
int sensorsDS18x20Init(int nrofSensors)
{
    ds18x20Pins = calloc(nrofSensors, sizeof(struct DS18x20Pin));
    if (ds18x20Pins == NULL) {
        return -1;
    }

    return 0;
}

#define DS18x20_MAX_DEVICES 20

int sensorsDS18x20Add(DeviceProfile_Ds18x20Config_t *config)
{
    ds18x20_addr_t deviceAddrs[DS18x20_MAX_DEVICES];
    struct DS18x20Pin *pinStruct;
    size_t nrofDevices = 0, i;
    gpio_config_t pinConfig;
    esp_err_t err;


    pinConfig.pin_bit_mask = 1<<config->pin;
    pinConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
    pinConfig.intr_type = GPIO_INTR_DISABLE;
    pinConfig.mode = GPIO_MODE_INPUT;
    pinConfig.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&pinConfig);

    ESP_LOGI(TAG, "addDS18x20: Searching pin %u", config->pin);
    err = ds18x20_scan_devices(config->pin, deviceAddrs, DS18x20_MAX_DEVICES, &nrofDevices);

    if ((err != ESP_OK) || (nrofDevices == 0)) {
        ESP_LOGE(TAG, "addDS18x20: Failed find any sensor!");
        return -1;
    }
    if (nrofDevices > DS18x20_MAX_DEVICES) {
        ESP_LOGW(TAG, "addDS18x20: Found %d devices however only %d are supported on a pin", nrofDevices, DS18x20_MAX_DEVICES);
        nrofDevices = DS18x20_MAX_DEVICES;
    } else {
        ESP_LOGI(TAG, "addDS18x20: Found %d devices", nrofDevices);
    }
    pinStruct = ds18x20Pins;
    pinStruct->temperatureCorrection = config->temperatureCorrection;
    pinStruct->sensors = calloc(nrofDevices, sizeof(struct DS18x20Sensor));
    if (pinStruct->sensors == NULL) {
        ESP_LOGE(TAG, "addDS18x20: Failed to allocate memory for sensors");
        return -1;
    }
    pinStruct->pin = config->pin;
    pinStruct->nrofSensors = nrofDevices;
    ds18x20Pins++;
    for (i = 0; i < nrofDevices; i++) {
        pinStruct->sensors[i].addr = deviceAddrs[i];
        pinStruct->sensors[i].id = NOTIFICATIONS_ID_ERROR;
        if ((i == 0) && (config->id)) {
            pinStruct->sensors[i].id = notificationsNewId(config->id);
        }
        pinStruct->sensors[i].element = iotNewElement(&temperatureElementDescription, 0, NULL, NULL, "temperature%08x%08x", (uint32_t)(deviceAddrs[i]>> 32), (uint32_t)(deviceAddrs[i]));
    }
    pinStruct->measureTimer = xTimerCreate("ds18x20M", SECS_TO_TICKS(5), pdFALSE, pinStruct, ds18x20MeasureTimer);
    pinStruct->readTimer = xTimerCreate("ds18x20R", MSECS_TO_TICKS(750), pdFALSE, pinStruct, ds18x20ReadTimer);
    xTimerStart(pinStruct->measureTimer, 0);
    return 0;
}

static void ds18x20MeasureTimer(TimerHandle_t xTimer)
{
    struct DS18x20Pin *pinStruct = pvTimerGetTimerID(xTimer);
    if (ds18x20_measure(pinStruct->pin, DS18X20_ANY, true) != ESP_OK) {
        ESP_LOGE(TAG, "ds18x20MeasureTimer: Failed to send measure");
        xTimerStart(pinStruct->measureTimer, 0);
        return;
    }
    xTimerStart(pinStruct->readTimer, 0);
}

static void ds18x20ReadTimer(TimerHandle_t xTimer)
{
    struct DS18x20Pin *pinStruct = pvTimerGetTimerID(xTimer);
    int i;
    onewire_depower(pinStruct->pin);

    for (i = 0; i < pinStruct->nrofSensors; i++) {
        float temp;
        if (ds18x20_read_temperature(pinStruct->pin, pinStruct->sensors[i].addr, &temp) == ESP_OK) {
            int itemp = (int)((temp + pinStruct->temperatureCorrection) * 100.0);
            struct Sensor sensor;
            sensor.id = pinStruct->sensors[i].id;
            sensor.element = pinStruct->sensors[i].element;
            sensorsUpdateForHundredth(&sensor, TEMPERATURE_PUB_INDEX_TEMPERATURE, Notifications_Class_Temperature, itemp);
        }
    }

    xTimerStart(pinStruct->measureTimer, 0);
}
#endif