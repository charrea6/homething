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
#include "tsl2561.h"
#include "sensors.h"
#include "sensorsInternal.h"

struct TSL2561 {
    tsl2561_t dev;
};


#if defined(CONFIG_TSL2561)
static void tsl2561MeasureTimer(Sensor_t *sensor);

IOT_DESCRIBE_ELEMENT_NO_SUBS(
    lightElementDescription,
    IOT_ELEMENT_TYPE_SENSOR_LIGHT,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, LUX, IOT_PUB_USE_ELEMENT)
    )
);

static char const TAG[]="sensorsLight";


static uint32_t nrofTsl2561Sensors = 0;
static struct TSL2561 *tsl2561Devices;


int sensorsTSL2561Init(int nrofSensors)
{
    tsl2561Devices = calloc(nrofSensors, sizeof(struct TSL2561));
    if (tsl2561Devices == NULL) {
        return -1;
    }
    return nrofSensors;
}

int sensorsTSL2561Add(DeviceProfile_Tsl2561Config_t *config)
{
    Sensor_t *sensor;
    struct TSL2561 *tsl = &tsl2561Devices[nrofTsl2561Sensors];
    iotValue_t value;
    uint32_t sensorId = nrofTsl2561Sensors++;
    esp_err_t err;

    err = tsl2561_init_desc(&tsl->dev, config->addr, 0, config->sda, config->scl);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addTSL2561: Failed to init desc %d", err);
        return -1;
    }
    err = tsl2561_init(&tsl->dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "addTSL2561: Failed to init %d", err);
        return -1;
    }

    if (sensorsAddSensor(&sensor) != 0) {
        return -1;
    }

    sensor->element = iotNewElement(&lightElementDescription, 0, NULL, sensor, "illuminance%d", sensorId);
    if (config->id) {
        sensor->id = notificationsNewId(config->id);
    }
    if (config->name) {
        iotElementSetHumanDescription(sensor->element, config->name);
    }

    value.i = 0;
    iotElementPublish(sensor->element, 0, value);
    sensor->details.dev = tsl;
    sensorsCreateSecondsTimer(sensor, "tsl2561", 5, tsl2561MeasureTimer);

    return 0;
}

static void tsl2561MeasureTimer(Sensor_t *sensor)
{
    struct TSL2561 *tsl = sensor->details.dev;
    uint32_t lux;
    iotValue_t value;
    esp_err_t err;
    err = tsl2561_read_lux(&tsl->dev, &lux);
    ESP_LOGI(TAG, "tsl2561MeasureTimer: Lux %d", lux);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tsl2561MeasureTimer: Failed to read lux %d", err);
        return;
    }
    value.i = lux;
    iotElementPublish(sensor->element, 0, value);
}
#endif