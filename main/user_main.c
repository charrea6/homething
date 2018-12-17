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

//
// Lights
// 
#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
static Light_t light0;
#endif
#if defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
static Light_t light1;
#endif
#if defined(CONFIG_LIGHTS_3)
static Light_t light2;
#endif

//
// DHT22 : Temperature and humidity + fan
//
#if defined(CONFIG_DHT22)
static DHT22Sensor_t thSensor;
static char temperatureStr[6];
static iotElement_t *temperatureElement;
static iotElementPub_t *temperaturePub;

static HumidityFan_t fan0;
#endif

#if defined(CONFIG_MOTION)
static Motion_t motion0;
#endif

#if defined(CONFIG_DHT22)
static void temperatureUpdate(void *userData, int16_t tenthsUnit)
{
    iotValue_t value;
    sprintf(temperatureStr, "%d.%d", tenthsUnit / 10, tenthsUnit % 10);
    value.s = temperatureStr;
    iotElementPubUpdate(temperaturePub, value);
}
#endif

#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
static void lightSwitchCallback(void *userData, int state)
{
    Light_t *light = userData;
    lightToggle(light);
}

static void setupLight(Light_t *light, int switchPin, int relayPin)
{
    lightInit(light, relayPin);
    switchAdd(switchPin, lightSwitchCallback, light);
}
#endif

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    iotInit(CONFIG_ROOM);
#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
    setupLight(&light0, 14, 5);
#endif
#if defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
    setupLight(&light1, 12, 4);
#endif
#if defined(CONFIG_LIGHTS_3)
    setupLight(&light2, 13, 0);
#endif

#if defined(CONFIG_MOTION)    
    motionInit(&motion0, 16);
#endif

#if defined(CONFIG_DHT22) 
    iotValue_t value;
    
    dht22Init(&thSensor, 4);
    sprintf(temperatureStr, "0.0");
    value.s = temperatureStr;
    iotElementAdd("temperature", &temperatureElement);
    iotElementPubAdd(temperatureElement, "", iotValueType_String, value, &temperaturePub);
    dht22AddTemperatureCallback(&thSensor, temperatureUpdate, NULL);
#if defined(CONFIG_FAN)
    humidityFanInit(&fan0, 12, 75);
    dht22AddHumidityCallback(&thSensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fan0);
#endif
    dht22Start(&thSensor);
#endif

#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3) || defined(CONFIG_MOTION)
    switchStart();
#endif
    iotStart();
}
