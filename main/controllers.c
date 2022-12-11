#include "deviceprofile.h"
#include "humidityfan.h"
#include "thermostat.h"

static void controllersInitFinished(void *user, NotificationsMessage_t *message);

#ifdef CONFIG_THERMOSTAT
static void thermostatsInit();

static DeviceProfile_ThermostatConfig_t *thermostatConfig = NULL;
static uint32_t thermostatCount = 0;
#endif

#ifdef CONFIG_HUMIDISTAT
static void humidistatsInit();

static DeviceProfile_HumidistatConfig_t *humidistatConfig = NULL;
static uint32_t humidistatCount = 0;
#endif


void initControllers(DeviceProfile_DeviceConfig_t *config)
{
    notificationsRegister(Notifications_Class_System, NOTIFICATIONS_ID_ALL, controllersInitFinished, NULL);
#ifdef CONFIG_THERMOSTAT
    thermostatConfig = config->thermostatConfig;
    thermostatCount = config->thermostatCount;
    /* Take over ownership of the config structures */
    config->thermostatConfig = NULL;
    config->thermostatCount = 0;
#endif
#ifdef CONFIG_HUMIDISTAT
    humidistatConfig = config->humidistatConfig;
    humidistatCount = config->humidistatCount;
    /* Take over ownership of the config structures */
    config->humidistatConfig = NULL;
    config->humidistatCount = 0;
#endif
}

static void controllersInitFinished(void *user, NotificationsMessage_t *message)
{
#ifdef CONFIG_THERMOSTAT
    if (thermostatCount > 0) {
        thermostatsInit();
        free(thermostatConfig);
        thermostatConfig = NULL;
        thermostatCount = 0;
    }
#endif
#ifdef CONFIG_HUMIDISTAT
    if (humidistatCount > 0) {
        humidistatsInit();
        free(humidistatConfig);
        humidistatConfig = NULL;
        humidistatCount = 0;
    }
#endif
}

#ifdef CONFIG_THERMOSTAT
static void thermostatsInit()
{
    uint32_t i;
    Thermostat_t *thermostats = calloc(thermostatCount, sizeof(Thermostat_t));
    if (thermostats == NULL) {
        return;
    }
    for (i = 0; i < thermostatCount; i++) {
        Relay_t *relay = relayFind(thermostatConfig[i].relay);
        Notifications_ID_t sensor = notificationsFindId(thermostatConfig[i].sensor);
        if ((relay != NULL) && (sensor != NOTIFICATIONS_ID_ERROR)) {
            thermostatInit(&thermostats[i], relay, sensor);
            if (thermostatConfig[i].name) {
                iotElementSetHumanDescription(thermostats[i].element, thermostatConfig[i].name);
            }
        }
    }

}
#endif

#ifdef CONFIG_HUMIDISTAT
static void humidistatsInit()
{
    uint32_t i;
    HumidityFan_t *humidistats = calloc(humidistatCount, sizeof(HumidityFan_t));
    if (humidistats == NULL) {
        return;
    }
    for (i = 0; i < humidistatCount; i++) {
        Relay_t *relay = relayFind(humidistatConfig[i].relay);
        Notifications_ID_t sensor = notificationsFindId(humidistatConfig[i].sensor);
        if ((relay != NULL) && (sensor != NOTIFICATIONS_ID_ERROR)) {
            humidityFanInit(&humidistats[i], relay, sensor, 80);
            if (humidistatConfig[i].name) {
                iotElementSetHumanDescription(humidistats[i].element, humidistatConfig[i].name);
            }
        }
    }
}
#endif