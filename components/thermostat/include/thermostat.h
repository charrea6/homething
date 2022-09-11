#ifndef _THERMOSTAT_H_
#define _THERMOSTAT_H_
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "iot.h"
#include "notifications.h"
#include "relay.h"

typedef void (*ThermostatCallForHeatStateSet_t)(void *, bool);
typedef bool (*ThermostatCallForHeatStateGet_t)(void *);

typedef struct Thermostat {
    int id;
    Relay_t *relay;

    int targetTemperature;

    iotElement_t element;
    char modeState[44]; // {"mode":"manual","secondsLeft":1234567890}

    uint32_t manualModeSecsLeft;
    bool manualMode;
    TimerHandle_t manualModeTimer;
    int lastTemperature;
} Thermostat_t;

void thermostatInit(Thermostat_t *thermostat, Relay_t *relay,
                    Notifications_ID_t temperatureSensor);

#endif