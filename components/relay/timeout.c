#include <stdbool.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "iot.h"
#include "relay.h"
#include "notifications.h"

#define SECS_TO_TICKS(secs) ((secs * 1000) / portTICK_RATE_MS)

static const char TAG[] = "relay_timeout";

static void relayTimeoutCheckRelay(RelayTimeout_t *timeout);
static void relayTimeoutElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);
static void relayTimeoutNotification(void *user,  NotificationsMessage_t *message);
static void relayTimeoutSecondTimer(TimerHandle_t xTimer);
static void relayTimeoutUpdateState(RelayTimeout_t *timeout, bool timerRunning);

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, STRING, "state")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(STRING, IOT_SUB_DEFAULT_NAME)
    )
);

void relayTimeoutInit(uint8_t id, char *relay, bool targetValue, uint32_t seconds, RelayTimeout_t *timeout)
{
    timeout->targetValue = targetValue;
    timeout->timeoutSeconds = seconds;
    timeout->secondsLeft = seconds;
    timeout->stateStr = NULL;
    timeout->relay = relayFind(relay);

    if (timeout->relay != NULL) {
        ESP_LOGI(TAG, "Timeout created for relay %s timeout %d value %s", relay, seconds, targetValue ?"on":"off");
        timeout->element = iotNewElement(&elementDescription, IOT_ELEMENT_FLAGS_DONT_ANNOUNCE, relayTimeoutElementCallback, timeout, "relayTimeout%d", id);
        timeout->timer = xTimerCreate(iotElementGetName(timeout->element), SECS_TO_TICKS(1), pdTRUE, timeout, relayTimeoutSecondTimer);
        notificationsRegister(Notifications_Class_Relay, timeout->relay->id, relayTimeoutNotification, timeout);
        relayTimeoutCheckRelay(timeout);
        if (timeout->stateStr == NULL) {
            relayTimeoutUpdateState(timeout, false);
        }
    } else {
        ESP_LOGI(TAG, "Failed to find relay %s", relay);
    }
}

static void relayTimeoutCheckRelay(RelayTimeout_t *timeout)
{
    bool origState = xTimerIsTimerActive(timeout->timer) != pdFALSE;
    bool newState = origState;

    if(relayIsOn(timeout->relay) == timeout->targetValue) {
        xTimerStop(timeout->timer, 0);
        newState = false;
    } else {
        timeout->secondsLeft = timeout->timeoutSeconds;
        xTimerStart(timeout->timer, 0);
        newState = true;
    }

    if (origState != newState) {
        // update IOT state
        relayTimeoutUpdateState(timeout, newState);
    }
}

static void relayTimeoutElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    if (reason == IOT_CALLBACK_ON_SUB) {
        RelayTimeout_t *timeout = userData;
        if (strcasecmp("reset", details->value.s)==0) {
            timeout->secondsLeft = timeout->timeoutSeconds;
            // update IOT state
            relayTimeoutUpdateState(timeout, xTimerIsTimerActive(timeout->timer) != pdFALSE);
        }
    }
}

static void relayTimeoutNotification(void *user,  NotificationsMessage_t *message)
{
    RelayTimeout_t *timeout = user;
    if (message->id == timeout->relay->id) {
        relayTimeoutCheckRelay(timeout);
    }
}

static void relayTimeoutSecondTimer(TimerHandle_t xTimer)
{
    RelayTimeout_t *timeout = pvTimerGetTimerID(xTimer);
    bool state = xTimerIsTimerActive(timeout->timer) != pdFALSE;

    if (timeout->secondsLeft > 0) {
        timeout->secondsLeft --;
    }
    if (timeout->secondsLeft == 0) {
        xTimerStop(timeout->timer, 0);
        state = false;
        relaySetState(timeout->relay, timeout->targetValue);
    }
    // update IOT state
    relayTimeoutUpdateState(timeout, state);
}

static void relayTimeoutUpdateState(RelayTimeout_t *timeout, bool timerRunning)
{
    char *newState = NULL;
    asprintf(&newState, "{\"active\":%s,\"left\":%d,\"timeout\":%d}", timerRunning ? "true":"false", timeout->secondsLeft, timeout->timeoutSeconds);
    if (newState != NULL) {
        iotValue_t value;
        value.s = newState;
        iotElementPublish(timeout->element, 0, value);
        if (timeout->stateStr != NULL) {
            free(timeout->stateStr);
        }
        timeout->stateStr = newState;
    }
}