#ifndef _HUMIDITYFAN_H_
#define _HUMIDITYFAN_H_
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "iot.h"
#include "relay.h"

typedef struct HumidityFan{
    int id;
    char humidity[6];
    bool override;
    Relay_t relay;
    iotElement_t element;
    int lastHumidity;
    int threshold;
    uint32_t runOnSeconds;
    uint32_t overThresholdSeconds;
    uint32_t manualModeSecsLeft;
    bool manualMode;
    TimerHandle_t runOnTimer;
    TimerHandle_t overThresholdTimer;
    TimerHandle_t manualModeTimer;
}HumidityFan_t;

void humidityFanInit(HumidityFan_t *fan, int relayPin, int threshold);

void humidityFanUpdateHumidity(HumidityFan_t *fan, int humidityTenths);

#endif