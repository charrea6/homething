#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "humidityfan.h"


#define SECS_TO_TICKS(secs) ((secs * 1000) / portTICK_RATE_MS)

#define THRESHOLD_CMD "threshold "
#define THRESHOLD_CMD_LEN (sizeof(THRESHOLD_CMD) - 1)

#define MANUAL_CMD "manual "
#define MANUAL_CMD_LEN (sizeof(MANUAL_CMD) - 1)

#define AUTO_CMD "auto"

static void humidityFanCtrl(void *userData, iotElement_t *element, iotValue_t value);
static void humidityFanRunOnTimeout(TimerHandle_t xTimer);
static void humidityOverThresholdTimeout(TimerHandle_t xTimer);
static void humidityManulModeSecsTimeout(TimerHandle_t xTimer);

static const char TAG[] = "HFAN";
static int fanCount=0;

#define PUB_ID_STATE       0
#define PUB_ID_HUMIDITY    1
#define PUB_ID_THRESHOLD   2
#define PUB_ID_MANUAL      3
#define PUB_ID_MANUAL_SECS 4

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BOOL, "state"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, "humidity"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "threshold"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BOOL, "manual"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "manualSecs"),
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(IOT_VALUE_TYPE_STRING, IOT_SUB_DEFAULT_NAME, (iotElementSubUpdateCallback_t)humidityFanCtrl)
    )
);

void humidityFanInit(HumidityFan_t *fan, int relayPin, int threshold)
{
    iotValue_t value;
    fan->id = fanCount;
    fan->lastHumidity = -1;
    fan->override = false;
    fan->threshold = threshold;
    fan->runOnSeconds = 30;
    fan->overThresholdSeconds =  10;
    fan->manualMode = false;
    fan->manualModeSecsLeft = 0u;

    sprintf(fan->humidity, "0.0");

    relayInit(relayPin, &fan->relay);
    fan->element = iotNewElement(&elementDescription, fan, "fan%d", fanCount);
    fanCount ++;

    value.i = fan->threshold;
    iotElementPublish(fan->element, PUB_ID_THRESHOLD, value);
    value.s = fan->humidity;
    iotElementPublish(fan->element, PUB_ID_HUMIDITY, value);
    
    fan->runOnTimer = xTimerCreate("fRO", SECS_TO_TICKS(fan->runOnSeconds), pdFALSE, fan, humidityFanRunOnTimeout);
    fan->overThresholdTimer = xTimerCreate("fOT", SECS_TO_TICKS(fan->overThresholdSeconds), pdFALSE, fan, humidityOverThresholdTimeout);
    fan->manualModeTimer = xTimerCreate("fMM", SECS_TO_TICKS(1), pdTRUE, fan, humidityManulModeSecsTimeout);
}

static void humidityFanSetState(HumidityFan_t *fan, bool state)
{
    iotValue_t value;
    RelayState_t realState = relayGetState(&fan->relay);
    RelayState_t newState = state?RelayState_On:RelayState_Off;
    if (newState != realState)
    {
        relaySetState(&fan->relay, newState);
        value.b = state;
        iotElementPublish(fan->element, PUB_ID_STATE, value);
    }
    if (state)
    {
        if (xTimerIsTimerActive(fan->overThresholdTimer) == pdTRUE)
        {
            xTimerStop(fan->overThresholdTimer, 0);
        }
        if (xTimerIsTimerActive(fan->runOnTimer) == pdTRUE)
        {
            xTimerStop(fan->runOnTimer, 0);
        }
    }
}

