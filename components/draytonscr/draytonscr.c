/*
 * Inspiration for this code and more details about the Drayton SCR protocol can be found here:
 * https://github.com/tul/drayton_controller
 */
#include <stdbool.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gpiox.h"
#include "relay.h"
#include "draytonscr.h"

#include "esp_log.h"

static void draytonSCRSetState(Relay_t *relay, bool on);

static const RelayInterface_t intf = {
    .setState = draytonSCRSetState
};

typedef struct DraytonSCR {
    Relay_t relay;

    const char* onSequence;
    const char* offSequence;
    TaskHandle_t task;
} DraytonSCR_t;

#define BASE_DURATION 500 /* microseconds */
#define RETRANSMIT_INTERVAL 5000 /* milliseconds */
#define RETRANSMIT_COUNT 3
#define REPEAT_INTERVAL 60*1000 /* milliseconds */

static void draytonSCRTransmitTask(void* pvParameters);

static const char TAG[] = "draytonSCR";

#define SWITCH_THREAD_NAME TAG
#define SWITCH_THREAD_PRIO 8
#define SWITCH_THREAD_STACK_WORDS 2048

Relay_t* draytonSCRInit(uint8_t pin, const char *onSequence, const char *offSequence)
{
    DraytonSCR_t *draytonSCR = malloc(sizeof(DraytonSCR_t));
    if (draytonSCR == NULL) {
        return NULL;
    }
    draytonSCR->relay.intf = &intf;
    draytonSCR->relay.fields.id = 0;
    draytonSCR->relay.fields.pin = pin;
    draytonSCR->relay.fields.on = false;
    draytonSCR->onSequence = onSequence;
    draytonSCR->offSequence = offSequence;
    relayNewIOTElement(&draytonSCR->relay, "draytonscr%d");
    xTaskCreate(draytonSCRTransmitTask,
                SWITCH_THREAD_NAME,
                SWITCH_THREAD_STACK_WORDS,
                draytonSCR,
                SWITCH_THREAD_PRIO,
                &draytonSCR->task);
    return &draytonSCR->relay;
}

static void draytonSCRSetState(Relay_t *relay, bool on)
{
    DraytonSCR_t *draytonSCR = (DraytonSCR_t*)(((void*) relay) - offsetof(DraytonSCR_t, relay));
    ESP_LOGI(TAG, "draytonSCR: Is on? %s", on ? "On":"Off");
    relay->fields.on = on;

    xTaskNotifyGive(draytonSCR->task);
}

static void draytonSCRTransmit(DraytonSCR_t *draytonSCR, const char *code)
{
    int i;
    GPIOX_Pins_t pins, onValues, offValues;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_SET(pins, draytonSCR->relay.fields.pin);
    GPIOX_PINS_CLEAR_ALL(onValues);
    GPIOX_PINS_SET(onValues, draytonSCR->relay.fields.pin);
    GPIOX_PINS_CLEAR_ALL(offValues);

    for (i = 0; code[i]; i ++) {
        switch(code[i]) {
        case 'A':
            gpioxSetPins(&pins, &onValues);
            ets_delay_us(BASE_DURATION);
            gpioxSetPins(&pins, &offValues);
            ets_delay_us(BASE_DURATION);
            break;
        case 'B':
            gpioxSetPins(&pins, &offValues);
            ets_delay_us(BASE_DURATION);
            gpioxSetPins(&pins, &onValues);
            ets_delay_us(BASE_DURATION);
            break;
        case 'C':
            gpioxSetPins(&pins, &onValues);
            ets_delay_us(BASE_DURATION * 2);
            break;
        case 'D':
            gpioxSetPins(&pins, &offValues);
            ets_delay_us(BASE_DURATION * 2);
            break;
        }
    }

    gpioxSetPins(&pins, &offValues);
}

static void draytonSCRTransmitTask(void* pvParameters)
{
    DraytonSCR_t *draytonSCR = pvParameters;
    GPIOX_Pins_t pins;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_SET(pins, draytonSCR->relay.fields.pin);

    gpioxSetup(&pins, GPIOX_MODE_OUT);

    int transmitCount = 0;
    TickType_t toWait;
    while(true) {
        ESP_LOGI(TAG, "Sending \"%s\" sequence", draytonSCR->relay.fields.on ? draytonSCR->onSequence : draytonSCR->offSequence);
        draytonSCRTransmit(draytonSCR, draytonSCR->relay.fields.on ? draytonSCR->onSequence : draytonSCR->offSequence);
        transmitCount ++;
        if (transmitCount >= RETRANSMIT_COUNT) {
            toWait = (REPEAT_INTERVAL / portTICK_RATE_MS);
            transmitCount = 0;
        } else {
            toWait = (RETRANSMIT_INTERVAL / portTICK_RATE_MS);
        }
        if (ulTaskNotifyTake(pdTRUE, toWait) != 0) {
            transmitCount = 0;
        }
    }
}
