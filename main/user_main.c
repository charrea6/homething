#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
#include "doorbell.h"
#include "updater.h"
const char TAG[] = "main";
/* 
  Pin Allocations
  ---------------

  | Profile  | Functions
  |----------|----------------------------------------------------
  | Bathroom | Light/Switch/Motion/Temperature/Humidity Fan
  | Utility  | 3x Light/3x Switch/Motion/Temperature/Humidity Fan
  | Light    | Light/Switch
  | Doorbell | doorbell(Switch)
  
  |    Pins       |             Profile                   |
  |GPIO | NodeMcu | Bathroom | Doorbell | Light | Utility |
  |-----|---------|----------|----------|-------|---------|
  |  0  |   D3    | Light    |          | Light | Light 1 |
  |  1  |   TX    | TX       | TX       | TX    | TX      |
  |  2  |   D4    |          |          |       | Light 2 |
  |  3  |   RX    | RX       | RX       | RX    | RX      |
  |  4  |   D2    | DHT22    |          |       | DHT22   |
  |  5  |   D1    |          |  Switch  |       | Switch 2|
  |6-11 |   --    |  --      |  --      | --    |  --     |  
  | 12  |   D6    | Switch   |          | Switch| Switch 1|
  | 13  |   D7    | Motion   |          |       | Motion  |
  | 14  |   D5    | Fan      |          |       | Fan     |
  | 15  |   D8    |          |          |       | Switch 3|
  | 16  |   D0    |          |          |       | Light 3 |

GPIO0(D3) - used to indicate to bootloader to go into upload mode + tied to Flash button.
GPIO16(D0) - possibly tied to RST to allow exit from deep sleep by pulling GPIO16 low.

| LED | Pin |
|-----|-----|
| Red | 16  |
| Blue|  2  |

*/

//
// Lights
// 
#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
#define ENABLE_LIGHT_1
static Light_t light0;
#endif
#if defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3)
#define ENABLE_LIGHT_2
static Light_t light1;
#endif
#if defined(CONFIG_LIGHTS_3)
#define ENABLE_LIGHT_3
static Light_t light2;
#endif

//
// DHT22 : Temperature and humidity + fan
//
#if defined(CONFIG_DHT22)
static DHT22Sensor_t thSensor;
static char temperatureStr[6];
static iotElement_t temperatureElement;
static iotElementPub_t temperaturePub;

#if defined(CONFIG_FAN)
static HumidityFan_t fan0;
#endif
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
    iotElementPubUpdate(&temperaturePub, value);
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
    ESP_LOGI(TAG, "Adding light %d switch %d", relayPin, switchPin);
    lightInit(light, relayPin);
    switchAdd(switchPin, lightSwitchCallback, light);
}
#endif

void app_main(void)
{
    struct timeval tv = {.tv_sec = 0, .tv_usec=0};

    ESP_ERROR_CHECK( nvs_flash_init() );

    settimeofday(&tv, NULL);
    
    iotInit();

#ifdef ENABLE_LIGHT_1
    setupLight(&light0, 12, 0);
#endif
#ifdef ENABLE_LIGHT_2
    setupLight(&light1, 2, 5);
#endif
#ifdef ENABLE_LIGHT_3
    setupLight(&light2, 15, 16);
#endif

#if defined(CONFIG_MOTION)
    ESP_LOGI(TAG, "Adding motion");
    motionInit(&motion0, 13);
#endif

#if defined(CONFIG_DOORBELL)
    ESP_LOGI(TAG , "Adding doorbell");
    doorbellInit(5);
#endif

#if defined(CONFIG_DHT22)
    ESP_LOGI(TAG, "Adding temperature...");
    dht22Init(&thSensor, 4);

    sprintf(temperatureStr, "0.0");
    
    temperatureElement.name = "temperature";
    iotElementAdd(&temperatureElement);

    temperaturePub.name = "";
    temperaturePub.type = iotValueType_String;
    temperaturePub.retain = false;
    temperaturePub.value.s = temperatureStr;
    iotElementPubAdd(&temperatureElement, &temperaturePub);

    dht22AddTemperatureCallback(&thSensor, temperatureUpdate, NULL);
#if defined(CONFIG_FAN)
    ESP_LOGI(TAG, "Adding Fan");
    humidityFanInit(&fan0, 14, 75);
    dht22AddHumidityCallback(&thSensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fan0);
#endif
    dht22Start(&thSensor);
#endif

#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3) || defined(CONFIG_MOTION)
    switchStart();
#endif
    updaterInit();
    iotStart();
}