void humidityFanUpdateHumidity(HumidityFan_t *fan, int humidityTenths)
{
    iotValue_t value;
    RelayState_t state = relayGetState(&fan->relay);

    ESP_LOGI(TAG, "%d: Humidity %d (Threshold %d) Manual Mode %d", fan->id, (humidityTenths / 10), fan->threshold, fan->manualMode);
    fan->lastHumidity = humidityTenths;
    if (fan->manualMode == false)
    {
        if ((humidityTenths / 10) >= fan->threshold)
        {
            if (!fan->override && (state == RelayState_Off))
            {
                if (xTimerIsTimerActive(fan->overThresholdTimer) == pdFALSE)
                {
                    ESP_LOGI(TAG, "%d: Start over threshold timer", fan->id);
                    xTimerStart(fan->overThresholdTimer, 0);
                }
            }
        }
        else
        {
            if (state == RelayState_On)
            {
                if (xTimerIsTimerActive(fan->runOnTimer) == pdFALSE)
                {
                    ESP_LOGI(TAG, "%d: Start run on timer", fan->id);
                    xTimerStart(fan->runOnTimer, 0);
                }
            }
            else
            {
                fan->override = false;
            }
            if (xTimerIsTimerActive(fan->overThresholdTimer) == pdTRUE)
            {
                ESP_LOGI(TAG, "%d: Stop over threshold timer", fan->id);
                xTimerStop(fan->overThresholdTimer, 0);
            }
        }
    }
    sprintf(fan->humidity, "%d.%d", humidityTenths / 10, humidityTenths % 10);
    value.s = fan->humidity;
    iotElementPublish(fan->element, PUB_ID_HUMIDITY, value);
}

static void humidityFanCtrl(void *userData, iotElement_t *element, iotValue_t value)
{
    HumidityFan_t *fan = userData;
    bool on;
    if (!iotStrToBool(value.s, &on))
    {
        if (!on)
        {
            if (relayGetState(&fan->relay) == RelayState_On)
            {
                fan->override = true;
            }    
        }

        humidityFanSetState(fan, on);
    }
    else if (strncmp(THRESHOLD_CMD, value.s, THRESHOLD_CMD_LEN) == 0)
    {
        int threshold;
        if (sscanf(value.s + THRESHOLD_CMD_LEN, "%d", &threshold) == 1)
        {
            if (threshold <= 100)
            {
                fan->threshold = threshold;
                iotElementPublish(fan->element, PUB_ID_THRESHOLD, value);
            }
        }
    }
    else if (strncmp(MANUAL_CMD, value.s, MANUAL_CMD_LEN) == 0)
    {
        uint32_t secs;
        if (sscanf(value.s + MANUAL_CMD_LEN, "%u", &secs) == 1)
        {
            iotValue_t value;
            xTimerReset(fan->manualModeTimer, 0);
            fan->manualMode = true;
            fan->manualModeSecsLeft = secs;
            value.b = true;
            iotElementPublish(fan->element, PUB_ID_MANUAL, value);
            value.i = secs;
            iotElementPublish(fan->element, PUB_ID_MANUAL_SECS, value);
        }
    }
    else if (strcmp(AUTO_CMD, value.s) == 0)
    {
        if (fan->manualMode)
        {
            fan->manualMode = false;
            xTimerStop(fan->manualModeTimer, 0);
            humidityFanUpdateHumidity(fan, fan->lastHumidity);
            value.b = false;
            iotElementPublish(fan->element, PUB_ID_MANUAL, value);
        }
    }
}

static void humidityFanRunOnTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    ESP_LOGI(TAG, "%d: Run on timer fired", fan->id);
    fan->override = false;
    humidityFanSetState(fan, false);
}

static void humidityOverThresholdTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    ESP_LOGI(TAG, "%d: Over threshold timer fired", fan->id);
    humidityFanSetState(fan, true);
}

static void humidityManulModeSecsTimeout(TimerHandle_t xTimer)
{
    iotValue_t value;
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    fan->manualModeSecsLeft --;
    value.i = fan->manualModeSecsLeft;
    iotElementPublish(fan->element, PUB_ID_MANUAL_SECS, value);

    ESP_LOGI(TAG, "%d: Manual mode seconds left %d", fan->id, fan->manualModeSecsLeft);
    if (fan->manualModeSecsLeft == 0)
    {
        fan->manualMode = false;
        value.b = false;
        iotElementPublish(fan->element, PUB_ID_MANUAL, value);
        xTimerStop(xTimer, 0);
        humidityFanUpdateHumidity(fan, fan->lastHumidity);
    }
}