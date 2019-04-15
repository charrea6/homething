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

#include "iot.h"
#include "updater.h"

#include "sdkconfig.h"

#include "version.h"

#define MAX_VERSION_LEN 31

#define UPDATER_THREAD_NAME "updater"
#define UPDATER_THREAD_PRIO 7
#define UPDATER_THREAD_STACK_WORDS 8192

static const char TAG[]="Updater";
const int UPDATE_BIT = BIT0;

static iotElement_t updaterElement;
static iotElementSub_t updateSub;
static iotElementPub_t versionPub;
static iotElementPub_t statusPub;
static iotElementPub_t profilePub;

static EventGroupHandle_t updateEventGroup;

static char newVersion[MAX_VERSION_LEN + 1];

char updaterStatusBuffer[UPDATER_STATUS_BUFFER_SIZE];
static char updatePath[256];

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
    while(true)
    {   
        xEventGroupWaitBits(updateEventGroup, UPDATE_BIT, false, true, portMAX_DELAY);
        xEventGroupClearBits(updateEventGroup, UPDATE_BIT);
        updaterUpdateStatusf("Updating to %s", newVersion);

#ifdef CONFIG_ESPTOOLPY_FLASHSIZE_1MB 
        {
            const esp_partition_t *updatePartition;
            int partitionId;
            updatePartition = esp_ota_get_next_update_partition(NULL);
            partitionId = (updatePartition->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_0) + 1;
            if ((partitionId <= 0) || (partitionId > 16))
            {
                partitionId = 1;
            }
            snprintf(updatePath, sizeof(updatePath),"%s/homething.app%d__%s__%s.ota", CONFIG_UPDATER_PATH_PREFIX, partitionId, deviceProfile, newVersion);
        }
#else
        snprintf(updatePath, sizeof(updatePath),"%s/homething__%s__%s.ota", CONFIG_UPDATER_PATH_PREFIX, deviceProfile, newVersion);
#endif

        updaterUpdate(CONFIG_UPDATER_HOST, CONFIG_UPDATER_PORT, updatePath);
    }
}

static void updateVersion(void *userData, struct iotElementSub *sub, iotValue_t value)
{
    ESP_LOGI(TAG, "Updating to version %s", value.s);
    if (strlen(value.s) <= MAX_VERSION_LEN)
    {
        strcpy(newVersion, value.s);
        xEventGroupSetBits(updateEventGroup, UPDATE_BIT);
    }
    else
    {
        updaterUpdateStatus("Failed : Version too long");
    }
}

void updaterInit()
{
    ESP_LOGI(TAG, "Updater initialised, Version: %s Profile: %s", appVersion, deviceProfile);
    updaterElement.name = "sw";
    iotElementAdd(&updaterElement);

    versionPub.name = "version";
    versionPub.type = iotValueType_String;
    versionPub.retain = true;
    versionPub.value.s = appVersion;
    iotElementPubAdd(&updaterElement, &versionPub);
    
    statusPub.name = "status";
    statusPub.type = iotValueType_String;
    statusPub.retain = true;
    statusPub.value.s = "";
    iotElementPubAdd(&updaterElement, &statusPub);

    profilePub.name = "profile";
    profilePub.type = iotValueType_String;
    profilePub.retain = true;
    profilePub.value.s = deviceProfile;
    iotElementPubAdd(&updaterElement, &profilePub);

    updateSub.name = "update";
    updateSub.type = iotValueType_String;
    updateSub.callback = updateVersion;
    updateSub.userData = NULL;
    iotElementSubAdd(&updaterElement, &updateSub);
    
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
    iotValue_t value;
    value.s = status;
    iotElementPubUpdate(&statusPub, value);
}
