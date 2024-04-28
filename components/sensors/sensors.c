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
#include "sensors.h"
#include "sensorsInternal.h"

typedef int (*SensorInit_t)(int nrofSensors);
typedef int (*SensorAdd_t)(void *config);
struct SensorDef {
    const char *name;
    off_t count;
    off_t elements;
    size_t elementSize;
    SensorInit_t init;
    SensorAdd_t add;
};

static void sensorsTimerHandler(TimerHandle_t xTimer);

static char const TAG[]="sensors";

struct SensorDef sensorDefs[] = {
#ifdef CONFIG_DHT22
    {
        .name = "DHT22",
        .count = offsetof(DeviceProfile_DeviceConfig_t, dht22Count),
        .elements = offsetof(DeviceProfile_DeviceConfig_t, dht22Config),
        .elementSize = sizeof(DeviceProfile_Dht22Config_t),
        .init = sensorsDHT22Init,
        .add = (SensorAdd_t)sensorsDHT22Add
    },
#endif
#ifdef CONFIG_BME280
    {
        .name = "BME280",
        .count = offsetof(DeviceProfile_DeviceConfig_t, bme280Count),
        .elements = offsetof(DeviceProfile_DeviceConfig_t, bme280Config),
        .elementSize = sizeof(DeviceProfile_Bme280Config_t),
        .init = sensorsBME280Init,
        .add = (SensorAdd_t)sensorsBME280Add
    },
#endif
#ifdef CONFIG_SI7021
    {
        .name = "SI7021",
        .count = offsetof(DeviceProfile_DeviceConfig_t, si7021Count),
        .elements = offsetof(DeviceProfile_DeviceConfig_t, si7021Config),
        .elementSize = sizeof(DeviceProfile_Si7021Config_t),
        .init = sensorsSI7021Init,
        .add = (SensorAdd_t)sensorsSI7021Add
    },
#endif
#ifdef CONFIG_DS18x20
    {
        .name = "DS18x20",
        .count = offsetof(DeviceProfile_DeviceConfig_t, ds18x20Count),
        .elements = offsetof(DeviceProfile_DeviceConfig_t, ds18x20Config),
        .elementSize = sizeof(DeviceProfile_Ds18x20Config_t),
        .init = sensorsDS18x20Init,
        .add = (SensorAdd_t)sensorsDS18x20Add
    },
#endif
#ifdef CONFIG_TSL2561
    {
        .name = "TSL2561",
        .count = offsetof(DeviceProfile_DeviceConfig_t, tsl2561Count),
        .elements = offsetof(DeviceProfile_DeviceConfig_t, tsl2561Config),
        .elementSize = sizeof(DeviceProfile_Tsl2561Config_t),
        .init = sensorsTSL2561Init,
        .add = (SensorAdd_t)sensorsTSL2561Add
    },
#endif
};


#if defined(CONFIG_DHT22) || defined(CONFIG_BME280) || defined(CONFIG_SI7021) || defined(CONFIG_TSL2561)
static uint32_t sensorsTotal = 0;
static uint32_t sensorsCount = 0;

static struct Sensor *sensors = NULL;
#endif

int sensorsAddSensor(struct Sensor **sensor)
{
    struct Sensor *newSensor;
    if (sensors == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for sensors");
        return -1;
    }

    if (sensorsCount >= sensorsTotal) {
        ESP_LOGE(TAG, "All sensors already allocated!");
        return -1;
    }
    newSensor = &sensors[sensorsCount];
    newSensor->id = NOTIFICATIONS_ID_ERROR;
    *sensor = newSensor;
    sensorsCount ++;
    return 0;
}

void sensorsCreateSecondsTimer(Sensor_t *sensor, const char *name, uint32_t seconds, SensorTimerCallback_t callback)
{
    sensor->callback = callback;
    xTimerStart(xTimerCreate(name, SECS_TO_TICKS(seconds), pdTRUE, sensor, sensorsTimerHandler), 0);
}

static void sensorsTimerHandler(TimerHandle_t xTimer)
{
    Sensor_t *sensor = pvTimerGetTimerID(xTimer);
    sensor->callback(sensor);
}

void sensorsUpdateForHundredth(Sensor_t *sensor, int index, Notifications_Class_e clazz, int hundredths)
{
    NotificationsData_t data;
    iotValue_t value;

    data.temperature = hundredths;
    ESP_LOGI(TAG, "sensorsUpdateForHundredth: Class %d Id %d: Index: %d Value %d", clazz, sensor->id, index, hundredths);
    value.i = hundredths;
    iotElementPublish(sensor->element, index, value);
    if (sensor->id != NOTIFICATIONS_ID_ERROR) {
        notificationsNotify(clazz, sensor->id, &data);
    }
}

void sensorsInit(DeviceProfile_DeviceConfig_t *config)
{
    ESP_LOGI(TAG, "Initialising sensors");
    int i;
    for (i = 0; i < sizeof(sensorDefs) / sizeof(struct SensorDef); i++) {
        struct SensorDef *def = &sensorDefs[i];
        uint32_t nrofSensors = *((uint32_t *)((void *)config + def->count));
        if (nrofSensors == 0) {
            continue;
        }
        ESP_LOGI(TAG, "Initialising %s sensors (%d defined)", def->name, nrofSensors);
        int result = def->init(nrofSensors);
        if (result >= 0) {
            sensorsTotal += nrofSensors;
        } else {
            ESP_LOGE(TAG, "Failed to initialise %s sensors", def->name);
        }
    }

    if (sensorsTotal == 0) {
        sensors = NULL;
    } else {
        sensors = calloc(sizeof(struct Sensor), sensorsTotal);
        if (sensors == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for humidity sensors");
            return;
        }
    }

    for (i = 0; i < sizeof(sensorDefs) / sizeof(struct SensorDef); i++) {
        struct SensorDef *def = &sensorDefs[i];
        uint32_t nrofSensors = *((uint32_t *)((void *)config + def->count));
        uint32_t j;
        for (j = 0; j < nrofSensors; j++) {
            void *sensorConfig = *(void **)((void *)config + def->elements);
            def->add(sensorConfig + (j * def->elementSize));
        }
    }
}