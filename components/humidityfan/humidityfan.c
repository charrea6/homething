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

static void humidityFanCtrl(void *userData, iotElementSub_t *sub, iotValue_t value);
static void humidityFanRunOnTimeout(TimerHandle_t xTimer);
static void humidityOverThresholdTimeout(TimerHandle_t xTimer);
static void humidityManulModeSecsTimeout(TimerHandle_t xTimer);

static const char TAG[] = "HFAN";
static int fanCount=0;

void humidityFanInit(HumidityFan_t *fan, int relayPin, int threshold)
{
    iotElementPub_t *pub;
    iotElementSub_t *sub;
    sprintf(fan->name, "fan%d", fanCount);
    fanCount ++;

    fan->lastHumidity = -1;
    fan->override = false;
    fan->threshold = threshold;
    fan->runOnSeconds = 30;
    fan->overThresholdSeconds =  10;
    sprintf(fan->humidity, "0.0");

    relayInit(relayPin, &fan->relay);
    
    fan->element.name = fan->name;
    iotElementAdd(&fan->element);
    pub = &fan->statePub;
    pub->name = "state";
    pub->type = iotValueType_Bool;
    pub->retain = true;
    pub->value.b = false;
    iotElementPubAdd(&fan->element, pub);
    pub = &fan->humidityPub;
    pub->name = "humidity";
    pub->type = iotValueType_String;
    pub->retain = false;
    pub->value.s = fan->humidity;
    iotElementPubAdd(&fan->element,pub);
    pub = &fan->thresholdPub;
    pub->name = "threshold";
    pub->type = iotValueType_Int;
    pub->retain = true;
    pub->value.i = fan->threshold;
    iotElementPubAdd(&fan->element, pub);
    sub = &fan->ctrl;
    sub->name = IOT_DEFAULT_CONTROL;
    sub->type = iotValueType_String;
    sub->callback = humidityFanCtrl;
    sub->userData = fan;
    iotElementSubAdd(&fan->element,sub);
    
    fan->manualMode = false;
    fan->manualModeSecsLeft = 0u;

    fan->runOnTimer = xTimerCreate(fan->name, SECS_TO_TICKS(fan->runOnSeconds), pdFALSE, fan, humidityFanRunOnTimeout);
    fan->overThresholdTimer = xTimerCreate(fan->name, SECS_TO_TICKS(fan->overThresholdSeconds), pdFALSE, fan, humidityOverThresholdTimeout);
    fan->manualModeTimer = xTimerCreate(fan->name, SECS_TO_TICKS(1), pdTRUE, fan, humidityManulModeSecsTimeout);
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
        iotElementPubUpdate(&fan->statePub, value);
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
    ESP_LOGI(TAG, "%s: Humidity %d (Threshold %d)", fan->name, (humidityTenths / 10), fan->threshold);
    fan->lastHumidity = humidityTenths;
    if (fan->manualMode == false)
    {
        if ((humidityTenths / 10) >= fan->threshold)
        {
            if (!fan->override)
            {
                if (xTimerIsTimerActive(fan->overThresholdTimer) == pdFALSE)
                {
                    xTimerStart(fan->overThresholdTimer, 0);
                }
            }
        }
        else
        {
            RelayState_t state = relayGetState(&fan->relay);
            if (state == RelayState_On)
            {
                if (xTimerIsTimerActive(fan->runOnTimer) == pdFALSE)
                {
                    ESP_LOGI(TAG, "%s: Start run on timer", fan->name);
                    xTimerStart(fan->runOnTimer, 0);
                }
            }
            else
            {
                fan->override = false;
            }
            if (xTimerIsTimerActive(fan->overThresholdTimer) == pdTRUE)
            {
                xTimerStop(fan->overThresholdTimer, 0);
            }
        }
    }
    sprintf(fan->humidity, "%d.%d", humidityTenths / 10, humidityTenths % 10);
    value.s = fan->humidity;
    iotElementPubUpdate(&fan->humidityPub, value);
}

static void humidityFanCtrl(void *userData, iotElementSub_t *sub, iotValue_t value)
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
                iotElementPubUpdate(&fan->thresholdPub, value);
            }
        }
    }
    else if (strncmp(MANUAL_CMD, value.s, MANUAL_CMD_LEN) == 0)
    {
        uint32_t secs;
        if (sscanf(value.s + MANUAL_CMD_LEN, "%u", &secs) == 1)
        {
            xTimerReset(fan->manualModeTimer, 0);
            fan->manualMode = true;
            fan->manualModeSecsLeft = secs;
        }
    }
    else if (strcmp(AUTO_CMD, value.s))
    {
        if (fan->manualMode)
        {
            fan->manualMode = false;
            xTimerStop(fan->manualModeTimer, 0);
            humidityFanUpdateHumidity(fan, fan->lastHumidity);
        }
    }
}

static void humidityFanRunOnTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    ESP_LOGI(TAG, "%s: Run on timer fired", fan->name);
    fan->override = false;
    humidityFanSetState(fan, false);
}

static void humidityOverThresholdTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    humidityFanSetState(fan, true);
}

static void humidityManulModeSecsTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    fan->manualModeSecsLeft --;
    if (fan->manualModeSecsLeft == 0)
    {
        fan->manualMode = false;
        xTimerStop(xTimer, 0);
        humidityFanUpdateHumidity(fan, fan->lastHumidity);
    }
}