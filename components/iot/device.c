
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

#define DEVICE_PUB_INDEX_UPTIME      0
#define DEVICE_PUB_INDEX_IP          1
#define DEVICE_PUB_INDEX_MEM_FREE    2
#define DEVICE_PUB_INDEX_MEM_LOW     3
#define DEVICE_PUB_INDEX_PROFILE     4
#define DEVICE_PUB_INDEX_TOPICS      5
#define DEVICE_PUB_INDEX_DESCRIPTION 6
#define DEVICE_PUB_INDEX_TASK_STATS  7

static iotElement_t deviceElement;

#define UPTIME_UPDATE_MS 5000

static void iotDeviceUpdateUptime(TimerHandle_t xTimer);
static void iotDeviceUpdateMemoryStats(void);
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static uint8_t taskStatsBuffer[1024];
static iotBinaryValue_t taskStatsBinaryValue;
static void iotDeviceUpdateTaskStats(TimerHandle_t xTimer);
#endif
static void iotDeviceControl(void *userData, iotElement_t element, iotValue_t value);
static int iotGetAnnouncedTopics(iotBinaryValue_t *binValue);
static void iotDeviceOnConnect(void *userData, iotElement_t element, int pubId, bool release, iotValueType_t *valueType, iotValue_t *value);
static char* iotDeviceGetDescription(void);

IOT_DESCRIBE_ELEMENT(
    deviceElementDescription,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, INT, "uptime"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "ip"),
        IOT_DESCRIBE_PUB(RETAINED, INT, "memFree"),
        IOT_DESCRIBE_PUB(RETAINED, INT, "memLow"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "profile"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "topics"),
        IOT_DESCRIBE_PUB(RETAINED, ON_CONNECT, "description")
#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
        , IOT_DESCRIBE_PUB(RETAINED, BINARY, "taskStats")
#endif
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(BINARY, IOT_SUB_DEFAULT_NAME, iotDeviceControl)
    )
);

int iotDeviceInit(void)
{
    iotValue_t value;
    deviceElement = iotNewElement(&deviceElementDescription, IOT_ELEMENT_FLAGS_DONT_ANNOUNCE, NULL, "device");
    iotDeviceUpdateMemoryStats();


    value.callback = iotDeviceOnConnect;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_IP, value);
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_PROFILE, value);
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_TOPICS, value);
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_DESCRIPTION, value);
    return 0;
}

void iotDeviceStart(void)
{
    xTimerStart(xTimerCreate("updUptime", UPTIME_UPDATE_MS / portTICK_RATE_MS, pdTRUE, NULL, iotDeviceUpdateUptime), 0);

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    xTimerStart(xTimerCreate("stats", 30*1000 / portTICK_RATE_MS, pdTRUE, NULL, iotDeviceUpdateTaskStats), 0);
#endif
}

static void iotDeviceUpdateMemoryStats(void)
{
    iotValue_t value;
    value.i = (int) esp_get_free_heap_size();
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_MEM_FREE, value);
    value.i = (int)esp_get_minimum_free_heap_size();
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_MEM_LOW, value);
}

