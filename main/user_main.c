#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "MQTTClient.h"

#include "light.h"
#include "switch.h"
#include "iot.h"
#include "dht.h"
#include "motion.h"
#include "humidityfan.h"

static Light_t light0;
static DHT22Sensor_t thSensor;
static Motion_t motion0;
static HumidityFan_t fan0;

static void temperature(void *userData, int16_t tenthsUnit)
{
    printf("Temperature: %d.%d\n", tenthsUnit / 10, tenthsUnit % 10);
}

static void lightSwitchCallback(void *userData, int state)
{
    Light_t *light = userData;
    lightToggle(light);
}

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    iotInit(CONFIG_ROOM);

    lightInit(&light0, 14);
    dht22Init(&thSensor, 5);
    motionInit(&motion0, 13);
    humidityFanInit(&fan0, 14, 75);

    dht22AddTemperatureCallback(&thSensor, temperature, NULL);
    dht22AddHumidityCallback(&thSensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fan0);
    
    switchAdd(4, lightSwitchCallback, &light0);
    switchStart();
    iotStart();
    dht22Start(&thSensor);
}
