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

#define BUFFSIZE 1500
#define TEXT_BUFFSIZE 1024

#define MAX_VERSION_LEN 31

#define UPDATER_THREAD_NAME "updater"
#define UPDATER_THREAD_PRIO 7
#define UPDATER_THREAD_STACK_WORDS 8192

#define updateStatusf(fmt...) snprintf(statusBuffer, sizeof(statusBuffer), fmt); updateStatus(statusBuffer)

static const char TAG[]="Updater";
const int UPDATE_BIT = BIT0;

static iotElement_t *updaterElement;
static iotElementSub_t *updateSub;
static iotElementPub_t *versionPub;
static iotElementPub_t *statusPub;

static EventGroupHandle_t updateEventGroup;

static char newVersion[MAX_VERSION_LEN + 1];

static char statusBuffer[64];


static int connectToServer(char *host, int port)
{
    struct sockaddr_in sAddr;
    int mySocket = -1;
    struct hostent* entry;
    
    entry = gethostbyname(host);
    if (entry != NULL) 
    {
        sAddr.sin_family = AF_INET;
        sAddr.sin_addr.s_addr = ((struct in_addr*)(entry->h_addr))->s_addr;
        sAddr.sin_port = htons(port);

        mySocket = socket(AF_INET, SOCK_STREAM, 0);
        if (mySocket >= 0) 
        {
            if (connect(mySocket, (struct sockaddr*)&sAddr, sizeof(sAddr)) == 0)
            {
                return mySocket;
            }
            else
            {
                close(mySocket);
            }
        }
    }
    return -1;
}

static void updateStatus(char *status)
{
    iotValue_t value;
    value.s = status;
    iotElementPubUpdate(statusPub, value);
}

static void updaterThread(void *pvParameter)
{
    esp_err_t err;
    /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

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

    while(true)
    {
        updateStatus("Waiting for update");
        xEventGroupWaitBits(updateEventGroup, UPDATE_BIT, false, true, portMAX_DELAY);
        xEventGroupClearBits(updateEventGroup, UPDATE_BIT);
        updateStatusf("Updating to %s", newVersion);

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
        updateStatus("Failed : Version too long");
    }
}

void updaterInit(void)
{
    iotValue_t value;
    ESP_LOGI(TAG, "Updater initialised");
    iotElementAdd("sw", &updaterElement);
    value.s = appVersion;
    iotElementPubAdd(updaterElement, "version", iotValueType_String, true, value, &versionPub);
    value.s = "";
    iotElementPubAdd(updaterElement, "status", iotValueType_String, true, value, &statusPub);
    iotElementSubAdd(updaterElement, "update", iotValueType_String, updateVersion, NULL, &updateSub);
    
    updateEventGroup = xEventGroupCreate();
    xTaskCreate(updaterThread,
                UPDATER_THREAD_NAME,
                UPDATER_THREAD_STACK_WORDS,
                NULL,
                UPDATER_THREAD_PRIO,
                NULL);
}