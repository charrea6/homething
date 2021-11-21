#include <stdlib.h>
#include <stdint.h>
#include "esp_log.h"
#include "sdkconfig.h"
#include "iot.h"
#include "led_strip_spi.h"
#include "deviceprofile.h"
#include "notifications.h"
#include "safestring.h"

static const char TAG[] = "led_strip_spi";

#define PUB_IDX_LEDCOUNT   0
#define PUB_IDX_STATE      1
#define PUB_IDX_RGB        2
#define PUB_IDX_BRIGHTNESS 3

struct LEDStrip {
    led_strip_spi_t strip;
    iotElement_t element;
    rgb_t color;
    char colorStr[12]; /* XXX,XXX,XXX\0 */
    uint8_t brightness;
    bool state;
} *ledStrip = NULL;

static void ledStripSPIElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details);
static void ledStripSPIControl(iotValue_t value);
static void ledStripSPIUpdate(void);
static void ledStripSPIUpdateColor(void);

IOT_DESCRIBE_ELEMENT(
    elementDescription,
    IOT_ELEMENT_TYPE_OTHER,
    IOT_PUB_DESCRIPTIONS(
        IOT_DESCRIBE_PUB(RETAINED, INT, "ledcount"),
        IOT_DESCRIBE_PUB(RETAINED, BOOL, "state"),
        IOT_DESCRIBE_PUB(RETAINED, STRING, "rgb"),
        IOT_DESCRIBE_PUB(RETAINED, INT, "brightness")
    ),
    IOT_SUB_DESCRIPTIONS(
        IOT_DESCRIBE_SUB(STRING, IOT_SUB_DEFAULT_NAME)
    )
);

int initLEDStripSPI(int nrofStrips)
{
    esp_err_t err;
    err = led_strip_spi_install();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_spi_install(): %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

Notifications_ID_t addLEDStripSPI(CborValue *entry)
{
    esp_err_t err;
    led_strip_spi_t strip = LED_STRIP_SPI_DEFAULT();
    iotValue_t value;

    if (ledStrip != NULL) {
        ESP_LOGE(TAG, "Only a single LED strip is supported!");
        return NOTIFICATIONS_ID_ERROR;
    }

    if (deviceProfileParserEntryGetUint32(entry, &strip.length)) {
        ESP_LOGE(TAG, "Failed to get number of LEDs!");
        return NOTIFICATIONS_ID_ERROR;
    }

#if HELPER_TARGET_IS_ESP32
    static spi_device_handle_t device_handle;
    strip.device_handle = device_handle;
    strip.max_transfer_sz = LED_STRIP_SPI_BUFFER_SIZE(strip.length);
    strip.clock_speed_hz = 1000000 * 10; // 10Mhz
#endif
#if HELPER_TARGET_IS_ESP8266
    strip.clk_div = SPI_10MHz_DIV;
#endif

    ledStrip = calloc(1, sizeof(struct LEDStrip));
    if (ledStrip == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for led strip");
        return NOTIFICATIONS_ID_ERROR;
    }

    ESP_LOGI(TAG, "Initializing LED strip");
    err = led_strip_spi_init(&strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init led strip, %08x", err);
        free(ledStrip);
        return NOTIFICATIONS_ID_ERROR;
    }
    ledStrip->strip = strip;
    ledStrip->element = iotNewElement(&elementDescription, 0, ledStripSPIElementCallback, ledStrip, "ledStrip");
    value.i = strip.length;
    iotElementPublish(ledStrip->element, PUB_IDX_LEDCOUNT, value);

    value.b = ledStrip->state;
    iotElementPublish(ledStrip->element, PUB_IDX_STATE, value);

    ledStripSPIUpdate();

    value.i = ledStrip->brightness;
    iotElementPublish(ledStrip->element, PUB_IDX_BRIGHTNESS, value);
    ledStripSPIUpdateColor();
    return 0;
}


void ledStripSPIControl(iotValue_t value)
{
    uint32_t red, green, blue, brightness;
    bool currentState = ledStrip->state;
    iotValue_t updateValue;

    if (sscanf(value.s, "color %u,%u,%u", &red, &green, &blue) == 3) {
        ledStrip->color.red = red;
        ledStrip->color.green = green;
        ledStrip->color.blue = blue;
        ledStripSPIUpdate();
        ledStripSPIUpdateColor();
    } else if (sscanf(value.s, "brightness %u", &brightness) == 1) {
        if (brightness > 100) {
            brightness = 100;
        }
        ledStrip->brightness = (uint8_t)brightness;
        ledStripSPIUpdate();
        updateValue.i = ledStrip->brightness;
        iotElementPublish(ledStrip->element, PUB_IDX_BRIGHTNESS, updateValue);
    } else if (strcmp("on", value.s) == 0) {
        if (!ledStrip->state) {
            ledStrip->state = true;
        }
    } else if (strcmp("off", value.s) == 0) {
        if (ledStrip->state) {
            ledStrip->state = false;
        }
    }

    if (currentState != ledStrip->state) {
        ledStripSPIUpdate();
        updateValue.b = ledStrip->state;
        iotElementPublish(ledStrip->element, PUB_IDX_STATE, updateValue);
    }
}

void ledStripSPIUpdate(void)
{
    esp_err_t err;

    uint8_t brightness = ledStrip->state ? ledStrip->brightness: 0;

    err = led_strip_spi_fill_brightness(&ledStrip->strip, 0, ledStrip->strip.length, ledStrip->color, brightness);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set all leds to %d, %d, %d, %d => %08x",
                 ledStrip->color.red, ledStrip->color.green, ledStrip->color.blue, brightness, err);
        return;
    }
    err = led_strip_spi_flush(&ledStrip->strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to flush all leds to %d, %d, %d, %d => %08x",
                 ledStrip->color.red, ledStrip->color.green, ledStrip->color.blue, ledStrip->brightness, err);
        return;
    }
}

void ledStripSPIUpdateColor(void)
{
    iotValue_t value;
    sprintf(ledStrip->colorStr, "%d,%d,%d", ledStrip->color.red, ledStrip->color.green, ledStrip->color.blue);
    value.s = ledStrip->colorStr;
    iotElementPublish(ledStrip->element, PUB_IDX_RGB, value);
}

static void ledStripSPIElementCallback(void *userData, iotElement_t element, iotElementCallbackReason_t reason, iotElementCallbackDetails_t *details)
{
    if (reason == IOT_CALLBACK_ON_SUB) {
        ledStripSPIControl(details->value);
    }
}