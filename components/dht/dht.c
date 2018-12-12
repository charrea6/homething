/*
 * FORKED from :
 *  
 * Part of esp-open-rtos
 * Copyright (C) 2016 Jonathan Hartsuiker (https://github.com/jsuiker)
 * BSD Licensed as described in the file LICENSE
 *
 */
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "driver/gpio.h"

#include "dht.h"

static const char *TAG="DHT";

#define DHT_THREAD_NAME "dht"
#define DHT_THREAD_PRIO 8
#define DHT_THREAD_STACK_WORDS 8192

// DHT timer precision in microseconds
#define DHT_TIMER_INTERVAL 2
#define DHT_DATA_BITS 40

#define DEBUG_DHT
#ifdef DEBUG_DHT
#define debug(fmt, ...) ESP_LOGI("DHT", fmt , ## __VA_ARGS__);
#else
#define debug(fmt, ...) /* (do nothing) */
#endif

/*
 *  Note:
 *  A suitable pull-up resistor should be connected to the selected GPIO line
 *
 *  __           ______          _______                              ___________________________
 *    \    A    /      \   C    /       \   DHT duration_data_low    /                           \
 *     \_______/   B    \______/    D    \__________________________/   DHT duration_data_high    \__
 *
 *
 *  Initializing communications with the DHT requires four 'phases' as follows:
 *
 *  Phase A - MCU pulls signal low for at least 18000 us
 *  Phase B - MCU allows signal to float back up and waits 20-40us for DHT to pull it low
 *  Phase C - DHT pulls signal low for ~80us
 *  Phase D - DHT lets signal float back up for ~80us
 *
 *  After this, the DHT transmits its first bit by holding the signal low for 50us
 *  and then letting it float back high for a period of time that depends on the data bit.
 *  duration_data_high is shorter than 50us for a logic '0' and longer than 50us for logic '1'.
 *
 *  There are a total of 40 data bits transmitted sequentially. These bits are read into a byte array
 *  of length 5.  The first and third bytes are humidity (%) and temperature (C), respectively.  Bytes 2 and 4
 *  are zero-filled and the fifth is a checksum such that:
 *
 *  byte_5 == (byte_1 + byte_2 + byte_3 + btye_4) & 0xFF
 *
*/

/**
 * Wait specified time for pin to go to a specified state.
 * If timeout is reached and pin doesn't go to a requested state
 * false is returned.
 * The elapsed time is returned in pointer 'duration' if it is not NULL.
 */
static bool dht_await_pin_state(uint8_t pin, uint32_t timeout,
        bool expected_pin_state, uint32_t *duration)
{
    for (uint32_t i = 0; i < timeout; i += DHT_TIMER_INTERVAL) {
        // need to wait at least a single interval to prevent reading a jitter
        usleep(DHT_TIMER_INTERVAL);
        if (gpio_get_level(pin) == expected_pin_state) {
            if (duration) {
                *duration = i;
            }
            return true;
        }
    }

    return false;
}

static void setupGpio(int pin)
{
    gpio_config_t config;

    config.pin_bit_mask = 1<<pin;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    config.mode = GPIO_MODE_INPUT;    
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&config);
}

/**
 * Request data from DHT and read raw bit stream.
 * The function call should be protected from task switching.
 * Return false if error occurred.
 */
static inline bool dht_fetch_data(uint8_t pin, bool bits[DHT_DATA_BITS])
{
    uint32_t low_duration;
    uint32_t high_duration, b,c,d = 0;
    bool result = false;

    
    gpio_set_direction(pin, GPIO_MODE_DEF_OUTPUT);
    gpio_set_level(pin, 1);
    vTaskDelay(250 / portTICK_RATE_MS);
    // Phase 'A' pulling signal low to initiate read sequence
    gpio_set_level(pin, 0);
    usleep(1000);

    taskENTER_CRITICAL();
    gpio_set_level(pin, 1);
    gpio_set_direction(pin, GPIO_MODE_DEF_INPUT);
    usleep(40);

    // Step through Phase 'B', 40us
    if (!dht_await_pin_state(pin, 40, false, &b)) {
        debug("Initialization error, problem in phase 'B'");
        goto exit;
    }

    // Step through Phase 'C', 88us
    if (!dht_await_pin_state(pin, 88, true, &c)) {
        debug("Initialization error, problem in phase 'C'");
        goto exit;
    }

    // Step through Phase 'D', 88us
    if (!dht_await_pin_state(pin, 88, false, &d)) {
        debug("Initialization error, problem in phase 'D'");
        goto exit;
    }

    // Read in each of the 40 bits of data...
    for (int i = 0; i < DHT_DATA_BITS; i++) {
        if (!dht_await_pin_state(pin, 1000, true, &low_duration)) {
            debug("LOW bit timeout");
            goto exit;
        }
        if (!dht_await_pin_state(pin, 1000, false, &high_duration)) {
            debug("HIGH bit timeout");
            goto exit;
        }
        bits[i] = high_duration > low_duration;
    }
    result = true;
exit:
    taskEXIT_CRITICAL();
    return result;
}

