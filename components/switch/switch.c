#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "switch.h"

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
    gpio_config_t config;
    ESP_LOGI(TAG, "Switch thread starting, count %d", switchCount);
    for (i=0; i < switchCount; i++)
    {
        config.pin_bit_mask = 1<<switches[i].pin;
        config.mode = GPIO_MODE_DEF_INPUT;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        ESP_LOGI(TAG, "Configuring switch %d pin %d", i, switches[i].pin);
        gpio_config(&config);
        switches[i].state = gpio_get_level(switches[i].pin);
        ESP_LOGI(TAG, "Switch %d configured pin %d state %d", i, switches[i].pin, switches[i].state);
    }
    while(true)
    {
        vTaskDelay(10 / portTICK_RATE_MS);  //send every 0.01 seconds
        for (i=0; i < switchCount; i++)
        {
            new_state = gpio_get_level(switches[i].pin);
            if (new_state != switches[i].state) 
            {
                switches[i].cb(switches[i].userData);
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