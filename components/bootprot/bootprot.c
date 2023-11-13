#include <stddef.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_log.h"

#include "bootprot.h"
#define BOOT_SUCCESSFUL_MS (2*60*1000)

#define MAGIC1 0x686f6d65
#define MAGIC2 0x7468696e

#define MAX_FAILED_BOOTS 3

#ifndef __NOINIT_ATTR
#define __NOINIT_ATTR _SECTION_ATTR_IMPL(".noinit", __COUNTER__)
#endif

__NOINIT_ATTR static struct BootProtection {
    uint32_t magic[2];
    uint32_t bootCount;
    uint32_t lastSuccessfulBoot;
} bootProtection;

static void bootprotBootSuccessfulTimer( TimerHandle_t xTimer );

static char TAG[]="bootprot";


void bootprotInit(void) {
    ESP_LOGD(TAG, "Boot Protection: magic = {%08x, %08x} count = %u lastSuccessfulBoot = %u", bootProtection.magic[0], bootProtection.magic[1], bootProtection.bootCount, bootProtection.lastSuccessfulBoot);

    if ((bootProtection.magic[0] != MAGIC1) || (bootProtection.magic[1] != MAGIC2)) {
        bootProtection.magic[0] = MAGIC1;
        bootProtection.magic[1] = MAGIC2;
        bootProtection.bootCount = 0;
        bootProtection.lastSuccessfulBoot = 0;
        ESP_LOGI(TAG, "Boot Protection: Cold Boot");
    } else {
        bootProtection.bootCount ++;
        ESP_LOGI(TAG, "Boot Protection: Warm Boot, count %u (last successful boot %u)", bootProtection.bootCount, bootProtection.lastSuccessfulBoot);
    }
    TimerHandle_t bootSuccessfulTimer;

    bootSuccessfulTimer = xTimerCreate("bootsuccess", BOOT_SUCCESSFUL_MS / portTICK_PERIOD_MS, pdFALSE, NULL, bootprotBootSuccessfulTimer);
    if (bootSuccessfulTimer != NULL) {
        xTimerStart(bootSuccessfulTimer, 0);
    }
}

bool bootprotTriggered(void) {
    if ((bootProtection.bootCount > MAX_FAILED_BOOTS)  && (bootProtection.lastSuccessfulBoot < bootProtection.bootCount - MAX_FAILED_BOOTS))
    {
        ESP_LOGI(TAG, "Boot Protection triggered!");
        return true;
    }
    return false;
}

static void bootprotBootSuccessfulTimer( TimerHandle_t xTimer ) {
    bootProtection.lastSuccessfulBoot = bootProtection.bootCount;
}