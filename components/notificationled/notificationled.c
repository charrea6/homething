#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "notifications.h"
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

#define LED_SUBSYS_WIFI 0
#define LED_SUBSYS_MQTT 1
#define LED_STATE_ALL_CONNECTED ((1 << LED_SUBSYS_WIFI) | (1 << LED_SUBSYS_MQTT))

#define LED_ON 0
#define LED_OFF 1

#ifdef CONFIG_NOTIFICATION_LED
static const char TAG[]="notificationled";

static uint32_t onOffTimes[LEDPattern_Max][2] = {
    {0, 0},
    {500 / portTICK_RATE_MS, 500 / portTICK_RATE_MS},
    {500 / portTICK_RATE_MS, 1000 / portTICK_RATE_MS},
    {1000 / portTICK_RATE_MS, 1000 / portTICK_RATE_MS},
    {500 / portTICK_RATE_MS, 5000 / portTICK_RATE_MS},
    {1000 / portTICK_RATE_MS, 5000 / portTICK_RATE_MS},
    {0, 0}
};

static LEDPattern_e ledPatterns[State_Max];

static TimerHandle_t ledTimer;
static uint8_t connectedSubsys = 0;
static LEDPattern_e currentPattern = LEDPattern_Off;
static bool ledState = false;

static void readPatternSetting(nvs_handle handle, const char *name, LEDPattern_e default_pattern, LEDPattern_e *output) ;
static void setPattern(LEDPattern_e newPattern);
static void onLedTimer(TimerHandle_t xTimer);
static void onNetworkStatusUpdated(void *user,  NotificationsMessage_t *message);


void notificationLedInit()
{
    gpio_config_t config;
    nvs_handle handle;
    esp_err_t err;

    config.pin_bit_mask = 1 << CONFIG_NOTIFICATION_LED_PIN;
    config.mode = GPIO_MODE_DEF_OUTPUT;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    gpio_set_level(CONFIG_NOTIFICATION_LED_PIN, LED_OFF);

    notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_ALL, onNetworkStatusUpdated, NULL);

    err = nvs_open("notificationled", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        readPatternSetting(handle, "wifi", LEDPattern_0_5s_On_Off, &ledPatterns[State_Wifi]);
        readPatternSetting(handle, "mqtt", LEDPattern_0_5s_On_Off, &ledPatterns[State_MQTT]);
        readPatternSetting(handle, "connected", LEDPattern_On, &ledPatterns[State_Connected]);
        nvs_close(handle);
    }

    ledTimer = xTimerCreate("NOTIFLED", 500 / portTICK_RATE_MS, pdFALSE, NULL, onLedTimer);
}

static void readPatternSetting(nvs_handle handle, const char *name, LEDPattern_e default_pattern, LEDPattern_e *output)
{
    esp_err_t err;
    int32_t p;
    err = nvs_get_i32(handle, name, &p);
    if (err == ESP_OK) {
        if ((p >= LEDPattern_Off) && (p < LEDPattern_Max)) {
            *output = p;
        } else {
            err = ESP_ERR_INVALID_STATE;
        }
    }

    if (err != ESP_OK) {
        *output = default_pattern;
        err = nvs_set_i32(handle, name, (int32_t)default_pattern);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to update %s setting, err 0x%x", name, err);
        }
    }
    ESP_LOGI(TAG, "readPatternSetting: %s pattern %d", name, *output);
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
        setPattern(ledPatterns[State_Wifi]);
    } else if ((connectedSubsys & (1<<LED_SUBSYS_MQTT)) == 0) {
        ESP_LOGI(TAG, "onNetworkStatusUpdated: selecting mqtt pattern");
        setPattern(ledPatterns[State_MQTT]);
    } else {
        ESP_LOGI(TAG, "onNetworkStatusUpdated: selecting connected pattern");
        setPattern(ledPatterns[State_Connected]);
    }
}

static void setPattern(LEDPattern_e newPattern)
{
    if (newPattern != currentPattern) {
        currentPattern = newPattern;
        ESP_LOGI(TAG, "Selecting pattern %d", currentPattern);
        if (currentPattern == LEDPattern_Off) {
            xTimerStop(ledTimer, 0);
            gpio_set_level(CONFIG_NOTIFICATION_LED_PIN, LED_OFF);
            ledState = false;
        } else if (currentPattern == LEDPattern_On) {
            xTimerStop(ledTimer, 0);
            gpio_set_level(CONFIG_NOTIFICATION_LED_PIN, LED_ON);
            ledState = true;
        } else {
            onLedTimer(NULL);
        }
    }
}

static void onLedTimer(TimerHandle_t xTimer)
{
    uint32_t delay;
    if (ledState) {
        delay = onOffTimes[currentPattern][1];
    } else {
        delay = onOffTimes[currentPattern][0];
    }
    if (delay == 0) {
        return;
    }
    ledState = !ledState;
    gpio_set_level(CONFIG_NOTIFICATION_LED_PIN, ledState ? LED_ON: LED_OFF);
    xTimerChangePeriod(ledTimer, delay, 0);
}
#endif