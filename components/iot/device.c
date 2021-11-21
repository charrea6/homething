
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "cbor.h"
#include "cJSON.h"

#include "iot.h"
#include "iotInternal.h"
#include "wifi.h"
#include "notifications.h"
#include "sdkconfig.h"
#include "deviceprofile.h"
#include "utils.h"
#include "safestring.h"

static const char *TAG="IOT-DEV";
static const char *DESC="desc";
static const char *UPTIME="uptime";
static const char *MEMORY="mem";
static const char *MEMORY_FREE="free";
static const char *MEMORY_LOW="low";
static const char *TASK_STATS="tasks";
static const char *TASK_NAME="name";
static const char *TASK_STACK="stackMinLeft";

#define DEVICE_PUB_INDEX_INFO        0
#define DEVICE_PUB_INDEX_PROFILE     1
#define DEVICE_PUB_INDEX_TOPICS      2
#define DEVICE_PUB_INDEX_DIAG        3

static iotElement_t deviceElement;

#define DIAG_UPDATE_MS (1000 * 30) // 30 Seconds
static char *diagValue = NULL;

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static TaskStatus_t *getTaskStats(unsigned long *nrofTasks);
#endif
static void iotDeviceUpdateDiag(TimerHandle_t xTimer);
static void iotDeviceControl(iotValue_t value);
static int iotGetAnnouncedTopics(iotBinaryValue_t *binValue);
static void iotDeviceElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);
static void iotDeviceOnConnect(int pubId, bool release, iotValueType_t *valueType, iotValue_t *value);
static char* iotDeviceGetDescription(void);
static char *iotDeviceGetInfo(void);

/* CBOR Helper functions */
static size_t getCborStrRequirement(const char *str);
static size_t getCborUintRequirement(const uint value) ;

IOT_DESCRIBE_ELEMENT(
    deviceElementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "info"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "profile"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "topics"),
        IOT_DESCRIBE_PUB(RETAINED, STRING, "diag")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(BINARY, IOT_SUB_DEFAULT_NAME)
    )
);

int iotDeviceInit(void)
{
    deviceElement = iotNewElement(&deviceElementDescription, IOT_ELEMENT_FLAGS_DONT_ANNOUNCE, 
                                  iotDeviceElementCallback, NULL, "device");
    iotDeviceUpdateDiag(NULL);
    return 0;
}

void iotDeviceStart(void)
{
    xTimerStart(xTimerCreate("deviceDiag", DIAG_UPDATE_MS / portTICK_RATE_MS, pdTRUE, NULL, iotDeviceUpdateDiag), 0);
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
#endif
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

static void iotDeviceUpdateDiag(TimerHandle_t xTimer)
{
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
    cJSON_AddNumberToObject(object, UPTIME, (double)tv.tv_sec);

    cJSON *mem = cJSON_AddObjectToObject(object, MEMORY);
    if (mem != NULL) {
        cJSON_AddNumberToObject(mem, MEMORY_FREE, (double)esp_get_free_heap_size());
        cJSON_AddNumberToObject(mem, MEMORY_LOW, (double)esp_get_minimum_free_heap_size());
    }
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = getTaskStats(&nrofTasks);
    if (tasksStatus != NULL) {
#ifdef PRINT_TASK_STATS
        printTaskStats(tasksStatus, nrofTasks);
#endif
        cJSON *tasks = cJSON_AddArrayToObject(object, TASK_STATS);
        
        int i;
        for (i=0; i < nrofTasks; i++) {
            cJSON *task = cJSON_CreateObject();
            if (task == NULL) {
                break;
            }
            if (cJSON_AddStringToObject(task, TASK_NAME, tasksStatus[i].pcTaskName) == NULL){
                cJSON_Delete(task);
                break;
            }
            if (cJSON_AddNumberToObject(task, TASK_STACK, tasksStatus[i].usStackHighWaterMark) == NULL){
                cJSON_Delete(task);
                break;
            }
            cJSON_AddItemToArray(tasks, task);
        }
        free(tasksStatus);
    }
#endif
    diagValue = cJSON_PrintUnformatted(object);
    uint32_t free_after_format = esp_get_free_heap_size();
    cJSON_Delete(object);
    if (diagValue == NULL) {
        return;
    }
    iotValue_t value;
    value.s = diagValue;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_DIAG, value);
    uint32_t free_at_end = esp_get_free_heap_size();
    ESP_LOGW(TAG, "Diag entry memory: %u @start %u @formatted %u @end", free_at_start, free_after_format, free_at_end);
}

