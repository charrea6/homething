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

#define HUMIDITY_PUB_INDEX_HUMIDITY   0
#define HUMIDITY_PUB_INDEX_TEMPERTURE 1
#define HUMIDITY_PUB_INDEX_PRESSURE   2

#define TEMPERATURE_PUB_INDEX_TEMPERATURE 0
#define TEMPERATURE_PUB_INDEX_PRESSURE    1

#define MSECS_TO_TICKS(msecs) ((msecs) / portTICK_RATE_MS)
#define SECS_TO_TICKS(secs) MSECS_TO_TICKS(secs * 1000)


struct Sensor {
    Notifications_ID_t id;
    iotElement_t element;
    union {
        uint8_t pin;
        void *dev;
    } details;
};

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
    struct DS18x20Sensor *sensors;
    TimerHandle_t measureTimer;
    TimerHandle_t readTimer;
};

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021) || defined(CONFIG_DS18x20)
static void updateForHundredth(int hundredths, Notifications_Class_e clazz, struct Sensor *sensor, int index);
#endif

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021)
static int addHumiditySensor(struct Sensor **sensor, uint32_t *sensorId);
#endif

#ifdef CONFIG_DHT22
static void dht22MeasureTimer(TimerHandle_t xTimer);
#endif

#ifdef CONFIG_BME280
static void bme280MeasureTimer(TimerHandle_t xTimer);
#endif

#ifdef CONFIG_SI7021
static void si7021MeasureTimer(TimerHandle_t xTimer);
#endif

#ifdef CONFIG_DS18x20
static void ds18x20MeasureTimer(TimerHandle_t xTimer);
static void ds18x20ReadTimer(TimerHandle_t xTimer);
#endif

static char const TAG[]="sensors";

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

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021)
static uint32_t nrofHumiditySensors = 0;
static uint32_t humiditySensorCount = 0;

static struct Sensor *humiditySensors = NULL;
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
    nrofHumiditySensors += nrofSensors;
    return 0;
}

int sensorsDHT22Add(DeviceProfile_Dht22Config_t *config)
{
    struct Sensor *dht;
    gpio_config_t pinConfig;
    iotValue_t value;
    uint32_t sensorId;

    if (addHumiditySensor(&dht, &sensorId)) {
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
    iotElementPublish(dht->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);

    xTimerStart(xTimerCreate("dht22", SECS_TO_TICKS(5), pdTRUE, dht, dht22MeasureTimer), 0);
    return dht->id;
}

static void dht22MeasureTimer(TimerHandle_t xTimer)
{
    struct Sensor *dev = pvTimerGetTimerID(xTimer);
    int16_t temperature, humidity;

    if (dht_read_data(DHT_TYPE_AM2301, dev->details.pin, &humidity, &temperature) != ESP_OK) {
        ESP_LOGE(TAG, "dht22MeasureTimer: Failed reading sensor pin %d", dev->details.pin);
        return;
    }
    updateForHundredth(temperature * 10, Notifications_Class_Temperature, dev, HUMIDITY_PUB_INDEX_TEMPERTURE);
    updateForHundredth(humidity * 10, Notifications_Class_Humidity, dev, HUMIDITY_PUB_INDEX_HUMIDITY);
}
#endif

#ifdef CONFIG_BME280
int sensorsBME280Init(int nrofSensors)
{
    bme280Devices = calloc(nrofSensors, sizeof(struct BME280));
    if (bme280Devices == NULL) {
        return -1;
    }
    nrofHumiditySensors += nrofSensors;

    return 0;
}

int sensorsBME280Add(DeviceProfile_Bme280Config_t *config)
{
    struct Sensor *bme;
    iotValue_t value;
    uint32_t sensorId;

    bmp280_params_t params;
    bmp280_init_default_params(&params);
    struct BME280 *dev;
    esp_err_t err;

    if (addHumiditySensor(&bme, &sensorId)) {
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
    ESP_LOGI(TAG, "addBME280: found %s\n", bme280p ? "BME280" : "BMP280");

    value.i = 0;

    if (bme280p) {
        bme->element = iotNewElement(&htpElementDescription, 0, NULL, bme, "humidity%d", sensorId);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_HUMIDITY, value);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
        iotElementPublish(bme->element, HUMIDITY_PUB_INDEX_PRESSURE, value);
    } else {
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

    xTimerStart(xTimerCreate("bme", SECS_TO_TICKS(5), pdTRUE, bme, bme280MeasureTimer), 0);

    return bme->id;
}

static void bme280MeasureTimer(TimerHandle_t xTimer)
{
    struct Sensor *sensor = pvTimerGetTimerID(xTimer);
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
    updateForHundredth(temperature, Notifications_Class_Temperature, sensor,
                       bme280p ? HUMIDITY_PUB_INDEX_TEMPERTURE: TEMPERATURE_PUB_INDEX_TEMPERATURE);

    if (bme280p) {
        updateForHundredth((humidity * 100) / 1024, Notifications_Class_Humidity, sensor, HUMIDITY_PUB_INDEX_HUMIDITY);
    }

    updateForHundredth(pressure / 256, Notifications_Class_Pressure, sensor,
                       bme280p ? HUMIDITY_PUB_INDEX_PRESSURE: TEMPERATURE_PUB_INDEX_PRESSURE);
}
#endif

#ifdef CONFIG_SI7021
int sensorsSI7021Init(int nrofSensors)
{
    si7021Devices = calloc(nrofSensors, sizeof(struct SI7021));
    if (si7021Devices == NULL) {
        return -1;
    }
    nrofHumiditySensors += nrofSensors;

    return 0;
}

int sensorsSI7021Add(DeviceProfile_Si7021Config_t *config)
{
    struct Sensor *sensor;
    iotValue_t value;
    uint32_t sensorId;
    struct SI7021 *dev;
    esp_err_t err;

    if (addHumiditySensor(&sensor, &sensorId)) {
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
    iotElementPublish(sensor->element, HUMIDITY_PUB_INDEX_TEMPERTURE, value);
    sensor->details.dev = dev;
    xTimerStart(xTimerCreate("si7021", SECS_TO_TICKS(5), pdTRUE, sensor, si7021MeasureTimer), 0);
    return sensor->id;
}

static void si7021MeasureTimer(TimerHandle_t xTimer)
{
    struct Sensor *sensor = pvTimerGetTimerID(xTimer);
    struct SI7021 *dev = sensor->details.dev;
    esp_err_t err;
    float temperature, humidity;

    err = si7021_measure_temperature(&dev->dev, &temperature);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read temperature %d", err);
        return;
    }

    updateForHundredth((int32_t) (temperature * 100.0), Notifications_Class_Temperature, sensor, HUMIDITY_PUB_INDEX_TEMPERTURE);

    err = si7021_measure_humidity(&dev->dev, &humidity);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "si7021MeasureTimer: Failed to read humidity %d", err);
        return;
    }
    updateForHundredth((int32_t) (humidity * 100.0), Notifications_Class_Humidity, sensor, HUMIDITY_PUB_INDEX_HUMIDITY);
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
    Notifications_ID_t notificationId = NOTIFICATIONS_ID_ALL;
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
    return notificationId;
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
            int itemp = (int)(temp * 100.0);
            struct Sensor sensor;
            sensor.id = pinStruct->sensors[i].id;
            sensor.element = pinStruct->sensors[i].element;
            updateForHundredth(itemp, Notifications_Class_Temperature, &sensor, TEMPERATURE_PUB_INDEX_TEMPERATURE);
        }
    }

    xTimerStart(pinStruct->measureTimer, 0);
}
#endif

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021)
static int addHumiditySensor(struct Sensor **sensor, uint32_t *sensorId)
{
    struct Sensor *newSensor;
    if (humiditySensors == NULL) {
        humiditySensors = calloc(sizeof(struct Sensor), nrofHumiditySensors);
        if (humiditySensors == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for humidity sensors");
            return -1;
        }
    }
    if (humiditySensorCount >= nrofHumiditySensors) {
        ESP_LOGE(TAG, "All humidity sensors already allocated!");
        return -1;
    }
    newSensor = &humiditySensors[humiditySensorCount];
    newSensor->id = NOTIFICATIONS_ID_ERROR;
    *sensor = newSensor;
    *sensorId = humiditySensorCount;
    humiditySensorCount ++;
    return 0;
}
#endif

