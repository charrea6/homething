#ifndef _THERMOSTAT_H_
#define _THERMOSTAT_H_
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "iot.h"
#include "notifications.h"

typedef void (*ThermostatCallForHeatStateSet_t)(void *, bool);
typedef bool (*ThermostatCallForHeatStateGet_t)(void *);

typedef struct Thermostat {
    int id;
    ThermostatCallForHeatStateSet_t setState;
    ThermostatCallForHeatStateGet_t getState;
    void *context;

    int targetTemperature;

    iotElement_t element;
    char modeState[44]; // {"mode":"manual","secondsLeft":1234567890}

    uint32_t manualModeSecsLeft;
    bool manualMode;
    TimerHandle_t manualModeTimer;
    int lastTemperature;
} Thermostat_t;

void thermostatInit(Thermostat_t *thermostat, ThermostatCallForHeatStateSet_t setState,
                    ThermostatCallForHeatStateGet_t getState, void *context,
                    Notifications_ID_t temperatureSensor);

#endif