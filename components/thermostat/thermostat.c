#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "thermostat.h"


#define SECS_TO_TICKS(secs) ((secs * 1000) / portTICK_RATE_MS)

#define TARGET_CMD "target "
#define TARGET_CMD_LEN (sizeof(TARGET_CMD) - 1)

#define MANUAL_CMD "manual "
#define MANUAL_CMD_LEN (sizeof(MANUAL_CMD) - 1)

#define AUTO_CMD "auto"

static void thermostatElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason,
                                      iotElementCallbackDetails_t *details);
static void thermostatCtrl(Thermostat_t *fan, iotValue_t value);
static void thermostatManualModeSecsTimeout(TimerHandle_t xTimer);
static void thermostatManualModeDisable(Thermostat_t *thermostat);
static void thermostatReevaluateState(Thermostat_t *thermostat);
static void thermostatUpdateTemperature(Thermostat_t *thermostat, NotificationsMessage_t *message);
static void thermostatUpdateMode(Thermostat_t *thermostat);
static void thermostatUpdateTargetTemps(Thermostat_t *thermostat);

static const char TAG[] = "thermostat";
static int thermostatCount=0;

#define TARGET_HYSTERESIS        50 // 0.5 C
#define TEMPERATURE_RANGE_LOW     0 // 0.0 C
#define TEMPERATURE_RANGE_HIGH 3500 // 35.0 C

#define PUB_ID_STATE       0
#define PUB_ID_TEMPERATURE 1
#define PUB_ID_HEAT_ON      2
#define PUB_ID_HEAT_OFF     3
#define PUB_ID_MODE        4

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, BOOL, "state"),
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, "temperature"),
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, "heatOn"),
        IOT_DESCRIBE_PUB(RETAINED, CELSIUS, "heatOff"),
        IOT_DESCRIBE_PUB(RETAINED, STRING, "mode")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(STRING, IOT_SUB_DEFAULT_NAME)
    )
);

void thermostatInit(Thermostat_t *thermostat, ThermostatCallForHeatStateSet_t setState,
                    ThermostatCallForHeatStateGet_t getState, void *context,
                    Notifications_ID_t temperatureSensor)
{
    iotValue_t value;
    thermostat->heatOn = 1950; // Default 19.50 C
    thermostat->heatOff = 2050; // Default 20.50 C
    thermostat->manualMode = false;
    thermostat->manualModeSecsLeft = 0u;
    thermostat->context = context;
    thermostat->getState = getState;
    thermostat->setState = setState;

    thermostat->element = iotNewElement(&elementDescription, 0, thermostatElementCallback, thermostat, "thermostat%d", thermostatCount);
    thermostatCount ++;

    value.i = thermostat->heatOn;
    iotElementPublish(thermostat->element, PUB_ID_HEAT_ON, value);
    value.i = thermostat->heatOff;
    iotElementPublish(thermostat->element, PUB_ID_HEAT_OFF, value);
    value.i = 0;
    iotElementPublish(thermostat->element, PUB_ID_TEMPERATURE, value);
    thermostatUpdateMode(thermostat);

    thermostat->manualModeTimer = xTimerCreate("tMM", SECS_TO_TICKS(1), pdTRUE, thermostat, thermostatManualModeSecsTimeout);
    notificationsRegister(Notifications_Class_Temperature, temperatureSensor, (NotificationsCallback_t)thermostatUpdateTemperature, thermostat);
}

static void thermostatSetState(Thermostat_t *thermostat, bool state)
{
    iotValue_t value;
    thermostat->setState(thermostat->context, state);
    value.b = state;
    iotElementPublish(thermostat->element, PUB_ID_STATE, value);
}

