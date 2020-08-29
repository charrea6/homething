#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "provisioning.h"
#include "provisioning_int.h"
#include "captdns.h"

#include "wifi.h"
#include "iot.h"

#include "cbor.h"

#define MAX_CONTENT_LENGTH 1024

typedef enum FieldType {
    FT_USERNAME = 0,
    FT_PASSWORD,
    FT_HOSTNAME,
    FT_PORT,
    FT_CHECKBOX,
    FT_MAX
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

#include "provisioningSettings.h"

const char *TAG="CONFIG";

static void reboot(TimerHandle_t xTimer);
static bool getVariables(nvs_handle handle, struct setting *setting, CborEncoder *encoder);
static char *setVariables(nvs_handle handle, struct setting *setting, CborValue *it);

typedef bool (*cborTypeTest)(const CborValue *);

static const cborTypeTest fieldTypeToCborType[FT_MAX] = {
    cbor_value_is_text_string,
    cbor_value_is_text_string,
    cbor_value_is_text_string,
    cbor_value_is_unsigned_integer,
    cbor_value_is_boolean
};

esp_err_t provisioningConfigPostHandler(httpd_req_t *req)
{
    char *buf = NULL;
    char *errorMsg = NULL;
    ESP_LOGI(TAG, "/config handler read content length %d", req->content_len);
    if (req->content_len > MAX_CONTENT_LENGTH) {
        errorMsg = "config content length too big";
        goto error;
    }
    
    nvs_handle handle;
    size_t off = 0;
    int    ret;

    buf = malloc(req->content_len);
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

    CborParser parser;
    CborValue it;
    CborError err = cbor_parser_init((uint8_t*)buf, req->content_len, 0, &parser, &it);
    if (err || (cbor_value_get_type(&it) != CborMapType)) {
        errorMsg = "Failed to parse payload as CBOR or not a map type";
        goto error;
    }
    err = cbor_value_enter_container(&it, &it);
    if (err) {
        errorMsg = "Failed to enter container";
        goto error;
    }

    while (!cbor_value_at_end(&it)) {
        CborError err;
        char *key;
        size_t keyLen;
        int i;

        if (cbor_value_get_type(&it) != CborTextStringType) {
            errorMsg = "Invalid key in map";
            goto error;
        }
        err = cbor_value_dup_text_string(&it, &key, &keyLen, &it);
        if (err) {
            errorMsg = "Failed to extract key";
            goto error;
        }
        struct setting *foundSetting = NULL;
        for (i=0; i < nrofSettings; i++) {
            if (strcmp(key, settings[i].name) == 0) {
                foundSetting = &settings[i];
                break;
            }
        }
        free(key);
        if (foundSetting == NULL) {
            errorMsg = "Unexpected key";
            goto error;
        }
        ESP_LOGI(TAG, "Processing variables for %s", foundSetting->name);
        if (cbor_value_get_type(&it) != CborMapType) {
            errorMsg = "Key value not a map type";
            goto error;
        }
        err = nvs_open(foundSetting->name, NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open %s error %d", foundSetting->name, err);
            errorMsg = "Failed to open NVS";
            goto error;
        }
        CborValue variableIt;
        err = cbor_value_enter_container(&it, &variableIt);
        if (err == CborNoError) {
            errorMsg = setVariables(handle, foundSetting, &variableIt);
        }
        nvs_close(handle);
        if (errorMsg) {
            goto error;
        }
        err = cbor_value_leave_container(&it, &variableIt);
        if (err != CborNoError) {
            errorMsg = "Leave variable container failed";
            goto error;
        }
    }
    ESP_LOGI(TAG, "Finished processing settings, will now reboot");
    httpd_send(req, "Saved", 5);
    xTimerStart(xTimerCreate("REBOOT", 10000 / portTICK_RATE_MS, pdTRUE, NULL, reboot), 0);

    free(buf);
    return ESP_FAIL;

error:
    ESP_LOGE(TAG, "ERROR: %s", errorMsg);
    httpd_resp_set_status(req, HTTPD_400);
    httpd_resp_send(req, errorMsg, -1);
    ESP_LOGE(TAG, errorMsg);

    if (buf) {
        free(buf);
    }
    return ESP_FAIL;
}

esp_err_t provisioningConfigGetHandler(httpd_req_t *req)
{
    char *errorMsg = NULL;
    int i;
    uint8_t *buf = NULL;
    esp_err_t err;
    nvs_handle handle;
    CborEncoder encoder, settingEncoder;
    CborError cborErr;
    buf = malloc(MAX_CONTENT_LENGTH);
    if (buf == NULL) {
        errorMsg = "Failed to allocate cbor buffer";
        goto error;
    }
    cbor_encoder_init(&encoder, buf, MAX_CONTENT_LENGTH, 0);
    cborErr = cbor_encoder_create_map(&encoder, &settingEncoder, CborIndefiniteLength);
    if (cborErr != CborNoError) {
        errorMsg = "Failed to create settings encoder";
        goto error;
    }
    for (i=0; i < nrofSettings; i++) {
        CborEncoder variablesEncoder;

        err = nvs_open(settings[i].name, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open %s error %d", settings[i].name, err);
            err = ESP_OK;
            continue;
        }
        cborErr = cbor_encode_text_stringz(&settingEncoder, settings[i].name);
        if (cborErr != CborNoError) {
            errorMsg = "Failed to add setting to map";
            goto variableError;
        }
        cborErr = cbor_encoder_create_map(&settingEncoder, &variablesEncoder, CborIndefiniteLength);
        if (cborErr != CborNoError) {
            errorMsg = "Failed to create variables encoder";
            goto variableError;
        }
        if (!getVariables(handle, &settings[i], &variablesEncoder)) {
            errorMsg = "Failed to add variables";
            goto variableError;
        }
        cborErr = cbor_encoder_close_container(&settingEncoder, &variablesEncoder);
        if (cborErr != CborNoError) {
            errorMsg = "Failed to close variable container";
        }
variableError:
        nvs_close(handle);
        if (errorMsg) {
            goto error;
        }
    }
    cborErr = cbor_encoder_close_container(&encoder, &settingEncoder);
    if (cborErr != CborNoError) {
        errorMsg = "Failed to close settings container";
        goto error;
    }
    provisioningSetContentType(req, CT_CBOR);
    httpd_resp_send(req, (const char*)buf, cbor_encoder_get_buffer_size(&encoder, buf));
    free(buf);

    return ESP_OK;
error:
    ESP_LOGE(TAG, "ERROR: %s", errorMsg);
    httpd_resp_set_status(req, HTTPD_400);
    httpd_resp_send(req, errorMsg, -1);
    ESP_LOGE(TAG, errorMsg);

    if (buf) {
        free(buf);
    }
    return ESP_FAIL;
}

static void reboot(TimerHandle_t xTimer) {
    esp_restart();
}

static char *setVariables(nvs_handle handle, struct setting *setting, CborValue *it)
{
    esp_err_t err = ESP_OK;
    char *errorMsg = NULL;

    while (!cbor_value_at_end(it)) { 
        CborError cborErr;
        char *key;
        size_t keyLen;
        int i;

        if (cbor_value_get_type(it) != CborTextStringType) {
            ESP_LOGE(TAG, "Was execting string got %d", cbor_value_get_type(it));
            errorMsg = "Invalid variable key in map";
            goto error;
        }
        cborErr = cbor_value_dup_text_string(it, &key, &keyLen, it);
        if (cborErr) {
            errorMsg = "Failed to extract variable key";
            goto error;
        }
        struct variable *foundVariable = NULL;
        for (i=0; i < setting->nrofVariables; i ++){
            if (strcmp(key, setting->variables[i].name) == 0) {
                foundVariable = &setting->variables[i];
                break;
            }
        }
        free(key);
        if (foundVariable == NULL) {
            errorMsg = "Unexpected variable key";
            goto error;
        }
        if (!fieldTypeToCborType[foundVariable->type](it)) {
            ESP_LOGE(TAG, "Unexpected variable type for %s, expected FT type %d got Cbor type %d", foundVariable->name, foundVariable->type, cbor_value_get_type(it));
            errorMsg = "Unexpected variable type";
            goto error;
        }
        switch(foundVariable->type) {
            case FT_USERNAME:
            case FT_PASSWORD:
            case FT_HOSTNAME: {       
                char *value;
                size_t valueLen;
                cborErr = cbor_value_dup_text_string(it, &value, &valueLen, it);
                if (cborErr) {
                    errorMsg = "Failed to extract string variable value";
                    goto error;
                }
                ESP_LOGI(TAG,"Setting %s to \"%s\" (len %u)", foundVariable->name, value, valueLen);
                err = nvs_set_str(handle, foundVariable->name, value);
                free(value);
            }
            break;
            case FT_PORT: {
                uint64_t value;
                cborErr = cbor_value_get_uint64(it, &value);
                if (cborErr) {
                    errorMsg = "Failed to extract int variable value";
                    goto error;
                }
                cbor_value_advance(it);
                ESP_LOGI(TAG,"Setting %s to %u", foundVariable->name, (uint16_t)value);
                err = nvs_set_u16(handle, foundVariable->name, (uint16_t)value);
            }
            break;
            case FT_CHECKBOX: {
                bool value;
                cborErr = cbor_value_get_boolean(it, &value);
                if (cborErr) {
                    errorMsg = "Failed to extract bool variable value";
                    goto error;
                }
                cbor_value_advance(it);
                ESP_LOGI(TAG,"Setting %s to %s", foundVariable->name, value ? "true":"false");
                err = nvs_set_u8(handle, foundVariable->name, value);
            }
            break;
            default:
            err = ESP_FAIL;
            break;
        }
        if (err != ESP_OK){
            ESP_LOGW(TAG, "setVariable failed for variable %s type %d err %d", foundVariable->name, foundVariable->type, err);
        }
    }
error:
    return errorMsg;
}

static bool getVariables(nvs_handle handle, struct setting *setting, CborEncoder *encoder)
{
    int n;
    
    for (n = 0; n < setting->nrofVariables; n++){
        esp_err_t err = ESP_OK;
        CborError cborErr = CborNoError;
        struct variable *var = &setting->variables[n];
        CborEncoder beforeVariable = *encoder;
        cborErr = cbor_encode_text_stringz(encoder, var->name);
        if (cborErr != CborNoError) {
            goto error;
        }
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
                        /* nvs includes the \0 in the length which we don't want for cbor,
                           use strlen to find the true lenght of the string.
                        */
                        cborErr = cbor_encode_text_string(encoder, value, strlen(value));
                    }
                    free(value);
                }
            }
            break;
            case FT_PASSWORD:
            cborErr = cbor_encode_text_string(encoder, "", 0);
            break;
            case FT_PORT: {
                uint16_t value;
                err = nvs_get_u16(handle, var->name, &value);
                if (err == ESP_OK) {
                    cborErr = cbor_encode_uint(encoder, (uint64_t)value);
                }
            }
            break;
            case FT_CHECKBOX: {
                uint8_t value;
                err = nvs_get_u8(handle, var->name, &value);
                if (err == ESP_OK) {
                    cborErr = cbor_encode_boolean(encoder, value?true:false);
                }
            }
            break;
            default:
            err = ESP_FAIL;
            break;
        }
        if (err != ESP_OK){
            ESP_LOGW(TAG, "addVariable failed for variable %s type %d err %d", var->name, var->type, err);
            *encoder = beforeVariable;
        }
        if (cborErr != CborNoError) {
            goto error;
        }
    }
    return true;
error:
    return false;
}