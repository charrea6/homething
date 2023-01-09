#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "provisioning.h"
#include "provisioning_int.h"

#include "wifi.h"
#include "iot.h"
#include "utils.h"

#include "cJSON.h"
#include "cJSON_AddOns.h"


#define MAX_CONTENT_LENGTH 1024

typedef enum FieldType {
    FT_USERNAME = 0,
    FT_SSID,
    FT_PASSWORD,
    FT_HOSTNAME,
    FT_PORT,
    FT_CHECKBOX,
    FT_DEVICE_ID,
    FT_STRING,
    FT_CHOICE,
    FT_MAX
} FieldType;

struct variable {
    const char *name;
    FieldType type;
};

struct setting {
    const char *name;
    int nrofVariables;
    struct variable *variables;
};

#include "provisioningSettings.h"

const char *TAG="CONFIG";

static void reboot(TimerHandle_t xTimer);
static bool getVariables(nvs_handle handle, struct setting *setting, cJSON *object);
static char *setVariables(nvs_handle handle, struct setting *setting, cJSON *object);

esp_err_t provisioningConfigPostHandler(httpd_req_t *req)
{
    char *buf = NULL;
    char *errorMsg = NULL;
    ESP_LOGI(TAG, "/config handler read content length %d", req->content_len);
    if (req->content_len > MAX_CONTENT_LENGTH) {
        errorMsg = "config content length too big";
        goto error;
    }

    int i;
    size_t off = 0;
    int    ret;

    buf = malloc(req->content_len + 1);
    if (!buf) {
        errorMsg = "failed to allocate buffer";
        goto error;
    }

    while (off < req->content_len) {
        /* Read data received in the request */
        ret = httpd_req_recv(req, buf + off, req->content_len - off);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free (buf);
            return ESP_FAIL;
        }
        off += ret;
        ESP_LOGI(TAG, "/config handler recv length %d", ret);
    }
    buf[req->content_len] = 0;

    cJSON *object = cJSON_Parse(buf);
    if (object == NULL) {
        errorMsg = "failed to parse buffer";
        goto error;
    }
    free(buf);

    for (i=0; i < nrofSettings; i++) {
        esp_err_t err;
        nvs_handle handle;
        cJSON *settingsGroup = cJSON_GetObjectItem(object, settings[i].name);
        if ((settingsGroup != NULL) && (settingsGroup->type == cJSON_Object)) {
            err = nvs_open(settings[i].name, NVS_READWRITE, &handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open %s error %d", settings[i].name, err);
                errorMsg = "Failed to open NVS";
                goto error;
            }
            errorMsg = setVariables(handle, &settings[i], settingsGroup);
            nvs_close(handle);
            if (errorMsg) {
                goto error;
            }
        }
    }
    ESP_LOGI(TAG, "Finished processing settings, will now reboot");
    httpd_send(req, "Saved", 5);
    xTimerStart(xTimerCreate("REBOOT", 10000 / portTICK_RATE_MS, pdTRUE, NULL, reboot), 0);

    cJSON_Delete(object);
    return ESP_FAIL;

error:
    ESP_LOGE(TAG, "ERROR: %s", errorMsg);
    httpd_resp_set_status(req, HTTPD_400);
    httpd_resp_send(req, errorMsg, -1);

    if (buf) {
        free(buf);
    }
    return ESP_FAIL;
}

esp_err_t provisioningConfigGetHandler(httpd_req_t *req)
{
    char *errorMsg = NULL;
    int i;
    esp_err_t err;
    nvs_handle handle;

    uint32_t free_at_start = esp_get_free_heap_size();
    cJSON *object = NULL;
    object = cJSON_CreateObject();
    if (object == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for settings map");
        errorMsg = "Out of memory";
        goto error;
    }

    for (i=0; i < nrofSettings; i++) {
        cJSON *settingGroup;

        err = nvs_open(settings[i].name, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open %s error %d", settings[i].name, err);
            err = ESP_OK;
            continue;
        }
        settingGroup = cJSON_AddObjectToObjectCS(object, settings[i].name);
        if (settingGroup == NULL) {
            ESP_LOGE(TAG, "Failed to create object for settings group %s", settings[i].name);
            errorMsg = "Out of memory";
            goto error;
        }

        if (!getVariables(handle, &settings[i], settingGroup)) {
            errorMsg = "Failed to add variables";
            goto variableError;
        }
variableError:
        nvs_close(handle);
        if (errorMsg) {
            goto error;
        }
    }
    provisioningSetContentType(req, CT_JSON);
    char *settingsValue = cJSON_PrintUnformatted(object);
    uint32_t free_after_format = esp_get_free_heap_size();
    cJSON_Delete(object);
    if (settingsValue == NULL) {
        ESP_LOGE(TAG, "Failed to format JSON object");
        errorMsg = "Out of memory";
        goto error;
    }
    uint32_t free_at_end = esp_get_free_heap_size();
    ESP_LOGW(TAG, "Config memory: %u @start %u @formatted %u @end", free_at_start, free_after_format, free_at_end);
    httpd_resp_send(req, (const char*)settingsValue, strlen(settingsValue));
    free(settingsValue);

    return ESP_OK;
error:
    ESP_LOGE(TAG, "ERROR: %s", errorMsg);
    httpd_resp_set_status(req, HTTPD_400);
    httpd_resp_send(req, errorMsg, -1);

    if (object) {
        cJSON_Delete(object);
    }
    return ESP_FAIL;
}

