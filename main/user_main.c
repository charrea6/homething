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

#include "sdkconfig.h"
#include "light.h"
#include "switch.h"
#include "iot.h"
#include "dht.h"
#include "motion.h"
#include "humidityfan.h"
#include "doorbell.h"
#include "updater.h"
#include "gpiox.h"

#include "provisioning.h"

static const char TAG[] = "main";
static const char PROFILE[] = "profile";
/* 
  Pins
  ----

  |    Pins       |
  |GPIO | NodeMcu |
  |-----|---------|
  |  0  |   D3    |
  |  1  |   TX    |
  |  2  |   D4    |
  |  3  |   RX    |
  |  4  |   D2    |
  |  5  |   D1    |
  |6-11 |   --    |
  | 12  |   D6    |
  | 13  |   D7    |
  | 14  |   D5    |
  | 15  |   D8    |
  | 16  |   D0    |

GPIO0(D3) - used to indicate to bootloader to go into upload mode + tied to Flash button.
GPIO16(D0) - possibly tied to RST to allow exit from deep sleep by pulling GPIO16 low.

| LED | Pin |
|-----|-----|
| Red | 16  |
| Blue|  2  |

*/

static int nrofLights = 0;
static int nrofBells = 0;
static int nrofMotions = 0;
static int nrofTemps = 0;
static int nrofFans = 0;

//
// Lights
// 
#if defined(CONFIG_LIGHT)
static Light_t *lights;
#endif

//
// DHT22 : Temperature and humidity + fan
//
#if defined(CONFIG_DHT22)
struct TemperatureSensor {
    DHT22Sensor_t sensor;
    char name[13];
    char temperatureStr[6];
    iotElement_t temperatureElement;
    iotElementPub_t temperaturePub;
};
static struct TemperatureSensor *thSensors;

#if defined(CONFIG_FAN)
static HumidityFan_t *fans;
#endif
#endif

#if defined(CONFIG_MOTION)
static Motion_t *motionSenors;
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

static void initTHSensor(struct TemperatureSensor *thSensor, int id, int pin)
{
    sprintf(thSensor->name, "temperature%d", id);
    dht22Init(&thSensor->sensor, pin);

    sprintf(thSensor->temperatureStr, "0.0");
    
    thSensor->temperatureElement.name = thSensor->name;
    iotElementAdd(&thSensor->temperatureElement);

    thSensor->temperaturePub.name = "";
    thSensor->temperaturePub.type = iotValueType_String;
    thSensor->temperaturePub.retain = false;
    thSensor->temperaturePub.value.s = thSensor->temperatureStr;
    iotElementPubAdd(&thSensor->temperatureElement, &thSensor->temperaturePub);

    dht22AddTemperatureCallback(&thSensor->sensor, temperatureUpdate, thSensor);
}

#endif

#if defined(CONFIG_LIGHT)
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

