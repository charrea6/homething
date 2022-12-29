
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "cJSON.h"
#include "cJSON_AddOns.h"

#include "iot.h"
#include "wifi.h"
#include "notifications.h"
#include "sdkconfig.h"
#include "deviceprofile.h"
#include "utils.h"
#include "safestring.h"
#include "updater.h"
#include "iotDevice.h"

static const char *TAG="IOT-DEV";
static const char *DESC="desc";
static const char *IP="ip";
static const char *DESCRIPTION="description";
static const char *UPTIME="uptime";
static const char *CAPABILITIES="capabilities";
static const char *VERSION="version";
static const char *DEVICE="device";
static const char *MEMORY="mem";
static const char *MEMORY_FREE="free";
static const char *MEMORY_LOW="low";
static const char *WIFI="wifi";
static const char *TASK_STATS="tasks";
static const char *TASK_NAME="name";
static const char *TASK_STACK="stackMinLeft";

#ifdef CONFIG_IDF_TARGET
#define DEVICE_STR CONFIG_IDF_TARGET
#else
#define DEVICE_STR "unknown"
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define MEM_AVAILABLE 96
#elif CONFIG_IDF_TARGET_ESP32
#define MEM_AVAILABLE 320
#else
#define MEM_AVAILABLE 0
#endif

#define DEVICE_PUB_INDEX_INFO        0
#define DEVICE_PUB_INDEX_PROFILE     1
#define DEVICE_PUB_INDEX_TOPICS      2
#define DEVICE_PUB_INDEX_DIAG        3
#define DEVICE_PUB_INDEX_STATUS      4

static iotElement_t deviceElement;

#define DIAG_UPDATE_MS (1000 * 30) // 30 Seconds
static char *diagValue = NULL;
static time_t wifiScanTime = 0;
static uint8_t wifiScanRecordsCount = 0;
static wifi_ap_record_t *wifiScanRecords = NULL;

static const char *version = NULL;
static const char *capabilities = NULL;

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static TaskStatus_t *getTaskStats(unsigned long *nrofTasks);
#endif
static void iotDeviceUpdateDiag(TimerHandle_t xTimer);
static void iotDeviceControl(iotValue_t value);
static char* iotGetAnnouncedTopics(void);
static void iotDeviceElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);
static void iotDeviceOnConnect(int pubId, bool release, iotValueType_t *valueType, iotValue_t *value);
static char* iotDeviceGetDescription(void);
static char *iotDeviceGetInfo(void);
static void iotDeviceWifiScan();

static bool iotElementDescriptionToJson(const iotElementDescription_t *desc, cJSON *object) ;

IOT_DESCRIBE_ELEMENT(
    deviceElementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "info"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "profile"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "topics"),
        IOT_DESCRIBE_PUB(RETAINED, STRING, "diag"),
        IOT_DESCRIBE_PUB(RETAINED, STRING, "status")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(BINARY, IOT_SUB_DEFAULT_NAME)
    )
);

int iotDeviceInit(const char *_version, const char *_capabilities)
{
    version = _version;
    capabilities = _capabilities;
    deviceElement = iotNewElement(&deviceElementDescription, IOT_ELEMENT_FLAGS_DONT_ANNOUNCE,
                                  iotDeviceElementCallback, NULL, "device");
    iotDeviceUpdateDiag(NULL);
    xTimerStart(xTimerCreate("deviceDiag", DIAG_UPDATE_MS / portTICK_RATE_MS, pdTRUE, NULL, iotDeviceUpdateDiag), 0);
    updaterAddStatusCallback(iotDeviceUpdateStatus);
    return 0;
}

void iotDeviceUpdateStatus(char *status)
{
    iotValue_t value;
    value.s = status;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_STATUS, value);
}

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static TaskStatus_t *getTaskStats(unsigned long *nrofTasks)
{
    *nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = malloc(sizeof(TaskStatus_t) * (*nrofTasks));
    if (tasksStatus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        return NULL;
    }
    *nrofTasks = uxTaskGetSystemState( tasksStatus, *nrofTasks, NULL);
    return tasksStatus;
}
#ifdef PRINT_TASK_STATS
static void printTaskStats(TaskStatus_t *tasksStatus, unsigned long nrofTasks)
{
    int i;
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
    printf("Name                : Stack Left\n"
           "--------------------------------\n");
    for (i=0; i < nrofTasks; i++) {
        printf("%-20s: % 10d\n", tasksStatus[i].pcTaskName, tasksStatus[i].usStackHighWaterMark);
    }
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
}
#endif
#endif