#define SETPROFILE "setprofile"
static void iotDeviceControl(iotValue_t value)
{
    if (strcmp("restart", (const char *) value.bin->data) == 0) {
        esp_restart();
    } else if (safestrcmp(SETPROFILE, (const char *) value.bin->data, (int)value.bin->len) == 0) {
        uint8_t *profile = value.bin->data + sizeof(SETPROFILE);
        size_t profileLen = value.bin->len - sizeof(SETPROFILE);
        if (deviceProfileSetProfile(profile, profileLen) == 0) {
            esp_restart();
        }
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
        if (release) {
            if (value->bin) {
                free((void *)value->bin);
            }
        } else {
            iotBinaryValue_t *bin;
            *valueType = IOT_VALUE_TYPE_BINARY;
            bin = malloc(sizeof(iotBinaryValue_t));

            if (bin == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for iotBinaryValue_t");
            } else {
                if (deviceProfileGetProfile(&bin->data, (size_t*)&bin->len) != 0) {
                    free(bin);
                    bin = NULL;
                }
            }
            value->bin = bin;
        }
        break;
    case DEVICE_PUB_INDEX_TOPICS:
        if (release) {
            if (value->bin) {
                free(value->bin->data);
                free((void *)value->bin);
            }
        } else {
            iotBinaryValue_t *bin;
            *valueType = IOT_VALUE_TYPE_BINARY;
            bin = malloc(sizeof(iotBinaryValue_t));

            if (bin == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for iotBinaryValue_t");
            } else {
                if (iotGetAnnouncedTopics(bin) != 0) {
                    free(bin);
                    bin = NULL;
                }
            }
            value->bin = bin;
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

static int iotGetAnnouncedTopics(iotBinaryValue_t *binValue)
{
    int i, nrofElements = 0;
    iotElementIterator_t iterator = IOT_ELEMENT_ITERATOR_START;
    iotElement_t element;
    iotElementDescription_t const **descriptions;
    size_t descriptionsEstimate = 1, elementsEstimate = 1, totalEstimate;
    int nrofDescriptions = 0;
    uint8_t *buffer = NULL;
    CborEncoder encoder, deArrayEncoder, deMapEncoder;

    while(iotElementIterate(&iterator, true, &element)) {
        nrofElements++;
        // String length + 2 bytes for CBOR encoding of string + 5 bytes for description id
        elementsEstimate += getCborStrRequirement(element->name) + 5;
    }

    descriptions = calloc(nrofElements, sizeof(iotElementDescription_t *));
    if (descriptions == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for descriptions");
        goto error;
    }
    
    iterator = IOT_ELEMENT_ITERATOR_START;
    while(iotElementIterate(&iterator, true, &element)) {
        bool found = false;
        size_t estimate = 1; // 1 byte for array of pubs and subs + 2 for pubs map
        for (i = 0; i < nrofDescriptions; i++) {
            if (descriptions[i] == element->desc) {
                found = true;
                break;
            }
        }
        if (found) {
            continue;
        }
        descriptions[nrofDescriptions] = element->desc;
        nrofDescriptions ++;

        if (element->desc->nrofPubs > 255) {
            ESP_LOGE(TAG, "iotUpdateAnnouncedTopics: Too many pubs for %s", element->name);
            goto error;
        }
        if (element->desc->nrofSubs > 256) {
            ESP_LOGE(TAG, "iotUpdateAnnouncedTopics: Too many subs for %s", element->name);
            goto error;
        }
        estimate = getCborUintRequirement(element->desc->nrofPubs);

        for (i = 0; i < element->desc->nrofPubs; i++) {
            // String length + 2 bytes for CBOR encoding of string + 1 byte for type
            estimate += getCborStrRequirement(element->desc->pubs[i].name) + 2;
        }
        estimate += getCborUintRequirement(element->desc->nrofSubs);
        if (element->desc->nrofSubs) {
            for (i = 0; i < element->desc->nrofSubs; i++) {
                const char *name;
                if (element->desc->subs[i].name[0] == 0) {
                    name = IOT_DEFAULT_CONTROL_STR;
                } else {
                    name = element->desc->subs[i].name;
                }
                // String length + 2 bytes for CBOR encoding of string + 1 byte for type
                estimate += getCborStrRequirement(name) + 2;
            }
        }

        // Description array + 5 bytes for CBOR encoding of description id (the ptr value of element->desc)
        descriptionsEstimate += estimate + 5;
    }
    totalEstimate = descriptionsEstimate + elementsEstimate + 1;
    buffer = malloc(totalEstimate);
    if (!buffer) {
        ESP_LOGE(TAG, "iotUpdateAnnouncedTopics: Failed to allocate buffer");
        goto error;
    }
    cbor_encoder_init(&encoder, buffer, totalEstimate, 0);

#define CHECK_CBOR_ERROR(call, message) do{ CborError err = call; if (err != CborNoError){ ESP_LOGE(TAG, message ", error %d", err); goto error;} }while(0)

    CHECK_CBOR_ERROR(cbor_encoder_create_array(&encoder, &deArrayEncoder, 2), "Failed to create descriptions/elements array");

    // Encode descriptions: { description ptr: { topic name: type... } ..}
    CHECK_CBOR_ERROR(cbor_encoder_create_map(&deArrayEncoder, &deMapEncoder, nrofDescriptions), "Failed to create descriptions map");
    for (i = 0; i < nrofDescriptions; i++) {
        CborEncoder descriptionArrayEncoder, psMapEncoder;
        int psIndex;
        CHECK_CBOR_ERROR(cbor_encode_uint(&deMapEncoder, (uint32_t)descriptions[i]), "encode failed for description key");
        CHECK_CBOR_ERROR(cbor_encoder_create_array(&deMapEncoder, &descriptionArrayEncoder, 2), "Failed to create description array");

        CHECK_CBOR_ERROR(cbor_encoder_create_map(&descriptionArrayEncoder, &psMapEncoder, descriptions[i]->nrofPubs), "Failed to create pubs map");
        for (psIndex = 0; psIndex < descriptions[i]->nrofPubs; psIndex++) {
            CHECK_CBOR_ERROR(cbor_encode_text_stringz(&psMapEncoder, descriptions[i]->pubs[psIndex].name), "encode failed for element name");
            CHECK_CBOR_ERROR(cbor_encode_uint(&psMapEncoder, (uint32_t)descriptions[i]->pubs[psIndex].type), "encode failed for element type");

        }
        CHECK_CBOR_ERROR(cbor_encoder_close_container(&descriptionArrayEncoder, &psMapEncoder), "Failed to close pubs map");

        CHECK_CBOR_ERROR(cbor_encoder_create_map(&descriptionArrayEncoder, &psMapEncoder, descriptions[i]->nrofSubs), "Failed to create subs map");
        for (psIndex = 0; psIndex < descriptions[i]->nrofSubs; psIndex++) {
            const char *name;
            if (descriptions[i]->subs[psIndex].name[0] == 0) {
                name = IOT_DEFAULT_CONTROL_STR;
            } else {
                name = descriptions[i]->subs[psIndex].name;
            }
            CHECK_CBOR_ERROR(cbor_encode_text_stringz(&psMapEncoder, name), "encode failed for element name");
            CHECK_CBOR_ERROR(cbor_encode_uint(&psMapEncoder, (uint32_t)descriptions[i]->subs[psIndex].type), "encode failed for element description");
        }
        CHECK_CBOR_ERROR(cbor_encoder_close_container(&descriptionArrayEncoder, &psMapEncoder), "Failed to close subs map");

        CHECK_CBOR_ERROR(cbor_encoder_close_container(&deMapEncoder, &descriptionArrayEncoder), "Failed to close description array");

    }
    CHECK_CBOR_ERROR(cbor_encoder_close_container(&deArrayEncoder, &deMapEncoder), "Failed to close descriptions map");

    // Encode elements: { element name: description ptr ...}
    CHECK_CBOR_ERROR(cbor_encoder_create_map(&deArrayEncoder, &deMapEncoder, nrofElements), "Failed to create elements map");
    
    iterator = IOT_ELEMENT_ITERATOR_START;
    while(iotElementIterate(&iterator, true, &element)) {
        CHECK_CBOR_ERROR(cbor_encode_text_stringz(&deMapEncoder, element->name), "encode failed for element name");
        CHECK_CBOR_ERROR(cbor_encode_uint(&deMapEncoder, (uint32_t)element->desc), "encode failed for element description");
    }
    CHECK_CBOR_ERROR(cbor_encoder_close_container(&deArrayEncoder, &deMapEncoder), "Failed to close elements map");

    CHECK_CBOR_ERROR(cbor_encoder_close_container(&encoder, &deArrayEncoder), "Failed to close descriptions/elements array");
    binValue->data = buffer;
    binValue->len = cbor_encoder_get_buffer_size(&encoder, buffer);
    ESP_LOGI(TAG, "iotUpdateAnnouncedTopics: Estimate %d Used %d", totalEstimate, binValue->len);
    return 0;

error:
    if (descriptions) {
        free(descriptions);
    }
    if (buffer) {
        free(buffer);
    }
    return -1;
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

    cJSON_AddStringToObject(object, "ip", wifiGetIPAddrStr());
    
    char *desc = iotDeviceGetDescription();
    cJSON_AddStringToObject(object, "description", desc);
    free(desc);


    char *result = cJSON_PrintUnformatted(object);
    cJSON_Delete(object);
    printf("Info: %s\n", result);
    return result;
}

static size_t getCborStrRequirement(const char *str)
{
    size_t len = strlen(str);
    size_t req = len + getCborUintRequirement(len);
    if (len > 0xffff) {
        ESP_LOGE(TAG, "getCborStrRequirement: Very large string! len %u", len);
    }
    return req;
}

static size_t getCborUintRequirement(const uint value) 
{
    size_t req = 1;
    if (value > 23) {
        req++;
        if (value > 255) {
            req++;
            if (value > 0xffff) {
                req += 2;
            }
        }
    }
    return req;
}