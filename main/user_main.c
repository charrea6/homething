#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
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
#include "gpiox.h"
#include "notificationled.h"
#include "notifications.h"
#include "provisioning.h"
#include "wifi.h"
#include "logging.h"
#include "profile.h"
#include "homeassistant.h"

static const char TAG[] = "main";

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

    notificationsInit();

#ifdef CONFIG_NOTIFICATION_LED
    notificationLedInit();
#endif

    loggingInit();

    CHECK_ERROR(wifiInit());
    CHECK_ERROR(iotInit());
    CHECK_ERROR(provisioningInit());
    CHECK_ERROR(gpioxInit());
    CHECK_ERROR(switchInit());
    CHECK_ERROR(iotDeviceInit());

    processProfile();

    updaterInit();

#ifdef CONFIG_HOMEASSISTANT
    homeAssistantDiscoveryInit();
#endif

    switchStart();
    wifiStart();

    CHECK_ERROR(provisioningStart());
}
