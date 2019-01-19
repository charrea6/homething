#ifndef _HUMIDITYFAN_H_
#define _HUMIDITYFAN_H_
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "iot.h"
#include "relay.h"

typedef struct HumidityFan{
    char name[5];
    char humidity[6];
    bool override;
    Relay_t relay;
    iotElement_t element;
    iotElementPub_t statePub;
    iotElementPub_t thresholdPub;
    iotElementPub_t humidityPub;
    iotElementSub_t ctrl;
    iotElementSub_t thresholdSub;
    int threshold;
    int runOnSeconds;
    TimerHandle_t runOnTimer;
}HumidityFan_t;

void humidityFanInit(HumidityFan_t *fan, int relayPin, int threshold);

void humidityFanUpdateHumidity(HumidityFan_t *fan, int humidityTenths);

#endif