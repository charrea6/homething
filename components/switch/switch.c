#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sdkconfig.h"
#include "switch.h"
#include "gpiox.h"
#include "notifications.h"

static const char *TAG="switch";

#define SWITCH_THREAD_NAME "switches"
#define SWITCH_THREAD_PRIO 8
#define SWITCH_THREAD_STACK_WORDS 2048

static GPIOX_Pins_t switchPins, switchValues;

int switchInit()
{
    GPIOX_PINS_CLEAR_ALL(switchPins);
    return 0;
}

Notifications_ID_t switchAdd(int pin)
{
    if (pin > GPIOX_PINS_MAX) {
        ESP_LOGE(TAG, "switchAdd: Invalid Pin number %d", pin);
        return -1;
    }
    GPIOX_PINS_SET(switchPins, pin);
    return NOTIFICATIONS_MAKE_ID(GPIOSWITCH, pin);
}

static void switchThread(void* pvParameters)
{
    int i;
    GPIOX_Pins_t newValues, diff;
    NotificationsData_t data;

    ESP_LOGI(TAG, "Switch thread starting");

    gpioxSetup(&switchPins, GPIOX_MODE_IN_PULLUP);
    gpioxGetPins(&switchPins, &switchValues);

    ESP_LOGI(TAG, "Switches configured");
    while(true) {
        vTaskDelay(10 / portTICK_RATE_MS);  //send every 0.01 seconds
        gpioxGetPins(&switchPins, &newValues);
        GPIOX_PINS_DIFF(diff, newValues, switchValues);
        for (i=0; i < GPIOX_PINS_MAX; i++) {
            if (GPIOX_PINS_IS_SET(diff, i)) {
                data.switchState = GPIOX_PINS_IS_SET(newValues, i);
                notificationsNotify(Notifications_Class_Switch, NOTIFICATIONS_MAKE_ID(GPIOSWITCH, i), &data);
            }
        }
        switchValues = newValues;
    }
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