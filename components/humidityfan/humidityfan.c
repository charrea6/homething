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

static void humidityFanUpdateHumidity(HumidityFan_t *fan, NotificationsMessage_t *message);
static void humidityFanCtrl(void *userData, iotElement_t *element, iotValue_t value);
static void humidityFanRunOnTimeout(TimerHandle_t xTimer);
static void humidityFanOverThresholdTimeout(TimerHandle_t xTimer);
static void humidityFanManualModeSecsTimeout(TimerHandle_t xTimer);
static void humidityFanManualModeDisable(HumidityFan_t *fan);

static const char TAG[] = "HFAN";
static int fanCount=0;

#define PUB_ID_STATE       0
#define PUB_ID_HUMIDITY    1
#define PUB_ID_THRESHOLD   2
#define PUB_ID_MANUAL      3
#define PUB_ID_MANUAL_SECS 4
#define PUB_ID_RELAY       5

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BOOL, "state"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_STRING, "humidity"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "threshold"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_BOOL, "manual"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "manualSecs"),
        IOT_DESCRIBE_PUB(IOT_VALUE_TYPE_RETAINED_INT, "relay")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(IOT_VALUE_TYPE_STRING, IOT_SUB_DEFAULT_NAME, (iotElementSubUpdateCallback_t)humidityFanCtrl)
    )
);

void humidityFanInit(HumidityFan_t *fan, Relay_t *relay, Notifications_ID_t humiditySensor, int threshold)
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
    fan->relay = relay;

    fan->element = iotNewElement(&elementDescription, 0, fan, "fan%d", fanCount);
    fanCount ++;

    value.i = relayId(relay);
    iotElementPublish(fan->element, PUB_ID_RELAY, value);
    value.i = fan->threshold;
    iotElementPublish(fan->element, PUB_ID_THRESHOLD, value);
    value.s = fan->humidity;
    iotElementPublish(fan->element, PUB_ID_HUMIDITY, value);
    
    fan->runOnTimer = xTimerCreate("fRO", SECS_TO_TICKS(fan->runOnSeconds), pdFALSE, fan, humidityFanRunOnTimeout);
    fan->overThresholdTimer = xTimerCreate("fOT", SECS_TO_TICKS(fan->overThresholdSeconds), pdFALSE, fan, humidityFanOverThresholdTimeout);
    fan->manualModeTimer = xTimerCreate("fMM", SECS_TO_TICKS(1), pdTRUE, fan, humidityFanManualModeSecsTimeout);
    notificationsRegister(Notifications_Class_Humidity, humiditySensor, (NotificationsCallback_t)humidityFanUpdateHumidity, fan);
}

static void humidityFanSetState(HumidityFan_t *fan, bool state)
{
    iotValue_t value;
    if (state != relayIsOn(fan->relay))
    {
        relaySetState(fan->relay, state);
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

static void humidityFanUpdateHumidity(HumidityFan_t *fan, NotificationsMessage_t *message)
{
    iotValue_t value;
    int32_t humidityTenths = message->data.humidity;

    ESP_LOGI(TAG, "%d: Humidity %d (Threshold %d) Manual Mode %d", fan->id, (humidityTenths / 100), fan->threshold, fan->manualMode);
    fan->lastHumidity = humidityTenths;
    if (fan->manualMode == false)
    {
        if ((humidityTenths / 100) >= fan->threshold)
        {
            if (!fan->override && !relayIsOn(fan->relay))
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
            if (relayIsOn(fan->relay))
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
    sprintf(fan->humidity, "%d.%02d", humidityTenths / 100, humidityTenths % 100);
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
            if (relayIsOn(fan->relay))
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
            humidityFanManualModeDisable(fan);
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

static void humidityFanOverThresholdTimeout(TimerHandle_t xTimer)
{
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    ESP_LOGI(TAG, "%d: Over threshold timer fired", fan->id);
    humidityFanSetState(fan, true);
}

static void humidityFanManualModeSecsTimeout(TimerHandle_t xTimer)
{
    iotValue_t value;
    HumidityFan_t *fan = pvTimerGetTimerID(xTimer);
    fan->manualModeSecsLeft --;
    value.i = fan->manualModeSecsLeft;
    iotElementPublish(fan->element, PUB_ID_MANUAL_SECS, value);

    ESP_LOGI(TAG, "%d: Manual mode seconds left %d", fan->id, fan->manualModeSecsLeft);
    if (fan->manualModeSecsLeft == 0)
    {
        humidityFanManualModeDisable(fan);
    }
}

static void humidityFanManualModeDisable(HumidityFan_t *fan) 
{
    iotValue_t value;
    NotificationsMessage_t message;
    fan->manualMode = false;
    xTimerStop(fan->manualModeTimer, 0);
    message.data.humidity = fan->lastHumidity;
    humidityFanUpdateHumidity(fan, &message);
    value.b = false;
    iotElementPublish(fan->element, PUB_ID_MANUAL, value);
}