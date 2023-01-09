#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <nvs_flash.h>

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "provisioning.h"
#include "provisioning_int.h"

#include "wifi.h"
#include "utils.h"

#include "cJSON.h"
#include "cJSON_AddOns.h"

static void provisioningWifiScanResult(uint8_t nrofAPs, wifi_ap_record_t *records);
static bool provisioningWifiAPRecordToJson(wifi_ap_record_t *record, cJSON *object);

#define RESCAN_SECONDS 30
static time_t wifiScanTime = - RESCAN_SECONDS;
static uint8_t wifiScanRecordsCount = 0;
static wifi_ap_record_t *wifiScanRecords = NULL;
static bool wifiScanStarted = false;

esp_err_t provisioningWifiScanGetHandler(httpd_req_t *req)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if ((tv.tv_sec - wifiScanTime > RESCAN_SECONDS) && (!wifiScanStarted)) {
        if (wifiScan(provisioningWifiScanResult) == 0) {
            wifiScanStarted = true;
        }
    }

    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        goto error;
    }

    cJSON *networks = cJSON_AddArrayToObjectCS(response, "networks");
    if (networks == NULL) {
        goto error;
    }

    if (wifiScanRecords == NULL) {
        wifi_ap_record_t record;

        if (esp_wifi_sta_get_ap_info(&record) == ESP_OK) {
            cJSON *result = cJSON_CreateObject();
            if (provisioningWifiAPRecordToJson(&record, result)) {
                cJSON_AddItemToArray(networks, result);
            } else {
                cJSON_free(result);
            }
        }
    } else {
        uint8_t idx;
        for (idx = 0; idx < wifiScanRecordsCount; idx ++) {
            cJSON *result = cJSON_CreateObject();
            if (result == NULL) {
                goto error;
            }
            cJSON_AddItemToArray(networks, result);
            if (!provisioningWifiAPRecordToJson(&wifiScanRecords[idx], result)) {
                goto error;
            }
        }
    }
    char *json = cJSON_PrintUnformatted(response);
    if (json == NULL) {
        goto error;
    }

    cJSON_free(response);
    provisioningSetContentType(req, CT_JSON);
    httpd_resp_send(req, (const char*)json, -1);
    free(json);
    return ESP_OK;
error:
    if (response != NULL) {
        cJSON_free(response);
    }
    httpd_resp_set_status(req, HTTPD_500);
    httpd_resp_send(req, "Out of memory", -1);
    return ESP_FAIL;
}


static void provisioningWifiScanResult(uint8_t nrofAPs, wifi_ap_record_t *records)
{
    wifiScanStarted = false;
    if (records != NULL) {
        wifi_ap_record_t *prev = wifiScanRecords;
        wifiScanRecordsCount = 0;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        wifiScanTime = tv.tv_sec;
        wifiScanRecords = records;
        wifiScanRecordsCount = nrofAPs;
        if (prev != NULL) {
            free(prev);
        }
    }
}

static bool provisioningWifiAPRecordToJson(wifi_ap_record_t *record, cJSON *object)
{
    if (cJSON_AddStringToObjectCS(object, "name", (char *)record->ssid) == NULL) {
        return false;
    }
    if (cJSON_AddIntToObjectCS(object, "rssi", record->rssi) == NULL) {
        return false;
    }
    if (cJSON_AddUIntToObjectCS(object, "channel", record->primary) == NULL) {
        return false;
    }
    return true;
}