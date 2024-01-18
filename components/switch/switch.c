#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sdkconfig.h"
#include "switch.h"
#include "gpiox.h"
#include "notifications.h"

static const char *TAG="switch";
static void processSwitchHistories(GPIOX_Pins_t *history, int end);
static void printPinHistory(GPIOX_Pins_t *history, int end, int pin);

#define SWITCH_THREAD_NAME "switches"
#define SWITCH_THREAD_PRIO 8
#ifdef CONFIG_IDF_TARGET_ESP8266
#define SWITCH_THREAD_STACK_WORDS 2048
#elif CONFIG_IDF_TARGET_ESP32
#define SWITCH_THREAD_STACK_WORDS 3*1024
#endif
#define MAX_HISTORY 10
static GPIOX_Pins_t switchPins, switchValues;
static GPIOX_Pins_t history[MAX_HISTORY];
static uint8_t noiseFilterValues[GPIOX_PINS_MAX];

int switchInit()
{
    GPIOX_PINS_CLEAR_ALL(switchPins);
    return 0;
}

Notifications_ID_t switchAdd(int pin, uint8_t noiseFilter)
{
    if (pin > GPIOX_PINS_MAX) {
        ESP_LOGE(TAG, "switchAdd: Invalid Pin number %d", pin);
        return -1;
    }
    GPIOX_PINS_SET(switchPins, pin);
    if (noiseFilter > MAX_HISTORY) {
        noiseFilter = MAX_HISTORY;
    }
    noiseFilterValues[pin] = noiseFilter;
    return NOTIFICATIONS_MAKE_ID(GPIOSWITCH, pin);
}

static void switchThread(void* pvParameters)
{
    int historyIdx;

    ESP_LOGI(TAG, "Switch thread starting");

    gpioxSetup(&switchPins, GPIOX_MODE_IN_PULLUP);
    gpioxGetPins(&switchPins, &switchValues);

    for (historyIdx = 0; historyIdx < MAX_HISTORY; historyIdx ++) {
        history[historyIdx] = switchValues;
    }

    ESP_LOGI(TAG, "Switches configured");
    while(true) {
        vTaskDelay(10 / portTICK_RATE_MS);  //send every 0.01 seconds
        historyIdx ++;
        if (historyIdx >= MAX_HISTORY) {
            historyIdx = 0;
        }
        gpioxGetPins(&switchPins, &history[historyIdx]);
        processSwitchHistories(history, historyIdx);
    }
}

static void processSwitchHistories(GPIOX_Pins_t *history, int end)
{
    int pin;
    for (pin=0; pin < GPIOX_PINS_MAX; pin++) {
        if (GPIOX_PINS_IS_SET(switchPins, pin)) {
            uint8_t currentState = GPIOX_PINS_IS_SET(switchValues, pin);
            uint8_t newState = GPIOX_PINS_IS_SET(history[end], pin);
            if (noiseFilterValues[pin] != 0) {
                int historyIdx = end, i;

                for (i = 0; i < noiseFilterValues[pin]; i++) {
                    historyIdx --;
                    if (historyIdx < 0) {
                        historyIdx = MAX_HISTORY - 1;
                    }
                    if (GPIOX_PINS_IS_SET(history[historyIdx], pin) != newState) {
                        printPinHistory(history, end, pin);
                        newState = currentState;
                        break;
                    }
                }
            }
            if (currentState != newState) {
                NotificationsData_t data;
                data.switchState = newState;
                ESP_LOGI(TAG, "switch %d: state %d", pin, data.switchState);
                notificationsNotify(Notifications_Class_Switch, NOTIFICATIONS_MAKE_ID(GPIOSWITCH, pin), &data);
                if (newState) {
                    GPIOX_PINS_SET(switchValues, pin);
                } else {
                    GPIOX_PINS_CLEAR(switchValues, pin);
                }
            }
        }
    }
}

static void printPinHistory(GPIOX_Pins_t *history, int end, int pin)
{
    uint8_t pinHistory[MAX_HISTORY + 1];
    int pinHistoryIdx, historyIdx = end + 1;
    if (historyIdx >= MAX_HISTORY) {
        historyIdx = 0;
    }
    pinHistory[MAX_HISTORY] = 0;
    for (pinHistoryIdx = 0; pinHistoryIdx < MAX_HISTORY; pinHistoryIdx++) {
        if (GPIOX_PINS_IS_SET(history[historyIdx], pin)) {
            pinHistory[pinHistoryIdx] = '1';
        } else {
            pinHistory[pinHistoryIdx] = '0';
        }
        historyIdx ++;
        if (historyIdx >= MAX_HISTORY) {
            historyIdx = 0;
        }
    }
    ESP_LOGI(TAG, "switch %d: history %s", pin, pinHistory);
}

void switchStart()
{
    int i;
    for (i=0; i < GPIOX_PINS_MAX; i++) {
        if (GPIOX_PINS_IS_SET(switchPins, i)) {
            xTaskCreate(switchThread,
                        SWITCH_THREAD_NAME,
                        SWITCH_THREAD_STACK_WORDS,
                        NULL,
                        SWITCH_THREAD_PRIO,
                        NULL);
            return;
        }
    }
}