/**
 * Pack two data bytes into single value and take into account sign bit.
 */
static inline int16_t dht_convert_data(uint8_t msb, uint8_t lsb)
{
    int16_t data;
    data = msb & 0x7F;
    data <<= 8;
    data |= lsb;
    if (msb & BIT(7)) {
        data = 0 - data;       // convert it to negative
    }
    return data;
}

bool dht_read_data(uint8_t pin, int16_t *humidity, int16_t *temperature)
{
    bool bits[DHT_DATA_BITS];
    uint8_t data[DHT_DATA_BITS/8] = {0};
    bool result;

    result = dht_fetch_data(pin, bits);
    if (!result) {
        return false;
    }

    for (uint8_t i = 0; i < DHT_DATA_BITS; i++) {
        // Read each bit into 'result' byte array...
        data[i/8] <<= 1;
        data[i/8] |= bits[i];
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        debug("Checksum failed, invalid data received from sensor");
        return false;
    }

    *humidity = dht_convert_data(data[0], data[1]);
    *temperature = dht_convert_data(data[2], data[3]);

    debug("Sensor data: humidity=%d, temp=%d", *humidity, *temperature);

    return true;
}

static void dhtCallCbs(int nrofEntries, DHT22CBEntry_t *entries, int16_t value)
{
    int i;
    for (i = 0; i < nrofEntries; i++)
    {
        entries[i].cb(entries[i].userData, value);
    }
}

static void dhtThread(void* pvParameters)
{
    bool lastNotSet = true;
    int16_t temp, hum, lastTemp=0, lastHum=0;

    DHT22Sensor_t *sensor = pvParameters;
    ESP_LOGI(TAG, "DHT thread starting, pin %d", sensor->pin);
    while(true)
    {
        bool ok = dht_read_data(sensor->pin, &hum, &temp);
        if (ok)
        {
            if ((lastTemp != temp) || lastNotSet)
            {
                // Update temp callbacks
                dhtCallCbs(sensor->nrofTemperatureCBs, sensor->temperatureCBs, temp);
                lastTemp = temp;
            }
            if ((lastHum != hum) || lastNotSet)
            {
                // Update humidity callbacks
                dhtCallCbs(sensor->nrofHumidityCBs, sensor->humidityCBs, hum);
                lastHum = hum;
            }
            lastNotSet = false;
        }
        vTaskDelay(5000 / portTICK_RATE_MS);  //send every 5 seconds
    }
}


void dht22Init(DHT22Sensor_t *sensor, int8_t pin)
{
    sensor->pin = pin;
    sensor->nrofHumidityCBs = sensor->nrofTemperatureCBs = 0;
    setupGpio(pin);
}

void dht22AddHumidityCallback(DHT22Sensor_t *sensor, DHT22CallBack_t cb, void *userData)
{
    if (sensor->nrofHumidityCBs >= DHT22_MAX_CALLBACKS)
    {
        return;
    }
    sensor->humidityCBs[sensor->nrofHumidityCBs].cb = cb;
    sensor->humidityCBs[sensor->nrofHumidityCBs].userData = userData;
    sensor->nrofHumidityCBs ++;
}

void dht22AddTemperatureCallback(DHT22Sensor_t *sensor, DHT22CallBack_t cb, void *userData)
{
    if (sensor->nrofTemperatureCBs >= DHT22_MAX_CALLBACKS)
    {
        return;
    }
    sensor->temperatureCBs[sensor->nrofTemperatureCBs].cb = cb;
    sensor->temperatureCBs[sensor->nrofTemperatureCBs].userData = userData;
    sensor->nrofTemperatureCBs ++;
}

void dht22Start(DHT22Sensor_t *sensor)
{
    xTaskCreate(dhtThread,
                DHT_THREAD_NAME,
                DHT_THREAD_STACK_WORDS,
                sensor,
                DHT_THREAD_PRIO,
                NULL);
}