static void iotDeviceUpdateDiag(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Diag update starting");
    if (diagValue != NULL) {
        free(diagValue);
    }
    uint32_t free_at_start = esp_get_free_heap_size();
    cJSON *object;
    object = cJSON_CreateObject();
    if (object == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for diag");
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    cJSON_AddUIntToObjectCS(object, UPTIME, tv.tv_sec);

    ESP_LOGI(TAG, "Memory: %d/%d", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    cJSON *mem = cJSON_AddObjectToObjectCS(object, MEMORY);
    if (mem != NULL) {
        cJSON_AddUIntToObjectCS(mem, MEMORY_FREE, esp_get_free_heap_size());
        cJSON_AddUIntToObjectCS(mem, MEMORY_LOW, esp_get_minimum_free_heap_size());
    }
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = getTaskStats(&nrofTasks);
    if (tasksStatus != NULL) {
#ifdef PRINT_TASK_STATS
        printTaskStats(tasksStatus, nrofTasks);
#endif
        cJSON *tasks = cJSON_AddArrayToObjectCS(object, TASK_STATS);

        int i;
        for (i=0; i < nrofTasks; i++) {
            cJSON *task = cJSON_CreateObject();
            if (task == NULL) {
                break;
            }
            if (cJSON_AddStringToObjectCS(task, TASK_NAME, tasksStatus[i].pcTaskName) == NULL) {
                cJSON_Delete(task);
                break;
            }
            if (cJSON_AddUIntToObjectCS(task, TASK_STACK, tasksStatus[i].usStackHighWaterMark) == NULL) {
                cJSON_Delete(task);
                break;
            }
            cJSON_AddItemToArray(tasks, task);
        }
        free(tasksStatus);
    }
#endif

    cJSON *wifi = cJSON_AddObjectToObjectCS(object, WIFI);
    if (wifi != NULL) {
        cJSON_AddIntToObjectCS(wifi, "count", wifiGetConnectionCount());
        if (wifiScanRecords != NULL) {
            uint8_t idx;
            cJSON_AddIntToObjectCS(wifi, "scanAge", tv.tv_sec - wifiScanTime);
            cJSON *results = cJSON_AddArrayToObjectCS(wifi, "scanResults");
            for (idx = 0; idx < wifiScanRecordsCount; idx ++) {
                cJSON *result = cJSON_CreateObject();
                if (result == NULL) {
                    break;
                }
                if (cJSON_AddStringToObjectCS(result, "name", (char *)wifiScanRecords[idx].ssid) == NULL) {
                    cJSON_Delete(result);
                    break;
                }
                if (cJSON_AddIntToObjectCS(result, "rssi", wifiScanRecords[idx].rssi) == NULL) {
                    cJSON_Delete(result);
                    break;
                }
                if (cJSON_AddUIntToObjectCS(result, "channel", wifiScanRecords[idx].primary) == NULL) {
                    cJSON_Delete(result);
                    break;
                }
                cJSON_AddItemToArray(results, result);
            }
        }
    }

    diagValue = cJSON_PrintUnformatted(object);
    uint32_t free_after_format = esp_get_free_heap_size();
    cJSON_Delete(object);
    if (diagValue == NULL) {
        ESP_LOGW(TAG, "Diag failed to allocate memory for formatted string");
        return;
    }
    iotValue_t value;
    value.s = diagValue;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_DIAG, value);
    uint32_t free_at_end = esp_get_free_heap_size();
    ESP_LOGW(TAG, "Diag entry memory: %u @start %u @formatted %u @end", free_at_start, free_after_format, free_at_end);
}

#define RESTART             "restart"
#define SETPROFILE          "setprofile"
#define UPDATE              "update "
#define VALUE_UPDATE_POLICY "valueupdatepolicy "
#define WIFI_SCAN           "wifiscan"
static void iotDeviceControl(iotValue_t value)
{
    if (strcmp(RESTART, (const char *)value.bin->data) == 0) {
        esp_restart();
    } else if (safestrcmp(SETPROFILE, (const char *)value.bin->data, (int)value.bin->len) == 0) {
        char *profile = (char * )(value.bin->data + sizeof(SETPROFILE));
        if (deviceProfileSetProfile(profile) == 0) {
            esp_restart();
        }
    } else if (strncmp(UPDATE, (const char *)value.bin->data, sizeof(UPDATE) - 1) == 0) {
        char *version = (char *)value.bin->data + sizeof(UPDATE) - 1 /* remove the \0 */;
        for (; isspace((int)*version) && *version != 0; version ++);
        updaterUpdate(version);
    } else if (strncmp(VALUE_UPDATE_POLICY, (const char *)value.bin->data, sizeof(VALUE_UPDATE_POLICY) - 1) == 0) {
        char *policy = (char *)value.bin->data + sizeof(VALUE_UPDATE_POLICY) - 1 /* remove the \0 */;
        for (; isspace((int)*policy) && *policy != 0; policy ++);
        ESP_LOGE(TAG, "Policy: %s", policy);
        if (strcasecmp(policy, "always") == 0) {
            ESP_LOGE(TAG, "Policy updated to always");
            iotSetValueUpdatePolicy(IOT_VALUE_UPDATE_POLICY_ALWAYS);
        } else if (strcasecmp(policy, "onchange") == 0) {
            ESP_LOGE(TAG, "Policy updated to on change");
            iotSetValueUpdatePolicy(IOT_VALUE_UPDATE_POLICY_ON_CHANGE);
        }
    } else if (strncmp(WIFI_SCAN, (const char *)value.bin->data, sizeof(WIFI_SCAN) - 1) == 0) {
        iotDeviceWifiScan();
    }
}

static void iotDeviceElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    switch(reason) {
    case IOT_CALLBACK_ON_CONNECT:
        iotDeviceOnConnect(details->index, false, &details->valueType, &details->value);
        break;
    case IOT_CALLBACK_ON_CONNECT_RELEASE:
        iotDeviceOnConnect(details->index, true, &details->valueType, &details->value);
        break;
    case IOT_CALLBACK_ON_SUB:
        iotDeviceControl(details->value);
    default:
        break;
    }
}

