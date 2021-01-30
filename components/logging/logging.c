#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi.h"
#include "logging.h"
#include "notifications.h"
#include "utils.h"

#define ESP_RETURN_ON_ERR(expr) do { esp_err_t err = expr; if (err != ESP_OK) { ESP_LOGE(TAG, #expr " failed with error %d", err); return -1;}}while(0)

#define MAGIC_SIZE 4
#define SEQUENCE_OFFSET MAGIC_SIZE
#define SEQUENCE_SIZE sizeof(uint32_t)
#define MAC_OFFSET (MAGIC_SIZE + SEQUENCE_SIZE)
#define MAC_SIZE 6
#define HEADER_SIZE (MAGIC_SIZE + SEQUENCE_SIZE + MAC_SIZE)
#define BUFFER_SIZE 1024

#define NROF_BUFFERS 3

struct Buffer {
    char data[HEADER_SIZE + BUFFER_SIZE];

#define BUFFER_READY_TO_SEND 0x80000000
    uint32_t len;
};

static void loggingSendBuffer(struct Buffer *buffer);
static int loggingPutChar(int ch);
static void loggingWifiNotification(void *user,  NotificationsMessage_t *message);

static const char TAG[]="logging";
static struct Buffer *buffers;
static int nrofBuffers = 0;
static int currentBuffer = 0;
static uint32_t sequence = 0;

static char *loggingHost;
static uint16_t loggingPort;
static int loggingSocket = -1;

static putchar_like_t originalPutChar;
static SemaphoreHandle_t *loggingMutex;


int loggingInit()
{
    nvs_handle handle;
    esp_err_t err;
    uint8_t enabled = 0;
    int i;

    ESP_RETURN_ON_ERR(nvs_open("log", NVS_READONLY, &handle));

    if ((nvs_get_u8(handle, "enable", &enabled) == ESP_OK) && enabled) {

        if (nvs_get_str_alloc(handle, "host", &loggingHost) != ESP_OK) {
            goto error;
        }
        uint16_t p;
        err = nvs_get_u16(handle, "port", &p);
        if (err == ESP_OK) {
            loggingPort = (int) p;
        } else {
            loggingPort = 5555;
        }


        buffers = calloc(NROF_BUFFERS, sizeof(struct Buffer));
        if (buffers == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for buffers");
            goto error;
        }

        for (i=0; i < NROF_BUFFERS; i ++) {
            strcpy(buffers[i].data, "LOG");
            esp_read_mac((uint8_t*)&buffers[i].data[MAC_OFFSET], ESP_MAC_WIFI_STA);
        }
        nrofBuffers = NROF_BUFFERS;
        currentBuffer = 0;
        loggingMutex = xSemaphoreCreateMutex();
        if (loggingMutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
            goto error;
        }
        originalPutChar = esp_log_set_putchar(loggingPutChar);
        notificationsRegister(Notifications_Class_Wifi, NOTIFICATIONS_ID_WIFI_STATION, loggingWifiNotification, NULL);
    }
    return 0;
error:
    if (loggingMutex != NULL) {
        vSemaphoreDelete(loggingMutex);
    }
    if (buffers != NULL) {
        free(buffers);
    }
    if (loggingHost != NULL) {
        free(loggingHost);
    }
    nvs_close(handle);
    return -1;
}

static int loggingPutChar(int ch)
{
    if (originalPutChar != NULL) {
        originalPutChar(ch);
    }
    if (xSemaphoreTake(loggingMutex, 10) == pdTRUE) {
        buffers[currentBuffer].data[HEADER_SIZE + buffers[currentBuffer].len] = ch;
        buffers[currentBuffer].len ++;
        if (buffers[currentBuffer].len >= BUFFER_SIZE) {
            uint32_t networkSequence;
            loggingSendBuffer(&buffers[currentBuffer]);
            currentBuffer++;
            if (currentBuffer >= NROF_BUFFERS) {
                currentBuffer = 0;
            }
            sequence++;
            networkSequence = htonl(sequence);
            memcpy(&buffers[currentBuffer].data[SEQUENCE_OFFSET], &networkSequence, sizeof(sequence));
            buffers[currentBuffer].len = 0;
        }
        xSemaphoreGive(loggingMutex);
    }
    return ch;
}

static void loggingWifiNotification(void *user,  NotificationsMessage_t *message)
{
    if (message->data.connectionState) {
        // Get Logging Host address
        loggingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (loggingSocket == -1) {
            printf("Failed to create logging socket!\n");
            return;
        }

        // If successful send pending buffers.
        if (xSemaphoreTake(loggingMutex, 10) == pdTRUE) {
            int i;
            for (i = 0; i < nrofBuffers; i++) {
                if (buffers[i].len & BUFFER_READY_TO_SEND) {
                    loggingSendBuffer(&buffers[i]);
                }
            }
            xSemaphoreGive(loggingMutex);
        }

        // Start timer
    } else {
        closesocket(loggingSocket);
        loggingSocket = -1;
    }
}

static void loggingSendBuffer(struct Buffer *buffer)
{
    if (loggingSocket != -1) {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = inet_addr(loggingHost);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(loggingPort);
        sendto(loggingSocket, buffer->data, buffer->len + HEADER_SIZE, 0, (struct sockaddr *)&destAddr, sizeof(destAddr));
        buffer->len = 0;
    } else {
        buffer->len |= BUFFER_READY_TO_SEND;
    }
}