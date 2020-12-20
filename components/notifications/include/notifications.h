#ifndef _NOTIFICATIONS_H_
#define _NOTIFICATIONS_H_

#include <stddef.h>
#include <stdint.h>


typedef enum{
    Notifications_Class_Switch = 0,
    Notifications_Class_Temperature,
    Notifications_Class_Humidity,
    Notifications_Class_Max
}Notifications_Class_e;

typedef union {
    uint32_t humidity;
    uint32_t temperature;
    bool switchState;
} NotificationsData_t; 

typedef struct {
    Notifications_Class_e clazz;
    uint32_t id;
    NotificationsData_t data;
}NotificationsMessage_t;

typedef uint32_t Notifications_ID_t;

typedef void (*NotificationsCallback_t)(void *user,  NotificationsMessage_t *message);

#define NOTIFICATIONS_ID_GPIOSWITCH_BASE  0x00000000
#define NOTIFICATIONS_ID_DHT22_BASE       0x00000000

#define NOTIFICATIONS_ID_ALL              0xffffffff
#define NOTIFICATIONS_ID_ERROR            0xffffffff

#define NOTIFICATIONS_MAKE_ID(_name, _id) ( NOTIFICATIONS_ID_ ## _name ## _BASE | _id)

void notificationsInit(void);
void notificationsRegister(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsCallback_t callback, void *user);
void notificationsNotify(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsData_t *data);
#endif