static void iotDeviceOnConnect(int pubId, bool release, iotValueType_t *valueType, iotValue_t *value)
{
    switch(pubId) {
    case DEVICE_PUB_INDEX_PROFILE:
        if (!release) {
            *valueType = IOT_VALUE_TYPE_STRING;
            if (deviceProfileGetProfile(&value->s) != 0) {
                value->s = "";
            }
        }
        break;
    case DEVICE_PUB_INDEX_TOPICS:
        if (release) {
            if (value->s) {
                free((char*)value->s);
            }
        } else {
            *valueType = IOT_VALUE_TYPE_STRING;
            value->s = iotGetAnnouncedTopics();
        }
        break;
    case DEVICE_PUB_INDEX_INFO:
        if (release) {
            if (value->s) {
                free((char*)value->s);
            }
        } else {
            *valueType = IOT_VALUE_TYPE_STRING;
            value->s = iotDeviceGetInfo();
        }
        break;
    default:
        ESP_LOGE(TAG, "Unexpected pub id (%d) in onConnect!", pubId);
        break;
    }
}

static char* iotGetAnnouncedTopics(void)
{
    int i, nrofElements = 0;
    char *json = NULL;
    iotElementIterator_t iterator = IOT_ELEMENT_ITERATOR_START;
    iotElement_t element;
    iotElementDescription_t const **descriptions;
    int nrofDescriptions = 0;
    cJSON *object = NULL, *descriptorsArray, *elementsArray;

    while(iotElementIterate(&iterator, true, &element)) {
        nrofElements++;
    }

    descriptions = calloc(nrofElements, sizeof(iotElementDescription_t *));
    if (descriptions == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for descriptions");
        goto error;
    }
    object = cJSON_CreateObject();
    if (object == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for object");
        goto error;
    }

    descriptorsArray = cJSON_AddArrayToObjectCS(object, "descriptions");
    if (descriptorsArray == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for descriptions");
        goto error;
    }

    elementsArray = cJSON_AddArrayToObjectCS(object, "elements");
    if (elementsArray == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for elements");
        goto error;
    }

    iterator = IOT_ELEMENT_ITERATOR_START;
    while(iotElementIterate(&iterator, true, &element)) {
        bool found = false;
        int foundIdx = 0;
        char *elementName = iotElementGetName(element);
        const iotElementDescription_t *desc = iotElementGetDescription(element);

        for (i = 0; i < nrofDescriptions; i++) {
            if (descriptions[i] == desc) {
                found = true;
                foundIdx = i;
                break;
            }
        }
        if (!found) {

            descriptions[nrofDescriptions] = desc;
            foundIdx = nrofDescriptions;
            nrofDescriptions ++;
            cJSON *description = cJSON_CreateObject();
            if (description == NULL) {
                ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for description");
                goto error;
            }
            cJSON_AddItemToArray(descriptorsArray, description);

            if (!iotElementDescriptionToJson(desc, description)) {
                goto error;
            }
        }

        cJSON *element = cJSON_CreateObject();
        if (element == NULL) {
            ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for element");
            goto error;
        }
        cJSON_AddItemToArray(elementsArray, element);
        if (cJSON_AddStringReferenceToObjectCS(element, "name", elementName) == NULL) {
            ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for element name");
            goto error;
        }
        if (cJSON_AddUIntToObjectCS(element, "index", (uint32_t)foundIdx) == NULL) {
            ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for element index");
            goto error;
        }
    }

    json = cJSON_PrintUnformatted(object);
    ESP_LOGI(TAG, "JSON: %s", json);
error:

    if (descriptions) {
        free(descriptions);
    }

    if (object) {
        cJSON_Delete(object);
    }
    return json;
}

