#include <stdlib.h>
#include "utils.h"

esp_err_t nvs_get_str_alloc(nvs_handle handle, const char* key, char** out_value)
{
    size_t len;
    esp_err_t err;
    char *out;

    err = nvs_get_str(handle, key, NULL, &len);
    if (err != ESP_OK) {
        return err;
    }
    out = malloc(len);
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = nvs_get_str(handle, key, out, &len);
    if (err != ESP_OK) {
        free(out);
        return err;
    }
    *out_value = out;
    return ESP_OK;
}

esp_err_t nvs_get_blob_alloc(nvs_handle handle, const char* key, void** out_value, size_t* length)
{
    esp_err_t err;
    void *out;

    err = nvs_get_blob(handle, key, NULL, length);
    if (err != ESP_OK) {
        return err;
    }
    out = malloc(*length);
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = nvs_get_blob(handle, key, out, length);
    if (err != ESP_OK) {
        free(out);
        return err;
    }
    *out_value = out;
    return ESP_OK;

}