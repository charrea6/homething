#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <cJSON.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "provisioning.h"
#include "provisioning_int.h"
#include "captdns.h"

#include "wifi.h"
#include "iot.h"

#define MAX_CONTENT_LENGTH 2048

typedef enum FieldType {
    FT_USERNAME,
    FT_PASSWORD,
    FT_HOSTNAME,
    FT_PORT,
    FT_CHECKBOX
}FieldType;

struct variable {
    const char *name;
    FieldType type;
};

struct setting {
    const char *name;
    int nrofVariables;
    struct variable *variables;
};

#include "settings.h"

const char *TAG="CONFIG";

static void addVariable(cJSON *obj, nvs_handle handle, struct variable *var);
static esp_err_t setVariable(nvs_handle handle, struct variable *var, cJSON *obj);
static void reboot(TimerHandle_t xTimer);

esp_err_t provisioningConfigPostHandler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "/config handler read content length %d", req->content_len);
    if (req->content_len > MAX_CONTENT_LENGTH) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_err_t err;
    nvs_handle handle;
    char*  buf = malloc(req->content_len + 1);
    size_t off = 0;
    int    ret,i;

    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
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
    buf[off] = 0;
    cJSON *obj = cJSON_Parse(buf);
    if (obj == NULL) {
        httpd_resp_set_status(req, HTTPD_400);
        httpd_resp_send(req, "Unable to parse json", -1);
        return ESP_FAIL;
    }
    for (i=0; i < nrofSettings; i++) {
        int n;
        err = nvs_open(settings[i].name, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open %s error %d", settings[i].name, err);
            err = ESP_OK;
            break;
        }

        cJSON *settingObj = cJSON_GetObjectItem(obj, settings[i].name);
        if (settingObj) {
            for (n=0; n < settings[i].nrofVariables; n++) {
                err = setVariable(handle, &settings[i].variables[n], settingObj);
            }
        }

        nvs_close(handle);
        if (err != ESP_OK) {
            break;
        }
    }

    cJSON_Delete(obj);
    if (err != ESP_OK) { 
        return httpd_resp_send_500(req);
    }

    httpd_send(req, "Saved, rebooting...", -1);
    xTimerStart(xTimerCreate("REBOOT", 1000 / portTICK_RATE_MS, pdTRUE, NULL, reboot), 0);
    return ESP_OK;
}

esp_err_t provisioningConfigGetHandler(httpd_req_t *req)
{
    int i;
    char *buffer;
    cJSON *respObj;
    esp_err_t err;
    nvs_handle handle;

    respObj = cJSON_CreateObject();
    
    for (i=0; i < nrofSettings; i++) {
        int n;
        err = nvs_open(settings[i].name, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open %s error %d", settings[i].name, err);
            err = ESP_OK;
            continue;
        }
        cJSON *settingObj = cJSON_CreateObject();
        if (settingObj == NULL) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        cJSON_AddItemToObject(respObj, settings[i].name, settingObj);

        for (n=0; n < settings[i].nrofVariables; n++) {
            addVariable(settingObj, handle, &settings[i].variables[n]);
        }

        nvs_close(handle);
        if (err != ESP_OK) {
            break;
        }
    }
    if (err != ESP_OK) { 
        return httpd_resp_send_500(req);
    }

    buffer = cJSON_PrintUnformatted(respObj);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Free heap    : %d", esp_get_free_heap_size());
        ESP_LOGE(TAG, "Min Free heap: %d",esp_get_minimum_free_heap_size());
        return httpd_resp_send_500(req);
    }
    cJSON_Delete(respObj);
    provisioningSetContentType(req, CT_JSON);
    httpd_resp_send(req, buffer, strlen(buffer));
    free(buffer);
    return ESP_OK;
}

static void addVariable(cJSON *obj, nvs_handle handle, struct variable *var) {
    esp_err_t err = ESP_OK;
    switch(var->type) {
        case FT_USERNAME:
        case FT_HOSTNAME: {
            char *value;
            size_t length = 0;
            err = nvs_get_str(handle, var->name, NULL, &length);
            if (err == ESP_OK) {
                value = malloc(length);
                if (value == NULL) {
                    err = ESP_ERR_NO_MEM;
                    break;
                }
                err = nvs_get_str(handle, var->name, value, &length);
                if (err == ESP_OK) {
                    cJSON_AddStringToObject(obj, var->name, value);
                }
                free(value);
            }
        }
        break;
        case FT_PASSWORD:
        /* Not exporting passwords */
        break;
        case FT_PORT: {
            uint16_t value;
            err = nvs_get_u16(handle, var->name, &value);
            if (err == ESP_OK) {
                cJSON_AddNumberToObject(obj, var->name, (double)value);
            }
        }
        break;
        case FT_CHECKBOX: {
            uint8_t value;
            err = nvs_get_u8(handle, var->name, &value);
            if (err == ESP_OK) {
                cJSON_AddBoolToObject(obj, var->name, value);
            }
        }
        break;
        default:
        err = ESP_FAIL;
        break;
    }
    if (err != ESP_OK){
        ESP_LOGW(TAG, "addVariable failed for variable %s type %d err %d", var->name, var->type, err);
    }
}

static esp_err_t setVariable(nvs_handle handle, struct variable *var, cJSON *obj) {
    esp_err_t err = ESP_OK;
    cJSON *varObj = cJSON_GetObjectItem(obj, var->name);
    if (varObj == NULL) {
        return ESP_OK;
    } 

    switch(var->type) {
        case FT_USERNAME:
        case FT_PASSWORD:
        case FT_HOSTNAME: {       
            char *value = cJSON_GetStringValue(varObj);
            if (value) {
                err = nvs_set_str(handle, var->name, value);
            } else {
                ESP_LOGW(TAG, "Variable %s was not a string", var->name);
            }
        }
        break;
        case FT_PORT: {
            if (cJSON_IsNumber(varObj)) {
                uint16_t value = (uint16_t)varObj->valueint;
                err = nvs_set_u16(handle, var->name, value);
            } else {
                ESP_LOGW(TAG, "Variable %s was not a number", var->name);
            }
        }
        break;
        case FT_CHECKBOX: {
            if (cJSON_IsBool(varObj)){
                uint8_t value = (uint8_t) varObj->valueint;
                err = nvs_set_u8(handle, var->name, value);
            } else {
                ESP_LOGW(TAG, "Variable %s was not a bool", var->name);
            }
        }
        break;
        default:
        err = ESP_FAIL;
        break;
    }
    if (err != ESP_OK){
        ESP_LOGW(TAG, "setVariable failed for variable %s type %d err %d", var->name, var->type, err);
    }
    return err;
}

static void reboot(TimerHandle_t xTimer) {
    esp_restart();
}
