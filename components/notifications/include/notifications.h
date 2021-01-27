#ifndef _NOTIFICATIONS_H_
#define _NOTIFICATIONS_H_

#include <stddef.h>
#include <stdint.h>


typedef enum{
    Notifications_Class_Wifi = 0,
    Notifications_Class_Switch,
    Notifications_Class_Temperature,
    Notifications_Class_Humidity,
    Notifications_Class_Pressure,
    Notifications_Class_Max
}Notifications_Class_e;

typedef union {
    uint32_t humidity;  // %RH * 100
    uint32_t pressure;  // hPa * 100
    int32_t temperature; // degrees C * 100
    bool switchState;
    bool connectionState;
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
#define NOTIFICATIONS_ID_I2C_BASE         0x01000000
#define NOTIFICATIONS_ID_DS18x20_BASE     0x02000000

#define NOTIFICATIONS_ID_WIFI_STATION     0x00000000
#define NOTIFICATIONS_ID_WIFI_AP          0x00000001

#define NOTIFICATIONS_ID_ALL              0xffffffff
#define NOTIFICATIONS_ID_ERROR            0xffffffff

#define NOTIFICATIONS_MAKE_ID(_name, _id) ( NOTIFICATIONS_ID_ ## _name ## _BASE | _id)
#define NOTIFICATIONS_MAKE_I2C_ID(sda, scl, addr) ( NOTIFICATIONS_ID_I2C_BASE | (sda) << 16 | (scl) << 8 | (addr))

void notificationsInit(void);
void notificationsRegister(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsCallback_t callback, void *user);
void notificationsNotify(Notifications_Class_e clazz, Notifications_ID_t id, NotificationsData_t *data);
#endif