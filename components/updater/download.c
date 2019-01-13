#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mbedtls/md5.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"

#include "iot.h"
#include "updater.h"
#include "http_parser.h"
#include "sdkconfig.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static const char TAG[]="Updater";

#define MAX_HASH_LENGTH 16

static struct Header{
    size_t binLen;
    char digest[MAX_HASH_LENGTH];
} updateHeader;

static size_t updateHeaderBytes = 0;
static size_t updateBinBytes = 0;
static esp_ota_handle_t updateHandle = 0;

#define MAX_RECV_BUFFER_LENGTH 1500
static char recvBuffer[MAX_RECV_BUFFER_LENGTH];

static mbedtls_md5_context updateDigest;

static int connectToServer(char *host, int port)
{
    struct sockaddr_in sAddr;
    int mySocket = -1;
    struct hostent* entry;
    
    entry = gethostbyname(host);
    if (entry != NULL) 
    {
        sAddr.sin_family = AF_INET;
        sAddr.sin_addr.s_addr = ((struct in_addr*)(entry->h_addr))->s_addr;
        sAddr.sin_port = htons(port);

        mySocket = socket(AF_INET, SOCK_STREAM, 0);
        if (mySocket >= 0) 
        {
            if (connect(mySocket, (struct sockaddr*)&sAddr, sizeof(sAddr)) == 0)
            {
                return mySocket;
            }
            else
            {
                close(mySocket);
            }
        }
    }
    return -1;
}

static int sendGet(int sock, char *host, int port, char *path)
{
    char *req;
    int l;
    int r = 0;
    const char *GET_FORMAT =
        "GET %s HTTP/1.0\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: hiot/1.0 esp8266\r\n\r\n";

    l = asprintf(&req,  GET_FORMAT, path, host, port);
    if (l == -1)
    {
        return -1;
    }
    if (send(sock, req, l, 0) == -1)
    {
        r = -1;
    }
    free(req);
    return r;
}

static int httpSendGet(char *host, int port, char *path)
{
    int sock = connectToServer(host, port);
    if (sock == -1)
    {
        return -1;
    }
    if (sendGet(sock, host, port, path) == -1)
    {
        close(sock);
        sock = -1;
    }
    return sock;
}

static int onMessageComplete(http_parser* parser)
{
    bool *done = (bool *)parser->data;
    *done = true;
    return 0;
}

static int onStatus(http_parser* parser, const char *at, size_t length)
{
    if (parser->status_code != HTTP_STATUS_OK)
        {
            ESP_LOGW(TAG, "Download failed %d", parser->status_code);
            updaterUpdateStatusf("Download failed with error code %d", parser->status_code);
            return -1;
        }
    return 0;
}

static int onBody(http_parser* parser, const char *at, size_t length)
{
    int r = 0;
    if (updateHeaderBytes < sizeof(struct Header))
    {
        int toCopy = MIN(sizeof(struct Header) - updateHeaderBytes, length);
        memcpy(&updateHeader, at, toCopy);
        updateHeaderBytes += toCopy;
        at += toCopy;
        length -= toCopy;
        updaterUpdateStatusf("Downloading Header: %d/%d", updateHeaderBytes, sizeof(struct Header));
    }
    
    if(updateHeaderBytes == sizeof(struct Header))
    {
        if (esp_ota_write(updateHandle, at, length) != ESP_OK)
        {
            return -1;
        }
        mbedtls_md5_update_ret(&updateDigest, (unsigned char *)at, length);
        updateBinBytes += length;
        updaterUpdateStatusf("Downloading Bin: %d/%d", updateBinBytes, updateHeader.binLen);
        if (updateBinBytes > updateHeader.binLen)
        {
            ESP_LOGE(TAG, "Have now download more bytes than expected (%d > %d)", updateBinBytes, updateHeader.binLen);
            return -1;
        }
    }

    return r;
}

void updaterUpdate(char *host, int port, char *path)
{
    int sock;
    int err = -1;
    http_parser parser;
    http_parser_settings parserSettings;
    size_t len;
    bool done = false;
    const esp_partition_t *updatePartition;
    unsigned char digest[16];

    updateHeaderBytes = 0;
    updateBinBytes = 0;
    mbedtls_md5_init(&updateDigest);
    mbedtls_md5_starts_ret(&updateDigest);
    
    updatePartition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             updatePartition->subtype, updatePartition->address);

    err = esp_ota_begin(updatePartition, OTA_SIZE_UNKNOWN, &updateHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
        updaterUpdateStatus("esp_ota_begin failed");
        return;
    }

    ESP_LOGI(TAG, "Downloading from %s:%d%s", host, port, path);
    sock = httpSendGet(host, port, path);
    if (sock == -1)
    {
        updaterUpdateStatus("Failed to connect to update host");
        esp_ota_end(updateHandle);
        return;
    }
    ESP_LOGI(TAG, "Connected to update host");

    http_parser_init(&parser, HTTP_RESPONSE);
    parser.data = &done;
    http_parser_settings_init(&parserSettings);
    parserSettings.on_body = onBody;
    parserSettings.on_status = onStatus;
    parserSettings.on_message_complete = onMessageComplete;

    while (!done)
    {
        len = recv(sock, recvBuffer, MAX_RECV_BUFFER_LENGTH, 0);
        if (len == -1)
        {
            goto exit;
        }
        http_parser_execute(&parser, &parserSettings, recvBuffer, len);
        switch (parser.http_errno )
        {
            case HPE_OK: break;
            case HPE_CB_body: done = true; break;
            default:
                goto exit;
        }
    }
    close(sock);

    err = esp_ota_end(updateHandle);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "esp_ota_end failed! err=0x%x", err);
        updaterUpdateStatus("Failed: esp_ota_end");
        return;
    }

    mbedtls_md5_finish_ret(&updateDigest, digest);
    if (memcmp(digest, updateHeader.digest, 16) != 0)
    {
        ESP_LOGE(TAG, "esp_ota_end failed! err=0x%x", err);
        updaterUpdateStatus("Failed: digests don't match");
        return;
    }

    err = esp_ota_set_boot_partition(updatePartition);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", err);
        updaterUpdateStatus("Failed: esp_ota_set_boot_partition");
        return;
    }

    updaterUpdateStatus("Update successful, restarting in 1 second");
    vTaskDelay(1000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "Prepare to restart system!");
    esp_restart();

exit:
    esp_ota_end(updateHandle);
    close(sock);    
}
