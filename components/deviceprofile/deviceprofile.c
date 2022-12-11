#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "utils.h"

#include "deviceprofile.h"

#define SUPPORTED_VERSION 1

static const char TAG[] = "devprofile";
static const char THING[] = "thing";
static const char PROFILE[] = "deviceprofile";
static char *deviceProfile = NULL;

int deviceProfileGetProfile(const char **profile)
{
    nvs_handle handle;
    esp_err_t err;
    if (deviceProfile == NULL) {
        err = nvs_open(THING, NVS_READONLY, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open thing section, err %d", err);
            return -1;
        }

        err = nvs_get_str_alloc(handle, PROFILE, (char**)&deviceProfile);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read device profile, err %d", err);
        }

        nvs_close(handle);
        if (err!= ESP_OK) {
            return -1;
        }
    }
    *profile = deviceProfile;
    return 0;
}

int deviceProfileSetProfile(const char *profile)
{
    nvs_handle handle;
    esp_err_t err;
    int ret = 0;
    err = nvs_open(THING, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open thing section, err %d", err);
        return -1;
    }
    err = nvs_set_str(handle, PROFILE, profile);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set profile str, err %d", err);
        ret = -1;
    }
    nvs_close(handle);
    return ret;
}
