
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#ifdef CONFIG_IDF_TARGET_ESP8266
#include "esp_event_loop.h"
#include "tcpip_adapter.h"
#elif CONFIG_IDF_TARGET_ESP32
#include "esp_event.h"
#include "esp_netif.h"
#endif

#include "wifi.h"
#include "sdkconfig.h"
#include "notifications.h"
#include "utils.h"

#define UNIQ_NAME_PREFIX  "homething-"
#define MAC_STR "%02x%02x%02x%02x%02x%02x"
#define UNIQ_NAME_LEN (sizeof(UNIQ_NAME_PREFIX) + (6 * 2)) /* 6 bytes for mac address as 2 hex chars */

#define SECS_BEFORE_AP 30 /* Number of seconds attempting to connect to an SSID before starting an AP */
#define SECS_BEFORE_REBOOT (60 * 5) /* Number of seconds before rebooting */

#define MAX_LENGTH_WIFI_NAME 32
#define MAX_LENGTH_WIFI_PASSWORD 64

static const char *TAG="WIFI";

static char *wifiSsid = NULL;
static char *wifiPassword = NULL;
static char ipAddr[16]; // ddd.ddd.ddd.ddd\0
static bool connected = false;
static time_t disconnectedSeconds = 0;

static void wifiDriverInit(void);
static void wifiStartStation(void);
static void wifiSetupStation(void);
static void wifiSetupAP(bool andStation);

static void getUniqName(char *name);

int wifiInit(void)
{
    int result = 0;
    esp_err_t err;
    nvs_handle handle;
    char hostname[UNIQ_NAME_LEN];

    sprintf(ipAddr, IPSTR, 0, 0, 0, 0);
    err = nvs_open("wifi", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        if (nvs_get_str_alloc(handle, "ssid", &wifiSsid) == ESP_OK) {
            if (nvs_get_str_alloc(handle, "pass", &wifiPassword) != ESP_OK) {
                free(wifiSsid);
                wifiSsid = NULL;
            }
        }
        nvs_close(handle);
    }

    getUniqName(hostname);

    wifiDriverInit();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    return result;
}

#ifdef CONFIG_IDF_TARGET_ESP8266
static esp_err_t wifiEventHandler(void *ctx, system_event_t *event)
{
    struct timeval tv;
    char hostname[UNIQ_NAME_LEN];
    NotificationsData_t notification;
    esp_err_t err;
    wifi_sta_list_t staList;

    gettimeofday(&tv, NULL);
    ESP_LOGI(TAG, "System Event: %d (secs %ld disconnectedSeconds %ld)", event->event_id, tv.tv_sec, disconnectedSeconds);
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        connected = false;
        disconnectedSeconds = tv.tv_sec;
        getUniqName(hostname);
        err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, \"%s\") == %d", hostname, err);
        }
        notification.connectionState = Notifications_ConnectionState_Connecting;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);

        wifiStartStation();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        sprintf(ipAddr, IPSTR, IP2STR(&event->event_info.got_ip.ip_info.ip));
        ESP_LOGI(TAG, "Connected to SSID, IP=%s", ipAddr);
        connected = true;
        disconnectedSeconds = 0;
        notification.connectionState = Notifications_ConnectionState_Connected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        sprintf(ipAddr, IPSTR, 0, 0, 0, 0);
        wifiStartStation();
        if (connected) {
            connected = false;
            notification.connectionState = Notifications_ConnectionState_Disconnected;
            notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);

            disconnectedSeconds = tv.tv_sec;
        } else {
            if ((disconnectedSeconds + SECS_BEFORE_AP) <= tv.tv_sec) {
                wifi_mode_t mode = WIFI_MODE_NULL;
                esp_wifi_get_mode(&mode);
                if (mode != WIFI_MODE_APSTA) {
                    ESP_LOGI(TAG, "Timeout connecting to SSID, enabling AP...");
                    wifiSetupAP(true);
                }
                if ((disconnectedSeconds + SECS_BEFORE_REBOOT) <tv.tv_sec) {
                    ESP_LOGI(TAG, "Timeout connecting to SSID, rebooting...");
                    esp_restart();
                }
            }
        }
        break;
    case SYSTEM_EVENT_AP_START:
        getUniqName(hostname);
        err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, hostname);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_AP, \"%s\") == %d", hostname, err);
        }
        notification.connectionState = Notifications_ConnectionState_Connecting;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        staList.num = 0;
        if ((esp_wifi_ap_get_sta_list(&staList) == ESP_OK) && (staList.num == 1)) {
            notification.connectionState = Notifications_ConnectionState_Connected;
            notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
        }
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        staList.num = 0;
        if ((esp_wifi_ap_get_sta_list(&staList) == ESP_OK) && (staList.num == 0)) {
            notification.connectionState = Notifications_ConnectionState_Disconnected;
            notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);

            notification.connectionState = Notifications_ConnectionState_Connecting;
            notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void wifiDriverInit(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifiEventHandler, NULL));
}
#elif CONFIG_IDF_TARGET_ESP32

static esp_netif_t *stationNetif = NULL, *apNetif = NULL;

