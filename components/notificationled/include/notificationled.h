#ifndef _NOTIFICATIONLED_H_
#define _NOTIFICATIONLED_H_
#include <stdint.h>
#include <stdbool.h>
typedef struct NotificationLedPattern {
    bool on; // If true the LED will be turned on for the specified on time before switching off for the off time and then repeating.
    uint32_t onTime; // if on time is 0 then the LED will stay on otherwise it will be turned off after the specified number of milliseconds
    uint32_t offTime; // the LED will stay off for the specified number of milliseconds before the cycle repeats.
} NotificationLedPattern_t;

typedef struct NotificationLed *NotificationLed_t;

void notificationLedInit();

NotificationLed_t notificationLedNew(int pin);
void notificationLedSetPattern(NotificationLed_t led, NotificationLedPattern_t *pattern);
void notificationLedGetPattern(NotificationLed_t led, NotificationLedPattern_t *pattern);

#endif