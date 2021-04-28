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

static void loggingDriverInit(void);
static void loggingSendBuffer(struct Buffer *buffer);
static void loggingWifiNotification(void *user,  NotificationsMessage_t *message);

static const char TAG[]="logging";
static struct Buffer *buffers;
static int nrofBuffers = 0;
static int currentBuffer = 0;
static uint32_t sequence = 0;

static char *loggingHost;
static uint16_t loggingPort;
static int loggingSocket = -1;
static SemaphoreHandle_t *loggingMutex;


void loggingInit()
{
    nvs_handle handle;
    esp_err_t err;
    uint8_t enabled = 0;
    int i;

    err = nvs_open("log", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return;
    }

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
        loggingDriverInit();
        notificationsRegister(Notifications_Class_Network, NOTIFICATIONS_ID_WIFI_STATION, loggingWifiNotification, NULL);
    }
    nvs_close(handle);
    return;
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
    return;
}

#ifdef CONFIG_IDF_TARGET_ESP8266
static putchar_like_t originalPutChar;

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
static void loggingDriverInit(void)
{
    originalPutChar = esp_log_set_putchar(loggingPutChar);
}
#endif

#if CONFIG_IDF_TARGET_ESP32
static vprintf_like_t originalVprintf;

static int loggingVprintf(const char *fmt, va_list args)
{
    char *output = NULL;
    int r = vasprintf(&output, fmt, args);
    if (output != NULL) {
        if (r) {
            if (xSemaphoreTake(loggingMutex, 10) == pdTRUE) {
                int i;
                for (i=0; i < r; i++) {
                    buffers[currentBuffer].data[HEADER_SIZE + buffers[currentBuffer].len] = output[i];
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
                }
                xSemaphoreGive(loggingMutex);
            }
        }
        free(output);
    }
    return r;
}

static void loggingDriverInit(void)
{
    originalVprintf = esp_log_set_vprintf(loggingVprintf);
}

#endif

static void loggingWifiNotification(void *user,  NotificationsMessage_t *message)
{
    switch(message->data.connectionState) {
    case Notifications_ConnectionState_Connected: {
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
    }
    break;
    case Notifications_ConnectionState_Disconnected:
        closesocket(loggingSocket);
        loggingSocket = -1;
        break;
    default:
        break;
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