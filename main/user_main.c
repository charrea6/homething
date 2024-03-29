#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "i2cdev.h"
#include "switch.h"
#include "iot.h"
#include "iotDevice.h"
#include "updater.h"
#include "notificationled.h"
#include "notifications.h"
#include "provisioning.h"
#include "wifi.h"
#include "logging.h"
#include "profile.h"
#include "homeassistant.h"
#include "bootprot.h"

static const char TAG[] = "main";
extern char appVersion[]; /* this is defined in version.c which is autogenerated */
/* Device capabilities string creation - start */
static char capabilities[] = "flash" CONFIG_ESPTOOLPY_FLASHSIZE ",configV2"
#ifdef CONFIG_DHT22
                             ",dht22"
#endif
#ifdef CONFIG_BME280
                             ",si7201"
#endif
#ifdef CONFIG_BME280
                             ",bme280"
#endif
#ifdef CONFIG_DS18x20
                             ",ds18x20"
#endif
#ifdef CONFIG_HUMIDISTAT
                             ",humidistat"
#endif
#ifdef CONFIG_THERMOSTAT
                             ",thermostat"
#endif
#ifdef CONFIG_HOMEASSISTANT
                             ",homeassistant"
#endif
#ifdef CONFIG_DRAYTONSCR
                             ",draytonscr"
#endif
#ifdef CONFIG_LED_STRIP
                             ",led_strip"
#endif
                             ;
/* Device capabilities string creation - finish */

#define CHECK_ERROR( __func ) do {if (__func) { ESP_LOGE(TAG, "%s failed!", #__func); return;} } while(0)

void app_main(void)
{
    struct timeval tv = {.tv_sec = 0, .tv_usec=0};
    settimeofday(&tv, NULL);

    ESP_ERROR_CHECK( nvs_flash_init() );
#if defined(CONFIG_BME280) || defined(CONFIG_SI7021) || defined(CONFIG_GPIOX_EXPANDERS)
    ESP_ERROR_CHECK( i2cdev_init() );
#endif
    cJSON_InitHooks(NULL);

    bootprotInit();

    notificationsInit();

#ifdef CONFIG_NOTIFICATION_LED
    notificationLedInit();
#endif

#ifdef CONFIG_LOG_SET_LEVEL
    esp_log_level_set("gpio", ESP_LOG_WARN);
#endif

    loggingInit();

    CHECK_ERROR(wifiInit());
    CHECK_ERROR(iotInit());
    CHECK_ERROR(provisioningInit());
    CHECK_ERROR(switchInit());
    updaterInit();
    CHECK_ERROR(iotDeviceInit(appVersion, capabilities));

    processProfile();

#ifdef CONFIG_HOMEASSISTANT
    homeAssistantDiscoveryInit();
#endif

    switchStart();
    wifiStart();

    CHECK_ERROR(provisioningStart());
}
