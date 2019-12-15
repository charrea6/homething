#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "switch.h"
#include "gpiox.h"

static const char *TAG="switch";

#define SWITCH_THREAD_NAME "switches"
#define SWITCH_THREAD_PRIO 8
#define SWITCH_THREAD_STACK_WORDS 8192

#define MAX_SWITCHES 4

static struct Switch{
    int pin;
    int state;
    SwitchCallback_t cb;
    void *userData;
}switches[MAX_SWITCHES];

static int switchCount = 0;

void switchAdd(int pin, SwitchCallback_t cb, void *userData)
{
    if (switchCount >= MAX_SWITCHES)
    {
        ESP_LOGE(TAG, "No available switches! Used %d", switchCount);
        return;
    }
    switches[switchCount].pin = pin;
    switches[switchCount].cb = cb;
    switches[switchCount].userData = userData;
    switchCount++;
}


static void switchThread(void* pvParameters)
{
    int new_state, i;
    GPIOX_Pins_t pins, values;
    GPIOX_PINS_CLEAR_ALL(pins);

    ESP_LOGI(TAG, "Switch thread starting, count %d", switchCount);
    for (i=0; i < switchCount; i++)
    {
        ESP_LOGI(TAG, "Configuring switch %d pin %d", i, switches[i].pin);
        GPIOX_PINS_SET(pins, switches[i].pin);
    }
    gpioxSetup(&pins, GPIOX_MODE_IN_PULLUP);
    gpioxGetPins(&pins, &values);
    for (i=0; i < switchCount; i++)
    {
        switches[i].state = GPIOX_PINS_IS_SET(values, switches[i].pin);
        ESP_LOGI(TAG, "Switch %d configured pin %d state %d", i, switches[i].pin, switches[i].state);
    }
    ESP_LOGI(TAG, "Switches configured");
    while(true)
    {
        vTaskDelay(10 / portTICK_RATE_MS);  //send every 0.01 seconds
        gpioxGetPins(&pins, &values);
        for (i=0; i < switchCount; i++)
        {
            new_state = GPIOX_PINS_IS_SET(values, switches[i].pin);
            if (new_state != switches[i].state) 
            {
                switches[i].cb(switches[i].userData, new_state);
                switches[i].state = new_state;
            }
        }
    }
}

void switchStart()
{
    xTaskCreate(switchThread,
                SWITCH_THREAD_NAME,
                SWITCH_THREAD_STACK_WORDS,
                NULL,
                SWITCH_THREAD_PRIO,
                NULL);
}