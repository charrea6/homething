#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"

#include "updater.h"

#include "sdkconfig.h"
#include "updaterInternal.h"

#define MAX_VERSION_LEN 31

#define UPDATER_THREAD_NAME "updater"
#define UPDATER_THREAD_PRIO 7
#define UPDATER_THREAD_STACK_WORDS 2048
#define MAX_CALLBACKS 1

static const char TAG[]="Updater";
static const int UPDATE_BIT = BIT0;

static int nrofCallbacks = 0;
static updaterStatusCallback_t callbacks[MAX_CALLBACKS];

static EventGroupHandle_t updateEventGroup;

static char newVersion[MAX_VERSION_LEN + 1];

char updaterStatusBuffer[UPDATER_STATUS_BUFFER_SIZE];
static char updatePath[256];

extern char appVersion[]; /* this is defined in version.c which is autogenerated */

static void updaterThread(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting OTA : flash %s", CONFIG_ESPTOOLPY_FLASHSIZE);

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(TAG, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(TAG, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    updaterUpdateStatus("Waiting for update");
    while(true) {
        xEventGroupWaitBits(updateEventGroup, UPDATE_BIT, false, true, portMAX_DELAY);
        xEventGroupClearBits(updateEventGroup, UPDATE_BIT);
        updaterUpdateStatusf("Updating to %s", newVersion);

#ifdef CONFIG_ESPTOOLPY_FLASHSIZE_1MB
        {
            const esp_partition_t *updatePartition;
            int partitionId;
            updatePartition = esp_ota_get_next_update_partition(NULL);
            partitionId = (updatePartition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0) + 1;
            if ((partitionId <= 0) || (partitionId > 16)) {
                partitionId = 1;
            }
            snprintf(updatePath, sizeof(updatePath), CONFIG_UPDATER_PATH_PREFIX "/homething." CONFIG_IDF_TARGET ".app%d.%s.ota", partitionId, newVersion);
        }
#else
        snprintf(updatePath, sizeof(updatePath), CONFIG_UPDATER_PATH_PREFIX "/homething." CONFIG_IDF_TARGET ".%s.ota", newVersion);
#endif

        updaterDownloadAndUpdate(CONFIG_UPDATER_HOST, CONFIG_UPDATER_PORT, updatePath);
    }
}

void updaterInit()
{
    ESP_LOGI(TAG, "Updater initialised, Version: %s", appVersion);
    updateEventGroup = xEventGroupCreate();
    xTaskCreate(updaterThread,
                UPDATER_THREAD_NAME,
                UPDATER_THREAD_STACK_WORDS,
                NULL,
                UPDATER_THREAD_PRIO,
                NULL);
}

void updaterUpdateStatus(char *status)
{
    int i;
    for (i=0; i < nrofCallbacks; i++) {
        callbacks[i](status);
    }
}

void updaterUpdate(const char *updateVersion)
{
    ESP_LOGI(TAG, "Updating to version %s", updateVersion);
    if (strlen(updateVersion) <= MAX_VERSION_LEN) {
        strcpy(newVersion, updateVersion);
        xEventGroupSetBits(updateEventGroup, UPDATE_BIT);
    } else {
        updaterUpdateStatus("Failed : Version too long");
    }
}

void updaterAddStatusCallback(updaterStatusCallback_t callback)
{
    if (nrofCallbacks >= MAX_CALLBACKS) {
        ESP_LOGE(TAG, "Maximum number of status callbacks reached");
        return;
    }
    callbacks[nrofCallbacks] = callback;
    nrofCallbacks++;
}