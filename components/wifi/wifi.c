
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "driver/gpio.h"

#include "tcpip_adapter.h"

#include "wifi.h"
#include "sdkconfig.h"

#define UNIQ_NAME_PREFIX  "homething-"
#define MAC_STR "%02x%02x%02x%02x%02x%02x"
#define UNIQ_NAME_LEN (sizeof(UNIQ_NAME_PREFIX) + (6 * 2)) /* 6 bytes for mac address as 2 hex chars */

#define SECS_BEFORE_AP 60 /* Number of seconds attempting to connect to an SSID before starting an AP */

#define MAX_LENGTH_WIFI_NAME 32
#define MAX_LENGTH_WIFI_PASSWORD 64

static const char *TAG="WIFI";

static char wifiSsid[MAX_LENGTH_WIFI_NAME];
static char wifiPassword[MAX_LENGTH_WIFI_PASSWORD];
static char ipAddr[16]; // ddd.ddd.ddd.ddd\0
static WifiConnectionCallback_t connectionCallback;
static TimerHandle_t connectionTimer;

static void wifiStartStation(void);
static void wifiSetupStation(void);
static void wifiSetupAP(void);

static void getUniqName(char *name);
static void onConnectionTimeout(TimerHandle_t xTimer);

int wifiInit(WifiConnectionCallback_t callback)
{
    int result = 0;
    esp_err_t err;
    nvs_handle handle;

    connectionCallback = callback;

    sprintf(ipAddr, IPSTR, 0, 0, 0, 0);
    wifiSsid[0] = 0;
    wifiPassword[0] = 0;
    err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        size_t len = sizeof(wifiSsid);
        if (nvs_get_str(handle, "ssid", wifiSsid, &len) == ESP_OK)
        {
            len = sizeof(wifiPassword);
            if (nvs_get_str(handle, "pass", wifiPassword, &len) != ESP_OK)
            {
                wifiSsid[0] = 0;
            }
        }
        nvs_close(handle);
    }

    connectionTimer = xTimerCreate("WIFICONN", (SECS_BEFORE_AP * 1000) / portTICK_RATE_MS, pdFALSE, NULL, onConnectionTimeout);

    return result;
}

static esp_err_t wifiEventHandler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        wifiStartStation();
        break;
    case SYSTEM_EVENT_STA_GOT_IP: 
        sprintf(ipAddr, IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        ESP_LOGI(TAG, "Connected to SSID, IP=%s", ipAddr);
        if (connectionCallback != NULL) {
            connectionCallback(true);
        }
        xTimerStop(connectionTimer, 0);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        sprintf(ipAddr, IPSTR, 0, 0, 0, 0);
        if (connectionCallback != NULL) {
            connectionCallback(false);
        }
        if (!xTimerIsTimerActive(connectionTimer)) {
            wifiStartStation();
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifiStart(void)
{
    char hostname[UNIQ_NAME_LEN];
    getUniqName(hostname);

    tcpip_adapter_init();
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
    tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname);

    ESP_ERROR_CHECK( esp_event_loop_init(wifiEventHandler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    //wifiSetupAP();
    
    if (wifiSsid[0])
    {
        wifiSetupStation();
        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    }
    else
    {
        ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_AP) );
    }

    ESP_ERROR_CHECK( esp_wifi_start() );
}

void wifiSetSSID(char *ssid, char *password)
{
    wifi_mode_t mode;
    strcpy(wifiSsid, ssid);
    strcpy(wifiPassword, password);

    ESP_ERROR_CHECK( esp_wifi_get_mode(&mode) );
    if (mode == WIFI_MODE_STA)
    {
        wifiSetupStation();
        ESP_ERROR_CHECK( esp_wifi_disconnect());
    }
    else
    {
        wifiSetupStation();
    }
}

const char* wifiGetIPAddrStr(void)
{
    return ipAddr;
}

static void wifiSetupStation(void)
{
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strcpy((char *)wifi_config.sta.ssid, wifiSsid);
    strcpy((char *)wifi_config.sta.password, wifiPassword);
    ESP_LOGI(TAG, "Setting WiFi station SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
}

static void wifiStartStation(void)
{
    /* Start connection timer */
    xTimerReset(connectionTimer, 0);
    esp_wifi_connect();
}

static void wifiSetupAP(void)
{
    char name[UNIQ_NAME_LEN];
    wifi_config_t wifi_config;
    getUniqName(name);
    memset(&wifi_config, 0, sizeof(wifi_config));
    strcpy((char*)wifi_config.ap.ssid, name);
    strcpy((char*)wifi_config.ap.password, name);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;
    ESP_LOGI(TAG, "Setting WiFi AP SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
}


static void getUniqName(char *name)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(name, UNIQ_NAME_PREFIX MAC_STR, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void onConnectionTimeout(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Timeout connecting to SSID, enabling AP...");
    wifiSetupAP();
}