#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021) || defined(CONFIG_DS18x20)
static void updateForHundredth(int hundredths, Notifications_Class_e clazz, struct Sensor *sensor, int index)
{
    NotificationsData_t data;
    iotValue_t value;

    data.temperature = hundredths;
    ESP_LOGI(TAG, "updateForHundredth: Class %d Id %d: Index: %d Value %d", clazz, sensor->id, index, hundredths);
    value.i = hundredths;
    iotElementPublish(sensor->element, index, value);
    if (sensor->id != NOTIFICATIONS_ID_ERROR) {
        notificationsNotify(clazz, sensor->id, &data);
    }
}
#endif

void sensorsInit(DeviceProfile_DeviceConfig_t *config)
{
    ESP_LOGI(TAG, "Initialising sensors");
#ifdef CONFIG_DHT22
    sensorsDHT22Init(config->dht22Count);
#endif
#ifdef CONFIG_BME280
    sensorsBME280Init(config->bme280Count);
#endif
#ifdef CONFIG_SI7021
    sensorsSI7021Init(config->si7021Count);
#endif
#ifdef CONFIG_DS18x20
    sensorsDS18x20Init(config->ds18x20Count);
#endif

#ifdef CONFIG_DHT22
    {
        int i;
        for (i = 0; i < config->dht22Count; i ++) {
            sensorsDHT22Add(&config->dht22Config[i]);
        }
    }
#endif
#ifdef CONFIG_BME280
    {
        int i;
        for (i = 0; i < config->bme280Count; i ++) {
            sensorsBME280Add(&config->bme280Config[i]);
        }
    }
#endif
#ifdef CONFIG_SI7021
    {
        int i;
        for (i = 0; i < config->si7021Count; i ++) {
            sensorsSI7021Add(&config->si7021Config[i]);
        }
    }
#endif
#ifdef CONFIG_DS18x20
    {
        int i;
        for (i = 0; i < config->ds18x20Count; i ++) {
            sensorsDS18x20Add(&config->ds18x20Config[i]);
        }
    }
#endif
}