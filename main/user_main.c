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

static Light_t light0;
static DHT22Sensor_t thSensor;

static void temperature(void *userData, int16_t tenthsUnit)
{
    printf("Temperature: %d.%d\n", tenthsUnit / 10, tenthsUnit % 10);
}


void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    iotInit(CONFIG_ROOM);

    lightInit(&light0, 14);
    dht22Init(&thSensor, 5);
    dht22AddTemperatureCallback(&thSensor, temperature, NULL);
    switchAdd(4, (SwitchCallback_t)lightToggle, &light0);
    switchStart();
    iotStart();

    dht22Start(&thSensor);
}
