#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp8266/gpio_struct.h"
#include "driver/gpio.h"
#include "i2cdev.h"
#include "pcf8574.h"
#include "gpiox.h"

static const char *TAG="GPIOX";

#if CONFIG_GPIOX_NROF_EXPANDERS > 0
static uint8_t expander_pin_settings[CONFIG_GPIOX_NROF_EXPANDERS] = {0};
static i2c_dev_t expander_devices[CONFIG_GPIOX_NROF_EXPANDERS];
#endif

#define BASE_ADDR 0x20

int gpioxInit(void)
{
#if CONFIG_GPIOX_NROF_EXPANDERS > 0
    ESP_ERROR_CHECK(i2cdev_init());
    for (int i=0; i < CONFIG_GPIOX_NROF_EXPANDERS; i++)
    {
        memset(&expander_devices[i], 0, sizeof(i2c_dev_t));

        if (pcf8574_init_desc(&expander_devices[i], 0, BASE_ADDR + (i * 2), CONFIG_GPIOX_SDA_PIN, CONFIG_GPIOX_SCL_PIN) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create PCF8574 device for expander %d", i);
            return 1;
        }
    }
#endif
    return 0;
}

int gpioxSetup(GPIOX_Pins_t *pins, GPIOX_Mode_t mode)
{
    if (pins->pins[0] != 0)
    {
        for (int i=0; i < GPIO_NUM_MAX; i++)
        {
            if (GPIOX_PINS_IS_SET(*pins, i))
            {
                gpio_config_t config;

                config.pin_bit_mask = 1<<i;
                config.intr_type = GPIO_INTR_DISABLE;
                config.pull_down_en = GPIO_PULLDOWN_DISABLE;
                config.pull_up_en = GPIO_PULLUP_DISABLE;

                switch(mode)
                {
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
                if (gpio_config(&config) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Invalid config for pin %d", i);
                    return 1;
                }
            }
        }
    }
#if CONFIG_GPIOX_NROF_EXPANDERS > 0
    if (pins->pins[1] != 0)
    {
        for (int i = 0; i < CONFIG_GPIOX_NROF_EXPANDERS; i ++)
        {
            uint8_t expander_pins = pins->pins[1] >> (8 * i);
            switch(mode)
            {
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
            if (pcf8574_port_write(&expander_devices[i], expander_pin_settings[i]) != ESP_OK)
            {
                ESP_LOGE(TAG, "Pin setup failed for expander %d", i);
                return 1;
            }
        }
    }
#endif
    return 0;   
}

int gpioxGetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values)
{
    GPIOX_PINS_CLEAR_ALL(*values);
    if (pins->pins[0] != 0)
    {
        uint32_t internal_pins = pins->pins[0] & 0xffff;
        if (internal_pins != 0)
        {
            values->pins[0] = GPIO.in & internal_pins;
        }
        if (GPIOX_PINS_IS_SET(*pins, 16))
        {
            if (gpio_get_level(16) == 1)
            {
                GPIOX_PINS_SET(*values, 16);
            }
        }
    }
#if CONFIG_GPIOX_NROF_EXPANDERS > 0
    if (pins->pins[1] != 0)
    {
        for (int i = 0; i < CONFIG_GPIOX_NROF_EXPANDERS; i ++)
        {
            uint8_t expander_pins = pins->pins[1] >> (8 * i);
            uint8_t value;
            if (pcf8574_port_read(&expander_devices[i], &value) != ESP_OK)
            {
                ESP_LOGE(TAG, "Pin read failed for expander %d", i);
                return 1;
            }
            values->pins[1] |= (value & expander_pins) << (i * 8);
        }
    }
#endif
    return 0;
}

int gpioxSetPins(GPIOX_Pins_t *pins, GPIOX_Pins_t *values)
{
    ESP_LOGI(TAG,"Pins 0x%08x 0x%08x Values 0x%08x 0x%08x", pins->pins[0], pins->pins[1],
    values->pins[0], values->pins[1]);
    if (pins->pins[0] != 0)
    {
        for (int i=0; i < GPIO_NUM_MAX; i++)
        {
            if (GPIOX_PINS_IS_SET(*pins, i))
            {
                gpio_set_level(i, GPIOX_PINS_IS_SET(*values, i));
            }
        }
    }
#if CONFIG_GPIOX_NROF_EXPANDERS > 0
    if (pins->pins[1] != 0)
    {
        for (int i = 0; i < CONFIG_GPIOX_NROF_EXPANDERS; i ++)
        {
            uint8_t expander_pins = pins->pins[1] >> (8 * i);
            uint8_t value = ((values->pins[1] >> (8 * i)) & expander_pins) | (expander_pin_settings[i] & ~expander_pins);
            if (pcf8574_port_write(&expander_devices[i], value) != ESP_OK)
            {
                ESP_LOGE(TAG, "Pin write failed for expander %d", i);
                return 1;
            }
            ESP_LOGI(TAG, "Set expander %d pins 0x%02x to 0x%02x", i, expander_pins, value);
            expander_pin_settings[i] = value;
        }
    }
#endif
    return 0;
}