static void reboot(TimerHandle_t xTimer)
{
    esp_restart();
}

static char *setVariables(nvs_handle handle, struct setting *setting, cJSON *object)
{
    esp_err_t err = ESP_OK;
    char *errorMsg = NULL;
    int i;
    for (i=0; i < setting->nrofVariables; i ++) {
        struct variable *foundVariable = &setting->variables[i];
        cJSON *value = cJSON_GetObjectItem(object, foundVariable->name);
        if (value == NULL) {
            ESP_LOGI(TAG, "No variable %s defined for group %s", foundVariable->name, setting->name);
            continue;
        }
        switch(foundVariable->type) {
        case FT_STRING:
        case FT_SSID:
        case FT_USERNAME:
        case FT_PASSWORD:
        case FT_HOSTNAME: {
            char *valueStr = cJSON_GetStringValue(value);
            if (valueStr == NULL) {
                errorMsg = "Failed to extract string variable value";
                goto error;
            }
            ESP_LOGI(TAG,"Setting %s to \"%s\"", foundVariable->name, valueStr);
            err = nvs_set_str(handle, foundVariable->name, valueStr);
        }
        break;
        case FT_PORT: {
            if (!cJSON_IsNumber(value)) {
                errorMsg = "Failed to extract int variable value";
                goto error;
            }
            uint16_t valueNumber = (uint16_t) value->valuedouble;
            ESP_LOGI(TAG,"Setting %s to %u", foundVariable->name, valueNumber);
            err = nvs_set_u16(handle, foundVariable->name, valueNumber);
        }
        break;
        case FT_CHECKBOX: {
            if (!cJSON_IsBool(value)) {
                errorMsg = "Failed to extract bool variable value";
                goto error;
            }
            ESP_LOGI(TAG,"Setting %s to %s", foundVariable->name, cJSON_IsTrue(value) ? "true":"false");
            err = nvs_set_u8(handle, foundVariable->name, cJSON_IsTrue(value));
        }
        break;
        case FT_DEVICE_ID:
            break;
        case FT_CHOICE: {
            if (!cJSON_IsNumber(value)) {
                errorMsg = "Failed to extract int variable value";
                goto error;
            }
            int32_t choiceValue = (int32_t) value->valuedouble;
            ESP_LOGI(TAG,"Setting %s to %d", foundVariable->name, choiceValue);
            err = nvs_set_i32(handle, foundVariable->name, choiceValue);
        }
        break;
        default:
            err = ESP_FAIL;
            break;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "setVariable failed for variable %s type %d err %d", foundVariable->name, foundVariable->type, err);
        }
    }
error:
    return errorMsg;
}

static bool getVariables(nvs_handle handle, struct setting *setting, cJSON *object)
{
    int n;

    for (n = 0; n < setting->nrofVariables; n++) {
        esp_err_t err = ESP_OK;
        struct variable *var = &setting->variables[n];
        cJSON *valueObject = NULL;
        switch(var->type) {
        case FT_STRING:
        case FT_SSID:
        case FT_USERNAME:
        case FT_HOSTNAME: {
            char *value;

            err = nvs_get_str_alloc(handle, var->name, &value);
            if (err == ESP_OK) {
                valueObject = cJSON_AddStringToObjectCS(object, var->name, value);
                free(value);
            }
        }
        break;
        case FT_PASSWORD:
            valueObject = object; // Non-null value
            break;
        case FT_PORT: {
            uint16_t value;
            err = nvs_get_u16(handle, var->name, &value);
            if (err == ESP_OK) {
                valueObject = cJSON_AddUIntToObjectCS(object, var->name, (uint32_t)value);
            }
        }
        break;
        case FT_CHECKBOX: {
            uint8_t value = 0;
            err = nvs_get_u8(handle, var->name, &value);
            if (err == ESP_OK) {
                valueObject = cJSON_CreateBool(value);
                if (valueObject != NULL) {
                    cJSON_AddItemToObjectCS(object, var->name, valueObject);
                }
            }
        }
        break;
        case FT_DEVICE_ID: {
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            char macStr[13];
            sprintf(macStr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4],mac[5]);
            valueObject = cJSON_AddStringToObjectCS(object, var->name, macStr);
        }
        break;
        case FT_CHOICE: {
            int32_t value;
            err = nvs_get_i32(handle, var->name, &value);
            if (err == ESP_OK) {
                valueObject = cJSON_AddIntToObjectCS(object, var->name, value);
            }
        }
        break;
        default:
            err = ESP_FAIL;
            break;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "addVariable failed for variable %s type %d err %d", var->name, var->type, err);
        } else {
            if (valueObject == NULL) {
                ESP_LOGW(TAG, "Failed to create json object for %s", var->name);
                goto error;
            }
        }
    }
    return true;
error:
    return false;
}