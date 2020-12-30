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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "sdkconfig.h"
#include "switch.h"
#include "iot.h"
#include "dht.h"
#include "humidityfan.h"
#include "updater.h"
#include "gpiox.h"
#include "notifications.h"

#include "provisioning.h"
#include "profile.h"

static const char TAG[] = "main";

void app_main(void)
{
    struct timeval tv = {.tv_sec = 0, .tv_usec=0};
    settimeofday(&tv, NULL);

    ESP_ERROR_CHECK( nvs_flash_init() );

    notificationsInit();
    iotInit();
    provisioningInit();
    gpioxInit();
    switchInit();
    processProfile();

#if defined(CONFIG_DHT22)
    dht22Start();
#endif

    switchStart();

    updaterInit();
    iotStart();
    provisioningStart();
}
