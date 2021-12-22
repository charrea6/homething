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
#include "draytonscr.h"

#include "esp_log.h"

struct DraytonSCR {
    uint8_t pin;
    bool on;

    const char* onSequence;
    const char* offSequence;
    TaskHandle_t task;
};

#define BASE_DURATION 500 /* microseconds */
#define RETRANSMIT_INTERVAL 5000 /* milliseconds */
#define RETRANSMIT_COUNT 3
#define REPEAT_INTERVAL 60*1000 /* milliseconds */

static void draytonSCRTransmitTask(void* pvParameters);

static const char TAG[] = "draytonSCR";

#define SWITCH_THREAD_NAME TAG
#define SWITCH_THREAD_PRIO 8
#define SWITCH_THREAD_STACK_WORDS 2048

DraytonSCR_t* draytonSCRInit(uint8_t pin, const char *onSequence, const char *offSequence)
{
    DraytonSCR_t *draytonSCR = malloc(sizeof(DraytonSCR_t));
    if (draytonSCR == NULL) {
        return NULL;
    }

    draytonSCR->pin = pin;
    draytonSCR->on = false;
    draytonSCR->onSequence = onSequence;
    draytonSCR->offSequence = offSequence;

    xTaskCreate(draytonSCRTransmitTask,
                SWITCH_THREAD_NAME,
                SWITCH_THREAD_STACK_WORDS,
                draytonSCR,
                SWITCH_THREAD_PRIO,
                &draytonSCR->task);
    return draytonSCR;
}

void draytonSCRSetState(DraytonSCR_t *draytonSCR, bool on)
{
    if (on == draytonSCR->on) {
        return;
    }

    ESP_LOGI(TAG, "draytonSCR: Is on? %s", on ? "On":"Off");
    draytonSCR->on = on;

    xTaskNotifyGive(draytonSCR->task);
}

bool draytonSCRIsOn(DraytonSCR_t *draytonSCR)
{
    return draytonSCR->on;
}

static void draytonSCRTransmit(DraytonSCR_t *draytonSCR, const char *code)
{
    int i;
    GPIOX_Pins_t pins, onValues, offValues;
    GPIOX_PINS_CLEAR_ALL(pins);
    GPIOX_PINS_SET(pins, draytonSCR->pin);
    GPIOX_PINS_CLEAR_ALL(onValues);
    GPIOX_PINS_SET(onValues, draytonSCR->pin);
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
    GPIOX_PINS_SET(pins, draytonSCR->pin);

    gpioxSetup(&pins, GPIOX_MODE_OUT);

    int transmitCount = 0;
    TickType_t toWait;
    while(true) {
        ESP_LOGI(TAG, "Sending \"%s\" sequence", draytonSCR->on ? draytonSCR->onSequence : draytonSCR->offSequence);
        draytonSCRTransmit(draytonSCR, draytonSCR->on ? draytonSCR->onSequence : draytonSCR->offSequence);
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
