#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#ifdef CONFIG_IDF_TARGET_ESP8266
#include "esp8266/gpio_struct.h"
#endif

#include "driver/gpio.h"
#include "i2cdev.h"
#include "pcf8574.h"
#include "gpiox.h"


static const char *TAG="GPIOX";

#if CONFIG_GPIOX_EXPANDERS == 1
#define MAX_EXPANDERS 4
#define BASE_ADDR 0x20

static uint8_t nrofExpanders = 0;
static uint8_t expander_pin_settings[MAX_EXPANDERS] = {0};
static i2c_dev_t expander_devices[MAX_EXPANDERS];
#endif

#ifdef CONFIG_IDF_TARGET_ESP8266
#define EXPANDERS_PIN_IDX 1
#define HAS_INTERNAL_PINS_ENABLED(_pins) ((_pins)->pins[0] != 0)
#elif CONFIG_IDF_TARGET_ESP32
#define EXPANDERS_PIN_IDX 2
#define HAS_INTERNAL_PINS_ENABLED(_pins) (((_pins)->pins[0] != 0) || ((_pins)->pins[1] != 0))
#endif


int gpioxInit(void)
{
    int result = 0;
#if CONFIG_GPIOX_EXPANDERS == 1
    nvs_handle handle;
    uint8_t scl, sda;

    esp_err_t err = nvs_open("gpiox", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        err = nvs_get_u8(handle, "num", &nrofExpanders);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to retrieve number of expanders, defaulting to 0: %d", err);
        }

        if (nrofExpanders > MAX_EXPANDERS) {
            ESP_LOGW(TAG, "Number of expanders %d > %d! limiting to %d", nrofExpanders, MAX_EXPANDERS, MAX_EXPANDERS);
            nrofExpanders = MAX_EXPANDERS;
        }
        if (nrofExpanders > 0) {
            err = nvs_get_u8(handle, "scl", &scl);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to retrieve scl: %d", err);
                result = 1;
            }

            err = nvs_get_u8(handle, "sda", &sda);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to retrieve sda: %d", err);
                result = 1;
            }
        }
        nvs_close(handle);
    }
    if ((nrofExpanders > 0) && (result == 0)) {
        for (int i=0; i < nrofExpanders; i++) {
            memset(&expander_devices[i], 0, sizeof(i2c_dev_t));
            err = pcf8574_init_desc(&expander_devices[i], 0, BASE_ADDR + i, sda, scl);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create PCF8574 device for expander %d: %d", i, err);
                result = 1;
                break;
            }
        }
    }
#endif
    return result;
}

int gpioxSetup(GPIOX_Pins_t *pins, GPIOX_Mode_t mode)
{
    ESP_LOGI(TAG,"Setting up Pins 0x%08x 0x%08x Mode %d", pins->pins[0], pins->pins[1], mode);

    if (HAS_INTERNAL_PINS_ENABLED(pins)) {
        for (int i=0; i < GPIO_NUM_MAX; i++) {
            if (GPIOX_PINS_IS_SET(*pins, i)) {
                gpio_config_t config;

#ifdef CONFIG_IDF_TARGET_ESP8266
                config.pin_bit_mask = 1<<i;
#elif CONFIG_IDF_TARGET_ESP32
                config.pin_bit_mask = BIT64(i);
#endif
                config.intr_type = GPIO_INTR_DISABLE;
                config.pull_down_en = GPIO_PULLDOWN_DISABLE;
                config.pull_up_en = GPIO_PULLUP_DISABLE;

                switch(mode) {
                case GPIOX_MODE_OUT:
                    config.mode = GPIO_MODE_OUTPUT;
                    break;
                case GPIOX_MODE_IN_FLOAT:
                    config.mode = GPIO_MODE_INPUT;
                    break;
                case GPIOX_MODE_IN_PULLUP:
                    config.mode = GPIO_MODE_INPUT;
                    config.pull_up_en = GPIO_PULLUP_ENABLE;
                    break;
                case GPIOX_MODE_IN_PULLDOWN:
                    config.mode = GPIO_MODE_INPUT;
                    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
                    break;
                default:
                    return 1;
                }
                if (gpio_config(&config) != ESP_OK) {
                    ESP_LOGE(TAG, "Invalid config for pin %d", i);
                    return 1;
                }
            }
        }
    }
#if CONFIG_GPIOX_EXPANDERS == 1
    if (nrofExpanders > 0) {
        if (pins->pins[1] != 0) {
            for (int i = 0; i < nrofExpanders; i ++) {
                uint8_t expander_pins = pins->pins[EXPANDERS_PIN_IDX] >> (8 * i);
                switch(mode) {
                case GPIOX_MODE_OUT:
                    expander_pin_settings[i] &= ~expander_pins;
                    break;
                case GPIOX_MODE_IN_PULLUP:
                    expander_pin_settings[i] |= expander_pins;
                    break;
                default:
                    return 1;
                }
                ESP_LOGI(TAG, "Setup expander %d pins 0x%02x to 0x%02x", i, expander_pins, expander_pin_settings[i]);
                if (pcf8574_port_write(&expander_devices[i], expander_pin_settings[i]) != ESP_OK) {
                    ESP_LOGE(TAG, "Pin setup failed for expander %d", i);
                    return 1;
                }
            }
        }
    }
#endif
    return 0;
}

int gpioxGetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values)
{
    GPIOX_PINS_CLEAR_ALL(*values);
    if (HAS_INTERNAL_PINS_ENABLED(pins)) {
#ifdef CONFIG_IDF_TARGET_ESP8266
        uint32_t internal_pins = pins->pins[0] & 0xffff;
        if (internal_pins != 0) {
            values->pins[0] = GPIO.in & internal_pins;
        }
        if (GPIOX_PINS_IS_SET(*pins, 16)) {
            if (gpio_get_level(16) == 1) {
                GPIOX_PINS_SET(*values, 16);
            }
        }
#elif CONFIG_IDF_TARGET_ESP32
        for (int i=0; i < GPIO_NUM_MAX; i++) {
            if (GPIOX_PINS_IS_SET(*pins, i)) {
                if (gpio_get_level(i) == 1) {
                    GPIOX_PINS_SET(*values, i);
                }
            }
        }               
#endif
    }
#if CONFIG_GPIOX_EXPANDERS == 1
    if (nrofExpanders > 0) {
        if (pins->pins[EXPANDERS_PIN_IDX] != 0) {
            for (int i = 0; i < nrofExpanders; i ++) {
                uint8_t expander_pins = pins->pins[1] >> (8 * i);
                uint8_t value;
                if (pcf8574_port_read(&expander_devices[i], &value) != ESP_OK) {
                    ESP_LOGE(TAG, "Pin read failed for expander %d", i);
                    return 1;
                }
                values->pins[1] |= (value & expander_pins) << (i * 8);
            }
        }
    }
#endif
    return 0;
}

int gpioxSetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values)
{
    if (HAS_INTERNAL_PINS_ENABLED(pins)) {
        for (int i=0; i < GPIO_NUM_MAX; i++) {
            if (GPIOX_PINS_IS_SET(*pins, i)) {
                gpio_set_level(i, GPIOX_PINS_IS_SET(*values, i));
            }
        }
    }
#if CONFIG_GPIOX_EXPANDERS == 1
    if (nrofExpanders > 0) {
        if (pins->pins[EXPANDERS_PIN_IDX] != 0) {
            for (int i = 0; i < nrofExpanders; i ++) {
                uint8_t expander_pins = pins->pins[EXPANDERS_PIN_IDX] >> (8 * i);
                uint8_t value = ((values->pins[EXPANDERS_PIN_IDX] >> (8 * i)) & expander_pins) | (expander_pin_settings[i] & ~expander_pins);
                if (pcf8574_port_write(&expander_devices[i], value) != ESP_OK) {
                    ESP_LOGE(TAG, "Pin write failed for expander %d", i);
                    return 1;
                }
                ESP_LOGI(TAG, "Set expander %d pins 0x%02x to 0x%02x", i, expander_pins, value);
                expander_pin_settings[i] = value;
            }
        }
    }
#endif
    return 0;
}