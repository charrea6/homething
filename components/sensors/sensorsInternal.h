#ifndef __SENSORS_INTERNAL_H__
#define __SENSORS_INTERNAL_H__
#include <stdint.h>
#include "iot.h"
#include "notifications.h"

#define MSECS_TO_TICKS(msecs) ((msecs) / portTICK_RATE_MS)
#define SECS_TO_TICKS(secs) MSECS_TO_TICKS(secs * 1000)

typedef struct Sensor Sensor_t;
typedef void (*SensorTimerCallback_t )(struct Sensor *sensor);

struct Sensor {
    Notifications_ID_t id;
    iotElement_t element;
    union {
        uint8_t pin;
        void *dev;
    } details;
    SensorTimerCallback_t callback;
};




int sensorsAddSensor(struct Sensor **sensor);
void sensorsCreateSecondsTimer(Sensor_t *sensor, const char *name, uint32_t seconds, SensorTimerCallback_t callback);
void sensorsUpdateForHundredth(Sensor_t *sensor,int index, Notifications_Class_e clazz, int hundredths);

#ifdef CONFIG_DHT22
int sensorsDHT22Init(int nrofSensors);
int sensorsDHT22Add(DeviceProfile_Dht22Config_t *config);
#endif

#ifdef CONFIG_BME280
int sensorsBME280Init(int nrofSensors);
int sensorsBME280Add(DeviceProfile_Bme280Config_t *config);
#endif

#ifdef CONFIG_SI7021
int sensorsSI7021Init(int nrofSensors);
int sensorsSI7021Add(DeviceProfile_Si7021Config_t *config);
#endif

#ifdef CONFIG_DS18x20
int sensorsDS18x20Init(int nrofSensors);
int sensorsDS18x20Add(DeviceProfile_Ds18x20Config_t *config);
#endif

#ifdef CONFIG_TSL2561
int sensorsTSL2561Init(int nrofSensors);
int sensorsTSL2561Add(DeviceProfile_Tsl2561Config_t *config);
#endif
#endif