esp_err_t processProfile(uint8_t *profile, size_t len)
{
    int i;
    int bell=0, light=0, motion=0, temp=0, fan=0;

    // First pass of profile string to work out how many of each type of sensor/switch/relay we have
    for (i=0; i < len; i++)
    {
        switch(profile[i])
        {
            case 'L': nrofLights++; i+=2;
            break;
            case 'B': i+=1;
            if (nrofBells >= 1) {
                ESP_LOGW(TAG, "Only 1 doorbell supported");
            } else {
                nrofBells++;
            }
            break;
            case 'M': nrofMotions++; i+=1;
            break;
            case 'T': nrofTemps++; i+=1;
            break;
            case 'F': nrofFans++; i+=1;
            break;
            default:
            printf("Unknown profile type '%c'!\n", profile[i]);
            abort();
        }
    }

    ESP_LOGI(TAG, "Lights: %d Doorbells: %d Motion Detectors: %d Temperature Sensors: %d Humidity Fans: %d", 
        nrofLights, nrofBells, nrofMotions, nrofTemps, nrofFans);
    
    switchInit(nrofLights + nrofBells + nrofMotions);

#if defined(CONFIG_LIGHT)
    lights = calloc(nrofLights, sizeof(Light_t));
    if (lights == NULL) 
    {
        return ESP_ERR_NO_MEM;
    }
#endif
#if defined(CONFIG_MOTION)
    motionSenors = calloc(nrofMotions, sizeof(Motion_t));
    if (motionSenors == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
#endif
#if defined(CONFIG_DHT22)
    thSensors = calloc(nrofTemps, sizeof(struct TemperatureSensor));
    if (thSensors == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
#if defined(CONFIG_FAN)
    fans = calloc(nrofFans, sizeof(HumidityFan_t));
    if (fans == NULL)
    {
        return ESP_ERR_NO_MEM;
    }
#endif
#endif

    // Second pass to create the sensors/lights etc
    for (i=0; i < len; i++)
    {
        switch(profile[i])
        {
            case 'L': 
#if defined(CONFIG_LIGHT)
            {
                int relay_pin = profile[++i];
                int sw_pin = profile[++i];
                setupLight(&lights[light], sw_pin, relay_pin);
                light++;
            }
#else
            i+=2;
#endif
            break;
            case 'B': 
#if defined(CONFIG_DOORBELL)
            if (bell == 0)
            {
                doorbellInit((int) profile[++i]);
                bell++;
            }
#else
            i+=1;
#endif
            break;
            case 'M': 
#if defined(CONFIG_MOTION)
            motionInit(&motionSenors[motion], (int)profile[++i]);
            motion++;
#else
            i+=1;
#endif
            break;
            case 'T': 
#if defined(CONFIG_DHT22)
            initTHSensor(&thSensors[temp], temp, (int)profile[++i]);
            temp++;
#else
            i+=1;
#endif
            break;
            case 'F': 
#if defined(CONFIG_FAN)
            if (fan >= nrofTemps)
            {
                ESP_LOGE(TAG, "More fans than temperature sensors, reducing to the same as sensors!");
            }
            else
            {
                humidityFanInit(&fans[fan], (int)profile[++i], CONFIG_FAN_HUMIDITY);
                dht22AddHumidityCallback(&thSensors[fan].sensor, (DHT22CallBack_t)humidityFanUpdateHumidity, &fans[fan]);
                fan++;
            }
#else
            i+=1;
#endif
            break;
            default:
            printf("Unknown profile type '%c'!\n", profile[i]);
            abort();
        }
    }
    return ESP_OK;
}

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
static void taskStats(TimerHandle_t xTimer)
{
    unsigned long nrofTasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tasksStatus = malloc(sizeof(TaskStatus_t) * nrofTasks);
    if (tasksStatus == NULL) {
        ESP_LOGE(TAG, "Failed to allocate task status array");
        return;
    }
    nrofTasks = uxTaskGetSystemState( tasksStatus, nrofTasks, NULL);
    int i;
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    putchar('\n');
    
    printf("TASK STATS # %lu\n", nrofTasks);
    for (i=0; i < nrofTasks; i++) {
        printf("%-30s: % 10d % 10d\n", tasksStatus[i].pcTaskName, tasksStatus[i].eCurrentState, tasksStatus[i].usStackHighWaterMark);
    }
    free(tasksStatus);
    for (i=0; i < 80; i++) {
        putchar('=');
    }
    printf("\n\n");
}
#endif

/* NVS Configuration
 *
 * NS: wifi
 * net = Network to connect to
 * pass = Wifi network password
 * 
 * NS: mqtt
 * host = Server IP address
 * port = Server Port
 * user = username
 * pass = password
 * 
 * NS: thing
 * profile = Profile configuration string (str)
 *     <b> == byte
 * 
 *     L<b1><b2> : b1= Relay pin, b2= Switch pin
 *     B<b1>     : b1= Doorbell switch pin
 *     M<b1>     : b1= Motion detector pin
 *     T<b1>     : b1= DHT22 pin
 *     F<b1>     : b1= Relay pin
 * relayOnLevel = Relay activation level (1/0)
 * 
 * NS: gpiox
 * num = GPIOX number of expanders (byte)
 * sda = SDA pin (byte)
 * scl = SCL pin (byte)
 * 
 */
void app_main(void)
{
    nvs_handle handle;
    esp_err_t err;
    uint8_t relayOnLevel = 0;
    size_t len;
    uint8_t *profile = NULL;
    struct timeval tv = {.tv_sec = 0, .tv_usec=0};
    settimeofday(&tv, NULL);

    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( nvs_open("thing", NVS_READONLY, &handle) );

    err = nvs_get_u8(handle, "relayOnLevel", &relayOnLevel);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to retrieve relayOnLevel, defaulting to 0: %d", err);
    }
    relaySetOnLevel(relayOnLevel);

    ESP_ERROR_CHECK( nvs_get_blob(handle, PROFILE, NULL, &len) );
    profile = malloc(len);
    if (profile == NULL)
    {  
        printf("Failed to allocate memory for profile string (%d bytes required)\n", len);
        abort();
    }

    ESP_ERROR_CHECK( nvs_get_blob(handle, PROFILE, profile, &len) );

    nvs_close(handle);

    iotInit();
    provisioningInit();
    gpioxInit();
    
    ESP_ERROR_CHECK( processProfile(profile, len));
    free(profile);

#if defined(CONFIG_DHT22)
    dht22Start();
#endif

#if defined(CONFIG_LIGHT) || \
    defined(CONFIG_MOTION) || \
    defined(CONFIG_DOORBELL)
    switchStart();
#endif

    updaterInit();
    iotStart();
    provisioningStart();

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
    xTimerStart(xTimerCreate("stats", 30*1000 / portTICK_RATE_MS, pdTRUE, NULL, taskStats), 0);
#endif
}