static void iotDeviceUpdateUptime(TimerHandle_t xTimer)
{
    iotValue_t value;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    value.i = tv.tv_sec;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_UPTIME, value);

    iotDeviceUpdateMemoryStats();
}

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static void iotDeviceUpdateTaskStats(TimerHandle_t xTimer)
{
    iotValue_t value;
    CborEncoder encoder, taskArrayEncoder, taskEntryEncoder;
    CborError cborErr;
    int i;
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = malloc(sizeof(TaskStatus_t) * nrofTasks);
    if (tasksStatus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        return;
    }
    nrofTasks = uxTaskGetSystemState( tasksStatus, nrofTasks, NULL);
    taskStatsBinaryValue.data = taskStatsBuffer;
    value.bin = &taskStatsBinaryValue;
    cbor_encoder_init(&encoder, taskStatsBuffer, sizeof(taskStatsBuffer), 0);

    cborErr = cbor_encoder_create_array(&encoder, &taskArrayEncoder, nrofTasks);
    if (cborErr != CborNoError) {
        ESP_LOGE(TAG, "Failed to create task stats array");
        goto error;
    }

    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
    printf("Name                : Stack Left\n"
           "--------------------------------\n");

    for (i=0; i < nrofTasks; i++) {
        cborErr = cbor_encoder_create_array(&taskArrayEncoder, &taskEntryEncoder, 2);
        if (cborErr != CborNoError) {
            ESP_LOGE(TAG, "Failed to create task entry array");
            goto error;
        }
        cbor_encode_text_stringz(&taskEntryEncoder, tasksStatus[i].pcTaskName);
        cbor_encode_uint(&taskEntryEncoder, tasksStatus[i].usStackHighWaterMark);
        cbor_encoder_close_container(&taskArrayEncoder, &taskEntryEncoder);
        printf("%-20s: % 10d\n", tasksStatus[i].pcTaskName, tasksStatus[i].usStackHighWaterMark);
    }
    cbor_encoder_close_container(&encoder, &taskArrayEncoder);
    taskStatsBinaryValue.len = cbor_encoder_get_buffer_size(&encoder, taskStatsBuffer);
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_TASK_STATS, value);
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');

    free(tasksStatus);
    return;

error:
    taskStatsBinaryValue.len = 0;
    iotElementPublish(deviceElement, DEVICE_PUB_INDEX_TASK_STATS, value);
    free(tasksStatus);
}
#endif

#define SETPROFILE "setprofile"
static void iotDeviceControl(void *userData, iotElement_t element, iotValue_t value)
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

static void iotDeviceOnConnect(void *userData, iotElement_t element, int pubId, bool release, iotValueType_t *valueType, iotValue_t *value)
{
    switch(pubId) {
    case DEVICE_PUB_INDEX_IP:
        if (!release) {
            *valueType = IOT_VALUE_TYPE_STRING;
            value->s = wifiGetIPAddrStr();
        }
        break;
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
    case DEVICE_PUB_INDEX_DESCRIPTION:
        if (release) {
            if (value->s) {
                free((char *)value->s);
            }
        } else {
            *valueType = IOT_VALUE_TYPE_STRING;
            value->s = iotDeviceGetDescription();
        }
        break;
    default:
        ESP_LOGE(TAG, "Unexpected pub id (%d) in onConnect!", pubId);
        break;
    }
}

static size_t get_cbor_str_requirement(const char *str)
{
    size_t len = strlen(str);
    size_t req = len + 1;
    if (len > 23) {
        req++;
        if (len > 255) {
            req++;
            if (len > 0xffff) {
                ESP_LOGE(TAG, "get_cbor_str_requirement: Very large string! len %u", len);
                req += 2;
            }
        }
    }
    return req;
}

