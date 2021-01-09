#ifndef _UTILS_H_
#define _UTILS_H_
#include "esp_err.h"
#include "nvs_flash.h"

esp_err_t nvs_get_str_alloc(nvs_handle handle, const char* key, char** out_value);
esp_err_t nvs_get_blob_alloc(nvs_handle handle, const char* key, void** out_value, size_t* length);
#endif