static void wifiEventStationStart(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct timeval tv;
    char hostname[UNIQ_NAME_LEN];
    NotificationsData_t notification;
    esp_err_t err;

    gettimeofday(&tv, NULL);
    connected = false;
    disconnectedSeconds = tv.tv_sec;
    getUniqName(hostname);
    err = esp_netif_set_hostname(stationNetif, hostname);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_set_hostname(stationNetif, \"%s\") == %d", hostname, err);
    }
    notification.connectionState = Notifications_ConnectionState_Connecting;
    notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);

    wifiStartStation();
}

static void wifiEventStationGotIP(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    NotificationsData_t notification;
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

    sprintf(ipAddr, IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Connected to SSID, IP=%s", ipAddr);
    connected = true;
    disconnectedSeconds = 0;
    notification.connectionState = Notifications_ConnectionState_Connected;
    notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);
}

static void wifiEventStationDisconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct timeval tv;
    NotificationsData_t notification;

    gettimeofday(&tv, NULL);
    sprintf(ipAddr, IPSTR, 0, 0, 0, 0);
    wifiStartStation();
    if (connected) {
        connected = false;
        notification.connectionState = Notifications_ConnectionState_Disconnected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, &notification);

        disconnectedSeconds = tv.tv_sec;
    } else {
        if ((disconnectedSeconds + SECS_BEFORE_AP) <= tv.tv_sec) {
            wifi_mode_t mode = WIFI_MODE_NULL;
            esp_wifi_get_mode(&mode);
            if (mode != WIFI_MODE_APSTA) {
                ESP_LOGI(TAG, "Timeout connecting to SSID, enabling AP...");
                wifiSetupAP(true);
            }
            if ((disconnectedSeconds + SECS_BEFORE_REBOOT) <tv.tv_sec) {
                ESP_LOGI(TAG, "Timeout connecting to SSID, rebooting...");
                esp_restart();
            }
        }
    }
}

static void wifiEventAPStart(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    struct timeval tv;
    char hostname[UNIQ_NAME_LEN];
    NotificationsData_t notification;
    esp_err_t err;

    gettimeofday(&tv, NULL);

    getUniqName(hostname);
    err = esp_netif_set_hostname(apNetif, hostname);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_set_hostname(apNetif, \"%s\") == %d", hostname, err);
    }
    notification.connectionState = Notifications_ConnectionState_Connecting;
    notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
}

static void wifiEventAPStationIPAssigned(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    NotificationsData_t notification;
    wifi_sta_list_t staList;

    staList.num = 0;
    if ((esp_wifi_ap_get_sta_list(&staList) == ESP_OK) && (staList.num == 1)) {
        notification.connectionState = Notifications_ConnectionState_Connected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
    }
}

static void wifiEventAPStationDisconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    NotificationsData_t notification;
    wifi_sta_list_t staList;

    staList.num = 0;
    if ((esp_wifi_ap_get_sta_list(&staList) == ESP_OK) && (staList.num == 0)) {
        notification.connectionState = Notifications_ConnectionState_Disconnected;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);

        notification.connectionState = Notifications_ConnectionState_Connecting;
        notificationsNotify(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_AP, &notification);
    }
}

static void wifiDriverInit(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    stationNetif = esp_netif_create_wifi(WIFI_IF_STA, (esp_netif_inherent_config_t *)ESP_NETIF_BASE_DEFAULT_WIFI_STA);
    apNetif = esp_netif_create_wifi(WIFI_IF_AP, (esp_netif_inherent_config_t *)ESP_NETIF_BASE_DEFAULT_WIFI_AP);
    esp_wifi_set_default_wifi_sta_handlers();
    esp_wifi_set_default_wifi_ap_handlers();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_START,
                    wifiEventStationStart,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_STA_DISCONNECTED,
                    wifiEventStationDisconnected,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    wifiEventStationGotIP,
                    NULL,
                    NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_AP_START,
                    wifiEventAPStart,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    WIFI_EVENT_AP_STADISCONNECTED,
                    wifiEventAPStationDisconnected,
                    NULL,
                    NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_AP_STAIPASSIGNED,
                    wifiEventAPStationIPAssigned,
                    NULL,
                    NULL));
}
#else
#error "Unknown target!"
#endif

void wifiStart(void)
{
    if (wifiSsid != NULL) {
        wifiSetupStation();
    } else {
        wifiSetupAP(false);
    }

    ESP_ERROR_CHECK( esp_wifi_start() );
}

const char* wifiGetIPAddrStr(void)
{
    return ipAddr;
}

bool wifiIsConnected(void)
{
    return connected;
}

static void wifiSetupStation(void)
{
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strcpy((char *)wifi_config.sta.ssid, wifiSsid);
    strcpy((char *)wifi_config.sta.password, wifiPassword);
    ESP_LOGI(TAG, "Setting WiFi station SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_ps(WIFI_PS_NONE) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
}

static void wifiStartStation(void)
{
    /* Start connection timer */
    ESP_LOGI(TAG, "Starting Wifi Connection...");
    esp_wifi_connect();
}

static void wifiSetupAP(bool andStation)
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
    ESP_ERROR_CHECK( esp_wifi_set_mode(andStation ? WIFI_MODE_APSTA: WIFI_MODE_AP) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config) );
}

static void getUniqName(char *name)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(name, UNIQ_NAME_PREFIX MAC_STR, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