static int iotGetAnnouncedTopics(iotBinaryValue_t *binValue)
{
    int i, nrofElements = 0;
    iotElement_t element = iotElementsHead;
    iotElementDescription_t const **descriptions;
    size_t descriptionsEstimate = 1, elementsEstimate = 1, totalEstimate;
    int nrofDescriptions = 0;
    uint8_t *buffer = NULL;
    CborEncoder encoder, deArrayEncoder, deMapEncoder;

    for (element = iotElementsHead; element; element = element->next) {
        if ((element->flags & IOT_ELEMENT_FLAGS_DONT_ANNOUNCE) != 0) {
            continue;
        }
        nrofElements++;
        // String length + 2 bytes for CBOR encoding of string + 5 bytes for description id
        elementsEstimate += get_cbor_str_requirement(element->name) + 5;
    }

    descriptions = calloc(nrofElements, sizeof(iotElementDescription_t *));
    if (descriptions == NULL) {
        ESP_LOGW(TAG, "iotUpdateAnnouncedTopics: Failed to allocate memory for descriptions");
        goto error;
    }

    for (element = iotElementsHead; element; element = element->next) {
        if ((element->flags & IOT_ELEMENT_FLAGS_DONT_ANNOUNCE) != 0) {
            continue;
        }

        bool found = false;
        size_t estimate = 1 + 1; // 1 byte for array of pubs and subs + 2 for pubs map
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

        if (element->desc->nrofPubs > 23) {
            estimate++;
            if (element->desc->nrofPubs > 255) {
                ESP_LOGE(TAG, "iotUpdateAnnouncedTopics: Too many pubs for %s", element->name);
                goto error;
            }
        }

        for (i = 0; i < element->desc->nrofPubs; i++) {
            // String length + 2 bytes for CBOR encoding of string + 1 byte for type
            estimate += get_cbor_str_requirement(PUB_GET_NAME(element, i)) + 1;
        }

        estimate += 1; // for subs map
        if (element->desc->nrofSubs) {
            if (element->desc->nrofSubs > 23) {
                estimate += 1;
                if (element->desc->nrofSubs > 256) {
                    ESP_LOGE(TAG, "iotUpdateAnnouncedTopics: Too many subs for %s", element->name);
                    goto error;
                }
            }
            for (i = 0; i < element->desc->nrofSubs; i++) {
                const char *name;
                if (element->desc->subs[i].type_name[SUB_INDEX_NAME] == 0) {
                    name = IOT_DEFAULT_CONTROL_STR;
                } else {
                    name = SUB_GET_NAME(element, i);
                }
                // String length + 2 bytes for CBOR encoding of string + 1 byte for type
                estimate += get_cbor_str_requirement(name) + 1;
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
            CHECK_CBOR_ERROR(cbor_encode_text_stringz(&psMapEncoder, &descriptions[i]->pubs[psIndex][PUB_INDEX_NAME]), "encode failed for element name");
            CHECK_CBOR_ERROR(cbor_encode_uint(&psMapEncoder, (uint32_t)VT_BARE_TYPE(descriptions[i]->pubs[psIndex][PUB_INDEX_TYPE])), "encode failed for element type");

        }
        CHECK_CBOR_ERROR(cbor_encoder_close_container(&descriptionArrayEncoder, &psMapEncoder), "Failed to close pubs map");

        CHECK_CBOR_ERROR(cbor_encoder_create_map(&descriptionArrayEncoder, &psMapEncoder, descriptions[i]->nrofSubs), "Failed to create subs map");
        for (psIndex = 0; psIndex < descriptions[i]->nrofSubs; psIndex++) {
            const char *name;
            if (descriptions[i]->subs[psIndex].type_name[SUB_INDEX_NAME] == 0) {
                name = IOT_DEFAULT_CONTROL_STR;
            } else {
                name = &descriptions[i]->subs[psIndex].type_name[SUB_INDEX_NAME];
            }
            CHECK_CBOR_ERROR(cbor_encode_text_stringz(&psMapEncoder, name), "encode failed for element name");
            CHECK_CBOR_ERROR(cbor_encode_uint(&psMapEncoder, (uint32_t)VT_BARE_TYPE(descriptions[i]->subs[psIndex].type_name[SUB_INDEX_TYPE])), "encode failed for element description");
        }
        CHECK_CBOR_ERROR(cbor_encoder_close_container(&descriptionArrayEncoder, &psMapEncoder), "Failed to close subs map");

        CHECK_CBOR_ERROR(cbor_encoder_close_container(&deMapEncoder, &descriptionArrayEncoder), "Failed to close description array");

    }
    CHECK_CBOR_ERROR(cbor_encoder_close_container(&deArrayEncoder, &deMapEncoder), "Failed to close descriptions map");

    // Encode elements: { element name: description ptr ...}
    CHECK_CBOR_ERROR(cbor_encoder_create_map(&deArrayEncoder, &deMapEncoder, nrofElements), "Failed to create elements map");
    for (element = iotElementsHead; element; element = element->next) {
        if ((element->flags & IOT_ELEMENT_FLAGS_DONT_ANNOUNCE) != 0) {
            continue;
        }

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