static void thermostatUpdateTemperature(Thermostat_t *thermostat, NotificationsMessage_t *message)
{
    iotValue_t value;
    int32_t celsiusTenths = message->data.temperature;

    ESP_LOGI(TAG, "%s: Temperature %d (Heat On %d Heat Off %d) Manual Mode %d", iotElementGetName(thermostat->element),
             celsiusTenths, thermostat->heatOn, thermostat->heatOff, thermostat->manualMode);

    if (thermostat->manualMode == false) {
        if (thermostat->getState(thermostat->context)) {
            // Call for Heat is on, so wait until we're greater than or equal to heatOff
            if (celsiusTenths >= thermostat->heatOff) {
                thermostatSetState(thermostat, false);
            }

        } else {
            // Call for Heat is off, so wait until we're below target by 0.5 C before turning on
            if (celsiusTenths <= thermostat->heatOn) {
                thermostatSetState(thermostat, true);
            }
        }
    }
    value.i = celsiusTenths;
    iotElementPublish(thermostat->element, PUB_ID_TEMPERATURE, value);
    thermostat->lastTemperature = celsiusTenths;
}

static void thermostatElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason,
                                      iotElementCallbackDetails_t *details)
{
    if (reason == IOT_CALLBACK_ON_SUB) {
        thermostatCtrl((Thermostat_t *)userData, details->value);
    }
}

static void thermostatCtrl(Thermostat_t *thermostat, iotValue_t value)
{
    bool on;
    if (!iotStrToBool(value.s, &on)) {
        thermostatSetState(thermostat, on);
    } else if (strncmp(TARGET_CMD, value.s, TARGET_CMD_LEN) == 0) {
        iotValue_t targetTemp;
        if (iotParseString(value.s + TARGET_CMD_LEN, IOT_VALUE_TYPE_CELSIUS, &targetTemp) == 0) {
            if ((targetTemp.i >= TEMPERATURE_RANGE_LOW) && (targetTemp.i <= TEMPERATURE_RANGE_HIGH)) {
                thermostat->heatOn = targetTemp.i - TARGET_HYSTERESIS;
                thermostat->heatOff = targetTemp.i + TARGET_HYSTERESIS;
                thermostatUpdateTargetTemps(thermostat);
                thermostatReevaluateState(thermostat);
            }
        }
    } else if (strncmp(MANUAL_CMD, value.s, MANUAL_CMD_LEN) == 0) {
        uint32_t secs;
        if (sscanf(value.s + MANUAL_CMD_LEN, "%u", &secs) == 1) {
            xTimerReset(thermostat->manualModeTimer, 0);
            thermostat->manualMode = true;
            thermostat->manualModeSecsLeft = secs;
            thermostatUpdateMode(thermostat);
        }
    } else if (strcmp(AUTO_CMD, value.s) == 0) {
        if (thermostat->manualMode) {
            thermostatManualModeDisable(thermostat);
        }
    }
}

static void thermostatManualModeSecsTimeout(TimerHandle_t xTimer)
{
    Thermostat_t *thermostat = pvTimerGetTimerID(xTimer);
    thermostat->manualModeSecsLeft --;
    thermostatUpdateMode(thermostat);

    ESP_LOGI(TAG, "%s: Manual mode seconds left %d", iotElementGetName(thermostat->element), thermostat->manualModeSecsLeft);
    if (thermostat->manualModeSecsLeft == 0) {
        thermostatManualModeDisable(thermostat);
    }
}

static void thermostatManualModeDisable(Thermostat_t *thermostat)
{
    thermostat->manualMode = false;
    thermostatUpdateMode(thermostat);
    thermostatReevaluateState(thermostat);
}

static void thermostatReevaluateState(Thermostat_t *thermostat)
{
    NotificationsMessage_t message;
    message.data.temperature = thermostat->lastTemperature;
    thermostatUpdateTemperature(thermostat, &message);
}

static void thermostatUpdateMode(Thermostat_t *thermostat)
{
    iotValue_t value;
    sprintf(thermostat->modeState, "{\"mode\":\"%s\",\"secondsLeft\":%u}",
            thermostat->manualMode ? "manual":"auto", thermostat->manualModeSecsLeft);
    value.s = thermostat->modeState;
    iotElementPublish(thermostat->element, PUB_ID_MODE, value);
}

static void thermostatUpdateTargetTemps(Thermostat_t *thermostat)
{
    iotValue_t value;
    value.i = thermostat->heatOn;
    iotElementPublish(thermostat->element, PUB_ID_HEAT_ON, value);
    value.i = thermostat->heatOff;
    iotElementPublish(thermostat->element, PUB_ID_HEAT_OFF, value);
}