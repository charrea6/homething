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

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static void taskStats(TimerHandle_t xTimer)
{
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = malloc(sizeof(TaskStatus_t) * nrofTasks);
    if (tasksStatus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        return;
    }
    nrofTasks = uxTaskGetSystemState( tasksStatus, nrofTasks, NULL);
    int i;
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
    
    printf("TASK STATS # %lu\n"
           "----------------\n\n", nrofTasks);
    printf("Name                : St Pr Stack Left\n"
           "--------------------------------------\n");
    for (i=0; i < nrofTasks; i++) {
        printf("%-20s: %2d %2lu % 10d\n", tasksStatus[i].pcTaskName, tasksStatus[i].eCurrentState, tasksStatus[i].uxCurrentPriority, tasksStatus[i].usStackHighWaterMark);
    }
    free(tasksStatus);
    printf("\nMEMORY STATS\n"
             "------------\n"
             "Free: %u\n"
             "Low : %u\n\n", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    printf("\n\n");
}
#endif

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

#if defined(CONFIG_LIGHT) || \
    defined(CONFIG_MOTION) || \
    defined(CONFIG_DOORBELL)
    switchStart();
#endif

    updaterInit();
    iotStart();
    provisioningStart();

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    xTimerStart(xTimerCreate("stats", 30*1000 / portTICK_RATE_MS, pdTRUE, NULL, taskStats), 0);
#endif
}
