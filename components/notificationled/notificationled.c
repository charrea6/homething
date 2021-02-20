#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "notifications.h"
#include "notificationled.h"
#include "sdkconfig.h"

typedef enum {
    LEDPattern_Off = 0,
    LEDPattern_0_5s_On_Off,
    LEDPattern_0_5s_On_1s_Off,
    LEDPattern_1s_On_Off,
    LEDPattern_0_5s_On_5s_Off,
    LEDPattern_1s_On_5s_Off,
    LEDPattern_On,

    LEDPattern_Max
} LEDPattern_e;

typedef enum {
    State_Wifi = 0,
    State_MQTT,
    State_Connected,
    State_Max
} State_e;

struct NotificationLed {
    TimerHandle_t timer;
    uint8_t pin;
    bool state;
    NotificationLedPattern_t pattern;
};

#define LED_SUBSYS_WIFI 0
#define LED_SUBSYS_MQTT 1
#define LED_STATE_ALL_CONNECTED ((1 << LED_SUBSYS_WIFI) | (1 << LED_SUBSYS_MQTT))

#define LED_ON 0
#define LED_OFF 1

static const char TAG[]="notificationled";
static void notificationLedTimer(TimerHandle_t xTimer);

#ifdef CONFIG_NOTIFICATION_LED
static NotificationLedPattern_t patternDetails[LEDPattern_Max] = {
    {.on = false, .onTime = 0, .offTime = 0},
    {.on = true, .onTime = 500, .offTime = 500},
    {.on = true, .onTime = 500, .offTime = 1000},
    {.on = true, .onTime = 1000, .offTime = 1000},
    {.on = true, .onTime = 500, .offTime = 5000},
    {.on = true, .onTime = 1000, .offTime = 5000},
    {.on = true, .onTime = 0, .offTime = 0}
};

static NotificationLedPattern_t *statePatterns[State_Max];
static NotificationLed_t networkLed;
static uint8_t connectedSubsys = 0;

static void readPatternSetting(nvs_handle handle, const char *name, LEDPattern_e default_pattern, NotificationLedPattern_t **output) ;
static void onNetworkStatusUpdated(void *user,  NotificationsMessage_t *message);


void notificationLedInit()
{
    nvs_handle handle;
    esp_err_t err;

    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_ALL, onNetworkStatusUpdated, NULL);

    err = nvs_open("notificationled", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        readPatternSetting(handle, "wifi", LEDPattern_0_5s_On_Off, &statePatterns[State_Wifi]);
        readPatternSetting(handle, "mqtt", LEDPattern_0_5s_On_Off, &statePatterns[State_MQTT]);
        readPatternSetting(handle, "connected", LEDPattern_On, &statePatterns[State_Connected]);
        nvs_close(handle);
    }

    networkLed = notificationLedNew(CONFIG_NOTIFICATION_LED_PIN);
}

static void readPatternSetting(nvs_handle handle, const char *name, LEDPattern_e default_pattern, NotificationLedPattern_t **output)
{
    esp_err_t err;
    int32_t p;
    err = nvs_get_i32(handle, name, &p);
    if (err == ESP_OK) {
        if ((p >= LEDPattern_Off) && (p < LEDPattern_Max)) {
            *output = &patternDetails[p];
        } else {
            err = ESP_ERR_INVALID_STATE;
        }
    }

    if (err != ESP_OK) {
        *output = &patternDetails[default_pattern];
        err = nvs_set_i32(handle, name, (int32_t)default_pattern);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update %s setting, err 0x%x", name, err);
        }
    }
}

static void onNetworkStatusUpdated(void *user,  NotificationsMessage_t *message)
{
    int subsystem = -1;
    if (message->id == NOTIFICATIONS_ID_WIFI_STATION) {
        subsystem = LED_SUBSYS_WIFI;
    } else if (message->id == NOTIFICATIONS_ID_MQTT) {
        subsystem = LED_SUBSYS_MQTT;
    }
    if (subsystem == -1) {
        return;
    }
    if (message->data.connectionState == Notifications_ConnectionState_Connected) {
        connectedSubsys |= 1<< subsystem;
    } else {
        connectedSubsys &= ~(1<< subsystem);
    }
    ESP_LOGI(TAG, "onNetworkStatusUpdated: Subsys %d ConnectionState %d connectedSubsys %d", subsystem, message->data.connectionState, connectedSubsys);
    if ((connectedSubsys & (1<<LED_SUBSYS_WIFI)) == 0) {
        ESP_LOGI(TAG, "onNetworkStatusUpdated: selecting wifi pattern");
        notificationLedSetPattern(networkLed, statePatterns[State_Wifi]);
    } else if ((connectedSubsys & (1<<LED_SUBSYS_MQTT)) == 0) {
        ESP_LOGI(TAG, "onNetworkStatusUpdated: selecting mqtt pattern");
        notificationLedSetPattern(networkLed, statePatterns[State_MQTT]);
    } else {
        ESP_LOGI(TAG, "onNetworkStatusUpdated: selecting connected pattern");
        notificationLedSetPattern(networkLed, statePatterns[State_Connected]);
    }
}
#endif

NotificationLed_t notificationLedNew(int pin)
{
    gpio_config_t config;

    NotificationLed_t result = calloc(1, sizeof(struct NotificationLed));
    if (result == NULL) {
        return NULL;
    }

    config.pin_bit_mask = 1 << pin;
    config.mode = GPIO_MODE_DEF_OUTPUT;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    gpio_set_level(pin, LED_OFF);
    result->pin = pin;
    result->state = false;
    result->pattern.on = false;
    result->pattern.onTime = 0;
    result->pattern.offTime = 0;
    result->timer = xTimerCreate("NOTIFLED", 500 / portTICK_RATE_MS, pdFALSE, result, notificationLedTimer);
    return result;
}

void notificationLedSetPattern(NotificationLed_t led, NotificationLedPattern_t *pattern)
{
    if (memcmp(pattern, &led->pattern, sizeof(NotificationLedPattern_t)) != 0) {
        led->pattern = *pattern;
        if (led->timer) {
            xTimerStop(led->timer, 0);
        }

        if (led->pattern.on) {
            if (led->pattern.onTime == 0) {
                gpio_set_level(led->pin, LED_ON);
            } else {
                if (led->timer) {
                    notificationLedTimer(led->timer);
                } else {
                    ESP_LOGE(TAG, "No timer for LED pin %d", led->pin);
                }
            }
        } else {
            gpio_set_level(led->pin, LED_OFF);
        }
    }
}

void notificationLedGetPattern(NotificationLed_t led, NotificationLedPattern_t *pattern)
{
    *pattern = led->pattern;
}

static void notificationLedTimer(TimerHandle_t xTimer)
{
    NotificationLed_t led = pvTimerGetTimerID(xTimer);
    uint32_t delay;
    if (led->state) {
        delay = led->pattern.offTime;
    } else {
        delay = led->pattern.onTime;
    }
    if (delay == 0) {
        return;
    }
    led->state = !led->state;
    gpio_set_level(led->pin, led->state ? LED_ON: LED_OFF);
    xTimerChangePeriod(led->timer, delay / portTICK_RATE_MS, 0);
}