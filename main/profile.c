#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"

#include "deviceprofile.h"
#include "notifications.h"
#include "iot.h"

#include "relays.h"
#include "switches.h"
#include "sensors.h"
#include "led.h"
#include "led_strips.h"
#include "controllers.h"


static const char TAG[] = "profile";

void processProfile(void)
{
    uint8_t *profile = NULL;
    size_t profileLen = 0;
    DeviceProfile_DeviceConfig_t config;

    ESP_LOGI(TAG, "Processing Profile");
    if (deviceProfileGetProfile(&profile, &profileLen)) {
        ESP_LOGE(TAG, "Failed to load profile!");
        return;
    }
    
    if (deviceProfileDeserialize(profile, profileLen, &config)) {
        ESP_LOGE(TAG, "Failed to deserialise profile!");
        return;
    }

    initRelays(&config);

    sensorsInit(&config);

    initSwitches(config.switchConfig, config.switchCount);

    initLeds(config.ledConfig, config.ledCount);

#ifdef CONFIG_LED_STRIP
    initLEDStripSPI(config.ledStripSpiConfig, config.ledStripSpiCount);
#endif

    initControllers(&config);

    ESP_LOGI(TAG, "Signalling Profile finished processing");
    NotificationsData_t notification;
    notification.systemState = Notifications_SystemState_InitFinished;
    notificationsNotify(Notifications_Class_System, NOTIFICATIONS_ID_ALL, &notification);
}
