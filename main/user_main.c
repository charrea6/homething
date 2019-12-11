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
#include "gpiox.h"

static const char TAG[] = "main";
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
  |  3  |   RX    | RX       | RX       | RX    | DHT22   |

  |  4  |   D2    | DHT22    |          |       | Switch 3|
  |  5  |   D1    |          |  Switch  |       | Switch 2|
  |6-11 |   --    |  --      |  --      | --    |  --     |  
  | 12  |   D6    | Switch   |          | Switch| Switch 1|
  | 13  |   D7    | Motion   |          |       | Motion  |
  | 14  |   D5    | Fan      |          |       | Fan     |
  | 15  |   D8    |          |          |       |         |
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
#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3) || defined(CONFIG_LIGHTS_4)
#define ENABLE_LIGHT_1
static Light_t light0;
#endif
#if defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3) || defined(CONFIG_LIGHTS_4)
#define ENABLE_LIGHT_2
static Light_t light1;
#endif
#if defined(CONFIG_LIGHTS_3) || defined(CONFIG_LIGHTS_4)
#define ENABLE_LIGHT_3
static Light_t light2;
#endif
#if defined(CONFIG_LIGHTS_4)
#define ENABLE_LIGHT_4
static Light_t light3;
#endif

//
// DHT22 : Temperature and humidity + fan
//
#if defined(CONFIG_DHT22)
struct TemperatureSensor {
    DHT22Sensor_t sensor;
    char temperatureStr[6];
    iotElement_t temperatureElement;
    iotElementPub_t temperaturePub;
};
static struct TemperatureSensor thSensor0;

#if defined(CONFIG_DHT22)
static struct TemperatureSensor thSensor1;
#endif

#if defined(CONFIG_FAN)
static HumidityFan_t fan0;
#if defined(CONFIG_FAN_2)
static HumidityFan_t fan1;
#endif
#endif
#endif

#if defined(CONFIG_MOTION)
static Motion_t motion0;
#endif

#if defined(CONFIG_DHT22)
static void temperatureUpdate(void *userData, int16_t tenthsUnit)
{
    struct TemperatureSensor *thSensor = userData;
    iotValue_t value;
    sprintf(thSensor->temperatureStr, "%d.%d", tenthsUnit / 10, tenthsUnit % 10);
    value.s = thSensor->temperatureStr;
    iotElementPubUpdate(&thSensor->temperaturePub, value);
}

static void initTHSensor(struct TemperatureSensor *thSensor, char *name, int pin)
{
    dht22Init(&thSensor->sensor, pin);

    sprintf(thSensor->temperatureStr, "0.0");
    
    thSensor->temperatureElement.name = name;
    iotElementAdd(&thSensor->temperatureElement);

    thSensor->temperaturePub.name = "";
    thSensor->temperaturePub.type = iotValueType_String;
    thSensor->temperaturePub.retain = false;
    thSensor->temperaturePub.value.s = thSensor->temperatureStr;
    iotElementPubAdd(&thSensor->temperatureElement, &thSensor->temperaturePub);

    dht22AddTemperatureCallback(&thSensor->sensor, temperatureUpdate, thSensor);
}

#endif

#if defined( CONFIG_LIGHTS_1) || defined(CONFIG_LIGHTS_2) || defined(CONFIG_LIGHTS_3) || defined(CONFIG_LIGHTS_4)
static void lightSwitchCallback(void *userData, int state)
{
    Light_t *light = userData;
    lightToggle(light);
}

static void setupLight(Light_t *light, int switchPin, int relayPin)
{
    ESP_LOGI(TAG, "Adding light %d switch %d", relayPin, switchPin);
    lightInit(light, relayPin);
    if (switchPin != -1)
    {
        switchAdd(switchPin, lightSwitchCallback, light);
    }
}
#endif

void app_main(void)
{
    struct timeval tv = {.tv_sec = 0, .tv_usec=0};
    ESP_ERROR_CHECK( nvs_flash_init() );
    
    settimeofday(&tv, NULL);
    
    iotInit();
    gpioxInit();
#ifdef ENABLE_LIGHT_1
    setupLight(&light0, CONFIG_LIGHT_1_SWITCH_PIN, CONFIG_LIGHT_1_RELAY_PIN);
#endif
#ifdef ENABLE_LIGHT_2
    setupLight(&light1, CONFIG_LIGHT_2_SWITCH_PIN, CONFIG_LIGHT_2_RELAY_PIN);
#endif
#ifdef ENABLE_LIGHT_3
    setupLight(&light2, CONFIG_LIGHT_3_SWITCH_PIN, CONFIG_LIGHT_3_RELAY_PIN);
#endif
#ifdef ENABLE_LIGHT_4
    setupLight(&light3, CONFIG_LIGHT_4_SWITCH_PIN, CONFIG_LIGHT_4_RELAY_PIN);
#endif

#if defined(CONFIG_MOTION)
    ESP_LOGI(TAG, "Adding motion");
    motionInit(&motion0, CONFIG_MOTION_PIN);
#endif

#if defined(CONFIG_DOORBELL)
    ESP_LOGI(TAG , "Adding doorbell");
    doorbellInit(CONFIG_DOORBELL_PIN);
#endif

#if defined(CONFIG_DHT22)
    ESP_LOGI(TAG, "Adding temperature...");
    initTHSensor(&thSensor0, "temperature0", CONFIG_DHT22_PIN);
#if defined(CONFIG_DHT22)
    initTHSensor(&thSensor1, "temperature1", CONFIG_DHT22_2_PIN);
#endif
#if defined(CONFIG_FAN)
    ESP_LOGI(TAG, "Adding Fan");
    humidityFanInit(&fan0, CONFIG_FAN_PIN, CONFIG_FAN_HUMIDITY);
    dht22AddHumidityCallback(&thSensor0.sensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fan0);
#if defined(CONFIG_FAN_2)
    ESP_LOGI(TAG, "Adding Fan 2");
    humidityFanInit(&fan1, CONFIG_FAN_2_PIN, CONFIG_FAN_HUMIDITY);
    dht22AddHumidityCallback(&thSensor1.sensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fan1);
#endif

#endif

    dht22Start();
#endif

#if defined(CONFIG_LIGHTS_1) || \
    defined(CONFIG_LIGHTS_2) || \
    defined(CONFIG_LIGHTS_3) || \
    defined(CONFIG_LIGHTS_4) || \
    defined(CONFIG_MOTION) || \
    defined(CONFIG_DOORBELL)
    switchStart();
#endif
    updaterInit();
    iotStart();
}