static char* iotDeviceGetDescription(void)
{
    esp_err_t err;
    nvs_handle handle;
    char *desc = NULL;

    err = nvs_open("thing", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open thing section (%d)", err);
        return NULL;
    }
    err = nvs_get_str_alloc(handle, DESC, &desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get description (%d)", err);
        return NULL;
    }
    return desc;
}

static char *iotDeviceGetInfo(void)
{
    cJSON *object = cJSON_CreateObject();
    if (object == NULL) {
        return NULL;
    }

    cJSON_AddStringReferenceToObjectCS(object, IP, wifiGetIPAddrStr());
    cJSON_AddStringReferenceToObjectCS(object, VERSION, version);
    cJSON_AddStringReferenceToObjectCS(object, DEVICE, DEVICE_STR);
    cJSON_AddUIntToObjectCS(object, MEMORY, MEM_AVAILABLE);
    cJSON_AddStringReferenceToObjectCS(object, CAPABILITIES, capabilities);

    char *desc = iotDeviceGetDescription();
    cJSON_AddStringToObjectCS(object, DESCRIPTION, desc);
    free(desc);

    char *result = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    return result;
}

static bool iotElementDescriptionToJson(const iotElementDescription_t *desc, cJSON *object)
{
    cJSON *pub = cJSON_AddObjectToObjectCS(object, "pub");
    if (pub == NULL) {
        ESP_LOGW(TAG, "iotElementDescriptionToJson: Failed to allocate memory for pubs");
        return false;
    }

    int psIndex;

    for (psIndex = 0; psIndex < desc->nrofPubs; psIndex++) {
        if (cJSON_AddUIntToObjectCS(pub, desc->pubs[psIndex].name, desc->pubs[psIndex].type) == NULL) {
            ESP_LOGW(TAG, "iotElementDescriptionToJson: Failed to allocate memory for pub type");
            return false;
        }
    }

    cJSON *sub = cJSON_AddObjectToObjectCS(object, "sub");
    if (sub == NULL) {
        ESP_LOGW(TAG, "iotElementDescriptionToJson: Failed to allocate memory for subs");
        return false;
    }

    for (psIndex = 0; psIndex < desc->nrofSubs; psIndex++) {
        const char *name = desc->subs[psIndex].name;
        if ((name == NULL) || (name[0] == 0)) {
            name = IOT_DEFAULT_CONTROL_STR;
        }

        if (cJSON_AddUIntToObjectCS(sub, name, desc->subs[psIndex].type) == NULL) {
            ESP_LOGW(TAG, "iotElementDescriptionToJson: Failed to allocate memory for sub type");
            return false;
        }
    }

    return true;
}

static void iotDeviceWifiScanResult(uint8_t nrofAPs, wifi_ap_record_t *records)
{
    ESP_LOGI(TAG, "Scan completed %u APs found", nrofAPs);
    uint8_t idx;
    for (idx = 0; idx < nrofAPs; idx ++) {
        ESP_LOGI(TAG, "%u) %s (%02x:%02x:%02x:%02x:%02x:%02x) rssi %d channel %u", idx, records[idx].ssid,
                 records[idx].bssid[0], records[idx].bssid[1], records[idx].bssid[2], records[idx].bssid[3],
                 records[idx].bssid[4], records[idx].bssid[5], records[idx].rssi, records[idx].primary);
    }

    if (records != NULL) {
        wifi_ap_record_t *prev = wifiScanRecords;
        wifiScanRecordsCount = 0;
        wifiScanRecords = records;
        wifiScanRecordsCount = nrofAPs;
        if (prev != NULL) {
            free(prev);
        }
    }
}

static void iotDeviceWifiScan()
{
    wifiScan(iotDeviceWifiScanResult);
}