#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "humidityfan.h"

#define THRESHOLD_CMD "threshold "
#define THRESHOLD_CMD_LEN (sizeof(THRESHOLD_CMD) - 1)

static void humidityFanCtrl(void *userData, iotElementSub_t *sub, iotValue_t value);
static void humidityFanRunOnTimeout(TimerHandle_t xTimer);

static const char TAG[] = "HFAN";
static int fanCount=0;

void humidityFanInit(HumidityFan_t *fan, int relayPin, int threshold)
{
    iotElementPub_t *pub;
    iotElementSub_t *sub;
    sprintf(fan->name, "fan%d", fanCount);
    fanCount ++;

    fan->override = false;
    fan->threshold = threshold;
    fan->runOnSeconds = 30000;
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

    fan->runOnTimer = xTimerCreate(fan->name, fan->runOnSeconds / portTICK_RATE_MS, pdFALSE, fan, humidityFanRunOnTimeout);
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
    if ((humidityTenths / 10) >= fan->threshold)
    {
        if (!fan->override)
        {
            humidityFanSetState(fan, true);
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
}

static void humidityFanRunOnTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    ESP_LOGI(TAG, "%s: Run on timer fired", fan->name);
    fan->override = false;
    humidityFanSetState(fan, false);
}
