#ifndef _DHT_H_
#define _DHT_H_

#include <stdint.h>

#define DHT22_MAX_CALLBACKS 2

typedef void (*DHT22CallBack_t)(void *userData, int16_t tenthsUnit);

typedef struct DHT22CBEntry {
    DHT22CallBack_t cb;
    void *userData;
} DHT22CBEntry_t;

typedef struct {
    int8_t pin;
    int8_t nrofTemperatureCBs;
    int8_t nrofHumidityCBs;
    DHT22CBEntry_t temperatureCBs[DHT22_MAX_CALLBACKS];
    DHT22CBEntry_t humidityCBs[DHT22_MAX_CALLBACKS];
}DHT22Sensor_t;

void dht22Init(DHT22Sensor_t *sensor, int8_t pin);
void dht22AddHumidityCallback(DHT22Sensor_t *sensor, DHT22CallBack_t cb, void *userData);
void dht22AddTemperatureCallback(DHT22Sensor_t *sensor, DHT22CallBack_t cb, void *userData);
void dht22Start(DHT22Sensor_t *sensor);
#endif
